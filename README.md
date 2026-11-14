# OCTOPUS

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

**Visual octopus preview:** double-click `run_preview.cmd` (installed Godot 4.6.2).
This separate scene has eight animated arms, 128 suckers and an orbit camera.
It is a visual prototype, not completed physics. See [docs/PREVIEW.md](docs/PREVIEW.md)
for controls, exact commands and actual rendered screenshots. The original P0
startup scene and independent C++ work are preserved.

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
