# Renderer backend selection

PlagC uses the same engine-owned Renderer facade with Ogre OpenGL or Vulkan.
OpenGL remains the default. Vulkan is opt-in; explicit unavailable selections
fail instead of silently falling back. This implements startup selection, not
live switching under existing GPU resources.

## Windows setup

1. Install the Vulkan SDK and a Vulkan-capable GPU driver. Open a new terminal
   so `VULKAN_SDK` points to the SDK with `Include` and `Lib` directories.
2. Run `GenerateProjects.bat --vulkan`, then build Sandbox in Visual Studio.
   Use this option each time you regenerate a Vulkan-enabled solution.
3. Run from the selected build output directory:

```bat
Sandbox.exe --renderer vulkan
Sandbox.exe --renderer opengl
```

The existing game/world selection works with either choice. Normal build staging
copies shaders/assets and the compiler licence beside each configuration's binary.
Changing APIs does not require asset recooking. Release/Dist retain `/O2` and Dist
retains its windowed executable configuration. The driver supplies the Vulkan
loader; PlagC does not bundle a replacement driver.

Code selection happens before constructing the engine:

```cpp
PlagC::EngineConfig config;
config.rendererBackend = PlagC::RendererBackend::Vulkan;
PlagC::Engine engine(config);
auto active = engine.Graphics().Backend();
auto compiled = PlagC::Renderer::BackendCapabilities();
```

Direct Renderer users set `RendererConfig::backend`. Choices are Auto, OpenGL,
Vulkan and Null; Auto prefers OpenGL. Capabilities report compiled adapters, not
GPU/driver availability. Headless/null flags conflict with explicit graphical
backends. Offscreen supports Linux OpenGL EGL or Ogre's Vulkan null-window target.

## Build and shader integration

Premake builds `Glslang` and `OgreRenderVulkan` alongside OpenGL when `--vulkan`
is selected. glslang is pinned to 14.3.0, commit
`fa9c3deb49e035a8abcabe366f26aac010f6cbfb`, and compiled with the engine's runtime
and configuration. No vendor CMake builds or SDK shader-compiler binaries are used.
SPIR-V optimizer support is disabled; the game's custom shaders use GLSL.

Generated Ogre translation units use the real pinned SpvOptions declaration
instead of an ABI-sensitive local copy, and allow XCB initialization without a
display for offscreen mode. Vendor sources stay immutable. GLFW owns the native
window; Vulkan borrows HWND on Windows or the X11 display/window pair on Linux.

The Vulkan PBS adapter also reserves the otherwise-unused reflection binding in
cutout shadow layouts: this pinned Ogre version binds the complete material set,
including the reflection texture, even though shadow shading does not sample it.
The cutout + reflection smoke case protects against descriptor overflow here.

Sky, bloom and tone mapping use unified GL/Vulkan delegates sharing shader math.
Vulkan uses explicit locations, uniform blocks and separate textures/samplers.
HLMS remains Ogre's responsibility. The existing GPU profiler is OpenGL-only;
Vulkan GPU timings report unsupported, not zero.

Linux needs Vulkan headers/loader, X11-XCB and XCB RandR development libraries,
plus the existing GL/EGL dependencies. Generate with `premake5 gmake2 --vulkan`;
`VULKAN_SDK` is optional when development libraries are installed system-wide.
The current adapters support Windows and Linux, not macOS Vulkan.

## Validation commands and limits

`Scripts/Tests/RendererBackendTests.cpp` covers selection and invalid combinations.
After a Vulkan-enabled native Dist build, `bash Scripts/TestRenderBackends.sh`
runs both adapters with textured PBR geometry, HDR/bloom/sky, dynamic mesh updates,
resizing, HDR toggles and resource cleanup. Captures go to `Logs/Backend-*.ppm`.
The smoke executable's second argument `window` selects a native surface instead
of offscreen. Enable `VK_LAYER_KHRONOS_validation` for Vulkan API validation.

`bash Scripts/TestTPSVisual.sh vulkan` exercises the public TPS scene after the
full asset package has been cooked. Local-only SurvivalGame/CityStreet content
requires the user's staged GameAssets; it is not included or substituted here.
Windows presentation and GPU performance must be validated on target hardware;
Linux software rendering does not establish either.

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
