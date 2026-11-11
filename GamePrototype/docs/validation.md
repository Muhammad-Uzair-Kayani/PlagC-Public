# Validation record — November 12, 2026

## Scope and build

The roadmap's engine-independent semester deliverable is implemented. M9 still
requires the unavailable PlagC engine integration. This is a synthetic flat
world with real gameplay rules, not a rendered 3D application. CMake Release
build and all three CTest suites passed on Linux with GCC 14 and Python 3.12.
Windows transport/build paths and a Linux/Windows CI matrix are supplied;
Windows has not been executed locally in this session.

## Acceptance evidence

| Roadmap gate | Evidence |
|---|---|
| T01 replay | 20 seeded scenarios compare every tick across two independent worlds |
| T02 solo | A* reaches the destination using the closed-gate bypass; shortest cost matches BFS |
| T03 peaceful | All five baseline survivors arrive alive in easy episodes |
| T04 gate | Proximity, one actor, interrupted activation fail; two distinct explicit holds succeed |
| T05 bypass | Closed gate still has a reachable independent southern route |
| T06 hostile | Seeded aggressor fires; perceived threat enables real avoid/defend commands |
| T07 perception | Witnessed assistance changes trust, occluded assistance does not; observation idempotent |
| T08 replanning | Blocked corridor reroutes; fully trapped agent reports no path |
| T09 transport | Malformed JSON, wrong version/correlation, absent/unknown/duplicate actions and timeout fall back safely |
| T10 integrity | No duplicate pickup, ammo-free damage, idle completion or kill reward; dead/arrived actors stop acting |
| T11 generalization | Six test seeds (901–906) disjoint from train (101–104) and validation (501–502) |
| T12 boundary | FakeWorldPort tests link only survival_core, without SyntheticWorld, console or Python |

Python tests also cover masked deterministic inference, model round trips,
non-finite/missized inputs, protocol recovery, elitism and deterministic breeding.
Representative authored cooperation and defensive-response events and outcomes
are in `docs/samples`. They are not presented as learned cooperation.

## Measured training and evaluation

Command: `python3 python/train.py train --runtime build/cmake/GameRuntime
--population 30 --generations 10 --seed 42 --output data/training`.
Each candidate was evaluated on the four fixed cases (1,200 training episodes).
Initial generation best fitness was 885.500; champion fitness reached 1019.725.
Population mean rose from 433.536 to 814.839. These are training results, not
an independent statistical proof of generalization.

The selected champion was evaluated once on six reserved cases (30 survivor
outcomes per controller). See `samples/comparison.csv` for complete metrics.

| Controller | Arrival alive | Median successful ticks | Episode fitness mean ± population SD |
|---|---:|---:|---:|
| Evolved neural | 86.7% | 31 | 921.10 ± 176.09 |
| Authored rule | 63.3% | 28 | 677.12 ± 413.52 |
| Random valid intention | 50.0% | 66 | 526.40 ± 539.74 |
| Neural, social inputs zeroed | 86.7% | 29.5 | 930.17 ± 174.41 |

All evaluated policy calls succeeded without fallback. Champion average health
at arrival was 95.38. It made three harmful first attacks and opened no gates.
The ablation did not reduce arrival and slightly improved fitness, so this run
does not establish a benefit from social inputs. No learned friendship,
team formation, revenge or reliable cooperative policy is claimed. Six small
maps/seeds share topology; broader randomized layouts and repeated independent
training runs are needed for stronger conclusions. Timing columns are local
measurements and vary by machine.

## Reproducibility

The small sample champion, training history, test outcomes and event examples
are versioned. Full population/checkpoint/traces remain generated data outside
source control. Regenerate them with the README commands. The sample champion
can be loaded directly using `--model docs/samples/champion.json`.

Checkpoints save the next generation population and Python RNG state. Resume
continues from that exact state, not from the champion alone. Training output
paths are independent of simulation semantics. Tests do not assert noisy
wall-clock timing or require matplotlib. Charts are optional; raw CSVs are the
measurement source of truth.

## Premake/Python launcher update

The Premake path now builds the separate core/synthetic libraries, runtime and
both C++ tests. Python is detected for Visual Studio debugger launches and can
be overridden with `--python-exe`. Legacy embedding flags are removed because
the runtime uses a persistent subprocess, not pybind11.

Verified with a clean Premake gmake2 Release build, both C++ test executables,
Python/protocol integration tests against that executable, and a neural episode
through the new launcher. A copied checkout whose path contains spaces launched
successfully from a different working directory with zero policy failures.
Generated VS2022 debugger XML was checked for correctly quoted interpreter and
asset paths. Windows execution and the batch launcher require Windows and were
not executed in this Linux session.
