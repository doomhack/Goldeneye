#ifndef _PHYSICS_H_
#define _PHYSICS_H_

#include <ultra64.h>
#include <bondtypes.h>

extern coord3d g_ZeroCoord;

void physRandomTumble(Mtxf *mtx);
void physRandomLaunch(coord3d *velocity, Mtxf *tumble);
void physFall(f32 *height, f32 *speed, f32 ticks);
void physMoveWithGravity(coord3d *pos, coord3d *velocity, f32 ticks);
void physApplyTumble(Mtxf *mtx, Mtxf *tumble, s32 ticks);

#endif
