#!/usr/bin/env python3
"""Compare two csreport.py --json reports (before and after a change), per game frame.

Usage: cscompare.py BEFORE.json AFTER.json [--top N]
"""
import argparse
import json


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('before')
    ap.add_argument('after')
    ap.add_argument('--top', type=int, default=20)
    args = ap.parse_args()
    a, b = json.load(open(args.before)), json.load(open(args.after))
    fa, fb = max(a['game_frames'], 1), max(b['game_frames'], 1)

    def per(r, f):
        acc, rm, wm, wb, cwb = r['totals']
        return {'accesses': acc / f, 'misses': (rm + wm) / f, 'write-backs': wb / f}

    pa, pb = per(a, fa), per(b, fb)
    print(f'per game frame        before      after     change')
    for k in pa:
        print(f'  {k:14} {pa[k]:10.0f} {pb[k]:10.0f} {100 * (pb[k] - pa[k]) / max(pa[k], 1):+9.1f}%')
    names = set(a['objects']) | set(b['objects'])
    rows = sorted(((b['objects'].get(n, 0) / fb - a['objects'].get(n, 0) / fa, n) for n in names))
    print(f'\nBiggest changes in misses per game frame (before -> after):')
    for d, n in rows[:args.top] + [r for r in rows[-args.top:] if r[0] > 0][::-1]:
        if abs(d) >= 1:
            print(f'  {d:+8.0f}  {a["objects"].get(n, 0) / fa:8.0f} -> {b["objects"].get(n, 0) / fb:8.0f}  {n}')


if __name__ == '__main__':
    main()
