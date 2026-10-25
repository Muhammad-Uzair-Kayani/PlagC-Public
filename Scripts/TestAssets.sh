#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python Scripts/BuildAssetVendors.py
flags=(-std=c++20 -O2 -DNDEBUG -pthread -IPlagC/include -IBuild/Pch -IVendor/entt/src -IBuild/Config/Assimp -IVendor/assimp/include -IVendor/assimp/contrib -IVendor/ozz/include -IVendor/openal/include)
output=Bin-Int/AssetTests/tests
if [[ "${ASSET_SANITIZE:-0}" == 1 ]]; then flags+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer);output=Bin-Int/AssetTests/tests-asan;fi
"${CXX:-g++}" "${flags[@]}" PlagC/src/Assets/*.cpp PlagC/src/Animation/*.cpp PlagC/src/Audio/*.cpp \
    PlagC/src/Jobs.cpp PlagC/src/World/World.cpp PlagC/src/Physics/PhysicsTypes.cpp \
    Scripts/Tests/RecordingRenderer.cpp Scripts/Tests/AssetTests.cpp \
    Bin-Int/AssetTests/libAssimp.a Bin-Int/AssetTests/libOzz.a -LBin-Int/AssetTests -lOpenAL \
    '-Wl,-rpath,$ORIGIN' -o "$output"
"$output"
