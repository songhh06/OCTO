# P0 environment and acceptance evidence - 2026-11-14

## Environment observed locally

| Item | Observed |
|---|---|
| OS | Windows NT 10.0.26200.0, x64 |
| CPU | AMD Ryzen 5 5600X 6-Core Processor (registry), 12 logical processors |
| GPU | NVIDIA GeForce RTX 4060, 8188 MiB, driver 610.88 (nvidia-smi) |
| RAM | Not established; CIM query access denied |
| Git | 2.49.0.windows.1 |
| CMake / Ninja | 3.31.7 / 1.12.1 |
| C++ | MSVC 19.43.34809.0, toolset directory 14.43.34808, C++20 |
| Python | C:/Anaconda/python.exe 3.13.9; jsonschema 4.25.0 |
| Godot installed | 4.6.2.stable.official.71f334935 |
| Godot required | 4.7.2-stable; not installed or archive-verified |
| clang-format / clang-tidy | 19.1.1 / 19.1.1 |
| Other dependencies | godot-cpp, Eigen, Catch2, Blender unavailable/unverified |

No tool was downloaded or installed. All local executable digests are in
toolchain.lock.json, separately from official archive digests (currently null).
These are observations, not a successfully completed reproducible tool lock.
The Godot 4.7.2 release page was readable through web retrieval, but this does
not provide a locally downloaded verified binary or a compatible godot-cpp build.

## Acceptance matrix

| ID | Result | Evidence / remaining work |
|---|---|---|
| P0.1 | BLOCKED | Environment inventoried; local hashes recorded. Official archive URLs/digests and missing dependency commits not verified; installed engine mismatch. |
| P0.2 | PARTIAL | main initial commit 4541a59, feature/P0-bootstrap, ignore/attributes, license draft, requested origin. Later git add/commit denied at .git/index.lock; implementation remains uncommitted. No remote fetch/push or actual license selection. |
| P0.3 | PARTIAL | C++20 core/tests/CLI, dev/release/ASan each CTest 5/5, formatting/static analysis passed. CI YAML is a fail-closed draft; upstream framework/dependency locking and CI installation pending. |
| P0.4 | BLOCKED | Godot 4.6.2 empty 3D import/run passed, P0_SCENE_READY marker. Native source/template prepared; --bridge-smoke exits 2, P0_BRIDGE_MISSING. No visual acceptance or 4.7.2 compatibility evidence. |
| P0.5 | PASS for P0 format scope | Versioned scene/replay schemas; SI/Y-up, finite values, SPD inertia, quaternion/ID/material and tick validation. 5 Python test methods including invalid cases. Runtime C++ loader and replay playback are not implemented. |

**P0 is not accepted. No P1-P4 feature or V01-V14 acceptance is claimed.**
All material values in fixtures are ASSUMED and are not simulated or measured.
BIO: unvalidated. Human play/accessibility gates have not occurred.

## Reproduction

```powershell
. ./tools/dev-shell.ps1
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure --no-tests=error
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure --no-tests=error
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure --no-tests=error
./tools/verify.ps1
python tools/verify_lock.py --require-verified
godot --headless --single-threaded-scene --render-thread safe --path game -- --bridge-smoke 2>&1 | Out-Host
git ls-remote origin
```

Passed CTest IDs (all three configurations): P0.core_contract, P0.cli_tick,
P0.cli_reject_negative, P0.schemas, P0.core_engine_boundary. ASan reported no
error during these short empty-world tests. This is not a physical stability,
leak/10-minute soak or V13 performance claim. CPU/GPU solver/frame p50/p95 are
not meaningful for this empty world and have not been measured. Full game
performance and real keyboard/gamepad verification remain future stages.

`./tools/direct-check.ps1` additionally passed native debug/optimized tests and
1200 empty ticks, invalid CLI rejection, schemas and core boundary. It was used
to isolate CMake environment failures, not to bypass the CMake acceptance gate.

## Resolved local issues

1. rg absent: used PowerShell file traversal; no missing-tool assumption.
2. Python app alias unavailable: found actual installed Anaconda interpreter.
3. CMake compiler-prefix detection crash with Korean compiler output: VSLANG=1033.
4. PDB server error C1902 in compiler probe: embedded /Z7 information (CMP0141).
   Compiler working checks and runtime tests still run; none were forced to pass.
5. Godot startup crash in diagnostic invocation: safe render thread and single
   threaded scene workaround. Default/multithreaded execution remains unverified.
6. PowerShell does not reliably await a GUI executable with bare invocation:
   verification uses Start-Process -Wait, captures stdout/stderr and checks actual
   process status and required markers. The final consolidated run passed.

## Remaining blockers and next action

See docs/issues/001-network-and-toolchain.md. Obtain official 4.7.2 engine and
compatible godot-cpp, Eigen, test framework and remaining tooling with verified
archives/commits; then compile bridge, activate descriptor, pass tick roundtrip
and full clean-checkout CI. Fetch origin and inspect history before integrating.
No force push. Do not merge/tag as completed P0 until all criteria are satisfied.
Git metadata writes are also blocked: see docs/issues/002-git-write.md. Current
HEAD is the initial supplied-document commit, not a commit of the implementation.
