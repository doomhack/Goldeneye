#ifndef _INTERP_H_
#define _INTERP_H_

#include <ultra64.h>
#include "game/math/mathtypes.h"

void interpLerp3(const coord3d *from, const coord3d *to, f32 fraction, coord3d *result);
void interpCatmullRom3(const coord3d *p0, const coord3d *p1, const coord3d *p2, const coord3d *p3, f32 fraction, coord3d *result);
void interpCardinal3(const coord3d *prev, const coord3d *start, const coord3d *end, const coord3d *next, f32 fraction, f32 tangentScale, coord3d *result);

#endif
