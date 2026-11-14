# P1/P2 work plan — 2026-11-14

User explicitly requested P1 and P2 implementation after the independent visual
preview. Proceed with engine-independent implementation despite unresolved P0
environment gates; do not infer P0 acceptance or lower any V01–V12 criterion.

Preserve empty-world API/tests and visual preview. Add standalone dynamics API,
SO(3), tapered Cosserat energy/force assembly, muscle channels, implicit solver
with failure rollback, pressure/seal/peel and fluid models, coupled body/rod
constraints, versioned snapshots and an engine-independent replay viewer path.

Validation commands: tools/verify.ps1 (dev/release/ASan + regressions),
octo_physics_tests per V ID, release CLI trace generation and Godot playback.
Record solver convergence/fallback, conservation, volume and measured timings.
No scene animation may be labelled as physical simulation without a native trace.

Branch creation attempted; .git metadata write permission remains an environment
blocker. Existing files remain untouched unless a change is documented.
