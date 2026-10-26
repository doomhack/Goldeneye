#include <ultra64.h>
#include "ramrom.h"
#include <macro.h>

/**
 * @file ramrom.c
 * This file contains code to handle reading and writing rom addresses.
 */


OSIoMesg m_romIoMesg;
OSMesg m_romMesg;
OSMesgQueue m_romQueue;

/**
 * 6760	70005B60
 * external
 * romCreateMesgQueue
 * creates a message queue
 */
void romCreateMesgQueue(void)
{
    osCreateMesgQueue(&m_romQueue, &m_romMesg, 1);
}

/**
 * 6790	70005B90
 * romStartDma
 * invalidate cache and do pi dma
 */
void romStartDma(void *target, void *source, u32 size)
{
    osInvalDCache(target, size);
    osPiStartDma(&m_romIoMesg, OS_MESG_PRI_NORMAL, OS_READ, source, target, size, &m_romQueue);
}

/**
 * 67F0	70005BF0
 * romWaitDma
 * receives a message queue
 */
void romWaitDma(void)
{
    osRecvMesg(&m_romQueue, NULL, OS_MESG_BLOCK);
}

/**
 * 681C	70005C1C
 * external
 * romCopy
 * copy from rom to ram
 */
/**
 * A DMA target that shares a data cache line with other data is not safe on
 * real hardware: if the CPU touches that line while the DMA is in flight, the
 * line is refilled from RAM before the DMA lands and the stale copy wins.
 * Emulators complete DMAs instantly, so this only ever shows up on a console.
 * Unaligned copies (typically buffers on the stack) go through this buffer.
 */
#define ROM_BOUNCE_SIZE 0x400
static u8 romBounceBuffer[ROM_BOUNCE_SIZE] __attribute__((aligned(16)));

static void romCopyBounced(u8 *target, u8 *source, u32 size)
{
    u32 chunk;

    while (size != 0)
    {
        chunk = (size < ROM_BOUNCE_SIZE) ? size : ROM_BOUNCE_SIZE;
        romStartDma(romBounceBuffer, source, ALIGN16_a(chunk));
        romWaitDma();
        bcopy(romBounceBuffer, target, chunk);
        target += chunk;
        source += chunk;
        size -= chunk;
    }
}

void romCopy(void *target, void *source, u32 size)
{
    u32 direct;

    if ((u32) target & 0xF)
    {
        romCopyBounced(target, source, size);
        return;
    }

    // Whole cache lines go straight to the target, a partial last line is bounced
    direct = size & ~0xF;

    if (direct != 0)
    {
        romStartDma(target, source, direct);
        romWaitDma();
    }

    if (direct != size)
    {
        romCopyBounced((u8 *) target + direct, (u8 *) source + direct, size - direct);
    }
}

/**
 * 6844	70005C44
 * external
 * romCopyUnaligned
 * aligns data, does a romCopy(), then returns aligned pointer to target
 */
s32 romCopyUnaligned(void *target, void *source, s32 length)
{
    s32 target_offset;
    s32 *target_aligned;
    s32 *source_aligned;
    s32 *source_offset;

    source_aligned = align_addr_even((s32)source);
    source_offset = (s32)source - (s32)source_aligned;
    target_aligned = ALIGN16_a((s32)target);
    target_offset = source_offset;
    romCopy(target_aligned, source_aligned, ALIGN16_a((s32)source_offset + length));
    return ((s32)target_aligned + target_offset);
}

/**
 * 68A8	70005CA8
 * romStartWrite
 * actually writes to rom (buffer on Indy)
 */
void romStartWrite(void *source, void *target, u32 size)
{
    osWritebackDCache(source, size);
    osPiStartDma(&m_romIoMesg, OS_MESG_PRI_NORMAL, OS_WRITE, target, source, size, &m_romQueue);
}

/**
 * 6908	70005D08
 * external
 * romWrite
 * let's write to the rom (buffer on Indy)
 */
void romWrite(void *source, void *target, u32 size)
{
    romStartWrite(source, target, size);
    romWaitDma();
}
