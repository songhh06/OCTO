#pragma once
#include "octo/contact.hpp"
#include "octo/math.hpp"
#include "octo/world.hpp"
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace octo::dynamics {
using math::Q;
using math::V3;
struct Material {
    double young = 60000, shear = 20000, density = 1050;
    double sigma_max = 20000, tau_on = 0.08, tau_off = 0.15, damping_time = 0.01;
    double min_stretch = 0.3;
    void validate() const;
};
struct Body {
    V3 p{}, v{}, w{}, inertia{1, 1, 1}, force{}, torque{};
    Q q{};
    double mass = 1, radius = 0.02, volume = 0;
    bool fixed = false;
    int arm = -1, station = -1;
    bool box = false;
    V3 half_extents{0.1, 0.1, 0.1};
    double reference_s = 0;
    V3 inertia_offdiag{}; // body-frame Ixy, Ixz, Iyz (symmetric tensor)
    double friction = 0.7, sealability = 1, conductance = 1e-13;
};
struct Anchor {
    std::size_t body = 0;
    V3 local{};
    Q rotation{};
};
struct Element {
    Anchor a, b;
    double ds = 1.0 / 24, radius0 = 0.04;
    std::array<double, 7> command{}, activation{};
    V3 active_n{}, active_m{};
    double stretch = 1, radius = 0.04;
};
enum class CupPhase { Free, Touch, Sealing, Attached, Peeling, Leaking };
struct CupMaterial {
    double friction = 0.7, sealability = 1, conductance = 1e-13;
};
struct Cup {
    Anchor a, b;
    bool target_static = true, command = false;
    V3 static_point{}, normal{0, 1, 0};
    double radius = 0.015, external_pressure = 101325, pressure = 101325;
    double capacitance = 1e-10, pump_rate = 1e-6, pressure_max = 40000, vapor_pressure = 2300;
    double seal_time = 0, seal_fraction = 0, force_limit = 0;
    double rim_stiffness = 10000, peel_rate = 8, gap_reference = 0.001, gap_conductance = 1e-10;
    std::array<double, 12> gaps{}, damage{};
    CupMaterial material{};
    CupPhase phase = CupPhase::Free;
    bool query_surface = false;
    int plane = -1;
    V3 rim_axis{0, 1, 0};
    double normal_multiplier = 0;
    V3 tangent_multiplier{}, tangent_origin{};
    void advance(double h, double tension = 0, double eccentricity = 0);
};
struct Link {
    Anchor a, b;
    bool target_static = false;
    V3 static_point{};
    double rest = 0, stiffness = 1000, capacity = 1e30;
};
struct Plane {
    V3 normal{0, 1, 0};
    double offset = 0, stiffness = 1e5;
    double friction = 0.7;
    double sealability = 1, conductance = 1e-13;
};
struct ContactConstraint {
    std::uint64_t key = 0;
    std::array<Anchor, 4> anchors{};
    std::array<Anchor, 4> normal_anchors{};
    std::array<double, 4> weights{};
    unsigned count = 0;
    V3 fixed_point{}, normal{0, 1, 0}, tangent_origin{}, tangent_multiplier{};
    V3 normal_fixed_point{};
    double normal_offset = 0;
    double normal_multiplier = 0, friction = 0.7;
};
struct Hinge {
    Anchor a, b;
    bool target_static = true;
    V3 static_point{}, axis_a{0, 1, 0}, axis_b{0, 1, 0};
    V3 position_multiplier{}, axis_multiplier{};
};
struct SolverSettings {
    unsigned max_newton = 20, max_cg = 200, max_line_search = 24;
    double force_tolerance = 1e-6, relative_tolerance = 1e-7;
    double fd_epsilon = 1e-7;
    bool exact_hessian = true;
    unsigned max_outer = 32;
    double constraint_penalty = 1e5, contact_margin = 0.003;
    double constraint_tolerance = 2e-5, dual_tolerance = 0.002, pressure_tolerance = 0.01;
};
struct StepResult {
    bool converged = false;
    unsigned iterations = 0, substeps = 1, contacts = 0;
    double residual = 0, cpu_ms = 0, volume_error = 0, energy = 0, penetration = 0;
    V3 support_force{}, support_torque{};
    std::string failure;
    unsigned outer_iterations = 0, ccd_hits = 0;
    double pressure_error = 0, constraint_error = 0, friction_error = 0;
};
struct State {
    std::uint32_t version = 3;
    std::uint64_t tick = 0, seed = 42;
    std::vector<Body> bodies;
    std::vector<Element> elements;
    std::vector<Cup> cups;
    std::vector<ContactConstraint> contacts;
    std::vector<Hinge> hinges;
};
class System {
  public:
    std::ostream *diagnostics = nullptr; // Optional runtime log sink, never serialized.
    Material material;
    SolverSettings solver;
    State state;
    std::vector<Link> links;
    std::vector<Plane> planes;
    collision::TriangleBvh terrain;
    V3 gravity{}, water_velocity{};
    double water_density = 0, drag_perp = 1.2, drag_parallel = 0.1;
    double water_surface = 0;
    bool self_contact = false;
    bool coupled_constraints = true;
    std::size_t add_body(Body body);
    std::vector<std::size_t> add_rod(Anchor root, Q frame, double length = 1,
                                     double proximal_radius = 0.075, double distal_radius = 0.012,
                                     unsigned segments = 24, int arm = 0);
    StepResult step(double h = octo::fixed_dt);
    StepResult equilibrate();
    [[nodiscard]] double elastic_energy() const;
    [[nodiscard]] V3 linear_momentum() const;
    [[nodiscard]] V3 angular_momentum() const;
    [[nodiscard]] double volume_error() const;
    double objective(const std::vector<double> &x, double h,
                     std::vector<double> *gradient = nullptr) const;
    [[nodiscard]] std::vector<double> elastic_gradient() const;
    std::vector<double> hessian_action(const std::vector<double> &x, double h,
                                       const std::vector<double> &direction) const;
    void save(std::ostream &out) const;
    void restore(std::istream &in);
    [[nodiscard]] std::uint64_t state_hash() const;
    [[nodiscard]] V3 point(Anchor anchor) const;
    std::vector<ContactConstraint> build_contacts(const std::vector<double> &x, double h,
                                                  unsigned *ccd = nullptr) const;
    void sample_cup(Cup &cup, const std::vector<double> &x) const;

  private:
    bool solve_substep(double h, StepResult &result);
    bool minimize(double h, StepResult &result, std::vector<double> &x);
    bool solve_coupled(double h, StepResult &result);
    void commit_increment(const std::vector<double> &x, double h, StepResult &result);
    bool frozen_contacts_ = false;
    double active_penalty_ = 0;
    double penalty() const {
        return active_penalty_ > 0 ? active_penalty_ : solver.constraint_penalty;
    }
    void prepare(double h);
    void update_geometry();
    void contact_metrics(StepResult &result) const;
    struct HessianBlock {
        std::array<std::size_t, 4> ids{};
        unsigned count = 0;
        std::array<double, 576> matrix{};
    };
    std::vector<HessianBlock> hessian_blocks(const std::vector<double> &x, double h) const;
};
V3 drag_force(V3 relative_velocity, V3 tangent, double diameter, double length, double density,
              double perpendicular = 1.2, double parallel = 0.1);
double friction_limit(double compression, double adhesion, double coefficient);
} // namespace octo::dynamics
