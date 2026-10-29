#ifndef _LINE_TRI_INTERSECT_H_
#define _LINE_TRI_INTERSECT_H_

#include <ultra64.h>
#include "game/math/mathtypes.h"

bool intersectRayTriangle(const coord16 *vertex0, const coord16 *vertex1, const coord16 *vertex2,
                          const coord3d *vertexOffset, const coord3d *rayStart, const coord3d *linePoint,
                          const coord3d *rayDirection, coord3d *hitPos, coord3d *normal);

#endif
