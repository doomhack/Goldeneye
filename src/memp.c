#include <ultra64.h>
#include <deb.h>
#include "memp.h"
#include "game/text/language.h"

/**
 * EU .data, offset from start of data_seg : 0x3640
*/

/**
 * @file memp.c
 * This file contains code for memp.
 */

//bss
MemoryPool m_mempPools[MEMPOOL_COUNT];

//data
void *m_mempDebug = 0;
s32 m_mempOverflowed = 0;
s32 D_80024408 = 0;
s32 D_8002440C = 0;
s32 D_80024410 = 0;

//overloaded
struct MempPoolSizes m_mempDefaultSizes = {
    MEMPOOL_MF + 1,    0,  // MEMPOOL_MF
    MEMPOOL_ML + 1,    82, // MEMPOOL_ML
    MEMPOOL_ME + 1,    15, // MEMPOOL_ME
    0,                 0   // MEMPOOL_END
};

void mempInit(void)
{
    debTryAdd(&m_mempDebug, "memp_c_debug");
}

const char *tokenFind(s32 arg0, const char *arg1);
long int strtol(const char *str, char **endptr, int base);
void mempInitPools(s32 poolAreaStart, s32 poolAreaSize)
{
    MempPoolSizes poolSizes;

    //set pool 0 to what boss wants (room_model_buffer)
    //pool 0 = TotalPoolArea
    m_mempPools[MEMPOOL_TOTAL].start = poolAreaStart;
    m_mempPools[MEMPOOL_TOTAL].end = poolAreaStart + poolAreaSize;

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
        poolSizes.me = ((j_text_trigger ? 308 : 296) * 1024);
        poolSizes.ml = poolAreaSize - poolSizes.me;
    }

    mempSetPoolSizes((s32*)&poolSizes);
}

void mempSetPoolSizes(s32 poolSizes[MEMPOOL_COUNT+1])
{
    s32 i;
    s32 bankstarts[MEMPOOL_COUNT] = {0};
    s32 mempLen;
    s32 mempRequested;
    s32 mempStart;

    //set MF, ML, ME first
    i = 0;
    do
    {
        // assign the "xxxIndex" the value of xxx+1 then skip "Indices", 0=2=mf, 2=4=ml, 4=6=me, 6=8=end
        bankstarts[poolSizes[i]] = poolSizes[i+1];
        i += 2;
    } while (poolSizes[i] != 0); //while sizes not = 0 (bank 7 = 0)
    //  0 1 2 3            4           5     6
    // {0,0,0,0,poolAreaSize - 303104, 0, 303104}

    //for each bankstart, add current to next
    for (i = MEMPOOL_TOTAL; i < MEMPOOL_COUNT - 1; i++)
    {
        bankstarts[i + 1] += bankstarts[i];
    }
    // {0,0,0,0,poolAreaSize - 303104, poolAreaSize - 303104, poolAreaSize}


    mempRequested = bankstarts[MEMPOOL_COUNT - 1]; //total accumulated size of banks = poolAreaSize
    mempLen  = (m_mempPools[MEMPOOL_TOTAL].end - m_mempPools[MEMPOOL_TOTAL].start);

    //for each bankstart, multiply by total pool size, then divide by size of banks 1-7
    //spread each bank evenly
    for (i = MEMPOOL_TOTAL; i < MEMPOOL_COUNT; i++)
    {
        bankstarts[i] = ((s64)bankstarts[i] * mempLen) / mempRequested;
    }
    // {0,0,0,0,poolAreaSize - 303104, poolAreaSize - 303104, poolAreaSize}

    for (i = MEMPOOL_TOTAL; i < MEMPOOL_COUNT; i++)
    {
        bankstarts[i] = ALIGN16_b(bankstarts[i]);
    }
    // {0,0,0,0,poolAreaSize - 303104, poolAreaSize - 303104, poolAreaSize}


    mempStart = m_mempPools[MEMPOOL_TOTAL].start;
    //for each bank 1-7, add new start position
    for (i = MEMPOOL_TOTAL; i < MEMPOOL_COUNT - 1; i++)
    {
        m_mempPools[i + 1].start = bankstarts[i] + mempStart;
        m_mempPools[i + 1].pos   = 0;
        m_mempPools[i + 1].end   = bankstarts[i + 1] + mempStart;
    }
    /*
                           rel-start              size
    g_memPools[TOTAL]      0                      poolArea
    g_memPools[MF]         0                      0
    g_memPools[2]          0                      0
    g_memPools[ML]         0                      0
    g_memPools[STAGE]      0                      poolAreaSize - 303104
    g_memPools[ME]         poolAreaSize - 303104  0
    g_memPools[PERMANENT]  poolAreaSize - 303104  303104
    */
}


void *mempAlloc(u32 bytes, u8 poolnum)
{
    /*
     * Retain this address expression. Using
     * &m_mempPools[poolnum] changes regalloc.
     */
    MemoryPool *pool = (MemoryPool *)(((u8 **)m_mempPools) + ((poolnum * 2) << 1));
    u8 *allocation = pool->pos;

#ifdef DEBUG
    if ((poolnum < 0) || (4 < poolnum))
    {
        osSyncPrintf("mempAlloc from invalid heap %d!", poolnum);
    }
#endif

    if (pool->pos == NULL)
    {
        while (1);
    }

    if (pool->pos > pool->end)
    {
        mempCheckPools();

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
            if (m_mempOverflowed);
            if (&D_8002440C == &D_80024408);
            if (m_mempOverflowed);
            if (&D_80024410 == &D_80024408);
            if (!m_mempOverflowed);

            m_mempOverflowed = TRUE;

            return mempAlloc(bytes, MEMPOOL_PERMANENT);
        }

        mempCheckPools();

        while (1);
    }

    pool->pos += bytes;
    pool->prevpos = allocation;

    if (m_mempOverflowed);

    return allocation;
}


/**
 * Resize the most recent allocation in a pool without moving it.
 */
MEMP_RESIZE_RESULT mempResizeLast(void *allocation, s32 newsize, u8 poolnum)
{
    MemoryPool *pool;
    s32 origsize;
    s32 growsize;

    if (m_mempOverflowed && allocation == m_mempPools[MEMPOOL_PERMANENT].prevpos)
    {
        poolnum = MEMPOOL_PERMANENT;
    }

    allocation = (void *)(u64)allocation;
    pool = &m_mempPools[poolnum];

    if (pool->pos == 0)
    {
        while (TRUE);
    }

    if (allocation != pool->prevpos)
    {
        return MEMP_RESIZE_NOT_LAST;
    }

    origsize = pool->pos - pool->prevpos;
    growsize = newsize - origsize;

    if (growsize <= 0)
    {
        pool->pos += growsize;
        return MEMP_RESIZE_OK;
    }

    if (pool->pos > pool->end)
    {
        mempCheckPools();
        while (TRUE);
    }

    if (pool->pos + growsize > pool->end)
    {
        mempCheckPools();
        while (TRUE);
    }

    pool->pos += growsize;
    return MEMP_RESIZE_OK;
}

void mempCheckPools(void) {
    u8 bank;
    for (bank = MEMPOOL_MF; bank < MEMPOOL_COUNT; bank++)
    {
    }
}

s32 mempGetFreeBytes(u8 bank) {
    if (m_mempOverflowed) {
        bank = MEMPOOL_PERMANENT;
    }

    if ((bank == MEMPOOL_STAGE) && (m_mempPools[MEMPOOL_STAGE].start == m_mempPools[MEMPOOL_STAGE].end))
    {
        bank = MEMPOOL_PERMANENT;
    }

    return m_mempPools[bank].end - m_mempPools[bank].pos;
}

// Last three bits contains the bank, the rest contains the size.
u32 mempAllocPacked(u32 sizeandbank) {
    return mempAlloc((sizeandbank >> 3), (sizeandbank & 7));
}

void mempResetPool(u8 bank) {
    m_mempPools[bank].prevpos = 0;
    m_mempPools[bank].pos = m_mempPools[bank].start;
}

void mempClosePool(u8 bank) {
    mempCheckPools();
    if (m_mempPools[bank].pos != 0) {
        m_mempPools[bank].pos = 0;
    }
}
