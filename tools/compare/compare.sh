#!/bin/sh
# Compare this tree's compiled C code with another commit's, function by function.
#
#   tools/compare/compare.sh [BASE] [VERSION]
#
# BASE is any git revision (default HEAD, i.e. compare your uncommitted changes with the
# last commit). VERSION is US (default) or EU. The base is checked out into a temporary
# worktree and only its C objects are built there, so no assets are needed. Exits 0 when
# every function and data object is unchanged (renames are matched up and allowed).
set -e
BASE=${1:-HEAD}
VERSION=${2:-US}
case "$VERSION" in US) CODE=u ;; EU) CODE=e ;; *) echo "VERSION must be US or EU" >&2; exit 2 ;; esac

ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"
WORK=$(mktemp -d)
trap 'git worktree remove --force "$WORK/base" >/dev/null 2>&1; rm -rf "$WORK"' EXIT

objects() {
    find src -name '*.c' | sed "s#^#build/$CODE/#; s#\.c\$#.o#"
}

echo "Building C objects for this tree..."
make -s create_directories VERSION=$VERSION >/dev/null
make -s -j"$(nproc)" VERSION=$VERSION $(objects) >/dev/null

echo "Building C objects for $BASE..."
git worktree add --detach "$WORK/base" "$BASE" >/dev/null 2>&1
(cd "$WORK/base" && make -s create_directories VERSION=$VERSION >/dev/null &&
    make -s -j"$(nproc)" VERSION=$VERSION $(objects) >/dev/null)

python3 "$ROOT/tools/compare/codecmp.py" "$WORK/base/build/$CODE/src" "build/$CODE/src"
