# 007 — Experimental monolithic torso FEM and GameReady visual adapter

**Status: P2V.1 prototype implemented; full V16–V21 NOT accepted.**

## Implemented

- `core/src/torso.cpp`: a seven-node, eight-positive-tetrahedron
  deformable octahedron centered on the native rod root.
- `core/src/dynamics.cpp`: mixed neo-Hookean element energies and gradients
  assembled into the same Newton solve as the eight Cosserat arms and dynamic
  box. Pressure is locally condensed by the element Schur complement; this is
  **not** a fully pressure-stabilized production mixed FEM discretization.
- Tissue mass and buoyant volume from lumped tetra volumes are added once.
  A retained root mass accounts for beak/core plus already assembled rod
  contributions. Three compliant interface tethers transmit root rotation
  torque and its equal/opposite reaction to FEM surface nodes.
- `core/src/constraints.cpp`: construct triangle-face surface contact
  stencils (center and edge midpoints) using the *current FEM vertices*.
  Ground-plane/terrain constraints act on up to three barycentric nodes; a
  rigid box receives the opposite fourth Jacobian contribution.
  This supplements, rather than replaces, node contacts.
- Checkpoint v4 persists/restores the FEM reference mesh, node-body mapping
  and assumed material. Legacy v2/v3 continue to load.
- Live RPC `--torso-fem` is an opt-in C++ single-world mode, returns seven
  torso vertices with each snapshot, and keeps the legacy rigid comparison.
  The uploaded GameReady GLB enables opt-in mode in the Godot demo;
  F3 toggles a diagnostic eight-triangle rendering of the exact FEM boundary.
- GLB skinned arm deformation uses the 192 bone pose chains and a stable
  native-arm to art-arm mapping. Model import is conditional on the actual
  binary at `game/assets/Octopus_GameReady_v5.glb`.
- Headless native test verifies deformable response, positive tet determinants,
  checkpoint round-trip, total mass, face manifold and box reaction stencil,
  and root torque response. A 120-tick live test checks the native FEM protocol.

## Current validation

Windows Actions:
- Run https://github.com/songhh06/OCTO/actions/runs/43591573923:
  baseline FEM.torso_monolithic and 120-tick live.torso_protocol **passed**.
- Subsequent surface-contact and root-attachment changes are exercised in the
  latest PR #2 CI. Do not claim those tests passed until the newer run is green.
- A small synthetic 193-joint triangle GLB is generated in CI to exercise
  Skeleton3D import, 18-GPU-morph generation, bone posing and FEM boundary
  visualization. This was successful at commit `0dcbe9c`:
  https://github.com/songhh06/OCTO/actions/runs/43594758175.
  **It is not a visual/physics acceptance test of the user's actual GLB.**

## Missing to meet user requirements

1. A real 500–1,500-tetra mantle/head/root mesh from a validated closed
   collision surface with material regions, boundary quality, pressure
   stabilization and adaptive timestepping. The current eight tets are
   **only an early physics prototype**.
2. Production-quality volumetric embedding is not implemented. The Godot
   adapter now builds 18 GPU relative-blendshape channels from six FEM
   boundary node translations and applies them to Root-weighted body vertices
   while preserving mesh materials and 192 arm bones. This directional
   boundary interpolation is an **engineering approximation**, not a
   tetrahedral barycentric embedding; deformation error, normal correction
   and silhouette/contact distances are not yet measured against the actual
   uploaded art. The separate eight-triangle debug overlay shows the true
   coarse FEM collision boundary.
3. A collision-capable deformable octopus arm cross section (rod+cage),
   deformation-dependent sucker rim/normal/pressure, chamber hydraulic
   pressure, beak narrow-gap passability and closed 60 s body contact tests.
4. V16–V21 full acceptance, 120 Hz performance and physical/material
   (BIO) calibration have not passed.

Parameters `shear=5000 Pa`, `bulk=100000 Pa`, `interface stiffness=120 N/m`
are **ASSUMED engineering placeholders**, not measured octopus tissue constants.

Run test from a clean Windows checkout:

```powershell
. ./tools/dev-shell.ps1
cmake --preset release
cmake --build --preset release --parallel 4
ctest --test-dir build/release -R 'FEM.torso_monolithic|live.torso_protocol' --output-on-failure
```

For actual supplied GLB, place the binary in `game/assets/`, build native and run
`run_live_demo.cmd`. To exercise the full skin/pose path in headless Godot,
execute `godot --headless --path game --editor --quit`, then
`godot --headless --path game res://scenes/live_demo.tscn -- --live-model-test`.
Expected marker: `LIVE_MODEL_SKIN_OK`.
