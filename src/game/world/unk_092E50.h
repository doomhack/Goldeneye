#ifndef _UNK_092E50_H_
#define _UNK_092E50_H_
#include <ultra64.h>
#include "game/player/bondview.h"


extern f32 g_SkyCloudOffset;
extern struct hand D_8003FDA0;

extern f32 g_WaterScrollS;
extern f32 g_WaterScrollT;
extern f32 g_WaterBlendPhase;

void waterTick(void);
Gfx* waterSetupGdl(Gfx *gdl, s32 arg1);
Gfx* waterSetupGdlCi(Gfx *gdl, s32 arg1);

#endif
