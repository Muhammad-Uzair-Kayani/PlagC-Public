#!/usr/bin/env python3
"""Fetch the authored CC0 PBR maps from an immutable revision and SHA-256 lock."""

from pathlib import Path
from urllib.request import Request, urlopen
import argparse, hashlib, json, os, tempfile

ROOT = Path(__file__).resolve().parents[1]


def fetch(entry, out, offline=False):
    target = out / entry["path"]

    def valid(path):
        return (
            path.is_file()
            and path.stat().st_size == entry["size"]
            and hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]
        )

    if valid(target):
        return
    if offline:
        raise ValueError("missing/corrupt visual texture: " + str(target))
    target.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=target.parent, suffix=".part")
    try:
        req = Request(entry["url"], headers={"User-Agent": "PlagC-AssetPipeline/3.0"})
        with os.fdopen(fd, "wb") as dst, urlopen(req, timeout=120) as src:
            while chunk := src.read(1024 * 1024):
                dst.write(chunk)
        if not valid(Path(tmp)):
            raise ValueError("visual texture checksum mismatch: " + entry["path"])
        os.replace(tmp, target)
    finally:
        Path(tmp).unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    lock = json.loads((ROOT / "Assets/Manifests/VisualTextures.lock.json").read_text())
    for entry in lock["files"]:
        fetch(entry, ROOT / "Assets/Textures", args.offline)
    print("Verified", len(lock["files"]), "pinned CC0 PBR textures")


if __name__ == "__main__":
    main()
