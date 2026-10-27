#include <ultra64.h>
#include "game/player/ejectedcartridges.h"
#include "bondtypes.h"
#include "game/player/gun.h"

// Copied from another file. Might need code reorganization to prevent the use of extra externs.
extern ALSoundState* g_CasingSfxState;
extern ALSoundState* g_UnusedSfxState;
extern ALSoundState* g_ImpactSfxStates[NUM_IMPACT_SFX_STATES];

extern u32 g_GunDebApplyKeyframe;
extern u32 g_gunDebKeyframeIndex;

extern CartridgeModelFileRecord ejected_cartridge[] ;

void casingsLoadModels(void) 
{
    s32 i = 0;

    g_CasingSfxState = NULL;

    while (i < NUM_IMPACT_SFX_STATES)
    {
        g_ImpactSfxStates[i] = NULL;
        i++;
    }

    i = 0;
    g_GunDebApplyKeyframe = 0;
    g_gunDebKeyframeIndex = 0;

    while (ejected_cartridge[i].header != 0) 
    {
        modelFileLoadToPool(ejected_cartridge[i].header, ejected_cartridge[i].text);
        i++;
    }
}
