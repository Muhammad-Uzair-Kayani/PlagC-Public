# Phase 3.5 — input, FPS control and integration stress scene

Delivery branch: `prototype`, based on `prototype` commit `6f713b0`.

## Client API

```cpp
engine.SetFPSLocked(false); // No software delay; requests Ogre VSync off.
engine.SetFPSLocked(true);  // Software cap at 60 FPS; VSync off.
bool locked = engine.IsFPSLocked();
engine.Graphics().SetVSync(true); // Optional, separate refresh synchronization.
```

The integration scene explicitly calls `SetFPSLocked(false)` after all existing
regression demos. The unconditional 1 ms loop sleep was removed. `true` limits
frame starts to approximately 60 per second; slow frames are never delayed further.
Pacing uses a monotonic clock and accounts for update/render/event work. Scheduler
precision can make a locked frame longer than 16.67 ms. `false` removes engine caps,
not GPU-driver or compositor overrides. An uncapped workload can also naturally
run below 60 FPS. The displayed lock/VSync flags report requested engine settings.
A client calling `SetVSync(true)` or `SetSettings` with `vsync=true` can intentionally
restore refresh synchronization. F5 unlock always disables both caps again.

## Input

`PlagC/Input/Input.h`, `KeyCodes.h` and `MouseCodes.h` expose engine-owned types.
`Input::IsKeyPressed`, `IsMouseButtonPressed`, `GetMousePosition`, `GetMouseX/Y`,
`GetMouseDelta` and `GetScrollDelta` are main-thread polling operations.
`InputState` supports standalone event replay without a window.

`Window::Poll` resets transient deltas, polls GLFW, updates input state in callbacks
and posts events. The engine pumps events before the client update. Mouse deltas
accumulate; the first position after creation, cursor-mode change or focus loss
establishes a baseline. Focus loss clears held states (consumers also receive the
Focus event; synthetic key-release events are not generated).

Existing EventBus and EventType remain. Keyboard events now provide `event.key`,
mouse-button events `event.button`, and mouse movement `dx/dy`. `event.code` for
keyboard/buttons now contains engine enum values, **not GLFW numbers**. Text input
continues to use a Unicode code point. Modifier bits: shift=1, control=2, alt=4,
super=8, caps=16, num=32. Client consumers must migrate numeric keyboard comparisons
to `KeyCode`. GLFW translation is confined to the platform source directory.

## Scene and controls

The scene creates a camera, ground, directional light, four moving point lights,
and initially 1,000 cubes through World components. One cube mesh/material is shared.
Jobs receive a detached transform snapshot, update disjoint chunks, and join before
the main thread commits transforms and synchronizes the scene. Worker count uses
the existing job-system default; `--workers N` overrides it.

| Control | Action |
|---|---|
| W/A/S/D | Camera movement |
| Q / E | Down / up |
| Hold right mouse + move | Captured mouse look |
| Left Shift | Faster movement |
| F1 / F2 | Wireframe / shadows |
| F3 / F4 | Sky / environment reflections |
| F5 | Toggle 60 FPS lock; either transition disables VSync |
| F6 | Toggle VSync independently |
| 1 / 2 / 3 / 4 | 100 / 1,000 / 5,000 / 10,000 cubes; pause automatic growth |
| Insert / Delete | Add / remove 100 cubes; pause automatic growth |
| G | Toggle automatic growth: +1,000 every five seconds, maximum 10,000 |
| Up / Down | Double / halve per-entity math iterations, 1–4,096 |
| Escape | Exit |

Entity count is cube count plus seven. Manual controls never exceed 10,000 cubes.
Automatic growth avoids catch-up bursts after a long frame. A one-second title and
console report includes FPS, frame time, entity/light counts, workers, submitted
jobs per frame, math iterations, growth, lock and VSync state. CPU timings separate
snapshot, jobs including wait, commit, scene sync and the previous render call.
The render timing includes driver waits and is not a GPU timer. Workload changes
inside an interval make the counts represent the final frame of that interval.

This measures the complete integration path, not isolated EnTT throughput. More
entities do not guarantee 100% CPU use: render submission or the GPU may limit FPS.
Use Up to increase CPU math and inspect timings in a Release build. The first
startup runs still execute the earlier Phase 1–3 checks and image captures.

## Validation

- CPU suite: `bash Scripts/ValidateIntegration.sh` after fetching dependencies.
- Existing `Scripts/Validate.bat` / `.sh` also execute input/pacing/job tests and
  integration scene lifecycle checks through Sandbox.
- Added startup integration checks: create/delete, zero cubes, automatic growth,
  scene counts, event-driven FPS/VSync transitions and cleanup of live resources.
- Keep the existing Phase 1–3 regression tests and rendered captures.

In this development environment: the standalone CPU suite passed using pinned EnTT
and the real JobSystem with one and four workers. Changed non-Ogre translation
units passed compiler syntax checks, including the GLFW Null window adapter.
Address/undefined-behavior sanitizer checks also passed with leak detection disabled;
the sandbox prevented LeakSanitizer from inspecting process threads. Leak detection
remains unverified. Full Ogre linking, rendered images, native input, resize/minimize/restore, Windows
MSVC, and actual presentation-rate behavior remain pending for this revision.

Windows acceptance:
1. Pull `prototype`, run `GenerateProjects.bat`, build Release, run from the repo root.
2. Confirm startup checks pass; title shows lock=0 and vsync=0 with 1,000 cubes.
3. Press G to hold load steady. Compare F5 locked/unlocked FPS; inspect any driver
   override if both stay exactly at refresh rate. Test F6 and unlock again with F5.
4. Test every control, focus loss while holding movement, mouse capture/release,
   resize, minimize/restore, automatic growth and close with thousands of entities.
5. Run `Scripts\Validate.bat` for Debug, Release and Dist acceptance.
