#include <ultra64.h>
#include <snd.h>
#include <PR/libaudio.h>
#include "game/player/bondview.h"

void levelStopTankSfx(void) {
    if (g_TankSfxState[0] != 0)
    {
        if (sndGetPlayingState(g_TankSfxState[0]) != AL_STOPPED)
        {
            sndDeactivate(g_TankSfxState[0]);
        }
    }

    if (g_TankSfxState[1] != 0)
    {
        if (sndGetPlayingState(g_TankSfxState[1]) != AL_STOPPED)
        {
            sndDeactivate(g_TankSfxState[1]);
        }
    }

}
