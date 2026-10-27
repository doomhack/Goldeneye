#!/usr/bin/env python3
"""
List functions and data that nothing in the game can reach.

    tools/compare/deadcode.py [VERSION]

Recompiles every source file under src/ with -ffunction-sections -fdata-sections into a
scratch folder, links the objects with `ld --gc-sections` starting from the boot code, and
prints what the linker throws away: one line per dead function or variable, with its
object file and size. Files are built at -O0 so that inlining can't hide a call. Assets aren't linked in, so nothing they point at is kept alive by them
(game code never relies on that).

The ROM build itself is untouched. VERSION is US (default) or EU.
"""
import os, re, shlex, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

version = sys.argv[1] if len(sys.argv) > 1 else 'US'
code = {'US': 'u', 'EU': 'e'}[version]
root = subprocess.run(['git', 'rev-parse', '--show-toplevel'], capture_output=True, text=True).stdout.strip()
os.chdir(root)

dry = subprocess.run(['make', '-B', '-n', 'VERSION=' + version], capture_output=True, text=True).stdout
cmds = [l for l in dry.split('\n')
        if re.match(r'^mips-linux-gnu-(gcc -c|as) ', l) and re.search(r' src/\S+\.[cs]$', l)]

out = tempfile.mkdtemp(prefix='deadcode-')
jobs = []
for cmd in cmds:
    args = shlex.split(cmd)
    i = args.index('-o')
    obj = os.path.join(out, os.path.relpath(args[i + 1], 'build/' + code))
    os.makedirs(os.path.dirname(obj), exist_ok=True)
    args[i + 1] = obj
    args = [a for a in args if a not in ('-MMD', '-MP')]
    if args[0].endswith('gcc'):
        # -O0 keeps every call: at -O2 a function inlined into all its callers, or a call
        # to an empty function, would look unreferenced.
        args = [a for a in args if not a.startswith('-O')]
        args[2:2] = ['-O0', '-fno-inline', '-ffunction-sections', '-fdata-sections']
    jobs.append((args, obj))

def run(job):
    r = subprocess.run(job[0], capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr)
    return job[1]

with ThreadPoolExecutor(os.cpu_count()) as ex:
    objs = list(ex.map(run, jobs))

r = subprocess.run(['mips-linux-gnu-ld', '--gc-sections', '--print-gc-sections', '-e', '_start', '-u', 'init',
                    '--unresolved-symbols=ignore-all', '--no-warn-mismatch', '--noinhibit-exec',
                    '-o', os.path.join(out, 'game.elf')] + objs, capture_output=True, text=True)

dead = []
for line in r.stderr.split('\n'):
    m = re.search(r"removing unused section '([^']+)' in file '([^']+)'", line)
    if not m:
        continue
    sec, obj = m.groups()
    sm = re.match(r'\.(text|data|rodata|bss|sdata|sbss)\.(.+)$', sec)
    if not sm:
        continue
    kind = 'func' if sm.group(1) == 'text' else 'data'
    dead.append((os.path.relpath(obj, out), kind, sm.group(2)))

sizes = {}
for obj in set(d[0] for d in dead):
    ro = subprocess.run(['mips-linux-gnu-readelf', '-SW', os.path.join(out, obj)], capture_output=True, text=True).stdout
    for m in re.finditer(r'\]\s+\.(?:text|data|rodata|bss|sdata|sbss)\.(\S+)\s+\S+\s+\S+\s+\S+\s+([0-9a-f]+)', ro):
        sizes[(obj, m.group(1))] = int(m.group(2), 16)

for obj, kind, name in sorted(dead):
    print('%-40s %-4s %-40s %d' % (obj, kind, name, sizes.get((obj, name), 0)))
subprocess.run(['rm', '-rf', out])
