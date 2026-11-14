# Decisions

## D001 - 2026-11-14 - preserve inputs, use requested remote
Preserved supplied specifications in initial main commit. Development occurs on
feature/P0-bootstrap. Origin is the user-specified repository. Remote inspection
failed (DNS); do not infer that remote main is empty or force push over history.
No publication or remote CI has occurred.

## D002 - empty independent world before physics
P0 core uses standard C++20 only. It advances the fixed clock and snapshots;
it has no body dynamics, no rod solver and no biomechanical validity claim.
Eigen and a third-party test framework remain required, pending verified download.
The temporary exception-based tests remain active in Release (no assert reliance).
CLI intentionally only accepts --ticks and --seed at P0. Scene validation is a
separate Python command, not a claim of a C++ scene loader or replay playback.
Implement the full CLI contract in its owning stages, with regression tests.

## D003 - format v1
SI, right-handed +Y-up; quaternion x,y,z,w; uint64 ticks and seeds; fixed_dt=1/120 s.
Schemas reject extra fields and unknown versions. Validator checks finite numbers,
mass, normalized quaternions, symmetric positive definite inertia, IDs/material
references, replay monotonicity and one intent per tick. P0 replay is a format
fixture, not a recorded simulation. Profile and scene hashes are exact file bytes.
Any format expansion requires explicit migration/version review. Actual replay
hash compatibility, C++ scene loading and on-disk snapshot restore are P2 tasks.

## D004 - installed engine is diagnostic only
Installed Godot is 4.6.2, requested engine is 4.7.2. No engine/scope change approved
or made. Compatibility rendering in the bootstrap scene is temporary diagnostic
configuration; product Forward+ acceptance remains pending on the pinned engine.
Bridge source is prepared but uncompiled. Do not copy the .gdextension template
into game before a verified compatible godot-cpp build exists. Missing native
class must fail --bridge-smoke, never silently substitute GDScript simulation.

## D005 - observed hashes are not upstream verification
Lock records exact installed binary SHA256 separately from archive SHA256. Null
archive fields mean unresolved provenance, not successful locking. Required gate
tools/verify_lock.py --require-verified fails until all official downloads are
verified. No fabricated versions, commits or digests. CI draft fails closed.

## D006 - 2026-11-14 - separate visual prototype explicitly requested
The user authorized an independent Godot 4.6.2 visual preview before resolving P0.
Add octopus_preview.tscn and procedural GDScript only; preserve the default P0
main scene and C++ boundary. Exactly eight visual arms, 128 cup assemblies,
time-based visual deformation and orbit camera. Clearly mark the UI as visual
only. This does not advance P0/P1 acceptance, switch the product engine or claim
BIO. Launcher and rendered snapshots are independent of the locked toolchain.

## D007 - 2026-11-14 - proceed with native P1/P2 work, acceptance unchanged
The user explicitly requested P1 and P2 after the visual prototype. Native work
proceeds despite unresolved P0 environment gates. This authorizes development,
not retroactive P0 acceptance or reduced physics criteria. Branch creation and
commits remain blocked by .git write permissions. Existing visual/P0 files stay.

## D008 - native prototype solver and honest subset boundaries
Use authored standard C++20 vector/quaternion/AD code while external math/test
dependencies remain unavailable; do not pretend this completes dependency locking.
Implement energy-derived implicit Newton-CG mechanics, bounded compliant suction
and initial node/OBB/BVH contact. Rod directors are nodal, matching the two-frame
curvature formula rather than the earlier element-only pseudocode allocation.
Full complements/CCD/rim strong coupling and live GDExtension remain acceptance
gaps, explicitly itemized in PHYSICS_MODEL.md and the P1/P2 report. Do not change
GAME_SPEC thresholds to match the reduced collision or resting-soak coverage.

## D009 - native visualization is recorded state playback
Add a separate physics_lab.tscn consuming only successful C++ traces. Hide the
decorative visual-preview cups rather than imply native rim state rendering.
Use the same snapshot positions/directors/radii; label offline playback in UI.
Camera and pause are live, muscle commands regenerate the trace. This is an
intermediate inspection path, not a substitute for the GDExtension gate.


## D010 - 2026-11-14 - native P2 completion work
User requested all P2. Implement geometric capsule/CCD contact, rebuilt AABB
BVH, projected AL friction/adhesion, twelve-rim geometry and tick-boundary
pressure/load outer iteration. Exact second-order AD blocks improve the
moving-arm solver without altering criteria. MSVC stack reserve is 8 MiB,
required by four-body debug Jet2 stencils (reproduced 0xC00000FD at 1 MiB).
Version-2 runtime scene/input/replay and version-3 native snapshots coexist
with P0 files. Full performance measurements record actual gaps; headless P2
passes never imply V13/P3/P4, BIO, live bridge or P0 provenance acceptance.

Final static analysis exposed width-dependent size_t feature-ID shifts and
a filesystem flag-enum diagnostic inside the MSVC STL file_size path. Feature
keys now explicitly use uint64_t before shifting. JSON file reads use bounded
8 KiB streaming with a 50 MiB cap, also avoiding a size-query/read race. No
analyzer check was disabled and no physical acceptance tolerance was changed.


## D011 - Inspect P2 using a separate recorded demo
User requests a demo to see results. Reuse native snapshots in a separate Godot
gallery with pause/scrub/case selection and a reference tip marker. Native core
is unchanged; no live solver, tissue validation or P3 completion is claimed.
All five scenes rendered and captured using installed Godot 4.6.2.


## D012 — 사용자 체적 연성체 필수 요구 반영

APPLY_TO_EXISTING_PROJECT.md와 VOLUMETRIC_SOFTBODY_ADDENDUM.md 전체를 읽었다.
강체 몸통은 비교 기준으로 보존하고 체적 몸통/rod+체적 단면을 필수로 한다.
렌더 전용 변형은 합격 대안이 아니다. 기존 완료 기록은 역사적 검사 결과로 보존하되
전체 P2는 진행 중으로 정정한다. 새 요구는 명시적 사용자 범위이므로 재승인을
요구하지 않는다. 8.33ms 달성 위험과 FEM/rod 질량·강성 이중 계산을 추적한다.
대안 full-arm FEM은 같은 모드 rod 에너지를 끄고 비교 결과를 기록해야 한다.


## D013 — live Godot native world over asynchronous stdio

User prioritizes a live movable-box contact demo. Installed Godot 4.6.2 launches
one native C++ world through its supported nonblocking stdio API; no extra
network dependency or engine change. Engine-side framing is bounded to one
in-flight command. Input applies a bounded position/velocity and upright torque
controller to a dynamic box; it never teleports its pose. Native 8-arm states
are rendered from responses. Preserve old replay/prototype/FEM work.
Use 12 rod elements per arm explicitly for this interactive comparison; this
is not 24-element convergence evidence or 120 Hz performance acceptance.
Initially an unrestricted 40 N/rotation-free hand drove a deep wrap and failed
at tick 319; failure retained in work/live-protocol-failure.json. Use an explicit
10 N upright hand model and retain the same 360-tick aggressive path regression.
The GDExtension/provenance gate and volumetric full-world implementation remain
open. Working IPC live integration is reported separately from those gates.


## D014 - Fix contact CCD/resting normal switching

Reproduced the saved tick-319 live stress failure. The inner Newton residual
was small while a self-contact normal alternated across outer iterations.
The meaningful distinction is an active resting pair versus a genuine swept
crossing; preserve current geometry for the first and CCD orientation for the
second. Do not fix this by relaxing physical tolerances or shrinking the input.
Add the original checkpoint and sixty unloaded continuation ticks as regression.
Save diagnostics before responding to a failed client request; wide Windows
argv preserves Korean paths. Existing generic error wording is replaced with
actual failure stage. Exact current user action was not logged in the old demo.
