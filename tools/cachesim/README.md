# Data cache model

The VR4300 has an 8 KB, direct-mapped, write-back data cache with 16-byte lines, and no
counters that tell you how often it misses. Emulators don't model it either. These tools
add a model of it to the mupen64plus core, so you can see where the game's cache misses
come from and check whether a change (for example a smaller or reordered struct) helps.

## What is modelled

- Every CPU load and store to KSEG0 (0x80000000-0x9fffffff) goes through an 8 KB
  direct-mapped cache with 16-byte lines, write-back and write-allocate. Uncached (KSEG1)
  accesses are not counted.
- The `cache` instruction's data cache operations (index/hit invalidate and write-back,
  create dirty exclusive), so buffers the game invalidates before a DMA miss afterwards as
  they do on the console.
- Counts per physical 16-byte line (accesses, read misses, write misses, dirty write-backs)
  and per CPU PC (accesses, misses).
- Every call to `mempAlloc` and `memaAlloc` from boot, so heap misses can be named by the
  function that allocated the block.

- An instruction count and a 16 KB direct-mapped instruction cache with 32-byte lines, for
  code run from KSEG0. The idle thread's empty loop (`idleproc`) is left out of the count,
  since it only fills the time the game leaves over.

Not modelled: DMA by the RSP/RDP/PI (it doesn't go through the CPU
cache), and timing. The core runs its pure interpreter, so how many game frames fit in a
second differs from a console; results are given per game frame.

## Use

    tools/cachesim/build-core.sh                 # once; prints the core library path
    export M64RUN_CORE=/tmp/cachesim-core/projects/unix/libmupen64plus.so.2.0.0
    make OPTIMIZATION="-O2 -g"                   # same code; -g gives struct layouts
    tools/cachesim/csdemos.py u /tmp/cs/base     # all 14 demos, about 6 minutes
    tools/cachesim/csreport.py build/u/ge007.u.elf /tmp/cs/base/demo*.cs \
        --fields PropRecord,player --json /tmp/cs/base.json

Then make a change, rebuild, run csdemos.py and csreport.py again into another directory,
and compare:

    tools/cachesim/cscompare.py /tmp/cs/base.json /tmp/cs/new.json

To compare how much CPU work two builds do, count the same game frames in both: with
`--ticks N` csdemos.py counts N game frames from game frame `--from` (g_LvTickCount)
instead of a number of VI frames, so a faster build doesn't cover more play. Then

    tools/cachesim/csdemos.py u /tmp/cs/a --demos 0,2,3,4,5,6,7,8,9,10,11,12,13 --from 30 --ticks 200
    tools/cachesim/csspeed.py /tmp/cs/a /tmp/cs/b

prints instructions, I-cache and D-cache misses and write-backs per game frame, and a rough
cycle estimate.

US Dam 2 (demo 1) desyncs under every emulator setting, so leave it out of US reports.
csreport.py needs `pahole` (package dwarves) for `--fields`, and xvfb-run for the video
plugin (the game doesn't produce frames without one).

`m64run.py --cachesim FILE` runs the model on any test run: counting starts
`--cachesim-from` frames after the demo starts (or at that frame without `--demo`), lasts
`--cachesim-frames` frames, then FILE (counts), FILE.ram (RDRAM), FILE.blocks (heap
blocks) and FILE.json (game frames counted) are saved and the run stops. The state
checksums are switched off while counting, because they walk every prop, guard and player
each frame and would add misses the game doesn't have.

## Which members of a struct are used

`--watch [*]SYMBOL:SIZE` on csdemos.py (or `--cachesim-watch` on m64run.py) also counts
accesses and misses for each 4-byte word of one struct instance: SIZE bytes at SYMBOL, or with
`*` at the address SYMBOL holds when counting starts. Then

    tools/cachesim/cswords.py build/u/ge007.u.elf player /tmp/cs/pw/demo*.cs

lists each member's accesses and misses per game frame, and each 16-byte line's totals. For
example `--watch '*g_playerPointers:4728'` watches player 1's struct player.

## Lines used once

`csusage.py ELF DUMP...` lists read-only lines that were filled and then left the cache after
one access (or two), by data object, by the code that loaded them, and by the pair. Those
are where an uncached (KSEG1) read, or keeping the hot field somewhere denser, could help.
Needs a core built from the current patch (it writes DUMP.usage).
