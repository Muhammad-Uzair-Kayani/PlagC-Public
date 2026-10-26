#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
# Requires prior project generation. AssetCooker does not depend on the game asset pack.
make -C Build/Generated config=release -j"${BUILD_JOBS:-2}" AssetCooker
python3 Scripts/BuildAAAAssets.py --cooker Bin/linux-x86_64/Release/AssetCooker "$@"
