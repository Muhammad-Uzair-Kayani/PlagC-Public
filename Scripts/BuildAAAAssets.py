#!/usr/bin/env python3
"""Fetch, cook, validate and optionally stage a pinned set; every failure is fatal."""

from __future__ import annotations
import argparse, json, os, pathlib, subprocess, sys, tempfile
import FetchAAAWorld as fetch
from RuntimeAssetManifest import RECEIPT, digest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cooker", required=True, type=pathlib.Path)
    parser.add_argument("--asset", action="append")
    parser.add_argument("--offline", action="store_true")
    parser.add_argument(
        "--output",
        action="append",
        type=pathlib.Path,
        help="stage to this build folder (repeatable)",
    )
    args = parser.parse_args()
    cooker = args.cooker.resolve()
    if not cooker.is_file():
        parser.error("AssetCooker not found: " + str(cooker))
    command = [sys.executable, str(ROOT / "Scripts/FetchAAAWorld.py")]
    for slug in args.asset or []:
        command += ["--asset", slug]
    if args.offline:
        command += ["--offline"]
    subprocess.run(command, cwd=ROOT, check=True)
    lock = ROOT / "Assets/Manifests/AAAWorld.lock.json"
    receipt_path = ROOT / RECEIPT
    receipt = {"lock_sha256": digest(lock), "models": {}}
    if receipt_path.is_file():
        try:
            previous = json.loads(receipt_path.read_text())
        except (OSError, ValueError):
            previous = {}
        if previous.get("lock_sha256") == receipt["lock_sha256"]:
            receipt = previous
    catalog = {a["output"]: a for a in json.loads(lock.read_text())["assets"]}
    count = 0
    selected = set(args.asset or [a["slug"] for a in catalog.values()])
    for asset in catalog.values():
        if asset["slug"] not in selected:
            continue
        source = "Assets/Source/AAA/" + asset["slug"] + "/" + asset["primary"]
        dest = asset["output"]
        out = ROOT / dest
        out.parent.mkdir(parents=True, exist_ok=True)
        temporary = out.with_suffix(".plasset.part")
        try:
            command = [str(cooker), str(ROOT / source), str(temporary)]
            for name in asset.get(
                "nodes", ([asset["node"]] if asset.get("node") else [])
            ):
                command += ["--node", name]
            subprocess.run(command, cwd=ROOT, check=True)
            subprocess.run(
                [str(cooker), "--inspect", str(temporary)], cwd=ROOT, check=True
            )
            os.replace(temporary, out)
        finally:
            temporary.unlink(missing_ok=True)
        asset = catalog[dest]
        receipt["models"][asset["slug"]] = {"output": dest, "sha256": digest(out)}
        count += 1
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(dir=receipt_path.parent, suffix=".part")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as out:
            out.write(json.dumps(receipt, indent=2) + "\n")
        os.replace(temporary, receipt_path)
    finally:
        pathlib.Path(temporary).unlink(missing_ok=True)
    subprocess.run(
        [sys.executable, str(ROOT / "Scripts/FetchVisualAssets.py")]
        + (["--offline"] if args.offline else []),
        cwd=ROOT,
        check=True,
    )
    for output in args.output or []:
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "Scripts/StageRuntimeAssets.py"),
                "--root",
                str(ROOT),
                "--output",
                str(output.resolve()),
            ],
            check=True,
        )
    print(f"Cooked and decoded {count} models successfully.")


if __name__ == "__main__":
    try:
        main()
    except (Exception, KeyboardInterrupt) as error:
        print("AAA build failed:", error, file=sys.stderr)
        sys.exit(1)
