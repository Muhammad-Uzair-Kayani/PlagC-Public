Current acquisition pins and static-part recipes are in `Assets/Manifests/AAAWorld.lock.json`; the build verifies source SHA-256 hashes. Original source packages remain unchanged. See `Docs/AssetStabilization.md` for the current workflow.

# OUTPOST ZERO — Cinematic Asset Provenance

All third-party environment models acquired by `Scripts/FetchAAAWorld.py` come from **Poly Haven** and are released under **CC0 1.0 Universal (Public Domain Dedication)**. They may be copied, modified, distributed and used commercially without attribution. PlagC nevertheless records the API-returned creator metadata, source URL and `files_hash` in `Assets/Source/AAA/provenance.json` at acquisition time.

License: https://creativecommons.org/publicdomain/zero/1.0/
Poly Haven license: https://polyhaven.com/license

## Environment library

| Asset slug | OUTPOST ZERO use |
|---|---|
| old_military_crate | high-detail gameplay supply crates |
| portable_generator | hero generator machinery |
| security_camera_01 / security_camera_02 | gate, tower, control and lab surveillance |
| modular_factory_facade | authored exterior architecture/silhouette |
| modular_industrial_pipes_01 / modular_pipes | warehouse, generator and lab service networks |
| steel_frame_shelves_01 | warehouse storage |
| exterior_aircon_unit | exterior mechanical dressing |
| utility_box_01 | electrical infrastructure |
| barrel_03 / plastic_crate_02 / metal_jerrycan | layered logistics clutter |
| modular_airduct_rectangular_01 / modular_airduct_circular_01 | HVAC ceiling networks |
| modular_electric_cables | cable/service layers |
| modular_chainlink_fence / concrete_road_barrier | perimeter/security composition |
| industrial_wall_lamp / industrial_caged_sconce / mounted_fluorescent_lights | visible practical fixtures paired with PlagC lights |
| fire_alarm | safety/detail dressing |
| metal_toolbox / hand_truck | workshop storytelling props |

Every source page is `https://polyhaven.com/a/<asset slug>` and the acquisition script records that exact URL. The script prefers the 2K glTF package when available, then 1K, balancing cinematic material detail against the current target GPU budget.

## Character/animation boundary

The environment is now aggressively upgraded, but the project must not label third-party character animation as integrated until PlagC has a correct skeleton-retargeting path. Quaternius' Universal Animation Library is CC0 and is the selected candidate for the future locomotion/combat library, but cross-skeleton clips are not silently attached to the current Operative. Correct completion requires source-to-target bone mapping, rest/bind-space conversion, retargeted translation policy and cooked validation before those clips enter gameplay.

This distinction keeps the public repository legally clean and prevents a visually impressive but technically invalid animation setup from being called complete.
