#!/usr/bin/env python3
"""Fetch the CC0 Poly Haven PBR maps used by OUTPOST ZERO Visual Max.

Assets remain individually licensed CC0 1.0; see Assets/Textures/OUTPOST_ZERO_CC0.md.
The source mirror contains unmodified 1K Poly Haven downloads plus provenance.
"""
from pathlib import Path
from urllib.request import Request, urlopen
import shutil

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Assets" / "Textures"
BASE = "https://raw.githubusercontent.com/AetherRadar/operation-steel-tide/main/assets/textures/"
FILES = [
    "asphalt_03_diff_1k.jpg", "asphalt_03_normal_1k.jpg", "asphalt_03_rough_1k.jpg",
    "concrete_floor_diff_1k.jpg", "concrete_floor_normal_1k.jpg", "concrete_floor_rough_1k.jpg",
    "corrugated_iron_diff_1k.jpg", "corrugated_iron_normal_1k.jpg", "corrugated_iron_rough_1k.jpg",
    "gravel_embedded_concrete_diff_1k.jpg", "gravel_embedded_concrete_normal_1k.jpg",
    "gravel_embedded_concrete_rough_1k.jpg",
    "rusty_painted_metal_diff_1k.jpg", "rusty_painted_metal_normal_1k.jpg",
    "rusty_painted_metal_rough_1k.jpg",
]

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name in FILES:
        target = OUT / name
        if target.is_file() and target.stat().st_size > 4096:
            continue
        print(f"[Visual Assets] {name}")
        req = Request(BASE + name, headers={"User-Agent": "PlagC-AssetFetcher/1.0"})
        with urlopen(req, timeout=90) as src, target.open("wb") as dst:
            shutil.copyfileobj(src, dst)
        if target.stat().st_size <= 4096:
            target.unlink(missing_ok=True)
            raise RuntimeError(f"invalid asset download: {name}")
    print(f"[Visual Assets] ready: {len(FILES)} CC0 PBR maps")

if __name__ == "__main__":
    main()
