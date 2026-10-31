/**
 * A player's life: choosing a spawn pad, resetting a player for a new life
 * (playerResetLife, playerStartNewLife), crouch, lean and setting the player's
 * position. Also the level-wide flags the AI scripts set (timer, stop time,
 * invincible). See docs/subsystems/player.md.
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
#include "game/player/headbob.h"
#include "game/player/inventory.h"
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
#include "game/level/gamemode.h"

ITEM_IDS starting_weapon[2];


PadRecord *g_Startpad[0x10];

s32 startpadcount;


// Set by AI scripts for the whole level. stop_time_flag: end the level on the
// next button press (2 once pressed). is_timer_active: FALSE while the player
// has no control. g_PlayerInvincible: no damage.
s32 stop_time_flag = 0;
s32 is_timer_active = 1;
bool g_PlayerInvincible = FALSE;


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

    if (gameIsMulti() || (gameIsTwoPlayerMission() && (person > 0)))
    {
        g_CurrentPlayer->controldef = frontGetMpControlStyle(person);
        optionsSetControlStyle(frontGetMpControlStyle(person));

        if (person == PLAYER_2)
        {
            optionsCoopCopyFromPlayer1();
            fileCoopRestorePartnerOptions();
        }
    }
    else if (gameIsTwoPlayerMission() && (playerGetCurrentNum() > 0))
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
