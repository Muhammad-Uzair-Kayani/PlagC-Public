#!/usr/bin/env python3
"""Stage the runtime data needed by Sandbox beside a selected build output.

The copy is incremental: unchanged files are left alone, changed/new files are copied,
and stale files under the staged roots are removed. This makes the staging step cheap
enough to run on every Sandbox build, including asset-only rebuilds.
"""

from __future__ import annotations

import argparse
import shutil
import filecmp
import sys
from RuntimeAssetManifest import verify
from pathlib import Path


def sync_tree(source: Path, destination: Path) -> None:
    filecmp.clear_cache()
    if not source.is_dir():
        raise SystemExit(f"Runtime asset source is missing: {source}")

    if (
        source.resolve() == destination.resolve()
        or source.resolve().is_relative_to(destination.resolve())
        or destination.resolve().is_relative_to(source.resolve())
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

    if destination.exists():
        for dst in sorted(
            (p for p in destination.rglob("*") if p.is_file()), reverse=True
        ):
            relative = dst.relative_to(destination)
            if relative not in source_files:
                dst.unlink()
        for directory in sorted(
            (p for p in destination.rglob("*") if p.is_dir()), reverse=True
        ):
            try:
                directory.rmdir()
            except OSError:
                pass


def sync_optional_tree(source: Path, destination: Path) -> bool:
    """Synchronize a local-only runtime tree when present.

    Missing optional sources are not an error because Default game builds do not
    require external GameAssets. If present, the tree is staged beside Sandbox.exe
    so game mounts resolve consistently from Visual Studio's output working directory.
    """
    if not source.is_dir():
        return False
    sync_tree(source, destination)
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sentinel", type=Path)
    parser.add_argument(
        "--profile", choices=["base", "visual", "inspection"], default="visual"
    )
    parser.add_argument("--asset", action="append")
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()

    if (
        output == root
        or output.is_relative_to(root / "Assets")
        or output.is_relative_to(root / "Vendor/ogre-next/Samples/Media")
    ):
        raise ValueError("Output must not overlap repository asset sources")

    # Preserve the paths used by RendererConfig and AssetManager so Sandbox can
    # run with its working directory set to the configuration output folder.
    verify(root, args.profile, args.asset)
    for directory in ["Cooked", "Textures", "Shaders", "Compositors", "Manifests"]:
        sync_tree(root / "Assets" / directory, output / "Assets" / directory)
    media = Path("Vendor/ogre-next/Samples/Media")
    for directory in ["Hlms", "models"]:
        sync_tree(root / media / directory, output / media / directory)
    verify(output, args.profile, args.asset)

    # Game definitions are tracked source and must always be staged because the
    # runtime host loads world manifests relative to the executable working directory.
    sync_tree(
        root / "Game" / "SurvivalGame" / "Config",
        output / "Game" / "SurvivalGame" / "Config",
    )

    # GameAssets is intentionally gitignored. Developers may place cooked game
    # content here; when present it is staged beside the binary without ever being
    # added to the repository.
    sync_optional_tree(
        root / "GameAssets" / "SurvivalGame" / "Runtime",
        output / "GameAssets" / "SurvivalGame" / "Runtime",
    )

    # Visual Studio's custom-build rule declares this file as its output. Create
    # it only after staging and verification succeed, so MSBuild never reports a
    # successful command with a missing declared output.
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
