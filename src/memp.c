#include <ultra64.h>
#include "memp.h"

/**
 * memp: the pool allocator, which provides almost all of the game's memory.
 *
 * All RAM from the end of the game's code and data to the top of the 8 MB is split into
 * pools. A pool hands out memory like a stack: each allocation comes straight after the
 * previous one, nothing is freed on its own, and the whole pool is emptied at once.
 *
 *  - The stage pool holds everything for the current level and is emptied each time a
 *    level (or the title screen) loads. mema's heap is one allocation in it.
 *  - The permanent pool, 296 KB, holds things loaded once at boot.
 *
 * There is no error handling: running out of space hangs the game, except that when the
 * stage pool is full, allocations spill over into the permanent pool.
 */

MemoryPool m_mempPools[MEMPOOL_COUNT];

/** Set once the stage pool has filled up and allocations have moved to the permanent pool. */
s32 m_mempOverflowed = 0;

/**
 * Each size goes to the pool after the one named: the "ml" size sizes MEMPOOL_STAGE and
 * the "me" size MEMPOOL_PERMANENT. These defaults are never used, because boss.c always
 * sets "-ml0 -me0" (see mempInitPools).
 */
struct MempPoolSizes m_mempDefaultSizes = {
    MEMPOOL_MF + 1,    0,
    MEMPOOL_ML + 1,    82,
    MEMPOOL_ME + 1,    15,
    0,                 0
};

const char *tokenFind(s32 index, const char *token);
long int strtol(const char *str, char **endptr, int base);
/**
 * Splits the area from `areaStart` into pools. The sizes come from the "-mf", "-ml" and
 * "-me" command-line tokens. With "-me0", which boss.c always sets, the permanent pool gets
 * 296 KB and the stage pool the rest.
 */
void mempInitPools(s32 areaStart, s32 areaSize)
{
    MempPoolSizes poolSizes;

    m_mempPools[MEMPOOL_TOTAL].start = areaStart;
    m_mempPools[MEMPOOL_TOTAL].end = areaStart + areaSize;

    poolSizes = m_mempDefaultSizes;

    if (tokenFind(1, "-mf"))
    {
        poolSizes.mf = strtol(tokenFind(1, "-mf"), NULL, 0);
    }
    if (tokenFind(1, "-ml"))
    {
        poolSizes.ml = strtol(tokenFind(1, "-ml"), NULL, 0);
    }
    if (tokenFind(1, "-me"))
    {
        poolSizes.me = strtol(tokenFind(1, "-me"), NULL, 0);
    }
    if (poolSizes.me == 0)
    {
        poolSizes.mf = 0;
        poolSizes.me = 296 * 1024;
        poolSizes.ml = areaSize - poolSizes.me;
    }

    mempSetPoolSizes((s32*)&poolSizes);
}

/**
 * Lays the pools out one after another, each the size given in `poolSizes` (a
 * MempPoolSizes) scaled so that together they fill MEMPOOL_TOTAL, and rounded down to
 * 16 bytes.
 */
void mempSetPoolSizes(s32 poolSizes[MEMPOOL_COUNT+1])
{
    s32 i;
    s32 poolEnds[MEMPOOL_COUNT] = {0};
    s32 areaLen;
    s32 totalSize;
    s32 areaStart;

    // Put each size in its pool's slot. With "-ml0 -me0" (boss.c) this gives
    // {0, 0, 0, 0, areaSize - 296K, 0, 296K}.
    i = 0;
    do
    {
        poolEnds[poolSizes[i]] = poolSizes[i+1];
        i += 2;
    } while (poolSizes[i] != 0);

    // Running totals: poolEnds[i] becomes the end of pool i and the start of pool i + 1
    for (i = MEMPOOL_TOTAL; i < MEMPOOL_COUNT - 1; i++)
    {
        poolEnds[i + 1] += poolEnds[i];
    }

    // Scale so that the pools fill the area exactly
    totalSize = poolEnds[MEMPOOL_COUNT - 1];
    areaLen  = (m_mempPools[MEMPOOL_TOTAL].end - m_mempPools[MEMPOOL_TOTAL].start);

    for (i = MEMPOOL_TOTAL; i < MEMPOOL_COUNT; i++)
    {
        poolEnds[i] = ((s64)poolEnds[i] * areaLen) / totalSize;
    }

    for (i = MEMPOOL_TOTAL; i < MEMPOOL_COUNT; i++)
    {
        poolEnds[i] = ALIGN16_b(poolEnds[i]);
    }

    // The pools start closed (pos is NULL) until mempResetPool opens them
    areaStart = m_mempPools[MEMPOOL_TOTAL].start;

    for (i = MEMPOOL_TOTAL; i < MEMPOOL_COUNT - 1; i++)
    {
        m_mempPools[i + 1].start = poolEnds[i] + areaStart;
        m_mempPools[i + 1].pos   = 0;
        m_mempPools[i + 1].end   = poolEnds[i + 1] + areaStart;
    }
}

/**
 * Allocates `bytes` from a pool. Hangs if the pool is closed or full, except that when the
 * stage pool is full the allocation comes from the permanent pool instead. Sizes are not
 * rounded, so callers keep their own sizes aligned.
 */
void *mempAlloc(u32 bytes, u8 poolId)
{
    /*
     * Retain this address expression. Using
     * &m_mempPools[poolId] changes regalloc.
     */
    MemoryPool *pool = (MemoryPool *)(((u8 **)m_mempPools) + ((poolId * 2) << 1));
    u8 *allocation = pool->pos;


    if (pool->pos == NULL)
    {
        while (1);
    }

    if (pool->pos > pool->end)
    {

        while (1);
    }

    if (pool->pos + bytes > pool->end)
    {
        if (m_mempPools[MEMPOOL_PERMANENT].pos + bytes <= m_mempPools[MEMPOOL_PERMANENT].end)
        {
            /*
             * There was probably debug code in the original that got mostly
             * stripped, but it still perturbs register allocation. These
             * statements fill t1/t3/t4 so the registers match.
             */

            m_mempOverflowed = TRUE;

            return mempAlloc(bytes, MEMPOOL_PERMANENT);
        }


        while (1);
    }

    pool->pos += bytes;
    pool->prevpos = allocation;


    return allocation;
}


/**
 * Grows or shrinks the most recent allocation in a pool to `newSize` bytes, without
 * moving it. Returns MEMP_RESIZE_NOT_LAST, and does nothing, if `allocation` isn't the
 * most recent one. Hangs if the pool can't fit the new size.
 */
MEMP_RESIZE_RESULT mempResizeLast(void *allocation, s32 newSize, u8 poolId)
{
    MemoryPool *pool;
    s32 oldSize;
    s32 growBy;

    if (m_mempOverflowed && allocation == m_mempPools[MEMPOOL_PERMANENT].prevpos)
    {
        poolId = MEMPOOL_PERMANENT;
    }

    allocation = (void *)(u64)allocation;
    pool = &m_mempPools[poolId];

    if (pool->pos == 0)
    {
        while (TRUE);
    }

    if (allocation != pool->prevpos)
    {
        return MEMP_RESIZE_NOT_LAST;
    }

    oldSize = pool->pos - pool->prevpos;
    growBy = newSize - oldSize;

    if (growBy <= 0)
    {
        pool->pos += growBy;
        return MEMP_RESIZE_OK;
    }

    if (pool->pos > pool->end)
    {
        while (TRUE);
    }

    if (pool->pos + growBy > pool->end)
    {
        while (TRUE);
    }

    pool->pos += growBy;
    return MEMP_RESIZE_OK;
}

/** Bytes left in a pool, taking the stage pool's overflow into account. */
s32 mempGetFreeBytes(u8 pool) {
    if (m_mempOverflowed) {
        pool = MEMPOOL_PERMANENT;
    }

    if ((pool == MEMPOOL_STAGE) && (m_mempPools[MEMPOOL_STAGE].start == m_mempPools[MEMPOOL_STAGE].end))
    {
        pool = MEMPOOL_PERMANENT;
    }

    return m_mempPools[pool].end - m_mempPools[pool].pos;
}

/** Frees everything in a pool (and opens it, if it was closed). */
void mempResetPool(u8 pool) {
    m_mempPools[pool].prevpos = 0;
    m_mempPools[pool].pos = m_mempPools[pool].start;
}

/** Closes a pool: any allocation from it hangs until mempResetPool. */
void mempClosePool(u8 pool) {
    if (m_mempPools[pool].pos != 0) {
        m_mempPools[pool].pos = 0;
    }
}
