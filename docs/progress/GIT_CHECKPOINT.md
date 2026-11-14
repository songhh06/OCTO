# Requested Git checkpoint — work in progress

2026-11-14. User requested publishing the current working tree to
https://github.com/Aggang-77/Octopus-simulator.git . This is not a completed P2 release.

Included: existing P0/bootstrap work, native rod/contact/replay core, visual and
recorded demos, live native-process demo, volumetric addendum and initial FEM kernel.
Existing tests and source are preserved. Do not infer whole-stage completion.

Known current failure: actual user live drag at tick 726, target
[0.391229748725891,0,0.227256760001183]. Contact iteration exhausted at
geometry residual 5.09e-8 m, normalized dual update .005096 > .002. Reproduction
is preserved in tests/scenes/regressions/live_drag_726.octostate and .json.
Investigation found two nearly coincident capsule/box endpoint/interior constraints;
this case is NOT resolved or validated yet. Development was paused for this upload.
The earlier tick-319 CCD normal fix had passed Release 36, Debug/ASan 27 each;
that evidence does not establish success of the newer tick-726 case.

Full volumetric body/arm/chamber/beak coupling, V16–V21 and performance are pending.
No BIO or completed-game release claim. There is no new commit yet in this environment.

Local constraints: .git branch/ref/index writes denied; GitHub ls-remote failed
with 'getaddrinfo() thread failed to start'. No remote history was fetched or overwritten.
Source/project files can be written. The supplied tools/push-current-work.ps1
stages only named project paths, creates a WIP branch, commits and pushes without
force. It must run in a terminal with normal Git permissions and network access.
