#include "octo/dynamics.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <sstream>

using namespace octo::dynamics;
using namespace octo::math;
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
void close(double actual, double expected, double tolerance, const char *message) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << message << ": actual=" << actual << " expected=" << expected
                  << " tolerance=" << tolerance << '\n';
        throw std::runtime_error(message);
    }
}
System rod(bool fixed = true, double r0 = 0.075, double r1 = 0.012) {
    System s;
    Body root;
    root.fixed = fixed;
    s.add_body(root);
    s.add_rod({0, {}, {}}, Q{}, 1, r0, r1);
    return s;
}
void step(System &s, unsigned count) {
    for (unsigned j = 0; j < count; ++j) {
        auto r = s.step();
        if (!r.converged) {
            std::cerr << "tick=" << s.state.tick << " residual=" << r.residual
                      << " iterations=" << r.iterations << '\n';
            throw std::runtime_error(r.failure);
        }
    }
}
void math_test() {
    for (double angle : {0.0, 1e-10, 0.3, pi - 1e-8, pi}) {
        V3 a = V3{1, 2, 3} * (angle / std::sqrt(14.0));
        auto q = exp(a);
        auto b = log(q);
        close(norm(b - a), 0, 1e-8, "SO3 inverse");
        close(norm(q.rotate({1, 0, 0})), 1, 1e-12, "SO3 norm");
        close(norm(log(-q) - b), 0, 1e-10, "quaternion sign");
    }
    auto s = rod();
    s.state.bodies[12].p.x += 0.001;
    s.state.bodies[13].q = exp(V3{0.01, 0.02, 0.03});
    std::vector<double> x(s.state.bodies.size() * 6), g;
    s.objective(x, octo::fixed_dt, &g);
    double error = 0;
    for (std::size_t j = 0; j < x.size(); ++j) {
        x[j] = 1e-7;
        double plus = s.objective(x, octo::fixed_dt);
        x[j] = -1e-7;
        double minus = s.objective(x, octo::fixed_dt);
        x[j] = 0;
        error = std::max(error, std::abs((plus - minus) / 2e-7 - g[j]));
    }
    close(error, 0, 1e-5, "AD gradient vs finite differences");
    std::cout << "max_gradient_error=" << error << '\n';
}
void v01() {
    auto s = rod(false);
    auto before = s.state;
    double e = s.elastic_energy();
    step(s, 1200);
    double drift = 0;
    for (std::size_t i = 0; i < before.bodies.size(); ++i)
        drift = std::max(drift, norm(before.bodies[i].p - s.state.bodies[i].p));
    require(drift < 0.002, "V01 drift");
    require(s.elastic_energy() <= e + 1e-12, "V01 energy");
    std::cout << "seconds=10 drift_m=" << drift << '\n';
}
void v02() {
    auto solve = [](double force) {
        auto s = rod(true, 0.04, 0.04);
        s.solver.max_cg = 220;
        s.solver.max_newton = 40;
        s.state.bodies.back().force = {force, 0, 0};
        auto r = s.equilibrate();
        std::cout << "static converged=" << r.converged << " residual=" << r.residual << '\n';
        require(r.converged, "V02 static convergence");
        return s.state.bodies.back().p.x;
    };
    const double f = 0.001, E = 60000, radius = 0.04, I = pi * std::pow(radius, 4) / 4;
    const double expected = f / (3 * E * I) + f / (20000 * pi * radius * radius);
    double d = solve(f), d2 = solve(f * 2);
    std::cout << "deflection=" << d << " beam=" << expected
              << " relative_error=" << std::abs(d / expected - 1) << '\n';
    require(std::abs(d / expected - 1) < 0.1, "V02 beam error");
    require(d2 > d, "V02 monotonicity");
}
void v03() {
    auto run = [](std::size_t channel) {
        auto s = rod();
        for (auto &e : s.state.elements)
            e.command[channel] = 0.06;
        step(s, 60);
        std::cout << "channel=" << channel << " tip_x=" << s.state.bodies.back().p.x << '\n';
        return s.state.bodies.back().p.x;
    };
    const double a = run(0), b = run(2);
    require(a > 0 && b < 0, "V03 opposite bending signs");
}
void v04() {
    auto s = rod();
    for (auto &e : s.state.elements)
        e.command[4] = 0.15;
    step(s, 120);
    double length = 0;
    for (const auto &e : s.state.elements) {
        length += e.stretch * e.ds;
        require(e.radius < e.radius0, "V04 radius decrease");
    }
    require(length > 1, "V04 extension");
    require(s.volume_error() < 0.02, "V04 volume");
    std::cout << "length_m=" << length << " volume_error=" << s.volume_error() << '\n';
}
void v05() {
    auto run = [](std::size_t channel) {
        auto s = rod();
        for (auto &e : s.state.elements)
            e.command[channel] = 0.04;
        step(s, 60);
        auto g = s.elastic_gradient();
        V3 force{}, moment{};
        for (std::size_t i = 0; i < s.state.bodies.size(); ++i) {
            V3 f{-g[6 * i], -g[6 * i + 1], -g[6 * i + 2]},
                t{-g[6 * i + 3], -g[6 * i + 4], -g[6 * i + 5]};
            force += f;
            moment += t + cross(s.state.bodies[i].p, f);
        }
        require(norm(moment) < 1e-6, "V05 internal torque balance");
        std::cout << "channel=" << channel << " twist=" << log(s.state.bodies.back().q).z
                  << " torque_residual=" << norm(moment) << '\n';
        return log(s.state.bodies.back().q).z;
    };
    require(run(5) * run(6) < 0, "V05 opposite twist signs");
}
void v06() {
    Cup c;
    c.command = true;
    for (int j = 0; j < 120; ++j)
        c.advance(octo::fixed_dt);
    double load = c.force_limit, p = c.pressure;
    require(load > 0 && p < c.external_pressure, "V06 suction growth");
    c.gaps.fill(0.005);
    for (int j = 0; j < 120; ++j)
        c.advance(octo::fixed_dt);
    require(c.pressure > p && c.force_limit == 0, "V06 leak release");
    require(c.pressure >= c.vapor_pressure && c.pressure <= c.external_pressure, "V06 bounds");
    std::cout << "sealed_force_N=" << load << " sealed_pressure_Pa=" << p
              << " leaked_pressure_Pa=" << c.pressure << '\n';
}
void v07() {
    Cup c;
    c.command = true;
    for (int j = 0; j < 120; ++j)
        c.advance(octo::fixed_dt);
    c.pump_rate = 0;
    c.material.conductance = 0;
    double previous = c.force_limit;
    for (int j = 0; j < 200; ++j) {
        c.advance(octo::fixed_dt, previous * 1.5, 0.8);
        require(c.force_limit <= previous + 1e-10, "V07 nonmonotone peel");
        previous = c.force_limit;
    }
    require(c.damage[0] > c.damage[6], "V07 eccentric rim ordering");
    c.command = false;
    c.advance(octo::fixed_dt);
    require(c.force_limit == 0 && c.phase == CupPhase::Free, "V07 residual adhesion");
}
void v08() {
    Cup glass, porous;
    glass.command = true;
    porous.command = true;
    porous.material.sealability = 0.2;
    porous.material.conductance = 1e-8;
    for (int j = 0; j < 120; ++j) {
        glass.advance(octo::fixed_dt);
        porous.advance(octo::fixed_dt);
    }
    require(glass.force_limit > porous.force_limit, "V08 material direction");
    require(friction_limit(0, glass.force_limit, 0.7) > friction_limit(0, porous.force_limit, 0.2),
            "V08 friction capacity");
    std::cout << "glass_force=" << glass.force_limit << " porous_force=" << porous.force_limit
              << " (model-only test)\n";
}
void v09() {
    System s;
    Body a, b;
    a.p = {-0.5, 0, 0};
    a.mass = 2;
    a.v = {0.3, 0.2, 0};
    b.p = {0.5, 0, 0};
    b.mass = 1;
    b.v = {-0.1, 0, 0};
    s.add_body(a);
    s.add_body(b);
    Link l;
    l.a = {0, {0, 0.1, 0}, {}};
    l.b = {1, {0, -0.1, 0}, {}};
    l.rest = 0.8;
    l.stiffness = 10;
    s.links.push_back(l);
    const V3 p0 = s.linear_momentum(), l0 = s.angular_momentum();
    step(s, 600);
    double pe = norm(s.linear_momentum() - p0) / norm(p0),
           le = norm(s.angular_momentum() - l0) / std::max(norm(l0), 0.01);
    std::cout << "linear_error=" << pe << " angular_error=" << le
              << " (two rigid bodies, rod case pending)\n";
    require(pe < 0.01 && le < 0.01, "V09 pair conservation");
}
void v09_rod() {
    auto run = [](double mass) {
        auto s = rod(false, 0.04, 0.04);
        s.state.bodies[0].mass += 1;
        Body box;
        box.box = true;
        box.mass = mass;
        box.half_extents = {0.15, 0.15, 0.15};
        box.inertia = V3{1, 1, 1} * (mass * 0.015);
        box.p = {-0.02, 0, 1.20};
        auto target = s.add_body(box);
        Cup c;
        c.a = {24, {0, 0, 0.05}, {}};
        c.b = {target, {0.02, 0, -0.15}, {}};
        c.target_static = false;
        c.command = true;
        c.phase = CupPhase::Attached;
        c.pressure = c.external_pressure - 40000;
        c.force_limit = 40000 * pi * c.radius * c.radius;
        c.material.conductance = 0;
        c.pump_rate = 0;
        c.normal = {0, 0, -1};
        s.state.cups.push_back(c);
        s.self_contact = true;
        for (auto &e : s.state.elements)
            for (std::size_t j = 0; j < 4; ++j)
                e.command[j] = 0.01;
        for (auto &b : s.state.bodies)
            b.v = {0.1, 0.05, 0};
        V3 p0 = s.linear_momentum(), l0 = s.angular_momentum();
        auto origin = s.state.bodies[0].p, box0 = box.p;
        double max_mount_gap = 0;
        for (unsigned tick = 0; tick < 600; ++tick) {
            step(s, 1);
            max_mount_gap = std::max(max_mount_gap, norm(s.point(c.a) - s.point(c.b)));
        }
        require(max_mount_gap < 0.06, "V09 cup mount crossed target box");
        double pe = norm(s.linear_momentum() - p0) / norm(p0),
               le = norm(s.angular_momentum() - l0) / std::max(norm(l0), 0.01);
        double body_move = s.state.bodies[0].p.z - origin.z,
               box_move = s.state.bodies[target].p.z - box0.z;
        std::cout << "box_mass=" << mass << " body_dz=" << body_move << " box_dz=" << box_move
                  << " linear_error=" << pe << " angular_error=" << le
                  << " max_mount_gap=" << max_mount_gap << '\n';
        require(pe < 0.01 && le < 0.01, "V09 rod-body-box momentum");
        require(body_move > 0 && box_move < 0, "V09 bidirectional pull");
        return std::abs(box_move / body_move);
    };
    require(run(0.5) > run(8), "V09 relative movement mass dependence");
}
void v10_rest() {
    System s;
    Body body;
    body.radius = 0.2;
    body.mass = 2;
    s.add_body(body);
    for (int a = 0; a < 8; ++a) {
        double angle = 2 * pi * a / 8;
        Q q = exp(V3{0, angle, 0});
        auto ids = s.add_rod({0, q.rotate({0, 0, 0.3}), {}}, q, 1, 0.075, 0.012, 24, a);
        for (unsigned j = 0; j < 16; ++j) {
            Cup c;
            c.a = {ids[2 + j], {}, Q{}};
            c.static_point = s.point(c.a);
            c.command = true;
            c.normal = {0, 1, 0};
            s.state.cups.push_back(c);
        }
    }
    s.self_contact = true;
    std::vector<double> timings;
    double max_error = 0;
    for (unsigned tick = 0; tick < 7200; ++tick) {
        auto r = s.step();
        if (!r.converged) {
            std::cerr << "tick=" << tick << " residual=" << r.residual << '\n';
            throw std::runtime_error(r.failure);
        }
        timings.push_back(r.cpu_ms);
        max_error = std::max(max_error, r.volume_error);
    }
    require(s.state.cups.size() == 128, "V10 cup count");
    for (const auto &c : s.state.cups)
        require(c.phase == CupPhase::Attached, "V10 attached state");
    std::sort(timings.begin(), timings.end());
    std::cout << "rest_only_8arms_128cups_seconds=60 p50_ms=" << timings[timings.size() / 2]
              << " p95_ms=" << timings[timings.size() * 95 / 100]
              << " max_volume_error=" << max_error << '\n';
}
void contacts() {
    using namespace octo::collision;
    TriangleBvh bvh;
    bvh.build({{{-1, 0, -1}, {1, 0, -1}, {0, 0, 1}, 42}});
    auto h = bvh.closest({0, 0.03, 0}, 0.1);
    require(h.found && h.feature == 42, "stable mesh feature");
    close(h.distance, 0.03, 1e-12, "triangle distance");
    require(h.normal.y > 0, "triangle normal");
    auto pair = closest_segments({-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0});
    close(norm(pair.a - pair.b), 0, 1e-12, "capsule centerline crossing");
    System s;
    Body a;
    a.p = {0, 0.19, 0};
    a.radius = 0.1;
    Body b;
    b.fixed = true;
    b.box = true;
    b.half_extents = {1, 0.1, 1};
    s.add_body(a);
    s.add_body(b);
    s.self_contact = true;
    auto g = s.elastic_gradient();
    require(g[1] < 0, "sphere-box contact pushes upward");
    close(g[1] + g[7], 0, 1e-10, "contact action reaction");
    System mesh;
    mesh.add_body(a);
    mesh.terrain = bvh;
    mesh.state.bodies[0].p.y = 0.09;
    auto mg = mesh.elastic_gradient();
    require(mg[1] < 0, "triangle contact pushes upward");
}
void v11() {
    System s;
    Body b;
    b.mass = 2;
    b.volume = 0.001;
    b.radius = 0.1;
    b.p = {0, 10, 0};
    s.add_body(b);
    s.gravity = {0, -9.81, 0};
    s.water_density = 1000;
    step(s, 3600);
    double expected =
        std::sqrt(2 * (b.mass - 1000 * b.volume) * 9.81 / (1000 * pi * b.radius * b.radius));
    double speed = -s.state.bodies[0].v.y;
    std::cout << "terminal_speed=" << speed << " analytical=" << expected << '\n';
    require(std::abs(speed / expected - 1) < 0.1, "V11 terminal speed");
    auto d = drag_force({2, 0, 0}, {0, 0, 1}, 0.1, 1, 1000);
    auto d2 = drag_force({4, 0, 0}, {0, 0, 1}, 0.1, 1, 1000);
    require(d.x < 0 && d2.x < d.x, "V11 drag direction");
}
void v12() {
    auto a = rod();
    for (auto &e : a.state.elements)
        e.command[0] = 0.01;
    step(a, 20);
    std::ostringstream stream;
    a.save(stream);
    System b;
    std::istringstream source(stream.str());
    b.restore(source);
    require(a.state_hash() == b.state_hash(), "V12 checkpoint hash");
    step(a, 1200);
    step(b, 1200);
    require(a.state_hash() == b.state_hash(), "V12 restored continuation");
    auto hash = b.state_hash();
    std::istringstream bad("OCTO_DYNAMICS 99");
    try {
        b.restore(bad);
        throw std::runtime_error("bad checkpoint accepted");
    } catch (const std::invalid_argument &) {
    }
    require(hash == b.state_hash(), "V12 failed restore atomicity");
}
void rollback() {
    auto s = rod();
    s.solver.max_newton = 0;
    s.state.bodies.back().force = {1, 0, 0};
    auto hash = s.state_hash();
    auto r = s.step();
    require(!r.converged && r.substeps == 4, "fallback missing");
    require(hash == s.state_hash(), "failure changed state");
}
void implicit_exact() {
    System s;
    Body b;
    b.p = {1, 0, 0};
    s.add_body(b);
    Link link;
    link.a = {0, {}, Q{}};
    link.target_static = true;
    link.stiffness = 500;
    s.links.push_back(link);
    step(s, 1);
    const double h = octo::fixed_dt;
    close(s.state.bodies[0].p.x, 1 / (1 + 500 * h * h), 1e-8, "implicit Euler analytic spring");
    Body invalid;
    invalid.inertia_offdiag = {2, 0, 0};
    try {
        s.add_body(invalid);
        throw std::runtime_error("non-SPD accepted");
    } catch (const std::invalid_argument &) {
    }
}
void free_spin() {
    System s;
    Body b;
    b.inertia = {1, 2, 3};
    b.w = {0.2, 0.3, 0.4};
    s.add_body(b);
    V3 initial = s.angular_momentum();
    step(s, 600);
    double error = norm(s.angular_momentum() - initial) / norm(initial);
    std::cout << "free_spin_angular_momentum_error=" << error << '\n';
    require(error < 0.01, "anisotropic free rotation conservation");
}
int main(int argc, char **argv) {
    std::map<std::string, std::function<void()>> tests{{"math", math_test},
                                                       {"V01", v01},
                                                       {"V02", v02},
                                                       {"V03", v03},
                                                       {"V04", v04},
                                                       {"V05", v05},
                                                       {"V06", v06},
                                                       {"V07", v07},
                                                       {"V08_model", v08},
                                                       {"V09_pair", v09},
                                                       {"V09_rod", v09_rod},
                                                       {"V10_rest", v10_rest},
                                                       {"contact", contacts},
                                                       {"V11", v11},
                                                       {"V12", v12},
                                                       {"rollback", rollback},
                                                       {"implicit_exact", implicit_exact},
                                                       {"free_spin", free_spin}};
    try {
        if (argc != 2 || !tests.contains(argv[1]))
            throw std::invalid_argument("choose a test ID");
        tests.at(argv[1])();
        std::cout << argv[1] << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << (argc > 1 ? argv[1] : "arguments") << ": " << e.what() << '\n';
        return 1;
    }
}
