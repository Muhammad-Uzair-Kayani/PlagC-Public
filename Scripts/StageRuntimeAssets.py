#!/usr/bin/env python3
"""Stage engine-owned runtime data beside a selected build output.

The copy is incremental: unchanged files are left alone, changed/new files are copied,
and stale files under staged roots are removed. Client/game assets are intentionally
outside this script and must be staged by the client project.
"""

from __future__ import annotations

import argparse
import filecmp
import shutil
import sys
from pathlib import Path

from RuntimeAssetManifest import verify


def sync_tree(source: Path, destination: Path) -> None:
    filecmp.clear_cache()
    if not source.is_dir():
        raise SystemExit(f"Runtime asset source is missing: {source}")

    source = source.resolve()
    destination = destination.resolve()
    if (
        source == destination
        or source.is_relative_to(destination)
        or destination.is_relative_to(source)
    ):
        raise ValueError("Runtime source/destination trees overlap")

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
            copy = not filecmp.cmp(src, dst, shallow=False)
        if copy:
            shutil.copy2(src, dst)

    for dst in sorted(
        (path for path in destination.rglob("*") if path.is_file()), reverse=True
    ):
        relative = dst.relative_to(destination)
        if relative not in source_files:
            dst.unlink()

    for directory in sorted(
        (path for path in destination.rglob("*") if path.is_dir()), reverse=True
    ):
        try:
            directory.rmdir()
        except OSError:
            pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sentinel", type=Path)
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()

    if (
        output == root
        or output.is_relative_to(root / "Assets")
        or output.is_relative_to(root / "Vendor/ogre-next/Samples/Media")
    ):
        raise ValueError("Output must not overlap repository asset sources")

    verify(root)

    # The Sandbox is an engine smoke client. It needs renderer resources, not cooked
    # gameplay content. User projects own their own cooked-asset staging.
    for directory in ["Textures", "Shaders", "Compositors"]:
        sync_tree(root / "Assets" / directory, output / "Assets" / directory)

    media = Path("Vendor/ogre-next/Samples/Media")
    for directory in ["Hlms", "models"]:
        sync_tree(root / media / directory, output / media / directory)

    verify(output)

    compiler_license = root / "Vendor/glslang/LICENSE.txt"
    if compiler_license.is_file():
        licenses = output / "Licenses"
        licenses.mkdir(parents=True, exist_ok=True)
        shutil.copy2(compiler_license, licenses / "glslang-LICENSE.txt")

    if args.sentinel is not None:
        sentinel = args.sentinel.resolve()
        sentinel.parent.mkdir(parents=True, exist_ok=True)
        sentinel.touch()

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as error:
        print("Runtime staging failed:", error, file=sys.stderr)
        raise SystemExit(1)
