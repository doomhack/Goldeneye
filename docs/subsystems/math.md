# Maths (`src/game/math`)

The maths library everything else builds on: vectors, matrices, quaternions, inverse
trig, rounding, curves, and a little physics. It has no state apart from the world scale
(below) and depends on nothing in the game except the random number generator.

| File | What it holds |
| --- | --- |
| `matrix.c` | 4x4 float matrices (`Mtxf`): building, multiplying, inverting, and converting to the N64's fixed-point `Mtx` |
| `quaternion.c` | Quaternions for animation: conversion to and from matrices, slerp and splines |
| `trig.c` | `mathAtan2`, `mathAcos`, `mathAsin` and their 16-bit fixed-point versions |
| `math_sincos.s` | `sinf` and `cosf`, still in assembly |
| `round.c` | `mathFloor`, `mathCeil` and their `...ToInt` versions |
| `interp.c` | Linear and cubic interpolation between points |
| `physics.c` | Gravity, and the random launch and tumble of thrown objects |
| `line_tri_intersect.c` | `intersectRayTriangle`, a line against a triangle of the level geometry or an object |

## Conventions

**Units.** Angles are in radians unless a name says otherwise. Times are in game ticks of
1/60 second.

**Matrices** are stored the way libultra expects: `m[row][column]`, with points as row
vectors on the left (`p' = p * M`). Rows 0, 1 and 2 are the object's x, y and z axes
after the transform, and row 3 is the translation, so `m[3][0..2]` is the position.

`mtx4Mult(a, b, out)` gives a matrix that applies `b` first and then `a`. The `InPlace`
versions write the result into the second argument: `mtx4MultInPlace(a, b)` makes
`b = a * b`. The `Affine` versions assume the matrices have no perspective, which is
faster.

Functions that build a matrix from scratch are `mtx4Load...`; ones that change one part of
an existing matrix are `mtx4Set...` or `mtx4Scale...`.

**Quaternions** (`quatf`) are four floats in the order w, x, y, z.

**Fixed point.** The RSP takes matrices as s15.16 fixed point in libultra's `Mtx` layout:
the integer halves of all 16 values first, then the fraction halves. `mtxF2L` converts a
float matrix to that layout, like libultra's `guMtxF2L`, but also multiplies columns 0-2
by the level's *world scale* (`mtxSetWorldScale`, from the "visibility" value in the level
table). The first-person gun code sets the scale to 1 around its own matrices with
`mtxPushUnitWorldScale` and `mtxPopWorldScale`.

## Traps

* **These are not the C library's functions.** `mathAtan2(y, x)` returns `[0, 2pi)`, not
  `(-pi, pi]`. `mathAcos` and `mathAsin` go through a 16-bit lookup table, so they are only
  accurate to about 0.00005 radians. They were called `atan2f`, `acosf` and `asinf`, which
  hid this; the names changed so that nobody swaps in the real ones by accident.
* **`mathAtan2` argument order.** The game mostly calls it as `mathAtan2(dx, dz)`, which
  gives a heading measured from +z towards +x.
* **Rounding through `s32`.** `mathFloor` and `mathCeil` convert to an integer and back, so
  they break for values beyond about 2 billion.
* **`mtx4Scale...` names count rows and columns of `m[row][column]`.** `mtx4ScaleRows012`
  scales the object before it is placed (the x, y and z axes), `mtx4ScaleColumns012` scales
  the result, translation included.
* **Gravity is per tick.** `physFall` uses 1/3.6 units per tick per tick, so a fall depends
  on `g_GlobalTimerDelta` being in ticks.
* **Unused functions.** `mtx4MultPerspective`, `mtxF2LUnpacked`, `mtxL2FUnpacked`,
  `mtxPackFixed`, `mtx4Invert`, `interpCatmullRom`, `interpHermite`, `physStep` and several
  quaternion helpers are never called. Phase 2 removes them.

## PAL differences

PAL builds add five 3x3 matrix helpers (`mtx3Copy`, `mtx3Mult`, `mtx3MultInPlace`,
`mtx3ToMtx4`, `mtx4ToMtx3`) that Rare added to the PAL version.
