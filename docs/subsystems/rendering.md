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
| `src/game/gfx/image.c` | Loading and decompressing textures, texture pools |
| `src/game/gfx/imagebank.c` | The global image bank (ammo icons, crosshair, smoke, flares) |
| `src/game/gfx/tex.c` | Rewriting a model's display list to load its textures; surface sound tables |
| `src/game/gfx/texselect.c` | Setting up the RDP to draw one image from an image table |
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
`texLoad` copies texture NNNN from the textures segment of the ROM, decompresses it into a
texture pool, and writes the address back over the number, so each texture is only loaded
once per pool.

`g_Textures` has one entry per texture: its offset in ROM (`texResolveTableOffsets` turns
the stored sizes into offsets at boot), and the sound and impact texture a bullet makes on
it.

A pool (`struct texpool`) fills from both ends: the pixel data grows from the left, and a
`struct tex` describing each texture (number, size, format, levels of detail) grows from the
right. `texFindInPool` searches the right-hand list. The shared pool `g_TexSharedPool` is
allocated each level by `texInitSharedPool`, sized by the level's memory budget (plus
512 KB in co-op); models can pass their own. When a pool is full, the texture points at the start of
the pool instead, which draws garbage rather than crashing.

### Compression

The first byte of a texture says whether it is zlib-compressed and how many levels of detail
it has.

* zlib textures are always paletted (`texInflateZlib`).
* The others (`texInflateNonZlib`) use one of the `TEXCOMPMETHOD_*` methods: raw, Huffman,
  RLE, a colour lookup table, or a combination. Some are blurred after decoding.

The decoders read their input through a small bit reader (`texBitsInit`, `texBitsRead`).
Smaller levels of detail can be stored, or made by shrinking the full image
(`texShrinkPaletted`, `texShrinkNonPaletted`).

`rleExpand8Bit` is a separate, simpler RLE used only for the title screen backdrop.

### From texture to TMEM

Model display lists only name a texture and how to tile it. `texLoadFromGdl`, called when a
model file is loaded, copies the display list and replaces those commands with real RDP
commands: `LoadBlock` into TMEM, then the tile settings for each level of detail
(`texHandleType0` to `texHandleType4` for the different combinations of detail texture and
mipmaps). It caches the tile state so it doesn't write the same settings twice.

HUD, sky, menu and effect images don't go through a model. They are entries in an image
table (`struct sImageTableEntry`), and `texSelect` writes the load and tile commands for one
of them directly, along with a render mode (`texSetRenderMode`).

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

* Many parameters are still `s32` where they hold a pointer, and `texSelect`'s arguments are
  unnamed flags.
* `texLoadFromGdl` is long and only partly understood; `docs/subsystems/models.md` covers the
  model file side.
* `tex.c` holds the surface sound tables, which belong with bullet impacts in `prop/`.
