#ifndef _BONDINV_H_
#define _BONDINV_H_
#include <ultra64.h>
#include <bondconstants.h>
#include "game/player/bondview.h"
#include "bondtypes.h"

void invReset(void);
s32 invHasFlagToken(void);
s32 invCountItems(void);

InvItem *invGetItemByIndex(s32 index);
textoverride *invGetOverrideForObj(ObjectRecord *obj);
textoverride *invGetOverrideForWeapon(ITEM_IDS weaponnum);

void invCycleBackward(s32 *nextright, s32 *nextleft, s32 requireammo);
void invCycleForward(s32 *nextright, s32 *nextleft, s32 requireammo);
bool invHasGoldenGun(void);
int invAddItem(ITEM_IDS item);
int invAddDualItem(ITEM_IDS right, ITEM_IDS left);
s32 invGetAllGunsFlag(void);
void invSetAllGunsFlag(s32 all_guns);
bool          invHasProp(PropRecord *prop);
bool          invAnyPlayerHasProp(PropRecord *prop);
void          invResetKeepProps(void);
WeaponObjRecord *invRemoveWeaponProp(ITEM_IDS weaponnum);
void invRemoveItemByID(ITEM_IDS weaponnum);
s32 invGetWeaponOfChoice(s32 *weapon1, s32 *weapon2);
s32 invItemAvailableForHand(ITEM_IDS right, ITEM_IDS left);
s32 invItemAvailable(ITEM_IDS weaponid);
void invAddTextOverride(textoverride *override);

void invResetHeldTimes(void);
void invAllocate(s32 additionalentries);

#endif
