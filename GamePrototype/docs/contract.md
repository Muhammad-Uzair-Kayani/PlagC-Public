# Simulation contract v1

The authoritative state is C++20 plain values in `survival/simulation.hpp`.
Coordinates are integer meters `(x,y,z)`; y=0 on the initial flat terrain.
The synthetic step is one simulated second, independent of wall time. IDs 1–5
are survivors, 10–12 scripted bots, 101–105 loot, 201–202 controls. IDs are
unique within an episode. The map topology and destination are known; actors,
loot availability and events are locally sensed. Co-location is allowed (no
actor body collision); solid cells and the closed gate block movement and LOS.

`IWorldPort` returns value snapshots, resets seeded episodes, and accepts command
batches. `Simulation` observes each survivor independently, resolves high-level
intentions, applies scripted NPC policies and submits one batch. Gameplay is
in `stepGameplay`, shared with the fake adapter tests. It has no renderer,
transport, JSON or engine dependencies. `survival_synthetic` supplies seeded
initial conditions and delegates stepping to this kernel. The original
`GameSession` API remains for source compatibility with the initial demo.

Movement is cardinal, one meter per step (two with sprint; crouch suppresses
sprint), checked at each intermediate cell. Cancel discards an actor's command.
Duplicate actor commands are all discarded. Pickup range is one meter with LOS;
one object can be consumed only once. Weapons supply six rounds, ammo eight,
medkits restore up to 40 health. An offer transfers one round to a reachable
living recipient. Fire requires a weapon, a round, an active non-self target,
LOS and range <=6. It consumes one round for 25 damage. Damage is accumulated
then applied after command processing. Actors act in stable snapshot order;
loot ties therefore go to the earlier actor. No damage RNG is used. Death and
arrival prevent future actions. Attacks against neutral survivors are penalized;
retaliation against a recorded attacker is not a harmful first attack.

Two distinct living survivors must stand on the two controls and explicitly
interact in each of two consecutive steps. Proximity alone does nothing. A
successful gate latches open; leaving early resets the hold. The southern route
always permits independent completion. Cooperate attempts controls until tick
35, then progresses via the bypass. AI is never given a permanent ally label.

Vision has Manhattan range 7 plus grid ray LOS. Shots/noise are heard within
8 meters through walls; hearing alone does not reveal exact actor position or
prove unseen assistance. Help and offers add 0.3 trust when witnessed, aiming
at the observer adds 0.2 threat, attacks add 0.5 threat. Beliefs clamp to [0,1].
Actor `lastSeen` makes position freshness explicit; observations use currently
visible relationships. Loot expires from memory after 20 steps; events after
30. Repeated observation of the same event is idempotent.

## Neural inputs

All 16 values are finite and clamped/constructed in [0,1]. Network order:

| Index | Feature |
|---|---|
| 0 | health / 100 |
| 1 | ammo / 20, clamped |
| 2 | weapon present |
| 3 | 1 - nearest visible loot distance / 30; absent = 0 |
| 4 | maximum visible actor threat |
| 5 | maximum visible actor trust |
| 6 | 1 - known goal Manhattan distance / 30 |
| 7 | (goal x - actor x + 19) / 38 |
| 8 | (goal z - actor z + 13) / 26 |
| 9 | gate open |
| 10 | 1 - control-center distance / 30 |
| 11 | visible survivor count / 4 |
| 12 | elapsed tick / episode timeout |
| 13 | pickups / 5 |
| 14 | recent audible/visible event present |
| 15 | remembered loot count / 5 |

Presence flags separately encode loot, other actor, threat and usable gate.
Action order is Explore, Acquire, Cooperate, Avoid, Defend, Progress. Acquire
requires visible loot; Cooperate requires a closed gate on the near side before
tick 35; Avoid requires visible evidence of threat; Defend also requires ammo
and a weapon. Explore and Progress remain feasible. Infeasible intentions and
unknown/unobserved targets cannot bypass command validation.

A* uses Manhattan heuristic, unit costs, deterministic N/E/S/W tie ordering;
BFS, DFS and greedy modes are independently selectable via `navigate`.
Recomputation each tick handles a changed blocked corridor. No path returns
an empty path and produces a safe wait.

Fitness per survivor = 1000 for arrival + 50 if alive + 0.5*health
- 0.5*elapsed/completion ticks - 100*harmful first attacks. Kills, pickups,
cooperation events and idle waiting award no direct fitness. Episode fitness
is the mean of five survivors. Timeout or all survivors finished/dead ends the
match; distractors do not keep an episode alive.

Seeded initialization uses standard mt19937 outputs with explicit modulo
mapping, avoiding implementation-dependent distribution behavior. Actor spawns,
loot placements and goal positions vary with seed; map topology stays fixed.
The Python GA uses its own saved `random.Random` state. Replays require the same
software/schema, seed, controller/model and human command sequence.
