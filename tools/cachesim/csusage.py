#!/usr/bin/env python3
"""Find data that is read into the cache and then barely used: candidates for uncached
(KSEG1) reads.

Usage: csusage.py ELF DUMP [DUMP...] [--top N]

Reads DUMP.usage, written by m64run.py --cachesim with a core that has cachesim_dump_usage.
A "fill" is a line brought in by a read miss and never written while resident. A "once" fill
was accessed only by the access that missed before it left the cache: one bus transaction
for one load, and it evicted another line. Read through KSEG1 instead, that load would cost
a single-word uncached read and leave the cache alone. "twice" fills had one more access.
Lines that were written while resident are left out: they must stay cached.
"""
import argparse
import bisect
import collections
import json
import struct
import sys

sys.path.insert(0, __import__('os').path.dirname(__file__))
from csreport import RAMLINES, PCSLOTS, load_dump, symbols  # noqa: E402


def load_usage(path):
    raw = open(path, 'rb').read()
    hdr = struct.unpack_from('<4Q', raw, 0)
    if hdr[0] != 0x43535531:
        sys.exit(f'{path}: not a usage dump')
    off, arrays = 32, []
    for n in (RAMLINES,) * 4 + (PCSLOTS,) * 3:
        arrays.append(memoryview(raw)[off:off + n * 4].cast('I'))
        off += n * 4
    events = []
    if off < len(raw):
        n = struct.unpack_from('<I', raw, off)[0]
        events = memoryview(raw)[off + 4:off + 4 + 8 * n].cast('I')
    return hdr[1:], arrays, events


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('elf')
    ap.add_argument('dumps', nargs='+')
    ap.add_argument('--top', type=int, default=30)
    ap.add_argument('--json')
    args = ap.parse_args()
    data, funcs, heapstart = symbols(args.elf)
    daddrs = [d[0] for d in data]
    faddrs = [f[0] for f in funcs]

    def funcname(pc):
        j = bisect.bisect_right(faddrs, pc) - 1
        return funcs[j][2] if j >= 0 and pc < funcs[j][0] + funcs[j][1] else f'?{pc:06x}'

    frames = 0
    tot = [0, 0, 0]
    misses = 0
    obj = collections.defaultdict(lambda: [0, 0, 0, 0])   # fills, once, twice, once with dirty victim
    fn = collections.defaultdict(lambda: [0, 0, 0])
    pairs = collections.Counter()
    for path in args.dumps:
        frames += json.load(open(path + '.json'))['game_frames']
        mt, _, _, blocks, owner = load_dump(path)
        misses += mt[1] + mt[2]
        t, (lfill, lonce, ltwice, lwb, pfill, ponce, ptwice), events = load_usage(path + '.usage')
        tot = [a + b for a, b in zip(tot, t)]
        def objname(line):
            addr = line * 16
            if addr >= heapstart and owner[line]:
                baddr, bsize, caller, entry = blocks[owner[line] - 1]
                return f'heap: {funcname(caller & 0x7fffff)}'
            j = bisect.bisect_right(daddrs, addr + 15) - 1
            return data[j][2] if j >= 0 and addr < data[j][0] + data[j][1] else f'(other {addr >> 16:02x}xxxx)'
        for i in range(0, len(events), 2):
            pairs[(objname(events[i]), funcname(events[i + 1] & 0x7fffff))] += 1
        for line in range(RAMLINES):
            if not lfill[line]:
                continue
            name = objname(line)
            o = obj[name]
            o[0] += lfill[line]; o[1] += lonce[line]; o[2] += ltwice[line]; o[3] += lwb[line]
        for slot in range(PCSLOTS):
            if pfill[slot]:
                f = fn[funcname(slot * 4)]
                f[0] += pfill[slot]; f[1] += ponce[slot]; f[2] += ptwice[slot]
    pf = max(frames, 1)
    print(f'{len(args.dumps)} runs, {frames} game frames. Per game frame: {misses / pf:.0f} misses, '
          f'{tot[0] / pf:.0f} read-only fills, {tot[1] / pf:.0f} used once, {tot[2] / pf:.0f} used twice')
    print('\nData, per game frame: read-only fills / used once / used twice / once that also evicted a dirty line')
    for name, o in sorted(obj.items(), key=lambda kv: -kv[1][1])[:args.top]:
        print(f'  {o[0] / pf:7.0f} {o[1] / pf:7.0f} {o[2] / pf:7.0f} {o[3] / pf:7.0f}  {name}')
    print('\nCode that did the filling load, per game frame: read-only fills / once / twice')
    for name, f in sorted(fn.items(), key=lambda kv: -kv[1][1])[:args.top]:
        print(f'  {f[0] / pf:7.0f} {f[1] / pf:7.0f} {f[2] / pf:7.0f}  {name}')
    print('\nUsed-once fills by data and the code that loaded it, per game frame')
    for (o, f), n in pairs.most_common(args.top):
        print(f'  {n / pf:7.0f}  {o:40} {f}')
    if args.json:
        json.dump({'frames': frames, 'totals': tot, 'objects': obj, 'functions': fn}, open(args.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
