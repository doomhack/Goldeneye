#ifndef _PROPCOLLISION_H_
#define _PROPCOLLISION_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

void objUpdateCollisionBounds(ObjectRecord *obj);
void objGetCollisionBounds(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom);
void objSetMoving(PropRecord *prop, bool collidable);

void propGetCollisionPolygon(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom);
void propGetCollisionPolygon2d(PropRecord *prop, coord2d **polygon, s32 *edges);
void propGetCollisionRadius(PropRecord *prop, f32 *radius, f32 *height, f32 *ymin);
f32  propGetGroundY(PropRecord *prop);
void propSetMoving(PropRecord *prop, bool collidable);

s32  propDoorGetCdTypes(PropRecord *prop);
bool propIsOfCdType(PropRecord *prop, s32 cdtypes);

bool polygonContainsPoint(coord3d *point, coord2d *polygon, s32 edges);
bool polygonsOverlap(coord2d *poly1, s32 edges1, coord2d *poly2, s32 edges2);
bool polygonTouchesCircle(coord3d *point, f32 radius, coord2d *polygon, s32 edges);
bool propIsUnblocked(PropRecord *mover);

#endif
