#include <ultra64.h>
#include "dbgscreen.h"
#include "mema.h"

/**
 * mema: a small heap with real frees, used for room geometry.
 *
 * mema's heap is one allocation in memp's stage pool, so it starts empty each time a level
 * loads. Its size comes from the level's memory budget in boss.c (100 to 400 KB, doubled
 * in co-op). Only world/bg.c uses it: rooms are loaded into it as they come into view and
 * freed again when they go out of view.
 *
 * mema keeps a list of the free ranges in the heap, sorted by address. Allocations
 * themselves aren't recorded anywhere, so memaFree needs the caller to pass the size. A
 * freed range is merged with the free ranges right before and after it, so the list never
 * holds two ranges that touch.
 *
 * Every size is rounded up to 16 bytes, which keeps every allocation 16-byte aligned (room
 * data is loaded by DMA, see romCopy).
 */

/** The most free ranges the list can hold. */
#define MEMA_MAX_FREE 508

#define MEMA_ALIGN(x) (((x) + 15) & ~(typeof(x)) 15)

typedef struct MemaRange {
    u8 *start;
    u32 size;
} MemaRange;

/** The free ranges, sorted by address. */
static MemaRange m_memaFree[MEMA_MAX_FREE];
static s32 m_memaFreeCount;

/** Removes free range `index` from the list. */
static void memaRemoveRange(s32 index)
{
    m_memaFreeCount--;

    for (s32 i = index; i < m_memaFreeCount; i++)
    {
        m_memaFree[i] = m_memaFree[i + 1];
    }
}

/** Inserts a free range at position `index` in the list, which must have room for it. */
static void memaInsertRange(s32 index, u8 *start, u32 size)
{
    for (s32 i = m_memaFreeCount; i > index; i--)
    {
        m_memaFree[i] = m_memaFree[i - 1];
    }

    m_memaFree[index].start = start;
    m_memaFree[index].size = size;
    m_memaFreeCount++;
}

/** Returns the index of the first free range that starts after `addr`. */
static s32 memaFindRangeAfter(u8 *addr)
{
    s32 low = 0;
    s32 high = m_memaFreeCount;

    while (low < high)
    {
        s32 mid = (low + high) / 2;

        if (m_memaFree[mid].start <= addr)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }

    return low;
}

/** Returns the index of the smallest free range. The list must not be empty. */
static s32 memaFindSmallestRange(void)
{
    s32 smallest = 0;

    for (s32 i = 1; i < m_memaFreeCount; i++)
    {
        if (m_memaFree[i].size < m_memaFree[smallest].size)
        {
            smallest = i;
        }
    }

    return smallest;
}

/** Makes the `size` bytes at `heap` the heap, all of it free. */
void memaReset(void *heap, u32 size)
{
    u8 *start = (u8 *) MEMA_ALIGN((uintptr_t) heap);

    m_memaFreeCount = 0;
    size -= start - (u8 *) heap;
    size &= ~15u;

    if (size != 0)
    {
        memaInsertRange(0, start, size);
    }
}

/**
 * Allocates `size` bytes and returns them, or returns NULL if no free range is big enough.
 * It takes the smallest free range that fits (the best fit), to leave the big ones for big
 * rooms.
 */
void *memaAlloc(u32 size)
{
    s32 best = -1;
    u8 *allocation;

    size = MEMA_ALIGN(size);

    if (size == 0)
    {
        return NULL;
    }

    for (s32 i = 0; i < m_memaFreeCount; i++)
    {
        if (m_memaFree[i].size >= size && (best < 0 || m_memaFree[i].size < m_memaFree[best].size))
        {
            best = i;

            if (m_memaFree[i].size == size)
            {
                break;
            }
        }
    }

    if (best < 0)
    {
        return NULL;
    }

    allocation = m_memaFree[best].start;
    m_memaFree[best].start += size;
    m_memaFree[best].size -= size;

    if (m_memaFree[best].size == 0)
    {
        memaRemoveRange(best);
    }

    return allocation;
}

/**
 * Frees the `size` bytes at `addr`. The size must be the one the memory was allocated (or
 * last resized) with. Freeing memory that is already free stops the game.
 */
void memaFree(void *addr, u32 size)
{
    u8 *start = addr;
    s32 next;
    bool joinsPrev;
    bool joinsNext;

    size = MEMA_ALIGN(size);

    if (size == 0)
    {
        return;
    }

    next = memaFindRangeAfter(start);

    // Catch double frees and bad sizes: the range mustn't overlap the free space around it
    if ((next > 0 && m_memaFree[next - 1].start + m_memaFree[next - 1].size > start)
        || (next < m_memaFreeCount && start + size > m_memaFree[next].start))
    {
        dbgFatal("mema: freeing free memory at", (uintptr_t) start);
    }

    joinsPrev = next > 0 && m_memaFree[next - 1].start + m_memaFree[next - 1].size == start;
    joinsNext = next < m_memaFreeCount && start + size == m_memaFree[next].start;

    if (joinsPrev && joinsNext)
    {
        m_memaFree[next - 1].size += size + m_memaFree[next].size;
        memaRemoveRange(next);
    }
    else if (joinsPrev)
    {
        m_memaFree[next - 1].size += size;
    }
    else if (joinsNext)
    {
        m_memaFree[next].start = start;
        m_memaFree[next].size += size;
    }
    else
    {
        if (m_memaFreeCount == MEMA_MAX_FREE)
        {
            // The heap is too fragmented to record another range. Give up the smallest
            // range instead (until the next level), or this one if it's smaller still.
            s32 smallest = memaFindSmallestRange();

            if (m_memaFree[smallest].size <= size)
            {
                memaRemoveRange(smallest);

                if (smallest < next)
                {
                    next--;
                }
            }
            else
            {
                return;
            }
        }

        memaInsertRange(next, start, size);
    }
}

/** Returns the biggest allocation mema can make right now. */
u32 memaGetLongestFree(void)
{
    u32 longest = 0;

    for (s32 i = 0; i < m_memaFreeCount; i++)
    {
        if (m_memaFree[i].size > longest)
        {
            longest = m_memaFree[i].size;
        }
    }

    return longest;
}

/**
 * Resizes the allocation at `addr` in place from `oldSize` to `newSize` bytes. Shrinking
 * always works; growing only works if the free range right after the allocation is big
 * enough. Returns whether it worked.
 */
bool memaRealloc(void *addr, u32 oldSize, u32 newSize)
{
    u8 *end;
    s32 next;
    u32 growBy;

    oldSize = MEMA_ALIGN(oldSize);
    newSize = MEMA_ALIGN(newSize);
    end = (u8 *) addr + oldSize;

    if (newSize < oldSize)
    {
        memaFree((u8 *) addr + newSize, oldSize - newSize);
        return true;
    }

    if (newSize == oldSize)
    {
        return true;
    }

    growBy = newSize - oldSize;
    next = memaFindRangeAfter(end - 1);

    if (next >= m_memaFreeCount || m_memaFree[next].start != end || m_memaFree[next].size < growBy)
    {
        return false;
    }

    m_memaFree[next].start += growBy;
    m_memaFree[next].size -= growBy;

    if (m_memaFree[next].size == 0)
    {
        memaRemoveRange(next);
    }

    return true;
}
