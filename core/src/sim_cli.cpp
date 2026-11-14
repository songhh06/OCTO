#include "octo/dynamics.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

using namespace octo::dynamics;
using namespace octo::math;
namespace {
double number(const std::string &s) {
    double n{};
    auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (e != std::errc{} || p != s.data() + s.size() || !std::isfinite(n))
        throw std::invalid_argument("invalid numeric argument");
    return n;
}
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
        if (b.arm >= 0)
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
    o << "]}";
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::map<std::string, std::string> args;
        for (int j = 1; j < argc; j += 2) {
            if (j + 1 >= argc)
                throw std::invalid_argument("expected --key value");
            const std::string key = argv[j];
            if (key != "--scenario" && key != "--channel" && key != "--command" &&
                key != "--mass" && key != "--ticks" && key != "--stride" && key != "--trace" &&
                key != "--stats" && key != "--checkpoint" && key != "--restore")
                throw std::invalid_argument("unknown option: " + key);
            args[key] = argv[j + 1];
        }
        auto get = [&](std::string key, std::string fallback) {
            return args.contains(key) ? args.at(key) : fallback;
        };
        const std::string scene = get("--scenario", "arm");
        const double tick_arg = number(get("--ticks", "600")),
                     stride_arg = number(get("--stride", "4"));
        if (tick_arg < 1 || tick_arg > 72000 || tick_arg != std::floor(tick_arg) ||
            stride_arg < 1 || stride_arg != std::floor(stride_arg))
            throw std::invalid_argument("invalid tick/stride count");
        const auto ticks = static_cast<unsigned>(tick_arg),
                   stride = static_cast<unsigned>(stride_arg);
        const double channel_arg = number(get("--channel", "0")),
                     command = number(get("--command", "0.12"));
        if (channel_arg < 0 || channel_arg > 6 || channel_arg != std::floor(channel_arg) ||
            command < 0 || command > 1)
            throw std::invalid_argument("invalid muscle command");
        const auto channel = static_cast<std::size_t>(channel_arg);
        System s;
        Body body;
        body.fixed = scene == "arm";
        body.mass = 2;
        body.radius = scene == "eight" ? 0.2 : 0.01;
        s.add_body(body);
        if (scene == "arm" || scene == "coupled") {
            s.add_rod({0, {}, {}}, Q{});
            for (auto &e : s.state.elements)
                e.command[channel] = command;
            if (scene == "coupled") {
                s.self_contact = true;
                for (auto &e : s.state.elements) {
                    e.command.fill(0);
                    for (std::size_t j = 0; j < 4; ++j)
                        e.command[j] = command;
                }
                Body box;
                box.box = true;
                box.mass = number(get("--mass", "0.5"));
                box.half_extents = {0.15, 0.15, 0.15};
                box.inertia = V3{1, 1, 1} * (box.mass * 0.015);
                box.p = {0, 0, 1.20};
                auto target = s.add_body(box);
                Cup c;
                c.a = {24, {0, 0, 0.05}, Q{}};
                c.b = {target, {0, 0, -0.15}, Q{}};
                c.target_static = false;
                c.command = true;
                c.phase = CupPhase::Attached;
                c.pressure = c.external_pressure - 40000;
                c.force_limit = 40000 * pi * c.radius * c.radius;
                c.material.conductance = 0;
                c.pump_rate = 0;
                c.normal = {0, 0, -1};
                s.state.cups.push_back(c);
            }
        } else if (scene == "eight") {
            s.self_contact = true;
            for (int j = 0; j < 8; ++j) {
                Q q = exp(V3{0, 2 * pi * j / 8, 0});
                auto ids = s.add_rod({0, q.rotate({0, 0, 0.3}), {}}, q, 1, 0.075, 0.012, 24, j);
                for (int k = 0; k < 16; ++k) {
                    Cup c;
                    c.a = {ids[static_cast<std::size_t>(k + 2)], {}, Q{}};
                    c.command = true;
                    c.static_point = s.point(c.a);
                    s.state.cups.push_back(c);
                }
            }
        } else
            throw std::invalid_argument("scenario must be arm, coupled or eight");
        if (args.contains("--restore")) {
            std::ifstream file(args.at("--restore"));
            if (!file)
                throw std::runtime_error("cannot read checkpoint");
            s.restore(file);
        }
        std::ofstream trace(get("--trace", "physics_trace.json"));
        if (!trace)
            throw std::runtime_error("cannot open trace output");
        trace << std::setprecision(17);
        trace << "{\"schema_version\":1,\"source\":\"native-cxx20-cosserat\",\"playback_only\":"
                 "true,\"scene\":\""
              << scene << "\",\"fixed_dt\":" << octo::fixed_dt << ",\"stride\":" << stride
              << ",\"channel\":" << channel << ",\"command\":" << command << ",\"frames\":[";
        snapshot(trace, s);
        std::vector<double> timings;
        bool success = true;
        unsigned fallback = 0, max_iterations = 0, max_contacts = 0;
        double max_residual = 0, max_volume = 0, max_penetration = 0;
        V3 reaction{}, reaction_torque{};
        for (unsigned j = 0; j < ticks; ++j) {
            auto r = s.step();
            timings.push_back(r.cpu_ms);
            if (r.substeps > 1)
                ++fallback;
            max_iterations = std::max(max_iterations, r.iterations);
            max_residual = std::max(max_residual, r.residual);
            max_volume = std::max(max_volume, r.volume_error);
            max_contacts = std::max(max_contacts, r.contacts);
            max_penetration = std::max(max_penetration, r.penetration);
            reaction = r.support_force;
            reaction_torque = r.support_torque;
            if (!r.converged) {
                success = false;
                std::ofstream fail(get("--checkpoint", "physics_failure.octostate"));
                s.save(fail);
                std::cerr << "tick " << s.state.tick << ": " << r.failure << '\n';
                break;
            }
            if ((j + 1) % stride == 0 || j + 1 == ticks) {
                trace << ',';
                snapshot(trace, s);
            }
        }
        trace << "],\"success\":" << (success ? "true" : "false") << '}';
        trace.close();
        if (args.contains("--checkpoint")) {
            std::ofstream file(args.at("--checkpoint"));
            s.save(file);
        }
        std::sort(timings.begin(), timings.end());
        std::ofstream stats(get("--stats", "physics_stats.json"));
        if (!stats)
            throw std::runtime_error("cannot open stats output");
        stats << std::setprecision(17);
        stats << "{\"success\":" << (success ? "true" : "false") << ",\"tick\":" << s.state.tick
              << ",\"state_fnv1a64\":\"" << s.state_hash()
              << "\",\"p50_ms\":" << timings[timings.size() / 2]
              << ",\"p95_ms\":" << timings[timings.size() * 95 / 100]
              << ",\"max_volume_error\":" << max_volume << ",\"max_residual\":" << max_residual
              << ",\"max_newton_iterations_including_retries\":" << max_iterations
              << ",\"fallback_ticks\":" << fallback << ",\"bodies\":" << s.state.bodies.size()
              << ",\"elements\":" << s.state.elements.size() << ",\"cups\":" << s.state.cups.size()
              << ",\"max_contacts\":" << max_contacts
              << ",\"max_penetration_m\":" << max_penetration << ",\"last_support_force\":";
        vec(stats, reaction);
        stats << ",\"last_support_torque\":";
        vec(stats, reaction_torque);
        stats << '}';
        std::cout << "native scene=" << scene << " ticks=" << s.state.tick << " success=" << success
                  << " p95_ms=" << timings[timings.size() * 95 / 100] << '\n';
        return success ? 0 : 2;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
