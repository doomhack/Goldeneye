/**
 * Starting the RSP graphics task each frame: the microcode, its DRAM stack, the yield
 * buffer and the RDP output buffer.
 *
 * There are two tasks, used in turn, so the one being set up isn't the one the scheduler
 * may still be holding from the last frame.
 */

#include <ultra64.h>
#include <init.h>
#include <memp.h>
#include <sched.h>
#include <PR/sptask.h>
#include <PR/ucode.h>
#include "game/gfx/rsp.h"

#define RDP_OUTPUT_BUFFER_SIZE 0xa000

// The RSP reads and writes these by DMA: keep them on whole data cache lines.
alignas(16) static u8 s_DramStack[SP_DRAM_STACK_SIZE8];
// Where the Fast3D microcode saves its state when an audio task interrupts it
#define GFX_YIELD_DATA_SIZE 0xBA0

alignas(16) static u8 s_YieldBuffer[GFX_YIELD_DATA_SIZE];

static u8 *s_RdpOutputBuffer;

static struct RspGfxTask s_Tasks[2];
static s32 s_NextTask;

struct RspGfxTask *g_RspNextTask = &s_Tasks[0];

extern long long int rspbootTextStart[];
extern long long int rspbootTextEnd[];
extern long long int gsp3DTextStart[];
extern long long int gsp3DDataStart[];

void rspAllocateBuffers(void)
{
    s_RdpOutputBuffer = mempAlloc(RDP_OUTPUT_BUFFER_SIZE, MEMPOOL_PERMANENT);
}

/**
 * Sends the display list from `firstGdl` to `gdl` to the scheduler to draw into the
 * framebuffer in g_RspNextTask. The scheduler replies with `replyMsg` on gfxFrameMsgQ
 * when it's done.
 */
void rspGfxTaskStart(Gfx *firstGdl, Gfx *gdl, OSMesg replyMsg)
{
    OSScTask *sctask = &g_RspNextTask->task;
    OSTask *task = &sctask->list;

    task->t.type = M_GFXTASK;
    task->t.flags = OS_TASK_DP_WAIT;
    task->t.ucode_boot = (u64 *)rspbootTextStart;
    task->t.ucode_boot_size = (u8 *)rspbootTextEnd - (u8 *)rspbootTextStart;
    task->t.ucode = (u64 *)gsp3DTextStart;
    task->t.ucode_size = SP_UCODE_SIZE;
    task->t.ucode_data = (u64 *)gsp3DDataStart;
    task->t.ucode_data_size = SP_UCODE_DATA_SIZE;
    task->t.dram_stack = (u64 *)s_DramStack;
    task->t.dram_stack_size = sizeof(s_DramStack);
    task->t.output_buff = (u64 *)s_RdpOutputBuffer;
    // The microcode takes the end of the buffer here, not its size
    task->t.output_buff_size = (u64 *)(s_RdpOutputBuffer + RDP_OUTPUT_BUFFER_SIZE);
    task->t.data_ptr = (u64 *)firstGdl;
    task->t.data_size = (gdl - firstGdl) * sizeof(Gfx);
    task->t.yield_data_ptr = (u64 *)s_YieldBuffer;
    task->t.yield_data_size = sizeof(s_YieldBuffer);

    sctask->next = NULL;
    sctask->flags = OS_SC_NEEDS_RSP | OS_SC_NEEDS_RDP | OS_SC_LAST_TASK | OS_SC_SWAPBUFFER;
    sctask->msgQ = &gfxFrameMsgQ;
    sctask->msg = replyMsg;
    sctask->framebuffer = g_RspNextTask->framebuffer;

    osWritebackDCacheAll();
    osSendMesg(sched_cmdQ, (OSMesg)sctask, OS_MESG_BLOCK);

    s_NextTask ^= 1;
    g_RspNextTask = &s_Tasks[s_NextTask];
}
