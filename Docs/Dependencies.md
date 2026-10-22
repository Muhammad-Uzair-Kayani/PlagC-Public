# Dependency lock

| Dependency | Upstream | Release | Pinned commit | Build |
|---|---|---|---|---|
| GLFW | https://github.com/glfw/glfw | 3.4 | 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 | PlagC-maintained Premake StaticLib |
| spdlog | https://github.com/gabime/spdlog | v1.14.1 | 27cb4c76708608465c413f6d0e6b8d99a4d84302 | PlagC-maintained Premake StaticLib |

| Ogre-Next | https://github.com/OGRECave/ogre-next | v3.0.0 | 75643c3997f5b6d2aa1d7bd8400b9be6736d9908 | Separate Premake static libraries |
| RapidJSON | https://github.com/Tencent/rapidjson | v1.1.0 | f54b0e47a08782a6131cc3d60f94d038fa6e0a51 | Header-only Premake utility |

The Git index gitlinks are authoritative. `.gitmodules` records upstream URLs.
These are deliberate reproducible pins, not claims of latest available releases.
Premake is a build tool, not a linked dependency; install it separately in Tools
or on PATH. The tested Linux generator is 5.0.0-beta2. Vendor CMake files remain
present in the submodules but are not used. spdlog's bundled fmt is built as part
of spdlog. GLFW Null backend is included alongside the selected desktop backend.

To deliberately update a vendor: choose a commit, checkout that commit inside
the submodule, validate builds and Sandbox, update this file, then commit the
submodule gitlink in the parent repository. Normal generation never updates pins.

EnTT, Jolt, Assimp, Ozz, OpenAL and ImGui are added in their own phases,
with their own Premake source lists, generated configuration needs and tests.

Ogre's bundled STBI and DDS codecs are enabled; FreeImage and ZIP archives are
excluded. Sample robot.mesh/robot.skeleton and HLMS shader resources are loaded
from the pinned Ogre source tree for the temporary demo. No shader compiler SDK
or CMake configure stage is required for this OpenGL backend.
