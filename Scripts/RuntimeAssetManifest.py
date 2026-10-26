"""Shared runtime preflight. Presence alone is not a successful asset build."""

import hashlib, json
from pathlib import Path

BASE = [
    "Assets/Cooked/TPS/Operative.plasset",
    "Assets/Cooked/TPS/Crate.plasset",
    "Assets/Cooked/TPS/Generator.plasset",
    "Assets/Textures/PlagCEnvironment.dds",
    "Assets/Shaders/PlagCSky.material",
    "Assets/Shaders/PlagCSky_ps.glsl",
    "Assets/Shaders/PlagCSky_vs.glsl",
    "Assets/Shaders/PlagCToneMap.material",
    "Assets/Shaders/PlagCToneMap_ps.glsl",
    "Assets/Shaders/PlagCToneMap_vs.glsl",
    "Assets/Compositors/PlagC.compositor",
]
RECEIPT = Path("Assets/Cooked/TPS/AAA/build-receipt.json")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(root, profile="visual", selected=None):
    root = Path(root)
    missing = []
    for name in BASE:
        if not (root / name).is_file():
            missing.append(name)
    if missing:
        raise ValueError("Missing runtime files:\n  " + "\n  ".join(missing))
    if profile == "base":
        return
    lock = root / "Assets/Manifests/AAAWorld.lock.json"
    assets = json.loads(lock.read_text())["assets"]
    if profile == "inspection":
        if not selected:
            raise ValueError("inspection staging requires --asset ID")
        known = {a["slug"] for a in assets}
        if set(selected) - known:
            raise ValueError("unknown inspection asset")
        assets = [a for a in assets if a["slug"] in selected]
    if not (root / RECEIPT).is_file():
        raise ValueError(
            "No validated AAA build receipt. Run BuildAAAAssets.bat before building Sandbox."
        )
    receipt = json.loads((root / RECEIPT).read_text())
    if receipt["lock_sha256"] != digest(lock):
        raise ValueError(
            "asset build receipt uses a different source lock; recook assets"
        )
    for asset in assets:
        path = root / asset["output"]
        entry = receipt["models"].get(asset["slug"])
        if (
            not entry
            or entry["output"] != asset["output"]
            or not path.is_file()
            or digest(path) != entry["sha256"]
        ):
            missing.append(
                asset["output"] + " (missing, changed or not validated by AssetCooker)"
            )
    if profile == "visual":
        textures = json.loads(
            (root / "Assets/Manifests/VisualTextures.lock.json").read_text()
        )
        for entry in textures["files"]:
            path = root / "Assets/Textures" / entry["path"]
            if not path.is_file() or digest(path) != entry["sha256"]:
                missing.append(str(path))
    if missing:
        raise ValueError(
            "Runtime preflight failed:\n  "
            + "\n  ".join(missing)
            + "\nRun BuildAAAAssets.bat before building Sandbox."
        )
