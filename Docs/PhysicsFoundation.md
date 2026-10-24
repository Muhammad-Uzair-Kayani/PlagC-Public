# Phase 4 — physics foundation

> Historical foundation notes. The current APIs, controls, scenes and validation limits are documented in [Physics Lab](PhysicsLab.md).

This describes the first Phase 4 increment, not the current feature set.
It establishes the engine boundary and sparse ECS handoff before adding gameplay
physics features. The existing integration stress scene remains the default.

## Build and run

Update `prototype`, run `FetchDependencies` and regenerate the Premake project.
Jolt v5.3.0 is pinned at `0373ec0dd762e4bc2f6acdb08371ee84fa23c6db`.
The PlagC-owned `Build/Jolt.lua` builds it as a static CPU library. No CMake,
AVX requirement, debug renderer or object-stream feature is enabled. The
SerializableObject RTTI source is required even with object streaming disabled.
The allocator definition is identical in Jolt and the private engine adapter.

Run a Release Sandbox from the repository root:

```sh
Sandbox --physics --physics-bodies 1000 --physics-workers 2
```

Controls: Space pauses, N advances one fixed tick, R resets box positions and
linear velocities, I applies an impulse to the first box, Escape closes.
Rendering explicitly calls `SetFPSLocked(false)`; simulation remains 60 Hz.
The scene uses separated columns of boxes, a static floor, a light and camera.
Select body count at startup (0–16000). This is the first lab scenario; it is
not yet a full scenario browser or free-flight camera.

`--physics-workers` selects Jolt background workers (0 runs on the calling
thread). `--workers` selects PlagC workers. With no explicit values, the CLI
reserves one main thread and up to two Jolt workers, then assigns the remaining
hardware threads to PlagC, capped at 32. On one hardware thread, PlagC still
requires one background worker. Explicit worker overrides may oversubscribe;
the lab prints both counts. Jolt uses its native pool, not a custom adapter.

CPU integration tests independent of Ogre:

```sh
bash Scripts/TestPhysics.sh
bash Scripts/BenchmarkWorld.sh
```

The first script builds the real pinned Jolt library with GCC (or `CXX`), then
links physics/ECS tests and a recording renderer. `BUILD_JOBS` controls compiler
parallelism. Its archive cache is invalidated by source/header or compiler
changes. This is not a substitute for native Ogre/MSVC testing.

## Ownership and execution

- Public headers expose PlagC types only. Shapes: box, sphere, capsule, cylinder;
  material friction/restitution, mass, gravity factor, damping, sleep and CCD.
- `World::SetPhysics` attaches the body/collider description; `Advance(0)` creates
  pending bodies as one batch without advancing time. Destruction/removal is
  reconciled on the next physics operation. One PhysicsSystem owns each World.
- Dynamic transforms belong to physics. `World::SetTransform`, public transform
  batches and TransformSystem reject dynamic writes. Use PhysicsSystem::Teleport
  for explicit repositioning; set an initial transform before attaching physics.
- Static transforms are level geometry. Kinematic transforms are gameplay targets;
  each fixed tick reaches the latest target, then stops at it. Runtime scale
  changes require `SetPhysics` again to rebuild the collider. Physics cameras are
  rejected in this increment because their ownership/interpolation is not defined.
- Simulation uses an accumulator, 60 Hz by default, at most four catch-up steps.
  Extra whole steps are dropped and reported; fractional time is retained.
  Forces are applied to the next fixed step and cleared by Jolt. Impulses are
  instantaneous. Gameplay must not apply frame-rate-dependent forces.
- Structural ECS extraction runs only when physics components change. Stable
  bindings track engine entity/body identity; stale handles never alias new bodies.
  Non-dynamic input revisions are checked per tick. Dynamic/sleeping entities
  are not scanned to find gameplay inputs.
- Physics writeback iterates the union of active-before and active-after bodies,
  including bodies that sleep during the tick. It directly updates internal ECS
  transforms/revisions, without Snapshot, sorting, names, ApplyTransforms or a
  reconstructed per-frame entity hash set. Final sleepers are retained across
  catch-up ticks for presentation, then disappear from subsequent update lists.
- `Presentation()` interpolates position and quaternion rotation between fixed
  ticks. Pass it to `SceneRenderer::Sync(world, presentation)`. It overrides only
  renderer objects and does not mutate authoritative ECS transforms. Sleeping
  transitions and teleports snap to their final pose. Lights remain authoritative.
- Collision categories are StaticWorld, Dynamic, Character, Vehicle, Projectile,
  Sensor, Debris and Ragdoll. StaticWorld is reserved for static motion. The
  default matrix allows pairs except static/static, sensor/static, sensor/sensor.
  Sensor bodies are deliberately rejected until trigger events are implemented.
  Categories do not imply a character/vehicle/ragdoll controller implementation.
- Raycasts use displacement (direction * distance), return an engine body handle,
  point, normal and fraction, and respect the supplied category's collision mask.
  `EntityFor` maps a current physics hit back to its ECS entity.

## Measurement and remaining work

Lab logs report body/active/sleeping counts, fixed steps, dropped seconds, ECS
writes, physics update time, input/output extraction time, interpolation and
render synchronization time for the sampled frame. Zero-step frames legitimately
report zero physics work. PhysicsWorld exposes cumulative ray query counters/time.
The baseline uses Jolt's malloc-backed temporary allocator; it does not yet expose
peak allocator, island/contact, queue-depth or task timing telemetry. Measure and
replace that allocator under representative workloads before claiming a budget.

Still to implement and validate: triggers with queued main-thread events, overlap
and shape casts, constraints, compound/convex/mesh shapes, collision groups for
ragdolls, virtual characters, vehicles, the remaining lab scenarios and detailed
telemetry. A custom PlagC job adapter follows correctness/performance comparison
against this native Jolt pool. No low-end frame-budget or GPU performance claim is
made from these CPU tests.
