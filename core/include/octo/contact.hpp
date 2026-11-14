#pragma once
#include "octo/math.hpp"
#include <cstdint>
#include <limits>
#include <vector>

namespace octo::collision {
using math::V3;
struct Triangle {
    V3 a, b, c;
    std::uint32_t id{};
    double friction = 0.7, sealability = 1, conductance = 1e-13;
};
struct Hit {
    V3 point{}, normal{};
    double distance = std::numeric_limits<double>::infinity();
    std::uint32_t feature{};
    bool found = false;
    V3 surface_normal{0, 1, 0};
    double friction = 0.7, sealability = 1, conductance = 1e-13;
};
struct SegmentPair {
    double u = 0, v = 0;
    V3 a{}, b{};
};
SegmentPair closest_segments(V3 a, V3 b, V3 c, V3 d);
V3 closest_triangle(V3 p, const Triangle &t);
struct BoxBounds {
    V3 low{}, high{};
    std::size_t id = 0;
};
class Broadphase {
  public:
    void build(std::vector<BoxBounds> bounds);
    [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> pairs() const;

  private:
    struct Node {
        BoxBounds box;
        std::size_t left = 0, right = 0, begin = 0, end = 0;
    };
    std::vector<BoxBounds> boxes_;
    std::vector<Node> nodes_;
    std::size_t split(std::size_t begin, std::size_t end);
};
struct SegmentHit {
    V3 a{}, b{};
    double u = 0, distance = 0;
    std::uint32_t feature = 0;
};
SegmentHit closest_segment_triangle(V3 a, V3 b, const Triangle &t);
class TriangleBvh {
  public:
    void build(std::vector<Triangle> triangles);
    [[nodiscard]] Hit closest(V3 point, double maximum_distance) const;
    [[nodiscard]] std::vector<std::size_t> query(BoxBounds bounds) const;
    [[nodiscard]] const std::vector<Triangle> &triangles() const { return triangles_; }

  private:
    struct Node {
        V3 low{}, high{};
        std::size_t begin = 0, end = 0, left = 0, right = 0;
    };
    std::vector<Triangle> triangles_;
    std::vector<std::size_t> order_;
    std::vector<Node> nodes_;
    std::size_t build_node(std::size_t begin, std::size_t end);
};
} // namespace octo::collision
