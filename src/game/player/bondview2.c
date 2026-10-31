/**
 * The rest of the player. In file order: loading Bond's body, the head bob, height
 * and falling (playerUpdateVertical), the per-frame update (playerUpdate, playerMove,
 * playerMoveFrozen), cuffs, small getters and setters, and the player prop tick
 * (playerTick). Input is in playerinput.c, health and damage in playerdamage.c,
 * fades in playerfade.c, the HUD in hud/, the pause watch in watch.c, respawning in
 * respawn.c, counter-op in counterop.c, the tank in tank.c, moving with collision
 * in playermove.c, camera modes in cammode.c and the view in camview.c. See
 * docs/subsystems/player.md.
 */
#include <ultra64.h>
#include <math.h>
#include <bondtypes.h>
#include "game/audio/sfxvolume.h"
#include "game/menu/cheat.h"
#include <boss.h>
#include <fr.h>
#include <joy.h>
#include <music.h>
#include <snd.h>
#include <str.h>
#include "game/menu/watchmenu.h"
#include "game/world/bg.h"
#include "game/world/bgfog.h"
#include "game/world/roommtx.h"
#include "game/hud/blood.h"
#include "game/player/bondhead.h"
#include "game/player/bondinv.h"
#include "game/hud/hud.h"
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/chr/chraction.h"
#include "game/chr/chrai.h"
#include "game/prop/explosion.h"
#include "game/save/file.h"
#include "game/level/frametiming.h"
#include "game/menu/front.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/player/gun.h"
#include "game/model/anim.h"
#include "game/text/language.h"
#include "game/prop/setup.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/audio/levelmusic.h"
#include "game/menu/mpmenu.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/mission/objective.h"
#include "os_extension.h"
#include "game/player/player.h"
#include "game/prop/propobj.h"
#include "game/math/quaternion.h"
#include "random.h"
#include "game/nav/geometry.h"
#include "game/nav/stan.h"
#include "game/text/text.h"
#include "game/math/interp.h"


#include "game/player/playertune.h"
#include "game/asset/asset.h"
#include "game/gfx/dyn.h"
#include "game/hud/radar.h"
#include "game/prop/prop.h"
#include "game/replay/ramromreplay.h"
#include "game/prop/alarm.h"
#include "game/math/physics.h"

/**
 * When set, will increment each tick until reaching a threshold value (4).
 * Then current items will be unequipped from left and run hands.
*/
s32 g_bondviewForceDisarm = 0;

s32 resolution = 0;

/**
 * Flag to toggle invisibility cheat.
 * 1 = visible to guards
 * 0 = not visible to guards
 */
s32 g_VisibleToGuardsFlag = TRUE;

f32 g_PlayerWorldScale = 1.0;

static f32 g_PlayerWorldScaleInv = 1.0;

s32 g_bondviewBondDeathAnimations[] = {
    PTR_ANIM_death_forward_face_down,
    PTR_ANIM_death_forward_spin_face_up,
    PTR_ANIM_death_backward_fall_face_up1,
    PTR_ANIM_death_backward_spin_face_down_right,
    PTR_ANIM_death_backward_spin_face_up_right,
    PTR_ANIM_death_backward_spin_face_down_left,
    PTR_ANIM_death_backward_spin_face_up_left,
    PTR_ANIM_death_forward_face_down_hard,
    PTR_ANIM_death_forward_face_down_soft,
    PTR_ANIM_death_fetal_position_right,
    PTR_ANIM_death_fetal_position_left,
    PTR_ANIM_death_backward_fall_face_up2,
    0
};

s32 g_bondviewBondDeathAnimationsCount = 0;

// Defaults for playerMoveFrozen's position and move offset
static coord3d g_DefaultMoveBondOffset = { 0 };

// struct player `pos`
static coord3d g_DefaultFrozenPlayerPos = { 0 };

// struct player `pos2`
static coord3d g_DefaultFrozenPlayerPos2 = { {0, 0, 1.0f} };

// struct player `offset`
static coord3d g_DefaultFrozenPlayerOffset = { {0, 1.0f, 0} };

// struct player `offset`
static coord3d g_DefaultFrozenMoveOffset = { 0 };

static s32 g_PlayerTickCount = 0;

struct firing_anim_struct firing_animation_groups[][6] = {
    {{pistol_firing_animation_group1, 0, 0.1, 79.0, 87.0},
     {&g_ChrMoveFireAnims[2], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[3], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[9], 0, 0.5, 0.0, -1.0},
     {crouched_pistol_firing_animation_group1, 0, 0.1, 56.0, 68.0}},
    {{rifle_firing_animation_group1, 0, 0.050000001, 35.0, 40.0},
     {&g_ChrMoveFireAnims[0], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[1], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[9], 0, 0.5, 0.0, -1.0},
     {&crouched_rifle_firing_animation_group1[1], 0, 0.1, 45.0, 55.0}},
    {{NULL, 0x8194, 0.25, 0.0, -1.0},
     {NULL, 0x8204, 0.5, 0.0, -1.0},
     {NULL, 0x777C, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[9], 0, 0.5, 0.0, -1.0},
     {NULL, 0x6C18, 0.050000001, 28.0, 29.0}},
    {{doubles_firing_animation_group1, 0, 0.1, 32.0, 42.0},
     {&g_ChrMoveFireAnims[4], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[5], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {crouched_doubles_firing_animation_group1, 0, 0.1, 37.0, 47.0}}
 };


coord3d g_ForceBondMoveOffset;
s32 g_CoopForceMovePlayer = 0;


coord3d g_TankModelPositionOffset;

static s32 g_TankEngineSfxVolume;

/**
 * State 0: begin.
 * State 1: Finished sitting down/turning, queue audio.
 * State 2: complete
*/
s32 g_EnterTankAudioState;

f32 g_TankEnteringSitHeight;

f32 g_TankEnteringSitHeightRemain;

f32 g_TankEnterBondHorizAngleDeg;

f32 g_TankEnterBondVertAngleDeg;


coord3d g_EnterTankCoord;


ITEM_IDS starting_weapon[2];


s32 mission_timer;

#if defined(VERSION_EU)
f32 watch_time_0;
#else
s32 watch_time_0;
#endif


#define ALIGN64_V3(val) (((val) | 0x3f) ^ 0x3f)

void playerLoadBody(void)
{
    f32                         yaw;
    ChrRecord                  *self;
    struct texpool              pool;
    ModelFileHeader            *bodyheader;
    ModelFileHeader            *headheader;
    ModelFileHeader            *pitemheader;
    u8                         *weaponbuf0;
    u8                         *weaponbuf1;
    s32                         cursor;
    s32                         size0;
    s32                         size1;
    s32                         helddst;
    struct player             **pp;
    s32                         prop;
    ITEM_IDS                    item;
    s32                         body;
    s32                         head;
    Model                      *model;

    yaw = bondviewGetPlayerYawRadians();
    if (g_CurrentPlayer->prop->chr == NULL)
    {
        weaponbuf0 = gunGetHandModelBuffer(GUNRIGHT);
        weaponbuf1 = gunGetHandModelBuffer(GUNLEFT);
        cursor     = 0;
        size0      = gunGetHandModelBufferSize(GUNRIGHT);
        size1      = gunGetHandModelBufferSize(GUNLEFT);
        item       = gunGetShownWeapon(GUNRIGHT);
        body       = BODY_Formal_Wear;
        head       = HEAD_Male_Brosnan_Default;
        model      = NULL;
        bodyheader = NULL;

        playerRemoveFromRooms(g_CurrentPlayer);

        if (counterOpIsGuardPlayer(playerGetCurrentNum()) && counterOpGetGuardModels(&head, &body))
        {
            // Counter-op: the guard player wears the guard they took over.
        }
        // Counter-op: until then (the intro) the guard player is a Bond, as
        // player 2's co-op character may be one with no head or body entry.
        else if (!isMpGame() && ((playerGetCurrentNum() == 0) || frontCoopPartnerIsBond() || counterOpIsGuardPlayer(playerGetCurrentNum())))
        {
            helddst = fileGetBondForCurrentFolder();
            switch (g_CurrentPlayer->bondtype)
            {
                case CUFF_BLUE:
                    break;

                case CUFF_BOILER:
                    body = BODY_Special_Operations_Uniform;
                    break;

                case CUFF_JUNGLE:
                    body = BODY_Jungle_Fatigues;
                    break;

                case CUFF_SNOW:
                    body = BODY_Parka;
                    break;

                case CUFF_BROSNAN:
                    body = BODY_Brosnan_Tuxedo;
                    break;

                case CUFF_CONNERY:
                    body = BODY_Brosnan_Tuxedo;
                    break;

                case CUFF_DALTON:
                    body = BODY_Brosnan_Tuxedo;
                    break;

                case CUFF_MOORE:
                    body = BODY_Brosnan_Tuxedo;
                    break;

                case CUFF_FOLDER:
                    switch (helddst)
                    {
                        case BOND_BROSNAN:
                            body = BODY_Brosnan_Tuxedo;
                            break;

                        case BOND_CONNERY:
                            body = BODY_Brosnan_Tuxedo;
                            break;

                        case BOND_DALTON:
                            body = BODY_Brosnan_Tuxedo;
                            break;

                        case BOND_MOORE:
                            body = BODY_Brosnan_Tuxedo;
                            break;
                    }

                    break;
            }

            switch (helddst)
            {
                case BOND_BROSNAN:
                    switch (g_CurrentPlayer->bondtype)
                    {
                        case CUFF_BLUE:
                            break;

                        case CUFF_BOILER:
                            head = HEAD_Male_Brosnan_Boiler;
                            break;

                        case CUFF_JUNGLE:
                            head = HEAD_Male_Brosnan_Jungle;
                            break;

                        case CUFF_BROSNAN:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;

                        case CUFF_CONNERY:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;

                        case CUFF_DALTON:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;

                        case CUFF_MOORE:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;

                        case CUFF_FOLDER:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;
                    }

                    break;

                case BOND_CONNERY:
                    head = HEAD_Male_Brosnan_Tuxedo;
                    break;

                case BOND_DALTON:
                    head = HEAD_Male_Brosnan_Tuxedo;
                    break;

                case BOND_MOORE:
                    head = HEAD_Male_Brosnan_Tuxedo;
                    break;
            }
        }
        else if (g_CoopMode)
        {
            head = frontCoopGetPartnerHead();
            body = frontCoopGetPartnerBody();
        }
        else
        {
            head = frontGetMpCharHead(playerGetCurrentNum());
            body = frontGetMpCharBody(playerGetCurrentNum());
        }

        if (g_CameraMode == CAMERAMODE_SWIRL)
        {
            item = starting_weapon[GUNRIGHT];
        }

        if (getPlayerCount() == 1)
        {
            gunClearHandModel(GUNLEFT);
            gunClearHandModel(GUNRIGHT);
            texInitPool(&pool, weaponbuf1, size1);
            bodyheader  = gunGetHandModelHeader(GUNRIGHT);
            *bodyheader = *c_item_entries[body].header;
            modelFileLoad(bodyheader, c_item_entries[body].filename, weaponbuf0, size0, &pool);
            cursor = assetGetSizeByName(c_item_entries[body].filename);

            do
            {
                cursor      = ALIGN64_V3(cursor + 0x3f);
                headheader  = (ModelFileHeader *)(weaponbuf0 + cursor);
                cursor      = ALIGN64_V3(cursor + sizeof(ModelFileHeader) + 0x3f);
                *headheader = *c_item_entries[head].header;


                modelFileLoad(headheader, c_item_entries[head].filename, weaponbuf0 + cursor, size0 - cursor, &pool);
                cursor = ALIGN64_V3(assetGetSizeByName(c_item_entries[head].filename) + cursor + 0x3f);
                model  = (Model *)(weaponbuf0 + cursor);
                cursor = ALIGN64_V3(cursor + 0xfb);
                modelCalculateRwDataLen(bodyheader);
                modelCalculateRwDataLen(headheader);

                {
                    u32 *animdata;
                    s32  nrec;
                    animdata = (u32 *)(weaponbuf0 + cursor);
                    nrec     = (bodyheader->numRecords + headheader->numRecords) + 0xa;
                    cursor   = ALIGN64_V3(cursor + (nrec << 2) + 0x3f);
                    animInit(model, bodyheader, animdata);
                    model->rwdatalen = nrec;
                }

            } while (FALSE);
        }
        else
        {
            bodyheader = c_item_entries[body].header;

            if (bodyheader->RootNode == NULL)
            {
                modelFileLoadToPool(bodyheader, c_item_entries[body].filename);
            }
            if (c_item_entries[body].hasHead)
            {
                head       = -1;
                headheader = NULL;
            }
            else
            {
                headheader = c_item_entries[head].header;

                if (headheader->RootNode == NULL)
                {
                    modelFileLoadToPool(headheader, c_item_entries[head].filename);
                }
            }
        }

        g_CurrentPlayer->bodyModel = bodyCreateModel(body, head, bodyheader, headheader, 0, model);
        modelSetScale(g_CurrentPlayer->bodyModel, g_CurrentPlayer->bodyModel->scale * 0.97f);
        chrInit(g_CurrentPlayer->prop, g_CurrentPlayer->bodyModel, &g_CurrentPlayer->prop->pos, yaw, g_CurrentPlayer->prop->stan, NULL);
        pp = &g_CurrentPlayer;
        (*pp)->prop->type = PROP_TYPE_VIEWER;
        self              = (*pp)->prop->chr;
        self->chrflags |= CHRFLAG_INIT;
        modelSetRootPosition((*pp)->bodyModel, &(*pp)->prop->pos);
        modelSetRootRotY(g_CurrentPlayer->bodyModel, yaw);
        self->headnum = head;
        self->bodynum = body;
        prop = weaponGetChrModel(item);

        if (prop >= 0)
        {
            if (getPlayerCount() == 1)
            {
                helddst      = cursor;
                helddst      = ((s32)weaponbuf0) + helddst;
                cursor       = ALIGN64_V3(cursor + 0xc7);
                pitemheader  = gunGetHandModelHeader(GUNLEFT);
                *pitemheader = *PitemZ_entries[prop].header;
                modelFileLoad(pitemheader, PitemZ_entries[prop].filename, weaponbuf0 + cursor, size0 - cursor, &pool);
                assetGetSizeByName(PitemZ_entries[prop].filename);
                modelCalculateRwDataLen(pitemheader);
            }
            else
            {
                helddst     = 0;
                pitemheader = NULL;
            }

            chrGiveWeaponEx(self, prop, item, 0, (WeaponObjRecord *)helddst, pitemheader);
        }

        chrStandImmediate(self, 0.0f);
    }
    else
    {
        self = g_CurrentPlayer->prop->chr;

        if (self->model->anim != NULL)
        {
            return;
        }

        self->chrflags |= CHRFLAG_INIT;
        chrStandImmediate(self, 0.0f);
        modelSetRootPosition(g_CurrentPlayer->bodyModel, &g_CurrentPlayer->prop->pos);
        modelSetRootRotY(g_CurrentPlayer->bodyModel, yaw);
    }
}


void playerRemoveBody(void)
{
    if ((g_CurrentPlayer->prop->chr) && (getPlayerCount() == 1))
    {
        chrFreeResources(g_CurrentPlayer->prop);
        g_CurrentPlayer->prop->chr = NULL;
        g_CurrentPlayer->bodyModel = 0;
        g_bondviewForceDisarm = 1;
        playerUpdateRooms(g_CurrentPlayer);
    }
}


/**
 * Will apply a move animation update. The pass through call to bheadUpdate is
 * what allows Bond to move. This will also trigger the death animation once
 * Bond dies. This chooses a random death animation from g_bondviewBondDeathAnimations.
*/
void playerTickMoveAnim(f32 speed, f32 speedforwards, f32 speedsideways)
{
    f32 percent_speed;
    Mtxf view;
    Mtxf rot;

    percent_speed = 0.0f;

    if (g_CurrentPlayer->bonddead == 0)
    {
        bheadAdjustAnimation(speed);

        if (speed != 0.0f)
        {
            percent_speed = speedforwards / speed;
        }
        else if (speedforwards == 0.0f)
        {
            //
        }
    }
    else
    {
        if (g_CurrentPlayer->startnewbonddie)
        {
            // HACK: g_AnimTable dereference addition is backwards.
            // this should be:
            // g_AnimTable->data[g_bondviewBondDeathAnimations[((u32) randomGetNext() % (u32) g_bondviewBondDeathAnimationsCount)]]
            bheadStartDeathAnimation((struct ModelAnimation *) ((s32)g_bondviewBondDeathAnimations[((u32) randomGetNext() % (u32) g_bondviewBondDeathAnimationsCount)] + (s32)&g_AnimTable->data[0]), randomGetNext() & 1, 0.0f, 1.0f);
            g_CurrentPlayer->startnewbonddie = FALSE;
        }

        bheadSetSpeed(0.5f);
        speedsideways = 0.0f;
    }

    bheadUpdate(percent_speed, speedsideways);

    mtx4LoadXRotation((360.0f - g_CurrentPlayer->vv_verta360) * DegToRad1Fact(1), &view);
    mtx4LoadFacing(&rot, 0.0f, 0.0f, 0.0f, -g_CurrentPlayer->headlook.f[0], -g_CurrentPlayer->headlook.f[1], -g_CurrentPlayer->headlook.f[2], g_CurrentPlayer->headup.f[0], g_CurrentPlayer->headup.f[1], g_CurrentPlayer->headup.f[2]);
    mtx4MultInPlace(&rot, &view);
    mtx4LoadYRotation((360.0f - g_CurrentPlayer->vv_theta) * DegToRad1Fact(1), &rot);
    mtx4MultInPlace(&rot, &view);

    g_CurrentPlayer->bond.applied_view.f[0] = view.m[2][0];
    g_CurrentPlayer->bond.applied_view.f[1] = view.m[2][1];
    g_CurrentPlayer->bond.applied_view.f[2] = view.m[2][2];

    g_CurrentPlayer->bond.applied_view2.f[0] = view.m[1][0];
    g_CurrentPlayer->bond.applied_view2.f[1] = view.m[1][1];
    g_CurrentPlayer->bond.applied_view2.f[2] = view.m[1][2];

}


f32 playerGetFloorY(StandTile *tile, f32 x, f32 z)
{
    f32 ret;

    if (g_PlayerTankProp != NULL)
    {
        ObjectRecord * obj = ((PropRecord *)g_PlayerTankProp)->obj;
        PropRecord *p = obj->prop;

        ret = stanGetFloorY(p->stan, p->pos.x, p->pos.z);

        ret += g_PlayerTankYOffset;
    }
    else
    {
        ret = stanGetFloorY(tile, x, z);
    }

    return ret;
}


/**
 * US Address 0x7F080DF8.
 * EU Address 0x7F080E9C.
 */
void playerUpdateVertical(s32 use_stanHeight, f32 stanHeight_offset)
{
    s32 i;
    f32 floorY;
    StandTile *stan;
    f32 collision_radius;
    f32 height;
    f32 always_30;
    f32 tankfloorY;
    f32 newmanground;
    f32 newfallspeed;
    f32 ftemp2;
    f32 gravity;


    if (g_PlayerIsInTank == 1)
    {
        g_CurrentPlayer->stanHeight = playerGetFloorY(
            g_CurrentPlayer->bond.current_tile_ptr,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2]);

        g_CurrentPlayer->sumGround = g_CurrentPlayer->manGround / (1.0f - TANK_Y_SMOOTHING);

        for (i=0; i<g_ClockTimer; i++)
        {
            g_CurrentPlayer->sumGround = (g_CurrentPlayer->sumGround * TANK_Y_SMOOTHING) + g_CurrentPlayer->stanHeight;
        }

        g_CurrentPlayer->manGround = g_CurrentPlayer->sumGround * (1.0f - TANK_Y_SMOOTHING);
    }
    else
    {
        if (use_stanHeight != 0)
        {
            g_CurrentPlayer->stanHeight = g_CurrentPlayer->stanHeight + stanHeight_offset;

            tankfloorY = playerGetFloorY(
                g_CurrentPlayer->bond.current_tile_ptr,
                g_CurrentPlayer->bond.collision_position.f[0],
                g_CurrentPlayer->bond.collision_position.f[2]);

            if (g_CurrentPlayer->stanHeight < tankfloorY)
            {
                g_CurrentPlayer->stanHeight = tankfloorY;
            }
        }
        else
        {
            stan = g_CurrentPlayer->bond.current_tile_ptr;

            playerGetCollisionSize(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

            floorY = playerGetFloorY(
                g_CurrentPlayer->bond.current_tile_ptr,
                g_CurrentPlayer->bond.collision_position.f[0],
                g_CurrentPlayer->bond.collision_position.f[2]);

            // Another error checking block, it seems this condition is almost never triggered in the game.
            if (stanTestCircleMaxHeight(
                &stan,
                g_CurrentPlayer->bond.collision_position.f[0],
                g_CurrentPlayer->bond.collision_position.f[2],
                collision_radius,
                playerGetEyeHeight(g_CurrentPlayer) + floorY) >= 0)
            {
                if (floorY < g_CurrentPlayer->stanHeight)
                {
                    floorY = g_CurrentPlayer->stanHeight;
                }
            }

            g_CurrentPlayer->stanHeight = floorY;
        }

        if ((g_CurrentPlayer->deltaPos.y >= 0.0f) || (g_CurrentPlayer->manGround < g_CurrentPlayer->stanHeight))
        {
            g_CurrentPlayer->sumGround = g_CurrentPlayer->manGround / (1.0f - TANK_Y_SMOOTHING);

            for (i=0; i<g_ClockTimer; i++)
            {
                g_CurrentPlayer->sumGround = (g_CurrentPlayer->sumGround * TANK_Y_SMOOTHING) + g_CurrentPlayer->stanHeight;
            }

            if (g_CurrentPlayer->manGround < g_CurrentPlayer->stanHeight)
            {
                g_CurrentPlayer->manGround = g_CurrentPlayer->sumGround * (1.0f - TANK_Y_SMOOTHING);
            }
        }

        if (g_CurrentPlayer->stanHeight < g_CurrentPlayer->manGround)
        {
            newfallspeed = g_CurrentPlayer->deltaPos.y;
            newmanground = g_CurrentPlayer->manGround;

            if ((g_CheatTurboMode) && (g_ForceBondMoveOffset.f[0] == 0.0f) && (g_ForceBondMoveOffset.f[2] == 0.0f))
            {
                gravity = 1.388889f;
            }
            else
            {
                gravity = 0.27777779f;
            }

            ftemp2 = newfallspeed - (g_GlobalTimerDelta * gravity);
            newmanground += (g_GlobalTimerDelta * (newfallspeed + ftemp2) * 0.5f);
            newfallspeed = ftemp2;

            if (newmanground < g_CurrentPlayer->stanHeight)
            {
                newmanground = g_CurrentPlayer->stanHeight;
                newfallspeed = -sqrtf((g_CurrentPlayer->deltaPos.y * g_CurrentPlayer->deltaPos.y) + (((2.0f * (g_CurrentPlayer->manGround - g_CurrentPlayer->stanHeight) * 0.27777779f) / 60.0f) * 60.0f));
            }

            g_CurrentPlayer->manGround = newmanground;
            g_CurrentPlayer->deltaPos.y = newfallspeed;
        }

        if ((g_CurrentPlayer->deltaPos.y < 0.0f) && (g_CurrentPlayer->manGround <= g_CurrentPlayer->stanHeight))
        {
            if (g_CurrentPlayer->deltaPos.y < -13.333333f)
            {
                g_CurrentPlayer->crouchTime240 = LANDING_DIP_FRAMES;
                g_CurrentPlayer->vertical_bounce_adjust = -90.0f;
            }
            else if (g_CurrentPlayer->deltaPos.y < -5.0f)
            {
                g_CurrentPlayer->crouchTime240 = LANDING_DIP_FRAMES;
                g_CurrentPlayer->vertical_bounce_adjust = ((-5.0f - g_CurrentPlayer->deltaPos.y) * -90.0f) / 8.333333f;
            }

            g_CurrentPlayer->deltaPos.y = 0.0f;
        }
    }

    for (i=0; i<g_ClockTimer; i++)
    {
        if (g_CurrentPlayer->crouchTime240 > 0)
        {
            g_CurrentPlayer->sumCrouch = (g_CurrentPlayer->sumCrouch * CROUCH_DAMP) + g_CurrentPlayer->vertical_bounce_adjust;
            g_CurrentPlayer->crouchTime240 += -1;
        }
        else
        {
            if (g_CurrentPlayer->vertical_bounce_adjust < 0.0f)
            {
                g_CurrentPlayer->vertical_bounce_adjust -= CROUCH_FALL_STEP;

                if (0.0f <= g_CurrentPlayer->vertical_bounce_adjust)
                {
                    g_CurrentPlayer->vertical_bounce_adjust = 0.0f;
                }
            }

            g_CurrentPlayer->sumCrouch = (g_CurrentPlayer->sumCrouch * CROUCH_DAMP) + g_CurrentPlayer->vertical_bounce_adjust;
        }
    }

    g_CurrentPlayer->crouchHeight = g_CurrentPlayer->sumCrouch * CROUCH_DAMP_INV;
}


/**
 * Address 0x7F081478 (NTSC).
 * Address 0x7F08151C (PAL).
*/
void playerUpdatePosFromCollision(void)
{
    f32 height;
    s32 i;
    StandTile *tile;

    g_CurrentPlayer->eyeheight = (g_CurrentPlayer->headpos.f[1] * g_playerPerm->player_perspective_height) + 7.0f;

    height = g_CurrentPlayer->eyeheight +
        ((g_CurrentPlayer->crouchHeight + g_CurrentPlayer->ducking_height_offset) * g_playerPerm->player_perspective_height);

    if (height < 30.0f)
    {
        height = 30.0f;
    }

    g_CurrentPlayer->bond.collision_position.f[1] = g_CurrentPlayer->manGround + height;

    if (((g_CameraMode != CAMERAMODE_DEATH_CAM_SP) && (g_CameraMode != CAMERAMODE_DEATH_CAM_MP) && (g_CameraMode != CAMERAMODE_POSEND))
        || (g_CurrentPlayer->bodyModel == 0))
    {
        g_CurrentPlayer->bond.pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
        g_CurrentPlayer->bond.pos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
        g_CurrentPlayer->bond.pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2];
    }

    if (g_CurrentPlayer->bonddead != FALSE)
    {
        if (g_CurrentPlayer->lookDownDrop > 0.0f)
        {
            g_CurrentPlayer->lookDownDrop -= 0.25f;

            if (g_CurrentPlayer->lookDownDrop < 0.0f)
            {
                g_CurrentPlayer->lookDownDrop = 0.0f;
            }
        }
    }

    if (g_CurrentPlayer->vv_verta < 0.0f)
    {
        g_CurrentPlayer->bond.pos.f[1] += -(1.0f - g_CurrentPlayer->vv_cosverta) * g_CurrentPlayer->lookDownDrop;
    }

    tile = g_CurrentPlayer->bond.current_tile_ptr;

    stanWalkLine(
        &tile,
        g_CurrentPlayer->bond.collision_position.f[0],
        g_CurrentPlayer->bond.collision_position.f[2],
        g_CurrentPlayer->bond.pos.f[0],
        g_CurrentPlayer->bond.pos.f[2]);


    g_CurrentPlayer->bond.current_tile_ptr_for_portals = tile;

    g_CurrentPlayer->bond.pos3.f[0] = g_CurrentPlayer->bond.pos.f[0];
    g_CurrentPlayer->bond.pos3.f[2] = g_CurrentPlayer->bond.pos.f[2];

    g_CurrentPlayer->bond.pos3.f[1] = playerGetFloorY(
        g_CurrentPlayer->bond.current_tile_ptr_for_portals,
        g_CurrentPlayer->bond.pos.f[0],
        g_CurrentPlayer->bond.pos.f[2]);

    g_CurrentPlayer->prop->stan = g_CurrentPlayer->bond.current_tile_ptr;

    g_CurrentPlayer->prop->pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
    g_CurrentPlayer->prop->pos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
    g_CurrentPlayer->prop->pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2];

#if defined(VERSION_EU)
#define EYE_DAMP 0.881200015545f
#else
#define EYE_DAMP 0.9f
#endif
    for (i=0; i<g_ClockTimer; i++)
    {
        g_CurrentPlayer->dampEyeSum.f[0] = (EYE_DAMP * g_CurrentPlayer->dampEyeSum.f[0]) + g_CurrentPlayer->bond.pos.f[0];
        g_CurrentPlayer->dampEyeSum.f[1] = (EYE_DAMP * g_CurrentPlayer->dampEyeSum.f[1]) + g_CurrentPlayer->bond.pos.f[1];
        g_CurrentPlayer->dampEyeSum.f[2] = (EYE_DAMP * g_CurrentPlayer->dampEyeSum.f[2]) + g_CurrentPlayer->bond.pos.f[2];
    }

#if defined(VERSION_EU)
#define EYE_DAMP_INV 0.118799984455f
#else
#define EYE_DAMP_INV 0.100000024f
#endif
    g_CurrentPlayer->dampEye.x = g_CurrentPlayer->dampEyeSum.f[0] * EYE_DAMP_INV;
    g_CurrentPlayer->dampEye.y = g_CurrentPlayer->dampEyeSum.f[1] * EYE_DAMP_INV;
    g_CurrentPlayer->dampEye.z = g_CurrentPlayer->dampEyeSum.f[2] * EYE_DAMP_INV;
}


/**
 * Fixes vv_verta within -90 and +90.
 * Updates vv_costheta, vv_sintheta, vv_verta360, vv_cosverta, vv_sinverta, bond.theta_transform.
 *
 * Perfect Dark function bmoveUpdateVerta.
*/
void playerUpdateVertaTheta(void)
{
    while (g_CurrentPlayer->vv_verta < -180.0f)
    {
        g_CurrentPlayer->vv_verta += 360.0f;
    }

    while (g_CurrentPlayer->vv_verta >= 180.0f)
    {
        g_CurrentPlayer->vv_verta -= 360.0f;
    }

    if (g_CurrentPlayer->vv_verta > 90.0f)
    {
        g_CurrentPlayer->vv_verta = 90.0f;
    }
    else if (g_CurrentPlayer->vv_verta < -90.0f)
    {
        g_CurrentPlayer->vv_verta = -90.0f;
    }

    g_CurrentPlayer->vv_costheta = cosf(g_CurrentPlayer->vv_theta * DegToRad1Fact(1));
    g_CurrentPlayer->vv_sintheta = sinf(g_CurrentPlayer->vv_theta * DegToRad1Fact(1));

    g_CurrentPlayer->vv_verta360 = g_CurrentPlayer->vv_verta;
    if (g_CurrentPlayer->vv_verta360 < 0.0f)
    {
        g_CurrentPlayer->vv_verta360 += 360.0f;
    }

    g_CurrentPlayer->vv_cosverta = cosf(g_CurrentPlayer->vv_verta360 * DegToRad1Fact(1));

    g_CurrentPlayer->bond.theta_transform.f[0] = -g_CurrentPlayer->vv_sintheta;
    g_CurrentPlayer->bond.theta_transform.f[1] = 0;
    g_CurrentPlayer->bond.theta_transform.f[2] = g_CurrentPlayer->vv_costheta;
}


/**
 * NTSC Address 0x7F084648.
 * NTSC-J address 0x7F084CF8.
 *
 * Met by Saint Jon The Archangel in the writhing pits of hell, the beast was
 * pinned, prone on the floor. He had taken many forms throughout the ages.
 * From the creator of man to the far reaches of the universe, his perpetual
 * reincarnations reigned throughout space and time... until now. His form was
 * weak, the opposition strong. The time for atonement had finally come.
 *
 * Battered, torn, exposed, the beast slacked his gaping maw.
 *
 * "I'm sorry, Jon" he whispered.
 *
 * The archangel raised his flaming blade.
 *
 * "You are forgiven, Garfield"
 *
 * And the beast exhaled his last.
 *
 * 10 months of off and on work to match.
 * Thanks Trevor.
 * - Bethany Burns
 */
void playerMove(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons)
{
    coord3d move_offset;
    f32 ftemp;
    f32 theta;
    f32 animSideways;
    s32 i;
    f32 maxspeed;
    s32 use_stanHeight;
    f32 climbOffset;

    move_offset = g_DefaultMoveBondOffset;

    use_stanHeight = 0;
    maxspeed = 0.0f;
    climbOffset = 0.0f;


    if (g_bondviewForceDisarm > 0)
    {
        g_bondviewForceDisarm++;
        if (g_bondviewForceDisarm >= 4)
        {
            g_bondviewForceDisarm = 0;
            g_CurrentPlayer->handModelLocked[GUNLEFT] = 0;
            g_CurrentPlayer->handModelLocked[GUNRIGHT] = 0;
            gunUnequip(GUNLEFT, gunGetWeapon(GUNLEFT));
            gunUnequip(GUNRIGHT, gunGetWeapon(GUNRIGHT));
        }
    }

    playerSetCameraModeRaw(0);
    playerTickDamageAndHealth();
    playerTickExplode();
    playerProcessInput(stick_x, stick_y, buttons, oldbuttons);

    if (lvGetControlsLocked())
    {
        pauseStopSounds();
    }

    if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
    {
        watchTick();
    }

    /*
        Apply bondshotspeed vector to speedforwards scalar and speedsideways scalar.
        Crouching applies a 50% base speed reduction before applying boost.
        Bond can't be boosted while in the tank.
    */
    if (g_PlayerIsInTank == 0)
    {
        // This `if` block is Perfect Dark bwalkApplyCrouchSpeed.
        if (currentPlayerGetCrouchPos() == CROUCH_SQUAT)
        {
            g_CurrentPlayer->speedforwards *= 0.5f;
            g_CurrentPlayer->speedsideways *= 0.5f;
        }

        if ((g_CurrentPlayer->bondshotspeed.f[0] != 0.0f) || (g_CurrentPlayer->bondshotspeed.f[2] != 0.0f))
        {
            // boost forwards
            f32 shotboost_forward;
            // boost sideways
            f32 shotboost_sideways;
            f32 shotboost_norm;

            // Assigning these two variables is done in Perfect Dark bmove0f0cba88.
            shotboost_forward =
                (-g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->vv_sintheta)
                + (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->vv_costheta);
            shotboost_sideways =
                (-g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->vv_costheta)
                - (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->vv_sintheta);

            shotboost_norm = sqrtf(
                (g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->bondshotspeed.f[0]) +
                (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->bondshotspeed.f[2]));

            g_CurrentPlayer->speedforwards += shotboost_forward;
            g_CurrentPlayer->speedsideways += shotboost_sideways;

            // 3: x,y,z components of bondshotspeed
            for (i=0; i<3; i++)
            {
                if (g_CurrentPlayer->bondshotspeed.f[i] != 0.0f)
                {
                    if (g_CurrentPlayer->bondshotspeed.f[i] > 0.0f)
                    {
                        g_CurrentPlayer->bondshotspeed.f[i] -=
                            ((0.06666667f * g_GlobalTimerDelta * g_CurrentPlayer->bondshotspeed.f[i]) / shotboost_norm);

                        if (g_CurrentPlayer->bondshotspeed.f[i] < 0.0f)
                        {
                            g_CurrentPlayer->bondshotspeed.f[i] = 0.0f;
                        }
                    }
                    else if (g_CurrentPlayer->bondshotspeed.f[i] < 0.0f)
                    {
                        g_CurrentPlayer->bondshotspeed.f[i] -=
                            ((0.06666667f * g_GlobalTimerDelta * g_CurrentPlayer->bondshotspeed.f[i]) / shotboost_norm);

                        if (g_CurrentPlayer->bondshotspeed.f[i] > 0.0f)
                        {
                            g_CurrentPlayer->bondshotspeed.f[i] = 0.0f;
                        }
                    }
                }
            }
        }
    }

    /**
     * This section updates the tank turret horizontal position (turning left and right),
     * as well as turning the tank left and right.
    */
    if (g_PlayerIsInTank == 1)
    {
        coord3d check_collision_p1;
        coord3d check_collision_p2;
        s32 step;
        f32 curTankAngleRad;
        f32 tankChangeInAngle;
        f32 savedTurretAngle;
        coord3d tank_collision_pt1;
        coord3d tank_collision_pt2;
        f32 tank_collision_dx;
        f32 tank_collision_dz;

        tankChangeInAngle = DegToRad1Fact(g_CurrentPlayer->speedtheta * g_GlobalTimerDelta) * 3.5f;
        curTankAngleRad = g_TankOrientationAngle + tankChangeInAngle;

        if (curTankAngleRad >= M_TAU_F)
        {
            curTankAngleRad -= M_TAU_F;
        }

        if (curTankAngleRad < 0.0f)
        {
            curTankAngleRad += M_TAU_F;
        }

        if (tankTestCollision(
            &g_CurrentPlayer->bond.collision_position,
            g_CurrentPlayer->bond.current_tile_ptr,
            curTankAngleRad,
            &check_collision_p1,
            &check_collision_p2))
        {
            g_TankOrientationAngle = curTankAngleRad;
        }
        else
        {
            f32 edgeDist2b;
            f32 edgeDist2a;
            f32 pushX;
            f32 pushZ;
            f32 pushDist;
            f32 edgeDist1;
            f32 tank_collision_norm;
            f32 playerDist;
            f32 normX;
            f32 normZ;
            f32 normDist;

            pushDist = -1;
            normDist = -1;

            stanGetCollisionEdge(&tank_collision_pt1, &tank_collision_pt2);

            tank_collision_dx = (tank_collision_pt2.f[0] - tank_collision_pt1.f[0]);
            tank_collision_dz = (tank_collision_pt2.f[2] - tank_collision_pt1.f[2]);

            tank_collision_norm = 1.0f / sqrtf((tank_collision_dx * tank_collision_dx) + (tank_collision_dz * tank_collision_dz));

            tank_collision_dx *= tank_collision_norm;
            tank_collision_dz *= tank_collision_norm;
            pushX = tank_collision_dz;
            pushZ = -tank_collision_dx;

            playerDist =
                ((g_CurrentPlayer->bond.collision_position.f[0] - check_collision_p2.f[0]) * pushX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - check_collision_p2.f[2]) * pushZ);

            if (playerDist < 0.0f)
            {
                playerDist = -playerDist;
                pushX = -pushX;
                pushZ = -pushZ;
            }

            edgeDist1 =
                ((g_CurrentPlayer->bond.collision_position.f[0] - tank_collision_pt1.f[0]) * pushX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - tank_collision_pt1.f[2]) * pushZ);

            if (edgeDist1 < playerDist)
            {
                pushDist = playerDist - edgeDist1;
            }

            tank_collision_dx = check_collision_p2.f[0] - check_collision_p1.f[0];
            tank_collision_dz = check_collision_p2.f[2] - check_collision_p1.f[2];

            tank_collision_norm = 1.0f / sqrtf((tank_collision_dx * tank_collision_dx) + (tank_collision_dz * tank_collision_dz));

            tank_collision_dx *= tank_collision_norm;
            tank_collision_dz *= tank_collision_norm;
            normX = tank_collision_dz;
            normZ = -tank_collision_dx;

            playerDist =
                ((g_CurrentPlayer->bond.collision_position.f[0] - check_collision_p2.f[0]) * normX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - check_collision_p2.f[2]) * normZ);

            if (playerDist < 0.0f)
            {
                playerDist = -playerDist;
                normX = -normX;
                normZ = -normZ;
            }

            edgeDist2a =
                ((g_CurrentPlayer->bond.collision_position.f[0] - tank_collision_pt1.f[0]) * normX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - tank_collision_pt1.f[2]) * normZ);
            edgeDist2b =
                ((g_CurrentPlayer->bond.collision_position.f[0] - tank_collision_pt2.f[0]) * normX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - tank_collision_pt2.f[2]) * normZ);

            if (edgeDist2b < edgeDist2a)
            {
                edgeDist2a = edgeDist2b;
            }

            if (edgeDist2a < playerDist)
            {
                normDist = playerDist - edgeDist2a;
            }

            if ((normDist >= 0.0f) && ((normDist < pushDist) || (pushDist < 0.0f)))
            {
                pushX = normX;
                pushZ = normZ;
                pushDist = normDist;
            }

            if (pushDist >= 0.0f)
            {
                move_offset.f[0] = pushDist * pushX * 1.01f;
                move_offset.f[2] = pushDist * pushZ * 1.01f;

                playerMoveWithCollision(&move_offset, 1);

                move_offset.f[0] = 0.0f;
                move_offset.f[2] = 0.0f;

                if (tankTestCollision(
                    &g_CurrentPlayer->bond.collision_position,
                    g_CurrentPlayer->bond.current_tile_ptr,
                    curTankAngleRad,
                    &check_collision_p1,
                    &check_collision_p2))
                {
                    g_TankOrientationAngle = curTankAngleRad;
                }
                else
                {
                    tankChangeInAngle = 0.0f;
                }
            }
            else
            {
                tankChangeInAngle = 0.0f;
            }
        }

        savedTurretAngle = g_TankTurretOrientationAngleRad;
        g_TankTurretAngle += g_TankTurretTurn;
        if (g_TankTurretAngle >= M_TAU_F)
        {
            g_TankTurretAngle -= M_TAU_F;
        }

        if (g_TankTurretAngle < 0.0f)
        {
            g_TankTurretAngle += M_TAU_F;
        }

        ftemp = (DegToRad1Fact(g_CurrentPlayer->speedtheta * 3.5f) * 4.0f) + g_TankTurretAngle;

        if (ftemp < 0.0f)
        {
            ftemp += M_TAU_F;
        }

        if (ftemp >= M_TAU_F)
        {
            ftemp -= M_TAU_F;
        }

        if ((ftemp - g_TankTurretOrientationAngleRad) >= M_PI_F)
        {
            ftemp -= M_TAU_F;
        }
        else if ((ftemp - g_TankTurretOrientationAngleRad) < -M_PI_F)
        {
            ftemp += M_TAU_F;
        }

        for (step=0; step<g_ClockTimer; step++)
        {
            g_TankTurretOrientationAngleDeg = ((TANKUPDATEROTATION_SCALE) * g_TankTurretOrientationAngleDeg) + ftemp;
        }

        g_TankTurretOrientationAngleRad = g_TankTurretOrientationAngleDeg * (1.0f - TANKUPDATEROTATION_SCALE);

        if (g_TankTurretOrientationAngleRad >= M_TAU_F)
        {
            g_TankTurretOrientationAngleRad -= M_TAU_F;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / (1.0f - TANKUPDATEROTATION_SCALE);
        }

        if (g_TankTurretOrientationAngleRad < 0.0f)
        {
            g_TankTurretOrientationAngleRad += M_TAU_F;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / (1.0f - TANKUPDATEROTATION_SCALE);
        }

        if (tankTestCollisionAt(
            &g_CurrentPlayer->bond.collision_position,
            g_CurrentPlayer->bond.current_tile_ptr,
            g_TankOrientationAngle) == 0)
        {
            g_TankTurretOrientationAngleRad = savedTurretAngle;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / (1.0f - TANKUPDATEROTATION_SCALE);
            g_TankTurretAngle = savedTurretAngle;
        }

        if (g_PlayerTankProp != NULL)
        {
            // sp 0x300
            struct TankRecord *tankRec;
            coord3d tank_move_offset;
            Mtxf rot;

            tankRec = (struct TankRecord *)g_PlayerTankProp->obj;

            tank_move_offset.f[1] = 0.0f;
            tank_move_offset.f[0] = g_TankModelPositionOffset.f[0];
            tank_move_offset.f[2] = g_TankModelPositionOffset.f[2];

            mtx4LoadYRotation(tankChangeInAngle, &rot);
            mtx4RotateVecInPlace(&rot, &tank_move_offset);
            tankUpdateTurretOffset();


            tank_move_offset.f[1] = 0.0f;
            tank_move_offset.f[0] = g_TankModelPositionOffset.f[0] - tank_move_offset.f[0];
            tank_move_offset.f[2] = g_TankModelPositionOffset.f[2] - tank_move_offset.f[2];

            mtx4LoadYRotation(M_TAU_F - g_TankOrientationAngle, &rot);
            mtx4ScaleRows012(tankRec->model->scale, &rot);
            mtx4RotateVecInPlace(&rot, &tank_move_offset);
            playerMoveWithCollision(&tank_move_offset, 1);
        }

        if (g_ClockTimer > 0) {
            for (i=0; i<g_ClockTimer; i++)
            {
                tank_turret_turn_speed = (TANKUPDATEROTATION_SCALE * tank_turret_turn_speed) + (g_TankTurretTurn / g_GlobalTimerDelta);
            }
        }

        ftemp = tank_turret_turn_speed * (1.0f - TANKUPDATEROTATION_SCALE);

        g_CurrentPlayer->vv_theta = (
            g_TankOrientationAngle +
            g_TankTurretOrientationAngleRad +
            ((DegToRad1Fact(g_CurrentPlayer->speedtheta * 3.5f)) * (4.0f)) +
            (ftemp * 4.0f)
            ) * 360.0f / M_TAU_F;

        while (g_CurrentPlayer->vv_theta < 0.0f)
        {
            g_CurrentPlayer->vv_theta += 360.0f;
        }
        while (g_CurrentPlayer->vv_theta >= 360.0f)
        {
            g_CurrentPlayer->vv_theta -= 360.0f;
        }
    }
    else
    {
        theta = g_CurrentPlayer->vv_theta + (g_CurrentPlayer->speedtheta * g_GlobalTimerDelta * 3.5f);

        while (theta < 0.0f)
        {
            theta += 360.0f;
        }
        while (theta >= 360.0f)
        {
            theta -= 360.0f;
        }

        g_CurrentPlayer->vv_theta = theta;
    }

    playerUpdateVertaTheta();

    // Handle crouching, and animation between standing and crouching.
    // Add basic block to declare local variables at the correct stack position.
    {
        f32 targetDuck;

        targetDuck = 0.0f;
        if (currentPlayerGetCrouchPos() == CROUCH_SQUAT)
        {
            targetDuck = FULL_CROUCH_OFFSET;
        }
        else if (currentPlayerGetCrouchPos() == CROUCH_HALF)
        {
            targetDuck = -60.0f;
        }
        else
        {
            // removed?
            currentPlayerGetCrouchPos();
        }

        if (targetDuck != g_CurrentPlayer->ducking_height_offset)
        {
            physMoveTowards(
                &g_CurrentPlayer->ducking_height_offset,
                targetDuck,
                &g_CurrentPlayer->crouchSpeed,
                CHR_OBJ_ACCEL_SPEED_FACTOR,
                CHR_OBJ_ACCEL_SPEED_FACTOR,
                CHR_OBJ_MAXSPEED, g_ClockTimer);
        }

        if (targetDuck == g_CurrentPlayer->ducking_height_offset)
        {
            g_CurrentPlayer->crouchSpeed = 0.0f;
        }
    }

    /**
     * Update forwards/backwards movement.
    */
    if (g_PlayerIsInTank == 1)
    {
        /**
         * This section handles the forward/backwards movement of the tank.
        */

        Mtxf seatRot;
        coord3d seatPos;
        f32 turretYawDeg;
        f32 pitch;
        s32 step;
        f32 yawDiff;
        f32 tank_engine_utilization_percent;
        struct TankRecord *tank_obj;

        /**
         * Check to see if Bond is just now entering the tank.
         * If so, initialize the tank prop.
         * This also handles spinning Bond around (if required) to face the same direction as the turret.
        */
        if (g_EnterTankAudioState == TANK_RUN_STATE_NOT_RUNNING)
        {
            if (g_PlayerTankProp != NULL)
            {
                tank_obj = (struct TankRecord *)g_PlayerTankProp->obj;
                mtx4LoadYRotation(M_TAU_F - g_TankOrientationAngle, &seatRot);
                mtx4ScaleRows012(tank_obj->model->scale, &seatRot);

                seatPos.f[0] = g_TankModelPositionOffset.f[0];
                seatPos.f[1] = g_TankModelPositionOffset.f[1];
                seatPos.f[2] = g_TankModelPositionOffset.f[2];
                mtx4RotateVecInPlace(&seatRot, &seatPos);

                seatPos.f[0] += tank_obj->runtime_pos.f[0];
                seatPos.f[1] += tank_obj->runtime_pos.f[1];
                seatPos.f[2] += tank_obj->runtime_pos.f[2];

                turretYawDeg = ((g_TankOrientationAngle + g_TankTurretOrientationAngleRad) * 360.0f) / M_TAU_F;
                pitch = g_CurrentPlayer->vv_verta;
                if (pitch < -20.0f)
                {
                    pitch = -20.0f;
                }

                g_TankEnteringSitHeight += g_GlobalTimerDelta / 45.0f;
                if (g_TankEnteringSitHeight >= 1.0f)
                {
                    g_TankEnteringSitHeight = 1.0f;
                }

                g_TankEnteringSitHeightRemain = (cosf(g_TankEnteringSitHeight * M_TAU_F * 0.5f) + 1.0f) * 0.5f;

                g_CurrentPlayer->vv_verta =
                    (g_TankEnteringSitHeightRemain * g_TankEnterBondVertAngleDeg)
                    + ((1.0f - g_TankEnteringSitHeightRemain) * pitch);

                yawDiff = turretYawDeg - g_TankEnterBondHorizAngleDeg;
                if (yawDiff > 180.0f)
                {
                    turretYawDeg -= 360.0f;
                }
                if (yawDiff < -180.0f)
                {
                    turretYawDeg += 360.0f;
                }

                g_CurrentPlayer->vv_theta =
                    (g_TankEnteringSitHeightRemain * g_TankEnterBondHorizAngleDeg)
                    + ((1.0f - g_TankEnteringSitHeightRemain) * turretYawDeg);

                if (g_CurrentPlayer->vv_theta >= 360.0f)
                {
                    g_CurrentPlayer->vv_theta -= 360.0f;
                }

                if (g_CurrentPlayer->vv_theta < 0.0f)
                {
                    g_CurrentPlayer->vv_theta += 360.0f;
                }

                move_offset.f[0] = (
                        (g_TankEnteringSitHeightRemain * g_EnterTankCoord.f[0]) +
                        ((1.0f - g_TankEnteringSitHeightRemain) * seatPos.f[0])
                    ) -
                    g_CurrentPlayer->bond.collision_position.f[0];

                move_offset.f[1] = 0.0f;

                move_offset.f[2] = (
                    (g_TankEnteringSitHeightRemain * g_EnterTankCoord.f[2]) +
                    ((1.0f - g_TankEnteringSitHeightRemain) * seatPos.f[2])
                    ) -
                    g_CurrentPlayer->bond.collision_position.f[2];
            }

            if (!(g_TankEnteringSitHeight >= 1.0f))
            {
            }
            else
            {
                g_EnterTankAudioState = TANK_RUN_STATE_STARTING;
            }
        }
        /**
         * Else, Bond has already entered the tank.
        */
        else
        {
            /**
             * There's an initial "starting" step.
            */
            if (g_EnterTankAudioState == TANK_RUN_STATE_STARTING)
            {
                g_EnterTankAudioState = TANK_RUN_STATE_RUNNING;
                if ((g_TankSfxState[0] == NULL) && (lvGetControlsLocked() == 0))
                {
                    sndPlaySfx(g_musicSfxBufferPtr, TRUCK_START_SFX, &g_TankSfxState[0]);
                }

                sndCreatePostEvent(g_TankSfxState[0], 8, 0x61A8);
                g_TankEngineSfxVolume = 0x61A8;
            }
            /**
             * Else Bond has fully entered the tank, and the engine is running.
             * Update turret vertical angle.
             * Update engine sound effect volume based on current tank speed.
            */
            else
            {
                f32 tank_scaled_speedforwards;
                f32 tank_scaled_speedtheta;
                f32 tank_vertical_angle;

                tank_scaled_speedforwards = g_CurrentPlayer->speedforwards / TANK_MAX_SPEED;
                tank_scaled_speedtheta = g_CurrentPlayer->speedtheta / 0.3f;

                if (tank_scaled_speedforwards < 0.0f)
                {
                    tank_scaled_speedforwards = -tank_scaled_speedforwards;
                }
                if (tank_scaled_speedtheta < 0.0f)
                {
                    tank_scaled_speedtheta = -tank_scaled_speedtheta;
                }

                tank_engine_utilization_percent = tank_scaled_speedforwards;
                if (tank_scaled_speedforwards < tank_scaled_speedtheta)
                {
                    tank_engine_utilization_percent = tank_scaled_speedtheta;
                }

                if (tank_engine_utilization_percent > 0.0f)
                {
                    if (tank_engine_utilization_percent > 1.0f)
                    {
                        tank_engine_utilization_percent = 1.0f;
                    }

                    if (g_TankSfxState[1] == NULL)
                    {
                        if (lvGetControlsLocked() == 0)
                        {
                            sndPlaySfx(g_musicSfxBufferPtr, TANK_SFX, &g_TankSfxState[1]);
                        }
                    }

                    if (g_TankSfxState[1] != NULL)
                    {
                        s32 sfxPitch;

                        sfxPitch = 0x7FFF;
                        if (tank_engine_utilization_percent < 0.15f)
                        {
                            sfxPitch = (s32) ((tank_engine_utilization_percent * 20000.0f) / 0.15f);
                        }
                        else if (tank_engine_utilization_percent < 0.9f)
                        {
                            sfxPitch = (s32) ((((tank_engine_utilization_percent - 0.15f) * 12767.0f) / 0.75f) + 20000.0f);
                        }

                        sndCreatePostEvent(g_TankSfxState[1], 8, sfxPitch);
                    }
                }
                else
                {
                    if (g_TankSfxState[1] != NULL)
                    {
                        if (sndGetPlayingState(g_TankSfxState[1]) != 0)
                        {
                            sndDeactivate(g_TankSfxState[1]);
                        }
                    }
                }

                if (g_TankSfxState[0] == NULL)
                {
                    if (lvGetControlsLocked() == 0)
                    {
                        sndPlaySfx(g_musicSfxBufferPtr, TRUCK_RUN_SFX, &g_TankSfxState[0]);
                    }
                }

                if (g_TankSfxState[0] != NULL)
                {
                    g_TankEngineSfxVolume = 0x7FFF;
                    if (tank_engine_utilization_percent < 0.9f)
                    {
                        g_TankEngineSfxVolume = (s32) (((tank_engine_utilization_percent * 7767.0f) / 0.9f) + 25000.0f);
                    }

                    sndCreatePostEvent(g_TankSfxState[0], 8, g_TankEngineSfxVolume);
                }

                if (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
                {
                    tank_vertical_angle = g_CurrentPlayer->bodyAimPitch;
                    tank_vertical_angle += 0.17453294f; /* should be DegToRad1Fact(10), but that yields 0.17453293f */
                }
                else
                {
                    tank_vertical_angle = g_TankTurretVerticalAngle;
                }

                if (tank_vertical_angle > DegToRad1Fact(25))
                {
                    tank_vertical_angle = DegToRad1Fact(25);
                }

                /* -0.087266468f should be DegToRad1Fact(-5), but that yields -0.0872664600611 */
                if (tank_vertical_angle < -0.087266468f)
                {
                    tank_vertical_angle = -0.087266468f;
                }

                for (step=0; step<g_ClockTimer; step++)
                {
                    g_TankTurretVerticalAngleRelated = (TANKTURRETVERTICALANGLERELATED_SCALE * g_TankTurretVerticalAngleRelated) + tank_vertical_angle;
                }

                g_TankTurretVerticalAngle = g_TankTurretVerticalAngleRelated * (1.0f - TANKTURRETVERTICALANGLERELATED_SCALE);
            }

        }

        g_CurrentPlayer->bondbreathing -= (0.750f * g_GlobalTimerDelta) / 2700.0f;

        if (g_CurrentPlayer->bondbreathing < 0.0f)
        {
            g_CurrentPlayer->bondbreathing = 0.0f;
        }

        playerTickMoveAnim(0.0f, 0.0f, 0.0f);

        move_offset.f[0] += g_CurrentPlayer->speedforwards * sinf(M_TAU_F - g_TankOrientationAngle) * g_GlobalTimerDelta;
        move_offset.f[2] += g_CurrentPlayer->speedforwards * cosf(M_TAU_F - g_TankOrientationAngle) * g_GlobalTimerDelta;

        playerMoveWithCollision(&move_offset, 1);

        if ((g_EnterTankAudioState == TANK_RUN_STATE_RUNNING) && (g_ClockTimer > 0))
        {
            f32 calc_x;
            f32 calc_z;
            f32 calc_speedforwards;

#if defined(VERSION_EU)
            // Divide by zero check.
            if (g_GlobalTimerDelta == 0)
            {
            }
#endif

            calc_x = (g_CurrentPlayer->bond.collision_position.f[0] - g_CurrentPlayer->bondprevpos.f[0]) / g_GlobalTimerDelta;
            calc_z = (g_CurrentPlayer->bond.collision_position.f[2] - g_CurrentPlayer->bondprevpos.f[2]) / g_GlobalTimerDelta;
            calc_speedforwards = sqrtf((calc_x * calc_x) + (calc_z * calc_z));

            if (g_CurrentPlayer->speedforwards < 0.0f)
            {
                calc_speedforwards = -calc_speedforwards;
            }

            g_CurrentPlayer->speedforwards = calc_speedforwards;
        }
    }
    else // not in tank: g_PlayerIsInTank != 1
    {
        f32 swayDx;
        f32 swayDz;
        f32 dist;
        f32 movedX;
        f32 movedZ;
        f32 start_collision_pos_x;
        f32 start_collision_pos_z;
        struct StandTile *tile;
        f32 speedforwards;
        f32 swayTargetZ;
        f32 speedsideways;
        f32 speedtheta;
        f32 startDist;
        f32 endDist;
        f32 swayTargetX;
        f32 nd; // canonical name
        f32 animScale;
        f32 moveRatio;
        StanSpecialTiles curLocus;
        LadderQuad bondCollision;
        f32 shorten; // canonical name
        f32 headpos_x;
        f32 headpos_z;
        struct StandTile *ladderTile1;
        struct StandTile *ladderTile2;
        f32 edgeNormX;
        f32 edgeNormZ;

        if ((g_TankSfxState[0] != NULL) && (sndGetPlayingState(g_TankSfxState[0]) != 0))
        {
            #if defined(VERSION_US)
            g_TankEngineSfxVolume -= (g_ClockTimer * 1000);
            #endif

            #if defined(VERSION_EU)
            g_TankEngineSfxVolume -= (s32)(1000.0f * g_GlobalTimerDelta);
            #endif

            if (g_TankEngineSfxVolume > 0)
            {
                sndCreatePostEvent(g_TankSfxState[0], 8, g_TankEngineSfxVolume);
            }
            else
            {
                sndDeactivate(g_TankSfxState[0]);
            }
        }

        if ((g_TankSfxState[1] != NULL) && (sndGetPlayingState(g_TankSfxState[1]) != 0))
        {
            sndDeactivate(g_TankSfxState[1]);
        }


        animScale = (g_BondMoveAnimationSetup[1].speedMultiplier * 0.5f  * g_GlobalTimerDelta);
        animSideways  = g_CurrentPlayer->speedsideways * animScale;

        /*
            The following is similar to a block of Perfect Dark bwalk0f0c69b8.
        */

        swayTargetX = -g_CurrentPlayer->swaytarget * g_CurrentPlayer->bond.theta_transform.f[2];
        swayTargetZ = g_CurrentPlayer->swaytarget * g_CurrentPlayer->bond.theta_transform.f[0];

        swayDx = (swayTargetX) - g_CurrentPlayer->swayoffset0;
        swayDz = (swayTargetZ) - g_CurrentPlayer->swayoffset2;

        dist = (swayDx * swayDx) + (swayDz * swayDz);

        if (dist >= 100.0f)
        {
            swayDx *= 0.6f * 1.0f;
            swayDz *= 0.6f * 1.0f;
        }

        speedsideways = g_CurrentPlayer->speedsideways * MAX_SPEED_FACTOR;
        speedforwards = g_CurrentPlayer->speedforwards;
        speedtheta = g_CurrentPlayer->speedtheta * MAX_SPEED_FACTOR;

        if (speedsideways < 0.0f)
        {
            speedsideways = -speedsideways;
        }
        if (speedforwards < 0.0f)
        {
            speedforwards = -speedforwards;
        }
        if (speedtheta < 0.0f)
        {
            speedtheta = -speedtheta;
        }

        maxspeed = speedforwards;

#if defined(VERSION_EU)
        if (maxspeed < speedsideways && 1)
#else
        if (maxspeed < speedsideways)
#endif
        {
            maxspeed = speedsideways;
        }
        if (maxspeed < speedtheta)
        {
            maxspeed = speedtheta;
        }

        if (dist >= 0.1f && maxspeed < MAX_SPEED_FACTOR)
        {
            maxspeed = MAX_SPEED_FACTOR;
        }

        if (maxspeed >= 0.750f)
        {
            g_CurrentPlayer->bondbreathing += (maxspeed - 0.750f) * g_GlobalTimerDelta / 900.0f ;
        }
        else
        {
            g_CurrentPlayer->bondbreathing -= (0.750f - maxspeed) * g_GlobalTimerDelta / 2700.0f;
        }

        if (g_CurrentPlayer->bondbreathing < 0.0f)
        {
            g_CurrentPlayer->bondbreathing = 0.0f;
        }
        else if (g_CurrentPlayer->bondbreathing > 1.0f)
        {
            g_CurrentPlayer->bondbreathing = 1.0f;
        }

        // perfect dark call: bmove0f0cc654
        playerTickMoveAnim(maxspeed, g_CurrentPlayer->speedforwards, animSideways);

        headpos_x = g_CurrentPlayer->headpos.f[0];
        headpos_z = g_CurrentPlayer->headpos.f[2];

        move_offset.f[0] +=
            (
                (headpos_z * g_CurrentPlayer->bond.theta_transform.f[0]) -
                (headpos_x * g_CurrentPlayer->bond.theta_transform.f[2])
            ) * g_GlobalTimerDelta;

        move_offset.f[2] +=
            (
                (headpos_z * g_CurrentPlayer->bond.theta_transform.f[2]) +
                (headpos_x * g_CurrentPlayer->bond.theta_transform.f[0])
            ) * g_GlobalTimerDelta;


        move_offset.f[0] += swayDx;
        move_offset.f[2] += swayDz;

        start_collision_pos_x = g_CurrentPlayer->bond.collision_position.f[0];
        start_collision_pos_z = g_CurrentPlayer->bond.collision_position.f[2];
        tile = g_CurrentPlayer->bond.current_tile_ptr;

        if (g_CheatTurboMode)
        {
            move_offset.f[0] +=
                (
                    (g_CurrentPlayer->bond.theta_transform.f[0] * g_CurrentPlayer->speedforwards) -
                    (g_CurrentPlayer->bond.theta_transform.f[2] * g_CurrentPlayer->speedsideways)
                ) * g_GlobalTimerDelta * 10.0f;

            move_offset.f[2] +=
                (
                    (g_CurrentPlayer->bond.theta_transform.f[2] * g_CurrentPlayer->speedforwards) +
                    (g_CurrentPlayer->bond.theta_transform.f[0] * g_CurrentPlayer->speedsideways)
                ) * g_GlobalTimerDelta * 10.0f;
        }

        playerMoveWithCollision(&move_offset, (g_CurrentPlayer->swaytarget == 0.0f));

        stanCircleGetSpecial(
            &tile,
            start_collision_pos_x,
            start_collision_pos_z,
            g_CurrentPlayer->bond.collision_radius * 1.16f,
            &curLocus);

        /* almost never true */
        if (curLocus.ladder)
        {
            use_stanHeight = 1;
        }

        stanCircleGetSpecial(
            &g_CurrentPlayer->bond.current_tile_ptr,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2],
            g_CurrentPlayer->bond.collision_radius * 1.01f,
            &curLocus);

        /* almost never true */
        if (curLocus.ladder)
        {
            use_stanHeight = 1;
        }

        stanCircleGetSpecial(
            &g_CurrentPlayer->bond.current_tile_ptr,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2],
            g_CurrentPlayer->bond.collision_radius,
            &curLocus);

        /* almost always true */
        if (!curLocus.ladder)
        {
            stanCircleGetSpecial(
                &tile,
                start_collision_pos_x,
                start_collision_pos_z,
                g_CurrentPlayer->bond.collision_radius * 0.990099f,
                &curLocus);
        }

        /* almost never true */
        if (curLocus.ladder)
        {
            use_stanHeight = 1;
            stanGetLadderQuad(&ladderTile1, &ladderTile2, &bondCollision);

            if (g_CurrentPlayer->stanHeight <= bondCollision.high0.f[1])
            {
                f32 moveLen;
                f32 climbDir2Z;
                f32 climbDir2X;
                f32 climbDirZ;
                f32 climbDirX;

                edgeNormZ = bondCollision.low0.f[0] - bondCollision.low1.f[0];
                edgeNormX = bondCollision.low1.f[2] - bondCollision.low0.f[2];

                nd = sqrtf((edgeNormX * edgeNormX) + (edgeNormZ * edgeNormZ));

                edgeNormZ = edgeNormZ / nd;
                edgeNormX = edgeNormX / nd;

                startDist =
                    ((start_collision_pos_x - bondCollision.low0.f[0]) * edgeNormX) +
                    ((start_collision_pos_z - bondCollision.low0.f[2]) * edgeNormZ);

                endDist =
                    (((start_collision_pos_x + move_offset.f[0]) - bondCollision.low0.f[0]) * edgeNormX) +
                    (((start_collision_pos_z + move_offset.f[2]) - bondCollision.low0.f[2]) * edgeNormZ);

                if ((startDist * endDist) <= 0.0f)
                {
                    shorten = 0.0f;
                }
                else
                {
                    if (endDist < 0.0f)
                    {
                        startDist = -startDist;
                        endDist = -endDist;
                    }

                    if (startDist <= endDist)
                    {
                        shorten = 0.0f;
                    }
                    else if (g_CurrentPlayer->bond.collision_radius < endDist)
                    {
                        shorten = 0.0f;
                    }
                    else if (startDist < g_CurrentPlayer->bond.collision_radius)
                    {
                        shorten = 0.0f;
                    }
                    else
                    {
                        shorten = (startDist - g_CurrentPlayer->bond.collision_radius) / (startDist - endDist);
                    }
                }

                moveLen = sqrtf((move_offset.f[0] * move_offset.f[0]) + (move_offset.f[2] * move_offset.f[2]));
                if (moveLen > 0.0f)
                {
                    f32 remaining;

                    remaining = (1.0f - shorten) * moveLen;
                    climbOffset = remaining * 0.25f;

                    if (bondCollision.high0.f[1] <= (g_CurrentPlayer->stanHeight + climbOffset))
                    {
                        climbOffset = (bondCollision.high0.f[1] - g_CurrentPlayer->stanHeight);
                        remaining -= (climbOffset / 0.25f);
                        shorten += (remaining / moveLen);
                    }
                }

                climbDirX = bondCollision.high0.f[0] - bondCollision.low0.f[0];
                climbDirZ = bondCollision.high0.f[2] - bondCollision.low0.f[2];
                climbDir2X = bondCollision.high1.f[0] - bondCollision.low0.f[0];
                climbDir2Z = bondCollision.high1.f[2] - bondCollision.low0.f[2];

                if (((climbDir2X * climbDir2X) + (climbDir2Z * climbDir2Z)) < ((climbDirX * climbDirX) + (climbDirZ * climbDirZ)))
                {
                    climbDirX = climbDir2X;
                    climbDirZ = climbDir2Z;
                }

                move_offset.f[0] = (move_offset.f[0] * shorten) + (climbDirX * (climbOffset / (bondCollision.high0.f[1] - bondCollision.low0.f[1])));
                move_offset.f[2] = (move_offset.f[2] * shorten) + (climbDirZ * (climbOffset / (bondCollision.high0.f[1] - bondCollision.low0.f[1])));

                g_CurrentPlayer->bond.collision_position.f[0] = start_collision_pos_x;
                g_CurrentPlayer->bond.collision_position.f[2] = start_collision_pos_z;
                g_CurrentPlayer->bond.current_tile_ptr = tile;

                playerMoveWithCollision(&move_offset, (g_CurrentPlayer->swaytarget == 0.0f));
            }
        }

        movedX = g_CurrentPlayer->bond.collision_position.f[0] - start_collision_pos_x;
        movedZ = g_CurrentPlayer->bond.collision_position.f[2] - start_collision_pos_z;
        moveRatio = (move_offset.f[0] * move_offset.f[0]) + (move_offset.f[2] * move_offset.f[2]);
        if (moveRatio != 0.0f)
        {
            moveRatio = ((movedX * movedX) + (movedZ * movedZ)) / moveRatio;
        }
        moveRatio = sqrtf(moveRatio);
        g_CurrentPlayer->swayoffset0 += moveRatio * swayDx;
        g_CurrentPlayer->swayoffset2 += moveRatio * swayDz;
    }

    // add basic block
    {
        f32 breathing;
        f32 turnSpeed;
        f32 weapon_speed_verta;

        turnSpeed = g_CurrentPlayer->speedtheta;
        weapon_speed_verta =
            (g_CurrentPlayer->speedverta / 0.7f) +
            (g_CurrentPlayer->crouchSpeed / CHR_OBJ_MAXSPEED);

        /*
        Following matches the end of Perfect Dark bwalk0f0c69b8
        */
        breathing = bheadGetBreathingValue();

        if (weapon_speed_verta > 1.0f)
        {
            weapon_speed_verta = 1.0f;
        }
        else if (weapon_speed_verta < -1.0f)
        {
            weapon_speed_verta = -1.0f;
        }

        if (g_CurrentPlayer->headanim == 1)
        {
            breathing *= 1.2f;
        }

        // Perfect Dark call bgun0f09d8dc
        gunUpdateSway(breathing, maxspeed, weapon_speed_verta, turnSpeed);

        // Perfect Dark call bgunSetAdjustPos
        gunSetPullback(DegToRad1Fact(g_CurrentPlayer->vv_verta360));
    }

    // end perfect dark `void bwalk0f0c69b8(void)`

    /**
     * The following section updates the TankRecord fields, and handles prop collision detection
     * with the tank. If colliding with character, play the "arrrhghhg" sound effect, or if
     * colliding with prop then set tank movement penalty and create an explosion.
    */
    if ((g_PlayerTankProp != NULL) && (g_PlayerIsInTank == 1) && (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING))
    {
        struct PropRecord *prop;
        struct TankRecord *tankRec;
        struct ObjectRecord *tankObj;
        f32 smoothY;
        ModelRoData_BoundingBoxRecord *bbox;
        Mtxf tankMtx;
        coord3d tankPos;
        s32 step;
        coord2d tankBounds[4];
        // roomids
        s32 rooms[8];
        s16 *lookup_index;

        tankRec = ((struct TankRecord *)g_PlayerTankProp->obj);
        tankObj = (struct  ObjectRecord*)g_PlayerTankProp->obj;
        bbox = &tankObj->model->obj->Switches[5]->Data->BoundingBox;

        tankRec->is_firing_tank = (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
            && gunGetFiringStatus(GUNRIGHT);

        tankRec->turret_vertical_angle = g_TankTurretVerticalAngle;
        tankRec->turret_orientation_angle = g_TankTurretOrientationAngleRad;
        tankRec->tank_orientation_angle = g_TankOrientationAngle;

        mtx4LoadYRotation(M_TAU_F - g_TankOrientationAngle, &tankMtx);
        mtx4ScaleRows012(tankObj->model->scale, &tankMtx);

        tankPos.f[0] = -g_TankModelPositionOffset.f[0];
        tankPos.f[1] = -g_TankModelPositionOffset.f[1];
        tankPos.f[2] = -g_TankModelPositionOffset.f[2];

        mtx4RotateVecInPlace(&tankMtx, &tankPos);

        tankPos.f[0] += g_CurrentPlayer->bond.collision_position.f[0];
        tankPos.f[2] += g_CurrentPlayer->bond.collision_position.f[2];

        tankObj->prop->stan = g_CurrentPlayer->bond.current_tile_ptr;

        stanWalkLine(
            &tankObj->prop->stan,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2],
            tankPos.f[0],
            tankPos.f[2]);

        tankRec->stan_y = stanGetFloorY(tankObj->prop->stan, tankPos.f[0], tankPos.f[2]);

        for (step=0; step<g_ClockTimer; step++)
        {
            tankRec->smoothYSum = (tankRec->smoothYSum * TANK_Y_SMOOTHING) + tankRec->stan_y;
        }

        smoothY = (tankRec->smoothYSum * (1.0f - TANK_Y_SMOOTHING));
        tankPos.f[1] = smoothY
            - (bboxGetYmin(bbox) * tankObj->model->scale)
            + 4.0f;

        mtx4Copy(&tankMtx,  &tankObj->mtx);

        tankObj->runtime_pos.f[0] = tankObj->prop->pos.f[0] = tankPos.f[0];
        tankObj->runtime_pos.f[1] = tankObj->prop->pos.f[1] = tankPos.f[1];
        tankObj->runtime_pos.f[2] = tankObj->prop->pos.f[2] = tankPos.f[2];

        setupUpdateObjectRoomPosition(tankObj);
        objUpdateCollisionBounds(tankObj);
        tankGetCollisionBounds(tankBounds, &g_CurrentPlayer->bond.collision_position, g_TankOrientationAngle);
        propGetRoomIds(tankObj->prop, rooms);

        // update g_NumRoomPropsFound
        roomGetProps(rooms);

        for (lookup_index=g_RoomPropsFound; *lookup_index>=0; lookup_index++)
        {
            prop = &g_Props[*lookup_index];
            if (prop != tankObj->prop)
            {
                if (prop->type == PROP_TYPE_CHR)
                {
                    f32 ymax;
                    f32 ymin;
                    f32 radius;
                    s32 missed;
                    coord3d explosionPos;
                    struct ChrRecord *hitChr;

                    missed = 1;
                    hitChr = prop->chr;
                    propGetCollisionRadius(prop, &radius, &ymax, &ymin);

                    if (polygonContainsPoint(&prop->pos, tankBounds, 4))
                    {
                        missed = 0;

                        if (hitChr->actiontype == ACT_DIE)
                        {
                            if ((hitChr->chrflags & CHRFLAG_01000000) == 0 && lvGetControlsLocked() == 0)
                            {
                                hitChr->chrflags |= CHRFLAG_01000000;
                                if ((g_LvTickCount % 3) < 2)
                                {
                                    sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, CRUSHED_YELL_SFX, NULL), &prop->pos);
                                }
                                if ((g_LvTickCount % 3) > 0)
                                {
                                    sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, TANK_CRUSH_MAN_SFX, NULL), &prop->pos);
                                }
                            }
                        }
                    }

                    if (missed && (polygonTouchesCircle(&prop->pos, radius, tankBounds, 4)))
                    {
                        missed = 0;
                    }

                    if (missed == 0)
                    {
                        explosionPos.f[0] = tankObj->runtime_pos.f[0];
                        explosionPos.f[1] = prop->pos.f[1];
                        explosionPos.f[2] = tankObj->runtime_pos.f[2];

                        chrExplosionDamage(prop->chr, &explosionPos, 3.0f, 1);
                    }
                }
                else if (prop->type == PROP_TYPE_OBJ)
                {
                    coord2d *polygon;
                    s32 edges;

                    propGetCollisionPolygon2d(prop, &polygon, &edges);
                    if ((edges > 0) && polygonsOverlap(polygon, edges, tankBounds, 4))
                    {
                        // Explode destroyable props when the tank touches them
                        objDetonateRecursive(prop, 10000.0f, &prop->obj->runtime_pos, 0x20, playerGetCurrentNum());
                        g_TankDamagePenaltyTicks = TANK_DAMAGE_PENTALTY_TICKS;
                    }
                }
            }
        }
    }

    playerUpdateVertical(use_stanHeight, climbOffset);
    playerUpdatePosFromCollision();
    playerUpdateCollisionBounds();
}


/**
 * US address 7F086990.
 * EU address 7F086AB0.
*/
void playerMoveFrozen(u16 buttons, u16 oldbuttons)
{
    coord3d property_pos;
    coord3d property_pos2;
    coord3d property_offset;
    coord3d offset;
    struct StandTile *room_pointer_tile;
    coord3d stan_walk_start;

    property_pos = g_DefaultFrozenPlayerPos;
    property_pos2 = g_DefaultFrozenPlayerPos2;
    property_offset = g_DefaultFrozenPlayerOffset;
    offset = g_DefaultFrozenMoveOffset;

    playerTickDamageAndHealth();
    playerTickExplode();
    playerProcessInput(0, 0, 0, 0);
    playerUpdateVertaTheta();
    playerTickMoveAnim(0, 0, 0);

    if (((g_ForceBondMoveOffset.f[0] != 0.0f) || (g_ForceBondMoveOffset.f[2] != 0.0f))
        && (!g_CoopMode || (playerGetCurrentNum() == g_CoopForceMovePlayer)))
    {
        offset.f[0] += g_ForceBondMoveOffset.f[0] * g_GlobalTimerDelta;
        offset.f[2] += g_ForceBondMoveOffset.f[2] * g_GlobalTimerDelta;
    }

    offset.f[0] += ((g_CurrentPlayer->headpos.f[2] * g_CurrentPlayer->bond.theta_transform.f[0]) - (g_CurrentPlayer->headpos.f[0] * g_CurrentPlayer->bond.theta_transform.f[2])) * g_GlobalTimerDelta;
    offset.f[2] += ((g_CurrentPlayer->headpos.f[2] * g_CurrentPlayer->bond.theta_transform.f[2]) + (g_CurrentPlayer->headpos.f[0] * g_CurrentPlayer->bond.theta_transform.f[0])) * g_GlobalTimerDelta;

    playerMoveWithCollision(&offset, 1);
    playerUpdateVertical(0, 0.0f);
    playerUpdatePosFromCollision();

    if ((g_CameraMode == CAMERAMODE_FP_NOINPUT) || (g_CameraMode == CAMERAMODE_FP) || (g_CameraMode == CAMERAMODE_FADE_TO_TITLE))
    {
        playerSetCameraModeRaw(0);
        return;
    }

    playerTickFrozenCamera(buttons, oldbuttons, &property_pos, &property_pos2, &room_pointer_tile, &stan_walk_start);
    playerSetCameraModeRaw(1);
    playerSetPosition(&property_pos, &property_pos2, &property_offset, room_pointer_tile, &stan_walk_start);
}


/**
 * Sets/updates viewport for player.
 * Refreshes autoaim setting.
 * Arguments are passed into playerMove or playerMoveFrozen.
 * Checks if necessary to call bossReturnTitleStage.
 * Set player->buttons_pressed to arg2.
 *
 * Address 0x7F086F9C (VERSION_US).
 * Address 0x7F0870BC (VERSION_EU).
 * Address 0x7F087668 (VERSION_JP).
 */
void playerUpdate(s8 stick_x, s8 stick_y, u16 buttons)
{
#ifdef VERSION_EU
    f32 faspect;
#endif

    playerSetFovY(FOV_Y_F);

    // This call doesn't do anything, the call viSetFovY(g_CurrentPlayer->fovy); in lvRender
    // will actually change the field of view.
    // The call above should set g_CurrentPlayer->fovy, but it doesn't seem to affect
    // the fov....
    viSetFovY(FOV_Y_F);

    if (cameraFrameCounter1 != 0)
    {
        if ((cameraFrameCounter1 >= 4) && (resolution != 0) && (viGetFrameBuf2() == (u8*)(cfb_16[1])))
        {
            cameraBufferToggle = 1;
            cameraFrameCounter1 = 0;
        }
        else
        {
            cameraFrameCounter1 += 1;
        }
    }
    else
    {
        if (cameraFrameCounter2 != 0)
        {
            if ((cameraFrameCounter2 >= 4) && (viGetFrameBuf2() == (u8*)(cfb_16[0])))
            {
                cameraBufferToggle = 0;
                cameraFrameCounter2 = 0;
            }
            else
            {
                cameraFrameCounter2 += 1;
            }
        }
    }

    if ((cameraBufferToggle != 0) && (viGetFrameBuf2() == (u8*)(cfb_16[1])))
    {
        viSetFrameBuf2((u8 *) resolution);
    }

#ifdef VERSION_EU
    if (optionsGetScreenRatio() == SCREEN_RATIO_16_9)
    {
        faspect = ((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight()) * 0.75f * WIDESCREEN_ASPECT;
    }
    else
    {
        faspect = (f32) playerGetViewportWidth() / (f32) playerGetViewportHeight();
    }

    if (cameraBufferToggle == 0)
    {
        faspect *= EU_CAMERA_8003642C_ASPECT;
    }

    playerSetAspect(faspect);
    viSetAspect(faspect);

#else

    if (optionsGetScreenRatio() == SCREEN_RATIO_16_9)
    {
        playerSetAspect(((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight()) * 0.75f * WIDESCREEN_ASPECT);
        viSetAspect(((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight()) * 0.75f * WIDESCREEN_ASPECT);
    }
    else
    {
        playerSetAspect((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight());
        viSetAspect((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight());
    }
#endif

    playerSetScreenSize( playerGetViewportWidth(), playerGetViewportHeight());
    playerSetViewportPos( playerGetViewportLeft(), playerGetViewportTop());
    viSetXY(viGetFrameWidth(), viGetFrameHeight());
    viSetBuf(viGetFrameWidth(), viGetFrameHeight());
    // Hi-res doubles the physical width only; game logic stays 320 wide.
    viSetScaleX((optionsGetHiRes() && (cameraBufferToggle == 0)) ? VI_MAX_SCALEX : 1);
    viSetViewSize(playerGetViewportWidth(), playerGetViewportHeight());
    viSetViewPosition(playerGetViewportLeft(), playerGetViewportTop());
    currentPlayerTickColourFade();
    currentPlayerTickChrFade();
    currentPlayerSetYAutoAimEnabled(optionsGetAutoAim());
    currentPlayerSetXAutoAimEnabled(optionsGetAutoAim());
    currentPlayerSetLookAheadSetting(optionsGetLookAhead());
    gunSetGunAmmoVisible(GUNAMMOREASON_OPTION, optionsGetAmmoOnScreen());

    gunSetSightVisible(
        GUNSIGHTREASON_1,
        (!isMpGame() && optionsGetSightOnScreen())
            || (isMpGame() && g_playerPerm->sight)
    );

#if defined(VERSION_EU)
#endif

    if ((g_CameraMode == CAMERAMODE_NONE) || ((g_CameraMode == CAMERAMODE_FP) && (is_timer_active != 0)) || (g_CameraMode == CAMERAMODE_FADE_TO_TITLE))
    {
        if (playerGetCurrentNum() == 0)
        {
            mission_timer += g_ClockTimer;
        }

        playerMove(stick_x, stick_y, buttons, (u16) g_CurrentPlayer->buttons_pressed);
    }
    else
    {
        playerMoveFrozen(buttons, (u16) g_CurrentPlayer->buttons_pressed);
    }

#if defined(BUGFIX_R1)
    watch_time_0 += g_FrameDelta60;
#else
    // VERSION_US
    watch_time_0 += g_FrameDelta;
#endif

    if (stop_time_flag != 0)
    {
        if ((lvGetControlsLocked() == 0) && ((buttons & ~(g_CurrentPlayer->buttons_pressed) & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON | CONT_R | CONT_L))))
        {
            stop_time_flag = 2;

            if (currentPlayerIsFadeComplete())
            {
                if (g_CurrentPlayer->colourscreenfrac == 0.0f)
                {
                    currentPlayerSetFadeColour(0, 0, 0, 0.0f);
                    currentPlayerSetFadeFrac(60.0f, 1.0f);
                }
            }
            else
            {
                if (g_CurrentPlayer->colourfadefracnew == 0.0f)
                {
                    currentPlayerSetFadeFrac(g_CurrentPlayer->colourfadetime60, 1.0f);
                }
            }
        }

        if ((stop_time_flag == 2) && currentPlayerIsFadeComplete() && (g_CurrentPlayer->colourscreenfrac == 1.0f))
        {
            bossReturnTitleStage();
        }
    }

    if (g_CameraAfterCinema)
    {
        playerAdvanceCameraMode();
    }

    if (g_CurrentPlayer->bonddead)
    {
        if (g_CurrentPlayer->redbloodfinished == FALSE)
        {
            gunEquip(GUNLEFT, 0);
            gunEquip(GUNRIGHT, 0);

            if (0)
            {
                // removed?
            };
        }

        if (g_CurrentPlayer->redbloodfinished && g_CurrentPlayer->deathanimfinished && (camera_mode >= CAMERAMODE_SWIRL))
        {
            bossRunTitleStage();
        }
    }

    if ((g_CameraMode == CAMERAMODE_FADE_TO_TITLE) && currentPlayerIsFadeComplete())
    {
        bossRunTitleStage();
    }

    g_CurrentPlayer->buttons_pressed = buttons;
}


void playerSelectCuff(Model *model, ModelFileHeader *header, s32 switchindex)
{
    s32 local;
    ModelNode **switches;
    ModelNode **base;
    ModelNode *node;
    s32 offset;
    s32 *rwdata;
    s32 index;
    s32 visible;

    local = fileGetBondForCurrentFolder();
    switches = header->Switches;
    offset = switchindex << 2;


    // byte-indexed on purpose: offset = switchindex * 4. &switches[i] won't match.
    base = (ModelNode **) (((u8 *) switches) + offset);

    if (base[0] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, base[0]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_BOILER;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 1;

    if (((void *) (index * 0)) != base[1])
    {
        node = switches[index];
        rwdata = (s32 *) modelGetNodeRwData(model, node);
        visible = g_CurrentPlayer->bondtype == CUFF_BROSNAN;
        if (visible == 0)
        {
            visible = g_CurrentPlayer->bondtype == CUFF_DALTON;
            if (visible == 0)
            {
                visible = g_CurrentPlayer->bondtype == CUFF_MOORE;
                if (visible == 0)
                {
                    visible = g_CurrentPlayer->bondtype == CUFF_FOLDER;
                    if (visible != 0)
                    {
                        visible = local != 1;
                    }
                }
            }
        }
        *rwdata = visible;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 2;

    if (base[2] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        visible = g_CurrentPlayer->bondtype == CUFF_CONNERY;

        if (visible == 0)
        {
            visible = g_CurrentPlayer->bondtype == CUFF_FOLDER;
            if (visible != 0)
            {
                visible = local == 1;
            }
        }

        *rwdata = visible;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 3;

    if (base[3] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_BLUE;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = (switchindex + 4) ^ (((switchindex + 4) ^ 0) * 0);

    if (base[4])
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_JUNGLE;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 5;

    if (base[5] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_SNOW;
    }
}


void playerSetWorldScale(f32 scale)
{
    f32 scalar;
    struct PlayerBond* col;

    scalar = g_PlayerWorldScale / scale;

    col = &g_CurrentPlayer->bond;
    col->collision_position.x *= scalar;
    col->collision_position.z *= scalar;

    g_PlayerWorldScale = scale;
    g_PlayerWorldScaleInv = 1.0f / scale;
}


f32 playerGetStanHeight(struct player *player)
{
    return player->stanHeight;
}


f32 playerGetEyeHeight(struct player *player)
{
    return player->eyeheight + player->crouchHeight + player->ducking_height_offset;
}


PropRecord* currentPlayerGetProp(void) {
    return g_CurrentPlayer->prop;
}


f32 playerGetBreathing(void)
{
    return g_CurrentPlayer->bondbreathing;
}


/**
 * Gets the current player's heading angle, converted from degrees to radians.
 * @return Heading (Yaw) in Radians
*/
f32 bondviewGetPlayerYawRadians(void)
{
    return DegToRad(360.0f - g_CurrentPlayer->vv_theta);
}


/**
 * Gets the current player's vertical look angle, converted from degrees to radians.
 * @return Pitch in radians.
 */
f32 bondviewGetPlayerPitchRadians(void)
{
    return DegToRad(g_CurrentPlayer->vv_verta);
}


s32 playerPressedActivate(void) {
    return g_CurrentPlayer->activatePressed;
}


/**
 * Sets g_VisibleToGuardsFlag.
 * 1 = visible, 0 = not visible.
 */
void playerSetVisibleToGuards(s32 visible)
{
  g_VisibleToGuardsFlag = visible;
}

/**
 * Gets g_VisibleToGuardsFlag.
 * 1 = visible, 0 = not visible
 */
s32 playerGetVisibleToGuards(void)
{
    return g_VisibleToGuardsFlag;
}


u8 playerGetRoom(void)
{
    if ((g_CurrentPlayer->cameramode == 1) && (g_CurrentPlayer->cameratile != 0))
    {
        return g_CurrentPlayer->cameratile->room;
    }

    return g_CurrentPlayer->bond.current_tile_ptr_for_portals->room;
}


coord3d *playerGetPos(void)
{
    if (g_CurrentPlayer->cameramode == 1)
    {
        return &g_CurrentPlayer->pos;
    }

    return &g_CurrentPlayer->bond.pos;
}


coord3d * playerGetPos3(void)
{

    if (g_CurrentPlayer->cameramode == 1)
    {
        return &g_CurrentPlayer->pos3;
    }

    return &g_CurrentPlayer->bond.pos3;
}

coord3d *playerGetPrevPos(void)
{
    return &g_CurrentPlayer->bondprevpos;
}


void playerSetMoving(PropRecord *prop, s32 flag)
{
    s32 playerIndex;

    playerIndex = playerGetIndexForProp(prop);

    if (prop->chr != NULL)
    {
        chrSetMoving(prop->chr, flag);
    }

    if (g_PlayerTankProp != NULL)
    {
        // When commented out tank shells fired from the tank detonate immediately.
        objSetMoving(g_PlayerTankProp, flag);
    }

    g_playerPointers[playerIndex]->perimEnabled = flag;
}


void playerGetPropBounds(PropRecord *prop, coord2d **bounds, s32 *numpoints, f32 *height_related, f32 *collision)
{
    s32 index;

    index = playerGetIndexForProp(prop);
    if (g_playerPointers[index]->perimEnabled != 0)
    {
        // What is this doing and why is it 1 player only?
        if (getPlayerCount() == 1 || g_playerPointers[index]->bonddead == FALSE)
        {
            if (g_playerPointers[index]->cameramode != 1)
            {
                *numpoints = 4;
                *bounds = g_playerPointers[index]->collision_bounds;
                *collision = g_playerPointers[index]->manGround;
                *height_related = *collision + playerGetEyeHeight(g_playerPointers[index]) + 10.0f;

                return;
            }
        }
    }

    *numpoints = 0;
}


void playerUpdateCollisionBounds(void)
{

    if (g_PlayerIsInTank == 1)
    {
        tankGetCollisionBounds(g_CurrentPlayer->collision_bounds, &g_CurrentPlayer->bond.collision_position, g_TankOrientationAngle);

        return;
    }

    g_CurrentPlayer->collision_bounds[0].x = (g_CurrentPlayer->bond.collision_position.f[0] + g_CurrentPlayer->bond.collision_radius);
    g_CurrentPlayer->collision_bounds[0].y = g_CurrentPlayer->bond.collision_position.f[2];
    g_CurrentPlayer->collision_bounds[1].x = g_CurrentPlayer->bond.collision_position.f[0];
    g_CurrentPlayer->collision_bounds[1].y = (g_CurrentPlayer->bond.collision_position.f[2] + g_CurrentPlayer->bond.collision_radius);
    g_CurrentPlayer->collision_bounds[2].x = (g_CurrentPlayer->bond.collision_position.f[0] - g_CurrentPlayer->bond.collision_radius);
    g_CurrentPlayer->collision_bounds[2].y = g_CurrentPlayer->bond.collision_position.f[2];
    g_CurrentPlayer->collision_bounds[3].x = g_CurrentPlayer->bond.collision_position.f[0];
    g_CurrentPlayer->collision_bounds[3].y = (g_CurrentPlayer->bond.collision_position.f[2] - g_CurrentPlayer->bond.collision_radius);
}


/**
 * @param prop: prop
 * @param collision_radius: out parameter, will be set to bond.collision_radius
 * @param height: out parameter, will be set to character height - 30
 * @param always_30: out parameter, will be set to 30
 */
void playerGetCollisionSize(PropRecord *prop, f32 *collision_radius, f32 *height, f32 *always_30)
{
    struct player *player = g_playerPointers[playerGetIndexForProp(prop)];

    *collision_radius = player->bond.collision_radius;
    *height = (playerGetEyeHeight(player) + 10.0f) - 30.0f;
    *always_30 = 30.0f;
}


s32 playerTick(PropRecord *prop)
{
    s32 index;
    ChrRecord *chr;
    s32 group;
    s32 ret;
    s32 sub;
    PropRecord *leftprop;
    f32 mtx[15];
    s32 tailret;
    s32 anim;
    f32 angle;
    f32 frame;
    f32 local90;
    f32 local8c;
    f32 local88;
    f32 fwd;
    f32 startframe;
    struct weapon_firing_animation_table *firingtable;
    coord3d off;
    s32 i;
    s32 found;
    s32 cur;
    PropRecord *rightprop;
    struct WeaponObjRecord *leftobj;
    struct WeaponObjRecord *rightobj;
    s32 setanim;
    struct player **ppointers;
 
    index = playerGetIndexForProp(prop);
    chr = prop->chr;
 
    if (chr != NULL)
    {
        if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
        {
            chr->hidden &= ~CHRHIDDEN_FREEZE;
        }
    }

    // Co-op: the partner sits out cutscenes and isn't shown in them.
    if (g_CoopSoloViewIsCinema && (index != g_CoopSoloView))
    {
        goto clear_and_return;
    }
 
    if (chr != NULL)
    {
        if ((g_playerPointers[index]->bodyModel != NULL) && (g_playerPointers[index]->cameramode == 1))
        {
            g_playerPointers[index]->perimEnabled = 0;
            ret = chrTick(prop);
            g_playerPointers[index]->perimEnabled = 1;
 
            g_playerPointers[index]->bond.collision_position.x = g_playerPointers[index]->prop->pos.x;
            g_playerPointers[index]->bond.collision_position.y = g_playerPointers[index]->prop->pos.y;
            g_playerPointers[index]->bond.collision_position.z = g_playerPointers[index]->prop->pos.z;
            g_playerPointers[index]->bond.current_tile_ptr = g_playerPointers[index]->prop->stan;
            playerUpdateRooms(g_playerPointers[index]);
 
            if (prop->flags & PROPFLAG_ONSCREEN)
            {
                RenderPosView *rp = g_playerPointers[index]->bodyModel->render_pos;
                mtx4MultAffine(camGetViewToWorldMtxf(), (Mtxf *) rp, (Mtxf *) mtx);
                g_playerPointers[index]->bond.pos.x = mtx[12] + (mtx[4] * 7.0f);
                g_playerPointers[index]->bond.pos.y = mtx[13] + (mtx[5] * 7.0f);
                g_playerPointers[index]->bond.pos.z = mtx[14] + (mtx[6] * 7.0f);
            }
 
            return ret;
        }
    }
 
    if (chr == NULL)
    {
        goto clear_and_return;
    }

    if (g_playerPointers[index]->bodyModel == NULL)
    {
        goto clear_and_return;
    }

    if (getPlayerCount() < 2)
    {
        goto clear_and_return;
    }

    if (playerGetCurrentNum() == index)
    {
        goto clear_and_return;
    }
 
    anim = 0;
    firingtable = NULL;
    local90 = -1.0f;
    frame = -1;
    leftprop = chrGetEquippedWeaponProp(chr, GUNLEFT);
    rightprop = chrGetEquippedWeaponProp(chr, GUNRIGHT);
    leftobj = NULL;
    rightobj = NULL;
    setanim = 0;
 
    if (leftprop != NULL)
    {
        leftobj = leftprop->weapon;
    }
    
    if (rightprop != NULL)
    {
        rightobj = rightprop->weapon;
    }
 
    ppointers = g_playerPointers;
 
    if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
    {
        g_PlayerTickCount = g_PlayerTickCount + 1;
    }
 
    /**
     * If the player count is 1 we jump to the bottom of the function with goto clear_and_return, so this block only applies to MP.
     * g_PlayerTickCount advances once for each remote player's prop ticked during the pass of the viewport whose player is first in the shuffle order.
     * That means g_PlayerTickCount reaches 2 after one frame of a 2 player game, and part way through the first frame of a 3 or 4 player game.
     * Nothing *ever* resets g_PlayerTickCount, so we're skipping this whole block only once per boot. The reason for doing this though
     * isn't quite clear so do chime in if you have any theories.
     */
    if (g_PlayerTickCount >= 2)
    {
        local8c = ppointers[index]->bodyAimPitch;
        local88 = ppointers[index]->bodyAimYaw;
 
        if (ppointers[index]->bonddead != FALSE)
        {
            found = 0;
 
            for (i = 0; i < g_bondviewBondDeathAnimationsCount; i++)
            {
                cur = ppointers[index]->players_cur_animation;
 
                if (cur == (g_bondviewBondDeathAnimations[i] + ((s32) g_AnimTable)))
                {
                    found = 1;
                }
            }
 
            if (found)
            {
                anim = ppointers[index]->players_cur_animation;
                angle = 0.5f;
            }
            else
            {
                anim = g_bondviewBondDeathAnimations[randomGetNext() % g_bondviewBondDeathAnimationsCount] + ((s32) g_AnimTable);
                angle = 0.5f;
            }
 
            cur = ppointers[index]->players_cur_animation;
            local8c = 0.0f;
            local88 = 0.0f;
            goto join_768;
        }
 
        if ((leftprop != NULL) && (rightprop != NULL))
        {
            group = 3;
        }
        else if ((leftprop == NULL) && (rightprop == NULL))
        {
            group = 2;
        }
        else if ((leftobj != NULL) && (weaponHasFlag(leftobj->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN) == 0))
        {
            group = 2;
        }
        else if ((rightobj != NULL) && (weaponHasFlag(rightobj->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN) == 0))
        {
            group = 2;
        }
        else if ((leftobj != NULL) && (weaponHasFlag(leftobj->weaponnum, WEAPONSTATBITFLAG_ONLY_1_HANDED) != 0))
        {
            group = 0;
        }
        else if ((rightobj != NULL) && (weaponHasFlag(rightobj->weaponnum, WEAPONSTATBITFLAG_ONLY_1_HANDED) != 0))
        {
            group = 0;
        }
        else
        {
            group = 1;
        }
 
        if (playerGetCrouchPos(index) == 1)
        {
            goto set_crouch_lean;
        }
        else if (playerGetCrouchPos(index) == 0)
        {
set_crouch_lean:
            angle = 1.0f;
            sub = 5;
        }
        else if ((ppointers[index]->speedsideways < 0.0f) && (firing_animation_groups[group][4].pointer != NULL))
        {
            sub = 4;
            angle = -ppointers[index]->speedsideways;
 
            if (ppointers[index]->bodyStrafeTurn < 90.0f)
            {
                ppointers[index]->bodyStrafeTurn = ppointers[index]->bodyStrafeTurn + 15.0f;
            }
        }
        else if ((ppointers[index]->speedsideways > 0.0f) && (firing_animation_groups[group][3].pointer != NULL))
        {
            sub = 3;
            angle = ppointers[index]->speedsideways;
 
            if (ppointers[index]->bodyStrafeTurn > (-90.0f))
            {
                ppointers[index]->bodyStrafeTurn = ppointers[index]->bodyStrafeTurn - 15.0f;
            }
        }
        else
        {
            frame = ppointers[index]->speedtheta;
 
            if (frame < 0.0f)
            {
                frame = -frame;
            }
 
            fwd = ppointers[index]->speedforwards;
 
            if (fwd < -0.050000001f)
            {
                frame = -frame;
 
                if (fwd < frame)
                {
                    frame = fwd;
                }
 
                if (ppointers[index]->headanim == 0)
                {
                    goto shared_double_neg;
                }
 
                sub = 2;
 
                if (-0.40000001f < fwd)
                {
shared_double_neg:
                    angle = frame + frame;
                    sub = 1;
 
                    if (angle < (-1.0f))
                    {
                        angle = -1.0f;
                    }
                }
                else
                {
                    angle = frame;
                    goto lean_return_to_centre;
                }
            }
            else
            {
                if (0.050000001f < fwd)
                {
                    goto shared_framefwd;
                }
                else if (0.050000001f < frame)
                {
shared_framefwd:
                    if (frame < fwd)
                    {
                        frame = fwd;
                    }
                }
                else
                {
                    goto set_full_lean;
                }
 
                if (ppointers[index]->headanim == 0)
                {
                    goto shared_double_pos;
                }
 
                sub = 2;
 
                if (fwd < 0.40000001f)
                {
shared_double_pos:
                    angle = frame + frame;
                    sub = 1;
 
                    if (1.0f < angle)
                    {
                        angle = 1.0f;
                    }
                }
                else
                {
                    angle = frame;
                    goto lean_return_to_centre;
                }
            }
 
            goto lean_return_to_centre;
 
set_full_lean:
            angle = 1.0f;
            sub = 0;
 
lean_return_to_centre:
            if (0.0f < ppointers[index]->bodyStrafeTurn)
            {
                ppointers[index]->bodyStrafeTurn = ppointers[index]->bodyStrafeTurn - 15.0f;
            }
 
            if (ppointers[index]->bodyStrafeTurn < 0.0f)
            {
                ppointers[index]->bodyStrafeTurn = ppointers[index]->bodyStrafeTurn + 15.0f;
            }
        }
 
        {
            struct firing_anim_struct *fa = &firing_animation_groups[group][sub];
 
            firingtable = fa->pointer;
 
            if (fa->anim != 0)
            {
                anim = fa->anim + (s32) g_AnimTable;
            }
 
            angle *= fa->x;
            local90 = fa->z;
            frame = fa->y;
        }
 
        cur = ppointers[index]->players_cur_animation;
 
join_768:
        if ((firingtable != NULL) && (anim == 0))
        {
            anim = *((s32 *) firingtable);
        }
 
        if (anim != cur)
        {
            setanim = 1;
        }
 
        if (0.0f <= frame)
        {
            if ((ppointers[index]->bodyModel->animlooping == 0) || (frame != ppointers[index]->bodyModel->animloopframe))
            {
                setanim = 1;
            }
        }
 
        if (frame < 0.0f)
        {
            if (ppointers[index]->bodyModel->animlooping)
            {
                setanim = 1;
            }
        }
 
        if (setanim != 0)
        {
            if (ppointers[index]->bodyModel->anim2 == NULL)
            {
                startframe = (0.0f <= frame) ? (frame) : (0.0f);
                modelSetAnimation(ppointers[index]->bodyModel, (ModelAnimation *) anim, 0, startframe, angle, 16.0f);
                ppointers[index]->players_cur_animation = anim;
                ppointers[index]->bodyAnimSpeed = angle;
 
                if (0.0f <= frame)
                {
                    modelSetAnimLooping(ppointers[index]->bodyModel, frame, 16.0f);
                }
 
                if (0.0f <= local90)
                {
                    modelSetAnimEndFrame(ppointers[index]->bodyModel, local90);
                }
            }
 
            cur = ppointers[index]->players_cur_animation;
        }
        else
        {
            if (angle != ppointers[index]->bodyAnimSpeed)
            {
                modelSetAnimSpeed(ppointers[index]->bodyModel, angle, 1.0f);
                ppointers[index]->bodyAnimSpeed = angle;
            }
 
            cur = ppointers[index]->players_cur_animation;
        }
 
        if (anim == cur)
        {
            if (firingtable != NULL)
            {
                chr->hidden &= ~CHRHIDDEN_0400;
                chrUpdateAimendbackShoulders(chr, firingtable, 0, 1, local8c);
            }
            else
            {
                chr->aimendrshoulder = 0.0f;
                chr->aimendlshoulder = 0.0f;
                chr->hidden |= CHRHIDDEN_0400;
                chr->aimendback = local8c;
            }
        }
 
        chr->aimendsideback = local88;
        chr->aimendcount = 10;
    }
 
    prop->pos.x = ppointers[index]->bond.collision_position.x;
    prop->pos.y = ppointers[index]->bond.collision_position.y;
    prop->pos.z = ppointers[index]->bond.collision_position.z;
    prop->stan = ppointers[index]->bond.current_tile_ptr;
 
    modelGetRootPosition(chr->model, &off);
    off.x = prop->pos.x;
    off.z = prop->pos.z;
    modelSetRootPosition(chr->model, &off);
    modelSetRootRotY(chr->model, (((360.0f - ppointers[index]->vv_theta) + ppointers[index]->bodyStrafeTurn) * M_TAU_F) / 360.0f);
 
    chr->chrflags |= CHRFLAG_INIT;
    chr->actiontype = ACT_BONDMULTI;
    chr->act_bondmulti.animfloats = firingtable;
 
    chrSetFiring(chr, GUNRIGHT, ppointers[index]->hands[GUNRIGHT].firing);
    chrSetFiring(chr, GUNLEFT, ppointers[index]->hands[GUNLEFT].firing);
 
    tailret = chrTick(prop);
 
    for (i = 0; i != 2; i++)
    {
        if (chrGetHandPos(chr, i, &ppointers[index]->bodyHandPos[i]) != 0)
        {
            ppointers[index]->bodyHandPosTick[i] = g_LvTickCount;
        }
        else if (ppointers[index]->bodyHandPosTick[i] < (g_LvTickCount - 1))
        {
            ppointers[index]->bodyHandPos[i].x = ppointers[index]->hands[i].muzzlePos.x;
            ppointers[index]->bodyHandPos[i].y = ppointers[index]->hands[i].muzzlePos.y;
            ppointers[index]->bodyHandPos[i].z = ppointers[index]->hands[i].muzzlePos.z;
        }
    }
 
    chr->hidden |= CHRHIDDEN_FREEZE;
 
    prop->pos.x = ppointers[index]->bond.collision_position.x;
    prop->pos.y = ppointers[index]->bond.collision_position.y;
    prop->pos.z = ppointers[index]->bond.collision_position.z;
    prop->stan = ppointers[index]->bond.current_tile_ptr;
 
    return tailret;
 
clear_and_return:
    prop->flags &= ~PROPFLAG_ONSCREEN;

    return TICKOP_NONE;
}


Gfx *playerRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    if (prop->chr != NULL)
    {
        gdl = chrRenderProp(prop, gdl, withalpha);
    }

    return gdl;
}


s32 lvGetMissionTime60(void) {
#ifdef VERSION_EU
    return (mission_timer * 60) / 50;
#else
    return mission_timer;
#endif
}


