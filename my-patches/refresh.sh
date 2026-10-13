#!/bin/sh
# Save the local changes of the separately-cloned subprojects as patch files.
# Restore one:  git clone <url> <dir> && git -C <dir> checkout <commit> && git -C <dir> apply ../my-patches/<dir>.patch
# (url and commit are in BASES.txt)
set -e
cd "$(dirname "$0")/.."
OUT=my-patches
: > "$OUT/BASES.txt"
for d in am-kernels archbench fceux-am nvboard ysyxSoC; do
  [ -d "$d/.git" ] || continue
  idx="/tmp/patchidx-$d-$$"
  ( cd "$d" && GIT_INDEX_FILE="$idx" git read-tree HEAD \
      && GIT_INDEX_FILE="$idx" git add -A \
      && GIT_INDEX_FILE="$idx" git diff --cached --binary HEAD ) > "$OUT/$d.patch"
  rm -f "$idx"
  echo "$d $(git -C "$d" remote get-url origin) $(git -C "$d" rev-parse HEAD)" >> "$OUT/BASES.txt"
done
ls -l "$OUT"
