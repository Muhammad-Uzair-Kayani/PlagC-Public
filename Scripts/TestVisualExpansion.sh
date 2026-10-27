#!/usr/bin/env bash
# Run after a native Dist build and BuildAAAAssets.sh. No user benchmark is required.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p Bin-Int/TPS Logs
python3 -c 'import sys;sys.path.insert(0,"Scripts");from RuntimeAssetManifest import verify;verify(".")'
libs=Bin/linux-x86_64/Dist
"${CXX:-g++}" -std=c++20 -O2 -DNDEBUG -DPC_TPS_VISUAL_MAX -pthread -IPlagC/include -IBuild/Pch \
    Game/src/*.cpp Scripts/Tests/VisualExpansion.cpp -L"$libs" ${LDFLAGS:-} \
    -lPlagC -lGLFW -lspdlog -lJolt -lOzz -lOpenAL -lOgreHlmsPbs -lOgreHlmsUnlit \
    -lOgreHlmsCommon -lOgreRenderNULL -lOgreRenderGL3Plus -lOgreMain \
    -latomic -lX11 -lxcb -lGL -lEGL -lXrandr -lpthread -ldl -lm \
    '-Wl,-rpath,$ORIGIN/../../Bin/linux-x86_64/Dist' -o Bin-Int/TPS/expansion
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 Bin-Int/TPS/expansion
python3 Scripts/VerifyExpansionImages.py
