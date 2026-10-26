#!/usr/bin/env python3
"""Play Rare's 14 attract-mode demos headless and compare the game state frame by frame.

  demos.py record VERSION OUTDIR      play every demo and save the state logs as a baseline
  demos.py check VERSION BASELINE     play every demo and compare against a baseline

VERSION is u or e and selects build/VERSION/ge007.VERSION.z64 and its map, so run this
from the repository root after building. Each demo is a recording of real play: the
controller input for every frame plus the random seed, which the game checks as it goes.
A demo that ends early with 'desynced' means the game no longer behaves as it did when
Rare recorded it.

'check' reports, per demo, the first game frame whose exact checksum (float bits) or
coarse checksum (positions to whole units, health to 1%) differs from the baseline.
A pure refactor should match exactly. Exits 0 when every demo matches.
"""
import concurrent.futures
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEMOS = ['Dam 1', 'Dam 2', 'Facility 1', 'Facility 2', 'Facility 3', 'Runway 1', 'Runway 2',
         'Bunker 1', 'Bunker 2', 'Silo 1', 'Silo 2', 'Frigate 1', 'Frigate 2', 'Train']


def play(version, index, outdir):
    rom = f'build/{version}/ge007.{version}.z64'
    mapfile = f'build/{version}/ge007.{version}.map'
    cmd = ['xvfb-run', '-a', '-s', '-screen 0 1024x768x24', sys.executable,
           os.path.join(HERE, 'm64run.py'), rom, '--frames', '20000', '--demo', str(index),
           '--map', mapfile, '--outdir', outdir]
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1800)
    try:
        return json.load(open(os.path.join(outdir, 'result.json')))
    except FileNotFoundError:
        return {'status': 'no result'}


def read_log(path):
    rows = []
    for line in open(path):
        timer, exact, coarse = line.split()
        rows.append((int(timer), exact, coarse))
    return rows


def play_all(version, outdir):
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        jobs = {i: pool.submit(play, version, i, os.path.join(outdir, f'demo{i:02d}'))
                for i in range(len(DEMOS))}
        return {i: job.result() for i, job in jobs.items()}


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ('record', 'check'):
        sys.exit(__doc__)
    mode, version, target = sys.argv[1:]

    if mode == 'record':
        results = play_all(version, target)
        for i, r in results.items():
            frames = len(read_log(os.path.join(target, f'demo{i:02d}', 'state.txt'))) if r['status'] == 'ok' else 0
            print(f'{DEMOS[i]:<11} {r.get("demo", r["status"]):<12} {frames} game frames')
        sys.exit(0 if all(r.get('demo') == 'ended' for r in results.values()) else 1)

    outdir = tempfile.mkdtemp(prefix='demos-')
    results = play_all(version, outdir)
    failures = 0
    for i, r in results.items():
        name = f'demo{i:02d}'
        base = read_log(os.path.join(target, name, 'state.txt'))
        status = r.get('demo', r['status'])
        if r['status'] != 'ok':
            print(f'{DEMOS[i]:<11} FAIL  {r["status"]} at frame {r.get("frame")} pc {r.get("pc")}')
            failures += 1
            continue
        new = read_log(os.path.join(outdir, name, 'state.txt'))
        exact = next((n for n, (a, b) in enumerate(zip(base, new)) if a[:2] != b[:2]), None)
        coarse = next((n for n, (a, b) in enumerate(zip(base, new)) if (a[0], a[2]) != (b[0], b[2])), None)
        if len(base) != len(new) and exact is None:
            exact = min(len(base), len(new))
        try:
            base_status = json.load(open(os.path.join(target, name, 'result.json'))).get('demo')
        except FileNotFoundError:
            base_status = 'ended'
        if exact is None and status == base_status:
            note = '' if status == 'ended' else f', {status} like the baseline'
            print(f'{DEMOS[i]:<11} same  ({len(new)} frames{note})')
            continue
        failures += 1
        detail = f'exact differs from frame {exact}' if exact is not None else 'exact same'
        if coarse is not None:
            detail += f', coarse from frame {coarse}'
        print(f'{DEMOS[i]:<11} DIFF  {status}, {detail} ({len(new)} vs {len(base)} frames)')
    print(f'results in {outdir}')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
