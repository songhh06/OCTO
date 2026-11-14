"""Check the opt-in shared FEM torso + eight rods in the native RPC world."""
import json
import subprocess
import sys

p = subprocess.Popen([sys.argv[1], "--torso-fem"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8")

def recv():
    line = p.stdout.readline()
    if not line:
        raise RuntimeError("native child exited: " + p.stderr.read())
    result = json.loads(line)
    if not result["ok"]:
        raise RuntimeError(result.get("error", "native step failed"))
    return result

try:
    first = recv()
    assert first["snapshot"]["tick"] == 0
    assert len(first["snapshot"]["arms"]) == 8
    assert len(first["snapshot"]["torso_nodes"]) == 7
    for tick in range(120):
        target = [1.45 - 1.9 * min(tick / 100, 1), 0, 0.8]
        p.stdin.write(json.dumps({"op": "step", "target": target}) + "\n")
        p.stdin.flush()
        state = recv()
        assert state["snapshot"]["tick"] == tick + 1
        assert len(state["snapshot"]["torso_nodes"]) == 7
    p.stdin.write('{"op":"reset"}\n')
    p.stdin.flush()
    reset = recv()
    assert reset["snapshot"]["tick"] == 0 and len(reset["snapshot"]["torso_nodes"]) == 7
    p.stdin.write('{"op":"quit"}\n')
    p.stdin.flush()
    assert p.wait(timeout=10) == 0
    print("LIVE_TORSO_FEM_120_OK")
finally:
    if p.poll() is None:
        p.kill()
        p.wait(timeout=10)
