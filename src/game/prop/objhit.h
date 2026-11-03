#ifndef _OBJHIT_H_
#define _OBJHIT_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

bool modelFindTriangleHit(const Model *model, ModelNode *startNode, const coord3d *rayPos, const coord3d *rayDir, HitThing *hitthing, s32 *dstmtxindex, ModelNode **dstnode);
void objTestShot(PropRecord *prop, ShotData *shot);
void objHit(const ShotData *shot, const BulletHit *hit);

#endif
