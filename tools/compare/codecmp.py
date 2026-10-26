#!/usr/bin/env python3
"""Compare the machine code of two builds function by function.

  codecmp.py OLD_DIR NEW_DIR [--verbose]

OLD_DIR and NEW_DIR are object directories, e.g. build/u/src from two checkouts.
Every function and data object in every .o file is compared after normalising
relocations: a relocated field is compared by what it points at (symbol name
plus offset, or the string it points to), not by its raw bits. So moving a
function to another file, reordering data or deleting unrelated code doesn't
count as a change, but any different instruction does.

Functions and objects that only exist on one side are matched up by content,
so a rename shows up as a rename, and references to a renamed symbol compare
equal. Exits 0 if all code and data is the same.
"""
import argparse
import collections
import hashlib
import os
import struct
import sys

SHT_SYMTAB, SHT_REL = 2, 9
STT_OBJECT, STT_FUNC, STT_SECTION = 1, 2, 3
R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, R_MIPS_GPREL16 = 2, 4, 5, 6, 7


class Elf:
    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        shoff, = struct.unpack_from('>I', d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from('>HHH', d, 0x2e)
        self.secs = []
        for i in range(shnum):
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from(
                '>10I', d, shoff + i * shentsize)
            self.secs.append(dict(name=name, type=typ, off=off, size=size, link=link, info=info))
        strtab = self.secs[shstrndx]
        for s in self.secs:
            s['name'] = self.cstr(strtab['off'] + s['name'])
        self.syms = []
        for s in self.secs:
            if s['type'] == SHT_SYMTAB:
                st = self.secs[s['link']]
                for j in range(s['size'] // 16):
                    nm, val, sz, info, oth, shn = struct.unpack_from('>IIIBBH', d, s['off'] + j * 16)
                    self.syms.append(dict(name=self.cstr(st['off'] + nm), value=val, size=sz,
                                          type=info & 0xf, bind=info >> 4, shndx=shn))
        self.rels = {}
        for s in self.secs:
            if s['type'] == SHT_REL:
                target = s['info']
                entries = []
                for j in range(s['size'] // 8):
                    off, info = struct.unpack_from('>II', d, s['off'] + j * 8)
                    entries.append((off, info >> 8, info & 0xff))
                self.rels[target] = entries

    def cstr(self, o):
        return self.d[o:self.d.index(b'\0', o)].decode('latin1')

    def data(self, shndx):
        s = self.secs[shndx]
        if s['type'] == 8:  # NOBITS
            return bytes(s['size'])
        return self.d[s['off']:s['off'] + s['size']]

    def describe(self, shndx, addend):
        """Name a place in a section: a symbol + offset, or the string stored there."""
        best = None
        for sym in self.syms:
            if sym['shndx'] == shndx and sym['type'] in (STT_OBJECT, STT_FUNC) and sym['name']:
                if sym['value'] <= addend < sym['value'] + max(sym['size'], 1):
                    best = sym
        if best:
            return f"{best['name']}+{addend - best['value']}"
        data = self.data(shndx)
        if 0 <= addend < len(data):
            end = data.find(b'\0', addend)
            if end > addend and all(32 <= c < 127 or c in (9, 10) for c in data[addend:end]):
                return repr(data[addend:end].decode('latin1'))
        return f"{self.secs[shndx]['name']}+{addend:#x}"


def normalise(elf, shndx, start, size):
    """The bytes of [start, start+size) with relocated fields replaced by what they point at."""
    data = bytearray(elf.data(shndx)[start:start + size])
    refs = []
    rels = [r for r in elf.rels.get(shndx, []) if start <= r[0] < start + size]
    pending_hi = {}
    for off, symi, typ in rels:
        sym = elf.syms[symi]
        word, = struct.unpack_from('>I', elf.data(shndx), off)
        rel = off - start
        if typ == R_MIPS_32:
            addend, mask = word, 0xffffffff
        elif typ == R_MIPS_26:
            addend, mask = (word & 0x3ffffff) << 2, 0x3ffffff
        elif typ == R_MIPS_HI16:
            pending_hi[symi] = word & 0xffff
            addend, mask = None, 0xffff
        elif typ in (R_MIPS_LO16, R_MIPS_GPREL16):
            lo = word & 0xffff
            lo -= 0x10000 if lo & 0x8000 else 0
            addend = (pending_hi.pop(symi, 0) << 16) + lo if typ == R_MIPS_LO16 else lo
            mask = 0xffff
        else:
            addend, mask = word, 0xffffffff
        struct.pack_into('>I', data, rel, word & ~mask & 0xffffffff)
        if sym['type'] == STT_SECTION or not sym['name']:
            target = elf.describe(sym['shndx'], addend) if addend is not None else elf.secs[sym['shndx']]['name']
        else:
            target = sym['name'] if addend in (None, 0) else f"{sym['name']}+{addend}"
        refs.append((rel, typ, target))
    return bytes(data), refs


def load(root):
    """name -> (kind, object file, bytes, refs) for every function and data object under root."""
    items = {}
    for dirpath, _, files in os.walk(root):
        for f in sorted(files):
            if not f.endswith('.o'):
                continue
            path = os.path.join(dirpath, f)
            elf = Elf(path)
            rel = os.path.relpath(path, root)
            for sym in elf.syms:
                if sym['type'] not in (STT_OBJECT, STT_FUNC) or not sym['name'] or sym['shndx'] in (0, 0xfff1, 0xfff2):
                    continue
                if sym['shndx'] >= len(elf.secs):
                    continue
                key = sym['name'] if sym['bind'] else f"{rel}:{sym['name']}"
                kind = 'function' if sym['type'] == STT_FUNC else 'data'
                body, refs = normalise(elf, sym['shndx'], sym['value'], sym['size'])
                items[key] = (kind, rel, body, refs)
    return items


def shape(item):
    """A hash of an item that ignores the names it refers to, for matching renames."""
    kind, _, body, refs = item
    return hashlib.sha1(body + repr([(r, t) for r, t, _ in refs]).encode()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('old')
    ap.add_argument('new')
    ap.add_argument('--verbose', action='store_true')
    args = ap.parse_args()

    old, new = load(args.old), load(args.new)
    only_old = {k for k in old if k not in new}
    only_new = {k for k in new if k not in old}

    # Pair up renamed items: same shape and only one candidate each way.
    by_shape_old = collections.defaultdict(list)
    for k in only_old:
        by_shape_old[shape(old[k])].append(k)
    by_shape_new = collections.defaultdict(list)
    for k in only_new:
        by_shape_new[shape(new[k])].append(k)
    renames = {}
    for h, ks in by_shape_old.items():
        if len(ks) == 1 and len(by_shape_new.get(h, [])) == 1:
            renames[ks[0]] = by_shape_new[h][0]
    plain = {k.split(':', 1)[-1]: v.split(':', 1)[-1] for k, v in renames.items()}

    def rename_ref(target):
        name, sep, rest = target.partition('+')
        return plain.get(name, name) + sep + rest

    changed = []
    for k in sorted(old):
        nk = k if k in new else renames.get(k)
        if nk is None:
            continue
        kind, obj, body, refs = old[k]
        _, nobj, nbody, nrefs = new[nk]
        refs = [(r, t, rename_ref(x)) for r, t, x in refs]
        if body != nbody or refs != nrefs:
            changed.append((kind, k, obj))
            if args.verbose:
                for a, b in zip(refs, nrefs):
                    if a != b:
                        print(f'  {k}: {a} -> {b}')
                        break

    removed = sorted(only_old - set(renames))
    added = sorted(only_new - set(renames.values()))
    for kind, k, obj in changed:
        print(f'changed {kind}: {k} ({obj})')
    for k in sorted(renames):
        print(f'renamed: {k} -> {renames[k]}')
    for k in removed:
        print(f'removed {old[k][0]}: {k}')
    for k in added:
        print(f'added {new[k][0]}: {k}')
    total = sum(1 for k in old if old[k][0] == 'function')
    print(f'{total} functions compared: {len(changed)} changed, {len(renames)} renamed, '
          f'{len(removed)} removed, {len(added)} added')
    sys.exit(1 if changed or removed or added else 0)


if __name__ == '__main__':
    main()
