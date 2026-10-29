# Maths (`src/game/math`)

The maths library everything else builds on: vectors, matrices, quaternions, inverse
trig, rounding, curves, a little physics, and ray-triangle hits. It has no state apart from
the world scale (below) and depends on nothing in the game except the random number
generator.

| File | What it holds |
| --- | --- |
| `mathtypes.h` | The types: `coord3d` (float vector), `coord16` (16-bit vertex position), `Mtxf` (4x4), `Mtx3f` (3x3) and `quatf` (quaternion). bondtypes.h includes it |
| `matrix.c` | 4x4 and 3x3 float matrices: building, multiplying, inverting, cameras, and converting to the N64's fixed-point `Mtx` |
| `quaternion.c` | Quaternions for animation: from Euler angles, to matrices, slerp and splines |
| `trig.c` | `sinf` and `cosf` (single precision, replacing the C library's), `mathAtan2` and `mathAcos` |
| `round.c` | `mathFloor`, `mathCeil` and their `...ToInt` versions |
| `interp.c` | Linear and cubic interpolation between points |
| `physics.c` | Gravity, the random launch and tumble of thrown objects, and easing a value towards a target (`physMoveTowards`: doors, crouching; `physTurnTowards`: autoguns, vehicles) |
| `line_tri_intersect.c` | `intersectRayTriangle`, a ray against a triangle of the level geometry or an object; `intersectRaySphere` for bounds checks |

`tools/hosttest/mathtest.c` tests all of it on the PC (see [Testing.md](../Testing.md)).

## Conventions

**Arguments.** Inputs come first and are `const`; the output comes last. The `InPlace`
versions write the result over their last argument. The camera builders
(`mtx4LoadLookAt` and friends) follow libultra's `guLookAtF` instead: the matrix first,
then nine floats.

**Units.** Angles are in radians unless a name says otherwise. Times are in game ticks of
1/60 second (1/50 in PAL).

**Matrices** are stored the way libultra expects: `m[row][column]`, with points as row
vectors on the left (`p' = p * M`). Rows 0, 1 and 2 are the object's x, y and z axes
after the transform, and row 3 is the translation, so `m[3][0..2]` is the position.

`mtx4Mult(a, b, out)` gives a matrix that applies `b` first and then `a`. `mtx4MultInPlace(a, b)`
makes `b = a * b`. The `Affine` versions assume the matrices have no perspective (column 3
is 0, 0, 0, 1), which is faster.

Functions that build a matrix from scratch are `mtx4Load...`; ones that change one part of
an existing matrix are `mtx4Set...` or `mtx4Scale...`.

`Mtx3f` is the top left 3x3 of a `Mtxf`: rotation and scale only. PAL builds keep spent
cartridge cases' spin in them to save memory (`mtx3MultInPlace`, `mtx3ToMtx4`, `mtx4ToMtx3`).

**Quaternions** (`quatf`) have `w` (the real part), then `x`, `y`, `z`.

**Fixed point.** The RSP takes matrices as s15.16 fixed point in libultra's `Mtx` layout:
the integer halves of all 16 values first, then the fraction halves. `mtxF2L` converts a
float matrix to that layout, like libultra's `guMtxF2L`, but also multiplies columns 0-2
by the level's *world scale* (`mtxSetWorldScale`, from the "visibility" value in the level
table). The first-person gun code sets the scale to 1 around its own matrices with
`mtxPushUnitWorldScale` and `mtxPopWorldScale`.

## Exact results matter

The game's state depends on these functions' exact results, down to the last bit: Rare's
attract-mode demos replay recorded controller input, and a guard that turns 0.0001 radians
differently can end up somewhere else a minute later. So the rewrite (phase 5c) kept the
order of every floating-point operation. It was checked by compiling the old and new code
side by side on the PC and comparing 14 million results bit for bit (0 differences), and by
the demos on the N64.

That rules out making the functions more accurate. Where they are inaccurate, the traps
below say so.

## Traps

* **These are not the C library's functions.** `mathAtan2(y, x)` returns `[0, 2pi)`, not
  `(-pi, pi]`. They were called `atan2f` and `acosf`, which hid this; the names changed so
  that nobody swaps in the real ones by accident.
* **`mathAcos` and `mathAtan2` are only roughly right.** `mathAcos` goes through a 16-bit
  lookup table: it is out by up to 5e-4 radians, and up to 5e-3 (0.3 degrees) near cosines
  of +-1, where acos is steep. `mathAcos(1)` is 0.0028, not 0. `mathAtan2` uses `mathAcos`,
  and is out by up to 0.3 degrees near 90 and 270 degrees.
* **`sinf` and `cosf` lose accuracy for big angles.** They are good to about 1e-7 for
  angles within +-pi/2, but the second half of pi used to reduce bigger angles is wrong
  (`PI_LO` in trig.c), so the error grows to about 1e-7 times the angle. `cosf` adds a
  slightly low pi/2 and calls `sinf`.
* **`mathAtan2` argument order.** The game mostly calls it as `mathAtan2(dx, dz)`, which
  gives a heading measured from +z towards +x.
* **Rounding through `s32`.** `mathFloor` and `mathCeil` convert to an integer and back, so
  they break for values beyond about 2 billion.
* **`mtx4Scale...` names count rows and columns of `m[row][column]`.** `mtx4ScaleRows012`
  scales the object before it is placed (the x, y and z axes), `mtx4ScaleColumns012` scales
  the result, translation included.
* **Two aligns.** `mtx4LoadAlign` (libultra's `guAlignF`, in radians) turns -z to point
  along a direction; `mtx4LoadAlignInverse` is its transpose, written with its own (slightly
  different) rounding. The gun code uses both.
* **`mtxF2L` and `mtxL2F` can't work in place**, and their fixed-point side is typed `Mtxf`
  because the game keeps `Mtx` arrays in `Mtxf`-typed buffers.
* **Gravity is per tick.** `physFall` uses 1/3.6 units per tick per tick, so a fall depends
  on `g_GlobalTimerDelta` being in ticks. PAL's random tumble turns 1.2 times as far per
  tick, to make up for its longer ticks.
