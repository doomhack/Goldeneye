#ifndef _DYN_H_
#define _DYN_H_

#include <ultra64.h>
void dynInitMemory(void);
Gfx *dynGetMasterDisplayList(void);
Vtx *dynAllocateVertices(s32 count);
Mtx *dynAllocateMatrix(void);
Light *dynAllocateLights(s32 count);
void *dynAllocate(s32 size);
void dynSwapBuffers(void);
s32 dynGetFreeGfx(Gfx *gdl);

#endif
