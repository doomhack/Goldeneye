#ifndef _HUD_H_
#define _HUD_H_
#include <ultra64.h>

Gfx *hudRender(Gfx *gdl);
Gfx *hudRenderHealthBars(Gfx *gdl);
Gfx *hudRenderCredits(Gfx *gdl);

extern s32 g_CreditsScrollFrame;
extern s32 credits_state;
extern struct CreditsEntry_s *credits_pointer;

#endif
