#ifndef _SKY_H_
#define _SKY_H_
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player/player.h"

void skySetStageNum(s32 stagenum);
void skyTick(void);
Gfx * skyRender(Gfx *gdl);
void skyProjectVertex(SkyVtx *vtx, Mtxf *mtx, u16 perspScale, f32 sScale, f32 tScale, SkyScreenVtx *out);
Gfx *skyRenderTri(Gfx *gdl, SkyScreenVtx *v1, SkyScreenVtx *v2, SkyScreenVtx *v3, f32 perspScale, bool textured);
Gfx *skyRenderFull(Gfx *gdl, SkyScreenVtx *v1, SkyScreenVtx *v2, SkyScreenVtx *v3, SkyScreenVtx *v4, f32 perspScale);

#endif
