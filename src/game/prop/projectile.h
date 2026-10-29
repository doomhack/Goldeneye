#ifndef _PROJECTILE_H_
#define _PROJECTILE_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chr/chrai.h"

extern Projectile g_Projectiles[PROJECTILES_ARR_MAX];
extern Embedment g_Embedments[EMBEDMENT_ARR_MAX];

void projectilesReset(void);

void projectileFree(Projectile *projectile);
Embedment *embedmentAllocate(void);
void embedmentFree(Embedment *embedment);
void objEnsureProjectile(PropRecord *prop);
void objFreeEmbedmentOrProjectile(PropRecord *prop);
void projectileEnableCollide3D(PropRecord *prop);

f32 objGetWidth(ObjectRecord *obj);

void projectileUpdateKnifeSound(ObjectRecord *obj);
TICKOP projectileTick(PropRecord *prop, bool *moved);

#endif
