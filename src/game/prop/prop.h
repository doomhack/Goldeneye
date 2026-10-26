#ifndef _PROP_H_
#define _PROP_H_
#include <ultra64.h>

extern const u32 only_read_by_stageload[];

#include <bondtypes.h>

void setupCreateProps(enum LEVELID stageId);
void setupFreeProps(s32 stage);
void setupLinkSwitch(struct LinkRecord *arg0);
void setupLinkLockDoor(struct LockDoorRecord *arg0);
void setupLinkSafeItem(struct ObjectRecord *arg0);

void propsInit(void);
void propsResetLevelState(void);
void objSoundsInit(void);
void objSoundsStopAll(void);

#endif
