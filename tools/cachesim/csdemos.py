#!/usr/bin/env python3
"""Count data cache misses over Rare's attract demos.

Usage:
  csdemos.py VERSION OUTDIR [--demos 0,1,...] [--from N] [--frames N] [--jobs N]

Plays each demo of build/<v>/ge007.<v>.z64 under the cache model (see README.md) and saves
OUTDIR/demoNN.cs (+ .ram, .blocks, .json). Counting starts --from VI frames after the demo
starts and lasts --frames VI frames. Then run csreport.py on the dumps.
"""
import argparse
import concurrent.futures
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, '..', 'testrun', 'm64run.py')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('version', choices=['u', 'e'])
    ap.add_argument('outdir')
    ap.add_argument('--demos', default=','.join(str(i) for i in range(14)))
    ap.add_argument('--from', dest='start', type=int, default=30)
    ap.add_argument('--frames', type=int, default=300)
    ap.add_argument('--ticks', type=int, help='count N game frames from game frame --from instead of VI frames')
    ap.add_argument('--watch', metavar='[*]SYMBOL:SIZE', help='passed to m64run.py --cachesim-watch')
    ap.add_argument('--jobs', type=int, default=os.cpu_count())
    args = ap.parse_args()
    if 'M64RUN_CORE' not in os.environ:
        sys.exit('set M64RUN_CORE to the cache-model core (see tools/cachesim/README.md)')
    v = args.version
    rom, mapfile = f'build/{v}/ge007.{v}.z64', f'build/{v}/ge007.{v}.map'
    os.makedirs(args.outdir, exist_ok=True)

    def play(i):
        out = os.path.join(args.outdir, f'demo{i:02d}')
        cmd = ['xvfb-run', '-a', sys.executable, RUN, rom, '--frames', '20000', '--demo', str(i),
               '--map', mapfile, '--outdir', out, '--cachesim', out + '.cs',
               '--cachesim-from', str(args.start), '--cachesim-frames', str(args.frames), '--stall', '120']
        if args.ticks:
            cmd += ['--cachesim-ticks', str(args.ticks)]
        if args.watch:
            cmd += ['--cachesim-watch', args.watch]
        r = subprocess.run(cmd, capture_output=True, text=True)
        ok = os.path.exists(out + '.cs.json')
        return i, ok, r.stdout.strip().splitlines()[-1:] if r.stdout else r.stderr[-300:]

    demos = [int(d) for d in args.demos.split(',')]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for i, ok, msg in pool.map(play, demos):
            print(f'demo {i:2}: {"ok" if ok else "FAILED"} {msg}', flush=True)


if __name__ == '__main__':
    main()
