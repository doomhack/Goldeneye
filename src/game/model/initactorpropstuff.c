#include <ultra64.h>
#include "game/model/initactorpropstuff.h"
#include "game/model/anim.h"
#include "game/chr/chr.h"
#include "game/player/gun.h"
#include "math.h"
#include "game/math/round.h"
#include "game/model/model.h"


/**
 * Gets the number of currently allocated heads and bodies
 * Note: Compile-time static? why bother with a function?
 */
void reset_counter_rand_body_head(void)
{
    num_bodies = 0;
    while (list_of_bodies[num_bodies] >= 0)
    {
        num_bodies++;
    }
    num_male_heads = 0;
    while (random_male_heads[num_male_heads] >= 0)
    {
        num_male_heads++;
    }

    num_female_heads = 0;
    while (random_female_heads[num_female_heads] >= 0)
    {
        num_female_heads++;
    }
}


u32 animSumRootTurn(void *ani, int aniid, int endframe) {
    coord16 motion;
    u16 result = 0;

    while (aniid < endframe) {
        result += animReadRootMotion16(0, 0, &skeleton_guard, ani, aniid, &motion);
        aniid++;
    }
    return result;
}


s32 animSumRootForward(ModelAnimation *anim, s32 startframe, s32 endframe)
{
    s32 sum;
    coord16 out;

    sum = 0;

    if (startframe < endframe)
    {
        do
        {
            animReadRootMotion16(0, 0, &skeleton_guard, anim, startframe, &out);

            startframe++;
            sum += out.z;
        }
        while (startframe < endframe);
    }

    return sum;
}


/**
 * pd is raceInitAnimGroup
 * 
 * Initializes a null-terminated table of weapon firing animation configs.
 * 
 * @returns the number of table entires.
 */
s32 initResolveAnimGroupTable(struct weapon_firing_animation_table *animconfig)
{
    s32 animoffset;
    s32 numconfigs;
    struct weapon_firing_animation_table *config;

    union
    {
        u32 offset;
        struct ModelAnimation *anim;
    } *initialanim;

    s32 endframe;
    u32 angle16;
    f32 duration;

    numconfigs = 0;
    config = animconfig;

    if (animconfig->anim.offset != 0)
    {
        f32 fullturn = M_TAU_F;
        f32 angleconv = 0.0000958738f;

        initialanim = (void *)&animconfig->anim;
        animoffset = (*initialanim).offset;

        do
        {
            config->anim.anim = (struct ModelAnimation *)(animoffset + (s32)g_AnimTable);
            endframe = mathFloorToInt(config->turnEndFrame);
            angle16 = animSumRootTurn(config->anim.anim, 0, endframe) & 0xffff;
            duration = config->turnEndFrame;

            if (duration > 0.0f)
            {
                if (((s32)angle16) < 0x8000)
                {
                    config->turn_angle_per_frame = (angle16 * angleconv) / duration;
                }
                else
                {
                    config->turn_angle_per_frame = ((angle16 * angleconv) - fullturn) / duration;
                }
            }
            else
            {
                config->turn_angle_per_frame = 0.0f;
            }

            animoffset = config[1].anim.offset;
            config++;
            numconfigs++;
        }
        while (animoffset != 0);
    }

    return numconfigs;
}


//pd is raceInitAnimGroups
void initResolveAnimGroups(struct anim_group_info **groups)
{
    s32 i;

    for (i = 0; i < 32; i++)
    {
        if (groups[i]->len < 0)
        {
            groups[i]->len = initResolveAnimGroupTable(groups[i]->table);
        }
    }
}


/**
 * Resolves each entry's anim offset into an absolute ModelAnimation*.
 * @returns the entry count.
 */
s32 initResolveAnimTable(struct StruckAnim *entries)
{
    s32 count = 0;

    for (; entries->struck_anim != NULL; entries++)
    {
        entries->struck_anim = (ModelAnimation *)((u32)g_AnimTable + (u32)entries->struck_anim);
        count++;
    }

    return count;
}


#define ANIM_PTR(anim) \
    ((ModelAnimation *)((s32)&anim + ((s32)g_AnimTable)))

#define ANIM_FRAC(anim) \
    ((((f32)animSumRootForward(ANIM_PTR(anim), 0, ANIM_PTR(anim)->numFrames - 1)) * 0.10000001f) / \
        ((f32)((u32)ANIM_PTR(anim)->numFrames)))

#define ANIM_FRAC_MUL_FIRST(anim) \
    ((0.10000001f * ((f32)animSumRootForward(ANIM_PTR(anim), 0, ANIM_PTR(anim)->numFrames - 1))) / \
        ((f32)((u32)ANIM_PTR(anim)->numFrames)))

void initWeaponAnimGroups(void)
{
    s32 i;

    if (g_HitReactionTable[0].hitpart != (-1))
    {
        i = 0;

        do
        {
            if (g_HitReactionTable[i].deathAnims != NULL)
            {
                g_HitReactionTable[i].deathAnimCount = initResolveAnimTable(g_HitReactionTable[i].deathAnims);
            }

            if (g_HitReactionTable[i].flinchAnims != NULL)
            {
                g_HitReactionTable[i].flinchAnimCount = initResolveAnimTable(g_HitReactionTable[i].flinchAnims);
            }

            i++;
        }
        while (g_HitReactionTable[i].hitpart != (-1));
    }


    initResolveAnimTable(death_stagger);

    initResolveAnimGroups(ptr_rifle_firing_animation_groups);
    initResolveAnimGroups(ptr_pistol_firing_animation_groups);
    initResolveAnimGroups(ptr_doubles_firing_animation_groups);
    initResolveAnimGroups(ptr_crouched_rifle_firing_animation_groups);
    initResolveAnimGroups(ptr_crouched_pistol_firing_animation_groups);
    initResolveAnimGroups(ptr_crouched_doubles_firing_animation_groups);

    initResolveAnimGroupTable(g_ChrFireAnims);
    initResolveAnimGroupTable(g_ChrMoveFireAnims);

    g_ChrSpeedWalk = ANIM_FRAC(ANIM_DATA_walking);
    g_ChrSpeedRun = ANIM_FRAC(ANIM_DATA_running);
    g_ChrSpeedSprint = ANIM_FRAC(ANIM_DATA_sprinting);
    g_ChrSpeedWalkUnarmed = ANIM_FRAC(ANIM_DATA_walking_unarmed);
    g_ChrSpeedRunOneHanded = ANIM_FRAC(ANIM_DATA_running_one_handed_weapon);
    g_ChrSpeedSprintOneHanded = ANIM_FRAC(ANIM_DATA_sprinting_one_handed_weapon);
    g_ChrSpeedWalkFemale = ANIM_FRAC(ANIM_DATA_walking_female);
    g_ChrSpeedRunFemale = ANIM_FRAC_MUL_FIRST(ANIM_DATA_running_female);
    g_ChrSpeedSprintOneHanded2 = ANIM_FRAC(ANIM_DATA_sprinting_one_handed_weapon);
}

#undef ANIM_PTR
#undef ANIM_FRAC
#undef ANIM_FRAC_MUL_FIRST


void casingsInit(void) 
{
    initCasingPool();
}


/**
 * Sets the header field of every g_Casings entry to NULL.
 */
void initCasingPool(void) 
{
    CasingRecord *end = &g_Casings[20];
    CasingRecord *ptr = &g_Casings[0];
    
    while (end > ptr) 
    {
        ptr->header = NULL;
        ptr++;
    }

    g_NumLiveCasings = 0;
}
