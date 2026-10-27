# OUTPOST ZERO: valley expansion

The Visual Max Sandbox is now one playable forest-outpost scene. Existing rooms, targets,
sliding door, stairs, underground lab and supplies remain connected; the south gate opens
onto a road network and a six-house settlement in a 512 m terrain region.

## Build and run on Windows

From the project root in PowerShell:

```powershell
git pull --ff-only --no-recurse-submodules origin prototype
.\GenerateProjects.bat
.\BuildAAAAssets.bat
```

Open `Build/Generated/PlagC.sln`, build the solution in Release x64, and run Sandbox.
Dist remains a windowed application. Runtime staging supplies the same content to every
configuration. The asset command now discovers a VS2022 C++ installation via `vswhere`
when MSBuild is absent from PATH; Developer Command Prompt still works.

The industrial lock is unchanged. Existing validated industrial models are reused instead
of recooked; `--rebuild` forces cooking again. The new landscape lock adds about 14 MiB of
source downloads: the human, animation library, three grass maps and licenses. The human
cooks locally; no source model download is necessary during gameplay. A changed human
assembly script causes the next asset build to recook it. Preflight rejects missing or
changed human/terrain content before staging.

## Controls

| Key | Action |
| --- | --- |
| WASD / mouse | Jog / look |
| Left Alt + WASD | Walk |
| Left Shift + WASD | Sprint |
| Space | Jump |
| Right mouse / left mouse | Aim / fire |
| R / E | Reload / interact with door or supplies |
| Escape / Backspace | Release/capture mouse / redeploy |
| F3 / F4 | Statistics / wireframe |
| F6 | Landscape detail: Balanced → High → Low |
| F7 | Bloom on/off |
| F11 | FPS lock on/off |

There are no scene-selection keys. Start inside the compound; walk around the central
bollard and through the opening in the south wall to reach the settlement.

## Human pipeline

Source: Quaternius Universal Base Characters and Universal Animation Library, CC0. Immutable
mirror revision and SHA-256 values are in `Assets/Manifests/Landscape.lock.json`; original
license text and modifications are in `Landscape.LICENSES.txt`, also staged at runtime.
The Kenney weapon keeps its existing pinned source/license.

`PrepareHuman.py` preserves the textured human, skeleton, skin weights and bind matrices,
then adds a field uniform, armor plates/pouches, helmet, goggles and a named weapon socket.
It transfers animation rotation deltas in world space, compensates the destination bind
pose and retains destination bone lengths. Horizontal root movement is removed because
Jolt owns character movement. This is a controlled same-family rig pipeline, not an
arbitrary humanoid retargeter.

23 clips are cooked: the 14 source actions plus nine derived aim-locomotion clips. Source
walk/jog motion is combined with an aiming upper-body mask offline; reversing time and
turning lower-body orientation produces backward/strafe/diagonal variants. Gameplay drives
crossfades between idle, walk, jog, sprint, aim locomotion, fire, reload, jump/fall/land,
hit and death. Crouch clips are included for future use; crouching the physics capsule is
not bound to a key in this change. The firearm source actions are adapted pistol clips,
not a newly authored rifle motion-capture set. Hand IK and continuous aim-pitch offsets
are not implemented.

## Landscape and rendering

- Deterministic terrain tiles, hills, outer ridges and graded facility/house pads. Jolt uses
  the exact full-detail sampled triangles. Visual LOD is independent; the player's nearby
  ground stays at full detail. The underground lab/tunnel opening is preserved.
- A terrain-following road grid, six enterable houses with pitched roofs/gables, windows,
  porches, foundations and lighting. The original industrial compound retains its gameplay.
- Original procedural pine meshes with a generated needle opacity texture, geometry grass
  patches and textured boulders. These are original procedural assets, not scanned trees
  or production sculpted buildings. Grass ground maps are ambientCG Grass 007.
- Shared pine/rock geometry and materials let Ogre v2 automatically instance/batch matching
  items. Grass is merged per spatial patch. Ogre performs frustum culling; application
  distance culling, three mesh detail levels, hysteresis and reduced distant shadow work
  limit populated-scene cost. F6 changes detail distances without duplicating source meshes.
- Terrain LOD edge skirts hide cracks. Far visual LOD can differ slightly from collision;
  collision always retains the high-detail surface. Boulders/trunks use coarse static
  proxies, while terrain/house floors and walls support traversal and camera collision.
- Facility box geometry is cached by dimensions with tiled UVs, avoiding one texture
  stretched over an entire courtyard/wall. PBR sampling uses mipmaps and 8× anisotropy.
- Cutout alpha testing and two-sided PBR lighting are explicit material options. This does
  not add general alpha-blended material import. The previous chain-link fallback remains.
- Depth-based distance fog and the sky's artistic atmosphere improve depth separation.
  Fog uses named HDR depth and camera clip uniforms, including after camera changes/resize;
  it is not volumetric lighting or physical atmospheric scattering.

## VFX passes

HDR scene + depth → half-resolution soft-knee bright extraction → quarter-resolution
reduction → horizontal Gaussian blur → vertical Gaussian blur → half-resolution upsample
and combine → final HDR composition/tone mapping. FXAA/grading/dithering stay in the final
pass. Bloom no longer reconstructs a multi-radius blur from the scene in that shader.
Each pass binds its source samplers explicitly. F7 changes bloom composition strength.

Gunfire emits a short warm point light at the animated weapon muzzle and an HDR flash.
Hits spawn bounded pooled spark or opaque dust/debris particles. Effects expire and clean
up on redeploy. These are mesh particles, not transparent smoke sprites, collision-simulated
fragments or persistent bullet decals.

## Verification and measurement scope

`Scripts/TestVisualExpansion.sh` (native Linux build) checks the populated scene, gate
traversal, grounding, shared terrain/Jolt ray agreement, LOD triangle reduction, instancing/
batching counters, cleanup and GL errors. It then runs an isolated bright-object bloom
on/off/resize test and verifies an actual halo outside the object. Combat effects are tested
separately for bounded resources and cleanup. `HumanPipelineTests.cpp` samples all 23 clips
at four times and validates finite/bounded deformation, the weapon socket and textures.
`Scripts/TestTPS.sh` retains the existing gameplay checks; `TPS_VISUAL_MAX=1` exercises the
new runtime human and authored materials as well.

The populated native smoke view submitted about 2,850 live objects through 138
Ogre draw command groups, with about 2.4 million triangles across its rendered passes.
The smoke run has both an offscreen window and a capture workspace. These are software-Mesa geometry/batching observations, not a new Vega 7 FPS claim. They
support shared geometry, grass patches and LOD choices; target-driver frame-time tuning
remains hardware-dependent. No further user performance capture is required to run this
scene. Windows/MSVC execution cannot be validated in this Linux environment.

The result is a substantial playable scene expansion with procedural environment art. It
should not be described as photorealistic or a finished commercial character/environment.

Native render captures: [overview](Evidence/Valley-overview.jpg), [gameplay](Evidence/Valley-gameplay.jpg), [settlement](Evidence/Valley-settlement.jpg).
