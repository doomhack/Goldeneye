#!/bin/sh
# Build the mupen64plus core with the data cache model (mupen64plus-core.patch).
# Usage: tools/cachesim/build-core.sh [WORKDIR]   (default /tmp/cachesim-core)
# Prints the path of the built library; pass it to the tools as M64RUN_CORE.
# Needs: git, a C compiler, libsdl2-dev, libpng-dev, zlib1g-dev, libfreetype-dev, libminizip-dev.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=${1:-/tmp/cachesim-core}
COMMIT=dc1411a196b0e35745e9a5661a44e60809fad250
if [ ! -d "$WORK/.git" ]; then
    git clone -q https://github.com/mupen64plus/mupen64plus-core.git "$WORK"
fi
cd "$WORK"
git checkout -q -f "$COMMIT"
git clean -qfd src
git apply "$HERE/mupen64plus-core.patch"
cd projects/unix
make -j"$(nproc)" all NO_ASM=1 OPTFLAGS=-O2 VULKAN=0 >/dev/null
echo "$WORK/projects/unix/libmupen64plus.so.2.0.0"
