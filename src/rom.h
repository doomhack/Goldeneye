#ifndef _ROM_H_
#define _ROM_H_
#include <ultra64.h>

void romCreateMesgQueue(void);
void romCopy(void *target, void *source, u32 size);
void *romCopyUnaligned(const void *target, void *source, s32 length);
void romWrite(void *source, const void *target, u32 size);

#endif
