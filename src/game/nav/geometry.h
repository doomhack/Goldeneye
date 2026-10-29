#ifndef _GEOMETRY_H_
#define _GEOMETRY_H_
#include <ultra64.h>
#include "game/math/mathtypes.h"

/*
 * 2D geometry on the floor plane, x and z. Collision runs on these, so they must give the
 * same results bit for bit as the originals (see docs/subsystems/nav.md).
 */

f32 geoDist2d(f32 ax, f32 az, f32 bx, f32 bz);
f32 geoSignedLineDist(f32 ax, f32 az, f32 bx, f32 bz, f32 px, f32 pz);
bool geoProjectsOntoSegment(f32 ax, f32 az, f32 bx, f32 bz, f32 px, f32 pz);
bool geoSegmentsCross(f32 ax, f32 az, f32 bx, f32 bz, f32 cx, f32 cz, f32 dx, f32 dz, bool touching);
bool geoSegmentsIntersect(f32 ax, f32 az, f32 bx, f32 bz, f32 cx, f32 cz, f32 dx, f32 dz);
f32 geoSegmentHitFrac(const coord2d *start, const coord2d *end, const coord2d *edgeA, const coord2d *edgeB);
f32 geoCircleSweepFrac(f32 radius, const coord2d *centre, const coord2d *move, const coord2d *edgeA, const coord2d *edgeB);

#endif
