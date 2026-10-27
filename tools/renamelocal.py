#!/usr/bin/env python3
"""Rename local variables and parameters inside one function.

  tools/renamelocal.py renames.txt

The list names a file, then one function per line with its renames:

  @ src/game/gfx/tex.c
  texLoadTile: sp88=maskS sp84=maskT arg4=filter
  texFoo: temp_v0=count

Each OLD is replaced as a whole word (but not after . or ->) from the function's first
line, so parameters are covered, to its closing brace at column 0. Every definition of the
function is changed (US and PAL can have one each), comments inside it too.
The script refuses a rename when NEW is already used in that function, and reports any
OLD it can't find. Afterwards run tools/compare/romcmp.sh: renaming locals should leave
the ROM byte-identical (if it doesn't, GCC chose a different stack order; run the demos).
"""
import re
import sys


def find_functions(lines, name):
    """Yields (first, last) line spans of every definition of name (one per #if branch)."""
    for i, line in enumerate(lines):
        if line[:1] in (' ', '\t', '#', '/', '*', '}', '{', '') or line.startswith('//'):
            continue
        if re.search(r'\b' + re.escape(name) + r'\s*\(', line) and not line.rstrip().endswith(';'):
            # Skip prototypes split over lines: the definition reaches '{' before any ';'.
            j = i
            while j < len(lines) and '{' not in lines[j] and ';' not in lines[j]:
                j += 1
            if j < len(lines) and '{' in lines[j]:
                for k in range(i + 1, len(lines)):
                    if lines[k].startswith('}'):
                        yield i, k
                        break


def word(name):
    # A whole word that isn't a struct field (after . or ->).
    return r'(?<![.>])\b' + re.escape(name) + r'\b'


def main():
    path = None
    errors = 0
    files = {}
    for raw in open(sys.argv[1]):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        if line.startswith('@'):
            path = line[1:].strip()
            files.setdefault(path, [])
            continue
        func, _, pairs = line.partition(':')
        files[path].append((func.strip(), [p.split('=') for p in pairs.split()]))

    for path, funcs in files.items():
        lines = open(path, encoding='latin1').read().split('\n')
        for func, pairs in funcs:
            spans = list(find_functions(lines, func))
            if not spans:
                print(f'{path}: function {func} not found')
                errors += 1
                continue
            for old, new in pairs:
                found = False
                for a, b in spans:
                    body = '\n'.join(lines[a:b + 1])
                    if not re.search(word(old), body):
                        continue
                    found = True
                    if old != new and re.search(word(new), body):
                        print(f'{path}: {func}: {new} already used')
                        errors += 1
                        continue
                    lines[a:b + 1] = re.sub(word(old), new, body).split('\n')
                if not found:
                    print(f'{path}: {func}: {old} not found')
                    errors += 1
        open(path, 'w', encoding='latin1').write('\n'.join(lines))
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()
