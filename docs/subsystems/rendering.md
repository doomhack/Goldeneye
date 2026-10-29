# Rendering

The low-level drawing code: where display lists and per-frame graphics data are allocated,
how textures get from ROM into memory and into the RDP's TMEM, and how the frame is
handed to the RSP. What is drawn (rooms, models, the HUD) lives in `world/`, `model/` and
`hud/`.

| File | What it holds |
| --- | --- |
| `src/game/gfx/dyn.c` | The double-buffered display list and vertex memory for each frame |
| `src/game/gfx/rsp.c` | Starting the RSP graphics task |
| `src/game/gfx/zbuf.c` | The z-buffer |
| `src/game/gfx/texrect.c` | Drawing a textured rectangle on screen (`texRectDrawImage`), flipped or rotated; was `hud/bondwalk2.c` |
| `src/game/gfx/texload.c` | Finding textures in the ROM, texture pools |
| `src/game/gfx/texdecode.c` | Decompressing textures (no state, tested on the PC by `tools/hosttest/textest.c`) |
| `src/game/gfx/imagebank.c` | The global image bank (ammo icons, crosshair, smoke, flares) |
| `src/game/gfx/texcmds.c` | The RDP commands that load textures into TMEM: expanding the texture commands in model and room display lists (`texLoadFromGdl`), and drawing with one image from an image table (`texSelect`) |
| `src/game/gfx/vtxstore.c` | Per-instance vertex copies for models that change shape or colour |

## A frame

1. `bossMainloop` takes the display list for this frame from `dynGetMasterDisplayList`.
2. The level loop draws each player's view into it. Code that needs vertices, matrices or
   lights for this frame only gets them from `dynAllocate*`.
3. `rspGfxTaskStart` hands the list to the scheduler, which runs it on the RSP.
4. `dynSwapBuffers` switches to the other half of both buffers, so the CPU builds the next
   frame while the RSP draws this one.

Both buffers are sized per level by the level's memory budget in boss.c
(`LevelMemBudget`), doubled in co-op. There is no overflow check.

## Textures

A texture is referred to by number. Display lists in model files and room geometry carry
`G_SETTIMG` commands whose address is `0xabcdNNNN`, meaning "texture NNNN, not loaded yet".
`texLoad` copies texture NNNN from the textures segment of the ROM, decodes it into a
texture pool with `texDecode`, and writes the address back over the number, so each texture
is only loaded once per pool.

`g_Textures` has one entry per texture: its offset in ROM (`texResolveTableOffsets` turns
the stored sizes into offsets at boot), and the sound and impact texture a bullet makes on
it.

A pool (`struct texpool`) fills from both ends: the pixel data grows from the left, and a
`struct tex` describing each texture (number, size, format, levels of detail) grows from the
right. `texFindInPool` (by number) and `texFindByData` (by address) search the right-hand
list. Each texture's pixels follow 8 bytes holding its number, which the bullet hit code
reads (from the address in a `G_SETTIMG`), and a texture with stored levels of detail has
their sizes before that (`struct texlodsizes`, read by `texGetWidthAtLod`). The shared pool `g_TexSharedPool` is allocated
each level by `texInitSharedPool`, sized by the level's memory budget (plus 512 KB in
co-op); models can pass their own. When a texture doesn't fit, it points at the start of
the pool instead, which draws garbage rather than crashing.

### Compression

`texdecode.c` describes the format in detail. The first byte of a texture says whether it is
zlib-compressed and how many levels of detail it has.

* zlib textures are always paletted.
* The others are in Rare's own formats, image by image: uncompressed, Huffman or
  run-length coded one colour channel at a time, indices into a table of colours, or
  differences from neighbouring pixels.

Smaller levels of detail are either stored, or made by shrinking the full image (averaging
2x2 squares; for a paletted texture, then finding the nearest palette colour). Odd rows have
their words swapped, the way the RDP expects a texture loaded with `LoadBlock`.

Decoding has to match Rare's encoder bit for bit, odd parts included (the Huffman tree's
16-bit frequencies, for one). `textest.c` decodes all 2698 textures in the ROM and compares
each with a stored hash (`tools/hosttest/texhashes.txt`), so any change to what the game
draws shows up there. The decoder was checked against Rare's code texture by texture when it
was rewritten (phase 5d), which found two bugs, fixed since: one IA4 texture (2496) and the
small levels of detail of eight more came out striped, and eight IA16_CI4 textures made their
smaller levels of detail from the wrong palette entries.

### From texture to TMEM

Model and room display lists only name a texture and how to tile it, with Rare's own
command, `G_SETTEX` (`gsSPUseTexture` in `gbi_extension.h`). `texLoadFromGdl`, called when a
model file or a room is loaded, copies the display list and replaces each `G_SETTEX` with
real RDP commands: one `LoadBlock` of all the texture's levels of detail into TMEM (and a
`LoadTLUT` of its palette, which is stored after the pixels, into the top half of TMEM), then
the tiles that draw it. The texture types (`TEXTURETYPE_*`) are a single tile, a mipmap, and
two kinds that blend into tile 0 with LOD: the texture itself, or a separate detail texture
loaded first in TMEM. The tile settings are remembered as they're written, so the same
settings aren't written twice. The water and light fixture textures get extra handling
(`water.c`, `lightfixture.c`).

HUD, sky, menu and effect images don't go through a model. They are entries in an image
table (`struct sImageTableEntry`), and `texSelect` writes the load and tile commands for one
of them directly, along with a render mode (`enum TexBlendMode`, `enum TexZbufferMode`).

`texcmds.c` was rewritten in phase 5d and checked against Rare's code with a build that ran
both on every display list and image: identical output through all 28 demos, the menus, and
every solo level, in US and PAL.

### The image bank

Images used all over the game (ammo icons, the crosshair, explosion smoke, bullet
impacts, lens flares, menu folders) are in one ROM file, the global image bank.
`imageBankLoad` copies it into the stage pool on each level load, aligned to 4 KB, and
points the `g_Image*` globals at its tables. The bank is also mapped as an RSP segment
(`SPSEGMENT_GETITLE`), so display lists inside it can refer to each other by segment
address.

## The vertex store

Model vertices are normally shared by every instance of a model. Two things change them per
instance, and those instances get their own copy from the vertex store:

* a blood stain painted onto a character (`chrCreateBloodStain`, store `VTXSTORE_CHR`);
* damage to a prop (`VTXSTORE_OBJ`). Props with the same model and damage level can share
  a copy, and `vtxstoreTick` merges duplicates when the store gets low.

Each store is a block of vertices split into batches, kept in a doubly linked list in
address order, so freeing a batch can merge it with free neighbours. When a store is full,
the allocation fails and the instance keeps using the shared vertices.

## Traps

* Many parameters are still `s32` where they hold a pointer.
* `texLoadFromGdl` sets the levels of detail in the display list's own `gSPTexture` by
  changing it in place when no triangles have been drawn since it, and otherwise writes a
  copy. A second texture before any triangles changes the original, not the copy.
* A palette more than about 1023 texels into a texture can't be addressed by `LoadTLUT`'s
  coordinates (see `loadPalette`); none of the game's textures are that big.
