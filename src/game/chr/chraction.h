#ifndef _CHR2_H_
#define _CHR2_H_
#include <ultra64.h>
#include <bondgame.h>
#include "game/model/chrobjdata.h"

// Value is 3, it just needs to be less than MAX_CHRWAYPOINTS
#define PATH_FINDING_WP_LIMIT (MAX_CHRWAYPOINTS - 3)

#ifdef REFRESH_PAL
#define CHRLV_RECENT_TIME_CHECK 151
#define CHRLV_TICK_DEAD_CHECK 75
#define CHRLV_SEEN_RECENT_CHECK 100
#define CHRLV_LASTMOVEOK60_CHECK 50
#define CHRLV_ATTACKWALK_CLOCK_TIMER30_MIN 0x11
#define CHRLV_ATTACKWALK_CLOCK_TIMER30_MAX 0x1a
#define CHRLV_ATTACKWALK_TIMER40_A 0x4b
#define CHRLV_ATTACKWALK_TIMER40_B 0x10
#define CHRLV_ATTACKWALK_TIMER40_C 0x96
#define CHRLV_ATTACKWALK_TIMER40_D 0x21

#define CHRLV_DEFAULT_TIMER 0x96
#define CHRLV_10_SEC_TIMER 0x1f4 /* 500 */
#define CHRLV_FRAMERATE_F 50.0f

#else

#define CHRLV_RECENT_TIME_CHECK 181
#define CHRLV_TICK_DEAD_CHECK 90
#define CHRLV_SEEN_RECENT_CHECK 120
#define CHRLV_LASTMOVEOK60_CHECK 60
#define CHRLV_ATTACKWALK_CLOCK_TIMER30_MIN 0x15
#define CHRLV_ATTACKWALK_CLOCK_TIMER30_MAX 0x1F
#define CHRLV_ATTACKWALK_TIMER40_A 0x5a
#define CHRLV_ATTACKWALK_TIMER40_B 0x14
#define CHRLV_ATTACKWALK_TIMER40_C 0xb4
#define CHRLV_ATTACKWALK_TIMER40_D 0x28

#define CHRLV_DEFAULT_TIMER 0xb4
#define CHRLV_10_SEC_TIMER 0x258 /* 600 */
#define CHRLV_FRAMERATE_F 60.0f
#endif






bool chrTryModifyAttack(ChrRecord *self, s32 newtargettype, s32 newtargetid);
void chrStandImmediate(ChrRecord *self, f32 mergetime);
void chrLineLineIntersection(coord3d *line1_p1, coord3d *line1_p2, coord3d *line2_p3, coord3d *dir, coord3d *result);
void chrStanLineDirIntersection(coord3d *line2_p3, coord3d *dir, coord3d *result);
void chrStanPointPointIntersection(coord3d *pos, coord3d *dir, coord3d *result);
bool chrIsDead(ChrRecord *chr);
bool chrTrySidestep(ChrRecord *self);
bool chrTryJumpOut(ChrRecord *self);
bool chrTryRunSideways(ChrRecord *self);
bool chrTryAttackWalk(ChrRecord *self);
bool chrTryAttackRun(ChrRecord *self);
bool chrTryAttackRoll(ChrRecord *self);
bool chrTryAttackStand(ChrRecord *self, s32 attack_type, s32 b);
bool chrTryAttackKneel(ChrRecord *self, s32 targettype, s32 targetid);
bool chrTryStop(ChrRecord *self, s32 faceentitytype, s32 faceentityid);
bool chrTryStartPatrol(ChrRecord *self, PathRecord *path);
void chrActionTick(ChrRecord *self);
void bgChrsTick(void);

/* Starting actions, for chrscript.c */
bool chrIsNotDeadOrShot(ChrRecord *self);
s32  chrGetGuard007SpeedRatingInt(ChrRecord *self, s32 scale);
void chrStandCheckWall(ChrRecord *self);
void chrKneelChooseAnimation(ChrRecord *self);
void chrStartAnim(ChrRecord *self, s32 animID, s32 startframe, s32 endframe, u8 bitfield, s32 interpol_time60);
void chrStartAlarmChooseAnimation(ChrRecord *self);
void chrThrowGrenade(ChrRecord *self, GUNHAND hand, s32 startframe);
void chrActorShuffleFeet(ChrRecord *self);
void chrStartSurprised(ChrRecord *self);
void chrActorLookFlustered(ChrRecord *self);
void chrActorThrowWeaponSurrender(ChrRecord *self);
void chrActorFadeAway(ChrRecord *self);

/* Used by chrnav.c, chrattack.c and chrdamage.c */
void chrStandImmediateCheckWall(ChrRecord *self, f32 mergetime);
f32  chrGetClearDistInDirDefault(PropRecord *prop, f32 angle, f32 maxdist);
f32  chrGetGuard007ArghRating(ChrRecord *self, f32 min, f32 max);
f32  chrGetGuard007SpeedRating(ChrRecord *self, f32 min, f32 max);
u32  weaponIsOneHanded(PropRecord *weaponprop);
void chrStandChooseAnimation(ChrRecord *self, f32 duration);

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
