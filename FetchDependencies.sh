#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
fail() { echo "Dependency setup: $*" >&2; exit 1; }
command -v git >/dev/null || fail "Git is required."
[[ -f .gitmodules ]] || fail "Missing .gitmodules; use a complete Git clone."
git rev-parse --show-toplevel >/dev/null 2>&1 || fail "Clone PlagC with Git; ZIP downloads are unsupported."
jobs="${PLAGC_FETCH_JOBS:-1}"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail "PLAGC_FETCH_JOBS must be a positive integer."
# Interrupted clones can leave a .git file pointing at missing/incomplete metadata.
# Preserve both pieces before Git's foreach/sync/update tries to open them. Never
# delete vendor sources or repair a healthy checkout by force.
while read -r key dependency; do
    name="${key#submodule.}"
    name="${name%.path}"
    metadata="$(git rev-parse --git-path "modules/$name")"
    broken=false
    if [[ -e "$dependency/.git" ]] && ! git -C "$dependency" rev-parse --git-dir >/dev/null 2>&1; then
        broken=true
    elif [[ ! -e "$dependency/.git" && -e "$metadata" ]] &&
         ! git --git-dir="$metadata" rev-parse --is-bare-repository >/dev/null 2>&1; then
        broken=true
    fi
    if [[ "$broken" == true ]]; then
        backup="$(mktemp -d "$(git rev-parse --absolute-git-dir)/plagc-recovery.XXXXXX")"
        [[ ! -e "$dependency" ]] || mv -- "$dependency" "$backup/worktree"
        [[ ! -e "$metadata" ]] || mv -- "$metadata" "$backup/gitdir"
        echo "Preserved interrupted $dependency clone in $backup"
    fi
done < <(git config -f .gitmodules --get-regexp '^submodule\..*\.path$')
# Never force checkout or reset a developer's modified vendor sources.
git submodule foreach --quiet --recursive '
    test -z "$(git status --porcelain --untracked-files=normal)" || {
        echo "Local changes in $displaypath; commit or stash them before setup." >&2
        exit 1
    }
' || fail "Dependency working trees must be clean."
git submodule sync --recursive || fail "Cannot synchronize submodule configuration."
# A single shallow update uses local pinned objects when available and fetches
# only when necessary. Do not immediately retry a failed parallel clone: Git may
# have left stale worktree pointers. The next invocation preserves those first.
git submodule update --init --recursive --checkout --depth 1 --jobs "$jobs" ||
    fail "Pinned checkout failed. Check connectivity, then rerun; interrupted top-level clones will be preserved and retried. No full-history fallback is performed."
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

