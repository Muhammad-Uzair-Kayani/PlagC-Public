#!/usr/bin/env python3
"""Remove development-only linker artifacts from a completed Dist output.

Dist is a self-contained windowed application: PlagC and its compiled vendors are
linked statically, so import/static libraries and debug linker artifacts do not
belong in the distributable folder. Runtime data staged under Assets/ and
Vendor/ is intentionally preserved.
"""

from __future__ import annotations

import argparse
from pathlib import Path


DEVELOPMENT_SUFFIXES = {".lib", ".exp", ".pdb", ".ilk"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    output = args.output.resolve()
    if not output.is_dir():
        raise SystemExit(f"Dist output does not exist: {output}")

    removed = 0
    # Only prune the package root. Assets/ and Vendor/ contain runtime data and
    # must never be treated as linker output merely because a file extension
    # happens to match a development artifact.
    for entry in output.iterdir():
        if entry.is_file() and entry.suffix.lower() in DEVELOPMENT_SUFFIXES:
            entry.unlink()
            removed += 1

    print(f"Dist package pruned: removed {removed} development artifact(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
