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
// Where the graphics microcode saves its state when an audio task interrupts it
#ifdef UCODE_F3DEX3
#define GFX_YIELD_DATA_SIZE 0xC00 /* what F3DEX3 needs */
#else
#define GFX_YIELD_DATA_SIZE 0xBA0
#endif

alignas(16) static u8 s_YieldBuffer[GFX_YIELD_DATA_SIZE];

static u8 *s_RdpOutputBuffer;

static struct RspGfxTask s_Tasks[2];
static s32 s_NextTask;

struct RspGfxTask *g_RspNextTask = &s_Tasks[0];

extern long long int rspbootTextStart[];
extern long long int rspbootTextEnd[];
extern long long int gsp3DTextStart[];
extern long long int gsp3DDataStart[];

#ifdef UCODE_F3DEX3
extern long long int gspF3dex3PaTextStart[];
extern long long int gspF3dex3PaDataStart[];
extern long long int gspF3dex3PbTextStart[];
extern long long int gspF3dex3PbDataStart[];

volatile s32 g_RspProfileMode = RSP_PROFILE_DEFAULT;
volatile u32 g_RspCounterSums[RSP_PROFILE_COUNT][RSPCOUNT_MAX];
volatile u32 g_RspCounterTasks[RSP_PROFILE_COUNT];

/* What F3DEX3 leaves in the end of the yield buffer */
typedef struct
{
    union
    {
        struct
        {
            u16 vertexCount;
            u16 rdpOutTriCount;
            u32 rspInTriCount : 18;
            u32 rectCount : 14;
            u32 stallRDPFifoFullCycles;
            u32 dummy;
        } def;
        struct
        {
            u32 vertexProcCycles;
            u16 fetchedDLCommandCount;
            u16 dlCommandCount;
            u32 stallRDPFifoFullCycles;
            u32 triProcCycles;
        } a;
        struct
        {
            u16 vertexCount;
            u16 litVertexCount;
            u32 occlusionPlaneCullCount : 18;
            u32 clippedTriCount : 14;
            u32 allOverlayLoadCount : 18;
            u32 lightingOverlayLoadCount : 14;
            u32 clippingOverlayLoadCount : 18;
            u32 miscOverlayLoadCount : 14;
        } b;
        u64 alignment[2];
    };
    u32 taskdataptr;
    u32 ucode;
} F3dex3YieldFooter;

/**
 * Called by the scheduler when a graphics task has finished on the RSP (not
 * yielded): adds its F3DEX3 counters to the sums for the build that ran it.
 */
void rspReadCounters(OSTask *task)
{
    F3dex3YieldFooter *footer = (F3dex3YieldFooter *)(s_YieldBuffer + sizeof(s_YieldBuffer) - sizeof(F3dex3YieldFooter));
    F3dex3YieldFooter c;
    // Which build ran the task, so its counters are read the right way
    s32 mode = (task->t.ucode == (u64 *)gspF3dex3PaTextStart) ? RSP_PROFILE_A
        : (task->t.ucode == (u64 *)gspF3dex3PbTextStart) ? RSP_PROFILE_B
        : RSP_PROFILE_DEFAULT;

    osInvalDCache(footer, sizeof(*footer));
    c = *footer;
    // Out of the cache again, or a later write-back could overwrite newer counts
    osInvalDCache(footer, sizeof(*footer));

    volatile u32 *sums = g_RspCounterSums[mode];

    switch (mode)
    {
        case RSP_PROFILE_A:
            sums[RSPCOUNT_VTX_CYCLES] += c.a.vertexProcCycles;
            sums[RSPCOUNT_TRI_CYCLES] += c.a.triProcCycles;
            sums[RSPCOUNT_A_FIFO_STALL_CYCLES] += c.a.stallRDPFifoFullCycles;
            sums[RSPCOUNT_DL_COMMANDS] += c.a.dlCommandCount;
            break;
        case RSP_PROFILE_B:
            sums[RSPCOUNT_B_VERTICES] += c.b.vertexCount;
            sums[RSPCOUNT_LIT_VERTICES] += c.b.litVertexCount;
            sums[RSPCOUNT_CLIPPED_TRIS] += c.b.clippedTriCount;
            sums[RSPCOUNT_OVERLAY_LOADS] += c.b.allOverlayLoadCount;
            break;
        default:
            sums[RSPCOUNT_VERTICES] += c.def.vertexCount;
            sums[RSPCOUNT_TRIS_IN] += c.def.rspInTriCount;
            sums[RSPCOUNT_TRIS_OUT] += c.def.rdpOutTriCount;
            sums[RSPCOUNT_RECTS] += c.def.rectCount;
            sums[RSPCOUNT_FIFO_STALL_CYCLES] += c.def.stallRDPFifoFullCycles;
            break;
    }

    g_RspCounterTasks[mode]++;
}
#endif

#ifdef UCODE_F3DEX3
LookAt g_Fast3dDefaultLookAt = {
    .l = { { .l = { .dir = { 0, 127, 0 } } }, { .l = { .dir = { 127, 0, 0 } } } },
};
#endif

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

#ifdef UCODE_F3DEX3
    if (g_RspProfileMode == RSP_PROFILE_A)
    {
        task->t.ucode = (u64 *)gspF3dex3PaTextStart;
        task->t.ucode_data = (u64 *)gspF3dex3PaDataStart;
    }
    else if (g_RspProfileMode == RSP_PROFILE_B)
    {
        task->t.ucode = (u64 *)gspF3dex3PbTextStart;
        task->t.ucode_data = (u64 *)gspF3dex3PbDataStart;
    }
#endif
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
