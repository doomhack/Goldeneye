#!/bin/sh
# Check that this tree builds exactly the same ROM as another commit.
#
#   tools/compare/romcmp.sh [BASE] [VERSION]
#
# BASE is any git revision (default HEAD). VERSION is US (default) or EU. Both trees are
# built with the same SOURCE_DATE_EPOCH, so the build timestamp doesn't count. The base is
# checked out into a temporary worktree that borrows this tree's extracted assets, so run
# `make extractassets` here first. Exits 0 when the two ROMs are byte-identical.
#
# This is the strongest check there is: renaming functions, variables, struct fields or
# files leaves the ROM byte-identical. When it doesn't, use compare.sh to see which
# functions changed.
#
# One catch: assert() puts __FILE__ into the ROM, so moving or renaming a C file that uses
# assert changes the ROM by that string.
set -e
BASE=${1:-HEAD}
VERSION=${2:-US}
case "$VERSION" in US) CODE=u ;; EU) CODE=e ;; *) echo "VERSION must be US or EU" >&2; exit 2 ;; esac

ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"
WORK=$(mktemp -d)
trap 'git worktree remove --force "$WORK/base" >/dev/null 2>&1; rm -rf "$WORK"' EXIT
export SOURCE_DATE_EPOCH=0
# Rebuild the files that use __DATE__ so the fixed date takes effect. Touch them rather than
# deleting their objects: the Makefile marks every target .SECONDARY, so make doesn't rebuild
# a missing object while the ROM is newer than its source.
grep -rl --include='*.c' __DATE__ src | xargs -r touch

echo "Building $VERSION ROM for this tree..."
make -s -j"$(nproc)" VERSION=$VERSION >"$WORK/new.log" 2>&1 ||
    { tail -20 "$WORK/new.log"; exit 2; }

echo "Building $VERSION ROM for $BASE..."
git worktree add --detach "$WORK/base" "$BASE" >/dev/null 2>&1
# Borrow the extracted assets and base ROMs (everything git ignores, apart from builds).
git ls-files --others --ignored --exclude-standard -z | grep -zv '^build/' |
    (cd "$ROOT" && xargs -0 cp --parents -l -t "$WORK/base" 2>/dev/null) || true
(cd "$WORK/base" && make -s -j"$(nproc)" VERSION=$VERSION >"$WORK/base.log" 2>&1) ||
    { tail -20 "$WORK/base.log"; exit 2; }

NEW=build/$CODE/ge007.$CODE.z64
OLD=$WORK/base/build/$CODE/ge007.$CODE.z64
if cmp -s "$OLD" "$NEW"; then
    echo "ROMs are byte-identical ($(sha1sum "$NEW" | cut -c1-12))"
else
    echo "ROMs differ: $(cmp -l "$OLD" "$NEW" | wc -l) bytes"
    exit 1
fi
