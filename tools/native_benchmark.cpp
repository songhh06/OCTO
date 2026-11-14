#include "octo/dynamics.hpp"
#include "octo/json.hpp"
#include "octo/scene.hpp"
#include <algorithm>
#include <iostream>

using namespace octo::dynamics;
using namespace octo::math;
int main(int argc, char **argv) {
    const bool curved = argc == 2 && std::string(argv[1]) == "--curved";
    try {
        System s;
        s.solver.max_outer = 32;
        s.solver.max_newton = 20;
        s.solver.max_cg = 200;
        s.gravity = {0, -9.81, 0};
        s.water_density = 1025;
        Body root;
        root.mass = 2;
        root.volume = 2.0 / 1050;
        root.radius = 0.04;
        s.add_body(root);
        auto height = [=](double x, double z) {
            return curved ? -0.077 + 0.063 * (std::sqrt(x * x + z * z) - 0.3) : -0.08;
        };
        std::vector<octo::collision::Triangle> triangles;
        triangles.reserve(100000);
        unsigned id = 0;
        for (unsigned x = 0; x < 250; ++x)
            for (unsigned z = 0; z < 200; ++z) {
                double x0 = -2 + 4.0 * x / 250, x1 = -2 + 4.0 * (x + 1) / 250,
                       z0 = -2 + 4.0 * z / 200, z1 = -2 + 4.0 * (z + 1) / 200;
                V3 a{x0, height(x0, z0), z0}, b{x1, height(x1, z0), z0}, c{x0, height(x0, z1), z1},
                    d{x1, height(x1, z1), z1};
                triangles.push_back({a, c, b, id++});
                triangles.push_back({b, c, d, id++});
            }
        s.terrain.build(std::move(triangles));
        for (int arm = 0; arm < 8; ++arm) {
            Q q = exp(V3{0, 2 * pi * arm / 8, 0});
            auto ids = s.add_rod({0, q.rotate({0, 0, 0.3}), {}}, q, 1, 0.075, 0.012, 24, arm);
            for (unsigned j = 0; j < 16; ++j) {
                Cup c;
                c.a = {ids[j + 2], {}, Q{}};
                auto hit = s.terrain.closest(s.point(c.a), 0.1);
                if (!hit.found)
                    throw std::runtime_error("benchmark rim has no surface");
                const auto &b = s.state.bodies[c.a.body];
                c.a.local = b.q.conjugate().rotate(hit.point - b.p);
                c.rim_axis = b.q.conjugate().rotate(hit.surface_normal);
                c.command = true;
                c.query_surface = true;
                c.static_point = hit.point;
                c.normal = hit.surface_normal;
                c.radius = 0.006;
                c.pressure = c.external_pressure - c.pressure_max;
                c.force_limit = c.pressure_max * pi * c.radius * c.radius;
                c.normal_multiplier = c.force_limit;
                c.phase = CupPhase::Attached;
                s.state.cups.push_back(c);
            }
        }
        for (unsigned j = 0; j < 20; ++j) {
            double angle = 2 * pi * j / 20, r = 1.65;
            Body b;
            b.box = true;
            b.mass = 1;
            b.volume = 0.0009;
            b.half_extents = {0.06, 0.06, 0.06};
            b.inertia = {0.0024, 0.0024, 0.0024};
            b.p = {r * std::cos(angle), 0, r * std::sin(angle)};
            b.p.y = height(b.p.x, b.p.z);
            if (curved) {
                V3 n{-0.063 * std::cos(angle), 1, -0.063 * std::sin(angle)};
                n = n / norm(n);
                V3 axis = cross(V3{0, 1, 0}, n);
                b.q = exp(axis * (std::acos(n.y) / norm(axis)));
                b.p += n * 0.0602;
            } else
                b.p.y += 0.0602;
            b.radius = norm(b.half_extents);
            s.add_body(b);
        }
        s.self_contact = true;
        {
            std::ofstream initial("work/V13-initial.octostate");
            s.save(initial);
        }
        std::vector<double> times;
        unsigned max_contacts = 0, min_active = 128, completed = 0;
        double max_pen = 0;
        bool success = true;
        std::string failure;
        for (unsigned tick = 0; tick < 10; ++tick) {
            s.diagnostics = curved && tick >= 1 ? &std::cerr : nullptr;
            auto result = s.step();
            times.push_back(result.cpu_ms);
            max_contacts = std::max(max_contacts, result.contacts);
            max_pen = std::max(max_pen, result.penetration);
            unsigned active = 0;
            for (const auto &c : s.state.cups)
                if (c.phase == CupPhase::Attached || c.phase == CupPhase::Peeling)
                    ++active;
            min_active = std::min(min_active, active);
            std::cout << "benchmark tick=" << tick << " success=" << result.converged
                      << " contacts=" << result.contacts << " residual=" << result.residual
                      << " outer=" << result.outer_iterations
                      << " constraint=" << result.constraint_error
                      << " dual=" << result.friction_error << " pressure=" << result.pressure_error
                      << " ms=" << result.cpu_ms << std::endl;
            if (!result.converged) {
                success = false;
                failure = result.failure;
                std::ofstream file("work/V13-failure.octostate");
                s.save(file);
                break;
            }
            ++completed;
        }
        std::sort(times.begin(), times.end());
        using J = octo::json::Value;
        J report(J::Object{{"build_id", J(octo::io::build_id())},
                           {"mode", J("Release native CPU")},
                           {"arms", J::integer(8)},
                           {"elements", J::integer(192)},
                           {"cups", J::integer(128)},
                           {"other_dynamic_objects", J::integer(20)},
                           {"triangles", J::integer(100000)},
                           {"completed_ticks", J::integer(completed)},
                           {"min_attached_cups", J::integer(min_active)},
                           {"max_contacts", J::integer(max_contacts)},
                           {"target_contacts", J::integer(300)},
                           {"p50_ms", J(times[times.size() / 2])},
                           {"p95_ms", J(times[times.size() * 95 / 100])},
                           {"target_p95_ms", J(8.333333333333334)},
                           {"max_penetration_m", J(max_pen)},
                           {"success", J(success)},
                           {"failure", J(failure)},
                           {"V13_accepted", J(false)}});
        std::filesystem::create_directories("docs/progress/physics");
        octo::json::save(curved ? "docs/progress/physics/V13-curved-workload.json"
                                : "docs/progress/physics/V13-full-workload.json",
                         report);
        return success ? 0 : 2;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
