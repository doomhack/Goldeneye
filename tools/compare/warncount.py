#!/usr/bin/env python3
"""Count compiler warnings per source file, to track cleanup progress.

  tools/compare/warncount.py [VERSION=US|EU] [--flags '-Wall -Wextra'] [--csv FILE] [--show FILE]

The normal build compiles with -w (all warnings off). This takes the build's own compile
commands (from 'make -n'), swaps -w for the warning flags, and checks each C file under
src/ with -fsyntax-only. Run from the repository root. Prints a table of warning counts
per file and per warning kind; --show prints the full warnings for one file.
"""
import argparse
import collections
import concurrent.futures
import os
import re
import shlex
import subprocess


def compile_commands(version):
    code = 'u' if version == 'US' else 'e'
    out = subprocess.run(['make', '-n', '-B', f'VERSION={version}', f'build/{code}/ge007.{code}.elf'],
                         capture_output=True, text=True).stdout
    commands = {}
    for line in out.splitlines():
        if ' -c ' not in line or '-gcc' not in line.split()[0]:
            continue
        args = shlex.split(line)
        src = args[-1]
        if src.startswith('src/') and src.endswith('.c'):
            commands[src] = args
    return commands


def check(args, flags):
    cmd = []
    skip = False
    for a in args:
        if skip:
            skip = False
            continue
        if a == '-o':
            skip = True
            continue
        if a in ('-w', '-Werror', '-MMD', '-MP'):
            continue
        cmd.append(a)
    cmd[1:1] = flags + ['-o', '/dev/null']
    return subprocess.run(cmd, capture_output=True, text=True).stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('version', nargs='?', default='VERSION=US')
    ap.add_argument('--flags', default='-Wall -Wextra')
    ap.add_argument('--csv')
    ap.add_argument('--show')
    args = ap.parse_args()
    version = args.version.split('=')[-1]

    commands = compile_commands(version)
    flags = args.flags.split()
    if args.show:
        print(check(commands[args.show], flags))
        return

    per_file = {}
    kinds = collections.Counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        results = dict(zip(commands, pool.map(lambda a: check(a, flags), commands.values())))
    for src, stderr in sorted(results.items()):
        found = re.findall(r'^' + re.escape(src) + r':\d+:\d+: warning: .*\[(-W[^\]=]+)', stderr, re.M)
        per_file[src] = len(found)
        kinds.update(found)

    total = sum(per_file.values())
    clean = sum(1 for n in per_file.values() if n == 0)
    print(f'{total} warnings in {len(per_file)} files ({clean} files clean)\n')
    print('By kind:')
    for kind, n in kinds.most_common():
        print(f'  {n:6} {kind}')
    print('\nWorst files:')
    for src, n in sorted(per_file.items(), key=lambda kv: -kv[1])[:25]:
        print(f'  {n:6} {src}')
    if args.csv:
        with open(args.csv, 'w') as f:
            f.write('file,warnings\n')
            for src, n in sorted(per_file.items()):
                f.write(f'{src},{n}\n')


if __name__ == '__main__':
    main()
