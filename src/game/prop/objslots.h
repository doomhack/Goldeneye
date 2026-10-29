#ifndef _OBJSLOTS_H_
#define _OBJSLOTS_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

extern s32 g_NextWeaponSlot;
extern s32 g_NextHatSlot;

WeaponObjRecord *weaponCreate(bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef);
HatRecord       *hatCreate(bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef);
AmmoCrateRecord *ammocrateAllocate(void);

#endif
