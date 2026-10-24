# Physics validation — 24 October 2026

## Completed checks

- 107 physics assertions passed against pinned Jolt v5.3.0, GCC 13.3, C++20, `-O2 -DNDEBUG`.
- All 14 scenarios passed in both variants: 28 creation/step/query/teardown cases,
  covering native and shared schedulers, trigger detection and collider visualization.
- 107 assertions also passed with AddressSanitizer and UndefinedBehaviorSanitizer on
  PlagC code. The optimized Jolt archive was not instrumented. LeakSanitizer could
  not inspect `/proc` in this host; the successful run used `detect_leaks=0`.
- 65 existing ECS/render hot-path assertions passed with the recording backend.
- Lab, HUD, scenarios, entry point and Sandbox passed `-Wall -Wextra` syntax checks.
- Benchmark runner passed Python compilation and command-generation smoke checks.
- `git diff --check` passed.

## CPU-only comparison

140 cases, 16,800 measured ticks: all 14 scenes × native/shared scheduler ×
1/2/4/6/11 background workers. Each case used 60 warmup ticks and 120 samples,
500 requested bodies (eight virtual characters), 10k terrain triangles and 500
mixed queries. This run was repeated after compilation/sanitizer work finished.
The static streaming scene does not reach its five-second reload during this
short test; its streaming lifecycle is covered by correctness tests. Settled
scenes legitimately have almost no Jolt update work during the measured window.

Host: INTEL(R) XEON(R) PLATINUM 8573C in a shared Linux development environment. This is **not** the
Ryzen 7430U laptop, a native Ogre render benchmark, or a Debug/Dist acceptance run.
Small workloads often do not benefit from more workers. These timings are
observations, not performance thresholds or guaranteed speedups.

Illustrative **two-worker median Jolt update milliseconds**, including adapter
drain but excluding character updates, events, ECS and rendering:

| Scene | Native ms | PlagC adapter ms | Native active bodies |
|---|---:|---:|---:|
| Shape Gallery | 0.091 | 0.750 | 4 |
| Material Ramps | 0.412 | 0.632 | 60 |
| CCD Range | 2.562 | 2.205 | 500 |
| Pyramid | 13.673 | 14.150 | 500 |
| Island Comparison | 0.002 | 0.002 | 0 |
| Sleeping City | 0.002 | 0.002 | 0 |
| Static World | 0.002 | 0.002 | 0 |
| Queries | 0.004 | 0.003 | 0 |
| Trigger Zones | 4.640 | 2.573 | 500 |
| Character Course | 0.036 | 0.723 | 1 |
| Constraints | 0.943 | 0.835 | 250 |
| District Streaming | 0.002 | 0.002 | 0 |
| Physics LOD | 0.105 | 0.631 | 17 |
| Ragdolls | 1.764 | 2.260 | 498 |

Full aggregate counts and median/p95 timings: [PhysicsCPUValidation.csv](PhysicsCPUValidation.csv).
Native Jolt remains the default; the shared adapter remains an opt-in experiment.

## Target validation still required

Run `Scripts/BenchmarkPhysics.py` with real Windows Debug and Dist builds for the
requested low-end baseline. Verify Ogre pixels, camera/HUD and keyboard controls
on Windows. The scripted tests validate mesh data and resource lifetimes, not
native display behavior. No GPU timing or 60 FPS budget claim is made.

Ozz animation-to-ragdoll handoff and production city LOD remain later work as
specified in the original plan. Current ragdolls are physics-only; current LOD is
a controlled experiment. The constraint lab is not a full vehicle controller.
Per-island, broad/narrow-phase and peak allocator telemetry are not yet exposed.
