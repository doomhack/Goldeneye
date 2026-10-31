#ifndef _WATCHMODEL_H_
#define _WATCHMODEL_H_
#include <ultra64.h>
#include <bondtypes.h>

// Drawing items and the controller in 3D on the watch menu

Gfx *watchRenderItem(Gfx *gdl, ITEM_IDS itemid, Mtxf *mtx, s32 alpha, s32 colour);
void watchControllerButtonMtx(coord3d *base, Mtxf *m1, Mtxf *m2, Mtxf *m3, coord3d *pos, Mtxf *m5, Mtxf *out);
Gfx *watchRenderController(Gfx *gdl, Mtxf *basemtx, s32 envcolour, bool animatebuttons, WatchContButtonPositions *buttonpositions, s8 *contpadnum);
Gfx *watchRenderControllerOpaque(Gfx *gdl, Mtxf *basemtx, bool animatebuttons, WatchContButtonPositions *buttonpositions, s8 *contpadnum);

f32 itemGetWatchPosX(ITEM_IDS item);
f32 itemGetWatchPosY(ITEM_IDS item);
f32 itemGetWatchPosZ(ITEM_IDS item);
f32 itemGetWatchRotX(ITEM_IDS item);
f32 itemGetWatchRotY(ITEM_IDS item);
f32 itemGetWatchFov(void);
char *itemGetWatchTitle1(ITEM_IDS item);
char *itemGetWatchTitle2(ITEM_IDS item);
char *itemGetWatchText(ITEM_IDS item);
f32 itemGetEquipFov(void);
f32 itemGetEquipPosX(ITEM_IDS item);
f32 itemGetEquipPosY(ITEM_IDS item);
f32 itemGetEquipPosZ(ITEM_IDS item);
f32 invGetEquipFovForIndex(void);
f32 invGetEquipPosXForIndex(s32 index);
f32 invGetEquipPosYForIndex(s32 index);
f32 invGetEquipPosZForIndex(s32 index);
char *invGetWatchTitle1ForIndex(s32 index);
char *invGetWatchTitle2ForIndex(s32 index);
f32 invGetWatchFovForIndex(void);
f32 invGetWatchPosXForIndex(s32 index);
f32 invGetWatchPosYForIndex(s32 index);
f32 invGetWatchPosZForIndex(s32 index);
f32 invGetWatchRotXForIndex(s32 index);
f32 invGetWatchRotYForIndex(s32 index);

#endif
