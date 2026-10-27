# Assets and memory

How the game gets memory, and how it gets files from the cartridge into it. Everything
else that loads data (levels, models, text, textures, music) goes through these.

| File | What it holds |
| --- | --- |
| `src/memp.c` | The pool allocator: almost all the game's memory comes from it |
| `src/mema.c` | A small heap with real `free`, carved out of the stage pool, used for room geometry |
| `src/rom.c` | Copying from cartridge ROM to RAM by DMA (`romCopy`) |
| `src/game/asset/fileload.c` | The file table, and loading files by name into a pool or a buffer |
| `src/game/asset/rzip.c` | Decompressing Rare's "1172" format (deflate) |
| `src/inflate/inflate.c` | The boot code's own copy of inflate, which unpacks the game code at startup |

## Memory pools (memp)

All RAM from the end of the game's code and data (`_heapSegmentStart`) to the top of the
8 MB is split into pools. A pool is a stack: `mempAlloc` takes the next bytes, nothing is
freed on its own, and `mempResetPool` empties the whole pool at once.

Only two pools are ever used:

| Pool | Size | Lifetime |
| --- | --- | --- |
| `MEMPOOL_STAGE` | everything else (about 5.7 MB) | emptied each time a level or the title screen loads |
| `MEMPOOL_PERMANENT` | 296 KB | filled once at boot (menu text, for example) |

The other `MEMPOOL_` values are leftovers and are always empty. The sizes come from
command-line tokens: `-ml` sizes the stage pool and `-me` the permanent pool (each size goes
to the pool *after* the one its name suggests), and boss.c always sets `-ml0 -me0`, which
means "permanent gets 296 KB, stage gets the rest".

There is no error handling. A full or closed pool hangs the game, with one exception: when
the stage pool is full, `mempAlloc` takes the memory from the permanent pool instead.
`mempResizeLast` can grow or shrink only the most recent allocation in a pool.

## The room heap (mema)

mema is a classic free-list heap: `memaAlloc` and `memaFree`. Its heap is one allocation in
the stage pool, sized by the level's `-ma` token in boss.c (150 to 400 KB, doubled in co-op
because two players see more rooms at once). Only `world/bg.c` uses it, for the geometry of
rooms as they come into view and are dropped again.

It tracks free space, not allocations: an array of (address, size) entries, one per free
block. So `memaFree` needs the caller to pass the size. When the array gets fragmented,
`memaDefrag` sorts and merges neighbouring entries; the data itself never moves.

## ROM access

`romCopy(dst, romAddr, size)` copies from the cartridge and waits until the copy has
finished. The ROM address must be even. The target should be 16-byte aligned: a target
that isn't, and a partial cache line at the end, go through a small bounce buffer, because
a DMA into a cache line the CPU is also using gets overwritten on real hardware (but not in
emulators). `romCopyUnaligned` copies from an odd ROM address.

## Files

Every file the game loads by name has an entry in `g_FileTable` (generated into
`assets/obseg/file_resource_table.inc.c`): its name and ROM address. A file's compressed
size is the distance to the next file in the ROM. `fileGetIndex` looks a name up.

Most files are compressed. `fileLoadByNameToPool(name, mode, 0, pool)` loads one into a new
pool allocation:

1. The decompressed size isn't known the first time, so it allocates all the free space in
   the pool.
2. It copies the compressed data to the *end* of that space and decompresses it to the
   start. The output can catch up with the input; if it does, rzip hangs on purpose.
3. With `FILELOAD_TRIM` it gives back the space it didn't use. With
   `FILELOAD_KEEP_SPARE` it leaves the allocation as it is, so the caller can unpack the
   file further in place and then call `fileSetSize` (the model loader does this).
4. It remembers the decompressed size (`fileGetLoadedSize`), so later loads allocate just
   that. Resetting the stage pool forgets all the sizes.

`fileLoadByNameToAddr` does the same into a buffer the caller gives. `fileLoadPartByName`
reads a range of a file without decompressing; the level geometry files are read this way
because each room in them is compressed on its own.

### The 1172 format

A compressed file is the bytes `0x11 0x72` followed by a raw deflate stream: a gzip file
with its 10-byte header and 8-byte trailer cut off (`tools/1172compress.sh`). `rzipInflate`
is gzip 1.2.4's inflate, writing straight into the output buffer instead of a window. It
needs about 8 KB of scratch space for its Huffman tables, which the caller provides.
Textures and music are compressed the same way and call `rzipInflate` directly.

## Traps

* **`fileload` vs `save/file.c`.** The save code also uses a `file` prefix, for save
  files. That gets renamed with the save subsystem.
* **Unprototyped calls.** `fileload.c` calls `romCopy` with an extra argument and without
  its prototype, and `bg.c` calls several functions here without prototypes. Phase 3
  (C23) makes those errors and fixes them.
* **`fileGetIndex` and missing files.** A name that isn't in the table gives index 0.
  (Rare's version then asked the development kit's host PC for the file; that code is gone.)
* **Matching leftovers.** Several functions here still contain statements that only
  existed to make the decomp compile to the original bytes (`if (1);`, unused locals,
  `mempAlloc`'s odd pointer arithmetic). Phase 2 removes them.
