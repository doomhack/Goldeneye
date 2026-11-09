#ifndef _CHR2_H_
#define _CHR2_H_
#include <ultra64.h>
#include <bondgame.h>
#include "game/model/chrobjdata.h"

// Value is 3, it just needs to be less than MAX_CHRWAYPOINTS
#define PATH_FINDING_WP_LIMIT (MAX_CHRWAYPOINTS - 3)

#define CHRLV_RECENT_TIME_CHECK TICKS(181)
#define CHRLV_TICK_DEAD_CHECK TICKS(90)
#define CHRLV_SEEN_RECENT_CHECK TICKS(120)
#define CHRLV_LASTMOVEOK60_CHECK TICKS(60)
#define CHRLV_ATTACKWALK_CLOCK_TIMER30_MIN TICKS_DOWN(0x15)
#define CHRLV_ATTACKWALK_CLOCK_TIMER30_MAX TICKS(0x1F)
#define CHRLV_ATTACKWALK_TIMER40_A TICKS(0x5a)
#define CHRLV_ATTACKWALK_TIMER40_B TICKS_DOWN(0x14)
#define CHRLV_ATTACKWALK_TIMER40_C TICKS(0xb4)
#define CHRLV_ATTACKWALK_TIMER40_D TICKS(0x28)
#define CHRLV_DEFAULT_TIMER TICKS(0xb4)
#define CHRLV_10_SEC_TIMER TICKS(0x258) /* 600 */
#define CHRLV_FRAMERATE_F TICKSF(60.0f)


/* The per-frame tick: runs a chr's AI list and current action */
void chrActionTick(ChrRecord *self);
void bgChrsTick(void);

/* Standing and simple actions */
void chrStandImmediate(ChrRecord *self, f32 mergetime);
void chrStandImmediateCheckWall(ChrRecord *self, f32 mergetime);
void chrStandCheckWall(ChrRecord *self);
void chrStandChooseAnimation(const ChrRecord *self, f32 duration);
void chrKneelChooseAnimation(ChrRecord *self);
void chrStartAnim(ChrRecord *self, s32 animID, s32 startframe, s32 endframe, u8 bitfield, s32 interpol_time60);
void chrStartAlarmChooseAnimation(ChrRecord *self);
void chrThrowGrenade(ChrRecord *self, GUNHAND hand, s32 startframe);
void chrActorShuffleFeet(ChrRecord *self);
void chrStartSurprised(ChrRecord *self);
void chrActorLookFlustered(ChrRecord *self);
void chrActorThrowWeaponSurrender(ChrRecord *self);
void chrActorFadeAway(ChrRecord *self);

/* The actions the AI commands start; each fails if the guard can't do it now */
bool chrIsDead(const ChrRecord *chr);
bool chrIsNotDeadOrShot(const ChrRecord *self);
bool chrTrySidestep(ChrRecord *self);
bool chrTryJumpOut(ChrRecord *self);
bool chrTryRunSideways(ChrRecord *self);
bool chrTryAttackWalk(ChrRecord *self);
bool chrTryAttackRun(ChrRecord *self);
bool chrTryAttackRoll(ChrRecord *self);
bool chrTryAttackStand(ChrRecord *self, s32 attack_type, s32 b);
bool chrTryAttackKneel(ChrRecord *self, s32 targettype, s32 targetid);
bool chrTryModifyAttack(ChrRecord *self, s32 newtargettype, s32 newtargetid);
bool chrTryStop(ChrRecord *self, s32 faceentitytype, s32 faceentityid);
bool chrTryStartPatrol(ChrRecord *self, PathRecord *path);

/* Difficulty scaling of guard speed and reactions */
f32  chrGetGuard007SpeedRating(const ChrRecord *self, f32 min, f32 max);
s32  chrGetGuard007SpeedRatingInt(const ChrRecord *self, s32 scale);
f32  chrGetGuard007ArghRating(const ChrRecord *self, f32 min, f32 max);

/* Helpers */
u32  weaponIsOneHanded(const PropRecord *weaponprop);
f32  chrGetClearDistInDirDefault(const PropRecord *prop, f32 angle, f32 maxdist);
void chrLineLineIntersection(const coord3d *line1_p1, const coord3d *line1_p2, const coord3d *line2_p3, const coord3d *dir, coord3d *result);
void chrStanLineDirIntersection(const coord3d *line2_p3, const coord3d *dir, coord3d *result);
void chrStanPointPointIntersection(const coord3d *pos, const coord3d *dir, coord3d *result);

/* Being hit, hurt and killed */
#include "game/chr/chrdamage.h"

/* Attacking and firing */
#include "game/chr/chrattack.h"

/* How guards move */
#include "game/chr/chrnav.h"

/* What guards see and hear */
#include "game/chr/chrsight.h"

/* What the AI script commands use */
#include "game/chr/chrscript.h"

#endif
