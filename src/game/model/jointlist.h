#ifndef _JOINTLIST_H_
#define _JOINTLIST_H_
#include <ultra64.h>
#include <bondtypes.h>

extern JointListEntry *g_JointListFree;

void jointListInit(void);
JointListEntry *jointListAddModel(JointListEntry *head, Model *model);
void jointListFree(JointListEntry *entry);
void jointListCalcDepths(JointListEntry *list);
JointListEntry *jointListSortByDepth(JointListEntry *list);
void jointListDraw(ModelRenderData *renderdata, JointListEntry *entry);
s32 jointListFindHitNext(JointListEntry **entryptr, coord3d *raypos, coord3d *raydir, Model **outModel, ModelNode **inoutNode);
s32 jointListFindHit(JointListEntry **entryptr, coord3d *raypos, coord3d *raydir, Model **outModel, ModelNode **outNode);

#endif
