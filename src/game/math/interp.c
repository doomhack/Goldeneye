/**
 * Interpolation: straight lines and cubic curves between points.
 *
 * `fraction` runs from 0.0 at the start of a segment to 1.0 at its end. Values outside
 * that range extrapolate.
 */
#include <ultra64.h>
#include "game/math/matrix.h"
#include "game/math/interp.h"

/** Linear interpolation between two points. */
void interpLerp3(coord3d *from, coord3d *to, f32 fraction, coord3d *result)
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
void interpCatmullRom3(coord3d *p0, coord3d *p1, coord3d *p2, coord3d *p3, f32 fraction, coord3d *result)
{
    f32 weight0;
    f32 weight1;
    f32 weight2;
    f32 weight3;

    f32 square = fraction * fraction;
    f32 cube = fraction * fraction * fraction;

    weight0 = square - 0.5f * (fraction + cube);
    weight1 = 1.5f * cube - 2.5f * square + 1.0f;
    weight2 = -1.5f * cube + 2.0f * square + 0.5f * fraction;
    weight3 = 0.5f * (cube - square);

    result->x = weight0 * p0->f[0] + weight1 * p1->f[0] + weight2 * p2->f[0] + weight3 * p3->f[0];
    result->y = weight0 * p0->f[1] + weight1 * p1->f[1] + weight2 * p2->f[1] + weight3 * p3->f[1];
    result->z = weight0 * p0->f[2] + weight1 * p1->f[2] + weight2 * p2->f[2] + weight3 * p3->f[2];
}

/**
 * Cardinal spline interpolation from `start` to `end`. `prev` and `next` set the curve's
 * direction at each end, and `tangentScale` how strongly: 0 ignores them, 0.5 gives a
 * Catmull-Rom curve, more makes the curve looser.
 *
 * The intro camera swirl and the gun's movement between keyframes use this.
 */
void interpCardinal3(coord3d *prev, coord3d *start, coord3d *end, coord3d *next, f32 fraction, f32 tangentScale, coord3d *result)
{
    f32 square;
    f32 scale;
    f32 cube;
    f32 weightPrev;
    f32 weightStart;
    f32 weightEnd;

    square = fraction * fraction;
    scale = tangentScale;
    cube = square * fraction;

    weightPrev = ((2.0f * square) - (fraction + cube)) * scale;
    weightStart = (((2.0f - scale) * cube) + (square * (scale - 3.0f))) + 1.0f;
    weightEnd = (((scale - 2.0f) * cube) + (square * (3.0f - (2.0f * scale)))) + (fraction * scale);

    result->f[0] = (((weightPrev * prev->f[0]) + (weightStart * start->f[0])) + (weightEnd * end->f[0])) + (((cube - square) * tangentScale) * next->f[0]);
    result->f[1] = (((weightPrev * prev->f[1]) + (weightStart * start->f[1])) + (weightEnd * end->f[1])) + (((cube - square) * tangentScale) * next->f[1]);
    result->f[2] = (((weightPrev * prev->f[2]) + (weightStart * start->f[2])) + (weightEnd * end->f[2])) + (((cube - square) * tangentScale) * next->f[2]);
}

