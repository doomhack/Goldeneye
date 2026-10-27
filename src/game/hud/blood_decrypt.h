#ifndef _BLOOD_DECRYPT_H_
#define _BLOOD_DECRYPT_H_

#include <ultra64.h>

u8 *bloodDecodeFrame(u8 *arg0, u8 arg1, u8 arg2, u8 *arg3, u8 *arg4);

void bloodImgTranspose(u8 *src, s32 srcwidth, s32 srcheight, u8 *dst);
void bloodSmoothDownRight(u8*, s32, u8*);
void bloodSmoothUpLeft(u8*, s32, u8*);
void bloodPack4Bit(u8*, u16, u8*);
u8 *sub_GAME_7F01D1C0(u8 *arg0, s32 arg1, s32 arg2, u8 *arg3);

#endif

