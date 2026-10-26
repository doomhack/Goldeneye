#ifndef _VTXSTORE_H_
#define _VTXSTORE_H_
#include <ultra64.h>

void vtxstoreReset(void);
void vtxstoreTick(void);
s32 vtxstoreAllocate(s32 arg0, s32 type, s32 arg2, s32 arg3);
void vtxstoreFree(Vertex* arg0);

#endif
