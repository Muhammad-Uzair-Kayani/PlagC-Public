#!/usr/bin/env python3
"""Run the native inspection executable against every validated runtime model."""

import argparse, json, os, shutil, subprocess
from pathlib import Path
from RuntimeAssetManifest import verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("Logs/Inspections"))
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    binary = args.binary.resolve()
    output = args.output.resolve()
    verify(runtime)
    output.mkdir(parents=True, exist_ok=True)
    lock = json.loads((runtime / "Assets/Manifests/AAAWorld.lock.json").read_text())
    env = dict(os.environ, EGL_PLATFORM="surfaceless", LIBGL_ALWAYS_SOFTWARE="1")
    for asset in lock["assets"]:
        name = asset["slug"]
        with (output / (name + ".log")).open("w") as log:
            subprocess.run(
                [str(binary), asset["output"].removeprefix("Assets/Cooked/")],
                cwd=runtime,
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=True,
            )
        shaderlog = (runtime / "Logs/Ogre.log").read_text()
        if any(
            token in shaderlog
            for token in ["active samplers", "GLSL validation result", "GL_INVALID"]
        ):
            raise RuntimeError("unexpected shader diagnostic: " + name)
        shutil.copy2(runtime / "Logs/AssetSlice-HDR.ppm", output / (name + ".ppm"))
        print("PASS", name, flush=True)
    print("Inspected", len(lock["assets"]), "runtime models")


if __name__ == "__main__":
    main()
