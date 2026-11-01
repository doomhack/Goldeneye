#!/bin/bash
# Build the GCC 15 cross compiler the game is built with: binutils + a C-only GCC for
# mips-linux-gnu, with link-time optimisation (LTO) support.
#
#   tools/toolchain/build-gcc.sh [PREFIX]     (default /opt/ge-gcc15)
#
# Needs a host C/C++ compiler, make, bison, flex, texinfo and the GMP, MPFR and MPC
# development packages. Takes about half an hour on 4 cores.
#
# LTO needs the linker to load GCC's plugin (liblto_plugin.so), which a fully static ld
# can't do, so ld and the plugin link only against the C library, and everything else
# (GMP, MPFR, MPC, libstdc++) is linked in statically. Build it on an old distribution
# (the published toolchain was built on Ubuntu 22.04) and it runs on that and anything
# newer. If the GMP, MPFR and MPC shared libraries are installed, move their .so links
# aside first so the static ones are used.
#
# The target is mips-linux-gnu rather than a bare-metal mips64-elf because the Linux
# assembler rounds every section up to its 16-byte alignment, and the asset data still
# depends on that padding. Only the compiler is built (no C library or libgcc); the game
# brings its own (src/gcc/libgcc_compat.c).
set -e
PREFIX=${1:-/opt/ge-gcc15}
GCC_VERSION=15.2.0
BINUTILS_VERSION=2.45
TARGET=mips-linux-gnu
WORK=$(mktemp -d)
cd "$WORK"

curl -fLO https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz
curl -fLO https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz
tar xf binutils-$BINUTILS_VERSION.tar.xz
tar xf gcc-$GCC_VERSION.tar.xz

mkdir build-binutils && cd build-binutils
../binutils-$BINUTILS_VERSION/configure --prefix="$PREFIX" --target=$TARGET --with-cpu=vr4300 \
    --disable-nls --disable-werror --disable-gdb --disable-sim --disable-gprofng --enable-plugins \
    --without-zstd LDFLAGS="-static-libgcc -static-libstdc++"
make -j"$(nproc)"
make install
cd ..

export PATH="$PREFIX/bin:$PATH"
mkdir build-gcc && cd build-gcc
../gcc-$GCC_VERSION/configure --prefix="$PREFIX" --target=$TARGET \
    --with-arch=vr4300 --with-tune=vr4300 --with-abi=32 --enable-languages=c,lto --without-headers \
    --disable-shared --disable-threads --disable-multilib --disable-nls --disable-libssp \
    --disable-libquadmath --disable-libgomp --disable-libatomic --disable-decimal-float \
    --enable-lto --without-zstd LDFLAGS="-static-libgcc -static-libstdc++"
make -j"$(nproc)" all-gcc
make install-gcc
cd /
rm -rf "$WORK"
echo "Installed to $PREFIX; add $PREFIX/bin to PATH."
