/**
 * Most of the player. In file order: loading Bond's body, camera modes and the intro,
 * cutscene and death cameras, auto-aim, look and walk speeds, screen fades, input
 * (playerProcessInput), health and damage, the per-frame update (playerUpdate,
 * playerMove), the frame's camera setup (camSetupFrame) and the player prop tick
 * (playerTick). The HUD is in hud/hud.c and hud/hudmsg.c, the pause watch in watch.c,
 * respawning in respawn.c, counter-op in counterop.c, the tank in tank.c and moving
 * with collision in playermove.c. See docs/subsystems/player.md.
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
#include "game/player/bondview.h"
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

#ifdef VERSION_EU

    #define BONDVIEW_AUTOAIM_TIME 0x19 /* 25 */


    #define EU_CAMERA_8003642C_ASPECT 1.19047617912f

#else

    #define BONDVIEW_AUTOAIM_TIME 0x1e /* 30 */


#endif


#if defined(VERSION_EU)
    #define TANKUPDATEROTATION_SCALE 0.904799997807f
    #define TANKTURRETVERTICALANGLERELATED_SCALE 0.928399980068f
    #define TANK_Y_SMOOTHING 0.79960000515f
    #define CHR_OBJ_ACCEL_SPEED_FACTOR 0.6f
    #define CHR_OBJ_MAXSPEED 6.0f
    #define MAX_SPEED_FACTOR 0.8f
    #define TANK_DAMAGE_PENTALTY_TICKS 75

    #define TANK_VERT_ANGLE_FACTOR 0.0716000199318f
    #define TANK_VERT_ANGLE_RAD_FACTOR 0.0952f

    #define MAX_AIMLOCK_SPEED_DEFAULT 0.8344f

    #define THREE_SECOND_TICKS 150
    #define PLAYER_TICKEXPLODE_FACTOR 12

    #define CROUCH_DAMP 0.765100002289f
    #define CROUCH_DAMP_INV 0.234899997711f
    #define LANDING_DIP_FRAMES 12
    #define CROUCH_FALL_STEP -5.625f
#else
    #define TANKUPDATEROTATION_SCALE 0.92f
    #define TANKTURRETVERTICALANGLERELATED_SCALE 0.94f
    #define TANK_Y_SMOOTHING 0.83f
    #define CHR_OBJ_ACCEL_SPEED_FACTOR 0.5f
    #define CHR_OBJ_MAXSPEED 5.0f
    #define MAX_SPEED_FACTOR 0.8f
    #define TANK_DAMAGE_PENTALTY_TICKS 90

    #define TANK_VERT_ANGLE_FACTOR 0.0600000023842f
    #define TANK_VERT_ANGLE_RAD_FACTOR 0.0799999833107f
    #define MAX_AIMLOCK_SPEED_DEFAULT 0.86f

    #define THREE_SECOND_TICKS 180
    #define PLAYER_TICKEXPLODE_FACTOR 15

    #define CROUCH_DAMP 0.8f
    #define CROUCH_DAMP_INV 0.19999999f
    #define LANDING_DIP_FRAMES 15
    #define CROUCH_FALL_STEP -4.5f
#endif

#define SPEED_REGULAR_MAX  1.0f
#define SPEED_RUN_MAX      1.25f
#define SPEED_TICK_ADJUST  0.01f
#define TANK_MAX_SPEED     15.0f


#define FLOAT_TEN_A 10.0f
#define FLOAT_TEN_B 10.00f

#include "game/player/bondview_internal.h"
#include "game/asset/asset.h"
#include "game/gfx/dyn.h"
#include "game/hud/radar.h"
#include "game/prop/prop.h"
#include "game/replay/ramromreplay.h"
#include "game/prop/alarm.h"
#include "game/math/physics.h"



coord3d g_ForceBondMoveOffset;
s32 g_CoopForceMovePlayer = 0;

s32 g_SurroundBondWithExplosionsTicks;
s32 g_PlayerTickExplodeCreatePosition;

coord3d g_TankModelPositionOffset;

s32 g_TankEngineSfxVolume;

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

coord3d g_DeathCamPos;

struct PropRecord* g_DeathCamProp;

PadRecord * g_CameraLookAtBondPad;
CutsceneRecord *gBondViewCutscene;
f32 g_OrbitCamAngle;
f32 g_OrbitCamSpeed;
f32 g_OrbitCamDist;
f32 g_OrbitCamHeight;
f32 g_OrbitCamTargetHeight;
s32 g_OrbitCamPad;
enum CAMERAMODE g_CutsceneCamLookAtBond;
s32 g_CutsceneCamUnusedFlag;
s32 mission_timer;

#if defined(VERSION_EU)
f32 watch_time_0;
#else
s32 watch_time_0;
#endif


PadRecord *g_Startpad[0x10];

s32 startpadcount;


f32 g_MpSwirlRotateSpeed;
f32 g_MpSwirlAngleDegrees;
f32 g_MpSwirlForwardSpeed;
f32 g_MpSwirlDistance;

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


u32 playerGetCameraMode(void) {
    return g_CameraMode;
}


s32 playerPickDeathCamera(PropRecord *prop1, coord3d *pos, PropRecord *prop2, coord3d *collision_pos, StandTile *tile, f32 camera_dist)
{
    s32 found;
    s32 outertries;
    f32 camclearance;
    f32 dist;
    f32 angle;
    StandTile *checktile;
    coord3d dir;
    coord3d farpos;
    f32 angleRange;
    f32 frac;
    f32 floorY;
    s32 lineok;
    s32 angletries;

    g_DeathCamProp = prop1;

    found = 0;
    outertries = 0;
    angleRange = M_TAU_F;

    while ((outertries <= 0x80) && (!found))
    {
        camclearance = g_CurrentPlayer->bond.collision_radius;
        dist = 1500.0f + camclearance;

        angle = ((f32) randomGetNext()) * 2.3283064e-10f;
        angle = angle * angleRange;

        angletries = 0;

        while ((angletries < 0x10) && (!found))
        {
            angleRange = M_TAU_F;

            angle += 0.39269909f;

            if (angleRange <= angle)
            {
                angle -= angleRange;
            }

            dir.x = sinf(angle);
            dir.y = 0.0f;
            dir.z = cosf(angle);

            farpos.x = (((f32 *) (&dir))[0] * dist) + pos->x;
            farpos.y = pos->y;
            farpos.z = (((f32 *) (&dir))[2] * dist) + pos->z;

            stanResetHits();

            checktile = tile;

            propSetMoving(prop2, 0);

            lineok = stanTestLineUnobstructed(&checktile, collision_pos->x, collision_pos->z, farpos.x, farpos.z, 0x13, 0.0f, 1.0f, 0.0f, 1.0f);

            propSetMoving(prop2, 1);

            if (!lineok)
            {
                chrStanPointPointIntersection(collision_pos, &dir, &farpos);

                {
                    f32 dx = farpos.x - collision_pos->x;
                    f32 dz = farpos.z - collision_pos->z;

                    dist = sqrtf((dx * dx) + (dz * dz));
                }
            }

            dist -= camclearance;

            if (camera_dist <= dist)
            {
                frac = 1.0f;

                while ((0.0f < frac) && (!found))
                {
                    f32 spare;
                    f32 candidateDist;

                    spare = dist - camera_dist;

                    candidateDist = ((f32) randomGetNext()) * 2.3283064e-10f;
                    candidateDist *= spare;
                    candidateDist *= frac;
                    candidateDist += camera_dist;

                    g_DeathCamPos.x = (dir.x * candidateDist) + pos->x;
                    g_DeathCamPos.y = pos->y;
                    g_DeathCamPos.z = (dir.z * candidateDist) + pos->z;

                    checktile = tile;

                    propSetMoving(prop2, 0);

                    lineok = stanTestLineUnobstructed(&checktile, collision_pos->x, collision_pos->z, g_DeathCamPos.x, g_DeathCamPos.z, 0x13, 0.0f, 1.0f, 0.0f, 1.0f);

                    propSetMoving(prop2, 1);

                    if (lineok)
                    {
                        lineok = stanTestVolume(&checktile, g_DeathCamPos.x, g_DeathCamPos.z, camclearance, 0x1f, 0.0f, 1.0f);

                        if (lineok < 0)
                        {
                            floorY = stanGetFloorY(checktile, g_DeathCamPos.x, g_DeathCamPos.z);

                            g_DeathCamPos.y = (floorY + camclearance) + ((((f32) randomGetNext()) * 2.3283064e-10f) * (185.0f - camclearance));

                            candidateDist = g_DeathCamPos.y - pos->y;

                            if (((-1000.0f) < candidateDist) && (candidateDist < 1000.0f))
                            {
                                found = 1;
                            }
                        }
                    }

                    frac -= 0.25f;

                }
            }

            angletries++;
        }

        angleRange = M_TAU_F;

        outertries++;
    }

    return found;
}


/**
 * Co-op: the player a cutscene, the intro or the final death replay plays
 * from. Set by playerSetCameraMode from the current player.
 */
s32 g_CoopCinemaPlayer = 0;

static s32 coopIsCinemaCameraMode(s32 mode)
{
    switch (mode)
    {
        case CAMERAMODE_INTRO:
        case CAMERAMODE_FADESWIRL:
        case CAMERAMODE_SWIRL:
        case CAMERAMODE_DEATH_CAM_SP:
        case CAMERAMODE_DEATH_CAM_MP:
        case CAMERAMODE_POSEND:
            return TRUE;
    }

    return FALSE;
}

/**
 * Co-op: once per frame, before the players move, decide whether one view
 * fills the screen on its own (g_CoopSoloView). That is the case while a
 * player has their watch out, which pauses the game for both, and while the
 * camera plays a cutscene, the intro or the final death replay. The other
 * player is then neither moved nor drawn, and during a cutscene their body
 * is hidden as well.
 */
void coopUpdateSoloView(void)
{
    s32 count = getPlayerCount();
    s32 i;

    g_CoopSoloView = -1;
    g_CoopSoloViewIsCinema = FALSE;

    if (!g_CoopMode || (count < 2))
    {
        return;
    }

    for (i = 0; i < count; i++)
    {
        if (g_playerPointers[i]->watch_animation_state != WATCH_ANIMATION_0x0)
        {
            g_CoopSoloView = i;
            return;
        }
    }

    if (coopIsCinemaCameraMode(g_CameraMode) && (g_CoopCinemaPlayer >= 0) && (g_CoopCinemaPlayer < count))
    {
        g_CoopSoloView = g_CoopCinemaPlayer;
        g_CoopSoloViewIsCinema = TRUE;
    }
}

/**
 * Co-op: TRUE if another player has their watch out. Only one player can
 * pause at a time.
 */
static s32 coopPartnerHasWatchOut(void)
{
    s32 count = getPlayerCount();
    s32 i;

    if (!g_CoopMode)
    {
        return FALSE;
    }

    for (i = 0; i < count; i++)
    {
        if ((i != playerGetCurrentNum()) && (g_playerPointers[i]->watch_animation_state != WATCH_ANIMATION_0x0))
        {
            return TRUE;
        }
    }

    return FALSE;
}

void playerSetCameraMode(s32 mode)
{

    if (g_CoopMode && coopIsCinemaCameraMode(mode))
    {
        g_CoopCinemaPlayer = playerGetCurrentNum();
    }

    g_CameraMode = mode;
    g_CameraAfterCinema = 0;

    if (g_CameraMode == CAMERAMODE_INTRO)
    {
        if ((g_IntroCamera != NULL) && (get_recording_ramrom_flag() == 0) && (get_is_ramrom_flag() == 0))
        {
            camera_transition_timer = 0.0f;
            currentPlayerSetFadeColour(0, 0, 0, 1.0f);
            currentPlayerSetFadeFrac(60.0f, 0.0f);
            fogLoadLevelEnvironment(bossGetStageNum(), 1);
            g_CurrentPlayer->cameratile = NULL;
        }
        else
        {
            playerSetCameraMode(CAMERAMODE_SWIRL);
        }
    }
    else if (g_CameraMode == CAMERAMODE_FADESWIRL)
    {
        currentPlayerSetFadeColour(0, 0, 0, 0.0f);
        currentPlayerSetFadeFrac(60.0f, 1.0f);
    }
    else if (g_CameraMode == CAMERAMODE_MP)
    {
        g_MpSwirlRotateSpeed = 0.0f;
        g_MpSwirlAngleDegrees = -90.0f;
        g_MpSwirlForwardSpeed = 0.0f;
        g_MpSwirlDistance = 80.0f;
        fogLoadLevelEnvironment(bossGetStageNum(), 0);
    }
    else if (g_CameraMode == CAMERAMODE_SWIRL)
    {
        struct ModelAnimation *anim;
        f32 endframe;
        f32 ftemp_3;
        f32 ftemp_1;
        struct ChrRecord *chr;

        camera_fade_active = 0;
        currentPlayerSetFadeColour(0, 0, 0, 1.0f);
        currentPlayerSetFadeFrac(60.0f, 0.0f);
        fogLoadLevelEnvironment(bossGetStageNum(), 0);

        if ((g_IntroSwirl != 0) && (get_recording_ramrom_flag() == 0) && (get_is_ramrom_flag() == 0))
        {
            camera_transition_timer = 0.0f;
            intro_camera_index = CAMERAMODE_INTRO;
            currentPlayerStartChrFade(0.0f, 1.0f);
            playerLoadBody();

            // HACK: g_AnimTable->data regalloc is backwards
            anim = (struct ModelAnimation *)((s32)stage_intro_anim_table[g_IntroAnimationIndex].anonymous_0 + (s32)&g_AnimTable->data);
            endframe = stage_intro_anim_table[g_IntroAnimationIndex].anonymous_2;
            ftemp_1 = stage_intro_anim_table[g_IntroAnimationIndex].anonymous_1;
            ftemp_3 = stage_intro_anim_table[g_IntroAnimationIndex].anonymous_3;

            modelSetAnimation(
                g_CurrentPlayer->bodyModel,
                anim,
                0,
                ftemp_1,
                ftemp_3,
                0.0f);

            if (endframe > 0.0f)
            {
                modelSetAnimEndFrame(g_CurrentPlayer->bodyModel, endframe);
            }

            chr = g_CurrentPlayer->prop->chr;
            chr->actiontype = ACT_BONDINTRO;
            chr->sleep = 0;
            g_CurrentPlayer->cameratile = NULL;
        }
        else
        {
            playerSetCameraMode(CAMERAMODE_FP);
        }
    }
    else if (g_CameraMode == CAMERAMODE_FP)
    {
        if (bossGetStageNum() == LEVELID_CUBA)
        {
            currentPlayerSetFadeColour(0, 0, 0, 1.0f);
            currentPlayerSetFadeFrac(0.0f, 1.0f);
        }
        else if (camera_fade_active != 0)
        {
            currentPlayerSetFadeColour(0, 0, 0, 1.0f);
            currentPlayerSetFadeFrac(60.0f, 0.0f);
        }

        if (isMpGame())
        {
            fogLoadLevelEnvironment(bossGetStageNum(), 0);
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
        {
            gunEquip(GUNLEFT, starting_weapon[GUNLEFT]);
            gunEquip(GUNRIGHT, starting_weapon[GUNRIGHT]);
        }

        stop_time_flag = 0;
    }
    else if (g_CameraMode == CAMERAMODE_DEATH_CAM_SP)
    {
        f32 camdist;
        PropRecord *focusprop;
        coord3d focuspos;
        StandTile *focustile;
        coord3d collisionpos;
        PropRecord *collisionprop;
        struct ChrRecord *chr;

        camera_transition_timer = 0.0f;
        intro_camera_index = CAMERAMODE_INTRO;
        currentPlayerSetFadeColour(0, 0, 0, 1.0f);
        currentPlayerSetFadeFrac(60.0f, 0.0f);

        if (g_ExplodeTankOnDeathFlag && (g_PlayerTankProp != NULL))
        {
            // removed
        }
        else
        {
            // This branch restarts Bond's death animation for his death replay
            g_PlayerIsInTank = 0;

            // struct copy
            g_CurrentPlayer->bond = g_CurrentPlayer->bondDie;

            g_CurrentPlayer->vv_theta = g_CurrentPlayer->thetadie;
            g_CurrentPlayer->vv_verta = g_CurrentPlayer->vertadie;
            g_CurrentPlayer->prop->pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
            g_CurrentPlayer->prop->pos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
            g_CurrentPlayer->prop->pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2];
            g_CurrentPlayer->prop->stan = g_CurrentPlayer->bond.current_tile_ptr;

            playerUpdateVertaTheta();
            playerTickMoveAnim(0, 0, 0);
            playerUpdatePosFromCollision();
            currentPlayerStartChrFade(0.0f, 1.0f);
            playerLoadBody();

            modelSetAnimation(
                g_CurrentPlayer->bodyModel,
                modelGetAnim((Model *) &g_CurrentPlayer->model),
                modelGetAnimFlip(&g_CurrentPlayer->model),
                0.0f,
                0.5f,
                0.0f);

            chr = g_CurrentPlayer->prop->chr;
            chr->actiontype = ACT_BONDDIE;
            chr->sleep = 0;
            chr->chrflags |= CHRFLAG_INIT;

            modelSetRootPosition(g_CurrentPlayer->bodyModel, &g_CurrentPlayer->prop->pos);
            camdist = bondviewGetPlayerYawRadians();
            modelSetRootRotY(g_CurrentPlayer->bodyModel, camdist);
        }

        if (g_ExplodeTankOnDeathFlag && (g_PlayerTankProp != NULL))
        {
            focusprop = g_PlayerTankProp;
            camdist = 500.0f; // distance to place the camera
            focuspos.f[0] = g_PlayerTankProp->pos.f[0];
            focuspos.f[1] = g_PlayerTankProp->pos.f[1];
            focuspos.f[2] = g_PlayerTankProp->pos.f[2];
            collisionprop = g_PlayerTankProp;
            collisionpos.f[0] = g_PlayerTankProp->pos.f[0];
            collisionpos.f[1] = g_PlayerTankProp->pos.f[1];
            collisionpos.f[2] = g_PlayerTankProp->pos.f[2];
            focustile = g_PlayerTankProp->stan;
        }
        else
        {
            camdist = 200.0f; // distance to place the camera
            focusprop = g_CurrentPlayer->prop;
            focuspos.f[0] = g_CurrentPlayer->dampEye.x;
            focuspos.f[1] = g_CurrentPlayer->dampEye.y;
            focuspos.f[2] = g_CurrentPlayer->dampEye.z;
            collisionprop = g_CurrentPlayer->prop;
            collisionpos.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
            collisionpos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
            collisionpos.f[2] = g_CurrentPlayer->bond.collision_position.f[2];
            focustile = g_CurrentPlayer->bond.current_tile_ptr;
        }

        if (playerPickDeathCamera(focusprop, &focuspos, collisionprop, &collisionpos, focustile, camdist) != 0)
        {
            if (camera_mode == 0)
            {

                musicTrack1Play(M_INTROSWOOSH);
                sndSetScalerApplyVolumeAllSfxSlot(0.5f);
            }

            if ((g_ExplodeTankOnDeathFlag != 0) && (g_PlayerTankProp != NULL))
            {
                explosionCreate(g_PlayerTankProp, &g_PlayerTankProp->pos, g_PlayerTankProp->stan, EXPLOSION_DEF_STANDARD, 0, playerGetCurrentNum(), g_PlayerTankProp->rooms, 0);
            }
        }
        else
        {
            // playerPickDeathCamera has returned 0. This happens when no possible angles were found
            // to place the camera at the requested distance or when the three death replays are finished.
            bossRunTitleStage();
        }
    }
    else if (g_CameraMode == CAMERAMODE_DEATH_CAM_MP)
    {
        currentPlayerSetFadeColour(0, 0, 0, 0.0f);
        currentPlayerSetFadeFrac(60.0f, 1.0f);
    }
    else if (g_CameraMode == CAMERAMODE_POSEND)
    {
        playerLoadBody();
        g_CurrentPlayer->cameratile = NULL;
    }
    else if (g_CameraMode == CAMERAMODE_FP_NOINPUT)
    {
        playerRemoveBody();
        g_CameraMode = CAMERAMODE_FP;
    }
    else if (g_CameraMode == CAMERAMODE_FADE_TO_TITLE)
    {
        s32 i;

        for (i = 0; i < getPlayerCount(); i++)
        {
            playerSetCurrent(i);
            currentPlayerSetFadeColour(0, 0, 0, 0.0f);
            currentPlayerSetFadeFrac(60.0f, 1.0f);
        }

        playerSetCurrent(0);
    }
}


void playerAdvanceCameraMode(void)
{
    enum CAMERAMODE mode = g_CameraMode;

    g_CameraMode = CAMERAMODE_NONE;
    g_CameraAfterCinema = CAMERAMODE_NONE;

    if (mode == CAMERAMODE_INTRO)
    {
        playerSetCameraMode(CAMERAMODE_FADESWIRL);
    }
    else if (mode == CAMERAMODE_FADESWIRL)
    {
        hudmsgResetIntroText();
        playerSetCameraMode(CAMERAMODE_SWIRL);
    }
    else if (mode != CAMERAMODE_MP)
    {
        if (mode == CAMERAMODE_SWIRL)
        {
            playerRemoveBody();
            currentPlayerStartChrFade(0.0f, 1.0f); // What's the point of this call?
            playerSetCameraMode(CAMERAMODE_FP);
        }
        else if (mode != CAMERAMODE_FP)
        {
            if (mode == CAMERAMODE_DEATH_CAM_SP)
            {
                playerSetCameraMode(CAMERAMODE_DEATH_CAM_MP);
            }
            else if (mode == CAMERAMODE_DEATH_CAM_MP)
            {
                camera_mode++;

                if (camera_mode < CAMERAMODE_SWIRL)
                {
                    playerSetCameraMode(CAMERAMODE_DEATH_CAM_SP);
                }
            }
        }
    }
}


/**
 * Smoothly interpolate the camera between the points on the intro swirl path.
 */
void playerCalcIntroSwirlCamera(s32 index, f32 time, coord3d *pos, coord3d *lookat)
{
    struct SetupIntroSwirl *base;
    struct SetupIntroSwirl *loopbase;
    // four coord3d control points are written and read (pointbuf[9..11] is the last)
    f32 pointbuf[12];
    struct SetupIntroSwirl *swirl;
    f32 frac;
    f32 *dst;

    base = g_IntroSwirl;
    swirl = base;
    swirl += index;
    frac = 0.0f;

    if (swirl->duration.fval > 0.0f)
    {
        frac = time / swirl->duration.fval;
    }

    {
        struct SetupIntroSwirl *entry;
        union {
            struct SetupIntroSwirl *swirl;
            struct player *player;
        } target;
        s32 i;

        loopbase = base + (u32) index;

        for (i = -1; i < 3; i++)
        {
            entry = loopbase;
            loopbase = base + (u32) index;
            target.swirl = entry + i;
            dst = &pointbuf[i * 3];

            if (i < 0)
            {
                if (target.swirl < base)
                {
                    entry = base;
                }
                else
                {
                    entry = target.swirl;
                }
            }
            else
            {
                while (entry < target.swirl)
                {
                    if (entry[1].bitflags & 1)
                    {
                        break;
                    }

                    entry++;
                }
            }

            if (entry->bitflags & 2)
            {
                target.player = g_CurrentPlayer;
                dst[3] = (entry->offsetfromBond[2].fval * target.player->bond.theta_transform.f[0])
                    + (entry->offsetfromBond[0].fval * target.player->bond.theta_transform.f[2]);
                dst[4] = entry->offsetfromBond[1].fval;
                dst[5] = (entry->offsetfromBond[2].fval * target.player->bond.theta_transform.f[2])
                    - (entry->offsetfromBond[0].fval * target.player->bond.theta_transform.f[0]);
            }
            else
            {
                dst[3] = entry->offsetfromBond[0].fval;
                dst[4] = entry->offsetfromBond[1].fval;
                dst[5] = entry->offsetfromBond[2].fval;
            }
        }
    }

    {
        f32 scale;

        base = swirl;
        scale = base->scale.fval;
        base = (void *)(index << 5);

        interpCardinal3((coord3d *) &pointbuf[0], (coord3d *) &pointbuf[3], (coord3d *) &pointbuf[6], (coord3d *) &pointbuf[9], frac, scale, pos);

        pos->x += g_CurrentPlayer->dampEye.x;
        pos->y += g_CurrentPlayer->dampEye.y;
        pos->z += g_CurrentPlayer->dampEye.z;

        lookat->x = g_CurrentPlayer->dampEye.x;
        lookat->y = g_CurrentPlayer->dampEye.y;
        lookat->z = g_CurrentPlayer->dampEye.z;

        swirl = (void *)(((u32) g_IntroSwirl) + (u32) base);

        if (!(swirl->bitflags & 4))
        {
            if (!(swirl[1].bitflags & 4))
            {
                scale = 1.0f;
            }
            else
            {
                scale = 1.0f - frac;
            }
        }
        else if (swirl[1].bitflags & 4)
        {
            scale = 0.0f;
        }
        else
        {
            scale = frac;
        }

        lookat->x += (g_CurrentPlayer->bond.applied_view.x * 40.0f) * scale;
        lookat->y += (g_CurrentPlayer->bond.applied_view.y * 40.0f) * scale;
        lookat->z += (g_CurrentPlayer->bond.applied_view.z * 40.0f) * scale;
    }
}


/**
 * US address 7F07B56C.
 * JP address 7F07BB8C.
 * EU address 7F07B604.
*/
void playerTickFrozenCamera(u16 buttons, u16 oldbuttons, coord3d *pos, coord3d *pos2, StandTile **stan, coord3d *lookat)
{
    s32 i;
    f32 swirlangle;
    s32 i2;
    f32 introtime = 0.0f;
    void *p;
    struct PadRecord *setupPad;

    if ((g_CameraMode == CAMERAMODE_INTRO) || (g_CameraMode == CAMERAMODE_FADESWIRL))
    {
        if (g_CameraMode == CAMERAMODE_INTRO)
        {
            if ((camera_transition_timer < 120.0f) && ((camera_transition_timer + g_GlobalTimerDelta) >= 120.0f))
            {
#if defined(VERSION_US)
                hudmsgSetFonts(g_FontZurichChars, g_FontZurich);
                hudmsgBottomShow(g_IntroCamera->lang1c.lang_ptr);
#else
                hudmsgBottomShow(g_IntroCamera->lang1c.lang_ptr, g_FontZurichChars, g_FontZurich);
#endif
            }

            if (g_IntroCamera->lang20.lang_ptr != NULL)
            {
                if ((camera_transition_timer < 300.0f) && ((camera_transition_timer + g_GlobalTimerDelta) >= 300.0f))
                {
#if defined(VERSION_US)
                    hudmsgBottomShow(g_IntroCamera->lang20.lang_ptr);
#else
                    hudmsgBottomShow(g_IntroCamera->lang20.lang_ptr, g_FontZurichChars, g_FontZurich);
#endif
                }

                if (camera_transition_timer > 480.0f)
                {
                    g_CameraAfterCinema = CAMERAMODE_INTRO;
                }
            }
            else if (camera_transition_timer > 300.0f)
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }

            camera_transition_timer += g_GlobalTimerDelta;

            if ((lvGetControlsLocked() == 0)
                && (buttons & ~oldbuttons & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON | CONT_R | CONT_L)))
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }
        }
        else if (g_CurrentPlayer->colourfadetimemax60 < 0.0f)
        {
            g_CameraAfterCinema = CAMERAMODE_INTRO;
        }

        pos->f[0] = g_IntroCamera->x.fval;
        pos->f[1] = g_IntroCamera->y.fval;
        pos->f[2] = g_IntroCamera->z.fval;

        pos2->f[0] = pos->f[0] + (cosf(g_IntroCamera->pitch.fval) * sinf(g_IntroCamera->yaw.fval));
        pos2->f[1] = pos->f[1] + sinf(g_IntroCamera->pitch.fval);
        pos2->f[2] = pos->f[2] - (cosf(g_IntroCamera->pitch.fval) * cosf(g_IntroCamera->yaw.fval));

        p = &g_CurrentSetup.pads[g_IntroCamera->pad];
        setupPad = p;
        *stan = setupPad->stan;

        lookat->f[0] = setupPad->pos.f[0];
        lookat->f[1] = setupPad->pos.f[1];
        lookat->f[2] = setupPad->pos.f[2];
    }
    else if (g_CameraMode == CAMERAMODE_MP)
    {
        /**
        * CAMERAMODE_MP: Perfect Dark method playerTickMpSwirl
        */
        if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
        {
            for (i2=0; i2<g_ClockTimer; i2++)
            {
                if (g_MpSwirlAngleDegrees < 179.5f)
                {
                    if (g_MpSwirlAngleDegrees < -20.0f)
                    {
                        g_MpSwirlRotateSpeed += 0.1f;
                    }

                    if (g_MpSwirlAngleDegrees > 110.0f)
                    {
                        g_MpSwirlRotateSpeed -= 0.1f;
                    }

                    g_MpSwirlAngleDegrees += g_MpSwirlRotateSpeed;
                }
#if defined(VERSION_EU)
                if (g_MpSwirlAngleDegrees >= 179.5f)
#else
                else
#endif
                {
                    g_MpSwirlAngleDegrees = 180.0f;
                }

                if (g_MpSwirlAngleDegrees > 80.0f)
                {
                    if (g_MpSwirlDistance > 60.0f)
                    {
                        g_MpSwirlForwardSpeed -= 0.1f;
                    }
                    else
                    {
                        g_MpSwirlForwardSpeed += 0.015f;
                    }

                    g_MpSwirlDistance += g_MpSwirlForwardSpeed;

                    if (g_MpSwirlDistance < 1.0f)
                    {
                        g_MpSwirlDistance = 1.0f;
                    }
                }
            }
        }

        swirlangle = ((g_MpSwirlAngleDegrees - g_CurrentPlayer->vv_theta) * M_PI_F) / 180.0f;

        pos->f[0] = g_CurrentPlayer->bond.pos.f[0] + (sinf(swirlangle) * g_MpSwirlDistance);
        pos->f[1] = g_CurrentPlayer->bond.pos.f[1] + (g_MpSwirlDistance * 0.08f);
        pos->f[2] = g_CurrentPlayer->bond.pos.f[2] + (cosf(swirlangle) * g_MpSwirlDistance);

        pos2->f[0] = g_CurrentPlayer->bond.pos.f[0];
        pos2->f[1] = g_CurrentPlayer->bond.pos.f[1];
        pos2->f[2] = g_CurrentPlayer->bond.pos.f[2];

        *stan = g_CurrentPlayer->prop->stan;

        lookat->f[0] = g_CurrentPlayer->bond.pos.f[0];
        lookat->f[1] = g_CurrentPlayer->bond.pos.f[1] + (g_MpSwirlDistance * 0.08f);
        lookat->f[2] = g_CurrentPlayer->bond.pos.f[2];

        if (((playerGetShuffledIndex(playerGetCurrentNum()) + 1) == getPlayerCount()) && (g_MpSwirlDistance < 5.0f))
        {
            g_CameraAfterCinema = CAMERAMODE_INTRO;
        }
    }
    else if (g_CameraMode == CAMERAMODE_SWIRL)
    {
        camera_transition_timer += g_GlobalTimerDelta;

        while (g_IntroSwirl[intro_camera_index].duration.fval <= camera_transition_timer)
        {
            if (!(g_IntroSwirl[intro_camera_index + 3].bitflags & 1))
            {
                camera_transition_timer -= g_IntroSwirl[intro_camera_index].duration.fval;
                intro_camera_index++;
            }
            else
            {
                camera_transition_timer = g_IntroSwirl[intro_camera_index].duration.fval;
                g_CameraAfterCinema = CAMERAMODE_INTRO;
                break;
            }
        }

        introtime += (g_IntroSwirl[intro_camera_index].duration.fval - camera_transition_timer);

        for (i = intro_camera_index + 1; !(g_IntroSwirl[i+2].bitflags & 1); i++)
        {
            introtime += g_IntroSwirl[i].duration.fval;
        }

        // Fade player body from opaque to transparent just before the player takes control.
        if ((introtime < 30.0f) && ((introtime + g_GlobalTimerDelta) >= 30.0f))
        {
            currentPlayerStartChrFade(30.0f, 0.0f);
        }

        if (camera_fade_active != 0)
        {
            if (currentPlayerIsFadeComplete() != 0)
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }
        }

        if ((introtime > 60.0f) && (camera_fade_active == 0))
        {
            if ((lvGetControlsLocked() == 0)
                && (buttons & ~oldbuttons & (A_BUTTON | B_BUTTON | Z_TRIG | START_BUTTON | L_TRIG | R_TRIG)))
            {
                camera_fade_active = 1;
                currentPlayerSetFadeColour(0, 0, 0, g_CurrentPlayer->colourscreenfrac);

                if (currentPlayerIsFadeComplete() != 0)
                {
                    currentPlayerSetFadeFrac(60.0f, 1.0f);
                }
                else
                {
                    currentPlayerSetFadeFrac(g_CurrentPlayer->colourfadetime60, 1.0f);
                }
            }
        }

        playerCalcIntroSwirlCamera(intro_camera_index, camera_transition_timer, pos, pos2);

        if (g_IntroSwirl[intro_camera_index].pad >= 0)
        {
            p = &g_CurrentSetup.pads[g_IntroSwirl[intro_camera_index].pad];
            setupPad = p;
            *stan = setupPad->stan;

            lookat->f[0] = setupPad->pos.f[0];
            lookat->f[1] = setupPad->pos.f[1];
            lookat->f[2] = setupPad->pos.f[2];
        }
        else
        {
            *stan = g_CurrentPlayer->bond.current_tile_ptr;
            lookat->f[0] = g_CurrentPlayer->bond.collision_position.f[0];
            lookat->f[1] = g_CurrentPlayer->bond.collision_position.f[1];
            lookat->f[2] = g_CurrentPlayer->bond.collision_position.f[2];
        }
    }
    else if ((g_CameraMode == CAMERAMODE_DEATH_CAM_SP) || (g_CameraMode == CAMERAMODE_DEATH_CAM_MP))
    {
        if (g_CameraMode == CAMERAMODE_DEATH_CAM_SP)
        {
            camera_transition_timer += g_GlobalTimerDelta;

            if (g_CurrentPlayer->bodyModel != NULL)
            {
                if (modelGetAnimFrame((Model *) g_CurrentPlayer->bodyModel)
                    >= modelGetAnimEndFrame((Model *) g_CurrentPlayer->bodyModel))
                {
                    g_CameraAfterCinema = CAMERAMODE_INTRO;
                }
            }
            else if (camera_transition_timer >= 180.0f)
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }

            if ((buttons & ~oldbuttons & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON))
                && (g_CurrentPlayer->bonddead)
                && (g_CurrentPlayer->redbloodfinished)
                && (g_CurrentPlayer->deathanimfinished))
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
                camera_mode = CAMERAMODE_FADESWIRL;
            }
        }
        else if (g_CameraMode == CAMERAMODE_DEATH_CAM_MP)
        {
            camera_transition_timer += g_GlobalTimerDelta;

            if (g_CurrentPlayer->colourfadetimemax60 < 0.0f)
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }

            if ((buttons & ~oldbuttons & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON))
                && (g_CurrentPlayer->bonddead)
                && (g_CurrentPlayer->redbloodfinished)
                && (g_CurrentPlayer->deathanimfinished))
            {
                camera_mode = CAMERAMODE_FADESWIRL;
            }
        }

        pos->f[0] = g_DeathCamPos.f[0];
        pos->f[1] = g_DeathCamPos.f[1];
        pos->f[2] = g_DeathCamPos.f[2];

        if (g_DeathCamProp == g_CurrentPlayer->prop)
        {
            pos2->f[0] = g_CurrentPlayer->dampEye.x;
            pos2->f[1] = g_CurrentPlayer->dampEye.y;
            pos2->f[2] = g_CurrentPlayer->dampEye.z;
        }
        else
        {
            pos2->f[0] = g_DeathCamProp->pos.f[0];
            pos2->f[1] = g_DeathCamProp->pos.f[1];
            pos2->f[2] = g_DeathCamProp->pos.f[2];
        }

        *stan = g_CurrentPlayer->bond.current_tile_ptr;

        lookat->f[0] = g_CurrentPlayer->bond.collision_position.f[0];
        lookat->f[1] = g_CurrentPlayer->bond.collision_position.f[1];
        lookat->f[2] = g_CurrentPlayer->bond.collision_position.f[2];
    }
    else if (g_CameraMode == CAMERAMODE_POSEND)
    {
        if (g_CameraLookAtBondPad != NULL)
        {
            pos->f[0] = g_CameraLookAtBondPad->pos.f[0];
            pos->f[1] = g_CameraLookAtBondPad->pos.f[1];
            pos->f[2] = g_CameraLookAtBondPad->pos.f[2];

            pos2->f[0] = g_CurrentPlayer->dampEye.x;
            pos2->f[1] = g_CurrentPlayer->dampEye.y;
            pos2->f[2] = g_CurrentPlayer->dampEye.z;

            *stan = g_CameraLookAtBondPad->stan;

            lookat->f[0] = g_CameraLookAtBondPad->pos.f[0];
            lookat->f[1] = g_CameraLookAtBondPad->pos.f[1];
            lookat->f[2] = g_CameraLookAtBondPad->pos.f[2];

            return;
        }

        if (gBondViewCutscene != NULL)
        {
            if (isNotBoundPad(gBondViewCutscene->pad))
            {
                p = &g_CurrentSetup.pads[gBondViewCutscene->pad];
            }
            else
            {
                p = &g_CurrentSetup.boundpads[getBoundPadNum(gBondViewCutscene->pad)];
            }

            pos->f[0] = gBondViewCutscene->pos.f[0];
            pos->f[1] = gBondViewCutscene->pos.f[1];
            pos->f[2] = gBondViewCutscene->pos.f[2];

            setupPad = p;
            *stan = setupPad->stan;

            lookat->f[0] = setupPad->pos.f[0];
            lookat->f[1] = setupPad->pos.f[1];
            lookat->f[2] = setupPad->pos.f[2];

            if (g_CutsceneCamLookAtBond == CAMERAMODE_INTRO)
            {
                pos2->f[0] = g_CurrentPlayer->dampEye.x;
                pos2->f[1] = g_CurrentPlayer->dampEye.y;
                pos2->f[2] = g_CurrentPlayer->dampEye.z;
            }
            else
            {
                pos2->f[0] = pos->f[0] + (cosf(gBondViewCutscene->verta) * sinf(gBondViewCutscene->theta));
                pos2->f[1] = pos->f[1] + sinf(gBondViewCutscene->verta);
                pos2->f[2] = pos->f[2] - (cosf(gBondViewCutscene->verta) * cosf(gBondViewCutscene->theta));
            }

            return;
        }

        if (isNotBoundPad(g_OrbitCamPad))
        {
            setupPad = &g_CurrentSetup.pads[g_OrbitCamPad];
        }
        else
        {
            setupPad = (struct PadRecord*)&g_CurrentSetup.boundpads[getBoundPadNum(g_OrbitCamPad)];
        }

        *stan = setupPad->stan;

        lookat->f[0] = setupPad->pos.f[0];
        lookat->f[1] = setupPad->pos.f[1];
        lookat->f[2] = setupPad->pos.f[2];

        pos2->f[0] = setupPad->pos.f[0] + cosf(g_OrbitCamAngle) * 0.0f;
        pos2->f[1] = setupPad->pos.f[1] + g_OrbitCamTargetHeight;
        pos2->f[2] = setupPad->pos.f[2] + sinf(g_OrbitCamAngle) * 0.0f;

        pos->f[0] = setupPad->pos.f[0] + (sinf(g_OrbitCamAngle) * g_OrbitCamDist) + cosf(g_OrbitCamAngle) * 0.0f;
        pos->f[1] = setupPad->pos.f[1] + g_OrbitCamTargetHeight + g_OrbitCamHeight;
        pos->f[2] = setupPad->pos.f[2] + (cosf(g_OrbitCamAngle) * g_OrbitCamDist) + sinf(g_OrbitCamAngle) * 0.0f;

        g_OrbitCamAngle += g_OrbitCamSpeed * g_GlobalTimerDelta;

        while (g_OrbitCamAngle >= M_TAU_F)
        {
            g_OrbitCamAngle -= M_TAU_F;
        }

        while (g_OrbitCamAngle < 0.0f)
        {
            g_OrbitCamAngle += M_TAU_F;
        }
    }
}


void currentPlayerSetLookAheadSetting(bool enabled)
{
    g_CurrentPlayer->automovecentreenabled = enabled;
}


void currentPlayerSetYAutoAimEnabled(bool enabled)
{
  g_CurrentPlayer->autoyaimenabled = enabled;
}


bool currentPlayerGetYAutoAimEnabled(void)
{
    if (!isMpGame())
    {
        return g_CurrentPlayer->autoyaimenabled;
    }

    return (bool) g_playerPerm->autoaim;
}


bool currentPlayerGetYAutoAimEnabledRedirect(void)
{
    return currentPlayerGetYAutoAimEnabled();
}


bool currentPlayerGetIsAiming(void)
{
  return g_CurrentPlayer->insightaimmode;
}


/**
 * Updates autoyaimtime60 by g_ClockTimer.
 * Will update player->autoaimy if new autoyaimtime60 < 0 or autoaim_target != g_CurrentPlayer->autoaim_target_y.
 */
void playerUpdateAutoAimY(struct PropRecord *autoaim_target, f32 auto_aim_y)
{
    if (g_CurrentPlayer->autoyaimtime60 >= 0)
    {
        g_CurrentPlayer->autoyaimtime60 = g_CurrentPlayer->autoyaimtime60 - g_ClockTimer;
    }

    if (autoaim_target != g_CurrentPlayer->autoaim_target_y)
    {
        if (g_CurrentPlayer->autoyaimtime60 < 0)
        {
            g_CurrentPlayer->autoyaimtime60 = BONDVIEW_AUTOAIM_TIME;
            g_CurrentPlayer->autoaim_target_y = autoaim_target;
        }
        else
        {
            return;
        }
    }

    g_CurrentPlayer->autoaimy = auto_aim_y;
}


void currentPlayerSetXAutoAimEnabled(bool enabled)
{
  g_CurrentPlayer->autoxaimenabled = enabled;

  return;
}


bool currentPlayerGetXAutoAimEnabled(void)
{
    if (!isMpGame())
    {
        return g_CurrentPlayer->autoxaimenabled;
    }

    return (bool) g_playerPerm->autoaim;
}


bool currentPlayerGetXAutoAimEnabledRedirect(void)
{
    return currentPlayerGetXAutoAimEnabled();
}


/**
 * Updates autoxaimtime60 by g_ClockTimer.
 * Will update player->autoaimx if new autoxaimtime60 < 0 or autoaim_target_x != g_CurrentPlayer->autoaim_target_x.
 */
void playerUpdateAutoAimX(struct PropRecord *autoaim_target, f32 auto_aim_x)
{
    if (g_CurrentPlayer->autoxaimtime60 >= 0)
    {
        g_CurrentPlayer->autoxaimtime60 = g_CurrentPlayer->autoxaimtime60 - g_ClockTimer;
    }

    if (autoaim_target != g_CurrentPlayer->autoaim_target_x)
    {
        if (g_CurrentPlayer->autoxaimtime60 < 0)
        {
            g_CurrentPlayer->autoxaimtime60 = BONDVIEW_AUTOAIM_TIME;
            g_CurrentPlayer->autoaim_target_x = autoaim_target;
        }
        else
        {
            return;
        }
    }

    g_CurrentPlayer->autoaimx = auto_aim_x;

}


/**
 * US address 7F07FCC4.
 * Perfect Dark bwalkUpdateSpeedSideways.
*/
void playerUpdateSpeedSideways(s32 dir) {
    if (dir == -1) {
        g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe - g_GlobalTimerDelta);
        if (g_CurrentPlayer->speedstrafe < -1.0f) {
            g_CurrentPlayer->speedstrafe = -1.0f;
        }
    }
    else {
        if (dir == 1) {
            g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe + g_GlobalTimerDelta);
            if (1.0f < g_CurrentPlayer->speedstrafe) {
                g_CurrentPlayer->speedstrafe = 1.0f;
            }
        } else {
            if (0.0f < g_CurrentPlayer->speedstrafe) {
                g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe - g_GlobalTimerDelta);
                if (g_CurrentPlayer->speedstrafe < 0.0f) {
                    g_CurrentPlayer->speedstrafe = 0.0f;
                }
            } else {
                g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe + g_GlobalTimerDelta);
                if (0.0f < g_CurrentPlayer->speedstrafe) {
                    g_CurrentPlayer->speedstrafe = 0.0f;
                }
            }
        }
    }
    g_CurrentPlayer->speedsideways = g_CurrentPlayer->speedstrafe;
}

/**
 * US address 7F07FE1C.
 * Perfect Dark bwalkUpdateSpeedForwards.
*/
void playerUpdateSpeedForwards(s32 dir) {
    if (dir == 1) {
        g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo + g_GlobalTimerDelta);
        if (1.0f < g_CurrentPlayer->speedgo) {
            g_CurrentPlayer->speedgo = 1.0f;
        }
    } else {
        if (dir == -1) {
            g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo - g_GlobalTimerDelta);
            if (g_CurrentPlayer->speedgo < -1.0f) {
                g_CurrentPlayer->speedgo = -1.0f;
            }
        } else {
            if (0.0f < g_CurrentPlayer->speedgo) {
                g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo - g_GlobalTimerDelta);
                if (g_CurrentPlayer->speedgo < 0.0f) {
                    g_CurrentPlayer->speedgo = 0.0f;
                }
            } else {
                g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo + g_GlobalTimerDelta);
                if (0.0f < g_CurrentPlayer->speedgo) {
                    g_CurrentPlayer->speedgo = 0.0f;
                }
            }
        }
    }
    g_CurrentPlayer->speedforwards = g_CurrentPlayer->speedgo;
}

/**
 * US address 7F07FF74.
 * Duplicate of playerScaleLookSpeedTheta.
*/
f32 playerScaleLookSpeedVerta(f32 value) {
    if (value > 0) {
        return (viGetFovY() * value * -0.7f) / FOV_Y_F;
    }

    if (value < 0) {
        return (viGetFovY() * -value * 0.7f) / FOV_Y_F;
    }

    return 0;
}


void playerUpdateSpeedVerta(f32 value)
{
    f32 mult = viGetFovY() / FOV_Y_F;
    f32 limit = playerScaleLookSpeedVerta(value);

    if (value > 0.0f)
    {
        if (g_CurrentPlayer->speedverta > 0.0f)
        {
            g_CurrentPlayer->speedverta -= (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedverta -= (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedverta < limit)
        {
            g_CurrentPlayer->speedverta = limit;
        }
    }
    else if (value < 0.0f)
    {
        if (g_CurrentPlayer->speedverta < 0.0f)
        {
            g_CurrentPlayer->speedverta += (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedverta += (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedverta > limit)
        {
            g_CurrentPlayer->speedverta = limit;
        }
    }
    else
    {
        if (g_CurrentPlayer->speedverta > limit)
        {
            g_CurrentPlayer->speedverta -= (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedverta < limit)
            {
                g_CurrentPlayer->speedverta = limit;
            }
        }
        else
        {
            g_CurrentPlayer->speedverta += (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedverta > limit)
            {
                g_CurrentPlayer->speedverta = limit;
            }
        }
    }
}


/**
 * US address 7F080228.
 * Duplicate of playerScaleLookSpeedVerta.
*/
f32 playerScaleLookSpeedTheta(f32 speed) {
    if (0.0f < speed) {
        return (viGetFovY() * speed * -0.7f) / FOV_Y_F;
    } else if (speed < 0.0f) {
        return (viGetFovY() * -speed * 0.7f) / FOV_Y_F;
    } else {
        return 0.0f;
    }
}

void playerUpdateSpeedTheta(f32 value)
{
    f32 mult = viGetFovY() / FOV_Y_F;
    f32 limit = playerScaleLookSpeedTheta(value);

    if (value > 0.0f)
    {
        if (g_CurrentPlayer->speedtheta > 0.0f)
        {
            g_CurrentPlayer->speedtheta -= (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedtheta -= (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedtheta < limit)
        {
            g_CurrentPlayer->speedtheta = limit;
        }
    }
    else if (value < 0.0f)
    {
        if (g_CurrentPlayer->speedtheta < 0)
        {
            g_CurrentPlayer->speedtheta += (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedtheta += (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (limit < g_CurrentPlayer->speedtheta)
        {
            g_CurrentPlayer->speedtheta = limit;
        }
    }
    else
    {
        if (limit < g_CurrentPlayer->speedtheta)
        {
            g_CurrentPlayer->speedtheta -= (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedtheta < limit)
            {
                g_CurrentPlayer->speedtheta = limit;
            }
        }
        else
        {
            g_CurrentPlayer->speedtheta += (0.05f * g_GlobalTimerDelta * mult);

            if (limit < g_CurrentPlayer->speedtheta)
            {
                g_CurrentPlayer->speedtheta = limit;
            }
        }
    }
}


Gfx *currentPlayerDrawFade(Gfx *gdl)
{
    f32 frac = g_CurrentPlayer->colourscreenfrac;
    s32 r = g_CurrentPlayer->colourscreenred;
    s32 g = g_CurrentPlayer->colourscreengreen;
    s32 b = g_CurrentPlayer->colourscreenblue;
    if ((cameraFrameCounter1 != 0) || (cameraFrameCounter2 != 0)) {
        frac = 1.0f;
        b = 0;
        g = 0;
        r = 0;
    }
    if (frac > 0) {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        gDPSetColorDither(gdl++, G_CD_DISABLE);
        gDPSetTexturePersp(gdl++, G_TP_NONE);
        gDPSetAlphaCompare(gdl++, G_AC_NONE);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetTextureFilter(gdl++, G_TF_BILERP);
        gDPSetTextureConvert(gdl++, G_TC_FILT);
        gDPSetTextureLUT(gdl++, G_TT_NONE);
        gDPSetRenderMode(gdl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gdl++, 0, 0, r, g, b, (s32)(frac * 255.0f));
        gDPFillRectangle(gdl++, VI_SX(viGetViewLeft()), viGetViewTop(), VI_SX(viGetViewLeft() + viGetViewWidth()), (viGetViewTop() + viGetViewHeight()));
        gDPPipeSync(gdl++);
        gDPSetColorDither(gdl++, G_CD_BAYER);
        gDPSetTexturePersp(gdl++, G_TP_PERSP);
        gDPSetTextureLOD(gdl++, G_TL_LOD);
    }

    return gdl;
}

void currentPlayerSetFadeColour(s32 r, s32 g, s32 b, f32 frac) {
    g_CurrentPlayer->colourscreenred = r;
    g_CurrentPlayer->colourscreengreen = g;
    g_CurrentPlayer->colourscreenblue = b;
    g_CurrentPlayer->colourscreenfrac = frac;
}

void currentPlayerAdjustFade(f32 maxfadetime, s32 r, s32 g, s32 b, f32 frac)
{
    g_CurrentPlayer->colourfadetime60    = 0;
    g_CurrentPlayer->colourfadetimemax60 = maxfadetime;
    g_CurrentPlayer->colourfaderedold    = g_CurrentPlayer->colourscreenred;
    g_CurrentPlayer->colourfaderednew    = r;
    g_CurrentPlayer->colourfadegreenold  = g_CurrentPlayer->colourscreengreen;
    g_CurrentPlayer->colourfadegreennew  = g;
    g_CurrentPlayer->colourfadeblueold   = g_CurrentPlayer->colourscreenblue;
    g_CurrentPlayer->colourfadebluenew   = b;
    g_CurrentPlayer->colourfadefracold   = g_CurrentPlayer->colourscreenfrac;
    g_CurrentPlayer->colourfadefracnew   = frac;
}

void currentPlayerSetFadeFrac(f32 maxfadetime, f32 frac)
{
    currentPlayerAdjustFade(maxfadetime, g_CurrentPlayer->colourscreenred, g_CurrentPlayer->colourscreengreen, g_CurrentPlayer->colourscreenblue, frac);
}

bool currentPlayerIsFadeComplete(void)
{
	return g_CurrentPlayer->colourfadetimemax60 < 0;
}

void currentPlayerTickColourFade(void)
{
    if (g_CurrentPlayer->colourfadetimemax60 >= 0)
    {
        g_CurrentPlayer->colourfadetime60 += g_GlobalTimerDelta;

        if (g_CurrentPlayer->colourfadetime60 < g_CurrentPlayer->colourfadetimemax60)
        {
            f32 mult                           = g_CurrentPlayer->colourfadetime60 / g_CurrentPlayer->colourfadetimemax60;
            g_CurrentPlayer->colourscreenfrac  = g_CurrentPlayer->colourfadefracold + (g_CurrentPlayer->colourfadefracnew - g_CurrentPlayer->colourfadefracold) * mult;
            g_CurrentPlayer->colourscreenred   = g_CurrentPlayer->colourfaderedold + (s32)((g_CurrentPlayer->colourfaderednew - g_CurrentPlayer->colourfaderedold) * mult);
            g_CurrentPlayer->colourscreengreen = g_CurrentPlayer->colourfadegreenold + (s32)((g_CurrentPlayer->colourfadegreennew - g_CurrentPlayer->colourfadegreenold) * mult);
            g_CurrentPlayer->colourscreenblue  = g_CurrentPlayer->colourfadeblueold + (s32)((g_CurrentPlayer->colourfadebluenew - g_CurrentPlayer->colourfadeblueold) * mult);
            return;
        }

        g_CurrentPlayer->colourscreenfrac    = g_CurrentPlayer->colourfadefracnew;
        g_CurrentPlayer->colourscreenred     = g_CurrentPlayer->colourfaderednew;
        g_CurrentPlayer->colourscreengreen   = g_CurrentPlayer->colourfadegreennew;
        g_CurrentPlayer->colourscreenblue    = g_CurrentPlayer->colourfadebluenew;
        g_CurrentPlayer->colourfadetimemax60 = -1;
    }
}

void currentPlayerStartChrFade(f32 duration60, f32 targetfrac)
{
    ChrRecord *chr = g_CurrentPlayer->prop->chr;

    if (chr)
    {
        g_CurrentPlayer->bondfadetime60    = 0;
        g_CurrentPlayer->bondfadetimemax60 = duration60;
        g_CurrentPlayer->bondfadefracold   = chr->fadealpha / 255.0f;
        g_CurrentPlayer->bondfadefracnew   = targetfrac;
    }
}

void currentPlayerTickChrFade(void)
{
    if (g_CurrentPlayer->bondfadetimemax60 >= 0)
    {
        ChrRecord *chr = g_CurrentPlayer->prop->chr;
        f32        frac;

        g_CurrentPlayer->bondfadetime60 += g_GlobalTimerDelta;

        if (g_CurrentPlayer->bondfadetime60 < g_CurrentPlayer->bondfadetimemax60)
        {
            frac = g_CurrentPlayer->bondfadefracold + (g_CurrentPlayer->bondfadefracnew - g_CurrentPlayer->bondfadefracold) * g_CurrentPlayer->bondfadetime60 / g_CurrentPlayer->bondfadetimemax60;
        }
        else
        {
            frac = g_CurrentPlayer->bondfadefracnew;
            g_CurrentPlayer->bondfadetimemax60 = -1;
        }

        if (chr)
        {
            chr->fadealpha = (s8)(frac * 255);
        }
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
 * US address 7F081974.
 * EU address 7F081A18.
 * Perfect Dark method bmoveProcessInput.
*/
/**
 * Co-op: set when a player respawns, until they let go of the buttons they
 * respawned with, so that pressing Start to respawn doesn't also open the watch.
 */
s32 g_CoopRespawnButtonsHeld[4];

void playerProcessInput(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons)
{
    struct MoveData moveData;

    s8 c2stickx;
    s8 c2sticky;
    u16 c2buttons;
    u16 c2oldbuttons;
    s32 adjustedStickX;
    s32 tmpc2sticky;
    s32 fireHeld;
    s32 firePressed;
    s32 aimHeld;
    s32 aimPressed;
    u16 shootButtons;
    u16 aimButtons;
    u16 invButtons;
    TankRecord *exitTank;
    s32 axis;
    TankRecord *enterTank;
    f32 accelScale;
    f32 noiseRadius;
    f32 zoomFov;
    f32 targetSpeed;
    f32 walkFrac;
    f32 unadjustedTargetSpeed;
    f32 dx;
    f32 dz;
    f32 dy;
    f32 targetPitch;
    StandTile *tile;
    f32 groundDist;
    f32 limit;
    coord3d lookPos;
    coord3d size;
    f32 fovScale;
    f32 pitchInput;
    f32 fovY;
    f32 turnScale;
    f32 turnInput;
    f32 turnTarget;
    s32 step;
    f32 swivelY;
    s32 canCycleWeapons;
    f32 swivelX;


    moveData.canSwivelGun = 0;
    moveData.canManualAim = 0;
    moveData.triggerOn = 0;
    moveData.btap = 0;
    moveData.canLookAhead = 0;
    moveData.canTurnTank = 0;
    moveData.canNaturalTurn = 0;
    moveData.canNaturalPitch = 0;
    moveData.digitalStepForward = 0;
    moveData.digitalStepBack = 0;
    moveData.digitalStepLeft = 0;
    moveData.digitalStepRight = 0;
    moveData.tankTurnLeftSpeed = 0;
    moveData.tankTurnRightSpeed = 0;
    moveData.speedVertaDown = 0;
    moveData.speedVertaUp = 0;
    moveData.aimTurnLeftSpeed = 0;
    moveData.aimTurnRightSpeed = 0;
    moveData.weaponBackOffset = 0;
    moveData.weaponForwardOffset = 0;
    moveData.aiming = 0;
    moveData.zooming = 0;
    moveData.zoomOutFovPersec = 0;
    moveData.zoomInFovPersec = 0;
    moveData.crouchDown = 0;
    moveData.crouchUp = 0;
    moveData.rLeanLeft = 0;
    moveData.rLeanRight = 0;
    moveData.detonating = 0;
    moveData.canAutoAim = 0;
    moveData.invertPitch = optionsGetLookInverted() == 0;
    moveData.disableLookAhead = 0;

    if (stick_x < -5) {
		moveData.controlStickXSafe = stick_x + 5;
	} else if (stick_x > 5) {
		moveData.controlStickXSafe = stick_x - 5;
	} else {
		moveData.controlStickXSafe = 0;
	}

	if (stick_y < -5) {
		moveData.controlStickYSafe = stick_y + 5;
	} else if (stick_y > 5) {
		moveData.controlStickYSafe = stick_y - 5;
	} else {
		moveData.controlStickYSafe = 0;
	}

    moveData.controlStickXRaw = (s32)stick_x;
    moveData.controlStickYRaw = (s32)stick_y;

    moveData.analogTurn = moveData.controlStickXSafe;
    moveData.analogPitch = moveData.controlStickYSafe;
    moveData.analogStrafe = moveData.controlStickXSafe;
    moveData.analogWalk = moveData.controlStickYSafe;

    if (g_CoopMode && g_CoopRespawnButtonsHeld[playerGetCurrentNum()])
    {
        if (buttons & START_BUTTON)
        {
            oldbuttons |= START_BUTTON;
        }
        else
        {
            g_CoopRespawnButtonsHeld[playerGetCurrentNum()] = FALSE;
        }
    }

    if (g_CurrentPlayer->bonddead == FALSE
        && g_bondviewForceDisarm <= 0
        && (
            (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x5
                && ((buttons & ~oldbuttons) & START_BUTTON)
            )
            ||
            (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5
                && g_CurrentPlayer->open_close_solo_watch_menu)
        )
        && !isMpGame()
        && !coopPartnerHasWatchOut())
    {
        // Co-op: either player's watch pauses the game for both and fills
        // the screen while it is out (see coopUpdateSoloView).
        watchTrigger(0);
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0
        && g_CurrentPlayer->bonddead == FALSE
        && (
            !isMpGame()
            || (
                g_MpStopPlay == 0
                && g_MpGameOver == 0)))
    {
        if (optionsGetControlStyle() == CONTROLLER_CONFIG_DOMINO /* 2.3 */
            || optionsGetControlStyle() == CONTROLLER_CONFIG_GOODHEAD /* 2.4 */
            || optionsGetControlStyle() == CONTROLLER_CONFIG_GALORE /* 2.2 */
            || optionsGetControlStyle() == CONTROLLER_CONFIG_PLENTY /* 2.1 */
            )
        {
            c2stickx = joyGetStickX(playerGetCurrentNum() + getPlayerCount());
            c2sticky = joyGetStickY(playerGetCurrentNum() + getPlayerCount());
            c2buttons = joyGetButtons(playerGetCurrentNum() + getPlayerCount() , (u32)ANY_BUTTON);

            c2oldbuttons = g_CurrentPlayer->prev_buttons_pressed;

            if (c2stickx < -5)
            {
                adjustedStickX = c2stickx + 5;
            }
            else if (c2stickx > 5)
            {
                adjustedStickX = c2stickx - 5;
            }
            else
            {
                adjustedStickX = 0;
            }

            if (c2sticky < -5)
            {
                tmpc2sticky = c2sticky + 5;
            }
            else if (c2sticky >= 6)
            {
                tmpc2sticky = c2sticky - 5;
            }
            else
            {
                tmpc2sticky = 0;
            }

            /* 2.1 and 2.3 */
            if (optionsGetControlStyle() == CONTROLLER_CONFIG_PLENTY || (optionsGetControlStyle() == CONTROLLER_CONFIG_DOMINO))
            {
                moveData.analogStrafe = adjustedStickX;
                moveData.analogPitch = tmpc2sticky;
            }
            else
            {
                if (g_PlayerIsInTank == 1 && !g_CurrentPlayer->insightaimmode)
                {
                    moveData.analogTurn = adjustedStickX;
                }
                else
                {
                    moveData.analogStrafe = adjustedStickX;
                }

                moveData.analogWalk = tmpc2sticky;
            }

            /* 2.1 and 2.2 */
            if (optionsGetControlStyle() == CONTROLLER_CONFIG_PLENTY || optionsGetControlStyle() == CONTROLLER_CONFIG_GALORE)
            {
                aimHeld = (c2buttons & Z_TRIG) != 0;
                aimPressed = ((c2buttons & ~c2oldbuttons) & Z_TRIG) != 0;
                fireHeld = (buttons & Z_TRIG) != 0;
                firePressed = ((buttons & ~oldbuttons) & Z_TRIG) != 0;
            }
            else
            {
                aimHeld = (buttons & Z_TRIG) != 0;
                aimPressed = ((buttons & ~oldbuttons) & Z_TRIG) != 0;
                fireHeld = (c2buttons & Z_TRIG) != 0;
                firePressed = ((c2buttons & ~c2oldbuttons) & Z_TRIG) != 0;
            }

            if (lvGetControlsLocked() == 0 && mpMenuCanPlayerAct())
            {
                if (optionsGetAimControl() == 0)
                {
                    g_CurrentPlayer->insightaimmode = aimHeld;
                }
                else if (aimPressed)
                {
                    g_CurrentPlayer->insightaimmode = !g_CurrentPlayer->insightaimmode;
                }

                moveData.canSwivelGun = !g_CurrentPlayer->insightaimmode;
                moveData.canAutoAim = !g_CurrentPlayer->insightaimmode;

                moveData.canManualAim = g_CurrentPlayer->insightaimmode;

                moveData.btap = (
                    (((buttons & ~oldbuttons) & B_BUTTON) != 0)
                    ||
                    ((((c2buttons & ~c2oldbuttons) & B_BUTTON)) != 0)
                    );

                moveData.canLookAhead = !g_CurrentPlayer->insightaimmode;
                moveData.canTurnTank = 1;
                moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                moveData.canNaturalPitch = !g_CurrentPlayer->insightaimmode;

                if (g_CurrentPlayer->insightaimmode && (stick_y > 60))
                {
                    moveData.speedVertaDown = (f32) (stick_y - 60) / FLOAT_TEN_B;
                    if (moveData.speedVertaDown > 1.0f)
                    {
                        moveData.speedVertaDown = 1.0f;
                    }
                }
                else
                {
                    //moveData.speedVertaDown = 0;
                }

                if (g_CurrentPlayer->insightaimmode && (stick_y < -60))
                {
                    moveData.speedVertaUp = (f32) (-60 - stick_y) / FLOAT_TEN_B;
                    if (moveData.speedVertaUp > 1.0f)
                    {
                        moveData.speedVertaUp = 1.0f;
                    }
                }
                else
                {
                    //moveData.speedVertaUp = 0;
                }


                if (g_CurrentPlayer->insightaimmode && (stick_x < -60))
                {
                    moveData.aimTurnLeftSpeed = (f32) (-60 - stick_x) / FLOAT_TEN_B;
                    if (moveData.aimTurnLeftSpeed > 1.0f)
                    {
                        moveData.aimTurnLeftSpeed = 1.0f;
                    }
                }
                else
                {
                    //moveData.aimTurnLeftSpeed = 0;
                }

                if (g_CurrentPlayer->insightaimmode && (stick_x > 60) )
                {
                    moveData.aimTurnRightSpeed = (f32) (stick_x - 60) / FLOAT_TEN_B;
                    if (moveData.aimTurnRightSpeed > 1.0f)
                    {
                        moveData.aimTurnRightSpeed = 1.0f;
                    }
                }
                else
                {
                    //moveData.aimTurnRightSpeed = 0;
                }

                moveData.weaponBackOffset = (
                        ((buttons & A_BUTTON) != 0)
                        ||
                        ((c2buttons & A_BUTTON) != 0)
                    )
                    && (firePressed);

               moveData.weaponForwardOffset = (
                    (
                       (((buttons & ~oldbuttons) & A_BUTTON) != 0)
                       ||
                       (((c2buttons & ~c2oldbuttons) & A_BUTTON) != 0)
                    ))
                    && (!fireHeld);

                moveData.aiming = g_CurrentPlayer->insightaimmode;
                moveData.zooming = g_CurrentPlayer->insightaimmode;

                if ((weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH))
                    && g_CurrentPlayer->insightaimmode)
                {
                    if (tmpc2sticky < 0)
                    {
                        moveData.zoomOutFovPersec = (f32) -tmpc2sticky / 70.0f;
                        if (moveData.zoomOutFovPersec > 1.0f)
                        {
                            moveData.zoomOutFovPersec = 1.0f;
                        }

                        moveData.zoomOutFovPersec *= 2.0f;
                    }

                    if (tmpc2sticky > 0)
                    {
                        moveData.zoomInFovPersec = (f32) tmpc2sticky / 70.0f;
                        if (moveData.zoomInFovPersec > 1.0f)
                        {
                            moveData.zoomInFovPersec = 1.0f;
                        }

                        moveData.zoomInFovPersec *= 2.0f;
                    }
                }

                moveData.crouchDown = (weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                    && (g_CurrentPlayer->insightaimmode)
                    && (c2sticky < -30);

                moveData.crouchUp = (weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                    && (g_CurrentPlayer->insightaimmode)
                    && (c2sticky > 30);

                if ((
                           (((buttons & A_BUTTON) != 0) && (((buttons & ~oldbuttons) & B_BUTTON) != 0))
                        || (((buttons & B_BUTTON) != 0) && (((buttons & ~oldbuttons) & A_BUTTON) != 0))
                        || ((c2buttons & A_BUTTON) && ((c2buttons & ~c2oldbuttons) & B_BUTTON))
                        || ((c2buttons & B_BUTTON) && ((c2buttons & ~c2oldbuttons) & A_BUTTON)))
                    && (gunGetWeapon(GUNRIGHT) == ITEM_REMOTEMINE))
                {
                    moveData.detonating = 1;
                    moveData.weaponBackOffset = 0;
                    moveData.weaponForwardOffset = 0;
                    moveData.btap = 0;
                }

                if (g_PlayerIsInTank == 1 && g_CurrentPlayer->insightaimmode)
                {
                    if (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
                    {
                        moveData.controlStickXRaw = 0;

                        if (moveData.analogStrafe == 0)
                        {
                            moveData.analogStrafe = moveData.analogTurn;
                        }
                    }
                    else if (moveData.analogStrafe == 0)
                    {
                        if (moveData.aimTurnLeftSpeed > 0)
                        {
                            moveData.tankTurnLeftSpeed = moveData.aimTurnLeftSpeed;
                        }

                        if (moveData.aimTurnRightSpeed > 0)
                        {
                            moveData.tankTurnRightSpeed = moveData.aimTurnRightSpeed;
                        }
                    }

                    moveData.aimTurnLeftSpeed = 0;
                    moveData.aimTurnRightSpeed = 0;
                }
            }

            moveData.triggerOn = (fireHeld)
                && (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
                && ((buttons & A_BUTTON) == 0)
                && ((c2buttons & A_BUTTON) == 0);

            moveData.disableLookAhead = 1;
            g_CurrentPlayer->prev_buttons_pressed = c2buttons;
        }
        else
        {
            /* 1.3 and 1.4 */
            if (optionsGetControlStyle() == CONTROLLER_CONFIG_KISSY
                || optionsGetControlStyle() == CONTROLLER_CONFIG_GOODNIGHT)
            {
                shootButtons = A_BUTTON;
                aimButtons = Z_TRIG;
                invButtons = L_TRIG | R_TRIG;
            }
            else
            {
                shootButtons = Z_TRIG;
                aimButtons = L_TRIG | R_TRIG;
                invButtons = A_BUTTON;
            }

            if (lvGetControlsLocked() == 0)
            {
                if (mpMenuCanPlayerAct())
                {
                    if (optionsGetAimControl() == 0)
                    {
                        g_CurrentPlayer->insightaimmode = (buttons & aimButtons) != 0;
                    }
                    else if ((buttons & ~oldbuttons) & aimButtons)
                    {
                        g_CurrentPlayer->insightaimmode = !g_CurrentPlayer->insightaimmode;
                    }

                    moveData.canSwivelGun = !g_CurrentPlayer->insightaimmode;
                    moveData.canAutoAim = !g_CurrentPlayer->insightaimmode;
                    moveData.btap = ((buttons & ~oldbuttons) & B_BUTTON) != 0;
                    moveData.canManualAim = g_CurrentPlayer->insightaimmode;

                    /* 1.2 and 1.4 */
                    if (optionsGetControlStyle() == CONTROLLER_CONFIG_SOLITARE
                        || optionsGetControlStyle() == CONTROLLER_CONFIG_GOODNIGHT)
                    {
                        if ((buttons & (L_JPAD | L_CBUTTONS)) != 0)
                        {
                            if (!g_CurrentPlayer->insightaimmode)
                            {
                                if (g_PlayerIsInTank == 1)
                                {
                                    moveData.aimTurnLeftSpeed = 1.0f;
                                }
                                else
                                {
                                    moveData.digitalStepLeft = 1;
                                }
                            }
                            else
                            {
                                moveData.tankTurnLeftSpeed = 1.0f;
                            }
                        }

                        if ((buttons & (R_JPAD | R_CBUTTONS)) != 0)
                        {
                            if (!g_CurrentPlayer->insightaimmode)
                            {
                                if (g_PlayerIsInTank == 1)
                                {
                                    moveData.aimTurnRightSpeed = 1.0f;
                                }
                                else
                                {
                                    moveData.digitalStepRight = 1;
                                }
                            }
                            else
                            {
                                moveData.tankTurnRightSpeed = 1.0f;
                            }
                        }

                        moveData.digitalStepForward = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (U_JPAD | U_CBUTTONS)) );

                        moveData.digitalStepBack = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (D_JPAD | D_CBUTTONS)));

                        moveData.canNaturalPitch = !g_CurrentPlayer->insightaimmode;

                        if (g_PlayerIsInTank == 1)
                        {
                            moveData.canTurnTank = !g_CurrentPlayer->insightaimmode;
                        }
                        else
                        {
                            moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                        }
                    }
                    else
                    {
                        if ((buttons & (s32)(L_JPAD | L_CBUTTONS)) != 0)
                        {
                            moveData.tankTurnLeftSpeed = 1.0f;
                        }
                        /* optional else statement, matches with or without. */
                        else
                        {
                            moveData.tankTurnLeftSpeed = 0;
                        }

                        if ((buttons & (s32)(R_JPAD | R_CBUTTONS)) != 0)
                        {
                            moveData.tankTurnRightSpeed = 1.0f;
                        }
                        /* optional else statement, matches with or without. */
                        else
                        {
                            moveData.tankTurnRightSpeed = 0;
                        }

                        moveData.digitalStepLeft = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (s32)(L_JPAD | L_CBUTTONS)) );

                        moveData.digitalStepRight = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (s32)(R_JPAD | R_CBUTTONS)));

                        moveData.canLookAhead = !g_CurrentPlayer->insightaimmode;

                        if ((!g_CurrentPlayer->insightaimmode) && (buttons & (U_JPAD | U_CBUTTONS)) )
                        {
                            moveData.speedVertaDown = 1.0f;
                        }

                        if ((!g_CurrentPlayer->insightaimmode) && (buttons & (D_JPAD | D_CBUTTONS)))
                        {
                            moveData.speedVertaUp = 1.0f;
                        }

                        moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_y > 60))
                    {
                        moveData.speedVertaDown = (f32) (stick_y - 60) / FLOAT_TEN_B;
                        if (moveData.speedVertaDown > 1.0f)
                        {
                            moveData.speedVertaDown = 1.0f;
                        }
                    }
                    else if ((g_CurrentPlayer->insightaimmode) && (stick_y < -60))
                    {
                        moveData.speedVertaUp = (f32) (-60 - stick_y) / FLOAT_TEN_B;
                        if (moveData.speedVertaUp > 1.0f)
                        {
                            moveData.speedVertaUp = 1.0f;
                        }
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_x < -60))
                    {
                        moveData.aimTurnLeftSpeed = (f32) (-60 - stick_x) / FLOAT_TEN_B;
                        if (moveData.aimTurnLeftSpeed > 1.0f)
                        {
                            moveData.aimTurnLeftSpeed = 1.0f;
                        }
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_x > 60))
                    {
                        moveData.aimTurnRightSpeed = (f32) (stick_x - 60) / FLOAT_TEN_B;
                        if (moveData.aimTurnRightSpeed > 1.0f)
                        {
                            moveData.aimTurnRightSpeed = 1.0f;
                        }
                    }

                    moveData.weaponBackOffset =
                        ((buttons & invButtons) != 0)
                        &&
                        (((buttons & ~oldbuttons) & shootButtons) != 0)
                        ;

                    moveData.weaponForwardOffset =
                        (((buttons & ~oldbuttons) & invButtons) != 0)
                        &&
                        ((buttons & shootButtons) == 0)
                        ;

                    moveData.aiming = g_CurrentPlayer->insightaimmode;
                    moveData.zooming = g_CurrentPlayer->insightaimmode;

                    if ((weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH))
                        && g_CurrentPlayer->insightaimmode
                        )
                    {
                        /* down = 0x404 */
                        if ((buttons & (D_JPAD | D_CBUTTONS)) != 0)
                        {
                            moveData.zoomOutFovPersec = 1.0f;
                        }

                        if ((buttons & (U_JPAD | U_CBUTTONS)) != 0)
                        {
                            moveData.zoomInFovPersec = 1.0f;
                        }
                    }

                    moveData.crouchDown = (weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                        && (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (D_JPAD | D_CBUTTONS)));

                    moveData.crouchUp = (weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                        && (g_CurrentPlayer->insightaimmode)
                        && ((~buttons & (U_JPAD | U_CBUTTONS)));

                    moveData.rLeanLeft = (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (L_JPAD | L_CBUTTONS)));

                    moveData.rLeanRight = (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (R_JPAD | R_CBUTTONS)));

                    if (
                        ((((buttons & invButtons) != 0) && (((buttons & ~oldbuttons) & B_BUTTON) != 0))
                            || ((buttons & B_BUTTON) && (((buttons & ~oldbuttons) & invButtons) != 0)))
                        && (gunGetWeapon(GUNRIGHT) == ITEM_REMOTEMINE))
                    {
                        moveData.detonating = 1;
                        moveData.weaponBackOffset = 0;
                        moveData.weaponForwardOffset = 0;
                        moveData.btap = 0;
                    }

                    if ((g_PlayerIsInTank == 1) && (g_CurrentPlayer->insightaimmode))
                    {
                        if (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
                        {
                            moveData.controlStickXRaw = 0;
                            moveData.canTurnTank = 1;
                        }
                        else if ((moveData.tankTurnLeftSpeed == 0) && (moveData.tankTurnRightSpeed == 0))
                        {
                            if (moveData.aimTurnLeftSpeed > 0)
                            {
                                moveData.tankTurnLeftSpeed = moveData.aimTurnLeftSpeed;
                            }

                            if (moveData.aimTurnRightSpeed > 0)
                            {
                                moveData.tankTurnRightSpeed = moveData.aimTurnRightSpeed;
                            }
                        }

                        moveData.aimTurnLeftSpeed = 0;
                        moveData.aimTurnRightSpeed = 0;
                    }
                }
            }

            moveData.triggerOn = ((buttons & shootButtons)  != 0)
                && (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
                && ((buttons & invButtons) == 0);

            /* 1.2 and 1.4 */
            if (optionsGetControlStyle() == CONTROLLER_CONFIG_SOLITARE || optionsGetControlStyle() == CONTROLLER_CONFIG_GOODNIGHT)
            {
                moveData.disableLookAhead = 1;
            }
        }
    }

    g_CurrentPlayer->activatePressed = 0;

    if (moveData.btap)
    {
        /* If Bond is in the tank and pressed B, then exit. */
        if (g_PlayerIsInTank == 1)
        {
            exitTank = (struct TankRecord *)g_PlayerTankProp->obj;
            exitTank->storedShells = weaponGetHeldAmmo(ITEM_TANKSHELLS);

            weaponGiveAmmo(ITEM_TANKSHELLS, 0);
            invRemoveItemByID(ITEM_TANKSHELLS);

            if (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
            {
                exitTank->storedShells += gunGetMagazineAmmo(GUNRIGHT);
                gunSwitchWhenEmpty();
            }

            exitTank->is_firing_tank = 0;
            g_PlayerIsInTank = 0;
            g_CurrentPlayer->speedsideways = 0;
            g_CurrentPlayer->speedforwards = 0;
            g_CurrentPlayer->speedtheta = 0;

            for (axis=0; axis<3; axis++)
            {
                g_CurrentPlayer->bondshotspeed.f[axis] = 0;
            }

            g_CurrentPlayer->crouchpos = CROUCH_STAND;
        }
        /* If Bond is standing on the tank and pressed B, enter the tank. */
        else if (g_PlayerTankProp != NULL
            && g_PlayerTankProp->type == PROP_TYPE_OBJ
            && g_PlayerTankProp->obj->type == PROPDEF_TANK
            && g_BondCanEnterTank
            // Counter-op: the guard player can't drive the tank.
            && !counterOpIsGuardPlayer(playerGetCurrentNum()))
        {
            enterTank = (struct TankRecord *)g_PlayerTankProp->obj;

            invAddItem(ITEM_TANKSHELLS);
            weaponGiveAmmo(ITEM_TANKSHELLS, enterTank->storedShells);
            enterTank->storedShells = 0;
            g_TankTurretVerticalAngle = enterTank->turret_vertical_angle;
            g_TankTurretVerticalAngleRelated = g_TankTurretVerticalAngle / TANK_VERT_ANGLE_FACTOR;
            g_TankTurretAngle = enterTank->turret_orientation_angle;
            g_TankTurretOrientationAngleRad = enterTank->turret_orientation_angle;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / TANK_VERT_ANGLE_RAD_FACTOR;
            tank_turret_turn_speed = 0;
            g_TankOrientationAngle = enterTank->tank_orientation_angle;
            g_TankTurnSpeed = 0;
            g_PlayerIsInTank = 1;
            g_EnterTankAudioState = TANK_RUN_STATE_NOT_RUNNING;
            g_CurrentPlayer->speedsideways = 0;
            g_CurrentPlayer->speedforwards = 0;
            g_CurrentPlayer->speedtheta = 0;
            g_CurrentPlayer->crouchpos = CROUCH_HALF;
            g_TankEnteringSitHeight = 0;
            g_TankEnteringSitHeightRemain = 1.0f;
            g_TankEnterBondHorizAngleDeg = g_CurrentPlayer->vv_theta;
            g_TankEnterBondVertAngleDeg = g_CurrentPlayer->vv_verta;
            g_EnterTankCoord.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
            g_EnterTankCoord.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
            g_EnterTankCoord.f[2] = g_CurrentPlayer->bond.collision_position.f[2];
            g_TankDamagePenaltyTicks = 0;

            tankUpdateTurretOffset();
        }
        else
        {
            g_CurrentPlayer->activatePressed = 1;
        }
    }

    if (moveData.invertPitch == 0)
    {
        f32 tmp;

        moveData.controlStickYRaw = (s32) -stick_y;
        moveData.analogPitch = -moveData.analogPitch;
        tmp = moveData.speedVertaDown;
        moveData.speedVertaDown = moveData.speedVertaUp;
        moveData.speedVertaUp = tmp;
    }

    if (playerIsShowingDamage() && !isMpGame())
    {
        moveData.triggerOn = 0;
    }

    moveData.triggerOn = counterOpSuicidePillTick(moveData.triggerOn);

    gunTickGameplay(moveData.triggerOn);

    if (playerGetVisibleToGuards()
        && (gunGetFiringStatus(GUNRIGHT)
            || gunGetFiringStatus(GUNLEFT)))
    {
        noiseRadius = 0;

        if (gunGetFiringStatus(GUNRIGHT) && gunGetNoise(GUNRIGHT) > noiseRadius)
        {
            noiseRadius = gunGetNoise(GUNRIGHT);
        }

        if (gunGetFiringStatus(GUNLEFT) && noiseRadius < gunGetNoise(GUNLEFT))
        {
            noiseRadius = gunGetNoise(GUNLEFT);
        }

        chrCheckGuardsHeardSound(noiseRadius);
    }

    gunSetSightVisible(GUNSIGHTREASON_NOTAIMING, moveData.aiming);

    if (moveData.zoomOutFovPersec > 0)
    {
        gunZoomOut(moveData.zoomOutFovPersec);
    }

    if (moveData.zoomInFovPersec > 0)
    {
        gunZoomIn(moveData.zoomInFovPersec);
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
    {
        zoomFov = 60.0f;

        if (moveData.zooming)
        {
            zoomFov = gunGetZoomFov();

            if (zoomFov <= 0)
            {
                zoomFov = 60.0f;
            }
        }

        watchZoomTo(zoomFov);
        watchUpdateZoom();
    }

    if (g_PlayerIsInTank == 1)
    {
        g_TankTurretTurn = 0;

        if (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING)
        {
            if (moveData.tankTurnRightSpeed > 0)
            {
                g_TankTurretTurn += g_GlobalTimerDelta * moveData.tankTurnRightSpeed * DegToRad1Fact(1);
            }
            else if (moveData.tankTurnLeftSpeed > 0)
            {
                g_TankTurretTurn -= g_GlobalTimerDelta * moveData.tankTurnLeftSpeed * DegToRad1Fact(1);
            }
            else if (moveData.canTurnTank)
            {
                targetSpeed = (f32) moveData.analogStrafe / 70.0f;

                if (targetSpeed > 1.0f)
                {
                    targetSpeed = 1.0f;
                }

                if (targetSpeed < -1.0f)
                {
                    targetSpeed = -1.0f;
                }

                g_TankTurretTurn += DegToRad1Fact(1) * targetSpeed * g_GlobalTimerDelta;
            }

            if (!g_CurrentPlayer->insightaimmode)
            {
                walkFrac = 0;
                targetSpeed = 1.0f;
                accelScale = 1.0f;

                if (moveData.canLookAhead)
                {
                    walkFrac = (f32) moveData.analogWalk / 70.0f;
                }
                else if (moveData.digitalStepForward)
                {
                    walkFrac = 1.0f;
                }
                else if (moveData.digitalStepBack)
                {
                    walkFrac = -1.0f;
                }

                if (walkFrac > 1.0f)
                {
                    walkFrac = 1.0f;
                }
                else if (walkFrac < -1.0f)
                {
                    walkFrac = -1.0f;
                }

                unadjustedTargetSpeed = walkFrac * TANK_MAX_SPEED;
                targetSpeed = unadjustedTargetSpeed;

                if (g_TankDamagePenaltyTicks > 0)
                {
                    targetSpeed = unadjustedTargetSpeed * 0.5f;
                    accelScale = 4.0f;
                    g_TankDamagePenaltyTicks -= g_ClockTimer;
                }

                if (targetSpeed != g_CurrentPlayer->speedforwards)
                {
                    if (g_CurrentPlayer->speedforwards < targetSpeed)
                    {
                        unadjustedTargetSpeed = ((((((targetSpeed - g_CurrentPlayer->speedforwards) / 4.0f) / TANK_MAX_SPEED) + 0.5f) * accelScale * FLOAT_TEN_A) / 60.0f);

                        g_CurrentPlayer->speedforwards += (unadjustedTargetSpeed) * g_GlobalTimerDelta;

                        if (targetSpeed < g_CurrentPlayer->speedforwards)
                        {
                            g_CurrentPlayer->speedforwards = targetSpeed;
                        }
                    }
                    else if (targetSpeed < g_CurrentPlayer->speedforwards)
                    {
                        unadjustedTargetSpeed = ((((((g_CurrentPlayer->speedforwards - targetSpeed) / 4.0f) / TANK_MAX_SPEED) + 0.5f) * accelScale * -FLOAT_TEN_A) / 60.0f);

                        g_CurrentPlayer->speedforwards += (unadjustedTargetSpeed) * g_GlobalTimerDelta;

                        if (g_CurrentPlayer->speedforwards < targetSpeed)
                        {
                            g_CurrentPlayer->speedforwards = targetSpeed;
                        }
                    }
                }
            }
        }
    }
    else
    {
        if (moveData.digitalStepLeft)
        {
            playerUpdateSpeedSideways(-1);
        }
        else if (moveData.digitalStepRight)
        {
            playerUpdateSpeedSideways(1);
        }
        else
        {
            playerUpdateSpeedSideways(0);
        }

        if (moveData.canTurnTank) // ?? not sure why this tank property is used here. Is the name wrong?
        {
            g_CurrentPlayer->speedsideways = (f32) moveData.analogStrafe / 70.0f;
        }

        if (moveData.digitalStepForward)
        {
            playerUpdateSpeedForwards(1);
            g_CurrentPlayer->speedmaxtime60 += g_ClockTimer;
        }
        else if (moveData.digitalStepBack)
        {
            playerUpdateSpeedForwards(-1);
        }
        else
        {
            playerUpdateSpeedForwards(0);
        }

        if (moveData.canLookAhead)
        {
            g_CurrentPlayer->speedforwards = (f32) moveData.analogWalk / 70.0f;

            if (moveData.analogWalk > 60)
            {
                g_CurrentPlayer->speedmaxtime60 += g_ClockTimer;
            }
            else
            {
                g_CurrentPlayer->speedmaxtime60 = 0;
            }
        }

        if (g_CurrentPlayer->speedforwards > 1.0f)
        {
            g_CurrentPlayer->speedforwards = 1;
        }

        if (g_CurrentPlayer->speedforwards < -1.0f)
        {
            g_CurrentPlayer->speedforwards = -1.0f;
        }

        if (g_CurrentPlayer->speedsideways > 1)
        {
            g_CurrentPlayer->speedsideways = 1;
        }

        if (g_CurrentPlayer->speedsideways < -1)
        {
            g_CurrentPlayer->speedsideways = -1;
        }

        g_CurrentPlayer->speedforwards *= 1.08f;
        g_CurrentPlayer->speedforwards *= g_CurrentPlayer->speedboost;

        if ((moveData.canLookAhead == 0) && (moveData.digitalStepForward == 0))
        {
            g_CurrentPlayer->speedmaxtime60 = 0;
        }

        if (moveData.rLeanLeft)
        {
            playerSetLeanTarget(-1);
        }
        else if (moveData.rLeanRight)
        {
            playerSetLeanTarget(1);
        }
        else
        {
            playerSetLeanTarget(0);
        }

        if (moveData.crouchDown)
        {
            playerAdjustCrouch(-2);
        }
        else if (moveData.crouchUp)
        {
            playerAdjustCrouch(2);
        }
    }

    if (g_CurrentPlayer->speedmaxtime60 >= THREE_SECOND_TICKS)
    {
        if (g_CurrentPlayer->speedboost < SPEED_RUN_MAX)
        {
            g_CurrentPlayer->speedboost += (SPEED_TICK_ADJUST * g_GlobalTimerDelta);
        }

        if (g_CurrentPlayer->speedboost > SPEED_RUN_MAX)
        {
            g_CurrentPlayer->speedboost = SPEED_RUN_MAX;
        }
    }
    else
    {
        if (g_CurrentPlayer->speedboost > SPEED_REGULAR_MAX)
        {
            g_CurrentPlayer->speedboost -= (SPEED_TICK_ADJUST * g_GlobalTimerDelta);
        }

        if (g_CurrentPlayer->speedboost < SPEED_REGULAR_MAX)
        {
            g_CurrentPlayer->speedboost = SPEED_REGULAR_MAX;
        }
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
    {
        // By default the camera is pitched slightly below the horizon
        targetPitch = -4.0f;

        // lookaheadcentreenabled is always true, so this block always executes.
        if (g_CurrentPlayer->lookaheadcentreenabled)
        {
            tile = g_CurrentPlayer->bond.current_tile_ptr;
            groundDist = 300.0f;

            // prop, f32 *collision_radius, f32 *height, f32 *always_30
            playerGetCollisionSize(g_CurrentPlayer->prop, &size.f[0], &size.f[2], &size.f[1]);

            lookPos.f[0] = g_CurrentPlayer->bond.collision_position.f[0] + (g_CurrentPlayer->bond.theta_transform.f[0] * 300.0f);
            lookPos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
            lookPos.f[2] = g_CurrentPlayer->bond.collision_position.f[2] + (g_CurrentPlayer->bond.theta_transform.f[2] * 300.0f);

            stanResetHits();

            if (stanTestLineUnobstructed(&tile, g_CurrentPlayer->bond.collision_position.f[0], g_CurrentPlayer->bond.collision_position.f[2], lookPos.f[0], lookPos.f[2], CDTYPE_CLOSEDDOORS, size.f[2], size.f[1], 0, 1.0f))
            {
                lookPos.f[1] = playerGetFloorY(tile, lookPos.f[0], lookPos.f[2]);
            }
            else
            {
                chrStanPointPointIntersection(&g_CurrentPlayer->bond.collision_position, &g_CurrentPlayer->bond.theta_transform, (coord3d *) &lookPos);
                dx = lookPos.f[0] - g_CurrentPlayer->bond.collision_position.f[0];
                dz = lookPos.f[2] - g_CurrentPlayer->bond.collision_position.f[2];
                groundDist = sqrtf((dx * dx) + (dz * dz));
                lookPos.f[1] = playerGetFloorY(tile, lookPos.f[0], lookPos.f[2]);
            }

            if (groundDist > 0)
            {
                dy = lookPos.f[1] - g_CurrentPlayer->stanHeight;

                if ((dy > -300.0f) && (dy < 500.0f))
                {
                    targetPitch = ((mathAtan2(dy, groundDist) * 360.0f) / M_TAU_F) + -4.0f;

                    if (targetPitch >= 180.0f)
                    {
                        targetPitch -= 360.0f;
                    }

                    if (targetPitch > 0)
                    {
                        targetPitch *= 0.8666667f;
                    }
                }
            }
        }

        if ((g_CurrentPlayer->movecentrerelease) && (moveData.analogWalk < 40) && (moveData.analogWalk > -40))
        {
            g_CurrentPlayer->movecentrerelease = FALSE;
        }


        if (g_PlayerIsInTank == 0)
        {
            /**
             * If the player is giving manual pitch inputs, stop the automatic look ahead pitch adjust.
             */
            if ((moveData.speedVertaDown > 0) || (moveData.speedVertaUp > 0))
            {
                g_CurrentPlayer->docentreupdown = FALSE;
                g_CurrentPlayer->prevupdown = TRUE;
                g_CurrentPlayer->automovecentre = FALSE;
            }
            else
            {
                if (moveData.disableLookAhead)
                {
                    g_CurrentPlayer->automovecentre = FALSE;
                }
                else if (g_CurrentPlayer->automovecentreenabled)
                {
                    if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < -60)))
                    {
                        g_CurrentPlayer->automovecentre = TRUE;
                    }

                    /**
                     * If the player's camera pitch is 5 degrees above the target pitch or 10 degrees below the target pitch,
                     * and move centre is allowed, look ahead (docentreupdown) can be activated.
                     */
                    if ((g_CurrentPlayer->automovecentre)
                        && (( ((targetPitch + 5.0f) < g_CurrentPlayer->vv_verta)) || (g_CurrentPlayer->vv_verta < (targetPitch + -FLOAT_TEN_A)))
                        && (g_CurrentPlayer->movecentrerelease == FALSE))
                    {
                        g_CurrentPlayer->docentreupdown = TRUE;
                    }
                }
                /**
                 * fastmovecentreenabled is never set to true, so this block can never execute. Cut option?
                 */
                else if ((g_CurrentPlayer->fastmovecentreenabled)
                    && (moveData.canLookAhead)
                    && ((moveData.analogWalk > 60) || (moveData.analogWalk < -60))
                    && (( ((targetPitch + 5.0f) < g_CurrentPlayer->vv_verta)) || (g_CurrentPlayer->vv_verta < (targetPitch + -FLOAT_TEN_A)))
                    && (g_CurrentPlayer->movecentrerelease == FALSE))
                {
                    g_CurrentPlayer->docentreupdown = TRUE;
                }

                g_CurrentPlayer->prevupdown = FALSE;
            }
        }

        /**
         * If look ahead (docentreupdown) is active, adjust the player's camera pitch to the target pitch.
         */
        if (g_CurrentPlayer->docentreupdown)
        {
            if (g_PlayerIsInTank == 0)
            {
                limit = (g_CurrentPlayer->speedverta * g_CurrentPlayer->speedverta * 0.5f) / 0.05f;

                if ((targetPitch + limit) < g_CurrentPlayer->vv_verta)
                {
                    playerUpdateSpeedVerta(1.0f);
                }
                else if (g_CurrentPlayer->vv_verta < (targetPitch - limit))
                {
                    playerUpdateSpeedVerta(-1.0f);
                }
                else
                {
                    playerUpdateSpeedVerta(0);
                }

                limit = g_CurrentPlayer->vv_verta + (2.0f * (g_CurrentPlayer->speedverta * g_GlobalTimerDelta));

                if ((targetPitch < g_CurrentPlayer->vv_verta) && (targetPitch < limit))
                {
                    g_CurrentPlayer->vv_verta = limit;
                }
                else if ((g_CurrentPlayer->vv_verta < targetPitch) && (limit < targetPitch))
                {
                    g_CurrentPlayer->vv_verta = limit;
                }
                else
                {
                    g_CurrentPlayer->vv_verta = targetPitch;
                    g_CurrentPlayer->speedverta = 0;

                    if (g_CurrentPlayer->prevupdown == FALSE)
                    {
                        g_CurrentPlayer->docentreupdown = FALSE;
                    }
                }
            }
        }
        else
        {
            if (moveData.canNaturalPitch)
            {
                fovScale = viGetFovY() / 60.0f;
                pitchInput = (f32) moveData.analogPitch / 70.0f;

                if (pitchInput > 1)
                {
                    pitchInput = 1;
                }
                else if (pitchInput < -1)
                {
                    pitchInput = -1;
                }

                if (pitchInput >= 0)
                {
                    pitchInput *= pitchInput;
                }
                else
                {
                    pitchInput *= -pitchInput;
                }

                g_CurrentPlayer->speedverta = -pitchInput * fovScale;
            }
            else if (moveData.speedVertaDown > 0)
            {
                playerUpdateSpeedVerta(moveData.speedVertaDown);

                // Bug? This is true for every value except exactly 60.
                if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < 60)))
                {
                    g_CurrentPlayer->movecentrerelease = TRUE;
                }
            }
            else if (moveData.speedVertaUp > 0)
            {
                playerUpdateSpeedVerta(-moveData.speedVertaUp);

                if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < 60)))
                {
                    g_CurrentPlayer->movecentrerelease = TRUE;
                }
            }
            else
            {
                playerUpdateSpeedVerta(0);
            }

            g_CurrentPlayer->vv_verta += g_CurrentPlayer->speedverta * g_GlobalTimerDelta * 3.5f;

            if ((g_PlayerIsInTank == 1) && (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING) && (g_CurrentPlayer->vv_verta < -20.0f))
            {
                g_CurrentPlayer->vv_verta = -20.0f;
            }
        }
    }

    if (moveData.canNaturalTurn)
    {
        fovY = viGetFovY();
        turnInput = (f32) moveData.analogTurn / 70.0f;

        if (turnInput > 1)
        {
            turnInput = 1;
        }
        else if (turnInput < -1)
        {
            turnInput = -1;
        }

        if (turnInput >= 0)
        {
            turnInput *= turnInput;
        }
        else
        {
            turnInput *= -turnInput;
        }

        turnScale = (fovY / FOV_Y_F);
        g_CurrentPlayer->speedtheta = turnInput * turnScale;
    }
    else if (moveData.aimTurnLeftSpeed > 0)
    {
        playerUpdateSpeedTheta(moveData.aimTurnLeftSpeed);
    }
    else if (moveData.aimTurnRightSpeed > 0)
    {
        playerUpdateSpeedTheta(-moveData.aimTurnRightSpeed);
    }
    else
    {
        playerUpdateSpeedTheta(0);
    }

    if (g_PlayerIsInTank == 1)
    {
        if (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING)
        {
            turnTarget = 0;

            if (moveData.canNaturalTurn)
            {
                turnTarget = g_CurrentPlayer->speedtheta * 0.3f;
            }
            else if (moveData.aimTurnLeftSpeed > 0)
            {
                turnTarget = playerScaleLookSpeedTheta(1) * 0.3f;
            }
            else if (moveData.aimTurnRightSpeed > 0)
            {
                turnTarget = playerScaleLookSpeedTheta(-1) * 0.3f;
            }

            for (step=0; step<g_ClockTimer; step++)
            {
                g_TankTurnSpeed = (TANKUPDATEROTATION_SCALE * g_TankTurnSpeed) + turnTarget;
            }

            g_CurrentPlayer->speedtheta = g_TankTurnSpeed * TANK_VERT_ANGLE_RAD_FACTOR;
        }
        else
        {
            g_CurrentPlayer->speedtheta = 0;
        }
    }

    if (moveData.detonating)
    {
        g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_IDLE;
        g_CurrentPlayer->hands[GUNRIGHT].weapon_current_animation = 0;
        remoteMinesDetonate();
    }

    canCycleWeapons = 1;

    if ((isMpGame()) && (frontGetMpScenario() == 2) && (invHasFlagToken()))
    {
        canCycleWeapons = 0;
    }

    if (canCycleWeapons)
    {
        if (moveData.weaponBackOffset)
        {
            gunCycleBackward();
        }

        if (moveData.weaponForwardOffset)
        {
            gunCycleForward();
        }
    }

    if (moveData.canSwivelGun)
    {
        g_CurrentPlayer->controldef = CONTROLLER_CONFIG_HONEY;
    }
    else if (moveData.canManualAim)
    {
        g_CurrentPlayer->controldef = CONTROLLER_CONFIG_KISSY;
    }

    if (g_CurrentPlayer->controldef == CONTROLLER_CONFIG_HONEY)
    {
        if (moveData.canAutoAim
            && currentPlayerGetXAutoAimEnabledRedirect()
            && g_CurrentPlayer->autoaim_target_x
            && weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_HAS_AUTO_AIM))
        {
            swivelX = g_CurrentPlayer->autoaimx;
        }
        else
        {
            swivelX = g_CurrentPlayer->speedtheta * 0.3f;
        }

        if (moveData.canAutoAim
            && currentPlayerGetYAutoAimEnabledRedirect()
            && g_CurrentPlayer->autoaim_target_y
            && weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_HAS_AUTO_AIM))
        {
            swivelY = g_CurrentPlayer->autoaimy;
        }
        else
        {
            swivelY = -g_CurrentPlayer->speedverta * 0.1f;
        }

        gunUpdateCrosshairSwivel(swivelX, swivelY, MAX_AIMLOCK_SPEED_DEFAULT);
    }
    else if (g_CurrentPlayer->controldef == CONTROLLER_CONFIG_KISSY)
    {
        gunUpdateCrosshairManual(((f32) moveData.controlStickXRaw * 0.65f) / 80.0f, ((f32) moveData.controlStickYRaw * 0.65f) / 80.0f);
    }
}


/**
 * NTSC address 7F083FC8.
 * NTSC-J address 7F0845D8.
 * EU address 7F08406C.
*/
void playerTickDamageAndHealth(void)
{
    // update damage showtime
    if (g_CurrentPlayer->damageshowtime >= 0)
    {
        // 0: This is the first frame of damage
        if (g_CurrentPlayer->damageshowtime == 0)
        {
            gunSetGunAmmoVisible(GUNAMMOREASON_DAMAGE, FALSE);
            gunSetSightVisible(GUNSIGHTREASON_DAMAGE, FALSE);
            hudmsgsSetOff(4);
            hudmsgTopSetFlag(PLAYERFLAG_NOTIMER);
            countdownTimerSetVisible(COUNTDOWN_HIDE_DAMAGE, FALSE);

            g_CurrentPlayer->damagetype = (s32)(currentPlayerGetHealth() * 8.0f);

            if (g_CurrentPlayer->damagetype >= 8)
            {
                g_CurrentPlayer->damagetype = 7;
            }

// Ensure we don't read out of bounds of the g_DamageTypes array.
            if (g_CurrentPlayer->damagetype < 0)
            {
                g_CurrentPlayer->damagetype = 0;
            }
        }

#ifdef VERSION_US
        if (
            (g_DamageTypes[g_CurrentPlayer->damagetype].fadeEndFrame >= g_CurrentPlayer->damageshowtime)
            || (g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame >= g_CurrentPlayer->damageshowtime))
        {
            if (!g_CurrentPlayer->bonddead)
            {
#else
        if (!g_CurrentPlayer->bonddead
            && (
                (g_DamageTypes[g_CurrentPlayer->damagetype].fadeEndFrame >= g_CurrentPlayer->damageshowtime)
                || (g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame >= g_CurrentPlayer->damageshowtime)))
        {
#endif
            if (g_CurrentPlayer->damageshowtime >= g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame
                && g_CurrentPlayer->damageshowtime <= g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame)
            {
                f32 frac;
#ifdef VERSION_US
                s32 flashdoneframes;
                s32 totalframes;
                s32 flashfullframe;
#else
                f32 flashdoneframes;
                f32 totalframes;
                f32 flashfullframe;
#endif

                flashdoneframes = g_CurrentPlayer->damageshowtime - g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame;
                flashfullframe = g_DamageTypes[g_CurrentPlayer->damagetype].flashFullFrame;
                totalframes = g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame - g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame;

                if (flashdoneframes < flashfullframe)
                {
                    frac = (g_DamageTypes[g_CurrentPlayer->damagetype].maxAlpha * (f32)flashdoneframes) / (f32)flashfullframe;
                }
                else
                {
                    frac = (g_DamageTypes[g_CurrentPlayer->damagetype].maxAlpha * (f32)(totalframes - flashdoneframes)) / (f32)(totalframes - flashfullframe);
                }

                currentPlayerSetFadeColour(
                    g_DamageTypes[g_CurrentPlayer->damagetype].red,
                    g_DamageTypes[g_CurrentPlayer->damagetype].green,
                    g_DamageTypes[g_CurrentPlayer->damagetype].blue,
                    frac);
            }
#ifdef VERSION_US
            }
#endif

            if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
            {
#if defined(VERSION_US)
                g_CurrentPlayer->damageshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->damageshowtime += g_GlobalTimerDelta;
#endif
            }
            else
            {
#if defined(VERSION_US)
                g_CurrentPlayer->damageshowtime += g_FrameDelta;
#else
                g_CurrentPlayer->damageshowtime += g_FrameDelta60;
#endif
            }
        }
        else /* (damage showtime is over) */
        {
            g_CurrentPlayer->damageshowtime = -1;
            currentPlayerSetFadeColour(0xFF, 0xFF, 0xFF, 0);

            if (!g_CurrentPlayer->bonddead)
            {
                gunSetGunAmmoVisible(GUNAMMOREASON_DAMAGE, TRUE);
                gunSetSightVisible(GUNSIGHTREASON_DAMAGE, TRUE);
                hudmsgsSetOn(4);
                hudmsgTopClearFlag(PLAYERFLAG_NOTIMER);
                countdownTimerSetVisible(COUNTDOWN_HIDE_DAMAGE, TRUE);
            }
        }
    }

    // update health showtime
    if (g_CurrentPlayer->healthshowtime >= 0)
    {
        // 0: This is the first frame of damage
        if (g_CurrentPlayer->healthshowtime == 0)
        {
            g_CurrentPlayer->healthdamagetype = (s32)(currentPlayerGetHealth() * 8.0f);

            if (g_CurrentPlayer->healthdamagetype >= 8)
            {
                g_CurrentPlayer->healthdamagetype = 7;
            }

            if (g_CurrentPlayer->healthdamagetype < 0)
            {
                g_CurrentPlayer->healthdamagetype = 0;
            }
        }

        if (!g_CurrentPlayer->bonddead)
        {
            if ((g_CurrentPlayer->healthshowtime >= g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].validStartFrame)
                && (g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].updateToRealHealthFrame >= g_CurrentPlayer->healthshowtime))
            {
                g_CurrentPlayer->apparenthealth = g_CurrentPlayer->oldhealth;
                g_CurrentPlayer->apparentarmour = g_CurrentPlayer->oldarmour;
#if defined(VERSION_US)
                g_CurrentPlayer->healthshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->healthshowtime += g_GlobalTimerDelta;
#endif
            }
            else if ((g_CurrentPlayer->healthshowtime >= g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].validStartFrame)
                && (g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].hideHealthFrame >= g_CurrentPlayer->healthshowtime))
            {
                g_CurrentPlayer->apparenthealth = g_CurrentPlayer->bondhealth;
                g_CurrentPlayer->apparentarmour = g_CurrentPlayer->bondarmour;
#if defined(VERSION_US)
                g_CurrentPlayer->healthshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->healthshowtime += g_GlobalTimerDelta;
#endif
            }
            else
            {
                g_CurrentPlayer->healthshowtime = -1;
            }
        }
        else
        {
            g_CurrentPlayer->healthshowtime = -1;
        }
    }
}

/**
 * If global flag g_SurroundBondWithExplosionsFlag is set then explosions
 * will be randomly created around Bond.
 * NTSC address 7F084360.
 * EU address 7F0844A4.
*/
void playerTickExplode(void)
{
    g_PlayerTickExplodeCreatePosition++;

    if (g_SurroundBondWithExplosionsFlag
        && (g_PlayerInvincible == FALSE)
        && g_SurroundBondWithExplosionsTicks < g_GlobalTimer)
    {
        coord3d pos;

        pos.f[0] = g_CurrentPlayer->prop->pos.f[0];
        pos.f[1] = g_CurrentPlayer->prop->pos.f[1];
        pos.f[2] = g_CurrentPlayer->prop->pos.f[2];

        switch (g_PlayerTickExplodeCreatePosition % 4)
        {
            case 0: pos.x += 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 1: pos.x -= 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 2: pos.z += 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 3: pos.z -= 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
        }

        pos.y += 200.0f * RANDOMGETNEXT_F32() - 100.0f;

        explosionCreate(0, &pos, g_CurrentPlayer->prop->stan, EXPLOSION_DEF_PLAYER, 0, 0, g_CurrentPlayer->prop->rooms, 0);

        g_SurroundBondWithExplosionsTicks = (randomGetNext() % (u32)PLAYER_TICKEXPLODE_FACTOR) + g_GlobalTimer + PLAYER_TICKEXPLODE_FACTOR;
    }
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


s16 viGetFrameWidth(void)
{
    if (cameraBufferToggle != 0)
    {
        return SCREEN_WIDTH_440;
    }

    return SCREEN_WIDTH_320;
}


s16 viGetFrameHeight(void)
{
    if (cameraBufferToggle != 0)
    {
        return SCREEN_HEIGHT_330;
    }

    return SCREEN_HEIGHT;
}

s16 playerGetViewportWidth(void)
{
    if (viewIsHalfWidth())
    {
        return VIEWPORT_WIDTH_4P;
    }

    if (cameraBufferToggle != 0)
    {
        return SCREEN_WIDTH_440;
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
    {
        return VIEWPORT_WIDTH_WIDESCREEN;
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
    {
        return VIEWPORT_WIDTH_CINEMA;
    }

    return VIEWPORT_WIDTH_FULLSCREEN;
}

s16 playerGetViewportLeft(void)
{
    if (viewIsHalfWidth())
    {
        // Counter-op: each view stays where its controller's player sits.
        if ((coopPlayerPerson(playerGetCurrentNum()) == 1) || (playerGetCurrentNum() == 3))
        {
                return 0xa1;
        }
    }

    return 0;
}


s16 playerGetViewportHeight(void)
{
    f32 t;

    // Co-op side by side views are full height: see the single view sizes.
    if ((getViewCount() >= 2) && !coopSplitSideBySide())
    {
        return VIEWPORT_HEIGHT_4P;
    }

    if (cameraBufferToggle != 0)
    {
        if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
        {
            return VIEWPORT_HEIGHT_WIDESCREEN;
        }
        else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
        {
            return VIEWPORT_HEIGHT_CINEMA;
        }
        else
        {
            return VIEWPORT_HEIGHT_FULLSCREEN;
        }
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
    {
        t = watchGetAnimFrac();
        return (s16) ((s32) (WIDESCREEN_SCALE_FACTOR * t) + VIEWPORT_OFFSET_HEIGHT_WIDESCREEN);
    }
    else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
    {
        t = watchGetAnimFrac();
        return (s16) ((s32) (CINEMA_SCALE_FACTOR * t) + VIEWPORT_OFFSET_HEIGHT_CINEMA);
    }
    else
    {
        return VIEWPORT_HEIGHT_DEFAULT;
    }
}


s16 playerGetViewportTop(void)
{
    f32 t;

    if ((getViewCount() == 2) && !coopSplitSideBySide())
    {
        if (coopPlayerPerson(playerGetCurrentNum()) == 0)
        {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_2P_PLAYER_1;
#endif
        }

        return VIEWPORT_ULY_2P_PLAYER_2;
    }

    if (getViewCount() >= 3)
    {
        if (playerGetCurrentNum() < 2)
        {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_4P_PLAYER_12;
#endif
        }

        return VIEWPORT_ULY_4P_PLAYER_34;
    }

    if (cameraBufferToggle != 0)
    {
        if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
        {
            return VIEWPORT_ULY_CAM_WIDESCREEN;
        }
        else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
        {
            return VIEWPORT_ULY_CAM_CINEMA;
        }
        else
        {
            return VIEWPORT_ULY_CAM_FULLSCREEN;
        }
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
    {
        t = watchGetAnimFrac();
        return (s16) ((s32) (WIDESCREEN_ULY_SCALE_FACTOR * t) + VIEWPORT_ULY_WIDESCREEN_OFFSET);
    }
    else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
    {
        t = watchGetAnimFrac();
        return (s16) ((s32) (CINEMA_ULY_SCALE_FACTOR * t) + VIEWPORT_ULY_CINEMA_OFFSET);
    }
    else
    {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_DEFAULT;
#endif
    }
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


void camSetRoomOrigin(s32 room)
{
    roomGetWorldPos(room, &g_CurrentPlayer->current_model_pos);
    roomMtxSetOriginRoom(room);
}


void camStorePrevOrigin(void) {
    g_CurrentPlayer->previous_model_pos.f[0] = g_CurrentPlayer->current_model_pos.f[0];
    g_CurrentPlayer->previous_model_pos.f[1] = g_CurrentPlayer->current_model_pos.f[1];
    g_CurrentPlayer->previous_model_pos.f[2] = g_CurrentPlayer->current_model_pos.f[2];
    mtx4RotateVecInPlace(camGetWorldToScreenMtxf(), &g_CurrentPlayer->previous_model_pos);
}


void camUpdateMatrices(coord3d* cam_pos, coord3d* cam_look_dir, coord3d* cam_up)
{
    Mtx lookatmtx;
    LookAt *lookat;
    Mtxf lookmtx;
    coord3d clpos;
    coord3d scaledpos;
    f32 scale;
    Mtx *projview;
    Mtxf *projmtx;
    Mtxf projviewf;
    Mtx *worldtoview;
    Mtxf *worldtoscreen;
    Mtxf *viewtoworld;
    s32 j;
    s32 i;

    i = playerGetRoom();
    camSetRoomOrigin(i);

    worldtoview = dynAllocateMatrix();
    worldtoscreen = (Mtxf *) dynAllocateMatrix();
    viewtoworld = (Mtxf *) dynAllocateMatrix();

    lookat = (LookAt *)dynAllocateLights(2); // a LookAt is two Lights

    scale = g_CamPosScale;

    scaledpos.x = (cam_pos->x - g_CurrentPlayer->current_model_pos.x) * scale;
    scaledpos.y = (cam_pos->y - g_CurrentPlayer->current_model_pos.y) * scale;
    scaledpos.z = (cam_pos->z - g_CurrentPlayer->current_model_pos.z) * scale;

    clpos.f[0] = scaledpos.f[0] + cam_look_dir->f[0];
    clpos.f[1] = scaledpos.f[1] + cam_look_dir->f[1];
    clpos.f[2] = scaledpos.f[2] + cam_look_dir->f[2];

    mtx4LoadLookDir(&lookmtx,
        scaledpos.x, scaledpos.y, scaledpos.z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    guLookAtReflect(&lookatmtx, lookat,
        scaledpos.x, scaledpos.y, scaledpos.z,
        clpos.x, clpos.y, clpos.z,
        cam_up->x, cam_up->y, cam_up->z);

    mtx4LoadLookDir(worldtoscreen,
        cam_pos->x, cam_pos->y, cam_pos->z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    mtx4LoadFacingDir(viewtoworld,
        cam_pos->x, cam_pos->y, cam_pos->z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    projview = dynAllocateMatrix();

    projmtx = camGetProjectionMtxf();
    mtx4Mult(projmtx, &lookmtx, &projviewf);

	for (i = 0; i < 4; i++)
    {
		for (j = 0; j < 4; j++)
        {
			if (projviewf.m[i][j] > 32000.0f)
            {
				projviewf.m[i][j] = 32000.0f;
			}
            else if (projviewf.m[i][j] < -32000.0f)
            {
				projviewf.m[i][j] = -32000.0f;
			}
		}
	}

    guMtxF2L((f32 (*)[4]) &projviewf, projview);
    camSetProjViewMtx(projview);

    scale = bgGetLevelVisibilityScale();

    mtx4ScaleRows012(scale, &lookmtx);
    guMtxF2L((f32 (*)[4]) &lookmtx, worldtoview);

    camSetWorldToViewMtx(worldtoview);
    camSetWorldToScreenMtxf(worldtoscreen);
    camSetViewToWorldMtxf(viewtoworld);

    camSetLookAt(lookat);
    camUpdateFrustumPlanes();
    camStorePrevOrigin();
}


Gfx *camSetupFrame(Gfx *gdl)
{
    coord3d cam_pos;
    coord3d cam_look;
    coord3d cam_up;
    struct PlayerBond *collision;
    coord3d shake;
    coord3d vec;
    coord3d zeropos;
    f32 vec_y;
    f32 horizontal_len;
    f32 angle;
    f32 vertical_rot;
    f32 ft4;

#if defined(VERSION_EU)
    if (bossGetStageNum() == LEVELID_CUBA)
    {
        if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
        {
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
        }
        else if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
        {
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
        }
        else
        {
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
        }
    }
#endif

    if (g_CurrentPlayer->cameramode == 1) {
        cam_pos.x = g_CurrentPlayer->pos.x;
        cam_pos.y = g_CurrentPlayer->pos.y;
        cam_pos.z = g_CurrentPlayer->pos.z;

        cam_look.x = g_CurrentPlayer->pos2.x - g_CurrentPlayer->pos.x;
        cam_look.y = g_CurrentPlayer->pos2.y - g_CurrentPlayer->pos.y;
        cam_look.z = g_CurrentPlayer->pos2.z - g_CurrentPlayer->pos.z;

        cam_up.x = g_CurrentPlayer->offset.x;
        cam_up.y = g_CurrentPlayer->offset.y;
        cam_up.z = g_CurrentPlayer->offset.z;
    } else {
        collision = &g_CurrentPlayer->bond;

        shake = ZeroCoordShake;

        if (!g_CurrentPlayer->bonddead) {
            explosionScreenShake(
                &collision->pos,
                &collision->applied_view,
                &shake
            );
        } else {
            viShake(0.0f);
        }

        cam_pos.x = collision->pos.x;
        cam_pos.y = collision->pos.y;
        cam_pos.z = collision->pos.z;

        cam_look.x = collision->applied_view.x;
        cam_look.y = collision->applied_view.y;
        cam_look.z = collision->applied_view.z;

        cam_up.x = collision->applied_view2.x;
        cam_up.y = collision->applied_view2.y;
        cam_up.z = collision->applied_view2.z;
    }

    camUpdateMatrices(&cam_pos, &cam_look, &cam_up);
    gunGetAimRay(&zeropos, &vec);


    vec_y = vec.y;
    horizontal_len = sqrtf((vec.z * vec.z) + (vec.x * vec.x));
    vertical_rot = bondviewGetPlayerPitchRadians();
    ft4 = mathAtan2(vec_y, horizontal_len) + vertical_rot;

    if (ft4 >= M_PI_F) {
        ft4 -= M_TAU_F;
    }
    g_CurrentPlayer->bodyAimPitch = ft4;

    angle = mathAtan2(-vec.x, -vec.z);
    if (angle >= M_PI_F) {
        angle -= M_TAU_F;
    }
    g_CurrentPlayer->bodyAimYaw = angle;

    return gdl;
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


void camSetPosScale(f32 scale)
{
  g_CamPosScale = scale;
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


void playerDie(void)
{
    if ((g_CurrentPlayer->cheatBondInvincible == 0) && (g_CurrentPlayer->bonddead == FALSE))
    {
        if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
        {
            watchTrigger(1);
        }

        if (g_CoopMode)
        {
            g_CoopDeathTime[playerGetCurrentNum()] = g_GlobalTimer;
        }

        // In co-op the mission carries on while the partner is alive.
        if (!coopPartnerAlive())
        {
            g_FrontBondKilled = 1;
        }

        g_CurrentPlayer->bonddead = 1;

        g_CurrentPlayer->bondDie = g_CurrentPlayer->bond;

        g_CurrentPlayer->thetadie = g_CurrentPlayer->vv_theta;
        g_CurrentPlayer->vertadie = g_CurrentPlayer->vv_verta;

        if ((g_PlayerTankProp != NULL) && !coopTankUsedByPartner())
        {
            g_ExplodeTankOnDeathFlag = 1;
        }

        // Counter-op: the guard player drops their gun like a guard.
        if (counterOpIsGuardPlayer(playerGetCurrentNum()) && (g_CurrentPlayer->prop->chr != NULL))
        {
            playerDropInventory();
        }

        gunEquip(GUNLEFT, 0);
        gunEquip(GUNRIGHT, 0);

        if ((lvGetMissionTime60() - g_CurrentPlayer->lifestarttime60) < g_playerPerm->shortest_inning)
        {
            g_playerPerm->shortest_inning = lvGetMissionTime60() - g_CurrentPlayer->lifestarttime60;
        }

        g_CurrentPlayer->lifestarttime60 = lvGetMissionTime60();
    }
}


/**
 * @param damage_amount: damage amount
 * @param vectorx: damage source x coordinate
 * @param vectorz: damage source y coordinate
 * @param playerid: player index of player causing the damage
 * @param arg4: boolean, does the damage apply to body armor (e.g. false when gas)
 *
 * Address US 7F08991C.
 * Address EU 7F089A84.
 * Address JP 7F089FF0.
 */
void playerTakeDamage(f32 damage_amount, f32 vectorx, f32 vectorz, s32 playerid, s32 affects_armor) {
    f32 damage_dealt = g_playerPerm->handicap * damage_amount;
    s32 cur_player_num;
    f32 angle;
    s32 prevplayer;
    s32 hadgoldengun;

    if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
    {
        hudMakeDamageSegments(g_CurrentPlayer->armor_display_values, 1, currentPlayerGetArmor());
        hudMakeDamageSegments(g_CurrentPlayer->health_display_values, -1, currentPlayerGetHealth());
    }

    if (!isMpGame() || (g_MpStopPlay == 0 && g_MpGameOver == 0))
    {
        if (g_PlayerIsInTank == 1)
        {
            damage_dealt *= 0.25f;
        }

        if (g_CurrentPlayer->bonddead == FALSE && g_CurrentPlayer->cheatBondInvincible == FALSE)
        {
            joyRumblePakStart(playerGetCurrentNum(), 0.25);
            if (optionsGetControlStyle() >= 4)
            {
                // rumble second controller in 2.x
                joyRumblePakStart(playerGetCurrentNum() + getPlayerCount(), 0.25);
            }
        }

        if (isMpGame() && frontGetMpScenario() == SCENARIO_LTK)
        {
            // the damage dealt is always equivalent to how much health and armor the player has
            // the result of this is to always kill the player regardless of how much damage he can sustain
            damage_dealt = (g_CurrentPlayer->bondhealth * g_CurrentPlayer->actual_health) + (g_CurrentPlayer->bondarmour * g_CurrentPlayer->actual_armor);
        }

        if (g_CurrentPlayer->cheatBondInvincible == FALSE && g_CurrentPlayer->bonddead == FALSE && g_PlayerInvincible == FALSE &&
            (g_CurrentPlayer->damageshowtime < 0 || (isMpGame() && g_CurrentPlayer->damageshowtime == 0)
                // Counter-op: like the guard they took over, the guard
                // player feels every shot, not one per damage flash.
                || counterOpIsGuardPlayer(playerGetCurrentNum())))
        {
            if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x5 && g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0xc)
            {
                g_CurrentPlayer->oldhealth = g_CurrentPlayer->bondhealth;
                g_CurrentPlayer->oldarmour = g_CurrentPlayer->bondarmour;

                if (isMpGame())
                {
                    cur_player_num = playerGetCurrentNum();
                    angle = g_playerPointers[cur_player_num]->vv_theta - (360.0f - ((mathAtan2(vectorx, vectorz) * 180.0f) / 3.1415927f));

                    if (angle < 0.0f)
                    {
                        angle = -angle;
                    }

                    if (angle < 90.0f || angle > 270.0f)
                    {
                        // danger: if Bond could be damaged by toxic gas in multiplayer, playerid would be -1
                        // thus causing an out of bounds access
                        g_playerPlayerData[playerid].damage_to_backside++;
                    }
                }

                if (affects_armor && damage_dealt <= g_CurrentPlayer->bondarmour * g_CurrentPlayer->actual_armor)
                {
                    g_CurrentPlayer->bondarmour = g_CurrentPlayer->bondarmour - (damage_dealt / g_CurrentPlayer->actual_armor);
                }
                else
                {
                    if (affects_armor)
                    {
                        damage_dealt -= g_CurrentPlayer->bondarmour / g_CurrentPlayer->actual_armor;
                        g_CurrentPlayer->bondarmour = 0.0f;
                        g_CurrentPlayer->actual_armor = 1.0f;
                    }

                    g_CurrentPlayer->bondhealth = g_CurrentPlayer->bondhealth - (damage_dealt / g_CurrentPlayer->actual_health);

                    if (g_CurrentPlayer->bondhealth <= 0.0f)
                    {
                        if (isMpGame())
                        {
                            prevplayer = playerGetCurrentNum();
                            hadgoldengun = 0;

                            if (frontGetMpScenario() == 3 && invHasGoldenGun())
                            {
                                hadgoldengun = 1;
                            }

                            playerDropInventory();
                            if (prevplayer != playerid)
                            {
                                statsAddDeath();
                            }

                            playerSetCurrent(playerid);

                            if (prevplayer == playerid)
                            {
                                statsAddSuicide();
                            }
                            else
                            {
                                statsAddKill();

                                if (hadgoldengun != 0)
                                {
                                    statsAddGoldenGunKill();
                                }
                            }

                            playerSetCurrent(prevplayer);


                            g_playerPlayerData[playerid].kill_counts[prevplayer]++;
                        }
                        else if (g_CounterOpMode && (playerid >= 0) && (playerid < getPlayerCount())
                                 && (playerid != playerGetCurrentNum()))
                        {
                            // Counter-op: killing the other player counts
                            // as a kill, as killing a guard does.
                            prevplayer = playerGetCurrentNum();
                            playerSetCurrent(playerid);
                            statsAddKill();
                            playerSetCurrent(prevplayer);
                        }

                        playerDie();
                    }
                }

                if (g_CurrentPlayer->damageshowtime < 0)
                {
                    g_CurrentPlayer->bondshotspeed.x = g_CurrentPlayer->bondshotspeed.x + 2.0f * vectorx;
                    g_CurrentPlayer->bondshotspeed.z = g_CurrentPlayer->bondshotspeed.z + 2.0f * vectorz;
                }

                g_CurrentPlayer->damageshowtime = 0;
                g_CurrentPlayer->healthshowtime = 0;


                if (!lvGetControlsLocked())
                {
                    sndPlaySfx(g_musicSfxBufferPtr, BOND_GET_HIT1_SFX, 0);
                }
            }
        }
    }
}


/**
 * @param damage_amount: damage amount
 * @param rad:  damage source angle
 * @param player_id: player index of player causing the damage
 * @param affects_armor: boolean, does the damage apply to body armor (e.g. false when gas)
 */
void playerTakeDamageFromAngle(f32 damage_amount, f32 angle, s32 playerid, s32 affects_armor)
{
    playerTakeDamage(damage_amount, sinf(angle), cosf(angle), playerid, affects_armor);
}


int playerIsShowingDamage(void)
{
    return (g_CurrentPlayer->damageshowtime >= (s32)0);
}


int playerIsShowingHealth(void)
{
    return (g_CurrentPlayer->healthshowtime > (s32)0);
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


void playerSetInvincible(u32 invincible) {
    g_CurrentPlayer->cheatBondInvincible = invincible;
}


u8 playerGetInvincible(void) {
    return g_CurrentPlayer->cheatBondInvincible;
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


f32 currentPlayerGetHealth(void)
{
    return g_CurrentPlayer->bondhealth;
}


f32 currentPlayerGetArmor(void)
{
  return g_CurrentPlayer->bondarmour;
}


void currentPlayerAddArmor(f32 armour)
{
    g_playerPerm->body_armor_pickups += armour;
    g_CurrentPlayer->bondarmour = armour;
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


Gfx* camLoadWorldToViewMtx(Gfx* gdl)
{
    gSPMatrix(gdl++, camGetWorldToViewMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    return gdl;
}


void mtxsF2L(RenderPosView * views, s32 count)
{
    Mtxf mtx;
    s32 i;

    for (i = 0; i < count; i++)
    {
        mtx4Copy(&views[i].pos, &mtx);
        mtxF2L(&mtx, &views[i].pos);
    }
}


void camMtxsToRoomSpace(Mtxf *matrices, s32 count)
{
    Mtxf mtx;
    s32 i;
    s32 j;

    for (i = 0, j = 0; i < count; i++, j += sizeof(Mtxf))
    {
        mtx4MultAffine(camGetViewToWorldMtxf(), (Mtxf *)((u32)matrices + j), &mtx);

        mtx.m[3][0] -= g_CurrentPlayer->current_model_pos.f[0];
        mtx.m[3][1] -= g_CurrentPlayer->current_model_pos.f[1];
        mtx.m[3][2] -= g_CurrentPlayer->current_model_pos.f[2];

        mtxF2L(&mtx, matrices + i);
    }
}


s32 lvGetMissionTime60(void) {
#ifdef VERSION_EU
    return (mission_timer * 60) / 50;
#else
    return mission_timer;
#endif
}


void playerSurroundWithExplosions(int delay)
{
    g_SurroundBondWithExplosionsFlag = 1;
    g_SurroundBondWithExplosionsTicks = delay + g_GlobalTimer;
    g_PlayerTickExplodeCreatePosition = 0;
}
