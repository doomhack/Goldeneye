#ifndef _BLOOD_ANIMATION_H_
#define _BLOOD_ANIMATION_H_

#include <ultra64.h>

Gfx *screenClearBlack(Gfx *gdl);
Gfx *gunbarrelSetupDraw(Gfx *gdl);
s32 bloodNextFrame(s32 arg0);
Gfx *bloodRenderGunbarrel(Gfx *gdl);
Gfx *bloodRenderRedFill(Gfx *gdl);
Gfx *bloodRender(Gfx *gdl);

#endif

