#ifndef _PLAYER_WATCH_H_
#define _PLAYER_WATCH_H_
#include <ultra64.h>
#include <bondtypes.h>

void watchInitPauseTilt(void);
f32 watchSetupPauseTilt(bool topause);
void watchStartPauseTilt(f32 duration);
void watchStartUnpauseTilt(f32 duration);
bool watchIsPauseTilting(void);
void watchUpdatePauseTilt(void);
void watchStartZoom(f32 final, f32 time);
f32 watchGetZoomTargetFovy(void);
void watchZoomTo(f32 zoominfovy);
void watchZoomToDefault(void);
void watchZoomOnOpen(void);
void watchZoomOnExit(void);
s32 watchIsZooming(void);
void watchUpdateZoom(void);
f32 watchGetOpenFrac(void);
void watchInitHandModel(void);
void watchStartRaise(f32 speed);
void watchStartLower(f32 speed);
void watchStepAnim(void);
f32 watchGetAnimFrac(void);
void playerSetOutsideWatchMenu(s32 outside);
void pauseStopSounds(void);
void watchTick(void);
void watchRequestToggle(void);
void watchTrigger(s32 instant);
Gfx *watchRender(Gfx *gdl);

// Co-op: TRUE if another player has their watch out
s32 coopPartnerHasWatchOut(void);

extern f32 watch_transition_time;

#endif
