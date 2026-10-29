# Editor implementation

Vulkan optimization is paused. The editor follows the agreed backend-first plan.
Default layout: central viewport, hierarchy left, inspector right, asset browser
and console below, play/pause/stop toolbar above. The original wireframe is unavailable;
the user authorized continuing with this layout.

## Milestones

1. Authoring document, selection and undoable component edits.
2. Persistent IDs, stable asset references, versioned scene save/load. Validate a
   replacement world before replacing the document; never serialize GPU handles.
3. Separate Editor executable, pinned ImGui docking dependency, Ogre rendering
   integration supporting both OpenGL and Vulkan. Focus/capture-aware input.
4. Picking and gizmos; one drag is one undo command. Play/pause/stop uses a separate
   runtime world, leaving the authoring document untouched.
5. Convert the game scene to authored data after round-trip validation.

## Implemented

CommandHistory provides bounded undo/redo, branch-aware saved state and failure
handling. Commands must be atomic and may not re-enter history. SceneDocument owns
World, exposes read-only scene access, validates selection and applies rename and
transform commands. No-op edits do not dirty the scene. Main-thread access is enforced.
Tests cover branching, eviction, failed operations, real World edits, foreign selection
and worker rejection. Dynamic physics transforms still obey World's restriction;
separate authored body descriptions from runtime ownership before enabling their edits.

## Scene persistence v1

SceneDocument now serializes names, transforms, cameras, lights, active-camera
selection and model references. Scene-local persistent IDs are decimal strings
in JSON; they survive reload even though runtime Entity handles change. Model
references use AssetManager-relative or mounted paths, not GPU handles. This is
not yet an asset GUID database: renaming an asset still requires reference repair.

Deserialize/Load return a new document. The caller swaps it into the editor only
after success; the old document stays intact on failure. Component and reference
validation precedes model resolution. A supplied resolver can call
AssetManager::Load<ModelAsset>; missing assets fail rather than disappearing.
Resolver cache side effects are not rolled back, but the live scene is untouched.

Save writes to a reserved staging directory beside the destination, closes the
file, then replaces the destination (MoveFileExW on Windows, rename on POSIX).
The dirty flag clears only after successful replacement. This prevents truncating
the last save on ordinary write/serialization errors; it is not a power-loss
durability guarantee. Unsupported versions, duplicate fields/IDs, unknown fields,
bad transforms, malformed UTF-8, dangling cameras and unsafe asset paths fail.
Files are limited to 64 MiB and 100,000 entities.

Physics, animator and raw renderable components are explicitly rejected on save.
They are not silently discarded. The full gameplay scene cannot yet be saved;
its component schemas and persistent resource bindings are the next backend work.
Hierarchy, creation/deletion commands, ImGui, picking/gizmos and isolated play mode
also remain pending. There is no runnable editor executable yet.

## Verification

After fetching the pinned dependencies, run on Linux with GCC/Clang:

```sh
python3 Scripts/Tests/EditorBackendTests.py
```

The runner compiles current World and physics-validation sources, not a prebuilt
engine library. Coverage includes history failures/eviction/branching, real World
editing, selection, thread checks, JSON round-trips, stable IDs, model resolution,
malformed/unsupported documents and preservation of an existing save on failure.
`--sanitize` enables address/undefined-behavior checks. In restricted environments
where LeakSanitizer cannot inspect `/proc`, use `ASAN_OPTIONS=detect_leaks=0`;
this does not provide leak validation.

Linux backend tests and the serialization address/undefined-behavior sanitizer
run passed (leak detection disabled due to environment restrictions). Native
Windows/MSVC builds and file-replacement acceptance remain pending.
