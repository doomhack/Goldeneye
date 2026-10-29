#ifndef _PICKUP_H_
#define _PICKUP_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

bool   objTestForInteract(PropRecord *prop);
TICKOP objInteract(PropRecord *prop);

void   pickupPlayWeaponSound(ITEM_IDS weapon);
void   pickupShowWeaponText(ITEM_IDS weapon);
void   pickupAddAmmo(AMMOTYPE ammotype, s32 amount, bool playsound, bool showtext);

TICKOP propPickupByPlayer(PropRecord *prop, bool showtext);
TICKOP objTickPlayer(PropRecord *prop);
TICKOP weaponTickPlayer(PropRecord *prop);

#endif
