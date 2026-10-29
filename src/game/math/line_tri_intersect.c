/**
 * Where a ray hits a triangle, for bullets and line-of-sight tests against the level and
 * objects. The sums are in doubles (f64), which the N64's FPU does natively.
 */
#include <ultra64.h>
#include "game/math/line_tri_intersect.h"

/**
 * Tests whether the line through `linePoint` along `rayDirection` hits the triangle
 * (vertex0, vertex1, vertex2), at or in front of `rayStart`. If it does, writes where to
 * `hitPos` and the triangle's normal (not scaled to length 1) to `normal`.
 *
 * The triangle's vertices are in its model's space, which `vertexOffset` moves into the
 * line's. A line along the triangle's plane never hits it.
 */
bool intersectRayTriangle(const coord16 *vertex0, const coord16 *vertex1, const coord16 *vertex2,
                          const coord3d *vertexOffset, const coord3d *rayStart, const coord3d *linePoint,
                          const coord3d *rayDirection, coord3d *hitPos, coord3d *normal)
{
    f64 offset[3] = {vertexOffset->x, vertexOffset->y, vertexOffset->z};
    f64 dir[3] = {rayDirection->x, rayDirection->y, rayDirection->z};
    f64 point[3] = {linePoint->x, linePoint->y, linePoint->z};
    f64 edge01[3];
    f64 edge12[3];
    f64 edge02[3];
    f64 normalX;
    f64 normalY;
    f64 normalZ;
    f64 planeConstant;
    f64 denominator;
    f64 lineParameter;
    f64 hit[3];
    f64 relative[3];
    f64 baryU;
    f64 baryV;

    // Two edges, and the third for the barycentric weights. The offset cancels out.
    edge01[0] = vertex1->x - vertex0->x;
    edge12[0] = vertex2->x - vertex1->x;
    edge01[1] = vertex1->y - vertex0->y;
    edge12[1] = vertex2->y - vertex1->y;
    edge01[2] = vertex1->z - vertex0->z;
    edge12[2] = vertex2->z - vertex1->z;
    edge02[0] = vertex2->x - vertex0->x;
    edge02[1] = vertex2->y - vertex0->y;
    edge02[2] = vertex2->z - vertex0->z;

    // The face normal: edge01 x edge12
    normalX = edge01[1] * edge12[2] - edge12[1] * edge01[2];
    normalY = edge01[2] * edge12[0] - edge12[2] * edge01[0];
    normalZ = edge01[0] * edge12[1] - edge12[0] * edge01[1];

    // The plane: normal . p = planeConstant, through vertex0 moved into the line's space
    planeConstant = normalX * (vertex0->x + offset[0]) + normalY * (vertex0->y + offset[1]) + normalZ * (vertex0->z + offset[2]);
    denominator = normalX * dir[0] + normalY * dir[1] + normalZ * dir[2];

    if (denominator == 0.0)
    {
        return false; // the line runs along the plane
    }

    // Where the line meets the plane
    lineParameter = (planeConstant - normalX * point[0] - normalY * point[1] - normalZ * point[2]) / denominator;

    for (s32 i = 0; i < 3; i++)
    {
        hit[i] = point[i] + dir[i] * lineParameter;
    }

    relative[0] = hit[0] - (vertex0->x + offset[0]);
    relative[1] = hit[1] - (vertex0->y + offset[1]);
    relative[2] = hit[2] - (vertex0->z + offset[2]);

    // The weight of edge02 (u) and of edge01 (v) that make up `relative`: solve in the
    // xy plane, or yz or xz if the triangle is edge on to it
    denominator = edge01[1] * edge12[0] - edge01[0] * edge12[1];

    if (denominator != 0.0)
    {
        baryU = (relative[0] * edge01[1] - relative[1] * edge01[0]) / denominator;
    }
    else
    {
        denominator = edge01[2] * edge12[1] - edge01[1] * edge12[2];

        if (denominator != 0.0)
        {
            baryU = (relative[1] * edge01[2] - relative[2] * edge01[1]) / denominator;
        }
        else
        {
            denominator = edge01[0] * edge12[2] - edge01[2] * edge12[0];
            baryU = (relative[2] * edge01[0] - relative[0] * edge01[2]) / denominator;
        }
    }

    if (edge01[0] != 0.0)
    {
        baryV = (relative[0] - baryU * edge02[0]) / edge01[0];
    }
    else if (edge01[1] != 0.0)
    {
        baryV = (relative[1] - baryU * edge02[1]) / edge01[1];
    }
    else
    {
        baryV = (relative[2] - baryU * edge02[2]) / edge01[2];
    }

    // Inside the triangle (or on an edge)? (Written so that NaNs, from a triangle with no
    // area, count as a miss.)
    if (!(baryV >= 0.0 && baryU >= 0.0 && baryV + baryU <= 1.0))
    {
        return false;
    }

    // In front of the ray's start, not behind it?
    if (!(dir[0] * (hit[0] - rayStart->x) + dir[1] * (hit[1] - rayStart->y) + dir[2] * (hit[2] - rayStart->z) >= 0.0))
    {
        return false;
    }

    hitPos->x = hit[0];
    hitPos->y = hit[1];
    hitPos->z = hit[2];
    normal->x = normalX;
    normal->y = normalY;
    normal->z = normalZ;

    return true;
}
