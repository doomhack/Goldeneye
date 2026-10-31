#!/usr/bin/env python3
"""Shuffle the game's global variables in the linker script, to find code that relies on their order.

GCC is free to place globals in any order, so code must never treat neighbouring variables as one
array or struct, or run off the end of one into the next. A shuffle build checks this: the game
code is compiled with -fdata-sections (one section per variable), and this script rewrites the
linker script so every game object lists its variable sections one by one, in a random order, with
a guard gap after each. The gaps in loaded data (.data, .rodata and the small-data sections) are
filled with a poison byte; the gaps in .bss start as zero like the rest of it.

  shuffle.py ld LDSCRIPT --seed N [--gap BYTES] [--only OBJ,...]
      Rewrite LDSCRIPT in place (the Makefile does this when SHUFFLE=N is set). Each gap gets a
      symbol __shufgap_<n>; LDSCRIPT.gaps records which section comes before each one.

  shuffle.py check ELF RAMDUMP... [--gaps FILE]
      Report every guard gap whose bytes were changed in a RAM dump (8 MB RDRAM, for example
      hang-ram.bin or end-ram.bin from m64run.py --dumpram): something wrote past the end of the
      variable before it.

Run demos.py and menus.py on a shuffle build: a demo or menu that plays differently from the
normal build means some code reads past a variable; the gap check finds code that writes past one.
See docs/Testing.md.
"""
import argparse
import json
import os
import random
import re
import subprocess
import sys

POISON = 0xA5
OBJDUMP = 'mips-linux-gnu-objdump'
NM = 'mips-linux-gnu-nm'

# Per-object lines the Makefile's sed leaves in the script, for example
#   build/u/src/game/chr/chr.o (.data .data.*);
LINE = re.compile(r'^(\s*)(\S+\.o) \((\.data|\.rodata|\.bss) (?:\.data|\.rodata|\.bss)\.\*( COMMON)?\);$')
SMALL = re.compile(r'^(\s*)\* \((\.sdata|\.sbss) ')

# Objects whose layout is fixed on purpose (asset data, boot blobs) or that are not game code.
SKIP = ('libultra.a', '/assets/', 'c_data_filler', 'inflate', 'aspboot', 'gspboot',
        'stacks.o', 'cfb.o', 'rsp/')


def sections(obj):
    out = subprocess.run([OBJDUMP, '-h', obj], capture_output=True, text=True, check=True).stdout
    names = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 7 and parts[0].isdigit():
            names.append((parts[1], int(parts[2], 16)))
    return names


def game_object(obj, only):
    if only and not any(o in obj for o in only):
        return False
    return obj.endswith('.o') and '/src/' in obj and not any(s in obj for s in SKIP)


def cmd_ld(args):
    rng = random.Random(args.seed)
    args.only = [o for o in args.only.split(',') if o]
    text = open(args.ldscript).read().splitlines()
    gaps = []
    out = []
    objs = []

    def entries(obj, kind, names):
        rng.shuffle(names)
        lines = []
        for name in names:
            lines.append(f'      {obj} ({name});')
            gaps.append(f'{obj} {name}')
            lines.append(f'      __shufgap_{len(gaps) - 1} = .; . += {args.gap};')
        return lines

    for line in text:
        m = LINE.match(line)
        if m and game_object(m.group(2), args.only):
            obj, kind = m.group(2), m.group(3)
            if obj not in objs:
                objs.append(obj)
            names = sorted({n for n, size in sections(obj)
                            if size and (n == kind or n.startswith(kind + '.'))})
            out.extend(entries(obj, kind, names))
            if kind == '.bss':
                out.append(f'      {obj} (COMMON);')
            continue
        m = SMALL.match(line)
        if m:
            kind = m.group(2)
            names = []
            for obj in objs:
                names += [(obj, n) for n, size in sections(obj)
                          if size and (n == kind or n.startswith(kind + '.'))]
            rng.shuffle(names)
            for obj, name in names:
                out.append(f'      {obj} ({name});')
                gaps.append(f'{obj} {name}')
                out.append(f'      __shufgap_{len(gaps) - 1} = .; . += {args.small_gap};')
            out.append(line)
            continue
        out.append(line)

    # Poison the gaps: each output section that holds one gets a fill pattern.
    fill = '0x' + f'{POISON:02X}' * 4
    final, block = [], False
    for line in out:
        if line == '   {':
            block = False
        elif '__shufgap_' in line:
            block = True
        elif line == '   }' and block:
            line = '   } =' + fill
        final.append(line)
    text = '\n'.join(final) + '\n'
    open(args.ldscript, 'w').write(text)
    json.dump({'gap': args.gap, 'small_gap': args.small_gap, 'gaps': gaps},
              open(args.ldscript + '.gaps', 'w'))
    print(f'shuffle seed {args.seed}: {len(gaps)} variables in {len(objs)} objects')


def cmd_check(args):
    info = json.load(open(args.gaps or os.path.splitext(args.elf)[0] + '.ld.gaps'))
    syms = {}
    nm = subprocess.run([NM, args.elf], capture_output=True, text=True, check=True).stdout
    for line in nm.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2].startswith('__shufgap_'):
            syms[int(parts[2][len('__shufgap_'):])] = int(parts[0], 16)
    bad = 0
    for dump in args.ram:
        ram = open(dump, 'rb').read()
        for n, addr in sorted(syms.items()):
            what = info['gaps'][n]
            size = info['small_gap'] if ('.sdata' in what or '.sbss' in what) else info['gap']
            phys = addr & 0x1FFFFFFF
            # The dump is RDRAM as 32-bit words in host order: swap each word back.
            data = bytearray()
            start = phys & ~3
            for w in range(start, phys + size + 3, 4):
                data += ram[w:w + 4][::-1]
            data = data[phys - start:phys - start + size]
            expect = 0 if ' .bss' in what else POISON  # .sbss is loaded with .sdata
            if any(b != expect for b in data):
                print(f'{dump}: gap after {what} at {addr:08x} changed: {data.hex()}')
                bad += 1
    print(f'{bad} gaps changed')
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('ld')
    p.add_argument('ldscript')
    p.add_argument('--seed', type=int, required=True)
    p.add_argument('--gap', type=int, default=16)
    p.add_argument('--small-gap', type=int, default=8)
    p.add_argument('--only', default=os.environ.get('SHUFFLE_ONLY', ''),
                   help='comma-separated parts of object paths: shuffle only these objects '
                        '(default $SHUFFLE_ONLY), for bisecting')
    p = sub.add_parser('check')
    p.add_argument('elf')
    p.add_argument('ram', nargs='+')
    p.add_argument('--gaps', help='the .gaps file written with the linker script '
                                   '(default: next to ELF, e.g. build/u/ge007.u.ld.gaps)')
    args = ap.parse_args()
    sys.exit(cmd_ld(args) if args.cmd == 'ld' else cmd_check(args))


if __name__ == '__main__':
    main()
