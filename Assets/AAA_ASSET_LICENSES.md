# OUTPOST ZERO — High-Fidelity Asset Provenance

All third-party models acquired by `Scripts/FetchAAAWorld.py` come from **Poly Haven** and are
released under **CC0 1.0 Universal (Public Domain Dedication)**. They may be copied, modified,
distributed and used commercially without attribution. PlagC nevertheless records the source and
author metadata in `Assets/Source/AAA/provenance.json` at acquisition time.

License: https://creativecommons.org/publicdomain/zero/1.0/
Poly Haven license FAQ: https://polyhaven.com/license

| Asset | Poly Haven source | OUTPOST ZERO use |
|---|---|---|
| Old Military Crate | https://polyhaven.com/a/old_military_crate | replaces prototype supply crates |
| Portable Generator | https://polyhaven.com/a/portable_generator | replaces prototype generator props |
| Security Camera 01 | https://polyhaven.com/a/security_camera_01 | gate, tower and lab CCTV dressing |
| Modular Factory Facade | https://polyhaven.com/a/modular_factory_facade | exterior architectural silhouette |
| Modular Industrial Pipes 01 | https://polyhaven.com/a/modular_industrial_pipes_01 | warehouse, generator hall and lab pipe banks |
| Steel Frame Shelves 01 | https://polyhaven.com/a/steel_frame_shelves_01 | warehouse storage dressing |
| Exterior Aircon Unit | https://polyhaven.com/a/exterior_aircon_unit | exterior mechanical detail |
| Utility Box 01 | https://polyhaven.com/a/utility_box_01 | electrical infrastructure |
| Barrel 03 | https://polyhaven.com/a/barrel_03 | workshop/generator clutter |
| Plastic Crate 02 | https://polyhaven.com/a/plastic_crate_02 | warehouse/barracks clutter |
| Modular Airduct Rectangular 01 | https://polyhaven.com/a/modular_airduct_rectangular_01 | warehouse/workshop/lab ceiling services |

## Animation policy

No Mixamo content is redistributed by this automated pack. Adobe permits Mixamo characters and
animations in commercial projects, but its FAQ also places restrictions around redistribution of
raw character/animation files. PlagC therefore keeps its current redistributable character source
until a high-detail animation/character pack with a redistribution-compatible license is selected.
This prevents the public PlagC repository from accidentally becoming an asset redistribution source.

The acquisition script queries Poly Haven's public API at download time and writes the returned
creator metadata and `files_hash` to `provenance.json`, giving the checkout a machine-readable audit
trail in addition to this human-readable license manifest.
