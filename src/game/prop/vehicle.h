#ifndef _VEHICLE_H_
#define _VEHICLE_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

bool vehicleAlignToFloor(VehichleRecord *vehicle);
void vehicleTick(PropRecord *prop);
void vehicleGetModelMatrices(PropRecord *prop, Model *model, Mtxf *mtxs, bool issimowner);
void aircraftTick(PropRecord *prop);
void aircraftGetModelMatrices(PropRecord *prop, Model *model, Mtxf *mtxs, bool issimowner);
void tankGetModelMatrices(PropRecord *prop, Model *model, Mtxf *mtxs);

#endif
