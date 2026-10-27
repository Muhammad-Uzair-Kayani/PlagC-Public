# Visual Max performance baseline

Build Release x64 after pulling `prototype` and regenerating projects. Assets already cooked
by `BuildAAAAssets.bat` can be reused; a solution build stages them normally.

In PowerShell, change to the executable's output directory and run:

```powershell
cd "Bin\windows-x86_64\Release"
.\Sandbox.exe --performance-csv Logs\exterior.csv --performance-warmup 120 --performance-seconds 60
```

Keep the window visible and focused. Capture starts after 120 warm-up frames and exits
normally after 60 measured seconds, writing `Logs/exterior.csv` and
`Logs/exterior.csv.summary.txt`. Closing normally earlier also saves the partial summary.
A crash/forced termination may lose buffered output. Existing files with the same names are
overwritten. Dist uses the same options, writes files without requiring a console.

Run three separate captures at the same resolution and settings:

| Capture | Actions |
| --- | --- |
| `exterior.csv` | Remain at spawn facing the compound; keep the camera still. |
| `interior.csv` | During warm-up, enter the warehouse; remain still while looking at its props. Increase warm-up if needed. |
| `gameplay.csv` | Walk a repeatable route through the facility, aim, fire and reload. Record the route with the results. |

F3 toggles HUD statistics, F4 toggles wireframe, F11 toggles the FPS lock. Keep those states
constant between comparisons. TPS starts unlocked and requests VSync off; the initial Ogre
window-creation log can still say `vsync=true` before TPS startup changes it. CSV records
the actual engine settings on each sample. Driver-enforced caps are outside that setting.
The HUD itself adds rendering cost: use the same HUD setting in every capture.

## Measurements and boundaries

- Frame time: wall time between completed render/pacing iterations, including event polling,
  asset pumping, update, rendering, pacing and the preceding capture bookkeeping. It does
  not use the gameplay timestep, which TPS clamps. Summary reports mean FPS from total
  valid time and nearest-rank p50/p95/p99/max frame times.
- Update CPU: complete application `OnFrame`. Physics `Advance`, animation/asset scene
  `Update`, and scene-renderer `Sync` are nested CPU wall-time subsets. They do not add up
  to the entire update. Character input, camera, animation-controller selection, audio and
  HUD work are also inside update. Animation timing includes animated-mesh upload work.
- Render CPU: `Ogre::Root::renderOneFrame` plus renderer animation bookkeeping. This may
  include GPU/driver/presentation waits. **It is not GPU execution time.** `gpu_ms=NA` is
  deliberate; asynchronous GPU timestamps and VRAM reporting remain separate work.
- Draw calls and triangles: Ogre render-system metrics for the completed frame across
  scene, shadows and compositor passes; these are not just unique visible world meshes.
- Objects/meshes/materials/textures/lights: live engine-owned renderer handles, not bytes.
- Resident memory: process working set on Windows, resident pages on Linux, sampled about
  once per second. It includes shared/system allocations and is not VRAM or private heap
  usage. Unavailable memory reports `-1`.
- Startup summary splits engine initialization and scene startup; the first loop iteration
  is recorded separately to expose deferred shader compilation/uploads. These are wall
  times, not a claim that all GPU work has completed.
- Minimized frames remain in the CSV and duration but are excluded from summary percentiles
  and FPS. Window focus/occlusion is not detected: keep the benchmark focused. Loading/redeploy
  during a capture creates a real spike; do not redeploy for steady-state comparisons.
- `synthetic_backend=1` identifies null/headless/offscreen runs. These verify instrumentation
  only; they are not a Vega 7 baseline. Native/offscreen software rendering also cannot
  establish hardware performance. Keep the matching startup GPU/driver log with results.

Default capture: opt-in only. No performance file is written without `--performance-csv`.
The one PlagC job worker is intentional with the default `native` physics scheduler, whose
Jolt workers are separate. Do not change scheduler/worker counts between baseline runs.

## Local instrumentation check

```sh
c++ -std=c++20 -Wall -Wextra -Werror -IPlagC/include Scripts/Tests/PerformanceCaptureTests.cpp -o /tmp/plagc-perf-test
/tmp/plagc-perf-test
```

This checks warm-up exclusion, duration, minimized-frame exclusion, percentiles, mean FPS,
CSV row count and disabled capture. Gameplay regression: `bash Scripts/TestTPS.sh`.

Validation for this change: capture unit test and all 28 base TPS gameplay checks passed.
A native Mesa/EGL base-scene capture exercised startup, warm-up, duration-based exit,
nonzero draw/triangle/phase counters, memory sampling and unlocked/VSync-off fields.
Windows/MSVC capture execution and Vega 7 measurements still require the target machine.
