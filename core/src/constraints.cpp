#include "octo/dynamics.hpp"
#include <algorithm>
#include <functional>
#include <map>

namespace octo::dynamics {
using namespace math;
namespace {
constexpr std::size_t world_id = std::numeric_limits<std::size_t>::max();
std::vector<Body> displaced(const State &s, const std::vector<double> &x, double t = 1) {
    auto bodies = s.bodies;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        auto &b = bodies[i];
        b.p += V3{x[6 * i], x[6 * i + 1], x[6 * i + 2]} * t;
        b.q = normalized(exp(V3{x[6 * i + 3], x[6 * i + 4], x[6 * i + 5]} * t) * b.q);
    }
    return bodies;
}
V3 at(const std::vector<Body> &b, Anchor a) { return b.at(a.body).p + b[a.body].q.rotate(a.local); }
Anchor surface_anchor(const std::vector<Body> &b, std::size_t id, V3 p) {
    return {id, b[id].q.conjugate().rotate(p - b[id].p), Q{}};
}
V3 delta(const ContactConstraint &c, const std::vector<Body> &b) {
    V3 result = -c.fixed_point;
    for (unsigned j = 0; j < c.count; ++j)
        result += at(b, c.anchors[j]) * c.weights[j];
    return result;
}
double normal_gap(const ContactConstraint &c, const std::vector<Body> &b) {
    V3 d = -c.normal_fixed_point;
    for (unsigned j = 0; j < c.count; ++j)
        d += at(b, c.normal_anchors[j]) * c.weights[j];
    return dot(d, c.normal) - c.normal_offset;
}
collision::BoxBounds bounds(V3 a, V3 b, double r, std::size_t id) {
    collision::BoxBounds box;
    box.id = id;
    for (std::size_t j = 0; j < 3; ++j) {
        box.low[j] = std::min(a[j], b[j]) - r;
        box.high[j] = std::max(a[j], b[j]) + r;
    }
    return box;
}
void include(collision::BoxBounds &box, V3 p, double r) {
    for (std::size_t j = 0; j < 3; ++j) {
        box.low[j] = std::min(box.low[j], p[j] - r);
        box.high[j] = std::max(box.high[j], p[j] + r);
    }
}
struct Surface {
    V3 point{}, normal{};
    double gap = 0;
};
Surface box_surface(V3 p, const Body &b) {
    V3 q = b.q.conjugate().rotate(p - b.p), cp;
    bool outside = false;
    double best = -1e300;
    std::size_t face = 0;
    for (std::size_t j = 0; j < 3; ++j) {
        cp[j] = std::clamp(q[j], -b.half_extents[j], b.half_extents[j]);
        outside |= cp[j] != q[j];
        double d = std::abs(q[j]) - b.half_extents[j];
        if (d > best) {
            best = d;
            face = j;
        }
    }
    V3 n{};
    double gap;
    if (outside) {
        gap = norm(q - cp);
        n = (q - cp) / gap;
    } else {
        n[face] = q[face] >= 0 ? 1 : -1;
        cp = q;
        cp[face] = n[face] * b.half_extents[face];
        gap = best;
    }
    return {b.p + b.q.rotate(cp), b.q.rotate(n), gap};
}
V3 safe_normal(V3 d, V3 fallback = {0, 1, 0}) {
    double n = norm(d);
    return n > 1e-10 ? d / n : fallback;
}
// Triangle/OBB separating-axis test (three box axes, face normal, nine edge axes).
Surface box_triangle(const Body &b, const collision::Triangle &t) {
    auto local = [&](V3 p) { return b.q.conjugate().rotate(p - b.p); };
    V3 a = local(t.a), c = local(t.b), d = local(t.c);
    double best = -1e300;
    V3 bestn{0, 1, 0};
    auto axis = [&](V3 n) {
        double length = norm(n);
        if (length < 1e-12)
            return;
        n = n / length;
        double lo = std::min({dot(a, n), dot(c, n), dot(d, n)}),
               hi = std::max({dot(a, n), dot(c, n), dot(d, n)});
        double r = std::abs(n.x) * b.half_extents.x + std::abs(n.y) * b.half_extents.y +
                   std::abs(n.z) * b.half_extents.z;
        double gap = lo - r;
        V3 direction = -n;
        if (-hi - r > gap) {
            gap = -hi - r;
            direction = n;
        }
        if (gap > best) {
            best = gap;
            bestn = direction;
        }
    };
    std::array<V3, 3> axes{V3{1, 0, 0}, V3{0, 1, 0}, V3{0, 0, 1}};
    for (auto n : axes)
        axis(n);
    axis(cross(c - a, d - a));
    for (auto edge : {c - a, d - c, a - d})
        for (auto n : axes)
            axis(cross(edge, n));
    V3 n = b.q.rotate(bestn), q = b.q.conjugate().rotate(n);
    double support = std::abs(q.x) * b.half_extents.x + std::abs(q.y) * b.half_extents.y +
                     std::abs(q.z) * b.half_extents.z;
    return {b.p - n * (support + best), n, best};
}
double skin_radius(const Element &e, const std::vector<Body> &b, double minimum) {
    auto a = b[e.a.body].q * e.a.rotation, z = b[e.b.body].q * e.b.rotation;
    double stretch = midpoint(a, z).conjugate().rotate(at(b, e.b) - at(b, e.a)).z / e.ds;
    return e.radius0 / std::sqrt(std::max(stretch, minimum));
}
// Distance is Lipschitz under bounded endpoint translation and angular sweep.
double conservative_toi(const std::function<double(double)> &distance, double speed) {
    if (speed < 1e-12)
        return 1;
    double t = 0;
    for (unsigned j = 0; j < 512 && t < 1; ++j) {
        double d = distance(t);
        if (d <= 1e-7)
            return t;
        t += std::max(0.8 * d / speed, 1e-8);
    }
    return t < 1 ? t : 1;
}
} // namespace
std::vector<ContactConstraint> System::build_contacts(const std::vector<double> &x, double h,
                                                      unsigned *ccd) const {
    const auto b = displaced(state, x);
    const std::size_t nb = b.size(), ne = state.elements.size();
    std::vector<ContactConstraint> result;
    std::map<std::uint64_t, const ContactConstraint *> old;
    for (const auto &c : state.contacts)
        old[c.key] = &c;
    auto add = [&](ContactConstraint c) {
        const double gap = normal_gap(c, b);
        if (gap > solver.contact_margin)
            return;
        c.tangent_origin = delta(c, state.bodies);
        auto prior = old.find(c.key);
        if (prior != old.end() && dot(c.normal, prior->second->normal) > 0.99) {
            c.normal_multiplier = prior->second->normal_multiplier;
            c.tangent_multiplier = prior->second->tangent_multiplier -
                                   c.normal * dot(prior->second->tangent_multiplier, c.normal);
            // Retain a material stick patch until it separates; slip is projected below.
            bool same_weights = c.count == prior->second->count;
            for (unsigned j = 0; j < c.count && same_weights; ++j)
                same_weights = std::abs(c.weights[j] - prior->second->weights[j]) < 1e-6;
            if (same_weights &&
                std::abs(normal_gap(*prior->second, b)) < solver.contact_margin * 2) {
                auto geometric = c;
                c = *prior->second;
                c.normal_anchors = geometric.normal_anchors;
                c.normal_fixed_point = geometric.normal_fixed_point;
                c.normal_offset = geometric.normal_offset;
                c.normal = geometric.normal;
                c.tangent_multiplier =
                    c.tangent_multiplier - c.normal * dot(c.tangent_multiplier, c.normal);
            }
        } else
            c.tangent_origin = delta(c, state.bodies);
        result.push_back(c);
    };
    auto point_contact = [&](std::uint64_t key, std::size_t a, V3 pa, V3 n, std::size_t target,
                             V3 pb, double friction) {
        ContactConstraint c;
        c.key = key;
        c.count = target == world_id ? 1 : 2;
        c.anchors[0] = surface_anchor(b, a, pa);
        c.weights[0] = 1;
        c.normal = n;
        c.friction = friction;
        if (target == world_id)
            c.fixed_point = pb;
        else {
            c.anchors[1] = surface_anchor(b, target, pb);
            c.weights[1] = -1;
        }
        c.normal_anchors = c.anchors;
        c.normal_fixed_point = c.fixed_point;
        if (!b[a].box) {
            c.normal_anchors[0] = {a, {}, Q{}};
            c.normal_offset = b[a].radius;
        }
        if (target != world_id && !b[target].box) {
            c.normal_anchors[1] = {target, {}, Q{}};
            c.normal_offset += b[target].radius;
        }
        add(c);
    };
    auto capsule_contact = [&](std::uint64_t key, const Element &e, double u, V3 n, double radius,
                               std::size_t target, V3 target_point, double friction) {
        ContactConstraint c;
        c.key = key;
        c.count = target == world_id ? 2 : 3;
        c.normal = n;
        c.friction = friction;
        for (unsigned j = 0; j < 2; ++j) {
            auto anchor = j == 0 ? e.a : e.b;
            V3 p = at(b, anchor) - n * radius;
            c.anchors[j] = surface_anchor(b, anchor.body, p);
            c.weights[j] = j == 0 ? 1 - u : u;
        }
        if (target == world_id)
            c.fixed_point = target_point;
        else {
            c.anchors[2] = surface_anchor(b, target, target_point);
            c.weights[2] = -1;
        }
        c.normal_anchors = c.anchors;
        c.normal_anchors[0] = e.a;
        c.normal_anchors[1] = e.b;
        c.normal_offset = radius;
        c.normal_fixed_point = c.fixed_point;
        if (target != world_id && !b[target].box) {
            c.normal_anchors[2] = {target, {}, Q{}};
            c.normal_offset += b[target].radius;
        }
        add(c);
    };
    for (std::size_t i = 0; i < nb; ++i) {
        if (b[i].arm >= 0 || b[i].fixed)
            continue;
        for (std::size_t k = 0; k < planes.size(); ++k) {
            const auto &p = planes[k];
            if (!b[i].box) {
                V3 n = p.normal, pa = b[i].p - n * b[i].radius,
                   pb = pa - n * (dot(pa, n) - p.offset);
                point_contact((1ull << 60) | (static_cast<std::uint64_t>(i) << 32) | k, i, pa, n,
                              world_id, pb, p.friction);
            } else
                for (unsigned corner = 0; corner < 8; ++corner) {
                    V3 local{(corner & 1 ? 1 : -1) * b[i].half_extents.x,
                             (corner & 2 ? 1 : -1) * b[i].half_extents.y,
                             (corner & 4 ? 1 : -1) * b[i].half_extents.z};
                    V3 pa = b[i].p + b[i].q.rotate(local),
                       pb = pa - p.normal * (dot(pa, p.normal) - p.offset);
                    point_contact((2ull << 60) | (static_cast<std::uint64_t>(i) << 32) | (k << 8) |
                                      corner,
                                  i, pa, p.normal, world_id, pb, p.friction);
                }
        }
        if (!terrain.triangles().empty()) {
            double r = b[i].box ? norm(b[i].half_extents) : b[i].radius;
            auto query = bounds(b[i].p, state.bodies[i].p, r + solver.contact_margin, 0);
            for (auto ti : terrain.query(query)) {
                const auto &t = terrain.triangles()[ti];
                auto dist = [&](double f) {
                    V3 p = state.bodies[i].p + (b[i].p - state.bodies[i].p) * f;
                    return norm(p - collision::closest_triangle(p, t)) - r;
                };
                double toi = conservative_toi(dist, norm(b[i].p - state.bodies[i].p));
                V3 oldp = state.bodies[i].p, cp = collision::closest_triangle(b[i].p, t);
                V3 n = safe_normal(oldp - collision::closest_triangle(oldp, t),
                                   safe_normal(cross(t.b - t.a, t.c - t.a)));
                if (toi < 1 && ccd)
                    ++*ccd;
                double support = b[i].radius;
                if (b[i].box) {
                    auto hit = box_triangle(b[i], t);
                    if (hit.gap > solver.contact_margin)
                        continue;
                    n = hit.normal;
                    cp = hit.point;
                    auto local = b[i].q.conjugate().rotate(n);
                    support = std::abs(local.x) * b[i].half_extents.x +
                              std::abs(local.y) * b[i].half_extents.y +
                              std::abs(local.z) * b[i].half_extents.z;
                }
                point_contact((3ull << 60) | (static_cast<std::uint64_t>(i) << 32) | t.id, i,
                              b[i].p - n * support, n, world_id, cp, t.friction);
            }
        }
    }
    for (std::size_t i = 0; i < ne; ++i) {
        const auto &e = state.elements[i];
        const V3 a = at(b, e.a), z = at(b, e.b);
        const double r = skin_radius(e, b, material.min_stretch);
        for (std::size_t k = 0; k < planes.size(); ++k) {
            const auto &p = planes[k];
            for (unsigned end = 0; end < 2; ++end) {
                V3 center = end ? z : a, pa = center - p.normal * r,
                   pb = pa - p.normal * (dot(pa, p.normal) - p.offset);
                capsule_contact((4ull << 60) | (static_cast<std::uint64_t>(i) << 32) | (k << 8) |
                                    end,
                                e, double(end), p.normal, r, world_id, pb, p.friction);
            }
        }
        auto swept = bounds(a, z, r + solver.contact_margin, 0);
        include(swept, point(e.a), r);
        include(swept, point(e.b), r);
        for (auto ti : terrain.query(swept)) {
            const auto &t = terrain.triangles()[ti];
            auto hit = collision::closest_segment_triangle(a, z, t);
            auto oldhit = collision::closest_segment_triangle(point(e.a), point(e.b), t);
            const double speed = std::max(norm(a - point(e.a)), norm(z - point(e.b)));
            auto dist = [&](double f) {
                return collision::closest_segment_triangle(point(e.a) + (a - point(e.a)) * f,
                                                           point(e.b) + (z - point(e.b)) * f, t)
                           .distance -
                       r;
            };
            double toi = conservative_toi(dist, speed);
            if (hit.distance > r + solver.contact_margin && toi >= 1)
                continue;
            if (toi < 1 && ccd)
                ++*ccd;
            V3 n = safe_normal(oldhit.a - oldhit.b, safe_normal(cross(t.b - t.a, t.c - t.a)));
            if (toi >= 1)
                n = safe_normal(hit.a - hit.b, n);
            for (unsigned end = 0; end < 2; ++end) {
                V3 center = end ? z : a;
                V3 cp = collision::closest_triangle(center, t);
                V3 oldpoint = end ? point(e.b) : point(e.a);
                double speed_point = norm(center - oldpoint);
                auto point_distance = [&](double f) {
                    V3 p = oldpoint + (center - oldpoint) * f;
                    return norm(p - collision::closest_triangle(p, t)) - r;
                };
                double time = conservative_toi(point_distance, speed_point);
                if (norm(center - cp) <= r + solver.contact_margin || time < 1) {
                    V3 normal = safe_normal(oldpoint - collision::closest_triangle(oldpoint, t), n);
                    capsule_contact((5ull << 60) | (static_cast<std::uint64_t>(i) << 34) |
                                        (static_cast<std::uint64_t>(t.id) << 2) | end,
                                    e, double(end), normal, r, world_id, cp, t.friction);
                }
            }
            if (hit.u > 1e-5 && hit.u < 1 - 1e-5)
                capsule_contact((5ull << 60) | (static_cast<std::uint64_t>(i) << 34) |
                                    (static_cast<std::uint64_t>(t.id) << 2) | 2,
                                e, hit.u, n, r, world_id, hit.b, t.friction);
        }
    }
    std::vector<collision::BoxBounds> proxies;
    for (std::size_t i = 0; i < nb; ++i)
        if (b[i].arm < 0) {
            double r = b[i].box ? norm(b[i].half_extents) : b[i].radius;
            auto box = bounds(b[i].p, state.bodies[i].p, r + solver.contact_margin, i);
            include(box, state.bodies[i].p + state.bodies[i].v * h, r);
            proxies.push_back(box);
        }
    for (std::size_t i = 0; i < ne; ++i) {
        const auto &e = state.elements[i];
        double r = std::max(e.radius, skin_radius(e, b, material.min_stretch));
        auto box = bounds(at(b, e.a), at(b, e.b), r + solver.contact_margin, nb + i);
        include(box, point(e.a), r);
        include(box, point(e.b), r);
        include(box, point(e.a) + state.bodies[e.a.body].v * h, r);
        include(box, point(e.b) + state.bodies[e.b.body].v * h, r);
        double angular =
            std::max(norm(e.a.local) *
                         norm(V3{x[6 * e.a.body + 3], x[6 * e.a.body + 4], x[6 * e.a.body + 5]}),
                     norm(e.b.local) *
                         norm(V3{x[6 * e.b.body + 3], x[6 * e.b.body + 4], x[6 * e.b.body + 5]}));
        for (unsigned k = 0; k < 3; ++k) {
            box.low[k] -= angular;
            box.high[k] += angular;
        }
        proxies.push_back(box);
    }
    collision::Broadphase tree;
    tree.build(std::move(proxies));
    for (auto [i, j] : tree.pairs()) {
        if (i < nb && j < nb) {
            if (b[i].fixed && b[j].fixed)
                continue;
            if (b[i].box && b[j].box) {
                // SAT face/edge normals; feature is the deterministic winning axis.
                std::array<V3, 3> aa{b[i].q.rotate({1, 0, 0}), b[i].q.rotate({0, 1, 0}),
                                     b[i].q.rotate({0, 0, 1})},
                    bb{b[j].q.rotate({1, 0, 0}), b[j].q.rotate({0, 1, 0}),
                       b[j].q.rotate({0, 0, 1})};
                double best = -1e300;
                V3 normal{};
                unsigned feature = 0, id = 0;
                auto test = [&](V3 n) {
                    unsigned fid = id++;
                    if (norm(n) < 1e-10)
                        return;
                    n = n / norm(n);
                    double extent = 0;
                    for (unsigned k = 0; k < 3; ++k)
                        extent += std::abs(dot(n, aa[k])) * b[i].half_extents[k] +
                                  std::abs(dot(n, bb[k])) * b[j].half_extents[k];
                    double gap = std::abs(dot(b[i].p - b[j].p, n)) - extent;
                    if (gap > best) {
                        best = gap;
                        normal = dot(b[i].p - b[j].p, n) >= 0 ? n : -n;
                        feature = fid;
                    }
                };
                for (auto n : aa)
                    test(n);
                for (auto n : bb)
                    test(n);
                for (auto a : aa)
                    for (auto z : bb)
                        test(cross(a, z));
                if (best > solver.contact_margin)
                    continue;
                auto support = [&](std::size_t body, V3 n) {
                    auto local = b[body].q.conjugate().rotate(n);
                    for (unsigned k = 0; k < 3; ++k)
                        local[k] = std::abs(local[k]) < 1e-9
                                       ? 0
                                       : (local[k] > 0 ? 1 : -1) * b[body].half_extents[k];
                    return b[body].p + b[body].q.rotate(local);
                };
                point_contact((6ull << 60) | (static_cast<std::uint64_t>(i) << 36) | (j << 8) |
                                  feature,
                              i, support(i, -normal), normal, j, support(j, normal), 0.7);
            } else {
                std::size_t sphere = b[i].box ? j : i, target = b[i].box ? i : j;
                Surface s;
                if (b[target].box)
                    s = box_surface(b[sphere].p, b[target]);
                else {
                    V3 n = safe_normal(b[sphere].p - b[target].p);
                    s = {b[target].p + n * b[target].radius, n,
                         norm(b[sphere].p - b[target].p) - b[target].radius};
                }
                double angle = norm(V3{x[6 * target + 3], x[6 * target + 4], x[6 * target + 5]});
                double speed = norm(b[sphere].p - state.bodies[sphere].p) +
                               norm(b[target].p - state.bodies[target].p) +
                               angle * norm(b[target].half_extents);
                if (speed > b[sphere].radius * 0.5) {
                    auto distance = [&](double t) {
                        auto moving = displaced(state, x, t);
                        return (moving[target].box
                                    ? box_surface(moving[sphere].p, moving[target]).gap
                                    : norm(moving[sphere].p - moving[target].p) -
                                          moving[target].radius) -
                               moving[sphere].radius;
                    };
                    double toi = conservative_toi(distance, speed);
                    if (toi < 1) {
                        auto impact = displaced(state, x, toi);
                        if (impact[target].box) {
                            auto hit = box_surface(impact[sphere].p, impact[target]);
                            auto local =
                                impact[target].q.conjugate().rotate(hit.point - impact[target].p);
                            s = {
                                b[target].p + b[target].q.rotate(local),
                                b[target].q.rotate(impact[target].q.conjugate().rotate(hit.normal)),
                                0};
                        } else {
                            s.normal = safe_normal(impact[sphere].p - impact[target].p);
                            s.point = b[target].p + s.normal * b[target].radius;
                        }
                        if (ccd)
                            ++*ccd;
                    }
                }
                point_contact((7ull << 60) | (static_cast<std::uint64_t>(sphere) << 32) | target,
                              sphere, b[sphere].p - s.normal * b[sphere].radius, s.normal, target,
                              s.point, 0.7);
            }
        } else if (i < nb) {
            const auto &e = state.elements[j - nb];
            const double radius = skin_radius(e, b, material.min_stretch);
            if (e.a.body == i || e.b.body == i)
                continue;
            bool joint_overlap = false;
            for (const auto &root : state.elements)
                if (root.a.body == i && b[root.b.body].arm == b[e.b.body].arm) {
                    if (b[e.b.body].reference_s - e.ds < b[i].radius + radius)
                        joint_overlap = true;
                    break;
                }
            if (joint_overlap)
                continue;
            V3 a = at(b, e.a), z = at(b, e.b);
            double u = 0;
            if (b[i].box) {
                auto closest = [&](V3 pa, V3 pb, const Body &box) {
                    double lo = 0, hi = 1;
                    for (unsigned k = 0; k < 35; ++k) {
                        double p = (2 * lo + hi) / 3, q = (lo + 2 * hi) / 3;
                        if (box_surface(pa + (pb - pa) * p, box).gap <
                            box_surface(pa + (pb - pa) * q, box).gap)
                            hi = q;
                        else
                            lo = p;
                    }
                    return (lo + hi) / 2;
                };
                u = closest(a, z, b[i]);
                auto s = box_surface(a + (z - a) * u, b[i]);
                double speed =
                    std::max(norm(a - point(e.a)), norm(z - point(e.b))) +
                    norm(b[i].p - state.bodies[i].p) +
                    norm(V3{x[6 * i + 3], x[6 * i + 4], x[6 * i + 5]}) * norm(b[i].half_extents);
                if (speed > radius * 0.5) {
                    auto distance = [&](double t) {
                        auto moving = displaced(state, x, t);
                        auto pa = at(moving, e.a), pb = at(moving, e.b);
                        return box_surface(pa + (pb - pa) * closest(pa, pb, moving[i]), moving[i])
                                   .gap -
                               radius;
                    };
                    double toi = conservative_toi(distance, speed);
                    if (toi < 1) {
                        auto impact = displaced(state, x, toi);
                        V3 pa = at(impact, e.a), pb = at(impact, e.b);
                        u = closest(pa, pb, impact[i]);
                        auto hit = box_surface(pa + (pb - pa) * u, impact[i]);
                        auto local = impact[i].q.conjugate().rotate(hit.point - impact[i].p);
                        s = {b[i].p + b[i].q.rotate(local),
                             b[i].q.rotate(impact[i].q.conjugate().rotate(hit.normal)), 0};
                        if (ccd)
                            ++*ccd;
                    }
                }
                for (unsigned end = 0; end < 2; ++end) {
                    V3 center = end ? z : a;
                    auto hit = box_surface(center, b[i]);
                    // An endpoint can lie on a different box feature than the
                    // capsule interior. Its point and normal must come from
                    // the same query, even when a swept test is considered.
                    V3 normal = hit.normal;
                    V3 surface = hit.point;
                    if (speed > radius * 0.5) {
                        V3 old_center = end ? point(e.b) : point(e.a);
                        auto before = box_surface(old_center, state.bodies[i]);
                        if (dot(before.normal, s.normal) > 0.99) {
                            normal = s.normal;
                            surface = center - normal * (dot(center - s.point, normal));
                        }
                    } else
                        normal = hit.normal;
                    // The minimum and a nearby endpoint describe one contact
                    // patch, not two independent friction/compression constraints.
                    // Prefer the exact segment minimum; retain separated endpoints
                    // and different feature normals to support a real manifold.
                    if(u>1e-5 && u<1-1e-5 && norm(z-a)*std::abs(double(end)-u)<=solver.contact_margin && dot(normal,s.normal)>.99)
                        continue;
                    capsule_contact((8ull << 60) | (static_cast<std::uint64_t>((j - nb)) << 34) |
                                        (i << 2) | end,
                                    e, double(end), normal, radius, i, surface,
                                    std::sqrt(b[i].friction * 0.7));
                }
                if (u > 1e-5 && u < 1 - 1e-5)
                    capsule_contact(
                        (8ull << 60) | (static_cast<std::uint64_t>((j - nb)) << 34) | (i << 2) | 2,
                        e, u, s.normal, radius, i, s.point, std::sqrt(b[i].friction * 0.7));
            } else {
                auto d = z - a;
                u = std::clamp(dot(b[i].p - a, d) / std::max(dot(d, d), 1e-30), 0.0, 1.0);
                V3 center = a + d * u, n = safe_normal(center - b[i].p);
                capsule_contact((9ull << 60) | (static_cast<std::uint64_t>((j - nb)) << 32) | i, e,
                                u, n, radius, i, b[i].p + n * b[i].radius, 0.7);
            }
        } else if (self_contact) {
            const auto &ea = state.elements[i - nb];
            const auto &eb = state.elements[j - nb];
            const double ra = skin_radius(ea, b, material.min_stretch),
                         rb = skin_radius(eb, b, material.min_stretch);
            const auto &ba = b[ea.b.body];
            const auto &bb = b[eb.b.body];
            if (ea.a.body == eb.a.body || ea.a.body == eb.b.body || ea.b.body == eb.a.body ||
                ea.b.body == eb.b.body)
                continue;
            if (ba.arm == bb.arm &&
                (std::abs(ba.station - bb.station) <= 2 ||
                 std::abs(ba.reference_s - bb.reference_s) - std::max(ea.ds, eb.ds) < ra + rb))
                continue;
            V3 a = at(b, ea.a), z = at(b, ea.b), c = at(b, eb.a), d = at(b, eb.b);
            auto hit = collision::closest_segments(a, z, c, d);
            auto oldhit =
                collision::closest_segments(point(ea.a), point(ea.b), point(eb.a), point(eb.b));
            double speed = std::max(norm(a - point(ea.a)), norm(z - point(ea.b))) +
                           std::max(norm(c - point(eb.a)), norm(d - point(eb.b)));
            auto dist = [&](double t) {
                auto p = collision::closest_segments(
                    point(ea.a) + (a - point(ea.a)) * t, point(ea.b) + (z - point(ea.b)) * t,
                    point(eb.a) + (c - point(eb.a)) * t, point(eb.b) + (d - point(eb.b)) * t);
                return norm(p.a - p.b) - ra - rb;
            };
            double toi = conservative_toi(dist, speed);
            if (norm(hit.a - hit.b) > ra + rb + solver.contact_margin && toi >= 1)
                continue;
            if (toi < 1 && ccd)
                ++*ccd;
            V3 n = safe_normal(hit.a - hit.b,
                               safe_normal(oldhit.a - oldhit.b, safe_normal(cross(z - a, d - c))));
            // A resting/sliding pair starts inside the CCD distance epsilon.
            // TOI=0 there must not freeze yesterday's normal: trial radii can
            // toggle TOI=0/1 and produce a two-cycle in the coupled solve.
            // Keep the swept orientation only for a separated start or a
            // genuine crossing of the old separating plane.
            const auto old_delta = oldhit.a - oldhit.b;
            const auto new_delta = hit.a - hit.b;
            const std::uint64_t pair_key =
                (10ull << 60) | (static_cast<std::uint64_t>(i - nb) << 32) | (j - nb);
            const auto prior_pair = old.find(pair_key);
            const bool resting =
                prior_pair != old.end() && prior_pair->second->normal_multiplier > 0;
            if (toi < 1 && norm(old_delta) > 1e-9 &&
                ((!resting && norm(old_delta) > ea.radius + eb.radius + 1e-7) ||
                 dot(new_delta, old_delta) <= 0))
                n = safe_normal(old_delta);
            ContactConstraint constraint;
            constraint.key =
                (10ull << 60) | (static_cast<std::uint64_t>((i - nb)) << 32) | (j - nb);
            constraint.normal = n;
            constraint.count = 4;
            constraint.friction = 0.7;
            std::array<Anchor, 4> anchors{ea.a, ea.b, eb.a, eb.b};
            std::array<double, 4> w{1 - hit.u, hit.u, -(1 - hit.v), -hit.v};
            for (unsigned k = 0; k < 4; ++k) {
                V3 shift = n * (k < 2 ? -ra : rb);
                constraint.anchors[k] =
                    surface_anchor(b, anchors[k].body, at(b, anchors[k]) + shift);
                constraint.normal_anchors[k] = anchors[k];
                constraint.weights[k] = w[k];
            }
            constraint.normal_offset = ra + rb;
            add(constraint);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto &a, const auto &z) { return a.key < z.key; });
    return result;
}
void System::sample_cup(Cup &cup, const std::vector<double> &x) const {
    if (!cup.query_surface)
        return;
    auto bodies = displaced(state, x);
    const auto &body = bodies.at(cup.a.body);
    V3 center = at(bodies, cup.a);
    V3 axis = safe_normal(body.q.rotate(cup.a.rotation.rotate(cup.rim_axis)));
    V3 tangent = safe_normal(cross(axis, std::abs(axis.y) < 0.9 ? V3{0, 1, 0} : V3{1, 0, 0})),
       binormal = cross(axis, tangent);
    V3 normal_sum{};
    for (unsigned k = 0; k < 12; ++k) {
        V3 p =
            center + (tangent * std::cos(2 * pi * k / 12) + binormal * std::sin(2 * pi * k / 12)) *
                         cup.radius;
        if (!cup.target_static) {
            const auto &target = bodies.at(cup.b.body);
            auto hit = target.box
                           ? box_surface(p, target)
                           : Surface{target.p + safe_normal(p - target.p) * target.radius,
                                     safe_normal(p - target.p), norm(p - target.p) - target.radius};
            cup.gaps[k] = std::max(hit.gap, 0.0);
            cup.normal = hit.normal;
            cup.material = {target.friction, target.sealability, target.conductance};
        } else if (cup.plane >= 0) {
            const auto &plane = planes.at(static_cast<std::size_t>(cup.plane));
            cup.gaps[k] = std::max(dot(p, plane.normal) - plane.offset, 0.0);
            cup.normal = plane.normal;
            cup.material = {plane.friction, plane.sealability, plane.conductance};
            if (cup.phase != CupPhase::Attached && cup.phase != CupPhase::Peeling)
                cup.static_point =
                    center - plane.normal * (dot(center, plane.normal) - plane.offset);
        } else {
            auto hit = terrain.closest(p, cup.gap_reference * 10);
            cup.gaps[k] = hit.found ? std::max(dot(p - hit.point, hit.surface_normal), 0.0)
                                    : cup.gap_reference * 10;
            if (hit.found) {
                cup.normal = hit.surface_normal;
                cup.material = {hit.friction, hit.sealability, hit.conductance};
            }
        }
        normal_sum += cup.normal;
    }
    cup.normal = safe_normal(normal_sum, cup.normal);
    if (std::abs(dot(axis, cup.normal)) < 0.8)
        cup.gaps.fill(cup.gap_reference * 2);
    if (water_density > 0)
        cup.external_pressure =
            101325 + water_density * norm(gravity) * std::max(water_surface - center.y, 0.0);
}
} // namespace octo::dynamics
