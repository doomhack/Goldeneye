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

/**
 * The least the permanent pool needs. It gets whatever the framebuffers leave of the top
 * 1 MB bank (ge007.ld checks this fits).
 */
#define MEMP_PERMANENT_SIZE (296 * 1024)

void mempInit(u8 *stageStart, u8 *stageEnd, u8 *permanentStart, u8 *permanentEnd);
void *mempAlloc(u32 bytes, MemPool pool);
bool mempResizeLast(const void *allocation, u32 newSize);
u32 mempGetFreeBytes(MemPool pool);
void mempResetPool(MemPool pool);
void mempClosePool(MemPool pool);

#endif
