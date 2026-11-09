/**
 * The frame timer. frameWaitForNext waits until at least g_FrameMinFrames video frames
 * have passed since the last game frame, then frameAdvance records how many 60 Hz game
 * ticks that was in g_FrameDelta. lv.c turns that into g_ClockTimer.
 *
 * On NTSC a retrace is one tick. On PAL a retrace is 1.2 ticks: the whole ticks go to
 * this frame and the fraction carries to the next, so a 25 fps PAL game gets deltas of
 * 2, 3, 2, 3, 2 (12 ticks every 10 retraces).
 */

#include <ultra64.h>
#include <bondconstants.h>
#include "game/level/frametiming.h"
#include "sched.h"

// data
s32 g_FrameCounterPrev = -1;
s32 g_FrameCounter = 0;

/**
 * Appears to be rendered framerate, or some kind of counter since the last frame update.
 */
s32 g_FrameDelta = 1;

// EU address D_8004111C
f32 g_FrameDeltaF = 1.0f;

// EU address D_80041120
f32 g_FrameDelta60 = 1.0f;

s32 g_FrameHalfCounterPrev = -1;
s32 g_FrameHalfCounter = 0; // half of g_FrameCounter
s32 g_FrameCounterOdd = 0; // is g_FrameCounter Odd
s32 g_FrameHalfDelta = 0; // half - g_FrameHalfCounterPrev
u32 g_FramePrevCount = 0;
u32 g_FrameStartCount = 0;
s32 g_FrameMinFrames = 1; //usually 1

/*
 * The scheduler's retrace count when the current game frame started. Frames
 * are counted in whole video frames from this, so a game frame that starts
 * part way through a video frame (triple buffering) neither gains nor loses
 * game time: the deltas add up to the retraces that really happened.
 */
static u32 s_FrameStartRetrace;

#if VI_HZ != TICKS_PER_SECOND
/* The part of a tick carried to the next frame, in 1/VI_HZ ticks */
static u32 s_TickRemainder;
#endif

static u32 frameGetRetraceCount(void)
{
    return *(volatile u32 *)&os_scheduler.frameCount;
}

/**
 * The number of retraces since the current game frame started.
 */
s32 frameRetracesSinceStart(void)
{
    return (s32)(frameGetRetraceCount() - s_FrameStartRetrace);
}


/**
 * Stores the current OS count in the two global variables.
 */
void frameTimingStart(void)
{
    g_FrameStartCount = osGetCount();
    g_FramePrevCount = g_FrameStartCount;
    s_FrameStartRetrace = frameGetRetraceCount();
}


/**
 * Updates the timing-related counters and frame information based on the given argument.
 *
 * @param deltaFrames The number of frames to add to the current frame counter.
 */
void frameAdvance(s32 deltaFrames)
{
    g_FramePrevCount = (s32) g_FrameStartCount;
    g_FrameStartCount = osGetCount();
    s_FrameStartRetrace = frameGetRetraceCount();

    g_FrameCounterPrev = g_FrameCounter;
    g_FrameCounter = (s32) (g_FrameCounter + deltaFrames);
    g_FrameDelta = deltaFrames;

    g_FrameDeltaF = (f32) deltaFrames;
    g_FrameDelta60 = g_FrameDeltaF;

    g_FrameHalfCounterPrev = (s32) g_FrameHalfCounter;
    g_FrameHalfCounter = (s32) (g_FrameCounter / 2);
    g_FrameCounterOdd = (s32) (g_FrameCounter & 1);
    g_FrameHalfDelta = (s32) (g_FrameHalfCounter - g_FrameHalfCounterPrev);
}


/**
 * Converts retraces to whole 60 Hz ticks, carrying the fraction to the next call.
 */
static s32 frameRetracesToTicks(s32 retraces)
{
#if VI_HZ == TICKS_PER_SECOND
    return retraces;
#else
    s32 ticks;

    s_TickRemainder += (u32)retraces * TICKS_PER_SECOND;
    ticks = s_TickRemainder / VI_HZ;
    s_TickRemainder -= ticks * VI_HZ;

    return ticks;
#endif
}


/**
 * Waits until at least g_FrameMinFrames retraces have passed since the last
 * game frame started, then starts the next one with that many retraces as its
 * delta. The main loop only gets here once a retrace has passed, so this
 * normally doesn't wait.
 */
void frameWaitForNext(void)
{
    s32 retraces;

    do {
        retraces = frameRetracesSinceStart();
    } while (retraces < g_FrameMinFrames);

    g_FrameMinFrames = 1;
    frameAdvance(frameRetracesToTicks(retraces));
}
