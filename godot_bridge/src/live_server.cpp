#include "octo/dynamics.hpp"
#include "octo/json.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace octo::dynamics;
using namespace octo::math;
namespace {
void vec(std::ostream &o, V3 v) { o << '[' << v.x << ',' << v.y << ',' << v.z << ']'; }
void snapshot(std::ostream &o, const System &s) {
    o << "{\"tick\":" << s.state.tick << ",\"body\":{";
    const auto &body = s.state.bodies[0];
    o << "\"p\":";
    vec(o, body.p);
    o << ",\"q\":[" << body.q.x << ',' << body.q.y << ',' << body.q.z << ',' << body.q.w
      << "]},\"arms\":[";
    int previous = -1;
    bool first_arm = true;
    bool first_node = true;
    auto node = [&](const Element &e, Anchor a, double radius) {
        const auto &b = s.state.bodies[a.body];
        auto q = b.q * a.rotation;
        V3 p = s.point(a);
        if (!first_node)
            o << ',';
        first_node = false;
        o << '[' << p.x << ',' << p.y << ',' << p.z << ',' << q.x << ',' << q.y << ',' << q.z << ','
          << q.w << ',' << radius << ']';
        (void)e;
    };
    for (std::size_t j = 0; j < s.state.elements.size(); ++j) {
        const auto &e = s.state.elements[j];
        int arm = s.state.bodies[e.b.body].arm;
        if (arm != previous) {
            if (!first_arm)
                o << "]},";
            first_arm = false;
            o << "{\"id\":" << arm << ",\"nodes\":[";
            first_node = true;
            node(e, e.a, e.radius);
            previous = arm;
        }
        node(e, e.b, e.radius);
    }
    if (!first_arm)
        o << "]}";
    o << "],\"rigids\":[";
    bool first = true;
    for (std::size_t j = 1; j < s.state.bodies.size(); ++j) {
        const auto &b = s.state.bodies[j];
        if (b.arm >= 0 || b.station == -100)
            continue;
        if (!first)
            o << ',';
        first = false;
        o << "{\"id\":" << j << ",\"p\":";
        vec(o, b.p);
        o << ",\"q\":[" << b.q.x << ',' << b.q.y << ',' << b.q.z << ',' << b.q.w
          << "],\"half_extents\":";
        vec(o, b.half_extents);
        o << '}';
    }
    o << "],\"torso_nodes\":[";
    for (std::size_t j = 0; j < s.torso_body_ids.size(); ++j) {
        if (j)
            o << ',';
        vec(o, s.state.bodies[s.torso_body_ids[j]].p);
    }
    o << "]}";
}

System make_world(bool volumetric = false) {
    System s;
    Body root;
    root.mass = volumetric ? 0.35 : 2.0;
    root.radius = volumetric ? .075 : .18;
    if (volumetric)
        root.inertia = {.015, .015, .015};
    s.add_body(root);
    for (int arm = 0; arm < 8; ++arm) {
        Q q = exp(V3{0, 2 * pi * arm / 8, 0});
        s.add_rod({0, q.rotate({0, 0, .25}), {}}, q, .9, .06, .012, 12, arm);
    }
    if (volumetric)
        s.add_volumetric_torso(0);
    Body box;
    box.box = true;
    box.mass = .6;
    box.half_extents = {.16, .16, .16};
    box.radius = norm(box.half_extents);
    box.inertia = {.01024, .01024, .01024};
    box.p = {1.45, 0, .8};
    s.add_body(box);
    s.self_contact = true;
    s.water_density = 1000;
    Plane floor;
    floor.offset = -.22;
    s.planes.push_back(floor);
    return s;
}
V3 read_vec(const octo::json::Value &v) {
    auto &a = v.array();
    if (a.size() != 3)
        throw std::invalid_argument("vec3 required");
    return {a[0].number(), a[1].number(), a[2].number()};
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    try {
        std::filesystem::path diagnostic_dir = std::filesystem::absolute("work");
#ifdef _WIN32
        const bool has_diagnostics = argc == 3 && std::wstring(argv[1]) == L"--diagnostics";
#else
        const bool has_diagnostics = argc == 3 && std::string(argv[1]) == "--diagnostics";
#endif
        if (has_diagnostics)
            diagnostic_dir = std::filesystem::path(argv[2]);
#ifdef _WIN32
        const bool fem_requested = (argc == 4 && has_diagnostics && std::wstring(argv[3]) == L"--torso-fem") ||
                                   (argc == 2 && std::wstring(argv[1]) == L"--torso-fem");
#else
        const bool fem_requested = (argc == 4 && has_diagnostics && std::string(argv[3]) == "--torso-fem") ||
                                   (argc == 2 && std::string(argv[1]) == "--torso-fem");
#endif
        if (argc != 1 && !has_diagnostics && !fem_requested)
            throw std::invalid_argument("expected [--diagnostics directory] [--torso-fem]");
        if (argc == 4 && !fem_requested)
            throw std::invalid_argument("unknown native option");
        std::filesystem::create_directories(diagnostic_dir);
        std::cout << std::setprecision(17);
        System s = make_world(fem_requested);
        std::size_t box_id = s.state.bodies.size() - 1;
        std::vector<V3> initial;
        for (const auto &b : s.state.bodies)
            initial.push_back(b.p);
        std::string line;
        StepResult last;
        bool collision = true;
        auto send = [&](bool ok, const std::string &error) {
            double movement = 0;
            for (std::size_t j = 1; j < box_id; ++j)
                if (s.state.bodies[j].arm >= 0)
                    movement = std::max(movement, norm(s.state.bodies[j].p - initial[j]));
            std::cout << "{\"protocol\":1,\"live\":true,\"ok\":" << (ok ? "true" : "false")
                      << ",\"error\":" << octo::json::quote(error)
                      << ",\"contacts\":" << last.contacts
                      << ",\"penetration\":" << last.penetration << ",\"solver_ms\":" << last.cpu_ms
                      << ",\"arm_motion\":" << movement
                      << ",\"box_collision\":" << (collision ? "true" : "false")
                      << ",\"snapshot\":";
            snapshot(std::cout, s);
            std::cout << "}\n" << std::flush;
        };
        send(true, "");
        while (std::getline(std::cin, line)) {
            // Reject malformed client messages without killing the native child.
            // This leaves the reset RPC available after a protocol error.
            octo::json::Value command;
            std::string op;
            V3 target{};
            bool active = false;
            try {
                if (line.size() > 65536)
                    throw std::invalid_argument("live message too large");
                command = octo::json::Parser(line).parse();
                op = command.at("op").string();
                if (op == "step") {
                    target = read_vec(command.at("target"));
                    active = command.flag("drive", true);
                } else if (op != "quit" && op != "reset")
                    throw std::invalid_argument("unknown live operation");
            } catch (const std::exception &e) {
                last = {};
                send(false, std::string("Invalid live request: ") + e.what());
                continue;
            }
            if (op == "quit")
                break;
            if (op == "reset") {
                s = make_world(fem_requested);
                box_id = s.state.bodies.size() - 1;
                collision = true;
                last = {};
                send(true, "");
                continue;
            }
            target.x = std::clamp(target.x, -1.6, 1.6);
            target.y = std::clamp(target.y, -.15, .7);
            target.z = std::clamp(target.z, -1.6, 1.6);
            auto &box = s.state.bodies[box_id];
            box.force = active ? (target - box.p) * 30.0 - box.v * 10.0
                               : V3{}; // SI mouse joint: 30 N/m, 10 N*s/m.
            double force = norm(box.force);
            if (force > 10)
                box.force = box.force * (10 / force); // documented 10 N controller limit.
            box.torque =
                -log(box.q) * 4.0 - box.w * 0.8; // upright hand spring: N*m/rad, N*m*s/rad.
            const double torque = norm(box.torque);
            if (torque > 2)
                box.torque = box.torque * (2 / torque);
            last = s.step();
            if (!last.converged) {
                // Finish durable diagnostics before the client can close the pipe.
                std::ofstream fail(diagnostic_dir / "live-failure.octostate");
                if (!fail)
                    throw std::runtime_error("cannot write failure checkpoint");
                s.save(fail);
                fail.close();
                using J = octo::json::Value;
                octo::json::save(
                    diagnostic_dir / "live-failure.json",
                    J(J::Object{{"tick", J::integer(s.state.tick)},
                                {"command", command},
                                {"error", J(last.failure)},
                                {"residual", J(last.residual)},
                                {"outer_iterations", J::integer(last.outer_iterations)},
                                {"constraint_error", J(last.constraint_error)},
                                {"dual_error", J(last.friction_error)}}));
            }
            send(last.converged, last.failure);
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
