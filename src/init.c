#include <ultra64.h>
#include <PR/R4300.h>
#include <PR/os.h>
#include <PR/os_internal.h>
#include <PR/rcp.h>
#include <bondgame.h>
#include "inflate/inflate.h"
#include "sched.h"
#include "rmon.h" /*<PR/rmon.h>*/
#include "init.h"
#include "thread_config.h"
#include "boss.h"
#include "pi.h"
#include "token.h"
#include "vi.h"


/**
 * EU .data, offset from start of data_seg : 0x22B0
*/

/**
 * @file init.c
 * This file contains the initial non bootstrap code ran.
 *
 * In particular, it:
 *   - unpacks main data payload
 *   - starts idle and rmon loops
 *   - starts main loop
 */


#define NUM_FIELDS  1

#define MAXSP 7

/* Boot-time scratch inside the first 4MB, reused by the heap afterwards (see ge007.ld) */
#define RZIPLOADADDR 0x80300000
#define RZIPBUFADDR 0x80340000


OSThread rmonThread;
OSThread idleThread;
OSThread mainThread;
OSThread shedThread;
OSMesgQueue gfxFrameMsgQ;
OSMesg gfxFrameMsgBuf[32];
OSMesgQueue *sched_cmdQ;

void mainproc(void *args);


/**
 * init - The real main entry point, called from _start.s
 * Deflates the compressed data segment
 * Loads the game code into RAM
 * Then starts the main thread
 */
void init(void)
{
    u32 inflateromSize;
    u8 *csegmentSegmentVaddrStart;
    u8 *cdataSegmentRomStart;
    u32 flags;
    u32 cdataSegmentRomSize;
    u8 *inflateSegmentRomStart;
    s32 i;
    s32 j;

    u8 *datazipram;
    u32 gameSegmentSize;
    u32 copylen;
    s32 *stack_pointer;
    u8 *dataziprom;

    csegmentSegmentVaddrStart = (u8 *) &_csegmentSegmentStart;
    cdataSegmentRomStart = (u8 *) &_cdataSegmentRomStart;
    cdataSegmentRomSize = (u8 *) &_cdataSegmentRomEnd - cdataSegmentRomStart;
    inflateSegmentRomStart = (u8 *) &_inflateSegmentRomStart;
    inflateromSize = (u8 *) &_inflateSegmentRomEnd - inflateSegmentRomStart;
    copylen = cdataSegmentRomSize + inflateromSize;
    datazipram = (u8 *) (RZIPLOADADDR - cdataSegmentRomSize);
    dataziprom = csegmentSegmentVaddrStart;

    for (j = copylen - 1; j >= 0; j--)
    {
        datazipram[j] = dataziprom[j];
    }

    decompress_entry(datazipram, csegmentSegmentVaddrStart, (struct huft *) RZIPBUFADDR);

    // Load all game code into RAM at its link address. It used to be paged in
    // 8KB at a time through the TLB; with the Expansion Pak it simply stays resident.
    gameSegmentSize = (u32) &_gameSegmentEnd - (u32) &_gameSegmentStart;
    osWritebackDCacheAll();
    osPiRawStartDma(OS_READ, (u32) &_gameSegmentRomStart, &_gameSegmentStart, gameSegmentSize);
    while ((osPiGetStatus() & PI_STATUS_DMA_BUSY))
    {
    }
    osInvalDCache(&_gameSegmentStart, gameSegmentSize);

    osInitialize();

    // Refresh Cache
    osWritebackDCacheAll();
    osInvalICache((void *) K0BASE, ICACHE_SIZE);

    // Cleanup TLB, keeping only entry 0 (rdb, set up by osInitialize)
    for (i = 1; i < NTLBENTRIES + 1; i++)
    {
        osUnmapTLB(i);
    }

    // Setup floating point register
    flags = __osGetFpcCsr();
    flags |= FPCSR_EI; // enable inexact operation
    flags |= FPCSR_EO; // enable overflow
    flags |= FPCSR_EZ; // enable division by zero
    flags |= FPCSR_EV; // enable invalid operation
    __osSetFpcCsr(flags);

    stack_pointer = setSPToEnd(sp_main, sizeof(sp_main));
    osCreateThread(&mainThread, MAIN_THREAD_ID, &mainproc, NULL, stack_pointer, MAIN_THREAD_PRIORITY);
    osStartThread(&mainThread);
}


/**
 * setSPToEnd - set stack pointer to end of stack
 * @param stack
 * @param size
 * @return stack+size-8
 */
void *setSPToEnd(u8 *stack, u32 size)
{
    return stack + size - 8;
}

/**
 * idleproc - infinite loop
 * @param arg doesn't matter as this is infinite loop
 */
void idleproc(void *arg)
{
    for (;;);
}

/**
 * idleCreateThread - creates an empty thread;
 */
void idleCreateThread(void)
{
    osCreateThread(&idleThread, IDLE_THREAD_ID, idleproc, NULL, setSPToEnd(sp_idle, sizeof(sp_idle)), IDLE_THREAD_PRIORITY);
    osStartThread(&idleThread);
}

/**
 * rmonCreateThread - remote communication thread
 * rmonMain now forcably returns INDI_NOT_DETECTED (1)
 */
void rmonCreateThread(void)
{
    osCreateThread(&rmonThread, RMON_THREAD_ID, rmonMain, NULL, setSPToEnd(sp_rmon, sizeof(sp_rmon)), RMON_THREAD_PRIORITY);
    osStartThread(&rmonThread);
}

/**
 * schedulerInitThread - create scheduler thread based on TV type
 */
void schedulerInitThread(void)
{
    osCreateMesgQueue(&gfxFrameMsgQ, gfxFrameMsgBuf, 32);
    if (osTvType == OS_TV_MPAL)
    {
        osCreateScheduler(&os_scheduler, &shedThread, OS_VI_MPAL_LAN1, NUM_FIELDS);
    }
    else
    {
        osCreateScheduler(&os_scheduler, &shedThread, OS_VI_NTSC_LAN1, NUM_FIELDS);
    }

    osScAddClient(&os_scheduler, &gfxClient, &gfxFrameMsgQ, FALSE);
    sched_cmdQ = osScGetCmdQ(&os_scheduler);
}

/**
 * mainproc - main game setup and loop
 * calls command line parser, debug console setup, etc.
 * @param args
 */
void mainproc(void *args)
{
    idleCreateThread();
    viDebugRemoved();
    piCreateManager();
    rmonCreateThread();
    if (tokenReadIo())
    {
        osStopThread(RMON_THREAD_ID);
    }

    osSetThreadPri(RMON_THREAD_ID, MAIN_THREAD_PRIORITY);
    // Timers are initialized via:
    // schedulerInitThread -> osCreateScheduler -> osCreateViManager -> __osTimerServicesInit
    schedulerInitThread();
    bossEntry();
}

