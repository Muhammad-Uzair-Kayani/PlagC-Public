#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
fail() { echo "Dependency setup: $*" >&2; exit 1; }
command -v git >/dev/null || fail "Git is required."
[[ -f .gitmodules ]] || fail "Missing .gitmodules; use a complete Git clone."
git rev-parse --show-toplevel >/dev/null 2>&1 || fail "Clone PlagC with Git; ZIP downloads are unsupported."
jobs="${PLAGC_FETCH_JOBS:-4}"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail "PLAGC_FETCH_JOBS must be a positive integer."
# Never force checkout or reset a developer's modified vendor sources.
git submodule foreach --quiet --recursive '
    test -z "$(git status --porcelain --untracked-files=normal)" || {
        echo "Local changes in $displaypath; commit or stash them before setup." >&2
        exit 1
    }
' || fail "Dependency working trees must be clean."
git submodule sync --recursive || fail "Cannot synchronize submodule configuration."
# Prefer locally available commits before allowing a network fetch.
if ! git submodule update --init --recursive --checkout --no-fetch --depth 1 --jobs "$jobs"; then
    echo "Some pinned objects are missing; retrying with shallow fetching enabled."
    git submodule update --init --recursive --checkout --depth 1 --jobs "$jobs" ||
    fail "Pinned checkout failed. Check the Git error above, connectivity and upstream access, then rerun. No full-history fallback is performed."
fi
status="$(git submodule status --recursive)" || fail "Cannot inspect submodules."
if printf '%s\n' "$status" | LC_ALL=C grep -Eq '^[-+U]'; then
    printf '%s\n' "$status" >&2
    fail "Uninitialized, mismatched or conflicted submodule; resolve it before generating."
fi
while IFS= read -r source || [[ -n "$source" ]]; do
    source="${source%$'\r'}"
    [[ -z "$source" || "$source" == \#* ]] && continue
    [[ -f "$source" ]] || fail "Missing $source; dependency checkout is incomplete."
done < Build/DependencyFiles.txt
printf '%s\n' "$status"
echo "Dependencies verified at PlagC-pinned commits."
