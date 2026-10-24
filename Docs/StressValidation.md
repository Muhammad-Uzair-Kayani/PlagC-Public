# Integrated stress-world verification — 2026-10-24

This report covers the replacement of the former playable test scenes with one
persistent FPS facility. It does not claim Windows/Ogre visual acceptance or a
native interactive 10–30 minute soak.

## Executed checks

| Check | Result |
|---|---|
| Core/extended physics regression suite | 78 checks passed |
| Input/ECS/transform integration regressions | 47 checks passed |
| Retained World/SceneRenderer regression and CPU comparison | Passed |
| Stress model with native Jolt and shared PlagC schedulers | 39 checks passed |
| Real Engine + StressWorld with recording graphics/window stubs | 14 checks passed |
| ASan + UBSan stress model run | Passed; leak detection disabled; cached Jolt archive uninstrumented |
| C++20 syntax check with `-Wall -Wextra` | Passed without diagnostics for new scene/HUD/model, Sandbox and EntryPoint |
| Shell syntax and benchmark runner CLI | Passed |

Model checks cover grounding, movement, jumping, body-to-entity ray selection,
force-based hold, mass-scaled throw, persistent growth through 10,000, downscaling,
projectile recycling, pause, progression, constraints and resource cleanup.
Application checks pump queued keys through the real event bus, change presets,
spawn workloads, toggle wireframe/debug, reset the world repeatedly, verify graphics
resource counts, and verify both fixed-tick benchmark and elapsed-time soak output.
They do not simulate OS keyboard delivery or render pixels.

## CPU soak observations

Release-style `-O2 -DNDEBUG` code, pinned Jolt archive, two native Jolt workers,
60 Hz fixed simulation, seed 42, stationary player, projectile every two seconds,
automatic growth disabled. Counts are sampled once per simulated second. These
are accelerated simulation runs, **not** wall-clock interactive native-renderer runs.

| Entities | Simulated duration | Bodies | Active min | Active median | Active max | Requested / feasible target |
|---:|---:|---:|---:|---:|---:|---:|
| 1,000 | 600 s | 992 | 665 | 799.5 | 893 | 1,000 / 893 |
| 2,500 | 30 s | 2,492 | 942 | 1,081.5 | 2,386 | 1,000 / 1,000 |
| 5,000 | 30 s | 4,992 | 1,576 | 2,152 | 4,886 | 1,500 / 1,500 |
| 10,000 | 30 s | 9,992 | 4,754 | 6,313.5 | 9,879 | 2,000 / 2,000 |

All runs completed without entity/body ceiling violations or non-finite player
positions. The 600-second run recycled 7,428 objects by its final one-second sample.
[Machine-readable observations](StressCPUValidation.csv) are included. The 2,500
run briefly missed its activity target; the 1,000 total-entity preset cannot contain
1,000 active bodies after statics/camera/lights. Sleeping is intentionally enabled.
These counts establish workload activity, not native FPS, GPU throughput, or proof
that every body's state is correct for all possible interactions.

## Native acceptance still required

1. Regenerate projects and build Debug, Release and Dist on Windows. Confirm normal
   launch stays open, the prior Ogre v2 queue fix is present, and F1 displays controls.
2. Test mouse capture/focus loss, WASD, jump, sprint, stairs, ramps, moving platforms,
   the hinged bridge, body pushing and grab/throw/drop against multiple shapes.
3. Walk through all six districts. Inspect terrain, collider/render alignment,
   warehouse lights, sky/reflections, shadows, wireframe and HUD readability.
4. Change presets and progressive growth without leaving the world. Watch actual
   activity and dropped simulation time; do not substitute body totals for active bodies.
5. Run `--soak --seconds 600` through `--seconds 1800` interactively. Inspect CSV RSS,
   entity/body counts, activity and timing tails. Check graphics/resource stability
   after repeated world resets. No claim of a native memory-leak-free soak is made yet.

The renderer object count is not a visible-object query, process RSS is not physics
allocator memory, render CPU is from the previous completed render, and no GPU time
is exposed by this HUD. Full per-frame CSV remains available when the bounded
65,536-sample percentile reservoir is used for long captures.
