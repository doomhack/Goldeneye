#include <ultra64.h>
#include <bondconstants.h>
#include <memp.h>
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/debug/deb_loadallmodels.h"
#include "game/prop/setup.h"
#include "aicommands2.h"
#include "game/gfx/dyn.h"
#include "memp.h"
#include <macro.h>

#ifndef DEBUG
    #define osSyncPrintf(...)
#endif

//i belong in a header, probably to another file

ChrRecord g_BgChrTemplate = { 0 };

WeaponProjectileModels weapon_models_for_weapon_load = {
    PROJECTILES_TYPE_KNIFE, PROJECTILES_TYPE_GRENADE,
    PROJECTILES_TYPE_REMOTE_MINE, PROJECTILES_TYPE_PROX_MINE,
    PROJECTILES_TYPE_TIMED_MINE, PROJECTILES_TYPE_ROCKET_ROUND,
    PROJECTILES_TYPE_GLAUNCH_ROUND, 0xFFFFFFFF
};

void init_obj_register_difficulty_vals(void) {
    objectiveregisters1 = 0;
    g_AiAccuracyModifier = 1.0f;
    g_AiDamageModifier = 1.0f;
    g_AiHealthModifier = 1.0f;
    g_AiReactionSpeed = 1.0f;
    g_SeenBondRecentlyGuardCount = 0;
}


void bgChrsInit(void)
{
    s32 i;
    s32 count;

    g_BgChrs = NULL;
    g_NumBgChrs = 0;

    if (!g_CurrentSetup.ailists) { return; }

    count = 0;

    // Count the number of background AI lists
    i = 0;
    while (g_CurrentSetup.ailists[i].ailist)
    {
        if (g_CurrentSetup.ailists[i].ID >= 0x1000)
        {
            g_NumBgChrs++;
        }

        i++;
    }

    if (g_NumBgChrs > 0)
    {
        ChrRecord tmp;

        // Allocate BG chrs
        g_BgChrs = mempAlloc(ALIGN16_a(g_NumBgChrs * sizeof(ChrRecord)), MEMPOOL_STAGE);

        // Initialise BG chrs
        i = 0;
        while (g_CurrentSetup.ailists[i].ailist)
        {
            if (g_CurrentSetup.ailists[i].ID >= 0x1000)
            {
                tmp = g_BgChrTemplate;

                g_BgChrs[count] = tmp;

                g_BgChrs[count].chrnum = 0xFE;
                g_BgChrs[count].ailist = g_CurrentSetup.ailists[i].ailist;
                g_BgChrs[count].aioffset = 0;
                g_BgChrs[count].aireturnlist = -1;
                g_BgChrs[count].actiontype = ACT_NULL;
                count++;
            }

            i++;
        }
    }
}


void debug_object_load_all_models(void)
{
    u8 *cmd = &g_CurrentSetup.ailists[0].ailist->cmd;
    s32 i = 0;
    u16 id;

    if (!cmd) { return; }

    do
    {
        while (TRUE)
        {
            if (cmd[0] == AI_EndList)
            {
                break;
            }

            switch (cmd[0])
            {
                case AI_TRYDroppingItem:
                    id = cmd[2] | (cmd[1] << 8);
                    if (modelLoad(id)) osSyncPrintf("loaded dropguntype obj=%d", id);
                    break;

                case AI_TRYSpawningChrAtPad:
                    if (bodyLoad(cmd[1])) osSyncPrintf("loaded createchrhead obj=%d (body)", cmd[1]);
                    if ((s8)cmd[2] >= 0 && bodyLoad((s8)cmd[2])) osSyncPrintf("loaded createchrhead obj=%d (head)", cmd[2]);
                    break;

                case AI_TRYSpawningChrNextToChr:
                    if (bodyLoad(cmd[1])) osSyncPrintf("loaded createchrheadatchr obj=%d (body)", cmd[1]);
                    if ((s8)cmd[2] >= 0 && bodyLoad((s8)cmd[2])) osSyncPrintf("loaded createchrheadatchr obj=%d (head)", cmd[2]);
                    break;

                case AI_TRYGiveMeItem:
                    if (modelLoad(cmd[2] | (cmd[1] << 8))) osSyncPrintf("loaded createweaponheldchr obj=%d", cmd[2] | (cmd[1] << 8));
                    if (weaponLoadProjectileModels(cmd[3])) osSyncPrintf("loaded createweaponheldchr obj=%d (ammo)", cmd[2] | (cmd[1] << 8));
                    break;

                case AI_TRYGiveMeHat:
                    if (modelLoad(cmd[2] | (cmd[1] << 8))) osSyncPrintf("loaded createhatonchr obj=%d", cmd[2] | (cmd[1] << 8));
                    break;
            }

            cmd += aiCommandSize(cmd, 0);
        }

        i++;
        cmd = &g_CurrentSetup.ailists[i].ailist->cmd;
    } while (cmd);
}


void debug_weapon_load_table(void)
{
    WeaponProjectileModels tmp = weapon_models_for_weapon_load;
    s32 i;

    for (i = 0; tmp.array[i] >= 0; i++)
    {
         if (modelLoad(tmp.array[i])) osSyncPrintf("loaded weapon obj=%d", tmp.array[i]);
    }
}
#undef osSyncPrintf
