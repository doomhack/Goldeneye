# Testing changes

The build no longer matches the original ROM, so these tools are what tell us a change
didn't break something by accident. Pick the cheapest check that fits the change.

| Change | Check | Time |
| --- | --- | --- |
| Renames, comments, moving files | `tools/compare/romcmp.sh` reports byte-identical ROMs | about 4 min |
| Moving code between files, new headers, deleting dead code | `tools/compare/compare.sh` reports no changed functions | about 2 min |
| Anything that changes generated code | `tools/testrun/demos.py check` matches the baseline | about 4 min |
| Rewriting a leaf module (allocators, maths, decompression) | `tools/hosttest/run.sh`, plus the demos | seconds |
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

## Comparing ROMs: `tools/compare/romcmp.sh [BASE] [VERSION]`

Builds this tree and `BASE` (default `HEAD`) with the same fixed build date and checks that
the two ROMs are byte-identical. Symbol names never reach the ROM, so any rename passes.
The one catch is `assert`, which puts its file's path into the ROM: moving a file that
uses `assert` changes a string.

To rename things, use `tools/rename.py OLD NEW [OLD NEW ...]` (or `-f list.txt`). It
replaces whole words in `src/`, `include/`, `assets/`, `ld/` and `docs/`, and refuses a new
name that is already in use. It leaves C string literals alone, because debug messages
often contain function names and changing them would move data in the ROM.

Struct fields need `tools/renamefield.py TYPE OLD NEW [...]` instead, because names like
`unk04` are shared by many structs. Rename the field in the struct definition by hand, then
run it: it builds US and PAL, and renames each use the compiler reports as "TYPE has no
member named OLD", until there are none left.

Locals and parameters need `tools/renamelocal.py list.txt`, because names like `sp34` or
`arg0` repeat in every function. The list names a file (`@ src/game/world/bg.c`) and then
one function per line (`bgLoadRoom: arg2=dst sp50=size`); each rename only
touches that function, and is refused when the new name is already used there. Afterwards
`tools/syncprotos.py` copies the new parameter names into the prototypes.

## Comparing code: `tools/compare/compare.sh [BASE] [VERSION]`

Builds the C objects of `BASE` (default `HEAD`) in a temporary worktree and compares every
function and data object with your tree. Relocated fields are compared by what they point
at, so moving code around doesn't count as a change, and renamed functions are matched by
content. Use it for any change that should leave the machine code alone.

`VERBOSE=1` shows the first difference in each changed function and `KEEP=1` keeps both
sets of objects for `objdump`. `tools/compare/codecmp.py OLD_DIR NEW_DIR` does the
comparison on two object directories.

A change in which declarations a file sees (adding a header, say) can make GCC schedule or
allocate registers slightly differently in unrelated functions of that file. The code
still does the same thing, but compare.sh reports it as changed; check those with the
demo replays.

## Demo replays: `tools/testrun/demos.py`

GoldenEye has 14 attract-mode demos: Rare's recordings of real play on Dam, Facility, Runway,
Bunker, Silo, Frigate and Train. Each stores the controller input for every frame and the
random seed, and the game stops a demo if its random seed drifts from the recording.

```
tools/testrun/demos.py record u /tmp/baseline   # on the commit you start from
tools/testrun/demos.py check u /tmp/baseline    # after your change
```

The game logs two checksums every frame while a demo plays (`src/game/replay/testhooks.c`): an exact
one over the players' and guards' positions, health and armour, objective states and the
random seed, and a coarse one that rounds positions to whole units and health to 1%. `check`
prints the first frame where each one differs from the baseline. The hooks do nothing unless
the test runner switches them on, so tests run the same ROM that ships.

On master at the start of the rewrite, 13 of the 14 demos play to the end in sync with Rare's
recordings. Dam 2 desyncs after 87 frames.

## Host unit tests: `tools/hosttest/run.sh`

Some modules don't touch the hardware at all, so they can be compiled for the PC and tested
there, quickly and with the address and undefined-behaviour sanitisers on. `run.sh` builds each
test with the host's own `cc` and runs it. A test includes the module's `.c` file directly, so it
can check private state, and replaces anything hardware-specific with a stub
(`tools/hosttest/include/ultra64.h` stands in for the N64 headers; `dbgFatal` jumps back to the
test that expected the error).

| Test | Covers |
| --- | --- |
| `memtest.c` | memp (pools, resizing, closed and full pools) and mema (best fit, merging, double frees, a full free list, and random alloc/free/resize runs checked against a byte map) |
| `rziptest.c` | rzip: zlib-made streams (stored, fixed and dynamic blocks, several levels and strategies, up to 300 KB) decompressed separately and in place, a buffer one byte too small, hand-made bad blocks, and bit-flipped streams that must fail without overrunning. Needs zlib (`-lz`) |
| `mathtest.c` | The maths library: sin, cos, atan2, acos and rounding against the C library's double-precision functions (with Rare's accuracy limits written down as tolerances); matrices (rotation order, the three inverses, scaling, the 3x3 ones), cameras and aligns, fixed-point conversion, quaternions (slerp, splines), interpolation, physics, and ray-triangle hits |
| `textest.c` | The texture decoder: decodes every texture in the ROM (from `assets/images/combined/combined.bin`, so it needs a built ROM and is skipped without one), compares each with its hash in `texhashes.txt` (`TEXTEST_WRITE=1` rewrites them, for a change meant to change textures), and checks a buffer 8 bytes too small is refused without being overrun |

Demos can't tell whether a leaf function is right in the cases a level never hits; these tests
can, so a leaf rewrite gets one.

## Menus: `tools/testrun/menus.py`

The demos start straight in a level, so they never show the front end. `menus.py` starts from
power-on and presses its way through the title, file select, mode select, mission select (Dam),
difficulty and briefing pages, into the level and through the watch menu's pages, taking a
screenshot at each step. It plays with a rumble pak plugged in (`m64run.py --pak`), since the
pak code only runs when one is present and real consoles usually have one.

```
tools/testrun/menus.py record u /tmp/menus-base   # on the commit you start from
tools/testrun/menus.py check u /tmp/menus-base    # after your change
```

The front-end screens must match the baseline pixel for pixel (a diff image is saved for any that
don't). The title sequence and the in-level shots vary from run to run with the emulator's timing,
so `check` saves those side by side with the baseline for a person to look at.

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

`--dumpram` saves RDRAM at the end of a run too (`end-ram.bin`). `--readwatch START:END` logs every
CPU read and write of an address range (hex, END exclusive) to `access.txt` as `frame pc read|write
address`: the way to find which code touches a variable. It runs the slow interpreter.

## Finding which file broke a build: `tools/testrun/objbisect.py`

Given a good and a bad build of the same source tree, it mixes object files from the two,
relinks, and runs a test command until it finds the object whose bad version fails.

## Global layout: `make SHUFFLE=<seed>`

GCC may place globals in any order, so the code must not depend on their layout: no reading one
variable as the continuation of another, no running off the end of a table. A shuffle build checks
this. It compiles the game code with one section per variable, and `tools/testrun/shuffle.py`
rewrites the linker script so each object's variables come in a random order (from the seed),
with a guard gap after each one. Gaps in loaded data are filled with `0xA5`; gaps in `.bss` start
as zero.

```
make VERSION=US SHUFFLE=1            # in a clean tree: the objects differ from a normal build
DEMOS_DUMPRAM=1 tools/testrun/demos.py check u /tmp/baseline
tools/testrun/shuffle.py check build/u/ge007.u.elf /tmp/demos-XXXX/demo*/*-ram.bin
tools/testrun/menus.py check u /tmp/menus-base
```

A demo or menu that plays differently means some code reads past a variable; `shuffle.py check`
lists every guard gap that something wrote to. Try several seeds. To narrow a failure down,
`SHUFFLE_ONLY=a.o,b.o` (parts of object paths) shuffles only those objects, and `m64run.py
--readwatch` on a suspect gap names the code that reads it.

## Warning counts: `tools/compare/warncount.py`

The build compiles with `-w`. This rechecks every C file with `-Wall -Wextra` and prints
warning counts per file and per kind, to track the cleanup.

## Dead code: `tools/compare/deadcode.py [VERSION]`

Lists every function and variable in `src/` that nothing can reach from the boot code. It
rebuilds the sources at `-O0` with one section per function into a scratch folder and lets
`ld --gc-sections` say what it would throw away (the ROM build is untouched). Run it before
and after a deletion: anything new on the list was only used by what you deleted.

It answers "is this reachable", not "is this safe to delete": a call to a dead function can
still sit inside live code (for example in a branch that never runs), so delete the call
sites by hand and rebuild, rather than deleting every line that names the function.

Two more traps:

* Run it for both `US` and `EU` and only delete what is dead in both: code behind
  `#ifdef VERSION_EU` is invisible to the US run, and the other way round.
* A `const` variable whose value the compiler can see is folded into its users even at
  `-O0`, so it shows up as dead data although the source still reads it. Check for
  remaining references before deleting data.

## Flag branches: `tools/compare/foldflags.py FILE...`

Removes `#if` branches that come out the same way in the US and the PAL build: `#if 0`
blocks, flags no build defines, and fixes both builds take. It knows each build's flags
(listed at the top of the script, matching the Makefile) and leaves any condition that uses
another macro alone. The ROMs stay byte-identical, so check the result with `romcmp.sh` for
both versions.
