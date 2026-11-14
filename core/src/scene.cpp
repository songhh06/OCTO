#include "octo/scene.hpp"
#include "octo/sha256.hpp"
#include <set>
#include <sstream>

namespace octo::io {
using namespace dynamics;
using namespace math;
using J = json::Value;
namespace {
V3 vector(const J &v) {
    const auto &a = v.array();
    if (a.size() != 3)
        throw std::invalid_argument("vec3 required");
    return {a[0].number(), a[1].number(), a[2].number()};
}
Q quaternion(const J &v) {
    const auto &a = v.array();
    if (a.size() != 4)
        throw std::invalid_argument("xyzw quaternion required");
    Q q{a[3].number(), a[0].number(), a[1].number(), a[2].number()};
    double n = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    if (std::abs(n - 1) > 1e-6)
        throw std::invalid_argument("non-unit quaternion");
    return q;
}
void version(const J &v, unsigned expected) {
    if (v.at("schema_version").u64() != expected)
        throw std::invalid_argument("schema version mismatch");
}
double positive(double n) {
    if (!(n > 0))
        throw std::invalid_argument("positive SI parameter required");
    return n;
}
std::string architecture() {
#if defined(_M_X64) || defined(__x86_64__)
    return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#else
    return "other";
#endif
}
} // namespace
Scene load_scene(const std::filesystem::path &path) {
    Scene result;
    auto text = json::read(path);
    auto document = json::Parser(text).parse();
    version(document, 2);
    result.scene_sha256 = sha256(text);
    if (document.at("unit_system").string() != "SI" ||
        document.at("coordinate_system").string() != "RH_Y_UP")
        throw std::invalid_argument("unsupported scene units/axes");
    const auto profile_path =
        path.parent_path() / octo::json::path_utf8(document.at("octopus_profile").string());
    auto profile_text = json::read(profile_path);
    result.profile_sha256 = sha256(profile_text);
    auto profile = json::Parser(profile_text).parse();
    version(profile, 1);
    auto &world = result.world;
    world.gravity = vector(document.at("gravity"));
    world.water_density = document.at("water").get("density", 0);
    world.water_velocity = vector(document.at("water").at("velocity"));
    world.water_surface = document.at("water").get("surface_y", 0);
    if (world.water_density < 0)
        throw std::invalid_argument("negative water density");
    world.state.seed = document.has("seed") ? document.at("seed").u64() : 42;
    const auto &mat = profile.at("material");
    auto parameter = [&](const char *name, const char *unit) {
        const auto &p = mat.at(name);
        if (p.at("unit").string() != unit)
            throw std::invalid_argument("profile SI unit mismatch");
        double n = p.at("value").number();
        if (p.has("range")) {
            const auto &r = p.at("range").array();
            if (r.size() != 2 || n < r[0].number() || n > r[1].number())
                throw std::invalid_argument("profile parameter outside declared range");
        }
        return n;
    };
    auto &m = world.material;
    m.young = parameter("young", "Pa");
    m.shear = parameter("shear", "Pa");
    m.density = parameter("density", "kg/m^3");
    m.sigma_max = parameter("sigma_max", "Pa");
    m.tau_on = parameter("tau_on", "s");
    m.tau_off = parameter("tau_off", "s");
    m.damping_time = parameter("damping_time", "s");
    m.min_stretch = parameter("min_stretch", "1");
    m.validate();
    if (profile.has("solver")) {
        const auto &s = profile.at("solver");
        auto &cfg = world.solver;
        auto count = [&](const char *name, unsigned fallback, unsigned maximum) {
            auto n = s.has(name) ? s.at(name).u64() : fallback;
            if (n == 0 || n > maximum)
                throw std::invalid_argument("solver iteration limit invalid");
            return static_cast<unsigned>(n);
        };
        cfg.max_newton = count("max_newton", cfg.max_newton, 256);
        cfg.max_cg = count("max_cg", cfg.max_cg, 4096);
        cfg.max_outer = count("max_outer", cfg.max_outer, 128);
        cfg.fd_epsilon = positive(s.get("hessian_fd_step_m_and_rad", cfg.fd_epsilon));
        cfg.constraint_penalty =
            positive(s.get("constraint_penalty_N_per_m", cfg.constraint_penalty));
        cfg.exact_hessian = s.flag("exact_hessian", true);
        if (s.get("fixed_dt_s", octo::fixed_dt) != octo::fixed_dt)
            throw std::invalid_argument("profile fixed_dt mismatch");
    }
    Cup cup_defaults;
    if (profile.has("cup")) {
        const auto &c = profile.at("cup");
        auto assign = [&](const char *name, double &field) {
            field = positive(c.get(name, field));
        };
        assign("radius_m", cup_defaults.radius);
        assign("external_pressure_Pa", cup_defaults.external_pressure);
        cup_defaults.pressure = cup_defaults.external_pressure;
        assign("pressure_max_Pa", cup_defaults.pressure_max);
        assign("vapor_pressure_Pa", cup_defaults.vapor_pressure);
        assign("capacitance_m3_per_Pa", cup_defaults.capacitance);
        assign("pump_m3_per_s", cup_defaults.pump_rate);
        assign("gap_conductance_m3_per_Pa_s", cup_defaults.gap_conductance);
        assign("gap_reference_m", cup_defaults.gap_reference);
        assign("rim_stiffness_N_per_m", cup_defaults.rim_stiffness);
        assign("peel_rate_per_s", cup_defaults.peel_rate);
        if (c.at("rim_samples").u64() != 12)
            throw std::invalid_argument("coupled model requires twelve rim samples");
    }
    if (profile.has("fluid")) {
        const auto &f = profile.at("fluid");
        world.drag_perp = positive(f.get("rod_drag_perpendicular", world.drag_perp));
        world.drag_parallel = positive(f.get("rod_drag_parallel", world.drag_parallel));
    }
    std::map<std::uint64_t, CupMaterial> materials;
    for (const auto &v : document.at("surface_materials").array()) {
        auto id = v.at("id").u64();
        CupMaterial material;
        material.friction = v.at("friction").number();
        material.sealability = v.at("sealability").number();
        material.conductance = v.at("leak_conductance").number();
        if (material.friction < 0 || material.sealability < 0 || material.sealability > 1 ||
            material.conductance < 0 || v.at("roughness").number() < 0 ||
            v.at("peel_energy").number() <= 0)
            throw std::invalid_argument("invalid surface material");
        if (!materials.emplace(id, material).second)
            throw std::invalid_argument("duplicate material ID");
        (void)v.at("grade").string();
        (void)v.at("source").string();
    }
    std::map<std::uint64_t, std::size_t> ids, plane_ids;
    std::vector<collision::Triangle> triangles;
    for (const auto &v : document.at("dynamic_bodies").array()) {
        Body b;
        auto id = v.at("id").u64();
        b.mass = positive(v.at("mass").number());
        auto material = materials.at(v.at("material_id").u64());
        b.friction = material.friction;
        b.sealability = material.sealability;
        b.conductance = material.conductance;
        b.p = vector(v.at("pose").at("position"));
        b.q = quaternion(v.at("pose").at("rotation_xyzw"));
        b.v = vector(v.at("linear_velocity"));
        b.w = vector(v.at("angular_velocity"));
        b.fixed = v.flag("fixed", false);
        b.volume = v.get("volume", 0);
        const auto &inertia = v.at("inertia_body").array();
        if (inertia.size() != 3)
            throw std::invalid_argument("inertia tensor required");
        std::array<V3, 3> I{vector(inertia[0]), vector(inertia[1]), vector(inertia[2])};
        if (std::abs(I[0].y - I[1].x) > 1e-12 || std::abs(I[0].z - I[2].x) > 1e-12 ||
            std::abs(I[1].z - I[2].y) > 1e-12)
            throw std::invalid_argument("inertia must be symmetric");
        b.inertia = {I[0].x, I[1].y, I[2].z};
        b.inertia_offdiag = {I[0].y, I[0].z, I[1].z};
        auto shape = v.at("shape");
        if (shape.at("type").string() == "box") {
            b.box = true;
            b.half_extents = vector(shape.at("half_extents"));
            b.radius = norm(b.half_extents);
        } else if (shape.at("type").string() == "sphere")
            b.radius = positive(shape.at("radius").number());
        else
            throw std::invalid_argument("unsupported convex shape");
        auto index = world.add_body(b);
        if (!ids.emplace(id, index).second)
            throw std::invalid_argument("duplicate body ID");
    }
    for (const auto &v : document.at("static_colliders").array()) {
        auto material = materials.at(v.at("material_id").u64());
        auto type = v.at("type").string();
        auto id = v.at("id").u64();
        if (type == "plane") {
            Plane p;
            p.normal = vector(v.at("normal"));
            if (std::abs(norm(p.normal) - 1) > 1e-6)
                throw std::invalid_argument("plane normal must be unit");
            p.offset = v.at("offset").number();
            p.friction = material.friction;
            p.sealability = material.sealability;
            p.conductance = material.conductance;
            if (!plane_ids.emplace(id, world.planes.size()).second)
                throw std::invalid_argument("duplicate plane ID");
            world.planes.push_back(p);
        } else if (type == "triangles") {
            for (const auto &triangle : v.at("triangles").array()) {
                collision::Triangle t;
                t.a = vector(triangle.at("a"));
                t.b = vector(triangle.at("b"));
                t.c = vector(triangle.at("c"));
                auto feature = triangle.at("feature_id").u64();
                if (feature > 0xffffffffull)
                    throw std::invalid_argument("triangle feature ID overflow");
                t.id = static_cast<std::uint32_t>(feature);
                t.friction = material.friction;
                t.sealability = material.sealability;
                t.conductance = material.conductance;
                triangles.push_back(t);
            }
        } else
            throw std::invalid_argument("unknown static collider");
    }
    world.terrain.build(std::move(triangles));
    std::map<int, std::vector<std::size_t>> arms;
    if (document.has("octopus")) {
        const auto &o = document.at("octopus");
        auto root = ids.at(o.at("body_id").u64());
        auto count = o.at("arms").u64();
        if (count != 1 && count != 8)
            throw std::invalid_argument("octopus requires one test arm or eight arms");
        const auto &g = profile.at("geometry");
        auto resolution = g.at("elements").u64();
        if (resolution < 2 || resolution > 128)
            throw std::invalid_argument("rod resolution outside [2,128]");
        unsigned segments = static_cast<unsigned>(resolution);
        double length = positive(g.at("length_m").number()),
               r0 = positive(g.at("proximal_radius_m").number()),
               r1 = positive(g.at("distal_radius_m").number());
        double ring = o.get("root_radius", 0.3);
        if (ring < 0)
            throw std::invalid_argument("negative root radius");
        for (unsigned arm = 0; arm < count; ++arm) {
            Q local = exp(V3{0, 2 * pi * arm / count, 0});
            const auto &body = world.state.bodies[root];
            Q frame = body.q * local;
            V3 offset = local.rotate({0, 0, count == 1 ? 0 : ring});
            arms[static_cast<int>(arm)] = world.add_rod({root, offset, Q{}}, frame, length, r0, r1,
                                                        segments, static_cast<int>(arm));
        }
    }
    auto anchor = [&](const J &v) {
        Anchor a;
        if (v.has("arm")) {
            auto arm = static_cast<int>(v.at("arm").u64());
            auto station = v.at("station").u64();
            a.body = arms.at(arm).at(static_cast<std::size_t>(station));
        } else
            a.body = ids.at(v.at("body_id").u64());
        if (v.has("local"))
            a.local = vector(v.at("local"));
        if (v.has("rotation_xyzw"))
            a.rotation = quaternion(v.at("rotation_xyzw"));
        return a;
    };
    if (document.has("cups"))
        for (const auto &v : document.at("cups").array()) {
            Cup c = cup_defaults;
            c.a = anchor(v.at("a"));
            c.command = v.flag("command", false);
            c.radius = positive(v.get("radius", c.radius));
            c.query_surface = true;
            if (v.has("plane_id")) {
                c.plane = static_cast<int>(plane_ids.at(v.at("plane_id").u64()));
                const auto &p = world.planes[static_cast<std::size_t>(c.plane)];
                c.material = {p.friction, p.sealability, p.conductance};
                c.normal = p.normal;
                c.static_point =
                    world.point(c.a) - p.normal * (dot(world.point(c.a), p.normal) - p.offset);
            } else if (v.has("b")) {
                c.b = anchor(v.at("b"));
                c.target_static = false;
                const auto &b = world.state.bodies[c.b.body];
                c.material = {b.friction, b.sealability, b.conductance};
            } else if (v.flag("terrain", false)) {
                auto hit = world.terrain.closest(world.point(c.a), 0.02);
                if (!hit.found)
                    throw std::invalid_argument("cup has no terrain surface");
                c.normal = hit.surface_normal;
                c.static_point = hit.point;
            } else
                throw std::invalid_argument("cup target required");
            if (v.has("rim_axis"))
                c.rim_axis = vector(v.at("rim_axis"));
            if (std::abs(norm(c.rim_axis) - 1) > 1e-6)
                throw std::invalid_argument("rim axis must be unit");
            world.state.cups.push_back(c);
        }
    if (document.has("hinges"))
        for (const auto &v : document.at("hinges").array()) {
            Hinge joint;
            joint.a = anchor(v.at("a"));
            if (v.has("b")) {
                joint.b = anchor(v.at("b"));
                joint.target_static = false;
            } else
                joint.static_point = vector(v.at("static_point"));
            joint.axis_a = vector(v.at("axis_a"));
            joint.axis_b = vector(v.at("axis_b"));
            if (std::abs(norm(joint.axis_a) - 1) > 1e-6 || std::abs(norm(joint.axis_b) - 1) > 1e-6)
                throw std::invalid_argument("hinge axis must be unit");
            world.state.hinges.push_back(joint);
        }
    world.self_contact = document.flag("self_contact", true);
    (void)document.at("objectives").array();
    return result;
}
std::vector<Intent> load_inputs(const J &document) {
    version(document, 2);
    std::vector<Intent> out;
    std::set<std::pair<std::uint64_t, int>> seen;
    std::uint64_t last = 0;
    for (const auto &v : document.at("events").array()) {
        Intent i;
        i.tick = v.at("tick").u64();
        auto arm = v.at("arm").u64();
        if (arm > 7)
            throw std::invalid_argument("arm outside [0,7]");
        i.arm = static_cast<int>(arm);
        auto values = v.at("activation").array();
        if (values.size() != 7)
            throw std::invalid_argument("seven muscle commands required");
        for (unsigned j = 0; j < 7; ++j) {
            i.activation[j] = values[j].number();
            if (i.activation[j] < 0 || i.activation[j] > 1)
                throw std::invalid_argument("activation outside [0,1]");
        }
        i.suction = v.at("suction").boolean();
        if (i.tick < last || !seen.emplace(i.tick, i.arm).second)
            throw std::invalid_argument("input order/duplicate tick-arm");
        last = i.tick;
        out.push_back(i);
    }
    return out;
}
void apply(System &world, const Intent &intent) {
    if (intent.tick != world.state.tick)
        throw std::invalid_argument("input tick mismatch");
    bool found = false;
    for (auto &e : world.state.elements)
        if (world.state.bodies[e.b.body].arm == intent.arm) {
            e.command = intent.activation;
            found = true;
        }
    if (!found)
        throw std::invalid_argument("unknown arm in input");
    for (auto &c : world.state.cups)
        if (world.state.bodies[c.a.body].arm == intent.arm)
            c.command = intent.suction;
}
std::string build_id() {
#ifdef OCTO_BUILD_ID
    return OCTO_BUILD_ID;
#else
    return "unconfigured-build";
#endif
}
J record(Scene scene, const std::vector<Intent> &inputs, unsigned ticks) {
    if (ticks == 0 || ticks > 72000)
        throw std::invalid_argument("replay duration outside limit");
    auto &world = scene.world;
    std::ostringstream saved;
    world.save(saved);
    J::Array events, checks;
    for (const auto &i : inputs) {
        J::Array a;
        for (double v : i.activation)
            a.emplace_back(v);
        events.emplace_back(J::Object{{"tick", J::integer(i.tick)},
                                      {"arm", J::integer(static_cast<std::uint64_t>(i.arm))},
                                      {"activation", J(std::move(a))},
                                      {"suction", J(i.suction)}});
    }
    std::size_t cursor = 0;
    for (unsigned j = 0; j < ticks; ++j) {
        while (cursor < inputs.size() && inputs[cursor].tick == world.state.tick)
            apply(world, inputs[cursor++]);
        auto result = world.step();
        if (!result.converged) {
            std::ofstream failure("work/replay-failure.octostate");
            world.save(failure);
            throw std::runtime_error("recording solver failure at tick " +
                                     std::to_string(world.state.tick));
        }
        if ((j + 1) % 120 == 0 || j + 1 == ticks)
            checks.emplace_back(J::Object{{"tick", J::integer(world.state.tick)},
                                          {"hash", J(std::to_string(world.state_hash()))}});
    }
    if (cursor != inputs.size())
        throw std::invalid_argument("input outside recording duration");
    return J(J::Object{{"schema_version", J::integer(2)},
                       {"core_build_id", J(build_id())},
                       {"scene_sha256", J(scene.scene_sha256)},
                       {"profile_sha256", J(scene.profile_sha256)},
                       {"numeric_backend", J("cxx20-newton-cg-al-v3")},
                       {"cpu_arch", J(architecture())},
                       {"seed", J::integer(world.state.seed)},
                       {"fixed_dt", J(octo::fixed_dt)},
                       {"snapshot_version", J::integer(3)},
                       {"initial_state", J(saved.str())},
                       {"ticks", J::integer(ticks)},
                       {"events", J(std::move(events))},
                       {"checksums", J(std::move(checks))}});
}
System replay(const Scene &scene, const J &document) {
    version(document, 2);
    if (document.at("core_build_id").string() != build_id() ||
        document.at("numeric_backend").string() != "cxx20-newton-cg-al-v3" ||
        document.at("cpu_arch").string() != architecture() ||
        document.at("scene_sha256").string() != scene.scene_sha256 ||
        document.at("profile_sha256").string() != scene.profile_sha256 ||
        document.at("fixed_dt").number() != octo::fixed_dt ||
        document.at("snapshot_version").u64() != 3)
        throw std::invalid_argument("replay build/scene/profile/backend/architecture mismatch");
    System world;
    std::istringstream initial(document.at("initial_state").string());
    world.restore(initial);
    if (world.state_hash() != scene.world.state_hash() ||
        document.at("seed").u64() != world.state.seed)
        throw std::invalid_argument("replay initial state/seed mismatch");
    auto inputs = load_inputs(document);
    auto ticks = document.at("ticks").u64();
    if (ticks == 0 || ticks > 72000)
        throw std::invalid_argument("replay duration outside limit");
    std::map<std::uint64_t, std::string> checks;
    for (const auto &c : document.at("checksums").array())
        if (!checks.emplace(c.at("tick").u64(), c.at("hash").string()).second)
            throw std::invalid_argument("duplicate state checksum");
    std::size_t cursor = 0;
    for (std::uint64_t j = 0; j < ticks; ++j) {
        while (cursor < inputs.size() && inputs[cursor].tick == world.state.tick)
            apply(world, inputs[cursor++]);
        if (!world.step().converged)
            throw std::runtime_error("replay solve failed");
        if (auto it = checks.find(world.state.tick);
            it != checks.end() && it->second != std::to_string(world.state_hash()))
            throw std::runtime_error("replay state checksum mismatch at tick " +
                                     std::to_string(world.state.tick));
    }
    if (cursor != inputs.size() || !checks.contains(world.state.tick))
        throw std::invalid_argument("replay missing final checksum or unconsumed events");
    return world;
}
} // namespace octo::io
