#!/usr/bin/env bash
# Run after a native Dist build. Mesa/EGL can exercise the real renderer without a desktop.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p Bin-Int/TPS Logs
libs=Bin/linux-x86_64/Dist
"${CXX:-g++}" -std=c++20 -O2 -DNDEBUG -pthread -IPlagC/include -IBuild/Pch \
    Game/src/*.cpp Scripts/Tests/TPSVisual.cpp -L"$libs" ${LDFLAGS:-} \
    -lPlagC -lGLFW -lspdlog -lJolt -lOzz -lOpenAL -lOgreHlmsPbs -lOgreHlmsUnlit \
    -lOgreHlmsCommon -lOgreRenderNULL -lOgreRenderGL3Plus -lOgreMain \
    -latomic -lX11 -lxcb -lGL -lEGL -lXrandr -lpthread -ldl -lm \
    '-Wl,-rpath,$ORIGIN/../../Bin/linux-x86_64/Dist' -o Bin-Int/TPS/visual
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 Bin-Int/TPS/visual
