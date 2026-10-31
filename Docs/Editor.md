# PlagC Editor — Real Capability, Integration Status, and Stabilization Roadmap

> **Audit baseline:** `prototype` at `09d44fca76251470b11da89c0f2a8c234fd0827e`  
> **Product status:** **integration-stage alpha**

This document is the source of truth for the **actual usable state** of PlagC Editor.

The previous version overstated readiness by marking many milestones `DONE` when their source code, UI surface, or source-level regression existed, even though the complete user workflow had not been exercised end-to-end. That made the roadmap describe implementation coverage rather than product readiness.

This document intentionally separates those two things.

---

# 1. Status rules

A feature must not be called complete merely because:

- a button exists;
- an engine API exists;
- an ImGui panel exists;
- a source file contains the expected implementation token;
- a Python source-regression passes;
- the feature works in isolation but conflicts with another subsystem.

Use these statuses instead:

- **`ACCEPTED`** — the complete user workflow works in the running Editor, survives save/reload where applicable, is regression-protected by meaningful executable/integration coverage, and has no known blocker in its acceptance path.
- **`IMPLEMENTED`** — substantial real code exists and the main isolated workflow works, but full product acceptance has not yet been completed.
- **`PARTIAL`** — meaningful implementation exists, but required behavior is incomplete, inconsistent, or missing important integration.
- **`BROKEN`** — the intended workflow currently has a known logic defect that prevents correct use.
- **`FOUNDATION`** — backend capability exists, but the Editor-facing workflow is not yet a complete usable feature.
- **`NOT STARTED`** — the feature has not meaningfully entered implementation.
- **`BLOCKED`** — implementation cannot proceed until an explicitly named prerequisite is resolved.

## Acceptance rule

`ACCEPTED` requires **behavioral evidence**, not source-token evidence.

For editor-facing features, acceptance should normally include an actual sequence such as:

1. launch Editor;
2. create/open a scene;
3. perform the interaction through the UI;
4. verify renderer/runtime result;
5. save and reopen when persistence is relevant;
6. verify undo/redo when mutation is relevant;
7. verify Play/Edit isolation when runtime behavior is relevant;
8. verify Release/Dist where packaging or startup is relevant.

Source-inspection tests remain useful as architectural regressions, but they do **not** prove usability.

---

# 2. Architecture contract

## Engine ownership

The engine owns runtime truth:

- `World` / EnTT storage;
- renderer resources, targets and Ogre integration;
- `AssetManager`;
- `JobSystem`;
- logging and profiling;
- window, input and events;
- physics;
- animation;
- audio;
- gameplay/runtime lifecycle.

Engine hot paths must not depend on Dear ImGui.

## Editor backend ownership

`PlagC::Editor` owns authoring state and editor-safe operations:

- `EditorSession`;
- `SceneDocument`;
- persistent entity IDs;
- `CommandHistory`;
- hierarchy operations;
- transform transactions;
- selection state;
- asset-browser presentation models;
- viewport authoring state;
- isolated `PlaySession` runtime clones.

## Editor frontend ownership

The Editor executable owns:

- docking and workspace presentation;
- menus and toolbars;
- hierarchy;
- inspector panels;
- content browser;
- console and statistics presentation;
- viewport overlays and gizmos;
- drag/drop UI;
- import UI;
- editor preferences;
- editor-only visualization.

## Non-negotiable invariants

- Runtime Play must never simulate the authoring world directly.
- Stop must restore the unchanged authoring world.
- Persistent scene IDs, not transient EnTT handles, cross editor subsystem boundaries.
- Transform semantics must be consistent across generic entities, cameras, lights, physics and animation.
- Editor-only visualization must not leak into Game output.
- A feature may not claim `ACCEPTED` while another subsystem makes its documented workflow impossible.

---

# 3. Current real product state

PlagC Editor is **not a fake UI**. A large amount of real backend and frontend work exists. The problem is that many systems were completed independently and were marked done before their cross-system behavior was validated.

The result is an Editor that looks substantially more mature than its current end-to-end reliability.

## What is genuinely present

The current source contains real implementations for:

- standalone docked Editor executable;
- scene hierarchy and persistent IDs;
- create/delete/duplicate/rename/reparent operations;
- scene save/open/save-as;
- undo/redo infrastructure;
- model, material, camera, light, physics and animator authoring APIs;
- renderer-backed scene viewport;
- viewport picking;
- transform gizmos and editor camera navigation;
- built-in primitive model assets;
- content browser and asset import UI;
- asynchronous thumbnail/preview infrastructure;
- material/PBS authoring;
- animation controller/state-machine authoring;
- isolated Play/Pause/Step/Stop runtime world;
- runtime physics and animation update paths;
- console and profiler/statistics panels;
- Dist windowed Editor packaging support.

These are substantial foundations, but many are still `IMPLEMENTED` or `PARTIAL`, not `ACCEPTED`.

---

# 4. Confirmed integration defects

These are known product-level defects discovered by running and auditing the current Editor. They take priority over new feature work.

## P0 — Scene View and Game View are not correctly separated

**Status: `BROKEN`**

When Play starts, the renderer switches to the runtime world's active camera. However, editor viewport overlays are still drawn using the editor camera.

Current consequence:

- 3D geometry may be rendered through the game/runtime camera;
- grid, selection feedback and orientation UI may still be calculated from the editor camera;
- editor visualization can therefore be spatially inconsistent with the actual Play view;
- there is no clean Scene/Game visualization boundary yet.

Milestone 29 exists specifically to solve this and has not been completed.

### Required correction

Introduce an explicit visualization policy:

- **Edit / Scene View:** editor camera, grid, selection, gizmos, preview lights, collider/frustum/light helpers as requested.
- **Play / Game View:** runtime active camera and game rendering only by default.
- Optional runtime debug overlays must be separately enabled and use the runtime camera.

---

## P0 — Runtime selection/inspection is internally contradictory

**Status: `BROKEN`**

The runtime clone starts with no selection.

During Play:

- hierarchy selection is disabled because authoring is disabled;
- viewport picking exits because authoring is disabled;
- the Inspector still tells the user to select a runtime entity.

Therefore the UI asks for an action that currently has no valid interaction path.

### Required correction

Create explicit runtime selection state that:

- can be changed from hierarchy and Game/Scene viewport while Play is running;
- never mutates authoring selection/history;
- allows read-only runtime inspection;
- is discarded on Stop.

---

## P0 — Camera Transform rotation does not define camera orientation

**Status: `BROKEN`**

The runtime camera currently uses:

- `Transform.position` for camera position;
- `CameraComponent.target` for look direction.

`Transform.rotationDegrees` is not used to construct the runtime camera orientation.

This means rotating a Camera entity with the normal Rotate gizmo can change its Transform rotation without changing the actual game-camera direction.

### Required correction

Use one coherent camera pose model:

- Transform position + rotation define camera pose;
- Camera component stores projection settings such as FOV, near/far and aspect policy;
- editor alignment tools update Transform consistently;
- serialization preserves the same semantics.

A target/look-at helper may exist as an editor operation, but it must not compete with Transform rotation as a second independent orientation source.

---

## P0 — Directional/Spot Light Transform rotation does not define light direction

**Status: `BROKEN`**

Light position is synchronized from Transform position, but light direction is stored independently in `LightComponent.light.direction`.

This makes normal Rotate gizmo behavior inconsistent for directional and spot lights.

### Required correction

Directional and spot light direction should be derived from entity Transform rotation, or the Editor must use one clearly defined equivalent orientation representation. Generic transform rotation and light direction must not diverge.

---

## P1 — Light “Shadows” control is renderer-global, not per-light

**Status: `PARTIAL`**

The Light Inspector exposes a Shadows control, but it currently changes the renderer-wide shadow setting rather than a property of the selected light.

The current UI therefore implies a per-light capability that does not exist.

### Required correction

Either:

- implement real per-light `castShadows` support through scene data, renderer-neutral contracts, Ogre backend and serialization;

or:

- move the existing setting out of the Light Inspector and clearly label it as Scene/Renderer Shadows.

No fake per-light UI.

---

## P1 — Rendering Quality overstates MSAA completeness

**Status: `PARTIAL`**

The current quality audit uses a 1x MSAA baseline. That validates a non-multisampled target; it does not mean selectable real 2x/4x/8x MSAA is implemented.

### Required correction

Implement and validate actual multisampled viewport targets/resolves, backend fallback and user-selectable supported sample counts before calling MSAA complete.

---

## P1 — Test coverage is too source-oriented

**Status: `PARTIAL`**

A large portion of the current Editor regressions verify source tokens such as button names, function calls or implementation strings.

Those tests are useful for structural regression, but they do not prove that a user workflow works.

This is how contradictory states such as “runtime selection exists” and “runtime selection is impossible” can survive with green tests.

### Required correction

Critical workflows need executable/integration coverage, especially:

- open/save/reopen scene round-trip;
- camera transform -> game-camera result;
- light transform -> renderer direction result;
- Play camera selection;
- Scene/Game overlay isolation;
- runtime read-only selection;
- Pause/Step/Stop behavior;
- Stop restores authoring world exactly;
- asset missing/reload/reimport behavior;
- Dist startup and packaged runtime resources.

---

## P1 — Dist/package runtime still relies on relative working-directory data paths

**Status: `PARTIAL`**

Dist is now a windowed application and packaging preserves required runtime assets, but renderer/media paths are still relative to the process working directory.

Launching `Editor.exe` independently can fail if the working directory is not the package root.

### Required correction

Resolve runtime resource roots relative to the executable/package location rather than assuming the caller's current working directory.

---

# 5. Honest milestone status

The original 43 milestone numbers are retained so existing history and discussions remain meaningful. Statuses below represent **current product truth**, not how much code was written.

| # | Area | Real status | Current interpretation |
|---:|---|---|---|
| 1 | Viewport / Scene View | `IMPLEMENTED` | Strong authoring viewport exists; Game/Scene separation still affects integration acceptance. |
| 2 | Scene Hierarchy | `IMPLEMENTED` | Main edit hierarchy works; runtime-selection behavior is unresolved. |
| 3 | Entity Creation | `IMPLEMENTED` | Empty/model/camera/light/primitives/physics creation paths exist. |
| 4 | Inspector | `PARTIAL` | Edit inspection is substantial; runtime inspection path is currently impossible despite UI messaging. |
| 5 | Transform Inspector | `PARTIAL` | Generic transforms work, but camera/light rotation semantics are inconsistent. |
| 6 | Model / Mesh Inspector | `IMPLEMENTED` | Main model editing exists; broader asset integration still limits robustness. |
| 7 | Material System + Inspector | `IMPLEMENTED` | Real PBS/material authoring exists; full asset/project acceptance remains pending. |
| 8 | Lighting Inspector | `PARTIAL` | Light properties work, but transform-direction semantics and shadow ownership are wrong/incomplete. |
| 9 | Camera Inspector | `PARTIAL` | Projection/active-camera tools exist; camera orientation semantics and Play/Game integration are incomplete. |
| 10 | Physics Inspector | `IMPLEMENTED` | Main body/collider authoring exists; full debug/acceptance workflow remains pending. |
| 11 | Animator Inspector | `IMPLEMENTED` | Substantial animation authoring exists; real-asset workflow acceptance still required. |
| 12 | Animation State Machine | `IMPLEMENTED` | Real controller/state infrastructure exists; full production workflow acceptance still required. |
| 13 | Content Browser | `IMPLEMENTED` | Main browser UI exists; asset-manager integration remains incomplete. |
| 14 | Thumbnail / Preview | `PARTIAL` | Real async preview path exists; recent UI-texture crash showed integration was not yet robust. |
| 15 | Asset Import | `IMPLEMENTED` | Import/reimport infrastructure exists; end-to-end project asset lifecycle still needs acceptance. |
| 16 | Scene File Workflow | `IMPLEMENTED` | v4 persistence is substantial and primitive persistence was repaired; full project-scene acceptance still pending. |
| 17 | Play Toolbar | `PARTIAL` | Transport/runtime clone works, but Game View isolation and runtime inspection are broken. |
| 18 | Runtime / Game Logic | `FOUNDATION` | Runtime lifecycle, physics and gameplay hooks exist; Editor integration is not product-complete. |
| 19 | Console | `IMPLEMENTED` | Real console/log presentation exists. |
| 20 | Statistics / Profiler | `IMPLEMENTED` | Real live telemetry exists; acceptance/performance baselines should still be formalized. |
| 21 | Editor Toolbar | `IMPLEMENTED` | Main controls exist and are connected. |
| 22 | Menu Bar | `IMPLEMENTED` | Main menu surface exists; individual actions still inherit subsystem limitations. |
| 23 | Editor Theme / Visual System | `NOT STARTED` | Current appearance is primarily functional ImGui styling, not a finished PlagC visual system. |
| 24 | Icons / Fonts | `IMPLEMENTED` | Embedded editor icons/font path exists. |
| 25 | Docking / Workspace | `IMPLEMENTED` | Docking/default layout exists; broader UX acceptance still pending. |
| 26 | Selection System | `BROKEN` | Edit selection exists, but runtime selection/inspection contract is incomplete and contradictory. |
| 27 | Undo / Redo | `IMPLEMENTED` | Command history is substantial; needs workflow-level coverage across all mutable components. |
| 28 | Drag & Drop | `IMPLEMENTED` | Multiple DnD paths exist; depends on incomplete asset integration for full robustness. |
| 29 | Editor Preview Rendering | `BROKEN` | Required Scene/Game isolation is not implemented; editor overlays can leak/mis-project in Play. |
| 30 | Rendering Quality | `PARTIAL` | HDR/bloom/environment/shadows exist; real MSAA and stronger backend acceptance remain. |
| 31 | Asset Manager Integration | `FOUNDATION` | Loading/reload foundations exist; missing/moved/dependency/background-state UX is incomplete. |
| 32 | Prefab-like Reusable Objects | `NOT STARTED` | No complete prefab workflow. |
| 33 | Audio Editor Integration | `FOUNDATION` | Runtime/OpenAL foundation exists; usable editor authoring workflow is incomplete. |
| 34 | Debug Visualization | `PARTIAL` | Some light/camera/physics helpers exist, but no coherent debug-visualization system. |
| 35 | Keyboard Shortcuts | `PARTIAL` | Several shortcuts exist; complete context-safe routing and coverage remain incomplete. |
| 36 | Editor Settings | `PARTIAL` | Renderer/preferences infrastructure exists; complete persisted editor settings are not finished. |
| 37 | Performance | `PARTIAL` | Profiling and async work exist; large-scene responsiveness is not yet an accepted product property. |
| 38 | Error Handling | `PARTIAL` | Logging/dialogs exist and Dist fatal reporting was improved; failures are not yet consistently actionable in-editor. |
| 39 | Notifications / Feedback | `NOT STARTED` | No complete non-blocking status/toast system. |
| 40 | Testing | `PARTIAL` | Many regressions exist, but too many are source-token tests instead of executable workflows. |
| 41 | CI / Build | `PARTIAL` | Windows/MSVC validation exists; backend/runtime/Dist acceptance is not yet comprehensive. |
| 42 | Repository Cleanup | `PARTIAL` | `prototype` is authoritative, but historical branch/work cleanup remains unfinished. |
| 43 | First Usable Editor Acceptance | `NOT STARTED` | This is now the primary product gate and must not be marked complete by component-level tests. |

## Current `ACCEPTED` count

**0/43 milestones are currently declared `ACCEPTED` under the stricter product-level definition.**

This does **not** mean nothing works. It means the repository has not yet applied the new end-to-end acceptance standard to any milestone.

Existing implementation should be preserved and promoted to `ACCEPTED` only after the appropriate workflow is actually validated.

---

# 6. Stabilization plan before new feature expansion

The next Editor phase is not “add more panels.” It is **integration stabilization**.

## Phase A — P0 semantic fixes

### A1. Scene View / Game View contract

- Define explicit Scene View and Game View behavior.
- Runtime camera owns Game rendering in Play.
- Editor grid/gizmos/selection helpers are suppressed from Game output by default.
- Runtime debug overlays, when enabled, use the runtime camera.
- Preview lighting/environment never contaminates Game rendering.

### A2. Selection contract

- Separate authoring selection from runtime selection.
- Hierarchy and viewport can select runtime entities in Play.
- Runtime Inspector is read-only.
- Stop discards runtime selection and restores authoring selection/state.

### A3. Transform semantics

- Camera orientation derives from Transform rotation.
- Directional/Spot Light orientation derives from Transform rotation.
- Align-view/camera operations update the same canonical Transform representation.
- Save/reload preserves the same result.

## Phase B — misleading/incomplete controls

### B1. Shadows

- Implement real per-light shadow control, or remove the per-light implication from the Inspector.

### B2. Rendering quality

- Implement real MSAA or document 1x honestly as “no MSAA.”
- Validate OpenGL and Vulkan runtime behavior where hardware/context permits.

### B3. Runtime paths / Dist

- Resolve assets and Ogre media relative to the executable/package root.
- Validate Editor from a copied standalone Dist folder, not from the repository working directory.

## Phase C — integration tests

Create executable acceptance tests or deterministic harnesses for:

- scene create/save/open;
- transform editing;
- hierarchy mutation;
- camera pose;
- light pose;
- material assignment;
- physics Play;
- animation Play;
- runtime camera;
- runtime selection;
- Pause/Step/Stop;
- unchanged authoring world after Stop;
- missing assets;
- import/reimport;
- Dist startup/package resources.

Source-level tests may remain, but they cannot be the only evidence for user-facing behavior.

---

# 7. First Usable Editor acceptance gate

Milestone 43 is now the authoritative release gate for the first genuinely usable PlagC Editor.

It must be executed as a real workflow, not inferred from individual milestones.

## Acceptance scene

Use a non-trivial scene containing at least:

- several primitive and imported models;
- hierarchy nesting;
- multiple materials;
- directional + point/spot lighting;
- at least one active camera;
- static and dynamic physics;
- at least one animated asset/controller;
- enough entities to exercise hierarchy/content-browser usability.

The generated racing-circuit scene is a useful stress/visualization scene, but a final acceptance scene should also include real imported assets/materials/physics/animation so asset lifecycle paths are exercised.

## Required workflow

1. Launch the Editor from a clean packaged or normal development build.
2. Create a new scene.
3. Create primitives and import/drop real models.
4. Select entities from both Hierarchy and Scene View.
5. Translate, rotate and scale them.
6. Verify camera rotation changes the game-camera view.
7. Verify directional/spot light rotation changes light direction.
8. Organize entities with hierarchy parenting/reparenting.
9. Assign/edit materials and verify rendered result.
10. Add/edit lights and shadows using truthful controls.
11. Add and activate a camera.
12. Add physics to suitable entities.
13. Add animation/controller content.
14. Undo and redo representative edits.
15. Save the scene.
16. Close/reopen the scene.
17. Verify hierarchy, transforms, camera, lights, materials, physics and animation persisted.
18. Press Play.
19. Verify Game rendering uses only the runtime camera and expected game visualization.
20. Select a runtime entity and inspect it read-only.
21. Verify physics and animation run.
22. Pause.
23. Step exactly one simulation step.
24. Resume.
25. Stop.
26. Verify the authoring scene is unchanged from its pre-Play state.
27. Run the same scene through Release and Dist acceptance where applicable.

Only after this workflow passes should Milestone 43 become `ACCEPTED`.

---

# 8. Execution policy from this point forward

1. `prototype` remains the source of truth.
2. Do not mark a milestone `ACCEPTED` from source inspection alone.
3. Fix P0 integration defects before adding unrelated Editor features.
4. One assigned milestone/fix stream at a time unless the user explicitly assigns parallel work.
5. Preserve existing working implementation; do not rewrite subsystems merely to satisfy the roadmap.
6. Every fix must state:
   - user-visible defect;
   - root cause;
   - files changed;
   - regression/acceptance evidence;
   - known remaining limitations.
7. If a test only checks source structure, label it **source regression**, not behavioral acceptance.
8. Runtime/renderer/package claims must be verified in the relevant configuration before being stated as passed.
9. Update this document whenever a known limitation is fixed or a milestone is promoted in status.
10. Product truth takes priority over preserving old `DONE` labels.

---

# 9. Immediate priority order

1. **Fix Scene View vs Game View isolation.**
2. **Fix runtime selection and read-only runtime inspection.**
3. **Unify camera Transform rotation with camera orientation.**
4. **Unify light Transform rotation with directional/spot orientation.**
5. **Correct shadow-control semantics.**
6. **Add real workflow tests for Play/camera/selection/Stop restoration.**
7. **Make Dist runtime paths executable-relative and validate standalone launch.**
8. **Complete asset-manager integration and missing/reload UX.**
9. **Complete debug visualization/error/notification systems.**
10. **Execute Milestone 43 end-to-end.**

New visual polish, prefab expansion and other non-critical Editor features should come after this stabilization sequence unless explicitly reprioritized.

---

# 10. Historical note

The earlier milestone implementation history remains available in Git history and is still useful for locating code and prior commits. It should not be interpreted as proof that those milestones were product-complete.

From this revision onward, `Docs/Editor.md` describes **what the Editor can reliably do as a product**, not merely what code has been written.
