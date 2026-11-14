# 006 - Live RPC recovery and reproducible Windows verification

Status: targeted live native/transport recovery implemented on branch
`feature/P0-live-process-recovery`. A GitHub Actions Windows run for
commit `f09fcf1` passed 28/28 short CTests, the 360-tick live transport
test, and the Godot 4.6.2 child-death/stalled-request reset smokes.
The later integrated contact smoke and build-launcher changes require
their own run result; check the latest workflow before accepting.

## Reproduced defects and changes

1. **Dead native child:** R/Reset previously attempted to write `reset`
   to a pipe whose process had exited. It now creates a new child,
   clears pending replies and old snapshots, and waits for a new tick 0.
2. **Unresponsive native child:** a timeout set `stopped=true` but kept
   `in_flight=true`, so R/Reset deferred indefinitely. A reset of a
   stopped, in-flight child now terminates and replaces it.
3. **Malformed live commands:** invalid JSON, unknown operations, bad
   target dimensions and wrong field types previously threw out of the
   native command loop and terminated the process. The native server now
   returns `ok=false` without advancing the tick, and accepts a
   subsequent valid step/reset.
4. **Stale native .exe on launch:** `run_live_demo.cmd` previously skipped
   building when a tracked `build/release/octo_live.exe` existed.
   It now invokes an incremental build every time.
5. **Machine-specific checked-in build cache:** the native build helper
   rejects `CMakeCache.txt` originating from a different checkout path.
   CI explicitly deletes machine-specific release cache before configure.

A native solver failure that returns `ok=false` is **not** hidden or
automatically discarded. Its diagnostic checkpoint and command remain
available in `work/`; R/Reset requests a fresh world from the live child.
Unexpected death still requires manual R/Reset outside test mode.

## Regression coverage

- `live.invalid_request_recovery`: malformed JSON, wrong operation,
  invalid target, invalid drive flag, overlong message, followed by
  valid step, reset and quit without child termination.
- `live.protocol`: paired 360-tick no-contact and box-contact trials.
- `physics.live_wrap_regression`: previous CCD/resting-normal
  counterexample and sixty continuation ticks.
- Godot headless modes `--live-restart-test`,
  `--live-stall-reset-test` and `--live-test`. The first two
  completed on Windows Actions at commit `f09fcf1`. The third mode
  was added after that run and requires a fresh check.

Hosted workflow: `.github/workflows/live-regression.yml`.
Manual runner: `python tests/integration/godot_live_smoke.py <godot-executable>`.
The visual screenshot and actual interactive desktop play remain separate
from headless test acceptance.

## Not resolved

Numerical curvature/high-contact convergence (issue 004), V13 throughput
and 120 Hz performance acceptance, 10-minute stability, volumetric FEM,
compiled GDExtension, and biological validation remain open. Do not label
these as fixed because of the passing live transport suite.
