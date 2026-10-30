# PlagC Editor — architecture, usable-authoring roadmap, and execution log

## Purpose

This document is the source of truth for PlagC Editor work on `prototype`.
Every editor milestone is designed, implemented, tested, pushed, and then recorded here before work moves to the next milestone.

The next product target is deliberately concrete:

> Build a small illuminated 3D scene entirely inside PlagC Editor, save it, reopen it, press Play, and test it without leaving the editor.

A milestone is not complete because a backend API exists. It is complete when the authoring workflow exposed to the user is usable and regression-protected.

## Execution rules

1. `prototype` is the source of truth. Do not restart completed work on parallel `fix/*` branches.
2. Work on one numbered milestone at a time unless a prerequisite is strictly required by that milestone.
3. Before implementation, write/confirm the design and acceptance criteria.
4. Implement against existing engine/editor contracts instead of duplicating systems in the UI.
5. Add focused regression coverage for every milestone.
6. Run source regressions, project generation, and the Windows Editor build gate where applicable.
7. Push the milestone to `prototype`.
8. Update this document in the same milestone commit with status, tests, files, and commit evidence.
9. Stop after each milestone and wait for approval before starting the next one.

Assigned milestone lane for this implementation stream: **1, 4, 7, 10, 13, 16**. Work must not leak into other milestones; each assigned milestone must be finished before moving to the next assigned milestone.

## Status legend

- `DONE` — implemented, regression-protected, pushed.
- `IN PROGRESS` — current milestone only.
- `FOUNDATION` — meaningful backend support already exists, but the milestone's usable authoring experience is incomplete.
- `NOT STARTED` — milestone has not entered implementation.
- `BLOCKED` — requires an external prerequisite and must state it explicitly.

## Architecture contract

### Engine ownership

The engine owns runtime truth: `World`/EnTT storage, renderer resources and targets, `AssetManager`, `JobSystem`, logging/profiling, input/window/events, physics, animation and audio. Engine hot paths do not depend on Dear ImGui.

### Editor backend ownership

`PlagC::Editor` owns stable authoring operations and copied presentation data: `EditorSession`, `SceneDocument`, `CommandHistory`, persistent entity IDs, hierarchy operations, transform transactions, asset-browser models, console filtering, telemetry, viewport state, picking contracts, and isolated `PlaySession` runtime copies.

### Editor frontend ownership

The Editor executable owns docking, menus, toolbars, hierarchy/inspector/content/console/statistics presentation, shortcuts, dialogs, viewport presentation, gizmos, renderer picking integration, material/animation authoring UI, icons/fonts/theme and editor-only visualization.

### Non-negotiable invariants

- Authoring `World` mutation remains owner/main-thread only.
- Runtime Play never simulates the authoring world directly.
- Persistent scene IDs, not transient EnTT handles, cross editor subsystem boundaries.
- Ogre/OpenGL/Vulkan/Jolt/EnTT implementation objects do not leak into editor-facing contracts.
- The same authoring workflow must work over Ogre OpenGL and Ogre Vulkan.
- Continuous transform gestures produce one undo record.
- Worker-produced editor data uses owned results and generation rejection; workers do not mutate `SceneDocument`.

## Current baseline already present

The repository already contains substantial foundations that the milestones below build on:

- docked standalone Editor executable and backend-neutral ImGui submission;
- hierarchy snapshots and persistent-ID selection;
- create/delete/duplicate/rename and reparent authoring commands;
- scene save/open/save-as and unsaved-change handling;
- bounded undo/redo and transform transactions;
- camera/light/model/physics/animator authoring APIs;
- cooked model drag/drop into the viewport;
- transform gizmos and editor RMB/WASD/QE navigation;
- renderer-backed viewport picking for normal renderables and `ModelComponent` instances (`5c35f66`);
- isolated Play/Pause/Stop/Step runtime world with physics and animation;
- Play mode runtime active camera with editor-camera fallback (`3262faf`);
- Content Browser filesystem/registry model;
- Console filtering and engine telemetry;
- persistent renderer preferences and restart flow;
- standalone ImGui font texture path and UI-rendering regressions;
- MSVC/OpenGL Editor validation workflow with hosted-runner WGL limitation handled explicitly.

These foundations do **not** mark the product milestones below complete by themselves.

# Usable Editor Roadmap — 43 Milestones

## 1. Viewport / Scene View — `DONE`

Deliver a scene view suitable for day-to-day authoring rather than a raw render target.

Acceptance scope:

- readable editor preview lighting independent of scene lighting;
- editor background/environment presentation and exposure control;
- infinite/adaptive ground grid and world-axis visualization;
- reliable renderer-backed object picking and empty-space deselection;
- selection feedback in the scene view;
- translate/rotate/scale gizmo mode controls;
- local/world transform-space control;
- translation/rotation/scale snap controls;
- perspective/orthographic camera modes;
- front/back/left/right/top/bottom camera presets;
- focus-selected (`F`) and frame-all behavior;
- adjustable editor camera speed;
- viewport orientation indicator;
- viewport toolbar;
- shaded/wireframe controls and grid/gizmo visibility toggles;
- viewport resize/DPI correctness on OpenGL and Vulkan.

Regression coverage: `Scripts/Tests/EditorViewportSceneViewTests.py`, picking math/asset-drop/gizmo/play regressions, generated-project validation, and the Windows Editor CI build gate.

## 2. Scene Hierarchy — `DONE`

Replace the functional tree/debug presentation with a real hierarchy workflow: search/filter, type icons, expand/collapse, inline rename, context menu, visibility/lock controls, drag reparent/root drop, selection synchronization, stable expansion state and useful empty-scene presentation.

## 3. Entity Creation — `DONE`

Provide Create workflows for Empty, Model, Camera, Directional/Point/Spot Light, physics objects and basic primitives. New entities receive sensible required components and model drops create at a meaningful scene position.

## 4. Inspector — `DONE`

Turn the raw component/debug view into component cards with aligned property rows, add/remove/reset actions, validation, tooltips, read-only states and undo/redo-backed editing.

## 5. Transform Inspector — `DONE`

Professional X/Y/Z position/rotation/scale controls, color-coded axes, reset actions, uniform scaling option, local/world awareness, immediate viewport feedback, and one-gesture/one-undo behavior.

## 6. Model / Mesh Inspector — `DONE`

Model asset field, browse/drop/replace, visibility, shadow controls, mesh/submesh information, material slots, bounds, reload and locate-in-browser actions.

## 7. Material System + Inspector — `DONE`

Authorable material assets and Ogre PBS mapping for base color, base-color texture, normal, metallic, roughness, AO, emissive, alpha/cutout, double-sided state, material slots, save/load and immediate viewport preview with correct color-space handling.

## 8. Lighting Inspector — `DONE`

Directional/Point/Spot editing, color, intensity, range, cones, shadows, and viewport light visualization with immediate scene updates.

## 9. Camera Inspector — `DONE`

FOV/clip/aspect editing, active-camera controls, frustum visualization, camera preview, align-camera-to-view and align-view-to-camera actions.

## 10. Physics Inspector — `FOUNDATION`

Static/Kinematic/Dynamic body authoring, collider type and dimensions, mass/friction/restitution, layers/masks/triggers, validation, and collider debug visualization.

## 11. Animator Inspector — `DONE`

Skeleton/clip/controller assignment, playback controls, loop/speed, state and parameter editing, animation preview and skeleton diagnostics.

## 12. Animation State Machine Editor — `DONE`

Node-based states, entry state, transitions, conditions, parameters, transition duration/blending, state inspector, runtime active-state visualization and controller asset persistence.

## 13. Content Browser — `FOUNDATION`

Replace the registry-table presentation with folders, breadcrumbs, grid/list views, asset cards, search/filter/sort, context menus, drag/drop, double-click behavior, details, source-vs-cooked presentation and sensible filtering of internal files.

## 14. Thumbnail / Preview System — `NOT STARTED`

Preview scene/camera/lighting, model/material/texture thumbnails, cache and invalidation, asynchronous generation and non-blocking placeholders.

## 15. Asset Import Workflow — `DONE`

Import/reimport UI for supported model/texture formats, source-to-cooked relationship, scale/coordinate/normal/tangent/material/animation options and surfaced import errors.

## 16. Scene File Workflow — `FOUNDATION`

New/Open/Save/Save As/Recent scenes, dirty indicator, transactional replacement, unsaved prompts, scene-version validation, persistent hierarchy/components/material assignments and useful load/save errors.

## 17. Play Toolbar — `FOUNDATION`

Professional Play/Pause/Step/Stop transport, clear runtime visual state, shortcuts, authoring lock policy, runtime camera, physics/animation lifecycle and clean return to the unchanged authoring world.

## 18. Runtime / Game Logic — `DONE`

Define gameplay update/lifecycle hooks, editor-vs-game input routing, physics step callbacks, animation/audio ordering, trigger/collision routing and correct Pause/Step semantics.

## 19. Console — `FOUNDATION`

Severity styling/icons, compact entries, expandable details, category/search filtering, repeated-message collapsing, copy/context actions and separation of giant profiler dumps from normal logs.

## 20. Statistics / Profiler — `FOUNDATION`

Frame/FPS graphs, CPU/GPU/scene/physics/animation/asset/UI timings, job utilization, draw calls, triangles, counts, backend/GPU info, history and profiler freeze controls.

## 21. Editor Toolbar — `FOUNDATION`

Save/undo/redo, transform tools, local/world, pivot/center, snap, Play transport, settings and icon-based presentation.

## 22. Menu Bar — `FOUNDATION`

Complete File/Edit/Entity/Assets/View/Window/Debug/Help menus, shortcuts and context-sensitive enable/disable behavior.

## 23. Editor Theme / Visual System — `NOT STARTED`

A deliberate PlagC visual language: typography, spacing, component cards, headers, selection/hover/disabled colors, padding, borders, toolbar/dock styling and strong contrast instead of raw ImGui defaults.

## 24. Icons / Fonts — `NOT STARTED`

Consistent embedded icon/font assets for entities, folders, assets, cameras, lights, physics, transport and transform tools without shipping raw font source files.

## 25. Docking / Workspace — `FOUNDATION`

Strong default layout, save/restore/reset workspace, sane panel locations and support for useful detached/docked workflows.

## 26. Selection System — `FOUNDATION`

Hierarchy↔viewport synchronization, independent asset selection, Inspector tracking, safe deletion behavior and later Ctrl/Shift multi-selection.

## 27. Undo / Redo — `FOUNDATION`

Cover transform, rename, create/delete/duplicate, reparent, component add/remove/edit, model/material/light/camera/physics changes with correct command labels and transaction coalescing.

## 28. Drag & Drop — `FOUNDATION`

Asset→viewport, asset→Inspector, hierarchy reparenting, material→slot, animation→animator, validation and visible drop feedback.

## 29. Editor Preview Rendering — `NOT STARTED`

Explicit editor-vs-game visualization policy: preview lighting, grid, icons, selection, colliders, camera/light gizmos and debug views never leak into game output.

## 30. Rendering Quality — `FOUNDATION`

Validate PBS materials, texture color spaces, normal/metallic/roughness, HDR/tone mapping, shadows, environment/IBL, MSAA, Vulkan/OpenGL parity, resizing, DPI and UI compositing.

## 31. Asset Manager Integration — `FOUNDATION`

Loading states/failures, missing assets, reload, dependencies, stable asset references, move/rename behavior and background loading surfaced cleanly to editor workflows.

## 32. Prefab-like Reusable Objects — `NOT STARTED`

Reusable scene-object templates, hierarchy instantiation, overrides, apply/revert and later nested reusable objects. Not required for the first usable-editor acceptance target.

## 33. Audio Editor Integration — `FOUNDATION`

Audio source/listener authoring, clip fields, preview playback, volume/pitch/loop/spatialization/range and OpenAL-backed runtime preview.

## 34. Debug Visualization — `NOT STARTED`

Physics colliders, bounds, skeletons, camera frustums, light ranges, wireframe and later normals/tangents/navigation overlays.

## 35. Keyboard Shortcuts — `FOUNDATION`

Save/Save As/Undo/Redo/Delete/Duplicate/Focus/W-E-R/Play/Pause/Escape and context-safe routing that does not fire while typing or inside modal UI.

## 36. Editor Settings — `FOUNDATION`

Renderer backend, theme, camera speed, grid/snap values, asset paths, defaults and persisted settings with clear restart gating.

## 37. Performance — `FOUNDATION`

Responsive hierarchy/content browser at scale, async thumbnails/assets, minimal scene rebuilds, SceneRenderer sync improvements and editor responsiveness under large scene workloads.

## 38. Error Handling — `FOUNDATION`

No silent failures: actionable asset/scene/renderer/physics/save/import errors with safe rollback behavior.

## 39. Notifications / Feedback — `NOT STARTED`

Non-blocking editor toasts/status feedback for save/import/failure/restart-required/invalid-action events instead of relying exclusively on Console output.

## 40. Testing — `FOUNDATION`

Automated coverage for authoring workflows, persistence, drag/drop, transforms, materials, lighting, camera, physics, animation, transport, picking, gizmos, backend compilation, native runtime, DPI/resize and Release/Dist acceptance.

## 41. CI / Build — `FOUNDATION`

MSVC Editor gate, OpenGL compile validation, Vulkan compile validation, source regressions, architecture/persistence/play/picking/UI tests and dependency-pin checks without false failures from unavailable hosted GPU contexts.

## 42. Repository Cleanup — `FOUNDATION`

Audit old `fix/*` branches, preserve unique work, merge only genuinely missing commits, delete obsolete branches and keep `prototype` authoritative.

## 43. First Usable Editor Acceptance Target — `NOT STARTED`

Definition of done for the product milestone:

1. launch PlagC Editor;
2. create a new scene;
3. drag in a floor and several models;
4. select them from viewport and hierarchy;
5. move/rotate/scale them;
6. organize hierarchy;
7. add/edit lights;
8. assign/edit materials;
9. add and activate a camera;
10. add physics to an object;
11. save the scene;
12. close and reopen it with properties intact;
13. press Play and view through the game camera;
14. physics and animation run;
15. Pause and Step work;
16. Stop returns to the unchanged authoring scene.

# Milestone Execution Log

## Roadmap baseline — `DONE`

- Date: 2026-10-30
- Purpose: replace the obsolete integration-only remaining-work list with the 43-milestone usable-editor roadmap.
- Policy: every future editor milestone updates this file in the same implementation commit.

## 1. Viewport / Scene View — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 1 completion commit on `prototype`, building on `4022c07`, `e35d6b5`, and `8b62a22`.
- Delivered: renderer-backed selection and empty-space deselection; adaptive world grid/axes; selection feedback; viewport XYZ orientation indicator; Translate/Rotate/Scale toolbar; local/world transform space; movement/rotation/scale snapping; perspective and real Ogre orthographic projection; orthographic-aware picking and asset-drop rays; six axis view presets; `F` focus and Frame All; stable focus distance; adjustable camera speed; grid/gizmo/wireframe toggles; exposure control; three editor-only preview lights; and editor-only `PlagCEnvironment.dds` sky/reflection environment with a readable background that restores the pre-editor renderer state during Play and shutdown.
- Isolation: preview lights/environment are disabled/restored in Play mode; the runtime camera reasserts perspective after Scene View submission, so editor projection state cannot leak into game rendering.
- Regression: `Scripts/Tests/EditorViewportSceneViewTests.py` covers Scene View state/UI wiring, preview-light/environment lifetime and Play isolation, projection/picking, focus/presets, gizmo-space handling and roadmap completion; existing asset-drop, picking, gizmo and Play regressions remain active. The Windows workflow previously proved all Scene View source regressions plus fresh project generation before concurrent `prototype` pushes cancelled MSBuild; the completion commit remains under the same Editor CI gate.
- Scope boundary: no Inspector, Material, Physics, Content Browser or Scene File milestone work is included in Milestone 1.

## 2. Scene Hierarchy — `DONE`

- Date: 2026-10-30
- Commit evidence: this Milestone 2 implementation commit on `prototype`.
- Delivered: name/type search, component-type badges, explicit expand/collapse with stable persistent-ID expansion state, `F2` and context inline rename, entity context actions, command-backed model/renderable visibility, editor-only locks that block hierarchy selection/reparent/destructive actions and viewport picking, drag reparent/root-drop, hierarchy↔viewport selection synchronization, read-only Play presentation, and useful empty-scene actions.
- Architecture: hierarchy mutations continue through `SceneDocument`; visibility remains scene/runtime truth while lock/search/expansion remain editor-only presentation state and reset on document replacement.
- Regression: `Scripts/Tests/EditorSceneHierarchyMilestoneTests.py` plus the existing backend `EditorHierarchyTests.cpp`; `ValidateEditor.bat` and Windows CI run the new source gate.

## 3. Entity Creation — `DONE`

- Date: 2026-10-30
- Commit evidence: `75eab676cda60ce6d5393bfd91a6e06d7c85a359` on `prototype`.
- Delivered: command-backed creation for Empty, cooked ModelAsset, Camera, all three light types, renderer-backed primitives and useful static/dynamic/kinematic physics presets with contextual placement.
- Regression: `Scripts/Tests/EditorMilestone3Tests.py`, `EditorAssetInteractionTests.py`, Windows Editor CI wiring.

## 4. Inspector — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 4 implementation commit on `prototype`.
- Delivered: a coherent entity header with rename/status/parent context; removal of the raw component yes/no debug summary; reusable collapsible component cards; aligned two-column property rows; centralized `+ Add Component`; per-card `Reset` and `Remove Component` actions; validation messages and property tooltips; Camera/Light/Model/Physics/Animator baseline authoring; hierarchy-lock protection; and the same Inspector surface presented read-only for the isolated Play runtime.
- Architecture: Inspector mutations remain command-backed through `SceneDocument`; no editor-facing control mutates renderer/Jolt/EnTT implementation objects directly. Existing specialized Transform, Model/Mesh and Camera inspectors remain their milestone owners and are composed into the Inspector rather than reimplemented here.
- Regression: `Scripts/Tests/EditorMilestone4InspectorTests.py` is wired into both `ValidateEditor.bat` and the Windows Editor workflow, covering card structure, read-only policy, validation/help, command-backed mutation, roadmap completion and scope boundaries.
- Scope boundary: Milestone 4 does not add Milestone 5 transform depth, Milestone 7 material authoring, Milestone 10's complete physics layer/mask/debug workflow, Milestone 13 Content Browser work, or Milestone 16 scene-file work.
- Next assigned milestone: **7. Material System + Inspector** — do not begin until explicitly requested.

## Milestone 5 — Transform Inspector — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 5 implementation commit on `prototype`.
- Delivered: professional Position/Rotation/Scale rows with color-coded X/Y/Z controls, Local/World editing, parent-space context, proportional uniform scaling, per-row and full-transform reset actions, immediate viewport previews, hierarchy-lock and dynamic-physics read-only handling, and one continuous drag recorded as one undo command.
- Architecture: transform authoring continues through `SceneDocument::BeginTransformEdit`/`TransformEdit::Preview`/`Commit`; Local mode resolves through canonical `ComposeTransform`, World mode remains the engine-facing transform contract, and no general Inspector/component-card work from Milestone 4 is included.
- Regression: `Scripts/Tests/EditorMilestone5Tests.py` plus the existing transaction coverage in `EditorDocumentTests.cpp`; `ValidateEditor.bat` and Windows CI run the milestone gate.

## 8. Lighting Inspector — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 8 implementation commit on `prototype`.
- Delivered: complete Directional/Point/Spot authoring with type-specific controls, world-space direction where applicable, colour, renderer intensity, point/spot range, validated spot inner/outer cones, the existing renderer-wide Scene Shadows pass, hierarchy-lock read-only behavior, and immediate `SceneDocument::SetLight` updates while controls are manipulated.
- Viewport: editor-only light gizmos are integrated into Scene View using the existing editor camera projection path; directional lights show direction arrows, point lights show 3-axis range rings, spot lights show inner/outer cone guides, selected lights receive stronger feedback, and all light visualization obeys the viewport Gizmos toggle and is suppressed during Play.
- Architecture: no parallel light/runtime model was introduced. Authored values remain `LightComponent`/`LightDesc`, scene mutation remains command-backed through `SceneDocument`, rendering continues through `SceneRenderer`, and shadows use the already-owned renderer `RenderSettings::shadows` contract rather than leaking renderer implementation types into editor code.
- Scope boundary: no Material (7), Camera (9), Physics (10), general Debug Visualization (34), or Rendering Quality (30) milestone work is included beyond the light-specific Scene View guides required by Milestone 8.
- Regression: `Scripts/Tests/EditorMilestone8Tests.py`; `ValidateEditor.bat` and Windows Editor CI run the focused milestone gate.

## 6. Model / Mesh Inspector — `DONE`

- Date: 2026-10-30
- Commit evidence: this Milestone 6 implementation commit on `prototype`.
- Delivered: ModelAsset field and cooked-asset browse/drop/replace, state-preserving replacement, visibility, renderer-backed per-model shadow control, mesh/submesh statistics, imported material-slot diagnostics, geometry-derived local bounds, immutable-generation reload/rebind, and locate-in-browser highlighting.
- Architecture: edits remain command-backed through `SceneDocument`; renderer object ownership remains inside `AssetScene`; imported material slots remain read-only until Milestone 7.
- Regression: `Scripts/Tests/EditorMilestone6Tests.py` plus updated model interaction/entity-creation tests; `ValidateEditor.bat` and Windows CI run the milestone gate.

## 7. Material System + Inspector — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 7 implementation commit on `prototype`.
- Delivered: standalone versioned `.pgmaterial` assets; full renderer-neutral PBS values for base colour, base-colour texture, tangent-space normal/normal scale, metallic, roughness, AO, emissive, alpha cutout and double-sided state; per-model imported material-slot overrides; Save/Reload/Clear Override authoring; validation and explicit dirty/error feedback; hierarchy-lock and Play read-only policy; and immediate viewport preview through the existing `AssetScene` renderer path.
- Rendering: material overrides create real Ogre PBS materials through `MaterialDesc`; base-colour and emissive maps are loaded as sRGB colour data while normal, metallic, roughness and AO maps remain linear. Override renderer resources are instance-owned and rebuilt when the immutable material handle changes, while imported model materials remain the fallback for unassigned slots.
- Persistence: scene format v3 stores model shadow state and material override paths, while v1/v2 remain readable. Material assignments clone into the isolated Play world through the existing `ModelComponent` copy path. Standalone material files are validated and atomically replaced on save.
- Architecture: generic `AssetManager` expansion is intentionally deferred to Milestone 31; Milestone 7 uses a dedicated immutable material-asset path and does not expose Ogre objects to `SceneDocument` or Inspector code.
- Regression: `Scripts/Tests/EditorMilestone7MaterialTests.py`, updated scene serialization regression, `ValidateEditor.bat`, and Windows Editor CI cover the PBS surface, material persistence, slot commands, scene v3 compatibility, sRGB/linear mapping, live Inspector workflow and scope boundaries.
- Scope boundary: no Milestone 10 physics-authoring depth, Milestone 13 Content Browser redesign, or Milestone 16 general scene-workflow UI is included; scene-format work is limited to the material-assignment persistence required by this milestone.
- Next assigned milestone: **10. Physics Inspector** — do not begin until explicitly requested.

## 9. Camera Inspector — `DONE`

- Date: 2026-10-31
- Commit evidence: `45bd0fb450b715d51101ef845bcd1fe92cc7df74` on `prototype`.
- Delivered: command-backed FOV, near/far clip and aspect-override editing; active-camera set/clear controls; renderer-neutral selected-camera frustum visualization; camera preview through the selected authored camera; and bidirectional Align Camera to View / Align View to Camera workflows.
- Architecture: camera authoring remains in `CameraComponent`/`CameraDesc` and `SceneDocument`; no Ogre camera implementation object is exposed to editor-facing code. Preview state is restored when selection changes or Play begins.
- Regression: `Scripts/Tests/EditorMilestone9Tests.py` covers projection fields, active-camera controls, frustum overlay, preview, bidirectional alignment, Scene View integration and the renderer-neutral boundary; it is wired into `ValidateEditor.bat` and Windows CI.
- Review: re-audited against the current `prototype` implementation before this documentation correction; no acceptance-scope work remains for Milestone 9.

## 11. Animator Inspector — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 11 implementation commit on `prototype`.
- Delivered: cooked `ModelAsset` animation-source assignment by path or drag/drop with skeleton-layout compatibility checks; fallback to the rendered model's embedded skeleton/clips; named clip selection with duration; Play/Pause/Restart/Stop, loop, speed and time scrubbing; selected-entity-only edit-mode Scene View animation preview; existing `.pgcontroller` assignment/clear; controller state selection and typed Float/Bool/Trigger parameter editing; and clip, skeleton hierarchy and import-warning diagnostics.
- Runtime integration: `Animator` now samples clips from an optional compatible animation-source asset while the rendered model continues to own the target skeleton, mesh geometry, inverse-bind data and skinning output. `AssetScene` rebuilds animated instances when that source changes and can advance only one selected authoring animator during editor preview; normal Play still advances the isolated runtime world as before.
- Architecture: Animator authoring remains command-backed through `SceneDocument::SetAnimator`; M12 remains the owner of graph topology/state-machine asset authoring and M11 only assigns/operates those controller assets. No Ogre/Ozz implementation object is exposed through editor-facing contracts.
- Persistence boundary: scene-format support for Animator components remains Milestone 16 work; M11 does not broaden scene serialization merely to claim persistence.
- Scope boundary: no Animation State Machine graph redesign (12), Thumbnail/Preview System (14), Scene File Workflow (16), or general skeleton Debug Visualization (34) is included.
- Regression: `Scripts/Tests/EditorMilestone11Tests.py`; `ValidateEditor.bat` and Windows Editor CI run the focused milestone gate.

## 12. Animation State Machine Editor — `DONE`

- Date: 2026-10-31
- Commit evidence: `55a424a06c6c380ca122290ac1eb72ea986eba4b` on `prototype`.
- Delivered: persistent `.pgcontroller` controller assets; draggable node states and node positions; entry state; typed Float/Bool/Trigger parameters; transitions and typed conditions; transition/crossfade duration; state/parameter/condition inspectors; runtime active-state presentation; Save/Load/Apply authoring; and compilation into the engine's existing executable `AnimationController` rather than a duplicate editor runtime.
- Architecture: runtime transition evaluation and crossfading continue through the engine animation controller/animator path; Play-mode graph authoring is locked and the editor does not expose renderer implementation objects.
- Regression: `Scripts/Tests/EditorMilestone12Tests.py` covers controller persistence/validation, graph authoring, parameters, transitions/conditions, duration, runtime compilation/crossfade execution, active-state presentation, Play locking and Save/Load/Apply; it is wired into `ValidateEditor.bat` and Windows CI.
- Review: re-audited against the current `prototype` implementation before this documentation correction; no acceptance-scope work remains for Milestone 12.

## 15. Asset Import Workflow — `DONE`

- Date: 2026-10-31
- Commit evidence: `2bf403438f38d6672a8cbccdc622197c4db6d2b3` on `prototype`.
- Delivered: source-to-cooked model and texture import/reimport UI; persisted import receipts retaining source, cooked destination and import settings; configurable scale, coordinate/handedness conversion, UV orientation, normal generation/preservation, tangent/material/animation inclusion, texture vertical flip; standalone PNG/JPEG import as first-class `TextureAsset`; actionable visible/logged import errors; and immutable-generation reload of known cooked assets after reimport.
- Architecture: source processing and cooked serialization remain in the asset layer/`AssetManager`; the editor orchestrates the workflow without Ogre-specific coupling.
- Regression: `Scripts/Tests/EditorMilestone15Tests.py` covers the import options, texture asset path, receipt persistence, import/reimport execution, editor settings surface, error handling and normal editor-frame integration; it is wired into `ValidateEditor.bat` and Windows CI.
- Validation note: the exact Windows run for the original Milestone 15 commit was cancelled by CI concurrency after newer `prototype` pushes, not failed; the implementation remains covered by the branch-level Windows Editor validation gate.
- Review: re-audited against the current `prototype` implementation before this documentation correction; no acceptance-scope work remains for Milestone 15.

## 18. Runtime / Game Logic — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 18 implementation commit on `prototype`.
- Delivered: engine-owned `GameplayRuntime`/`GameplaySystem` lifecycle hooks for Start/Stop/Pause/Resume, routed input, variable Update, physics-scheduled FixedUpdate, trigger/collision callbacks and LateUpdate; isolated gameplay input state with keyboard/mouse capture gates and focus-loss reset; fixed-step gameplay callbacks executed through the existing `PhysicsSystem::Advance` scheduler; deterministic non-sensor collision Enter/Stay/Exit routing by stable physics/entity identity; and explicit Play-frame ordering of gameplay input/update → fixed gameplay/physics → physics events → animation → late gameplay/audio maintenance.
- Pause/Step semantics: a paused frame with no Step performs no gameplay Update/FixedUpdate/LateUpdate and only synchronizes structure; Step consumes exactly one configured fixed interval and advances gameplay, physics, physics events and animation once while remaining paused. Pause/Resume lifecycle callbacks fire only on state transitions.
- Architecture: Play continues to simulate the isolated runtime clone, never the authoring `World`; Jolt callbacks only update an internal thread-safe contact registry and never execute ECS or user gameplay code; collision events are translated back through `PhysicsSystem` after workers finish; no Jolt/EnTT/Ogre implementation object leaks into `GameplaySystem` contracts.
- Regression: `Scripts/Tests/EditorMilestone18Tests.py` covers lifecycle, editor-vs-game input isolation, fixed-step callback ordering, Pause/Step semantics, trigger/collision routing, animation/audio ordering, validation wiring and the Milestone 19 scope boundary. Existing `EditorPlayRuntimeTests.py` was updated for the fixed-step gameplay callback path. Both are wired into `ValidateEditor.bat` and Windows Editor CI.
- Scope boundary: no Console (19), profiler UI (20), toolbar redesign (21), menu work (22), theme/icons, scripting language, audio authoring or later editor milestone work is included.
- Next step: stop here and wait for explicit approval before starting any other milestone.

## Historical architecture note

Earlier revisions of this document contained a much longer backend handoff description. Its important contracts are condensed above and remain binding: owner-thread authoring, persistent IDs, command-backed edits, isolated Play runtime, renderer-neutral editor contracts, generation-safe asynchronous data, and no ImGui dependency in engine hot paths.