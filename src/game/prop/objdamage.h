#ifndef _OBJDAMAGE_H_
#define _OBJDAMAGE_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

s32 objGetShotsTaken(const ObjectRecord *obj);
s32 objGetDestroyedLevel(const ObjectRecord *obj);
bool objIsHealthy(const ObjectRecord *obj);
bool objIsCollectable(const PropDefHeaderRecord *obj);
bool objIsMortal(ObjectRecord *obj);

void objApplyDamage(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS itemnum, s32 playernum);
void objApplyShotDamage(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS item, s32 owner);
void objDetonateRecursive(const PropRecord *prop, f32 damage, coord3d *pos, s32 weaponnum, s32 owner);
ModelNode *objFindDlCollisionNode(const ObjectRecord *obj);
bool objNodeCanDeform(const ModelFileHeader *modeldef, const ModelNode *node);
void objDeform(ObjectRecord *obj, s32 stage);
s32 propExplode(PropRecord *prop, s32 explosionType);

#endif
