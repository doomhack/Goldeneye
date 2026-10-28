#include <ultra64.h>
#include "mema.h"

/**
 * mema - memory (ad hoc) allocation system.
 *
 * Mema's heap is one allocation in memp's stage pool, so it is reset each time a
 * level loads. Its size comes from the level's "-ma" token in boss.c (150 to 400 KB,
 * doubled in co-op).
 *
 * Unlike memp, mema can free individual allocations. GoldenEye only uses it for room
 * geometry (bg.c), which is loaded and dropped as the player moves around.
 *
 * Mema tracks what has been allocated by storing references to free spaces in
 * its spaces array. The allocations themselves are not referenced. When
 * initialising the spaces array, the first element is set to the entire heap
 * and the remaining elements are set to 0.
 *
 * This creates a bit of a terminology problem. Just remember that a MemaSpace
 * is not an allocation; it's a free space that is available for allocation.
 *
 * Due to the ability to free individual allocations, both the heap and the
 * spaces array can become fragmented. Mema supports defragmenting the spaces
 * array: entries are ordered by address, and back to back entries are merged.
 * The data in the heap itself is never moved, as that would require updating
 * pointers throughout the game code which mema cannot do.
 */

/* Number of free-space slots, plus one. PAL shrank this to 125 to fit 4MB; with the
 * Expansion Pak all versions use the NTSC size. */
#define MEMA_SLOTS 509

typedef struct MemaSpace {
    u32 addr;
    u32 size;
} MemaSpace;

/** addr of the end markers after the spaces array. */
#define MEMA_END 0xffffffff

/**
 * This structure contains dummy entries before and after the spaces array.
 * These are used as start and end markers, but could have been avoided by
 * using loop counters (eg. a typical i < numspaces loop).
 */
struct MemaHeap {
    struct MemaSpace start;
    struct MemaSpace spaces[MEMA_SLOTS - 1];
	struct MemaSpace end1;
	struct MemaSpace end2;
};

s32 m_memaHeapStart;
s32 m_memaHeapSize;
struct MemaHeap m_memaHeap;

/** Swaps two free-space entries. */
void memaSwap(MemaSpace *a, MemaSpace *b) {
    u32 tempaddr = a->addr;
    u32 tempsize = a->size;
    a->addr = b->addr;
    a->size = b->size;
    b->addr = tempaddr;
    b->size = tempsize;
}

/** Adds b's space to a (they must be adjacent) and empties b. */
void memaMerge(MemaSpace *a, MemaSpace *b) {
    a->size = (a->size + b->size);
    b->addr = 0;
    b->size = 0;
}

/**
 * One pass over the free-space list: swaps entries that are out of address order and
 * merges entries that touch. Returns TRUE if anything was merged.
 */
s32 memaDefragPass(struct MemaHeap *heap) {
    s32 any = FALSE;
	struct MemaSpace *prev = &heap->start;
	struct MemaSpace *curr = &heap->spaces[0];
	struct MemaSpace *last = &heap->spaces[MEMA_SLOTS - 2];

    u32 addr = 0;
    while (curr <= last) {
        if (curr->size != 0) {
            if (curr->addr < addr) {
                memaSwap(curr, prev);
            }
            if ((prev->size + addr) == curr->addr) {
                memaMerge(prev, curr);
                curr = prev;
                any = TRUE;
            }
            prev = curr;
            addr = curr->addr;
        }
        curr++;
    }
    return any;
}

/** Defragments the free-space list until nothing more can be merged. */
void memaDefrag(void)
{
    while (memaDefragPass(&m_memaHeap));
}

/**
 * Defrag the spaces list in an attempt to free up any slot.
 *
 * If none can be found, return the smallest run of free space so it can be
 * overwritten by the caller.
 */
MemaSpace *memaFindFreeSlot(struct MemaHeap *heap)
{
    struct MemaSpace *curr = &heap->spaces[0];
	struct MemaSpace *best;

    u32 min;
    s32 i;

    // Do 124 passes over the list. This ensures the list is in order by the
	// end. Though in most cases it's roughly in order anyway, and the excessive
	// looping is just wasting CPU cycles. In reality this situation probably
	// never occurs.
    for (i = 0; i < (MEMA_SLOTS - 1); i++) {
        while (curr <= &heap->spaces[MEMA_SLOTS - 2]) {
            if (curr->size == 0) {
                return curr;
            }

            if (curr[1].addr < curr[0].addr) {
                memaSwap(&curr[0], &curr[1]);
            }

            if (curr[1].addr == (curr[0].size + curr[0].addr)) {
                // Found two that can be merged
                curr[0].size += curr[1].size;
                curr[1].addr = 0;
                curr[1].size = 0;
                return &curr[1];
            }

            curr++;
        }

        curr = &heap->spaces[0];
    }

    // If this code is reached then the spaces list is so badly and unrepairably
	// fragmented that we can't find any slot to record the free space in.
	// Find the smallest run of free space and use that instead.
	// The caller will overwrite it with its own free allocation, causing the
	// original run of free space to be unusable until the mema heap is reset.
    min = 0xFFFFFFFF;
    best = curr;
    while (curr <= &heap->spaces[MEMA_SLOTS - 2]) {
        if (curr->size < min) {            
            best = curr;
            min = curr->size;
        }

        curr++;
    }

    return best;
}

/** Records the `size` bytes at `addr` as free space. */
void memaMarkFree(s32 addr, s32 size)
{
    // Choose an index in the spaces array which we'll mark a space as free,
	// based on how far into the heap the allocation is. This is a rough
	// estimate and doesn't need to be any particular index, but the defrag
	// function tries to order the spaces by address so the closer we get to it
	// the less work the defrag function will have to do should it be called.
    s32 index = ((addr - m_memaHeapStart) * (MEMA_SLOTS-1)) / m_memaHeapSize;
    struct MemaSpace *curr = &m_memaHeap.spaces[index];

    // If the entry is taken, keep moving forward until a zero is found.
    while (curr->size != 0) {
        curr++;
    }

    // If we reached the end of the spaces list, go backwards instead
    if (curr->addr == MEMA_END) {
        curr = &m_memaHeap.spaces[index];

        while (curr->size != 0) {
            curr--;
        }

        if (curr->addr == 0) {
            curr = memaFindFreeSlot(&m_memaHeap);
        }
    }

    // Mark this space as free
    curr->addr = addr;
    curr->size = size;
}

/** Makes the `heapsize` bytes at `heapaddr` the heap, all of it free. */
void memaReset(void *heapaddr, u32 heapsize)
{
   struct MemaSpace *space;


	m_memaHeap.start.addr = 0;
	m_memaHeap.start.size = 0;

	m_memaHeap.end1.addr = MEMA_END;
	m_memaHeap.end1.size = 0;
	m_memaHeap.end2.addr = MEMA_END;
	m_memaHeap.end2.size = 0xffffffff;

	for (space = &m_memaHeap.spaces[0]; space <= &m_memaHeap.spaces[MEMA_SLOTS - 2]; space++) {
		space->addr = 0;
		space->size = 0;
	}

	m_memaHeap.spaces[0].addr = m_memaHeapStart = (uintptr_t) heapaddr;
	m_memaHeap.spaces[0].size = m_memaHeapSize = heapsize;
}

void memaSingleDefragPass(void)
{
    memaDefragPass(&m_memaHeap);
}


/**
 * Allocates `amount` bytes and returns their address, or NULL if no free space is big
 * enough.
 *
 * It looks for the best fit among the first 16 free-space entries, stopping early at a
 * close fit. If none of those is big enough, it takes the first big enough entry after
 * them, and failing that defragments 8 times and takes the first big enough entry at all.
 */
void *memaAlloc(u32 amount) {
    s32 addr;
    u32 diff;
    s32 i;

    struct MemaSpace *curr;
	u32 bestdiff;
	struct MemaSpace *best;


	curr = &m_memaHeap.spaces[0];
	bestdiff = 0xffffffff;
	best = NULL;

    for (i = 0; i < 16; i++, curr++) {
        if (curr->size < amount) {
            continue;
        }

        if (curr->addr == MEMA_END) {
            break;
        }

        diff = (curr->size - amount);

        if (diff < bestdiff) {
            bestdiff = diff;
            best = curr;
            if ((diff < 64) || (diff < (amount / 4))) {
                break;
            }
        }
    }

    if (best == NULL) {
        while (curr->size < amount) {
            curr++;
        }

        if (curr->addr == MEMA_END) {
            for (i = 0; i < 8; i++) {
                memaDefragPass(&m_memaHeap);
            }

            curr = &m_memaHeap.spaces[0];

            while (curr->size < amount) {
                curr++;
            }

            if (curr->addr == MEMA_END) {
                return NULL;
            }
        }

        best = curr;
    }

    addr = best->addr;
    best->addr += amount;
    best->size -= amount;

    if (best->size == 0) {
        best->addr = 0;
    }

    return (void*)addr;
}

/**
 * Claims `amount` bytes of free space starting exactly at `addr`, which lets an
 * allocation that ends at `addr` grow in place. Returns `addr`, or 0 if there isn't that
 * much free space there.
 */
s32 memaGrow(u32 addr, u32 amount)
{
    u32        size = 0;
    u32        new_addr;
    MemaSpace *curr;


    curr     = &m_memaHeap.spaces[0];
    new_addr = addr;

    while (curr->addr != MEMA_END)
    {
        if (curr->addr == new_addr && curr->size >= amount)
        {
            goto found;
        }

        curr++;
    }

    return 0;

found:
    curr->addr += amount;
    curr->size -= amount;

    size = curr->size;

    if (size == 0)
    {
        curr->addr = 0;
    }

    return new_addr;
}

/** Frees `size` bytes at `addr`. mema doesn't record allocation sizes, so the caller must. */
void memaFree(void *addr, s32 size)
{
	memaMarkFree((uintptr_t) addr, size);
}


/**
 * Find and return the largest amount of contiguous free space in the pool.
 * ie. the biggest allocation that mema can currently make.
 */
s32 memaGetLongestFree(void)
{
	struct MemaSpace *curr;
	u32 biggest = 0;

	memaDefrag();

	curr = &m_memaHeap.spaces[0];

	while (curr->addr != MEMA_END) {
		if (curr->size > biggest) {
			biggest = curr->size;
		}

		curr++;
	}

	if (biggest) {
		return biggest;
	}

	return 0;
}

/**
 * Resizes the allocation at `addr` in place from `oldsize` to `newsize` bytes. Growing
 * only works if the free space right after it is big enough. Returns 1 on success, 0 if
 * it couldn't grow.
 */
s32 memaRealloc(s32 addr, u32 oldsize, u32 newsize)
{
    if ((newsize > oldsize))
    {
        if (memaGrow(addr + oldsize, newsize - oldsize) == 0)
        {
			return 0;
		}
        else
        {
            return 1;
        }
	}

    if ((oldsize > newsize))
    {
        memaFree((void *)(addr + newsize), oldsize - newsize);
    }
    
	return 1;
}
