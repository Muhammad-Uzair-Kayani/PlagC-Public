# Phase 2 — Ogre renderer

Status: renderer increment verified on Linux in Debug, Release and Dist.
The recovered source was rebuilt and rerun; current logs and rendered images are
in Docs/Evidence. This is not a claim that the entire Phase 2 scope is closed.
Windows/VS2022 acceptance is not yet claimed. Phase 1 is accepted from the user's tests.

## Design → Develop → Test → Verify → Checklist → Update → Polish → Notify

PlagC owns Ogre, scene, camera and resources. GLFW owns the native window/input.
Ogre is destroyed before GLFW. Public typed handles contain no Ogre types; main-thread
ownership, stale handles and in-use mesh/material deletion are checked. The lock-free
engine job pool is unchanged. Ogre's own internal standard work queue has two workers.

Separate Premake targets build OgreMain, Hlms Common/Pbs/Unlit, NULL, GL3Plus and
RapidJSON (header-only utility). Vendor CMake is never invoked. Git submodules are pinned.

Sandbox tests primitives/winding, materials/textures, three light types, transforms,
visibility, render queues, camera, shadow/wireframe/vsync settings, model/skeleton
loading, animation control, resize/pause, render targets, capture and resource cleanup.

Native mode uses GLFW plus OpenGL. --headless and --null use Ogre NULL to test resource
and lifecycle APIs, with drawing disabled because Ogre 3.0 NULL has incomplete window
depth and VAO drawing behavior. --offscreen uses real OpenGL via EGL on Linux and
produces pixels without a display server. EGL support depends on the installed driver.

Run from the repository root. Native Sandbox remains open; W toggles wireframe,
S toggles shadows, Escape closes. The first five frames exercise setting changes and
write P6 PPM images under Logs. Automated modes exit after six frames by default.

## Verification checklist

- [x] Debug Linux build and Null API tests after reconstruction.
- [x] EGL image capture, wireframe and visible cast shadows after reconstruction.
- [x] Release and Dist builds/tests.
- [x] VS2022 solution generation; MSVC compilation still pending.
- [ ] VS2022 Debug/Release/Dist builds and Sandbox tests.
- [ ] Windows native visuals, animation, resize/minimize/restore and exit.
- [ ] Native macOS/Linux desktop acceptance.

This increment includes a single focused shadow map for a directional/spot light,
ambient hemisphere light, DDS skyboxes and PBR specular environment maps. Point-light
shadows, cascades and HDR/bloom are explicitly deferred and do not block Phase 3.
Editor UI belongs to its later phase. Model import and
skeleton playback use Ogre temporarily until the Phase 5 asset pipeline.

The skybox/environment implementation gap is closed. Native Windows acceptance
of this increment is still required; this Linux verification does not claim it.

## Verification evidence (2026-10-22)

GCC 13, Premake 5.0.0-beta2, x86-64 Linux. EGL selected Mesa's software device.
Debug: Null with four workers, headless with one worker, EGL with four workers.
Release: EGL with four workers and headless with one worker.
Dist: EGL and Null with four workers. Each run exited successfully and reported
explicit client resource cleanup followed by engine shutdown.

The comparison images show filled geometry, wireframe, textured floor and cast
shadows. The model demo selects Walk by name. Logs retain Ogre's legacy robot
mesh/material warning (the imported material is replaced with PlagC's datablock).
Ogre also logs a sampler validation message immediately after shader linking,
before its later uniform binding. The captured scene rendered correctly; this
message has not been suppressed. Native Windows/GLX/Cocoa behavior is unverified.

## Windows acceptance procedure

1. Pull the `prototype` branch. Install Premake 5 in Tools or PATH.
2. Run GenerateProjects.bat to fetch the four pinned submodules and generate VS2022.
3. Run Scripts\Validate.bat from a VS2022 Developer Command Prompt.
4. Start native Debug Sandbox from VS (working directory is the repository root).
5. Verify the robot walks, the floor is textured, lights affect PBR objects, and
   the cyan unlit cylinder remains unlit. W toggles wireframe; S toggles shadows.
6. Resize, minimize/restore, focus, use keyboard/mouse, then close with Escape.
7. Repeat native Release/Dist and check the saved Logs/Phase2.ppm capture.

The lifecycle and APIs are ready for those checks. Also test E (skybox) and
R (environment reflections), independently and together.

## PCH and Windows build correction

PlagC and Sandbox now include `pgcpch.h` first in each translation unit.
`Build/Pch/pgcpch.h` collects stable standard-library headers; each target owns
its `pgcpch.cpp` and compiler-generated PCH. Public headers keep their direct
includes and remain independently includable. Ogre, GLFW and spdlog headers
remain in backend implementation files; vendor projects do not use the engine
PCH. Premake configures PCH creation/use for both engine and Sandbox.

The Windows integration uses MBCS for the pinned Ogre narrow Win32 API code,
sets `_CRT_SECURE_NO_WARNINGS` only on Ogre's MSVC targets, and excludes the
MinGW CRT shim from MSVC. Missing OgreMain.lib was a downstream symptom of
Ogre compilation failures. System import libraries are linked only into the
final Sandbox executable, not folded into static archives. Engine target
warning 4324 is disabled for intentional cache-line padding; Ogre header
CRT, macro-redefinition, conversion and alignment warnings are scoped around
the vendor includes. Engine code keeps its CRT diagnostics.

After pulling on Windows, close Visual Studio, rerun `GenerateProjects.bat`,
open the regenerated solution, and use **Rebuild Solution** (Debug/x64 first).
This removes old archives that may contain imported system-library members.
Then run `Scripts\Validate.bat` in a VS2022 Developer Command Prompt and the
native rendering checks above. Do not copy an OgreMain.lib from a different
configuration to work around a compile failure.

Verification (2026-10-22): generated VS2022 Debug/Release/Dist projects checked
for PCH Create/Use, Ogre MBCS and CRT definitions, MSVC shim exclusion, and
system imports only on the executable. All public engine headers passed
standalone GCC C++20 syntax checks. Linux Debug Null-backend build succeeded
with real PCH compilation; Sandbox `--headless --workers 1` and
`--null --workers 4` both exited successfully after automated checks and clean
shutdown. Existing Ogre GCC visibility/unused-parameter warnings remain.
Windows/MSVC execution and native graphics validation are still pending on a
Windows host; generating a VS solution is not an MSVC build.


## Skybox / environment increment — 2026-10-22

**Design:** `EnvironmentDesc` holds a resource name and independent `skybox` /
`reflections` flags. `Renderer::SetEnvironment` and `Environment` enforce owner
thread access. An empty name clears the environment. Existing and newly created
PBR materials inherit the global reflection cubemap; unlit materials do not.

**Develop:** a mipmapped DDS cube is decoded/validated before changing live
state, then uploaded synchronously. The renderer owns and releases its texture;
replacement detaches old bindings before destroying it. Repeated identical
settings reuse it. A far-plane compositor quad renders the sky with
rotation-aware camera rays and preserved opaque depth, on both the window and
render targets. OpenGL GLSL shaders live in `Assets/Shaders`. NULL mode validates
resources without parsing GPU programs or claiming pixel coverage.

This is static specular environment mapping, with authored mip levels used for
roughness. It does not compute diffuse irradiance, GGX-prefilter a panorama,
generate reflection probes, or introduce an HDR/bloom pipeline. Existing
hemisphere ambient light is unchanged. Supply a prefiltered DDS cubemap for
production-quality rough reflections. The original colored test fixtures are
reproducible using `Scripts/GenerateEnvironmentFixture.py` (Python is optional
and is not needed to build or run PlagC).

**Test / verify:** Linux Debug built and ran Sandbox with EGL software rendering
(4 workers), headless (1 worker) and NULL window (4 workers), all with successful
exit and cleanup. VS2022 projects generated successfully; Windows execution and
Release/Dist testing of this increment remain pending.

- [x] Load mipmapped cubemap, reject missing files / 2D images / absent mipmaps.
- [x] Preserve live configuration on invalid input; reject worker-thread mutation.
- [x] Toggle sky and reflections independently; clear, reload, replace, repeat.
- [x] Create/update materials after enabling the environment.
- [x] Real pixels: reflections alter surfaces while preserving the background.
- [x] Real pixels: sky changes background and preserves opaque foreground depth.
- [x] Real pixels: camera translation keeps sky at infinity; rotation changes direction.
- [x] Real pixels: clearing restores original image within four 8-bit levels.
- [x] Real pixels: replacing the cubemap changes the captured image.
- [x] Existing shadow/wireframe, target capture and resource-release checks pass.
- [ ] Windows native Debug/Release/Dist acceptance of these checks.

**Update / polish:** E toggles sky, R toggles reflections in the native Sandbox.
Startup runs the checks automatically and saves `Logs/Environment-*.ppm` on real
renderers. Evidence PNGs are under `Docs/Evidence/Environment-*.png`; the raw
cubemap's saturated colors intentionally make orientation/reflections obvious.
Point-light shadows, cascaded directional shadows and HDR/bloom stay deferred.
