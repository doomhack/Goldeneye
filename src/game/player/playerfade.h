#ifndef _PLAYERFADE_H_
#define _PLAYERFADE_H_
#include <ultra64.h>
#include <bondtypes.h>

// Screen fades and fading the player's body in and out

Gfx *currentPlayerDrawFade(Gfx *gdl);
Gfx *coopDrawViewFade(Gfx *gdl);
void currentPlayerSetFadeColour(s32 r, s32 g, s32 b, f32 frac);
void currentPlayerAdjustFade(f32 maxfadetime, s32 r, s32 g, s32 b, f32 frac);
void currentPlayerSetFadeFrac(f32 maxfadetime, f32 frac);
bool currentPlayerIsFadeComplete(void);
void currentPlayerTickColourFade(void);
void currentPlayerStartChrFade(f32 duration60, f32 targetfrac);
void currentPlayerTickChrFade(void);

#endif
