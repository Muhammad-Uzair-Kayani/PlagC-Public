# Architecture and integration

`survival_core` owns rules, observation building, memory, search, intent
resolution and scoring. `survival_synthetic` owns initial synthetic state.
`GameRuntime` owns console rendering, NDJSON evaluation server and Python
transport. Python owns neural inference, neuroevolution and analysis. The
vendored nlohmann JSON 3.11.3 header (MIT license included) is used only by the
runtime, never the gameplay library.

Build and run instructions are in the root README. Core tests link only
`survival_core` and use FakeWorldPort; synthetic code, Python and the console
are absent from that executable. Scenario tests separately exercise seeded
synthetic worlds. CTest includes Python and adversarial transport integration.

## Future PlagC boundary (M9)

Implement IWorldPort using engine entity poses and Jolt collision queries.
Keep IDs and meter/second coordinates stable. Feed canonical gameplay state
through the shared rules and translate validated movement/stance intents to
the engine character controller/UAL animation system. Preserve local sensing,
interaction, ammo and goal semantics. Do not put engine classes in DTOs or
duplicate inventory, trust or scoring in rendering scripts. Run equivalent
scenarios with the new adapter and reevaluate/retrain the champion; identical
interfaces do not guarantee identical behavior under continuous physics.

No PlagC API or library is supplied in this repository, so an engine adapter
cannot be compiled or parity-tested here. Full rendering, real physics,
advanced bonding/revenge/betrayal and animation remain the deferred milestones
identified by the roadmap. This deliverable is a flat-terrain simulation in 3D
coordinates with an ASCII projection.
