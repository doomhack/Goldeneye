#ifndef _BLOOD_H_
#define _BLOOD_H_

#include <ultra64.h>

Gfx *screenClearBlack(Gfx *gdl);
Gfx *gunbarrelSetupDraw(Gfx *gdl);
s32 bloodNextFrame(s32 mode);
Gfx *bloodRenderGunbarrel(Gfx *gdl);
Gfx *bloodRenderRedFill(Gfx *gdl);
Gfx *bloodRender(Gfx *gdl);

u8 *bloodDecodeFrame(u8 *src, u8 width, u8 height, u8 *dst, u8 *edgeOut);
void bloodImgTranspose(u8 *src, s32 srcwidth, s32 srcheight, u8 *dst);
void bloodSmoothDownRight(u8*, s32, u8*);
void bloodSmoothUpLeft(u8*, s32, u8*);
void bloodPack4Bit(u8*, u16, u8*);

#endif

