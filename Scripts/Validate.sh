#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./GenerateProjects.sh gmake2 --null-platform
for config in debug release dist; do
    make -C Build/Generated "config=$config" -j4
    case "$config" in debug) name=Debug;; release) name=Release;; dist) name=Dist;; esac
    "./Bin-null/linux-x86_64/$name/Sandbox" --null --workers 4
    "./Bin-null/linux-x86_64/$name/Sandbox" --headless --self-test --workers 1
done

