#include <ultra64.h>
#include <memp.h>
#include "bondview.h"
#include <PR/os.h>
#include <PR/gbi.h>
#include <macro.h>
#include "player.h"
#include <fr.h>

// bss
s32 z_buffer_width; // 0x8008e8f0
s32 z_buffer_height; // 0x8008e8f4

// data
s32 z_buffer = 0; // 0x8004f010

// TRUE when z_buffer is the gameplay buffer from zbufAllocate (sized for hi-res).
// Buffers handed in through zbufSetBuffer (front-end menus) keep their own width.
static s32 z_buffer_is_gameplay = FALSE;

/*
 * Address: 0x7f0d2870
*/
void zbufDeallocate(void) {
    z_buffer = 0;
}


/*
 * Address: 0x7f0d287c
*/
void zbufAllocate(void)
{
    if (resolution != 0)
    {
        z_buffer_width = Z_BUFFER_4_3_WIDTH;
        z_buffer_height = Z_BUFFER_4_3_HEIGHT;
    }
    else
    {
        // Sized for hi-res so the option can be toggled mid-level;
        // zbufInit sets the width actually used each frame.
        z_buffer_width = SCREEN_WIDTH * VI_MAX_SCALEX;

        if (getPlayerCount() == 1)
        {
            z_buffer_height = SCREEN_HEIGHT;
        }
        else
        {
            z_buffer_height = Z_BUFFER_HEIGHT;
        }
    }

    z_buffer = mempAllocBytesInBank((z_buffer_width * z_buffer_height * 2) + 64, MEMPOOL_STAGE);
    z_buffer = ALIGN64_V1(z_buffer);
    z_buffer_is_gameplay = (resolution == 0);
}


/*
 * Address: 0x7f0d2938
*/
void zbufSetBuffer(s32 buffer, s32 width, s32 height) {
    z_buffer = buffer;
    z_buffer_width = width;
    z_buffer_height = height;
    z_buffer_is_gameplay = FALSE;
}


/*
 * Address: 0x7f0d2954
*/
Gfx *zbufInit(Gfx *gdl) {
    s32 phi_a3;
    s32 test;
    if (z_buffer == 0) {
        zbufAllocate();
    }
    if (z_buffer_is_gameplay) {
        z_buffer_width = VI_SX(SCREEN_WIDTH);
    }
    if (!(get_cur_playernum() < 2) || ((getPlayerCount() == 2) && (get_cur_playernum() == 1))) {
        phi_a3 = z_buffer_width * SCREEN_HEIGHT;
    } else {
        phi_a3 = 0;
    }
    test = (z_buffer - phi_a3);
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
    gDPPipeSync(gdl++);
    gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, z_buffer_width, OS_K0_TO_PHYSICAL(z_buffer));
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetFillColor(gdl++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)));
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, VI_SX(viGetX()), viGetY());
    if (getPlayerCount() < 3) {
        start_x = 0;
        end_x = VI_SX(viGetX()) - 1;
    } else if ((get_cur_playernum() == 0) || (get_cur_playernum() == 2)) {
        start_x = 0;
        end_x = VI_SX(viGetX() / 2) - 1;
    } else {
        start_x = VI_SX(viGetX() / 2);
        end_x = VI_SX(viGetX()) - 1;
    }
    gDPFillRectangle(gdl++, start_x, 0, end_x, (z_buffer_height - 1));
    gDPPipeSync(gdl++);
    return gdl;
}
