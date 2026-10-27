#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p Bin-Int/AnimationTests
"${CXX:-g++}" -std=c++20 -O2 -IPlagC/include -IBuild/Pch -IVendor/ozz/include \
    PlagC/src/Assets/Asset.cpp PlagC/src/Assets/AssetIO.cpp PlagC/src/Animation/OzzAnimator.cpp \
    Scripts/Tests/WeaponAnimationTests.cpp Bin-Int/AssetTests/libOzz.a \
    -o Bin-Int/AnimationTests/weapon-tests
Bin-Int/AnimationTests/weapon-tests "$@"
