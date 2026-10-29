#ifndef _OBJDAMAGE_H_
#define _OBJDAMAGE_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

s32 objGetShotsTaken(ObjectRecord *obj);
s32 objGetDestroyedLevel(ObjectRecord *obj);
bool objIsHealthy(ObjectRecord *obj);
bool objIsCollectable(PropDefHeaderRecord *obj);
bool objIsMortal(ObjectRecord *obj);

void objApplyDamage(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS itemnum, s32 playernum);
void objApplyShotDamage(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS item, s32 owner);
void objDetonateRecursive(PropRecord *prop, f32 damage, coord3d *pos, s32 weaponnum, s32 owner);
ModelNode *objFindDlCollisionNode(ObjectRecord *obj);
bool objNodeCanDeform(ModelFileHeader *modeldef, ModelNode *node);
void objDeform(ObjectRecord *obj, s32 stage);
s32 propExplode(PropRecord *prop, s32 explosionType);

#endif
