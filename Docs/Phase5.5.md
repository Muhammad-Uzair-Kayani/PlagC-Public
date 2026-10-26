> Historical phase report: playable scene controls below are superseded by [OUTPOST ZERO](TPS.md).

# Phase 5.5: cameras, gameplay animation and PBR materials

The existing FPS facility is the test scene. It now includes a player-attached
RobotExpressive character and a six-channel material display near `(0, 0, 48)`.
The character is hidden in FPS view and visible in follow/free views.

## Controls

| Key | Action |
| --- | --- |
| F8 | Toggle FPS and follow camera; use follow to watch locomotion |
| F9 | Toggle independent free camera and return to character camera |
| F10 | Trigger Wave, then crossfade back to locomotion |
| WASD | Move character, or free camera when selected |
| Left Shift | Sprint: actual character velocity selects Run |
| Space / Left Ctrl | Rise / descend in free camera |
| F5 | Pause/resume beacon animation |
| F6 | Play/stop chime; HUD reports unavailable audio |
| F7 | Reload all four cooked fixtures |

Stop moving to return to Idle. Walk and sprint select Walk and Run with 0.2-second
crossfades. The HUD shows camera and animation state. Focus the game window; laptop
function rows may require Fn. These keys operate one scene, not a scene selector.

## Engine APIs

`CameraSystem` selects the active world camera and extracts renderer descriptions.
`CameraComponent::aspect == 0` uses the viewport; positive values override aspect.
`FPSCameraController` accepts a physics-owned eye position; `FreeCameraController`
owns independent movement. Controllers accept input values and do not poll GLFW.
SceneRenderer uses the camera system within its existing dirty synchronization path.

`AnimatorComponent` optionally owns an `AnimationController`. An
`AnimationStateMachine` defines named states, clip indices, transitions and typed
float/bool/trigger parameters. Conditions are ANDed; declaration order defines
transition priority. Selected transitions consume their triggers. Controllers copy
the graph and keep independent per-instance values and clocks. Gameplay sets Speed
from actual Jolt character velocity, Grounded from physics, and Wave from F10.

Ozz samples both clips and blends local joint poses before converting to model
space. Two-clip fades finish before another transition is evaluated. This baseline
uses CPU skinning, including tangent deformation; it does not implement root motion,
IK, additive layers, facial morph targets or GPU skinning.

MaterialAsset/MaterialDesc support base colour, normal, metallic, roughness, AO and
emissive textures, plus factors. Base colour/emissive use sRGB; data maps use linear
textures. Imported packed metallic/roughness channels are separated during cooking.
Textures generate mipmaps and use repeat/trilinear sampling. UV0 and opaque materials
are the supported profile; per-material sampler overrides and UV transforms are not
implemented. AO uses a custom Ogre PBS ambient-lighting piece, reserving the detail
weight slot; it does not darken direct lights or emissive output.

Cooked format v2 stores the new material bindings and vertex tangents. The reader
still accepts v1 assets. Engine-owned immutable asset handles outlive cache entries;
AssetScene creates and releases Ogre resources on the main thread.

## Test assets and reproduction

RobotExpressive is by Tomas Laulhe/Quaternius, with upstream modifications by Don
McCurdy, under CC0 1.0. See `Assets/Source/RobotExpressive-LICENSE.md` for the pinned
GitHub source, original file hash and attribution. Preparation removes facial morph
channels and makes node names unique; the original GLB is preserved.

The material fixture is generated from the engine's beacon geometry with distinct
procedural maps. Cooked fixtures are checked in so running the test needs no downloads.
To regenerate with a freshly built AssetCooker from the repository root:

```text
python Scripts/PreparePhase55Character.py
python Scripts/GenerateMaterialFixture.py
AssetCooker Bin-Int/Phase55/RobotExpressive.glb Assets/Cooked/RobotExpressive.plasset
AssetCooker Assets/Source/MaterialLab.gltf Assets/Cooked/MaterialLab.plasset
```

## Validation scope

`Scripts/TestAssets.sh` tests typed parameters, trigger consumption, independent
controllers, crossfade timing and distinct joint poses, camera movement/aspect,
six-channel import and cooked roundtrips, resource lifetime and OpenAL loopback.
`Scripts/TestStressApp.sh` exercises real gameplay/physics with recording graphics,
including camera keys, velocity-driven locomotion, asset reload and reset cleanup.
`Scripts/TestStressWorld.sh` covers world and physics regressions.

The restored workspace was rebuilt and tested on Linux, including the engine,
Sandbox and AssetCooker targets and an Ogre NULL-backend benchmark. The complete
from-source dependency build requires X11 development headers absent here; cached
pinned vendor libraries were used for target builds. NULL rendering and recording
renderers do not verify shader appearance. Native GPU rendering and Windows/MSVC
build/input/resize acceptance tests remain required. Earlier handoff claims of
native visual comparisons are not counted as evidence for this restored checkout.

Executed on the restored checkout: 50 asset/camera/animation assertions passed under
ASan/UBSan (`detect_leaks=0`; cached vendor libraries are uninstrumented), 28 application
assertions passed, and 39 physics/world assertions passed. The NULL-backend benchmark
completed 120 sample frames after 10 warmup frames. These results do not measure GPU
performance or prove material shader appearance.
