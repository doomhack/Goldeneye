# Assets and memory

How the game gets memory, and how it gets files from the cartridge into it. Everything
else that loads data (levels, models, text, textures, music) goes through these.

| File | What it holds |
| --- | --- |
| `src/memp.c` | The pool allocator: almost all the game's memory comes from it |
| `src/mema.c` | A small heap with real `free`, carved out of the stage pool, used for room geometry |
| `src/rom.c` | Copying from cartridge ROM to RAM by DMA (`romCopy`) |
| `src/game/asset/asset.c` | The file table, and loading files by name into a pool or a buffer |
| `src/game/asset/rzip.c` | Decompressing Rare's "1172" format (deflate). Built twice: once for the game, and once into the boot segment to unpack the game code at startup |

## Memory pools (memp)

All RAM from the end of the game's code and data (`_heapSegmentStart`) to the top of the
8 MB is split into pools. A pool is a stack: `mempAlloc` takes the next bytes, nothing is
freed on its own, and `mempResetPool` empties the whole pool at once.

There are two pools:

| Pool | Size | Lifetime |
| --- | --- | --- |
| `MEMPOOL_STAGE` | everything else (about 5.8 MB) | emptied each time a level or the title screen loads |
| `MEMPOOL_PERMANENT` | 296 KB (`MEMP_PERMANENT_SIZE`), at the top of RAM | filled once at boot (menu text, music buffers) |

`mempResizeLast` can grow or shrink only the most recent allocation in a pool. Between
levels boss.c closes the stage pool (`mempClosePool`), so anything that allocates from it
then is caught.

A level uses about 2 MB of the stage pool and boot about 110 KB of the permanent pool
(measured over the 14 demos), so there is plenty of headroom. Running out, or allocating
from a closed pool, stops the game with a `FATAL ERROR` screen (`dbgFatal`) naming the
problem and showing a rough backtrace. (Rare's version hung instead, and when the stage
pool was full it quietly took the memory from the permanent pool, where it was never freed
again.)

## Memory budgets

Each level has a `LevelMemBudget` in boss.c's `m_bossMemBudgets` table: the sizes, in KB,
of its display list and vertex buffers (dyn.c), its shared texture pool (texload.c) and
mema's room heap. Two-player and co-op games use the level's `_MP` entry when it has one,
and levels that aren't listed use `LEVELID_DEFAULT`'s. boss.c looks the budget up as each
level loads and leaves it in `g_LevelMemBudget`. Co-op doubles the gfx and vertex buffers
and the room heap, and adds 512 KB of textures.

(Rare passed these sizes around as text: a "command line" per level such as
`-mgfx70 -mvtx50 -mt625 -ma275`, parsed with `tokenFind`. That command line could also come
from the development kit, with debug switches such as `-level_`, `-hard` and `-nochr`; on a
cartridge it was always empty. Both are gone.)

## The room heap (mema)

mema is a small heap with real frees: `memaAlloc`, `memaFree` and `memaRealloc`. Its heap
is one allocation in the stage pool, sized by the level's memory budget (100 to 400 KB,
doubled in co-op because two players see more rooms at once). Only `world/bg.c` uses it,
for the geometry of rooms as they come into view and are dropped again.

It tracks free space, not allocations: a list of up to 508 free ranges, sorted by address.
So `memaFree` needs the caller to pass the size. A freed range is merged with its
neighbours straight away, so the list never holds two ranges that touch. `memaAlloc` takes
the smallest range that fits (best fit). Every size is rounded up to 16 bytes, so every
allocation is 16-byte aligned, as DMA wants.

`bgLoadRoomModelData` allocates `memaGetLongestFree()` bytes for a room it hasn't loaded
before (its size isn't known yet), loads it, then shrinks the allocation with
`memaRealloc`.

If the heap gets so fragmented that a free needs a 509th range, mema gives up the smallest
range until the next level, rather than overflow. Freeing memory that is already free (or
with a size that overlaps free space) stops the game with a fatal error.

## ROM access

`romCopy(dst, romAddr, size)` copies from the cartridge and waits until the copy has
finished. The ROM address must be even. The target should be 16-byte aligned: a target
that isn't, and a partial cache line at the end, go through a small bounce buffer, because
a DMA into a cache line the CPU is also using gets overwritten on real hardware (but not in
emulators). `romCopyUnaligned` copies from an odd ROM address.

## Files (assets)

Every file the game loads by name has an entry in `g_FileTable` (generated into
`assets/obseg/file_resource_table.inc.c`): its name and ROM address. A file's compressed
size is the distance to the next file in the ROM. `assetFind` looks a name up; a name
that isn't in the table stops the game with a fatal error.

Most files are compressed. `assetLoad(name, pool)` loads one into a new pool allocation:

1. The decompressed size isn't known the first time, so it allocates all the free space in
   the pool.
2. It copies the compressed data to the *end* of that space and decompresses it to the
   start (`rzipInflate` stops with an error, rather than corrupt the data, if the output
   would catch up with the input still to be read).
3. It gives back the space it didn't use, and remembers the decompressed size
   (`assetGetSize`), so later loads allocate just that. When the stage pool is emptied,
   boss.c calls `assetForgetSizes`.

`assetLoadUntrimmed` leaves the allocation as it is, so the caller can unpack the file
further in place and then call `assetSetSize` (the model loader does this).
`assetLoadInto` loads into a buffer the caller gives. `assetLoadPart` reads a range of a
file without decompressing; the level geometry files are read this way because each room
in them is compressed on its own.

A file that is missing, doesn't fit its buffer or is corrupt stops the game with a
`FATAL ERROR` screen naming the problem. (Rare's loader returned index 0 for a missing
file, which then asked the development kit's host PC for it, and didn't check sizes.)

### The 1172 format

A compressed file is the bytes `0x11 0x72` followed by a raw deflate stream (RFC 1951): a
gzip file with its 10-byte header and 8-byte trailer cut off (`tools/1172compress.sh`).

`rzipInflate(src, dst, dstSize)` decompresses one and returns the decompressed size, or 0
if the data is corrupt or doesn't fit in `dstSize` bytes. It writes straight into the
output buffer (deflate's 32 KB "window" is just the output so far). Huffman codes up to 9
bits long are decoded with one table lookup, longer ones with the canonical code ranges.
It needs no scratch memory from the caller. Callers turn a 0 into a fatal error: the asset
loader, room geometry (`bgDecompress`), compressed textures (texload.c) and music (music.c).

The source is built twice. The boot code links its own copy (`rzipboot.o`, built with
`RZIP_BOOT`, which names the function `bootInflate`) into the boot segment, because it has
to unpack the game code before any of it is in memory. That copy can't call into the game.

rzip keeps its Huffman tables in statics, so only one thread may decompress at a time. The
main thread does all of it (files, textures, rooms and music).

(Rare had two copies of gzip 1.2.4's inflate, one for boot and one for the game, which
needed an 8 KB scratch buffer from the caller, never checked the output size, and hung on
corrupt data.)

## Traps

* **`asset*` vs `file*`.** The asset loader used to be `fileload.c` with a `file*` prefix,
  which clashed with the save code's `file*` (save/file.c). Asset loading is `asset*` now.
* **In-place decompression.** A file loaded into a buffer only just big enough can fail
  where one with room to spare works, if the end of the file compresses very well: the
  output must not overtake the compressed data still to be read. The loader always has
  room the first time (it takes the whole pool), and later loads use the recorded size.
