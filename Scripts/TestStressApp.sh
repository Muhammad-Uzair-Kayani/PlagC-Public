#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -f Bin-Int/PhysicsTests/libJolt.a ]]; then bash Scripts/TestPhysics.sh; fi
"${CXX:-g++}" -std=c++20 -O2 -DNDEBUG -DJPH_DISABLE_CUSTOM_ALLOCATOR -pthread -IPlagC/include -IBuild/Pch -IVendor/jolt -IVendor/entt/src \
    PlagC/src/Application.cpp PlagC/src/Input/Input.cpp PlagC/src/Physics/PhysicsTypes.cpp PlagC/src/Physics/PhysicsShapes.cpp \
    PlagC/src/Jobs.cpp PlagC/src/Physics/JoltPhysicsWorld.cpp PlagC/src/Physics/PhysicsSystem.cpp \
    PlagC/src/World/World.cpp PlagC/src/World/TransformSystem.cpp PlagC/src/World/SceneRenderer.cpp \
    Sandbox/src/StressSimulation.cpp Sandbox/src/StressHud.cpp Sandbox/src/StressWorld.cpp \
    Scripts/Tests/RecordingRenderer.cpp Scripts/Tests/StressAppTests.cpp \
    Bin-Int/PhysicsTests/libJolt.a -o Bin-Int/PhysicsTests/stress-app-tests
Bin-Int/PhysicsTests/stress-app-tests
