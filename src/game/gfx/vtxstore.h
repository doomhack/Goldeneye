#ifndef _VTXSTORE_H_
#define _VTXSTORE_H_
#include <ultra64.h>

// Which store vtxstoreAllocate takes from
#define VTXSTORE_CHR 0xcccc // characters whose vertices are recoloured (blood)
#define VTXSTORE_OBJ 0x0b0b // damaged props, shared between props of the same model and damage

void vtxstoreReset(void);
void vtxstoreTick(void);
Vertex *vtxstoreAllocate(s32 numVtx, s32 store, ModelFileHeader *modeldef, s32 destroyedLevel);
void vtxstoreFree(Vertex *vertices);

#endif
