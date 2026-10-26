#include <ultra64.h>
#include "game/chr/chr.h"


void cleanupGuardData(void)
{
    int i;

    for (i = 0; i < g_NumChrSlots; i++) {
        if (g_ChrSlots[i].model != 0) {
            chrpropCleanupForRemoval(g_ChrSlots[i].prop);
            propDelist(g_ChrSlots[i].prop);
            propDisable(g_ChrSlots[i].prop);
            propFree(g_ChrSlots[i].prop);
        }
    }
}
