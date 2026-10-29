# PlagC Editor architecture and implementation plan

The Editor is a PlagC client. It does not own engine rendering, gameplay policy,
physics, asset lifetime, job scheduling or ECS storage. The recovered ImGui
wireframe defines the intended experience: a central scene viewport, Scene
Hierarchy on the left, Inspector on the right, Content Browser / Console /
Statistics tooling below, renderer controls, and Play/Pause/Stop/Step transport.
The UI can evolve without changing the backend contracts in this document.

The Editor backend must remain renderer-backend neutral so the same authoring
workflow can run over Ogre OpenGL and Ogre Vulkan. Dear ImGui is a presentation
integration dependency, not an engine-core dependency.

## Ownership boundaries

### Engine

The engine owns reusable runtime services:

- `World` / EnTT entity storage, validation and relationship metadata.
- renderer resources, render targets, settings and statistics.
- `AssetManager`, asset mounts, immutable handles and async CPU loading.
- `JobSystem`.
- `Log` and its bounded multi-producer record buffer.
- `Profiler` / GPU / renderer statistics.
- input, window and event delivery.
- physics, animation and audio runtime systems.

Engine hot paths do not depend on Dear ImGui. Editor panels must not retain or
expose Ogre, OpenGL, Vulkan, Jolt or EnTT implementation objects.

### Editor backend

`PlagC::Editor` translates engine state into stable authoring operations and
copied/read-only presentation data:

- `EditorSession` owns scene-file lifecycle and Play transport policy.
- `SceneDocument` owns the authoring `World`.
- `CommandHistory` owns bounded undo/redo history.
- persistent scene-local entity IDs survive runtime-handle replacement.
- `AuthoringEntity` snapshots feed Hierarchy and Inspector panels.
- transform transactions make one continuous drag one undo entry.
- hierarchy operations are persistent-ID based and undoable.
- `MainThreadMailbox` is the low-frequency worker-to-owner-thread commit gateway.
- `GenerationGate` rejects stale async results.
- `AssetBrowserModel` publishes generation-checked asset snapshots.
- console filtering consumes real `LogRecord` snapshots.
- telemetry consumes real profiler, renderer, asset and job statistics.
- `ViewportState` contains editor camera/input/gizmo presentation state.
- `ViewportTarget` owns only engine render-target lifecycle.
- `PickingProvider` is the backend-neutral selection contract.
- `PlaySession` owns the isolated runtime copy used by transport controls.

### Editor UI / integration

The separate Editor executable and Dear ImGui/Ogre integration own:

- dockspace, menus, toolbar, icons, fonts and styling.
- Hierarchy, Inspector, Console, Content Browser and Statistics drawing.
- keyboard shortcuts and focus/modal routing.
- file-dialog and unsaved-change modal presentation.
- presenting an engine render target inside ImGui.
- OpenGL/Vulkan-compatible ImGui renderer integration.
- concrete renderer picking implementation.
- gizmo rendering/manipulation.
- material, animation and profiler panel presentation.

The UI calls backend contracts; it does not duplicate scene lifecycle, undo,
runtime-isolation or ECS policy.

## Threading model

PlagC remains a multithreaded engine. Editor authoring uses owner-thread mutation
rather than adding locks around the ECS.

| Operation | Threading rule |
| --- | --- |
| `World` structural/component/relationship mutation | main/owner thread only |
| `SceneDocument` selection/history/mutation | main/owner thread only |
| `EditorSession` lifecycle/transport operations | main/owner thread only |
| Dear ImGui | main thread only |
| renderer resource/target operations | renderer owner/main thread |
| filesystem discovery | worker allowed |
| import/cook preparation | worker allowed |
| thumbnail CPU preparation | worker allowed |
| log production | any engine thread |
| profiler production | existing engine rules |
| UI consumption | copied/snapshotted data on main thread |

Workers produce owned results. They do not retain ECS references or mutate
`SceneDocument`. A worker may post a small result through `MainThreadMailbox`.
Long-running editor work receives a `GenerationGate` epoch; completion from an
older request is discarded instead of replacing newer UI state.

This keeps editor synchronization out of the simulation/render hot path.

## Authoring document

`SceneDocument` owns the authoring world. Public scene reads return engine copies
or `AuthoringEntity` snapshots; panels do not retain EnTT references.

Implemented authoring operations include:

- select by runtime or persistent ID.
- create entity.
- delete entity.
- duplicate entity.
- rename.
- set transform.
- reparent / clear parent.
- add/edit/remove camera.
- add/edit/remove light.
- add/edit/remove model and model visibility.
- set/clear active camera.

Commands identify targets by persistent ID rather than a runtime `Entity` handle.
This is required because undoing deletion recreates an entity with a new session
handle; older commands must still resolve the recreated entity correctly.

Persistent IDs use a document high-water mark and are not intentionally reused
after deletion or undo-history branching within the document session.

## Scene graph / hierarchy

`World` now owns first-class parent/child relationship metadata:

- `Parent(Entity)`.
- `Children(Entity)`.
- `SetParent(child, parent)`.
- `ClearParent(child)`.

Relationships are main-thread owned, cycle-safe and validated against stale or
foreign entities. `Children()` returns a deterministic ordering. Relationship
changes invalidate topology-dependent bindings conservatively.

Deleting a parent is non-recursive in this phase: its direct children are promoted
to roots. This avoids hidden cascading destruction and gives authoring commands a
well-defined restoration path.

`SceneDocument` exposes each entity's `parentPersistentID` and command-backed
reparenting. Reparent commands store persistent IDs, not transient runtime handles.
Consequently hierarchy survives:

- delete -> undo with runtime handle recreation.
- redo -> undo sequences.
- duplicate.
- save/load.
- authoring -> Play runtime cloning.

Delete/undo restores both the deleted entity's parent and its direct children.
Duplicate currently duplicates one entity as a sibling under the same parent; it
does not recursively duplicate a subtree.

### Transform contract

Parenting is relationship metadata only in the current phase. `Transform` remains
world-space everywhere in the engine. Reparenting therefore does **not** implicitly
move an entity or propagate parent transforms into children.

This is deliberate because the renderer, camera, light, physics and
`TransformSystem` currently interpret `Transform` as world-space. Local transforms,
world-transform propagation and preserve-world reparenting require an explicit
engine transform-graph phase rather than an editor-only shortcut.

Future scene-graph work:

- authored local transform plus resolved world transform.
- deterministic parent-before-child transform evaluation.
- dirty-subtree propagation.
- preserve-world reparenting for drag/drop hierarchy edits.
- optional sibling ordering for manual tree reordering.
- multi-selection reparenting rules.

## Undo/redo and continuous edits

`CommandHistory` is bounded (default 256), branch-aware and saved-state aware.
Failed Apply/Revert operations leave the history cursor unchanged. Command
callbacks cannot recursively mutate history.

A continuous Inspector or gizmo transform edit is a transaction:

1. `BeginTransformEdit` captures the starting transform.
2. `Preview` may update the authoring world repeatedly for interactive feedback.
3. `Commit` records exactly one command using the initial and final values.
4. `Cancel` restores the initial transform and records nothing.

While a transform transaction is active, other document/history mutations are
rejected. The UI should disable New/Open/Save/Play until the gesture is committed
or cancelled.

Dynamic physics transforms remain physics-owned and cannot enter an editor
transform transaction.

## Scene persistence v2

The current writer emits versioned `PlagC.Scene` **v2**.

Version 2 serializes:

- persistent IDs.
- parent persistent ID or `null`.
- names.
- world-space transforms.
- cameras.
- lights.
- active-camera selection.
- model references and visibility.

The loader remains backward compatible with scene v1. V1 files contain no parent
field and load all entities as roots. V2 relationship validation occurs before
asset resolution and rejects:

- missing parent IDs.
- self-parenting.
- hierarchy cycles.

Model references remain `AssetManager`-relative/mounted paths rather than GPU or
renderer handles. This is not yet an asset-GUID database, so asset renames still
require reference repair.

`Deserialize` / `Load` construct and validate a replacement document before the
caller replaces the current scene. Model resolution may call
`AssetManager::Load<ModelAsset>`. Missing or mismatched model assets fail rather
than silently disappearing.

Save stages beside the destination and then replaces the destination. The dirty
flag clears only after successful replacement. Files remain limited to 64 MiB and
100,000 entities. Unsupported versions, duplicate fields/IDs, unsafe paths,
invalid transforms, malformed UTF-8, invalid hierarchy and dangling active
cameras are rejected.

Raw renderable, physics and animator components are still intentionally unsupported
by persisted scenes. They are never silently dropped.

## Editor session lifecycle

`EditorSession` is the UI-facing controller for scene lifecycle. Panels and menu
callbacks must not independently replace the active `SceneDocument`, track the
current path, or decide whether authoring is writable during Play.

It owns:

- the current authoring `SceneDocument`.
- current scene path / untitled state.
- New/Open/Save/Save As policy.
- dirty-scene replacement checks.
- the `PlaySession` transport state.
- selection of the document currently shown by viewport/hierarchy panels.

### File commands

| UI command | Backend action |
| --- | --- |
| New Scene | `EditorSession::NewScene(...)` |
| Open Scene | `EditorSession::Open(...)` |
| Save | `EditorSession::Save()` |
| Save As | `EditorSession::SaveAs(...)` |
| Undo/Redo | authoring document history while stopped |

New/Open are replacement operations. If the authoring document is dirty, the UI
presents its modal and then passes the resulting replacement policy to the
session. Cancellation leaves document, path, selection and history untouched.

Open is transactional: the candidate scene must fully deserialize and resolve its
references before replacing the current document. A failed Open does not partially
mutate the active scene.

Save writes to the current path and is unavailable for an untitled scene until a
path is supplied. Save As updates the session path only after successful save.
File-dialog rendering remains UI work; path/lifecycle state remains backend work.

### Transport commands

| UI command | Backend action |
| --- | --- |
| Play | `EditorSession::Play()` |
| Pause/Resume | `EditorSession::TogglePause()` |
| Step | `EditorSession::Step()` while paused |
| Stop | `EditorSession::Stop()` |

While stopped, `ViewDocument()` is the authoring document and authoring mutation
is enabled. During Play/Pause, `ViewDocument()` is the runtime document while the
authoring document remains protected from editor mutation. Runtime hierarchy,
statistics, picking and diagnostics can still resolve the same persistent IDs.

New/Open/Save/Save As and authoring Undo/Redo must be disabled while a play
session owns the viewport. Stop destroys the runtime copy and returns to the
unchanged authoring document.

## Runtime isolation / play mode

`PlaySession` never simulates the authoring `World`.

On Play:

1. finish/cancel transient editor edits.
2. clone the complete in-memory authoring world into a new `SceneDocument`.
3. preserve persistent IDs and hierarchy relationships.
4. clear runtime undo history and authoring selection.
5. start runtime physics/animation/game systems against the clone.

Pause stops normal simulation updates. Step grants one paused simulation frame.
Stop destroys the runtime document. Runtime transforms, destruction, physics and
gameplay mutations cannot leak back into the authoring scene.

The in-memory clone copies currently supported `World` components, including
components the persistence format cannot yet save. Play is therefore not coupled
to scene persistence.

"Apply runtime changes" is intentionally not automatic. If added later, it must
be an explicit authoring command.

## Content Browser backend

`AssetBrowserModel` has two data sources:

1. worker-safe filesystem discovery from configured project roots/mounts.
2. owner-thread `AssetManager::Registry()` state.

Entries contain path, editor asset kind, optional runtime asset type/state, bytes,
generation and error text. Registry data can be merged into filesystem results.
Publication is immutable and generation-checked.

The Content Browser must not recursively scan disk every ImGui frame.

Future work:

- persistent asset GUID database.
- import settings and reimport metadata.
- thumbnail cache and generation jobs.
- asset move/rename reference repair.
- material and animation asset schemas.

## Console and statistics

The Console consumes `Engine::Logger().Snapshot()` rather than hardcoded rows.
`ConsoleFilter` filters trace/debug/info/warning/error/critical records and search
text without exposing spdlog.

`CaptureEngineEditorSnapshot` supplies the UI with:

- filtered log records.
- `AssetManager` registry state.
- latest frame/GPU/renderer statistics.
- job-system counters.
- entity count.

The Statistics panel reads the same data used by runtime profiling rather than
maintaining a second profiler.

## Viewport and renderer contract

`ViewportTarget` wraps `Renderer::CreateTarget`, `ResizeTarget` and
`DestroyTarget`, and tracks extent/aspect/revision. The authoring model never
exposes `GLuint`, `VkImage`, `VkImageView` or backend-specific ImGui calls.

Actual target presentation remains an Ogre/ImGui integration responsibility. The
renderer integration must provide a safe backend-specific presentation bridge for
both OpenGL and Vulkan without leaking that representation into scene authoring.

`ViewportState` owns editor-facing gizmo mode, grid/snap settings, extent and
focus/capture state. The UI adapter reports whether chrome/modal/gizmo interaction
blocks scene input; it must not blindly use Dear ImGui's global
`WantCaptureMouse`, because the viewport itself is an ImGui window.

## Picking

`PickingProvider` accepts viewport-relative pixel coordinates, viewport extent and
the active editor camera, and returns a persistent entity ID.

Preferred concrete implementation order:

1. renderer object-ID / ID-buffer picking so visible non-physics objects are
   selectable.
2. optional physics-ray fallback for collision-only tooling.

Selection ultimately enters `SceneDocument::SelectPersistent`, so panels,
renderer and runtime do not share transient EnTT handles.

## Small backend modules for the Editor executable

The UI engineer can depend on these narrow pieces rather than engine internals:

- `EditorSession` — New/Open/Save/Save As and transport lifecycle.
- `SceneDocument` / `CommandHistory` — authoring and undo/redo.
- `EditorModels` — mailbox, generations, assets, console, telemetry,
  viewport/input/picking data.
- `EngineEditorBridge` — snapshots from existing engine services.
- `ViewportTarget` — backend-neutral target lifecycle.
- `PlaySession` — isolated runtime clone and pause/step state.

No additional third-party dependency is required by these backend modules.

The separate UI executable still needs the pinned Dear ImGui docking dependency
and Ogre-compatible UI rendering integration selected by the engineer. Icons and
fonts are presentation dependencies, not engine dependencies.

## Implemented backend milestones

The backend now provides:

- bounded, re-entry-safe undo/redo plus transaction recording.
- persistent-ID-safe commands across delete/restore.
- undoable create/delete/duplicate.
- undoable camera/light/model authoring.
- one-drag/one-undo transform transactions.
- first-class cycle-safe world parent/child metadata.
- deterministic parent/children queries.
- undoable persistent-ID reparenting.
- exact hierarchy restoration across delete/undo and Play cloning.
- duplicate-as-sibling hierarchy behavior.
- scene v2 hierarchy persistence with v1 loading compatibility.
- isolated complete in-memory Play-world cloning.
- Play/Pause/Stop/Step session state.
- `EditorSession` New/Open/Save/Save As and dirty-replacement policy.
- read-only runtime document viewing during Play.
- worker-to-main editor mailbox and stale-result rejection.
- filesystem/registry asset-browser model.
- real engine log filtering and telemetry.
- backend-neutral viewport-target lifecycle.
- backend-neutral picking contract.
- architecture and portable backend regression tests.

No recovered wireframe/UI source is modified by these backend milestones.

## Remaining implementation

The remaining work is primarily integration plus deeper editor/runtime tooling:

1. engineer: separate runnable Editor executable, pinned ImGui docking and Ogre
   OpenGL/Vulkan integration.
2. wire Hierarchy/Inspector to `EditorSession::ViewDocument()` snapshots and
   stopped-mode authoring commands.
3. wire drag/drop hierarchy to `SceneDocument::SetParent/ClearParent`.
4. wire File/Edit/transport shortcuts to `EditorSession` and history.
5. wire Console/Statistics/Content Browser to real editor models.
6. implement renderer-target presentation inside ImGui.
7. implement concrete renderer picking provider.
8. connect gizmo drag lifecycle to `BeginTransformEdit`.
9. add local/world transform inheritance and preserve-world reparenting.
10. add authored physics/animation persistence and persistent asset GUIDs.
11. add thumbnail/import/reimport workflows.
12. add material and animation editor backends/panels.

## Verification

Fast source-level architecture checks:

```sh
python Scripts/Tests/EditorArchitectureTests.py
```

Portable GCC/Clang backend tests after pinned dependencies are fetched:

```sh
python3 Scripts/Tests/EditorBackendTests.py
python3 Scripts/Tests/EditorBackendTests.py --sanitize
```

The backend runner includes world hierarchy and editor hierarchy tests in addition
to the existing history, document, persistence, model, Play and session tests.

Windows acceptance remains the complete PlagC MSVC build through:

```bat
Scripts\Validate.bat
```

That compiles all `PlagC/src/Editor/*.cpp` and the modified world code in Debug,
Release and Dist. The runnable Editor executable remains a separate integration
acceptance item owned by the engineer's UI milestone.
