#ifndef _CHRSCRIPT_H_
#define _CHRSCRIPT_H_
#include <ultra64.h>
#include <bondtypes.h>

/* Looking things up by the ids AI scripts use */
s32        chrResolveId(ChrRecord *self, s32 id);
s32        chrResolvePadId(ChrRecord *self, s32 padnum);
ChrRecord *chrFindById(ChrRecord *self, s32 id);
coord3d   *chrGetChrOrPresetLocation(ChrRecord *self, s32 attacktype, s32 id, StandTile **stan);

/* Distances and angles */
f32 chrGetAngleToPos(ChrRecord *self, coord3d *pos);
f32 chrGetAngleToBond(ChrRecord *self);
f32 chrGetAngleFromBond(ChrRecord *self);
f32 chrGetDistanceToBond(ChrRecord *self);
f32 chrGetDistanceToPad(ChrRecord *self, s32 padnum);
f32 chrGetDistanceToChr(ChrRecord *self, s32 chrnum);
f32 chrGetDistanceFromBondToPad(ChrRecord *self, s32 padnum);

/* What the chr knows */
bool chrSawTargetRecently(ChrRecord *self);
bool chrHeardTargetRecently(ChrRecord *self);
bool chrIsTargetedByPlayer(ChrRecord *self);
bool chrIsTargetNearlyInSight(ChrRecord *self);
bool chrPadRoomIsVisible(ChrRecord *self, s32 padnum);
bool chrIfInPadRoom(ChrRecord *self, s32 chrnum, s32 padnum);
bool chrPlayerIsInPadRoom(ChrRecord *self, s32 padnum);

/* Flags and the timer */
void chrSetFlags2ById(ChrRecord *self, s32 chrnum, u8 flags);
void chrUnsetFlags2ById(ChrRecord *self, s32 chrnum, u8 flags);
bool chrHasFlags2ById(ChrRecord *self, s32 chrnum, u8 flags);
void chrSetStageFlags(u32 flags);
void chrUnsetStageFlags(u32 flags);
bool chrHasStageFlag(u32 flags);
void chrRestartTimer(ChrRecord *self);
f32  chrGetTimer(ChrRecord *self);

/* Presets */
void chrSetChrPreset(ChrRecord *self, s32 chrnum);
void chrSetChrPresetOf(ChrRecord *self, s32 chrnum, s32 presetchrnum);
bool chrSetChrPresetToNearbyChr(ChrRecord *self, f32 distance);
void chrSetPadPreset(ChrRecord *self, s32 padnum);
void chrSetPadPresetByChrnum(ChrRecord *self, s32 chrnum, s32 padnum);
bool chrSetPadPresetToWaypointInTargetQuadrant(ChrRecord *self, u8 quadrant);
bool chrSetPadPresetToWaypointInQuadrant(ChrRecord *self, u8 quadrant);

/* Spawning */
bool        chrAdjustPosForSpawn(coord3d *pos, StandTile **stan, f32 facing, bool allowonscreen);
PropRecord *chrSpawnAtCoord(s32 bodynum, s32 headnum, coord3d *pos, StandTile *stan, f32 angle, AIRecord *ailist, s32 spawnflags);
PropRecord *chrSpawnAtPad(ChrRecord *self, s32 bodynum, s32 headnum, s32 padnum, AIRecord *ailist, s32 spawnflags);
PropRecord *chrSpawnAtChr(ChrRecord *self, s32 bodynum, s32 headnum, s32 chrnum, AIRecord *ailist, s32 spawnflags);

/* Actions the scripts ask for: each fails if the chr is dead or reacting to a shot */
bool chrTryStopAnim(ChrRecord *self);
bool chrTryKneel(ChrRecord *self);
bool chrTryStartAnim(ChrRecord *self, s32 animnum, s32 startframe, s32 endframe, u8 flags, s32 mergetime);
bool chrTrySurprisedOneHand(ChrRecord *self);
bool chrTrySurprisedSurrender(ChrRecord *self);
bool chrTrySurprisedLookAround(ChrRecord *self);
bool chrTrySurrender(ChrRecord *self);
bool chrGoToBond(ChrRecord *self, SPEED speed);
bool chrGoToChr(ChrRecord *self, s32 chrnum, SPEED speed);
bool chrTryStartAlarm(ChrRecord *self, s32 padnum);
bool chrTryThrowGrenade(ChrRecord *self);
bool chrDropItem(ChrRecord *self, s32 modelnum, u8 weaponnum);

#endif
