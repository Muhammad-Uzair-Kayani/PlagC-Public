# Dependency lock

| Dependency | Upstream | Release | Pinned commit | Build |
|---|---|---|---|---|
| GLFW | https://github.com/glfw/glfw | 3.4 | 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 | PlagC-maintained Premake StaticLib |
| spdlog | https://github.com/gabime/spdlog | v1.14.1 | 27cb4c76708608465c413f6d0e6b8d99a4d84302 | PlagC-maintained Premake StaticLib |
| Ogre-Next | https://github.com/OGRECave/ogre-next | v3.0.0 | 75643c3997f5b6d2aa1d7bd8400b9be6736d9908 | Separate Premake static libraries |
| EnTT | https://github.com/skypjack/entt | v3.13.2 | 78213075654a688e9da6bc49f7f873d25c26d12c | Header-only Premake utility; private World backend |
| Jolt Physics | https://github.com/jrouwe/JoltPhysics | v5.3.0 | 0373ec0dd762e4bc2f6acdb08371ee84fa23c6db | PlagC-maintained Premake StaticLib; private physics adapter |
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

Assimp, Ozz, OpenAL and ImGui are added in their own phases,
with their own Premake source lists, generated configuration needs and tests.

Ogre's bundled STBI and DDS codecs are enabled; FreeImage and ZIP archives are
excluded. Sample robot.mesh/robot.skeleton and HLMS shader resources are loaded
from the pinned Ogre source tree for the temporary demo. No shader compiler SDK
or CMake configure stage is required for this OpenGL backend.

## Permanent dependency policy

- Parent-repository gitlinks own exact versions. Normal setup never uses
  `--remote`, pulls upstream HEAD, or modifies pins.
- Every new submodule declares `shallow = true` in `.gitmodules`. Setup runs
  sync, then recursive checkout with `--depth 1 --jobs 4`, then verifies status
  and the required files in `Build/DependencyFiles.txt`.
- Existing correct checkouts are reused. A clean wrong revision is restored to
  the parent pin. Dirty or untracked vendor files cause an early failure.
- Shallow fetches can require an additional fetch for an older pinned SHA.
  Servers must make that commit reachable. Errors remain visible; setup never
  silently falls back to downloading full history. Existing full clones are
  reused, not automatically shrunk or deleted.
- Recursive dependencies are supported and verified against their own parent's
  pins. Add required nested source markers when integrating a new dependency.
- Git obtains sources; PlagC-owned Premake files define the build. GLFW,
  spdlog and Ogre each have their own integration under Build/. Header-only
  RapidJSON remains a utility project. Vendor CMake is not invoked. Evaluate
  generated-source requirements explicitly when adding complex libraries.
- Keep third-party types inside subsystem implementations/adapters, out of
  engine-facing APIs wherever practical.
- To add a library: add a submodule at a chosen commit, set shallow=true, add
  required file markers and a Premake integration, document its pin, then test
  its features through Sandbox before committing the parent gitlink.
- To update a library: choose the revision deliberately, build and run Sandbox,
  record Windows/Linux/macOS results (including platforms not yet tested),
  update this table, then commit the new gitlink. Never claim untested platforms.

Windows wrappers use Bash bundled with Git for Windows, so fetching and
verification have one implementation across platforms. Premake also checks the
source markers when invoked directly. Use the generation wrappers for Git pin
verification. CI should call FetchDependencies before generation and Sandbox.

## Setup verification — 2026-10-22

- Fresh fetch: all four pins plus RapidJSON's nested googletest checked out;
  all five repositories report shallow history.
- Repeated fetch with HTTPS remotes blocked: passed, approximately 0.2 seconds.
- Clean wrong GLFW revision: restored to parent pin without network access.
- Modified vendor source: setup failed early and preserved the edit.
- Premake 5.0.0-beta2: VS2022 solution and gmake2 files generated successfully.
- Direct Premake invocation with a missing required header: failed with a
  named file and recovery command.
- Windows batch execution and MSVC compilation require Windows validation;
  solution generation was tested on Linux. Engine runtime code is unchanged.

Setup first attempts checkout using existing objects (`--no-fetch`), then
retries with shallow fetching if objects are missing. Initial missing-module
clones still download sources. Neither path changes the parent's gitlinks.

