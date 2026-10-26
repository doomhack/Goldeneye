#include <ultra64.h>
#include "game/prop/glass.h"



void glassResetShards(void) {
    s32 i;
    for (i = 0; i < SHATTERED_WINDOW_PIECES_BUFFER_LEN; i++)
    {
        ptr_shattered_window_pieces[i].active = 0;
    }
}

