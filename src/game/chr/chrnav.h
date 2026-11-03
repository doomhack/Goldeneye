#ifndef _CHRNAV_H_
#define _CHRNAV_H_
#include <ultra64.h>
#include <bondtypes.h>

/* Starting a move */
s32  chrGoToPos(ChrRecord *self, const coord3d *pos, StandTile *stan, SPEED speed);
bool chrGoToPad(ChrRecord *self, s32 padid, SPEED speed);
void chrRunToPos(ChrRecord *self, coord3d *pos);
void chrStartPatrol(ChrRecord *self, PathRecord *path);
bool chrHasStoppedOrPatroling(const ChrRecord *self);

/* Action ticks, from chrActionTick */
void chrTickGoPos(ChrRecord *self);
void chrTickPatrol(ChrRecord *self);
void chrTickRunPos(ChrRecord *self);

/* Turning to face a point while moving */
s32  chrApplySpeed(const ChrRecord *self, const coord3d *target, s32 speed, f32 *speedPtr);
f32  chrGetRunAnimSpeed(const ChrRecord *self);
void chrGetMovementTarget(const ChrRecord *self, coord3d *target);

/* Can a guard walk there? */
s32  propCanWalkLine(const PropRecord *prop, const coord3d *dest, const coord3d *dir);
s32  propCanWalkInDir(const PropRecord *prop, const coord3d *dir, f32 dist);
s32  chrCanWalkViaPoint(ChrRecord *self, const coord3d *mypos, StandTile *mystan, const coord3d *viapoint, const coord3d *bondpos, const StandTile *bondstan, s32 cdtypes);
waypoint *waypointFindNearPos(const coord3d *pos, StandTile *starttile);

/* Arrival tests, also used by vehicles */
s32  rayPassesNearPoint2d(const coord3d *origin, const coord3d *dir, const coord3d *point, f32 range);
s32  chrIsArrivingLaterallyAtPos(const coord3d *prevpos, const coord3d *curpos, const coord3d *targetpos, f32 range);

#endif
