# 005 - Live contact failure: CCD/resting normal two-cycle

2026-11-14. Fixed and verified: Release 36/36, Debug 27/27, ASan 27/27; formatting and static analysis passed. Godot drag/contact/pause/reset passed.
User observed a generic Newton-CG/line-search/subdivision failure in the live demo.
The surviving local checkpoint was from an earlier tick-319 stress test, so it
is a reproducible related counterexample, not a confirmed capture of that user's
exact input sequence. It is preserved at tests/scenes/regressions/live_wrap_319.octostate.

Reproduction before fix: Newton residual 2.03e-8, but the self-contact normal
alternated between old CCD orientation and fresh closest geometry; normalized
dual update stayed .00643 after four-way subdivision. More outer iterations and
multiplier relaxation did not remove the cycle; that trial was reverted.

Fix: when a cached capsule pair is actively touching, use its current closest
normal; preserve swept orientation for separated approaches and actual crossings
of the old plane. CCD distance epsilon and deformed trial radius must not toggle
an existing contact between two normal definitions. Capsule/OBB endpoints also
use the normal belonging to their own closest point rather than another feature.
No force, penetration, dual or Newton tolerance was widened.

The saved failure now solves at h=1/120, three outer iterations, residual 1.42e-8,
geometric residual 2.18e-7 m and dual update .00131 < .002. The regression then
releases external force/torque and checks sixty subsequent ticks and <2mm penetration.

Diagnostics: optional System::diagnostics stream is runtime-only, never serialized.
Failure messages distinguish Newton minimization from contact outer iteration.
The native bridge completes checkpoint and command/residual JSON writes before
sending failure, avoiding a close-before-save race. The client passes an absolute
work directory; Windows uses wide argv so Korean paths are valid. Reset clears
stopped state, accepts the reset response and reopens stepping. A terminated
child is detected before reading a broken pipe.

```powershell
. ./tools/dev-shell.ps1
cmake --build --preset release
ctest --preset release -R 'live_wrap_regression|P2.ccd|P2.capsules' --output-on-failure
```

Remaining limits: this does not prove robustness for every interactive contact.
New failures write work/live-failure.octostate and work/live-failure.json for
exact reproduction. FEM/P2 completion and 120 Hz performance remain separate.
