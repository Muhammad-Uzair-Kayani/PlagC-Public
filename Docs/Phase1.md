# Phase 1 — foundation delivery

Status: complete following user-reported Windows tests (window, events, resize, Escape).
The checklist below is retained as a regression procedure, not a new MSVC test claim.
Later phases remain pending; this delivery does not claim a working renderer/editor.

## Design → Develop → Test → Verify → Checklist → Update → Polish → Notify

| Step | Result |
|---|---|
| Design | Ownership, event delivery, job capacity/progress and shutdown contracts in Design.md |
| Develop | Public APIs, implementations, separate Premake vendor projects and client Sandbox |
| Test | Automated cases run inside Sandbox, including concurrency and failure scenarios |
| Verify | Debug/Release/Dist, sanitizer results, generator and bootstrap checks recorded below |
| Checklist | Completed items and platform checks separated explicitly |
| Update | README, dependency pins, design and this phase tracker maintained with implementation |
| Polish | Readable event logs, bounded buffers, isolated sanitizer outputs, RAII cleanup, build instructions |
| Notify | Deliver source repository/package and Windows verification instructions |

## Implemented checklist

- [x] Local Git repository, submodule pins, ignores and cross-platform entry scripts.
- [x] PlagC StaticLib and Sandbox ConsoleApp; Debug/Release/Dist.
- [x] Separate Premake GLFW and spdlog libraries; no CMake invocation.
- [x] Engine entry point, client factory, timing loop and shutdown ownership.
- [x] Structured logging, console/file sinks, bounded editor-ready snapshots.
- [x] Thread names/IDs, main/worker queries and affinity enforcement.
- [x] GLFW window lifecycle, resize, title, minimized state and close request.
- [x] Window, framebuffer, keyboard, Unicode text, mouse and focus/minimize events.
- [x] Immediate dispatch, queued cross-thread delivery and RAII subscriptions.
- [x] Lock-free bounded job storage/transport and per-worker work stealing.
- [x] Sleeping workers, completion groups, helping waits and failure accounting.
- [x] Submission/draining/shutdown race policy and explicit Full/Stopped results.
- [x] Sandbox exercises new systems as engine client code.

## Automated verification cases

All assertions remain enabled in Release/Dist: failures throw and main returns 1.

1. Unsubscribe during dispatch; defer newly added subscription.
2. Reentrant dispatch and callback-exception bookkeeping.
3. Four event producers; 4,000 events with per-producer order checks.
4. Event overflow, owner-thread enforcement and connection outliving bus.
5. Four job producers; 24,000 distinct jobs must each execute exactly once.
6. Nested worker submission; 2,048 child jobs and helping waits.
7. Intentional job exception and illegal self-group wait; workers survive.
8. Repeated idle-worker wake-up; forced stealing from a non-popping owner.
9. Submitted/executed counter agreement after completion.
10. Exhaust 4,096 descriptors; verify Full, drain and Stopped.
11. Race shutdown against four continuously submitting producers.
12. Concurrent worker logging and structured thread metadata.
13. GLFW Null window creation, title, resize callbacks and close flag.

The two deliberate failed jobs are expected; `failed=2` is part of the test.
Tests exercise representative schedules, not a formal lock-free correctness proof.

## Windows acceptance gate — VS2022

- [ ] Run GenerateProjects.bat from the delivered repository; both submodules download.
- [ ] Build Debug, Release and Dist for x64 with MSVC.
- [ ] Run Sandbox --headless --workers 1 and --workers 4; every check passes.
- [ ] Run native Sandbox and observe a window; resize/move/minimize/restore/focus.
- [ ] Confirm KeyPressed/Repeated/Released, TextInput, MouseMoved/Scrolled/Pressed/Released logs.
- [ ] Confirm resize/framebuffer-size events and title change.
- [ ] Close via Escape, then rerun and close using the OS close button.
- [ ] Confirm shutdown message and inspect Logs/PlagC.log.
- [ ] Confirm no VS debugger lifetime/access violation at exit.

Native X11 and macOS builds/runs are also pending. Linux Null testing does not
establish native desktop compatibility. Linux X11 source lists and macOS Cocoa
source lists/framework links are present. Initial architecture is x86-64.

## Phase tracker

| Phase | Status |
|---|---|
| 1 Foundation | Complete; Windows tests reported by user |
| 2 Renderer | In development; see Phase2.md |
| 3 World/ECS | Pending |
| 4 Physics | Pending |
| 5 Assets/animation/audio | Pending |
| 6 Editor backend | Pending |
| 7 ImGui frontend | Pending |

The wireframe's Script/Particle/Phong/advanced-animation scope decisions remain
open for their relevant future phases; they do not block the foundation.
