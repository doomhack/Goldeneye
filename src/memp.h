#ifndef _MEMP_H_
#define _MEMP_H_

#include <ultra64.h>

/*
* Align to 16 bit boundary. Version "b", without preliminary addition.
*/
#define ALIGN16_b(val)        (((val) | 0xf) ^ 0xf)

/** A pool is a stack: allocations are taken from `pos` upwards and freed all at once. */
typedef struct MemoryPool {
    u8 *start;
    u8 *pos;     /* next free byte; NULL when the pool is closed */
    u8 *end;
    u8 *prevpos; /* start of the most recent allocation */
} MemoryPool;

/**
 * Pool sizes as (pool, size) pairs, ended by a pool of 0. The sizes are relative: they are
 * scaled to fill the whole pool area. The "ml" size goes to the stage pool and the "me"
 * size to the permanent pool (see mempInitPools).
 */
typedef struct MempPoolSizes {
    u32 mfPool;
    u32 mf;
    u32 mlPool;
    u32 ml;
    u32 mePool;
    u32 me;
    u32 endPool;
    u32 endSize;
} MempPoolSizes;

/**
 * The pools. Only STAGE and PERMANENT ever have any space; the others are leftovers from
 * a layout Rare stopped using, and are always empty.
 */
enum MEMPOOL
{
    MEMPOOL_SIGNED_ENUM = -1,
    MEMPOOL_TOTAL,     /* the whole area, from _heapSegmentStart to the top of RAM */
    MEMPOOL_MF,
    MEMPOOL_2,
    MEMPOOL_ML,
    MEMPOOL_STAGE,     /* everything for the current level; emptied when a level loads */
    MEMPOOL_ME,
    MEMPOOL_PERMANENT, /* things loaded once at boot, such as the menu text */
    MEMPOOL_COUNT
};

typedef enum MEMP_RESIZE_RESULT
{
    MEMP_RESIZE_RESULT_SIGNED_ENUM = -1,
    MEMP_RESIZE_OK = 1,
    MEMP_RESIZE_NOT_LAST = 2
} MEMP_RESIZE_RESULT;
void mempInitPools(int areaStart, int areaSize);
void mempSetPoolSizes(s32 sizes[8]);
void *mempAlloc(u32 bytes, u8 pool);
MEMP_RESIZE_RESULT mempResizeLast(void *allocation, s32 newSize, u8 pool);
s32 mempGetFreeBytes(u8 pool);
void mempResetPool(u8 pool);
void mempClosePool(u8 pool);

#endif
