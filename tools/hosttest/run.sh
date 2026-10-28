#!/bin/sh
# Compiles game modules for the host PC and runs their unit tests. See docs/Testing.md.
set -e
cd "$(dirname "$0")"
out=$(mktemp -d)
cc -std=gnu2x -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover \
   -I include -I ../../src memtest.c -o "$out/memtest"
"$out/memtest"
rm -rf "$out"
