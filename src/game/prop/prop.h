#ifndef _PROP_H_
#define _PROP_H_
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

extern PropRecord g_Props[];
extern PropRecord *g_ActivePropsTail;
extern PropRecord *g_ActivePropsHead;
extern PropRecord *g_FreeProps;
extern s32 g_OnScreenPropCount;
extern PropRecord **g_LastOnScreenProp;
extern PropRecord *g_OnScreenPropList[];

void        propsInit(void);
void        propsSortOnscreen(void);
void        propEnable(PropRecord *prop);
void        propDisable(PropRecord *prop);
PropRecord *propsGetActiveTail(void);
PropRecord *propAllocate(void);
void        propFree(PropRecord *prop);
void        propActivate(PropRecord *prop);
void        propActivateThisFrame(PropRecord *prop);
void        propDelist(PropRecord *prop);
void        propReparent(PropRecord *child, PropRecord *host);
void        propDetach(PropRecord *prop);
Gfx        *propsRenderPass(Gfx *gdl, s32 roomid, s32 renderpass);
void        propExecuteTickOperation(PropRecord *prop, TICKOP op);
PropRecord *propFindForInteract(void);
bool        playerInteract(void);
void        propsTickOnce(void);
void        propsTickPerPlayer(void);
void        propsTickPickups(void);
void        switchActivateLinkedDoors(PropRecord *prop);
bool        doorIsPadlockFree(DoorRecord *door);
bool        objCanPickupFromSafe(ObjectRecord *obj);
ObjectRecord *objFindAtPad(s32 padid);
ObjectRecord *objFindAtPos(coord3d *pos, s32 roomid);

#include "game/prop/proprooms.h"
#include "game/prop/propcollision.h"
#include "game/prop/bbox.h"
#include "game/prop/autoaim.h"

#endif
