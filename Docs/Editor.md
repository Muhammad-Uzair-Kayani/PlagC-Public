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

There is no runnable editor UI yet. Persistence, hierarchy, assets, ImGui and play
mode are subsequent work. Windows/MSVC acceptance remains pending.
