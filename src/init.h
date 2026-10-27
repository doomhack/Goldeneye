#ifndef _INIT_H_
#define _INIT_H_
#include <ultra64.h>

extern OSThread rmonThread;
extern OSThread idleThread;
extern OSThread mainThread;
extern OSThread shedThread;
extern OSMesgQueue gfxFrameMsgQ;
extern OSMesg gfxFrameMsgBuf[32];
extern OSMesgQueue *sched_cmdQ;


typedef struct
{
    unsigned int inst1;
    unsigned int inst2;
    unsigned int inst3;
    unsigned int inst4;
} __exceptionVector;

void init(void);
void *setSPToEnd(u8 *stack, u32 size);

#endif
