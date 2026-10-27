/**
 * The z-buffer: allocated in the stage pool the first time a level draws (wide enough for
 * hi-res, so the option can change mid-level), and cleared for each player's view as it is
 * drawn. Front-end screens hand in their own buffer instead (zbufSetBuffer).
 */

#include <ultra64.h>
#include <memp.h>
#include "game/player/bondview.h"
#include <PR/os.h>
#include <PR/gbi.h>
#include <macro.h>
#include "game/player/player.h"
#include <fr.h>

// bss
s32 g_ZbufWidth; // 0x8008e8f0
s32 g_ZbufHeight; // 0x8008e8f4

// data
s32 g_Zbuf = 0; // 0x8004f010

// TRUE when g_Zbuf is the gameplay buffer from zbufAllocate (sized for hi-res).
// Buffers handed in through zbufSetBuffer (front-end menus) keep their own width.
static s32 g_ZbufIsGameplay = FALSE;

void zbufDeallocate(void) {
    g_Zbuf = 0;
}


void zbufAllocate(void)
{
    if (resolution != 0)
    {
        g_ZbufWidth = Z_BUFFER_4_3_WIDTH;
        g_ZbufHeight = Z_BUFFER_4_3_HEIGHT;
    }
    else
    {
        // Sized for hi-res so the option can be toggled mid-level;
        // zbufInit sets the width actually used each frame.
        g_ZbufWidth = SCREEN_WIDTH * VI_MAX_SCALEX;

        // Co-op also shows one view full-screen at times (watch, cutscenes).
        if ((getPlayerCount() == 1) || g_CoopMode)
        {
            g_ZbufHeight = SCREEN_HEIGHT;
        }
        else
        {
            g_ZbufHeight = Z_BUFFER_HEIGHT;
        }
    }

    g_Zbuf = mempAlloc((g_ZbufWidth * g_ZbufHeight * 2) + 64, MEMPOOL_STAGE);
    g_Zbuf = ALIGN64_V1(g_Zbuf);
    g_ZbufIsGameplay = (resolution == 0);
}


void zbufSetBuffer(s32 buffer, s32 width, s32 height) {
    g_Zbuf = buffer;
    g_ZbufWidth = width;
    g_ZbufHeight = height;
    g_ZbufIsGameplay = FALSE;
}


Gfx *zbufInit(Gfx *gdl) {
    s32 offset;
    s32 addr;
    if (g_Zbuf == 0) {
        zbufAllocate();
    }
    if (g_ZbufIsGameplay) {
        g_ZbufWidth = VI_SX(SCREEN_WIDTH);
    }
    if (!(playerGetCurrentNum() < 2) || ((getViewCount() == 2) && !coopSplitSideBySide() && (coopPlayerPerson(playerGetCurrentNum()) == 1))) {
        offset = g_ZbufWidth * SCREEN_HEIGHT;
    } else {
        offset = 0;
    }
    addr = (g_Zbuf - offset);
    addr = addr & ~0x3F;
    gDPPipeSync(gdl++);
    gDPSetDepthImage(gdl++, addr);
    return gdl;
}


Gfx *zbufClearCurrentPlayer(Gfx *gdl) {
    s32 start_x;
    s32 end_x;
    s32 height = g_ZbufHeight;

    // Stacked views (co-op or 2 player multiplayer) only need half the rows
    // cleared.
    if ((getViewCount() == 2) && !coopSplitSideBySide() && (height > Z_BUFFER_HEIGHT)) {
        height = Z_BUFFER_HEIGHT;
    }

    gDPPipeSync(gdl++);
    gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, g_ZbufWidth, OS_K0_TO_PHYSICAL(g_Zbuf));
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetFillColor(gdl++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)));
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, VI_SX(viGetX()), viGetY());
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
