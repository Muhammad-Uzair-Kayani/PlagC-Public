# Last Light — Frostworks

A Windows x64 third-person tactical survival game with one player, four persistent AI survivors, and optional roaming bots. Explore an industrial Arctic base, manage supplies, restore communications, and secure an evacuation.

The C++ game is a built-in module compiled with Godot **4.5.1** source. Godot is pinned as a submodule at `vendor/godot`. The Windows package includes the compiled engine and matching game resources.

## Play immediately

[Download the Windows game ZIP](Deliverables/LastLight-Windows-x64.zip?raw=true)

1. Download `Deliverables/LastLight-Windows-x64.zip` from this repository.
2. Extract the **entire ZIP** into a writable folder.
3. Open the extracted `LastLight-Windows-x64` folder and run **`Play.bat`**.
4. Start a **new mission**. Choose **Peaceful** for your first exploration, **Standard** for five bots, or **Hostile** for eight.

Keep `LastLight.exe`, `LastLight.pck`, the launch scripts, and the bundled folders together. Players do not need to install Visual Studio, Premake, Godot, or Python.

Use `Play-With-Neural-AI.bat` to enable the optional bundled Python neural guide. Normal gameplay works without that guide.

The default **960×540 GL Compatibility** settings target integrated graphics. Use **F1** to inspect FPS; performance depends on your hardware.

## Mission

1. Recover the access key.
2. Restore generator power using fuel or hand cranking. Tools shorten the crank.
3. Calibrate any **two of the three relays**.
4. Activate the radio.
5. Activate either landing beacon with **E**. North airlift takes **22 seconds**; west airlift takes **32 seconds**.
6. Keep hostile raiders outside the landing perimeter, remain at the pad, and evacuate.

Hostiles inside the perimeter pause arrival or boarding. Radio activity can attract reinforcements. With a tool kit, secure the turbine service panel to prevent opportunistic generator sabotage.

Search caches, conserve compatible ammunition, use doors and cover, and choose between service roads, interiors, the raised gantry, and quieter perimeter routes. Smoke blocks sight; decoys create distracting noise.

## Controls

| Input | Action |
|---|---|
| WASD / Mouse | Move / look |
| Shift / Ctrl / Space / Alt | Sprint / crouch / jump / roll |
| Right mouse / Left mouse | Aim / attack |
| E / C | Interact, pick up, or revive / close a nearby door |
| R / X | Reload / cancel an action |
| 1 / 2 / 3 / 4 / 5 | Pistol / sword / SMG / rifle / shotgun |
| H / B / F / T | Medkit / bandage / eat / drink |
| Q / V | Smoke / decoy |
| G / Y | Offer compatible ammunition / invite a teammate |
| Z | Enter or leave nearby cover; A/D slide, right mouse exposes, W/S leaves |
| J / M | Inventory / tactical map |
| F2 / Mouse wheel | Toggle overhead view / zoom; right mouse aims toward the cursor |
| F3 / I | Squad orders / survivor inspector and reset |
| Tab / F10 | Spectate survivors / toggle autoplay |
| F5 / F9 | Save / load mission |
| F1 / Esc | Diagnostics / pause menu |

The live inventory, map, squad, and inspector panels **do not pause the world**. Use **Esc** to pause.

## Survivors, orders, and learning

| Survivor | Starting role |
|---|---|
| Maya | Medic |
| Rafi | Scout |
| Lian | Guard |
| Omar | Engineer |

Press **F3** to request independent planning, following, holding position, scavenging, or mission work. Orders last **45 seconds**. Voice commands require nearby sight; powered squad radio works across the base. Distrust can cause refusal. Immediate survival, treatment, and rescue can interrupt an order.

Press **I** to inspect personality/social scores, trust and fear, remembered contacts, confidence, resources, plans, tactical scores, and rejected actions. Planning uses bounded search, local evidence, personal memories, and learned action values. The optional Python neural guide and offline training are separate systems; the bundled neural weights are a historical baseline, with no claim of improved Frostworks performance.

Learning persists across missions and launches. To reset it:

- Open **I**, select a survivor, and choose **Reset this survivor's learned profile**.
- Use the main-menu reset checkbox to reset all four for a new game.

Live reset preserves health and inventory. Learning resumes afterward. Loading an older mission does not rewind persistent learning or undo a profile reset.

## Build from source

### Requirements

- Windows 10 x64 or newer.
- Git and Python 3.10+ with the `py` launcher.
- Visual Studio 2022 with **Desktop development with C++**, MSVC x64 tools, and a Windows SDK.
- Premake5 beside `GenerateProjects.bat` or available on `PATH`.

Use a short path such as `C:\Dev\LastLightGodot`. From the project folder, run:

```bat
Setup.bat
GenerateProjects.bat vs2022
```

Open `build\premake\vs2022\LastLightGodot.sln`. Select **Release | x64**, set **LastLight** as the startup project, and press **Ctrl+F5**.

Alternatively:

```bat
BuildGame.bat --jobs 1
RunGame.bat
```

Premake generates the Visual Studio frontend; its build task invokes Godot's SCons build. No CMake step is required. On an 8 GB machine, use **one compile job**. The first engine build takes substantially longer than incremental game-code builds.

`Setup.bat` obtains/registers the pinned Godot submodule and installs local build tools. When cloning through Git, use `--recurse-submodules`, or initialize an existing clone with:

```bat
git submodule update --init --recursive
```

Keep **`game/game-content.pck`** in the source checkout: it contains the selected runtime assets. When updating an existing checkout, preserve `vendor/godot` and `.tools`.

## Verify the game

For the downloaded Windows package:

```bat
Verify-Game.bat
```

It verifies package hashes and runs **41 isolated gameplay checks** using bundled Python. Results are written under `diagnostics/`, separate from personal saves.

For a built source checkout:

```bat
TestGame.bat
```

This runs unit/tool tests and custom-engine integration scenarios. Reports are written under `reports/`. The supplied Frostworks release passed native Windows MSVC/Premake acceptance, Python integration, packaged-resource checks, and the portable gameplay suite.

If a source build fails, inspect `reports/godot-build.log` and `reports/msvc-preflight.log` for the underlying compiler or SDK error.

## Publish matching updates

Commit the client sources, scripts, content, and these required files:

- `.gitmodules` and `godot.lock.json`.
- `vendor/godot` as a **submodule gitlink**.
- `game/game-content.pck`.
- `Deliverables/LastLight-Windows-x64.zip` for immediate play.

Run `Setup.bat` before the first source upload so the engine is registered as a real submodule. Keep generated build/cache files excluded using the supplied `.gitignore`.

After changing code or assets, rebuild and test, then replace the playable ZIP. To package a rebuilt `dist/`, reuse the `python-runtime` directory from the tested Windows package:

```bat
py -3 scripts\package_windows.py --dist dist --python-runtime "C:\Path\LastLight-Windows-x64\python-runtime" --output Deliverables\LastLight-Windows-x64.zip
```

Replace the example runtime path with its actual location. Extract the new ZIP and run its `Verify-Game.bat` before publishing. Larger distributions can be attached to GitHub Releases.

The playable ZIP shares the same build and assets; each player has separate local saves and learned survivor profiles.

## Saves and documentation

Personal data is stored at:

```text
%APPDATA%\Godot\app_userdata\Last Light\
```

- `save-frostworks.json`: current mission save.
- `survivors-v2.json`: persistent survivor learning; older v1 profiles migrate automatically.
- `settings.cfg`: local settings.

The previous `save.json` is preserved. Start a new mission when upgrading from an older map/save format.

See [the quick start](QUICKSTART-FROSTWORKS.txt), [implementation notes and limits](docs/frostworks-update.md), [validation records](docs/validation-frostworks.json), and [GitHub sharing guide](GITHUB-SHARING.md). Developers can select the previous Arctic map with `--world classic` or the courtyard regression world with `--world lab`.

Godot uses the MIT license. Asset-specific credits and license information are recorded in [ASSET-CREDITS.txt](game/content/ASSET-CREDITS.txt) and included in the Windows package. Keep those notices with redistributed builds.
