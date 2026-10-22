# Phase 1 design

Workflow: Design → Develop → Test → Verify → Checklist → Update → Polish → Notify.

## Ownership and initialization

The engine's main() initializes the main-thread identity, constructs Engine,
then calls the client's CreateApplication factory. Engine owns logging, event
bus, job pool and optional window. Sandbox initializes no vendor subsystem.
On normal shutdown, accepted jobs drain before OnStop and client destruction.
An RAII drain guard also drains jobs before client destruction on exceptions.
Engine destroys jobs before its window/event bus/logger. Client-owned job data
must remain alive until completion; OnStart stack data must be waited before
unwinding. Connection objects are owned by their subscribers.

GLFW and all window queries/mutations run on the main thread. Phase 1 explicitly
supports one window. It requests GLFW_NO_API so a future Ogre backend can own
graphics initialization. Renderer-specific native-handle extraction belongs in
Phase 2. Callbacks enqueue value events and never unwind exceptions through C.
Queue overflow is reported from Poll. Events are drained before frame updates.

## Jobs and progress guarantees

JobSystem owns fixed storage for 4,096 job descriptors, a 4,096-slot unordered
MPMC injection mailbox, and a bounded 1,024-slot Chase-Lev deque per worker.
Workers push/pop their own deque; thieves steal the opposite end. Worker
submissions that exceed local deque capacity fall back to the injection mailbox.
External submissions use the mailbox. Job order is not guaranteed.

TrySubmit uses a plain function pointer + caller-owned context. There are no
per-submission allocations, callable copies or queue mutexes. Pool claims use
CAS on atomic flags; mailbox slots transfer atomic pointers; deque slots are
atomic to prevent racing speculative thief reads during index reuse. All queue
ordering is seq_cst initially. Atomic pointer, 64-bit and bool lock-freedom is
checked at compile time. No spinlock is hidden in the queue implementation.
The mailbox scan favors simplicity over performance and is not a FIFO queue.

The deque uses signed, monotonically increasing 64-bit indices; practical engine
lifetimes must stay below index overflow. Storage never resizes or reclaims while
workers run, eliminating dynamic-buffer reclamation requirements.

Lock-free describes job storage admission and queue transport. Sleeping workers
use C++20 atomic wait/notify; OS wake-up implementation, job callbacks, logging,
and shutdown are not promised lock-free. TrySubmit returns Accepted, Full or
Stopped. Full is transient: caller chooses retry, defer, or other work. Workers
must not spin on Full while occupying all workers; they can wait/help an already
submitted child group. There is no silent job loss or inline fallback execution.

A combined closing-bit/submission-count admission gate prevents shutdown racing
with a partially published job. Shutdown closes admission, waits for publishers,
drains accepted work, wakes sleepers and joins. Even worker-spawned work is
rejected once shutdown begins; callbacks must handle Stopped. No cancellation.
Shutdown must be invoked on the thread that constructed the scheduler. Submitters
must stop using the scheduler before its object is destroyed.

Each job's descriptor is released only after its function returns. Group pending
is decremented after all group error bookkeeping. Wait uses the scheduler wake
epoch, so a completing job never notifies through an already-destroyed group.
Wait may return as soon as pending reaches zero; callers must finish initial
submission before waiting. A parent job keeps its parent group pending while
it schedules child work. Context data must be synchronized by its owner.

Worker Wait executes available work rather than blocking the whole pool.
Waiting for one's own group is rejected. Other dependency cycles and arbitrary
blocking callbacks are caller errors; there is no general graph cycle detector.
Failed callbacks increment group/global failure counters; worker threads survive.

Reference for the deque algorithm family (implementation here is engine-owned):
https://github.com/taskflow/work-stealing-queue

## Event semantics

Events are values with immutable const access during dispatch. Subscribe,
unsubscribe and Dispatch run on the bus owner thread. Connection destruction on
a different thread is a contract violation and terminates; it is not silently
racy. A weak state reference permits a connection to outlive its bus.

Dispatch snapshots subscriptions, supports reentrancy and checks whether each
slot is still active. Removing a subscription suppresses later calls in the same
dispatch. New subscriptions are visible on the next dispatch (including a nested
dispatch begun after registration). Exceptions restore dispatcher bookkeeping
and propagate to the caller; an exception during Pump discards its remaining
local batch and reaches the app fatal handler.

Post is cross-thread safe and bounded to 8,192 events, with explicit false on
capacity overflow. A small mutex protects this separate event queue; event
transport is not required to be lock-free. Per-producer order is preserved;
cross-producer order follows queue acquisition order. Pump swaps a batch, so
newly posted events wait until a later Pump. Subscription order determines
synchronous delivery. There is no dedicated broker thread.

## Logging and portability

spdlog is a compiled static dependency with console and rotating-file sinks.
An engine-owned bounded record buffer exposes sequence, epoch-millisecond timestamp,
severity, category, thread name/ID and message. Thread names are engine metadata,
not currently native debugger thread names. Clear affects the record buffer only.
Debug enables all levels; Release suppresses Trace; Dist suppresses Trace/Debug.
RAII and explicit references avoid static/global logger lifetime dependencies.

GLFW source/platform lists are maintained in Build/Dependencies.lua. Vendor
CMake scripts are never invoked. Premake defines all configurations; generated
solutions/makefiles are disposable. Submodule commits, not moving tags, pin builds.
Official GLFW manual-build reference: https://www.glfw.org/docs/3.4/compile.html
