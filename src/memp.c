#include <ultra64.h>
#include "dbgscreen.h"
#include "memp.h"

/**
 * memp: the pool allocator, which provides almost all of the game's memory.
 *
 * RAM is split into two pools (see bossInitMainthreadData for the map). The stage pool
 * runs from the end of the game's code, data and stacks up to the animation frames; the
 * permanent pool fills the rest of the framebuffers' bank, at the top of RAM. A pool hands out memory like a stack: each allocation comes straight after
 * the previous one, nothing is freed on its own, and the whole pool is emptied at once.
 *
 *  - The stage pool holds everything for the current level and is emptied each time a
 *    level (or the title screen) loads. mema's heap is one allocation in it.
 *  - The permanent pool holds things loaded once at boot.
 *
 * A level uses about 2 MB of the 5.8 MB stage pool, and boot about 110 KB of the permanent
 * pool, so running out means something is badly wrong: it stops the game with an error.
 */

typedef struct MemoryPool {
    u8 *start;
    u8 *pos;  /* the next free byte; NULL while the pool is closed */
    u8 *end;
    u8 *last; /* the start of the most recent allocation */
} MemoryPool;

static MemoryPool m_mempPools[MEMPOOL_COUNT];

/** Sets up the two pools' memory ranges. Both start closed. */
void mempInit(u8 *stageStart, u8 *stageEnd, u8 *permanentStart, u8 *permanentEnd)
{
    m_mempPools[MEMPOOL_STAGE] = (MemoryPool) { .start = stageStart, .end = stageEnd };
    m_mempPools[MEMPOOL_PERMANENT] = (MemoryPool) { .start = permanentStart, .end = permanentEnd };
}

/**
 * Allocates `bytes` from a pool. Sizes are not rounded, so callers keep their own sizes
 * aligned. Stops the game if the pool is closed or full.
 */
void *mempAlloc(u32 bytes, MemPool poolId)
{
    MemoryPool *pool = &m_mempPools[poolId];

    if (pool->pos == NULL)
    {
        dbgFatal("memp: allocating from a closed pool", poolId);
    }

    if (bytes > (u32) (pool->end - pool->pos))
    {
        dbgFatal(poolId == MEMPOOL_STAGE ? "memp: stage pool full, bytes" : "memp: permanent pool full, bytes", bytes);
    }

    pool->last = pool->pos;
    pool->pos += bytes;

    return pool->last;
}

/**
 * Grows or shrinks the most recent allocation in its pool to `newSize` bytes, without
 * moving it. Returns false, and does nothing, if `allocation` isn't the most recent
 * allocation in either pool. Stops the game if the pool can't fit the new size.
 */
bool mempResizeLast(const void *allocation, u32 newSize)
{
    for (s32 i = 0; i < MEMPOOL_COUNT; i++)
    {
        MemoryPool *pool = &m_mempPools[i];

        if (pool->pos != NULL && pool->last == allocation)
        {
            if (newSize > (u32) (pool->end - pool->last))
            {
                dbgFatal("memp: no room to grow to", newSize);
            }

            pool->pos = pool->last + newSize;
            return true;
        }
    }

    return false;
}

/** Returns how many bytes are left in a pool. */
u32 mempGetFreeBytes(MemPool poolId)
{
    const MemoryPool *pool = &m_mempPools[poolId];

    return pool->pos == NULL ? 0 : pool->end - pool->pos;
}

/** Frees everything in a pool, and opens it if it was closed. */
void mempResetPool(MemPool poolId)
{
    m_mempPools[poolId].pos = m_mempPools[poolId].start;
    m_mempPools[poolId].last = NULL;
}

/** Closes a pool: allocating from it stops the game until mempResetPool opens it again. */
void mempClosePool(MemPool poolId)
{
    m_mempPools[poolId].pos = NULL;
}
