#ifndef _DOOR_H_
#define _DOOR_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

PropRecord *doorInit(DoorRecord *door, coord3d *pos, Mtxf *mtx, StandTile *stan, coord3d *slideDir, coord3d *centre);
void doorTick(PropRecord *prop);
void doorUpdateSoundVolume(DoorRecord *door);
void doorStopSounds(DoorRecord *door);

void doorMove(DoorRecord *door, DOORSTATE state);
void doorToggle(PropRecord *prop);
void doorOpenInstantly(DoorRecord *door);
bool doorIsClosed(DoorRecord *door);
void doorChooseSwingDirection(PropRecord *chrprop, DoorRecord *door);

bool doorTestForInteract(PropRecord *prop);
TICKOP doorInteract(PropRecord *prop);

void doorOpenPortal(DoorRecord *door);
void doorClosePortal(DoorRecord *door);

void doorGetMtx(DoorRecord *door, Mtxf *mtx);
void doorGetModelMatrices(DoorRecord *door, Model *model, Mtxf *mtxs, f32 prevOpenPosition, bool playSounds);
void doorShatterGlass(DoorRecord *door);

#endif
