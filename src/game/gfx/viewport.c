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

/*
 * Address: 0x7f0d2870
*/
void zbufDeallocate(void) {
    g_Zbuf = 0;
}


/*
 * Address: 0x7f0d287c
*/
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


/*
 * Address: 0x7f0d2938
*/
void zbufSetBuffer(s32 buffer, s32 width, s32 height) {
    g_Zbuf = buffer;
    g_ZbufWidth = width;
    g_ZbufHeight = height;
    g_ZbufIsGameplay = FALSE;
}


/*
 * Address: 0x7f0d2954
*/
Gfx *zbufInit(Gfx *gdl) {
    s32 phi_a3;
    s32 test;
    if (g_Zbuf == 0) {
        zbufAllocate();
    }
    if (g_ZbufIsGameplay) {
        g_ZbufWidth = VI_SX(SCREEN_WIDTH);
    }
    if (!(get_cur_playernum() < 2) || ((getViewCount() == 2) && !coopSplitSideBySide() && (coopPlayerPerson(get_cur_playernum()) == 1))) {
        phi_a3 = g_ZbufWidth * SCREEN_HEIGHT;
    } else {
        phi_a3 = 0;
    }
    test = (g_Zbuf - phi_a3);
    test = test & ~0x3F;
    gDPPipeSync(gdl++);
    gDPSetDepthImage(gdl++, test);
    return gdl;
}


/*
 * Address: 0x7f0d2a08
*/
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
    } else if ((coopPlayerPerson(get_cur_playernum()) == 0) || (get_cur_playernum() == 2)) {
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
