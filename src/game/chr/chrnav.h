#ifndef _CHRNAV_H_
#define _CHRNAV_H_
#include <ultra64.h>
#include <bondtypes.h>

/* Starting a move */
s32  chrGoToPos(ChrRecord *self, coord3d *pos, StandTile *stan, SPEED speed);
bool chrGoToPad(ChrRecord *self, s32 padid, SPEED speed);
void chrRunToPos(ChrRecord *self, coord3d *pos);
void chrStartPatrol(ChrRecord *self, PathRecord *path);
bool chrHasStoppedOrPatroling(ChrRecord *self);

/* Action ticks, from chrActionTick */
void chrTickGoPos(ChrRecord *self);
void chrTickPatrol(ChrRecord *self);
void chrTickRunPos(ChrRecord *self);

/* Turning to face a point while moving */
s32  chrApplySpeed(ChrRecord *self, coord3d *target, s32 speed, f32 *speedPtr);
f32  chrGetRunAnimSpeed(ChrRecord *self);
void chrGetMovementTarget(ChrRecord *self, coord3d *target);

/* Can a guard walk there? */
s32  propCanWalkLine(PropRecord *prop, coord3d *dest, coord3d *dir);
s32  propCanWalkInDir(PropRecord *prop, coord3d *dir, f32 dist);
s32  chrCanWalkViaPoint(ChrRecord *self, coord3d *mypos, StandTile *mystan, coord3d *viapoint, coord3d *bondpos, StandTile *bondstan, s32 cdtypes);
waypoint *waypointFindNearPos(coord3d *pos, StandTile *starttile);

/* Arrival tests, also used by vehicles */
s32  rayPassesNearPoint2d(coord3d *origin, coord3d *dir, coord3d *point, f32 range);
s32  chrIsArrivingLaterallyAtPos(coord3d *prevpos, coord3d *curpos, coord3d *targetpos, f32 range);

#endif
