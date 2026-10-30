#ifndef _AUTOAIM_H_
#define _AUTOAIM_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

extern f32 g_AutoAimTolerance;

f32  autoaimScoreProp(PropRecord *targetprop, coord3d *aimpos, f32 *xbounds, f32 *ybounds, coord2d *out_screen);
void autoaimUpdateTarget(void);

#endif
