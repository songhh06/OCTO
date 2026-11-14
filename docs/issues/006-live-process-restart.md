# 006 - Restart a terminated live native process

Status: implementation committed; Godot runtime regression **not executed in this environment**.

## Problem

The live Godot client treats an exited `octo_live.exe` as a fatal stop and tells the player to close the whole demo. Pressing R previously wrote `reset` to the defunct child pipe, so it could not recover.

This issue is distinct from a solver failure that returns `ok=false` while the child remains alive. Such failures retain the existing `reset` RPC path, diagnostic capture, and numerical tolerances.

## Change

- Extract native process startup and transport initialization into `start_native()`.
- On R/Reset, check the child PID; launch a fresh process if the old one is dead, otherwise preserve the existing in-process `reset` command.
- Discard old buffered bytes, snapshots, in-flight state, and streams on restart, avoiding cross-process protocol contamination.
- Preserve manual recovery for unexpected process exit; do not silently erase the failed simulation state.
- Add a headless `--live-restart-test` mode: receive at least two ticks, kill the child, relaunch it, require a fresh tick-zero snapshot, print `LIVE_GODOT_RESTART_OK`, and quit.

## Verification required (Windows)

The existing local toolchain and `build/release/octo_live.exe` are required:

```powershell
.\tools\build-live-demo.ps1
& 'C:\Tools\Godot\godot.exe' --headless --path game --single-threaded-scene --render-thread safe res://scenes/live_demo.tscn -- --live-restart-test
```

Required: exit code 0 and `LIVE_GODOT_RESTART_OK`. Also manually run
`run_live_demo.cmd`, terminate the child from Task Manager, then press R:
the demo should relaunch a new world, and normal box/arm contact should remain operational.

No Godot runtime, Windows executable, or native solver was available to execute this
regression in the patch authoring environment. The new test has **not passed yet**.

## Not fixed here

- Numerical Newton-CG or contact multiplier convergence, including the curved-contact
  workload in issue 004.
- Performance acceptance, volumetric FEM coupling, compiled GDExtension, or BIO
  validation.

The native solver's error response still stops the live demo and should write the
diagnostic checkpoint and command to `work/`. The user can press R to reset.
