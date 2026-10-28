#!/usr/bin/env python3
"""Click through the front-end menus headless and compare screenshots with a baseline.

  menus.py record VERSION OUTDIR      play the menu script and save the screenshots as a baseline
  menus.py check VERSION BASELINE     play it again and compare each screenshot with the baseline

The demos (demos.py) start straight in a level, so they never show the menus. This script
starts from power-on with a fresh save and presses its way through: title, file select, mode
select, mission select (Dam), difficulty, the briefing pages, the start of the level, and the
watch menu's pages. Each step is a screenshot; 'check' prints how many pixels differ from the
baseline and writes OUTDIR/diff-NN.png for each one that does. Garbled text or a hang shows
up here.

The front-end screens match exactly from run to run. The title sequence and the level do not (the
emulator's timing varies between runs, even for the same ROM), so those shots are not
compared: 'check' saves them side by side with the baseline (OUTDIR/look-NN.png)
for a person to look at.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

# Controller input: FROM TO PAD BUTTONS, timed in frames from power-on (see m64run.py).
SCRIPT = """\
1500 1502 1 START
1800 1802 1 A
1950 1952 1 A
2100 2102 1 A
2250 2252 1 A
2400 2402 1 A
2550 2552 1 A
2700 2702 1 START
3600 3602 1 START
3800 3802 1 R
3900 3902 1 R
4000 4002 1 R
"""

# Screenshot frames, what they show, and whether they must match the baseline exactly.
SHOTS = [
    (1350, 'title', False),
    (1700, 'file select', True),
    (1940, 'mode select', True),
    (2090, 'mission select', True),
    (2240, 'difficulty', True),
    (2390, 'briefing: objectives', True),
    (2540, 'briefing: background', True),
    (2690, 'briefing: M', True),
    (3300, 'level start', False),
    (3550, 'in level', False),
    (3700, 'watch: main', False),
    (3850, 'watch: inventory', False),
    (3950, 'watch: controls', False),
    (4100, 'watch: options', False),
]
FRAMES = 4150


def play(version, outdir):
    os.makedirs(outdir, exist_ok=True)
    script = os.path.join(outdir, 'menu-script.txt')
    open(script, 'w').write(SCRIPT)
    rom = f'build/{version}/ge007.{version}.z64'
    cmd = ['xvfb-run', '-a', '-s', '-screen 0 1024x768x24', sys.executable,
           os.path.join(HERE, 'm64run.py'), rom, '--frames', str(FRAMES), '--script', script,
           '--shots', ','.join(str(f) for f, _, _ in SHOTS), '--outdir', outdir]
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1800)
    shots = sorted(f for f in os.listdir(outdir) if f.endswith('.png') and f.startswith('ge007'))
    names = []
    for i, name in enumerate(shots):
        new = os.path.join(outdir, f'shot-{i:02d}.png')
        os.replace(os.path.join(outdir, name), new)
        names.append(new)
    return names


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ('record', 'check'):
        sys.exit(__doc__)
    mode, version, target = sys.argv[1:]
    if mode == 'record':
        shots = play(version, target)
        print(f'{len(shots)} of {len(SHOTS)} screenshots saved in {target}')
        sys.exit(0 if len(shots) == len(SHOTS) else 1)

    from PIL import Image, ImageChops

    outdir = tempfile.mkdtemp(prefix='menus-')
    shots = play(version, outdir)
    failures = 0
    for i, (frame, what, exact) in enumerate(SHOTS):
        base = os.path.join(target, f'shot-{i:02d}.png')
        if i >= len(shots):
            print(f'{i:02d} {what:<22} MISSING (the run ended early: hung?)')
            failures += 1
            continue
        a = Image.open(base).convert('RGB')
        b = Image.open(shots[i]).convert('RGB')
        if not exact:
            both = Image.new('RGB', (a.width + b.width, a.height))
            both.paste(a, (0, 0))
            both.paste(b, (a.width, 0))
            both.save(os.path.join(outdir, f'look-{i:02d}.png'))
            print(f'{i:02d} {what:<22} look  (baseline left, this build right)')
            continue
        diff = ImageChops.difference(a, b)
        r, g, b = diff.split()
        mask = ImageChops.lighter(ImageChops.lighter(r, g), b).point(lambda v: 255 if v else 0)
        changed = mask.histogram()[255]
        if changed:
            mask.save(os.path.join(outdir, f'diff-{i:02d}.png'))
            print(f'{i:02d} {what:<22} DIFF  {changed} pixels')
            failures += 1
        else:
            print(f'{i:02d} {what:<22} same')
    print(f'results in {outdir}')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
