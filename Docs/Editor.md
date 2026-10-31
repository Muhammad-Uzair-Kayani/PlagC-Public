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

Assigned milestone lane for the completed implementation stream: **1, 4, 7, 10, 13, 17**.
Current requested execution lane: **19, 22, 25, 28**. Work remains milestone-scoped and each requested milestone is pushed before the next begins.

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

## 10. Physics Inspector — `DONE`

Static/Kinematic/Dynamic body authoring, collider type and dimensions, mass/friction/restitution, layers/masks/triggers, validation, and collider debug visualization.

## 11. Animator Inspector — `DONE`

Skeleton/clip/controller assignment, playback controls, loop/speed, state and parameter editing, animation preview and skeleton diagnostics.

## 12. Animation State Machine Editor — `DONE`

Node-based states, entry state, transitions, conditions, parameters, transition duration/blending, state inspector, runtime active-state visualization and controller asset persistence.

## 13. Content Browser — `DONE`

Replace the registry-table presentation with folders, breadcrumbs, grid/list views, asset cards, search/filter/sort, context menus, drag/drop, double-click behavior, details, source-vs-cooked presentation and sensible filtering of internal files.

## 14. Thumbnail / Preview System — `DONE`

Preview scene/camera/lighting, model/material/texture thumbnails, cache and invalidation, asynchronous generation and non-blocking placeholders.

## 15. Asset Import Workflow — `DONE`

Import/reimport UI for supported model/texture formats, source-to-cooked relationship, scale/coordinate/normal/tangent/material/animation options and surfaced import errors.

## 16. Scene File Workflow — `FOUNDATION`

New/Open/Save/Save As/Recent scenes, dirty indicator, transactional replacement, unsaved prompts, scene-version validation, persistent hierarchy/components/material assignments and useful load/save errors.

## 17. Play Toolbar — `DONE`

Professional Play/Pause/Step/Stop transport, clear runtime visual state, shortcuts, authoring lock policy, runtime camera, physics/animation lifecycle and clean return to the unchanged authoring world.

## 18. Runtime / Game Logic — `DONE`

Define gameplay update/lifecycle hooks, editor-vs-game input routing, physics step callbacks, animation/audio ordering, trigger/collision routing and correct Pause/Step semantics.

## 19. Console — `DONE`

Severity styling/icons, compact entries, expandable details, category/search filtering, repeated-message collapsing, copy/context actions and separation of giant profiler dumps from normal logs.

## 20. Statistics / Profiler — `FOUNDATION`

Frame/FPS graphs, CPU/GPU/scene/physics/animation/asset/UI timings, job utilization, draw calls, triangles, counts, backend/GPU info, history and profiler freeze controls.

## 21. Editor Toolbar — `FOUNDATION`

Save/undo/redo, transform tools, local/world, pivot/center, snap, Play transport, settings and icon-based presentation.

## 22. Menu Bar — `DONE`

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

## Milestone 10 — Physics Inspector — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 10 completion commit on `prototype`.
- Delivered: complete Static/Kinematic/Dynamic body authoring; mass and gravity scale; linear/angular damping; sleep and continuous-CCD controls; primitive collider type/dimensions; friction, restitution and center-of-mass offset; collision layer, per-body mask, trigger and collision-group controls; actionable validation; and editor-only selected/all collider visualization in Scene View.
- Runtime integration: `RigidBodyComponent` now carries renderer-neutral `CollisionLayerMask` state. Jolt registers each body's layer/mask and rejects a candidate contact in `OnContactValidate` unless both bodies' masks accept the other body layer. The existing global `CollisionMatrix` remains the broad, cheap policy; the per-body mask is an additional narrow restriction, with the lookup protected for worker-thread contact validation.
- Architecture: Inspector mutations remain command-backed through `SceneDocument::SetPhysics`; no Jolt/Ogre implementation object is exposed to Editor code. Geometry-backed collider data is retained rather than fabricated, while safe primitive collider authoring remains available. Collider visualization is editor-only and suppressed during Play.
- Validation: layer/body/trigger consistency, unknown mask bits, mass/damping/material ranges, collider dimensions, primitive scale restrictions, geometry presence and static-only mesh/heightfield rules are surfaced before committing invalid authoring state.
- Scope boundary: no Milestone 13 Content Browser, Milestone 16 scene-file persistence/workflow, constraint/character-controller editor tooling, or Milestone 34 general debug-visualization system is included. Scene v3 still explicitly rejects physics persistence, leaving that ownership with Milestone 16.
- Regression: `Scripts/Tests/EditorMilestone10Tests.py`; `ValidateEditor.bat` and Windows Editor CI cover the authoring surface, command-backed edits, runtime mask wiring, validation, editor-only collider visualization and scope boundaries.
- Next assigned milestone: **13. Content Browser** — do not begin until explicitly requested.

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

## 13. Content Browser — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 13 implementation commit on `prototype`.
- Delivered: hierarchical folders with Up and clickable breadcrumb navigation; grid/list asset-card views; subtree search; Source/Cooked/Project origin filtering; model/texture/audio/material/scene/controller/shader/cooked/other type filtering; name/type/size sorting; explicit internal-file visibility control with manifests, shaders, compositor/support/import-receipt files hidden by default; selected-asset details with runtime type/state/generation/errors; source↔cooked relationships from the existing import receipts; context actions for select/copy/reveal/reload/refresh; canonical filesystem/registry path deduplication; and preserved cooked-model double-click creation plus the existing Inspector/Viewport drag payload.
- Architecture: the browser consumes the existing `AssetBrowserModel` snapshot, `AssetManager` registry and Milestone 15 import receipts instead of duplicating asset/import systems. Model Inspector reveal, Inspector assignment, viewport placement and Milestone 3 model creation continue through their existing editor/document contracts.
- Scope boundary: asset cards remain textual placeholders; no thumbnail/preview cache or generation (14), no new import/reimport processing (15), no scene open/save workflow (16), and no Play Toolbar work (17) is included.
- Regression: `Scripts/Tests/EditorMilestone13ContentBrowserTests.py` plus updated `EditorAssetInteractionTests.py` and Milestone 6 reveal coverage; `ValidateEditor.bat` and Windows CI run the focused gate.
- Next step: stop here and wait for explicit approval before starting Milestone 17.

## 14. Thumbnail / Preview System — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 14 implementation commit on `prototype`.
- Delivered: renderer-neutral 128×128 previews for models, materials and textures; a fixed studio preview camera plus three-light rig; model bounds fitting and CPU triangle rasterization; material-sphere previews using authored colour/roughness/metallic/emissive values; aspect-correct texture previews over an alpha checkerboard; selected-asset preview presentation and a non-blocking thumbnail cache strip in Content Browser; queued/generating/error placeholders; explicit regenerate-one/all controls; and clean cache shutdown.
- Async/cache design: PlagC `JobSystem` workers load/import immutable source data and generate owned RGBA buffers only. At most two thumbnail jobs run concurrently; the cache is bounded to 256 entries. File timestamp/size plus asset generation form the invalidation fingerprint, request serials reject stale results, and worker completion is observed through `JobGroup::pending` before the task storage is retired. The main/editor thread alone creates/destroys UI textures.
- Backend isolation: no Ogre/OpenGL/Vulkan object enters the thumbnail generator. Completed pixels use the existing renderer-neutral `CreateUiTexture` path, so the same preview cache feeds either renderer backend without a second Ogre root/scene or renderer-affine worker work.
- Scope boundary: no source import/reimport workflow changes (15), scene serialization/workflow (16), Play Toolbar work (17), or general editor/game debug visualization (29/34) is included.
- Regression: `Scripts/Tests/EditorMilestone14ThumbnailTests.py` plus the retained Milestone 13 browser regressions; `ValidateEditor.bat` and Windows Editor CI run the focused M14 gate.

## 15. Asset Import Workflow — `DONE`

- Date: 2026-10-31
- Commit evidence: `2bf403438f38d6672a8cbccdc622197c4db6d2b3` on `prototype`.
- Delivered: source-to-cooked model and texture import/reimport UI; persisted import receipts retaining source, cooked destination and import settings; configurable scale, coordinate/handedness conversion, UV orientation, normal generation/preservation, tangent/material/animation inclusion, texture vertical flip; standalone PNG/JPEG import as first-class `TextureAsset`; actionable visible/logged import errors; and immutable-generation reload of known cooked assets after reimport.
- Architecture: source processing and cooked serialization remain in the asset layer/`AssetManager`; the editor orchestrates the workflow without Ogre-specific coupling.
- Regression: `Scripts/Tests/EditorMilestone15Tests.py` covers the import options, texture asset path, receipt persistence, import/reimport execution, editor settings surface, error handling and normal editor-frame integration; it is wired into `ValidateEditor.bat` and Windows CI.
- Validation note: the exact Windows run for the original Milestone 15 commit was cancelled by CI concurrency after newer `prototype` pushes, not failed; the implementation remains covered by the branch-level Windows Editor validation gate.
- Review: re-audited against the current `prototype` implementation before this documentation correction; no acceptance-scope work remains for Milestone 15.

## 17. Play Toolbar — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 17 completion commit on `prototype`.
- Delivered: an always-visible centered transport with explicit `EDIT`, `PLAYING`, and `PAUSED` state; stable Play/Pause-or-Resume/Step/Stop controls with context-sensitive enabled states; and transport shortcuts `F5` Play/Stop, `F6` Pause/Resume, and `F7` single-step while paused.
- Shortcut routing: transport shortcuts are suppressed while typing, while an ImGui item is active, while modal/popup UI is open, or while Ctrl/Alt/Super modifiers are held, preventing editor transport commands from leaking through text/modal workflows.
- Runtime presentation: Scene View receives a play-state border and compact `PLAYING`/`PAUSED` banner that explicitly identifies authoring as locked and shows the applicable transport shortcuts. The runtime state is therefore visible even when focus is in the viewport rather than the menu bar.
- Transition semantics: entering Play commits any active Transform Inspector or viewport-gizmo transaction before cloning the runtime scene. `PlaySession` continues to clone the authoring document, authoring mutation remains rejected through `EditorSession::RequireStopped`, and Stop destroys the runtime clone so the unchanged authoring document becomes visible again.
- Runtime integration: Milestone 17 reuses the existing `EditorPlayRuntime`/Milestone 18 lifecycle rather than creating a second simulation path. Runtime camera selection/fallback, physics fixed stepping, gameplay callbacks, animation update ordering, Pause/Step semantics and audio maintenance remain owned by their existing systems.
- Scope boundary: no Console (19), Statistics/Profiler (20), general Editor Toolbar redesign (21), Menu Bar expansion (22), icon/font system (24), or broad Keyboard Shortcuts milestone (35) work is included beyond the transport-specific shortcuts required here.
- Regression: `Scripts/Tests/EditorMilestone17PlayToolbarTests.py` plus the existing `EditorPlayRuntimeTests.py`; `ValidateEditor.bat` and Windows Editor CI run the focused gate.
- Next step: stop here and wait for explicit approval before starting any other milestone.

## 18. Runtime / Game Logic — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 18 implementation commit on `prototype`.
- Delivered: engine-owned `GameplayRuntime`/`GameplaySystem` lifecycle hooks for Start/Stop/Pause/Resume, routed input, variable Update, physics-scheduled FixedUpdate, trigger/collision callbacks and LateUpdate; isolated gameplay input state with keyboard/mouse capture gates and focus-loss reset; fixed-step gameplay callbacks executed through the existing `PhysicsSystem::Advance` scheduler; deterministic non-sensor collision Enter/Stay/Exit routing by stable physics/entity identity; and explicit Play-frame ordering of gameplay input/update → fixed gameplay/physics → physics events → animation → late gameplay/audio maintenance.
- Pause/Step semantics: a paused frame with no Step performs no gameplay Update/FixedUpdate/LateUpdate and only synchronizes structure; Step consumes exactly one configured fixed interval and advances gameplay, physics, physics events and animation once while remaining paused. Pause/Resume lifecycle callbacks fire only on state transitions.
- Architecture: Play continues to simulate the isolated runtime clone, never the authoring `World`; Jolt callbacks only update an internal thread-safe contact registry and never execute ECS or user gameplay code; collision events are translated back through `PhysicsSystem` after workers finish; no Jolt/EnTT/Ogre implementation object leaks into `GameplaySystem` contracts.
- Regression: `Scripts/Tests/EditorMilestone18Tests.py` covers lifecycle, editor-vs-game input isolation, fixed-step callback ordering, Pause/Step semantics, trigger/collision routing, animation/audio ordering, validation wiring and the Milestone 19 scope boundary. Existing `EditorPlayRuntimeTests.py` was updated for the fixed-step gameplay callback path. Both are wired into `ValidateEditor.bat` and Windows Editor CI.
- Scope boundary: no Console (19), profiler UI (20), toolbar redesign (21), menu work (22), theme/icons, scripting language, audio authoring or later editor milestone work is included.
- Next step: stop here and wait for explicit approval before starting any other milestone.

## 19. Console — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 19 implementation commit on `prototype`.
- Delivered: severity-specific compact badges and styling; compact single-line messages with click-to-expand full details; timestamp/category/thread/sequence diagnostics; dynamic category selection plus existing search/severity filtering; consecutive repeated-message collapsing with repeat counts and first/last timing; per-entry context actions for copying message/details/category; clear and auto-scroll controls; and a dedicated Profiler tab that keeps giant `PROFILER` reports out of the normal diagnostic stream.
- Architecture: the Console continues to consume immutable `Log::Snapshot()` records and the existing backend `FilterLogRecords` contract. Milestone 19 adds presentation/grouping only; it does not mutate engine log records, introduce a second logger, or absorb Statistics/Profiler ownership from Milestone 20.
- Scope boundary: no Statistics/Profiler graphs (20), Editor Toolbar redesign (21), Menu Bar expansion (22), theme/icon system (23/24), or notification/toast framework (39) is included.
- Regression: `Scripts/Tests/EditorMilestone19ConsoleTests.py`; existing M17/M18 roadmap assertions were relaxed so completed downstream work no longer invalidates their historical scope tests. `ValidateEditor.bat` and Windows Editor CI run the focused M19 gate.

## 22. Menu Bar — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 22 implementation series on `prototype`.
- Delivered: the full File/Edit/Entity/Assets/View/Window/Debug/Help menu surface; existing context-safe file/edit shortcuts; authoring-aware entity create/duplicate/delete/clear-selection actions; asset-browser refresh and import/state-machine access; centralized panel visibility under Window; viewport/physics editor diagnostics under Debug; and editor shortcut/documentation discovery under Help.
- Composition: File remains owned by `EditorFileController`; Edit and the existing View compatibility surface remain in `EditorLayer`; `EditorMenuController` composes the missing menu groups and reuses `SceneDocument`, hierarchy locks, asset-panel state and viewport/debug state instead of introducing parallel command systems. The legacy Scene menu is retained as a compatibility alias while Entity is the authoritative M22 authoring menu.
- Context policy: destructive authoring commands are disabled during Play, during transient document edits, with no selection, or when the hierarchy entity is locked as applicable. Import authoring is Play-gated while pure presentation/window/debug actions remain available.
- Scope boundary: no workspace persistence/reset behavior (25), new icon/font system (24), broad shortcut redesign (35), or drag/drop redesign (28) is included.
- Regression: `Scripts/Tests/EditorMilestone22MenuBarTests.py`; M19's historical downstream-status assertion was relaxed, and both local and Windows validation gates include M22.

## Historical architecture note

Earlier revisions of this document contained a much longer backend handoff description. Its important contracts are condensed above and remain binding: owner-thread authoring, persistent IDs, command-backed edits, isolated Play runtime, renderer-neutral editor contracts, generation-safe asynchronous data, and no ImGui dependency in engine hot paths.
