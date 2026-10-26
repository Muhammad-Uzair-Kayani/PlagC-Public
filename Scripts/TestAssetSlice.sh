#!/usr/bin/env bash
# Native Linux/EGL inspection. Run after building AssetCooker and PlagC Dist.
set -euo pipefail
cd "$(dirname "$0")/.."
cooker="${ASSET_COOKER:-Bin/linux-x86_64/Dist/AssetCooker}"
libs=Bin/linux-x86_64/Dist
runtime=Bin-Int/AssetSlice/Runtime
mkdir -p Bin-Int/AssetSlice
if [[ "${INSPECT_ALL:-0}" == 1 ]]; then
  python3 Scripts/BuildAAAAssets.py --cooker "$cooker"
  python3 Scripts/StageRuntimeAssets.py --root . --output "$runtime"
else
  python3 Scripts/BuildAAAAssets.py --cooker "$cooker" --asset security_camera_01
  python3 Scripts/StageRuntimeAssets.py --root . --output "$runtime" --profile inspection --asset security_camera_01
fi
# Exercise the file-backed texture loader independently of the embedded cooked textures.
cp Assets/Source/AAA/security_camera_01/textures/security_camera_01_diff_1k.jpg "$runtime/Assets/Textures/SliceDiffuse.jpg"
"${CXX:-g++}" -std=c++20 -O2 -DNDEBUG -pthread -IPlagC/include -IBuild/Pch \
  Scripts/Tests/RuntimeAssetSlice.cpp -L"$libs" ${LDFLAGS:-} \
  -lPlagC -lGLFW -lspdlog -lJolt -lOzz -lOpenAL -lOgreHlmsPbs -lOgreHlmsUnlit \
  -lOgreHlmsCommon -lOgreRenderNULL -lOgreRenderGL3Plus -lOgreMain \
  -latomic -lX11 -lxcb -lGL -lEGL -lXrandr -lpthread -ldl -lm \
  '-Wl,-rpath,$ORIGIN/../../Bin/linux-x86_64/Dist' -o Bin-Int/AssetSlice/inspect
binary="$(pwd)/Bin-Int/AssetSlice/inspect"
# An absolute library path lets the test run from the staged folder, not the repository.
export LD_LIBRARY_PATH="$(pwd)/$libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if [[ "${INSPECT_ALL:-0}" == 1 ]]; then
  python3 Scripts/InspectRuntimeAssets.py --runtime "$runtime" --binary "$binary"
else
  (cd "$runtime" && mkdir -p Logs && EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 "$binary")
fi
if rg -i 'active samplers|GLSL.*error|GL_INVALID|GLSL validation result' "$runtime/Logs/Ogre.log"; then
  echo 'Shader diagnostics failed' >&2; exit 1
fi
