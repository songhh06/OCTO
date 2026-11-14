#include "octo/fem.hpp"
#include <iostream>
using namespace octo::fem;
using namespace octo::math;
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    try {
        Material m;
        TetMesh mesh;
        mesh.reference = {{0, 0, 0}, {.1, 0, 0}, {0, .1, 0}, {0, 0, .1}};
        mesh.elements.push_back(make_tet(mesh.reference, {0, 1, 2, 3}, m));
        auto &t = mesh.elements[0];
        std::array<V3, 4> rest;
        std::copy(mesh.reference.begin(), mesh.reference.end(), rest.begin());
        auto relaxed = evaluate(t, rest, 0, m);
        check(std::abs(relaxed.J - 1) < 1e-12 && std::abs(relaxed.energy) < 1e-12, "rest state");
        for (double g : relaxed.gradient)
            check(std::abs(g) < 1e-10, "nonzero rest gradient");
        auto x = rest;
        x[1].x *= .9;
        x[2].x += .015;
        x[3].y -= .005;
        double p = -10000;
        auto exact = evaluate(t, x, p, m);
        double ge = 0, he = 0;
        for (unsigned j = 0; j < 13; ++j) {
            double h = j == 12 ? .1 : 1e-7;
            auto plus = x, minus = x;
            double pp = p, pm = p;
            if (j == 12) {
                pp += h;
                pm -= h;
            } else {
                plus[j / 3][j % 3] += h;
                minus[j / 3][j % 3] -= h;
            }
            auto a = evaluate(t, plus, pp, m), b = evaluate(t, minus, pm, m);
            double g = (a.energy - b.energy) / (2 * h);
            ge = std::max(ge, std::abs(g - exact.gradient[j]) / std::max(1.0, std::abs(g)));
            for (unsigned k = 0; k < 13; ++k) {
                double H = (a.gradient[k] - b.gradient[k]) / (2 * h);
                he = std::max(he,
                              std::abs(H - exact.tangent[k * 13 + j]) / std::max(1.0, std::abs(H)));
            }
        }
        check(ge < 1e-4 && he < 1e-4, "independent energy/force differences");
        Q rotation = exp(V3{.3, -.2, .4});
        auto moved = x;
        for (auto &v : moved)
            v = rotation.rotate(v) + V3{2, -3, 4};
        auto invariant = evaluate(t, moved, p, m);
        check(std::abs(invariant.energy - exact.energy) < 1e-10, "rigid transform invariance");
        for (unsigned j = 0; j < 4; ++j) {
            V3 force{exact.gradient[3 * j], exact.gradient[3 * j + 1], exact.gradient[3 * j + 2]},
                g{invariant.gradient[3 * j], invariant.gradient[3 * j + 1],
                  invariant.gradient[3 * j + 2]};
            check(norm(g - rotation.rotate(force)) < 1e-8, "force covariance");
        }
        auto stationary = evaluate(t, x, m.bulk * (exact.J - 1), m);
        check(std::abs(stationary.gradient[12]) < 1e-15, "mixed pressure stationarity");
        bool rejected = false;
        try {
            auto inverted = rest;
            std::swap(inverted[1], inverted[2]);
            (void)evaluate(t, inverted, 0, m);
        } catch (const std::domain_error &) {
            rejected = true;
        }
        check(rejected, "inverted candidate accepted");
        rejected = false;
        try {
            (void)make_tet(mesh.reference, {0, 2, 1, 3}, m);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        check(rejected, "inverted reference accepted");
        auto skew_reference = mesh.reference;
        skew_reference[1] = {.1, .01, 0};
        skew_reference[2] = {.02, .1, .005};
        skew_reference[3] = {.004, -.003, .1};
        auto skew_tet = make_tet(skew_reference, {0, 1, 2, 3}, m);
        std::array<V3, 4> skew_x;
        std::copy(skew_reference.begin(), skew_reference.end(), skew_x.begin());
        auto skew_result = evaluate(skew_tet, skew_x, 0, m);
        check(std::abs(skew_result.J - 1) < 1e-12 && std::abs(skew_result.energy) < 1e-11,
              "skew reference inverse/rest invariance");
        using A = Jet2<13>;
        auto jump = pressure_jump_energy(A::variable(100, 0), A::variable(100, 1),
                                         t.reference_volume, m.bulk, .1);
        check(std::abs(jump.v) < 1e-20 && jump.h[0] < 0 && std::abs(jump.h[0] + jump.h[1]) < 1e-20,
              "pressure stabilization null mode/sign");
        auto bind = bind_inside(mesh, 0, {.02, .03, .01});
        std::vector<V3> deformed(x.begin(), x.end());
        check(norm(embedded_position(mesh, deformed, bind) -
                   (x[0] * .4 + x[1] * .2 + x[2] * .3 + x[3] * .1)) < 1e-12,
              "barycentric embedding");
        auto mass = nodal_masses(mesh, 1050);
        double total = 0;
        for (double value : mass)
            total += value;
        check(std::abs(total - t.reference_volume * 1050) < 1e-12, "element/nodal mass identity");
        std::cout << "P1V.1 mixed tetrahedron: gradient_relative=" << ge
                  << " tangent_relative=" << he << " J=" << exact.J << " mass=" << total
                  << " embedding_error<1e-12\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
