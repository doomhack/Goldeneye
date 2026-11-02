#ifndef _RSP_H_
#define _RSP_H_

#include <ultra64.h>
#include <sched.h>

/** A frame's graphics task, and the framebuffer it will draw into */
struct RspGfxTask
{
    OSScTask task;
    void *framebuffer; /* set before rspGfxTaskStart (fr.c) */
};

/** The task the next rspGfxTaskStart sends */
extern struct RspGfxTask *g_RspNextTask;

#ifdef UCODE_F3DEX3
/*
 * The LookAt Fast3D starts with (from its DMEM data): X along y, Y along x. Environment
 * mapped models drawn without a gSPLookAt of their own (the Nintendo and Rare logos)
 * relied on it, and F3DEX3 has none.
 */
extern LookAt g_Fast3dDefaultLookAt;

/*
 * F3DEX3's performance counters (its docs/Code/Counters.md). Each build of the
 * microcode counts a different set, left in the end of the yield buffer when a
 * task finishes. The fps counter picks the build (g_RspProfileMode) and shows
 * the counts, averaged per task.
 */
enum RspProfileMode
{
    RSP_PROFILE_DEFAULT, /* F3DEX3_BrZ_NOC, the normal one: vertices, triangles, rects, FIFO stalls */
    RSP_PROFILE_A,       /* _PA: vertex and triangle cycles, FIFO stalls, DL commands */
    RSP_PROFILE_B,       /* _PB: lit vertices, clipped triangles, overlay loads */
    RSP_PROFILE_COUNT
};

/* Counter totals, per mode */
enum RspCounter
{
    /* RSP_PROFILE_DEFAULT */
    RSPCOUNT_VERTICES = 0,
    RSPCOUNT_TRIS_IN,
    RSPCOUNT_TRIS_OUT,
    RSPCOUNT_RECTS,
    RSPCOUNT_FIFO_STALL_CYCLES,

    /* RSP_PROFILE_A */
    RSPCOUNT_VTX_CYCLES = 0,
    RSPCOUNT_TRI_CYCLES,
    RSPCOUNT_A_FIFO_STALL_CYCLES,
    RSPCOUNT_DL_COMMANDS,

    /* RSP_PROFILE_B */
    RSPCOUNT_B_VERTICES = 0,
    RSPCOUNT_LIT_VERTICES,
    RSPCOUNT_CLIPPED_TRIS,
    RSPCOUNT_OVERLAY_LOADS,

    RSPCOUNT_MAX = 5
};

extern volatile s32 g_RspProfileMode;
/* Running totals for each build, and the number of tasks they came from */
extern volatile u32 g_RspCounterSums[RSP_PROFILE_COUNT][RSPCOUNT_MAX];
extern volatile u32 g_RspCounterTasks[RSP_PROFILE_COUNT];

void rspReadCounters(OSTask *task);
#endif

void rspAllocateBuffers(void);
void rspGfxTaskStart(Gfx *firstGdl, Gfx *gdl, OSMesg replyMsg);

#endif
