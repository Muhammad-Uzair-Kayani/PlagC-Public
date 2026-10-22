#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
git submodule update --init --recursive
premake_bin="./Tools/premake5"
if [[ ! -x "$premake_bin" ]]; then premake_bin="$(command -v premake5 || true)"; fi
if [[ -z "$premake_bin" ]]; then echo 'Install Premake 5 in Tools/ or on PATH; see README.md.' >&2; exit 1; fi
action="${1:-gmake2}"
if [[ $# -gt 0 ]]; then shift; fi
"$premake_bin" "$action" "$@"
