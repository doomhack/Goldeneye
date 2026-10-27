#ifndef _SKY_H_
#define _SKY_H_
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player/bondview.h"

void skySetStageNum(s32 stagenum);
void skyTick(void);
Gfx * skyRender(Gfx *gdl);
void skyProjectVertex(SkyVtx *arg0, Mtxf *arg1, u16 arg2, f32 arg3, f32 arg4, SkyScreenVtx *arg5);
Gfx *skyRenderTri(Gfx *gdl, SkyScreenVtx *arg1, SkyScreenVtx *arg2, SkyScreenVtx *arg3, f32 arg4, bool textured);
Gfx *skyRenderFull(Gfx *gdl, SkyScreenVtx *arg1, SkyScreenVtx *arg2, SkyScreenVtx *arg3, SkyScreenVtx *arg4, f32 arg5);

#endif
