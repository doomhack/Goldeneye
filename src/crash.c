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

// Padding
u32 g_CrashEnabled = FALSE;

//The following regDesc's are similar to PR/Tools/Gload/Server.c 
/*
 * Cause Register
 */
static regDesc_t causeDesc[] = {
    {CAUSE_BD, CAUSE_BD, "BD"},
    /* Interrupt pending bits */
    {CAUSE_IP8, CAUSE_IP8, "IP8"},
    {CAUSE_IP7, CAUSE_IP7, "IP7"},
    {CAUSE_IP6, CAUSE_IP6, "IP6"},
    {CAUSE_IP5, CAUSE_IP5, "IP5"},
    {CAUSE_IP4, CAUSE_IP4, "IP4"},
    {CAUSE_IP3, CAUSE_IP3, "IP3"},
    {CAUSE_SW2, CAUSE_SW2, "IP2"},
    {CAUSE_SW1, CAUSE_SW1, "IP1"},
    /* Hardware exception codes */
    {CAUSE_EXCMASK, EXC_INT, "Int"},
    {CAUSE_EXCMASK, EXC_MOD, "TLBmod"},
    {CAUSE_EXCMASK, EXC_RMISS, "TLBload"},
    {CAUSE_EXCMASK, EXC_WMISS, "TLBstore"},
    {CAUSE_EXCMASK, EXC_RADE, "Address error on load or instruction fetch"},
    {CAUSE_EXCMASK, EXC_WADE, "Address error on store"},
    {CAUSE_EXCMASK, EXC_IBE, "Bus error exception on instruction fetch"},
    {CAUSE_EXCMASK, EXC_DBE, "Bus error exception on data reference"},
    {CAUSE_EXCMASK, EXC_SYSCALL, "Syscall"},
    {CAUSE_EXCMASK, EXC_BREAK, "Brk"},
    {CAUSE_EXCMASK, EXC_II, "Reserved instruction"},
    {CAUSE_EXCMASK, EXC_CPU, "Cop unusable"},
    {CAUSE_EXCMASK, EXC_OV, "Overflow"},
    {CAUSE_EXCMASK, EXC_TRAP, "Trap"},
    {CAUSE_EXCMASK,
     EXC_VCEI,
     "Virtual coherency exception on intruction fetch"},
    {CAUSE_EXCMASK, EXC_FPE, "Fp exception"},
    {CAUSE_EXCMASK, EXC_WATCH, "Watchpoint"},
    {CAUSE_EXCMASK, EXC_VCED, "Virtual coherency exception on data reference"},
    {0, 0, ""},
};

/*
 * Status register
 */
static regDesc_t srDesc[] = {
    {SR_CU3, SR_CU3, "CU3"},
    {SR_CU2, SR_CU2, "CU2"},
    {SR_CU1, SR_CU1, "CU1"},
    {SR_CU0, SR_CU0, "CU0"},
    {SR_RP, SR_RP, "RP"},
    {SR_FR, SR_FR, "FR"},
    {SR_RE, SR_RE, "RE"},
    {SR_BEV, SR_BEV, "BEV"},
    {SR_TS, SR_TS, "TS"},
    {SR_SR, SR_SR, "SR"},
    {SR_CH, SR_CH, "CH"},
    {SR_CE, SR_CE, "CE"},
    {SR_DE, SR_DE, "DE"},

    /*
     * Interrupt enable bits
     * (NOTE: bits set to 1 enable the corresponding level interrupt)
     */

    {SR_IBIT8, SR_IBIT8, "IM8"},
    {SR_IBIT7, SR_IBIT7, "IM7"},
    {SR_IBIT6, SR_IBIT6, "IM6"},
    {SR_IBIT5, SR_IBIT5, "IM5"},
    {SR_IBIT4, SR_IBIT4, "IM4"},
    {SR_IBIT3, SR_IBIT3, "IM3"},
    {SR_IBIT2, SR_IBIT2, "IM2"},
    {SR_IBIT1, SR_IBIT1, "IM1"},
    {SR_KX, SR_KX, "KX"},
    {SR_SX, SR_SX, "SX"},
    {SR_UX, SR_UX, "UX"},
    {SR_KSU_MASK, SR_KSU_USR, "USR"},
    {SR_KSU_MASK, SR_KSU_SUP, "SUP"},
    {SR_KSU_MASK, SR_KSU_KER, "KER"},
    {SR_ERL, SR_ERL, "ERL"},
    {SR_EXL, SR_EXL, "EXL"},
    {SR_IE, SR_IE, "IE"},
    {0, 0, ""}
};

/*
 * floating-point status register
 */
static regDesc_t fpcsrDesc[] = {
    {FPCSR_FS, FPCSR_FS, "FS"},
    {FPCSR_C, FPCSR_C, "C"},
    {FPCSR_CE, FPCSR_CE, "Unimplemented"},
    {FPCSR_CV, FPCSR_CV, "Invalid op"},
    {FPCSR_CZ, FPCSR_CZ, "/ by 0.0"},
    {FPCSR_CO, FPCSR_CO, "Overflow"},
    {FPCSR_CU, FPCSR_CU, "Underflow"},
    {FPCSR_CI, FPCSR_CI, "Inexact op"},
    {FPCSR_EV, FPCSR_EV, "EV"},
    {FPCSR_EZ, FPCSR_EZ, "EZ"},
    {FPCSR_EO, FPCSR_EO, "EO"},
    {FPCSR_EU, FPCSR_EU, "EU"},
    {FPCSR_EI, FPCSR_EI, "EI"},
    {FPCSR_FV, FPCSR_FV, "FV"},
    {FPCSR_FZ, FPCSR_FZ, "FZ"},
    {FPCSR_FO, FPCSR_FO, "FO"},
    {FPCSR_FU, FPCSR_FU, "FU"},
    {FPCSR_FI, FPCSR_FI, "FI"},
    {FPCSR_RM_MASK, FPCSR_RM_RN, "RN"},
    {FPCSR_RM_MASK, FPCSR_RM_RZ, "RZ"},
    {FPCSR_RM_MASK, FPCSR_RM_RP, "RP"},
    {FPCSR_RM_MASK, FPCSR_RM_RM, "RM"},
    {0, 0, ""}
};

void *g_StackPtrs1[STACK_POINTER_COUNT] = {&sp_rmon, &sp_idle, &sp_shed, &sp_main, &sp_audi};

/* Each stack's end is where the next one starts; the audio stack is last. */
void *g_StackPtrs2[STACK_POINTER_COUNT] = {&sp_idle, &sp_shed, &sp_main, &sp_audi, sp_audi + SP_AUDI_SZ};

void *g_StackPtrs3[STACK_POINTER_COUNT] = {&sp_rmon, &sp_idle, &sp_shed, &sp_main, &sp_audi};

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

// Padding
u32 D_80024184[4] = {0};

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
    OSIntMask mask;
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


/**
 * Given a pointer to an instruction and a stack frame pointer, attempt to find
 * the calling function. Return the address of the caller's stack frame and
 * populate regs with stack addresses where that register was saved. This can be
 * used to retrieve the RA value and invoke crashGetParentStackFrame again to
 * build a backtrace.
 *
 * @param origptr is a pointer to an instruction. This should be either the value of
 *     the PC register of the faulted thread, or an RA register if searching a
 *     parent.
 * @param minaddr is the memory address of the start of the code segment,
 *     ie. 0x70001050. This is used to prevent the function from walking out of
 *     the code segment.
 * @param origsp is the address of the stack frame for the given origptr.
 * @param regs is a pointer to an array of 32 words.
 *
 * The function works by walking backwards one instruction at a time, looking
 * for stack frame adjustments and stores of $ra to the stack. Once both of
 * these are are found, the function returns with this information.
 *
 * @returns The function will return 0 if it can't reliably find the caller. This will
 * happen if the function being analysed didn't adjust the stack pointer or
 * store $ra in the stack. It can also fail if the function being analysed uses
 * returns within branches.
 * 
 * note: name copied from PD.
 */
const u32 *crashGetParentStackFrame(const u32 *origptr, const u32 *minaddr, const u32 *origsp, u32 *regs)
{
    const u32 *sp = origsp;
    const u32 *addr;
    u32 sp_found = 0; // v0
    u32 ra_found = 0; // t0
    s32 sw_reg;
    
    // Holds value of address while iterating, but also reused
    // to iterate while loop at start of method.
    u32 addrval;
    s16 value;    
    
    // Do not remove the following trailing backslash. The while condition
    // needs to be on the same line as previous, otherwise the build breaks.
    // Search for WHILE_ONE_LINE to see other places in code like this.
    //
    // Note: this also occurs in PD:
    // src/game/game_0b69d0.c (bd15d298664e1a22cd31886cf87bfc40ab6842e9)
    // line 382:    p = i; while (sllen < 4) {
    addrval = 0; \
    while(addrval < 32)
    {
        regs[addrval++] = 0;
    }

    // Walk backwards through the instructions
    for (addr = origptr; addr >= minaddr; addr--)
    {
        // no validation on pointer
        addrval = *addr;
        value = addrval & 0xffff;

        if ((addrval & MIPS_ADDIU_SP_SP_ANY_MASK) == MIPS_ADDIU_SP_SP_ANY)
        {
            // Found an addiu $sp, $sp, <amount> instruction, which adjusts the
			// stack pointer. These can exist near the start and end of any
			// function. The "add" at the start is done with a negative value
			// which increases the size of the stack, as the stack expands to
			// the left. This function is interested in these negative adds,
			// because it needs to reverse it and move the sp variable forward
			// to the next stack frame (the frame of the caller).
            sp_found = 1;

            if (value > 0)
            {
                // Found the addiu sp at the end of the function. It's pretty
				// rare to crash (or jump elsewhere) after restoring the sp,
				// so this situation is not supported by this function.

                break;
            }

            // Change sp to point to the caller's stack frame
            sp -= (value >> 2);

            if (ra_found)
            {
                break;
            }
        }
        else if ((addrval & MIPS_SW_SP_ANY_MASK) == MIPS_SW_SP_ANY)
        {
            // This looks for a store from $ra to the stack, so the value can
			// be read from the stack and used to find the caller's address.
            sw_reg = (addrval >> 16) & 0x1F;
            regs[sw_reg] = (u32)(sp + (value >> 2));

            if (sw_reg == MIPS_REG_SOURCE_BITS_RA)
            {
                ra_found = 1;
            }

            if (sp_found && ra_found)
            {
                // Found a jr $ra statement, which is a return. This will happen if
                // this loop has walked out of the current function and into the one
                // prior to it, so bail.

                // It can also happen if the function has return statements within
                // branches, but handling these correctly would involve a lot of
                // complexity so that's unsupported. Because of this, this function
                // can end the backtrace prematurely if it encounters a function
                // that does this.

                break;
            }
        }
        else if (addrval == MIPS_JR_RA)
        {
            break;
        }
    }

    if (sp_found && ra_found)
    {
        return sp;
    }

    return 0;
}

/**
 * 5DE0    700051E0
 *     V0=TRUE if opcode that set RA A0 was a JAL or JALR type within bounds (70000450,70020D90)
 *     accepts: A0=p->70-mapped TLB function, presumably from RA
 */
bool crashIsReturnAddress(u32 *instruction)
{
    u32 prevop;

    if ((((s32) instruction & 3) == 0)
        && ((((u32) instruction >= (u32) &_codeSegmentStart) && ((u32) &_codeSegmentEnd >= (u32) instruction))
            || (((u32) instruction >= (u32) &_gameSegmentStart) && ((u32) &_gameSegmentEnd >= (u32) instruction))))
    {
        prevop = instruction[-2];
        if ((prevop & 0xFC00003C) == 9)
        {
            return TRUE;
        }
        if ((prevop & 0xFC000000) == 0xC000000)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * 5E58    70005258
 *     V0= strlen(A0); used exclusively for scanning ind.rea.buf
 *     accepts: A0=p->string
 * 
 * Max length returned is 256.
 */
s32 crashGetStrLen(u8 *arg0)
{
    s32 count = 0;
    u8 c;

    for (c = *arg0++; c != '\0'; c = *arg0++)
    {
        count++;
        if (count > 255)
        {
            break;
        }
    }

    return count;
}


/**
 * 6020    70005420
 *     V0=hardcoded SP for debug thread A1, corrected for address range A0
 *     accepts: A0=p->address space, A1=entry#
 * 
 * If sp is above 0x80000000U and tid is valid, returns g_StackPtrs2 entry.
 * Otherwise, if tid is valid, returns sp & 0xF0000000, bitwise OR'd
 * with the difference from g_StackPtrs2 entry (pointer value) to 
 * g_StackPtrs1 entry (pointer value).
 * Otherwise, returns null.
 * 
 * @param sp: address / address space
 * @param tid: index into g_StackPtrs2/3. Must be (1 <= tid <= STACK_POINTER_COUNT).
 */
void * crashGetStackEnd(u32 sp, u32 tid)
{
    /* IDO allows initialising an array from another array, GCC does not */
    void **localStackPointers1 = g_StackPtrs1;
    void **localStackPointers2 = g_StackPtrs2;
    void *p2;
    void *p1;

    if ((s32)tid <= (s32)0 || (u32)tid > (u32)STACK_POINTER_COUNT)
    {
        return NULL;
    }

    p1 = localStackPointers1[tid];
    p2 = localStackPointers2[tid];
    
    if (sp >= 0x80000000U)
    {
        return p2;
    }
    
    p2 = (void*)(
        (sp & 0xF0000000) | ((u32)p2 - (u32)p1)
        );
    return p2;
}

/**
 * 60E4    700054E4
 *     V0=hardcoded SP for debug thread A1, corrected for address range A0
 *     accepts: A0=p->address space, A1=entry#
 * 
 * If sp is above 0x80000000U and tid is valid, returns g_StackPtrs3 entry.
 * Otherwise, if tid is valid, returns sp & 0xF0000000.
 * Otherwise, returns null.
 * 
 * @param sp: address / address space
 * @param tid: index into g_StackPtrs3. Must be (1 <= tid <= STACK_POINTER_COUNT).
 */
void * crashGetStackStart(u32 sp, u32 tid)
{
    void **localStackPointers3 = g_StackPtrs3;
    void *p;

    if ((s32)tid <= (s32)0 || (u32)tid > (u32)STACK_POINTER_COUNT)
    {
        return NULL;
    }

    p = localStackPointers3[tid];

    if (sp >= 0x80000000U)
    {
        return p;
    }

    p = (void*)(sp & 0xF0000000);
    return p;
}


#define IEEE_FLOAT_FRACTION_BITMASK   0x7FFFFF
#define IEEE_FLOAT_FRACTION_BIT_COUNT       23
#define IEEE_FLOAT_EXPONENT_BITMASK 0x7F800000
#define IEEE_FLOAT_EXPONENT_BIT_COUNT        8
#define IEEE_FLOAT_SIGN_BITMASK     0x80000000
#define IEEE_FLOAT_SIGN_BIT_COUNT            1


/**
 * 6160	70005560
 *     V0= TRUE if F12 a normal single precision float
 *     accepts: F12= single-precision float
 */
s32 crashIsDouble(f32 value)
{
    u32 bits = *(u32*)&value;
    u32 fraction = bits & IEEE_FLOAT_FRACTION_BITMASK;
    u8 exponent = (u8)(bits >> IEEE_FLOAT_FRACTION_BIT_COUNT);
    
    return (fraction == 0) || (exponent != 0 && (exponent != 0xff));
}

/**
 * 61A4	700055A4
 *     V0= TRUE if A1 a normal single precision float; would have set result as short at A0
 *     accepts: A0=(unused) p->target, A1=single-precision float
 */
s32 crashPrintFloat(s32 index, f32 value)
{
    return crashIsDouble(value);
}

/**
 * 61C8	700055C8
 *     removed: set normality of single-precision floats A1, A2, A3, SP+10 in table at A0
 */
void crashPrint4Floats(s32 index, f32 value1, f32 value2, f32 value3, f32 value4)
{
    crashPrintFloat(index, value1);
    crashPrintFloat((index + 2), value2);
    crashPrintFloat((index + 4), value3);
    crashPrintFloat((index + 6), value4);

    return;
}

/**
 * 6228	70005628
 *     unconditional return
 */
void crashRender(void) {
    return;
}



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

