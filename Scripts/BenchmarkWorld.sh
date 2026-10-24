#!/usr/bin/env bash
# Isolated ECS/bridge correctness + before/after benchmark, with a recording backend.
# This does not measure Ogre, GPU rendering or native FPS. Run Release Sandbox for those.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/plagc-world-$$"
trap 'rm -f "$out"' EXIT
"${CXX:-g++}" -std=c++20 -O2 -DNDEBUG -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -IPlagC/include -IBuild/Pch -I"${PLAGC_ENTT_INCLUDE:-Vendor/entt/src}" \
  PlagC/src/Physics/PhysicsTypes.cpp PlagC/src/World/World.cpp PlagC/src/World/TransformSystem.cpp PlagC/src/World/SceneRenderer.cpp \
  PlagC/src/Jobs.cpp Scripts/Tests/WorldTests.cpp PlagC/src/Renderer/Geometry.cpp Sandbox/src/TransformSystemTests.cpp Sandbox/src/SceneSyncTests.cpp \
  Scripts/Tests/RecordingRenderer.cpp Scripts/Tests/LegacySceneRenderer.cpp Scripts/Tests/WorldHotPath.cpp -o "$out"
"$out"
