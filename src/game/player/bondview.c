/**
 * The player globals. Also choosing a spawn pad, resetting a player for a new life
 * (playerResetLife), crouch, lean and setting the player's position. The camera
 * helpers are in camview.c. See docs/subsystems/player.md.
 */
#include <ultra64.h>
#include <math.h>
#include <bondtypes.h>
#include <boss.h>
#include <fr.h>
#include <joy.h>
#include <music.h>
#include <snd.h>
#include <str.h>
#include "game/menu/watchmenu.h"
#include "game/world/bg.h"
#include "game/world/roommtx.h"
#include "game/hud/blood.h"
#include "game/player/bondhead.h"
#include "game/player/bondinv.h"
#include "game/player/bondview.h"
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
#include "game/nav/stan.h"
#include "game/nav/stan.h"
#include "game/text/text.h"

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




/**
 * When set, will increment each tick until reaching a threshold value (4).
 * Then current items will be unequipped from left and run hands.
*/
s32 g_bondviewForceDisarm = 0;

s32 resolution = 0;
s32 cameraBufferToggle = 0;
s32 cameraFrameCounter1 = 0;
s32 cameraFrameCounter2 = 0;
s32 g_CreditsScrollFrame = 0;
s32 credits_state = 0;
CreditsEntry *credits_pointer = NULL;
s32 g_SurroundBondWithExplosionsFlag = 0;

s32 g_PlayerIsInTank = 0;

struct PropRecord *g_WorldTankProp = NULL;

//D:80036450 cannonically bondonprop2
struct PropRecord *g_PlayerTankProp = NULL;

/**
 * Related to g_PlayerTankProp.
 */
f32 g_PlayerTankYOffset = 0;

/**
 * US address 80036458.
*/
ALSoundState * g_TankSfxState[2] = { NULL, NULL };

/**
 * min -3.749999, max +3.749999
*/
f32 g_TankTurnSpeed = 0;

f32 g_TankOrientationAngle = 0;

f32 tank_turret_unused_angle = 0.0f;

/**
 * Argument to sinf,cosf.
 */
f32 g_TankTurretVerticalAngle = 0;

f32 g_TankTurretVerticalAngleRelated = 0;

f32 g_TankTurretOrientationAngleRad = 0;

f32 g_TankTurretOrientationAngleDeg = 0;

f32 tank_turret_turn_speed = 0;

/**
 * Can enter tank, remains set once Bond is in tank.
 * Address 0x80036480.80036480
*/
s32 g_BondCanEnterTank = 0;

f32 g_TankTurretAngle = 0;

f32 g_TankTurretTurn = 0;

s32 g_ExplodeTankOnDeathFlag = 0;
s32 g_TankDamagePenaltyTicks = 0;
enum CAMERAMODE g_CameraMode = CAMERAMODE_NONE;
enum CAMERAMODE g_CameraAfterCinema = CAMERAMODE_NONE;
s32 camera_fade_active = 0;
s32 stop_time_flag = 0;
f32 camera_transition_timer = 0;
s32 intro_camera_index = 1;
struct SetupIntroSwirl *g_IntroSwirl = NULL;
s32 is_timer_active = 1;
bool g_PlayerInvincible = FALSE;
struct SetupIntroCamera* g_CurrentSetupIntroCamera = NULL;
s32 g_SetupIntroCameraCount = 0;
struct SetupIntroCamera *g_IntroCamera = NULL;

/**
 * Flag to toggle invisibility cheat.
 * 1 = visible to guards
 * 0 = not visible to guards
 */
s32 g_VisibleToGuardsFlag = TRUE;

s32 g_PlayerCollisionOn = TRUE;
f32 g_CamPosScale = 1.0;
f32 g_PlayerWorldScale = 1.0;
f32 g_PlayerWorldScaleInv = 1.0;

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

enum CAMERAMODE camera_mode = CAMERAMODE_NONE;
s32 g_IntroAnimationIndex = 0;

struct struct_4 stage_intro_anim_table[] = {
    {PTR_ANIM_extending_left_hand, 95.0, -1.0, 0.02},
    {PTR_ANIM_fire_standing_draw_one_handed_weapon_fast, 7.0, 40.0, 0.5},
    {PTR_ANIM_draw_one_handed_weapon_and_look_around, 0.0, -1.0, 0.5},
    {PTR_ANIM_draw_one_handed_weapon_and_stand_up, 0.0, -1.0, 0.5},
    {PTR_ANIM_aim_one_handed_weapon_left_right, 0.0, -1.0, 0.5},
    {PTR_ANIM_cock_one_handed_weapon_and_turn_around, 0.0, -1.0, 0.5},
    {PTR_ANIM_cock_one_handed_weapon_turn_around_and_stand_up, 0.0, -1.0, 0.5},
    {PTR_ANIM_draw_one_handed_weapon_and_turn_around, 0.0, -1.0, 0.5},
    {PTR_ANIM_bond_eye_fire_alt, 0.0, -1.0, 0.5}
};

f32 watch_transition_time = 0.90909088;

WeaponObjRecord dummy_08_pp7_obj[] = {
    {{{0x0100,
    0x00,
    0x08},
    PROP_CHRWPPK,
    0x4000,
    0x00000000,
    0x00000000,
    NULL,
    NULL,
    {
        {{1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}}
    },
    {{0.0f, 0.0f, 0.0f}},
    {0x00000000},
    NULL,
    {NULL},
    0.0f,
    1000.0f,
    {{0xff, 0xff, 0xff, 0x00}},
    {{0xff,
    0xff,
    0xff,
    0x00}}},
    ITEM_WPPK,
    -1,
    -1,
    NULL}
};


struct DamageType g_DamageTypes[] = {
        {   0,    10.0,    60.0,    0.6,    0,    5.0,    40.0,    1.0,            0xFF,       0xFF,       0xFF}, // 1 bars
        {   0,    10.0,    60.0,    0.6,    0,    5.0,    40.0,    1.0,            0xFF,       0xFF,       0xFF},
        {   0,    10.0,    50.0,    0.6,    0,    5.0,    30.0,    0.800000011921, 0xFF,       0xFF,       0xFF},
        {   0,    10.0,    40.0,    0.6,    0,    5.0,    25.0,    0.600000023842, 0xFF,       0xFF,       0xFF},
        {   0,    10.0,    35.0,    0.6,    0,    5.0,    22.0,    0.550000011921, 0xFF,       0xFF,       0xFF},
        {   0,    10.0,    30.0,    0.6,    0,    5.0,    19.0,    0.5,            0xFF,       0xFF,       0xFF},
        {   0,    10.0,    30.0,    0.6,    0,    5.0,    17.0,    0.449999988079, 0xFF,       0xFF,       0xFF},
        {   0,    10.0,    30.0,    0.6,    0,    5.0,    15.0,    0.40000000596,  0xFF,       0xFF,       0xFF}  // 8 bars
};


/**
 * The second column is how many frames before the gauge switches from showing the old health to the new health.
 * The third column is how many frames before the health display is hidden.
 */
struct HealthDisplayDuration g_HealthDisplayDurations[8] = {
    { 0, 40, 100 }, // 1 bar of health
    { 0, 30, 80 },
    { 0, 20, 60 },
    { 0, 20, 60 },
    { 0, 20, 60 },
    { 0, 20, 50 },
    { 0, 20, 50 },
    { 0, 20, 50 }  // 8 bars of health
};

/**
 * US Address 0x800367F4.
*/
coord3d g_DefaultMoveBondOffset = { 0 };

/**
 * struct player property `pos` .
 * US address 80036800.
 */
coord3d g_DefaultFrozenPlayerPos = { 0 };

/**
 * struct player property `pos2`.
 * US address 8003680C.
 */
coord3d g_DefaultFrozenPlayerPos2 = { {0, 0, 1.0f} };

/**
 * struct player property `offset`.
 * US address 80036818.
 */
coord3d g_DefaultFrozenPlayerOffset = { {0, 1.0f, 0} };

/**
 * struct player property `offset`.
 * US address 80036824.
 */
coord3d g_DefaultFrozenMoveOffset = { 0 };

coord3d ZeroCoordShake = { 0 };


coord3d ZeroCoordWatchPos = {0};
coord3d ZeroCoordSpawnPos = {0};
s32 g_PlayerTickCount = 0;

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

// forward declarations

s32 chrTick(PropRecord *prop);
void watchUpdateZoom(void);
void watchStartRaise(f32 speed);
void watchStartLower(f32 speed);
void watchStepAnim(void);
f32 watchGetAnimFrac(void);
void playerUpdateSpeedVerta(f32 value);
void playerUpdateSpeedTheta(f32 value);
void currentPlayerTickChrFade(void);
void currentPlayerTickColourFade(void);
bool currentPlayerIsFadeComplete(void);
void playerMoveFrozen(u16, u16);
void playerUpdate(s8 stick_x, s8 stick_y, u16 buttons);
void watchTrigger(s32 instant);
void playerUpdateCollisionBounds(void);
void playerMove(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons);
void playerProcessInput(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons);
void playerTickDamageAndHealth(void);
void playerTickExplode(void);
void pauseStopSounds(void);
void watchTick(void);
void playerUpdatePosFromCollision(void);

void playerUpdateVertaTheta(void);

f32 bheadGetBreathingValue(void);
void playerTickMoveAnim(f32 speed, f32 speedforwards, f32 speedsideways);
f32 watchSetupPauseTilt(bool topause);
void watchStartPauseTilt(f32 duration);
void watchStartUnpauseTilt(f32 duration);
bool watchIsPauseTilting(void);
f32 playerScaleLookSpeedTheta(f32 speed);
void playerSetLeanTarget(s32 value);
void playerAdjustCrouch(s32 value);
void playerUpdateSpeedSideways(s32 dir);
void playerUpdateSpeedForwards(s32 dir);
Gfx *watchRender(Gfx *gdl);


//split here makes sense to have the pd split make sense
s32 playerChooseSpawnPad(void)
{
    PadRecord *pad;
    PropRecord *player_prop;
    s32 player_count;
    f32 diff_x;
    s32 pad_index;
    s32 player_num;
    s32 player_index;
    s32 enemy_nearby;
    s32 attempt_num;
    f32 dist;
    f32 diff_z;

    // set up initial values
    player_num = playerGetCurrentNum();
    player_count = getPlayerCount();
    enemy_nearby = TRUE;

    // loop pads until no enemy is within 1000 units
    for (attempt_num = 0; enemy_nearby && (attempt_num < startpadcount);)
    {
        attempt_num++;
        enemy_nearby = FALSE;
        g_CurrentPlayer->spawnPadSeed++;
        pad_index = ( g_CurrentPlayer->spawnPadSeed) % (startpadcount);

        for (player_index = 0; player_index < player_count; player_index++)
        {
            // don't consider yourself as an enemy
            if (player_index == player_num) { continue; }

            // make sure the player prop is valid
            player_prop = g_playerPointers[player_index]->prop;
            if (player_prop == 0)
            {
                continue;
            }

            // find distance between enemy and this pad
            pad = g_Startpad[pad_index];
            diff_x = player_prop->pos.x - pad->pos.x;
            diff_z = player_prop->pos.z - pad->pos.z;
            dist = sqrtf((diff_x * diff_x) + (diff_z * diff_z));

            // if pad is within 1000, don't pick it
            if (dist < 1000)
            {
                enemy_nearby = TRUE;
            }
        }
    }

    do {} while (0); // leftover debug code? - Probably catch Player has no Prop

    // loop pads until no enemy is within 100 units
    for (; enemy_nearby && (attempt_num < startpadcount);)
    {
        attempt_num++;
        enemy_nearby = FALSE;
        g_CurrentPlayer->spawnPadSeed++;
        pad_index = ((s32) g_CurrentPlayer->spawnPadSeed) % ((s32) startpadcount);

        for (player_index = 0; player_index < player_count; player_index++)
        {
            // don't consider yourself as an enemy
            if (player_index == player_num) { continue; }

            // make sure the player prop is valid
            player_prop = g_playerPointers[player_index]->prop;
            if (player_prop == 0)
            {
                continue;
            }

            // find distance between enemy and this pad
            pad = g_Startpad[pad_index];
            diff_x = player_prop->pos.x - pad->pos.x;
            diff_z = player_prop->pos.z - pad->pos.z;
            dist = sqrtf((diff_x * diff_x) + (diff_z * diff_z));

            // if pad is within 100, don't pick it
            if (dist < 100.f)
            {
                enemy_nearby = TRUE;
            }
        }
    }

    // if we searched through all pads and failed to find a safe one, just pick one at random
    if (enemy_nearby)
    {
        pad_index = (randomGetNext() % (startpadcount));
    }

    return pad_index;
}


/**
 * Resets the current player's per-life state to defaults. Position,
 * health/armour, movement speed, etc...
 * 
 * Called when a stage is loaded, then called again for each time a player respawns in MP.
 */
void playerResetLife(void)
{
    // Co-op: player 1 keeps the save folder's options. Player 2 (in co-op
    // and in multiplayer) gets their own control style and watch options.
    // In counter-op with controller 2 playing Bond, the two swap.
    s32 person = coopPlayerPerson(playerGetCurrentNum());

    if (isMpGame() || (g_CoopMode && (person > 0)))
    {
        g_CurrentPlayer->controldef = frontGetMpControlStyle(person);
        optionsSetControlStyle(frontGetMpControlStyle(person));

        if (person == PLAYER_2)
        {
            optionsCoopCopyFromPlayer1();
            fileCoopRestorePartnerOptions();
        }
    }
    else if (g_CoopMode && (playerGetCurrentNum() > 0))
    {
        optionsSetControlStyle(fileGetFolderControlType(g_FrontFolder));
    }

    g_CurrentPlayer->current_model_pos.f[0] = 0.0f;
    g_CurrentPlayer->current_model_pos.f[1] = 0.0f;
    g_CurrentPlayer->current_model_pos.f[2] = 0.0f;
    g_CurrentPlayer->previous_model_pos.f[0] = 0.0f;
    g_CurrentPlayer->previous_model_pos.f[1] = 0.0f;
    g_CurrentPlayer->previous_model_pos.f[2] = 0.0f;
    g_CurrentPlayer->cameramode = 0;
    g_CurrentPlayer->pos.f[0] = 0.0f;
    g_CurrentPlayer->pos.f[1] = 0.0f;
    g_CurrentPlayer->pos.f[2] = 0.0f;
    g_CurrentPlayer->pos2.f[0] = 0.0f;
    g_CurrentPlayer->pos2.f[1] = 0.0f;
    g_CurrentPlayer->pos2.f[2] = 1.0f;
    g_CurrentPlayer->offset.f[0] = 0.0f;
    g_CurrentPlayer->offset.f[1] = 1.0f;
    g_CurrentPlayer->offset.f[2] = 0.0f;
    g_CurrentPlayer->pos3.f[0] = 0.0f;
    g_CurrentPlayer->pos3.f[1] = 0.0f;
    g_CurrentPlayer->pos3.f[2] = 0.0f;
    g_CurrentPlayer->cameratile = 0;
    g_CurrentPlayer->dampEye.x = 0.0f;
    g_CurrentPlayer->dampEye.y = 0.0f;
    g_CurrentPlayer->dampEye.z = 1.0f;
    g_CurrentPlayer->sumCrouch = 0.0f;
    g_CurrentPlayer->crouchHeight = 0.0f;
    g_CurrentPlayer->crouchTime240 = 0;
    g_CurrentPlayer->vertical_bounce_adjust = 0.0f;
    g_CurrentPlayer->swaytarget = 0.0f;
    g_CurrentPlayer->swayoffset0 = 0.0f;
    g_CurrentPlayer->swayoffset2 = 0.0f;
    g_CurrentPlayer->crouchpos = CROUCH_STAND;
    g_CurrentPlayer->autocrouchpos = CROUCH_STAND;
    g_CurrentPlayer->ducking_height_offset = 0.0f;
    g_CurrentPlayer->crouchSpeed = 0.0f;
    g_CurrentPlayer->perimEnabled = 1;
    g_CurrentPlayer->activatePressed = 0;
    g_CurrentPlayer->bonddead = 0;
    g_CurrentPlayer->bondhealth = 1.0f;
    g_CurrentPlayer->bondarmour = 0.0f;
    g_CurrentPlayer->oldhealth = 1.0f;
    g_CurrentPlayer->oldarmour = 0.0f;
    g_CurrentPlayer->apparenthealth = 1.0f;
    g_CurrentPlayer->apparentarmour = 0.0f;
    g_CurrentPlayer->damageshowtime = -1;
    g_CurrentPlayer->healthshowtime = -1;
    g_CurrentPlayer->watch_pause_time = 0;
    g_CurrentPlayer->timer_1C4 = 0;
    g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x0;
    g_CurrentPlayer->outside_watch_menu = TRUE;
    g_CurrentPlayer->open_close_solo_watch_menu = FALSE;
    g_CurrentPlayer->bondbreathing = 0.0f;
    g_CurrentPlayer->speedtheta = 0.0f;
    g_CurrentPlayer->vv_costheta = 1.0f;
    g_CurrentPlayer->vv_sintheta = 0.0f;
    g_CurrentPlayer->vv_verta = -4.0f;
    g_CurrentPlayer->vv_verta360 = (f32) g_CurrentPlayer->vv_verta;
    if (g_CurrentPlayer->vv_verta360 < 0.0f)
    {
        g_CurrentPlayer->vv_verta360 = (f32) (g_CurrentPlayer->vv_verta360 + 360.0f);
    }
    g_CurrentPlayer->speedverta = 0.0f;
    g_CurrentPlayer->vv_cosverta = 1.0f;
    g_CurrentPlayer->speedsideways = 0.0f;
    g_CurrentPlayer->speedstrafe = 0.0f;
    g_CurrentPlayer->speedforwards = 0.0f;
    g_CurrentPlayer->speedgo = 0.0f;
    g_CurrentPlayer->speedboost = 1.0f;
    g_CurrentPlayer->speedmaxtime60 = 0;
    g_CurrentPlayer->bondshotspeed.x = 0.0f;
    g_CurrentPlayer->bondshotspeed.y = 0.0f;
    g_CurrentPlayer->bondshotspeed.z = 0.0f;
    g_CurrentPlayer->docentreupdown = FALSE;
    g_CurrentPlayer->prevupdown = FALSE;
    g_CurrentPlayer->movecentrerelease = FALSE;
    g_CurrentPlayer->lookaheadcentreenabled = TRUE;
    g_CurrentPlayer->automovecentreenabled = TRUE;
    g_CurrentPlayer->fastmovecentreenabled = FALSE;
    g_CurrentPlayer->automovecentre = TRUE;
    g_CurrentPlayer->insightaimmode = FALSE;
    g_CurrentPlayer->autoyaimenabled = TRUE;
    g_CurrentPlayer->autoaimy = 0.0f;
    g_CurrentPlayer->autoaim_target_y = NULL;
    g_CurrentPlayer->autoyaimtime60 = -1;
    g_CurrentPlayer->autoxaimenabled = TRUE;
    g_CurrentPlayer->autoaimx = 0.0f;
    g_CurrentPlayer->autoaim_target_x = NULL;
    g_CurrentPlayer->autoxaimtime60 = -1;
    g_CurrentPlayer->colourscreenred = 0xff;
    g_CurrentPlayer->colourscreengreen = 0xff;
    g_CurrentPlayer->colourscreenblue = 0xff;
    g_CurrentPlayer->colourscreenfrac = 0.0f;
    g_CurrentPlayer->colourfadetime60 = -1.0f;
    g_CurrentPlayer->colourfadetimemax60 = -1.0f;
    g_CurrentPlayer->colourfaderedold = 0xff;
    g_CurrentPlayer->colourfaderednew = 0xff;
    g_CurrentPlayer->colourfadegreenold = 0xff;
    g_CurrentPlayer->colourfadegreennew = 0xff;
    g_CurrentPlayer->colourfadeblueold = 0xff;
    g_CurrentPlayer->colourfadebluenew = 0xff;
    g_CurrentPlayer->colourfadefracold = 0.0f;
    g_CurrentPlayer->colourfadefracnew = 0.0f;
    g_CurrentPlayer->bondfadetime60 = -1.0f;
    g_CurrentPlayer->bondfadetimemax60 = -1.0f;
    g_CurrentPlayer->bondfadefracold = 0.0f;
    g_CurrentPlayer->bondfadefracnew = 0.0f;
    g_CurrentPlayer->controldef = CONTROLLER_CONFIG_HONEY;
    g_CurrentPlayer->pause_starting_angle = 0.0f;
    g_CurrentPlayer->pause_saved_verta = 0.0f;
    g_CurrentPlayer->pause_target_verta = 0.0f;
    g_CurrentPlayer->pause_transition_time = 0.0f;
    g_CurrentPlayer->pause_transition_duration = 0.0f;
    g_CurrentPlayer->pause_state = 0;
    g_CurrentPlayer->step_in_view_watch_animation = 0;
    g_CurrentPlayer->pause_animation_counter = 0.0f;
    g_CurrentPlayer->pausing_flag = FALSE;
    g_CurrentPlayer->buttons_pressed = (u16)0;
    g_CurrentPlayer->prev_buttons_pressed = (u16)0;
    g_CurrentPlayer->lookDownDrop = 15.0f;
    g_CurrentPlayer->registeredroom = -1;
    g_CurrentPlayer->bodyAimPitch = 0.0f;
    g_CurrentPlayer->bodyAimYaw = 0.0f;
}


void playerStartNewLife(void)
{
    g_CurrentPlayer->eyeheight = ((g_playerPerm->player_perspective_height * 185.0f * (s32)1) - 10.0f);

    // Reset per-life counters
    g_CurrentPlayer->kills_this_life = 0;
    g_CurrentPlayer->lifestarttime60 = lvGetMissionTime60();
    g_CurrentPlayer->healthdisplaytime = 0;

    invAddItem(ITEM_FIST);

    if (getPlayerCount() >= 2)
    {
        gunEquip(GUNLEFT, starting_weapon[GUNLEFT]);
        gunEquip(GUNRIGHT, starting_weapon[GUNRIGHT]);

        if (g_CurrentPlayer->bodyModel == NULL)
        {
            playerLoadBody();
        }
    }
}


/**
 * Here sway refers to what we commonly call lean.
 * 
 * This function is called with the following values:
 * -1 to lean left
 *  0 for no lean
 *  1 to lean right
 */
void playerSetLeanTarget(s32 value)
{
    g_CurrentPlayer->swaytarget = (value * 75.0f);
}


void playerAdjustCrouch(s32 value)
{
    g_CurrentPlayer->crouchpos = g_CurrentPlayer->crouchpos + value;

    if (g_CurrentPlayer->crouchpos < CROUCH_SQUAT) 
    {
        g_CurrentPlayer->crouchpos = CROUCH_SQUAT;
    } 
    else if (g_CurrentPlayer->crouchpos > CROUCH_STAND) 
    {
        g_CurrentPlayer->crouchpos = CROUCH_STAND;
    }
}


s32 currentPlayerGetCrouchPos(void)
{
    return ((g_CurrentPlayer->crouchpos < g_CurrentPlayer->autocrouchpos) ? g_CurrentPlayer->crouchpos : g_CurrentPlayer->autocrouchpos);
}


s32 playerGetCrouchPos(s32 playernum)
{
	return (g_playerPointers[playernum]->crouchpos < g_playerPointers[playernum]->autocrouchpos)
		? g_playerPointers[playernum]->crouchpos
		: g_playerPointers[playernum]->autocrouchpos;
}


/**
 * Compares current player position to parameters. If different, sets current
 * player position values to parameter values.
 * Also updates related room pointer.
 */
void playerSetPosition(coord3d *pos, coord3d *pos2, coord3d *offset, StandTile *tile, coord3d *stan_walk_start)
{
    StandTile *tilefromstart;
    StandTile *tilefromprev;

    if (
        (pos->f[0] != g_CurrentPlayer->pos.f[0])
        || (pos->f[1] != g_CurrentPlayer->pos.f[1])
        || (pos->f[2] != g_CurrentPlayer->pos.f[2])
        || (pos2->f[0] != g_CurrentPlayer->pos2.f[0])
        || (pos2->f[1] != g_CurrentPlayer->pos2.f[1])
        || (pos2->f[2] != g_CurrentPlayer->pos2.f[2])
        || (offset->f[0] != g_CurrentPlayer->offset.f[0])
        || (offset->f[1] != g_CurrentPlayer->offset.f[1])
        || (offset->f[2] != g_CurrentPlayer->offset.f[2])
        || (g_CurrentPlayer->cameratile == NULL))
    {
        tilefromstart = tile;

        if (stanWalkLine(&tilefromstart, stan_walk_start->f[0], stan_walk_start->f[2], pos->f[0], pos->f[2]))
        {
            // @bug ...? This is either a bug or removed code, this function has no side effects.
            // Return value should used to check if point is safe for stan.
            stanIsNearlyInsideTri(tilefromstart, pos->f[0], pos->f[2]);
            g_CurrentPlayer->cameratile = tilefromstart;
        }
        else
        {
            if (g_CurrentPlayer->cameratile != NULL)
            {
                tilefromprev = g_CurrentPlayer->cameratile;
                if (stanWalkLine(&tilefromprev, g_CurrentPlayer->pos.f[0], g_CurrentPlayer->pos.f[2], pos->f[0], pos->f[2]))
                {
                    g_CurrentPlayer->cameratile = tilefromprev;
                }
                else
                {
                    g_CurrentPlayer->cameratile = tilefromstart;
                }
            }
            else
            {
                g_CurrentPlayer->cameratile = tilefromstart;
            }
        }

        g_CurrentPlayer->pos.f[0] = pos->f[0];
        g_CurrentPlayer->pos.f[1] = pos->f[1];
        g_CurrentPlayer->pos.f[2] = pos->f[2];
        g_CurrentPlayer->pos2.f[0] = pos2->f[0];
        g_CurrentPlayer->pos2.f[1] = pos2->f[1];
        g_CurrentPlayer->pos2.f[2] = pos2->f[2];
        g_CurrentPlayer->offset.f[0] = offset->f[0];
        g_CurrentPlayer->offset.f[1] = offset->f[1];
        g_CurrentPlayer->offset.f[2] = offset->f[2];
        g_CurrentPlayer->pos3.f[0] = g_CurrentPlayer->pos.f[0];
        g_CurrentPlayer->pos3.f[2] = g_CurrentPlayer->pos.f[2];
        g_CurrentPlayer->pos3.f[1] = stanGetFloorY(g_CurrentPlayer->cameratile, g_CurrentPlayer->pos.f[0], g_CurrentPlayer->pos.f[2]);
    }
}
