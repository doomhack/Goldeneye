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

void rspAllocateBuffers(void);
void rspGfxTaskStart(Gfx *firstGdl, Gfx *gdl, OSMesg replyMsg);

#endif
