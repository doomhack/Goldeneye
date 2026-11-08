#include <ultra64.h>
#include <PR/os.h>
#include "init.h"
#include "sched.h"
#include <bondgame.h>
#include "crash.h"
#include "fr.h"
#include "joy.h"
#include "music.h"
#include "thread_config.h"
#include "game/gfx/rsp.h"

s32 __scSchedule(OSSched *sc, OSScTask **sp, OSScTask **dp, s32 availRCP);
s32 __scTaskComplete(const OSScTask *t);

/**
 * EU .data, offset from start of data_seg : 0x2300
*/

/**
 * @file sched.c
 * This file contains the scheduler. 
 * 
 * In particular, it:
 *   - is based on but modified by rare from code included in sdk
 *   - 
 *   - 
 */

#define VIDEO_MSG       666
#define RSP_DONE_MSG    667
#define RDP_DONE_MSG    668
#define PRE_NMI_MSG     669
#define TASK_MSG        670 // a task was put on cmdQ: start it now, not at the next retrace


#define OS_SC_DP                0x0001
#define OS_SC_SP                0x0002
#define OS_SC_YIELD             0x0010
#define OS_SC_YIELDED           0x0020


#define OS_SC_XBUS      (OS_SC_SP | OS_SC_DP)
#define OS_SC_DRAM      (OS_SC_SP | OS_SC_DP | OS_SC_DRAM_DLIST)
#define OS_SC_DP_XBUS   (OS_SC_SP)
#define OS_SC_DP_DRAM   (OS_SC_SP | OS_SC_DRAM_DLIST)
#define OS_SC_SP_XBUS   (OS_SC_DP)
#define OS_SC_SP_DRAM   (OS_SC_DP | OS_SC_DRAM_DLIST)

// forward declarations
void __scHandleRetrace(OSSched *sc);
void __scHandleRSP(OSSched *sc);
void __scHandleRDP(OSSched *sc);
static void __scHandleNewTasks(OSSched *sc);
static void __scSwap(void *framebuffer);
void __scMain(void *arg);

u32 stderr_enabled = 0;
u32 stderr_active = 0;
u32 stderr_permitted = 0;

/**
 * EU .data, offset from start of data_seg : 0x2310
*/
u32 userCompareValue = 45000000;
u32 currentcount = 0;
u32 dp_busy = 0;
u32 dpCount = 0;
s32 g_schedViCurrentFrameBuffer = 0;
f32 g_ViXScales[NUM_VIDEO_FRAME_BUFFERS] = {1.0, 1.0, 1.0};
f32 g_ViYScales[NUM_VIDEO_FRAME_BUFFERS] = {1.0, 1.0, 1.0};
s32 g_ViChangeVideoModes[NUM_VIDEO_FRAME_BUFFERS] = {0, 0, 0}; // boolean

/*
 * Triple buffering. The VI shows one framebuffer and may have a second one
 * waiting for the next retrace (osViSwapBuffer). A finished frame that comes in
 * while a swap is still waiting is held here and handed to the VI at the next
 * retrace, so the RDP can go straight on to the third buffer instead of
 * waiting for the VI.
 */
static void *s_SwapHeldFramebuffer;

OSSched os_scheduler;
OSScClient gfxClient;
u32 g_DisplayPerformanceCounters[4]; // clock, cmc, pipe, tmem

/**
 * EU .bss 0x80051f00.
*/
OSViMode g_ViModes[NUM_VIDEO_FRAME_BUFFERS];
OSViMode *g_ViModePtrs[NUM_VIDEO_FRAME_BUFFERS];

/**
 * A0-> stderr.permitted	[8002309C]; fry AT
 */
void permit_stderr(u32 flag)
{
	stderr_permitted = flag;
}

/**
 * test to display stderr and update Count
 */
void CheckDisplayErrorBuffer(const u16 *buffer)
{
	if ((stderr_permitted && stderr_active) || stderr_enabled )
    {
		crashRenderFrame(buffer);
		currentcount = osGetCount();
	}
}

/**
 * test to display stderr every 16th frame
 */
void CheckDisplayErrorBufferEvery16Frames(u32 framecount)
{
	if (!(framecount & 0xf))
    {
		if ((stderr_permitted && stderr_active) || stderr_enabled)
        {
			if (userCompareValue < (osGetCount() - currentcount))
            {
				s32 i;

				for (i = 0; i < NUM_VIDEO_FRAME_BUFFERS; i++)
				{
					crashRenderFrame((u16*)cfb_16[i]);
				}
			}
		}
	}
}

// Remembers when the scheduler started, for the error buffer timing above
static void schedRecordStartCount(void)
{
	currentcount=osGetCount();
}

void osCreateScheduler (OSSched * sc, void * stack, u8 mode, u32 numFields)
{
    sc->curRSPTask = 0;
    sc->curRDPTask = 0;
    sc->clientList = 0;
    sc->frameCount = 0;
    sc->audioListHead = 0;
    sc->gfxListHead = 0;
    sc->audioListTail = 0;
    sc->gfxListTail = 0;
    sc->retraceMsg.type = OS_SC_RETRACE_MSG;
    sc->prenmiMsg.type = OS_SC_PRE_NMI_MSG;
    sc->thread = stack;
    osCreateMesgQueue(&sc->interruptQ, sc->intBuf, OS_SC_MAX_MESGS);
    osCreateMesgQueue(&sc->cmdQ, sc->cmdMsgBuf, OS_SC_MAX_MESGS);
    osCreateViManager(OS_PRIORITY_VIMGR);
    viMode = &osViModeTable[mode];
    g_viOriginalHstart = viMode->comRegs.hStart;
    g_viOriginalVstart0 = viMode->fldRegs[0].vStart;
    g_viOriginalVstart1 = viMode->fldRegs[1].vStart;
    osSetEventMesg(OS_EVENT_SP, &sc->interruptQ, (OSMesg)RSP_DONE_MSG); 
    osSetEventMesg(OS_EVENT_DP, &sc->interruptQ, (OSMesg)RDP_DONE_MSG);
    osSetEventMesg(OS_EVENT_PRENMI, &sc->interruptQ, (OSMesg)PRE_NMI_MSG);
    osViSetEvent(&sc->interruptQ, (OSMesg)VIDEO_MSG, numFields);
    schedRecordStartCount();
    osCreateThread(sc->thread, SCHED_THREAD_ID, &__scMain, sc, setSPToEnd(sp_shed, sizeof(sp_shed)), SCHED_THREAD_PRIORITY);
    osStartThread(sc->thread);
}

void osScAddClient(OSSched *sc, OSScClient *c, OSMesgQueue *msgQ, s32 halfRate)
{
    OSIntMask mask;

    mask = osSetIntMask(OS_IM_NONE);

    c->msgQ = msgQ;
    c->halfRate = halfRate;
    c->next = sc->clientList;
    sc->clientList = c;

    osSetIntMask(mask);
}


/**
 * V0= A0+78
 */
OSMesgQueue *osScGetCmdQ(OSSched *sc)
{
    return &sc->cmdQ;
}

/**
 * Based on libultra\sched\sched.c. In particular, unlike the other scheduler methods,
 * the arg is actually void* instead of OSSched *sc.
 */
void __scMain(void *arg)
{
    OSMesg msg = NULL;
    OSSched *sc = (OSSched *)arg;
    OSScClient *client;
    s32 done = FALSE;

    while (!done)
    {        
        osRecvMesg(&sc->interruptQ, &msg, OS_MESG_BLOCK);

        switch ((s32)msg)
        {
            case VIDEO_MSG:
                __scHandleRetrace(sc);
                break;

            case RSP_DONE_MSG:
                __scHandleRSP(sc);
                break;

            case RDP_DONE_MSG:
                __scHandleRDP(sc);
                break;

            case TASK_MSG:
                __scHandleNewTasks(sc);
                break;

            case PRE_NMI_MSG:
            do {
                joyRumblePakStop();

                for (client = sc->clientList; (client != NULL); client = client->next)
                {
                      osSendMesg(client->msgQ, (OSMesg)&sc->prenmiMsg, OS_MESG_NOBLOCK);
                }
                done = TRUE;
                break;
            } while(0);

            default:
                break;
        }
    }

    if (osTvType == OS_TV_MPAL)
    {
        osViSetMode(&osViModeTable[OS_VI_MPAL_LAN1]);
    }
    else
    {
        osViSetMode(&osViModeTable[OS_VI_NTSC_LAN1]);
    }

    osViSetXScale(1.0f);
    osViSetYScale(1.0f);
    osViRepeatLine(0);
    osViBlack(TRUE);

    while (TRUE)
    {
        do
        {
            osRecvMesg(&sc->interruptQ, &msg, OS_MESG_BLOCK);
        } while((s32)msg != VIDEO_MSG);

        joyPoll();
    }
}

void __scHandleRetrace(OSSched *sc) {
    OSScTask    *rspTask = 0;    
    OSScClient  *client;
    s32         state;
    OSScTask    *sp = 0;
    OSScTask    *dp = 0;
    sc->frameCount++;
    viVsyncRelated();
    joyPoll();
    musicFadeTick();

    // The VI has taken the buffer it was waiting for, so a held frame can go next
    if (s_SwapHeldFramebuffer != NULL && osViGetCurrentFramebuffer() == osViGetNextFramebuffer()) {
        void *fb = s_SwapHeldFramebuffer;

        s_SwapHeldFramebuffer = NULL;
        __scSwap(fb);
    }

    while (osRecvMesg(&sc->cmdQ, (OSMesg*)&rspTask, OS_MESG_NOBLOCK) != -1) {
        __scAppendList(sc, rspTask);
    }
    if (sc->doAudio && sc->curRSPTask) {
        __scYield(sc);
    } else {
        state = ((sc->curRSPTask == 0) << 1) | (sc->curRDPTask == 0);
        if ( __scSchedule (sc, &sp, &dp, state) != state)
            __scExec(sc, sp, dp);
    }
    for (client = sc->clientList; client != 0; client = client->next) {
        if (!client->halfRate || (sc->frameCount & 1) == 0) {
            osSendMesg(client->msgQ, (OSMesg) &sc->retraceMsg, OS_MESG_NOBLOCK);
        }
    }
    CheckDisplayErrorBufferEvery16Frames(sc->frameCount);
}

/*
 * The main thread has put a graphics task on cmdQ. Start it now if the RCP is
 * free, instead of leaving it until the next retrace.
 */
static void __scHandleNewTasks(OSSched *sc)
{
    OSScTask *task = NULL;
    OSScTask *sp = NULL;
    OSScTask *dp = NULL;
    s32 state;

    while (osRecvMesg(&sc->cmdQ, (OSMesg*)&task, OS_MESG_NOBLOCK) != -1) {
        __scAppendList(sc, task);
    }
    if (sc->doAudio && sc->curRSPTask) {
        __scYield(sc);
    } else {
        state = ((sc->curRSPTask == 0) << 1) | (sc->curRDPTask == 0);
        if (__scSchedule(sc, &sp, &dp, state) != state)
            __scExec(sc, sp, dp);
    }
}

void schedKickTasks(void)
{
    osSendMesg(&os_scheduler.interruptQ, (OSMesg)TASK_MSG, OS_MESG_NOBLOCK);
}

/*
 * Frame timing shown by the fps counter: when the current graphics task
 * started, and the OS counts its RSP and RDP work took, summed over tasks.
 * The counter samples the sums once a second.
 */
volatile u32 g_PerfGfxStart;
volatile u32 g_PerfRspSum;
volatile u32 g_PerfRdpSum;
volatile u32 g_PerfGfxTasks;
/* The RDP's own counters (RCP cycles), summed over graphics tasks: busy with
   commands, with the pixel pipeline, and with texture memory (TMEM) loads */
volatile u32 g_PerfRdpCmdSum;
volatile u32 g_PerfRdpPipeSum;
volatile u32 g_PerfRdpTmemSum;

void __scHandleRSP(OSSched *sc) {
    OSScTask *t, *sp = 0, *dp = 0;
    s32 state;
    t = sc->curRSPTask;
    sc->curRSPTask = 0;

    if ((t->state & OS_SC_YIELD) && osSpTaskYielded(&t->list)) {
        t->state |= OS_SC_YIELDED;
        if ((t->flags & OS_SC_TYPE_MASK) == OS_SC_XBUS) {
            t->next = sc->gfxListHead;
            sc->gfxListHead = t;
            if (sc->gfxListTail == 0) {
                sc->gfxListTail = t;
            }
        }
    } else {
        if (t->list.t.type != M_AUDTASK)
        {
            g_PerfRspSum += osGetCount() - g_PerfGfxStart;
            rspReadCounters(&t->list);
        }

        t->state &= ~OS_SC_NEEDS_RSP;
        __scTaskComplete(t);
    }
    state = ((sc->curRSPTask == 0) << 1) | (sc->curRDPTask == 0);
    if (__scSchedule(sc, &sp, &dp, state) != state) {
        __scExec(sc, sp, dp);
    }
}

void __scHandleRDP(OSSched *sc)
{
    OSScTask *t, *sp = NULL, *dp = NULL; 
    s32 state;
    if (sc->curRDPTask != NULL) {
        osDpGetCounters(g_DisplayPerformanceCounters);
        g_PerfRdpSum += osGetCount() - g_PerfGfxStart;
        g_PerfRdpCmdSum += g_DisplayPerformanceCounters[1];
        g_PerfRdpPipeSum += g_DisplayPerformanceCounters[2];
        g_PerfRdpTmemSum += g_DisplayPerformanceCounters[3];
        g_PerfGfxTasks++;
        t = sc->curRDPTask;
        sc->curRDPTask = NULL;
        t->state &= ~OS_SC_NEEDS_RDP;
        __scTaskComplete(t);
        state = ((sc->curRSPTask == 0) << 1) | (sc->curRDPTask == 0);
        if (__scSchedule(sc, &sp, &dp, state) != state) {
            __scExec(sc, sp, dp);
        }
    }
}

/*
 * A graphics task can start once its framebuffer is neither on screen, nor
 * waiting for the VI, nor held for the VI.
 */
const OSScTask *__scTaskReady(const OSScTask *t) 
{
    if (t) {
        const void *fb = t->framebuffer;

        if (fb == osViGetCurrentFramebuffer() || fb == osViGetNextFramebuffer() || fb == s_SwapHeldFramebuffer) {
            return 0;
        }

        return t;
    }

    return 0;
}

/*
 * Hands a finished frame to the VI with the video mode it was drawn for.
 * Frames reach here in the order the main thread made them, so the mode
 * slots are used in the same order viUpdateMode filled them.
 */
static void __scSwap(void *framebuffer)
{
    static int firsttime = 1;

    if (firsttime) {
        osViBlack(FALSE);
        firsttime = 0;
    }
    if (g_ViChangeVideoModes[g_schedViCurrentFrameBuffer]) {
        OSIntMask mask = osSetIntMask(OS_IM_VI);
        *g_ViModePtrs[g_schedViCurrentFrameBuffer] = g_ViModes[g_schedViCurrentFrameBuffer];
        osSetIntMask(mask);
    }
    osViSetXScale(g_ViXScales[g_schedViCurrentFrameBuffer]);
    osViSetYScale(g_ViYScales[g_schedViCurrentFrameBuffer]);
    g_schedViCurrentFrameBuffer = ((g_schedViCurrentFrameBuffer + 1) % NUM_VIDEO_FRAME_BUFFERS);
    CheckDisplayErrorBuffer(framebuffer);
    osViSwapBuffer(framebuffer);
}

s32 __scTaskComplete(const OSScTask *t)
{
    if ((t->state & OS_SC_RCP_MASK) == 0) {
        osSendMesg(t->msgQ, t->msg, OS_MESG_BLOCK);
        if (t->list.t.type == M_GFXTASK) {
            if ((t->flags & OS_SC_SWAPBUFFER) && (t->flags & OS_SC_LAST_TASK)){
                // With three buffers there is at most one frame held at a time:
                // the next task can't start while all three are taken.
                if (osViGetCurrentFramebuffer() != osViGetNextFramebuffer()) {
                    s_SwapHeldFramebuffer = t->framebuffer;
                } else {
                    __scSwap(t->framebuffer);
                }
            }
        }
        return 1;
    }    
    return 0;
}

void __scAppendList(OSSched *sc, OSScTask *t) 
{
    long type = t->list.t.type;
    
    if (type == M_AUDTASK) {
        if (sc->audioListTail)
            sc->audioListTail->next = t;
        else
            sc->audioListHead = t;
            
        sc->audioListTail = t;
        sc->doAudio = 1;
    } else {
        if (sc->gfxListTail)
            sc->gfxListTail->next = t;
        else
            sc->gfxListHead = t;
        
	sc->gfxListTail = t;
    }
    
    t->next = NULL;
    t->state = t->flags & OS_SC_RCP_MASK;    
}

void __scExec(OSSched *sc, OSScTask *sp, OSScTask *dp)
{
    if (sp)
    {
        if (sp->list.t.type == M_AUDTASK)
        {
            osWritebackDCacheAll();
        } 
        
        if ((sp->list.t.type != M_AUDTASK) && (sp->state & 0x10) == 0)
        {
            osDpSetStatus(0x3c0);
        }
        
        // A yielded graphics task resuming keeps its first start time.
        if ((sp->list.t.type != M_AUDTASK) && (sp->state & OS_SC_YIELDED) == 0)
        {
            g_PerfGfxStart = osGetCount();
        }
        sp->state &= ~(OS_SC_YIELD | OS_SC_YIELDED); 
        osSpTaskLoad(&sp->list);
        osSpTaskStartGo(&sp->list);
        sc->curRSPTask = sp;
        if (sp == dp)
        {
            sc->curRDPTask = dp;
        }
    }
    if (dp && (dp != sp))
    {
        osDpSetNextBuffer(dp->list.t.output_buff, *dp->list.t.output_buff_size);

        dp_busy = 1;
        dpCount = 0;

        sc->curRDPTask = dp;        
    }
}


void __scYield(const OSSched *sc) 
{
    if (sc->curRSPTask->list.t.type == M_GFXTASK) {
        sc->curRSPTask->state |= 0x0010;
        osSpTaskYield();
    } else {
      
    }    
}


/**
 * ???
 *	uses TLB pointers at 80028400
 *	7000167C	3
 *	70001704	2,6,7
 *	70001758	default; 1,4,5
 */
s32 __scSchedule(OSSched *sc, OSScTask **sp, OSScTask **dp, s32 availRCP) 
{
    s32 avail = availRCP;
    OSScTask *gfx = sc->gfxListHead;
    OSScTask *audio = sc->audioListHead;

    if (sc->doAudio && (avail & OS_SC_SP))
    {
        if (gfx && (gfx->flags & OS_SC_PARALLEL_TASK))
        {
            *sp = gfx;
            avail &= ~OS_SC_SP;
        } else {
            *sp = audio;
            avail &= ~OS_SC_SP;
            sc->doAudio = 0;
            sc->audioListHead = sc->audioListHead->next;
            if (sc->audioListHead == NULL)
                sc->audioListTail = NULL; 
        }        
    } else {            
        if (__scTaskReady(gfx))
        {
            switch (gfx->flags & OS_SC_TYPE_MASK)
            {
              case (OS_SC_XBUS):
                  if (gfx->state & OS_SC_YIELDED)
                  {
                      if (avail & OS_SC_SP)
                      {
                          *sp = gfx;
                          avail &= ~OS_SC_SP;
                      
                          if (gfx->state & OS_SC_DP)
                          { 
                              *dp = gfx;
                              avail &= ~OS_SC_DP;
                          }
                          sc->gfxListHead = sc->gfxListHead->next;
                          if (sc->gfxListHead == NULL)
                              sc->gfxListTail = NULL;
                      }                  
                  } else {
                      if (avail == (OS_SC_SP | OS_SC_DP))
                      {
                          *sp = *dp = gfx;
                          avail &= ~(OS_SC_SP | OS_SC_DP);
                          sc->gfxListHead = sc->gfxListHead->next;
                          if (sc->gfxListHead == NULL)
                              sc->gfxListTail = NULL;
                      }
                  }
                  break;
          
              case (OS_SC_DRAM):
              case (OS_SC_DP_DRAM):
              case (OS_SC_DP_XBUS):
                  if (gfx->state & OS_SC_SP) 
                  {
                      if (avail & OS_SC_SP)
                      {
                          *sp = gfx;
                          avail &= ~OS_SC_SP;
                      }
                  } else if (gfx->state & OS_SC_DP) {
                      if (avail & OS_SC_DP)
                      {
                          *dp = gfx;
                          avail &= ~OS_SC_DP;
                          sc->gfxListHead = sc->gfxListHead->next;
                          if (sc->gfxListHead == NULL)
                              sc->gfxListTail = NULL;
                      }
                  }
                  break;

              case (OS_SC_SP_DRAM):
              case (OS_SC_SP_XBUS):
              default:
                  break;
            }
        }
    }

    if (avail != availRCP)
        avail = __scSchedule(sc, sp, dp, avail);

    return avail;
    
}
