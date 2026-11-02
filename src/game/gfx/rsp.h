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
#endif

void rspAllocateBuffers(void);
void rspGfxTaskStart(Gfx *firstGdl, Gfx *gdl, OSMesg replyMsg);

#endif
