# OCTOPUS

## Start the current GameReady v5 octopus (feature/P2V)

**This experimental model is NOT on `main` yet.** The live Godot project
now launches `res://scenes/live_demo.tscn` by default, but the original
15.2 MB `Octopus_GameReady_v5.glb` is distributed separately. Unzip the
user-provided model installer into the project root, or run its
`INSTALL_AND_RUN.cmd` script to select the correct checkout, fetch and
switch branch, copy/verify the binary, and launch.

The running GameReady model is identified by console lines
`LIVE_MODEL_LOADED skeleton_bones=193` and
`LIVE_FEM_SKIN_BIND_OK ... morphs=18`. The GameReady model must be in
`game/assets/Octopus_GameReady_v5.glb`. If absent, the **interactive
default scene shows a MODEL NOT INSTALLED message**, not the old art.

- `run_live_demo.cmd`: validated original GameReady model + native torso
  C++ rebuild + Godot live physics, with F3 diagnostic FEM body surface.
- `run_preview.cmd`: alias to the same new live demo, so the old
  procedural preview is not mistaken for the active model.
- `run_legacy_preview.cmd`: separate old procedural visual experiment.
- `run_p2_demo.cmd`: separate recorded P2 preview, *not* the new GLB.

```powershell
git fetch origin
git switch feature/P2V-model-curved-contact-body
git pull --ff-only
# Extract the model ZIP here or use its INSTALL_AND_RUN.cmd
python tools/check_octopus_model.py game/assets/Octopus_GameReady_v5.glb
.\run_live_demo.cmd
```

The model-to-torso binding uses **7-node, 8-tetra prototype FEM with
18 directional morph channels**. The original curved 100k-triangle
multicontact case now converges, but fails real-time performance acceptance.
See `docs/issues/004-curved-contact-convergence.md` and
`docs/issues/007-volumetric-torso-model.md`.

**Live box-contact demo:** double-click `run_live_demo.cmd`. Drag the blue box
or use WASD/arrows to push real native arms. See [docs/LIVE_DEMO.md](docs/LIVE_DEMO.md).
This is live asynchronous input/solver state, separate from recorded playback.


**P2 results demo:** double-click `run_p2_demo.cmd`. Five native recorded cases,
case buttons, scrubbing, pause/slow playback and displacement measurements.
See [docs/P2_DEMO.md](docs/P2_DEMO.md). This is recorded playback, not live control.


**Native physics lab:** `run_physics_lab.cmd` plays recorded C++ dynamics.
See [docs/PHYSICS_LAB.md](docs/PHYSICS_LAB.md) for regeneration/controls and
[P1/P2 results](docs/progress/P1_P2_REPORT.md) for exact tested scope.
P2 native contact, suction, eight-arm coupling, hinges and versioned scene/input
replay are implemented. Final V06-V12 acceptance and known limitations are in
[the P2 report](docs/progress/P2_COMPLETION.md). P0/P1 live engine integration
remains open. The native viewer is labelled OFFLINE PLAYBACK.

**Legacy visual octopus preview:** double-click `run_legacy_preview.cmd` (installed Godot 4.6.2).
This separate scene has eight animated arms, 128 suckers and an orbit camera.
It is a visual prototype, not completed physics. See [docs/PREVIEW.md](docs/PREVIEW.md)
for controls, exact commands and actual rendered screenshots. The original P0
bootstrap scene remains available directly as `res://scenes/bootstrap.tscn`;
its old default startup assignment has been changed on this branch.

Fully 3D octopus physics game in development. **P0 is incomplete.** The native model is
experimental and not BIO validated; live playable game integration is pending. Read docs/ROADMAP.md and
docs/progress/STATUS.md for acceptance evidence and blockers.

In PowerShell with MSVC 2022, CMake 3.31.7, Ninja 1.12.1 and Python 3.13.9
(jsonschema 4.25.0), from the repository root:

```powershell
. ./tools/dev-shell.ps1
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure --no-tests=error
./tools/verify.ps1 -RequireCompleteP0
```

The script also runs Release, AddressSanitizer, formatting, static analysis and
Godot headless checks. The strict command is expected to fail while the tool provenance and native
bridge are unresolved. `./tools/direct-check.ps1` exercises native tests and CLI
in debug and optimized builds as a diagnostic fallback; it is not P0 completion.

```powershell
python tools/validate_data.py scene tests/scenes/P0_empty.json
python tools/validate_data.py replay replays/golden/P0_empty.json
godot --headless --single-threaded-scene --render-thread safe --path game --quit-after 3
```

Native extension preparation (not yet verified): obtain the official compatible
godot-cpp commit, verify its source archive hash, configure
`-DOCTO_GODOT_CPP_DIR=<source>`, build octo_bridge, then copy
godot_bridge/octo.gdextension.in to game/octo.gdextension and run the engine with
`-- --bridge-smoke`. Require `P0_BRIDGE_ROUNDTRIP_OK: tick=1` and exit 0.

Origin: https://github.com/Aggang-77/Octopus-simulator.git . Remote history has
not been fetched. Fetch and inspect before integrating; never force push.
