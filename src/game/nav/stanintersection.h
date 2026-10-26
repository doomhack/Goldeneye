#ifndef _stanintersection_H_
#define _stanintersection_H_
#include <ultra64.h>
#include "bondtypes.h"

f32 stanSegmentIntersectFrac(coord2d *start1, coord2d *end1, coord2d *start2, coord2d *end2);
f32 stanPointLineDist(struct coord3d *point3D, coord2d *lineStart, coord2d *lineEnd);
f32 stanRaySegmentFrac(coord3d *point3D, coord2d *lineStart, coord2d *lineEnd, coord2d *direction);

#endif
