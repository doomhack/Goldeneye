#ifndef _MEMP_H_
#define _MEMP_H_

#include <ultra64.h>

/** The memory pools. Each one is a stack of allocations that is emptied all at once. */
typedef enum MemPool : u8
{
    MEMPOOL_STAGE,     /* everything for the current level; emptied when a level loads */
    MEMPOOL_PERMANENT, /* things loaded once at boot, such as the menu text */
    MEMPOOL_COUNT
} MemPool;

/** The permanent pool's size. The stage pool gets the rest of the memory. */
#define MEMP_PERMANENT_SIZE ((296 + 1280) * 1024) /* + F3DEX3 spike translation buffers */

void mempInit(u8 *start, u8 *end);
void *mempAlloc(u32 bytes, MemPool pool);
bool mempResizeLast(void *allocation, u32 newSize);
u32 mempGetFreeBytes(MemPool pool);
void mempResetPool(MemPool pool);
void mempClosePool(MemPool pool);

#endif
