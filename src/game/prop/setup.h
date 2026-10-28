#ifndef _SETUP_H_
#define _SETUP_H_
#include <ultra64.h>

extern const u32 only_read_by_stageload[];

#include <bondtypes.h>

void setupCreateProps(enum LEVELID stageId);
void setupFreeProps(void);
void setupLinkSwitch(struct LinkRecord *link);
void setupLinkLockDoor(struct LockDoorRecord *link);
void setupLinkSafeItem(SafeObjectRecord *link);

s32                    getposstan(coord3d *pos, StandTile *stan, f32 radius, coord3d *posReturn, StandTile **stanReturn);
s32                    modelLoad(s32 modelid);
s32                    sizepropdef(struct PropDefHeaderRecord *pdef);
void                   setupUpdateObjectRoomPosition(struct ObjectRecord *);
struct PropDefHeaderRecord *setupGetPtrToCommandByIndex(s32 index);
struct ObjectRecord    *setupCommandGetObject(s32 index);
s32                    setupGetCommandIndexByProp(struct PropRecord *prop);
s32                    tagGetCommandIndex(struct ObjectRecord *tag);

void propsResetLevelState(void);
void objSoundsInit(void);
void objSoundsStopAll(void);

void setupGuard(GuardRecord *guard);

ObjectRecord *setupFindObjForReuse(s32 wanttype, ObjectRecord **offscreenobjptr, ObjectRecord **anyobjptr, bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef);

#endif
