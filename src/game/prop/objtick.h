#ifndef _OBJTICK_H_
#define _OBJTICK_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

TICKOP objTick(PropRecord *prop);
void   objTickOffscreen(PropRecord *prop);
void   objTickAttached(PropRecord *prop);
void   objDropRecursively(PropRecord *prop);
s32    glassCalculateOpacity(coord3d *pos, f32 xludist, f32 opadist, f32 minOpacity);

#endif
