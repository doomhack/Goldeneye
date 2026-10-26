# Testing changes

The build no longer matches the original ROM, so these tools are what tell us a change
didn't break something by accident. Pick the cheapest check that fits the change.

| Change | Check | Time |
| --- | --- | --- |
| Renames, comments, moving code between files, deleting dead code | `tools/compare/compare.sh` reports no changed functions | about 1 min |
| Anything that changes generated code | `tools/testrun/demos.py check` matches the baseline | about 4 min |
| Intended behaviour change | Look at where the demo checksums diverge, then re-record the baseline | |
| End of a milestone | ares, then a real N64 and an Analogue 3D | |

Emulators don't model the N64's caches or DMA timing, so some bugs only show up on hardware.

## Setup (Linux)

* Toolchain: GCC 15 for `mips-linux-gnu`. A prebuilt static copy lives in the project files
  (`toolchain/`), or build it with `tools/toolchain/build-gcc.sh`. Put its `bin` first in `PATH`.
* Base ROMs: `baserom.u.z64` and `baserom.e.z64` in the repository root (never commit them),
  then `make build_tools extractassets`.
* Headless emulator: `apt install mupen64plus-ui-console mupen64plus-video-rice mupen64plus-rsp-hle
  libmupen64plus-dev xvfb python3-pil`.

## Comparing code: `tools/compare/compare.sh [BASE] [VERSION]`

Builds the C objects of `BASE` (default `HEAD`) in a temporary worktree and compares every
function and data object with your tree. Relocated fields are compared by what they point
at, so moving code around doesn't count as a change, and renamed functions are matched by
content. Use it for any change that should leave the machine code alone.

`tools/compare/codecmp.py OLD_DIR NEW_DIR` does the comparison on two object directories.

## Demo replays: `tools/testrun/demos.py`

GoldenEye has 14 attract-mode demos: Rare's recordings of real play on Dam, Facility, Runway,
Bunker, Silo, Frigate and Train. Each stores the controller input for every frame and the
random seed, and the game stops a demo if its random seed drifts from the recording.

```
tools/testrun/demos.py record u /tmp/baseline   # on the commit you start from
tools/testrun/demos.py check u /tmp/baseline    # after your change
```

The game logs two checksums every frame while a demo plays (`src/game/testhooks.c`): an exact
one over the players' and guards' positions, health and armour, objective states and the
random seed, and a coarse one that rounds positions to whole units and health to 1%. `check`
prints the first frame where each one differs from the baseline. The hooks do nothing unless
the test runner switches them on, so tests run the same ROM that ships.

On master at the start of the rewrite, 13 of the 14 demos play to the end in sync with Rare's
recordings. Dam 2 desyncs after 87 frames.

## Running anything else: `tools/testrun/m64run.py`

Runs a ROM headless in mupen64plus with scripted controller input, timed in frames:

```
tools/testrun/m64run.py build/u/ge007.u.z64 --frames 3000 --script inputs.txt \
    --shots 600,1200,3000 --outdir out
```

Script lines are `FROM TO PAD BUTTONS [STICKX STICKY]`, e.g. `300 302 1 START`. It can also
log a RAM word every frame (`--map build/u/ge007.u.map --watch SYMBOL`) and poke globals
(`--poke SYMBOL=VALUE`). If the game stops producing frames for `--stall` seconds, the run
counts as hung: it saves the screen the N64 is showing (usually the crash screen) to
`hang-screen.png`, all of RDRAM to `hang-ram.bin`, and the CPU's PC to `result.json`.

## Finding which file broke a build: `tools/testrun/objbisect.py`

Given a good and a bad build of the same source tree, it mixes object files from the two,
relinks, and runs a test command until it finds the object whose bad version fails.

## Warning counts: `tools/compare/warncount.py`

The build compiles with `-w`. This rechecks every C file with `-Wall -Wextra` and prints
warning counts per file and per kind, to track the cleanup.
