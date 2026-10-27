#include <ultra64.h>
#include <PR/os.h>
#include <bondgame.h>
#include "fr.h"
#include "crash.h"
#include "rom.h"
#include <PR/R4300.h>
#include "thread_config.h"
#include "dbgscreen.h"

/**
 * @file deb_video.c
 * This file contains code to handle tlb paging. 
 * 
 * Additionally, it contains unused/removed debug code, and code for indy.
 * The function addresses seem to indicate these methods are all in the same file ...
 */

/**
 * Size in bytes.
 */
#define g_indyReadBuffer_LEN 0x60

/**
 * Copied from n64devkit\ultra\usr\src\pr\demos\fault\crash.c
 */
#define MSG_FAULT    0x10
#define MSG_WATCHDOG 0x11

#define TLB_MESSAGE_QUEUE_SIZE      1

/*
-----------------------------------------------------------------
| ADDIU     | ADD Immediate Unsigned word                       |
|-----------|---------------------------------------------------|
|001001 (9) |   rs    |   rt    |          immediate            |
------6----------5---------5-------------------16----------------
 Format:  ADDIU rt, rs, immediate
 Purpose: To add a constant to a 32-bit integer.
 Descrip: rt = rs + immediate

-----------------------------------------------------------------
| SW        | Store Word                                        |
|-----------|---------------------------------------------------|
|101011 (43)|  base   |   rt    |             offset            |
------6----------5---------5-------------------16----------------
 Format:  SW rt, offset(base)
 Purpose: To store a word to memory.
 Descrip: word[base+offset] = rt

-----------------------------------------------------------------
| JR        | Jump Register                                     |
|-----------|---------------------------------------------------|
|  000000   |   rs    |     0000 0000 0000 000      |001000 (8) |
------6----------5------------------15--------------------6------
 Format:  JR rs
 Purpose: To branch to an instruction address in a register.
*/

/**
 * Add immediate unsigned word. Upper 6 bits out of 32.
 */
#define MIPS_OP_BITS_ADDIU 0x09

/**
 * Store word. Upper 6 bits out of 32.
 */
#define MIPS_OP_BITS_SW 0x2b

/**
 * Jump to register. Upper 6 bits out of 32.
 */
#define MIPS_OP_BITS_JR 0x00

/**
 * Five bits defining register source or destination, for stack pointer register.
 */
#define MIPS_REG_SOURCE_BITS_SP 0x1d

/**
 * Five bits defining register source or destination, for return address register.
 */
#define MIPS_REG_SOURCE_BITS_RA 0x1f

/**
 * Builds 32 bit word to perform ADDIU.
 * 
 * No error checking or input size validation are performed here.
 * 
 * @param source_r: source register number (5 bits).
 * @param dest_r: destination register number (5 bits).
 * @param imm: 16 bit immediate value to add.
 */
#define MIPS_INSTR_ADDIU(source_r, dest_r, imm) ((MIPS_OP_BITS_ADDIU << 26) | (source_r << 21) | (dest_r << 16) | imm)

/**
 * Builds 32 bit word to perform SW.
 * 
 * No error checking or input size validation are performed here.
 * 
 * @param val_r: source register holding value to store (5 bits).
 * @param dest_r: register holding destination base address (5 bits).
 * @param offset: 16 bit offset from base address to store value at.
 */
#define MIPS_INSTR_SW(val_r, dest_r, offset) ((MIPS_OP_BITS_SW << 26) | (dest_r << 21) | (val_r << 16) | offset)

/**
 * Builds 32 bit word to perform SW.
 * 
 * No error checking or input size validation are performed here.
 * 
 * Note: lower 6 bits of JR instruction are always 0x08.
 * 
 * @param dest_r: register holding destination address (5 bits).
 */
#define MIPS_INSTR_JR(dest_r) ((MIPS_OP_BITS_JR << 26) | (dest_r << 21) | 8)

/**
 * Any MIPS 32-bit instruction matching $sp increment
 * 
 * 0x27BD0000 = # addiu $sp, $sp <any_immediate>
 */
#define MIPS_ADDIU_SP_SP_ANY        MIPS_INSTR_ADDIU(MIPS_REG_SOURCE_BITS_SP, MIPS_REG_SOURCE_BITS_SP, 0)
#define MIPS_ADDIU_SP_SP_ANY_MASK   0xFFFF0000

/**
 * Any MIPS 32-bit instruction matching sw into $sp
 * 
 * 0xAFA00000 = sw <any_register>, 0x[any_offset]($sp)
 */
#define MIPS_SW_SP_ANY       MIPS_INSTR_SW(0, MIPS_REG_SOURCE_BITS_SP, 0)
#define MIPS_SW_SP_ANY_MASK  0xFFE00000

/**
 * Any MIPS 32-bit instruction (should only be one...) matching jump to return address.
 * 
 * 0x03E00008 = jr ra
 */
#define MIPS_JR_RA MIPS_INSTR_JR(MIPS_REG_SOURCE_BITS_RA)

// bss - TLB section

OSThread g_tlbThread;
char g_tlbStack[0x2300] __attribute__((aligned(16)));
OSMesgQueue g_faultMesgQ;
OSMesg *g_faultMesgBuf;
OSTimer g_crashTimer;
#ifdef DEBUG_HANG_WATCHDOG
static u32 s_lastFrame;
static u32 s_stuckTicks;
#endif

/**
 * EU .data, offset from start of data_seg : 0x2540
*/

// 71 x 32 text buffer (32th line is not drawn)
unsigned char g_DebugOutputTextBuffer[32][71] = {0}; // unused in the final game (waste of space)

static s32 g_DebugOutputCurrentPosX = 0;
static s32 g_DebugOutputCurrentPosY = 0;

// 4 x 7 bitmaps of ascii characters (0x20 - 0x7E), each padded to 32 bits
u32 g_DebugOutputBitmaps[] = {
    0x00000000, 0x22220200, 0x55000000, 0x05F5F500, 0x27427200,     // ' ',  '!',  '\"', '#',  '$'
    0x05124500, 0x34255300, 0x22000000, 0x24444420, 0x42222240,     // '%',  '&',  '\'', '(',  ')'
    0x06F6F600, 0x00272000, 0x00000240, 0x00070000, 0x00000200,     // '*',  '+',  '\,',  '-',  '.'
    0x11224480, 0x25555200, 0x26222700, 0x25125700, 0x61211600,     // '/',  '0',  '1',  '2',  '3'
    0x33557300, 0x64611600, 0x24655200, 0x71112200, 0x25755200,     // '4',  '5',  '6',  '7',  '8'
    0x25531600, 0x00200200, 0x00200640, 0x01242100, 0x00707000,     // '9',  ':',  ';',  '<',  '='
    0x04212400, 0x07120200, 0x25FF5700, 0x02557D00, 0x06575E00,     // '>',  '?',  '@',  'A',  'B'
    0x07445300, 0x07555600, 0x07565700, 0x07564400, 0x07C95700,     // 'C',  'D',  'E',  'F',  'G'
    0x05575500, 0x07222700, 0x03111600, 0x05665500, 0x04445F00,     // 'H',  'I',  'J',  'K',  'L'
    0x0DFF9D00, 0x0F777D00, 0x07DD5700, 0x07564600, 0x07995770,     // 'M',  'N',  'O',  'P',  'Q'
    0x07565500, 0x07461E00, 0x07222200, 0x0D999600, 0x0D552200,     // 'R',  'S',  'T',  'U',  'V'
    0x0DF77500, 0x0D625500, 0x05622600, 0x07125700, 0x32222230,     // 'W',  'X',  'Y',  'Z',  '['
    0x44222110, 0x62222260, 0x25000000, 0x00000700, 0x42200000,     // '\\', ']',  '^',  '_',  '`'
    0x0067D700, 0x44755700, 0x00788600, 0x117DD700, 0x006FC700,     // 'a',  'b',  'c',  'd',  'e'
    0x32722700, 0x007DD730, 0x44755500, 0x02622700, 0x02711130,     // 'f',  'g',  'h',  'i',  'j',
    0x44766500, 0x62222700, 0x00FFFF00, 0x00755D00, 0x006DD600,     // 'k',  'l',  'm',  'n',  'o'
    0x00755740, 0x00799710, 0x00744600, 0x00775700, 0x02722300,     // 'p',  'q',  'r',  's',  't'
    0x00555700, 0x00552200, 0x00577500, 0x00562500, 0x00552220,     // 'u',  'v',  'w',  'x',  'y'
    0x00703700, 0x12242210, 0x02222220, 0x42212240, 0x005A0000      // 'z',  '{',  '|',  '}',  '~'
};

u16 *g_DebugOutputVideoBuffer1 = NULL;
u16 *g_DebugOutputVideoBuffer2 = NULL;

// forward declarations

void crashMain(void* arg0);

void crashPrintDescription(u32 mask, char *label, struct regDesc_t *description);
// end forward declarations

/**
 * 5AE0    70004EE0
 */
void crashInit(void)
{
    crashInitBuffers();
    osCreateMesgQueue(&g_faultMesgQ, (OSMesg *)&g_faultMesgBuf, TLB_MESSAGE_QUEUE_SIZE);
    // The stack grows down, so it starts at the end of the array. The original passed the start of the array
    // and ran on whatever the linker put in front of it (g_CrashMessage, as long as variables kept source order).
    osCreateThread(&g_tlbThread, TLB_THREAD_ID, &crashMain, NULL, g_tlbStack + sizeof(g_tlbStack), TLB_THREAD_PRIORITY);
    osStartThread(&g_tlbThread);
}

/**
 * 5B54    70004F54
 * 
 * @param arg0 Unused.
 */
extern OSThread *__osRunQueue;
void crashMain(void* arg0)
{
    OSMesg msg = 0;
    static OSThread *curr;
    static OSThread *last;
    /**
     * Target generates 5 separate dereferences of g_faultedThreadPtr,
     * only way I can get that to happen is with a pointer to a pointer.
     */
    //OSThread **ppfaultedThread = &g_faultedThreadPtr;

    osSetEventMesg(OS_EVENT_FAULT, &g_faultMesgQ, (OSMesg)MSG_FAULT);
    last = 0;

    // Wake twice a second: a fault message also wakes us straight away
    osSetTimer(&g_crashTimer, OS_USEC_TO_CYCLES(500000), OS_USEC_TO_CYCLES(500000), &g_faultMesgQ, (OSMesg)MSG_WATCHDOG);

    while (1)
    {
        osRecvMesg(&g_faultMesgQ, &msg, OS_MESG_BLOCK);
        curr = __osGetCurrFaultedThread();

        if (curr != NULL)
        {
            dbgShowFault(curr);
        }

#ifdef DEBUG_HANG_WATCHDOG
        if (g_DbgFrameCounter != s_lastFrame)
        {
            s_lastFrame = g_DbgFrameCounter;
            s_stuckTicks = 0;
        }
        else if (++s_stuckTicks >= 12)
        {
            dbgShowHang(s_stuckTicks / 2);
        }
#endif
    }


}


#define IEEE_FLOAT_FRACTION_BITMASK   0x7FFFFF
#define IEEE_FLOAT_FRACTION_BIT_COUNT       23
#define IEEE_FLOAT_EXPONENT_BITMASK 0x7F800000
#define IEEE_FLOAT_EXPONENT_BIT_COUNT        8
#define IEEE_FLOAT_SIGN_BITMASK     0x80000000
#define IEEE_FLOAT_SIGN_BIT_COUNT            1


/**
 * DebConTextPos
 * Sets the position where text will be displayed inside console window
 * (Does not draw - see print_to_vidbuff1)
 * @param x: column in which to display text (0-71)
 * @param y: line in which to display text (0-31)
 * @param c: Character to Print
 */
void crashPutChar(s32 x, s32 y, unsigned char c)
{
    // replace tabs and line feeds with nothing
    if ((c == '\t') || (c == '\n'))
    {
        c = '\0';
    }
    // is printable char? if not replace with "?"
    if ((c > '\0') && (c < ' ') || (c > '~'))
    {
        c = '?';
    }
    if (((x >= 0) && (x <= 71)) && ((y >= 0) && (y <= 31)))
    {
        // Debug Window is 71 chars wide 32 rows high
        //! BUG 71 is accepted as xpos but will overflow the buffer
        g_DebugOutputTextBuffer[y][x] = c;
    }
}

/**
 * crashAppendChar
 * Parse Character and save to On-Screen Buffer
 * @param c: Character to parse
 */
void crashAppendChar(unsigned char c)
{
    if (c != '\0')
    {
        if (c == '\t')
        {
            do
            {
                crashAppendChar(' '); // fill with blanks
            } while (g_DebugOutputCurrentPosX & 7);

            return;
        }
        if (c == '\n')
        {
            g_DebugOutputCurrentPosY++;
            g_DebugOutputCurrentPosX = 0;
        }
        if (g_DebugOutputCurrentPosY > 30)
        {
            crashScroll(g_DebugOutputCurrentPosY - 30);
            g_DebugOutputCurrentPosY = 30;
        }
        if (c != '\n')
        {
            crashPutChar(g_DebugOutputCurrentPosX, g_DebugOutputCurrentPosY, c);
            g_DebugOutputCurrentPosX++;
            if (g_DebugOutputCurrentPosX > 70)
            {
                g_DebugOutputCurrentPosX = 0;
                g_DebugOutputCurrentPosY++;
            }
        }
    }
}

/**
 * Scroll Buffer up by a number of lines
 * @param LinesToScroll: Number of Lines to scroll upward
 */
void crashScroll(s32 numlines)
{
    s32 y;
    s32 x;
    while (numlines-- > 0)
    {
        for (y = 0; y < 31; y++)
        {
            for (x = 0; x < 71; x++)
            {
                g_DebugOutputTextBuffer[y][x] = g_DebugOutputTextBuffer[y + 1][x];
            }
        }
    }
}
/**
 * Print 4x8 1bit Glyph to stderr screen at position (x,y)
 * @param x: Pixels from left within the stderr box
 * @param y: Pixels from top within the stderr box
 * @param c: the character to display
 */
void crashRenderChar(s32 x, s32 y, unsigned char c)
{
    s32  bitmap_x;
    s32  bitmap_y;
    u32  bitmap;
    s16  screen_w = viGetX();
    u16 *Vidbufp;
    if (c == '\0')
    {
        c = ' '; //replace nul with space
    }
    if ((c >= ' ') && (c <= '~')) //if valid char
    {
        Vidbufp = (g_DebugOutputVideoBuffer1 + x + (y * screen_w));
        bitmap  = g_DebugOutputBitmaps[c - ' '];
        for (bitmap_y = 0; bitmap_y < 7; bitmap_y++)
        {
            for (bitmap_x = 0; bitmap_x < 4; bitmap_x++)
            {
                if (bitmap & (1 << 31))
                {
                    *Vidbufp = GPACK_RGBA5551(255, 255, 255, 1);
                }
                else
                {
                    *Vidbufp = GPACK_RGBA5551(0, 0, 0, 1);
                }
                Vidbufp++;
                bitmap <<= 1; //
            }
            Vidbufp += screen_w;
            Vidbufp -= 4;
        }
    }
}

/**
 * Set pointers to video buffers 1 & 2.
 * Converts values to uncached addresses
 * @param cfba | A0000000 -> 8002417C
 * @param cfbb | A0000000 -> 80024180
 */
void crashSetBuffers(u16 *buffer1, u16 *buffer2)
{
    g_DebugOutputVideoBuffer1 = (void *)K0_TO_K1(buffer1);
    g_DebugOutputVideoBuffer2 = (void *)K0_TO_K1(buffer2);
}

/**
 * Set up the 2 16bit CFBs to Direct mapped, uncached memory
 */
void crashInitBuffers(void)
{
    crashSetBuffers(&cfb_16[0], &cfb_16[1]);
}

/**
 * crashRenderFrame
 * write stderr to video buffer
 * @param buffer: video buffer to render output 
 */
void crashRenderFrame(u16 *buffer)
{
    s32 screen_w;
    s32 screen_h;
    s32 output_w;
    s32 output_h;
    s32 x;
    s32 y;
    crashInitBuffers();
    g_DebugOutputVideoBuffer1 = (void *)K0_TO_K1(buffer); // overwrite cfba
    screen_w                  = ((viGetX() - 13) / 4);
    screen_h                  = ((viGetY() - 10) / 7);
    output_w                  = screen_w - 5; // - margin_w
    output_h                  = screen_h - 1; // - margin_h
    for (y = 0; ((y < output_h) && (y < 31)); y++)
    {
        for (x = 0; ((x < output_w) && (x < 71)); x++)
        {
            crashRenderChar(((x + 5) * 4), ((y + 1) * 7), g_DebugOutputTextBuffer[y][x]);
        }
    }
}

