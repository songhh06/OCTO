"""Malformed live commands must not kill the native subprocess or advance its tick."""

import json
import subprocess
import sys

exe = sys.argv[1]
proc = subprocess.Popen(
    [exe],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
    encoding="utf-8",
)

def receive():
    data = proc.stdout.readline()
    if not data:
        raise AssertionError("Native process closed its stdout prematurely")
    reply = json.loads(data)
    assert reply["protocol"] == 1 and reply["live"] is True, reply
    return reply

def send(command):
    proc.stdin.write(command + "\n")
    proc.stdin.flush()
    return receive()

try:
    initial = receive()
    assert initial["ok"] and initial["snapshot"]["tick"] == 0
    invalid = [
        '{"op":',
        '{"op":"unknown"}',
        '{"op":"step","target":[0,0]}',
        '{"op":"step","target":[0,0,0],"drive":"yes"}',
        "x" * 65537,
    ]
    for line in invalid:
        reply = send(line)
        assert not reply["ok"], reply
        assert "Invalid live request:" in reply["error"], reply
        assert reply["snapshot"]["tick"] == 0, reply
        assert proc.poll() is None

    step = send('{"op":"step","target":[1.45,0,0.8]}')
    assert step["ok"] and step["snapshot"]["tick"] == 1, step
    reset = send('{"op":"reset"}')
    assert reset["ok"] and reset["snapshot"]["tick"] == 0, reset
    proc.stdin.write('{"op":"quit"}\n')
    proc.stdin.flush()
    assert proc.wait(timeout=15) == 0
    print("LIVE_INVALID_REQUEST_RECOVERY_OK")
finally:
    if proc.poll() is None:
        proc.kill()
        proc.wait(timeout=15)
    proc.stdin.close()
    proc.stdout.close()
    proc.stderr.close()
