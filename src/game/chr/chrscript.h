#ifndef _CHRSCRIPT_H_
#define _CHRSCRIPT_H_
#include <ultra64.h>
#include <bondtypes.h>

/* Looking things up by the ids AI scripts use */
s32        chrResolveId(const ChrRecord *self, s32 id);
s32        chrResolvePadId(const ChrRecord *self, s32 padnum);
ChrRecord *chrFindById(const ChrRecord *self, s32 id);
coord3d   *chrGetChrOrPresetLocation(ChrRecord *self, s32 attacktype, s32 id, StandTile **stan);

/* Distances and angles */
f32 chrGetAngleToPos(const ChrRecord *self, const coord3d *pos);
f32 chrGetAngleToBond(const ChrRecord *self);
f32 chrGetAngleFromBond(const ChrRecord *self);
f32 chrGetDistanceToBond(const ChrRecord *self);
f32 chrGetDistanceToPad(const ChrRecord *self, s32 padnum);
f32 chrGetDistanceToChr(const ChrRecord *self, s32 chrnum);
f32 chrGetDistanceFromBondToPad(const ChrRecord *self, s32 padnum);

/* What the chr knows */
bool chrIsTargetedByPlayer(const ChrRecord *self);
bool chrIsTargetNearlyInSight(ChrRecord *self);
bool chrPadRoomIsVisible(const ChrRecord *self, s32 padnum);
bool chrIfInPadRoom(const ChrRecord *self, s32 chrnum, s32 padnum);
bool chrPlayerIsInPadRoom(const ChrRecord *self, s32 padnum);

/* Flags and the timer */
void chrSetFlags2ById(const ChrRecord *self, s32 chrnum, u8 flags);
void chrUnsetFlags2ById(const ChrRecord *self, s32 chrnum, u8 flags);
bool chrHasFlags2ById(const ChrRecord *self, s32 chrnum, u8 flags);
void chrSetStageFlags(u32 flags);
void chrUnsetStageFlags(u32 flags);
bool chrHasStageFlag(u32 flags);
void chrRestartTimer(ChrRecord *self);
f32  chrGetTimer(const ChrRecord *self);

/* Presets */
void chrSetChrPreset(ChrRecord *self, s32 chrnum);
void chrSetChrPresetOf(const ChrRecord *self, s32 chrnum, s32 presetchrnum);
bool chrSetChrPresetToNearbyChr(ChrRecord *self, f32 distance);
void chrSetPadPreset(ChrRecord *self, s32 padnum);
void chrSetPadPresetByChrnum(const ChrRecord *self, s32 chrnum, s32 padnum);
bool chrSetPadPresetToWaypointInTargetQuadrant(ChrRecord *self, u8 quadrant);
bool chrSetPadPresetToWaypointInQuadrant(ChrRecord *self, u8 quadrant);

/* Spawning */
bool        chrAdjustPosForSpawn(coord3d *pos, StandTile **stan, f32 facing, bool allowonscreen);
PropRecord *chrSpawnAtCoord(s32 bodynum, s32 headnum, const coord3d *pos, StandTile *stan, f32 angle, AIRecord *ailist, s32 spawnflags);
PropRecord *chrSpawnAtPad(const ChrRecord *self, s32 bodynum, s32 headnum, s32 padnum, AIRecord *ailist, s32 spawnflags);
PropRecord *chrSpawnAtChr(const ChrRecord *self, s32 bodynum, s32 headnum, s32 chrnum, AIRecord *ailist, s32 spawnflags);

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
