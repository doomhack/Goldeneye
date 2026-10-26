# Models

How characters, guns and props are stored, animated and drawn. The level geometry is not a
model; it lives in `world/`.

| File | What it holds |
| --- | --- |
| `src/game/model/model.c` | Model instances and slots, the node tree walk, joint matrices, animation playback, drawing |
| `src/game/model/anim.c` | The animation table, loaded at boot, and the buffer animation frames are read into |
| `src/game/model/jointlist.c` | Depth-sorted lists of character joints, for drawing and for shot hit tests |
| `src/game/model/modelfile.c` | Loading model files and their textures |
| `src/game/model/cobjdata.c`, `gobjdata.c`, `pobjdata.c` | The tables of character, gun and prop model files |
| `src/game/model/initactorpropstuff.c` | Not model code: weapon animation groups and casings, left for the player pass |

## Model files

A model file (`ModelFileHeader`) is a tree of nodes plus a switch table and a texture list.
In ROM, pointers are stored as offsets from `0x05000000`. `modelFileLoad` loads a file into
the stage pool (or a buffer it is given), turns the offsets into pointers, then loads the
textures: it moves the display lists to the end of the buffer, puts the textures after
them and trims the allocation to what is used.

Each node has an opcode, a pointer to its data, a parent, a first child and a next
sibling. Drawing and matrix work walk the tree depth-first.

| Opcode | Name | What it does |
| --- | --- | --- |
| 0x01 | `HEADER` | The root. Has a matrix and the model's root motion |
| 0x02 | `GROUP` | A joint: a position, and a matrix built from the animation's rotation for it |
| 0x03 | `OP03` | A joint that only turns about Y |
| 0x04 | `DL` | A display list, with its render mode. Most models draw through this. (The function is still called `modelRenderNodeGundl`) |
| 0x07 | `TUBE` | A stretched tube between two joints. Rare's name for the draw function was `dotube` |
| 0x08 | `LOD` | Level of detail: shows its child only between two distances |
| 0x09 | `BSP` | Chooses which of two children to draw first, by which side of a plane the camera is on |
| 0x0A | `BBOX` | Bounding box for a part, used by shot hit tests. Draws nothing |
| 0x0C | `GUNFIRE` | Muzzle flash, drawn only on the frames a gun fires |
| 0x0D | `SHADOW` | Round shadow under a character, faded by `g_ModelShadowAlpha` |
| 0x0F | `INTERLINK` | Two points and a radius, treated as a joint when sorting by depth. Purpose not yet known |
| 0x11 | `OP17` | A ray-test volume. Draws nothing |
| 0x12 | `SWITCH` | Shows or hides its children by a flag in the model's rwdata (open doors, broken glass) |
| 0x15 | `GROUPSIMPLE` | A fixed position with no animation, used for things like where casings eject and where held items go |
| 0x16 | `DLPRIMARY` | Display list with an optional scrolling or rotating texture |
| 0x17 | `HEAD` | The slot a character's head model is attached to (`modelAttachHead`) |
| 0x18 | `DLCOLLISION` | A display list that can be swapped at run time (its current list is in the rwdata) |

Opcodes 0x05, 0x06, 0x0B, 0x0E, 0x10, 0x13 and 0x14 appear in the enum and have draw
functions that do nothing. Nobody has found a model that uses them.

## Model instances and slots

A `Model` is one instance of a file: a pointer to the file, a scale, the animation state
and per-node runtime data (`rwdata`), such as a switch's on/off flag or a joint's matrix
index. Each node type that needs rwdata gets a slice of it, found by `modelGetNodeRwData`.

Instances come from slots allocated per level (`prop.c` calls `modelmgrAllocateModelSlots`
and `modelmgrAllocateAnimModelSlots` with counts from the setup file): plain slots for
props, bigger ones with animation state for characters and animated objects. A slot is
reused when the next model's rwdata fits (`modelmgrCanSlotFitRwdata`). The slot structs
in `model.h` (`ModelSlot`, `AnimModelSlot`) are overlays the size of a `Model` and still
have their decomp field names.

## Each frame

The owner of a model (a guard, a prop, Bond's gun) calls three things:

1. `modelCalcPosition`: moves the root by the animation's root motion for this frame,
   through `rootMoveCallback` if the owner set one (guards set one that can adjust or veto
   the move).
2. `modelCalcMatrices`: walks the tree and builds every joint's matrix: the root matrix
   (`modelCalcRootMtx`), then each joint's rotation from the animation
   (`modelCalcJointMtx`, `modelCalcYJointMtx`), multiplied by its parent's. It first loads
   the animation frames it needs. `modelCalcMatricesNoAnim` skips that, for models that
   are not animated (hats, held guns, the title screen gun).
3. `modelRender`: walks the tree again and calls `modelRenderNode` for each node. LOD, BSP,
   switch and head nodes choose which children are visited; the others draw.

The matrices go into `ModelRenderData`, together with the render mode, colours and the
display list being built. `g_ModelVtxAllocator` is where nodes that make vertices at run
time (tubes, shadows, gunfire) get them.

## Animation

Animations (`ModelAnimation`) are in one table, `g_AnimTable`. At boot `animLoadTable`
copies the headers into the permanent pool and turns their offsets into pointers
(`animResolveTable`). The frame data stays in ROM, and `animLoadFrame` copies each frame into
`g_AnimFrameBuffer` as it is needed, a few at a time.

A frame is a bit stream. Each joint's rotation is three angles of `angleBits` bits (a full
turn is 0x10000), read by `animReadJointRotation`. The skeleton gives each joint an index
into the frame for normal play and another for mirrored play (`flip`, used to play a
right-handed animation left-handed); mirroring also negates the Y and Z angles. The root's
motion is read separately (`animReadRootMotion16`).

Playback state lives in the `Model`:

* `frame` and `frameFrac`: the current frame and how far towards the next one it is. The two
  frames' data are in `frameaData` and `framebData`, and joint angles are blended between
  them (`modelLerpAngle`), so animations are smooth at any speed.
* `newplayspeed`, `oldplayspeed`, `timeplayspeed`, `elapseplayspeed`: speed changes are eased
  in over `timeplayspeed` ticks rather than applied at once.
* `anim2`, `frame2`, `mergeWeight`, `mergeTime`, `mergeElapsed`: when a new animation starts
  with a merge time (`modelSetAnimationWithMerge`), the old one keeps playing as `anim2`,
  and the two are mixed by `mergeWeight` going from 0 to 1 over `mergeTime` ticks.
* `endframe` and the looping fields decide what happens at the end.

`modelTickAnim` advances all of this by the number of ticks since the last frame.

## Joint lists

Each character keeps a list of the joints of its body, head and hat (`chr->field_20`):
one entry per positioning node (header, joints, simple groups and so on). `jointListCalcDepths` and `jointListSortByDepth` sort it by
distance from the camera. `jointListDraw` draws a character joint by joint from the back
to the front, each joint drawing its own nodes but not its child joints, and
`jointListFindHit` tests a shot against each joint's display lists and bounding boxes from
the front to the back, so the first hit is the nearest. The entries come from a pool of 600
(`JOINTLIST_LEN`) on a free list, emptied on level load (`jointListInit`).

## Traps

* Many functions still take `s32` where they mean `ModelRenderData *`, and several draw
  functions for unused opcodes are empty. Both go in Phase 2.
* Field names like `unk04` are shared by many structs, so rename `Model` and node data
  fields with `tools/renamefield.py`, not a text search.
* `chrGiveWeapon` is declared in `model.h` because the slot code calls it; it belongs to
  `chr/`.
