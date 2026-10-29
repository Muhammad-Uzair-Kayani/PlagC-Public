# PlagC

PlagC is a cross-platform C++20 engine foundation built around an engine-owned
application loop, GLFW platform integration, Ogre-Next rendering, EnTT world/ECS,
Jolt physics, Ozz animation, OpenAL audio, managed assets and editor authoring
infrastructure.

The `prototype` branch is currently focused on the editor. Gameplay experiments
that were previously compiled into Sandbox have been removed from the engine
runtime. PlagC now owns mechanisms only; client projects own gameplay policy,
content and world selection.

## Architecture

```text
PlagC engine
    |
    +-- PlagC::Application boundary
            |
            +-- Sandbox (engine smoke client)
            +-- Editor
            +-- future game/tool projects
```

`PlagC` must not depend on a game project. Sandbox is intentionally small: it
creates a primitive, material, light and camera through public engine APIs and
serves as the default runtime/build smoke test.

Reusable facilities promoted from earlier experiments include:

- `PlagC::AssetManifest` for loading/unloading named sets of managed assets.
- `PlagC::FpsOverlay` for renderer-agnostic diagnostic FPS display.
- backend-neutral performance and latency profiling.
- the existing World/SceneRenderer, camera, animation, physics and asset systems.

## Windows / Visual Studio 2022

Prerequisites:

- Git
- Visual Studio 2022 with **Desktop development with C++**
- a Windows SDK
- Python 3
- Premake 5.0.0-beta2 (the repository also currently tracks `premake5.exe`)

Clone and generate:

```bat
git clone --branch prototype https://github.com/Muhammad-Uzair-Kayani/PlagC-Public.git
cd PlagC-Public
GenerateProjects.bat
```

Open `Build\Generated\PlagC.sln`, select `Debug`, `Release` or `Dist`, and build
`Sandbox`.

Run the complete engine validation from a **VS2022 Developer Command Prompt**:

```bat
Scripts\Validate.bat
```

The validation script runs architecture/staging regression tests, builds
Debug/Release/Dist, and executes Sandbox in headless and GLFW-null modes.

Individual example:

```bat
msbuild Build\Generated\PlagC.sln /m /p:Configuration=Debug /p:Platform=x64
Bin\windows-x86_64\Debug\Sandbox.exe --headless --frames 120 --workers 4
```

## Renderer backends

By default the runtime can select from the backends compiled into the build.
Useful options are:

```text
--renderer auto|opengl|vulkan|null
--headless
--null
--offscreen
--frames N
--workers N
--no-audio
```

For Vulkan, install the Vulkan SDK and regenerate with:

```bat
GenerateProjects.bat --vulkan
```

See `Docs/RendererBackends.md` and `Docs/VulkanPerformance.md`.

## Linux / macOS

```sh
chmod +x GenerateProjects.sh Scripts/Validate.sh
./GenerateProjects.sh gmake2
make -C Build/Generated config=debug -j4
./Bin/linux-x86_64/Debug/Sandbox
```

For a machine without a window server:

```sh
./GenerateProjects.sh gmake2 --null-platform
make -C Build/Generated config=debug -j4
./Bin-null/linux-x86_64/Debug/Sandbox --null --frames 120 --workers 4
```

Linux native rendering requires the X11/OpenGL/EGL development packages used by
the pinned GLFW/Ogre configuration. macOS uses Cocoa and system frameworks.

## Runtime assets

`Scripts/StageRuntimeAssets.py` stages **engine-owned renderer resources only**:
textures, shaders, compositors and required Ogre media. Game/client cooked assets
are deliberately outside the engine staging contract and must be packaged by the
client project that owns them.

The generic managed-asset pipeline and AssetCooker remain engine tooling; see
`Docs/Assets.md`.

## Layout

- `PlagC/include/PlagC` — public engine API.
- `PlagC/src` — engine/backend implementation.
- `PlagC/include/PlagC/Editor`, `PlagC/src/Editor` — editor authoring backend.
- `Sandbox/src` — minimal client used for runtime smoke validation.
- `Tools/AssetCooker` — engine asset cooker.
- `Scripts/Tests` — engine, editor, renderer, physics and performance regressions.
- `Build` — pinned dependency/build integration.
- `Docs` — design, subsystem and historical validation material.

Older gameplay/visual documents and assets may remain as historical evidence or
future client-project material, but they are not compiled, staged or selected by
the PlagC engine.

## Validation and sanitizers

Linux validation:

```sh
Scripts/Validate.sh
```

Sanitizer builds:

```sh
./GenerateProjects.sh gmake2 --null-platform --sanitize=address
make -C Build/Generated config=debug -j4
./Bin-null-address/linux-x86_64/Debug/Sandbox --null --frames 120
```

`--sanitize=thread` is also supported on Linux where the host environment permits
ThreadSanitizer.

## Dependency policy

Dependencies are pinned submodules and generated through Premake. Setup scripts
never update pins to arbitrary upstream HEAD. See `Docs/Dependencies.md`.

PlagC uses the repository's MIT license; dependencies retain their own licenses.
