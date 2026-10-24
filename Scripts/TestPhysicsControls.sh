#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/plagc-physics-controls-$$"
trap 'rm -f "$out"' EXIT
"${CXX:-g++}" -std=c++20 -Wall -Wextra -pthread -IPlagC/include -IBuild/Pch \
    PlagC/src/Input/Input.cpp Scripts/Tests/PhysicsControls.cpp -o "$out"
"$out"
