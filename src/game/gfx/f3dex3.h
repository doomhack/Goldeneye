#ifndef _F3DEX3_SPIKE_H_
#define _F3DEX3_SPIKE_H_
#include <ultra64.h>

/*
 * F3DEX3 spike (throwaway, phase 0 of the F3DEX3 port plan): the game still
 * builds Fast3D display lists; when an F3DEX3 microcode is selected the whole
 * frame is translated to F3DEX3 commands just before the task starts, so the
 * same scene can be timed with either microcode.
 */

enum UcodeMode {
    UCODE_FAST3D,
    UCODE_F3DEX3_NOC,
    UCODE_F3DEX3_NOC_PA,
    UCODE_F3DEX3_NOC_PB,
    UCODE_COUNT
};

/* F3DEX3's counters, left in the end of the yield buffer after each task */
typedef struct {
    union {
        struct {
            u32 vertexProcCycles;
            u16 fetchedDLCommandCount;
            u16 dlCommandCount;
            u32 stallRDPFifoFullCycles;
            u32 triProcCycles;
        } a;
        struct {
            u16 vertexCount;
            u16 litVertexCount;
            u32 occlusionPlaneCullCount : 18;
            u32 clippedTriCount : 14;
            u32 allOverlayLoadCount : 18;
            u32 lightingOverlayLoadCount : 14;
            u32 clippingOverlayLoadCount : 18;
            u32 miscOverlayLoadCount : 14;
        } b;
        u64 dummy_alignment[2];
    };
    u32 taskdataptr;
    u32 ucode;
} F3DEX3YieldDataFooter;

extern volatile s32 g_UcodeMode;

/* Perf probe (fps counter on, P1 holds L): D-pad up = no sky, left = no props/chrs,
   right = no second view world, down = next microcode */
#define PERFSKIP_SKY    0x1
#define PERFSKIP_PROPS  0x4
#define PERFSKIP_VIEW2  0x8
extern s32 g_PerfSkip;
extern volatile s32 g_XlDebug;
extern s32 g_F3dex3Unsupported;   /* Fast3D commands the translator could not convert, last frame */
extern volatile s32 g_F3dex3LastOp;        /* the last one of those */
extern volatile F3DEX3YieldDataFooter g_F3dex3Counters; /* last finished task's counters */
extern volatile u32 g_F3dex3CounterSum[4];               /* summed per task, for the fps counter */
extern volatile u32 g_F3dex3CounterTasks;

void f3dex3AllocateBuffers(void);
/* Translates the frame's Fast3D list [first, end) and returns the F3DEX3 list's start; *outEnd gets its end */
Gfx *f3dex3TranslateFrame(Gfx *first, Gfx *end, Gfx **outEnd);
const char *f3dex3ModeName(s32 mode);

#endif
