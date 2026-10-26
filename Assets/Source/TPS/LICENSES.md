# TPS source assets

The original GLBs are preserved. `manifest.json` records the source repository,
immutable commit, source path and SHA-256 of each file.

* Mannequin and humanoid animation clips: KayKit by Kay Lousberg, CC0.
  https://kaylousberg.itch.io/kaykit-character-animations
  Retrieved from the NoLimitNexus/game-models mirror.
* Rifle and crate: Kenney Blaster Kit, CC0.
  https://kenney.nl/assets/blaster-kit
* Generator: Kenney Space Kit, CC0.
  https://kenney.nl/assets/space-kit
  Kenney assets retrieved from the Hidencod/tge-assets mirror.

CC0: https://creativecommons.org/publicdomain/zero/1.0/

`Scripts/PrepareTPSAssets.py` combines selected same-rig clips, replaces the
mannequin's body material with a navy uniform and attaches the rifle to the right
hand. It does not modify the original source files. This is a stylized humanoid,
not a production military character. Runtime facility geometry, procedural
material textures and synthesized audio are original PlagC project content.
