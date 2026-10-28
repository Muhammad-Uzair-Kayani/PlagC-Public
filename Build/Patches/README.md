# Pinned Ogre workarounds

## GLSL initialization workaround

`Ogre.lua` generates `Bin-Int/Generated/OgreGLSLMonolithicProgram.cpp` from the
unchanged Ogre pin `75643c3997f5b6d2aa1d7bd8400b9be6736d9908`. It inserts
`PlagCGLSLSamplerInitialization.inl` at the single existing pre-binding validation
site. Generation fails if that site disappears or becomes ambiguous. Only this
translation unit replaces its upstream counterpart in the GL3Plus project.
The generated source retains Ogre's MIT copyright/license header.

Ogre links a program and calls glValidateProgram before HLMS texture-register
parameters are installed. Mixed sampler types initially all use unit zero, so
that early validation reports an error even when draw-time bindings are valid.
For HLMS-generated programs only, the include assigns distinct initial units to
active sampler elements, then retains the original validation. Real HLMS register
values are still applied normally. It prefers direct program-uniform updates;
the OpenGL 3.3 fallback restores the previous valid program binding. Legacy custom
shader defaults and vendor source files are not changed.

`RuntimeAssetSlice.cpp` checks the actual PBR draw state and deliberately aliases
two different sampler types as a negative control. The invalid state must fail;
restoring the original bindings must pass. This workaround does not filter logs
or disable shader/driver validation. The legacy GL3.3 fallback and Windows driver
path have not been exercised in the Linux Mesa 4.5 validation environment.

## Vulkan performance workarounds

The pinned Ogre Vulkan backend is retained as a submodule and is never edited in
place. `GenerateProjects.bat --vulkan` creates a few fail-closed generated
translation units in `Bin-Int/Generated` instead.

`OgreRendererPerformance.lua` patches PlagC's renderer only in Vulkan-enabled
builds. CPU-skinned vertex buffers use `BT_DYNAMIC_PERSISTENT_COHERENT` rather
than `BT_DEFAULT`; the latter is GPU-only in Ogre and forces every per-frame
vertex update through a staging copy on Vulkan. The persistent buffer is mapped,
copied, and flushed through Ogre's buffered dynamic ring. OpenGL-only generation
continues to compile the normal renderer source unchanged.

The same renderer patch batches asset texture residency transitions. Vulkan no
longer calls `waitForStreamingCompletion()` after every individual asset texture;
pending uploads are drained once before the next rendered frame (and before a
texture is destroyed). The transient Ogre staging-texture budget is raised to
512 MiB for Vulkan to prevent the TPS bootstrap from repeatedly exhausting the
default budget. This does not increase permanent texture residency by 512 MiB.

`VulkanMemory.lua` keeps device-local memory first in Ogre's GPU-only allocation
preference list, retains only host-visible shared memory as a last-resort UMA
fallback, and caps small shared fallback pools at 8 MiB instead of reserving a
64 MiB pool for each small spill. When `VK_EXT_memory_budget` is available, the
allocator combines its tracked reservations with the driver's current heap
budget/usage before growing another pool. Allocation diagnostics report the
remaining calculated budget.

`VulkanDeviceExtensions.lua` patches the pinned Ogre 3.0 render-system extension
selection so `VK_EXT_memory_budget` is enabled when the driver advertises it.

`Scripts/Tests/VulkanMemoryTests.py` exercises allocator selection with a fake
memory topology. `Scripts/Tests/VulkanPerformancePatchTests.py` applies all
performance generation patches to the current engine source and pinned Ogre
source and verifies that they remain deterministic and fail closed when upstream
source changes.

Release profiling also uses Vulkan timestamp query pools. Queries are reset and
written inside compositor command recording and read back later without waiting;
there is no per-frame `vkDeviceWaitIdle`. The reported Vulkan total covers the
first-to-last instrumented compositor pass rather than presentation latency.
