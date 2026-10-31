/**
 * Camera modes (g_CameraMode): first person, the level intro (intro cameras,
 * then the swirl round Bond), cutscenes, the death cameras and the end-of-level
 * orbit. playerSetCameraMode switches mode, playerAdvanceCameraMode moves to the
 * next, and playerTickFrozenCamera places the camera in the modes where the player
 * isn't in control. Co-op: which player's view fills the screen (coopUpdateSoloView).
 * See docs/subsystems/player.md.
 */
#include <boss.h>
#include <music.h>
#include "game/chr/chraction.h"
#include "game/level/frametiming.h"
#include "game/level/lv.h"
#include "game/math/interp.h"
#include "game/menu/watchmenu.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/prop/explosion.h"
#include "game/replay/ramromreplay.h"
#include "game/text/text.h"
#include "game/world/bgfog.h"
#include "random.h"
#include "game/player/cammode.h"
#include "game/player/coop.h"
#include "game/level/gamemode.h"
#include "game/mission/mission.h"

static coord3d g_DeathCamPos;

static struct PropRecord* g_DeathCamProp;

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

static f32 g_MpSwirlRotateSpeed;

static f32 g_MpSwirlAngleDegrees;

static f32 g_MpSwirlForwardSpeed;

static f32 g_MpSwirlDistance;


enum CAMERAMODE g_CameraMode = CAMERAMODE_NONE;

enum CAMERAMODE g_CameraAfterCinema = CAMERAMODE_NONE;

s32 camera_fade_active = 0;

f32 camera_transition_timer = 0;

s32 intro_camera_index = 1;

struct SetupIntroSwirl *g_IntroSwirl = NULL;

struct SetupIntroCamera *g_IntroCamera = NULL;

enum CAMERAMODE camera_mode = CAMERAMODE_NONE;

s32 g_IntroAnimationIndex = 0;

static struct struct_4 stage_intro_anim_table[] = {
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


u32 playerGetCameraMode(void)
{
    return g_CameraMode;
}


/**
 * Picks a spot for the death camera (g_DeathCamPos) looking at pos: a random
 * direction, at least camera_dist away with clear floor between, at a random height
 * near the floor. prop2 is ignored by the line tests. FALSE if no spot was found.
 */
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
 * Co-op: switching between split screen and one full-screen view fades
 * through black. The screen fades out over COOP_VIEW_FADE_TICKS (60 Hz
 * ticks), the layout changes, then it fades back in. g_CoopViewFade is the
 * black overlay's strength, drawn over the whole frame by coopDrawViewFade.
 */
#define COOP_VIEW_FADE_TICKS 10.0f

f32 g_CoopViewFade = 0.0f;

static s32 s_CoopShownView = -1;
static s32 s_CoopShownIsCinema = FALSE;
static s32 s_CoopViewFadingOut = FALSE;
static s32 s_CoopViewSnap = TRUE;

/**
 * Co-op: at level start the first layout (the intro, full-screen) is shown
 * straight away, without a fade.
 */
void coopResetViewFade(void)
{
    g_CoopViewFade = 0.0f;
    s_CoopShownView = -1;
    s_CoopShownIsCinema = FALSE;
    s_CoopViewFadingOut = FALSE;
    s_CoopViewSnap = TRUE;
}

/**
 * Co-op: once per frame, before the players move, decide whether one view
 * fills the screen on its own (g_CoopSoloView). That is the case while a
 * player has their watch out, which pauses the game for both, and while the
 * camera plays a cutscene, the intro or the final death replay. The other
 * player is then neither moved nor drawn, and during a cutscene their body
 * is hidden as well. A change of layout waits for the fade to black, so the
 * old layout (and who moves) stays until the screen is black.
 */
void coopUpdateSoloView(void)
{
    s32 count = getPlayerCount();
    s32 wanted = -1;
    s32 wantedIsCinema = FALSE;
    s32 i;

    g_CoopSoloView = -1;
    g_CoopSoloViewIsCinema = FALSE;

    if (!gameIsTwoPlayerMission() || (count < 2))
    {
        g_CoopViewFade = 0.0f;
        return;
    }

    for (i = 0; i < count; i++)
    {
        if (g_playerPointers[i]->watch_animation_state != WATCH_ANIMATION_0x0)
        {
            wanted = i;
            break;
        }
    }

    if ((wanted < 0) && coopIsCinemaCameraMode(g_CameraMode) && (g_CoopCinemaPlayer >= 0) && (g_CoopCinemaPlayer < count))
    {
        wanted = g_CoopCinemaPlayer;
        wantedIsCinema = TRUE;
    }

    if (s_CoopViewSnap)
    {
        s_CoopViewSnap = FALSE;
        s_CoopShownView = wanted;
        g_CoopViewFade = 0.0f;
    }
    else
    {
        f32 step = g_FrameDelta / COOP_VIEW_FADE_TICKS;

        if (wanted != s_CoopShownView)
        {
            s_CoopViewFadingOut = TRUE;
        }

        if (s_CoopViewFadingOut)
        {
            g_CoopViewFade += step;

            if (g_CoopViewFade >= 1.0f)
            {
                g_CoopViewFade = 1.0f;
                s_CoopShownView = wanted;
                s_CoopViewFadingOut = FALSE;
            }
        }
        else if (g_CoopViewFade > 0.0f)
        {
            g_CoopViewFade -= step;

            if (g_CoopViewFade < 0.0f)
            {
                g_CoopViewFade = 0.0f;
            }
        }
    }

    // The same view staying full-screen (a watch closing into a cutscene,
    // say) needs no fade, only the new reason.
    if (s_CoopShownView == wanted)
    {
        s_CoopShownIsCinema = wantedIsCinema;
    }

    g_CoopSoloView = s_CoopShownView;
    g_CoopSoloViewIsCinema = (s_CoopShownView >= 0) && s_CoopShownIsCinema;
}


/**
 * Co-op: the intro shows both players. The partner, who stands beside player 1
 * (coopFindPartnerSpawn), plays the same intro animation in the swirl. Their
 * camera mode is set to 1 so their body is ticked like player 1's (chrTick);
 * their own player tick resets it when first person starts.
 */
static void coopStartPartnerIntro(struct ModelAnimation *anim, f32 startframe, f32 speed, f32 endframe)
{
    s32 cur = playerGetCurrentNum();
    s32 i;

    for (i = 0; i < getPlayerCount(); i++)
    {
        struct player *partner = g_playerPointers[i];
        struct ChrRecord *chr;

        if ((i == cur) || (partner->bodyModel == NULL) || (partner->prop->chr == NULL))
        {
            continue;
        }

        modelSetAnimation(partner->bodyModel, anim, 0, startframe, speed, 0.0f);

        if (endframe > 0.0f)
        {
            modelSetAnimEndFrame(partner->bodyModel, endframe);
        }

        chr = partner->prop->chr;
        chr->actiontype = ACT_BONDINTRO;
        chr->sleep = 0;
        partner->cameramode = 1;
    }
}

/**
 * Co-op: whether the partner is shown during a cinema camera mode. They sit
 * out cutscenes and death replays, but the level intro shows both players
 * unless they share a spawn spot.
 */
bool coopShowPartnerInCinema(void)
{
    // If the level start had no room for the partner (e.g. Facility's vent,
    // Frigate), they spawn inside player 1, so leave them out of the intro.
    if (!gameIsCoop() || g_CoopPlayersOverlap)
    {
        return FALSE;
    }

    return (g_CameraMode == CAMERAMODE_INTRO) || (g_CameraMode == CAMERAMODE_FADESWIRL)
        || (g_CameraMode == CAMERAMODE_SWIRL);
}

/**
 * Switches the current player's camera to mode and sets it up: the intro camera
 * (or the swirl if the level has none or a demo is playing), the swirl, first
 * person, the death cameras, and so on.
 */
void playerSetCameraMode(s32 mode)
{
    if (gameIsTwoPlayerMission() && coopIsCinemaCameraMode(mode))
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

            if (coopShowPartnerInCinema())
            {
                coopStartPartnerIntro(anim, ftemp_1, ftemp_3, endframe);
            }
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

        if (gameIsMulti())
        {
            fogLoadLevelEnvironment(bossGetStageNum(), 0);
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
        {
            gunEquip(GUNLEFT, starting_weapon[GUNLEFT]);
            gunEquip(GUNRIGHT, starting_weapon[GUNRIGHT]);
        }

        g_Mission.exitOnPress = 0;
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

        if (g_Mission.explodeTankOnDeath && (g_CurrentPlayer->tankMounted != NULL))
        {
            // removed
        }
        else
        {
            // This branch restarts Bond's death animation for his death replay
            g_CurrentPlayer->tankDriving = 0;

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

        if (g_Mission.explodeTankOnDeath && (g_CurrentPlayer->tankMounted != NULL))
        {
            focusprop = g_CurrentPlayer->tankMounted;
            camdist = 500.0f; // distance to place the camera
            focuspos.f[0] = g_CurrentPlayer->tankMounted->pos.f[0];
            focuspos.f[1] = g_CurrentPlayer->tankMounted->pos.f[1];
            focuspos.f[2] = g_CurrentPlayer->tankMounted->pos.f[2];
            collisionprop = g_CurrentPlayer->tankMounted;
            collisionpos.f[0] = g_CurrentPlayer->tankMounted->pos.f[0];
            collisionpos.f[1] = g_CurrentPlayer->tankMounted->pos.f[1];
            collisionpos.f[2] = g_CurrentPlayer->tankMounted->pos.f[2];
            focustile = g_CurrentPlayer->tankMounted->stan;
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

            if ((g_Mission.explodeTankOnDeath != 0) && (g_CurrentPlayer->tankMounted != NULL))
            {
                explosionCreate(g_CurrentPlayer->tankMounted, &g_CurrentPlayer->tankMounted->pos, g_CurrentPlayer->tankMounted->stan, EXPLOSION_DEF_STANDARD, 0, playerGetCurrentNum(), g_CurrentPlayer->tankMounted->rooms, 0);
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


/**
 * Moves on to the next camera mode when the current one ends: intro, then the
 * swirl, then first person; the single-player death camera to the multiplayer one.
 */
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
 * The camera while the player isn't in control (playerMoveFrozen): the intro
 * cameras and their text, the swirl round Bond, cutscenes, the death cameras and
 * the post-mission orbit. Writes the camera position, tile and look-at point.
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


// 1 while the camera is away from Bond's eyes (playerTickFrozenCamera), 0 in first person
void playerSetCameraModeRaw(s32 mode)
{
    g_CurrentPlayer->cameramode = mode;
}
