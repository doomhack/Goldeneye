#include <ultra64.h>
#include "rom.h"
#include <macro.h>

/**
 * Copying data between the cartridge ROM and RAM by PI DMA. Every copy waits until it
 * has finished.
 *
 * romWrite writes to the cartridge address space, which only does anything on Rare's
 * development kit (where the "ROM" was RAM on the host); the ramrom demo recorder used it.
 */


OSIoMesg m_romIoMesg;
OSMesg m_romMesg;
OSMesgQueue m_romQueue;

/** Creates the message queue that DMA completions are sent to. */
void romCreateMesgQueue(void)
{
    osCreateMesgQueue(&m_romQueue, &m_romMesg, 1);
}

/** Starts a DMA from ROM to RAM, after dropping the target from the data cache. */
void romStartDma(void *target, void *source, u32 size)
{
    osInvalDCache(target, size);
    osPiStartDma(&m_romIoMesg, OS_MESG_PRI_NORMAL, OS_READ, source, target, size, &m_romQueue);
}

/** Waits for the DMA in progress to finish. */
void romWaitDma(void)
{
    osRecvMesg(&m_romQueue, NULL, OS_MESG_BLOCK);
}

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

/**
 * Copies `size` bytes from ROM to RAM. The ROM address must be even. A target that is not
 * 16-byte aligned, and any partial cache line at the end, go through the bounce buffer.
 */
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
 * Copies from any ROM address, even an odd one: copies from the even address below it
 * into the 16-byte aligned address at or above `target`, and returns where the requested
 * data starts. The buffer needs about 17 bytes to spare.
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

/** Starts a DMA from RAM to the cartridge address space, after writing back the cache. */
void romStartWrite(void *source, void *target, u32 size)
{
    osWritebackDCache(source, size);
    osPiStartDma(&m_romIoMesg, OS_MESG_PRI_NORMAL, OS_WRITE, target, source, size, &m_romQueue);
}

/** Writes to the cartridge address space and waits. Development kit only. */
void romWrite(void *source, void *target, u32 size)
{
    romStartWrite(source, target, size);
    romWaitDma();
}
