#!/usr/bin/env python3
"""Compare the CPU work of two builds over the same demo play.

Usage:
  csspeed.py BASEDIR NEWDIR [NEWDIR...]

Each DIR holds the demoNN.cs files csdemos.py saved, best counted with --ticks so both
builds cover the same game frames. For each build it prints, per game frame (averaged
over the demos both have), the instructions run (the idle loop left out), I-cache misses
(16 KB, 32-byte lines), D-cache misses and dirty write-backs, and a rough cycle estimate:
1 per instruction, 40 per cache miss, 20 per write-back. The weights are a guess at the
VR4300's costs on a busy bus; use the estimate to rank builds, not as milliseconds.
"""
import glob
import json
import os
import struct
import sys

KEYS = [('instructions', 'instructions'), ('icache_misses', 'I-cache misses'),
        ('dcache_misses', 'D-cache misses'), ('writebacks', 'write-backs')]


def load(d):
    out = {}
    for j in sorted(glob.glob(os.path.join(d, 'demo??.cs.json'))):
        meta = json.load(open(j))
        hdr = struct.unpack('<6Q', open(j[:-5], 'rb').read(48))
        frames = meta['game_frames']
        out[os.path.basename(j)[:6]] = {
            'instructions': meta.get('instructions', 0) / frames,
            'icache_misses': meta.get('icache_misses', 0) / frames,
            'dcache_misses': (hdr[2] + hdr[3]) / frames,
            'writebacks': (hdr[4] + hdr[5]) / frames}
    return out


def cycles(t):
    return t['instructions'] + 40 * (t['icache_misses'] + t['dcache_misses']) + 20 * t['writebacks']


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    base = load(sys.argv[1])
    for d in sys.argv[2:]:
        new = load(d)
        demos = sorted(set(base) & set(new))
        if not demos:
            sys.exit(f'no demos in common between {sys.argv[1]} and {d}')
        if not all(base[k]['instructions'] for k in demos):
            sys.exit('these counts have no instruction counts: rebuild the core (build-core.sh)')
        tb = {k: sum(base[x][k] for x in demos) / len(demos) for k, _ in KEYS}
        tn = {k: sum(new[x][k] for x in demos) / len(demos) for k, _ in KEYS}
        print(f'{d} against {sys.argv[1]} ({len(demos)} demos), per game frame:')
        for k, label in KEYS:
            print(f'  {label:15} {tb[k]:11,.0f} -> {tn[k]:11,.0f}  {100 * (tn[k] / tb[k] - 1):+6.2f}%')
        print(f'  {"est. cycles":15} {cycles(tb):11,.0f} -> {cycles(tn):11,.0f}  {100 * (cycles(tn) / cycles(tb) - 1):+6.2f}%')


if __name__ == '__main__':
    main()
