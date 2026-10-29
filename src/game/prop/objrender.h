#ifndef _OBJRENDER_H_
#define _OBJRENDER_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

Gfx *weaponRenderTracers(Gfx *gdl);
Gfx *objRenderProp(PropRecord *prop, Gfx *gdl, s32 xlupass);

bool objGetOnscreenRenderBounds(PropRecord *prop, coord3d *viewpos, coord2d *xrange, coord2d *yrange);
bool propGetRoomsBbox2d(PropRecord *prop, bbox2d *bbox);
f32  propGetFogFade(PropRecord *prop, f32 size);
bool posIsOnScreen(PropRecord *prop, coord3d *pos, f32 radius, bool checkFogEnd);

void objGetTileShading(PropRecord *prop, rgba_u8 *colour);
void objGetShadeColour(PropRecord *prop, rgba_u8 *colour);
void objUpdateShading(rgba_u8 *dest, rgba_u8 *src);
void shadingLerp(rgba_s32 *dest, s32 enable, rgba_f32 *src);

#endif
