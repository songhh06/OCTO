#include "octo/dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace octo::dynamics;
using namespace octo::math;

int main() {
    try {
        System s;
        Body root;
        root.mass = .35; // Beak/core mass. Tissue mass added by the mesh ledger.
        root.radius = .075;
        root.fixed = true;
        s.add_body(root);
        s.add_volumetric_torso(0);
        if (s.torso_mesh.elements.size() != 8 || s.torso_body_ids.size() != 7)
            throw std::runtime_error("unexpected torso topology");
        const auto body_ids = s.torso_body_ids;
        double total = 0;
        for (auto id : body_ids)
            total += s.state.bodies[id].mass;
        // V(octahedron)=4abc/3. Root additionally owns 0.35 kg.
        const double expected = 0.35 + 1050.0 * 4.0 / 3.0 * .14 * .14 * .11;
        if (std::abs(total - expected) > 1e-8)
            throw std::runtime_error("double-counted FEM tissue mass");
        std::stringstream serialized;
        s.save(serialized);
        System copy;
        copy.restore(serialized);
        if (copy.torso_body_ids != body_ids || copy.torso_mesh.elements.size() != 8)
            throw std::runtime_error("torso checkpoint not preserved");
        auto top = body_ids[3];
        const auto rest = s.state.bodies[top].p;
        s.state.bodies[top].force = {0, -0.8, 0};
        double greatest = 0;
        for (unsigned j = 0; j < 120; ++j) {
            if (j == 60)
                s.state.bodies[top].force = {};
            const auto step = s.step();
            if (!step.converged)
                throw std::runtime_error("FEM solve failed at tick " + std::to_string(j) +
                                         ": " + step.failure);
            greatest = std::max(greatest, norm(s.state.bodies[top].p - rest));
            for (const auto &tet : s.torso_mesh.elements) {
                std::array<V3, 4> vertices;
                for (unsigned k = 0; k < 4; ++k)
                    vertices[k] = s.state.bodies[body_ids[tet.nodes[k]]].p;
                const auto J = fem::determinant(fem::deformation(tet, vertices));
                if (!(J > s.torso_material.min_J))
                    throw std::runtime_error("inverted torso tetrahedron");
            }
        }
        if (greatest < 1e-6)
            throw std::runtime_error("torso did not physically deform");
        std::cout << "P2V_TORSO_FEM_OK nodes=7 tets=8 total_mass_kg=" << total
                  << " max_displacement_m=" << greatest
                  << " after_release_m=" << norm(s.state.bodies[top].p - rest) << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "P2V_TORSO_FEM_FAIL " << e.what() << "\n";
        return 1;
    }
}
