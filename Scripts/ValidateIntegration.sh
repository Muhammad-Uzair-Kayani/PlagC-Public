#!/usr/bin/env bash
# Fast CPU-only regression suite. Uses the same pinned EnTT source as PlagC.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/plagc-integration-$$"
trap 'rm -f "$out"' EXIT
"${CXX:-g++}" -std=c++20 -O2 -pthread -DPC_INTEGRATION_TEST_MAIN \
  -IPlagC/include -IBuild/Pch -IVendor/entt/src \
  PlagC/src/Input/Input.cpp PlagC/src/Jobs.cpp PlagC/src/World/World.cpp \
  Sandbox/src/IntegrationWork.cpp Sandbox/src/IntegrationTests.cpp -o "$out"
"$out"
