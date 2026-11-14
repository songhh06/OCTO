# 004 - Curved high-contact multiplier convergence

OPEN. Owner stage: P3 performance/robustness; regression required before V13.
This is a known solver limitation, not a passed test.

```powershell
. ./tools/dev-shell.ps1
cmake --build --preset release
build/release/octo_benchmark.exe --curved
```

Eight 24-element arms, 128 presealed cups, 20 OBBs, 100,000 triangles on a
shallow conical floor. First tick succeeds; tick 1 exhausts outer iteration
and 1/2/4 subdivision. Complete tick state rolls back. Exact build/time/data:
../progress/physics/V13-curved-workload.json. Failure input/state is preserved
in work/V13-failure.octostate. Geometric/pressure residuals are small but dual
update remains about .027 > .002. A prior 128-iteration attempt also failed
at about .0046; see historical V13-curved-workload-failure.json.

Fixed: false OBB/triangle circumsphere contact (13-axis SAT), oriented/averaged
rim normals, tangent-plane projection, O(N) material lookup and dormant Hessian
blocks. Flat full-count workload now completes ten ticks, but this case remains.
Next: contact manifold reduction and curved patch multiplier conditioning.
Resolve by completing this same input without tolerance relaxation, adding a
regression and retaining V06–V12 passes. Preserve counterexample logs.
