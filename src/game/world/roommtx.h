#ifndef _ROOMMTX_H_
#define _ROOMMTX_H_
#include <ultra64.h>

void roomMtxSetOriginRoom(s32 param_1);
void roomMtxTick(void);
void roomGetWorldPos(s32 index, struct coord3d *param_2);
Gfx * roomMtxLoad(Gfx *DL, s32 index);
struct coord3d* roomGetPos(s32 roomID);

#endif
