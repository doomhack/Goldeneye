#ifndef _CHRDAMAGE_H_
#define _CHRDAMAGE_H_
#include <ultra64.h>
#include <bondtypes.h>

bool chrTakeShot(ChrRecord *self, s32 hitpart, coord3d *vector, s32 weaponid, bool isPlayer);
s32  chrExplosionDamage(ChrRecord *self, coord3d *pos, f32 damage, s32 playsound);

/* Action ticks, from chrActionTick */
void chrTickPreArgh(ChrRecord *self);
void chrTickArgh(ChrRecord *self);
void chrTickDie(ChrRecord *self);
void chrTickDead(ChrRecord *self);

#endif
