#ifndef _OBJHIT_H_
#define _OBJHIT_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

bool modelFindTriangleHit(Model *model, ModelNode *startNode, coord3d *rayPos, coord3d *rayDir, HitThing *hitthing, s32 *dstmtxindex, ModelNode **dstnode);
void objTestShot(PropRecord *prop, ShotData *shot);
void objHit(ShotData *shot, BulletHit *hit);

#endif
