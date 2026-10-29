#ifndef _PHYSICS_H_
#define _PHYSICS_H_

#include <ultra64.h>
#include "game/math/mathtypes.h"

void physRandomTumble(Mtxf *tumble);
void physRandomLaunch(coord3d *velocity, Mtxf *tumble);
void physFall(f32 *height, f32 *speed, f32 ticks);
void physMoveWithGravity(coord3d *pos, coord3d *velocity, f32 ticks);
void physApplyTumble(Mtxf *mtx, const Mtxf *tumble, s32 ticks);
void physMoveTowards(f32 *pos, f32 target, f32 *speed, f32 accel, f32 decel, f32 maxSpeed, s32 ticks);
void physTurnTowards(f32 *angle, f32 target, f32 *speed, f32 accel, f32 decel, f32 maxSpeed, s32 ticks);

#endif
