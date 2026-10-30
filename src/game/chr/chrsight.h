#ifndef _CHRSIGHT_H_
#define _CHRSIGHT_H_
#include <ultra64.h>
#include <bondtypes.h>

bool chrSawTargetRecently(ChrRecord *self);
bool chrHeardTargetRecently(ChrRecord *self);
bool chrCheckCanSeeTarget(ChrRecord *self, coord3d *pos, StandTile *tile);
bool chrCanSeeBond(ChrRecord *self);
bool chrHasLineOfSightToPos(ChrRecord *self, coord3d *pos, StandTile *stan);
bool chrCanSeePos(ChrRecord *self, coord3d *pos, StandTile *tile);
bool chrHasLineOfSightToPlayer(ChrRecord *self);
void chrSetTargetToPlayer(ChrRecord *self);
void chrAlertGuardToPlayerPosition(ChrRecord *self);
bool chrCheckTargetInSight(ChrRecord *self);
void chrCheckGuardsHeardSound(f32 noise);

/* Co-op */
void coopChrShotByCurrentPlayer(ChrRecord *self);
void coopChrSelectTarget(ChrRecord *self);

#endif
