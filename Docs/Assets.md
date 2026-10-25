# Asset, animation and audio foundation

PlagC owns CPU asset data, typed handles, the registry/cache and resource lifetimes.
Assimp is an import tool dependency. Ogre, Ozz and OpenAL remain private adapters.
The existing FPS facility now contains two original textured beacons near the player
spawn: one in its rest pose, one animated. A quiet positional chime comes from the
animated beacon. No downloaded character assets are required.

## Build and try it

Pull `prototype`, run `GenerateProjects.bat`, then build the solution in VS2022.
This adds three shallow, exact-commit submodules and the Assimp, Ozz, OpenAL,
PlagCImport and AssetCooker projects. All vendor compilation is owned by Premake;
none of their CMake builds are invoked. OpenAL is a shared library; keep OpenAL32.dll
beside Sandbox.exe. The projects place both in the same configuration directory.

Run Sandbox from the repository root (VS debug working directory is already set).
The small cooked fixtures are committed, so normal startup needs no cooking step.

| Key / option | Asset-phase action |
|---|---|
| F5 | Pause/resume the animated beacon |
| F6 | Pause/resume the positional chime |
| F7 | Reload both cooked fixtures; existing handles keep the previous snapshots |
| Shift+R | Reset world, including its asset instances |
| `--no-asset-demo` | Run the earlier procedural-only workload for comparisons |
| `--no-audio` | Disable device creation explicitly |

All existing FPS controls remain. Assets are inside the same persistent world;
the two beacon entities count toward the configured entity ceiling. They are display
models without physics bodies; generic grab/throw continues to operate on physical cargo.
Missing or invalid demo files are reported without preventing the facility from running.
No audio device is also a reported, nonfatal state.

```bat
Bin\windows-x86_64\Dist\AssetCooker.exe --demo
Bin\windows-x86_64\Dist\AssetCooker.exe Assets\Source\MyModel.glb Assets\Cooked\MyModel.plasset
Bin\windows-x86_64\Dist\AssetCooker.exe Assets\Source\Sound.wav Assets\Cooked\Sound.plasset
```

`--demo` regenerates the committed beacon and chime. The original glTF fixture is
reproducible with `python Scripts/GenerateAssetFixture.py`. Its buffer and PNG are
embedded, so it is one self-contained source file.

## Data flow and ownership

Source glTF/GLB → Assimp → PlagC model/skeleton/key tracks → versioned `.plasset`.
Source PCM WAV → PlagC AudioClip → versioned `.plasset`.
At runtime AssetManager reads only cooked PlagC files; Sandbox does not link Assimp.

The initial format is a model **bundle**: it owns mesh geometry, materials, RGBA
textures, node hierarchy, skeleton bindings and clips. Those subresources are
engine-owned records within the bundle, not independent top-level asset IDs yet.
Audio clips are a separate typed asset. This keeps dependency release deterministic
without exposing vendor data or making Ogre `.mesh` / Ozz archives the permanent format.

- `AssetID`: stable 64-bit hash of normalized root-relative cooked path. Collisions
  among registered paths are rejected. Renaming a path changes its ID; persistent
  GUID sidecars/editor rename remapping are not part of this first version.
- `AssetHandle<T>`: shared immutable CPU snapshot. Unload, reload and manager
  destruction do not invalidate existing handles. Published manager data is const.
- `AssetMetadata`: ID, type, normalized path, generation and cooked byte size.
- `AssetManager`: owner-thread registry/cache with Queued, Loading, Ready and Failed
  states. `Request` deduplicates paths and schedules CPU file work on PlagC jobs.
  `Pump` publishes completed results. `Load<T>` waits; `Get<T>` is nonblocking.
- Failed reload keeps the last valid snapshot and exposes the failure in `Registry()`.
  Call `Reload` explicitly after recooking; there is no automatic file watcher yet.
- `CollectUnused` drops cache entries with no external handles. Unload does not
  forcibly evict assets retained by scene instances or audio buffers.

Cooked files have magic, version, type, payload length and a checksum. Integers and
floats are explicitly little-endian; C++ structs and vendor archives are never dumped
as raw file layouts. Reads are bounded to 128 MiB per file, validate array lengths,
indices, finite values, hierarchy, weights and tracks, and reject unknown versions.
Writes use a temporary file and atomic replacement. Checksums detect accidental
corruption; they are not cryptographic authentication.

CPU reads never call Ogre/OpenAL. AssetScene and AudioSystem create/release backend
resources on the main thread. Engine destroys the asset manager while its job system
is still alive, drains accepted reads, and tears down audio before the renderer/window.

## ECS and Ogre adapter

```cpp
auto model = engine.Assets().Load<PlagC::ModelAsset>("Beacon.plasset");
auto entity = world.Create("Actor");
world.SetModel(entity, {model, true});
world.SetAnimator(entity, {}); // only if the model has a clip

PlagC::AssetScene assets(engine.Graphics());
assets.Update(world, dt);      // sampling, skinning, renderer upload
// Destroy/clear the bridge before destroying its Renderer.
```

`ModelComponent` and `AnimatorComponent` live in EnTT behind World APIs. AssetScene
visits only model entities, retains instances, detects asset replacement/destruction,
and caches textures/materials/static meshes by immutable model snapshot. Multiple
static instances share those resources. Animated instances own separate mutable
vertex buffers. Instances are destroyed before meshes, materials and textures.

Renderer additions are engine handles: `CreateTexture`, `DestroyTexture`, and
`CreateMesh(data, true)` / `UpdateMesh`. The Ogre adapter uploads decoded RGBA with
sRGB mipmaps, creates PBR materials, updates vertex buffers and refreshes bounds.
Per-mesh instance references avoid scanning the whole stress world for each animated
mesh update. The existing low-level procedural Renderer APIs remain available.

## Animation profile

`SkeletonAsset`, `AnimationClip`, `AnimationState` and `AnimatorComponent` are PlagC
structures. Each Animator privately builds Ozz runtime skeleton/clips, samples into
local poses, runs local-to-model conversion and produces PlagC matrices. It supports
clip selection, time/seek, speed, reverse looping, pause and nonlooping playback.

Skinning is initially **CPU skinning into dynamic Ogre vertex buffers**. Imported
hierarchy and bone offset matrices are preserved; normals use inverse-transpose
transforms. This is a tested correctness baseline, not a claim of scalable GPU
skinning. Ozz runtime build data is currently per Animator; sharing compressed runtime
clips, GPU skinning, blending/state machines, retargeting, root motion and morph targets
remain later work. Permanent cooked data stays independent of those choices.

Supported source profile:

- glTF 2.0 / GLB, triangle meshes, position/normal/UV0, generated tangents, hierarchy,
  up to four influences per vertex, skeleton and linear/spherical-linear key tracks.
- Positive TRS node scales, right-handed Y-up, metres and seconds. Shear, reflected
  transforms and morph animation are rejected rather than silently rendered incorrectly.
- STEP/CUBICSPLINE clips require baking to linear tracks before import.
- Opaque base colour, PNG/JPEG base-colour textures, scalar metallic and roughness.
  Other PBR texture channels and material alpha produce import warnings; full glTF
  material-extension support is not claimed. Texture source reads stay inside the
  model source directory. Only the glTF importer profile is compiled into Assimp.

## Audio profile

AudioSystem wraps OpenAL device/context, buffer and source ownership. AudioClip stores
PCM16 mono/stereo samples and sample rate. Source APIs cover play, pause, stop, gain,
pitch, looping, position, velocity and distance attenuation. Listener position,
orientation, velocity and gain are engine-owned. Spatial sources require mono clips;
stereo clips can play as nonspatial sources. Sources retain clips, and equal clip
snapshots share OpenAL buffers. Stale/foreign handles and invalid parameters fail.

Windows uses OpenAL Soft's WinMM device backend; macOS uses CoreAudio. The minimal
Linux build currently enables OSS plus OpenAL's null/loopback backends. Modern Linux
ALSA/Pulse/PipeWire device integration is not included in this first manual build
profile. Loopback tests exercise the actual OpenAL mixer without audio hardware.
Hardware playback remains a native acceptance check. There is no streaming music,
compressed-audio decoder, DSP graph or automatic physics-triggered sound system yet.
OpenAL Soft is built as a replaceable shared library; its upstream notices and LGPL
license remain in Vendor/openal. Assimp, Ozz and stb notices remain with their pinned
sources. Distributions must include applicable dependency licenses.

## Profiling and validation

The HUD adds asset instance/model counts, animation/deformation CPU time, upload time
and audio source/buffer counts. CSV captures add `asset_animation_ms` and
`asset_upload_ms`; summaries now contain fourteen timing distributions. Whole-frame
prepare time includes this integration. Use `--no-asset-demo` for the old baseline.

`bash Scripts/TestAssets.sh` builds the pinned vendors without CMake and checks import,
cooked roundtrip/corruption, typed async caching/reload, Ozz deformation, ECS/backend
lifetime and actual OpenAL loopback output. `ASSET_SANITIZE=1` instruments PlagC/test
code; cached vendor libraries are not instrumented. `Scripts/TestStressApp.sh` also
checks default-world asset creation, reload and reset using recording graphics.
See [AssetsValidation.md](AssetsValidation.md) for executed evidence and native limits.

### Checking the asset keys

F5/F6/F7 control the beacon demo in the same FPS world; they do not switch scenes.
The status line directly below the world header confirms each key action, reports
when no animated model or chime is ready, and reports load failures. F6 reports an
unavailable audio device and prints its detailed reason to the console. F7 reports
its request, then the model-ready message after the new model is finalized. Reloading
unchanged files should leave the model looking the same.

Press R to return to spawn and look toward the two beacons ahead-left/ahead-right.
If no key confirmation appears, focus the game window and try Fn+F5/F6/F7 on laptops
whose function row defaults to media controls. If the status says a load failed,
read the console error and verify Assets/Cooked contains Beacon.plasset and Chime.plasset.
