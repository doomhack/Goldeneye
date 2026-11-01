#!/usr/bin/env python3
"""Name the data cache misses counted by m64run.py --cachesim.

Usage:
  csreport.py ELF DUMP [DUMP...] [--fields STRUCT,...] [--top N] [--json OUT]

ELF is the build's build/<r>/ge007.<r>.elf. Build with OPTIMIZATION="-O2 -g" (the code is the
same; the debug info gives struct layouts through pahole, from the 'dwarves' package).
DUMP is a --cachesim file; its RAM snapshot (DUMP.ram) names heap objects. Several dumps
(for example one per demo) are added together.

Prints: totals; misses per data object (globals by symbol, heap arrays by the pointers in
HEAP below); misses per function (the code that missed); and with --fields, misses per member
of the named structs, where a member is charged every miss on a 16-byte line it shares.
"""
import argparse
import bisect
import collections
import json
import re
import struct
import subprocess
import sys

RAMLINES = 0x800000 // 16
PCSLOTS = 0x800000 // 4
TOOL = 'mips-linux-gnu-'

# Heap arrays: (struct, pointer symbol, count symbol or fixed count, is the symbol an array of pointers)
HEAP = [
    ('player', 'g_playerPointers', 4, True),
    ('ChrRecord', 'g_ChrSlots', 'g_NumChrSlots', False),
    ('ChrRecord', 'g_BgChrs', 'g_NumBgChrs', False),
    ('Model', 'g_AnimModelSlots', 'g_MaxAnimModelSlots', False),
]
# Globals whose element type is a struct worth breaking down by member.
STATIC_TYPES = {'g_Props': 'PropRecord', 'g_BgRoomInfo': 's_room_info'}


def load_dump(path):
    raw = open(path, 'rb').read()
    hdr = struct.unpack_from('<6Q', raw, 0)
    if hdr[0] != 0x43534d31:
        sys.exit(f'{path}: not a cachesim dump')
    off = 48
    arrays = []
    for n in (RAMLINES, RAMLINES, RAMLINES, RAMLINES, PCSLOTS, PCSLOTS):
        arrays.append(memoryview(raw)[off:off + n * 4].cast('I'))
        off += n * 4
    blk = open(path + '.blocks', 'rb').read()
    n = struct.unpack_from('<I', blk, 0)[0]
    blocks = [struct.unpack_from('<4I', blk, 4 + 16 * i) for i in range(n)]
    owner = memoryview(blk)[4 + 16 * n:4 + 16 * n + RAMLINES * 4].cast('I')
    return hdr[1:], arrays, open(path + '.ram', 'rb').read(), blocks, owner


def symbols(elf):
    out = subprocess.run([TOOL + 'readelf', '-sW', elf], capture_output=True, text=True).stdout
    data, funcs, heap = [], [], 0
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 8 or not parts[0].endswith(':') or parts[0] == 'Num:':
            continue
        addr, size, kind, name = int(parts[1], 16), int(parts[2], 0), parts[3], parts[7]
        if name == '_heapSegmentStart':
            heap = addr & 0x7fffff
        if not 0x80000000 <= addr < 0x80800000 or size == 0:
            continue
        if kind == 'FUNC':
            funcs.append((addr & 0x7fffff, size, name))
        elif kind == 'OBJECT':
            data.append((addr & 0x7fffff, size, name))
    data = sorted(set(data)); funcs = sorted(set(funcs))
    return data, funcs, heap


def layout(elf, name):
    """Top-level members of struct NAME: [(offset, size, member)], total size."""
    txt = subprocess.run(['pahole', '-C', name, elf], capture_output=True, text=True).stdout
    members, depth, total = [], 0, None
    for line in txt.splitlines()[1:]:
        if '{' in line:
            depth += 1
        if '}' in line:
            depth -= 1
            m = re.search(r'}\s*(\w*)(\[[^\]]*\])?;\s*/\*\s*(\d+)\s+(\d+)', line)
            if depth == 0 and m:
                members.append((int(m.group(3)), int(m.group(4)), m.group(1) or '<anon>'))
            continue
        m = re.search(r'size: (\d+)', line)
        if m:
            total = int(m.group(1))
        if depth:
            continue
        m = re.match(r'\s+.*?\**(\w+)(\[[^\]]*\])?(:\d+)?;\s*/\*\s*(\d+)(?::\d+)?\s+(\d+)', line)
        if m:
            members.append((int(m.group(4)), int(m.group(5)), m.group(1)))
    return sorted(members), total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('elf')
    ap.add_argument('dumps', nargs='+')
    ap.add_argument('--fields', default='')
    ap.add_argument('--top', type=int, default=25)
    ap.add_argument('--json')
    args = ap.parse_args()

    data, funcs, heapstart = symbols(args.elf)
    daddrs = [d[0] for d in data]
    faddrs = [f[0] for f in funcs]
    sym = {name: addr for addr, size, name in data}
    symsize = {name: size for addr, size, name in data}

    def funcname(pc):
        pc &= 0x7fffff
        j = bisect.bisect_right(faddrs, pc) - 1
        return funcs[j][2] if j >= 0 and pc < funcs[j][0] + funcs[j][1] else f'?{pc:06x}'

    totals = [0] * 5
    obj = collections.Counter()           # name -> misses
    objwb = collections.Counter()
    objacc = collections.Counter()
    fn = collections.Counter()
    fnacc = collections.Counter()
    linemiss = collections.Counter()       # physical line -> misses (all dumps)
    instances = {}                         # struct -> [(start, end)]
    sizes = {}

    def struct_size(name):
        if name not in sizes:
            sizes[name] = layout(args.elf, name)
        return sizes[name][1]

    frames = 0
    for path in args.dumps:
        try:
            frames += json.load(open(path + '.json'))['game_frames']
        except (OSError, KeyError, ValueError):
            pass
        tot, (acc, rmiss, wmiss, wb, pcacc, pcmiss), ram, blocks, owner = load_dump(path)
        totals = [a + b for a, b in zip(totals, tot)]
        # mupen keeps RDRAM as host-order 32-bit words
        word = lambda a: struct.unpack_from('<I', ram, a & 0x7fffff)[0]

        # Heap regions named from the RAM snapshot.
        regions = []
        for sname, psym, count, indirect in HEAP:
            if psym not in sym:
                continue
            size = struct_size(sname)
            if not size:
                continue
            if indirect:
                for i in range(count):
                    p = word(sym[psym] + 4 * i)
                    if p:
                        regions.append(((p & 0x7fffff), size, f'{sname} ({psym}[{i}])', sname))
            else:
                p = word(sym[psym])
                n = word(sym[count]) if isinstance(count, str) else count
                if p and 0 < n < 10000:
                    regions.append(((p & 0x7fffff), size * n, f'{sname}[{n}] ({psym})', sname))
        for name, sname in STATIC_TYPES.items():
            if name in sym:
                regions.append((sym[name], symsize[name], f'{sname} ({name})', sname))
        regions.sort()
        for start, length, label, sname in regions:
            instances.setdefault(sname, set()).add((start, start + length))
        raddrs = [r[0] for r in regions]

        for line in range(RAMLINES):
            m = rmiss[line] + wmiss[line]
            w = wb[line]
            a = acc[line]
            if not (m or w):
                continue
            addr = line * 16
            linemiss[line] += m
            i = bisect.bisect_right(raddrs, addr) - 1
            if i >= 0 and addr < regions[i][0] + regions[i][1]:
                name = regions[i][2]
            elif addr >= heapstart and owner[line]:
                # The heap: name the block by the function that allocated it.
                baddr, bsize, caller, entry = blocks[owner[line] - 1]
                name = f'heap: {funcname(caller)} ({bsize} B)'
            else:
                j = bisect.bisect_right(daddrs, addr + 15) - 1
                if j >= 0 and addr < data[j][0] + data[j][1]:
                    name = data[j][2]
                else:
                    name = f'(heap/other {addr >> 16:02x}xxxx)'
            obj[name] += m; objwb[name] += w; objacc[name] += a
        for slot in range(PCSLOTS):
            if pcacc[slot]:
                pc = slot * 4
                j = bisect.bisect_right(faddrs, pc) - 1
                name = funcs[j][2] if j >= 0 and pc < funcs[j][0] + funcs[j][1] else f'?{pc:06x}'
                fn[name] += pcmiss[slot]; fnacc[name] += pcacc[slot]

    acc_t, rm, wm, wbt, cwb = totals
    pf = max(frames, 1)
    print(f'{len(args.dumps)} runs, {frames} game frames; per game frame: {(rm + wm) / pf:.0f} misses, '
          f'{wbt / pf:.0f} write-backs')
    print(f'accesses {acc_t}  read misses {rm}  write misses {wm}  hit rate {100 - 100 * (rm + wm) / max(acc_t, 1):.2f}%  '
          f'write-backs {wbt} (+{cwb} from cache ops)')
    print(f'\nTop data, per game frame: misses / write-backs / accesses')
    for name, m in obj.most_common(args.top):
        print(f'  {m / pf:8.0f} {objwb[name] / pf:8.0f} {objacc[name] / pf:9.0f}  {name}')
    print(f'\nTop functions, per game frame: misses / accesses')
    for name, m in fn.most_common(args.top):
        print(f'  {m / pf:8.0f} {fnacc[name] / pf:9.0f}  {name}')

    report = {'game_frames': frames, 'totals': totals, 'objects': dict(obj), 'writebacks': dict(objwb), 'functions': dict(fn), 'fields': {}}
    for sname in [s for s in args.fields.split(',') if s]:
        members, size = sizes.get(sname) or layout(args.elf, sname)
        if not size:
            print(f'\n{sname}: no layout (build with -g?)')
            continue
        permember = collections.Counter()
        perline = collections.Counter()
        total = 0
        for start, end in instances.get(sname, ()):
            for base in range(start, end, size):
                for line in range(base >> 4, (base + size + 15) >> 4):
                    m = linemiss[line]
                    if not m:
                        continue
                    lo, hi = max(line * 16, base) - base, min(line * 16 + 16, base + size) - base
                    total += m
                    for off, msize, mname in members:
                        if off < hi and off + msize > lo:
                            permember[mname] += m
        print(f'\n{sname} ({size} B): {total / pf:.0f} misses per game frame on its lines; per member '
              '(charged for every line it touches):')
        for off, msize, mname in members:
            if permember[mname]:
                print(f'  {permember[mname] / pf:8.1f}  +{off:<5} {msize:5}  {mname}')
        report['fields'][sname] = {'total': total, 'members': dict(permember)}
    if args.json:
        json.dump(report, open(args.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
