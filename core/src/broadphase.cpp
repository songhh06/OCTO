#include "octo/contact.hpp"
#include <algorithm>
#include <functional>

namespace octo::collision {
using namespace math;
namespace {
bool overlaps(BoxBounds a, BoxBounds b) {
    for (std::size_t j = 0; j < 3; ++j)
        if (a.high[j] < b.low[j] || b.high[j] < a.low[j])
            return false;
    return true;
}
BoxBounds merge(BoxBounds a, BoxBounds b) {
    for (std::size_t j = 0; j < 3; ++j) {
        a.low[j] = std::min(a.low[j], b.low[j]);
        a.high[j] = std::max(a.high[j], b.high[j]);
    }
    return a;
}
} // namespace
void Broadphase::build(std::vector<BoxBounds> bounds) {
    boxes_ = std::move(bounds);
    nodes_.clear();
    for (const auto &b : boxes_)
        if (!finite(b.low) || !finite(b.high))
            throw std::invalid_argument("nonfinite swept AABB");
    if (!boxes_.empty())
        split(0, boxes_.size());
}
std::size_t Broadphase::split(std::size_t begin, std::size_t end) {
    Node n;
    n.begin = begin;
    n.end = end;
    n.box = boxes_[begin];
    for (std::size_t j = begin + 1; j < end; ++j)
        n.box = merge(n.box, boxes_[j]);
    auto id = nodes_.size();
    nodes_.push_back(n);
    if (end - begin > 2) {
        auto e = n.box.high - n.box.low;
        std::size_t axis = e.x > e.y ? 0 : 1;
        if (e.z > e[axis])
            axis = 2;
        std::stable_sort(boxes_.begin() + static_cast<std::ptrdiff_t>(begin),
                         boxes_.begin() + static_cast<std::ptrdiff_t>(end), [&](auto a, auto b) {
                             return a.low[axis] + a.high[axis] < b.low[axis] + b.high[axis];
                         });
        auto mid = (begin + end) / 2;
        auto l = split(begin, mid), r = split(mid, end);
        nodes_[id].left = l;
        nodes_[id].right = r;
    }
    return id;
}
std::vector<std::pair<std::size_t, std::size_t>> Broadphase::pairs() const {
    std::vector<std::pair<std::size_t, std::size_t>> result;
    std::function<void(std::size_t, std::size_t)> visit = [&](auto a, auto b) {
        const auto &x = nodes_[a];
        const auto &y = nodes_[b];
        if (!overlaps(x.box, y.box))
            return;
        if (!x.left && !x.right && !y.left && !y.right) {
            for (std::size_t i = x.begin; i < x.end; ++i)
                for (std::size_t j = y.begin; j < y.end; ++j) {
                    auto u = boxes_[i], v = boxes_[j];
                    if (u.id < v.id && overlaps(u, v))
                        result.emplace_back(u.id, v.id);
                }
            return;
        }
        if (a == b) {
            visit(x.left, x.left);
            visit(x.left, x.right);
            visit(x.right, x.left);
            visit(x.right, x.right);
        } else if (x.left || x.right) {
            visit(x.left, b);
            visit(x.right, b);
        } else {
            visit(a, y.left);
            visit(a, y.right);
        }
    };
    if (!nodes_.empty())
        visit(0, 0);
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
SegmentHit closest_segment_triangle(V3 a, V3 b, const Triangle &t) {
    auto ca = closest_triangle(a, t), cb = closest_triangle(b, t);
    SegmentHit hit{a, ca, 0, norm(a - ca), t.id};
    if (norm(b - cb) < hit.distance)
        hit = {b, cb, 1, norm(b - cb), t.id};
    auto n = cross(t.b - t.a, t.c - t.a);
    const auto d = b - a;
    double denom = dot(d, n);
    if (std::abs(denom) > 1e-14) {
        double u = dot(t.a - a, n) / denom;
        if (u >= 0 && u <= 1) {
            V3 p = a + d * u;
            V3 closest = closest_triangle(p, t);
            if (norm(p - closest) < 1e-10)
                return {p, p, u, 0, t.id};
        }
    }
    for (auto e : std::array<std::pair<V3, V3>, 3>{{{t.a, t.b}, {t.b, t.c}, {t.c, t.a}}}) {
        auto pair = closest_segments(a, b, e.first, e.second);
        double distance = norm(pair.a - pair.b);
        if (distance < hit.distance)
            hit = {pair.a, pair.b, pair.u, distance, t.id};
    }
    return hit;
}
std::vector<std::size_t> TriangleBvh::query(BoxBounds bounds) const {
    std::vector<std::size_t> result;
    std::function<void(std::size_t)> visit = [&](auto id) {
        const auto &n = nodes_[id];
        if (!overlaps(bounds, {n.low, n.high, 0}))
            return;
        if (n.left || n.right) {
            visit(n.left);
            visit(n.right);
        } else
            for (auto j = n.begin; j < n.end; ++j)
                result.push_back(order_[j]);
    };
    if (!nodes_.empty())
        visit(0);
    std::sort(result.begin(), result.end());
    return result;
}
} // namespace octo::collision
