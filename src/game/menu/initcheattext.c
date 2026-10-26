#include <ultra64.h>
#include "game/menu/initcheattext.h"
#include "game/player/player.h"
#include "game/menu/cheat.h"

void initCheatTextBuffer(void) {
    int i;
    
    for(i=0;i<75;i++)
    {
        g_CheatPlayerTextRelated[i] = 0;
    }

}


void disableOnscreenCheatText(void)
{
  g_CurrentPlayer->cheatInputBufferIndex = 0;
  g_CurrentPlayer->cheatInputCount = 0;
}

