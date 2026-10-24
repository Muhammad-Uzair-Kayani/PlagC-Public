#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# TestPhysics builds/caches the pinned Jolt archive and verifies core APIs first.
if [[ ! -f Bin-Int/PhysicsTests/libJolt.a ]]; then bash Scripts/TestPhysics.sh; fi
flags=(-std=c++20 -O2 -DNDEBUG -DJPH_DISABLE_CUSTOM_ALLOCATOR -pthread -IPlagC/include -IBuild/Pch -IVendor/jolt -IVendor/entt/src)
output=Bin-Int/PhysicsTests/stress-tests
if [[ "${STRESS_SANITIZE:-0}" == 1 ]]; then
    flags+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
    output=Bin-Int/PhysicsTests/stress-tests-asan
fi
"${CXX:-g++}" "${flags[@]}" PlagC/src/Physics/PhysicsTypes.cpp PlagC/src/Physics/PhysicsShapes.cpp \
    PlagC/src/Jobs.cpp PlagC/src/Physics/JoltPhysicsWorld.cpp PlagC/src/Physics/PhysicsSystem.cpp \
    PlagC/src/World/World.cpp PlagC/src/World/TransformSystem.cpp PlagC/src/World/SceneRenderer.cpp \
    Sandbox/src/StressSimulation.cpp Sandbox/src/StressHud.cpp Scripts/Tests/RecordingRenderer.cpp \
    Scripts/Tests/StressTests.cpp Bin-Int/PhysicsTests/libJolt.a -o "$output"
"$output"
