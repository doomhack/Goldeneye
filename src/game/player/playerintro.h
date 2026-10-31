#ifndef _PLAYERINTRO_H_
#define _PLAYERINTRO_H_

#include <ultra64.h>
#include <bondconstants.h>

void playerApplyIntro(void);
u32 weaponLoadProjectileModels(ITEM_IDS modelid);

void coopFindPartnerSpawn(coord3d *pos, StandTile **stan, f32 look_angle);

#endif
