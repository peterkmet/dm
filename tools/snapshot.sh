#!/bin/bash
# Save the whole working tree (tracked and untracked, not ignored) as a commit on branch wip-snapshot.
# The working tree, the index, and HEAD do not change.
set -euo pipefail
cd "$(dirname "$0")/.."
export GIT_INDEX_FILE=$(mktemp)
trap 'rm -f "$GIT_INDEX_FILE"' EXIT
git read-tree HEAD
git add -A
tree=$(git write-tree)
parent=$(git rev-parse -q --verify wip-snapshot || git rev-parse HEAD)
commit=$(git commit-tree "$tree" -p "$parent" -m "wip: snapshot $(date '+%Y-%m-%d %H:%M')")
git update-ref refs/heads/wip-snapshot "$commit"
echo "wip-snapshot -> $commit"
