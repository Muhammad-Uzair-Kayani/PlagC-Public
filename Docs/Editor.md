# PlagC Editor architecture and implementation plan

The editor is a PlagC client, not an engine subsystem that owns rendering or
gameplay policy. The recovered ImGui wireframe defines the intended experience:
a central viewport, Scene hierarchy on the left, Inspector on the right, Console
and asset/statistics tooling below, renderer controls, and play/pause/stop/step
transport controls. The UI may evolve without changing the authoring contracts
defined here.

Vulkan optimization remains separate from editor development. The editor backend
must stay renderer-backend neutral so the same authoring UI can run over Ogre
OpenGL and Ogre Vulkan.

## Ownership boundaries

### Engine

The engine remains responsible for the reusable runtime services:

- `World` / EnTT entity storage and component validation.
- renderer resources, render targets, settings and statistics.
- `AssetManager`, asset mounts, immutable asset handles and async CPU loading.
- `JobSystem`.
- `Log` and its bounded multi-producer record buffer.
- `Profiler` and renderer/GPU statistics.
- input/window/event delivery.
- physics, animation and audio runtime systems.

Engine hot paths do not depend on Dear ImGui. Editor code must not expose Ogre,
OpenGL, Vulkan, Jolt or EnTT implementation objects to panels.

### Editor backend

`PlagC::Editor` translates engine state into stable authoring operations and
immutable/read-only presentation data:

- `SceneDocument` owns the authoring `World`.
- `CommandHistory` owns undo/redo history.
- `EditorSession` owns scene-file lifecycle and transport policy.
- persistent scene-local entity IDs survive runtime-handle replacement.
- `AuthoringEntity` snapshots feed hierarchy/Inspector panels.
- transform transactions make one continuous drag one undo operation.
- `MainThreadMailbox` is the low-frequency worker-to-owner-thread commit gateway.
- `GenerationGate` rejects stale async results.
- `AssetBrowserModel` publishes generation-checked filesystem/registry snapshots.
- console filtering consumes real `LogRecord` snapshots.
- telemetry consumes real profiler, renderer, asset and job statistics.
- `ViewportState` contains editor camera/input/gizmo presentation state.
- `ViewportTarget` owns only the engine render-target lifecycle.
- `PickingProvider` is the backend-neutral selection contract.
- `PlaySession` owns the isolated runtime copy used by transport controls.

### Editor UI / integration

The separate Editor executable and Dear ImGui/Ogre integration own:

- dockspace, menus, toolbar and visual styling.
- hierarchy, Inspector, Console, Content Browser and Statistics panel drawing.
- keyboard shortcuts and focus/modal routing.
- file-dialog presentation and unsaved-change modal presentation.
- presenting the Ogre render target inside ImGui.
- concrete OpenGL/Vulkan-compatible ImGui renderer integration.
- a concrete picking provider (normally renderer ID-buffer; physics raycast may
  be an optional fallback for entities that own collision).
- gizmo rendering/manipulation.
- material/animation/profiler panels.

The backend deliberately does not include or call Dear ImGui.

## Threading model

PlagC remains a multithreaded engine. Editor authoring follows an owner-thread
model instead of adding locks around the ECS.

| Operation | Threading rule |
| --- | --- |
| `World` structural/component mutation | main/owner thread only |
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

Workers produce owned results. They do not hold ECS references and do not mutate
`SceneDocument`. A worker can post a small commit through `MainThreadMailbox`.
Long-running/editor-refresh work receives a `GenerationGate` epoch; a result that
finishes after a newer request is discarded rather than replacing newer UI data.

This keeps locks out of the simulation/render hot path. The editor mailbox and
asset snapshot locks are low-frequency tooling synchronization only.

## Authoring document

`SceneDocument` owns the authoring world. Public scene reads return engine copies
or `AuthoringEntity` snapshots; panels must not retain EnTT references.

Implemented authoring operations:

- select by runtime or persistent ID.
- create entity.
- delete entity.
- duplicate entity.
- rename.
- set transform.
- add/edit/remove camera.
- add/edit/remove light.
- add/edit/remove model and model visibility.
- set/clear active camera.

Create/delete/duplicate and component edits are undoable. Commands identify their
target by persistent ID rather than a runtime `Entity` handle. This is required
because undoing a deletion recreates the entity with a new session handle; older
commands must still resolve the restored entity.

Persistent IDs use a document high-water mark and are not intentionally reused
after deletion/branching within a document session.

### Hierarchy v1

The current engine `World` has no parent/child transform graph. The editor
therefore exposes a stable flat/root hierarchy snapshot ordered by persistent ID.
That is sufficient for the recovered wireframe's current `Main Scene` entity list.

Parenting/reparenting, local/world transform inheritance and hierarchy ordering are
a separate scene-graph phase. They should be added to `World`/authoring data first,
not simulated only inside ImGui. Current renderer, camera, light, physics and
`TransformSystem` code all interpret `Transform` as world-space, so introducing
parent-relative transforms requires an explicit engine scene-graph design rather
than an editor-only shortcut.

## Undo/redo and continuous edits

`CommandHistory` is bounded (default 256), branch-aware and saved-state aware.
Failed Apply/Revert operations leave the history cursor unchanged. Command
callbacks cannot recursively mutate history.

A continuous Inspector or gizmo edit is a transaction:

1. `BeginTransformEdit` captures the starting transform.
2. `Preview` may update the authoring world many times for interactive feedback.
3. `Commit` records exactly one command using the initial/final values.
4. `Cancel` restores the initial transform and records nothing.

While a transform transaction is active, other document/history mutations are
rejected. The UI should also disable scene Save/Open/Play until the gesture is
committed or cancelled.

Dynamic physics transforms remain physics-owned and cannot enter a transform edit.
A future authored rigid-body description must be separated from live simulation
ownership before the Inspector exposes dynamic-body authoring.

## Scene persistence v1

Persistence remains versioned `PlagC.Scene` v1.

It serializes:

- persistent IDs.
- names.
- transforms.
- cameras.
- lights.
- active-camera selection.
- model references and visibility.

Model references use `AssetManager`-relative/mounted paths, never renderer/GPU
handles. This is not yet an asset-GUID database, so asset renames still require
reference repair.

`Deserialize`/`Load` build a replacement document and validate it before the
caller replaces the current document. A model resolver may call
`AssetManager::Load<ModelAsset>`. Missing or mismatched model assets fail instead
of silently disappearing.

Save stages beside the destination and then replaces the destination. The dirty
flag clears only after successful replacement. Files are limited to 64 MiB and
100,000 entities. Unsupported versions, duplicate fields/IDs, unsafe paths,
invalid transforms, malformed UTF-8 and dangling active cameras are rejected.

Raw renderable, physics and animator components remain explicitly unsupported by
scene v1 serialization. They are never silently dropped.

## Editor session lifecycle

`EditorSession` is the UI-facing controller for scene lifecycle. Panels and menu
callbacks should not independently replace the active `SceneDocument`, track the
current file path, or decide whether authoring is writable during Play.

It owns:

- the current authoring `SceneDocument`.
- current scene path / untitled state.
- New/Open/Save/Save As policy.
- dirty-scene replacement checks.
- the `PlaySession` transport state.
- selection of the document currently shown in the viewport/hierarchy.

### File commands

The intended UI binding is:

| UI command | Backend action |
| --- | --- |
| New Scene | `EditorSession::NewScene(...)` |
| Open Scene | `EditorSession::Open(...)` |
| Save | `EditorSession::Save()` |
| Save As | `EditorSession::SaveAs(...)` |
| Undo/Redo | authoring document `CommandHistory` while not playing |

New/Open are replacement operations. If the authoring document is dirty, the UI
first presents its modal and then passes the resulting replacement policy to the
session. Cancellation leaves the existing document, path, selection and history
untouched.

Open is transactional: a candidate scene must fully deserialize and resolve its
references before it replaces the current document. A failed Open therefore does
not partially mutate the active scene.

Save writes to the current path and is unavailable for an untitled scene until a
path has been supplied. Save As updates the session path only after a successful
save. File-dialog rendering remains UI work; path/lifecycle state remains backend
work.

### Transport commands

The intended toolbar/menu binding is:

| UI command | Backend action |
| --- | --- |
| Play | `EditorSession::Play()` |
| Pause/Resume | `EditorSession::TogglePause()` / equivalent session call |
| Step | `EditorSession::Step()` while paused |
| Stop | `EditorSession::Stop()` |

While stopped, `ViewDocument()` is the authoring document and authoring mutation
is enabled. While playing or paused, `ViewDocument()` is the runtime document and
`AuthoringDocument()` remains protected from editor mutation. The runtime document
is deliberately exposed read-only to normal editor panels so hierarchy,
statistics, picking and diagnostics can continue to resolve persistent IDs during
Play without allowing accidental runtime-to-authoring leakage.

Scene New/Open/Save/Save As and authoring Undo/Redo must be disabled while a play
session owns the viewport. Stop destroys the runtime copy and returns the viewport
to the unchanged authoring document.

## Runtime isolation / play mode

`PlaySession` never simulates the authoring `World`.

On Play:

1. finish/cancel transient editor edits.
2. clone the complete in-memory authoring world into a new `SceneDocument`.
3. preserve persistent IDs so diagnostics/picking can correlate authoring/runtime
   entities.
4. clear runtime undo history and authoring selection.
5. start runtime physics/animation/game systems against the clone.

Pause stops normal simulation updates. Step grants one paused simulation frame.
Stop destroys the runtime document. Runtime transforms, destruction, physics and
gameplay changes therefore cannot leak back into the authoring scene.

The in-memory clone copies currently supported `World` components, including
components scene-v1 persistence cannot yet save. Play is therefore not coupled to
the persistence format.

"Apply runtime changes" is intentionally not automatic. If implemented later it
must be an explicit authoring command.

## Content Browser backend

`AssetBrowserModel` has two data sources:

1. worker-safe filesystem discovery from configured project roots/mounts.
2. owner-thread `AssetManager::Registry()` state.

Entries contain path, editor asset kind, optional runtime asset type/state,
bytes, generation and error text. Registry data can be merged into filesystem
results. Publication is immutable and generation-checked.

The Content Browser must not recursively scan disk every ImGui frame.

Future work:

- persistent asset GUID database.
- import settings and reimport metadata.
- thumbnail cache and generation jobs.
- asset move/rename reference repair.
- material and animation asset schemas.

## Console and statistics

The Console must consume `Engine::Logger().Snapshot()` rather than hardcoded rows.
`ConsoleFilter` filters trace/debug/info/warning/error/critical records and search
text without exposing spdlog.

`CaptureEngineEditorSnapshot` supplies the UI with:

- filtered log records.
- `AssetManager` registry state.
- latest frame/GPU/renderer statistics.
- job-system counters.
- entity count.

The Statistics panel therefore reads the same engine data used by runtime
profiling rather than maintaining a second profiler.

## Viewport and renderer contract

`ViewportTarget` wraps `Renderer::CreateTarget/ResizeTarget/DestroyTarget` and
tracks extent/aspect/revision. It never exposes `GLuint`, `VkImage`,
`VkImageView`, or backend-specific ImGui calls.

Actual target presentation is still an Ogre/ImGui integration responsibility.
The renderer integration must provide a safe backend-specific presentation token
or bridge for both OpenGL and Vulkan without leaking that representation into
the authoring model.

`ViewportState` owns editor-facing gizmo mode, grid/snap settings, extent and
focus/capture state. The UI adapter reports whether chrome/modal/gizmo interaction
blocks scene input; it must not blindly use Dear ImGui's global
`WantCaptureMouse`, because the viewport itself is an ImGui window.

## Picking

`PickingProvider` accepts viewport-relative pixel coordinates, viewport extent
and the active editor camera, and returns a persistent entity ID.

Preferred concrete implementation order:

1. renderer ID-buffer/object-ID picking, so visible non-physics objects are
   selectable.
2. optional physics-ray fallback for collision-only tooling.

Selection ultimately enters `SceneDocument::SelectPersistent`, so the panel,
renderer and runtime never share transient EnTT handles.

## Small backend modules for the Editor executable

The UI engineer can depend on these narrow pieces instead of engine internals:

- `EditorSession` for New/Open/Save/Save As and transport lifecycle.
- `SceneDocument` / `CommandHistory` for authoring and undo/redo.
- `EditorModels` (`MainThreadMailbox`, `GenerationGate`, asset/console/telemetry,
  viewport/input/picking data).
- `EngineEditorBridge` for snapshots from existing engine services.
- `ViewportTarget` for backend-neutral target lifecycle.
- `PlaySession` for the isolated runtime clone and paused/step state.

No additional third-party dependency is required by these backend modules.

The separate UI executable still needs the pinned Dear ImGui docking dependency
and the Ogre-compatible rendering integration chosen by the engineer. Icon/font
libraries are presentation dependencies, not engine dependencies.

## Implemented backend milestone

The backend now provides:

- bounded, re-entry-safe undo/redo plus transaction recording.
- persistent-ID-safe commands across delete/restore.
- undoable create/delete/duplicate.
- real hierarchy/Inspector snapshots.
- undoable camera/light/model authoring.
- one-drag/one-undo transform transactions.
- isolated complete in-memory play-world cloning.
- play/pause/stop/step session state.
- `EditorSession` New/Open/Save/Save As and dirty-replacement policy.
- read-only runtime document viewing while authoring mutation is locked during Play.
- worker-to-main editor mailbox.
- stale async result rejection.
- filesystem/registry asset-browser model.
- real engine log filtering.
- real profiler/job/asset/entity telemetry.
- backend-neutral viewport-target lifecycle.
- backend-neutral picking contract.
- source-level architecture regressions.

No recovered wireframe/UI source is modified by these backend milestones.

## Remaining implementation

The next work is primarily integration, not rebuilding these backend services:

1. engineer: separate runnable Editor executable, pinned ImGui docking and Ogre
   OpenGL/Vulkan integration.
2. wire hierarchy/Inspector to `EditorSession::ViewDocument()` snapshots and
   authoring command methods while stopped.
3. wire File/Edit/transport shortcuts to `EditorSession`; the UI owns dialogs and
   modal presentation, not scene lifecycle state.
4. wire Console/Statistics/Content Browser to real editor models.
5. implement renderer-target presentation inside ImGui.
6. implement concrete renderer picking provider.
7. connect gizmo drag lifecycle to `BeginTransformEdit`.
8. scene graph/parenting and transform inheritance as an explicit engine phase.
9. persistence v2 schemas for authored physics/animation and persistent asset GUIDs.
10. thumbnail/import/reimport workflows.
11. material and animation editor backends/panels.

## Verification

Fast source-level regression checks run on every platform:

```sh
python Scripts/Tests/EditorArchitectureTests.py
```

Linux GCC/Clang backend tests, after pinned dependencies are fetched:

```sh
python3 Scripts/Tests/EditorBackendTests.py
python3 Scripts/Tests/EditorBackendTests.py --sanitize
```

The backend runner compiles current `World`, physics validation, scene
serialization, authoring operations, editor models, play session and editor
session. It also compile-checks the engine bridge and viewport target without
requiring a renderer link.

Windows acceptance is covered by building the complete PlagC static library in
`Scripts\Validate.bat`; that exercises all new `PlagC/src/Editor/*.cpp` under
MSVC in Debug, Release and Dist. Native Windows file-replacement behavior and the
future runnable Editor executable remain acceptance items until the UI target is
pushed.
