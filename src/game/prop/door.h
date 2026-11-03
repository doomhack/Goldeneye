#ifndef _DOOR_H_
#define _DOOR_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

PropRecord *doorInit(DoorRecord *door, const coord3d *pos, const Mtxf *mtx, StandTile *stan, coord3d *slideDir, const coord3d *centre);
void doorTick(const PropRecord *prop);
void doorUpdateSoundVolume(const DoorRecord *door);
void doorStopSounds(const DoorRecord *door);

void doorMove(DoorRecord *door, DOORSTATE state);
void doorToggle(const PropRecord *prop);
void doorOpenInstantly(DoorRecord *door);
bool doorIsClosed(const DoorRecord *door);
void doorChooseSwingDirection(const PropRecord *chrprop, DoorRecord *door);

bool doorTestForInteract(const PropRecord *prop);
TICKOP doorInteract(const PropRecord *prop);

void doorOpenPortal(const DoorRecord *door);
void doorClosePortal(const DoorRecord *door);

void doorGetMtx(const DoorRecord *door, Mtxf *mtx);
void doorGetModelMatrices(const DoorRecord *door, const Model *model, Mtxf *mtxs, f32 prevOpenPosition, bool playSounds);
void doorShatterGlass(const DoorRecord *door);

#endif
