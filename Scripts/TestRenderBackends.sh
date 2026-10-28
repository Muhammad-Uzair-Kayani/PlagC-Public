#!/usr/bin/env bash
# Run after a native Vulkan-enabled Dist build; both adapters remain in one binary.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p Bin-Int/BackendTests Logs
libs=Bin/linux-x86_64/Dist
"${CXX:-g++}" -std=c++20 -O2 -DNDEBUG -DPC_DIST -pthread -IPlagC/include \
    Scripts/Tests/BackendRenderSmoke.cpp -L"$libs" ${LDFLAGS:-} \
    -Wl,--start-group -lPlagC -lGLFW -lspdlog -lJolt -lOzz -lOpenAL \
    -lOgreHlmsPbs -lOgreHlmsUnlit -lOgreHlmsCommon -lOgreRenderNULL -lOgreRenderGL3Plus \
    -lOgreRenderVulkan -lGlslang -lOgreMain -Wl,--end-group \
    -lvulkan -lX11-xcb -lxcb-randr -latomic -lX11 -lxcb -lGL -lEGL -lXrandr -ldl -lm \
    '-Wl,-rpath,$ORIGIN/../../Bin/linux-x86_64/Dist' -o Bin-Int/BackendTests/smoke
EGL_PLATFORM=surfaceless Bin-Int/BackendTests/smoke opengl
Bin-Int/BackendTests/smoke vulkan
