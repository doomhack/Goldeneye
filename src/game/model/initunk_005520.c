#include <ultra64.h>
#include <memp.h>
#include "game/model/model.h"
#include "game/model/initunk_005520.h"
#include "game/model/objecthandler.h"
#include "memp.h"

#define MODEL_SPARE_SLOTS          30
#define ANIM_MODEL_SPARE_SLOTS     10

#define MODEL_SPARE_RWDATALEN      0x14
#define ANIM_MODEL_SPARE_RWDATALEN 0x8c


void modelmgrResetSlotCounts(void)
{
    g_MaxAnimModelSlots = 0;
    g_MaxModelSlots = 0;
}


void modelmgrSetLevelResetting(bool resetting) 
{
    g_ModelIsLvResetting = resetting;
}

//this may be a file split

/**
 * NTSC address 0x7F005540.
*/
void modelmgrAllocateModelSlots(s32 numobjs)
{
    s32 i;

    g_MaxModelSlots = numobjs + MODEL_SPARE_SLOTS;
    
    g_ModelSlots = mempAlloc(g_MaxModelSlots * sizeof(struct ModelSlot), MEMPOOL_STAGE);

    for (i = 0; i < g_MaxModelSlots; i++)
    {
        g_ModelSlots[i].obj = 0;

        if (i < numobjs)
        {
            g_ModelSlots[i].datas = NULL;
        }
        else
        {
            g_ModelSlots[i].datas = mempAlloc(MODEL_SPARE_RWDATALEN * sizeof(u32), MEMPOOL_STAGE);
            g_ModelSlots[i].rwdatalen = MODEL_SPARE_RWDATALEN;
        }
    }
}


/**
 * NTSC address 0x7F005540.
*/
void modelmgrAllocateAnimModelSlots(s32 numanimated)
{
    s32 temp_t6;
    s32 i;

    g_MaxAnimModelSlots = numanimated + ANIM_MODEL_SPARE_SLOTS;

    // mips2c says: g_AnimModelSlots = mempAlloc(temp_t6 * 0xC0, 4);
    // however, the pointer is incremented by 0xbc in the loop below.
    g_AnimModelSlots = mempAlloc(g_MaxAnimModelSlots * (4 + sizeof(struct AnimModelSlot)), MEMPOOL_STAGE);

    for (i = 0; i < g_MaxAnimModelSlots; i++)
    {
        g_AnimModelSlots[i].obj = 0;

        if (i < numanimated)
        {
            g_AnimModelSlots[i].datas = NULL;
        }
        else
        {
            g_AnimModelSlots[i].datas = mempAlloc(ANIM_MODEL_SPARE_RWDATALEN * sizeof(u32), MEMPOOL_STAGE);
            g_AnimModelSlots[i].rwdatalen = ANIM_MODEL_SPARE_RWDATALEN;
        }
    }
}
