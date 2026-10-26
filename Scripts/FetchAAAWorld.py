#!/usr/bin/env python3
"""Fetch pinned Poly Haven glTF packages without flattening their dependencies.
Normal fetches use the checked-in lock, never mutable API metadata. Updating that
lock is an explicit maintainer operation. Source downloads remain out of git.
"""

from __future__ import annotations
import argparse, hashlib, json, os, pathlib, sys, tempfile, urllib.parse, urllib.request
from concurrent.futures import ThreadPoolExecutor
from AAAAssetCatalog import ASSETS, NODE_RECIPES

ROOT = pathlib.Path(__file__).resolve().parents[1]
LOCK = ROOT / "Assets/Manifests/AAAWorld.lock.json"
UA = "PlagC-AssetPipeline/3.0 (Powered by Poly Haven)"
API = "https://api.polyhaven.com"


def request(url):
    return urllib.request.urlopen(
        urllib.request.Request(url, headers={"User-Agent": UA}), timeout=120
    )


def get_json(url):
    with request(url) as stream:
        return json.load(stream)


def safe_relative(value):
    p = pathlib.PurePosixPath(value)
    if not value or "\\" in value or p.is_absolute() or ".." in p.parts or ":" in value:
        raise ValueError("unsafe package path: " + value)
    return p


def package(slug, output, metadata, resolution):
    try:
        primary = metadata["gltf"][resolution]["gltf"]
    except KeyError:
        raise ValueError(f"{slug}: no {resolution} glTF package") from None
    url = primary["url"]
    main = pathlib.PurePosixPath(urllib.parse.urlsplit(url).path).name
    files = [dict(path=main, url=url, size=primary["size"], md5=primary["md5"])]
    # API include keys are glTF-relative paths, NOT just URL basenames.
    for relative, item in sorted(primary.get("include", {}).items()):
        safe_relative(relative)
        files.append(
            dict(path=relative, url=item["url"], size=item["size"], md5=item["md5"])
        )
    if len({x["path"] for x in files}) != len(files):
        raise ValueError("duplicate dependency path")
    return dict(
        slug=slug,
        output=output,
        source="https://polyhaven.com/a/" + slug,
        license="CC0-1.0",
        resolution=resolution,
        primary=main,
        files=files,
    )


def matches(path, entry):
    if not path.is_file() or path.stat().st_size != entry["size"]:
        return False
    data = path.read_bytes()
    if "sha256" in entry:
        return hashlib.sha256(data).hexdigest() == entry["sha256"]
    return hashlib.md5(data).hexdigest() == entry["md5"]


def download(entry, destination):
    if matches(destination, entry):
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(
        prefix=destination.name + ".", suffix=".part", dir=destination.parent
    )
    try:
        with os.fdopen(fd, "wb") as out, request(entry["url"]) as stream:
            while chunk := stream.read(1024 * 1024):
                out.write(chunk)
        if not matches(pathlib.Path(tmp), entry):
            raise ValueError("size/checksum mismatch: " + entry["path"])
        os.replace(tmp, destination)
    finally:
        pathlib.Path(tmp).unlink(missing_ok=True)


def validate_gltf(directory, primary, files):
    doc = json.loads((directory / primary).read_text(encoding="utf-8"))
    allowed = {f["path"] for f in files}
    for item in doc.get("buffers", []) + doc.get("images", []):
        uri = item.get("uri", "")
        if not uri or uri.startswith("data:"):
            continue
        relative = urllib.parse.unquote(uri)
        safe_relative(relative)
        if relative not in allowed:
            raise ValueError("unlocked glTF dependency: " + relative)
        if not (directory / relative).is_file():
            raise ValueError("missing glTF dependency: " + relative)


def fetch_asset(asset, source, offline=False):
    directory = source / asset["slug"]
    for entry in asset["files"]:
        relative = safe_relative(entry["path"])
        target = directory / pathlib.Path(*relative.parts)
        if offline:
            if not matches(target, entry):
                raise ValueError("missing/corrupt cached dependency: " + str(target))
        else:
            download(entry, target)
    validate_gltf(directory, asset["primary"], asset["files"])
    return directory / asset["primary"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--asset", action="append", help="fetch only this Poly Haven ID (repeatable)"
    )
    parser.add_argument(
        "--jobs", type=int, default=3, help="bounded concurrent package downloads (1-8)"
    )
    parser.add_argument(
        "--offline", action="store_true", help="verify cache without network"
    )
    parser.add_argument(
        "--refresh-lock",
        action="store_true",
        help="explicitly resolve a new complete source lock",
    )
    parser.add_argument(
        "--resolution",
        choices=["1k", "2k"],
        default="1k",
        help="used only with --refresh-lock",
    )
    args = parser.parse_args()
    if args.refresh_lock:
        assets = []
        for slug, output in ASSETS:
            print("[LOCK]", slug, flush=True)
            asset = package(
                slug, output, get_json(API + "/files/" + slug), args.resolution
            )
            asset.update(NODE_RECIPES.get(slug, {}))
            assets.append(asset)
        # Publish a refreshed lock only after complete packages have been fetched and hashed.
        for asset in assets:
            primary = fetch_asset(asset, ROOT / "Assets/Source/AAA")
            for entry in asset["files"]:
                entry["sha256"] = hashlib.sha256(
                    (primary.parent / entry["path"]).read_bytes()
                ).hexdigest()
        LOCK.parent.mkdir(parents=True, exist_ok=True)
        LOCK.write_text(
            json.dumps(dict(version=1, provider="Poly Haven", assets=assets), indent=2)
            + "\n"
        )
    lock = json.loads(LOCK.read_text())
    selected = set(args.asset or [a["slug"] for a in lock["assets"]])
    known = {a["slug"] for a in lock["assets"]}
    if selected - known:
        parser.error("unknown asset IDs: " + ", ".join(sorted(selected - known)))
    source = ROOT / "Assets/Source/AAA"
    source.mkdir(parents=True, exist_ok=True)
    cook = []
    receipt = []
    if not 1 <= args.jobs <= 8:
        parser.error("--jobs must be between 1 and 8")
    assets = [a for a in lock["assets"] if a["slug"] in selected]

    def fetch_one(asset):
        print("[FETCH]", asset["slug"], flush=True)
        primary = fetch_asset(asset, source, args.offline)
        return primary, dict(
            slug=asset["slug"],
            files=[
                dict(
                    path=f["path"],
                    sha256=hashlib.sha256(
                        (primary.parent / f["path"]).read_bytes()
                    ).hexdigest(),
                )
                for f in asset["files"]
            ],
        )

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for asset, (primary, evidence) in zip(assets, pool.map(fetch_one, assets)):
            cook.append(
                str(primary.relative_to(ROOT)).replace("\\", "/")
                + "|"
                + asset["output"]
            )
            receipt.append(evidence)
    # The manifest describes exactly this invocation; failed fetches never publish a partial list.
    (source / "cook_manifest.txt").write_text("\n".join(cook) + "\n", encoding="utf-8")
    (source / "verified-source-receipt.json").write_text(
        json.dumps(receipt, indent=2) + "\n"
    )
    print(f"Verified {len(cook)} complete glTF packages")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("AAA fetch failed:", error, file=sys.stderr)
        sys.exit(1)
