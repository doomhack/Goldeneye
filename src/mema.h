#ifndef _MEMA_H_
#define _MEMA_H_

#include <ultra64.h>

void memaReset(void *heap, u32 size);
void *memaAlloc(u32 size);
void memaFree(void *addr, u32 size);
u32 memaGetLongestFree(void);
bool memaRealloc(void *addr, u32 oldSize, u32 newSize);

#endif
