#include <ultra64.h>
#include <PR/os.h>
#include <PR/os_internal.h>
#include <PR/rcp.h>
#include <bondgame.h>
#include "fr.h"
#include "dbgscreen.h"

/**
 * A crash screen that works on real hardware without the RDP.
 *
 * Text is drawn by the CPU straight into cfb_16[0] through uncached memory, and
 * the VI is programmed by hand with interrupts off, so it shows up even when
 * the RCP is wedged or the scheduler has stopped.
 */

#define DBG_COLS (SCREEN_WIDTH / 4)
#define DBG_ROWS (SCREEN_HEIGHT / 8)
#define DBG_LEFT 4

extern u32 g_DebugOutputBitmaps[];
extern OSThread *__osActiveQueue;

volatile u32 g_DbgFrameCounter = 0;

static s32 dbgRow;
static s32 dbgCol;

static void dbgPutChar(char c)
{
    u16 *fb;
    u32 bitmap;
    s32 x;
    s32 y;

    if (c == '\n')
    {
        dbgRow++;
        dbgCol = 0;
        return;
    }

    if ((dbgRow >= DBG_ROWS) || (dbgCol >= DBG_COLS - (DBG_LEFT * 2)))
    {
        return;
    }

    if ((c >= ' ') && (c <= '~'))
    {
        fb = (u16 *) K0_TO_K1(cfb_16[0]);
        fb += ((dbgRow * 8) + 4) * SCREEN_WIDTH + (DBG_LEFT + dbgCol) * 4;
        bitmap = g_DebugOutputBitmaps[c - ' '];

        for (y = 0; y < 7; y++)
        {
            for (x = 0; x < 4; x++)
            {
                fb[x] = (bitmap & (1 << 31)) ? GPACK_RGBA5551(255, 255, 255, 1) : GPACK_RGBA5551(0, 0, 96, 1);
                bitmap <<= 1;
            }

            fb += SCREEN_WIDTH;
        }
    }

    dbgCol++;
}

static void dbgPuts(const char *s)
{
    while (*s)
    {
        dbgPutChar(*s++);
    }
}

static void dbgHex(u32 value)
{
    s32 i;
    s32 digit;

    for (i = 28; i >= 0; i -= 4)
    {
        digit = (value >> i) & 0xF;
        dbgPutChar((digit < 10) ? ('0' + digit) : ('a' + digit - 10));
    }
}

static void dbgLabelHex(const char *label, u32 value)
{
    dbgPuts(label);
    dbgHex(value);
    dbgPutChar(' ');
}

static s32 dbgIsCodeAddress(u32 addr)
{
    if ((addr >= (u32) &_codeSegmentStart) && (addr < (u32) &_codeSegmentEnd))
    {
        return TRUE;
    }

    return (addr >= (u32) &_gameSegmentStart) && (addr < (u32) &_gameSegmentEnd);
}

/**
 * Print the code addresses found on a stack, starting at `sp`: a rough backtrace.
 */
static void dbgStackScanFrom(u32 *sp)
{
    s32 found = 0;
    s32 i;

    if (((u32) sp < 0x80000400) || ((u32) sp >= 0x80800000 - 0x400))
    {
        return;
    }

    dbgPuts("  stack:");

    for (i = 0; (i < 0x100) && (found < 24); i++)
    {
        if (dbgIsCodeAddress(sp[i]))
        {
            if ((found % 6) == 0)
            {
                dbgPuts("\n   ");
            }

            dbgPutChar(' ');
            dbgHex(sp[i]);
            found++;
        }
    }

    dbgPutChar('\n');
}

static void dbgStackScan(OSThread *t)
{
    dbgStackScanFrom((u32 *)(u32)t->context.sp);
}

static void dbgThread(OSThread *t)
{
    dbgLabelHex("id", t->id);
    dbgLabelHex("st", t->state);
    dbgLabelHex("pc", t->context.pc);
    dbgLabelHex("ra", t->context.ra);
    dbgLabelHex("sp", t->context.sp);
    dbgPutChar('\n');
}

static void dbgBegin(const char *title)
{
    u16 *fb = (u16 *) K0_TO_K1(cfb_16[0]);
    s32 i;

    // Stop everything else from touching the screen or the VI
    osSetIntMask(OS_IM_NONE);

    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
    {
        fb[i] = GPACK_RGBA5551(0, 0, 96, 1);
    }

    dbgRow = 0;
    dbgCol = 0;
    dbgPuts(title);
    dbgPutChar('\n');
}

[[noreturn]] static void dbgShowScreen(void)
{
    OSViMode *mode = &osViModeTable[(osTvType == OS_TV_MPAL) ? OS_VI_MPAL_LAN1 : OS_VI_NTSC_LAN1];

    IO_WRITE(VI_CONTROL_REG, mode->comRegs.ctrl);
    IO_WRITE(VI_WIDTH_REG, SCREEN_WIDTH);
    IO_WRITE(VI_BURST_REG, mode->comRegs.burst);
    IO_WRITE(VI_V_SYNC_REG, mode->comRegs.vSync);
    IO_WRITE(VI_H_SYNC_REG, mode->comRegs.hSync);
    IO_WRITE(VI_LEAP_REG, mode->comRegs.leap);
    IO_WRITE(VI_H_START_REG, mode->comRegs.hStart);
    IO_WRITE(VI_X_SCALE_REG, (SCREEN_WIDTH * 0x400) / 640);
    IO_WRITE(VI_V_START_REG, mode->fldRegs[0].vStart);
    IO_WRITE(VI_V_BURST_REG, mode->fldRegs[0].vBurst);
    IO_WRITE(VI_INTR_REG, mode->fldRegs[0].vIntr);
    IO_WRITE(VI_Y_SCALE_REG, mode->fldRegs[0].yScale);
    IO_WRITE(VI_ORIGIN_REG, osVirtualToPhysical(cfb_16[0]) + (SCREEN_WIDTH * 2));

    while (1)
    {
    }
}

void dbgShowFault(OSThread *t)
{
    dbgBegin("CPU EXCEPTION");
    dbgThread(t);
    dbgLabelHex("cause", t->context.cause);
    dbgLabelHex("badvaddr", t->context.badvaddr);
    dbgPutChar('\n');
    dbgStackScan(t);
    dbgShowScreen();
}

void dbgShowHang(u32 seconds)
{
    OSThread *t;
    s32 n = 0;

    dbgBegin("HANG: no frame for a few seconds");
    dbgLabelHex("frames", g_DbgFrameCounter);
    dbgLabelHex("secs", seconds);
    dbgLabelHex("dpc", IO_READ(DPC_STATUS_REG));
    dbgLabelHex("sp", IO_READ(SP_STATUS_REG));
    dbgPutChar('\n');
    dbgLabelHex("dpc start", IO_READ(DPC_START_REG));
    dbgLabelHex("cur", IO_READ(DPC_CURRENT_REG));
    dbgLabelHex("end", IO_READ(DPC_END_REG));
    dbgPutChar('\n');

    for (t = __osActiveQueue; (t != NULL) && (t->priority != -1) && (n < 12); t = t->tlnext, n++)
    {
        dbgThread(t);

        // The main game thread
        if (t->id == 3)
        {
            dbgStackScan(t);
        }
    }

    dbgShowScreen();
}

/**
 * Stops the game with `message` and `value` on screen, for errors the game can't recover
 * from (out of memory, for example). Works from any thread.
 */
void dbgFatal(const char *message, u32 value)
{
    dbgBegin("FATAL ERROR");
    dbgPuts(message);
    dbgPutChar(' ');
    dbgHex(value);
    dbgPutChar('\n');
    dbgLabelHex("called from", (u32) __builtin_return_address(0));
    dbgPutChar('\n');
    dbgStackScanFrom(__builtin_frame_address(0));
    dbgShowScreen();
}
