/**
 * The pause watch: Start raises Bond's arm and the watch (watchTrigger, watchTick,
 * watchStepAnim), the camera tilts down (pause tilt) and zooms in (watchZoomTo), and the
 * watch menu (menu/watchmenu.c) runs on its face. watchRender draws the arm and watch.
 * See docs/subsystems/player.md.
 */
#include "game/audio/levelmusic.h"
#include "game/gfx/dyn.h"
#include "game/hud/healthbar.h"
#include "game/level/frametiming.h"
#include "game/level/lv.h"
#include "game/math/quaternion.h"
#include "game/menu/watchmenu.h"
#include "game/model/model.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/prop/alarm.h"
#include "game/replay/ramromreplay.h"
#include "game/world/bgfog.h"
#include "game/player/watch.h"
#include <ultra64.h>
#include <bondtypes.h>
#include <fr.h>
#include <music.h>
#include "game/level/gamemode.h"

f32 watch_transition_time = 0.90909088;

static coord3d ZeroCoordWatchPos = {0};


void watchInitPauseTilt(void)
{
    g_CurrentPlayer->pause_starting_angle = g_CurrentPlayer->vv_verta;
    g_CurrentPlayer->pause_transition_time = 0.0f;
    g_CurrentPlayer->pause_state = 0;
}


/**
 * Set the pause tilt's start and end pitch. Also calculates how long the transition should take.
 * The greater the angular distance between the player's camera pitch and the pause pitch of -40 degrees,
 * the longer it takes to pause.
 *
 * @param topause: When set, pause_target_verta will be -40.0f, otherwise g_CurrentPlayer->vv_verta.
 * @return: How long the tilt should take.
 */
f32 watchSetupPauseTilt(bool topause)
{
    f32 anglediff;
    f32 duration;

    if (topause)
    {
        g_CurrentPlayer->pause_saved_verta = g_CurrentPlayer->vv_verta;
        g_CurrentPlayer->pause_target_verta = -40.0f;
    }
    else
    {
        g_CurrentPlayer->pause_saved_verta = g_CurrentPlayer->pause_starting_angle;
        g_CurrentPlayer->pause_target_verta = g_CurrentPlayer->vv_verta;
    }

    anglediff = g_CurrentPlayer->pause_saved_verta - g_CurrentPlayer->pause_target_verta;

    if (anglediff < 0.0f)
    {
        anglediff = -anglediff;
    }

    /**
     * If the delta between pause_saved_verta and pause_target_verta is large,
     * the time penalty per degree above 60 is cut in half.
     */
    if (anglediff >= 60.0f)
    {
        duration = (((anglediff - 60.0f) * 60.0f * 0.5f) / 60.0f) + 60.0f;
    }
    else if (anglediff <= 0.0f)
    {
        duration = 0.0f;
    }
    else
    {
        // this is a different `60` than the other values above!
        duration = (anglediff * 60.f) / 60.f;
    }

    return duration;
}


void watchStartPauseTilt(f32 duration) {
    g_CurrentPlayer->pause_transition_time = 0.0f;
    g_CurrentPlayer->pause_transition_duration = duration;
    g_CurrentPlayer->pause_state = 1;
}


void watchStartUnpauseTilt(f32 duration) {
    g_CurrentPlayer->pause_transition_time = 0.0f;
    g_CurrentPlayer->pause_transition_duration = duration;
    g_CurrentPlayer->pause_state = 2;
}


bool watchIsPauseTilting(void) {
    return (g_CurrentPlayer->pause_state != 0 && g_CurrentPlayer->pause_state != 3);
}


/**
 * Transition camera pitch from playing to watch menu (-40.0f degrees).
 * Then when the game is unpaused, transition the camera pitch back to its pitch before pausing began.
 */
void watchUpdatePauseTilt(void) {
    f32 prevverta;
    f32 frac;
    f32 weight;

    prevverta = g_CurrentPlayer->vv_verta;

    // Entering pause.
    if (g_CurrentPlayer->pause_state == 1) {
        g_CurrentPlayer->pause_transition_time += g_GlobalTimerDelta * watch_transition_time;

        if (g_CurrentPlayer->pause_transition_time < g_CurrentPlayer->pause_transition_duration) {
            // Cosine ease-in-out
            frac = g_CurrentPlayer->pause_transition_time / g_CurrentPlayer->pause_transition_duration;
            weight = (1.0f - cosf((frac * M_TAU_F) * 0.5f)) * 0.5f;

            g_CurrentPlayer->vv_verta = g_CurrentPlayer->pause_saved_verta
                + ((g_CurrentPlayer->pause_target_verta - g_CurrentPlayer->pause_saved_verta) * weight);
        } else {
            g_CurrentPlayer->vv_verta = g_CurrentPlayer->pause_target_verta;
            // Set pause state to paused.
            g_CurrentPlayer->pause_state = 3;
        }
    // Leaving pause.
    } else if (g_CurrentPlayer->pause_state == 2) {
        g_CurrentPlayer->pause_transition_time += g_GlobalTimerDelta * watch_transition_time;

        if (g_CurrentPlayer->pause_transition_time < g_CurrentPlayer->pause_transition_duration) {
            // Cosine ease-in-out
            frac = g_CurrentPlayer->pause_transition_time / g_CurrentPlayer->pause_transition_duration;
            weight = (1.0f - cosf((frac * M_TAU_F) * 0.5f)) * 0.5f;

            g_CurrentPlayer->vv_verta = g_CurrentPlayer->pause_target_verta
                + ((g_CurrentPlayer->pause_saved_verta - g_CurrentPlayer->pause_target_verta) * weight);
        } else {
            g_CurrentPlayer->vv_verta = g_CurrentPlayer->pause_saved_verta;
            // Set pause state to unpaused.
            g_CurrentPlayer->pause_state = 0;
        }
    }

    // Wrap vv_verta into [-180, 180)
    if (g_CurrentPlayer->vv_verta < -180.0f) {
        g_CurrentPlayer->vv_verta += 360.0f;
    } else if (g_CurrentPlayer->vv_verta >= 180.0f) {
        g_CurrentPlayer->vv_verta -= 360.0f;
    }

    /**
     * Calculate shortest angular velocity from previous frame,
     * scale it by g_GlobalTimerDelta,
     * clamp it to [-0.7, 0.7] so the pitch change is never too fast.
     */
    if (g_ClockTimer > 0) {
        g_CurrentPlayer->speedverta = g_CurrentPlayer->vv_verta - prevverta;

        if (g_CurrentPlayer->speedverta < 0.0f) {
            g_CurrentPlayer->speedverta += 360.0f;
        }

        if (g_CurrentPlayer->speedverta > 180.0f) {
            g_CurrentPlayer->speedverta -= 360.0f;
        }

        g_CurrentPlayer->speedverta /= g_GlobalTimerDelta + g_GlobalTimerDelta;

        if (g_CurrentPlayer->speedverta < -0.7f) {
            g_CurrentPlayer->speedverta = -0.7f;
        } else if (g_CurrentPlayer->speedverta > 0.7f) {
            g_CurrentPlayer->speedverta = 0.7f;
        }
    }
}


void watchStartZoom(f32 final, f32 time)
{
  g_CurrentPlayer->zoomintime = 0.00000000;
  g_CurrentPlayer->zoomintimemax = time;
  g_CurrentPlayer->zoominfovyold = g_CurrentPlayer->zoominfovy;
  g_CurrentPlayer->zoominfovynew = final;
}


f32 watchGetZoomTargetFovy(void) {

    if (g_CurrentPlayer->zoomintime < g_CurrentPlayer->zoomintimemax)
    {
        return g_CurrentPlayer->zoominfovynew;
    }

    return g_CurrentPlayer->zoominfovy;
}

/**
 * Triggers watch zoom if new value.
 *
 * @param zoominfovy: watch zoom fovy.
 */
void watchZoomTo(f32 zoominfovy)
{
    if (watchGetZoomTargetFovy() != zoominfovy)
    {
        if (zoominfovy < g_CurrentPlayer->zoominfovy)
        {
            watchStartZoom(zoominfovy, ((g_CurrentPlayer->zoominfovy - zoominfovy) * 15.0f) / 30.0f);

            return;
        }

        watchStartZoom(zoominfovy, ((zoominfovy - g_CurrentPlayer->zoominfovy) * 15.0f) / 30.0f);
    }
}


/**
 * Trigger watch zoom with default angle.
 */
void watchZoomToDefault(void)
{
    watchZoomTo(60.0f);
}


void watchZoomOnOpen(void)
{
    f32 f;

#if defined(VERSION_EU)
    f = ((6.09999990463f - g_CurrentPlayer->zoominfovy) * 45.0f) / -53.9000015259f;
#else
    f = ((5.9f - g_CurrentPlayer->zoominfovy) * 45.0f) / -54.1f;
#endif

    if (f < 0.0f)
    {
        f = -f;
    }

#if defined(VERSION_EU)
    watchStartZoom(6.09999990463f, f);
#else
    watchStartZoom(5.9f, f);
#endif

}


void watchZoomOnExit(void)
{
    f32 f;

#if defined(VERSION_EU)
    f = ((60.0f - g_CurrentPlayer->zoominfovy) * 45.0f) / -53.9000015259f;
#else
    f = ((60.0f - g_CurrentPlayer->zoominfovy) * 45.0f) / -54.1f;
#endif

    if (f < 0.0f)
    {
        f = -f;
    }

    watchStartZoom(60.0f, f);
}


s32 watchIsZooming(void)
{
    return (g_CurrentPlayer->zoomintime < g_CurrentPlayer->zoomintimemax);
}


void watchUpdateZoom(void)
{
    if (g_CurrentPlayer->zoomintime < g_CurrentPlayer->zoomintimemax)
    {
        if ((g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5) || (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc))
        {
#if defined(BUGFIX_R1)
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintime + (f32) g_FrameDelta60;
#else
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintime + (f32) g_FrameDelta;
#endif
        }
        else
        {
#if defined(BUGFIX_R1)
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintime + (g_FrameDelta60 * watch_transition_time);
#else
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintime + (g_FrameDelta * watch_transition_time);
#endif
        }

        if (g_CurrentPlayer->zoomintimemax < g_CurrentPlayer->zoomintime)
        {
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintimemax;
        }

        g_CurrentPlayer->zoominfovy =
            g_CurrentPlayer->zoominfovyold +
            (
                (g_CurrentPlayer->zoomintime *
                    (g_CurrentPlayer->zoominfovynew - g_CurrentPlayer->zoominfovyold)
                )
                / g_CurrentPlayer->zoomintimemax
            );
    }
    else
    {
        g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintimemax;
        g_CurrentPlayer->zoominfovy = g_CurrentPlayer->zoominfovynew;
    }

    playerSetFovY(g_CurrentPlayer->zoominfovy);
    viSetFovY(g_CurrentPlayer->zoominfovy);
}


f32 watchGetOpenFrac(void)
{
    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x4)
    {
        return ((45.0f - g_CurrentPlayer->zoomintimemax) + g_CurrentPlayer->zoomintime) / 45.0f;
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x6)
    {
        return (g_CurrentPlayer->zoomintimemax - g_CurrentPlayer->zoomintime) / 45.0f;
    }

    if ((g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5) || (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc))
    {
        return 1.0f;
    }

    return 0.0f;
}


void watchInitHandModel(void) {
    ModelFileHeader *itemheader;

    itemheader = gunGetHandModelHeader(1);
    modelCalculateRwDataLen(itemheader);
    animInit(&g_CurrentPlayer->watchModel, itemheader, g_CurrentPlayer->watchModelRwData);
    modelSetScale(&g_CurrentPlayer->watchModel, c_item_entries[41].scale * 0.10000001f);
    modelSetAnimation(&g_CurrentPlayer->watchModel, ANIM(bond_watch), 0, 0.0f, 0.5f * watch_transition_time, 0.0f);
    g_CurrentPlayer->step_in_view_watch_animation = 0;
}


void watchStartRaise(f32 speed)
{
    if (g_CurrentPlayer->step_in_view_watch_animation == 0)
    {
        g_CurrentPlayer->pause_watch_related_scaled = 20.0f / speed;
    }
    else
    {
        g_CurrentPlayer->pause_watch_related_scaled = (20.0f - g_CurrentPlayer->watchModel.frame) / speed;
    }

    g_CurrentPlayer->step_in_view_watch_animation = 1;
}


void watchStartLower(f32 speed)
{
    if (g_CurrentPlayer->step_in_view_watch_animation == 3)
    {
        g_CurrentPlayer->pause_watch_related_scaled = 20.0f / speed;
    }
    else
    {
        g_CurrentPlayer->pause_watch_related_scaled = g_CurrentPlayer->watchModel.frame / speed;
    }

    g_CurrentPlayer->step_in_view_watch_animation = 2;
}


void watchStepAnim(void)
{
    if ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
    {
        if (g_CurrentPlayer->step_in_view_watch_animation == 1)
        {
            g_CurrentPlayer->pause_animation_counter += g_GlobalTimerDelta * watch_transition_time * g_CurrentPlayer->pause_watch_related_scaled;

            if (g_CurrentPlayer->pause_animation_counter > 20.0f)
            {
                g_CurrentPlayer->pause_animation_counter = 20.0f;
                g_CurrentPlayer->step_in_view_watch_animation = 3;
            }
        }
        else if (g_CurrentPlayer->step_in_view_watch_animation == 2)
        {
            g_CurrentPlayer->pause_animation_counter -= g_GlobalTimerDelta * watch_transition_time * g_CurrentPlayer->pause_watch_related_scaled;

            if (g_CurrentPlayer->pause_animation_counter < 0.0f)
            {
                g_CurrentPlayer->pause_animation_counter = 0.0f;
                g_CurrentPlayer->step_in_view_watch_animation = 0;
            }
        }

        modelSetAnimFrame2(&g_CurrentPlayer->watchModel, g_CurrentPlayer->pause_animation_counter, 0.0f);
    }
}


f32 watchGetAnimFrac(void)
{
    if ((g_CurrentPlayer->step_in_view_watch_animation == 1) || (g_CurrentPlayer->step_in_view_watch_animation == 2))
    {
        return g_CurrentPlayer->pause_animation_counter / 20.0f;
    }

    if (g_CurrentPlayer->step_in_view_watch_animation == 3)
    {
        return 1.0f;
    }

    return 0.0f;
}


void playerSetOutsideWatchMenu(s32 outside) {
    g_CurrentPlayer->outside_watch_menu = outside;
}


void pauseStopSounds(void)
{
    struct hand *hand;
	s32 i;
    ObjectRecord *obj;
    PropRecord *prop;

    alarmStopSound();
    gasStopSound();

    for (i = 0; i < 2; i++)
    {
        hand = &g_CurrentPlayer->hands[i];

        if (hand->audioHandle && sndGetPlayingState(hand->audioHandle) != AL_STOPPED) {
			sndDeactivate(hand->audioHandle);
		}
    }

    for (i = 0; i < 2; i++)
    {
        if (g_Tank.sfx[i] && sndGetPlayingState(g_Tank.sfx[i]) != AL_STOPPED) {
			sndDeactivate(g_Tank.sfx[i]);
		}
    }

    for (prop = propsGetActiveTail(); prop; prop = prop->prev)
    {
        if (prop->type != PROP_TYPE_DOOR && prop->type == PROP_TYPE_OBJ)
        {
            obj = prop->obj;

            if (obj->type == PROPDEF_VEHICHLE)
            {
                VehichleRecord *vehicle = (VehichleRecord *)prop->obj;
                if (vehicle->Sound && sndGetPlayingState(vehicle->Sound) != AL_STOPPED) {
                    sndDeactivate(vehicle->Sound);
                }
            }
            else if (obj->type == PROPDEF_AIRCRAFT)
            {
                AircraftRecord *aircraft = (AircraftRecord *)prop->obj;
                if (aircraft->Sound && sndGetPlayingState(aircraft->Sound) != AL_STOPPED) {
                    sndDeactivate(aircraft->Sound);
                }
            }

        }
    }
}


/**
 * Runs the watch for a frame: steps the arm through its states (swap the gun away, raise,
 * zoom in, menu, zoom out, lower, swap back), tilting the camera, locking controls,
 * hiding the HUD and pausing the music around it, and ticks the watch menu while open.
 */
void watchTick(void)
{
#if defined(VERSION_EU)
    #define WATCH_VAR_LOWER 14
    #define WATCH_VAR_UPPER 29
#else
    #define WATCH_VAR_LOWER 17
    #define WATCH_VAR_UPPER 35
#endif

    s32 delay;
    s32 holdingWatchItem;
    f32 raiseTime;
    f32 tiltTime;
    f32 lowerTime;
    f32 untiltTime;
    f32 minTime;

    if (g_CurrentPlayer->watch_animation_state)
    {
        holdingWatchItem = gunGetWeapon(GUNRIGHT) == ITEM_TRIGGER || gunGetWeapon(GUNRIGHT) == ITEM_WATCHLASER;

#if defined (VERSION_US)
        delay = WATCH_VAR_LOWER;

        if (holdingWatchItem)
        {
            delay = WATCH_VAR_UPPER;
        }
#else
        delay = (holdingWatchItem) ? WATCH_VAR_UPPER : WATCH_VAR_LOWER;
#endif

        g_CurrentPlayer->timer_1C4 += g_ClockTimer;
        g_CurrentPlayer->watch_pause_time += 1;
        g_CurrentPlayer->pausing_flag = FALSE;

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x1)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                gunStartWatchSwap(GUNLEFT, ITEM_SUIT_LF_HAND);
            }
            else if (g_CurrentPlayer->watch_pause_time == 2)
            {
                if (holdingWatchItem)
                {
                    gunStartWatchSwap(GUNRIGHT, ITEM_UNARMED);
                }
            }
            else
            {
                if (
                    (gunGetShownWeapon(1) == ITEM_SUIT_LF_HAND)
                    && (gunHandModelIsEmpty(GUNLEFT) != ITEM_UNARMED)
                    && (
                        (holdingWatchItem == 0)
                        || (
                            (gunGetShownWeapon(0) == ITEM_UNARMED)
                            && (gunHandModelIsEmpty(GUNRIGHT) != ITEM_UNARMED)))
                    && (g_CurrentPlayer->timer_1C4 >= delay))
                {
                    g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x2;
                    g_CurrentPlayer->watch_pause_time = 1;
                    g_CurrentPlayer->timer_1C4 = 0;
                }
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x2)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                gunSetSightVisible(GUNAMMOREASON_DAMAGE, FALSE);
                gunSetGunAmmoVisible(GUNSIGHTREASON_NOCONTROL, FALSE);
                hudmsgsSetOff(PLAYERFLAG_LOCKCONTROLS);
                hudmsgTopSetFlag(PLAYERFLAG_LOCKCONTROLS);
                countdownTimerSetVisible(COUNTDOWN_HIDE_WATCH, FALSE);

                if ((g_CurrentPlayer->pause_state == 0) || (g_CurrentPlayer->pause_state == 2) || (g_CurrentPlayer->pause_state == 3))
                {
                    minTime = watchSetupPauseTilt(TRUE);

                    if (minTime < 30.0f)
                    {
                        minTime = 30.0f;
                    }

                    watchStartPauseTilt(minTime);
                }
            }

            if ((g_CurrentPlayer->pause_transition_duration - g_CurrentPlayer->pause_transition_time) < 30.0f)
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x3;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;

                watchInitHandModel();
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x3)
        {
            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                raiseTime = 40.0f;

                if ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
                {
                    raiseTime = ((20.0f - g_CurrentPlayer->watchModel.frame) * 40.0f) / 20.0f;
                }

                if ((g_CurrentPlayer->pause_state == 0) || (g_CurrentPlayer->pause_state == 2) || (g_CurrentPlayer->pause_state == 3))
                {
                    tiltTime = watchSetupPauseTilt(TRUE);
                    minTime = raiseTime - 10.0f;

                    if (tiltTime < minTime)
                    {
                        tiltTime = minTime;
                    }

                    watchStartPauseTilt(tiltTime);

                    minTime = tiltTime + 10.0f;

                    if (raiseTime < minTime)
                    {
                        raiseTime = minTime;
                    }
                }

                watchStartRaise(raiseTime);
            }

            if (
                ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
                ||
                watchIsPauseTilting()
                )
            {
                g_CurrentPlayer->pausing_flag = TRUE;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x4;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x4)
        {
            if ((g_CurrentPlayer->watch_pause_time == 1) && (g_CurrentPlayer->watchSfxPending != 0))
            {
                sndPlaySfx(g_musicSfxBufferPtr, WATCH_ON_SFX, NULL);
                g_CurrentPlayer->watchSfxPending = 0;
            }

            watchZoomOnOpen();

            if (watchIsZooming() != 0)
            {
                g_CurrentPlayer->pausing_flag = TRUE;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x5;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
                g_CurrentPlayer->watchSfxPending = 1;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5)
        {
            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                levelMusicPause();
            }

            g_CurrentPlayer->pausing_flag = TRUE;
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc)
        {
            g_CurrentPlayer->pausing_flag = TRUE;

            if (g_CurrentPlayer->watch_pause_time >= 3)
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x6;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
                sndPlaySfx(g_musicSfxBufferPtr, WATCH_OFF_SFX, NULL);
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x6)
        {
            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                watchZoomOnExit();
                if (holdingWatchItem)
                {
                    gunStartWatchSwap(GUNRIGHT, ITEM_UNARMED);
                }
                else
                {
                    gunClearWatchMenuWeapon(0);
                }
            }
            if (watchIsZooming())
            {
                g_CurrentPlayer->pausing_flag = TRUE;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x7;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
                g_CurrentPlayer->watchSfxPending = 1;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x7)
        {
            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                lowerTime = 40.0f;
                untiltTime = watchSetupPauseTilt(FALSE);

                if ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
                {
                    lowerTime = (g_CurrentPlayer->watchModel.frame * 40.0f) / 20.0f;
                }

                minTime = lowerTime + 20.0f;

                if (untiltTime < minTime)
                {
                    untiltTime = minTime;
                }

                watchStartUnpauseTilt(untiltTime);
                watchStartLower(lowerTime);
            }

            if ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
            {
                g_CurrentPlayer->pausing_flag = TRUE;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x8;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x8)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (g_CurrentPlayer->watch_pause_time == 1)
            {

                if ((gunGetShownWeapon(GUNLEFT) != gunGetWeapon(GUNLEFT)) && (gunHandModelIsEmpty(GUNLEFT) != ITEM_UNARMED))
                {
                    gunStartWatchSwap(GUNLEFT, gunGetWeapon(GUNLEFT));
                }

                if (holdingWatchItem)
                {
                    if ((gunGetShownWeapon(GUNRIGHT) != gunGetWeapon(GUNRIGHT)) && (gunHandModelIsEmpty(GUNRIGHT) != ITEM_UNARMED))
                    {
                        gunStartWatchSwap(GUNRIGHT, gunGetWeapon(GUNRIGHT));
                    }
                }
            }
            else if (!watchIsPauseTilting())
            {
                if (
                    (gunGetShownWeapon(GUNLEFT) == gunGetWeapon(GUNLEFT))
                    && (gunHandModelIsEmpty(GUNLEFT) != ITEM_UNARMED)
                    && (
                        (holdingWatchItem == 0)
                        || (((gunGetShownWeapon(GUNRIGHT) == gunGetWeapon(GUNRIGHT))) && (gunHandModelIsEmpty(GUNRIGHT) != ITEM_UNARMED))))
                {
                    gunClearWatchMenuWeapon(1);

                    if (holdingWatchItem)
                    {
                        gunClearWatchMenuWeapon(0);
                    }

                    gunSetSightVisible(GUNAMMOREASON_DAMAGE, TRUE);
                    gunSetGunAmmoVisible(GUNSIGHTREASON_NOCONTROL, TRUE);
                    hudmsgsSetOn(1);
                    hudmsgTopClearFlag(1);
                    countdownTimerSetVisible(COUNTDOWN_HIDE_WATCH, TRUE);

                    g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x0;
                    g_CurrentPlayer->watch_pause_time = 0;
                    g_CurrentPlayer->timer_1C4 = 0;
                }
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x9)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (
                (gunGetWeapon(GUNLEFT) != gunGetShownWeapon(GUNLEFT))
                && (gunHandModelIsEmpty(GUNLEFT) != ITEM_UNARMED)
                && (
                    (holdingWatchItem == 0)
                    || (((gunGetWeapon(GUNRIGHT) != gunGetShownWeapon(GUNRIGHT))) && (gunHandModelIsEmpty(GUNRIGHT) != ITEM_UNARMED))))
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x8;
                g_CurrentPlayer->watch_pause_time = 0;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xa)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                watchStartUnpauseTilt(watchSetupPauseTilt(FALSE));
            }

            if (
                (gunGetWeapon(GUNLEFT) != gunGetShownWeapon(GUNLEFT))
                && (gunHandModelIsEmpty(GUNLEFT) != ITEM_UNARMED)
                && (
                    (holdingWatchItem == 0)
                    || (((gunGetWeapon(GUNRIGHT) != gunGetShownWeapon(GUNRIGHT))) && (gunHandModelIsEmpty(GUNRIGHT) != ITEM_UNARMED))))
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x8;
                g_CurrentPlayer->watch_pause_time = 0;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xb)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (
                (gunGetWeapon(GUNLEFT) == gunGetShownWeapon(GUNLEFT))
                && (gunHandModelIsEmpty(GUNLEFT) != ITEM_UNARMED)
                && (
                    (holdingWatchItem == 0)
                    || (
                        ( (gunGetWeapon(GUNRIGHT) == gunGetShownWeapon(GUNRIGHT)))
                        && (gunHandModelIsEmpty(GUNRIGHT) != ITEM_UNARMED))))
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
                g_CurrentPlayer->watch_pause_time = 0;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xd)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (gunHandModelIsEmpty(GUNLEFT)
                && gunHandModelIsEmpty(GUNRIGHT))
            {
                if ((g_CurrentPlayer->hands[GUNLEFT].weapon_action_state != GUN_ANIM_STATE_SWITCH_LOWER)
                    && (g_CurrentPlayer->hands[GUNLEFT].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP)
                    && (g_CurrentPlayer->hands[GUNLEFT].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD)
                    && (g_CurrentPlayer->hands[GUNLEFT].weapon_action_state != GUN_ANIM_STATE_SWITCH_RAISE))
                {
                    g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
                    g_CurrentPlayer->watch_pause_time = 0;
                    g_CurrentPlayer->timer_1C4 = 0;
                }
            }
        }

        watchUpdatePauseTilt();
        watchStepAnim();
        watchUpdateZoom();
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5)
    {
        lvSetControlsLocked(TRUE);
        watchMenuTick();
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc)
    {
        lvSetControlsLocked(TRUE);
    }
    else
    {
        lvSetControlsLocked(FALSE);
    }

#undef WATCH_VAR_LOWER
#undef WATCH_VAR_UPPER
}


void watchRequestToggle(void) 
{
    g_CurrentPlayer->open_close_solo_watch_menu = TRUE;
}


/**
 * Start was pressed: opens the watch (raising the arm, or at once if instant) and builds
 * its health bars and screen tabs, or closes it if it is open.
 */
void watchTrigger(s32 instant)
{
    struct WatchVertex *ptr_a;
    Gfx *ptr_b;
    struct WatchVertex *ptr_copy;
    s32 i;

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
    {
        if (instant == 0)
        {
            watch_transition_time *= 1.1f;
            if (watch_transition_time > 1.7f)
            {
                watch_transition_time = 1.7f;
            }

            if ((gunHandModelIsEmpty(GUNLEFT) != ITEM_UNARMED)
                && (gunHandModelIsEmpty(GUNRIGHT) != ITEM_UNARMED)
                && (g_CurrentPlayer->hands[GUNLEFT].weapon_action_state != GUN_ANIM_STATE_SWITCH_LOWER)
                && (g_CurrentPlayer->hands[GUNLEFT].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP)
                && (g_CurrentPlayer->hands[GUNLEFT].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD)
                && (g_CurrentPlayer->hands[GUNLEFT].weapon_action_state != GUN_ANIM_STATE_SWITCH_RAISE))
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0xd;
            }

            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;

            watchInitPauseTilt();
            watchZoomToDefault();

            hudMakeDamageSegments(g_CurrentPlayer->watchGfx->armor_display_values, 1, currentPlayerGetArmor());
            hudRenderGaugeBar(g_CurrentPlayer->watchGfx->watchArmorGdl, g_CurrentPlayer->watchGfx->armor_display_values, 0x2E);

            hudMakeDamageSegments(g_CurrentPlayer->watchGfx->health_display_values, -1, currentPlayerGetHealth());
            hudRenderGaugeBar(g_CurrentPlayer->watchGfx->watchHealthGdl, g_CurrentPlayer->watchGfx->health_display_values, 0x2E);

            watchMenuOpen();

            /**
             * This section is for rendering the selected screen rectangles.
            */
            ptr_b = g_CurrentPlayer->watchGfx->watchTabsGdl;
            ptr_a = &g_CurrentPlayer->watchGfx->watchTabsVtx->vtx[0];

            for (i=0;
                i<(WATCH_NUMBER_SCREENS * WATCH_SCREEN_SELECT_RECTANGLE_HSTEP);
                i+=WATCH_SCREEN_SELECT_RECTANGLE_HSTEP)
            {
                // Note: colours are set here but overwritten by watchMenuSetTabColours
                ptr_copy = ptr_a;
                ptr_a = watchMakeRectVerts(ptr_a, i, 0, 0x64, 0x14, -0x12B, 0x136);
                ptr_b = watchRenderQuad(ptr_b, ptr_copy);
            }

            gSPEndDisplayList(ptr_b);
            /**
             * End watch screen select rectangles.
            */

            /**
             * This section is related to rendering static on the watch menu.
             * Static is defined by a horizontal bar in the middle of the screen.
            */
            ptr_a = &g_CurrentPlayer->watchGfx->watchStaticVtx->vtx[0];
            ptr_b = g_CurrentPlayer->watchGfx->watchStaticGdl;

            ptr_copy = &g_CurrentPlayer->watchGfx->watchStaticVtx->vtx[0];
            watchMakeRectVerts(ptr_a, 0, 0, 0x398, 0x14, -0x1CC, 0);
            ptr_b = watchRenderQuad(ptr_b, ptr_copy);

            gSPEndDisplayList(ptr_b);
            /**
             * End watch static section.
            */
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x1)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x9;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x2)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0xa;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x3)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x7;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x4)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x6;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5)
    {
        // Co-op: the save folder holds player 1's options; player 2's are
        // kept separately.
        if (!gameIsTwoPlayerMission() || (coopPlayerPerson(playerGetCurrentNum()) == 0))
        {
            fileClearCurrentFolder();
        }
        else
        {
            fileCoopStorePartnerOptions();
        }

        // The draw distance option may have changed.
        if (gameIsTwoPlayerMission())
        {
            fogReloadCurrentEnvironment();
        }

        levelMusicResume();
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0xc;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
        g_CurrentPlayer->open_close_solo_watch_menu = FALSE;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc)
    {
        // removed
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x6)
    {
        if (instant == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x4;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
            watchMenuOpen();
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x7)
    {
        if (instant == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x3;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
            watchMenuOpen();
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x8)
    {
        if (instant == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0xb;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
            watchMenuOpen();
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x9)
    {
        if (instant == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xa)
    {
        if (instant == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xb)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x8;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xd)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x0;
    }
}

Gfx *watchRender(Gfx *gdl)
{
    ModelRenderData renderdata;
    Mtxf watchmtx;
    coord3d watchpos;
    f32 t;
    Mtxf *matrices;
    ModelFileHeader *objheader;
    f32 *nodepos;
    union ModelRwData *rwdata;
    Mtx *perspmtx;
    u16 perspNorm;
 
    if (g_CurrentPlayer->watch_animation_state == 0)
    {
        goto end;
    }
 
    if (g_CurrentPlayer->pausing_flag == FALSE)
    {
        goto end;
    }
 
    renderdata = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};
    watchpos = ZeroCoordWatchPos;
    objheader = gunGetHandModelHeader(GUNLEFT);
    nodepos = (f32 *) objheader->Switches[3];
    rwdata = modelGetNodeRwData(&g_CurrentPlayer->watchModel, (ModelNode *) nodepos);
    perspmtx = dynAllocateMatrix();
#if defined(VERSION_EU)
    guPerspective(perspmtx, &perspNorm, g_CurrentPlayer->zoominfovy, 1.4005603f, 10.0f, 300.0f, 1.0f);
#else
    guPerspective(perspmtx, &perspNorm, g_CurrentPlayer->zoominfovy, 1.4545455f, 10.0f, 300.0f, 1.0f);
#endif
 
    gSPMatrix(gdl++, OS_PHYSICAL_TO_K0((u32) perspmtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPPerspNormalize(gdl++, perspNorm);

    {
        Mtxf targetmtx;
        coord3d currot;
        coord3d targetrot;
        quatf quat1;
        quatf quat2;
        quatf quat3;
        coord3d targetpos;
        s32 total_seconds;
        s32 seconds;
        s32 total_minutes;
        s32 minutes;
        f32 framesfrac;
        f32 secondsAngle;
        f32 minutesAngle;
        f32 hoursAngle;
        s32 time;
        f32 *nodepos2;
        Mtxf handmtx;
        Mtx *finalmtx;
 
        rwdata->Switch.visible = g_CurrentPlayer->outside_watch_menu;
    
        watchpos.x = (g_CurrentPlayer->bond.theta_transform.x * (g_CurrentPlayer->headbodyoffset.z + (-12.0f))) + (g_CurrentPlayer->bond.collision_position.x + (g_CurrentPlayer->headbodyoffset.x * (-g_CurrentPlayer->bond.theta_transform.z)));
        watchpos.y = g_CurrentPlayer->headbodyoffset.y + g_CurrentPlayer->bond.collision_position.y;
        watchpos.z = (g_CurrentPlayer->bond.theta_transform.z * (g_CurrentPlayer->headbodyoffset.z + (-12.0f))) + (g_CurrentPlayer->bond.collision_position.z + (g_CurrentPlayer->headbodyoffset.x * g_CurrentPlayer->bond.theta_transform.x));
    
        mtx4LoadYRotationAndTranslation(&watchpos, (360.0f - g_CurrentPlayer->vv_theta) * 0.017453292f, &watchmtx);
        mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), &watchmtx);
        matrices = dynAllocate(objheader->numMatrices << 6);
        playerSelectCuff(&g_CurrentPlayer->watchModel, objheader, 4);
        renderdata.basemtx = &watchmtx;
        renderdata.mtxlist = matrices;
        modelCalcMatrices(&renderdata, &g_CurrentPlayer->watchModel);
        nodepos = (f32 *) objheader->Switches[0]->Data;
        time = watch_time_0;
        t = g_CurrentPlayer->watchModel.frame / 20.0f;
    
        if (t > 1.0f)
        {
            t = 1.0f;
        }
    
        targetpos.x = matrices->m[3][0] + (((g_CurrentPlayer->watchPos.x - (nodepos[0] * g_CurrentPlayer->watchModel.scale)) - matrices->m[3][0]) * t);
        targetpos.y = matrices->m[3][1] + (((g_CurrentPlayer->watchPos.y + (nodepos[2] * g_CurrentPlayer->watchModel.scale)) - matrices->m[3][1]) * t);
        targetpos.z = matrices->m[3][2] + (((g_CurrentPlayer->watchPos.z - (nodepos[1] * g_CurrentPlayer->watchModel.scale)) - matrices->m[3][2]) * t);
    
        mtx4LoadFacing(&targetmtx, 0.0f, 0.0f, 0.0f, g_CurrentPlayer->watchLook.x, g_CurrentPlayer->watchLook.y, g_CurrentPlayer->watchLook.z, g_CurrentPlayer->watchUp.x, g_CurrentPlayer->watchUp.y, g_CurrentPlayer->watchUp.z);
        mtx4GetRotation(matrices, &currot);
        mtx4GetRotation(&targetmtx, &targetrot);
        quatFromAngles(&currot, &quat1);
        quatFromAngles(&targetrot, &quat2);
        quatMakeShortestPath(&quat1, &quat2);
        quatSlerp(&quat1, &quat2, t, &quat3);
        quatToMtx4(&quat3, matrices);
        mtx4SetTranslation(&targetpos, matrices);
        mtx4ScaleRows012(g_CurrentPlayer->watchModel.scale, matrices);
        total_seconds = time / 60;
        seconds = total_seconds % 60;
        total_minutes = total_seconds / 60;
        minutes = total_minutes % 60;
        framesfrac = ((f32) (time % 60)) / 60.0f;
        secondsAngle = ((-(((f32) seconds) + framesfrac)) * M_TAU_F) / 60.0f;
        minutesAngle = ((((-((f32) minutes)) * M_TAU_F) / 60.0f) * 1.0f) + (secondsAngle / 60.0f);
        hoursAngle = ((((-((f32) ((total_seconds / 3600) % 12))) * M_TAU_F) / 12.0f) + (minutesAngle / 12.0f)) + (secondsAngle / 720.0f);
    
        while (secondsAngle < 0.0f)
        {
            secondsAngle += M_TAU_F;
        }
    
        while (minutesAngle < 0.0f)
        {
            minutesAngle += M_TAU_F;
        }
    
        while (hoursAngle < 0.0f)
        {
            hoursAngle += M_TAU_F;
        }
    
        mtx4LoadYRotationAndTranslation((coord3d *) objheader->Switches[0]->Data, hoursAngle, &matrices[1]);
        mtx4MultInPlace(matrices, &matrices[1]);
        mtx4LoadYRotationAndTranslation((coord3d *) objheader->Switches[1]->Data, minutesAngle, &matrices[2]);
        mtx4MultInPlace(matrices, &matrices[2]);
        mtx4LoadYRotationAndTranslation((coord3d *) objheader->Switches[2]->Data, secondsAngle, &matrices[3]);
        mtx4MultInPlace(matrices, &matrices[3]);
        
        renderdata.flags = 3;
        renderdata.zbufferenabled = 0;
        renderdata.gdl = gdl;
        renderdata.PropType = PROP_TYPE_WEAPON;
    
        if ((g_CurrentPlayer->watch_animation_state == 5) || (g_CurrentPlayer->watch_animation_state == 12))
        {
            renderdata.envcolour.word = 0xcd;
        }
        else
        {
            renderdata.envcolour.word = g_CurrentPlayer->tileColor.a | (((g_CurrentPlayer->tileColor.r << 24) | (g_CurrentPlayer->tileColor.g << 16)) | (g_CurrentPlayer->tileColor.b << 8));
        }
    
        modelRender(&renderdata, &g_CurrentPlayer->watchModel);
        gdl = renderdata.gdl;
        nodepos2 = (f32 *) objheader->Switches[2]->Data;
        finalmtx = dynAllocateMatrix();
        mtx4LoadTranslation((coord3d *) nodepos2, &handmtx);
        mtx4MultInPlace(matrices, &handmtx);
        mtxPushUnitWorldScale();
        mtxF2L(&handmtx, (Mtxf *)finalmtx);
        mtxPopWorldScale();
        gdl = watchMenuRender(gdl, finalmtx, (g_CurrentPlayer->watch_animation_state == 5) || (g_CurrentPlayer->watch_animation_state == 12));
        mtxPushUnitWorldScale();
        mtxsF2L(g_CurrentPlayer->watchModel.render_pos, objheader->numMatrices);
        mtxPopWorldScale();
    }
 
    end:
    return gdl;
}


/**
 * Co-op: TRUE if another player has their watch out. Only one player can
 * pause at a time.
 */
s32 coopPartnerHasWatchOut(void)
{
    s32 count = getPlayerCount();
    s32 i;

    if (!gameIsTwoPlayerMission())
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
