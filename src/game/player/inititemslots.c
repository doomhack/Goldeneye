#include <ultra64.h>
#include <memp.h>
#include "game/player/player.h"
#include "game/player/bondinv.h"
#include "game/player/inititemslots.h"

void invResetHeldTimes(void) {
    s32 i;
  
    g_CurrentPlayer->equipallguns = FALSE;
    
    for (i = 0; i != 10; i++) {
        g_CurrentPlayer->gunheldarr[i].totaltime = -1;
    }
}

void invAllocate(s32 additionalentries) {
  g_CurrentPlayer->equipmaxitems = additionalentries + 0x1e;
    g_CurrentPlayer->p_itemcur     = mempAlloc((g_CurrentPlayer->equipmaxitems * 0x14 + 0xfU | 0xf) ^ 0xf, MEMPOOL_STAGE);
  invReset();
}
