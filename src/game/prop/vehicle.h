#ifndef _VEHICLE_H_
#define _VEHICLE_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

bool vehicleAlignToFloor(VehichleRecord *vehicle);
void vehicleTick(PropRecord *prop);
void vehicleGetModelMatrices(const PropRecord *prop, const Model *model, Mtxf *mtxs, bool issimowner);
void aircraftTick(PropRecord *prop);
void aircraftGetModelMatrices(const PropRecord *prop, const Model *model, Mtxf *mtxs, bool issimowner);
void tankGetModelMatrices(const PropRecord *prop, const Model *model, Mtxf *mtxs);

#endif
