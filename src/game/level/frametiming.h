#ifndef _UNK_0C0A70_H_
#define _UNK_0C0A70_H_
#include <ultra64.h>

extern s32 g_FrameCounterPrev;
extern s32 g_FrameCounter;
extern s32 g_FrameDelta;

#if defined (BUGFIX_R1)
extern f32 g_FrameDeltaF;
extern f32 g_FrameDelta60;
#endif

extern s32 g_FrameHalfCounterPrev;
extern s32 g_FrameHalfCounter;
extern s32 g_FrameCounterOdd;
extern s32 g_FrameHalfDelta;
extern u32 g_FramePrevCount;
extern u32 g_FrameStartCount;
extern s32 g_FrameMinFrames;

void frameWaitForNext(void);

void frameTimingStart(void);
void frameAdvance(s32 deltaFrames);

#endif
