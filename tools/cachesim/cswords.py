#!/usr/bin/env python3
"""Per-member accesses and misses of one struct, from runs made with csdemos.py --watch.

Usage: cswords.py ELF STRUCT DUMP [DUMP...]

Each DUMP.words holds per-4-byte-word counts over the watched range (one instance of
STRUCT). Members are listed in layout order with accesses and misses per game frame, and
each 16-byte line's total, so you can see which members are hot and which lines they share.
"""
import json
import struct
import sys

sys.path.insert(0, __import__('os').path.dirname(__file__))
from csreport import layout  # noqa: E402


def main():
    elf, sname, dumps = sys.argv[1], sys.argv[2], sys.argv[3:]
    members, size = layout(elf, sname)
    acc = [0] * (size // 4 + 1)
    miss = [0] * (size // 4 + 1)
    frames = 0
    for path in dumps:
        raw = open(path + '.words', 'rb').read()
        start, length = struct.unpack_from('<II', raw, 0)
        n = length // 4 + 1
        a = struct.unpack_from(f'<{n}I', raw, 8)
        m = struct.unpack_from(f'<{n}I', raw, 8 + 4 * n)
        for i in range(min(n, len(acc))):
            acc[i] += a[i]; miss[i] += m[i]
        frames += json.load(open(path + '.json'))['game_frames']
    pf = max(frames, 1)
    print(f'{sname} ({size} B), {len(dumps)} runs, {frames} game frames; per game frame:')
    print(f'  {"accesses":>9} {"misses":>8}  offset size member')
    for off, msize, name in members:
        words = range(off // 4, (off + msize + 3) // 4)
        a = sum(acc[w] for w in words if w < len(acc))
        m = sum(miss[w] for w in words if w < len(miss))
        if a or m:
            print(f'  {a / pf:9.1f} {m / pf:8.2f}  +{off:<5} {msize:4} {name}')
    print('\nPer 16-byte line (offset within the struct): accesses / misses per game frame')
    for line in range(0, size, 16):
        a = sum(acc[line // 4:line // 4 + 4]); m = sum(miss[line // 4:line // 4 + 4])
        if a:
            print(f'  +{line:<5} {a / pf:9.1f} {m / pf:8.2f}')


if __name__ == '__main__':
    main()
