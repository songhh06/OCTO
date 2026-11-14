# Native physics implementation (experimental)

Implementation date: 2026-11-14. This documents code, not biological validation.
The acceptance contract remains GAME_SPEC.md. Current P2 evidence is in
progress/P2_COMPLETION.md. P0/P1 integration remains separately open.

## Code and public API

`core/include/octo/math.hpp`: Vec, quaternion, Exp/Log, sign-continuous quaternion
midpoint, first-order Dual and second-order Jet2 automatic differentiation, up to
24 local contact DOFs.
`core/include/octo/dynamics.hpp`: Material, Body, Anchor, Element, Cup, Link,
Plane, System, State, StepResult. All lengths/masses/times are SI, right handed,
+Y up. Rotational increments and angular velocities are world-space; inertia
is a symmetric body-space tensor (diagonal plus Ixy/Ixz/Iyz). Internal stress
resultants and material strains use the rod frame. Quaternion C++ storage is
w,x,y,z; JSON render traces use x,y,z,w explicitly.

`System::add_rod` creates 24 elements and 25 positions/directors by default.
The root is an Anchor on a body rather than a second independent rigid body.
Differentiating root anchor position/orientation transfers forces and torques
to the same body's six DOFs. No Godot RigidBody3D controls these states.
There are 25 nodal directors (including endpoint directors), not the original
pseudocode's 24 element-only directors; this is needed by the two-director
curvature discretization in GAME_SPEC.md 2.1.

`step(1/120)` returns convergence, aggregate iteration count, used subdivisions,
residual, current contact count, maximum penetration, volume error, elastic
potential, CPU milliseconds and fixed-support wrench. It rejects other dt.
`equilibrate()` is a zero-velocity static equilibrium solve, not simulated time.
`save`/`restore` serialize native state; `state_hash` is a deterministic FNV-1a64
checksum, not a cryptographic digest. `objective`/`elastic_gradient` expose
verification hooks. Bodies/elements must not be edited concurrently with a step.

## Rod energy and muscle sign convention

For an element, ds is reference length, A0=pi*r0², I=pi*r0⁴/4, J=2I.
Using world anchor poses and quaternion midpoint Rm:

    nu = Rm^T (p_b - p_a) / ds
    kappa = Log(R_a^T R_b) / ds
    U = ds/2 [(nu-ez)^T diag(GA,GA,EA)(nu-ez)
               + kappa^T diag(EI,EI,GJ)kappa]
        + ds [n_active.(nu-ez) + m_active.kappa]

Forces and torques are the negative AD gradient of this energy, including
derivatives through anchor offsets, SO(3), and midpoint normalization. The test
compares every gradient component against independent central energy differences.
Reference stiffness uses A0/I0; geometry radius uses r=r0/sqrt(nu_z).
This makes A*nu_z=A0 identically within roundoff. It verifies the specified 1D
volume approximation; it does not independently establish det(F)=1 in 3D tissue.

Longitudinal channels 0..3 lie at +X,+Y,-X,-Y at 0.6*r0. A tensile force F
contributes n_z=F and m=x cross (0,0,F). Channel 0 therefore bends toward +X;
channel 2 toward -X. Transverse channel 4 has a negative axial energy resultant
and extends the rod. Channels 5/6 produce opposite axial moments plus tensile
axial resultants. Active fiber force is nonnegative. Activation uses the implicit
first-order update (a+h*u/tau)/(1+h/tau), with separate on/off constants.
Force-length and clamped force-velocity factors are evaluated from the previous
completed state. All coefficients are ASSUMED in profiles/dynamics-model.json.

Mass is lumped half to each segment endpoint; each half also receives cylinder
inertia. The root half is added to the body's mass/inertia, including rotated
tensor and parallel-axis terms. Symmetric eight-root and zero-offset single-root
fixtures are tested. Arbitrary asymmetric root mass distributions need a COM
shift/spatial-mass treatment before claiming general rigid-body equivalence.

## P2 coupled implicit solver (2026-11-14)

Fixed dt=1/120 s, SI units, right-handed +Y up. Newton-CG minimizes one
backward-Euler incremental potential for rod/body inertia, elastic and active
energy, dissipation, normal contact, suction preload, tangential friction and
hinges. Exact local second-order Jet2<12/24> AD Hessians are sparse blocks;
independent force derivatives verify relative error <1e-6. The finite-difference
Hessian mode remains available. MSVC native programs reserve an 8 MiB stack:
debug four-body Jet2<24> stencils exceed Windows' default 1 MiB (0xC00000FD).
This stack allocation fixes execution; it does not relax physical tolerances.

Projected augmented Lagrangian: normal compression lambda>=0; tangential lambda
projects onto the ball mu*compression for ordinary contact and
mu*(compression+Fs) for an attached cup. Suction adds Fs*g to the potential.
All interacting DOFs occur in the same objective, including target reaction.
Outer iteration updates active contacts, pressure and rim peeling from the
original tick state, so repeated iterations never advance pressure multiple h.
Acceptance requires unchanged feature keys, geometric violation <2e-5 m,
normalized dual change <.002 and suction force mismatch <1%. Penalty starts
at 1e5 N/m and increases fourfold every four iterations with excess violation,
up to 1e9. Gradient tolerances remain absolute 1e-6 and relative 1e-7.
`friction_error` reports maximum normalized normal/tangential dual update.
Residual still mixes raw force/moment SI components; nondimensional scaling
remains an upstream P1 improvement. Energy roundoff-only plateaus may accept
a gradient-improving iterate.

Failure retries the complete tick state with 1/2/4 substeps. Exhaustion rolls
back everything, leaves tick unchanged, returns failure and writes diagnostics
from the CLI. No pose overwrite, separate force-push pass or silent clamp.

## Geometric suction and fluids

Each cup queries twelve rim points against the actual plane, OBB, sphere or
triangle BVH. Terrain uses outward triangle winding for its surface normal. Cup normals
average the twelve samples; cached tangential reactions project into the
current tangent plane when the normal changes.
Material friction, sealability and leakage follow the contacted surface.
Pressure uses implicit C*dp/dt=G*(p_ext-p)-Q, bounded by vapor/external pressure
and maximum pressure difference. Fs=(p_ext-p)*pi*r^2*remaining seal fraction.
Rim gaps increase leakage cubically; local opening/eccentric load generates
ordered damage. The provisional load/capacity law is ASSUMED, not measured
fracture energy. Release clears all suction and cached adhesion multipliers.
Y-up water depth adds rho*|g|*max(surface_y-center_y,0) to external pressure.
General tilted water surfaces are outside this approximation.

Buoyancy is rho_water*g*volume per lump. Rod drag uses perpendicular/parallel
quadratic midpoint velocities through implicit cubic dissipation. Body drag
uses sphere projected area, coefficient 1. The terminal-speed test compares
an independent analytic solution within 10%; no CFD or added-mass claim.

## Collision and hinge scope

Deterministic rebuilt AABB BVH broadphase and static triangle BVH; dynamic
spheres/OBBs; deformed-radius rod capsules. OBB/OBB SAT, OBB/triangle thirteen
axes, segment/triangle endpoints/edges/face, capsule/OBB endpoints/interior,
four-body capsule self-contact. Conservative advancement covers swept
sphere/capsule versus triangles/OBBs and capsule pairs; 120 m/s thin-obstacle
regressions exercise these paths. Arbitrary convex meshes and fast rotating
OBB/OBB continuous contact remain unsupported; unknown scene shapes fail.
Adjacent/intrinsic reference tube overlap and own proximal root assembly are
excluded to prevent a straight thick rod repelling itself.
Hinges enforce coincident anchors and aligned axes in the same coupled solve.

## Public runtime and save/replay contracts

`octo_replay` loads version-2 SI/RH_Y_UP scenes: full rigid inertia, sphere/OBB,
planes/triangles/materials, one/eight rods, geometric cups and hinges. Original
P0 version-1 schema/fixtures are preserved. See schemas/*-v2.schema.json.
Material values enforce units/ranges. Runtime profile fields: all Material,
rod geometry, max_newton/max_cg/max_outer, Hessian mode/probe, constraint penalty,
cup radius/external pressure/capacitance/pump/pressure cap/vapor pressure/gap
conductance/reference/rim stiffness/peel rate and rod drag coefficients. Other
profile values describe fixed compiled defaults. Surface roughness/peel_energy
are audit metadata; sealability/conductance/peel_rate drive this model.
All coefficients remain ASSUMED; BIO is unvalidated.

Native snapshot OCTO_DYNAMICS 3 contains all geometry/materials/settings, body
poses/velocities/inertia, muscle commands/activation, cup pressure/phase/damage,
contacts, normal/tangential multipliers, hinge multipliers and seed/tick.
Classic locale and 17-digit decimal roundtrip; FNV checksum; temporary-state
validation before replacement. Version 2 migrates to the old uncoupled model.
Version-2 input files have ordered unique tick/arm, seven activation commands
and suction. Replay embeds initial checkpoint, source/profile SHA256, exact
source/compiler/configuration build fingerprint, CPU architecture/backend,
seed/dt and periodic/final state hashes. Mismatches/unknown versions fail.
Determinism is scoped to the same build/configuration and CPU architecture.

```powershell
. ./tools/dev-shell.ps1
cmake --preset release
cmake --build --preset release
build/release/octo_replay.exe --scene tests/scenes/V12_runtime.json --inputs tests/scenes/V12_inputs.json --record work/replay.json --ticks 1200 --checkpoint work/final.octostate
build/release/octo_replay.exe --scene tests/scenes/V12_runtime.json --replay work/replay.json
build/release/octo_replay.exe --scene tests/scenes/P2_eight_cups.json --validate-scene
```

Godot physics_lab.tscn remains labelled OFFLINE PLAYBACK of native snapshots,
with a common visual offset and 2.3x display scale. Decorative preview cups
are hidden because actual rim traces are not exported. Live GDExtension is
still the unresolved P0/P1 integration gate. The separate animated preview
remains a visual prototype and has not been relabelled as validated physics.
