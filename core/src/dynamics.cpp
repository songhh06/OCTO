#include "octo/dynamics.hpp"
#include <chrono>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>

namespace octo::dynamics {
using namespace math;
using D = Dual<12>;
namespace {
template <class T> struct Pose {
    Vec<T> p;
    Quat<T> q;
};
template <class T>
Pose<T> pose(const Body &b, const std::vector<double> &x, std::size_t id, std::size_t local) {
    Vec<T> dp, dr;
    for (std::size_t j = 0; j < 3; ++j) {
        if constexpr (std::is_same_v<T, double>) {
            dp[j] = x[id * 6 + j];
            dr[j] = x[id * 6 + 3 + j];
        } else {
            dp[j] = T::variable(x[id * 6 + j], local + j);
            dr[j] = T::variable(x[id * 6 + 3 + j], local + 3 + j);
        }
    }
    return {Vec<T>(b.p) + dp, exp(dr) * Quat<T>(b.q)};
}
template <class T> Pose<T> anchor_pose(Pose<T> a, Anchor b) {
    return {a.p + a.q.rotate(Vec<T>(b.local)), a.q * Quat<T>(b.rotation)};
}
template <class T> std::pair<Vec<T>, Vec<T>> strain(Pose<T> a, Pose<T> b, double ds) {
    auto mid = midpoint(a.q, b.q);
    return {mid.conjugate().rotate(b.p - a.p) / T(ds), log(a.q.conjugate() * b.q) / T(ds)};
}
template <class T>
T rod_energy(const Element &e, const Material &m, Pose<T> a, Pose<T> b, const Body &old_a,
             const Body &old_b, double h) {
    const auto [nu, kappa] = strain(anchor_pose(a, e.a), anchor_pose(b, e.b), e.ds);
    const double area = pi * e.radius0 * e.radius0, I = pi * std::pow(e.radius0, 4) / 4;
    const Vec<T> dn = nu - Vec<T>{T(0), T(0), T(1)};
    T energy = (T(m.shear * area) * (dn.x * dn.x + dn.y * dn.y) + T(m.young * area) * dn.z * dn.z +
                T(m.young * I) * (kappa.x * kappa.x + kappa.y * kappa.y) +
                T(m.shear * 2 * I) * kappa.z * kappa.z) *
               T(0.5 * e.ds);
    energy = energy + T(e.ds) * (dot(Vec<T>(e.active_n), dn) + dot(Vec<T>(e.active_m), kappa));
    if (h > 0 && m.damping_time > 0) {
        auto oldpa = anchor_pose(Pose<double>{old_a.p, old_a.q}, e.a);
        auto oldpb = anchor_pose(Pose<double>{old_b.p, old_b.q}, e.b);
        const auto [on, ok] = strain(oldpa, oldpb, e.ds);
        auto vn = nu - Vec<T>(on), vk = kappa - Vec<T>(ok);
        energy =
            energy +
            T(0.5 * e.ds * m.damping_time / h) *
                (T(m.shear * area) * (vn.x * vn.x + vn.y * vn.y) + T(m.young * area) * vn.z * vn.z +
                 T(m.young * I) * (vk.x * vk.x + vk.y * vk.y) + T(2 * m.shear * I) * vk.z * vk.z);
    }
    if (value(nu.z) < m.min_stretch)
        return T(std::numeric_limits<double>::infinity());
    return energy;
}
template <class T> T capped(T x, double k, double cap) {
    return value(x) < cap / k ? T(0.5 * k) * x * x : T(cap) * (x - T(0.5 * cap / k));
}
template <class T> T absolute(T x) { return value(x) >= 0 ? x : -x; }
template <class T> T normal_al(T gap, double multiplier, double penalty) {
    T trial = T(multiplier) - T(penalty) * gap;
    T projected = value(trial) > 0 ? trial : T(0);
    return (projected * trial - T(0.5) * projected * projected - T(0.5 * multiplier * multiplier)) /
           T(penalty);
}
template <class T> T tangent_al(Vec<T> displacement, V3 multiplier, double penalty, double limit) {
    if (limit <= 0)
        return T(0);
    auto trial = Vec<T>(multiplier) - displacement * T(penalty);
    T squared = dot(trial, trial);
    if (value(squared) < limit * limit || value(squared) < 1e-28)
        return (squared - T(dot(multiplier, multiplier))) / T(2 * penalty);
    return (T(limit) * sqrt(squared) - T(0.5 * (limit * limit + dot(multiplier, multiplier)))) /
           T(penalty);
}
template <class T> T body_gap(Pose<T> a, Pose<T> b, const Body &ba, const Body &bb) {
    if (!ba.box && !bb.box)
        return norm(a.p - b.p) - T(ba.radius + bb.radius);
    if (!ba.box && bb.box) {
        const auto local = b.q.conjugate().rotate(a.p - b.p);
        Vec<T> delta;
        bool outside = false;
        T inside(-1e300);
        for (std::size_t j = 0; j < 3; ++j) {
            const double v = value(local[j]), extent = bb.half_extents[j];
            if (v > extent) {
                delta[j] = local[j] - T(extent);
                outside = true;
            } else if (v < -extent) {
                delta[j] = local[j] + T(extent);
                outside = true;
            } else
                delta[j] = T(0);
            T gap = absolute(local[j]) - T(extent);
            if (value(gap) > value(inside))
                inside = gap;
        }
        return (outside ? norm(delta) : inside) - T(ba.radius);
    }
    if (ba.box && !bb.box)
        return body_gap(b, a, bb, ba);
    std::array<Vec<T>, 3> axes_a{a.q.rotate({T(1), T(0), T(0)}), a.q.rotate({T(0), T(1), T(0)}),
                                 a.q.rotate({T(0), T(0), T(1)})};
    std::array<Vec<T>, 3> axes_b{b.q.rotate({T(1), T(0), T(0)}), b.q.rotate({T(0), T(1), T(0)}),
                                 b.q.rotate({T(0), T(0), T(1)})};
    T best(-1e300);
    auto test_axis = [&](Vec<T> axis) {
        T n2 = dot(axis, axis);
        if (value(n2) < 1e-16)
            return;
        axis = axis / sqrt(n2);
        T extent(0);
        for (std::size_t j = 0; j < 3; ++j)
            extent = extent + absolute(dot(axis, axes_a[j])) * T(ba.half_extents[j]) +
                     absolute(dot(axis, axes_b[j])) * T(bb.half_extents[j]);
        T gap = absolute(dot(b.p - a.p, axis)) - extent;
        if (value(gap) > value(best))
            best = gap;
    };
    for (auto axis : axes_a)
        test_axis(axis);
    for (auto axis : axes_b)
        test_axis(axis);
    for (auto x : axes_a)
        for (auto y : axes_b)
            test_axis(cross(x, y));
    return best;
}
double maxabs(const std::vector<double> &a) {
    double n = 0;
    for (double v : a)
        n = std::max(n, std::abs(v));
    return n;
}
double inner(const std::vector<double> &a, const std::vector<double> &b) {
    return std::inner_product(a.begin(), a.end(), b.begin(), 0.0);
}
V3 inertia_times(const Body &b, V3 w) {
    const auto &i = b.inertia;
    const auto &o = b.inertia_offdiag;
    return {i.x * w.x + o.x * w.y + o.y * w.z, o.x * w.x + i.y * w.y + o.z * w.z,
            o.y * w.x + o.z * w.y + i.z * w.z};
}
bool excluded_pair(const State &s, std::size_t i, std::size_t j) {
    const auto &a = s.bodies[i];
    const auto &b = s.bodies[j];
    if (a.fixed && b.fixed)
        return true;
    if (a.arm == b.arm && a.arm >= 0 &&
        (std::abs(a.station - b.station) <= 2 ||
         std::abs(a.reference_s - b.reference_s) < a.radius + b.radius))
        return true;
    if ((a.arm < 0 && b.station == 1) || (b.arm < 0 && a.station == 1))
        for (const auto &e : s.elements)
            if ((e.a.body == i && e.b.body == j) || (e.a.body == j && e.b.body == i))
                return true;
    return false;
}
} // namespace
void Material::validate() const {
    for (double x : {young, shear, density, sigma_max, tau_on, tau_off, min_stretch})
        if (!std::isfinite(x) || x <= 0)
            throw std::invalid_argument("positive finite material value required");
    if (!std::isfinite(damping_time) || damping_time < 0 || min_stretch >= 1)
        throw std::invalid_argument("invalid damping/stretch bound");
}
std::size_t System::add_body(Body b) {
    if (!finite(b.p) || !finite(b.v) || !finite(b.w) || !finite(b.force) || !finite(b.torque) ||
        !finite(b.inertia) || !finite(b.inertia_offdiag) || !finite(b.half_extents) ||
        !(b.mass > 0) || !std::isfinite(b.mass) || b.inertia.x <= 0 || b.inertia.y <= 0 ||
        b.inertia.z <= 0 || !std::isfinite(b.radius) || b.radius < 0 || !std::isfinite(b.volume) ||
        b.volume < 0 || b.half_extents.x <= 0 || b.half_extents.y <= 0 || b.half_extents.z <= 0)
        throw std::invalid_argument("invalid body");
    const auto &i = b.inertia;
    const auto &o = b.inertia_offdiag;
    const double det =
        i.x * i.y * i.z + 2 * o.x * o.y * o.z - i.x * o.z * o.z - i.y * o.y * o.y - i.z * o.x * o.x;
    if (i.x * i.y - o.x * o.x <= 0 || det <= 0)
        throw std::invalid_argument("inertia must be SPD");
    double qn = b.q.w * b.q.w + b.q.x * b.q.x + b.q.y * b.q.y + b.q.z * b.q.z;
    if (!std::isfinite(qn) || std::abs(qn - 1) > 1e-6)
        throw std::invalid_argument("body quaternion not normalized");
    state.bodies.push_back(b);
    return state.bodies.size() - 1;
}
V3 System::point(Anchor a) const {
    const auto &b = state.bodies.at(a.body);
    return b.p + b.q.rotate(a.local);
}
std::vector<std::size_t> System::add_rod(Anchor root, Q frame, double length, double r0, double r1,
                                         unsigned count, int arm) {
    material.validate();
    if (count < 2 || count > 128 || length <= 0 || r0 <= 0 || r1 <= 0 ||
        root.body >= state.bodies.size())
        throw std::invalid_argument("invalid rod geometry");
    std::vector<std::size_t> ids{root.body};
    const double ds = length / count;
    const V3 origin = point(root);
    root.rotation = state.bodies[root.body].q.conjugate() * frame;
    for (unsigned i = 1; i <= count; ++i) {
        Body b;
        b.p = origin + frame.rotate(V3{0, 0, ds * i});
        b.q = frame;
        b.mass = 0;
        b.inertia = {0, 0, 0};
        b.radius = r0 + (r1 - r0) * double(i) / count;
        b.arm = arm;
        b.station = static_cast<int>(i);
        b.reference_s = ds * i;
        state.bodies.push_back(b);
        ids.push_back(state.bodies.size() - 1);
    }
    for (unsigned i = 0; i < count; ++i) {
        Element e;
        e.a = i == 0 ? root : Anchor{ids[i], {}, Q{}};
        e.b = {ids[i + 1], {}, Q{}};
        e.ds = ds;
        e.radius0 = r0 + (r1 - r0) * (double(i) + 0.5) / count;
        e.radius = e.radius0;
        const double area = pi * e.radius0 * e.radius0, mass = material.density * area * ds;
        const V3 inertia{mass * (3 * e.radius0 * e.radius0 + ds * ds) / 12,
                         mass * (3 * e.radius0 * e.radius0 + ds * ds) / 12,
                         mass * e.radius0 * e.radius0 / 2};
        for (auto id : {ids[i], ids[i + 1]}) {
            auto &b = state.bodies[id];
            b.mass += mass / 2;
            b.volume += area * ds / 2;
            const Q local = b.q.conjugate() * frame;
            const std::array<V3, 3> axes{local.rotate({1, 0, 0}), local.rotate({0, 1, 0}),
                                         local.rotate({0, 0, 1})};
            for (std::size_t k = 0; k < 3; ++k) {
                const auto &v = axes[k];
                const double scale = inertia[k] * 0.5;
                b.inertia += V3{v.x * v.x, v.y * v.y, v.z * v.z} * scale;
                b.inertia_offdiag += V3{v.x * v.y, v.x * v.z, v.y * v.z} * scale;
            }
            if (i == 0 && id == root.body) {
                const auto &r = root.local;
                b.inertia +=
                    V3{r.y * r.y + r.z * r.z, r.x * r.x + r.z * r.z, r.x * r.x + r.y * r.y} *
                    (mass / 2);
                b.inertia_offdiag += V3{-r.x * r.y, -r.x * r.z, -r.y * r.z} * (mass / 2);
            }
        }
        state.elements.push_back(e);
    }
    return ids;
}
V3 drag_force(V3 w, V3 tangent, double diameter, double length, double rho, double cp, double cl) {
    if (!finite(w) || !finite(tangent) || diameter <= 0 || length <= 0 || rho < 0 || cp < 0 ||
        cl < 0)
        throw std::invalid_argument("invalid fluid input");
    const double n = norm(tangent);
    if (n <= 0)
        throw std::invalid_argument("zero tangent");
    tangent = tangent / n;
    auto parallel = tangent * dot(w, tangent), perp = w - parallel;
    return (perp * (cp * norm(perp)) + parallel * (cl * norm(parallel))) *
           (-0.5 * rho * diameter * length);
}
double friction_limit(double compression, double adhesion, double mu) {
    if (compression < 0 || adhesion < 0 || mu < 0)
        throw std::invalid_argument("negative friction input");
    return mu * (compression + adhesion);
}
void Cup::advance(double h, double tension, double eccentricity) {
    if (!(h > 0) || !std::isfinite(h) || capacitance <= 0 || radius <= 0 ||
        material.conductance < 0 || !std::isfinite(tension) || tension < 0)
        throw std::invalid_argument("invalid cup input");
    for (double value :
         {external_pressure, pressure, capacitance, pump_rate, pressure_max, vapor_pressure,
          seal_time, rim_stiffness, peel_rate, gap_reference, gap_conductance, material.friction,
          material.sealability, material.conductance, eccentricity})
        if (!std::isfinite(value))
            throw std::invalid_argument("nonfinite cup parameter");
    if (pump_rate < 0 || pressure_max <= 0 || vapor_pressure <= 0 ||
        external_pressure < vapor_pressure || rim_stiffness <= 0 || peel_rate < 0 ||
        gap_reference <= 0 || gap_conductance < 0 || material.friction < 0 ||
        material.sealability < 0 || material.sealability > 1 || !finite(normal) ||
        std::abs(norm(normal) - 1) > 1e-6)
        throw std::invalid_argument("cup parameter range or unit normal invalid");
    for (double d : damage)
        if (!std::isfinite(d) || d < 0 || d > 1)
            throw std::invalid_argument("invalid rim damage");
    if (!command) {
        phase = CupPhase::Free;
        pressure = external_pressure;
        force_limit = 0;
        seal_time = 0;
        seal_fraction = 0;
        damage.fill(0);
        return;
    }
    unsigned touching = 0;
    double gap_flow = 0, remaining = 0;
    for (std::size_t i = 0; i < 12; ++i) {
        if (!std::isfinite(gaps[i]))
            throw std::invalid_argument("nonfinite rim gap");
        if (gaps[i] <= gap_reference * 0.1)
            ++touching;
        gap_flow += std::pow(std::max(gaps[i], 0.0) / gap_reference, 3) / 12;
        remaining += 1 - damage[i];
    }
    seal_fraction = double(touching) / 12 * material.sealability * remaining / 12;
    if (phase == CupPhase::Free && touching > 0)
        phase = CupPhase::Touch;
    if (touching >= 10 && material.sealability > 0.5) {
        seal_time += h;
        if (seal_time >= 0.05 && phase == CupPhase::Touch)
            phase = CupPhase::Sealing;
    } else {
        seal_time = 0;
        if (phase == CupPhase::Attached || phase == CupPhase::Peeling)
            phase = CupPhase::Peeling;
        else if (phase != CupPhase::Free)
            phase = CupPhase::Leaking;
    }
    const bool pump =
        phase == CupPhase::Sealing || phase == CupPhase::Attached || phase == CupPhase::Peeling;
    const double conductance = material.conductance + gap_conductance * gap_flow;
    pressure =
        (pressure + h / capacitance * (conductance * external_pressure - (pump ? pump_rate : 0))) /
        (1 + h * conductance / capacitance);
    pressure = std::clamp(pressure, std::max(vapor_pressure, external_pressure - pressure_max),
                          external_pressure);
    if (phase == CupPhase::Sealing && external_pressure - pressure > 3000)
        phase = CupPhase::Attached;
    const double undamaged_force = (external_pressure - pressure) * pi * radius * radius *
                                   double(touching) / 12 * material.sealability;
    if (phase == CupPhase::Attached || phase == CupPhase::Peeling) {
        for (std::size_t k = 0; k < 12; ++k) {
            const double demand =
                tension / 12 * std::max(0.0, 1 + eccentricity * std::cos(2 * pi * double(k) / 12));
            const double capacity = undamaged_force / 12 * std::max(1 - damage[k], 0.001);
            damage[k] =
                std::clamp(damage[k] + h * peel_rate *
                                           (std::max(demand / std::max(capacity, 1e-12) - 1, 0.0) +
                                            std::max(gaps[k] / gap_reference - 0.1, 0.0)),
                           0.0, 1.0);
            if (damage[k] > 0)
                phase = CupPhase::Peeling;
        }
    }
    remaining = 0;
    for (double d : damage)
        remaining += 1 - d;
    seal_fraction = double(touching) / 12 * material.sealability * remaining / 12;
    force_limit = (external_pressure - pressure) * pi * radius * radius * seal_fraction;
    if (remaining < 0.1 || touching == 0) {
        phase = CupPhase::Leaking;
        force_limit = 0;
    }
    if (phase == CupPhase::Leaking && external_pressure - pressure < 100) {
        phase = CupPhase::Free;
        force_limit = 0;
    }
    if (phase == CupPhase::Leaking || phase == CupPhase::Free)
        force_limit = 0;
}
void System::prepare(double h) {
    for (auto &e : state.elements) {
        for (std::size_t i = 0; i < 7; ++i) {
            if (!std::isfinite(e.command[i]) || e.command[i] < 0 || e.command[i] > 1)
                throw std::invalid_argument("muscle command outside [0,1]");
            const double tau = e.command[i] > e.activation[i] ? material.tau_on : material.tau_off;
            e.activation[i] = (e.activation[i] + h / tau * e.command[i]) / (1 + h / tau);
        }
        const double area = pi * e.radius0 * e.radius0;
        const auto &old_a = state.bodies[e.a.body];
        const auto &old_b = state.bodies[e.b.body];
        const auto mid = midpoint(old_a.q * e.a.rotation, old_b.q * e.b.rotation);
        const auto rate = mid.conjugate().rotate(
            (old_b.v - old_a.v) / e.ds -
            cross((old_a.w + old_b.w) * 0.5, (point(e.b) - point(e.a)) / e.ds));
        const double fl =
            std::exp(-std::pow((e.stretch - 1) / 0.5, 2)) * std::clamp(1 - 0.05 * rate.z, 0.2, 1.8);
        e.active_n = {};
        e.active_m = {};
        for (std::size_t j = 0; j < 4; ++j) {
            const double angle = double(j) * pi / 2;
            const double force = e.activation[j] * material.sigma_max * area * 0.25 * fl;
            e.active_n.z += force;
            e.active_m +=
                cross(V3{0.6 * e.radius0 * std::cos(angle), 0.6 * e.radius0 * std::sin(angle), 0},
                      V3{0, 0, force});
        }
        e.active_n.z -= e.activation[4] * material.sigma_max * area * 0.25 * fl;
        e.active_n.z += (e.activation[5] + e.activation[6]) * material.sigma_max * area * 0.05 * fl;
        e.active_m.z +=
            (e.activation[5] - e.activation[6]) * material.sigma_max * area * e.radius0 * 0.05 * fl;
    }
    for (auto &cup : state.cups) {
        if (coupled_constraints)
            continue;
        V3 delta = point(cup.a) - (cup.target_static ? cup.static_point : point(cup.b));
        const double tension = std::max(dot(delta, cup.normal), 0.0) * cup.rim_stiffness;
        cup.advance(h, tension);
    }
}
double System::objective(const std::vector<double> &x, double h,
                         std::vector<double> *gradient) const {
    const std::size_t n = state.bodies.size();
    if (x.size() != 6 * n)
        throw std::invalid_argument("DOF length mismatch");
    if (gradient)
        gradient->assign(x.size(), 0);
    double energy = 0;
    auto scatter = [&](D e, std::size_t a, std::size_t b) {
        energy += e.v;
        if (!gradient)
            return;
        for (std::size_t j = 0; j < 6; ++j) {
            (*gradient)[6 * a + j] += e.d[j];
            (*gradient)[6 * b + j] += e.d[6 + j];
        }
    };
    for (std::size_t i = 0; i < n; ++i) {
        const auto &b = state.bodies[i];
        V3 dp{x[6 * i], x[6 * i + 1], x[6 * i + 2]}, dr{x[6 * i + 3], x[6 * i + 4], x[6 * i + 5]};
        V3 force = b.force + gravity * (b.mass - water_density * b.volume);
        energy -= dot(force, dp);
        V3 torque = b.torque;
        if (h > 0) {
            const V3 momentum = b.q.rotate(inertia_times(b, b.q.conjugate().rotate(b.w)));
            torque = torque - cross(b.w, momentum);
        }
        energy -= dot(torque, dr);
        V3 gp = -force, gr = -torque;
        if (h > 0 && !b.fixed) {
            const auto dv = dp - b.v * h, dw = dr - b.w * h;
            const auto local = b.q.conjugate().rotate(dw);
            const auto ilocal = inertia_times(b, local);
            energy += (b.mass * dot(dv, dv) + dot(local, ilocal)) / (2 * h * h);
            gp += dv * (b.mass / (h * h));
            gr += b.q.rotate(ilocal) / (h * h);
            if (water_density > 0 && b.arm < 0) {
                const V3 relative = dp / h - water_velocity;
                const double speed = norm(relative), area = pi * b.radius * b.radius;
                energy += h * water_density * area * speed * speed * speed / 6;
                gp += relative * (0.5 * water_density * area * speed);
            }
        }
        if (gradient)
            for (std::size_t j = 0; j < 3; ++j) {
                (*gradient)[6 * i + j] += gp[j];
                (*gradient)[6 * i + 3 + j] += gr[j];
            }
    }
    for (const auto &e : state.elements) {
        auto a = pose<D>(state.bodies[e.a.body], x, e.a.body, 0),
             b = pose<D>(state.bodies[e.b.body], x, e.b.body, 6);
        auto energy_e =
            rod_energy(e, material, a, b, state.bodies[e.a.body], state.bodies[e.b.body], h);
        if (h > 0 && water_density > 0) {
            auto pa = anchor_pose(a, e.a).p, pb = anchor_pose(b, e.b).p;
            V3 old_a = point(e.a), old_b = point(e.b),
               tangent = (old_b - old_a) / norm(old_b - old_a);
            auto relative = (pa + pb - Vec<D>(old_a + old_b)) / D(2 * h) - Vec<D>(water_velocity);
            auto parallel = Vec<D>(tangent) * dot(relative, Vec<D>(tangent));
            auto perp = relative - parallel;
            auto cubic = [](Vec<D> v) {
                D v2 = dot(v, v);
                return value(v2) > 1e-24 ? v2 * sqrt(v2) : D(0);
            };
            energy_e =
                energy_e + D(h * water_density * 2 * e.radius * e.ds / 6) *
                               (D(drag_perp) * cubic(perp) + D(drag_parallel) * cubic(parallel));
        }
        scatter(energy_e, e.a.body, e.b.body);
    }
    for (const auto &link : links) {
        auto a = anchor_pose(pose<D>(state.bodies.at(link.a.body), x, link.a.body, 0), link.a).p;
        auto b =
            link.target_static
                ? Vec<D>(link.static_point)
                : anchor_pose(pose<D>(state.bodies.at(link.b.body), x, link.b.body, 6), link.b).p;
        auto delta = a - b;
        auto length2 = dot(delta, delta);
        if (value(length2) > 1e-24) {
            auto gap = sqrt(length2) - D(link.rest);
            scatter(capped(gap, link.stiffness, link.capacity), link.a.body,
                    link.target_static ? link.a.body : link.b.body);
        }
    }
    for (const auto &cup : state.cups) {
        if (cup.phase != CupPhase::Attached && cup.phase != CupPhase::Peeling)
            continue;
        auto a = anchor_pose(pose<D>(state.bodies.at(cup.a.body), x, cup.a.body, 0), cup.a).p;
        auto b = cup.target_static
                     ? Vec<D>(cup.static_point)
                     : anchor_pose(pose<D>(state.bodies.at(cup.b.body), x, cup.b.body, 6), cup.b).p;
        auto delta = a - b;
        auto normal = Vec<D>(cup.normal);
        D gap = dot(delta, normal);
        D e(0);
        if (coupled_constraints) {
            e = normal_al(gap, cup.normal_multiplier, penalty()) + D(cup.force_limit) * gap;
            auto tangent = delta - Vec<D>(cup.tangent_origin);
            tangent = tangent - normal * dot(tangent, normal);
            e = e + tangent_al(tangent, cup.tangent_multiplier, penalty(),
                               cup.material.friction * (cup.normal_multiplier + cup.force_limit));
        } else if (value(gap) > 0)
            e = capped(gap, cup.rim_stiffness, cup.force_limit);
        auto tangent = delta - normal * gap;
        D t2 = dot(tangent, tangent);
        if (!coupled_constraints && value(t2) > 1e-24)
            e = e + capped(sqrt(t2), cup.rim_stiffness, cup.material.friction * cup.force_limit);
        scatter(e, cup.a.body, cup.target_static ? cup.a.body : cup.b.body);
    }
    if (coupled_constraints) {
        using A = Dual<24>;
        const auto generated =
            frozen_contacts_ ? std::vector<ContactConstraint>{} : build_contacts(x, h);
        const auto &contacts = frozen_contacts_ ? state.contacts : generated;
        for (const auto &c : contacts) {
            if (c.normal_multiplier == 0 && norm(c.tangent_multiplier) == 0) {
                V3 center = -c.normal_fixed_point;
                for (unsigned j = 0; j < c.count; ++j)
                    center += anchor_pose(pose<double>(state.bodies[c.normal_anchors[j].body], x,
                                                       c.normal_anchors[j].body, 0),
                                          c.normal_anchors[j])
                                  .p *
                              c.weights[j];
                if (dot(center, c.normal) - c.normal_offset >= 0)
                    continue;
            }
            Vec<A> delta(-c.fixed_point);
            for (unsigned j = 0; j < c.count; ++j)
                delta += anchor_pose(
                             pose<A>(state.bodies[c.anchors[j].body], x, c.anchors[j].body, 6 * j),
                             c.anchors[j])
                             .p *
                         A(c.weights[j]);
            auto normal = Vec<A>(c.normal);
            Vec<A> center_delta(-c.normal_fixed_point);
            for (unsigned j = 0; j < c.count; ++j)
                center_delta += anchor_pose(pose<A>(state.bodies[c.normal_anchors[j].body], x,
                                                    c.normal_anchors[j].body, 6 * j),
                                            c.normal_anchors[j])
                                    .p *
                                A(c.weights[j]);
            A gap = dot(center_delta, normal) - A(c.normal_offset);
            auto tangent = delta - Vec<A>(c.tangent_origin);
            tangent = tangent - normal * dot(tangent, normal);
            A e = normal_al(gap, c.normal_multiplier, penalty()) +
                  tangent_al(tangent, c.tangent_multiplier, penalty(),
                             c.friction * c.normal_multiplier);
            energy += e.v;
            if (gradient)
                for (unsigned j = 0; j < c.count; ++j)
                    for (unsigned k = 0; k < 6; ++k)
                        (*gradient)[6 * c.anchors[j].body + k] += e.d[6 * j + k];
        }
        for (const auto &joint : state.hinges) {
            auto a =
                anchor_pose(pose<D>(state.bodies.at(joint.a.body), x, joint.a.body, 0), joint.a);
            auto b = joint.target_static
                         ? Pose<D>{Vec<D>(joint.static_point), Quat<D>{}}
                         : anchor_pose(pose<D>(state.bodies.at(joint.b.body), x, joint.b.body, 6),
                                       joint.b);
            auto position = a.p - b.p,
                 axis = cross(a.q.rotate(Vec<D>(joint.axis_a)), b.q.rotate(Vec<D>(joint.axis_b)));
            D e = D(0.5 * penalty()) * (dot(position, position) + dot(axis, axis)) -
                  dot(Vec<D>(joint.position_multiplier), position) -
                  dot(Vec<D>(joint.axis_multiplier), axis);
            scatter(e, joint.a.body, joint.target_static ? joint.a.body : joint.b.body);
        }
    }
    if (!coupled_constraints)
        for (std::size_t i = 0; i < n; ++i) {
            auto pa = pose<D>(state.bodies[i], x, i, 0);
            auto a = pa.p;
            for (const auto &plane : planes) {
                D extent(state.bodies[i].radius);
                if (state.bodies[i].box) {
                    auto normal = pa.q.conjugate().rotate(Vec<D>(plane.normal));
                    extent = D(0);
                    for (std::size_t j = 0; j < 3; ++j)
                        extent = extent + absolute(normal[j]) * D(state.bodies[i].half_extents[j]);
                }
                D gap = dot(a, Vec<D>(plane.normal)) - D(plane.offset) - extent;
                if (value(gap) < 0)
                    scatter(D(0.5 * plane.stiffness) * gap * gap, i, i);
            }
            if (!state.bodies[i].box && !terrain.triangles().empty()) {
                auto hit = terrain.closest(V3{a.x.v, a.y.v, a.z.v}, state.bodies[i].radius);
                if (hit.found) {
                    D gap =
                        dot(a - Vec<D>(hit.point), Vec<D>(hit.normal)) - D(state.bodies[i].radius);
                    if (value(gap) < 0)
                        scatter(D(50000) * gap * gap, i, i);
                }
            }
            if (!self_contact)
                continue;
            for (std::size_t j = i + 1; j < n; ++j) {
                const auto &ba = state.bodies[i];
                const auto &bb = state.bodies[j];
                if (excluded_pair(state, i, j))
                    continue;
                const V3 candidate_b = bb.p + V3{x[j * 6], x[j * 6 + 1], x[j * 6 + 2]};
                const double bound_a = ba.box ? norm(ba.half_extents) : ba.radius,
                             bound_b = bb.box ? norm(bb.half_extents) : bb.radius;
                V3 delta = V3{a.x.v, a.y.v, a.z.v} - candidate_b;
                if (dot(delta, delta) > (bound_a + bound_b) * (bound_a + bound_b))
                    continue;
                if (dot(delta, delta) < 1e-20 && !ba.box && !bb.box)
                    return std::numeric_limits<double>::infinity();
                auto pb = pose<D>(bb, x, j, 6);
                D gap = body_gap(pa, pb, ba, bb);
                if (value(gap) < 0)
                    scatter(D(50000) * gap * gap, i, j);
            }
        }
    return energy;
}
std::vector<System::HessianBlock> System::hessian_blocks(const std::vector<double> &x,
                                                         double h) const {
    std::vector<HessianBlock> blocks;
    blocks.reserve(state.bodies.size() + state.elements.size() + state.cups.size() +
                   state.contacts.size() + links.size() + state.hinges.size());
    auto add = [&](std::vector<std::size_t> ids, auto function) {
        std::vector<std::size_t> unique;
        for (auto id : ids)
            if (std::find(unique.begin(), unique.end(), id) == unique.end())
                unique.push_back(id);
        auto evaluate = [&]<std::size_t N>() {
            auto get = [&](std::size_t id) {
                std::size_t slot = static_cast<std::size_t>(
                    std::find(unique.begin(), unique.end(), id) - unique.begin());
                return pose<Jet2<N>>(state.bodies[id], x, id, 6 * slot);
            };
            auto value = function(get);
            HessianBlock block;
            block.count = static_cast<unsigned>(unique.size());
            std::copy(unique.begin(), unique.end(), block.ids.begin());
            unsigned size = block.count * 6;
            for (unsigned i = 0; i < size; ++i)
                for (unsigned j = 0; j < size; ++j)
                    block.matrix[i * 24 + j] = value.h[i * N + j];
            blocks.push_back(std::move(block));
        };
        if (unique.size() <= 2)
            evaluate.template operator()<12>();
        else
            evaluate.template operator()<24>();
    };
    if (h > 0)
        for (std::size_t id = 0; id < state.bodies.size(); ++id) {
            const auto &b = state.bodies[id];
            if (b.fixed)
                continue;
            HessianBlock block;
            block.count = 1;
            block.ids[0] = id;
            for (unsigned j = 0; j < 3; ++j) {
                block.matrix[j * 24 + j] = b.mass / (h * h);
                V3 unit{};
                unit[j] = 1;
                V3 column = b.q.rotate(inertia_times(b, b.q.conjugate().rotate(unit))) / (h * h);
                for (unsigned k = 0; k < 3; ++k)
                    block.matrix[(3 + k) * 24 + 3 + j] = column[k];
            }
            if (water_density > 0 && b.arm < 0) {
                V3 w = V3{x[6 * id], x[6 * id + 1], x[6 * id + 2]} / h - water_velocity;
                double speed = norm(w);
                if (speed > 1e-12) {
                    double scale = 0.5 * water_density * pi * b.radius * b.radius / h;
                    for (unsigned j = 0; j < 3; ++j)
                        for (unsigned k = 0; k < 3; ++k)
                            block.matrix[j * 24 + k] +=
                                scale * ((j == k ? speed : 0) + w[j] * w[k] / speed);
                }
            }
            blocks.push_back(block);
        }
    for (const auto &e : state.elements)
        add({e.a.body, e.b.body}, [&](auto get) {
            auto a = get(e.a.body), b = get(e.b.body);
            using T = std::decay_t<decltype(a.q.w)>;
            T result =
                rod_energy(e, material, a, b, state.bodies[e.a.body], state.bodies[e.b.body], h);
            if (h > 0 && water_density > 0) {
                auto pa = anchor_pose(a, e.a).p, pb = anchor_pose(b, e.b).p;
                V3 oa = point(e.a), ob = point(e.b), t = (ob - oa) / norm(ob - oa);
                auto relative = (pa + pb - Vec<T>(oa + ob)) / T(2 * h) - Vec<T>(water_velocity);
                auto parallel = Vec<T>(t) * dot(relative, Vec<T>(t)), perp = relative - parallel;
                auto cubic = [](auto v) {
                    auto squared = dot(v, v);
                    using S = std::decay_t<decltype(squared)>;
                    return value(squared) > 1e-24 ? squared * sqrt(squared) : S(0);
                };
                result =
                    result + T(h * water_density * 2 * e.radius * e.ds / 6) *
                                 (T(drag_perp) * cubic(perp) + T(drag_parallel) * cubic(parallel));
            }
            return result;
        });
    for (const auto &link : links)
        add(link.target_static ? std::vector<std::size_t>{link.a.body}
                               : std::vector<std::size_t>{link.a.body, link.b.body},
            [&](auto get) {
                auto a = anchor_pose(get(link.a.body), link.a).p;
                using T = std::decay_t<decltype(a.x)>;
                auto b = link.target_static ? Vec<T>(link.static_point)
                                            : anchor_pose(get(link.b.body), link.b).p;
                auto d = a - b;
                T squared = dot(d, d);
                return value(squared) > 1e-24
                           ? capped(sqrt(squared) - T(link.rest), link.stiffness, link.capacity)
                           : T(0);
            });
    for (const auto &cup : state.cups) {
        if (cup.phase != CupPhase::Attached && cup.phase != CupPhase::Peeling)
            continue;
        add(cup.target_static ? std::vector<std::size_t>{cup.a.body}
                              : std::vector<std::size_t>{cup.a.body, cup.b.body},
            [&](auto get) {
                auto a = anchor_pose(get(cup.a.body), cup.a).p;
                using T = std::decay_t<decltype(a.x)>;
                auto b = cup.target_static ? Vec<T>(cup.static_point)
                                           : anchor_pose(get(cup.b.body), cup.b).p;
                auto delta = a - b, n = Vec<T>(cup.normal);
                T gap = dot(delta, n), energy(0);
                if (coupled_constraints) {
                    energy =
                        normal_al(gap, cup.normal_multiplier, penalty()) + T(cup.force_limit) * gap;
                    auto t = delta - Vec<T>(cup.tangent_origin);
                    t = t - n * dot(t, n);
                    energy = energy + tangent_al(t, cup.tangent_multiplier, penalty(),
                                                 cup.material.friction *
                                                     (cup.normal_multiplier + cup.force_limit));
                } else {
                    if (value(gap) > 0)
                        energy = capped(gap, cup.rim_stiffness, cup.force_limit);
                    auto t = delta - n * gap;
                    T squared = dot(t, t);
                    if (value(squared) > 1e-24)
                        energy = energy + capped(sqrt(squared), cup.rim_stiffness,
                                                 cup.material.friction * cup.force_limit);
                }
                return energy;
            });
    }
    if (coupled_constraints) {
        for (const auto &c : state.contacts) {
            if (c.normal_multiplier == 0 && norm(c.tangent_multiplier) == 0) {
                V3 center = -c.normal_fixed_point;
                for (unsigned j = 0; j < c.count; ++j)
                    center += anchor_pose(pose<double>(state.bodies[c.normal_anchors[j].body], x,
                                                       c.normal_anchors[j].body, 0),
                                          c.normal_anchors[j])
                                  .p *
                              c.weights[j];
                if (dot(center, c.normal) - c.normal_offset >= 0)
                    continue;
            }
            std::vector<std::size_t> ids;
            for (unsigned k = 0; k < c.count; ++k)
                ids.push_back(c.anchors[k].body);
            add(ids, [&](auto get) {
                using T = std::decay_t<decltype(get(c.anchors[0].body).q.w)>;
                Vec<T> d(-c.fixed_point), center(-c.normal_fixed_point), n(c.normal);
                for (unsigned k = 0; k < c.count; ++k) {
                    d += anchor_pose(get(c.anchors[k].body), c.anchors[k]).p * T(c.weights[k]);
                    center += anchor_pose(get(c.normal_anchors[k].body), c.normal_anchors[k]).p *
                              T(c.weights[k]);
                }
                T gap = dot(center, n) - T(c.normal_offset);
                auto t = d - Vec<T>(c.tangent_origin);
                t = t - n * dot(t, n);
                return normal_al(gap, c.normal_multiplier, penalty()) +
                       tangent_al(t, c.tangent_multiplier, penalty(),
                                  c.friction * c.normal_multiplier);
            });
        }
        for (const auto &joint : state.hinges)
            add(joint.target_static ? std::vector<std::size_t>{joint.a.body}
                                    : std::vector<std::size_t>{joint.a.body, joint.b.body},
                [&](auto get) {
                    auto a = anchor_pose(get(joint.a.body), joint.a);
                    using T = std::decay_t<decltype(a.q.w)>;
                    auto b = joint.target_static ? Pose<T>{Vec<T>(joint.static_point), Quat<T>{}}
                                                 : anchor_pose(get(joint.b.body), joint.b);
                    auto position = a.p - b.p, axis = cross(a.q.rotate(Vec<T>(joint.axis_a)),
                                                            b.q.rotate(Vec<T>(joint.axis_b)));
                    return T(0.5 * penalty()) * (dot(position, position) + dot(axis, axis)) -
                           dot(Vec<T>(joint.position_multiplier), position) -
                           dot(Vec<T>(joint.axis_multiplier), axis);
                });
    }
    return blocks;
}
std::vector<double> System::hessian_action(const std::vector<double> &x, double h,
                                           const std::vector<double> &direction) const {
    if (x.size() != direction.size())
        throw std::invalid_argument("Hessian direction length");
    std::vector<double> out(x.size(), 0);
    for (const auto &block : hessian_blocks(x, h)) {
        unsigned size = block.count * 6;
        for (unsigned j = 0; j < size; ++j)
            for (unsigned k = 0; k < size; ++k)
                out[block.ids[j / 6] * 6 + j % 6] +=
                    block.matrix[j * 24 + k] * direction[block.ids[k / 6] * 6 + k % 6];
    }
    return out;
}
bool System::minimize(double h, StepResult &stats, std::vector<double> &x) {
    const std::size_t n = state.bodies.size() * 6;
    std::vector<double> g(n), pre(n), direction(n), r(n), z(n), p(n), hp(n), trial(n), gp(n), gm(n);
    for (std::size_t i = 0; i < state.bodies.size(); ++i) {
        const auto &b = state.bodies[i];
        for (std::size_t j = 0; j < 3; ++j) {
            pre[6 * i + j] = h > 0 ? b.mass / (h * h) : 0;
            pre[6 * i + 3 + j] =
                h > 0 ? std::min({b.inertia.x, b.inertia.y, b.inertia.z}) / (h * h) : 0;
        }
    }
    for (const auto &e : state.elements) {
        const double area = pi * e.radius0 * e.radius0;
        for (auto id : {e.a.body, e.b.body})
            for (std::size_t j = 0; j < 3; ++j) {
                pre[6 * id + j] += material.young * area / e.ds;
                pre[6 * id + 3 + j] += material.young * area * e.ds;
            }
    }
    if (coupled_constraints) {
        for (const auto &c : state.contacts)
            for (unsigned k = 0; k < c.count; ++k)
                for (unsigned j = 0; j < 3; ++j) {
                    pre[c.anchors[k].body * 6 + j] += penalty() * c.weights[k] * c.weights[k];
                    pre[c.anchors[k].body * 6 + 3 + j] +=
                        penalty() * dot(c.anchors[k].local, c.anchors[k].local) * c.weights[k] *
                        c.weights[k];
                }
        for (const auto &c : state.cups)
            if (c.phase == CupPhase::Attached || c.phase == CupPhase::Peeling) {
                for (auto a : {c.a, c.target_static ? c.a : c.b})
                    for (unsigned j = 0; j < 3; ++j) {
                        pre[a.body * 6 + j] += penalty();
                        pre[a.body * 6 + 3 + j] += penalty() * dot(a.local, a.local);
                    }
            }
    }
    auto project = [&](std::vector<double> &v) {
        for (std::size_t i = 0; i < state.bodies.size(); ++i)
            if (state.bodies[i].fixed)
                for (std::size_t j = 0; j < 6; ++j)
                    v[6 * i + j] = 0;
    };
    double energy = objective(x, h, &g);
    project(g);
    double initial = std::max(maxabs(g), 1.0);
    std::vector<HessianBlock> blocks;
    auto hessian = [&](const std::vector<double> &v, std::vector<double> &out) {
        if (solver.exact_hessian && coupled_constraints) {
            out.assign(n, 0);
            for (const auto &block : blocks) {
                unsigned size = block.count * 6;
                for (unsigned j = 0; j < size; ++j) {
                    double sum = 0;
                    for (unsigned k = 0; k < size; ++k)
                        sum += block.matrix[j * 24 + k] * v[block.ids[k / 6] * 6 + k % 6];
                    out[block.ids[j / 6] * 6 + j % 6] += sum;
                }
            }
            project(out);
            return;
        }
        const double length = std::sqrt(inner(v, v));
        if (length < 1e-30) {
            out.assign(n, 0);
            return;
        }
        const double eps = solver.fd_epsilon / length;
        for (std::size_t j = 0; j < n; ++j)
            trial[j] = x[j] + eps * v[j];
        objective(trial, h, &gp);
        for (std::size_t j = 0; j < n; ++j)
            trial[j] = x[j] - eps * v[j];
        objective(trial, h, &gm);
        for (std::size_t j = 0; j < n; ++j)
            out[j] = (gp[j] - gm[j]) / (2 * eps);
        project(out);
    };
    bool success = false;
    for (unsigned it = 0; it <= solver.max_newton; ++it) {
        stats.residual = maxabs(g);
        stats.iterations++;
        if (stats.residual < solver.force_tolerance + solver.relative_tolerance * initial) {
            success = true;
            break;
        }
        if (it == solver.max_newton || !std::isfinite(energy))
            break;
        if (solver.exact_hessian && coupled_constraints) {
            blocks = hessian_blocks(x, h);
            std::fill(pre.begin(), pre.end(), 0);
            for (const auto &block : blocks)
                for (unsigned j = 0; j < block.count * 6; ++j)
                    pre[block.ids[j / 6] * 6 + j % 6] +=
                        std::max(std::abs(block.matrix[j * 24 + j]), 1e-9);
        }
        std::fill(direction.begin(), direction.end(), 0);
        for (std::size_t j = 0; j < n; ++j) {
            r[j] = -g[j];
            z[j] = r[j] / std::max(pre[j], 1e-9);
            p[j] = z[j];
        }
        double rz = inner(r, z);
        const double threshold = std::max(1e-24, rz * 1e-8);
        for (unsigned k = 0; k < solver.max_cg && rz > threshold; ++k) {
            hessian(p, hp);
            const double php = inner(p, hp);
            if (php <= 1e-20) {
                if (k == 0)
                    direction = z;
                break;
            }
            const double alpha = rz / php;
            for (std::size_t j = 0; j < n; ++j) {
                direction[j] += alpha * p[j];
                r[j] -= alpha * hp[j];
                z[j] = r[j] / std::max(pre[j], 1e-9);
            }
            double next = inner(r, z), beta = next / rz;
            for (std::size_t j = 0; j < n; ++j)
                p[j] = z[j] + beta * p[j];
            rz = next;
        }
        double slope = inner(g, direction);
        if (slope >= 0) {
            for (std::size_t j = 0; j < n; ++j)
                direction[j] = -g[j] / std::max(pre[j], 1e-9);
            slope = inner(g, direction);
        }
        bool accepted = false;
        double alpha = 1;
        for (unsigned ls = 0; ls < solver.max_line_search; ++ls) {
            for (std::size_t j = 0; j < n; ++j)
                trial[j] = x[j] + alpha * direction[j];
            const double candidate = objective(trial, h);
            bool accept = std::isfinite(candidate) && candidate <= energy + 1e-4 * alpha * slope;
            if (!accept && std::isfinite(candidate) &&
                std::abs(candidate - energy) <= 128 * std::numeric_limits<double>::epsilon() *
                                                    std::max(1.0, std::abs(energy))) {
                objective(trial, h, &gp);
                project(gp);
                accept = maxabs(gp) < maxabs(g);
            }
            if (accept) {
                x = trial;
                energy = objective(x, h, &g);
                project(g);
                accepted = true;
                break;
            }
            alpha *= 0.5;
        }
        if (!accepted)
            break;
    }
    if (!success)
        return false;
    return true;
}
void System::commit_increment(const std::vector<double> &x, double h, StepResult &stats) {
    std::vector<double> g;
    objective(x, h, &g);
    stats.support_force = {};
    stats.support_torque = {};
    for (std::size_t i = 0; i < state.bodies.size(); ++i) {
        auto &b = state.bodies[i];
        const V3 dp{x[6 * i], x[6 * i + 1], x[6 * i + 2]},
            dr{x[6 * i + 3], x[6 * i + 4], x[6 * i + 5]};
        if (b.fixed) {
            V3 force{g[6 * i], g[6 * i + 1], g[6 * i + 2]},
                torque{g[6 * i + 3], g[6 * i + 4], g[6 * i + 5]};
            stats.support_force += force;
            stats.support_torque += torque + cross(b.p, force);
            continue;
        }
        b.p += dp;
        b.q = normalized(exp(dr) * b.q);
        b.v = h > 0 ? dp / h : V3{};
        b.w = h > 0 ? dr / h : V3{};
        if (!finite(b.p) || !finite(b.v) || !finite(b.w))
            throw std::runtime_error("nonfinite committed state");
    }
    update_geometry();
}
bool System::solve_substep(double h, StepResult &stats) {
    if (coupled_constraints && h > 0)
        return solve_coupled(h, stats);
    if (h > 0)
        prepare(h);
    std::vector<double> x(state.bodies.size() * 6, 0);
    if (!minimize(h, stats, x))
        return false;
    commit_increment(x, h, stats);
    return true;
}
StepResult System::equilibrate() {
    const auto before = state;
    StepResult result;
    result.converged = solve_substep(0, result);
    if (!result.converged) {
        state = before;
        result.failure = "Static Newton solve failed; state rolled back";
    }
    result.energy = elastic_energy();
    result.volume_error = volume_error();
    return result;
}
void System::update_geometry() {
    for (auto &e : state.elements) {
        const auto &a = state.bodies[e.a.body];
        const auto &b = state.bodies[e.b.body];
        auto [nu, k] = strain(anchor_pose(Pose<double>{a.p, a.q}, e.a),
                              anchor_pose(Pose<double>{b.p, b.q}, e.b), e.ds);
        (void)k;
        e.stretch = nu.z;
        e.radius = e.radius0 / std::sqrt(nu.z);
    }
}
StepResult System::step(double h) {
    if (h != octo::fixed_dt || !std::isfinite(h))
        throw std::invalid_argument("fixed tick must be 1/120 s");
    material.validate();
    if (!std::isfinite(solver.fd_epsilon) || solver.fd_epsilon <= 0 ||
        !std::isfinite(solver.force_tolerance) || solver.force_tolerance <= 0 ||
        !std::isfinite(solver.relative_tolerance) || solver.relative_tolerance < 0 ||
        solver.max_cg == 0 || solver.max_line_search == 0)
        throw std::invalid_argument("invalid solver settings");
    const auto before = state;
    StepResult result;
    const auto start = std::chrono::steady_clock::now();
    for (unsigned splits : {1u, 2u, 4u}) {
        state = before;
        result.substeps = splits;
        bool ok = true;
        try {
            for (unsigned j = 0; j < splits; ++j)
                if (!solve_substep(h / splits, result)) {
                    ok = false;
                    break;
                }
        } catch (...) {
            frozen_contacts_ = false;
            active_penalty_ = 0;
            state = before;
            throw;
        }
        if (ok) {
            result.converged = true;
            result.failure.clear();
            ++state.tick;
            break;
        }
    }
    if (!result.converged) {
        state = before;
        result.failure = "1/2/4 subdivision exhausted; state rolled back: " + result.failure;
    }
    active_penalty_ = 0;
    result.volume_error = volume_error();
    result.energy = elastic_energy();
    contact_metrics(result);
    result.cpu_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}
void System::contact_metrics(StepResult &result) const {
    result.contacts = 0;
    result.penetration = 0;
    if (coupled_constraints) {
        for (const auto &c : state.contacts) {
            V3 d = -c.normal_fixed_point;
            for (unsigned j = 0; j < c.count; ++j)
                d += point(c.normal_anchors[j]) * c.weights[j];
            double gap = dot(d, c.normal) - c.normal_offset;
            if (c.normal_multiplier > 1e-8 || gap < 0)
                ++result.contacts;
            result.penetration = std::max(result.penetration, std::max(-gap, 0.0));
        }
        return;
    }
    auto add = [&](double gap) {
        if (gap < 0) {
            ++result.contacts;
            result.penetration = std::max(result.penetration, -gap);
        }
    };
    for (std::size_t i = 0; i < state.bodies.size(); ++i) {
        const auto &a = state.bodies[i];
        for (const auto &plane : planes) {
            double extent = a.radius;
            if (a.box) {
                auto n = a.q.conjugate().rotate(plane.normal);
                extent = std::abs(n.x) * a.half_extents.x + std::abs(n.y) * a.half_extents.y +
                         std::abs(n.z) * a.half_extents.z;
            }
            add(dot(a.p, plane.normal) - plane.offset - extent);
        }
        if (!a.box) {
            auto hit = terrain.closest(a.p, a.radius);
            if (hit.found)
                add(hit.distance - a.radius);
        }
        if (!self_contact)
            continue;
        for (std::size_t j = i + 1; j < state.bodies.size(); ++j) {
            if (excluded_pair(state, i, j))
                continue;
            const auto &b = state.bodies[j];
            double bound = (a.box ? norm(a.half_extents) : a.radius) +
                           (b.box ? norm(b.half_extents) : b.radius);
            if (dot(a.p - b.p, a.p - b.p) > bound * bound)
                continue;
            if (!a.box && !b.box && norm(a.p - b.p) < 1e-12) {
                add(-bound);
                continue;
            }
            add(body_gap(Pose<double>{a.p, a.q}, Pose<double>{b.p, b.q}, a, b));
        }
    }
}
double System::elastic_energy() const {
    return objective(std::vector<double>(state.bodies.size() * 6, 0), 0);
}
std::vector<double> System::elastic_gradient() const {
    std::vector<double> g;
    objective(std::vector<double>(state.bodies.size() * 6, 0), 0, &g);
    return g;
}
V3 System::linear_momentum() const {
    V3 p;
    for (const auto &b : state.bodies)
        p += b.v * b.mass;
    return p;
}
V3 System::angular_momentum() const {
    V3 l;
    for (const auto &b : state.bodies) {
        auto w = b.q.conjugate().rotate(b.w);
        l += cross(b.p, b.v * b.mass) + b.q.rotate(inertia_times(b, w));
    }
    return l;
}
double System::volume_error() const {
    double v = 0, v0 = 0;
    for (const auto &e : state.elements) {
        double base = pi * e.radius0 * e.radius0 * e.ds;
        v0 += base;
        v += pi * e.radius * e.radius * e.stretch * e.ds;
    }
    return v0 > 0 ? std::abs(v - v0) / v0 : 0;
}
} // namespace octo::dynamics
