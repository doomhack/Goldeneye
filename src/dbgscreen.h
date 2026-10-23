#ifndef _DBGSCREEN_H_
#define _DBGSCREEN_H_
#include <ultra64.h>

/* Counts main loop frames; the crash thread uses it to spot a hang */
extern volatile u32 g_DbgFrameCounter;

void dbgShowFault(OSThread *t);
void dbgShowHang(u32 seconds);

#endif
