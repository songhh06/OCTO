# 004 — Dense curved terrain: convergence regression resolved, performance and CCD open

Updated 2026-11-14. The original reproducible high-contact failure at tick 1
is **fixed in the specified 10-tick benchmark**, without relaxing solver
`constraint_tolerance=2e-5` or `dual_tolerance=0.002`.
This does **not** mean every possible curved/multi-contact problem is solved.

## Root cause, observed in failed runs

20 OBBs on a 100,000-triangle shallow curved floor generated a rapidly
changing set of nearly redundant OBB–triangle augmented-Lagrangian contacts.
After the first tick, the primal geometry residual became very small while
the dual multiplier updates oscillated. Diagnostic family instrumentation
showed the worst update was terrain–rigid contacts (key kind 3), not suction
cup pressure: `contact_dual=0.035563` versus `cup_dual=5.8e-8`.
The earlier patch only merged contact features after generating unstable
triangle choices, and did not fix the cause.

## Patch

- For terrain with 10,000 or more triangles, replace per-triangle
  box-versus-triangle SAT multipliers with persistent contacts at the eight
  actual box support corners. Each corner obtains a signed terrain closest
  point and surface normal. Sphere terrain contacts use one stable feature.
- Preserve the existing swept/feature contacts for sparse terrain, where
  distinct triangle edges matter for CCD and V06–V12.
- For dense terrain arm-capsule contacts, reduce redundant nearby triangle
  features to deterministic source/normal-sector patches. Warmstart by stable
  patch identity and preserve the deepest active constraint.
- Allow up to 64 strict outer AL iterations; keep pressure, primal and dual
  acceptance thresholds unchanged.
- Record `contact_dual`, `cup_dual`, dominating contact family and counts.
- Keep state rollback and diagnostic checkpoint on unsatisfied tolerance.

## Verified regression

Run `.github/workflows/curved-contact.yml` or:

```powershell
. ./tools/dev-shell.ps1
cmake --preset release
cmake --build --preset release --target octo_benchmark octo_p2_tests octo_contact_regression
ctest --test-dir build/release -R 'physics.live_wrap_regression|P2.ccd|P2.capsules' --output-on-failure
New-Item -ItemType Directory -Force work | Out-Null
./build/release/octo_benchmark.exe --curved
```

Windows Actions (Release, MSVC): run
https://github.com/songhh06/OCTO/actions/runs/43593872655.
3/3 selected CCD regressions pass; original 8-arm/128-cup/20-box/
100k-triangle benchmark completes 10/10 ticks; tick 9 dual residual
`0.00193413` against `0.002` threshold; run p95 is ~19,786 ms.
This is **more than 2,000× slower** than the target 8.333 ms/tick.
Neither the 120 Hz performance gate V13 nor long-duration curved-motion
acceptance is satisfied.

## Remaining risks

- The new dense-terrain OBB corner query uses end-of-step nearest surface;
  a box rapidly crossing a thin dense mesh can still need swept CCD
  (the sparse-terrain CCD regression does not establish dense-mesh safety).
- Arbitrary pathological near-parallel geometric patches, friction transitions
  and 60-second contact endurance have not been exhaustively verified.
- Runtime cost is dominated by large contact/rod coupled nonlinear solves.
  Further active-set stabilization, contact sparsity and Hessian caching/
  preconditioning and a 120 Hz profile are still required.

Close the **specific** former nonconvergence reproduction but keep the
broader V13 convergence/performance task OPEN until these gates pass.
