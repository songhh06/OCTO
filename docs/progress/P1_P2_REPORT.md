> Historical partial implementation report. The later native P2 completion work
> and final acceptance evidence are in [P2_COMPLETION.md](P2_COMPLETION.md).
> P0/P1 live integration remains open; do not read historical P2 gaps as current.

# P1/P2 implementation and validation report

2026-11-14 / branch remains feature/P0-bootstrap (new branch write denied).
**P1: native mechanics implemented and V01–V05 tested; formal acceptance OPEN.**
**P2: native modules and specified subsets tested; formal acceptance OPEN.**
This is not a completed P2 game or biologically validated simulation.

## Implemented

- Godot-independent C++20 SO(3), local automatic differentiation, tapered
  24-element Cosserat energy and force/moment assembly, root reaction coupling.
- Seven muscle channels, implicit on/off activation, force-length/velocity
  factors, stretch-dependent circular radius and reference-volume accounting.
- Backward-Euler incremental Newton-CG solve, line search, 1/2/4 retry and
  atomic rollback; symmetric body-frame inertia and gyroscopic predictor.
- Cup pressure/seal/12-rim damage state machine, bounded tensile/tangential
  attachment potentials differentiated against rod and target DOFs together.
- Sphere/plane/OBB/triangle contact and static triangle BVH; initial node-level
  self-contact. Implicit quadratic directional rod drag, body drag and buoyancy.
- Native checkpoint v2 with complete state and deterministic roundtrip; native
  state trace CLI and a Godot offline viewer. Original visual preview preserved.

## Acceptance by roadmap task

| Task | Status and evidence |
|---|---|
| P1.1 | PASS native tests: 0/pi SO(3), quaternion sign, AD gradient vs independent finite difference |
| P1.2 | PASS tested rest/taper and 24-element uniform beam reference, nodal mass/inertia assembly |
| P1.3 | PARTIAL: Newton-CG/line search/rollback and analytic implicit spring check pass; final dimensionless residual and 12/24/32 convergence study pending |
| P1.4 | PASS tested channel direction, activation, extension/1D volume and torque sign/balance; material values ASSUMED |
| P1.5 | PARTIAL: Godot renders actual native snapshots; compiled live GDExtension input/physics path is not available |
| P2.1 | PARTIAL: sphere/OBB/triangle BVH tested; swept capsule, dynamic AABB tree, CCD and full cached feature contact pending |
| P2.2 | PARTIAL: pressure/rim model tests pass; gaps are supplied by fixtures, not full terrain/cup geometry query |
| P2.3 | PARTIAL: bounded attachment in the same global solve; active-set Coulomb/complementarity and <1% pressure/peel outer consistency pending |
| P2.4 | PARTIAL: 6-DOF root/body/box reciprocal response and mass dependence tested; hinge doors not implemented |
| P2.5 | PARTIAL: fluid analytic case passes; eight-arm REST soak passes, moving capsule/self-contact stress pending |
| P2.6 | PARTIAL: native checkpoint roundtrip and 10-second continuation pass; general scene/profile loader and tick-input replay contract pending |
| P2.7 | MEASURED prototype workloads only; representative V13 scene and target not established |

## Executed validation

Final common command: `./tools/verify.ps1`.

| Configuration / check | Result |
|---|---|
| MSVC dev | 17/17 selected tests passed, 8.89 s |
| MSVC Release | 23/23 tests passed, 72.10 s |
| MSVC AddressSanitizer | 17/17 selected tests passed, 17.43 s, no reported sanitizer error |
| clang-format dry-run / clang-tidy analyzer | passed |
| Godot 4.6.2 P0 import + empty scene | passed, original behavior preserved |
| Native trace renderer | OpenGL execution, PHYSICS_LAB_READY and capture marker; actual screenshot inspected |

The six expensive dynamic/soak tests (V03,V04,V05,V09_rod,V10_rest,V12) are run
in full in Release; dev and ASan exclude the explicit `long` label. No test was
deleted and no physical tolerance was widened. The same script is referenced
by the prepared CI; remote CI itself has not run and still fails the P0 tool gate.
Raw per-build logs are in this directory's physics/ folder.

## Measured physics results

| Test ID | Observation | Limit of conclusion |
|---|---|---|
| physics.math | max force/torque gradient discrepancy 2.36254e-11 | finite-difference comparison on a perturbed rod, not every possible pose |
| physics.V01 | 10 s rest, no drift/energy increase | unloaded world only |
| physics.V02 | tip deflection 0.00277184 m vs beam 0.00277305 m; relative error 0.04393% | small-load uniform beam reference; taper has no claimed closed-form comparison |
| physics.V03 | opposite channels give tip X +0.041561 / -0.041561 m | sign/finite-state test |
| physics.V04 | length 1.01236 m; algebraic 1D volume error 0 | circular hydrostat approximation, not tissue det(F) measurement |
| physics.V05 | opposite twists -0.105598 / +0.105598 rad; reported internal torque residual 0 | tested loads and orientations |
| physics.V06 / V07 | pressure falls on sealing, recovers on gaps; ordered eccentric rim damage; released adhesion 0 | supplied rim gaps, model-level tests |
| physics.V08_model | glass/porous force and friction-capacity direction correct | **not integrated surface sliding; V08 not accepted** |
| physics.V09_rod | 5 s: linear momentum relative error <=1.23e-6, angular <=1.81e-5 | closed body/rod/box experiment; no external fluid/terrain forces |
| physics.V09_rod | 0.5 kg box: body +1.651 mm, box -6.440 mm; 8 kg: body +7.344 mm, box -5.581 mm | expected mass-dependent reciprocal motion |
| physics.V10_rest | 8 arms, 192 elements, 128 attached fixture cups, 60 s finite; volume error 0 | **resting case only; moving V10 not accepted** |
| physics.V11 | terminal speed 0.790268 m/s equals analytical reference at printed precision | sphere body test; no CFD/added mass |
| physics.V12 | identical complete state hash after native checkpoint restore and 10 s continuation | same executable/CPU; not a versioned input-event replay acceptance |
| physics.free_spin | anisotropic torque-free angular momentum error 0.1988% over 5 s | finite-step first-order rotational approximation |
| physics.implicit_exact / rollback | linear spring implicit solution matches; exhausted retries preserve complete state hash | exercised analytic/failure paths |

Only V01–V05 and the explicitly described V06/V07/V11/native-checkpoint
subcases are supported. Do not treat the count of passing CTests as V01–V12
full scenario acceptance.

## Performance and environment

Windows 10.0.26200 x64; AMD Ryzen 5 5600X (12 logical processors), RTX 4060,
MSVC 19.43.34809, CMake 3.31.7 / Ninja 1.12.1. Native physics is CPU-only.

Eight-arm resting fixture, Release: step p50 3.4389 ms, p95 4.3886 ms measured
during the 7200-tick test. No 20 moving objects, 100k triangle mesh or 300 active
collisions were present. This is **not V13 acceptance**. Single-arm moving coupled
fixture has p95 around 12 ms (see latest coupled-stats.json), exceeding the
8.33 ms tick budget even before full eight-arm active work. Optimization and
general stress stability remain open. GPU/frame p95 and 10-minute memory growth
were not measured. The physics lab is offline playback, so its render rate does
not establish real-time physics throughput.

## Run and inspect

`run_physics_lab.cmd` opens the most recently generated native trace. Exact trace
generation and scenario commands are in docs/PHYSICS_LAB.md. Reproduction:

```powershell
. ./tools/dev-shell.ps1
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure --no-tests=error
./tools/physics-lab.ps1 -Scenario coupled -Command 0.04 -Capture
```

Latest real viewport screenshot: docs/preview/native_physics_lab.png.
SHA256 of audit profile, executable and trace: physics/manifest.json.
All materials and geometric/muscle/cup constants remain **ASSUMED**.

## Remaining environment and implementation blockers

P0 official archive/tool locking, compatible godot-cpp/live bridge, GitHub DNS
and Git metadata write permission remain unresolved. The root commit remains
4541a5975d62143c3cbf51a2c7a8cdd3adb81aac. No new branch/implementation commit,
remote push, release tag, BIO approval or human gameplay acceptance exists.

Separately, the unimplemented P2 solver/collision/replay items above are actual
engineering work, not consequences of Git or a claim that all physics is finished.
See docs/issues/003-physics-acceptance-gaps.md for the remaining tasks and resolved
collision/convergence failures. Stage completion is deliberately left open.
