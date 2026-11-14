#include "octo/dynamics.hpp"
#include <algorithm>
#include <iostream>

namespace octo::dynamics {
using namespace math;
namespace {
V3 project_ball(V3 v, double radius, V3 normal) {
    v = v - normal * dot(v, normal);
    double n = norm(v);
    return n > radius && n > 0 ? v * (radius / n) : v;
}
std::vector<Body> incremented(const State &s, const std::vector<double> &x) {
    auto b = s.bodies;
    for (std::size_t i = 0; i < b.size(); ++i) {
        b[i].p += V3{x[6 * i], x[6 * i + 1], x[6 * i + 2]};
        b[i].q = normalized(exp(V3{x[6 * i + 3], x[6 * i + 4], x[6 * i + 5]}) * b[i].q);
    }
    return b;
}
V3 point_at(const std::vector<Body> &b, Anchor a) {
    return b.at(a.body).p + b[a.body].q.rotate(a.local);
}
V3 contact_delta(const ContactConstraint &c, const std::vector<Body> &b) {
    V3 d = -c.fixed_point;
    for (unsigned j = 0; j < c.count; ++j)
        d += point_at(b, c.anchors[j]) * c.weights[j];
    return d;
}
bool attached(const Cup &c) {
    return c.phase == CupPhase::Attached || c.phase == CupPhase::Peeling;
}
} // namespace
bool System::solve_coupled(double h, StepResult &stats) {
    const bool diagnostic = diagnostics != nullptr;
    active_penalty_ = solver.constraint_penalty;
    const auto start_cups = state.cups;
    prepare(h);
    std::vector<double> x(state.bodies.size() * 6, 0);
    for (std::size_t j = 0; j < state.cups.size(); ++j) {
        auto &cup = state.cups[j];
        const double old_force = cup.force_limit;
        sample_cup(cup, x);
        cup.advance(h, std::max(old_force - cup.normal_multiplier, 0.0));
        cup.normal_multiplier = std::max(0.0, cup.normal_multiplier + cup.force_limit - old_force);
        if (!attached(cup)) {
            cup.normal_multiplier = 0;
            cup.tangent_multiplier = {};
        } else if (!attached(start_cups[j]))
            cup.tangent_origin =
                point(cup.a) - (cup.target_static ? cup.static_point : point(cup.b));
    }
    bool success = false;
    for (unsigned outer = 0; outer < solver.max_outer; ++outer) {
        ++stats.outer_iterations;
        state.contacts = build_contacts(x, h, &stats.ccd_hits);
        const auto keys = state.contacts;
        frozen_contacts_ = true;
        bool minimized = false;
        try {
            minimized = minimize(h, stats, x);
        } catch (...) {
            frozen_contacts_ = false;
            throw;
        }
        frozen_contacts_ = false;
        if (!minimized) {
            stats.failure =
                "Newton minimization failed; residual=" + std::to_string(stats.residual);
            if (diagnostic)
                (*diagnostics) << stats.failure << " outer=" << outer << " h=" << h << std::endl;
            active_penalty_ = 0;
            return false;
        }
        const auto bodies = incremented(state, x);
        double dual_error = 0, contact_dual = 0, cup_dual = 0, violation = 0, pressure_error = 0;
        std::uint64_t worst_contact_key = 0;
        std::array<unsigned, 16> kind_counts{};
        const double rho = penalty();
        for (auto &c : state.contacts) {
            ++kind_counts[c.key >> 60];
            V3 d = contact_delta(c, bodies), center = -c.normal_fixed_point;
            for (unsigned k = 0; k < c.count; ++k)
                center += point_at(bodies, c.normal_anchors[k]) * c.weights[k];
            double gap = dot(center, c.normal) - c.normal_offset;
            V3 tangent = d - c.tangent_origin;
            tangent = tangent - c.normal * dot(tangent, c.normal);
            double normal = std::max(0.0, c.normal_multiplier - rho * gap);
            V3 friction =
                project_ball(c.tangent_multiplier - tangent * rho, c.friction * normal, c.normal);
            const double change = std::max(
                std::abs(normal - c.normal_multiplier) / std::max(1.0, normal),
                norm(friction - c.tangent_multiplier) / std::max(1.0, norm(friction)));
            if (change > contact_dual) {
                contact_dual = change;
                worst_contact_key = c.key;
            }
            dual_error = std::max(dual_error, change);
            c.normal_multiplier = normal;
            c.tangent_multiplier = friction;
            violation = std::max(violation, std::max(-gap, 0.0));
        }
        for (std::size_t j = 0; j < state.cups.size(); ++j) {
            auto &cup = state.cups[j];
            auto predicted = start_cups[j];
            V3 d = point_at(bodies, cup.a) -
                   (cup.target_static ? cup.static_point : point_at(bodies, cup.b));
            double gap = dot(d, cup.normal);
            V3 tangent = d - cup.tangent_origin;
            tangent = tangent - cup.normal * dot(tangent, cup.normal);
            if (attached(cup)) {
                double normal = std::max(0.0, cup.normal_multiplier - rho * gap);
                V3 friction =
                    project_ball(cup.tangent_multiplier - tangent * rho,
                                 cup.material.friction * (normal + cup.force_limit), cup.normal);
                cup_dual = std::max(cup_dual, std::abs(normal - cup.normal_multiplier) /
                                                     std::max(1.0, cup.force_limit));
                cup_dual = std::max(cup_dual, norm(friction - cup.tangent_multiplier) /
                                                     std::max(1.0, cup.force_limit));
                dual_error = std::max(dual_error, cup_dual);
                cup.normal_multiplier = normal;
                cup.tangent_multiplier = friction;
                violation = std::max(violation, std::max(-gap, 0.0));
            }
            sample_cup(predicted, x);
            const double tension =
                std::max(0.0, cup.force_limit - cup.normal_multiplier) + rho * std::max(gap, 0.0);
            double opening = 0;
            for (double g : predicted.gaps)
                opening = std::max(opening, g);
            predicted.advance(h, tension, std::clamp(opening / predicted.gap_reference, 0.0, 2.0));
            pressure_error =
                std::max(pressure_error, std::abs(predicted.force_limit - cup.force_limit) /
                                             std::max(0.01, predicted.force_limit));
            double n = cup.normal_multiplier;
            V3 t = cup.tangent_multiplier, origin = cup.tangent_origin;
            const double previous_force = cup.force_limit;
            // Fixed-point predictor always starts from tick-boundary pressure/damage.
            if (attached(predicted) && attached(cup)) {
                predicted.pressure = 0.5 * (predicted.pressure + cup.pressure);
                for (unsigned k = 0; k < 12; ++k)
                    predicted.damage[k] = 0.5 * (predicted.damage[k] + cup.damage[k]);
                predicted.force_limit = (predicted.external_pressure - predicted.pressure) * pi *
                                        predicted.radius * predicted.radius *
                                        predicted.seal_fraction;
            }
            cup = predicted;
            cup.normal_multiplier =
                attached(cup) ? std::max(0.0, n + cup.force_limit - previous_force) : 0;
            cup.tangent_multiplier = attached(cup) ? t - cup.normal * dot(t, cup.normal) : V3{};
            cup.tangent_origin = origin;
        }
        for (auto &joint : state.hinges) {
            auto qa = bodies[joint.a.body].q * joint.a.rotation,
                 qb = joint.target_static ? Q{} : bodies[joint.b.body].q * joint.b.rotation;
            V3 position = point_at(bodies, joint.a) -
                          (joint.target_static ? joint.static_point : point_at(bodies, joint.b)),
               axis = cross(qa.rotate(joint.axis_a), qb.rotate(joint.axis_b));
            joint.position_multiplier = joint.position_multiplier - position * rho;
            joint.axis_multiplier = joint.axis_multiplier - axis * rho;
            violation = std::max({violation, norm(position), norm(axis)});
        }
        auto fresh = build_contacts(x, h);
        bool same = fresh.size() == keys.size();
        if (same)
            for (std::size_t j = 0; j < fresh.size(); ++j)
                if (fresh[j].key != keys[j].key) {
                    same = false;
                    break;
                }
        for (const auto &c : fresh) {
            V3 center = -c.normal_fixed_point;
            for (unsigned k = 0; k < c.count; ++k)
                center += point_at(bodies, c.normal_anchors[k]) * c.weights[k];
            violation = std::max(violation, std::max(c.normal_offset - dot(center, c.normal), 0.0));
        }
        if (diagnostic)
            (*diagnostics) << "outer=" << outer << " h=" << h
                           << " contacts=" << state.contacts.size() << " same=" << same
                           << " geometry=" << violation << " dual=" << dual_error
                           << " contact_dual=" << contact_dual << " cup_dual=" << cup_dual
                           << " worst_kind=" << (worst_contact_key >> 60)
                           << " kinds_1_2_3_4_5_8_10="
                           << kind_counts[1] << ',' << kind_counts[2] << ',' << kind_counts[3] << ','
                           << kind_counts[4] << ',' << kind_counts[5] << ',' << kind_counts[8]
                           << ',' << kind_counts[10]
                           << " pressure=" << pressure_error << std::endl;
        stats.pressure_error = pressure_error;
        stats.constraint_error = violation;
        stats.friction_error = dual_error;
        if (same && violation < solver.constraint_tolerance && dual_error < solver.dual_tolerance &&
            pressure_error < solver.pressure_tolerance) {
            success = true;
            break;
        }
        if (outer % 4 == 3 && violation > solver.constraint_tolerance)
            active_penalty_ = std::min(active_penalty_ * 4, 1e9);
    }
    if (!success) {
        stats.failure =
            "Contact iteration exhausted; geometry=" + std::to_string(stats.constraint_error) +
            ", dual=" + std::to_string(stats.friction_error);
        active_penalty_ = 0;
        return false;
    }
    frozen_contacts_ = true;
    commit_increment(x, h, stats);
    frozen_contacts_ = false;
    active_penalty_ = 0;
    return true;
}
} // namespace octo::dynamics
