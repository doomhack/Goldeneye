#ifndef _CHRSIGHT_H_
#define _CHRSIGHT_H_
#include <ultra64.h>
#include <bondtypes.h>

bool chrSawTargetRecently(const ChrRecord *self);
bool chrHeardTargetRecently(const ChrRecord *self);
bool chrCheckCanSeeTarget(ChrRecord *self, const coord3d *pos, StandTile *tile);
bool chrCanSeeBond(ChrRecord *self);
bool chrHasLineOfSightToPos(ChrRecord *self, const coord3d *pos, const StandTile *stan);
bool chrCanSeePos(ChrRecord *self, const coord3d *pos, const StandTile *tile);
bool chrHasLineOfSightToPlayer(const ChrRecord *self);
void chrSetTargetToPlayer(ChrRecord *self);
void chrAlertGuardToPlayerPosition(ChrRecord *self);
bool chrCheckTargetInSight(ChrRecord *self);
void chrCheckGuardsHeardSound(f32 noise);

/* Co-op */
void coopChrShotByCurrentPlayer(ChrRecord *self);
void coopChrSelectTarget(ChrRecord *self);

#endif
