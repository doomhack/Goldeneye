/**
 * Interpolation: straight lines and cubic curves between points.
 *
 * `fraction` runs from 0.0 at the start of a segment to 1.0 at its end. Values outside
 * that range extrapolate.
 */
#include <ultra64.h>
#include "game/math/interp.h"

/** Linear interpolation between two points. */
void interpLerp3(const coord3d *from, const coord3d *to, f32 fraction, coord3d *result)
{
    result->x = ((to->x - from->x) * fraction) + from->x;
    result->y = ((to->y - from->y) * fraction) + from->y;
    result->z = ((to->z - from->z) * fraction) + from->z;
}

/**
 * Catmull-Rom interpolation of a point: the curve goes from p1 to p2, and p0 and p3 shape
 * it.
 *
 * Every few seconds the game picks a small random offset for where the first-person gun
 * points; this smooths the gun's drift from one offset to the next.
 */
void interpCatmullRom3(const coord3d *p0, const coord3d *p1, const coord3d *p2, const coord3d *p3, f32 fraction, coord3d *result)
{
    f32 square = fraction * fraction;
    f32 cube = fraction * fraction * fraction;
    f32 weight0 = square - 0.5f * (fraction + cube);
    f32 weight1 = 1.5f * cube - 2.5f * square + 1.0f;
    f32 weight2 = -1.5f * cube + 2.0f * square + 0.5f * fraction;
    f32 weight3 = 0.5f * (cube - square);

    for (s32 i = 0; i < 3; i++)
    {
        result->f[i] = weight0 * p0->f[i] + weight1 * p1->f[i] + weight2 * p2->f[i] + weight3 * p3->f[i];
    }
}

/**
 * Cardinal spline interpolation from `start` to `end`. `prev` and `next` set the curve's
 * direction at each end, and `tangentScale` how strongly: 0 ignores them, 0.5 gives a
 * Catmull-Rom curve, more makes the curve looser.
 *
 * The intro camera swirl and the gun's movement between keyframes use this.
 */
void interpCardinal3(const coord3d *prev, const coord3d *start, const coord3d *end, const coord3d *next, f32 fraction, f32 tangentScale, coord3d *result)
{
    f32 square = fraction * fraction;
    f32 cube = square * fraction;
    f32 weightPrev = ((2.0f * square) - (fraction + cube)) * tangentScale;
    f32 weightStart = (((2.0f - tangentScale) * cube) + (square * (tangentScale - 3.0f))) + 1.0f;
    f32 weightEnd = (((tangentScale - 2.0f) * cube) + (square * (3.0f - (2.0f * tangentScale)))) + (fraction * tangentScale);
    f32 weightNext = (cube - square) * tangentScale;

    for (s32 i = 0; i < 3; i++)
    {
        result->f[i] = weightPrev * prev->f[i] + weightStart * start->f[i] + weightEnd * end->f[i] + weightNext * next->f[i];
    }
}

