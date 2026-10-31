#ifndef _WATER_H_
#define _WATER_H_
#include <ultra64.h>
#include "game/player/player.h"


extern f32 g_SkyCloudOffset;

extern f32 g_WaterScrollS;
extern f32 g_WaterScrollT;
extern f32 g_WaterBlendPhase;

void waterReset(void);
void waterTick(void);
Gfx* waterSetupGdl(Gfx *gdl, s32 usePrebuiltDl);
Gfx* waterSetupGdlCi(Gfx *gdl, s32 usePrebuiltDl);

#endif
