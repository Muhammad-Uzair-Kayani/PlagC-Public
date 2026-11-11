# NDJSON v1

One JSON object per line; stdout is protocol only. Diagnostics go to stderr.
Every request and response includes `v:1`, integer `request_id`, integer
`episode`, and a `type`. `episode` is the episode ID; agent `id` is the agent
ID. These names are canonical v1 names, matching the roadmap's examples.

The C++ transport starts a persistent Python subprocess once and batches all
active survivor observations each tick. POSIX uses fork/exec/pipes/poll; Windows
uses CreateProcess/anonymous pipes/PeekNamedPipe. No shell is invoked. Default
response timeout is 2000 ms; replies over 1 MiB are rejected. Transport failures
disable the stream to prevent late replies entering later requests. The
runtime catches failures and applies the deterministic rule controller. A
training run rejects any episode that used fallback rather than silently
scoring it as neural performance.

```json
{"v":1,"type":"infer_batch","request_id":42,"episode":7,"tick":120,"agents":[{"id":3,"features":[0.8,1,0.5,0.3,0.4,0.1,0,1,0.3,0.2,0.4,0.1,0.5,0.7,0,0.6],"valid_actions":[1,1,1,1,1,1],"presence":[1,1,1,1]}]}
{"v":1,"type":"intents","request_id":42,"episode":7,"actions":[{"id":3,"intent":"Progress","target_id":0,"policy_revision":1}]}
```

Python service commands: `hello` returns dimensions; `load_policy` accepts a
`path` or `weights` array; `infer_batch` (alias `infer`) returns masked neural
intentions; `reset_episode` and `episode_result` acknowledge lifecycle events;
`save_checkpoint` saves a model at `path`; `shutdown` replies then exits.
`evaluate_genome` on the inference service accepts agent observations for a
candidate already loaded. It does not move the world.

C++ `--server` commands: `hello`, `shutdown`, and `evaluate_genome`.
Evaluation accepts `seed`, `scenario`, `controller`, optional `weights`,
`max_ticks` and `log_dir`. It resets and runs an authoritative C++ episode,
returning `episode_result` with per-survivor metrics and fitness. The trainer
keeps this C++ process and its Python service alive across the population.

Feature lengths, finite bounds, action masks, duplicate agent IDs and model
schema are validated in Python. C++ checks version, request/episode correlation,
action count, unique known IDs, known feasible intentions and observed target
IDs. All commands are subsequently checked by gameplay rules. Errors return
structured diagnostics; no Python code has direct write access to world state.

Models contain schema_version=1, architecture=[16,16,6] and 374 finite weights:
256 input-to-hidden row-major, 16 hidden biases, 96 hidden-to-output row-major,
6 output biases. Hidden activation is tanh; outputs are linear with masked
argmax and stable first-index tie breaking. Checkpoint writes use temporary
files followed by replacement. `latest.json` additionally records population,
next generation index, RNG state, champion, history and training cases.
