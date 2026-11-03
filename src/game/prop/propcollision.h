#ifndef _PROPCOLLISION_H_
#define _PROPCOLLISION_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

void objUpdateCollisionBounds(const ObjectRecord *obj);
void objGetCollisionBounds(const PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom);
void objSetMoving(const PropRecord *prop, bool collidable);

void propGetCollisionPolygon(const PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom);
void propGetCollisionPolygon2d(const PropRecord *prop, coord2d **polygon, s32 *edges);
void propGetCollisionRadius(const PropRecord *prop, f32 *radius, f32 *height, f32 *ymin);
f32  propGetGroundY(const PropRecord *prop);
void propSetMoving(const PropRecord *prop, bool collidable);

s32  propDoorGetCdTypes(const PropRecord *prop);
bool propIsOfCdType(const PropRecord *prop, s32 cdtypes);

bool polygonContainsPoint(const coord3d *point, coord2d *polygon, s32 edges);
bool polygonsOverlap(coord2d *poly1, s32 edges1, coord2d *poly2, s32 edges2);
bool polygonTouchesCircle(const coord3d *point, f32 radius, coord2d *polygon, s32 edges);
bool propIsUnblocked(const PropRecord *mover);

#endif
