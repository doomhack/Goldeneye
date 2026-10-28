#ifndef _SETUP_H_
#define _SETUP_H_
#include <ultra64.h>

extern const u32 only_read_by_stageload[];

#include <bondtypes.h>

void setupCreateProps(enum LEVELID stageId);
void setupFreeProps(s32 stage);
void setupLinkSwitch(struct LinkRecord *link);
void setupLinkLockDoor(struct LockDoorRecord *link);
void setupLinkSafeItem(struct ObjectRecord *obj);

s32                    getposstan(struct coord3d *pos, StandTile *stan, f32 radius, struct coord3d *posReturn, StandTile **stanReturn);
s32                    modelLoad(s32 modelid);
s32                    sizepropdef(struct PropDefHeaderRecord *pdef);
void                   setupUpdateObjectRoomPosition(struct ObjectRecord *);
struct ObjectRecord    *setupGetPtrToCommandByIndex(s32 index);
struct ObjectRecord    *setupCommandGetObject(s32 stageID, s32 index);
s32                    setupGetCommandIndexByProp(struct PropRecord *prop);
s32                    tagGetCommandIndex(struct ObjectRecord *tag);

void propsResetLevelState(void);
void objSoundsInit(void);
void objSoundsStopAll(void);

void setupGuard(s32 stageid, GuardRecord *guard, s32 cmdindex);

#endif
