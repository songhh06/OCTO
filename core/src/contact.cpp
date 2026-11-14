#include "octo/contact.hpp"
#include <algorithm>
#include <functional>
#include <numeric>
#include <set>

namespace octo::collision {
using namespace math;
SegmentPair closest_segments(V3 a, V3 b, V3 c, V3 d) {
    V3 x = b - a, y = d - c, r = a - c;
    double aa = dot(x, x), bb = dot(x, y), cc = dot(y, y), dd = dot(x, r), ee = dot(y, r), u = 0,
           v = 0;
    if (aa < 1e-24 && cc < 1e-24)
        return {0, 0, a, c};
    if (aa < 1e-24)
        v = std::clamp(ee / cc, 0.0, 1.0);
    else if (cc < 1e-24)
        u = std::clamp(-dd / aa, 0.0, 1.0);
    else {
        double denominator = aa * cc - bb * bb;
        if (denominator > 1e-20 * aa * cc)
            u = std::clamp((bb * ee - cc * dd) / denominator, 0.0, 1.0);
        v = (bb * u + ee) / cc;
        if (v < 0) {
            v = 0;
            u = std::clamp(-dd / aa, 0.0, 1.0);
        } else if (v > 1) {
            v = 1;
            u = std::clamp((bb - dd) / aa, 0.0, 1.0);
        }
    }
    return {u, v, a + x * u, c + y * v};
}
V3 closest_triangle(V3 p, const Triangle &t) {
    auto u = t.b - t.a, v = t.c - t.a, w = p - t.a;
    double uu = dot(u, u), uv = dot(u, v), vv = dot(v, v), wu = dot(w, u), wv = dot(w, v),
           det = uu * vv - uv * uv;
    V3 best = t.a;
    double distance = dot(p - best, p - best);
    if (det > 1e-24) {
        double s = (wu * vv - wv * uv) / det, q = (wv * uu - wu * uv) / det;
        if (s >= 0 && q >= 0 && s + q <= 1)
            return t.a + u * s + v * q;
    }
    for (auto edge : std::array<std::pair<V3, V3>, 3>{{{t.a, t.b}, {t.b, t.c}, {t.c, t.a}}}) {
        auto d = edge.second - edge.first;
        double len = dot(d, d);
        auto candidate =
            edge.first + d * (len > 0 ? std::clamp(dot(p - edge.first, d) / len, 0.0, 1.0) : 0);
        double d2 = dot(p - candidate, p - candidate);
        if (d2 < distance) {
            distance = d2;
            best = candidate;
        }
    }
    return best;
}
void TriangleBvh::build(std::vector<Triangle> triangles) {
    std::set<std::uint32_t> ids;
    for (auto &t : triangles) {
        if (!finite(t.a) || !finite(t.b) || !finite(t.c) ||
            norm(cross(t.b - t.a, t.c - t.a)) < 1e-12 || !ids.insert(t.id).second)
            throw std::invalid_argument("invalid triangle or duplicate feature ID");
    }
    triangles_ = std::move(triangles);
    order_.resize(triangles_.size());
    std::iota(order_.begin(), order_.end(), 0);
    nodes_.clear();
    if (!triangles_.empty())
        build_node(0, order_.size());
}
std::size_t TriangleBvh::build_node(std::size_t begin, std::size_t end) {
    Node n;
    n.begin = begin;
    n.end = end;
    n.low = {1e300, 1e300, 1e300};
    n.high = {-1e300, -1e300, -1e300};
    for (std::size_t i = begin; i < end; ++i)
        for (auto p : {triangles_[order_[i]].a, triangles_[order_[i]].b, triangles_[order_[i]].c})
            for (std::size_t j = 0; j < 3; ++j) {
                n.low[j] = std::min(n.low[j], p[j]);
                n.high[j] = std::max(n.high[j], p[j]);
            }
    const auto index = nodes_.size();
    nodes_.push_back(n);
    if (end - begin > 4) {
        auto extent = n.high - n.low;
        std::size_t axis = extent.x > extent.y ? 0 : 1;
        if (extent.z > extent[axis])
            axis = 2;
        std::stable_sort(
            order_.begin() + static_cast<std::ptrdiff_t>(begin),
            order_.begin() + static_cast<std::ptrdiff_t>(end), [&](std::size_t a, std::size_t b) {
                const auto &x = triangles_[a];
                const auto &y = triangles_[b];
                return (x.a[axis] + x.b[axis] + x.c[axis]) < (y.a[axis] + y.b[axis] + y.c[axis]);
            });
        auto middle = (begin + end) / 2;
        auto left = build_node(begin, middle), right = build_node(middle, end);
        nodes_[index].left = left;
        nodes_[index].right = right;
    }
    return index;
}
Hit TriangleBvh::closest(V3 p, double maximum_distance) const {
    if (!finite(p) || maximum_distance < 0)
        throw std::invalid_argument("invalid BVH query");
    Hit hit;
    hit.distance = maximum_distance;
    std::function<void(std::size_t)> visit = [&](std::size_t id) {
        const auto &n = nodes_[id];
        double d2 = 0;
        for (std::size_t j = 0; j < 3; ++j) {
            double d = p[j] - std::clamp(p[j], n.low[j], n.high[j]);
            d2 += d * d;
        }
        if (d2 > hit.distance * hit.distance)
            return;
        if (n.left || n.right) {
            visit(n.left);
            visit(n.right);
            return;
        }
        for (std::size_t k = n.begin; k < n.end; ++k) {
            const auto &t = triangles_[order_[k]];
            auto point = closest_triangle(p, t);
            double distance = norm(p - point);
            if (distance <= hit.distance) {
                hit = {point,
                       distance > 1e-12
                           ? (p - point) / distance
                           : cross(t.b - t.a, t.c - t.a) / norm(cross(t.b - t.a, t.c - t.a)),
                       distance,
                       t.id,
                       true,
                       cross(t.b - t.a, t.c - t.a) / norm(cross(t.b - t.a, t.c - t.a)),
                       t.friction,
                       t.sealability,
                       t.conductance};
            }
        }
    };
    if (!nodes_.empty())
        visit(0);
    return hit;
}
} // namespace octo::collision
