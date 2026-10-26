#!/usr/bin/env python3
"""Find which object file(s) make a build fail, by mixing objects from a good and a bad build.

Both trees must be full builds of the same source layout (for example the same commit built
with two compilers, or two commits where only a few files differ). The mix tree starts as a
copy of the bad tree; each step copies a subset of objects from the good tree, relinks, and
runs the test command in the mix tree. Exit status 0 means the build passed.

Usage:
  objbisect.py GOOD_TREE BAD_TREE MIX_TREE --test 'COMMAND' [--version u] [--only GLOB]

Example (does the ROM still reach the title screen?):
  objbisect.py ../ge-gcc10 ../ge-gcc15 ../ge-mix \\
      --test 'python3 tools/testrun/m64run.py build/u/ge007.u.z64 --frames 1500 --gfx rice \\
              --outdir /tmp/bisect && grep -q ok /tmp/bisect/result.json'

Prints the single object whose bad version breaks the build. If no single object explains
the failure (two objects only fail together), it says so.
"""
import argparse
import fnmatch
import os
import shutil
import subprocess
import sys


def objects(tree, version, only):
    root = os.path.join(tree, 'build', version)
    found = []
    for dirpath, _, files in os.walk(root):
        for name in files:
            if name.endswith('.o'):
                rel = os.path.relpath(os.path.join(dirpath, name), tree)
                if not only or fnmatch.fnmatch(rel, only):
                    found.append(rel)
    return sorted(found)


def run(mix, good, bad, version, good_set, all_objs, test):
    for rel in all_objs:
        src = os.path.join(good if rel in good_set else bad, rel)
        dst = os.path.join(mix, rel)
        shutil.copy2(src, dst)
        os.utime(dst)
    for name in os.listdir(os.path.join(mix, 'build', version)):
        if name.startswith('ge007.'):
            os.remove(os.path.join(mix, 'build', version, name))
    vflag = 'VERSION=EU' if version == 'e' else 'VERSION=US'
    if subprocess.run(['make', '-j4', vflag], cwd=mix, stdout=subprocess.DEVNULL,
                      stderr=subprocess.DEVNULL).returncode != 0:
        sys.exit('relink failed')
    ok = subprocess.run(test, shell=True, cwd=mix, stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL).returncode == 0
    print(f'  {len(good_set)} good objects -> {"pass" if ok else "FAIL"}', flush=True)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('good')
    ap.add_argument('bad')
    ap.add_argument('mix')
    ap.add_argument('--test', required=True)
    ap.add_argument('--version', default='u')
    ap.add_argument('--only', help='glob limiting the candidate objects (default: everything under '
                                   'build/VERSION/src; asset objects outside it always come from the good tree)')
    args = ap.parse_args()

    if not os.path.exists(args.mix):
        shutil.copytree(args.bad, args.mix, symlinks=True)
    all_objs = objects(args.bad, args.version, None)
    candidates = objects(args.bad, args.version, args.only or f'build/{args.version}/src/*')
    print(f'{len(candidates)} candidate objects')

    def check(bad_part):
        """True if the build passes with only bad_part taken from the bad tree."""
        good_set = set(all_objs) - set(bad_part)
        return run(args.mix, args.good, args.bad, args.version, good_set, all_objs, args.test)

    if check(candidates):
        sys.exit('the bad tree passes the test; nothing to bisect')
    if not check([]):
        sys.exit('the good tree fails the test too')

    suspects = candidates
    while len(suspects) > 1:
        half = suspects[:len(suspects) // 2]
        if not check(half):
            suspects = half
        elif not check(suspects[len(suspects) // 2:]):
            suspects = suspects[len(suspects) // 2:]
        else:
            print('neither half fails on its own: the failure needs objects from both halves')
            print('\n'.join(suspects))
            return
    print('culprit:', suspects[0])


if __name__ == '__main__':
    main()
