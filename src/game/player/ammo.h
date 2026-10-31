#ifndef _AMMO_H_
#define _AMMO_H_
#include <ultra64.h>
#include <bondtypes.h>

extern AmmoStats g_AmmoStats[];

// Ammo: how much each player holds, giving it, and each weapon's ammo type

void ammoGive(s32 ammo_type, s32 ammo_amount);
s32 ammoGetHeld(AMMOTYPE ammotype);
s32 ammoGetTotal(AMMOTYPE ammotype);
s32 ammoGetMax(s32 ammotype);
void ammoGiveMaxAll(void);
s32 gunGetMagazineAmmo(GUNHAND hand);
s32 gunGetHeldAmmo(enum GUNHAND hand);
s32 weaponGetAmmoType(ITEM_IDS weapon);
s32 weaponGetHeldAmmo(ITEM_IDS weapon);
void weaponGiveAmmo(ITEM_IDS weapon, s32 ammo);
s32 weaponGetMaxAmmo(enum ITEM_IDS weapon);

#endif
