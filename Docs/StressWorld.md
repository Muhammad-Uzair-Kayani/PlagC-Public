# Integrated FPS Stress World

Sandbox now opens one continuous procedural facility. The former playable test
scenes and selectors are removed. Core physics, input, ECS and transform regressions
remain standalone; `--self-test` runs core physics checks before opening the world.

After pulling `prototype`, regenerate projects and rebuild. Windows adds the system
`psapi` library for working-set telemetry. Run Release/Dist for performance evidence.
FPS locking is disabled through `Engine::SetFPSLocked(false)`.

## Level and ownership

The north-to-south layout has three pairs of connected districts:

| West | East |
|---|---|
| Physics yard: domino lanes, cargo, ramp | Collapse works: falling towers and chute |
| Industrial hall: warehouse, lights, moving machinery | Constraint factory: hinged bridges and heavy chains |
| Dynamic city: open rooms and cargo modules | Terrain range: heightfield slopes and projectile space |

A central avenue and two cross streets join all six. There are no scene transitions.
The player starts on the north avenue, facing south. Different material colours,
warehouse point lighting, sunlight, sky/reflections and shadows distinguish areas.
All mesh geometry is procedural; the existing repository cubemap is reused.

`Stress::Simulation` owns a World and its PhysicsSystem. `StressWorld` owns rendering,
input and capture; `StressHud` owns text/crosshair resources. World/physics lifetime
is independent of the renderer. Preset changes preserve the camera and existing
facility. Additional bridge/chain/machinery tiers appear at 2,500 and 6,500 entities;
cargo growth adds tower, domino and pile modules throughout the level. Downscaling
removes excess cargo, retaining infrastructure already built. Shift+R fully rebuilds.

The capsule uses Jolt CharacterVirtual with gravity, ground/slope detection, stair
stepping, body pushing and moving-ground velocity. Camera motion interpolates fixed
physics poses. Generic raycasts resolve body handles to entities. Holding applies
a mass-scaled spring/damping force; throwing applies an impulse. It never teleports
render objects to fake interaction. F recycles a distant body into a CCD projectile.
Sensors exercise queued trigger events and player overlap queries.

## Controls

| Input | Action |
|---|---|
| WASD / mouse | Move / look |
| Space / Shift | Jump / sprint |
| E | Grab the dynamic object under the crosshair |
| Left mouse | Throw held object, otherwise push targeted body |
| G / F | Drop / fire CCD projectile |
| R / Shift+R | Reset player / reset entire world |
| Tab | Release or capture mouse; focus loss releases it |
| F1 / F2 | Help / active-sleeping physics colours and sensor zones |
| F3 / F4 | Performance HUD / wireframe |
| F5 / F6 / F7 | Asset animation / positional chime / cooked asset reload |
| 1 / 2 / 3 / 4 | 1,000 / 2,500 / 5,000 / 10,000 entities; disable automatic growth |
| B / V | Add 100 / 500 within the 10,000 ceiling |
| T | Toggle progressive growth |
| P / N | Pause / one fixed physics step |
| Escape | Exit |

F2 visualizes actual collider-derived render meshes, activity colours and sensor
volumes; it is not Jolt's native debug-draw overlay. Static terrain/lights cannot be
grabbed. Constraints remain connected while their dynamic members are held.

## Population and activity

Default growth is every 30 **simulated** seconds:
1,000 → 1,500 → 2,000 → 3,000 → 4,000 → 5,000 → 6,500 → 8,000 → 10,000.
Pausing freezes growth and factory clocks. The factory recycles 24 distant bodies
every two simulated seconds. Held/near-player bodies are excluded from recycling.
When activity is low, physical impulses disturb sleeping cargo in up to three
regions per second. Sleeping remains enabled; bodies are not pinned awake.

The entity ceiling includes statics, lights and the camera. Consequently a 1,000
entity world cannot contain 1,000 active dynamic bodies. The requested targets are
1,000 below 5k entities, 1,500 at 5k and 2,000 at 10k. The HUD separately reports the
feasible target (limited by dynamic+kinematic population), actual activity and
whether the target is met. This is a measured target, not a guarantee every frame.

## Capture and CLI

From the repository root, for example:

```bat
Bin\windows-x86_64\Dist\Sandbox.exe
Bin\windows-x86_64\Dist\Sandbox.exe --entities 5000 --no-progressive
Bin\windows-x86_64\Dist\Sandbox.exe --benchmark --seconds 600 --csv Logs\Automated.csv
Bin\windows-x86_64\Dist\Sandbox.exe --soak --seconds 1800 --csv Logs\InteractiveSoak.csv
python Scripts\BenchmarkStress.py --exe Bin\windows-x86_64\Dist\Sandbox.exe --seconds 30
```

`--benchmark` advances exactly one fixed tick per rendered frame, drives an automated
player route with grab/throw/projectile attempts, disables interactive controls
except Escape, and stops after `seconds * physicsHz` measured frames. Duration is
simulation duration, not wall time. It is useful for repeatable workload comparisons;
the route is not a navigation/pathfinding system and can be blocked by debris.

`--soak` preserves normal real-time simulation, input and HUD. After warmup it captures
for the requested elapsed frame time (600–1800 seconds for a 10–30 minute run).
Walk between districts, grab/throw cargo, fire at towers, change presets, and inspect
activity, RSS and dropped time. These two capture modes are mutually exclusive.

Both write every measured frame to CSV and a `.summary.csv` with median, p95, p99,
peak, total samples and quantile samples for fourteen timing series. At most 65,536
samples are retained in a uniform reservoir. Quantiles are exact below this limit,
estimates above it; peaks cover all frames. The complete CSV supports exact offline
analysis. Warmup defaults to 120 frames. Captures close on Escape or normal shutdown.

Other arguments: `--entities 1000..10000`, `--seed N`, `--no-progressive`,
`--warmup 0..10000`, `--seconds 1..1800`, `--physics-hz 1..240`,
`--physics-workers 0..64`, `--physics-scheduler native|plagc`, `--workers 1..32`,
`--frames N`, `--headless`, `--null`, `--offscreen`, `--self-test`.
`--physics` and `--stress` are aliases for the default world; old scene options are
rejected with a documentation link. Native scheduling defaults to one engine worker
and up to two Jolt workers, avoiding two full-size worker pools.

## What the measurements mean

The HUD/CSV show physics step, input/output, presentation, gameplay, character/event,
query, scene synchronization and frame timings, plus entities, bodies, activity,
contacts, constraints, ECS writes, renderer transforms/objects and query counts.
Render CPU refers to the **previous** completed render call; it is not GPU time.
Render objects are allocated instances, not a measured visible/frustum count.
RSS is the whole process's working set, not physics allocations. Physics-specific
memory is explicitly unavailable. Detailed SceneRenderer profiling is enabled.

## Validation

Run `bash Scripts/TestPhysics.sh`, `bash Scripts/ValidateIntegration.sh`,
`bash Scripts/TestStressWorld.sh`, and `bash Scripts/TestStressApp.sh` on Linux with
the pinned dependencies. `STRESS_SOAK_SECONDS=600 bash Scripts/TestStressWorld.sh`
adds a fixed-tick CPU soak and writes `Bin-Int/PhysicsTests/stress-soak.csv`.
`STRESS_SANITIZE=1` enables ASan/UBSan for the stress model/application-side code;
the cached Jolt archive is not instrumented by that option.

See [StressValidation.md](StressValidation.md) for executed checks. Recording-renderer
checks validate resource lifetime and control routing, not native Ogre pixels or
Windows keyboard/mouse delivery. Native Windows acceptance still requires testing
walking on stairs/ramps/bridges/platforms, warehouse lighting/shadows, focus capture,
all controls and a real 10–30 minute interactive soak on the target machine.

The [asset phase](Assets.md) adds two imported beacon entities within the existing
entity budget. Use `--no-asset-demo` for procedural-only comparisons and `--no-audio`
to disable device creation. Native hardware/render validation is documented separately.
