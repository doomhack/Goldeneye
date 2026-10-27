#include <ultra64.h>
#include <bondtypes.h>
#include <random.h>
#include "boss.h"
#include "game/chr/chr.h"
#include "game/level/lv.h"
#include "game/mission/objective_status.h"
#include "game/player/player.h"
#include "game/replay/testhooks.h"

s32 g_TestHooksEnabled = 0;
u32 g_TestStateLogCount = 0;
u32 g_TestStateLog[TEST_STATE_LOG_LEN][3];
s32 g_TestForceDemo = -1;
s32 g_TestDemoDesynced = 0;

/* FNV-1a */
static u32 hashWord(u32 hash, u32 value)
{
    for (s32 i = 0; i < 4; i++)
    {
        hash ^= (value >> (i * 8)) & 0xff;
        hash *= 16777619u;
    }

    return hash;
}

static u32 hashFloat(u32 hash, f32 value)
{
    union { f32 f; u32 u; } bits;

    bits.f = value;
    return hashWord(hash, bits.u);
}

static u32 hashRounded(u32 hash, f32 value, f32 scale)
{
    return hashWord(hash, (u32)(s32)(value * scale + (value < 0.0f ? -0.5f : 0.5f)));
}

void testhooksEndFrame(void)
{
    u32 exact = 2166136261u;
    u32 coarse = 2166136261u;
    s32 i;
    u32 *entry;

    if (!g_TestHooksEnabled || bossGetStageNum() >= LEVELID_MAX)
    {
        return;
    }

    exact = hashWord(exact, (u32)g_randomSeed);
    exact = hashWord(exact, (u32)(g_randomSeed >> 32));
    coarse = hashWord(coarse, (u32)g_randomSeed);

    for (i = 0; i < 4; i++)
    {
        struct player *player = g_playerPointers[i];

        if (player == NULL || player->prop == NULL)
        {
            continue;
        }

        exact = hashFloat(exact, player->prop->pos.x);
        exact = hashFloat(exact, player->prop->pos.y);
        exact = hashFloat(exact, player->prop->pos.z);
        exact = hashFloat(exact, player->bondhealth);
        exact = hashFloat(exact, player->bondarmour);

        coarse = hashRounded(coarse, player->prop->pos.x, 1.0f);
        coarse = hashRounded(coarse, player->prop->pos.y, 1.0f);
        coarse = hashRounded(coarse, player->prop->pos.z, 1.0f);
        coarse = hashRounded(coarse, player->bondhealth, 100.0f);
        coarse = hashRounded(coarse, player->bondarmour, 100.0f);
    }

    for (i = 0; i < g_NumChrSlots; i++)
    {
        ChrRecord *chr = &g_ChrSlots[i];

        if (chr->model == NULL || chr->prop == NULL)
        {
            continue;
        }

        exact = hashWord(exact, chr->chrnum);
        exact = hashWord(exact, chr->actiontype);
        exact = hashFloat(exact, chr->prop->pos.x);
        exact = hashFloat(exact, chr->prop->pos.y);
        exact = hashFloat(exact, chr->prop->pos.z);
        exact = hashFloat(exact, chr->damage);

        coarse = hashWord(coarse, chr->chrnum);
        coarse = hashRounded(coarse, chr->prop->pos.x, 1.0f);
        coarse = hashRounded(coarse, chr->prop->pos.y, 1.0f);
        coarse = hashRounded(coarse, chr->prop->pos.z, 1.0f);
        coarse = hashRounded(coarse, chr->damage, 100.0f);
    }

    for (i = 0; i < OBJECTIVES_MAX; i++)
    {
        exact = hashWord(exact, g_ObjectiveStatuses[i]);
        coarse = hashWord(coarse, g_ObjectiveStatuses[i]);
    }

    entry = g_TestStateLog[g_TestStateLogCount % TEST_STATE_LOG_LEN];
    entry[0] = g_GlobalTimer;
    entry[1] = exact;
    entry[2] = coarse;
    g_TestStateLogCount++;
}
