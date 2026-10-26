#!/usr/bin/env python3
"""Rename identifiers across the source tree.

  tools/rename.py OLD NEW [OLD NEW ...]
  tools/rename.py -f renames.txt          (one "OLD NEW" pair per line, # comments allowed)

Replaces whole-word matches in the game's C, headers, assembly and linker scripts
(src/, include/, assets/, ld/) and in docs/. In C files, text inside string literals is
left alone (debug messages often contain function names, and changing them would move
data in the ROM); comments are renamed. It refuses a rename when NEW is already
used anywhere, so two things can't silently end up with one name. Afterwards run
tools/compare/romcmp.sh: a pure rename leaves the ROM byte-identical.
"""
import argparse
import os
import re
import subprocess
import sys

ROOTS = ('src/', 'include/', 'assets/', 'ld/', 'docs/')
EXTS = ('.c', '.h', '.s', '.inc', '.def', '.ld', '.md')


# Comments first, so that quotes inside them don't start a string
LITERAL = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|[^"\'/]+|/',
                     re.S)


def files():
    out = subprocess.run(['git', 'ls-files', '--cached', '--others', '--exclude-standard'],
                         capture_output=True, text=True, check=True).stdout.split('\n')
    return [f for f in out if f.startswith(ROOTS) and f.endswith(EXTS) and os.path.isfile(f)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-f', '--file')
    ap.add_argument('pairs', nargs='*')
    args = ap.parse_args()

    pairs = list(zip(args.pairs[::2], args.pairs[1::2]))
    if len(args.pairs) % 2:
        sys.exit('give OLD NEW pairs')
    if args.file:
        for line in open(args.file):
            line = line.split('#', 1)[0].split()
            if line:
                pairs.append((line[0], line[1]))
    renames = dict(pairs)
    if len(renames) != len(pairs) or len(set(renames.values())) != len(renames):
        sys.exit('a name appears twice in the list')

    os.chdir(subprocess.run(['git', 'rev-parse', '--show-toplevel'], capture_output=True,
                            text=True, check=True).stdout.strip())
    texts = {f: open(f, encoding='latin1').read() for f in files()}
    word = re.compile(r'\b(' + '|'.join(map(re.escape, sorted(renames, key=len, reverse=True))) + r')\b')
    clash = re.compile(r'\b(' + '|'.join(map(re.escape, renames.values())) + r')\b')

    used = {}
    for f, t in texts.items():
        for m in clash.finditer(t):
            if m.group(1) not in renames:  # renaming a name that is itself renamed is fine
                used.setdefault(m.group(1), f)
    if used:
        for name, f in sorted(used.items()):
            print(f'{name} is already used in {f}', file=sys.stderr)
        sys.exit(1)

    found = set()
    changed = 0

    def rename(text):
        return word.sub(lambda m: (found.add(m.group(1)), renames[m.group(1)])[1], text)

    for f, t in texts.items():
        if f.endswith(('.c', '.h', '.inc')):
            # Rename outside string and character literals only
            new = LITERAL.sub(lambda m: m.group(0) if m.group(0)[0] in '"\'' else rename(m.group(0)), t)
        else:
            new = rename(t)
        if new != t:
            open(f, 'w', encoding='latin1').write(new)
            changed += 1
    for old in renames:
        if old not in found:
            print(f'warning: {old} not found', file=sys.stderr)
    print(f'{len(found)} names renamed in {changed} files')


if __name__ == '__main__':
    main()
