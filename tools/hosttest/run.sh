#!/bin/sh
# Compiles game modules for the host PC and runs their unit tests. See docs/Testing.md.
set -e
cd "$(dirname "$0")"
out=$(mktemp -d)
CC="cc -std=gnu2x -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover -I include -I ../../src"
$CC memtest.c -o "$out/memtest"
"$out/memtest"
$CC rziptest.c -lz -o "$out/rziptest"
"$out/rziptest"
# -fno-builtin: the maths library's own sinf and cosf, not the compiler's
$CC -fno-builtin mathtest.c -lm -o "$out/mathtest"
"$out/mathtest"
rm -rf "$out"
