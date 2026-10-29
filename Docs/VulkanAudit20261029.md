# Vulkan optimization audit — 29 October 2026

Baseline inspected: `5e92b76` on `prototype`. This supersedes the implementation-status
portion of the earlier `3c0e1b1` handoff, not its historical measurements.

## Already implemented at the baseline

| Handoff item | Current implementation | Qualification |
| --- | --- | --- |
| Vulkan startup/backend selection | Engine configuration and command line; benchmark game | OpenGL remains available/default |
| Shared heap fallback and texture fallback | Generated `VulkanMemory.lua` | Allocation failures still need real-driver testing |
| Memory budget extension | Optional extension enabled; per-heap availability combines driver budget and Ogre reservations | Not a complete memory dashboard or allocation retry policy |
| Shared fallback pool cap | Small allocations cap the default reservation at 8 MiB | Large resources can legitimately exceed the cap |
| Persistent dynamic vertices | Coherent dynamic ring, map/copy/unmap in Vulkan | Baseline maps on every UpdateMesh, even multiple times per submission |
| Batched texture loading | Defer streaming wait until render/destruction; environment drain | All-at-once batching, not a byte-bounded readiness queue |
| Staging budget | 512 MiB for Vulkan | Headroom increase, not demonstrated memory optimization |
| GPU profiling | Asynchronous Vulkan timestamp pool, pass callbacks, one-in-four pass sampling | Baseline reads each issued pass separately; query generation needs frame completion protection |
| HUD | Persistent geometry reuse and FPS overlay | Benchmark captures suppress overlay as configured |
| Presentation | Lazy acquisition, same-family spare queue where available, present worker, shared-queue mutex | Extra frame slots can trade latency/memory for throughput; native validation remains important |
| Semaphore lifetime | Image-index-based present semaphore recycling | Resize/recreate/device loss/multiple windows need target tests |
| Benchmark | Renderer-selection/stress game and batch script | Use identical settings and content; old chat logs are not current benchmark evidence |

## Changes in this follow-up

### Coalesce dynamic writes before GPU submission

The generated Vulkan renderer retains the latest vertices in mesh-owned CPU storage,
queues a dirty ID once, and writes each dirty mesh once before `renderOneFrame`. Only
dirty IDs are visited, not the entire mesh registry. Destroyed IDs are skipped, paused
rendering retains the latest pose, and storage capacity is reused. OpenGL still uses
its existing buffer upload path.

This fixes repeated ring advancement when callers update the same mesh more than once
before rendering, including Debug's duplicate-map rejection. It removes redundant GPU
writes in that case. **For one update per frame it adds a CPU copy**, so a throughput
improvement is not assumed. Validation and bounds calculation now share one traversal
instead of two in Vulkan-enabled builds, reducing repeated vertex reads while still
validating before modifying mesh state.

GPU map/copy/unmap work is now charged to renderer submission. The animation upload
timer contains validation, CPU staging and bounds propagation. Compare total frame
cost and submission cost, not the animation counter in isolation.

### Reduce timestamp readback overhead and reject stale slot generations

Vulkan now checks the owning Ogre frame using the nonblocking `isFrameFinished` API
before reading a reused query slot. Without that check, available results from the
previous use could be mistaken for new results before the GPU executes the reset.

Completed slots use one `vkGetQueryPoolResults` call for the entire slot, including
availability values, rather than one call for each issued pass. Unissued query pairs
may cause `VK_NOT_READY`; only availability of issued pairs determines completeness.
No WAIT flag is used. When several slots finish together, `Latest()` selects the newest
sample, not whichever ring index happened to be processed last. All completed samples
still contribute to pass statistics.

### Repair the regression suite

The baseline generation test incorrectly rejected the intentional OpenGL buffer-type
fallback substring. The allocator harness was missing newer Vulkan/Ogre type aliases.
The extension patch also accepted a second application, duplicating its extension
branch. Tests now cover the valid fallback, current types and duplicate-application
rejection. These are test/generation fixes, not claimed frame-time gains.

## Validation performed here

- Vulkan performance generation patches: passed, including LF/CRLF determinism.
- Vulkan allocator selection regressions: passed; unpatched source still reproduces
  the known selection failure. The stub harness does not emulate a real driver budget.
- Actual generated upload blocks executed against recording buffers: ownership,
  coalescing, unchanged-frame no-op, repeated frames, failed-map retry, pending
  destruction, and OpenGL dispatch passed.
- Actual Vulkan readback loop executed against recording query results: in-flight
  stale availability rejected, one read per completed slot, partial availability,
  newest-sample selection and API error handling passed.
- Generated Vulkan renderer and profiler translation units compile on Linux.
- Native smoke extended for twelve frames of repeated updates, pause/resume and
  destruction with a pending upload. **Native runtime and Radeon performance results
  for this change have not been established in this environment.**

Run logic/generation checks:

```sh
python Scripts/Tests/VulkanPerformancePatchTests.py
python Scripts/Tests/VulkanMemoryTests.py
python Scripts/Tests/VulkanUploadTests.py
python Scripts/Tests/VulkanProfilerReadbackTests.py
```

Lua defaults to `texlua`; the new upload test also accepts `LUA`. C++ tests use `CXX`
(default `g++`). Native tests require the matching built libraries and staged assets.

## Still required, in order

1. **Windows/Radeon validation and matched benchmarking.** Regenerate with `--vulkan`,
   build Debug/Release/Dist, run smoke with validation confirmed active, then compare
   identical routes in the existing renderer benchmark and DefaultScene. Check total
   CPU, GPU, P95/P99, bytes uploaded and input latency. No measured FPS improvement is
   claimed from compile/recording tests.
2. **Presentation lifecycle review.** Exercise resize, minimize/restore, swapchain
   recreation, teardown and errors while a present request is queued. Audit all queue
   and device-idle operations for synchronization with the presentation worker, not
   only submit/present. Verify the profiler/query-pool teardown is included.
3. **Profiler completeness.** Validate queue timestamp support and valid-bit wrap;
   repeated pass instances/multiple targets; command-buffer reset placement; timer
   coverage versus full GPU workload. Pass sampling is not a complete CPU trace.
4. **Bounded texture streaming and memory telemetry.** Track staging/resident/pool/CPU
   bytes separately; cap in-flight work by bytes and model readiness. Raising the staging
   threshold is not a substitute. Add actual-allocation failure cleanup/retry coverage.
5. **Texture residency.** Cook appropriate compressed/scalar/packed formats, verify mips
   and color spaces, deduplicate shared texture content and introduce safe CPU-pixel
   retention policies. These are not implemented by this follow-up.
6. **Animation/HUD/GPU work based on fresh measurements.** Evaluate GPU skinning and
   CPU scratch/layout improvements; profile HUD work; tune culling, LOD, shadows and
   bloom only where GPU timings demonstrate a bottleneck. Do not reduce quality only
   for Vulkan and call it a backend win.
7. **Launcher and live switching.** Verify current selection automation and error
   recovery; live backend switching remains a separate resource-lifecycle feature.

Keep frame-in-flight experiments (2/3/4) separate from equal-settings comparisons.
More buffered frames can improve throughput while increasing memory and input latency.
Retain OpenGL as the reference until Vulkan has repeatable correctness and performance
results on the target hardware.
