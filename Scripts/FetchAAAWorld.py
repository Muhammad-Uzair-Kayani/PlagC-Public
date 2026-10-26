#!/usr/bin/env python3
"""Fetch high-fidelity CC0 Poly Haven assets for OUTPOST ZERO.

Uses Poly Haven's public API rather than hard-coded CDN URLs. Model packages are kept
out of git and can be cooked into PlagC assets by BuildAAAAssets.bat.
"""
from __future__ import annotations
import hashlib, json, os, pathlib, sys, urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Assets" / "Source" / "AAA"
MANIFEST = SOURCE / "cook_manifest.txt"
API = "https://api.polyhaven.com"
UA = "PlagC-AssetPipeline/1.0 (Powered by Poly Haven)"

# slug, PlagC cooked target. The first two deliberately replace the prototype props
# already used by OUTPOST ZERO, so the existing world immediately benefits.
ASSETS = [
    ("old_military_crate", "Assets/Cooked/TPS/Crate.plasset"),
    ("portable_generator", "Assets/Cooked/TPS/Generator.plasset"),
    ("security_camera_01", "Assets/Cooked/TPS/AAA/SecurityCamera.plasset"),
    ("security_light", "Assets/Cooked/TPS/AAA/SecurityLight.plasset"),
    ("modular_factory_facade", "Assets/Cooked/TPS/AAA/FactoryFacade.plasset"),
    ("modular_industrial_pipes_01", "Assets/Cooked/TPS/AAA/IndustrialPipes.plasset"),
    ("old_military_compressor", "Assets/Cooked/TPS/AAA/MilitaryCompressor.plasset"),
    ("portable_welding_cart", "Assets/Cooked/TPS/AAA/WeldingCart.plasset"),
    ("mounted_fluorescent_lights", "Assets/Cooked/TPS/AAA/FluorescentLights.plasset"),
    ("steel_frame_shelves_01", "Assets/Cooked/TPS/AAA/SteelShelves.plasset"),
]

def get_json(url: str):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)

def file_nodes(node, keys=()):
    if isinstance(node, dict):
        if isinstance(node.get("url"), str):
            yield keys, node
        for k, v in node.items():
            if k not in {"url", "md5", "size"}:
                yield from file_nodes(v, keys + (str(k),))
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from file_nodes(v, keys + (str(i),))

def choose_model(files):
    candidates = []
    for keys, node in file_nodes(files):
        url = node["url"].split("?", 1)[0]
        ext = pathlib.PurePosixPath(url).suffix.lower()
        if ext not in {".gltf", ".glb"}:
            continue
        joined = "/".join(keys).lower()
        # Prefer 1K glTF: detailed enough for the sandbox while keeping VRAM/repo cache sane.
        score = (100 if "1k" in joined else 0) + (20 if ext == ".gltf" else 10)
        candidates.append((score, keys, node))
    if not candidates:
        raise RuntimeError("Poly Haven asset has no glTF/GLB variant")
    return max(candidates, key=lambda x: x[0])[1:]

def download(url: str, dst: pathlib.Path, md5: str | None = None):
    if dst.exists() and md5:
        if hashlib.md5(dst.read_bytes()).hexdigest().lower() == md5.lower():
            print("  cached", dst.name); return
    dst.parent.mkdir(parents=True, exist_ok=True)
    print("  download", dst.name)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=180) as r, open(dst, "wb") as f:
        while True:
            block = r.read(1024 * 1024)
            if not block: break
            f.write(block)
    if md5 and hashlib.md5(dst.read_bytes()).hexdigest().lower() != md5.lower():
        dst.unlink(missing_ok=True)
        raise RuntimeError("checksum mismatch for " + dst.name)

def main():
    SOURCE.mkdir(parents=True, exist_ok=True)
    cook = []
    evidence = []
    for slug, cooked in ASSETS:
        print("[AAA]", slug)
        info = get_json(f"{API}/info/{slug}")
        files = get_json(f"{API}/files/{slug}")
        keys, primary = choose_model(files)
        package = SOURCE / slug
        # Download every URL in the selected resolution/format branch when possible.
        branch = files
        try:
            for key in keys[:-1]: branch = branch[key]
        except (KeyError, TypeError): branch = primary
        nodes = list(file_nodes(branch)) or [(keys, primary)]
        primary_path = None
        for _, node in nodes:
            url = node["url"]
            name = pathlib.PurePosixPath(url.split("?", 1)[0]).name
            dst = package / name
            download(url, dst, node.get("md5"))
            if node is primary or url == primary["url"]: primary_path = dst
        if primary_path is None:
            url = primary["url"]
            primary_path = package / pathlib.PurePosixPath(url.split("?", 1)[0]).name
            download(url, primary_path, primary.get("md5"))
        cook.append(f"{primary_path.relative_to(ROOT)}|{cooked}")
        evidence.append({"slug": slug, "name": info.get("name", slug), "authors": info.get("authors", {}),
                         "source": f"https://polyhaven.com/a/{slug}", "license": "CC0 1.0 Universal",
                         "files_hash": info.get("files_hash", "")})
    MANIFEST.write_text("\n".join(cook) + "\n", encoding="utf-8")
    (SOURCE / "provenance.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    print(f"Prepared {len(cook)} high-fidelity CC0 model packages.")

if __name__ == "__main__":
    try: main()
    except Exception as exc:
        print("AAA asset fetch failed:", exc, file=sys.stderr); sys.exit(1)
