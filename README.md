# PlagC

Cross-platform C++ engine foundation. Phase 1 implements engine-owned startup,
logging, GLFW windows, typed event categories, thread identity and a bounded
lock-free work-stealing job system. Sandbox is the playable client; automated validation runs separately.
Phase 2 adds Ogre-Next rendering behind engine APIs, tested through Sandbox.
Phase 3 adds an EnTT-backed World, typed components and an Ogre scene bridge.
The default client is [OUTPOST ZERO, a third-person sandbox](Docs/TPS.md). [Managed assets, Ozz animation and OpenAL audio](Docs/Assets.md) are now integrated. Editor systems remain a future phase.
See Docs/Phase2.md for current verification and limitations.

## Windows / Visual Studio 2022

Prerequisites: Git, Visual Studio 2022 with Desktop development with C++ and a
Windows SDK, and Premake 5 (tested generator version: 5.0.0-beta2).
Download Premake from https://github.com/premake/premake-core/releases and place
`premake5.exe` in `Tools/` or on PATH. No CMake is used.

1. Clone the development branch (no recursive clone needed):
   `git clone --branch prototype https://github.com/Muhammad-Uzair-Kayani/PlagC-Public.git`
   then `cd PlagC-Public`.
2. Run `GenerateProjects.bat`. It calls `FetchDependencies.bat`, shallow-fetches exact pinned dependencies, verifies them, then runs Premake. Git for Windows supplies the required Bash.
3. Open `Build/Generated/PlagC.sln` in VS2022.
4. Choose Debug / x64, set Sandbox as startup project, build and run.
5. Sandbox opens OUTPOST ZERO with unlocked FPS. WASD/mouse move/look, right mouse
   aims, left mouse fires, R reloads, Space jumps and E interacts. See
   [the complete controls and TPS scope](Docs/TPS.md). There are no test-scene keys.
6. Repeat under Release and Dist. See `Docs/Phase2.md` for the acceptance checklist.

For all configurations, run `Scripts\Validate.bat` from a VS2022 Developer
Command Prompt. For an individual build, after generation:

```bat
msbuild Build\Generated\PlagC.sln /m /p:Configuration=Debug /p:Platform=x64
Bin\windows-x86_64\Debug\Sandbox.exe --headless --workers 4
```

`--headless` skips GLFW and runs the TPS sandbox with the Ogre NULL backend. `--null` creates a
real GLFW window object through its Null backend; it does not display a native
window. `--frames N` limits the app loop. `--workers N` configures the pool.
Headless/Null/offscreen modes exit after 120 frames unless `--frames` is supplied.
Physics and gameplay regressions run separately through `Scripts/TestPhysics.sh`
and `Scripts/TestTPS.sh`. Unexpected arguments/failures exit nonzero.

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
- `Sandbox/src`: playable client factory.
- `Game/src`: TPS gameplay, HUD and facility.
- `Scripts/Tests`: standalone regression fixtures.
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
`PLAGC_FETCH_JOBS` to change the default of 1. Local vendor edits stop setup;
commit or stash them first. Git errors stop generation with a nonzero exit code.
See [the permanent dependency policy](Docs/Dependencies.md). Deliveries are
committed to GitHub; clone the branch above instead of using source ZIPs.

## OUTPOST ZERO TPS sandbox

The playable stress world and asset-demo scene controls have been replaced by one
third-person facility. See [TPS 0–2 and controls](Docs/TPS.md). Regenerate projects
after pulling, because the playable code now lives under `Game/src`.

The [World hot path](Docs/WorldHotPath.md), [physics foundation](Docs/PhysicsFoundation.md),
[managed assets](Docs/Assets.md) and [Phase 5.5](Docs/Phase5.5.md) reports describe
engine APIs and historical validation. Their old playable-demo key maps are
superseded by TPS.md. Automated fixtures remain under `Scripts/Tests/Legacy`.

Keep the generated OpenAL32.dll beside the EXE on Windows. Use Release or Dist
for performance measurements.
