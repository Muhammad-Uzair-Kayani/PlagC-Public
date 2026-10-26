#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -f Bin-Int/PhysicsTests/libJolt.a ]]; then bash Scripts/TestPhysics.sh; fi
if [[ ! -f Bin-Int/AssetTests/libOzz.a || ! -f Bin-Int/AssetTests/libOpenAL.so ]]; then python Scripts/BuildAssetVendors.py; fi
extra=(-O2)
output=Bin-Int/PhysicsTests/tps-tests
if [[ "${TPS_SANITIZE:-0}" == 1 ]]; then extra=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer); output=Bin-Int/PhysicsTests/tps-tests-asan; fi
"${CXX:-g++}" "${extra[@]}" -std=c++20 -DNDEBUG -DJPH_DISABLE_CUSTOM_ALLOCATOR -pthread -IPlagC/include -IBuild/Pch -IVendor/jolt -IVendor/entt/src -IVendor/ozz/include -IVendor/openal/include \
    PlagC/src/Assets/Asset.cpp PlagC/src/Assets/AssetIO.cpp PlagC/src/Assets/AssetManager.cpp PlagC/src/Assets/AssetScene.cpp \
    PlagC/src/Animation/OzzAnimator.cpp PlagC/src/Audio/OpenALAudio.cpp \
    PlagC/src/Application.cpp PlagC/src/Input/Input.cpp PlagC/src/Physics/PhysicsTypes.cpp PlagC/src/Physics/PhysicsShapes.cpp \
    PlagC/src/Jobs.cpp PlagC/src/Physics/JoltPhysicsWorld.cpp PlagC/src/Physics/PhysicsSystem.cpp \
    PlagC/src/World/World.cpp PlagC/src/World/TransformSystem.cpp PlagC/src/World/SceneRenderer.cpp \
    Game/src/TPSGame.cpp Game/src/Facility.cpp Game/src/GameHud.cpp PlagC/src/Renderer/Geometry.cpp \
    Scripts/Tests/RecordingRenderer.cpp Scripts/Tests/TPSGameTests.cpp \
    Bin-Int/PhysicsTests/libJolt.a Bin-Int/AssetTests/libOzz.a -LBin-Int/AssetTests -lOpenAL '-Wl,-rpath,$ORIGIN/../AssetTests' -o "$output"
"$output"
