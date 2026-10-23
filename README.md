# PlagC

Cross-platform C++ engine foundation. Phase 1 implements engine-owned startup,
logging, GLFW windows, typed event categories, thread identity and a bounded
lock-free work-stealing job system. Sandbox is the client and validation app.
Phase 2 adds Ogre-Next rendering behind engine APIs, tested through Sandbox.
Phase 3 adds an EnTT-backed World, typed components and an Ogre scene bridge.
Physics, managed assets and editor systems remain future phases.
See Docs/Phase2.md for current verification and limitations.

## Windows / Visual Studio 2022

Prerequisites: Git, Visual Studio 2022 with Desktop development with C++ and a
Windows SDK, and Premake 5 (tested generator version: 5.0.0-beta2).
Download Premake from https://github.com/premake/premake-core/releases and place
`premake5.exe` in `Tools/` or on PATH. No CMake is used.

1. Clone the development branch (no recursive clone needed):
   `git clone --branch prototype https://github.com/Muhammad-Uzair-Kayani/PlagC-Public.git`
   then `cd PlagC-Public`.
2. Run `GenerateProjects.bat`. It calls `FetchDependencies.bat`, shallow-fetches exact pinned dependencies with four parallel jobs, verifies them, then runs Premake. Git for Windows supplies the required Bash.
3. Open `Build/Generated/PlagC.sln` in VS2022.
4. Choose Debug / x64, set Sandbox as startup project, build and run.
5. Sandbox executes automated checks, then leaves a native window open. Move,
   resize, minimize, focus, type, click and scroll; inspect EVENT log lines.
   Press W for wireframe, S for shadows, Escape or the OS close button to exit.
6. Repeat under Release and Dist. See `Docs/Phase2.md` for the acceptance checklist.

For all configurations, run `Scripts\Validate.bat` from a VS2022 Developer
Command Prompt. For an individual build, after generation:

```bat
msbuild Build\Generated\PlagC.sln /m /p:Configuration=Debug /p:Platform=x64
Bin\windows-x86_64\Debug\Sandbox.exe --headless --workers 4
```

`--headless` skips GLFW and runs core plus Ogre NULL resource/lifecycle checks. `--null` creates a
real GLFW window object through its Null backend; it does not display a native
window. `--frames N` limits the app loop. `--workers N` configures the pool.
Headless/Null modes exit automatically after their checks and six frames unless
an explicit frame limit is supplied. Unexpected arguments/failures exit nonzero.

## Linux / macOS structure

```sh
chmod +x GenerateProjects.sh
./GenerateProjects.sh gmake2
make -C Build/Generated config=debug -j4
./Bin/linux-x86_64/Debug/Sandbox
```

Linux native backend is X11 and requires its development headers (on Debian-like
systems: `xorg-dev libgl-dev libegl-dev libxcb1-dev`). Wayland is not configured yet. macOS uses Cocoa and the
system frameworks; use `./GenerateProjects.sh xcode4` with a Premake distribution
that supports that generator, or gmake2. The initial architecture is x86-64.
Windows and macOS execution are not validated in this Linux environment.

For a machine with no window server (Ogre still needs X11/XCB development headers):

```sh
./GenerateProjects.sh gmake2 --null-platform
make -C Build/Generated config=debug -j4
./Bin-null/linux-x86_64/Debug/Sandbox --null --workers 4
```

Native and Null-only builds use separate Bin/Bin-null outputs. Changing the
compiler or architecture requires a clean rebuild. Generation
scripts stop on dependency failures. Submodules stay pinned; scripts never pull
vendor HEAD or update pins automatically.

## Layout

- `PlagC/include/PlagC`: public engine API; no GLFW/spdlog headers.
- `PlagC/src`: lifecycle and backend implementation.
- `Sandbox/src`: client factory and feature-level acceptance checks.
- `Build/Dependencies.lua`: separate vendor projects, maintained by PlagC.
- `Docs/Design.md`: ownership, threading and queue contracts.
- `Docs/Phase1.md`: progress, verification evidence and remaining checks.
- `Scripts/Validate.sh`: reproducible automated Linux validation.

Logs are written to `Logs/PlagC.log` relative to the working directory, with
rotation. Structured snapshots are bounded at 4,096 records for future editor use.
Published in Muhammad-Uzair-Kayani/PlagC-Public on the prototype branch.
PlagC uses the repository's existing MIT license; dependencies retain their own licenses.

## Sanitizers

Generate with `--sanitize=address` or `--sanitize=thread` (Linux). These use
separate `Bin-address` / `Bin-thread` (or `Bin-null-address` / `Bin-null-thread`) and matching intermediate directories.
For example:

```sh
./GenerateProjects.sh gmake2 --null-platform --sanitize=address
make -C Build/Generated config=debug -j4
./Bin-null-address/linux-x86_64/Debug/Sandbox --null --workers 4
```

Use leak detection on a normal host. Some restricted execution environments
cannot run LeakSanitizer or ThreadSanitizer; see the verification report.

## Linux offscreen graphics validation

A native Linux build includes EGL alongside GLX. With a compatible EGL driver:

```sh
./Bin/linux-x86_64/Debug/Sandbox --offscreen --frames 6 --workers 4
```

This renders real OpenGL pixels without a display server. Logs/Phase2.ppm and
comparison images use P6 PPM. It does not validate the Windows native-window path.
NULL modes deliberately disable drawing; they cannot validate shaders or pixels.

## Dependency setup

Run `FetchDependencies.bat` (Windows) or `bash FetchDependencies.sh` separately
to diagnose downloads. Repeated setup reuses existing checkouts. Set
`PLAGC_FETCH_JOBS` to change the default of 4. Local vendor edits stop setup;
commit or stash them first. Git errors stop generation with a nonzero exit code.
See [the permanent dependency policy](Docs/Dependencies.md). Deliveries are
committed to GitHub; clone the branch above instead of using source ZIPs.

## Skybox and environment validation

Sandbox now loads an original mipmapped DDS cubemap, displays it as the sky,
and applies it to PBR reflections. Press **E** to toggle the sky and **R** to
toggle reflections. Startup runs resource/lifecycle checks in all modes and
pixel comparisons in native / `--offscreen` mode. Inspect `Logs/Environment-*.ppm`.
The renderer API is `SetEnvironment({"EnvironmentTest.dds", true, true})`;
`SetEnvironment({})` clears it. See `Docs/Phase2.md` for coverage and limitations.

## Phase 3 — World / ECS

Pull `prototype` and rerun `GenerateProjects.bat`: the build now shallow-fetches
pinned EnTT and adds its header-only utility project. Build Sandbox normally.
It runs the earlier regression checks, then displays sixteen cubes controlled
by World components and animated through four job-system tasks per frame.

`World` owns entities, names and component data. `SceneRenderer` synchronizes
that data with Ogre and owns its renderer instances. EnTT is private to the
engine implementation. See `Docs/Phase3.md` for API examples, ownership rules,
the test checklist and Windows acceptance steps. Captures are in `Logs/World-*.ppm`
and `Logs/Phase3.ppm`. Existing W/S/E/R controls still work.
