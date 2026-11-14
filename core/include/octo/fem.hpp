#pragma once
#include "octo/math.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace octo::fem {
using math::V3;
template <class T> struct Matrix3 {
    std::array<math::Vec<T>, 3> c;
};
template <class T> T determinant(const Matrix3<T> &a) {
    return math::dot(a.c[0], math::cross(a.c[1], a.c[2]));
}
template <class T> math::Vec<T> multiply(const Matrix3<T> &a, math::Vec<T> v) {
    return a.c[0] * v.x + a.c[1] * v.y + a.c[2] * v.z;
}
Matrix3<double> inverse(const Matrix3<double> &a);
struct Material {
    // SI: Pa, Pa, Pa*s; dimensionless positive-J domain and rest-quality bound.
    // Candidate tissue parameters, ASSUMED; no biological calibration.
    double shear = 20000, bulk = 2000000, viscosity = 20, min_J = 0.05,
           min_relative_determinant = 1e-6;
    void validate() const;
};
struct Tet {
    std::array<std::uint32_t, 4> nodes{};
    Matrix3<double> Dm_inverse{};
    double reference_volume = 0;
    std::uint32_t region = 0;
};
struct TetMesh {
    std::vector<V3> reference;
    std::vector<Tet> elements;
};
Tet make_tet(const std::vector<V3> &reference, std::array<std::uint32_t, 4> nodes,
             const Material &material, std::uint32_t region = 0);
template <class T> Matrix3<T> deformation(const Tet &t, const std::array<math::Vec<T>, 4> &x) {
    Matrix3<T> ds{{x[1] - x[0], x[2] - x[0], x[3] - x[0]}}, F;
    for (unsigned j = 0; j < 3; ++j)
        F.c[j] = multiply(ds, math::Vec<T>(t.Dm_inverse.c[j]));
    return F;
}
// Mixed displacement-pressure saddle energy. Pressure is an independent DOF,
// not clamped or minimized with a positive-definite displacement-only CG solve.
template <class T> T fractional_power(const T &a, double exponent) {
    return T::unary(a, std::pow(a.v, exponent), exponent * std::pow(a.v, exponent - 1),
                    exponent * (exponent - 1) * std::pow(a.v, exponent - 2));
}
template <> inline double fractional_power(const double &a, double exponent) {
    return std::pow(a, exponent);
}
template <class T>
T mixed_energy(const Tet &t, const std::array<math::Vec<T>, 4> &x, const T &pressure,
               const Material &m) {
    const auto F = deformation(t, x);
    T J = determinant(F);
    if (!(math::value(J) > m.min_J))
        throw std::domain_error("FEM candidate J outside positive domain");
    T trace(0);
    for (const auto &c : F.c)
        trace = trace + math::dot(c, c);
    return T(t.reference_volume) *
           (T(m.shear / 2) * (fractional_power(J, -2.0 / 3) * trace - T(3)) +
            pressure * (J - T(1)) - pressure * pressure / T(2 * m.bulk));
}
// Negative pressure-jump stabilization is consistent with the concave p block.
// The mesh-level coefficient must be selected by refinement/pressure-mode tests.
template <class T>
T pressure_jump_energy(T pa, T pb, double reference_volume, double bulk, double coefficient) {
    return T(-coefficient * reference_volume / (2 * bulk)) * (pa - pb) * (pa - pb);
}
struct ElementResult {
    double energy = 0, J = 0;
    std::array<double, 13> gradient{};
    std::array<double, 169> tangent{};
};
ElementResult evaluate(const Tet &t, const std::array<V3, 4> &x, double pressure,
                       const Material &m);
struct SkinBind {
    std::uint32_t tet = 0;
    std::array<double, 4> weights{};
};
SkinBind bind_inside(const TetMesh &mesh, std::uint32_t tet, V3 point, double tolerance = 1e-10);
V3 embedded_position(const TetMesh &mesh, const std::vector<V3> &x, const SkinBind &binding);
std::vector<double> nodal_masses(const TetMesh &mesh, double density);
} // namespace octo::fem
