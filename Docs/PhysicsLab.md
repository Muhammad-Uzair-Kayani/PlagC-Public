# Physics Lab

Run from the repository root after pulling `prototype` and running
`GenerateProjects.bat`. Rebuild Sandbox in Visual Studio; regeneration picks up
all newly added physics and lab translation units.

```bat
Bin\windows-x86_64\Dist\Sandbox.exe --physics
```

One executable loads **one scenario at a time**. Switching releases its physics
world, constraints, characters, scene objects, meshes and materials. The existing
non-physics integration test remains available by omitting `--physics`.
Rendering calls `Engine::SetFPSLocked(false)`; physics uses a separate fixed
60 Hz clock with interpolation and a maximum of four catch-up ticks.

## Scene selection

| Key | Scene | Alternate (`V`) |
|---|---|---|
| F1 | Shape Gallery: eight collider types | Same gallery |
| F2 | Material Ramps: friction, restitution, mass, damping | Half gravity |
| F3 | CCD Range: fast spheres and thin wall | Discrete collision |
| F4 | Pyramid | Same pyramid |
| F5 | Island Comparison: 50 separated piles | One large pile |
| F6 | Sleeping City | Sleeping disabled |
| F7 | Static World: triangle terrain | Heightfield |
| F8 | Queries: rays, sphere casts and overlaps | Same query mix |
| F9 | Trigger Zones: moving kinematic agents, 100 sensors | 1,000 sensors |
| F10 | Character Course: stairs, ramp, ledge, platform, boxes, door, crowd | Same course |
| F11 | Constraints: many short mixed-joint chains | One connected chain |
| F12 | District Streaming | Individual insertion (also `--physics-individual`) |
| Left Shift + F1 | Physics LOD | Same distance experiment |
| Left Shift + F2 | Ragdolls: distributed physics dolls | One concentrated pile |

Page Up / Page Down cycle all 14 scenes. `R` reloads the selected scene.
`Insert` / `Delete` change the requested workload by 500 (maximum 16,000).
Some scenes cap their specialized workload: 32 virtual characters, 5,000 joints,
60 material-test bodies; each ragdoll uses six bodies. HUD counts show actual
bodies, including floor/anchors. Geometry uses the actual collider surface,
including compound leaves and terrain, rather than placeholder cubes.

## Shared controls

| Key | Action |
|---|---|
| WASD, Q / E | Fly camera, down / up |
| Right mouse button | Hold to look |
| Left Shift | Faster camera movement |
| Arrow keys / Enter | Move first virtual character / jump |
| Space / N | Pause / advance one fixed tick |
| F | Fire a CCD projectile from the camera |
| C / K | Collider wireframe / active-versus-sleeping colours |
| H | Expand/collapse controls in HUD |
| J | Reload with native Jolt pool / experimental shared PlagC scheduler |
| O | Reload at 30 / 60 / 90 Hz |
| M | Reload district in scene 12 |
| B | Restart scene as a CSV benchmark and exit when finished |
| Escape | Exit |

## Engine APIs and ownership

Public physics types contain no Jolt headers or handles. `World::SetPhysics`
attaches descriptions; `PhysicsSystem::Advance(0)` reconciles a batch without
advancing simulation. The safe World APIs remain available. Dynamic transforms
are physics-owned; use `Teleport`, `ApplyImpulse`, `ApplyForce`, `SetVelocity`
and `SetAngularVelocity`. Kinematics use World transform targets. Structural
changes rebuild bindings only when required. Active-body extraction and sparse
ECS writeback avoid copying full snapshots and avoid writing settled sleepers.

Collider types: box, sphere, capsule, cylinder, convex hull, primitive compound,
static triangle mesh and static heightfield. Geometry is shared immutable data;
callers must not mutate it through another mutable alias after attachment.
A nonzero `collisionGroup` suppresses collisions between members of that group.
Center-of-mass offsets are supported. Mesh/heightfield dynamics are rejected.

`Raycast`, `ShapeCast`, and `Overlap` respect the collision matrix and return
PlagC body handles. Casts take a displacement, not an infinite direction.
`BodyFor` and `EntityFor` bridge physics and ECS identities. Queries currently
run on the main thread. Query counters are cumulative; lab query time is per frame.

Point, distance, hinge and fixed constraints use two valid body handles and
world-space anchors. Attach to a static body for a world anchor. Hinges support
limits and a velocity motor. Removing or rebuilding a body removes its connected constraints.

Sensors produce `Enter`, `Stay`, `Exit` records after Jolt worker completion.
`PhysicsSystem::TriggerEvents()` combines events across catch-up ticks. Process
these on the main thread after `Advance`; do not retain the returned span across
another advance. Exit records may contain a destroyed identity: `EntityFor`
then returns empty. Sleeping overlap verification prevents false exits when Jolt
removes cached sleeping contacts; this verification has a cost in sensor-heavy scenes.

`CharacterVirtual` is exposed through character handles/input/state methods.
Character transforms in the lab are separate ECS visual entities, with collision
performed by Jolt's virtual-character system. Crowd collision uses Jolt's simple
character list. It is a correctness demonstration, not a large-crowd solution. Virtual characters are not rigid-body query/trigger targets; gameplay can use overlap queries for their interaction volumes.

The default scheduler is Jolt's native pool. The experimental PlagC adapter
submits ready Jolt jobs to the shared pool, retains Jolt's dependency/barrier
semantics, drains queued wrappers after updates, and executes inline when queue
submission cannot proceed. The borrowed JobSystem must outlive PhysicsWorld.
It allocates job objects and is not claimed faster than the native pool.

## Repeatable benchmarks

```bat
Bin\windows-x86_64\Dist\Sandbox.exe --physics-benchmark --physics-scene 6 --physics-bodies 10000 --physics-workers 4 --workers 1 --physics-warmup 180 --physics-samples 600 --physics-csv Logs\SleepingCity.csv
python Scripts\BenchmarkPhysics.py --debug Bin\windows-x86_64\Debug\Sandbox.exe --dist Bin\windows-x86_64\Dist\Sandbox.exe
```

The matrix runner launches Debug and Dist sequentially with 1, 2, 4, 6 and 11
background physics workers, both schedulers, all scenes and relevant variants.
`--sweep` expands body/query/terrain counts; this can take many hours. `--dry-run`
prints commands. Use `--scenes 6 --workers 2 --schedulers native` for a small run.
Native runs use one otherwise idle PlagC worker; adapter runs use the requested
shared worker count. Both additionally have the calling main thread.

Other Sandbox flags: `--physics-scene 1..14`, `--physics-queries 0..10000`,
`--physics-triangles 0..500000`, `--physics-seed N`, `--physics-hz 1..240`,
`--physics-variant`, `--physics-individual`, `--physics-scheduler native|plagc`.
The terrain triangle count is rounded to a square grid. Query tests split the
requested total approximately equally between rays, casts and overlaps.

Benchmarks advance exactly one fixed tick per rendered sample, disable HUD,
query markers and interactive mutation, and use a repeatable LOD focus path.
CSV separates wall-frame time, CPU preparation, previous-frame renderer CPU time,
physics, input, output, interpolation, render synchronization, queries, events,
characters, streaming and LOD; it records counts and cumulative query totals.
The renderer measurement is CPU submission time, **not GPU time**. Streaming
cost is a spike every five simulated seconds and includes removal and insertion.
The matrix summary reports median and p95, and saves raw CSVs and logs.
A fixed seed/input stream enables comparisons; bitwise determinism across
architectures, compilers or worker counts is not promised.

## Scope and validation limits

The LOD scene demonstrates full box physics, simplified sphere physics and
state-only distant entities with hysteresis and preserved linear/angular velocity.
It is not yet a production city streaming/LOD system. Ragdolls are six-part
physics-only dolls with limited hinges; animation/Ozz handoff remains the later
phase described in the specification. Vehicle suspension/controller integration
is not implemented by the generic constraint playground.

`bash Scripts/TestPhysics.sh` builds the pinned Jolt sources and tests shapes,
queries, events, constraints, CharacterVirtual, scheduler teardown and all 14
scenes in both modes. Recording-renderer tests validate resource ownership,
not native graphics. `bash Scripts/BenchmarkWorld.sh` checks ECS/render regressions.
Native Windows/MSVC, Ogre display/input and the Ryzen 7430U performance matrix
must be run on that target. No low-end frame-budget guarantee follows from these
CPU integration tests. Per-island, broad/narrow-phase and peak allocator telemetry
are not exposed; current contact counts reflect Jolt's retained contact cache.

Sanitizer mode: `PHYSICS_SANITIZE=1 bash Scripts/TestPhysics.sh` instruments the
PlagC bridge, jobs, ECS and test code; the cached Jolt archive remains optimized
and uninstrumented. If the host blocks LeakSanitizer process inspection, rerun
`ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 Bin-Int/PhysicsTests/tests-asan`.
That checks address/undefined behavior but makes no leak-detection claim.

After building tests, `PC_PHYSICS_CPU_BENCH=cpu.csv Bin-Int/PhysicsTests/tests`
runs a smaller CPU-only matrix: all scenes, both schedulers, 1/2/4/6/11 workers,
500 requested bodies (eight characters), 10k terrain triangles, 500 queries,
60 warmup ticks and 120 samples. It omits rendering and is **not** the native
Debug/Dist acceptance matrix. See [validation results](PhysicsValidation.md).
