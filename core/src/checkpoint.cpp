#include "octo/dynamics.hpp"
#include <iomanip>
#include <istream>
#include <iterator>
#include <locale>
#include <sstream>

namespace octo::dynamics {
namespace {
void write(std::ostream &o, V3 v) { o << v.x << ' ' << v.y << ' ' << v.z << ' '; }
void write(std::ostream &o, Q v) { o << v.w << ' ' << v.x << ' ' << v.y << ' ' << v.z << ' '; }
void write(std::ostream &o, Anchor a) {
    o << a.body << ' ';
    write(o, a.local);
    write(o, a.rotation);
}
void read(std::istream &i, V3 &v) { i >> v.x >> v.y >> v.z; }
void read(std::istream &i, Q &v) { i >> v.w >> v.x >> v.y >> v.z; }
void read(std::istream &i, Anchor &a) {
    i >> a.body;
    read(i, a.local);
    read(i, a.rotation);
}
std::uint64_t hash(const std::string &s) {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}
} // namespace
void System::save(std::ostream &out) const {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << std::setprecision(17);
    o << "OCTO_DYNAMICS 3 " << state.tick << ' ' << state.seed << ' ';
    o << material.young << ' ' << material.shear << ' ' << material.density << ' '
      << material.sigma_max << ' ' << material.tau_on << ' ' << material.tau_off << ' '
      << material.damping_time << ' ' << material.min_stretch << ' ';
    o << solver.max_newton << ' ' << solver.max_cg << ' ' << solver.max_line_search << ' '
      << solver.force_tolerance << ' ' << solver.relative_tolerance << ' ' << solver.fd_epsilon
      << ' ';
    write(o, gravity);
    write(o, water_velocity);
    o << water_density << ' ' << drag_perp << ' ' << drag_parallel << ' ' << self_contact << ' ';
    o << state.bodies.size() << ' ' << state.elements.size() << ' ' << state.cups.size() << ' '
      << links.size() << ' ' << planes.size() << ' ' << terrain.triangles().size() << '\n';
    for (const auto &b : state.bodies) {
        write(o, b.p);
        write(o, b.q);
        write(o, b.v);
        write(o, b.w);
        write(o, b.inertia);
        write(o, b.force);
        write(o, b.torque);
        o << b.mass << ' ' << b.radius << ' ' << b.volume << ' ' << b.fixed << ' ' << b.arm << ' '
          << b.station << ' ' << b.box << ' ';
        write(o, b.half_extents);
        o << b.reference_s << ' ';
        write(o, b.inertia_offdiag);
        o << '\n';
    }
    for (const auto &e : state.elements) {
        write(o, e.a);
        write(o, e.b);
        o << e.ds << ' ' << e.radius0 << ' ' << e.stretch << ' ' << e.radius << ' ';
        for (double c : e.command)
            o << c << ' ';
        for (double a : e.activation)
            o << a << ' ';
        write(o, e.active_n);
        write(o, e.active_m);
        o << '\n';
    }
    for (const auto &c : state.cups) {
        write(o, c.a);
        write(o, c.b);
        write(o, c.static_point);
        write(o, c.normal);
        o << c.target_static << ' ' << c.command << ' ' << static_cast<int>(c.phase) << ' '
          << c.radius << ' ' << c.external_pressure << ' ' << c.pressure << ' ' << c.capacitance
          << ' ' << c.pump_rate << ' ' << c.pressure_max << ' ' << c.vapor_pressure << ' '
          << c.seal_time << ' ' << c.seal_fraction << ' ' << c.force_limit << ' ' << c.rim_stiffness
          << ' ' << c.peel_rate << ' ' << c.gap_reference << ' ' << c.gap_conductance << ' '
          << c.material.friction << ' ' << c.material.sealability << ' ' << c.material.conductance
          << ' ';
        for (double a : c.gaps)
            o << a << ' ';
        for (double a : c.damage)
            o << a << ' ';
        o << '\n';
    }
    for (const auto &l : links) {
        write(o, l.a);
        write(o, l.b);
        write(o, l.static_point);
        o << l.target_static << ' ' << l.rest << ' ' << l.stiffness << ' ' << l.capacity << '\n';
    }
    for (const auto &p : planes) {
        write(o, p.normal);
        o << p.offset << ' ' << p.stiffness << '\n';
    }
    for (const auto &t : terrain.triangles()) {
        write(o, t.a);
        write(o, t.b);
        write(o, t.c);
        o << t.id << '\n';
    }
    o << "COUPLED " << coupled_constraints << ' ' << solver.max_outer << ' '
      << solver.constraint_penalty << ' ' << solver.contact_margin << ' '
      << solver.constraint_tolerance << ' ' << solver.dual_tolerance << ' '
      << solver.pressure_tolerance << ' ' << state.contacts.size() << ' ' << state.hinges.size()
      << '\n';
    o << "SURFACES " << water_surface << ' ' << solver.exact_hessian << '\n';
    for (const auto &b : state.bodies)
        o << b.friction << ' ' << b.sealability << ' ' << b.conductance << ' ';
    o << '\n';
    for (const auto &t : terrain.triangles())
        o << t.friction << ' ' << t.sealability << ' ' << t.conductance << ' ';
    o << '\n';
    for (const auto &p : planes)
        o << p.sealability << ' ' << p.conductance << ' ';
    o << '\n';
    for (const auto &c : state.cups) {
        o << c.query_surface << ' ' << c.plane << ' ';
        write(o, c.rim_axis);
        o << c.normal_multiplier << ' ';
        write(o, c.tangent_multiplier);
        write(o, c.tangent_origin);
        o << '\n';
    }
    for (const auto &p : planes)
        o << p.friction << ' ';
    o << '\n';
    for (const auto &c : state.contacts) {
        o << c.key << ' ' << c.count << ' ';
        for (unsigned j = 0; j < 4; ++j) {
            write(o, c.anchors[j]);
            write(o, c.normal_anchors[j]);
            o << c.weights[j] << ' ';
        }
        write(o, c.fixed_point);
        write(o, c.normal_fixed_point);
        o << c.normal_offset << ' ';
        write(o, c.normal);
        write(o, c.tangent_origin);
        write(o, c.tangent_multiplier);
        o << c.normal_multiplier << ' ' << c.friction << '\n';
    }
    for (const auto &joint : state.hinges) {
        write(o, joint.a);
        write(o, joint.b);
        o << joint.target_static << ' ';
        write(o, joint.static_point);
        write(o, joint.axis_a);
        write(o, joint.axis_b);
        write(o, joint.position_multiplier);
        write(o, joint.axis_multiplier);
        o << '\n';
    }
    auto text = o.str();
    out << text << "CHECKSUM " << hash(text) << '\n';
}
void System::restore(std::istream &input) {
    std::string text((std::istreambuf_iterator<char>(input)), {});
    if (text.size() > 50 * 1024 * 1024)
        throw std::invalid_argument("checkpoint exceeds 50 MiB");
    const auto separator = text.rfind("CHECKSUM ");
    if (separator == std::string::npos)
        throw std::invalid_argument("checkpoint checksum missing");
    const std::string payload = text.substr(0, separator);
    std::istringstream tail(text.substr(separator + 9));
    std::uint64_t expected{};
    tail >> expected;
    std::string trailing;
    if (!tail || tail >> trailing || hash(payload) != expected)
        throw std::invalid_argument("checkpoint checksum mismatch");
    std::istringstream i(payload);
    i.imbue(std::locale::classic());
    std::string magic;
    unsigned version{};
    System next;
    i >> magic >> version;
    if (magic != "OCTO_DYNAMICS" || (version != 2 && version != 3))
        throw std::invalid_argument("checkpoint version mismatch");
    i >> next.state.tick >> next.state.seed;
    auto &m = next.material;
    i >> m.young >> m.shear >> m.density >> m.sigma_max >> m.tau_on >> m.tau_off >>
        m.damping_time >> m.min_stretch;
    auto &s = next.solver;
    i >> s.max_newton >> s.max_cg >> s.max_line_search >> s.force_tolerance >>
        s.relative_tolerance >> s.fd_epsilon;
    read(i, next.gravity);
    read(i, next.water_velocity);
    i >> next.water_density >> next.drag_perp >> next.drag_parallel >> next.self_contact;
    std::size_t nb{}, ne{}, nc{}, nl{}, np{}, nt{};
    i >> nb >> ne >> nc >> nl >> np >> nt;
    if (nb > 10000 || ne > 10000 || nc > 10000 || nl > 10000 || np > 10000 || nt > 200000)
        throw std::invalid_argument("checkpoint count limit exceeded");
    next.state.bodies.resize(nb);
    next.state.elements.resize(ne);
    next.state.cups.resize(nc);
    next.links.resize(nl);
    next.planes.resize(np);
    for (auto &b : next.state.bodies) {
        read(i, b.p);
        read(i, b.q);
        read(i, b.v);
        read(i, b.w);
        read(i, b.inertia);
        read(i, b.force);
        read(i, b.torque);
        i >> b.mass >> b.radius >> b.volume >> b.fixed >> b.arm >> b.station >> b.box;
        read(i, b.half_extents);
        i >> b.reference_s;
        read(i, b.inertia_offdiag);
    }
    for (auto &e : next.state.elements) {
        read(i, e.a);
        read(i, e.b);
        i >> e.ds >> e.radius0 >> e.stretch >> e.radius;
        for (auto &c : e.command)
            i >> c;
        for (auto &a : e.activation)
            i >> a;
        read(i, e.active_n);
        read(i, e.active_m);
    }
    for (auto &c : next.state.cups) {
        int phase{};
        read(i, c.a);
        read(i, c.b);
        read(i, c.static_point);
        read(i, c.normal);
        i >> c.target_static >> c.command >> phase >> c.radius >> c.external_pressure >>
            c.pressure >> c.capacitance >> c.pump_rate >> c.pressure_max >> c.vapor_pressure >>
            c.seal_time >> c.seal_fraction >> c.force_limit >> c.rim_stiffness >> c.peel_rate >>
            c.gap_reference >> c.gap_conductance >> c.material.friction >> c.material.sealability >>
            c.material.conductance;
        for (auto &a : c.gaps)
            i >> a;
        for (auto &a : c.damage)
            i >> a;
        if (phase < 0 || phase > 5)
            throw std::invalid_argument("invalid cup phase");
        c.phase = static_cast<CupPhase>(phase);
    }
    for (auto &l : next.links) {
        read(i, l.a);
        read(i, l.b);
        read(i, l.static_point);
        i >> l.target_static >> l.rest >> l.stiffness >> l.capacity;
    }
    for (auto &p : next.planes) {
        read(i, p.normal);
        i >> p.offset >> p.stiffness;
    }
    std::vector<collision::Triangle> triangles(nt);
    for (auto &t : triangles) {
        read(i, t.a);
        read(i, t.b);
        read(i, t.c);
        i >> t.id;
    }
    next.terrain.build(std::move(triangles));
    if (version == 3) {
        std::size_t contacts{}, hinges{};
        std::string tag;
        i >> tag >> next.coupled_constraints >> s.max_outer >> s.constraint_penalty >>
            s.contact_margin >> s.constraint_tolerance >> s.dual_tolerance >>
            s.pressure_tolerance >> contacts >> hinges;
        if (tag != "COUPLED" || contacts > 100000 || hinges > 10000)
            throw std::invalid_argument("invalid coupled checkpoint counts");
        i >> tag >> next.water_surface >> s.exact_hessian;
        if (tag != "SURFACES")
            throw std::invalid_argument("missing surface metadata");
        for (auto &b : next.state.bodies)
            i >> b.friction >> b.sealability >> b.conductance;
        auto mesh = next.terrain.triangles();
        for (auto &t : mesh)
            i >> t.friction >> t.sealability >> t.conductance;
        next.terrain.build(std::move(mesh));
        for (auto &p : next.planes)
            i >> p.sealability >> p.conductance;
        for (auto &c : next.state.cups) {
            i >> c.query_surface >> c.plane;
            read(i, c.rim_axis);
            i >> c.normal_multiplier;
            read(i, c.tangent_multiplier);
            read(i, c.tangent_origin);
        }
        for (auto &p : next.planes)
            i >> p.friction;
        next.state.contacts.resize(contacts);
        for (auto &c : next.state.contacts) {
            i >> c.key >> c.count;
            for (unsigned j = 0; j < 4; ++j) {
                read(i, c.anchors[j]);
                read(i, c.normal_anchors[j]);
                i >> c.weights[j];
            }
            read(i, c.fixed_point);
            read(i, c.normal_fixed_point);
            i >> c.normal_offset;
            read(i, c.normal);
            read(i, c.tangent_origin);
            read(i, c.tangent_multiplier);
            i >> c.normal_multiplier >> c.friction;
            if (c.count > 4)
                throw std::invalid_argument("invalid contact stencil");
        }
        next.state.hinges.resize(hinges);
        for (auto &joint : next.state.hinges) {
            read(i, joint.a);
            read(i, joint.b);
            i >> joint.target_static;
            read(i, joint.static_point);
            read(i, joint.axis_a);
            read(i, joint.axis_b);
            read(i, joint.position_multiplier);
            read(i, joint.axis_multiplier);
        }
    } else
        next.coupled_constraints = false;
    if (!i)
        throw std::invalid_argument("truncated checkpoint");
    i >> std::ws;
    if (!i.eof())
        throw std::invalid_argument("unexpected checkpoint data");
    m.validate();
    for (const auto &b : next.state.bodies) {
        System validator;
        validator.add_body(b);
    }
    auto valid_anchor = [&](Anchor a) {
        if (a.body >= nb || !math::finite(a.local))
            throw std::invalid_argument("invalid checkpoint anchor");
    };
    for (const auto &e : next.state.elements) {
        valid_anchor(e.a);
        valid_anchor(e.b);
        if (!(e.ds > 0 && e.radius0 > 0 && e.stretch > 0 && e.radius > 0))
            throw std::invalid_argument("invalid checkpoint rod");
        for (double a : e.activation)
            if (!std::isfinite(a) || a < 0 || a > 1)
                throw std::invalid_argument("invalid activation");
        for (double c : e.command)
            if (!std::isfinite(c) || c < 0 || c > 1)
                throw std::invalid_argument("invalid muscle command");
    }
    for (const auto &c : next.state.cups) {
        valid_anchor(c.a);
        if (!c.target_static)
            valid_anchor(c.b);
        Cup validation = c;
        validation.advance(1e-9);
    }
    for (const auto &l : next.links) {
        valid_anchor(l.a);
        if (!l.target_static)
            valid_anchor(l.b);
    }
    for (const auto &c : next.state.contacts) {
        if (c.count == 0 || c.count > 4 || c.normal_multiplier < 0 || c.friction < 0 ||
            std::abs(math::norm(c.normal) - 1) > 1e-6)
            throw std::invalid_argument("invalid checkpoint contact");
        for (unsigned j = 0; j < c.count; ++j) {
            valid_anchor(c.anchors[j]);
            valid_anchor(c.normal_anchors[j]);
        }
    }
    for (const auto &c : next.state.cups)
        if (c.plane < -1 || (c.plane >= 0 && static_cast<std::size_t>(c.plane) >= np) ||
            std::abs(math::norm(c.rim_axis) - 1) > 1e-6)
            throw std::invalid_argument("invalid checkpoint rim/plane");
    if (s.max_outer == 0 || s.max_newton == 0 || s.max_cg == 0 ||
        !(s.constraint_penalty > 0 && s.constraint_tolerance > 0 && s.pressure_tolerance > 0))
        throw std::invalid_argument("invalid checkpoint solver settings");
    for (const auto &joint : next.state.hinges) {
        valid_anchor(joint.a);
        if (!joint.target_static)
            valid_anchor(joint.b);
    }
    *this = std::move(next);
}
std::uint64_t System::state_hash() const {
    std::ostringstream s;
    save(s);
    return hash(s.str());
}
} // namespace octo::dynamics
