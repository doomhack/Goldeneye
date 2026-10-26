#ifndef _INITOBJECTS_H_
#define _INITOBJECTS_H_
#include <ultra64.h>

#include <bondtypes.h>

void propsInit(void);
void propsResetLevelState(void);
void objSoundsInit(void);
void sub_GAME_7F001910(struct ObjectRecord *arg0);
void setupLinkSwitch(struct LinkRecord *arg0);
void setupLinkLockDoor(struct LockDoorRecord *arg0);
void setupLinkSafeItem(struct ObjectRecord *arg0);

#endif
