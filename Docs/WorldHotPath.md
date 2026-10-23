# ECS → renderer hot-path revision

Target: `prototype`, following `152b668`. Preserve the safe World APIs and all
Phase 1–3 tests. Do not infer a GPU or scheduler diagnosis solely from CPU timings:
the supplied samples clearly identify sync as the largest measured section, but
build configuration and driver behavior still need controlled comparison.

## Retained render bindings

`SceneRenderer::Sync` no longer calls `World::Snapshot`, copies names/EntityRecords,
sorts entities or builds per-frame sets/maps. On first use, world switch or
renderable/light membership changes, internal EnTT views build retained bindings.
Only that structural pass reconciles instance creation/removal and uses identity
maps. Ordinary frames scan cached bindings and component revision counters.

- Changed transform: one `Renderer::SetTransform` for that object.
- Changed visibility/shadows/queue: only the corresponding changed setters.
- Changed mesh/material: create replacement before destroying the old instance.
- Unchanged renderable: no renderer calls.
- Lights/camera: update only when relevant component/transform/selection revisions change.
- Each bridge has its own observed revisions; one bridge does not consume another's changes.

This is **O(number of renderable/light bindings) revision scanning**, not an
O(changed entities) dirty queue. All animated cubes still require all transform
updates. That is intentional: the current workload changes every cube, and this
removes the heavyweight reconciliation without adding a journal/subscriber system.
Renderer handle validation and Ogre's own transform work remain unchanged.

## Transform system

```cpp
TransformSystem system;
system.Bind(world, selectedEntities); // Main thread; validate IDs/duplicates once.
auto values = system.Prepare();       // Reuses owned contiguous Transform storage.
// Jobs modify disjoint ranges of values. They never access EnTT.
jobs.Wait(group);
system.Commit();                     // Main thread; validate and commit atomically.
```

The bound system caches internal component addresses and a topology revision.
Creation/destruction/clear and camera membership changes invalidate bindings before
cached addresses may be read. A weak lifetime token protects against world destruction.
Rebind after invalidation. Preparing copies only selected transforms in selection
order; no names, hash filtering, allocation or sorting on a stable frame.

Commit still validates finite values, positive scale and camera constraints. It
rejects stale preparation, double commit, intervening transform/camera writes,
invalid topology and dead worlds before writing anything. It uses cached targets
instead of duplicate-ID hashes, repeated entity-map lookup and registry replacement.
Public `Read`, `Snapshot`, `TransformSnapshot` and `ApplyTransforms` remain available
with their existing validation/atomicity semantics. Safe writes also update revisions.

The span is a borrowed view of system-owned storage, not live ECS memory. Join all
jobs before preparing/binding again, committing or destroying the system. The main
thread must not read/write the job buffer while workers use it. Invalid batches can
be corrected and recommitted, or abandoned with a fresh Prepare.

## Instrumentation and repeatable comparison

- The Sandbox still starts unlocked, `math=1`, 1,000 cubes, +1,000 every five seconds
  up to 10,000, with the same animation math and worker selection.
- `snapshot` is now labeled `gather`: it measures Bind when necessary plus Prepare.
- Per-second output retains job+wait, commit, sync and render averages, and reports
  build configuration. Only compare optimized Release/Dist with the same configuration.
- Sync counters report scanned bindings, transform calls and property calls.
- F7 toggles detailed sync timing: structural reconciliation, resource replacement,
  transform calls, property calls, light/camera calls and remaining traversal overhead.
  Those detailed numbers are explicitly for the **last frame**, not the interval mean.
  Timing every call adds overhead; leave F7 off for ordinary benchmark comparisons.
- `SceneRenderer::Statistics()` exposes the same measurements and creation/removal counters.
- Structural frames will still cost more. Pause growth with G and compare several
  stable seconds at each count. Keep VSync off, camera, shadows, math, workers and
  window size identical. Inspect both total frame time and individual sections.

For a host-independent *CPU plumbing* comparison, `bash Scripts/BenchmarkWorld.sh`
builds the actual World/TransformSystem/SceneRenderer with a recording Renderer and
a frozen copy of the old bridge from `152b668`. It uses the same world and transform
changes for both paths, reports medians after warmup, and runs the existing World
checks plus new retained-bridge and transform-system tests. It deliberately excludes
jobs/GPU/driver timing. This is **not an Ogre/FPS benchmark**. The safe baseline
commit also updates the newly introduced revisions, so it is a conservative API-path
comparison, not an exact binary replay of the old revision.

## Validation boundaries

The isolated CPU tests cover static/dirty calls, independent readers, resource
replacement failure/retry, remove/readd, deletion, camera/light updates, world switches,
cleanup, invalid/stale/foreign transform batches, world lifetime and job-output parity.
They also run inside the normal Sandbox against the selected real renderer.

CPU recording-backend benchmark evidence is saved in `Docs/Evidence/WorldHotPath.txt`.
At 7,000 cubes on this host, median gather/commit/sync changed from
0.2875/0.5616/1.3843 ms to 0.0127/0.0423/0.0731 ms. These numbers exclude Ogre and
must not be extrapolated to the user's FPS. Original World regressions and new
bridge/system checks passed ASan/UBSan with leak detection disabled. The CPU
integration suite also passed, including exact job-result parity with one and four
workers. Evidence: `Docs/Evidence/WorldHotPath-safety.txt`.
Native Ogre linking, actual rendering, Windows/VS2022 acceptance and the laptop FPS
comparison remain pending for this revision. These changes do not promise a specific
FPS improvement or remove the cost of updating thousands of Ogre scene nodes.
