#include <ultra64.h>
#include <memp.h>
#include <bondconstants.h>
#include <boss.h>
#include "math_atan2f.h"
#include "bondview_r.h"
#include "bondview.h"
#include "random.h"
#include "game/bondinv.h"
#include "game/chrai.h"
#include "game/front.h"
#include "game/gun.h"
#include "game/language.h"
#include "game/player.h"
#include "game/ramromreplay.h"
#include "game/stan.h"


/**
 * Address 0x8002A780.
*/
struct coord3d default_start_position = { 0 };

u32 weaponLoadProjectileModels(ITEM_IDS modelid)
{
    s32 model;

    model = -1;
    switch(modelid)
    {
    case ITEM_THROWKNIFE:
        model = PROJECTILES_TYPE_KNIFE;
        break;
    case ITEM_GRENADELAUNCH:
        model = PROJECTILES_TYPE_GLAUNCH_ROUND;
        break;
    case ITEM_ROCKETLAUNCH:
        model = PROJECTILES_TYPE_ROCKET_ROUND;
        break;
    case ITEM_GRENADE:
        model = PROJECTILES_TYPE_GRENADE;
        break;
    case ITEM_TIMEDMINE:
        model = PROJECTILES_TYPE_TIMED_MINE;
        break;
    case ITEM_PROXIMITYMINE:
        model = PROJECTILES_TYPE_PROX_MINE;
        break;
    case ITEM_REMOTEMINE:
        model = PROJECTILES_TYPE_REMOTE_MINE;
        break;
    case ITEM_TANKSHELLS:
        model = PROJECTILES_TYPE_ROCKET_ROUND2;
        break;
    case ITEM_BOMBCASE:
        model = PROJECTILES_TYPE_BOMBCASE;
        break;
    case ITEM_PLASTIQUE:
        model = PROJECTILES_TYPE_PLASTIQUE;
        break;
    case ITEM_BUG:
        model = PROJECTILES_TYPE_BUG;
        break;
    case ITEM_MICROCAMERA:
        model = PROJECTILES_TYPE_MICROCAMERA;
    }

    if (-1 < model)
    {
        return modelLoad(model);
    }
    return 0;
}

/**
 * Co-op: spots tried for player 2 around player 1's start pad, as
 * (right, forward) offsets relative to the pad's facing: beside first, so
 * player 2 isn't staring at player 1's back, then behind and in front.
 */
static const f32 coopSpawnOffsets[][2] = {
    { 100.0f,    0.0f }, {-100.0f,   0.0f }, {   0.0f, -100.0f }, {   0.0f, 100.0f },
    { 160.0f,    0.0f }, {-160.0f,   0.0f }, {   0.0f, -160.0f }, {   0.0f, 160.0f },
};

#define COOP_SPAWN_CDTYPES (CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER)
#define COOP_SPAWN_RADIUS  30.0f
#define COOP_SPAWN_MAXSTEP 30.0f

/**
 * Move a co-op partner's start position off player 1's pad to a free spot
 * that can be walked to from the pad. If none fits (e.g. Facility's vent),
 * keep the pad position and let the players overlap until they move apart.
 */
static void coopFindPartnerSpawn(struct coord3d *pos, StandTile **stan, f32 look_angle)
{
    f32 fwd_x = -sinf(look_angle);
    f32 fwd_z = cosf(look_angle);
    f32 floor_y = bondviewYPositionRelated(*stan, pos->f[0], pos->f[2]);
    s32 i;

    for (i = 0; i < (s32)(sizeof(coopSpawnOffsets) / sizeof(coopSpawnOffsets[0])); i++)
    {
        f32 right = coopSpawnOffsets[i][0];
        f32 forward = coopSpawnOffsets[i][1];
        f32 x = pos->f[0] + (fwd_x * forward) + (fwd_z * right);
        f32 z = pos->f[2] + (fwd_z * forward) - (fwd_x * right);
        StandTile *tile = *stan;
        f32 y;

        stanResetHits();

        if (!stanTestLineUnobstructed(&tile, pos->f[0], pos->f[2], x, z, COOP_SPAWN_CDTYPES, 0.0f, 1.0f, 0.0f, 1.0f))
        {
            continue;
        }

        if (stanTestVolume(&tile, x, z, COOP_SPAWN_RADIUS, COOP_SPAWN_CDTYPES, 0.0f, 1.0f) >= 0)
        {
            continue;
        }

        y = bondviewYPositionRelated(tile, x, z);

        if ((y - floor_y > COOP_SPAWN_MAXSTEP) || (floor_y - y > COOP_SPAWN_MAXSTEP))
        {
            continue;
        }

        pos->f[0] = x;
        pos->f[2] = z;
        *stan = tile;
        return;
    }

    g_CoopPlayersOverlap = TRUE;
}

void bondviewLoadSetupIntroSection(void)
{

#define FLOAT_INIT 0


#if defined(VERSION_EU)
#define FIELD_6C_FACTOR 0.20039999485f
#define FIELD_3B8_FACTOR 0.118799984455f
#else
#define FIELD_6C_FACTOR 0.170000016689f
#define FIELD_3B8_FACTOR 0.100000023842f
#endif

    // declarations

    struct coord3d start_pos;
    f32 start_look_angle;
    StandTile *start_stan;
    struct SetupIntroEmpty *intro_record;
    s32 set_starting_weapon;
    s32 rand_camera_index;
    CreditsEntry *credits;
    s32 rand_pad_index;
    f32 stan_height;
    s32 i;
    struct SetupIntroItem *intro_item;
    struct SetupIntroSwirl *intro_swirl;
    struct SetupIntroWatch *intro_watch;
    struct SetupIntroCredits *intro_credits;
    s32 padding[5];

    // done with declarations

    start_pos = default_start_position;

    if (get_cur_playernum() == 0)
    {
        g_CoopPlayersOverlap = FALSE;
    }

    intro_record = (struct SetupIntroEmpty *)g_CurrentSetup.intro;
    g_isBondKIA = 0;
    g_bondviewForceDisarm = 0;
    resolution = 0;
    cameraBufferToggle = 0;
    cameraFrameCounter1 = 0;
    set_starting_weapon = 0;
    cameraFrameCounter2 = 0;
    start_look_angle = FLOAT_INIT;

    if (bossGetStageNum() == LEVELID_CUBA)
    {
        resolution = (s32)mempAllocBytesInBank(0x46EA0, MEMPOOL_STAGE);
        resolution = (resolution + 0x3f) & ~0x3F;
        cameraFrameCounter1 = 1;
    }

    camera_80036438 = 0;
    credits_state = 0;
    credits_pointer = NULL;
    g_ForceBondMoveOffset.f[0] = FLOAT_INIT;
    g_ForceBondMoveOffset.f[1] = FLOAT_INIT;
    g_ForceBondMoveOffset.f[2] = FLOAT_INIT;
    g_SurroundBondWithExplosionsFlag = 0;
    startpadcount = 0;
    g_PlayerIsInTank = 0;
    g_WorldTankProp = 0;
    g_PlayerTankProp = NULL;
    g_PlayerTankYOffset = FLOAT_INIT;
    g_TankSfxState[0] = NULL;
    g_TankSfxState[1] = NULL;
    g_TankTurnSpeed = FLOAT_INIT;
    g_TankOrientationAngle = FLOAT_INIT;
    tank_turret_unused_angle = FLOAT_INIT;
    g_TankTurretVerticalAngle = FLOAT_INIT;
    g_TankTurretVerticalAngleRelated = FLOAT_INIT;
    g_TankTurretOrientationAngleRad = FLOAT_INIT;
    g_TankTurretOrientationAngleDeg = FLOAT_INIT;
    tank_turret_turn_speed = FLOAT_INIT;
    g_BondCanEnterTank = 0;
    g_TankTurretAngle = FLOAT_INIT;
    g_TankTurretTurn = FLOAT_INIT;
    g_ExplodeTankOnDeathFlag = 0;
    is_timer_active = 1;
    g_PlayerInvincible = FALSE;
    g_CameraMode = 0;
    g_CameraAfterCinema = 0;
    camera_fade_active = 0;
    stop_time_flag = 0;
    camera_transition_timer = FLOAT_INIT;
    intro_camera_index = CAMERAMODE_INTRO;
    g_IntroSwirl = NULL;
    ptr_random06cam_entry = NULL;
    g_CurrentSetupIntroCamera = NULL;
    g_SetupIntroCameraCount = 0;
    mission_timer = 0;
    watch_time_0 = 0;
    g_IntroAnimationIndex = 0;
    watch_transition_time = 0.9090909f;
    starting_weapon[GUNLEFT] = ITEM_UNARMED;
    starting_weapon[GUNRIGHT] = ITEM_UNARMED;

    if (intro_record != NULL)
    {
        while (intro_record->type != INTROTYPE_END)
        {
            switch (intro_record->type)
            {
                case INTROTYPE_SPAWN:
                {
                    if (g_CurrentSetup.pads != NULL
                        && (check_ramrom_flags() == ((struct SetupIntroSpawn*)intro_record)->is_demo_playback))
                    {
                        g_Startpad[startpadcount] = &g_CurrentSetup.pads[((struct SetupIntroSpawn*)intro_record)->index];
                        startpadcount++;
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSpawn));
                }
                break;

                case INTROTYPE_ITEM:
                {
                    intro_item = (struct SetupIntroItem*)intro_record;

                    if (check_ramrom_flags() == intro_item->is_demo_playback)
                    {
                        weaponLoadProjectileModels(intro_item->item_right);

                        if (intro_item->item_left >= 0)
                        {
                            weaponLoadProjectileModels(intro_item->item_left);
                            bondinvAddDoublesInvItem(intro_item->item_right, intro_item->item_left);
                        }
                        else
                        {
                            bondinvAddInvItem(intro_item->item_right);
                        }

                        if (set_starting_weapon == 0)
                        {
                            starting_weapon[GUNRIGHT] = intro_item->item_right;

                            if(intro_item->item_left);

                            set_starting_weapon = 1;

                            if (intro_item->item_left >= 0)
                            {
                                starting_weapon[GUNLEFT] = intro_item->item_left;
                            }
                        }
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroItem));
                }
                break;

                case INTROTYPE_AMMO:
                {
                    if (check_ramrom_flags() == ((struct SetupIntroAmmo*)intro_record)->is_demo_playback)
                    {
                        give_cur_player_ammo(((struct SetupIntroAmmo*)intro_record)->ammo_type, ((struct SetupIntroAmmo*)intro_record)->ammo_amount);
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAmmo));
                }
                break;

                case INTROTYPE_SWIRL:
                {
                    intro_swirl = (struct SetupIntroSwirl*)intro_record;

                    if (g_IntroSwirl == NULL)
                    {
                        g_IntroSwirl = intro_swirl;
                    }

                    intro_swirl->unk08.fval = intro_swirl->unk08.ival / M_U16_MAX_VALUE_F;
                    intro_swirl->unk0C.fval = intro_swirl->unk0C.ival / M_U16_MAX_VALUE_F;
                    intro_swirl->unk10.fval = intro_swirl->unk10.ival / M_U16_MAX_VALUE_F;
                    intro_swirl->unk14.fval = intro_swirl->unk14.ival / M_U16_MAX_VALUE_F;
                    intro_swirl->unk18.fval = intro_swirl->unk18.ival / M_U16_MAX_VALUE_F;

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSwirl));
                }
                break;

                case INTROTYPE_ANIM:
                {
                    g_IntroAnimationIndex = ((struct SetupIntroAnim*)intro_record)->intro_anim;

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAnim));
                }
                break;

                case INTROTYPE_CUFF:
                {
                    g_CurrentPlayer->bondtype = ((struct SetupIntroCuff*)intro_record)->bondtype;

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCuff));
                }
                break;

                case INTROTYPE_CAMERA:
                {
                    if (get_cur_playernum() == 0)
                    {
                        ((struct SetupIntroCamera*)intro_record)->prev = g_CurrentSetupIntroCamera;
                        g_CurrentSetupIntroCamera = (struct SetupIntroCamera*)intro_record;
                        g_SetupIntroCameraCount = g_SetupIntroCameraCount + 1;

                        ((struct SetupIntroCamera*)intro_record)->unk04.fval = ((struct SetupIntroCamera*)intro_record)->unk04.ival / 100.0f;
                        ((struct SetupIntroCamera*)intro_record)->unk08.fval = ((struct SetupIntroCamera*)intro_record)->unk08.ival / 100.0f;
                        ((struct SetupIntroCamera*)intro_record)->unk0C.fval = ((struct SetupIntroCamera*)intro_record)->unk0C.ival / 100.0f;
                        ((struct SetupIntroCamera*)intro_record)->unk10.fval = ((struct SetupIntroCamera*)intro_record)->unk10.ival / M_U16_MAX_VALUE_F;
                        ((struct SetupIntroCamera*)intro_record)->unk14.fval = ((struct SetupIntroCamera*)intro_record)->unk14.ival / M_U16_MAX_VALUE_F;

                        ((struct SetupIntroCamera*)intro_record)->lang1c.lang_ptr = langGet(((struct SetupIntroCamera*)intro_record)->lang1c.lang_index[1]);

                        if (((struct SetupIntroCamera*)intro_record)->lang20.lang_index != 0)
                        {
                            ((struct SetupIntroCamera*)intro_record)->lang20.lang_ptr = langGet((u16)((struct SetupIntroCamera*)intro_record)->lang20.lang_index);
                        }
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCamera));
                }
                break;

                case INTROTYPE_WATCH:
                {
                    intro_watch = (struct SetupIntroWatch*)intro_record;

                    watch_time_0 = 0;

                    if (intro_watch->minutes > 0)
                    {
                        watch_time_0 += (intro_watch->minutes % 60) * (60*60);
                    }

                    if (intro_watch->hours > 0)
                    {
                        watch_time_0 += ((intro_watch->hours % 12) * (60*60*60));
                    }

                    if (watch_time_0);

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroWatch));
                }
                break;

                case INTROTYPE_CREDITS:
                {
                    intro_credits = (struct SetupIntroCredits*)intro_record;

                    // hack: bad address math
                    credits = (CreditsEntry*)((s32)g_ptrStageSetupFile + (s32)intro_credits->unk04);
                    credits_pointer = credits;

                    // what is the point of this?
                    while (credits->TextId1 != 0 || credits->TextId2 != 0)
                    {
                        credits++;
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCredits));
                }
                break;

                default:
                {
                    #ifdef DEBUG
                        ossyncprintf("unknown bondstart type %d!\n",intro_record->type);
                    #endif
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroEmpty));
                }
                break;

            }
        }
    }

    if (g_CurrentSetupIntroCamera != NULL)
    {
        ptr_random06cam_entry = g_CurrentSetupIntroCamera;
        rand_camera_index = (s32)(randomGetNext() % (u32) g_SetupIntroCameraCount);
        while (rand_camera_index > 0)
        {
            rand_camera_index--;
            ptr_random06cam_entry = ptr_random06cam_entry->prev;
        }
    }

    bondinvAddInvItem(ITEM_FIST);

    if (set_starting_weapon == 0)
    {
        starting_weapon[GUNRIGHT] = ITEM_FIST;
    }

    g_CurrentPlayer->field_78 = FLOAT_INIT;
    g_CurrentPlayer->field_7C = -0.0001f;
    g_CurrentPlayer->field_80 = FLOAT_INIT;

    if (startpadcount > 0)
    {
        if ((isMpGame()) && (startpadcount > 0))
        {
            rand_pad_index = bondviewGetRandomSpawnPadIndex();
        }
        else
        {
            rand_pad_index = 0;
        }

#ifdef DEBUG
        assert(g_Startpad[rand_pad_index]->stan); //              (".\\ported\\bondview_r.cpp",0x171,"Assertion failed: g_Startpad[sp]->stan");
#endif

        start_pos.f[0] = g_Startpad[rand_pad_index]->pos.f[0];
        start_pos.f[2] = g_Startpad[rand_pad_index]->pos.f[2];


        start_stan = g_Startpad[rand_pad_index]->stan;

        stan_height = bondviewYPositionRelated(start_stan, start_pos.f[0], start_pos.f[2]);
        start_pos.f[1] = g_CurrentPlayer->eyeheight + stan_height;
        g_CurrentPlayer->field_70 = stan_height;
        start_look_angle = M_TAU_F - atan2f(g_Startpad[rand_pad_index]->look.f[0], g_Startpad[rand_pad_index]->look.f[2]);

        if (g_CoopMode && (get_cur_playernum() > 0))
        {
            coopFindPartnerSpawn(&start_pos, &start_stan, start_look_angle);
            stan_height = bondviewYPositionRelated(start_stan, start_pos.f[0], start_pos.f[2]);
            start_pos.f[1] = g_CurrentPlayer->eyeheight + stan_height;
            g_CurrentPlayer->field_70 = stan_height;
        }
    }
    else
    {
        start_stan = sub_GAME_7F0AFB78(&start_pos.f[0], &start_pos.f[1], &start_pos.f[2], 30.0f);
        stan_height = bondviewYPositionRelated(start_stan, start_pos.f[0], start_pos.f[2]);
        start_pos.f[1] = g_CurrentPlayer->eyeheight + stan_height;
        g_CurrentPlayer->field_70 = stan_height;
    }

    g_CurrentPlayer->vv_theta = (start_look_angle * 360.0f) / M_TAU_F;
    g_CurrentPlayer->stanHeight = stan_height;
    g_CurrentPlayer->field_6C = stan_height / FIELD_6C_FACTOR;
    change_player_pos_to_target(&g_CurrentPlayer->field_488, &start_pos, start_stan);

    // Players' collision bounds are otherwise only set when they first move.
    // A co-op partner's body is created right next to player 1 and tests
    // against them, which read uninitialised floats (an FPU exception on
    // hardware and accurate emulators).
    bondviewUpdatePlayerCollisionBounds();
    g_CurrentPlayer->field_488.theta_transform.f[0] = -sinf(start_look_angle);
    g_CurrentPlayer->field_488.theta_transform.f[1] = FLOAT_INIT;
    g_CurrentPlayer->field_488.theta_transform.f[2] = cosf(start_look_angle);
    sub_GAME_7F089718(D_800364D0);
    dword_CODE_bss_80079DA0 = 0;


    for (i=0; i<BSS_80079DA8_LENGTH; i++)
    {
        dword_CODE_bss_80079DA4 = 0;
        dword_CODE_bss_80079DA8[i] = 0;
    }

    bondviewResetIntroCameraMessageDialogs();
    bondviewResetUpperTextDisplay();
    g_CurrentPlayer->prop = chrpropAllocate();
    g_CurrentPlayer->prop->obj = NULL;
    g_CurrentPlayer->prop->type = PROP_TYPE_VIEWER;

    g_CurrentPlayer->prop->pos.f[0] =
        g_CurrentPlayer->bondprevpos.f[0] = start_pos.f[0];

    g_CurrentPlayer->prop->pos.f[1] =
        g_CurrentPlayer->bondprevpos.f[1] = start_pos.f[1];

    g_CurrentPlayer->prop->pos.f[2] =
        g_CurrentPlayer->bondprevpos.f[2] = start_pos.f[2];

    g_CurrentPlayer->prop->stan = start_stan;

    chrpropActivate(g_CurrentPlayer->prop);
    chrpropEnable(g_CurrentPlayer->prop);
    g_CurrentPlayer->field_3B8.f[0] = (g_CurrentPlayer->field_488.pos.f[0] / FIELD_3B8_FACTOR);
    g_CurrentPlayer->field_3B8.f[1] = (g_CurrentPlayer->field_488.pos.f[1] / FIELD_3B8_FACTOR);
    g_CurrentPlayer->field_3B8.f[2] = (g_CurrentPlayer->field_488.pos.f[2] / FIELD_3B8_FACTOR);

    if (getPlayerCount() == 1)
    {
        bondviewSetCameraMode(CAMERAMODE_INTRO);
    }
    else
    {
        bondviewSetCameraMode(CAMERAMODE_MP);
    }

    g_bondviewBondDeathAnimationsCount = 0;
    while (g_bondviewBondDeathAnimations[g_bondviewBondDeathAnimationsCount] != 0)
    {
        g_bondviewBondDeathAnimationsCount++;
    }

    g_CurrentPlayer->startnewbonddie = TRUE;
    g_CurrentPlayer->redbloodfinished = FALSE;
    g_CurrentPlayer->deathanimfinished = FALSE;
    camera_mode = CAMERAMODE_NONE;
}
