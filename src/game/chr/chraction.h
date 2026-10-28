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






f32 chrGetDistanceToBond(ChrRecord *guardData);
bool chrCheckTargetInSight(ChrRecord *self);
bool chrTryModifyAttack(ChrRecord *self, s32 newtargettype, s32 newtargetid);
void chrAlertGuardToPlayerPosition(ChrRecord *);
void chrStandImmediate(ChrRecord *self, f32 mergetime);
f32 chrGetAngleToBond(ChrRecord *self);
bool chrHasStoppedOrPatroling(ChrRecord *);
s32 chrResolvePadId(ChrRecord *guardData,s32 padNo);
void chrLineLineIntersection(coord3d *line1_p1, coord3d *line1_p2, coord3d *line2_p3, coord3d *dir, coord3d *result);
void chrStanLineDirIntersection(coord3d *line2_p3, coord3d *dir, coord3d *result);
void chrStanPointPointIntersection(coord3d *pos, coord3d *dir, coord3d *result);
bool chrTakeShot(ChrRecord *self, s32 hitpart, coord3d *vector, s32 weaponid, bool isPlayer);
bool chrCanSeeBond(ChrRecord *self);
bool chrHasLineOfSightToPos(ChrRecord *self, coord3d *pos, StandTile *stan);
s32 chrSawTargetRecently(ChrRecord *);
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
bool chrGoToPad(ChrRecord *self, s32 padid, SPEED speed);
bool chrTryStartPatrol(ChrRecord *self, PathRecord *path);
f32 chrGetSubrotySideback(ChrRecord *self);
f32 chrGetAimLimitAngle(f32 sqdist);
void chrUpdateAimendbackShoulders(ChrRecord *self, struct weapon_firing_animation_table *anim, s32 leftgun, s32 rightgun, f32 next);
void chrSetFiring(ChrRecord *self, s32 hand, s32 firing);
s32 chrGetHandPos(ChrRecord *self, GUNHAND hand, coord3d *pos);
void chrTriggerFireWeapon(ChrRecord *self);
s32 rayPassesNearPoint2d(coord3d *origin, coord3d *dir, coord3d *point, f32 range);
s32 chrIsArrivingLaterallyAtPos(coord3d *prevpos, coord3d *curpos, coord3d *targetpos, f32 range);
void chrActionTick(ChrRecord *self);
void bgChrsTick(void);
s32 chrSawTargetRecently(ChrRecord *self);
s32 chrHeardTargetRecently(ChrRecord *self);
ChrRecord *chrFindById(ChrRecord *self, s32);
f32 chrGetAngleFromBond(ChrRecord *self);
f32 chrGetDistanceToPad(ChrRecord *self, s32 padID);
bool chrPadRoomIsVisible(ChrRecord *self, s32 padnum);
f32 chrGetDistanceToChr(ChrRecord *self, s32 chrID);
f32 chrGetDistanceFromBondToPad(ChrRecord *self, s32 padid);
void chrSetFlags2(ChrRecord *self, u8 flags2);
void chrUnsetFlags2(ChrRecord *self, u8 flags2);
s32 chrHasFlags2(ChrRecord *self, u8 flags2);
void chrSetFlags2ById(ChrRecord *self, s32 guard_id, u8 flags2);
void chrUnsetFlags2ById(ChrRecord *self, s32 guard_id, u8 flags2);
bool chrHasFlags2ById(ChrRecord *self, s32 guard_id, u8 flags2); //chrHasFlags2ById
void chrSetStageFlags(s32 flags);
void chrUnsetStageFlags(u32 flags);
bool chrHasStageFlag(s32 flags);
bool chrIsHearingBond(ChrRecord *self);
bool chrTrySurrender(ChrRecord *self);
bool chrFadeOut(ChrRecord *self);
void chrRestartTimer(ChrRecord *self);
f32 chrGetTimer(ChrRecord *self);
bool chrIsTargetedByPlayer(ChrRecord *self);
bool chrIfNearMiss(ChrRecord *self);
bool chrGoToBond(ChrRecord *self, SPEED speed);
bool chrGoToChr(ChrRecord *self, s32 chrid, SPEED speed);
s8 chrGetNumArghs(ChrRecord *self);
s8 chrGetNumCloseArghs(ChrRecord *self);
bool chrSawInjury(ChrRecord *self);
bool chrSawDeath(ChrRecord *self);
bool chraiStopAnimation(ChrRecord *self);
bool chrTrySurprisedOneHand(ChrRecord *self);
bool chrTrySurprisedSurrender(ChrRecord *self);
bool chrTrySurprisedLookAround(ChrRecord *self);
bool chrTryKneel(ChrRecord *self);
s32 chrTryStartAnim(ChrRecord *self, s32 animID, s32 b, s32 c, u8 d, s32 e);
bool chrCanHearAlarm(void);
bool chrSetPadPresetToWaypointInTargetQuadrant(ChrRecord *self, u8 quadrant);
bool chrSetPadPresetToWaypointInQuadrant(ChrRecord *self, u8 padnum);
bool chrSetChrPresetToNearbyChr(ChrRecord *self, f32 distance);
void chrSetChrPreset(ChrRecord *self, s32 id);
void chrSetChrPresetOf(ChrRecord *self, s32 id, s32 id2);
void chrSetPadPreset( ChrRecord *self, s32 padid);
void chrSetPadPresetByChrnum(ChrRecord *self, s32 chrid, s32 padid);
s32 chrIsTargetNearlyInSight(ChrRecord *self);
bool chrAdjustPosForSpawn(coord3d *pos, StandTile **tile, f32 facing, bool b);
PropRecord *chrSpawnAtPad(ChrRecord *self, s32 bodynum, s32 headnum, s32 padid, AIRecord *ailist, s32 flags);
PropRecord *chrSpawnAtChr(ChrRecord *self, s32 bodynum, s32 headnum, s32 chrnum, AIRecord *ailist, s32 flags);
bool chrIfInPadRoom(ChrRecord *self, s32 chrnum, s32 padnum);
bool chrPlayerIsInPadRoom(ChrRecord *self, s32 padnum);
bool chrRemovedAiCommand27(void);
bool chrRemovedAiCommand2B(void);
bool chrTryStartAlarm(ChrRecord *self, s32 PadId);
bool chrTryThrowGrenade(ChrRecord *self);
bool chrDropItem(ChrRecord *self, s32 modelnum, u8 weaponid);
void chrGetMovementTarget(ChrRecord *self, coord3d *target);
s32 chrExplosionDamage(ChrRecord *self, coord3d *pos, f32 damage, s32 playsound);

#endif
