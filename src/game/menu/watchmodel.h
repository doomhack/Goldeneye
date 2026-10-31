#ifndef _WATCHMODEL_H_
#define _WATCHMODEL_H_
#include <ultra64.h>
#include <bondtypes.h>

// Drawing items and the controller in 3D on the watch menu

Gfx *watchRenderItem(Gfx *gdl, ITEM_IDS itemid, Mtxf *mtx, s32 alpha, s32 colour);
void watchControllerButtonMtx(coord3d *base, Mtxf *m1, Mtxf *m2, Mtxf *m3, coord3d *pos, Mtxf *m5, Mtxf *out);
Gfx *watchRenderController(Gfx *gdl, Mtxf *basemtx, s32 envcolour, bool animatebuttons, WatchContButtonPositions *buttonpositions, s8 *contpadnum);
Gfx *watchRenderControllerOpaque(Gfx *gdl, Mtxf *basemtx, bool animatebuttons, WatchContButtonPositions *buttonpositions, s8 *contpadnum);

#endif
