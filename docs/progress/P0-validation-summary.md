# Final observed validation

Date: 2026-11-14. Machine: Windows 10.0.26200, Ryzen 5 5600X, RTX 4060.

`./tools/verify.ps1` finished with `Available bootstrap checks passed. Full P0
requires -RequireCompleteP0.` and process exit 0 after the Start-Process wait fix.

| Check | Result |
|---|---|
| CMake dev configure/build | passed |
| dev CTest | 5/5, 0 failures, 0.65 s total |
| CMake release configure/build | passed |
| release CTest | 5/5, 0 failures, 0.62 s total |
| CMake asan configure/build | passed |
| asan CTest | 5/5, 0 failures, 0.65 s total |
| clang-format --dry-run --Werror | exit 0 |
| clang-tidy core world/CLI analyzer checks | exit 0 |
| Godot 4.6.2 headless editor import | exit 0 |
| Godot 4.6.2 empty 3D scene | P0_SCENE_READY marker, exit 0 |
| Native bridge smoke (separate run) | P0_BRIDGE_MISSING, exit 2 |
| Strict archive provenance (separate run) | unresolved entries, exit 2 |
| git ls-remote origin | DNS/getaddrinfo failure, exit 128 |
| Implementation git add/commit | .git/index.lock permission denied |

CTest timings are whole test-suite durations, not simulation p50/p95 results.
Raw per-configuration LastTest logs and bridge/remote/lock diagnostics are beside
this file. The consolidated console log was not available as a persistent file;
this table records its observed output. No remote CI or clean-PC run occurred.
No V01-V14 or BIO acceptance is inferred from these P0 empty-world checks.
