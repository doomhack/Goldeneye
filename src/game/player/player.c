/**
 * The player list. g_playerPointers holds every player and g_CurrentPlayer the one being
 * worked on (playerSetCurrent). Also allocating each player (playerInit), the shuffled
 * draw order, per-player screen size and field of view, and setting up the hands at
 * level start (playerInitHands). See docs/subsystems/player.md.
 */
#include <ultra64.h>
#include <memp.h>
#include <fr.h>
#include <random.h>
#include "game/player/player.h"
#include "game/nav/stan.h"
#include "game/world/water.h"
#include "game/level/lv.h"
#include "game/menu/watchmenu.h"
#include "bondtypes.h"
#include "bondconstants.h"
#include "game/player/gun.h"
#include "game/player/inventory.h"
#include "game/prop/propobj.h"
#include "game/save/file.h"

/**
 * Flag to toggle invisibility cheat.
 * 1 = visible to guards
 * 0 = not visible to guards
 */
static s32 g_VisibleToGuardsFlag = TRUE;


struct player *g_playerPointers[4];
struct player_data g_playerPlayerData[4];

struct player *g_CurrentPlayer;
struct player_data *g_playerPerm;

s32 player_num;
s32 random_byte;
PLAYER_ID array_PLAYER_IDs[4];

/* Define default values that are different between EU and the rest for playerInit */
#ifdef VERSION_EU
#define DEFAULT_DAMP_EYE_SUM_Z     8.417509f
#define DEFAULT_HEADDAMP        0.9166f
#define DEFAULT_HEADLOOKSUM_VAL 11.990406f
#define DEFAULT_VIEWPORT_V_VAL  0x220
#define DEFAULT_C_SCREENHEIGHT  (f32)SCREEN_HEIGHT
#define DEFAULT_C_HALFHEIGHT    136.0f
#define DEFAULT_SCREENYMAXF     (f32)SCREEN_HEIGHT
#define DEFAULT_ASPECT          ASPECT_RATIO
#elif defined(VERSION_US)
#define DEFAULT_DAMP_EYE_SUM_Z     9.999998f
#define DEFAULT_HEADDAMP        0.93f
#define DEFAULT_HEADLOOKSUM_VAL 14.285716f
#define DEFAULT_VIEWPORT_V_VAL  0x1E0
#define DEFAULT_C_SCREENHEIGHT  (f32)SCREEN_HEIGHT
#define DEFAULT_C_HALFHEIGHT    120.0f
#define DEFAULT_SCREENYMAXF     (f32)SCREEN_HEIGHT
#define DEFAULT_ASPECT          ASPECT_RATIO
#endif

//newfile per EU

void playersResetHandicaps(void)
{
  float value  = 1.0f;


  g_playerPlayerData[PLAYER_1].player_perspective_height = value;
  g_playerPlayerData[PLAYER_1].handicap = value;
  g_playerPlayerData[PLAYER_2].player_perspective_height = value;
  g_playerPlayerData[PLAYER_2].handicap = value;
  g_playerPlayerData[PLAYER_3].player_perspective_height = value;
  g_playerPlayerData[PLAYER_3].handicap = value;
  g_playerPlayerData[PLAYER_4].player_perspective_height = value;
  g_playerPlayerData[PLAYER_4].handicap = value;
}

void playersReset(void)
{
    g_playerPointers[PLAYER_1] = NULL;
    g_playerPointers[PLAYER_2] = NULL;
    g_playerPointers[PLAYER_3] = NULL;
    g_playerPointers[PLAYER_4] = NULL;
    g_CurrentPlayer = NULL;
    g_playerPerm = NULL;
    player_num = PLAYER_1;
    random_byte = 0;
    array_PLAYER_IDs[PLAYER_1] = 0;
    array_PLAYER_IDs[PLAYER_2] = 1;
    array_PLAYER_IDs[PLAYER_3] = 2;
    array_PLAYER_IDs[PLAYER_4] = 3;
}

void playersInit(s32 playercount)
{
    s32 i;

    g_playerPointers[PLAYER_1] = NULL;
    g_playerPointers[PLAYER_2] = NULL;
    g_playerPointers[PLAYER_3] = NULL;
    g_playerPointers[PLAYER_4] = NULL;
    random_byte = (s32) (randomGetNext() & 0xFF);
    if (playercount > 0)
    {
        for (i = 0; i < playercount; i++)
        {
            playerInit(i);
        }
        playerSetCurrent(PLAYER_1);
        return;
    }
    playerInit(PLAYER_1);
    playerSetCurrent(PLAYER_1);
    playerSetScreenSize(viGetViewWidth(), viGetViewHeight());
    playerSetViewportPos(viGetViewLeft(), viGetViewTop());
}

s32 isMpGame(void)
{
    return !g_CoopMode && getPlayerCount() >= 2;
}

/**
 * Number of views drawn this frame. This is the player count, except in
 * co-op while one player's view is shown full-screen.
 */
s32 getViewCount(void)
{
    if (g_CoopSoloView >= 0)
    {
        return 1;
    }

    return getPlayerCount();
}

/**
 * The current view is half the screen wide: 3 or 4 player split, or co-op
 * side by side. The HUD then uses its narrow layout.
 */
s32 viewIsHalfWidth(void)
{
    return (getViewCount() >= 3) || coopSplitSideBySide();
}

s32 getPlayerCount(void)
{
    s32 count = 0;
    s32 i;
    for (i = 0; i < 4; i++) {
        if (g_playerPointers[i] != NULL) {
            count++;
        }
    }
    return count;
}

/* What each hand holds when a player is created: nothing */
static struct hand s_DefaultHand = {
    .weaponnum_watchmenu = -1,
    .heldItemVisible = 1,
    .animMtx = { { {1.0f,0.0f,0.0f,0.0f}, {0.0f,1.0f,0.0f,0.0f}, {0.0f,0.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f,1.0f} } },
    .sway_look_z = -1.0f,
    .sway_up_y = 1.0f,
    /* spring_look_z and spring_up_y = 1 / GUN_SPRING_SCALE, so the derived sway vector starts unit-length */
#if defined(BUGFIX_R2)
    .spring_look_z = -16.7504158f,
    .spring_up_y = 16.7504158f,
#else
    .spring_look_z = -19.999996f,
    .spring_up_y = 19.999996f,
#endif
    .blendlook = { {{0,0,-1.0f}}, {{0,0,-1.0f}}, {{0,0,-1.0f}}, {{0,0,-1.0f}} },
    .blendup = { {{0,1.0f,0}}, {{0,1.0f,0}}, {{0,1.0f,0}}, {{0,1.0f,0}} },
    .blendscale1 = 1.0f,
    .aimPos.z = 1000.0f,
    .weapon_beam.age = -1,
};

void playerInit(s32 player_num)
{
    s32 i;
    struct hand default_hand;
    s32 one;
    default_hand = s_DefaultHand;
#if defined(VERSION_US)
    g_playerPointers[player_num] = mempAlloc(0x2A80U, MEMPOOL_STAGE);
#elif defined(VERSION_EU)
    g_playerPointers[player_num] = mempAlloc(0x2A70U, MEMPOOL_STAGE);
#endif


    g_playerPointers[player_num]->cameramode = 0;
    g_playerPointers[player_num]->pos.x = 0.0f;
    g_playerPointers[player_num]->pos.y = 0.0f;
    g_playerPointers[player_num]->pos.z = 0.0f;
    g_playerPointers[player_num]->pos2.x = 0.0f;
    g_playerPointers[player_num]->pos2.y = 0.0f;
    g_playerPointers[player_num]->pos2.z = 1.0f;
    g_playerPointers[player_num]->offset.x = 0.0f;
    g_playerPointers[player_num]->offset.y = 1.0f;
    g_playerPointers[player_num]->offset.z = 0.0f;
    g_playerPointers[player_num]->pos3.x = 0.0f;
    g_playerPointers[player_num]->pos3.y = 0.0f;
    g_playerPointers[player_num]->pos3.z = 0.0f;
    g_playerPointers[player_num]->cameratile = NULL;
    g_playerPointers[player_num]->current_model_pos.x = 0.0f;
    g_playerPointers[player_num]->current_model_pos.y = 0.0f;
    g_playerPointers[player_num]->current_model_pos.z = 0.0f;
    g_playerPointers[player_num]->previous_model_pos.x = 0.0f;
    g_playerPointers[player_num]->previous_model_pos.y = 0.0f;
    g_playerPointers[player_num]->previous_model_pos.z = 0.0f;
    g_playerPointers[player_num]->sumGround = 0.0f;
    g_playerPointers[player_num]->manGround = 0.0f;
    g_playerPointers[player_num]->stanHeight = 0.0f;
    g_playerPointers[player_num]->deltaPos.x = 0.0f;
    g_playerPointers[player_num]->deltaPos.y = 0.0f;
    g_playerPointers[player_num]->deltaPos.z = 0.0f;
    g_playerPointers[player_num]->sumCrouch = 0.0f;
    g_playerPointers[player_num]->crouchHeight = 0.0f;
    g_playerPointers[player_num]->crouchTime240 = 0;
    g_playerPointers[player_num]->vertical_bounce_adjust = 0.0f;
    g_playerPointers[player_num]->swaytarget = 0.0f;
    g_playerPointers[player_num]->swayoffset0 = 0.0f;
    g_playerPointers[player_num]->swayoffset2 = 0.0f;
    g_playerPointers[player_num]->crouchpos = CROUCH_STAND;
    g_playerPointers[player_num]->autocrouchpos = CROUCH_STAND;
    g_playerPointers[player_num]->ducking_height_offset = 0.0f;
    g_playerPointers[player_num]->crouchSpeed = 0.0f;
    g_playerPointers[player_num]->prop = NULL;
    g_playerPointers[player_num]->perimEnabled = 1;
    g_playerPointers[player_num]->activatePressed = 0;
    g_playerPointers[player_num]->bodyModel = NULL;
    g_playerPointers[player_num]->bonddead = FALSE;
    g_playerPointers[player_num]->bondhealth = 1.0f;
    g_playerPointers[player_num]->bondarmour = 0.0f;
    g_playerPointers[player_num]->oldhealth = 1.0f;
    g_playerPointers[player_num]->oldarmour = 0.0f;
    g_playerPointers[player_num]->apparenthealth = 1.0f;
    g_playerPointers[player_num]->apparentarmour = 0.0f;
    g_playerPointers[player_num]->damageshowtime = -1;
    g_playerPointers[player_num]->healthshowtime = -1;
    g_playerPointers[player_num]->docentreupdown = FALSE;
    g_playerPointers[player_num]->prevupdown = 0;
    g_playerPointers[player_num]->movecentrerelease = FALSE;
    g_playerPointers[player_num]->lookaheadcentreenabled = TRUE;
    g_playerPointers[player_num]->automovecentreenabled = TRUE;
    g_playerPointers[player_num]->fastmovecentreenabled = FALSE;
    g_playerPointers[player_num]->automovecentre = TRUE;
    g_playerPointers[player_num]->insightaimmode = FALSE;
    g_playerPointers[player_num]->autoyaimenabled = TRUE;
    g_playerPointers[player_num]->autoaimy = 0.0f;
    g_playerPointers[player_num]->autoaim_target_y = NULL;
    g_playerPointers[player_num]->autoyaimtime60 = -1;
    g_playerPointers[player_num]->autoxaimenabled = TRUE;
    g_playerPointers[player_num]->autoaimx = 0.0f;
    g_playerPointers[player_num]->autoaim_target_x = NULL;
    g_playerPointers[player_num]->autoxaimtime60 = -1;
    g_playerPointers[player_num]->vv_theta = 0.0f;
    g_playerPointers[player_num]->speedtheta = 0.0f;
    g_playerPointers[player_num]->vv_costheta = 1.0f;
    g_playerPointers[player_num]->vv_sintheta = 0.0f;
    g_playerPointers[player_num]->vv_verta = -4.0f;
    // @bug
    // -229.1831 degrees = -4 radians
    // This doesn't matter, because playerUpdateVertaTheta overwrites vv_verta360 with the value from vv_verta
    g_playerPointers[player_num]->vv_verta360 = -229.1831f;
    g_playerPointers[player_num]->speedverta = 0.0f;
    g_playerPointers[player_num]->vv_cosverta = 1.0f;
    g_playerPointers[player_num]->speedsideways = 0.0f;
    g_playerPointers[player_num]->speedstrafe = 0.0f;
    g_playerPointers[player_num]->speedforwards = 0.0f;
    g_playerPointers[player_num]->speedgo = 0.0f;
    g_playerPointers[player_num]->speedboost = 1.0f;
    g_playerPointers[player_num]->bondshotspeed.x = 0.0f;
    g_playerPointers[player_num]->bondshotspeed.y = 0.0f;
    g_playerPointers[player_num]->bondshotspeed.z = 0.0f;
    g_playerPointers[player_num]->bondfadetime60 = -1.0f;
    g_playerPointers[player_num]->bondfadetimemax60 = -1.0f;
    g_playerPointers[player_num]->bondfadefracold = 0.0f;
    g_playerPointers[player_num]->bondfadefracnew = 0.0f;
    g_playerPointers[player_num]->bondbreathing = 0.0f;
    g_playerPointers[player_num]->watch_pause_time = 0;
    g_playerPointers[player_num]->timer_1C4 = 0;
    g_playerPointers[player_num]->watch_animation_state = WATCH_ANIMATION_0x0;
    g_playerPointers[player_num]->outside_watch_menu = TRUE;
    g_playerPointers[player_num]->open_close_solo_watch_menu = FALSE;
    g_playerPointers[player_num]->watchPos.x = 0.0f;
    g_playerPointers[player_num]->watchPos.y = 0.0f;
    g_playerPointers[player_num]->watchPos.z = -25;
    g_playerPointers[player_num]->watchLook.x = 0.0f;
    g_playerPointers[player_num]->watchLook.y = 1.0f;
    g_playerPointers[player_num]->watchLook.z = 0.0f;
    g_playerPointers[player_num]->watchUp.x = 0.0f;
    g_playerPointers[player_num]->watchUp.y = 0.0f;
    g_playerPointers[player_num]->watchUp.z = 1.0f;
    g_playerPointers[player_num]->pausing_flag = FALSE;
    g_playerPointers[player_num]->pause_starting_angle = 0.0f;
    g_playerPointers[player_num]->pause_saved_verta = 0.0f;
    g_playerPointers[player_num]->pause_target_verta = 0.0f;
    g_playerPointers[player_num]->pause_transition_time = 0.0f;
    g_playerPointers[player_num]->pause_transition_duration = 0.0f;
    g_playerPointers[player_num]->pause_state = 0;
    g_playerPointers[player_num]->watchSfxPending = 1;
    g_playerPointers[player_num]->step_in_view_watch_animation = 0;
    g_playerPointers[player_num]->pause_animation_counter = 0.0f;
    g_playerPointers[player_num]->buttons_pressed = 0;
    g_playerPointers[player_num]->prev_buttons_pressed = 0;
    g_playerPointers[player_num]->dampEyeSum.x = 0.0f;
    g_playerPointers[player_num]->dampEyeSum.y = 0.0f;
    g_playerPointers[player_num]->dampEyeSum.z = DEFAULT_DAMP_EYE_SUM_Z;
    g_playerPointers[player_num]->dampEye.x = 0.0f;
    g_playerPointers[player_num]->dampEye.y = 0.0f;
    g_playerPointers[player_num]->dampEye.z = 1.0f;
    g_playerPointers[player_num]->colourscreenred = 0xFF;
    g_playerPointers[player_num]->colourscreengreen = 0xFF;
    g_playerPointers[player_num]->colourscreenblue = 0xFF;
    g_playerPointers[player_num]->colourscreenfrac = 0.0f;
    g_playerPointers[player_num]->colourfadetime60 = -1.0f;
    g_playerPointers[player_num]->colourfadetimemax60 = -1.0f;
    g_playerPointers[player_num]->colourfaderedold = 0xFF;
    g_playerPointers[player_num]->colourfaderednew = 0xFF;
    g_playerPointers[player_num]->colourfadegreenold = 0xFF;
    g_playerPointers[player_num]->colourfadegreennew = 0xFF;
    g_playerPointers[player_num]->colourfadeblueold = 0xFF;
    g_playerPointers[player_num]->colourfadebluenew = 0xFF;
    g_playerPointers[player_num]->colourfadefracold = 0.0f;
    g_playerPointers[player_num]->colourfadefracnew = 0.0f;
    g_playerPointers[player_num]->bondtype = CUFF_BLUE;
    g_playerPointers[player_num]->startnewbonddie = TRUE;
    g_playerPointers[player_num]->redbloodfinished = FALSE;
    g_playerPointers[player_num]->deathanimfinished = FALSE;
    g_playerPointers[player_num]->controldef = CONTROLLER_CONFIG_HONEY;
    g_playerPointers[player_num]->resetheadpos = TRUE;
    g_playerPointers[player_num]->resetheadrot = TRUE;
    g_playerPointers[player_num]->headanim = 0;
    g_playerPointers[player_num]->headdamp = DEFAULT_HEADDAMP;
    g_playerPointers[player_num]->headwalkingtime60 = 0;
    g_playerPointers[player_num]->headamplitude = 1.0f;
    g_playerPointers[player_num]->sideamplitude = 1.0f;
    g_playerPointers[player_num]->headpos.f[0] = 0.0f;
    g_playerPointers[player_num]->headpos.f[1] = 0.0f;
    g_playerPointers[player_num]->headpos.f[2] = 0.0f;
    g_playerPointers[player_num]->headlook.f[0] = 0.0f;
    g_playerPointers[player_num]->headlook.f[1] = 0.0f;
    g_playerPointers[player_num]->headlook.f[2] = 1.0f;
    g_playerPointers[player_num]->headup.f[0] = 0.0f;
    g_playerPointers[player_num]->headup.f[1] = 1.0f;
    g_playerPointers[player_num]->headup.f[2] = 0.0f;
    g_playerPointers[player_num]->headpossum.f[0] = 0.0f;
    g_playerPointers[player_num]->headpossum.f[1] = 0.0f;
    g_playerPointers[player_num]->headpossum.f[2] = 0.0f;
    g_playerPointers[player_num]->headlooksum.f[0] = 0.0f;
    g_playerPointers[player_num]->headlooksum.f[1] = 0.0f;
    g_playerPointers[player_num]->headlooksum.f[2] = DEFAULT_HEADLOOKSUM_VAL;
    g_playerPointers[player_num]->headupsum.f[0] = 0.0f;
    g_playerPointers[player_num]->headupsum.f[1] = DEFAULT_HEADLOOKSUM_VAL;
    g_playerPointers[player_num]->headupsum.f[2] = 0.0f;
    g_playerPointers[player_num]->headbodyoffset.f[0] = 0.0f;
    g_playerPointers[player_num]->headbodyoffset.f[1] = 0.0f;
    g_playerPointers[player_num]->headbodyoffset.f[2] = 0.0f;
    g_playerPointers[player_num]->standheight = 0.0f;
    g_playerPointers[player_num]->standbodyoffset.x = 0.0f;
    g_playerPointers[player_num]->standbodyoffset.y = 0.0f;
    g_playerPointers[player_num]->standbodyoffset.z = 0.0f;
    g_playerPointers[player_num]->standfrac = 0.0f;
    g_playerPointers[player_num]->standlook[0].f[0] = 0.0f;
    g_playerPointers[player_num]->standlook[0].f[1] = 0.0f;
    g_playerPointers[player_num]->standlook[0].f[2] = 1.0f;
    g_playerPointers[player_num]->standlook[1].f[0] = 0.0f;
    g_playerPointers[player_num]->standlook[1].f[1] = 0.0f;
    g_playerPointers[player_num]->standlook[1].f[2] = 1.0f;
    g_playerPointers[player_num]->standup[0].f[0] = 0.0f;
    g_playerPointers[player_num]->standup[0].f[1] = 1.0f;
    g_playerPointers[player_num]->standup[0].f[2] = 0.0f;
    g_playerPointers[player_num]->standup[1].f[0] = 0.0f;
    g_playerPointers[player_num]->standup[1].f[1] = 1.0f;
    g_playerPointers[player_num]->standup[1].f[2] = 0.0f;
    g_playerPointers[player_num]->standcnt = 0;

    for (i = 0; i < 2; i++)
    {
        g_playerPointers[player_num]->viewports[i].vp.vscale[0] = 0x280;
        g_playerPointers[player_num]->viewports[i].vp.vscale[1] = DEFAULT_VIEWPORT_V_VAL;
        g_playerPointers[player_num]->viewports[i].vp.vscale[2] = 0x1FF;
        g_playerPointers[player_num]->viewports[i].vp.vscale[3] = 0;
        g_playerPointers[player_num]->viewports[i].vp.vtrans[0] = 0x280;
        g_playerPointers[player_num]->viewports[i].vp.vtrans[1] = DEFAULT_VIEWPORT_V_VAL;
        g_playerPointers[player_num]->viewports[i].vp.vtrans[2] = 0x1FF;
        g_playerPointers[player_num]->viewports[i].vp.vtrans[3] = 0;
    }

    g_playerPointers[player_num]->viewx = 0x64;
    g_playerPointers[player_num]->viewy = 0x64;
    g_playerPointers[player_num]->viewleft = 0;
    g_playerPointers[player_num]->viewtop = 0;
    g_playerPointers[player_num]->handModelState[GUNRIGHT] = 0;
    g_playerPointers[player_num]->handModelState[GUNLEFT] = 0;
    g_playerPointers[player_num]->handModelItem[GUNRIGHT] = ITEM_UNARMED;
    g_playerPointers[player_num]->handModelItem[GUNLEFT] = ITEM_UNARMED;
    g_playerPointers[player_num]->handModelNextItem[GUNRIGHT] = -1;
    g_playerPointers[player_num]->handModelNextItem[GUNLEFT] = -1;
    g_playerPointers[player_num]->handModelLocked[GUNRIGHT] = FALSE;
    g_playerPointers[player_num]->handModelLocked[GUNLEFT] = FALSE;
    g_playerPointers[player_num]->handModelBuffer[GUNRIGHT] = NULL;
    g_playerPointers[player_num]->handModelBuffer[GUNLEFT] = NULL;

    g_playerPointers[player_num]->hands[GUNRIGHT] = default_hand;
    g_playerPointers[player_num]->hands[GUNLEFT] = default_hand;

    g_playerPointers[player_num]->gunposamplitude = 1.0f;
    g_playerPointers[player_num]->trigger_released = 0;
    g_playerPointers[player_num]->trigger_down = 0;
    g_playerPointers[player_num]->prev_trigger_down = 0;
    g_playerPointers[player_num]->z_trigger_timer = 0;
    g_playerPointers[player_num]->current_trigger_hand = 0;
    g_playerPointers[player_num]->tileColor.rgba[0] = 0xFF;
    g_playerPointers[player_num]->tileColor.rgba[1] = 0xFF;
    g_playerPointers[player_num]->tileColor.rgba[2] = 0xFF;
    g_playerPointers[player_num]->tileColor.rgba[3] = 0;
    g_playerPointers[player_num]->resetshadecol = TRUE;
    g_playerPointers[player_num]->crosshair_angle.x = 0.0f;
    g_playerPointers[player_num]->crosshair_angle.y = 0.0f;
    g_playerPointers[player_num]->crosshair_x_pos = 0.0f;
    g_playerPointers[player_num]->crosshair_y_pos = 0.0f;
    g_playerPointers[player_num]->guncrossdamp = 0.9f;
    g_playerPointers[player_num]->gunAimScreenPos.x = 0.0f;
    g_playerPointers[player_num]->gunAimScreenPos.y = 0.0f;
    g_playerPointers[player_num]->gun_azimuth_angle = 0.0f;
    g_playerPointers[player_num]->gun_azimuth_turning = 0.0f;
    g_playerPointers[player_num]->gunaimdamp = 0.9f;
    g_playerPointers[player_num]->last_z_trigger_timer = 0;
    g_playerPointers[player_num]->copiedgoldeneye = FALSE;
    g_playerPointers[player_num]->gunammooff = FALSE;
    g_playerPointers[player_num]->gunsync = 0.0f;
    g_playerPointers[player_num]->syncchange = 0.0f;
    g_playerPointers[player_num]->synccount = 0.0f;
    g_playerPointers[player_num]->syncoffset = 0;
    g_playerPointers[player_num]->cycleSum = 0.0f;
    g_playerPointers[player_num]->gunAmpSum = 0.0f;
    g_playerPointers[player_num]->sniper_zoom = 60.0f;
    g_playerPointers[player_num]->camera_zoom = 60.0f;
    g_playerPointers[player_num]->curRoomIndex = -1;
    g_playerPointers[player_num]->c_screenwidth = 320.0f;
    g_playerPointers[player_num]->c_screenheight = DEFAULT_C_SCREENHEIGHT;
    g_playerPointers[player_num]->c_screenleft = 0.0f;
    g_playerPointers[player_num]->c_screentop = 0.0f;
    g_playerPointers[player_num]->c_perspfovy = 46.0f;
    g_playerPointers[player_num]->c_perspaspect = 1.0f;
    g_playerPointers[player_num]->c_halfwidth = 160.0f;
    g_playerPointers[player_num]->c_halfheight = DEFAULT_C_HALFHEIGHT;
    g_playerPointers[player_num]->c_scalex = 1.0f;
    g_playerPointers[player_num]->c_scaley = 1.0f;
    g_playerPointers[player_num]->c_recipscalex = 1.0f;
    g_playerPointers[player_num]->c_recipscaley = 1.0f;
    g_playerPointers[player_num]->worldToViewMtx = NULL;
    g_playerPointers[player_num]->worldToScreenMtxf = NULL;
    g_playerPointers[player_num]->viewtoworldmtxf = NULL;
    g_playerPointers[player_num]->projmatrix = NULL;
    g_playerPointers[player_num]->projmatrixf = NULL;
    g_playerPointers[player_num]->projViewMtx = NULL;
    g_playerPointers[player_num]->lookAt = NULL;
    g_playerPointers[player_num]->prevViewToWorldMtxf = NULL;
    g_playerPointers[player_num]->c_scalelod60 = 1.0f;
    g_playerPointers[player_num]->c_scalelod = 1.0f;
    g_playerPointers[player_num]->c_lodscalez = 1.0f;
    g_playerPointers[player_num]->screensize.min.x = 0.0f;
    g_playerPointers[player_num]->screensize.min.y = 0.0f;
    g_playerPointers[player_num]->screensize.max.x = 320.0f;
    g_playerPointers[player_num]->screensize.max.y = DEFAULT_SCREENYMAXF;
    g_playerPointers[player_num]->gunsightmode = 0;
    g_playerPointers[player_num]->bloodImgBufPtrArray[0] = NULL;
    g_playerPointers[player_num]->bloodImgBufPtrArray[1] = NULL;
    g_playerPointers[player_num]->bloodImgIdx = 0;
    g_playerPointers[player_num]->zoomintime = 0.0f;
    g_playerPointers[player_num]->zoomintimemax = 0.0f;
    g_playerPointers[player_num]->zoominfovy = 60.0f;
    g_playerPointers[player_num]->zoominfovyold = 60.0f;
    g_playerPointers[player_num]->zoominfovynew = 60.0f;
    g_playerPointers[player_num]->fovy = 60.0f;
    g_playerPointers[player_num]->aspect = DEFAULT_ASPECT;
    g_playerPointers[player_num]->hudmessoff = FALSE;
    g_playerPointers[player_num]->bondmesscnt = -1;
    g_playerPointers[player_num]->ptr_inventory_first_in_cycle = NULL;
    g_playerPointers[player_num]->p_itemcur = NULL;
    g_playerPointers[player_num]->equipmaxitems = FALSE;
    g_playerPointers[player_num]->equipallguns = FALSE;
    one = 1;
    g_playerPointers[player_num]->equipcuritem = ITEM_UNARMED;
    g_playerPointers[player_num]->textoverrides = NULL;
    g_playerPointers[player_num]->bodyStrafeTurn = 0.0f;
    g_playerPointers[player_num]->players_cur_animation = 0;
    g_playerPointers[player_num]->bodyAnimSpeed = 0.0f;
    g_playerPointers[player_num]->cheatBondInvincible = FALSE;
    g_playerPointers[player_num]->healthdamagetype = 7;
    g_playerPointers[player_num]->eyeheight = 1.0f;
    g_playerPointers[player_num]->lookDownDrop = 0.0f;
    g_playerPointers[player_num]->mpmenuon = FALSE;
    g_playerPointers[player_num]->damagetype = 7;
    g_playerPointers[player_num]->deathcount = 0;
    g_playerPointers[player_num]->spawnPadSeed = random_byte++;
    g_playerPointers[player_num]->last_kill_time[0] = -1;
    g_playerPointers[player_num]->last_kill_time[1] = -1;
    g_playerPointers[player_num]->last_kill_time[2] = -1;
    g_playerPointers[player_num]->last_kill_time[3] = -1;
    g_playerPointers[player_num]->healthdisplaytime = 0;
    g_playerPointers[player_num]->bodyHandPosTick[0] = 0;
    g_playerPointers[player_num]->bodyHandPosTick[1] = 0;
    g_playerPointers[player_num]->cur_item_weapon_getname = ITEM_FIST;
    g_playerPointers[player_num]->actual_health = 1.0f;
    g_playerPointers[player_num]->actual_armor = 1.0f;
    g_playerPointers[player_num]->cur_player_control_type_0 = CONTROLLER_CONFIG_HONEY;
    g_playerPointers[player_num]->cur_player_control_type_1 = CONTROLLER_CONFIG_HONEY;
    g_playerPointers[player_num]->cur_player_control_type_2 = 0.0f; //CONTROLLER_CONFIG_HONEY
    g_playerPointers[player_num]->neg_vspacing_for_control_type_entry = 0;
    g_playerPointers[player_num]->has_set_control_type_data = one;

    g_VisibleToGuardsFlag = TRUE;
    g_PlayerCollisionOn = TRUE;
}


void playerSetCurrent(s32 playernum)
{

    player_num = playernum;
    g_CurrentPlayer = g_playerPointers[playernum];
    // Player 2 has their own options in co-op and in multiplayer.
    optionsCoopUsePlayer(((coopPlayerPerson(playernum) == 1) && (g_CoopMode || isMpGame())) ? 1 : 0);
    g_playerPerm = &g_playerPlayerData[playernum];
}


s32 playerGetCurrentNum(void) {
    return player_num;
}

s32 playerGetIndexForProp(PropRecord* prop)
{
    s32 i;

    for(i = 0; i < getPlayerCount(); i++) {
        if (g_playerPointers[i]->prop == prop) {
           return i;
        }
    }

    return -1;
}


void playerSetScreenSize(u32 width, u32 height) {
  g_CurrentPlayer->viewx = width;
  g_CurrentPlayer->viewy = height;
}

void playerSetViewportPos(u32 ulx, u32 uly) {
  g_CurrentPlayer->viewleft = ulx;
  g_CurrentPlayer->viewtop = uly;
}

void playerSetFovY(f32 fovy) {
    g_CurrentPlayer->fovy = fovy;
}

void playerSetAspect(f32 aspect) {
    g_CurrentPlayer->aspect = aspect;
}


void playersShuffleOrder(void) {
    s32 i;
    u32 random;
    PLAYER_ID temp;

    for (i = 0; i < 4; i ++) {
        array_PLAYER_IDs[i] = i;
    }

    for (i = 0; i < 3; i ++) {
        random = randomGetNext();
        temp = array_PLAYER_IDs[i];
        array_PLAYER_IDs[i] = array_PLAYER_IDs[i + random % (4 - i)];
        array_PLAYER_IDs[i + random % (4 - i)] = temp;
    }

    // Co-op: the full-screen player goes first, so the per-view loops (which
    // run getViewCount() views) only move and draw that player.
    if (g_CoopSoloView >= 0)
    {
        for (i = 0; i < 4; i++)
        {
            if (array_PLAYER_IDs[i] == g_CoopSoloView)
            {
                array_PLAYER_IDs[i] = array_PLAYER_IDs[0];
                array_PLAYER_IDs[0] = g_CoopSoloView;
                break;
            }
        }
    }
}

s32 playerGetShuffledIndex(s32 current_player_num) {
    s32 i;
    s32 position = 0;

    for (i = 0; i < 4; i++) {
        if (current_player_num != array_PLAYER_IDs[i])
        {
            if (g_playerPointers[array_PLAYER_IDs[i]] != NULL)
            {
                position++;
            }
        }
        else
        {
            break;
        }
    }

    return position;
}

s32 playerGetShuffled(PLAYER_ID id)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        if (g_playerPointers[array_PLAYER_IDs[i]] != NULL) {
            if (id == 0) {
               return array_PLAYER_IDs[i];
            }
            id--;
        }
    }

    return 0;
}


void playerInitHands(void)
{
    s32 i;


    g_CurrentPlayer->handModelBuffer[GUNRIGHT] = mempAlloc(size_item_buffer[0], MEMPOOL_STAGE);

    if (!isMpGame())
    {
        g_CurrentPlayer->handModelBuffer[GUNLEFT] = mempAlloc(size_item_buffer[1], MEMPOOL_STAGE);
    }

    g_CurrentPlayer->handModelState[GUNRIGHT] = 0;
    g_CurrentPlayer->handModelState[GUNLEFT] = 0;
    g_CurrentPlayer->handModelItem[GUNRIGHT] = ITEM_UNARMED;
    g_CurrentPlayer->handModelItem[GUNLEFT] = ITEM_UNARMED;
    g_CurrentPlayer->handModelNextItem[GUNRIGHT] = -1;
    g_CurrentPlayer->handModelNextItem[GUNLEFT] = -1;
    g_CurrentPlayer->handModelLocked[GUNRIGHT] = 0;
    g_CurrentPlayer->handModelLocked[GUNLEFT] = 0;

    g_CurrentPlayer->hands[0] = s_DefaultHand;
    g_CurrentPlayer->hands[1] = s_DefaultHand;

    for (i=0; i<30; i++)
    {
        g_CurrentPlayer->ammoheldarr[i] = 0;
    }

    for (i=0; i<7; i++)
    {
        g_playerPerm->shot_count[i] = 0;
    }

    g_playerPerm->killed_civilians = 0;
    g_playerPerm->kill_count = 0;
    g_playerPerm->killed_gg_owner_count = 0;
    g_CurrentPlayer->deathcount = 0;
    g_CurrentPlayer->num_suicides = 0;
    g_CurrentPlayer->gunposamplitude = 1.0f;
    g_CurrentPlayer->trigger_released = 0;
    g_CurrentPlayer->trigger_down = 0;
    g_CurrentPlayer->prev_trigger_down = 0;
    g_CurrentPlayer->z_trigger_timer = 0;
    g_CurrentPlayer->current_trigger_hand = 0;
    g_CurrentPlayer->tileColor.rgba[0] = 0xFF;
    g_CurrentPlayer->tileColor.rgba[1] = 0xFF;
    g_CurrentPlayer->tileColor.rgba[2] = 0xFF;
    g_CurrentPlayer->tileColor.rgba[3] = 0;
    g_CurrentPlayer->resetshadecol = TRUE;
    g_CurrentPlayer->crosshair_angle.f[0] = 0.0f;
    g_CurrentPlayer->crosshair_angle.f[1] = 0.0f;
    g_CurrentPlayer->crosshair_x_pos = 0.0f;
    g_CurrentPlayer->crosshair_y_pos = 0.0f;
    g_CurrentPlayer->guncrossdamp = 0.9f;
    g_CurrentPlayer->gunAimScreenPos.f[0] = 0.0f;
    g_CurrentPlayer->gunAimScreenPos.f[1] = 0.0f;
    g_CurrentPlayer->gun_azimuth_angle = 0.0f;
    g_CurrentPlayer->gun_azimuth_turning = 0.0f;
    g_CurrentPlayer->gunaimdamp = 0.9f;
    g_CurrentPlayer->copiedgoldeneye = FALSE;
    g_CurrentPlayer->magnetattracttime = -1;
    g_CurrentPlayer->gunsync = 0.0f;
    g_CurrentPlayer->syncchange = 0.0f;
    g_CurrentPlayer->synccount = 0.0f;
    g_CurrentPlayer->syncoffset = 0;
    g_CurrentPlayer->cycleSum = 0.0f;
    g_CurrentPlayer->gunAmpSum = 0.0f;
    gunCalcBlend(GUNRIGHT);
    gunCalcBlend(GUNRIGHT);
    gunCalcBlend(GUNRIGHT);
    gunCalcBlend(GUNLEFT);
    gunCalcBlend(GUNLEFT);
    gunCalcBlend(GUNLEFT);
    g_CurrentPlayer->gunammooff = FALSE;
    g_CurrentPlayer->gunsightmode = 2; //GUNSIGHTREASON_AIMING
    g_CurrentPlayer->sniper_zoom = sniperrifle_stats.Zoom;
    g_CurrentPlayer->camera_zoom = camera_stats.Zoom;
}


void playersStoreFavouriteWeapons(void) {
    int right_hand;
    int left_hand;
    int player_count;
    int current_player;
    int player;

    player_count = getPlayerCount();
    current_player = playerGetCurrentNum();

    for (player = 0; player < player_count; player++) {
        playerSetCurrent(player);
        invGetWeaponOfChoice(&right_hand, &left_hand);
        fileSetFavWeapon(right_hand, left_hand);
    }

    playerSetCurrent(current_player);
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
