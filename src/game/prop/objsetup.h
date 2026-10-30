#ifndef _OBJSETUP_H_
#define _OBJSETUP_H_
#include <ultra64.h>
#include <bondtypes.h>

/* Doors are scaled about their portal by this (a PROPDEF_DOOR_SCALE record sets it) */
extern f32 g_DoorScale;

bool setupCheckPlacement(coord3d *pos, StandTile *stan, f32 radius, coord3d *posout, StandTile **stanout);
void padGetBboxCentre(BoundPadRecord *pad, coord3d *centre);
void setupUpdateObjectRoomPosition(ObjectRecord *obj);

void setupObject(ObjectRecord *obj);
void setupDoor(DoorRecord *door, s32 cmdindex);
void setupWeapon(WeaponObjRecord *weapon);
void setupHat(HatRecord *hat);
void setupCctv(CCTVRecord *cctv);
void setupAutogun(AutogunRecord *autogun);
void setupSingleMonitor(MonitorObjRecord *monitor, s32 cmdindex);
void setupMultiMonitor(MultiMonitorObjRecord *monitor);
void setupArmour(BodyArmourRecord *armour);
void setupTintedGlass(TintedGlassRecord *glass);
void setupAmmoCrate(MultiAmmoCrateRecord *crate, bool skipped);
void setupTank(TankRecord *tank);
void setupVehicle(VehichleRecord *vehicle);
void setupAircraft(AircraftRecord *aircraft);

#endif
