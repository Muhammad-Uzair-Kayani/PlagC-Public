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

## 1. Viewport / Scene View — `NEXT`

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

Required tests: source architecture regression for viewport state/UI wiring, camera projection/picking regression, Windows Editor build validation, native runtime acceptance on the available backend.

## 2. Scene Hierarchy — `DONE`

Replace the functional tree/debug presentation with a real hierarchy workflow: search/filter, type icons, expand/collapse, inline rename, context menu, visibility/lock controls, drag reparent/root drop, selection synchronization, stable expansion state and useful empty-scene presentation.

## 3. Entity Creation — `DONE`

Provide Create workflows for Empty, Model, Camera, Directional/Point/Spot Light, physics objects and basic primitives. New entities receive sensible required components and model drops create at a meaningful scene position.

## 4. Inspector — `FOUNDATION`

Turn the raw component/debug view into component cards with aligned property rows, add/remove/reset actions, validation, tooltips, read-only states and undo/redo-backed editing.

## 5. Transform Inspector — `FOUNDATION`

Professional X/Y/Z position/rotation/scale controls, color-coded axes, reset actions, uniform scaling option, local/world awareness, immediate viewport feedback, and one-gesture/one-undo behavior.

## 6. Model / Mesh Inspector — `DONE`

Model asset field, browse/drop/replace, visibility, shadow controls, mesh/submesh information, material slots, bounds, reload and locate-in-browser actions.

## 7. Material System + Inspector — `NOT STARTED`

Authorable material assets and Ogre PBS mapping for base color, base-color texture, normal, metallic, roughness, AO, emissive, alpha/cutout, double-sided state, material slots, save/load and immediate viewport preview with correct color-space handling.

## 8. Lighting Inspector — `FOUNDATION`

Directional/Point/Spot editing, color, intensity, range, cones, shadows, and viewport light visualization with immediate scene updates.

## 9. Camera Inspector — `FOUNDATION`

FOV/clip/aspect editing, active-camera controls, frustum visualization, camera preview, align-camera-to-view and align-view-to-camera actions.

## 10. Physics Inspector — `FOUNDATION`

Static/Kinematic/Dynamic body authoring, collider type and dimensions, mass/friction/restitution, layers/masks/triggers, validation, and collider debug visualization.

## 11. Animator Inspector — `FOUNDATION`

Skeleton/clip/controller assignment, playback controls, loop/speed, state and parameter editing, animation preview and skeleton diagnostics.

## 12. Animation State Machine Editor — `NOT STARTED`

Node-based states, entry state, transitions, conditions, parameters, transition duration/blending, state inspector, runtime active-state visualization and controller asset persistence.

## 13. Content Browser — `FOUNDATION`

Replace the registry-table presentation with folders, breadcrumbs, grid/list views, asset cards, search/filter/sort, context menus, drag/drop, double-click behavior, details, source-vs-cooked presentation and sensible filtering of internal files.

## 14. Thumbnail / Preview System — `NOT STARTED`

Preview scene/camera/lighting, model/material/texture thumbnails, cache and invalidation, asynchronous generation and non-blocking placeholders.

## 15. Asset Import Workflow — `FOUNDATION`

Import/reimport UI for supported model/texture formats, source-to-cooked relationship, scale/coordinate/normal/tangent/material/animation options and surfaced import errors.

## 16. Scene File Workflow — `FOUNDATION`

New/Open/Save/Save As/Recent scenes, dirty indicator, transactional replacement, unsaved prompts, scene-version validation, persistent hierarchy/components/material assignments and useful load/save errors.

## 17. Play Toolbar — `FOUNDATION`

Professional Play/Pause/Step/Stop transport, clear runtime visual state, shortcuts, authoring lock policy, runtime camera, physics/animation lifecycle and clean return to the unchanged authoring world.

## 18. Runtime / Game Logic — `FOUNDATION`

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

## 6. Model / Mesh Inspector — `DONE`

- Date: 2026-10-30
- Commit evidence: this Milestone 6 implementation commit on `prototype`.
- Delivered: ModelAsset field and cooked-asset browse/drop/replace, state-preserving replacement, visibility, renderer-backed per-model shadow control, mesh/submesh statistics, imported material-slot diagnostics, geometry-derived local bounds, immutable-generation reload/rebind, and locate-in-browser highlighting.
- Architecture: edits remain command-backed through `SceneDocument`; renderer object ownership remains inside `AssetScene`; imported material slots remain read-only until Milestone 7.
- Regression: `Scripts/Tests/EditorMilestone6Tests.py` plus updated model interaction/entity-creation tests; `ValidateEditor.bat` and Windows CI run the milestone gate.
- Next requested milestone: **9. Camera Inspector**.

## Historical architecture note

Earlier revisions of this document contained a much longer backend handoff description. Its important contracts are condensed above and remain binding: owner-thread authoring, persistent IDs, command-backed edits, isolated Play runtime, renderer-neutral editor contracts, generation-safe asynchronous data, and no ImGui dependency in engine hot paths.
