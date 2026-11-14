# 001 - P0 official toolchain and native bridge blocked

Status: open, environment/network blocker, not an approval request.

Reproduction from repository root:

```powershell
git ls-remote origin
Invoke-WebRequest https://api.github.com/repos/godotengine/godot-cpp/tags -TimeoutSec 15
python tools/verify_lock.py --require-verified
godot --headless --single-threaded-scene --render-thread safe --path game -- --bridge-smoke 2>&1 | Out-Host
```

Observed: Git exit 128, `getaddrinfo() thread failed to start`; PowerShell HTTP
`No such host is known (api.github.com:443)`. Root cause beyond this shell's
network/DNS environment is not established. No evidence that the remote is empty,
that credentials work or that account authorization is the cause.

Lock gate exits 2 because official archive URLs/SHA256 and dependency commits are
unverified. Local Godot 4.6.2 differs from required 4.7.2. Native smoke exits 2
with P0_BRIDGE_MISSING. These failures are retained, not skipped or downgraded.

Attempts: Git HTTPS and independent PowerShell HTTPS both failed. Installed tools
were found and used for native/debug/release/ASan verification. CMake compiler
probes were repaired via VSLANG and embedded debug info. Diagnostic Godot import
and empty scene work with safe/single-thread options. Web retrieval can read a
release page, but no official binary archive has been materialized and verified.

Alternative path after network becomes available: download exact official
archives on a connected machine, copy them into work/downloads, verify published
SHA256, record provenance, pin compatible godot-cpp/Eigen/framework source commits,
build native extension and rerun strict verification. Do not substitute fabricated
hashes, arbitrary engine versions, or GDScript physics. Recheck remote history
before any push. P1 remains blocked until P0 acceptance.
