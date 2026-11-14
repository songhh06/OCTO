#include "octo/dynamics.hpp"
#include "octo/scene.hpp"
#include "octo/sha256.hpp"
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>

using namespace octo::dynamics;
using namespace octo::math;
void check(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
StepResult advance(System &s) {
    auto r = s.step();
    if (!r.converged) {
        std::cerr << "tick=" << s.state.tick << " residual=" << r.residual
                  << " outer=" << r.outer_iterations << " pressure=" << r.pressure_error
                  << " constraint=" << r.constraint_error << " dual=" << r.friction_error << '\n';
        throw std::runtime_error(r.failure);
    }
    return r;
}
System carrier(double friction = 0.7, double sealability = 1, double conductance = 1e-13) {
    System s;
    Body b;
    b.box = true;
    b.p = {0, 0.01, 0};
    b.half_extents = {0.05, 0.01, 0.05};
    b.inertia = {0.001, 0.002, 0.001};
    b.radius = 0.01;
    s.add_body(b);
    Plane p;
    p.friction = friction;
    p.sealability = sealability;
    p.conductance = conductance;
    s.planes.push_back(p);
    Cup c;
    c.a = {0, {0, -0.01, 0}, {}};
    c.command = true;
    c.query_surface = true;
    c.plane = 0;
    c.static_point = {0, 0, 0};
    c.material = {friction, sealability, conductance};
    s.state.cups.push_back(c);
    return s;
}
void seal() {
    auto s = carrier();
    for (unsigned t = 0; t < 240; ++t)
        advance(s);
    auto initial = s.state.cups[0];
    check(initial.phase == CupPhase::Attached && initial.force_limit > 1,
          "geometric flat seal failed");
    s.state.cups[0].command = false;
    advance(s);
    check(s.state.cups[0].force_limit == 0 && norm(s.state.cups[0].tangent_multiplier) == 0 &&
              s.state.cups[0].normal_multiplier == 0,
          "residual released adhesion");
    std::cout << "geometric_seal_pressure=" << initial.pressure << " force=" << initial.force_limit
              << '\n';
}
void sliding() {
    auto run = [](double friction, double sealability, double leak) {
        auto s = carrier(friction, sealability, leak);
        s.gravity = {0, -9.81, 0};
        auto folder = octo::json::path_utf8(OCTO_SOURCE_DIR) / "docs/progress/physics";
        std::filesystem::create_directories(folder);
        std::ofstream csv(folder / (sealability > 0.5 ? "V08-glass.csv" : "V08-porous.csv"));
        csv << "time_s,pressure_Pa,force_N,x_m,friction_N\n";
        for (unsigned t = 0; t < 120; ++t)
            advance(s);
        s.state.bodies[0].force = {10, 0, 0};
        double maxpen = 0;
        for (unsigned t = 0; t < 120; ++t) {
            auto r = advance(s);
            maxpen = std::max(maxpen, r.penetration);
            check(r.pressure_error < 0.01, "pressure outer mismatch");
            for (const auto &c : s.state.contacts)
                check(norm(c.tangent_multiplier) <= c.friction * c.normal_multiplier + 1e-8,
                      "contact Coulomb cone");
            const auto &c = s.state.cups[0];
            check(norm(c.tangent_multiplier) <=
                      c.material.friction * (c.normal_multiplier + c.force_limit) + 1e-5,
                  "cup Coulomb cone");
            csv << t * octo::fixed_dt << ',' << c.pressure << ',' << c.force_limit << ','
                << s.state.bodies[0].p.x << ',' << norm(c.tangent_multiplier) << '\n';
        }
        check(maxpen < 0.002, "integrated slide penetration");
        std::cout << "material=" << sealability << " x=" << s.state.bodies[0].p.x
                  << " maxpen=" << maxpen << '\n';
        return s.state.bodies[0].p.x;
    };
    double glass = run(0.7, 1, 1e-13), porous = run(0.15, 0.2, 1e-8);
    check(glass < 0.005 && porous > glass + 0.05, "material sliding distinction failed");
}
void hinge() {
    System s;
    Body b;
    b.box = true;
    b.mass = 2;
    b.p = {0.5, 0.5, 0};
    b.half_extents = {0.5, 0.5, 0.05};
    b.inertia = {0.1683333, 0.1683333, 0.3333333};
    b.torque = {0, 0.02, 0};
    s.add_body(b);
    Hinge joint;
    joint.a = {0, {-0.5, 0, 0}, {}};
    joint.static_point = {0, 0.5, 0};
    s.state.hinges.push_back(joint);
    double maxerror = 0;
    for (unsigned t = 0; t < 120; ++t) {
        advance(s);
        maxerror = std::max(maxerror, norm(s.point(joint.a) - joint.static_point));
    }
    check(maxerror < 0.002 && log(s.state.bodies[0].q).y > 0.01, "hinged door failed");
    std::cout << "door_angle=" << log(s.state.bodies[0].q).y << " anchor_error=" << maxerror
              << '\n';
}
void ccd() {
    System s;
    Body b;
    b.p = {0, 0, 0.5};
    b.radius = 0.02;
    b.v = {0, 0, -120};
    s.add_body(b);
    s.terrain.build({{{-2, -2, 0}, {2, -2, 0}, {0, 2, 0}, 7}});
    auto r = advance(s);
    check(s.state.bodies[0].p.z >= 0.0199, "high-speed sphere tunneled through triangle");
    check(r.ccd_hits > 0, "CCD not exercised");
    std::cout << "fast_sphere_z=" << s.state.bodies[0].p.z << " hits=" << r.ccd_hits << '\n';
    System caps;
    Body root;
    root.fixed = false;
    root.p = {-0.5, 0, 0.5};
    root.radius = 0;
    caps.add_body(root);
    caps.add_rod({0, {}, {}}, exp(V3{0, pi / 2, 0}), 1, 0.02, 0.02, 4, 0);
    for (auto &body : caps.state.bodies)
        body.v = {0, 0, -120};
    caps.terrain = s.terrain;
    auto a = advance(caps);
    for (const auto &body : caps.state.bodies)
        check(body.p.z >= 0.0198, "swept capsule tunneled through triangle");
    check(a.ccd_hits > 0, "capsule CCD not exercised");
    for (bool rod : {false, true}) {
        System boxworld;
        Body obstacle;
        obstacle.fixed = true;
        obstacle.box = true;
        obstacle.half_extents = {2, 2, 0.01};
        auto target = boxworld.add_body(obstacle);
        (void)target;
        Body mover;
        mover.p = {rod ? -0.5 : 0, 0, 0.5};
        mover.radius = 0.02;
        auto root_id = boxworld.add_body(mover);
        if (rod)
            boxworld.add_rod({root_id, {}, {}}, exp(V3{0, pi / 2, 0}), 1, 0.02, 0.02, 4, 0);
        for (std::size_t j = 1; j < boxworld.state.bodies.size(); ++j)
            boxworld.state.bodies[j].v = {0, 0, -120};
        std::cout << "OBB CCD rod=" << rod << std::endl;
        auto result = advance(boxworld);
        for (std::size_t j = 1; j < boxworld.state.bodies.size(); ++j)
            check(boxworld.state.bodies[j].p.z >= 0.0298,
                  "fast sphere/capsule tunneled through OBB");
        check(result.ccd_hits > 0, "dynamic OBB CCD not exercised");
    }
    System separated;
    Body box;
    box.box = true;
    box.half_extents = {0.05, 0.05, 0.05};
    box.p = {0, 0.06, 0};
    separated.add_body(box);
    separated.terrain.build(
        {{{-1, 0, -1}, {-1, 0, 1}, {1, 0, 1}, 0}, {{-1, 0, -1}, {1, 0, 1}, {1, 0, -1}, 1}});
    auto contacts = separated.build_contacts(std::vector<double>(6, 0), octo::fixed_dt);
    check(contacts.empty(), "OBB circumsphere generated false terrain contact");
}
void moving() {
    System s;
    Body body;
    body.radius = 0.04;
    body.mass = 2;
    s.add_body(body);
    Plane plane;
    plane.offset = -0.08;
    s.planes.push_back(plane);
    Body obstacle;
    obstacle.fixed = true;
    obstacle.box = true;
    obstacle.half_extents = {0.08, 0.08, 0.05};
    obstacle.p = {0, 0, 1.3635};
    s.add_body(obstacle);
    for (int arm = 0; arm < 8; ++arm) {
        Q q = exp(V3{0, 2 * pi * arm / 8, 0});
        auto ids = s.add_rod({0, q.rotate({0, 0, 0.3}), {}}, q, 1, 0.075, 0.012, 24, arm);
        for (unsigned j = 0; j < 16; ++j) {
            Cup c;
            c.a = {ids[j + 2], {0, -0.08, 0}, {}};
            c.command = true;
            c.query_surface = true;
            c.plane = 0;
            c.static_point = s.point(c.a);
            s.state.cups.push_back(c);
        }
    }
    s.self_contact = true;
    std::vector<V3> start;
    for (const auto &b : s.state.bodies)
        start.push_back(b.p);
    std::vector<double> times;
    double max_motion = 0, max_pen = 0, max_pressure = 0;
    unsigned active_min = 128, max_contacts = 0;
    std::vector<unsigned> flips(128, 0), window(128, 0);
    std::vector<CupPhase> phases(128, CupPhase::Free);
    for (unsigned tick = 0; tick < 7200; ++tick) {
        double time = tick * octo::fixed_dt;
        for (auto &e : s.state.elements) {
            int arm = s.state.bodies[e.b.body].arm;
            e.command[4] = 0.01 * (1 + std::sin(time * 2 * pi / 3 + arm * 0.2));
        }
        for (std::size_t j = 0; j < s.state.cups.size(); ++j)
            s.state.cups[j].command = !(tick > 240 && (tick / 120) % 4 == 2 && j % 16 == 0);
        auto r = advance(s);
        times.push_back(r.cpu_ms);
        max_contacts = std::max(max_contacts, r.contacts);
        max_pen = std::max(max_pen, r.penetration);
        max_pressure = std::max(max_pressure, r.pressure_error);
        unsigned active = 0;
        std::array<unsigned, 8> per_arm{};
        for (std::size_t j = 0; j < s.state.bodies.size(); ++j) {
            check(finite(s.state.bodies[j].p) && finite(s.state.bodies[j].v),
                  "moving soak nonfinite");
            max_motion = std::max(max_motion, norm(s.state.bodies[j].p - start[j]));
        }
        for (std::size_t j = 0; j < 128; ++j) {
            const auto &c = s.state.cups[j];
            if (c.phase != phases[j]) {
                ++flips[j];
                ++window[j];
                phases[j] = c.phase;
            }
            if (c.phase == CupPhase::Attached || c.phase == CupPhase::Peeling) {
                ++active;
                ++per_arm[j / 16];
            }
            if (!c.command)
                check(c.force_limit == 0 && c.normal_multiplier == 0 &&
                          norm(c.tangent_multiplier) == 0,
                      "moving soak stale adhesion");
        }
        if (tick % 120 == 119) {
            for (auto count : window)
                check(count < 5, "cup chatter >=5 changes in one second");
            std::fill(window.begin(), window.end(), 0);
        }
        if (tick > 240)
            for (auto count : per_arm)
                check(count >= 8, "an arm lost simultaneous attachment");
        if (tick > 240)
            active_min = std::min(active_min, active);
        if (tick % 600 == 0)
            std::cout << "moving tick=" << tick << " active=" << active << " step_ms=" << r.cpu_ms
                      << std::endl;
        check(max_pen < 0.002 && r.volume_error < 0.02, "moving soak penetration/volume");
    }
    check(max_motion > 0.0001 && active_min >= 64 && max_contacts > 0,
          "soak did not move/attach/collide sufficiently");
    for (auto n : flips)
        check(n < 300, "cup state chatter >5/s");
    for (auto &c : s.state.cups)
        c.command = false;
    advance(s);
    for (const auto &c : s.state.cups)
        check(c.force_limit == 0 && c.normal_multiplier == 0 && norm(c.tangent_multiplier) == 0,
              "final release residual force");
    std::sort(times.begin(), times.end());
    std::cout << "moving_seconds=60 motion=" << max_motion << " penetration=" << max_pen
              << " pressure_error=" << max_pressure << " active_min=" << active_min
              << " p50_ms=" << times[times.size() / 2]
              << " p95_ms=" << times[times.size() * 95 / 100] << '\n';
}
void capsules() {
    System s;
    Body a;
    a.radius = 0;
    a.p = {-0.5, 0, 0.03};
    auto i = s.add_body(a);
    Body b;
    b.radius = 0;
    b.p = {0, -0.5, -0.03};
    auto j = s.add_body(b);
    s.add_rod({i, {}, {}}, exp(V3{0, pi / 2, 0}), 1, 0.02, 0.02, 4, 0);
    s.add_rod({j, {}, {}}, exp(V3{-pi / 2, 0, 0}), 1, 0.02, 0.02, 4, 1);
    for (auto &body : s.state.bodies) {
        if (body.arm == 0 || &body == &s.state.bodies[i])
            body.v = {0, 0, -0.04};
        else
            body.v = {0, 0, 0.04};
    }
    s.self_contact = true;
    V3 p0 = s.linear_momentum();
    unsigned contacts = 0;
    double penetration = 0;
    for (unsigned tick = 0; tick < 120; ++tick) {
        auto r = advance(s);
        contacts = std::max(contacts, r.contacts);
        penetration = std::max(penetration, r.penetration);
    }
    check(contacts > 0 && penetration < 0.002 && norm(s.linear_momentum() - p0) < 1e-5,
          "capsule collision/reaction failed");
    std::cout << "capsule_contacts=" << contacts << " penetration=" << penetration << '\n';
}
void peeling() {
    auto s = carrier();
    auto &body = s.state.bodies[0];
    body.box = false;
    body.radius = 0.001;
    body.p.y = 0.02;
    s.state.cups[0].a.local.y = -0.02;
    for (unsigned j = 0; j < 240; ++j)
        advance(s);
    double previous = s.state.cups[0].force_limit;
    check(previous > 1, "peel fixture not sealed");
    s.state.cups[0].pump_rate = 0;
    s.planes[0].conductance = 0;
    body.force = {0, previous * 0.5, 0};
    body.torque = {0.01, 0, 0};
    bool ordered = false;
    unsigned released = 0;
    auto folder = octo::json::path_utf8(OCTO_SOURCE_DIR) / "docs/progress/physics";
    std::filesystem::create_directories(folder);
    std::ofstream csv(folder / "V07-peel.csv");
    csv << "time_s,force_N,seal_fraction,damage_0,damage_6,pressure_Pa\n";
    for (unsigned j = 0; j < 120; ++j) {
        auto r = advance(s);
        const auto &c = s.state.cups[0];
        ordered |= c.damage[0] > c.damage[6] + 1e-6;
        check(c.force_limit <= previous + 1e-7, "eccentric peel force increased");
        previous = c.force_limit;
        if (c.force_limit == 0)
            ++released;
        csv << j * octo::fixed_dt << ',' << c.force_limit << ',' << c.seal_fraction << ','
            << c.damage[0] << ',' << c.damage[6] << ',' << c.pressure << '\n';
        check(r.pressure_error < 0.01, "peel pressure/load mismatch");
    }
    check(ordered && released > 0, "geometric sequential eccentric peel missing");
    s.state.cups[0].command = false;
    advance(s);
    const auto &c = s.state.cups[0];
    check(c.force_limit == 0 && c.normal_multiplier == 0 && norm(c.tangent_multiplier) == 0,
          "released peel retained force");
    std::cout << "eccentric_geometric_peeling=true released_ticks=" << released << '\n';
}
void hessian() {
    System s;
    Body b;
    b.fixed = true;
    b.radius = 0;
    s.add_body(b);
    s.add_rod({0, {}, {}}, Q{}, 1, 0.04, 0.04, 4, 0);
    s.state.bodies[2].p.x += 0.002;
    s.state.bodies[3].q = exp(V3{0.01, 0.02, -0.01});
    std::vector<double> x(s.state.bodies.size() * 6), direction(x.size()), plus(x.size()),
        minus(x.size()), gp, gm;
    double eps = 1e-7;
    for (std::size_t j = 0; j < x.size(); ++j) {
        direction[j] = std::sin(0.31 * j);
        plus[j] = eps * direction[j];
        minus[j] = -eps * direction[j];
    }
    auto exact = s.hessian_action(x, octo::fixed_dt, direction);
    s.objective(plus, octo::fixed_dt, &gp);
    s.objective(minus, octo::fixed_dt, &gm);
    double error = 0, scale = 1;
    for (std::size_t j = 0; j < x.size(); ++j) {
        error = std::max(error, std::abs(exact[j] - (gp[j] - gm[j]) / (2 * eps)));
        scale = std::max(scale, std::abs(exact[j]));
    }
    check(error / scale < 1e-6, "exact sparse AD Hessian vs independent force derivative");
    std::cout << "hessian_relative_error=" << error / scale << '\n';
}
void replay_io() {
    using namespace octo;
    using J = json::Value;
    check(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA256 empty");
    check(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA256 abc");
    check(json::Parser("18446744073709551615").parse().u64() == 18446744073709551615ull,
          "lossless uint64 JSON");
    for (const char *bad : {"{\"x\":1,\"x\":2}", "NaN", "1e999", "[1,]"}) {
        bool rejected = false;
        try {
            (void)json::Parser(bad).parse();
        } catch (const std::exception &) {
            rejected = true;
        }
        check(rejected, "bad JSON accepted");
    }
    const auto root = json::path_utf8(OCTO_SOURCE_DIR);
    auto scene = io::load_scene(root / "tests/scenes/V12_runtime.json");
    auto inputs = io::load_inputs(json::load(root / "tests/scenes/V12_inputs.json"));
    auto document = io::record(scene, inputs, 1200);
    auto final = io::replay(scene, document);
    check(final.state.tick == 1200, "input replay duration");
    auto invalid = document;
    std::get<J::Object>(invalid.data)["schema_version"] = J::integer(999);
    bool rejected = false;
    try {
        (void)io::replay(scene, invalid);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "unknown replay version accepted");
    invalid = document;
    std::get<J::Object>(invalid.data)["profile_sha256"] = J(std::string(64, '0'));
    rejected = false;
    try {
        (void)io::replay(scene, invalid);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "wrong profile hash accepted");
    auto a = scene.world;
    auto apply_inputs = [&](System &world) {
        for (const auto &input : inputs)
            if (input.tick == world.state.tick)
                io::apply(world, input);
    };
    for (unsigned tick = 0; tick < 120; ++tick) {
        apply_inputs(a);
        advance(a);
    }
    std::ostringstream state;
    a.save(state);
    System b;
    std::istringstream saved(state.str());
    b.restore(saved);
    check(a.state_hash() == b.state_hash(), "state/cache restore mismatch");
    for (unsigned tick = 0; tick < 1200; ++tick) {
        apply_inputs(a);
        apply_inputs(b);
        advance(a);
        advance(b);
    }
    check(a.state_hash() == b.state_hash(), "10s restored continuation mismatch");
    auto contact = carrier();
    contact.gravity = {0, -9.81, 0};
    for (unsigned j = 0; j < 240; ++j)
        advance(contact);
    contact.state.bodies[0].force = {1, 0, 0};
    advance(contact);
    std::ostringstream cache;
    contact.save(cache);
    System restored;
    std::istringstream saved_cache(cache.str());
    restored.restore(saved_cache);
    check(contact.state_hash() == restored.state_hash(), "coupled contact/cup cache roundtrip");
    for (unsigned j = 0; j < 1200; ++j) {
        advance(contact);
        advance(restored);
    }
    check(contact.state_hash() == restored.state_hash(),
          "10s coupled cache restore continuation mismatch");
    std::filesystem::create_directories(root / "docs/progress/physics");
    json::save(root / "docs/progress/physics/V12-input-replay.json", document);
    std::cout << "input_replay_ticks=1200 checkpoint_continuation_seconds=10 identical_hash="
              << a.state_hash() << '\n';
}
int main(int argc, char **argv) {
    std::map<std::string, std::function<void()>> tests{
        {"V06_geometry", seal}, {"V07", peeling},      {"V08", sliding},
        {"door", hinge},        {"ccd", ccd},          {"V10", moving},
        {"capsules", capsules}, {"V12_io", replay_io}, {"hessian", hessian}};
    try {
        check(argc == 2 && tests.contains(argv[1]), "unknown P2 test");
        tests.at(argv[1])();
        std::cout << argv[1] << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
