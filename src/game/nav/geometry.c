/**
 * 2D geometry on the floor plane. Points are (x, z) pairs; a coord2d holds x in f[0] and
 * z in f[1].
 *
 * The walks in stanwalk.c and the prop collision in propobj.c are built on these, and the
 * game's state depends on their exact results, so each keeps the order of the original's
 * float operations.
 */

#include <ultra64.h>
#include "game/nav/geometry.h"
#include "limits.h"
#include "math.h"

f32 geoDist2d(f32 ax, f32 az, f32 bx, f32 bz)
{
    bx -= ax;
    bz -= az;

    return sqrtf(bx * bx + bz * bz);
}

/**
 * The distance from p to the infinite line through a and b: positive on the left of a->b
 * (looking down, x right and z down), negative on the right. If a and b are the same
 * point, the distance from it, always positive.
 */
f32 geoSignedLineDist(f32 ax, f32 az, f32 bx, f32 bz, f32 px, f32 pz)
{
    f32 len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));

    if (len == 0.0f)
    {
        return sqrtf((px - bx) * (px - bx) + (pz - bz) * (pz - bz));
    }

    return ((bz - az) * (px - ax) + -(bx - ax) * (pz - az)) / len;
}

/** Whether p, dropped square onto the line through a and b, lands strictly between them */
bool geoProjectsOntoSegment(f32 ax, f32 az, f32 bx, f32 bz, f32 px, f32 pz)
{
    f32 dot;
    f32 lengthSq;

    px -= ax;
    pz -= az;
    bx -= ax;
    bz -= az;

    dot = px * bx + pz * bz;
    lengthSq = bx * bx + bz * bz;

    return dot > 0.0f && dot < lengthSq;
}

/**
 * Which way b turns from a: 1 anticlockwise, -1 clockwise. When they are in line: -1 if
 * they point opposite ways, 1 if b is the longer, and 0 if a is at least as long.
 */
static s32 turnSign(f32 ax, f32 az, f32 bx, f32 bz)
{
    if (az * bx < ax * bz)
    {
        return 1;
    }

    if (ax * bz < az * bx)
    {
        return -1;
    }

    if (ax * bx < 0.0f || az * bz < 0.0f)
    {
        return -1;
    }

    if (ax * ax + az * az < bx * bx + bz * bz)
    {
        return 1;
    }

    return 0;
}

/**
 * Whether segment a-b crosses segment c-d: each segment's ends are on opposite sides of
 * the other. With touching, an end lying on the other segment counts too.
 */
bool geoSegmentsCross(f32 ax, f32 az, f32 bx, f32 bz, f32 cx, f32 cz, f32 dx, f32 dz, bool touching)
{
    s32 limit = touching ? 1 : 0;
    s32 sidesOfCD = turnSign(bx - ax, bz - az, cx - ax, cz - az) * turnSign(bx - ax, bz - az, dx - ax, dz - az);
    s32 sidesOfAB = turnSign(dx - cx, dz - cz, ax - cx, az - cz) * turnSign(dx - cx, dz - cz, bx - cx, bz - cz);

    return sidesOfCD < limit && sidesOfAB < limit;
}

bool geoSegmentsIntersect(f32 ax, f32 az, f32 bx, f32 bz, f32 cx, f32 cz, f32 dx, f32 dz)
{
    return geoSegmentsCross(ax, az, bx, bz, cx, cz, dx, dz, TRUE);
}

/**
 * How far along start->end (0 to 1) it meets the line through edgeA and edgeB, or 1 if
 * it doesn't within that range.
 */
f32 geoSegmentHitFrac(const coord2d *start, const coord2d *end, const coord2d *edgeA, const coord2d *edgeB)
{
    f32 normalX = edgeA->f[1] - edgeB->f[1];
    f32 normalZ = edgeB->f[0] - edgeA->f[0];
    f32 toEdge = (edgeA->f[1] - start->f[1]) * normalZ + (edgeA->f[0] - start->f[0]) * normalX;
    f32 along = (end->f[1] - start->f[1]) * normalZ + (end->f[0] - start->f[0]) * normalX;
    f32 frac;

    if (along == 0.0f)
    {
        return 1.0f;
    }

    frac = toEdge / along;

    if (frac < 0.0f || frac > 1.0f)
    {
        return 1.0f;
    }

    return frac;
}

/**
 * How far a circle can move along the unit vector dir before it touches point, or
 * FLT_MAX if it never does. 0 if it already overlaps the point from behind.
 */
static f32 circleSweepToPoint(f32 radius, const coord2d *centre, const coord2d *dir, const coord2d *point)
{
    f32 dx = point->f[0] - centre->f[0];
    f32 dz = point->f[1] - centre->f[1];
    f32 across = dz * dir->f[0] - dx * dir->f[1];
    f32 along = dx * dir->f[0] + dz * dir->f[1];
    f32 halfChordSq = (radius - across) * (radius + across);

    if (halfChordSq < 0.0f)
    {
        return FLT_MAX;
    }

    along -= sqrtf(halfChordSq);

    if (along < 0.0f)
    {
        if (along * along + across * across <= radius * radius)
        {
            return 0.0f;
        }

        return FLT_MAX;
    }

    return along;
}

/**
 * How far along move (0 to 1) a circle can go before it touches the segment edgeA-edgeB:
 * 1 if it never does, 0 if it touches already. Bond and props use it to slide along a wall
 * they have run into, and it is only right for that: moving away from the segment it
 * gives 0, and a hit on an end is sometimes worked out for the wrong end.
 */
f32 geoCircleSweepFrac(f32 radius, const coord2d *centre, const coord2d *move, const coord2d *edgeA, const coord2d *edgeB)
{
    f32 moveLen = sqrtf(move->f[0] * move->f[0] + move->f[1] * move->f[1]);
    coord2d dir;
    f32 edgeX;
    f32 edgeZ;
    f32 edgeLen;
    f32 dist;

    if (moveLen == 0.0f)
    {
        return 1.0f;
    }

    dir.f[0] = move->f[0] * (1.0f / moveLen);
    dir.f[1] = move->f[1] * (1.0f / moveLen);

    edgeX = edgeB->f[0] - edgeA->f[0];
    edgeZ = edgeB->f[1] - edgeA->f[1];
    edgeLen = sqrtf(edgeX * edgeX + edgeZ * edgeZ);

    if (edgeLen == 0.0f)
    {
        dist = circleSweepToPoint(radius, centre, &dir, edgeB);
    }
    else
    {
        f32 invLen = 1.0f / edgeLen;
        f32 normalX = edgeZ * invLen;
        f32 normalZ = -edgeX * invLen;
        f32 offsetX = radius * normalX;
        f32 offsetZ = radius * normalZ;
        f32 sideA;
        f32 sideB;
        f32 sideCentre;

        /* The edge moved a radius towards the circle: the centre touches it when the circle touches the edge */
        if (offsetX * (centre->f[0] - edgeA->f[0]) + offsetZ * (centre->f[1] - edgeA->f[1]) < 0.0f)
        {
            offsetX = -offsetX;
            offsetZ = -offsetZ;
        }

        /* Where the moved edge's ends and the centre lie across the line of movement */
        sideA = move->f[1] * (edgeA->f[0] + offsetX) - (edgeA->f[1] + offsetZ) * move->f[0];
        sideCentre = centre->f[0] * move->f[1] - centre->f[1] * move->f[0];
        sideB = move->f[1] * (edgeB->f[0] + offsetX) - (edgeB->f[1] + offsetZ) * move->f[0];

        if (sideB < sideA)
        {
            const coord2d *swap = edgeA;
            f32 swapSide = sideA;

            sideA = sideB;
            sideB = swapSide;
            edgeA = edgeB;
            edgeB = swap;
            normalX = -normalX;
            normalZ = -normalZ;
        }

        if (sideB == sideA)
        {
            /* Moving along the edge: only its ends can be hit */
            f32 distB;

            dist = circleSweepToPoint(radius, centre, &dir, edgeA);
            distB = circleSweepToPoint(radius, centre, &dir, edgeB);

            if (distB < dist)
            {
                dist = distB;
            }
        }
        else if (sideB < sideCentre)
        {
            dist = circleSweepToPoint(radius, centre, &dir, edgeB);
        }
        else if (sideCentre < sideA)
        {
            dist = circleSweepToPoint(radius, centre, &dir, edgeA);
        }
        else
        {
            /* The centre's path crosses the moved edge's span: hit the middle of the edge */
            f32 startDist = normalX * (centre->f[0] - edgeA->f[0]) + normalZ * (centre->f[1] - edgeA->f[1]);
            f32 endDist = normalX * (centre->f[0] + move->f[0] - edgeA->f[0]) + normalZ * (centre->f[1] + move->f[1] - edgeA->f[1]);

            if (startDist == endDist)
            {
                return 1.0f;
            }

            dist = (startDist - radius) * moveLen / (startDist - endDist);
        }
    }

    if (moveLen < dist)
    {
        return 1.0f;
    }

    if (dist < 0.0f)
    {
        return 0.0f;
    }

    return dist * (1.0f / moveLen);
}
