#ifndef _CARRIED_H_
#define _CARRIED_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/model/jointlist.h"

void             hatAssignToChr(HatRecord *hat, ChrRecord *chr);
PropRecord      *hatCreateForChr(ChrRecord *chr, s32 modelnum, u32 flags);
HATTYPE          hatGetModel(PropRecord *prop);

void             weaponSetGunfireVisible(PropRecord *prop, s32 firing);
PropRecord      *weaponInitWithModelDef(ObjectRecord *obj, ModelFileHeader *modeldef);
void             weaponSetDual(WeaponObjRecord *leftweapon, WeaponObjRecord *rightweapon);
bool             chrEquipWeapon(WeaponObjRecord *weapon, ChrRecord *chr);
void             chrSetupHeldWeapon(WeaponObjRecord *weapon, ChrRecord *chr);
WeaponObjRecord *weaponCreateFromModel(PROP modelnum, s32 weaponid);
PropRecord      *chrGiveWeaponEx(ChrRecord *chr, s32 modelnum, ITEM_IDS weaponid, s32 flags, WeaponObjRecord *weapon, ModelFileHeader *modeldef);
PropRecord      *chrGiveWeapon(ChrRecord *chr, s32 modelnum, ITEM_IDS weaponid, s32 flags);
void             chrRemoveHeldWeapon(ChrRecord *chr, GUNHAND hand);
void             chrRenderHeldWeapon(PropRecord *chrprop, GUNHAND hand, JointListEntry **jointList);
WeaponObjRecord *weaponFindThrown(s32 weaponnum);

void             objSetDropType(PropRecord *prop, DROPTYPE droptype);
void             objDetach(PropRecord *prop);
bool             objDrop(PropRecord *prop);
void             playerDropInventory(void);

#endif
