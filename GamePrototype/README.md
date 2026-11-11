# Adaptive Social Survival TPS

A deterministic C++20 survival simulation with five independent survivors,
local social memory, navigation, combat, cooperative obstacles, a persistent
Python neural-policy service and genetic training. This implements the console
semester deliverable; PlagC graphics/physics integration is a later milestone.

## Build and verify

Requires a C++20 compiler, CMake >=3.20 and Python >=3.10. The simulation and
trainer require no pip dependencies. Optional `matplotlib` generates PNG plots.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
ctest --test-dir build -C Release --output-on-failure
./build/GameRuntime
```

Windows multi-configuration builds use `build/Release/GameRuntime.exe`.
`GenerateProjects.bat vs2022`/`vs2026` also builds the console and persistent
transport through Premake; Python embedding flags are legacy and unnecessary.
If CMake is unavailable with GCC/Clang, `python3 scripts/build.py` builds the
runtime and both C++ test binaries. Then run `build/core_tests`,
`build/scenario_tests`, and `python3 -m unittest discover -s python/tests -v`.
Set `SURVIVAL_RUNTIME` to the executable path when testing another build.

## Play and demonstrate

Actor 1 is human; actors 2–5 use separate observations and memories. WASD moves;
E interacts (stand directly on a control to activate it); F followed by an actor
ID fires; I inspects local observations; M shows the map; L shows recent events;
Enter/Space waits; P advances one autopilot tick; R resets; Q quits.
`*` is loot, `=` a control, G the closed gate, X the destination, B a distractor.
All commands are turn-stepped; there is no real-time input dependency.

```sh
./build/GameRuntime --scenario hostile --seed 103
./build/GameRuntime --headless --controller rule --seed 101 --log-dir data/peaceful
./build/GameRuntime --headless --controller cooperate --seed 102 --log-dir data/cooperation
./build/GameRuntime --headless --scenario hostile --seed 103 --log-dir data/defense
./build/GameRuntime --policy-service python/service.py --model data/training/champion.json
```

Add `--python python` on Windows or supply the exact Python executable path.
`--help` lists options. Four named scenarios are peaceful, cooperative, hostile
and distractors. Cooperative uses the same neutral world and permits learned
choice; `--controller cooperate` forces a reproducible cooperation demonstration.
Rule policy usually uses the longer bypass. Both are authored baselines.

## Train, resume, evaluate

```sh
python3 python/train.py train --runtime build/GameRuntime --population 30 --generations 10 --seed 42 --output data/training
python3 python/train.py train --runtime build/GameRuntime --resume data/training/latest.json --generations 5 --output data/training
python3 python/train.py evaluate --runtime build/GameRuntime --model data/training/champion.json --output data/evaluation
./build/GameRuntime --headless --controller neural --policy-service python/service.py --model data/training/champion.json
```

Training uses four fixed training cases per genome, tournament selection,
uniform crossover, Gaussian mutation and two elites. The 16-16-6 MLP has 374
weights/biases. All five survivors share candidate weights but keep independent
memory. Checkpoints preserve RNG and the next population for reproducible
resume. Validation seeds and held-out test seeds are disjoint from training.
Do not use repeated held-out evaluation to choose a champion.

Outputs include episode/generation CSVs, champion and resumable population JSON,
validation results, optional fitness PNG, held-out comparison CSV and per-tick
traces/events. Evaluation compares neural, rule, random and social-feature
ablation (indices 4,5,14,15 zeroed; feasibility rules remain). It reports arrival,
completion time, health, first attacks, damage, gate openings, path cost,
resource use, runtime and fitness variability. A gate opening is a cooperation
metric, not proof of learned friendship. No kill reward is used. See
`docs/validation.md` for measured results and limitations of the included run.

## Contracts

See [simulation/schema](docs/contract.md), [NDJSON and checkpoints](docs/protocol.md)
and [architecture/PlagC handoff](docs/architecture.md). C++ controls world state
and simulated time; Python only selects high-level intentions. Invalid policy
replies/timeouts use rule fallback during play and invalidate training scores.
Advanced social behavior, continuous 3D terrain, animation and PlagC are deferred.
