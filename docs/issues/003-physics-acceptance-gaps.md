# 003 - Integration and performance gates after native P2 work

P2 native contact/coupling/scene/replay acceptance evidence is now tracked in
../progress/P2_COMPLETION.md. Earlier partial-P2 statements in P1_P2_REPORT.md
are historical. P0 and P1 whole-stage acceptance is still open.

Resolved native P2 implementation gaps: capsule and swept contact; AABB BVH;
geometric twelve-rim sealing, eccentric peeling and material sliding; coupled
projected AL friction/adhesion; same-solve pressure/load outer iteration; hinge
doors; runtime versioned scene/profile/command replay; moving eight-arm soak.
Final test counts and measured results are in the completion report.

Remaining upstream/integration gates: verified toolchain/dependency provenance,
protected Git writes, remote authentication/DNS, real hosted CI, live compiled
Godot bridge/muscle-command path. Offline native playback is not live control.
P1 residual scaling and arbitrary asymmetric root COM reduction remain model
limitations. Original P0/bootstrap and visual prototype are preserved.

V13/P3 follow-up: representative workload p95 target 8.33 ms, 300 active
contacts, 10-minute stability/memory instrumentation, faster curved-surface
contact convergence, arbitrary convex assets and rotating OBB/OBB continuous
contact. All failures and measured gaps remain visible in physics/V13*.json.
No BIO, real keyboard/gamepad gameplay, human playtest or product-engine
acceptance follows from native headless tests. No completed-game release tag.


Live interaction update: asynchronous stdio integration now operates in Godot
4.6.2; see ../LIVE_DEMO.md. User moves a dynamic box through native 8-arm
contact. Drag/pause/reset and paired no-contact/contact experiments passed.
This resolves the live-input demonstration gap; compiled GDExtension and
full volumetric boundary integration/provenance remain distinct open tasks.
