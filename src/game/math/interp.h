#ifndef _INTERP_H_
#define _INTERP_H_

#include <ultra64.h>
#include <bondtypes.h>

void interpLerp3(coord3d *from, coord3d *to, f32 fraction, coord3d *result);
void interpCatmullRom3(coord3d *p0, coord3d *p1, coord3d *p2, coord3d *p3, f32 fraction, coord3d *result);
void interpCardinal3(coord3d *prev, coord3d *start, coord3d *end, coord3d *next, f32 fraction, f32 tangentScale, coord3d *result);

#endif
