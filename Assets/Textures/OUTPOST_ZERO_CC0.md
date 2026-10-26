# OUTPOST ZERO — CC0 PBR surface pack

The Visual Max scene uses the following Poly Haven assets. They are dedicated to the public domain under **CC0 1.0 Universal**; attribution is not required, but PlagC retains provenance and creator credit.

| Local prefix | Poly Haven asset | Creator(s) | Use |
| --- | --- | --- | --- |
| `concrete_floor_*_1k.jpg` | Concrete Floor | eye-candy.xyz | floors / concrete architecture |
| `corrugated_iron_*_1k.jpg` | Corrugated Iron | Dimitrios Savva; Jenelle van Heerden | industrial metal / roof / wall surfaces |
| `rusty_painted_metal_*_1k.jpg` | Rusty Painted Metal | Amal Kumar | painted structural metal / props |
| `asphalt_03_*_1k.jpg` | Asphalt 03 | Charlotte Baglioni; Dario Barresi | exterior yard |
| `gravel_embedded_concrete_*_1k.jpg` | Gravel Embedded Concrete | Charlotte Baglioni | rough exterior / tunnel concrete |

Maps used by PlagC: diffuse/base colour (sRGB), normal (linear), roughness (linear). Metallic response is supplied by the material factor where appropriate.

Official sources:
- https://polyhaven.com/a/concrete_floor
- https://polyhaven.com/a/corrugated_iron
- https://polyhaven.com/a/rusty_painted_metal
- https://polyhaven.com/a/asphalt_03
- https://polyhaven.com/a/gravel_embedded_concrete

License: https://creativecommons.org/publicdomain/zero/1.0/

The source files were selected as 1K maps to keep this renderer acceptance scene practical on mid-range hardware while still replacing the old generated 32x32 placeholders. `Scripts/FetchVisualAssets.py` downloads the exact files from the tracked public asset source and validates SHA-256 before installation.
