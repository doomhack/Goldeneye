/**
 * Level-wide events the AI scripts start: the alarm, the gas leak and the countdown
 * timer. There is one of each per level. They are ticked once a frame (alarmGasTimerTick,
 * from propsTickOnce) and reset at level load (alarmGasTimerReset).
 *
 * - The alarm sounds for a fixed time after it is set off. Guards hear it through the AI
 *   scripts (alarmIsActive).
 * - The gas leak thickens the fog over 60 seconds (2 in Egypt, where it is not harmful).
 *   Harmful gas makes Bond cough after 10 seconds and hurts him after 30.
 * - The countdown timer counts down in ticks, and is drawn at the bottom of the screen.
 */

#include <ultra64.h>
#include <bondconstants.h>
#include <boss.h>
#include <music.h>
#include <snd.h>
#include "game/audio/sfxvolume.h"
#include "game/prop/alarm.h"
#include "game/prop/propobj.h"
#include "game/level/lv.h"
#include "game/math/round.h"
#include "game/player/player.h"
#include "game/player/gun.h"
#include "game/text/text.h"
#include "game/world/bgfog.h"
#include "fr.h"
#include "game/mission/mission.h"

#if defined(VERSION_EU)
#define ALARM_DURATION_TICKS 1500
#define GAS_COUGH_INTERVAL_TICKS 187
#else
#define ALARM_DURATION_TICKS 1800
#define GAS_COUGH_INTERVAL_TICKS 225
#endif

/* Gas times, in frames (g_GlobalTimerDelta) */
#define GAS_FULL_TIME 3600.0f
#define GAS_FULL_TIME_EGYPT 120.0f
#define GAS_COUGH_TIME 600.0f
#define GAS_HARM_TIME 1800.0f

static s32 s_AlarmTicks;
static ALSoundState *s_AlarmSound;

static f32 s_GasLevel;      /* frames since the leak started, up to s_GasFullTime */
static bool s_GasRising;
static coord3d s_GasSource;
static s32 s_GasLastCough;
static f32 s_GasHissTime;
static ALSoundState *s_GasSound;
static f32 s_GasFullTime;
static bool s_GasHarmful;

static u32 s_CountdownHidden = COUNTDOWN_HIDE_SCRIPT;
static bool s_CountdownRunning;
static f32 s_CountdownTicks;

void alarmGasTimerReset(void)
{
    s_AlarmTicks = 0;
    s_AlarmSound = NULL;
    s_GasLevel = 0.0f;
    s_GasRising = FALSE;
    s_GasSource.x = 0.0f;
    s_GasSource.y = 0.0f;
    s_GasSource.z = 0.0f;
    s_GasLastCough = 0;
    s_GasHissTime = 0.0f;
    s_GasSound = NULL;
    s_CountdownHidden = COUNTDOWN_HIDE_SCRIPT;
    s_CountdownRunning = FALSE;
    s_CountdownTicks = 0.0f;
}

/** Sets the alarm off. It keeps sounding for its full time from the first time. */
void alarmActivate(void)
{
    if (s_AlarmTicks < 1)
    {
        s_AlarmTicks = 1;
    }
}

void alarmStopSound(void)
{
    if (s_AlarmSound != NULL && sndGetPlayingState(s_AlarmSound) != AL_STOPPED)
    {
        sndDeactivate(s_AlarmSound);
    }
}

void alarmDeactivate(void)
{
    s_AlarmTicks = 0;
    alarmStopSound();
}

bool alarmIsActive(void)
{
    return s_AlarmTicks > 0;
}

static void alarmTick(void)
{
    if (alarmIsActive())
    {
        if (s_AlarmSound == NULL && !lvGetControlsLocked())
        {
            sndPlaySfx(g_musicSfxBufferPtr, ALARM3_SFX, &s_AlarmSound);
        }

        s_AlarmTicks += g_ClockTimer;

        if (s_AlarmTicks > ALARM_DURATION_TICKS)
        {
            alarmDeactivate();
        }
    }
}

void gasStart(const coord3d *source)
{
    s_GasRising = TRUE;
    s_GasHissTime = 0.0f;
    s_GasSource = *source;

    if (bossGetStageNum() == LEVELID_EGYPT)
    {
        s_GasFullTime = GAS_FULL_TIME_EGYPT;
        s_GasHarmful = FALSE;
    }
    else
    {
        s_GasFullTime = GAS_FULL_TIME;
        s_GasHarmful = TRUE;
    }
}

void gasStopSound(void)
{
    if (s_GasSound != NULL && sndGetPlayingState(s_GasSound) != AL_STOPPED)
    {
        sndDeactivate(s_GasSound);
    }
}

bool gasIsActive(void)
{
    return s_GasLevel > 0;
}

static void gasTick(void)
{
    if (s_GasRising)
    {
        s_GasLevel += g_GlobalTimerDelta;

        if (s_GasFullTime <= s_GasLevel)
        {
            s_GasLevel = s_GasFullTime;
            s_GasRising = FALSE;
        }
    }

    if (s_GasLevel <= 0.0f || g_Mission.playerInvincible)
    {
        return;
    }

    fogSwitchToSolosky2(s_GasLevel / s_GasFullTime);

    if (!s_GasHarmful)
    {
        return;
    }

    if (s_GasLastCough < g_GlobalTimer - GAS_COUGH_INTERVAL_TICKS)
    {
        s_GasLastCough = g_GlobalTimer;

        if (s_GasLevel >= GAS_COUGH_TIME)
        {
            sndPlaySfx(g_musicSfxBufferPtr, COUGH_SFX, NULL);
        }

        if (s_GasLevel >= GAS_HARM_TIME)
        {
            playerTakeDamage(0.125f, 0.0f, 0.0f, -1, 0);
        }
    }

    /* The leak hisses while the gas is rising */
    if (s_GasHissTime < s_GasFullTime)
    {
        s_GasHissTime += g_GlobalTimerDelta;

        if (s_GasSound == NULL && !lvGetControlsLocked())
        {
            sndPlaySfx(g_musicSfxBufferPtr, GAS_HISS_SFX, &s_GasSound);
        }

        if (s_GasSound != NULL)
        {
            sfxSetVolumeAt(s_GasSound, &s_GasSource);
        }
    }
    else
    {
        gasStopSound();
    }
}

void countdownTimerSetVisible(enum CountdownHideReason reason, bool visible)
{
    if (visible)
    {
        s_CountdownHidden &= ~reason;
    }
    else
    {
        s_CountdownHidden |= reason;
    }
}

bool countdownTimerIsVisible(void)
{
    return s_CountdownHidden == 0;
}

void countdownTimerSetValue(f32 ticks)
{
    s_CountdownTicks = ticks;
}

f32 countdownTimerGetValue(void)
{
    return s_CountdownTicks;
}

void countdownTimerSetRunning(bool running)
{
    s_CountdownRunning = running;
}

bool countdownTimerIsRunning(void)
{
    return s_CountdownRunning;
}

static void countdownTimerTick(void)
{
    if (s_CountdownRunning)
    {
        s_CountdownTicks -= g_GlobalTimerDelta;
    }
}

/** Draws the timer as "MM:SS:HH" (minutes, seconds, hundredths) near the bottom of the view */
Gfx *countdownTimerRender(Gfx *gdl)
{
    static char colon[] = ":\n";
    f32 time;
    s32 mins;
    s32 secs;
    s32 hundredths;
    s32 y;

    if (!countdownTimerIsVisible())
    {
        return gdl;
    }

    time = s_CountdownTicks;

    if (time < 0.0f)
    {
        time = -time;
    }

    mins = (s32)mathFloor(time / 3600.0f);
    secs = (s32)mathFloor(time / 60.0f) - mins * 60;
    hundredths = (s32)mathFloor(time * 100.0f / 60.0f) - mins * 6000 - secs * 100;

#if defined(VERSION_US)
    y = viGetViewTop() + viGetViewHeight() - 18;
#else
    y = viGetViewTop() + viGetViewHeight() - 28;
#endif

    gdl = textSetupDraw(gdl);
    gdl = gunDrawHudInteger(gdl, mins % 100 / 10, 130, HUDHALIGN_MIDDLE, y, HUDVALIGN_MIDDLE, TRUE);
    gdl = gunDrawHudInteger(gdl, mins % 10, 138, HUDHALIGN_MIDDLE, y, HUDVALIGN_MIDDLE, TRUE);
    gdl = gunDrawHudString(gdl, colon, 147, HUDHALIGN_MIDDLE, y, HUDVALIGN_MIDDLE, TRUE);
    gdl = gunDrawHudInteger(gdl, secs % 60 / 10, 156, HUDHALIGN_MIDDLE, y, HUDVALIGN_MIDDLE, TRUE);
    gdl = gunDrawHudInteger(gdl, secs % 10, 164, HUDHALIGN_MIDDLE, y, HUDVALIGN_MIDDLE, TRUE);
    gdl = gunDrawHudString(gdl, colon, 173, HUDHALIGN_MIDDLE, y, HUDVALIGN_MIDDLE, TRUE);
    gdl = gunDrawHudInteger(gdl, hundredths % 100 / 10, 182, HUDHALIGN_MIDDLE, y, HUDVALIGN_MIDDLE, TRUE);
    gdl = gunDrawHudInteger(gdl, hundredths % 10, 190, HUDHALIGN_MIDDLE, y, HUDVALIGN_MIDDLE, TRUE);

    return textEndDraw(gdl);
}

void alarmGasTimerTick(void)
{
    alarmTick();
    gasTick();
    countdownTimerTick();
}

/** Stops the alarm and the gas sound when the level ends */
void alarmGasEndLevel(void)
{
    alarmDeactivate();
    gasStopSound();
}
