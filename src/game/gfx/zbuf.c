/**
 * The z-buffer: allocated in the stage pool the first time a level draws (wide enough for
 * hi-res, so the option can change mid-level), and cleared for each player's view as it is
 * drawn. Front-end screens hand in their own buffer instead (zbufSetBuffer).
 */

#include <ultra64.h>
#include <memp.h>
#include <PR/os.h>
#include <PR/gbi.h>
#include <macro.h>
#include "game/player/player.h"
#include <fr.h>
#include "game/level/gamemode.h"

static u8 *s_Zbuf;
static s32 s_ZbufWidth;
static s32 s_ZbufHeight;

// True when s_Zbuf is the gameplay buffer from zbufAllocate (sized for hi-res).
// Buffers handed in through zbufSetBuffer (front-end menus) keep their own width.
static bool s_ZbufIsGameplay;

/** Forgets the buffer (the stage pool it was in is being freed) */
void zbufDeallocate(void)
{
    s_Zbuf = NULL;
}

static void zbufAllocate(void)
{
    if (resolution != 0)
    {
        s_ZbufWidth = Z_BUFFER_4_3_WIDTH;
        s_ZbufHeight = Z_BUFFER_4_3_HEIGHT;
    }
    else
    {
        // Sized for hi-res so the option can be toggled mid-level;
        // zbufInit sets the width actually used each frame.
        s_ZbufWidth = SCREEN_WIDTH * VI_MAX_SCALEX;

        // Co-op also shows one view full-screen at times (watch, cutscenes).
        if ((getPlayerCount() == 1) || gameIsTwoPlayerMission())
        {
            s_ZbufHeight = SCREEN_HEIGHT;
        }
        else
        {
            s_ZbufHeight = Z_BUFFER_HEIGHT;
        }
    }

    s_Zbuf = mempAlloc((s_ZbufWidth * s_ZbufHeight * 2) + 64, MEMPOOL_STAGE);
    s_Zbuf = (u8 *)ALIGN64_V1((uintptr_t)s_Zbuf);
    s_ZbufIsGameplay = resolution == 0;
}


/** Uses `buffer` as the z-buffer, for the front-end screens */
void zbufSetBuffer(void *buffer, s32 width, s32 height)
{
    s_Zbuf = buffer;
    s_ZbufWidth = width;
    s_ZbufHeight = height;
    s_ZbufIsGameplay = false;
}


/**
 * Points the RDP at the z-buffer for the current player's view, allocating it the first
 * time. The bottom view of a stacked split screen (and players 3 and 4) start it a
 * screen's worth of rows earlier, so that their rows land in the buffer.
 */
Gfx *zbufInit(Gfx *gdl)
{
    s32 offset = 0;

    if (s_Zbuf == NULL)
    {
        zbufAllocate();
    }

    if (s_ZbufIsGameplay)
    {
        s_ZbufWidth = VI_SX(SCREEN_WIDTH);
    }

    if (playerGetCurrentNum() >= 2
        || (getViewCount() == 2 && !coopSplitSideBySide() && coopPlayerPerson(playerGetCurrentNum()) == 1))
    {
        offset = s_ZbufWidth * SCREEN_HEIGHT;
    }

    gDPPipeSync(gdl++);
    gDPSetDepthImage(gdl++, (uintptr_t)(s_Zbuf - offset) & ~0x3f);

    return gdl;
}


/** Clears the current player's part of the z-buffer */
Gfx *zbufClearCurrentPlayer(Gfx *gdl)
{
    s32 start_x;
    s32 end_x;
    s32 height = s_ZbufHeight;

    // Stacked views (co-op or 2 player multiplayer) only need half the rows
    // cleared.
    if ((getViewCount() == 2) && !coopSplitSideBySide() && (height > Z_BUFFER_HEIGHT)) {
        height = Z_BUFFER_HEIGHT;
    }

    gDPPipeSync(gdl++);
    gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, s_ZbufWidth, OS_K0_TO_PHYSICAL(s_Zbuf));
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetFillColor(gdl++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)));
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, VI_SX(viGetX()), VI_SY(viGetY()));
    if (!viewIsHalfWidth()) {
        start_x = 0;
        end_x = VI_SX(viGetX()) - 1;
    } else if ((coopPlayerPerson(playerGetCurrentNum()) == 0) || (playerGetCurrentNum() == 2)) {
        start_x = 0;
        end_x = VI_SX(viGetX() / 2) - 1;
    } else {
        start_x = VI_SX(viGetX() / 2);
        end_x = VI_SX(viGetX()) - 1;
    }
    gDPFillRectangle(gdl++, start_x, 0, end_x, (height - 1));
    gDPPipeSync(gdl++);
    return gdl;
}
