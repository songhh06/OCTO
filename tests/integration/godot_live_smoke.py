"""Run live Godot/native integration checks on a clean Windows runner.

Exit zero only when every requested runtime marker is reached.
"""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
godot = Path(sys.argv[1]).resolve()
commands = {
    "--live-restart-test": ("LIVE_GODOT_RESTART_OK",),
    "--live-stall-reset-test": ("LIVE_GODOT_STALL_RESET_OK",),
    "--live-test": ("LIVE_GODOT_CONTACT_OK", "LIVE_GODOT_RESET_OK"),
}

for mode, required in commands.items():
    command = [
        str(godot),
        "--headless",
        "--single-threaded-scene",
        "--render-thread",
        "safe",
        "--path",
        "game",
        "res://scenes/live_demo.tscn",
        "--",
        mode,
    ]
    try:
        run = subprocess.run(
            command,
            cwd=root,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=180,
        )
    except subprocess.TimeoutExpired as exc:
        raise SystemExit(f"{mode}: Godot timed out after 180 seconds") from exc
    output = run.stdout + run.stderr
    print(f"== {mode} ==\n{output}", flush=True)
    if run.returncode != 0:
        raise SystemExit(f"{mode}: exit code {run.returncode}")
    for marker in required:
        if marker not in output:
            raise SystemExit(f"{mode}: missing {marker}")
    if any(line.startswith(("ERROR:", "SCRIPT ERROR:")) for line in output.splitlines()):
        raise SystemExit(f"{mode}: engine error in output")

print("LIVE_GODOT_ALL_SMOKE_OK")
