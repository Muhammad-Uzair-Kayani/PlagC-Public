# PlagC Editor — architecture, usable-authoring roadmap, and execution log

## Purpose

This document is the source of truth for PlagC Editor work on `prototype`.
Every editor milestone is designed, implemented, tested, pushed, and then recorded here before work moves to the next milestone.

The next product target is deliberately concrete:

> Build a small illuminated 3D scene entirely inside PlagC Editor, save it, reopen it, press Play, and test it without leaving the editor.

A milestone is not complete because a backend API exists. It is complete when the authoring workflow exposed to the user is usable and regression-protected.

## Execution rules

1. `prototype` is the source of truth. Do not restart completed work on parallel `fix/*` branches.
2. Work on one numbered milestone at a time unless a prerequisite is strictly required by that milestone or the user explicitly assigns a batch.
3. Before implementation, write/confirm the design and acceptance criteria.
4. Implement against existing engine/editor contracts instead of duplicating systems in the UI.
5. Add focused regression coverage for every milestone.
6. Run source regressions, project generation, and the Windows Editor build gate where applicable.
7. Push the milestone to `prototype`.
8. Update this document in the same implementation stream with status, tests, files, and commit evidence.
9. Stop after the explicitly assigned milestone or batch and wait for approval before starting unrelated work.

Assigned milestone lane for the completed implementation stream: **1, 4, 7, 10, 13, 17**.
Current requested execution lane: **16, 20, 23, 26, 29**. Each milestone is completed and pushed independently before the next begins; concurrent completed work on other milestones is preserved.

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

## 16. Scene File Workflow — `DONE`

New/Open/Save/Save As/Recent scenes, dirty indicator, transactional replacement, unsaved prompts, scene-version validation, persistent hierarchy/components/material assignments and useful load/save errors.

## 17. Play Toolbar — `DONE`

Professional Play/Pause/Step/Stop transport, clear runtime visual state, shortcuts, authoring lock policy, runtime camera, physics/animation lifecycle and clean return to the unchanged authoring world.

## 18. Runtime / Game Logic — `DONE`

Define gameplay update/lifecycle hooks, editor-vs-game input routing, physics step callbacks, animation/audio ordering, trigger/collision routing and correct Pause/Step semantics.

## 19. Console — `DONE`

Severity styling/icons, compact entries, expandable details, category/search filtering, repeated-message collapsing, copy/context actions and separation of giant profiler dumps from normal logs.

## 20. Statistics / Profiler — `DONE`

Frame/FPS graphs, CPU/GPU/scene/physics/animation/asset/UI timings, job utilization, draw calls, triangles, counts, backend/GPU info, history and profiler freeze controls.

## 21. Editor Toolbar — `DONE`

Save/undo/redo, transform tools, local/world, pivot/center, snap, Play transport, settings and icon-based presentation.

## 22. Menu Bar — `DONE`

Complete File/Edit/Entity/Assets/View/Window/Debug/Help menus, shortcuts and context-sensitive enable/disable behavior.

## 23. Editor Theme / Visual System — `DONE`

A deliberate PlagC visual language: typography, spacing, component cards, headers, selection/hover/disabled colors, padding, borders, toolbar/dock styling and strong contrast instead of raw ImGui defaults.

## 24. Icons / Fonts — `DONE`

Consistent embedded icon/font assets for entities, folders, assets, cameras, lights, physics, transport and transform tools without shipping raw font source files.

## 25. Docking / Workspace — `DONE`

Strong default layout, save/restore/reset workspace, sane panel locations and support for useful detached/docked workflows.

## 26. Selection System — `FOUNDATION`

Hierarchy↔viewport synchronization, independent asset selection, Inspector tracking, safe deletion behavior and later Ctrl/Shift multi-selection.

## 27. Undo / Redo — `DONE`

Cover transform, rename, create/delete/duplicate, reparent, component add/remove/edit, model/material/light/camera/physics changes with correct command labels and transaction coalescing.

## 28. Drag & Drop — `DONE`

Asset→viewport, asset→Inspector, hierarchy reparenting, material→slot, animation→animator, validation and visible drop feedback.

## 29. Editor Preview Rendering — `NOT STARTED`

Explicit editor-vs-game visualization policy: preview lighting, grid, icons, selection, colliders, camera/light gizmos and debug views never leak into game output.

## 30. Rendering Quality — `DONE`

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
- Policy: every future editor milestone updates this file in the same implementation stream.

## 1. Viewport / Scene View — `DONE`

- Date: 2026-10-31
- Delivered: renderer-backed selection and empty-space deselection; adaptive world grid/axes; selection feedback; viewport XYZ orientation indicator; Translate/Rotate/Scale toolbar; local/world transform space; movement/rotation/scale snapping; perspective and real Ogre orthographic projection; orthographic-aware picking and asset-drop rays; six axis view presets; focus/frame controls; adjustable camera speed; grid/gizmo/wireframe toggles; exposure control; and editor-only preview lighting/environment.
- Isolation: preview lighting/environment is suppressed in Play; runtime projection remains independent.
- Regression: `Scripts/Tests/EditorViewportSceneViewTests.py` plus picking/gizmo/play validation.

## 2. Scene Hierarchy — `DONE`

- Date: 2026-10-30
- Delivered: name/type search, component-type presentation, expand/collapse, inline rename, context actions, visibility/lock controls, drag reparent/root-drop, selection synchronization and empty-scene actions.
- Architecture: scene mutation remains in `SceneDocument`; hierarchy UI state remains editor-only.
- Regression: `Scripts/Tests/EditorSceneHierarchyMilestoneTests.py` plus backend hierarchy tests.

## 3. Entity Creation — `DONE`

- Date: 2026-10-30
- Commit evidence: `75eab676cda60ce6d5393bfd91a6e06d7c85a359`.
- Delivered: command-backed Empty, model, camera, lights, primitives and physics presets.
- Regression: `Scripts/Tests/EditorMilestone3Tests.py` and asset interaction tests.

## 4. Inspector — `DONE`

- Date: 2026-10-31
- Delivered: coherent entity header; component cards; add/remove/reset actions; validation/tooltips; hierarchy-lock protection; read-only runtime presentation; command-backed mutation.
- Regression: `Scripts/Tests/EditorMilestone4InspectorTests.py`.

## 5. Transform Inspector — `DONE`

- Date: 2026-10-31
- Delivered: X/Y/Z controls, local/world editing, uniform scaling, reset actions, immediate previews, lock/physics ownership handling and one-gesture/one-undo transactions.
- Regression: `Scripts/Tests/EditorMilestone5Tests.py` plus document transaction coverage.

## 6. Model / Mesh Inspector — `DONE`

- Date: 2026-10-30
- Delivered: model asset browse/drop/replace, visibility/shadows, mesh statistics, material diagnostics, bounds, reload and locate.
- Regression: `Scripts/Tests/EditorMilestone6Tests.py`.

## 7. Material System + Inspector — `DONE`

- Date: 2026-10-31
- Delivered: versioned `.pgmaterial`, renderer-neutral PBS values, per-slot overrides, persistence, validation, live preview and correct sRGB/linear texture mapping.
- Architecture: material authoring remains renderer-neutral; Ogre resources remain inside rendering/asset presentation.
- Regression: `Scripts/Tests/EditorMilestone7MaterialTests.py`.

## 8. Lighting Inspector — `DONE`

- Date: 2026-10-31
- Delivered: Directional/Point/Spot authoring, colour/intensity/range/cones/shadows and editor-only light visualization.
- Regression: `Scripts/Tests/EditorMilestone8Tests.py`.

## 9. Camera Inspector — `DONE`

- Date: 2026-10-31
- Commit evidence: `45bd0fb450b715d51101ef845bcd1fe92cc7df74`.
- Delivered: FOV/clips/aspect, active camera, frustum, preview and align-camera/view workflows.
- Regression: `Scripts/Tests/EditorMilestone9Tests.py`.

## 10. Physics Inspector — `DONE`

- Date: 2026-10-31
- Delivered: body/collider authoring, physical properties, layers/masks/triggers, validation and editor-only collider visualization.
- Runtime integration: per-body masks participate in Jolt contact validation without leaking Jolt objects into editor contracts.
- Regression: `Scripts/Tests/EditorMilestone10Tests.py`.

## 11. Animator Inspector — `DONE`

- Date: 2026-10-31
- Delivered: animation source/clip/controller assignment, playback controls, loop/speed/scrub, preview, parameter editing and skeleton diagnostics.
- Regression: `Scripts/Tests/EditorMilestone11Tests.py`.

## 12. Animation State Machine Editor — `DONE`

- Date: 2026-10-31
- Commit evidence: `55a424a06c6c380ca122290ac1eb72ea986eba4b`.
- Delivered: persistent controller graph, states, entry state, parameters, transitions/conditions, blend duration, runtime compilation and active-state presentation.
- Regression: `Scripts/Tests/EditorMilestone12Tests.py`.

## 13. Content Browser — `DONE`

- Date: 2026-10-31
- Delivered: hierarchy/breadcrumb navigation, grid/list cards, search/filter/sort, source/cooked relationship, details, context actions, internal-file filtering and deduplication.
- Regression: `Scripts/Tests/EditorMilestone13ContentBrowserTests.py` plus retained interaction tests.

## 14. Thumbnail / Preview System — `DONE`

- Date: 2026-10-31
- Delivered: renderer-neutral model/material/texture previews, fixed studio setup, async bounded cache, generation rejection, placeholders, invalidation and main-thread UI texture upload.
- Regression: `Scripts/Tests/EditorMilestone14ThumbnailTests.py`.

## 15. Asset Import Workflow — `DONE`

- Date: 2026-10-31
- Commit evidence: `2bf403438f38d6672a8cbccdc622197c4db6d2b3`.
- Delivered: model/texture import/reimport, receipts, scale/coordinate/normal/tangent/material/animation/UV settings, texture import and surfaced errors.
- Regression: `Scripts/Tests/EditorMilestone15Tests.py`.

## Milestone 16 — Scene File Workflow — `DONE`

- Date: 2026-10-31
- Commit evidence: `43e5caa6d58005db25549baeaef8c8e893d60db5`.
- Delivered: New/Open/Save/Save As/Open Recent; persistent recent-scene history; scene identity and dirty indicator; Save/Discard/Cancel handling for replacement/exit; untitled Save As during both normal save and window-close flow; actionable file errors; and transactional document replacement that leaves the current authoring scene intact when loading fails.
- Persistence: scene format v4 keeps v1/v2/v3 readable and persists hierarchy, transforms, active camera, cameras, lights, model references, shadow/visibility state, material overrides, physics bodies/colliders including geometry-backed collider data, animator source/playback/controller parameters/state and upper-body state. Loaded persistent IDs advance the allocator before new authoring entities are created.
- Safety: raw renderer `RenderableComponent` handles remain explicitly non-serializable rather than being silently discarded; scene saves retain atomic replacement semantics and only mark history saved after the replacement succeeds.
- Architecture: the M22 menu and M21 toolbar continue to reuse `EditorFileController`; no parallel save/open subsystem was introduced. Scene persistence remains renderer/Jolt neutral at the editor contract boundary.
- Regression: `Scripts/Tests/EditorMilestone16SceneFileTests.py`, expanded `EditorSerializationTests.cpp`, and compatibility updates for prior scene-version assertions; local and Windows validation include the M16 gate.
- Scope boundary: no M20 profiler, M23 theme, M26 selection or M29 preview-rendering implementation is included.

## 17. Play Toolbar — `DONE`

- Date: 2026-10-31
- Commit evidence: `5e6aea8446a7c6121a16f2c803fc21068d124a88`.
- Delivered: state-driven Play/Pause/Resume/Step/Stop transport, F5/F6/F7 routing, runtime state presentation, authoring lock, committed pre-Play edit transactions and clean return to authoring.
- Runtime integration: existing `PlaySession`/`EditorPlayRuntime` owns camera, physics, animation/gameplay lifecycle and isolated runtime state.
- Regression: `Scripts/Tests/EditorMilestone17PlayToolbarTests.py` plus runtime tests.

## 18. Runtime / Game Logic — `DONE`

- Date: 2026-10-31
- Delivered: gameplay lifecycle, isolated input routing, variable/fixed/late update hooks, contact event routing, pause/step semantics and animation/audio ordering over the isolated runtime world.
- Regression: `Scripts/Tests/EditorMilestone18Tests.py` and `EditorPlayRuntimeTests.py`.

## 19. Console — `DONE`

- Date: 2026-10-31
- Delivered: severity styling, compact/expandable messages, category/search filters, repeated-message collapsing, copy/context actions and separate profiler-report presentation.
- Architecture: consumes immutable `Log::Snapshot()` data and does not replace the logger or M20 profiler ownership.
- Regression: `Scripts/Tests/EditorMilestone19ConsoleTests.py`.

## 20. Statistics / Profiler — `DONE`

- Date: 2026-10-31
- Commit evidence: `46cf8a5f6f226d568ed23b2e74545323112cb3fd`.
- Delivered: bounded 120-sample history with live Frame/FPS/application-CPU/GPU/UI-submit/job-utilization graphs; Freeze/Resume and Clear History controls; frame P95/P99/max and hitch counters; and a complete CPU timing breakdown for platform/events, asset pump, application, physics, animation, scene sync, audio, renderer submission/pacing and unaccounted time.
- GPU/rendering: exposes active backend, asynchronous GPU timing state/total/P99/max/query latency/sample count, draw calls, triangles, instances, objects, meshes, materials, lights and textures using existing renderer-neutral `FrameProfile`/`RenderStats` data.
- Jobs: `JobSystem` now maintains a lock-free cumulative worker busy-time counter; the Editor derives real interval utilization from busy-time deltas, elapsed wall time and worker count rather than guessing utilization from submitted/executed counts.
- UI timing: `ImGuiRendererBridge` records renderer-neutral UI submission cost without leaking Ogre/OpenGL/Vulkan objects into editor code.
- Composition: the profiler appends to the existing `Statistics` window and preserves M22 Window-menu/M25 workspace ownership; engine profiler, renderer and job system remain owners of runtime metrics.
- Regression: `Scripts/Tests/EditorMilestone20StatisticsTests.py`; local and Windows validation include the M20 gate.
- Scope boundary: no M23 theme, M26 selection or M29 preview-rendering implementation is included.

## 21. Editor Toolbar — `DONE`

- Date: 2026-10-31
- Delivered: a global toolbar between the menu bar and dockspace with Save/dirty state, command-aware Undo/Redo, Translate/Rotate/Scale, World/Local, Pivot/Center, Snap, the existing M17 transport and access to persistent Editor settings.
- Composition: the toolbar reuses `EditorFileController`, `EditorSession`/`SceneDocument`, shared `ViewportState`, M17 transport actions and `EditorPreferences`; it does not duplicate any authoring/runtime subsystem.
- Presentation: M24 supplies the final vector-icon presentation without changing M21 action semantics.
- Scope boundary: no M23 theme redesign, M22 menu ownership or broad M35 shortcut redesign is included.
- Regression: `Scripts/Tests/EditorMilestone21ToolbarTests.py`; local and Windows validation run the focused gate.

## 22. Menu Bar — `DONE`

- Date: 2026-10-31
- Delivered: File/Edit/Entity/Assets/View/Window/Debug/Help menus, context-sensitive authoring actions and existing shortcut integration.
- Composition: existing controllers remain owners; `EditorMenuController` composes the menu surface without parallel mutation systems.
- Regression: `Scripts/Tests/EditorMilestone22MenuBarTests.py`.

## 23. Editor Theme / Visual System — `DONE`

- Date: 2026-10-31
- Commit evidence: this Milestone 23 completion commit on `prototype`.
- Delivered: a deliberate PlagC dark visual language with high-contrast text, muted disabled text, blue interaction accent, elevated panel/control surfaces, explicit selection/hover/active states, consistent borders and modal dimming.
- Layout rhythm: centralized window/frame/cell padding, item spacing, indentation, scrollbar/grab sizing, rounding and border dimensions replace raw Dear ImGui defaults across the editor.
- Component/tooling integration: existing Inspector component cards inherit dedicated header/hover/active styling; the global toolbar inherits the themed child/button surfaces; tabs, tables, plots, drag/drop targets and docking previews share the same visual vocabulary without duplicating panel code.
- Typography/icons: M23 uses M24's embedded `EditorVisualAssets::TextFont()` as the canonical text face and owns only scale/spacing/color hierarchy; it does not ship or load a second raw font.
- Architecture: `EditorTheme` is frontend-only and renderer-neutral. It is applied once after ImGui/font initialization and before the first editor frame. No authoring/runtime state is touched.
- Regression: `Scripts/Tests/EditorMilestone23ThemeTests.py`; local and Windows validation include the M23 gate.
- Scope boundary: no M26 selection, M29 preview-rendering, or M36 settings/theme-selector implementation is included.

## 24. Icons / Fonts — `DONE`

- Date: 2026-10-31
- Delivered: a renderer-neutral `EditorVisualAssets` catalog covering entities, folders/assets, model/texture/material, camera/light/physics, Save/Undo/Redo, transform-space/pivot/snap, Play transport and settings; reusable vector-drawn `IconButton`/inline icon primitives; and accessible icon names for non-text surfaces.
- Fonts: editor text uses Dear ImGui's built-in embedded default font through `ImGui::GetFont()`; PlagC ships no raw TTF/OTF source font as part of this milestone.
- Rendering boundary: icons are ImGui draw-list primitives only; no Ogre/OpenGL/Vulkan resource is exposed to the icon system. The M21 toolbar uses this catalog for its authoring and transport presentation.
- Scope boundary: M23 now owns the completed theme/spacing/color system; M24 remains icon/font-asset ownership only.
- Regression: `Scripts/Tests/EditorMilestone24IconFontTests.py`; local and Windows validation run the focused gate.

## 25. Docking / Workspace — `DONE`

- Date: 2026-10-31
- Delivered: strong default layout, Save/Restore/Reset Workspace actions, startup restore, ImGui docking/window persistence, PlagC panel visibility persistence and local `User/Editor/` storage.
- Ownership: workspace state is presentation-only; Dear ImGui owns docking geometry while `EditorWorkspace` owns lifecycle and reset orchestration.
- Regression: `Scripts/Tests/EditorMilestone25WorkspaceTests.py`.

## 27. Undo / Redo — `DONE`

- Date: 2026-10-31
- Delivered: verified command-backed create/delete/duplicate/rename/reparent, camera/light/model/physics/material and component changes; action-aware `Add`/`Edit`/`Remove` labels for renderer/physics/animator component commands; and command labels surfaced through both menu and toolbar history controls.
- Transaction semantics: transform Inspector and viewport-gizmo gestures continue to preview transiently and commit exactly one `TransformCommand` through `RecordApplied`; history branching, bounded capacity, saved/dirty cursor state and re-entry protection remain canonical in `CommandHistory`.
- Runtime isolation: `EditorSession` rejects authoring history mutation while Play is active; no second history layer was introduced.
- Regression: `Scripts/Tests/EditorMilestone27UndoRedoTests.py` plus existing document/transform regressions; local and Windows validation run the focused gate.

## 28. Drag & Drop — `DONE`

- Date: 2026-10-31
- Delivered: cooked-model asset→Viewport and asset→Inspector placement/replacement; hierarchy reparent/root-drop; `.pgmaterial`→slot assignment; animation-source→Animator; validation and visible accept/reject feedback through a shared frontend payload contract.
- Architecture: payloads are frontend identifiers only; all mutations remain in `SceneDocument` and asset resolution remains in asset/material systems.
- Regression: `Scripts/Tests/EditorMilestone28DragDropTests.py` plus existing asset/hierarchy/material/Animator tests.

## 30. Rendering Quality — `DONE`

- Date: 2026-10-31
- Delivered: a renderer-neutral `RenderQuality` audit contract plus a live toolbar quality surface for HDR/tone mapping, bloom, scene shadows, exposure, bloom strength/threshold, environment state, logical/framebuffer dimensions, DPI scale, MSAA reporting and validation notices/errors.
- PBS validation: the existing material path is regression-protected for base colour, normal, metallic, roughness, AO/occlusion and emissive inputs; base-colour/emissive maps are treated as sRGB while normal/metallic/roughness/AO maps remain linear.
- HDR/environment validation: the Ogre path uses an RGBA16F scene target, bloom passes, tone mapping/exposure, shadow-node gating, skybox/environment handling and PBS reflection binding. The same renderer/editor quality contracts cover Ogre OpenGL and the pinned Vulkan build path.
- Resize/UI validation: Editor viewport targets are sized in framebuffer pixels using `DisplayFramebufferScale`; ImGui submission also consumes framebuffer scaling and submits through the renderer-neutral UI path.
- MSAA boundary: the current Editor target is deliberately reported as a validated **1× baseline**. The audit recognizes canonical 1/2/4/8 sample counts but does not claim unsupported multisampling; adding a selectable multisampled target remains a renderer capability expansion rather than a false M30 claim.
- Scope boundary: no M29 editor-preview policy or M34 general debug visualization is included; M23 now owns the completed visual theme.
- Regression: `Scripts/Tests/EditorMilestone30RenderingQualityTests.py`; local and Windows validation run the focused gate.

## Historical architecture note

Earlier revisions contained longer backend handoff notes. Their binding contracts remain represented above: owner-thread authoring, persistent IDs, command-backed edits, isolated Play runtime, renderer-neutral editor contracts, generation-safe asynchronous data, and no ImGui dependency in engine hot paths.

## Current execution stop

Milestone **23** is complete in implementation scope. Continue this explicitly assigned batch with **26 → 29**, pushing each milestone independently after validation.
