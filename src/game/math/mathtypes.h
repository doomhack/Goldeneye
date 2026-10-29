#ifndef _MATHTYPES_H_
#define _MATHTYPES_H_

#include <ultra64.h>

/* The maths library's types: vectors, matrices and quaternions. See docs/subsystems/math.md. */

/** A point or vector in floats. */
typedef union coord3d
{
    struct
    {
        f32 x;
        f32 y;
        f32 z;
    };
    f32 f[3];
} coord3d;

/** A point on the floor plane in floats: x, and z in y (or f[1]). */
typedef union coord2d
{
    struct
    {
        f32 x;
        f32 y;
    };
    f32 f[2];
} coord2d;

/** A point in 16-bit integers, as in the N64's vertices. */
typedef union coord16
{
    struct
    {
        s16 x;
        s16 y;
        s16 z;
    };
    s16 AsArray[3];
} coord16;

/**
 * A 4x4 float matrix, m[row][column], with points as row vectors on the left (p' = p * M):
 * rows 0-2 are the x, y and z axes after the transform, row 3 the translation. The RSP
 * takes the fixed-point Mtx made from it by mtxF2L.
 */
typedef union Mtxf
{
    f32 m[4][4];
    s32 unused;
} Mtxf;

/** A 3x3 float matrix (rotation and scale only), laid out like the top left of a Mtxf. */
typedef struct Mtx3f
{
    f32 m[3][3];
} Mtx3f;

/** A quaternion, for rotations: w is the real part. */
typedef union quatf
{
    struct
    {
        f32 w;
        f32 x;
        f32 y;
        f32 z;
    };
    f32 f[4];
} quatf;

#endif
