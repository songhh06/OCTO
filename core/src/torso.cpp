#include "octo/dynamics.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace octo::dynamics {
void System::add_volumetric_torso(std::size_t root, double radial, double vertical,
                                  double density) {
    if (root >= state.bodies.size() || !std::isfinite(radial) || radial <= 0 ||
        !std::isfinite(vertical) || vertical <= 0 || !std::isfinite(density) || density <= 0 ||
        !torso_mesh.elements.empty())
        throw std::invalid_argument("invalid or duplicate volumetric torso");

    // A shared central FEM degree of freedom transmits rod/torso reaction in
    // the same Newton solve. No second rigid torso shell is constructed.
    const auto center = state.bodies[root].p;
    torso_mesh.reference = {center,
                            center + V3{radial, 0, 0},
                            center + V3{-radial, 0, 0},
                            center + V3{0, vertical, 0},
                            center + V3{0, -vertical, 0},
                            center + V3{0, 0, radial},
                            center + V3{0, 0, -radial}};
    torso_body_ids.clear();
    torso_body_ids.push_back(static_cast<std::uint32_t>(root));
    for (std::size_t j = 1; j < torso_mesh.reference.size(); ++j) {
        Body node;
        node.p = torso_mesh.reference[j];
        node.mass = 1e-9; // Replaced by lumped tetra mass below.
        node.inertia = {1e-5, 1e-5, 1e-5};
        node.radius = 0; // Vertex contact, not an artificial rigid sphere.
        node.station = -100; // FEM surface vertex, persisted in checkpoints.
        torso_body_ids.push_back(static_cast<std::uint32_t>(add_body(node)));
    }
    torso_material = {};
    torso_material.shear = 5000; // ASSUMED Pa, not biological calibration.
    torso_material.bulk = 100000;
    torso_material.viscosity = 20;
    torso_material.min_J = 0.05;
    for (unsigned ix : {1u, 2u})
        for (unsigned iy : {3u, 4u})
            for (unsigned iz : {5u, 6u}) {
                std::array<std::uint32_t, 4> ids{0, ix, iy, iz};
                try {
                    torso_mesh.elements.push_back(
                        fem::make_tet(torso_mesh.reference, ids, torso_material));
                } catch (const std::invalid_argument &) {
                    std::swap(ids[2], ids[3]);
                    torso_mesh.elements.push_back(
                        fem::make_tet(torso_mesh.reference, ids, torso_material));
                }
            }
    // A single shared center node transfers translation but not root torque.
    // Three compliant attachment fibers connect the anatomical core/beak
    // orientation to distinct surface nodes. These links transmit equal and
    // opposite forces and moments in the SAME native solve. They model only
    // the local root-to-mantle interface, not extra rod bending stiffness.
    // The interface stiffness is an uncalibrated engineering placeholder.
    for (unsigned vertex : {1u, 3u, 5u}) {
        Link tether;
        tether.a = {root, state.bodies[root].q.conjugate().rotate(
                             torso_mesh.reference[vertex] - center), Q{}};
        tether.b = {torso_body_ids[vertex], {}, Q{}};
        tether.rest = 0;
        tether.stiffness = 120; // N/m, candidate core connective tissue
        tether.capacity = 1e30;
        links.push_back(tether);
    }
    const auto masses = fem::nodal_masses(torso_mesh, density);
    for (std::size_t j = 0; j < masses.size(); ++j) {
        auto &b = state.bodies[torso_body_ids[j]];
        if (j == 0)
            b.mass += masses[j]; // Root already owns any beak and attached-rod mass.
        else
            b.mass = masses[j];
        b.volume += masses[j] / density; // one mass / buoyancy ledger
    }
}
} // namespace octo::dynamics
