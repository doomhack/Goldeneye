/**
 * Fades: the coloured screen fade over a player's view (currentPlayerAdjustFade,
 * currentPlayerDrawFade) and fading the player's body (currentPlayerStartChrFade).
 * See docs/subsystems/player.md.
 */
#include <fr.h>
#include "game/level/lv.h"
#include "game/menu/watchmenu.h"
#include "game/player/player.h"
#include "game/world/bgfog.h"
#include "game/player/playerfade.h"


/**
 * Draws the current player's screen fade: a rectangle of the fade colour over
 * their view. Black while the camera buffers are being switched.
 */
Gfx *currentPlayerDrawFade(Gfx *gdl)
{
    f32 frac = g_CurrentPlayer->colourscreenfrac;
    s32 r = g_CurrentPlayer->colourscreenred;
    s32 g = g_CurrentPlayer->colourscreengreen;
    s32 b = g_CurrentPlayer->colourscreenblue;
    if ((cameraFrameCounter1 != 0) || (cameraFrameCounter2 != 0)) {
        frac = 1.0f;
        b = 0;
        g = 0;
        r = 0;
    }
    if (frac > 0) {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        gDPSetColorDither(gdl++, G_CD_DISABLE);
        gDPSetTexturePersp(gdl++, G_TP_NONE);
        gDPSetAlphaCompare(gdl++, G_AC_NONE);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetTextureFilter(gdl++, G_TF_BILERP);
        gDPSetTextureConvert(gdl++, G_TC_FILT);
        gDPSetTextureLUT(gdl++, G_TT_NONE);
        gDPSetRenderMode(gdl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gdl++, 0, 0, r, g, b, (s32)(frac * 255.0f));
        gDPFillRectangle(gdl++, VI_SX(viGetViewLeft()), viGetViewTop(), VI_SX(viGetViewLeft() + viGetViewWidth()), (viGetViewTop() + viGetViewHeight()));
        gDPPipeSync(gdl++);
        gDPSetColorDither(gdl++, G_CD_BAYER);
        gDPSetTexturePersp(gdl++, G_TP_PERSP);
        gDPSetTextureLOD(gdl++, G_TL_LOD);
    }

    return gdl;
}


// Sets the screen fade's colour and opacity at once
void currentPlayerSetFadeColour(s32 r, s32 g, s32 b, f32 frac)
{
    g_CurrentPlayer->colourscreenred = r;
    g_CurrentPlayer->colourscreengreen = g;
    g_CurrentPlayer->colourscreenblue = b;
    g_CurrentPlayer->colourscreenfrac = frac;
}


// Fades the screen to the colour and opacity over maxfadetime (60ths of a second)
void currentPlayerAdjustFade(f32 maxfadetime, s32 r, s32 g, s32 b, f32 frac)
{
    g_CurrentPlayer->colourfadetime60    = 0;
    g_CurrentPlayer->colourfadetimemax60 = maxfadetime;
    g_CurrentPlayer->colourfaderedold    = g_CurrentPlayer->colourscreenred;
    g_CurrentPlayer->colourfaderednew    = r;
    g_CurrentPlayer->colourfadegreenold  = g_CurrentPlayer->colourscreengreen;
    g_CurrentPlayer->colourfadegreennew  = g;
    g_CurrentPlayer->colourfadeblueold   = g_CurrentPlayer->colourscreenblue;
    g_CurrentPlayer->colourfadebluenew   = b;
    g_CurrentPlayer->colourfadefracold   = g_CurrentPlayer->colourscreenfrac;
    g_CurrentPlayer->colourfadefracnew   = frac;
}


// Fades the screen's opacity to frac, keeping its colour
void currentPlayerSetFadeFrac(f32 maxfadetime, f32 frac)
{
    currentPlayerAdjustFade(maxfadetime, g_CurrentPlayer->colourscreenred, g_CurrentPlayer->colourscreengreen, g_CurrentPlayer->colourscreenblue, frac);
}


// TRUE once the screen fade has finished
bool currentPlayerIsFadeComplete(void)
{
	return g_CurrentPlayer->colourfadetimemax60 < 0;
}


// Steps the screen fade by this frame's time
void currentPlayerTickColourFade(void)
{
    if (g_CurrentPlayer->colourfadetimemax60 >= 0)
    {
        g_CurrentPlayer->colourfadetime60 += g_GlobalTimerDelta;

        if (g_CurrentPlayer->colourfadetime60 < g_CurrentPlayer->colourfadetimemax60)
        {
            f32 mult                           = g_CurrentPlayer->colourfadetime60 / g_CurrentPlayer->colourfadetimemax60;
            g_CurrentPlayer->colourscreenfrac  = g_CurrentPlayer->colourfadefracold + (g_CurrentPlayer->colourfadefracnew - g_CurrentPlayer->colourfadefracold) * mult;
            g_CurrentPlayer->colourscreenred   = g_CurrentPlayer->colourfaderedold + (s32)((g_CurrentPlayer->colourfaderednew - g_CurrentPlayer->colourfaderedold) * mult);
            g_CurrentPlayer->colourscreengreen = g_CurrentPlayer->colourfadegreenold + (s32)((g_CurrentPlayer->colourfadegreennew - g_CurrentPlayer->colourfadegreenold) * mult);
            g_CurrentPlayer->colourscreenblue  = g_CurrentPlayer->colourfadeblueold + (s32)((g_CurrentPlayer->colourfadebluenew - g_CurrentPlayer->colourfadeblueold) * mult);
            return;
        }

        g_CurrentPlayer->colourscreenfrac    = g_CurrentPlayer->colourfadefracnew;
        g_CurrentPlayer->colourscreenred     = g_CurrentPlayer->colourfaderednew;
        g_CurrentPlayer->colourscreengreen   = g_CurrentPlayer->colourfadegreennew;
        g_CurrentPlayer->colourscreenblue    = g_CurrentPlayer->colourfadebluenew;
        g_CurrentPlayer->colourfadetimemax60 = -1;
    }
}


/**
 * Fades the player's body (as others see it, and in the intro and death cameras)
 * to the opacity targetfrac over duration60.
 */
void currentPlayerStartChrFade(f32 duration60, f32 targetfrac)
{
    ChrRecord *chr = g_CurrentPlayer->prop->chr;

    if (chr)
    {
        g_CurrentPlayer->bondfadetime60    = 0;
        g_CurrentPlayer->bondfadetimemax60 = duration60;
        g_CurrentPlayer->bondfadefracold   = chr->fadealpha / 255.0f;
        g_CurrentPlayer->bondfadefracnew   = targetfrac;
    }
}


// Steps the body fade by this frame's time
void currentPlayerTickChrFade(void)
{
    if (g_CurrentPlayer->bondfadetimemax60 >= 0)
    {
        ChrRecord *chr = g_CurrentPlayer->prop->chr;
        f32        frac;

        g_CurrentPlayer->bondfadetime60 += g_GlobalTimerDelta;

        if (g_CurrentPlayer->bondfadetime60 < g_CurrentPlayer->bondfadetimemax60)
        {
            frac = g_CurrentPlayer->bondfadefracold + (g_CurrentPlayer->bondfadefracnew - g_CurrentPlayer->bondfadefracold) * g_CurrentPlayer->bondfadetime60 / g_CurrentPlayer->bondfadetimemax60;
        }
        else
        {
            frac = g_CurrentPlayer->bondfadefracnew;
            g_CurrentPlayer->bondfadetimemax60 = -1;
        }

        if (chr)
        {
            chr->fadealpha = (s8)(frac * 255);
        }
    }
}
