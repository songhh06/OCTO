#include "octo/fem.hpp"
#include <set>
namespace octo::fem {
using namespace math;
Matrix3<double> inverse(const Matrix3<double> &a) {
    double d = determinant(a);
    if (!std::isfinite(d) || d == 0)
        throw std::invalid_argument("singular reference tetrahedron");
    auto r0 = cross(a.c[1], a.c[2]) / d, r1 = cross(a.c[2], a.c[0]) / d,
         r2 = cross(a.c[0], a.c[1]) / d;
    Matrix3<double> result;
    result.c[0] = {r0.x, r1.x, r2.x};
    result.c[1] = {r0.y, r1.y, r2.y};
    result.c[2] = {r0.z, r1.z, r2.z};
    return result;
}
void Material::validate() const {
    if (!(std::isfinite(shear) && shear > 0 && std::isfinite(bulk) && bulk > 0 &&
          std::isfinite(viscosity) && viscosity >= 0 && std::isfinite(min_J) && min_J > 0 &&
          min_J < 1 && std::isfinite(min_relative_determinant) && min_relative_determinant > 0 &&
          min_relative_determinant < 1))
        throw std::invalid_argument("invalid FEM material/domain parameters");
}
Tet make_tet(const std::vector<V3> &X, std::array<std::uint32_t, 4> ids, const Material &m,
             std::uint32_t region) {
    m.validate();
    std::set<std::uint32_t> unique;
    for (auto id : ids)
        if (id >= X.size() || !finite(X[id]) || !unique.insert(id).second)
            throw std::invalid_argument("invalid FEM reference node");
    Matrix3<double> dm{{X[ids[1]] - X[ids[0]], X[ids[2]] - X[ids[0]], X[ids[3]] - X[ids[0]]}};
    double d = determinant(dm), edge = 0;
    for (unsigned j = 0; j < 4; ++j)
        for (unsigned k = j + 1; k < 4; ++k)
            edge = std::max(edge, norm(X[ids[k]] - X[ids[j]]));
    if (!(d > m.min_relative_determinant * edge * edge * edge))
        throw std::invalid_argument("inverted or sliver reference tetrahedron");
    return {ids, inverse(dm), d / 6, region};
}
ElementResult evaluate(const Tet &t, const std::array<V3, 4> &x, double pressure,
                       const Material &m) {
    m.validate();
    if (!std::isfinite(pressure))
        throw std::invalid_argument("nonfinite FEM pressure");
    using A = Jet2<13>;
    std::array<Vec<A>, 4> nodes;
    for (unsigned j = 0; j < 4; ++j) {
        if (!finite(x[j]))
            throw std::invalid_argument("nonfinite FEM position");
        for (unsigned k = 0; k < 3; ++k)
            nodes[j][k] = A::variable(x[j][k], 3 * j + k);
    }
    A p = A::variable(pressure, 12), energy = mixed_energy(t, nodes, p, m);
    ElementResult result;
    result.energy = energy.v;
    result.J = determinant(deformation(t, x));
    result.gradient = energy.d;
    result.tangent = energy.h;
    return result;
}
SkinBind bind_inside(const TetMesh &mesh, std::uint32_t id, V3 point, double tolerance) {
    if (!finite(point) || !std::isfinite(tolerance) || tolerance < 0)
        throw std::invalid_argument("invalid binding query");
    const auto &t = mesh.elements.at(id);
    auto r = multiply(t.Dm_inverse, point - mesh.reference.at(t.nodes[0]));
    SkinBind b{id, {1 - r.x - r.y - r.z, r.x, r.y, r.z}};
    for (double w : b.weights)
        if (w < -tolerance || w > 1 + tolerance)
            throw std::invalid_argument(
                "skin point outside tetrahedron: boundary projection required");
    return b;
}
V3 embedded_position(const TetMesh &mesh, const std::vector<V3> &x, const SkinBind &b) {
    const auto &t = mesh.elements.at(b.tet);
    V3 p{};
    double sum = 0;
    for (unsigned j = 0; j < 4; ++j) {
        if (!std::isfinite(b.weights[j]))
            throw std::invalid_argument("nonfinite binding weight");
        p += x.at(t.nodes[j]) * b.weights[j];
        sum += b.weights[j];
    }
    if (std::abs(sum - 1) > 1e-9)
        throw std::invalid_argument("binding weights do not sum to one");
    return p;
}
std::vector<double> nodal_masses(const TetMesh &mesh, double density) {
    if (!std::isfinite(density) || density <= 0)
        throw std::invalid_argument("invalid FEM density");
    std::vector<double> mass(mesh.reference.size(), 0);
    for (const auto &t : mesh.elements) {
        if (!(t.reference_volume > 0))
            throw std::invalid_argument("nonpositive FEM reference volume");
        for (auto id : t.nodes)
            mass.at(id) += density * t.reference_volume / 4;
    }
    return mass;
}
} // namespace octo::fem
