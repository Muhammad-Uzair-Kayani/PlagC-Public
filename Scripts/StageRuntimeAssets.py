#!/usr/bin/env python3
"""Stage the runtime data needed by Sandbox beside a selected build output.

The copy is incremental: unchanged files are left alone, changed/new files are copied,
and stale files under the staged roots are removed.  This makes the staging step cheap
enough to run on every Sandbox build, including asset-only rebuilds.
"""
from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def sync_tree(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise SystemExit(f"Runtime asset source is missing: {source}")

    destination.mkdir(parents=True, exist_ok=True)
    source_files: set[Path] = set()

    for src in source.rglob("*"):
        if not src.is_file():
            continue
        relative = src.relative_to(source)
        source_files.add(relative)
        dst = destination / relative
        dst.parent.mkdir(parents=True, exist_ok=True)

        copy = not dst.exists()
        if not copy:
            src_stat = src.stat()
            dst_stat = dst.stat()
            copy = src_stat.st_size != dst_stat.st_size or src_stat.st_mtime_ns > dst_stat.st_mtime_ns
        if copy:
            shutil.copy2(src, dst)

    if destination.exists():
        for dst in sorted((p for p in destination.rglob("*") if p.is_file()), reverse=True):
            relative = dst.relative_to(destination)
            if relative not in source_files:
                dst.unlink()
        for directory in sorted((p for p in destination.rglob("*") if p.is_dir()), reverse=True):
            try:
                directory.rmdir()
            except OSError:
                pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()

    # Preserve the paths used by RendererConfig and AssetManager so Sandbox can
    # run with its working directory set to the configuration output folder.
    sync_tree(root / "Assets", output / "Assets")
    sync_tree(
        root / "Vendor" / "ogre-next" / "Samples" / "Media",
        output / "Vendor" / "ogre-next" / "Samples" / "Media",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
