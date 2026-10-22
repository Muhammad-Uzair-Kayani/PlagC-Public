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
flat background and ambient hemisphere light. Skyboxes/environment maps, point-light
shadows, cascades, HDR/bloom and editor UI remain unimplemented. Model import and
skeleton playback use Ogre temporarily until the Phase 5 asset pipeline.

Phase 2 remains open until its remaining scope and Windows acceptance are completed.

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

1. Extract the archive including .git. Install Premake 5 in Tools or PATH.
2. Run GenerateProjects.bat to fetch the four pinned submodules and generate VS2022.
3. Run Scripts\Validate.bat from a VS2022 Developer Command Prompt.
4. Start native Debug Sandbox from VS (working directory is the repository root).
5. Verify the robot walks, the floor is textured, lights affect PBR objects, and
   the cyan unlit cylinder remains unlit. W toggles wireframe; S toggles shadows.
6. Resize, minimize/restore, focus, use keyboard/mouse, then close with Escape.
7. Repeat native Release/Dist and check the saved Logs/Phase2.ppm capture.

The lifecycle and APIs are ready for those checks. Skybox/environment-map support
and the broader rendering scope listed above remain open work, not completed items.
