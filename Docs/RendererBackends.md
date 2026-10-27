# Renderer backend selection and runtime switching design

## Available now: selection in code at construction

```cpp
PlagC::EngineConfig config;
config.rendererBackend = PlagC::RendererBackend::OpenGL;
PlagC::Engine engine(config);
auto active = engine.Graphics().Backend();
auto capabilities = PlagC::Renderer::BackendCapabilities();
```

Direct Renderer users set `RendererConfig::backend`. Choices are Auto, OpenGL,
Vulkan, and Null. Auto selects OpenGL in the native build. Null must be explicitly
requested, or selected by the existing headless/null flags. Explicit incompatible
flags are rejected. Explicit unavailable backends never silently fall back.
Offscreen currently means the existing Linux OpenGL EGL path only.

Capabilities report compiled, integrated adapters; they do not probe GPU/driver
support. OpenGL and Null are integrated. Vulkan is a reserved selection and reports
unavailable: its plugin, build dependencies, shaders and surface setup have NOT
been implemented by this change. Initialization errors still propagate to the
application. Backend selection is logged, and Ogre is selected by exact render
system name, not registration order.

## Adding Vulkan without changing gameplay

Keep Renderer as the engine-owned facade and Ogre as its implementation. Add an
Ogre Vulkan adapter next to the OpenGL adapter, with its own plugin factory,
render-system name, platform surface options and shader profile. Only advertise
Vulkan capability once that adapter is functional. Do not enable the capability
flag alone. The explicit factory switch rejects missing adapters.

Build its pinned Ogre render-system sources and Vulkan dependencies in Premake.
Keep the existing GL implementation available in the same native build. Separate
GL-only sampler workarounds and EGL options from shared mesh/material/light code.
Select matching HLMS/custom shader resources: sky, bloom, tone mapping, depth
conventions and texture bindings all require independent validation. Preserve
engine asset formats, ECS, physics, animation and gameplay interfaces.

## Future: switch within the running executable

This is a planned lifecycle, not a live-switch API implemented today. Changing
Ogre render systems under existing GPU objects is not supported by PlagC.

1. A settings UI submits a backend-switch request. The engine queues it for a
   frame boundary on its owner thread; never perform the change in an input callback.
2. Validate compiled support and host support. If unavailable, leave the current
   renderer untouched and report the reason. Record desired and active backend separately.
3. Pause simulation and render submission, drain in-flight GPU work, and retain
   CPU assets, ECS state, camera/settings and procedural mesh/resource descriptions.
4. Notify render-resource owners, destroy AssetScene/SceneRenderer bindings,
   scene effects, HUD targets, compositor workspaces and GPU resources before
   destroying Ogre Root and its plugins. Recreate the native window/surface if needed.
5. Construct the requested adapter. Re-upload CPU-backed assets and procedural
   resources; recreate bindings, targets and UI; restore aspect, VSync and settings.
6. Publish the new active backend and resume only after a successful frame. If
   creation fails, rebuild the previous adapter from the retained descriptions;
   if rollback fails, enter a controlled error state rather than using stale handles.

Add a resource-owner lifecycle contract and a renderer generation to handles
before exposing this UI. Existing handles are instance-specific; all must be
invalidated and rebound, including handles cached by game objects. The current
TPS owns renderer-bound resources directly, so that ownership must be refactored
before live switching. Do not recreate the whole Engine: its physics/gameplay
state should survive a renderer restart. Two Ogre Roots or cross-API GPU resource
sharing are not prerequisites and should not be assumed safe.

## Acceptance checks

Policy tests: Auto preference, explicit choices, null-only builds, unavailable
Vulkan, invalid enum, conflicting headless flags and offscreen restrictions.
Compile/test both native and PC_OGRE_NULL_ONLY configurations. Future Vulkan must
pass the same scene/asset/render tests independently. Future runtime switching
must survive repeated GL-to-Vulkan-to-GL switches, resizing, resource failure and
rollback without leaks, stale handles, or loss of gameplay state.
