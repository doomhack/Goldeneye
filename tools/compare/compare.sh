#!/bin/sh
# Compare this tree's compiled C code with another commit's, function by function.
#
#   tools/compare/compare.sh [BASE] [VERSION]
#
# BASE is any git revision (default HEAD, i.e. compare your uncommitted changes with the
# last commit). VERSION is US (default) or EU. The base is checked out into a temporary
# worktree and only its C objects are built there, so no assets are needed. Exits 0 when
# every function and data object is unchanged (renames are matched up and allowed).
# Set VERBOSE=1 to see the first difference in each changed function, KEEP=1 to keep the
# compared objects for a closer look with objdump.
set -e
BASE=${1:-HEAD}
VERSION=${2:-US}
case "$VERSION" in US) CODE=u ;; EU) CODE=e ;; *) echo "VERSION must be US or EU" >&2; exit 2 ;; esac

ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"
WORK=$(mktemp -d)
trap 'git worktree remove --force "$WORK/base" >/dev/null 2>&1; [ -n "$KEEP" ] || rm -rf "$WORK"' EXIT

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

# Compare only the objects of each tree's current C files, so stale objects left in a
# build directory by moved or deleted files don't count.
collect() {
    (cd "$1" && objects | while read -r o; do
        mkdir -p "$2/$(dirname "$o")" && cp "$o" "$2/$o"
    done)
}
collect "$ROOT" "$WORK/new"
collect "$WORK/base" "$WORK/old"
[ -n "$KEEP" ] && echo "Objects kept in $WORK/old and $WORK/new"
python3 "$ROOT/tools/compare/codecmp.py" ${VERBOSE:+--verbose} "$WORK/old/build/$CODE/src" "$WORK/new/build/$CODE/src"
