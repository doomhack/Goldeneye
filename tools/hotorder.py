#!/usr/bin/env python3
"""Write the hot-function link order (ld/game.hot.ld.inc) from an ares profile.

Usage:
  hotorder.py GE007.ELF --callgrind PROFILE.callgrind [-o ld/game.hot.ld.inc]
  hotorder.py GE007.ELF --exec DUMP.exec [--exec DUMP2.exec ...]

The profile is either an ares callgrind file, or one or more per-instruction counts from the
cache model (tools/cachesim: m64run.py --cachesim FILE writes FILE.exec); for those, a
function's weight is the instructions it ran and the call counts come from how often each
jal in the ELF ran. The ELF must be the build the profile was taken from.

The VR4300 I-cache is 16 KB, direct mapped, so two hot functions whose addresses are
16 KB apart (or a multiple of it) keep evicting each other. Where every function lands
used to depend on the size of everything linked before it, so any edit could reshuffle
the conflicts (a 476-byte shift once cost 12% more I-cache misses). This script picks the
game-segment functions that cover COVERAGE of the profile's self cycles, orders them so
that callers and callees that call each other most sit next to each other (Pettis-Hansen
chain merging), puts the densest chains (cycles per byte) first, and writes one linker
line per function. ge007.ld places them at the start of the game segment, which is
aligned to 16 KB, so their cache placement only changes when hot code itself changes.

The ELF must be an LTO build with -ffunction-sections (the default), so each function
has its own .text.<name> section. Names that no longer exist are simply not matched by
the linker and those functions fall back to the normal order.

Each hot function also starts on a 32-byte I-cache line, so a short function never
straddles two lines (cache model, Facility demo 2: 2.4% fewer I-misses than ordering
alone, at ~2 KB of padding; -falign-functions=32 on all code did about the same for 11 KB).
"""
import argparse
import collections
import os
import subprocess

NM = os.environ.get('NM', '/opt/ge-gcc15/bin/mips-linux-gnu-nm')
OBJDUMP = os.environ.get('OBJDUMP', '/opt/ge-gcc15/bin/mips-linux-gnu-objdump')


def parse_callgrind(path):
    """Self cycles per function and call counts per (caller, callee)."""
    events = None
    selfc = collections.Counter()
    edges = collections.Counter()
    fn = cfn = None
    calls = None
    names = {}

    def name(v):
        # callgrind may compress names: "(id) name" defines, "(id)" reuses
        if v.startswith('('):
            i = v.index(')')
            key, rest = v[:i + 1], v[i + 1:].strip()
            if rest:
                names[key] = rest
            return names.get(key, rest)
        return v

    for line in open(path):
        line = line.rstrip('\n')
        if line.startswith('events:'):
            events = line.split()[1:]
        elif line.startswith('fn='):
            fn, cfn, calls = name(line[3:]), None, None
        elif line.startswith('cfn='):
            cfn = name(line[4:])
        elif line.startswith('calls='):
            calls = int(line[6:].split()[0])
        elif fn and line and (line[0].isdigit() or line[0] in '+-*'):
            if calls is not None and cfn:
                edges[(fn, cfn)] += calls
                calls = cfn = None
            else:
                v = line.split()
                selfc[fn] += int(v[1 + events.index('Cycles')]) if events and 'Cycles' in events else int(v[1])
    return selfc, edges


def parse_exec(elf, paths, syms):
    """Self instructions per function and call counts from cache-model .exec dumps."""
    import bisect
    import struct
    funcs = sorted((a, n) for n, (a, sz) in syms.items() if sz)
    starts = [a for a, _ in funcs]

    def owner(pc):
        i = bisect.bisect_right(starts, pc) - 1
        if i < 0:
            return None
        a, n = funcs[i]
        return n if pc < a + syms[n][1] else None

    ex = None
    for path in paths:
        b = open(path, 'rb').read()
        n = len(b) // 8
        e = struct.unpack('<%dI' % n, b[:4 * n])
        ex = list(e) if ex is None else [x + y for x, y in zip(ex, e)]
    selfc = collections.Counter()
    for k, v in enumerate(ex):
        if v:
            f = owner(0x80000000 + 4 * k)
            if f:
                selfc[f] += v
    edges = collections.Counter()
    out = subprocess.run([OBJDUMP, '-d', '--no-show-raw-insn', elf],
                         capture_output=True, text=True, check=True).stdout
    for line in out.split('\n'):
        if '\tjal\t' not in line:
            continue
        pc = int(line.split(':', 1)[0], 16)
        k = (pc - 0x80000000) // 4
        if k < 0 or k >= len(ex) or not ex[k]:
            continue
        target = int(line.split('\tjal\t', 1)[1].split()[0], 16)
        caller, callee = owner(pc), owner(target)
        if caller and callee:
            edges[(caller, callee)] += ex[k]
    return selfc, edges


def elf_functions(elf):
    syms = {}
    out = subprocess.run([NM, '-S', elf], capture_output=True, text=True, check=True).stdout
    for line in out.split('\n'):
        p = line.split()
        if len(p) == 4 and p[2] in 'tT':
            syms[p[3]] = (int(p[0], 16), int(p[1], 16))
        elif len(p) == 3 and p[1] in 'tTA':
            syms[p[2]] = (int(p[0], 16), 0)
    return syms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('elf')
    ap.add_argument('--callgrind', help='ares profile (.callgrind)')
    ap.add_argument('--exec', action='append', default=[], help='cache-model FILE.exec (repeatable)')
    ap.add_argument('-o', '--out', default='ld/game.hot.ld.inc')
    ap.add_argument('--coverage', type=float, default=0.97)
    ap.add_argument('--align', type=int, default=32,
                    help='start each hot function on this boundary (32 = one I-cache line)')
    a = ap.parse_args()

    syms = elf_functions(a.elf)
    if a.callgrind:
        selfc, edges = parse_callgrind(a.callgrind)
        source = os.path.basename(a.callgrind)
    elif a.exec:
        selfc, edges = parse_exec(a.elf, a.exec, syms)
        source = 'cache model ' + ', '.join(os.path.basename(p) for p in a.exec)
    else:
        ap.error('give --callgrind or --exec')
    gs, ge = syms['_gameSegmentStart'][0], syms['_gameSegmentEnd'][0]

    cands = [f for f in selfc if f in syms and syms[f][1] and gs <= syms[f][0] < ge]
    cands.sort(key=lambda f: -selfc[f])
    total = sum(selfc[f] for f in cands)
    hot, acc = [], 0
    for f in cands:
        if acc >= a.coverage * total:
            break
        hot.append(f)
        acc += selfc[f]
    hotset = set(hot)
    size = {f: syms[f][1] for f in hot}

    # Pettis-Hansen: merge chains along the heaviest call edges first, joining the two
    # chains in the orientation that puts the caller and callee closest together.
    chain = {f: [f] for f in hot}
    owner = {f: f for f in hot}
    weight = collections.Counter()
    for (x, y), n in edges.items():
        if x in hotset and y in hotset and x != y:
            weight[tuple(sorted((x, y)))] += n
    for (x, y), _ in sorted(weight.items(), key=lambda e: (-e[1], e[0])):
        cx, cy = owner[x], owner[y]
        if cx == cy:
            continue
        A, B = chain[cx], chain[cy]
        ia, ib = A.index(x), B.index(y)
        opts = [(A + B, len(A) - 1 - ia + ib),
                (B + A, len(B) - 1 - ib + ia),
                (A + B[::-1], len(A) - 1 - ia + len(B) - 1 - ib),
                (A[::-1] + B, ia + ib)]
        merged = min(opts, key=lambda o: o[1])[0]
        chain[cx] = merged
        del chain[cy]
        for f in merged:
            owner[f] = cx
    chains = sorted(chain.values(),
                    key=lambda c: (-sum(selfc[f] for f in c) / max(1, sum(size[f] for f in c)), c[0]))
    order = [f for c in chains for f in c]

    with open(a.out, 'w') as o:
        o.write('/* Generated by tools/hotorder.py from %s: hot game functions, placed first in\n'
                '   the game segment so their I-cache placement stays put. %d functions, %d bytes,\n'
                '   %.0f%% of the profiled game-segment cycles. */\n'
                % (source, len(order), sum(size.values()), 100 * acc / total))
        for f in order:
            o.write('      . = ALIGN(%d); *ltrans*.o (.text.%s);\n' % (a.align, f))
    print('%d functions, %d bytes, %.1f%% of game-segment self cycles -> %s'
          % (len(order), sum(size.values()), 100 * acc / total, a.out))


if __name__ == '__main__':
    main()
