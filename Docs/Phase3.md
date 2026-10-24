> Historical phase report. The former demo scenes are removed; current startup and controls are in [StressWorld.md](StressWorld.md).

# Phase 3 — World / ECS

Status: implementation and Linux Debug Sandbox verification complete;
Windows user acceptance pending. Phase 2 was accepted by the user on 2026-10-23.

## Design

World is the engine-owned scene data layer. EnTT v3.13.2 is a pinned, shallow,
header-only submodule with its own Premake utility project. Only World.cpp
includes EnTT; client code and public headers have no vendor types.

Each entity has a session identity, name and transform. Optional typed components
are Renderable, Light and Camera. Names may be duplicated or empty. Public entity
IDs increase across all worlds and are not recycled when EnTT reuses storage.
Stale, null and cross-world handles fail before accessing components. These IDs
are session handles, not persistent asset UUIDs or serialization identifiers.

Transforms are flat world-space TRS (positive scale). Camera components are
look-at cameras: their entity transform supplies position; component target is
world-space. Light position also comes from Transform; direction comes from the
light component. Entity rotation/scale apply to renderables, not look-at cameras
or light directions. These rules avoid implicit transform conventions.

World construction, destruction, queries and mutations are main-thread operations.
Reads and snapshots return owned copies. Snapshots are sorted by identity for
repeatable iteration. ApplyTransforms validates every handle, transform and
camera constraint before committing any value; duplicates invalidate the batch.
Jobs can modify disjoint ranges of a copied transform buffer; the client waits
for jobs, then commits it on main. The existing lock-free job system is unchanged.
There is no claim of concurrent EnTT registry mutation.

SceneRenderer owns Ogre-facing object/light instances while borrowing mesh and
material handles. The renderer must outlive the bridge. The bridge does not
retain a World reference, so World destruction itself is independent; the next
Sync or Clear removes obsolete instances. World destruction alone does not
implicitly call Ogre. Changes become visible at Sync, before Render.

A mesh/material change creates the replacement instance before releasing the
previous one. Invalid borrowed handles fail during Sync and leave that entity's
previous instance intact. Sync is not an atomic transaction across all entities;
a caller can fix invalid data and retry. Component removal/entity deletion
releases instances at the next successful Sync. Clearing the bridge releases
all its instances. Removing/deleting the active camera clears World selection;
without an active camera the renderer retains its last view.

This phase establishes entity/component lifecycle, scene synchronization and
job-compatible transform systems. Hierarchy, persistence, scene serialization,
undo/redo, editor selection and arbitrary user-defined component registration
are not exposed by this initial typed API; they require explicit extensions in
the editor/assets phases. Physics integration belongs to Phase 4. No placeholder
implementations of those systems are presented as complete.

## Develop

- World.h / World.cpp: identities, registry, components, validation and snapshots.
- SceneRenderer.h / SceneRenderer.cpp: object, light and active-camera synchronization.
- WorldDemo.cpp: executable client tests plus a live 16-cube ECS scene.
- EnTT.lua / .gitmodules / dependency markers: reproducible integration.

```cpp
PlagC::World world;
PlagC::SceneRenderer scene(renderer); // renderer outlives scene
const auto cube = world.Create("Cube");
world.SetTransform(cube, {{0, 1, 0}, {}, {1, 1, 1}});
world.SetRenderable(cube, {mesh, material}); // borrowed resources
scene.Sync(world);
renderer.Render(dt);
world.Destroy(cube);
scene.Sync(world); // mesh/material can now be released safely
```

## Test / verify / checklist

All new tests live in the Sandbox client and run automatically. Earlier Phase 1
and 2 checks still execute before the live World scene starts.

- [x] Entity create/read/rename/destroy/clear and stale/cross-world rejection.
- [x] Independent copied reads and deterministic snapshots.
- [x] Invalid/nonfinite transforms preserve prior state.
- [x] Component add/update/remove, repeated removal and invalid component input.
- [x] Active camera selection and removal/deletion cleanup.
- [x] Worker access rejected; 256 snapshot transforms processed through jobs.
- [x] Atomic batch validation for stale handles, duplicate IDs and invalid values.
- [x] Renderer instances created without duplication on repeated Sync.
- [x] Resource replacement, invalid borrowed handle recovery and in-use protection.
- [x] Removal/deletion/world switching and bridge destructor release instances.
- [x] Real EGL pixel comparisons for visibility, transforms, lighting and camera.
- [x] Live job-driven scene capture and full shutdown resource cleanup.
- [x] Public headers compile independently without EnTT/Ogre include paths.
- [x] VS2022 projects contain World sources and the private EnTT dependency.
- [ ] Windows Debug/Release/Dist build and runtime acceptance.
- [ ] Linux Release/Dist and macOS execution of this increment.

Verified on Linux Debug: headless with one worker, NULL window with four workers,
and EGL real rendering with four workers. Each run exited successfully. Evidence
and check output are under Docs/Evidence/Phase3* and Docs/Evidence/World-*.png.
VS2022 project generation is not an MSVC execution claim.

## Update / polish / notify

The phase tracker and README reflect Phase 2 acceptance and Phase 3 delivery.
Dependency pins and source ownership are documented. Existing W (wireframe),
S (shadows), E (sky) and R (reflections) work on the ECS scene. Escape exits.

On Windows: pull prototype, run GenerateProjects.bat, build Sandbox, and verify
sixteen rotating cubes above the checker floor. Check PASS lines and inspect
Logs/Phase3.ppm and Logs/World-*.ppm. Exercise W/S/E/R, resize/minimize/restore,
then close. Run Scripts\Validate.bat for the three build configurations.

