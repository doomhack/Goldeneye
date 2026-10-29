#ifndef _ALARM_H_
#define _ALARM_H_

#include <ultra64.h>
#include <bondtypes.h>

/* Why the countdown timer is hidden: it shows only when none of these is set */
enum CountdownHideReason
{
    COUNTDOWN_HIDE_SCRIPT = 0x1,    /* the level's AI scripts haven't shown it */
    COUNTDOWN_HIDE_PLASTIQUE = 0x2, /* a plastique charge has gone off around Bond */
    COUNTDOWN_HIDE_WATCH = 0x4,     /* the watch menu is open */
    COUNTDOWN_HIDE_DAMAGE = 0x8,    /* the screen is flashing red */
    COUNTDOWN_HIDE_NOCONTROL = 0x10 /* a cutscene has taken control */
};

void alarmGasTimerReset(void);
void alarmGasTimerTick(void);
void alarmGasEndLevel(void);

void alarmActivate(void);
void alarmDeactivate(void);
bool alarmIsActive(void);
void alarmStopSound(void);

void gasStart(const coord3d *source);
bool gasIsActive(void);
void gasStopSound(void);

void countdownTimerSetVisible(enum CountdownHideReason reason, bool visible);
bool countdownTimerIsVisible(void);
void countdownTimerSetValue(f32 ticks);
f32 countdownTimerGetValue(void);
void countdownTimerSetRunning(bool running);
bool countdownTimerIsRunning(void);
Gfx *countdownTimerRender(Gfx *gdl);

#endif
