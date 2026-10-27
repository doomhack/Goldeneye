#include <ultra64.h>
#include "game/menu/initcheattext.h"
#include "game/player/player.h"
#include "game/menu/cheat.h"

void cheatResetPlayerMasks(void) {
    int i;
    
    for(i=0;i<75;i++)
    {
        g_CheatPlayerMask[i] = 0;
    }

}


void cheatResetInput(void)
{
  g_CurrentPlayer->cheatInputBufferIndex = 0;
  g_CurrentPlayer->cheatInputCount = 0;
}

