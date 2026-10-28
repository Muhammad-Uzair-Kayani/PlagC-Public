# Renderer backend selection and benchmark

## Startup renderer selection

A Vulkan-enabled build contains both the OpenGL and Vulkan Ogre adapters. When `rendererBackend`
is `Auto`, Sandbox is interactive, and more than one graphical adapter is compiled in, the engine
prompts **before the Window/Renderer is initialized**:

```text
=== PlagC Renderer ===
[1] OpenGL (opengl)
[2] Vulkan (vulkan)
Select renderer [Enter = OpenGL]:
```

`--renderer opengl` and `--renderer vulkan` remain explicit overrides and skip the prompt. Headless,
Null, offscreen and Dist startup do not depend on console input.

The prompt is intentionally only a frontend. `RendererBackendCatalog()` and
`AvailableRendererBackends()` are the shared source of backend metadata for a future launcher,
settings panel or renderer-restart workflow. Live hot switching is not implemented yet because
renderer handles/resources are instance-owned and must be recreated during a backend restart.

## DriverStress benchmark scene

Select **Renderer Backend Benchmark** (`RendererBenchmark`) from the game menu. Its only world is
`DriverStress`.

The scene is designed to expose renderer/API CPU overhead instead of normal gameplay noise:

- fixed camera and completely static scene;
- FPS cap and VSync disabled;
- no physics, animation, audio workload or HUD updates;
- shadows, HDR, bloom, fog and environment disabled;
- low triangle geometry so fragment/GPU throughput is not the intended bottleneck;
- 2,500 objects by default;
- each object owns a separate mesh/VAO;
- up to 2,500 distinct materials to resist batching and force substantial draw/material submission.

The normal PlagC profiler remains active. The primary comparison is **Renderer submit** and
**Ogre renderOneFrame** at equal `RENDER STATS`. GPU total, P95/P99 and hitch counts are secondary
checks. A valid CPU-side API comparison should have approximately the same objects/triangles/draws
on both backends; do not interpret an FPS difference if the workloads differ materially.

The load can be changed without recompiling:

```bat
set PC_RENDER_BENCH_OBJECTS=4000
set PC_RENDER_BENCH_MATERIALS=4000
```

Accepted object range is 128-8000; material range is 1-8000 and is clamped to object count.

## Automated OpenGL vs Vulkan capture

Build Release from a Vulkan-enabled project first:

```bat
GenerateProjects.bat --vulkan
```

Then run from the repository root:

```bat
Scripts\BenchmarkRenderers.bat
```

The script performs a warmed 20-second OpenGL capture followed by the same Vulkan capture and writes
logs/CSVs under `Bin\windows-x86_64\Release\Logs`.

For meaningful results, use the same power mode, AC/battery state, window state, resolution and
background workload for both runs. Shader/resource creation happens before the benchmark capture;
the 240-frame warmup further excludes initial pipeline/driver compilation behavior from the CSV.

If the scene becomes GPU-bound, do not increase geometry complexity. The purpose of DriverStress is
to keep GPU work light while increasing the number of independent renderer submissions. Increase
`PC_RENDER_BENCH_OBJECTS` first and verify the resulting draw count in `RENDER STATS`.
