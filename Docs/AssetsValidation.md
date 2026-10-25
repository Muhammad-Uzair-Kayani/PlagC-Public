# Asset foundation validation — 2026-10-25

This verifies the first asset/animation/audio implementation described in [Assets.md](Assets.md).

| Check | Result |
|---|---|
| Actual pinned Assimp, Ozz and OpenAL source build | Passed; no vendor CMake invoked |
| Premake Linux gmake2 and Windows VS2022 project generation | Passed |
| Full Premake Linux Dist NULL build, Sandbox and AssetCooker | Passed |
| `AssetCooker --demo` | Passed; committed model and PCM fixtures regenerated |
| Asset integration tests | 33 passed |
| ASan + UBSan asset tests | 33 passed; PlagC/test code instrumented, cached vendors not instrumented; leak detection disabled |
| `Scripts/TestStressApp.sh` | 18 passed, including imported assets, reload and reset cleanup |
| `Scripts/TestStressWorld.sh` | 39 passed |
| `Scripts/ValidateIntegration.sh` | 47 passed |
| Actual Ogre NULL `--headless --self-test --workers 1` | Exit 0; 76 PASS checks |
| New asset/animation/audio modules and integration syntax check | GCC C++20, `-Wall -Wextra`, no diagnostics |
| Actual Ogre NULL benchmark with default asset demo | Exit 0; 120 recorded samples after 10 warmup frames |

The NULL benchmark retained the 1,000-entity ceiling and reported nonzero Ozz/CPU
skinning and Ogre vertex upload work. Mean asset animation time was 0.003839 ms and
mean asset upload time was 0.001485 ms on this host. These are correctness smoke-test
measurements, not GPU performance claims or a comparison with the user's laptop.

The real OpenAL loopback tests require nonzero mixed PCM, exercise playback/pause/stop,
shared buffers, stale handles and cleanup. Recording-renderer tests verify shared
static resources, per-instance animated meshes, entity destruction, repeated reload
and reset without accumulated resources. Cooked-file tests cover corruption,
truncation, validation, async deduplication, type mismatch and failed-reload retention.

The full Linux build used temporary X11 development headers because this test host
lacked them. The initial runtime failure was a missing existing environment DDS in
the restored workspace; regenerating it with `Scripts/GenerateEnvironmentFixture.py`
resolved it. No engine workaround or new binary dependency was introduced for that.

## Native acceptance still required

Windows project generation is verified, but MSVC compilation, interactive native-window
visuals and audible hardware playback have not been executed here. On Windows, run
`GenerateProjects.bat`, build the solution, keep OpenAL32.dll beside Sandbox.exe, and
start from the repository root. Verify the two textured beacons near spawn, F5 animation,
F6 positional audio, F7 reload and Shift+R reset while the existing FPS controls work.
The default headless configuration intentionally disables hardware audio.

CPU skinning is the current baseline. GPU skinning, shared compressed Ozz clip caches,
independent IDs for bundled model subresources, automatic file watching and compressed
or streaming audio remain outside this first version; see Assets.md for the full profile.
