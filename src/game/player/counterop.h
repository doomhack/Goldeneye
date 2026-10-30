#ifndef _COUNTEROP_H_
#define _COUNTEROP_H_
#include <ultra64.h>

void counterOpResetForStage(void);
s32  counterOpTakeOverGuard(void);
void counterOpTick(void);
s32  counterOpSuicidePillTick(s32 triggerOn);
f32  counterOpGetGuardHealth(void);
s32  counterOpGetGuardModels(s32 *head, s32 *body);

// End screen stats
s32  counterOpGetBodiesUsed(void);
f32  counterOpGetDamageToBond(void);
void counterOpNoteDamageToBond(f32 percent);

// In chrdamage.c: damage a guard does to Bond in counter-op
f32  counterOpPlayerDamage(s32 shooter, f32 damage, s32 weaponid);

#endif
