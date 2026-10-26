#!/usr/bin/env python3
"""Rename struct fields, letting the compiler find every use.

  tools/renamefield.py TYPE OLD NEW [TYPE OLD NEW ...]
  tools/renamefield.py -f renames.txt      (one "TYPE OLD NEW" per line, # comments allowed)

Field names like unk04 are shared by dozens of structs, so a text search can't tell which
uses belong to which struct. Instead, rename the field in the struct definition by hand
first. Then this script builds US and PAL, reads the compiler's "TYPE has no member named
OLD" errors, renames OLD to NEW at each reported place, and repeats until the build has no
such errors left. Afterwards run tools/compare/romcmp.sh: the ROM should be byte-identical.

TYPE is the struct's typedef or tag name as the compiler prints it (Model, ModelAnimation).
"""
import argparse
import os
import re
import subprocess
import sys

ERROR = re.compile(r"^(?P<file>[^:\s]+):(?P<line>\d+):(?P<col>\d+): error: "
                   r"'(?:struct |union )?(?P<type>\w+)'(?: \{aka '[^']*'\})? has no member named '(?P<old>\w+)'")


def build_errors():
    errors = set()
    for version in ('US', 'EU'):
        out = subprocess.run(['make', f'VERSION={version}', '-k', f'-j{os.cpu_count()}'],
                             capture_output=True, text=True)
        for line in (out.stdout + out.stderr).splitlines():
            m = ERROR.match(line)
            if m:
                errors.add((m['file'], int(m['line']), int(m['col']), m['type'], m['old']))
    return errors


def fix(path, lineno, col, old, new):
    lines = open(path, encoding='latin1').read().split('\n')
    text = lines[lineno - 1]
    spots = [m.start() for m in re.finditer(r'\b' + re.escape(old) + r'\b', text)]
    if not spots:
        return False
    # GCC points at the member name, or at the -> / . just before it
    best = min(spots, key=lambda s: (s < col - 3, abs(s - (col - 1))))
    lines[lineno - 1] = text[:best] + new + text[best + len(old):]
    open(path, 'w', encoding='latin1').write('\n'.join(lines))
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-f', '--file')
    ap.add_argument('triples', nargs='*')
    args = ap.parse_args()
    if len(args.triples) % 3:
        sys.exit('give TYPE OLD NEW triples')
    triples = list(zip(args.triples[::3], args.triples[1::3], args.triples[2::3]))
    if args.file:
        for line in open(args.file):
            line = line.split('#', 1)[0].split()
            if line:
                triples.append(tuple(line))
    renames = {(t, o): n for t, o, n in triples}

    os.chdir(subprocess.run(['git', 'rev-parse', '--show-toplevel'], capture_output=True,
                            text=True, check=True).stdout.strip())
    total = 0
    for attempt in range(20):
        errors = [e for e in build_errors() if (e[3], e[4]) in renames]
        if not errors:
            print(f'{total} uses renamed')
            return
        # Several errors can share a line; fix each place once per round
        for path, lineno, col, typ, old in sorted(set(errors), key=lambda e: (e[0], e[1], -e[2])):
            if fix(path, lineno, col, old, renames[(typ, old)]):
                total += 1
            else:
                print(f'{path}:{lineno}: could not find {old}', file=sys.stderr)
    sys.exit('still failing after 20 rounds')


if __name__ == '__main__':
    main()
