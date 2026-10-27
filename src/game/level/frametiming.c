/**
 * The frame timer. frameWaitForNext waits until at least g_FrameMinFrames video frames
 * have passed since the last game frame, using the CPU counter, then frameAdvance records
 * how many it was in g_FrameDelta (1 at full speed). lv.c turns that into g_ClockTimer.
 */

#include <ultra64.h>
#include "game/level/frametiming.h"

// data
s32 g_FrameCounterPrev = -1;
s32 g_FrameCounter = 0;

/**
 * Appears to be rendered framerate, or some kind of counter since the last frame update.
 */
s32 g_FrameDelta = 1;

#if defined(BUGFIX_R1)
// EU address D_8004111C
f32 g_FrameDeltaF = 1.0f;

// EU address D_80041120
f32 g_FrameDelta60 = 1.0f;
#endif

s32 g_FrameHalfCounterPrev = -1;
s32 g_FrameHalfCounter = 0; // half of g_FrameCounter
s32 g_FrameCounterOdd = 0; // is g_FrameCounter Odd
s32 g_FrameHalfDelta = 0; // half - g_FrameHalfCounterPrev
u32 g_FramePrevCount = 0;
u32 g_FrameStartCount = 0;
s32 g_FrameMinFrames = 1; //usually 1



/**
 * Stores the current OS count in the two global variables.
 */
void frameTimingStart(void)
{
    g_FrameStartCount = osGetCount();
    g_FramePrevCount = g_FrameStartCount;
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

    g_FrameCounterPrev = g_FrameCounter;
    g_FrameCounter = (s32) (g_FrameCounter + deltaFrames);
    g_FrameDelta = deltaFrames;

    #ifdef BUGFIX_R1
    g_FrameDeltaF = (f32) deltaFrames;
    #ifdef REFRESH_PAL
    g_FrameDelta60 = (g_FrameDeltaF * 60.0f) / 50.0f;
    #else
    g_FrameDelta60 = (f32) g_FrameDeltaF;
    #endif
    #endif

    g_FrameHalfCounterPrev = (s32) g_FrameHalfCounter;
    g_FrameHalfCounter = (s32) (g_FrameCounter / 2);
    g_FrameCounterOdd = (s32) (g_FrameCounter & 1);
    g_FrameHalfDelta = (s32) (g_FrameHalfCounter - g_FrameHalfCounterPrev);
}


/**
 * Waits until the appropriate time has passed before updating the frame counters.
 * This function effectively controls the frame rate by waiting for the next tick.
 */
void frameWaitForNext(void) //maybe WaitForTick
{
  u32 nextFrameTime; //next frame time?
  
  do {
    #ifdef REFRESH_PAL
    nextFrameTime = ((osGetCount() - g_FrameStartCount) + 465525) / 931050; 
    #else
    nextFrameTime = ((osGetCount() - g_FrameStartCount) + 387937) / 775875; //current time + 1/5
    #endif
  } while (nextFrameTime < g_FrameMinFrames);

  g_FrameMinFrames = 1;
  frameAdvance(nextFrameTime);
}


void frameSetMinFrames(s32 arg0) {
    #ifdef LEFTOVERDEBUG
    g_FrameMinFrames = arg0;
    #endif
}

#ifdef VERSION_EU
void eu_sub_7f0c00a4(void)
{
  
}
#endif




