#ifndef _BLOOD_H_
#define _BLOOD_H_

#include <ultra64.h>

Gfx *screenClearBlack(Gfx *gdl);
Gfx *gunbarrelSetupDraw(Gfx *gdl);
s32 bloodNextFrame(s32 arg0);
Gfx *bloodRenderGunbarrel(Gfx *gdl);
Gfx *bloodRenderRedFill(Gfx *gdl);
Gfx *bloodRender(Gfx *gdl);

u8 *bloodDecodeFrame(u8 *arg0, u8 arg1, u8 arg2, u8 *arg3, u8 *arg4);
void bloodImgTranspose(u8 *src, s32 srcwidth, s32 srcheight, u8 *dst);
void bloodSmoothDownRight(u8*, s32, u8*);
void bloodSmoothUpLeft(u8*, s32, u8*);
void bloodPack4Bit(u8*, u16, u8*);

#endif

