#include <os_extension.h>
#include <PR/os.h>
#include <PR/rcp.h>
#include <bondconstants.h>
#include <bondgame.h>
#include "boss.h"
#include "crash.h"
#include "game/menu/cheat.h"
#include "game/text/language.h"
#include "game/gfx/dyn.h"
#include "game/level/game.h"
#include "game/save/file.h"
#include "init.h"
#include "joy.h"
#include "game/level/lv.h"
#include "game/menu/front.h"
#include "mema.h"
#include "memp.h"
#include "music.h"
#include "game/asset/asset.h"
#include "game/mission/objective.h"
#include "rom.h"
#include "random.h"
#include "rmon.h" /*<PR/rmon.h>*/
#include "sched.h"
#include "fr.h"
#include "game/gfx/texload.h"
#include "dbgscreen.h"
#include "vi.h"
#include "exppak_msg.h"
#include "game/world/bg.h"
#include "game/level/lv.h"
#include "game/replay/ramromreplay.h"
#include "game/replay/testhooks.h"
#include "game/gfx/rsp.h"
#include "game/nav/stan.h"
#include "game/text/text.h"
#include "game/player/player.h"
#include "game/player/gun.h"
#include "game/player/headbob.h"
#include "game/level/frametiming.h"
#include "PR/R4300.h"
#include "game/model/anim.h"
#include "game/model/initactorpropstuff.h"
#include "game/prop/explosion.h"
#include "str.h"

/**
 * @file boss.c
 * @brief Main game loop and initialization functions.
 * 
 * This file contains the core functions that drive the game loop and manage stages.
 */


// #define	OS_CLOCK_RATE		62500000LL
// #define OS_CPU_COUNTER       (OS_CLOCK_RATE * 3 / 4)

#ifdef REFRESH_PAL

    #define CYCLES_PER_FRAME    ((u32) OS_CPU_COUNTER / 50U) // 937,500
    #define INTERVAL_INTER_MATH    (CYCLES_PER_FRAME / 2U) // 468,750

    // this is: g_FrameMinFrames * 931050 - 465525
    #define MAIN_LOOP_TICK_INTERVAL g_FrameMinFrames * (CYCLES_PER_FRAME - 6450U) - (INTERVAL_INTER_MATH - 3225U)

#else

    #define CYCLES_PER_FRAME    ((u32) OS_CPU_COUNTER / 60U) // 781,250
    #define INTERVAL_INTER_MATH    (CYCLES_PER_FRAME / 2U) // 390,625

    // note: 3225U * 5/6 = 2687.5
    #define MAIN_LOOP_TICK_INTERVAL (INTERVAL_INTER_MATH - 2688U) // 387,937

#endif

/**
 * Copied from n64devkit\ultra\usr\src\pr\demos_old\simple\gfx.h
 */
typedef union {

    struct {
        short   type;
    } gen;

    struct {
        short   type;
    } done;

    OSScMsg      app;

} GFXMsg;

// forward declarations
void bossMainloop(void);

/* data */
/* OS counts the CPU spent building frames, summed. See lvRenderFpsCounter. */
u32 g_PerfCpuSum = 0;
s32 g_StageNum = LEVELID_TITLE;
/**
 * Each level's memory budget, in KB. Two-player and co-op games use the level's _MP entry
 * when it has one. Levels not listed use LEVELID_DEFAULT's.
 */
static const LevelMemBudget m_bossMemBudgets[] = {
    /* level                gfx  vtx  tex  rooms */
    { LEVELID_DAM,           70,  50, 625, 275 },
    { LEVELID_FACILITY,      70,  50, 650, 160 },
    { LEVELID_RUNWAY,       100,  50, 610, 300 },
    { LEVELID_SURFACE,       70,  50, 600, 300 },
    { LEVELID_BUNKER1,      100,  50, 700, 150 },
    { LEVELID_SILO,          70,  50, 660, 150 },
    { LEVELID_FRIGATE,       70,  50, 750, 225 },
    { LEVELID_SURFACE2,     100,  50, 550, 350 },
    { LEVELID_BUNKER2,      100,  50, 725, 150 },
    { LEVELID_STATUE,        70,  50, 750, 220 },
    { LEVELID_ARCHIVES,      70,  50, 600, 250 },
    { LEVELID_STREETS,       60,  40, 635, 290 },
    { LEVELID_DEPOT,         60,  50, 710, 300 },
    { LEVELID_TRAIN,        100,  50, 600, 200 },
    { LEVELID_JUNGLE,        70,  50, 500, 200 },
    { LEVELID_CONTROL,       70,  50, 671, 200 },
    { LEVELID_CRADLE,       100,  50, 650, 250 },
    { LEVELID_CAVERNS,      100,  50, 530, 250 },
    { LEVELID_AZTEC,         60,  40, 855, 135 },
    { LEVELID_EGYPT,        100,  50, 600, 250 },
    { LEVELID_CITADEL,      100,  50, 650, 150 },
    { LEVELID_CUBA,         100,  50, 300, 300 },
    { LEVELID_TITLE,         80,  20, 646,   1 },
    { LEVELID_TEMPLE,       130, 100, 390, 100 },
    { LEVELID_COMPLEX,      130, 100, 400, 100 },
    { LEVELID_LIBRARY,      130, 100, 400, 100 },
    { LEVELID_BASEMENT,     130, 100, 400, 100 },
    { LEVELID_STACK,        130, 100, 400, 100 },
    { LEVELID_CAVES,        130, 100, 400, 300 },
    { LEVELID_BUNKER2_MP,   130, 100, 550, 170 },
    { LEVELID_ARCHIVES_MP,   80, 100, 550, 250 },
    { LEVELID_CAVERNS_MP,   130, 100, 440, 220 },
    { LEVELID_FACILITY_MP,   90, 100, 550, 230 },
    { LEVELID_EGYPT_MP,     110, 100, 350, 400 },
    { LEVELID_DEFAULT,      100,  50, 700, 400 },
};

/** The current level's memory budget. */
const LevelMemBudget *g_LevelMemBudget = &m_bossMemBudgets[ARRAYCOUNT(m_bossMemBudgets) - 1];

s32 g_MainStageNum = LEVELID_NONE;

OSScMsg g_bossGfxDoneMsg = { .type = OS_SC_DONE_MSG };


/** Looks up a level's memory budget (see m_bossMemBudgets). */
static const LevelMemBudget *bossFindMemBudget(LEVELID stage)
{
    const LevelMemBudget *budget;

    if (stage != LEVELID_TITLE && frontGetPlayerCount() >= 2)
    {
        for (budget = m_bossMemBudgets; budget->level != LEVELID_DEFAULT; budget++)
        {
            if (budget->level == stage + ENVIRONMENTDATA_PLAYERS_4)
            {
                return budget;
            }
        }
    }

    for (budget = m_bossMemBudgets; budget->level != LEVELID_DEFAULT; budget++)
    {
        if (budget->level == stage)
        {
            return budget;
        }
    }

    return budget;
}

// extern declarations
extern struct player *g_CurrentPlayer;

/**
 *     ??? - uses "-level_", "-m" strings
 */


/**
 * The mod needs the Expansion Pak: the game code is resident in RAM and the heap
 * runs to the top of 8MB. Without it, say so on screen instead of crashing.
 * Everything used here sits in the first 4MB.
 */
static void bossCheckExpansionPak(void)
{
    OSViMode *mode;
    u16 *fb;
    s32 i;
    s32 x;
    s32 y;
    s32 grey;

    if (osMemSize >= 0x800000)
    {
        return;
    }

    fb = (u16 *) K0_TO_K1(cfb_16[0]);

    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
    {
        fb[i] = GPACK_RGBA5551(0, 0, 0, 1);
    }

    // Pre-rendered anti-aliased text (tools/mkexppakmsg.py), 4 bits per pixel
    fb += ((SCREEN_HEIGHT - EXPPAK_MSG_HEIGHT) / 2) * SCREEN_WIDTH + (SCREEN_WIDTH - EXPPAK_MSG_WIDTH) / 2;

    for (y = 0; y < EXPPAK_MSG_HEIGHT; y++)
    {
        for (x = 0; x < EXPPAK_MSG_WIDTH; x++)
        {
            grey = g_ExpPakMsgBitmap[(y * EXPPAK_MSG_WIDTH + x) >> 1];
            grey = (x & 1) ? (grey & 0xF) : (grey >> 4);
            grey = grey * 17;
            fb[y * SCREEN_WIDTH + x] = GPACK_RGBA5551(grey, grey, grey, 1);
        }
    }

    // Program the VI by hand with interrupts off, so the VI manager and the
    // scheduler can't blank the screen again on the next retrace.
    osSetIntMask(OS_IM_NONE);

    mode = &osViModeTable[(osTvType == TV_TYPE_MPAL) ? OS_VI_MPAL_LAN1 : OS_VI_NTSC_LAN1];

    IO_WRITE(VI_CONTROL_REG, mode->comRegs.ctrl);
    IO_WRITE(VI_WIDTH_REG, mode->comRegs.width);
    IO_WRITE(VI_BURST_REG, mode->comRegs.burst);
    IO_WRITE(VI_V_SYNC_REG, mode->comRegs.vSync);
    IO_WRITE(VI_H_SYNC_REG, mode->comRegs.hSync);
    IO_WRITE(VI_LEAP_REG, mode->comRegs.leap);
    IO_WRITE(VI_H_START_REG, mode->comRegs.hStart);
    IO_WRITE(VI_X_SCALE_REG, mode->comRegs.xScale);
    IO_WRITE(VI_V_START_REG, mode->fldRegs[0].vStart);
    IO_WRITE(VI_V_BURST_REG, mode->fldRegs[0].vBurst);
    IO_WRITE(VI_INTR_REG, mode->fldRegs[0].vIntr);
    IO_WRITE(VI_Y_SCALE_REG, mode->fldRegs[0].yScale);
    IO_WRITE(VI_ORIGIN_REG, osVirtualToPhysical(cfb_16[0]) + mode->fldRegs[0].origin);

    while (1)
    {
    }
}

void bossInitMainthreadData(void)
{
    OSMesg bossmsg;
    OSTimer bosstimer;
    OSMesgQueue bossmq;
    s32 i;

    bossCheckExpansionPak();
    crashInit();
    romCreateMesgQueue();
    texResolveTableOffsets();
    bgInit();
    viInit();
    viInitVideoSettings();
    assetInit();
    joyInit();
    osCreateMesgQueue(&bossmq, &bossmsg, 1);

    for (i = 0; i != MAXCONTROLLERS; i++)
    {
        osSetTimer(&bosstimer, OS_USEC_TO_CYCLES(100000), 0, &bossmq, &bossmsg);
        osRecvMesg(&bossmq, &bossmsg, OS_MESG_BLOCK);
        if (i == 1)
        {
            joyCheckStatusThreadSafe();
        }
        else if (i >= 2)
        {
            joyConsumeSamplesWrapper();
        }
    }

    mempInit((u8 *) &_heapSegmentStart, (u8 *) PHYS_TO_K0(osMemSize));
    mempResetPool(MEMPOOL_PERMANENT);
    langInit();
    lvInit();
    textInit();
    playersResetHandicaps();
    frameTimingStart();
    smokeInitScaleMtx();
    null_init_main_2();
    casingsInit();
    animLoadTable();
    gunsInitAnimGroups();
    reset_counter_rand_body_head();
    null_init_main_3();
    headBobInitGaitModel();
    gameInitData();
    fileResetRamRomSave();
    clear_ramrom_block_buffer_heading_ptrs();
}

/**
 *     loads primary resources and starts main program loop
 *     this is infinite.  Loops unconditionally: JAL 70006060
 */
void bossEntry(void) {
    bossInitMainthreadData();
    rspAllocateBuffers();
    musicSeqPlayerInit();
    while(1){
       bossMainloop();
    }
}

/**
 * Main program loop.
 *
 *
 * Seems to have been based on devkit example at one point,
 * n64devkit\ultra\usr\src\pr\demos_old\simple\simple.c
 *
 * loop:
 *         70006090 tests memstring for "-level_##"
 *         700060DC if not title, tests memstring for "-hard#"
 *         70006160 follows...
 *         700061FC test if debug console unconnected [800241A4]
 *         700062EC follows...
 *         700062FC tests memstring for "-ma"
 *         7000633C allocates "-ma" bytes to mem bank 4
 *         7000635C reset player data pointers
 *         70006364 offsets stage number based on number of players unless main menu
 *         700063A0 parses and sets memory allocation, loads stage, etc.
 *         ...
 *         7000674C writes a full sync, end display list combo
 *         700067C0 follows...
 *         70006854 follows... (700068BC - stop demos)
 */
void bossMainloop(void)
{
    // declarations

    s32 done;
    GFXMsg *localGfxFrameMsg;
    OSScMsg localGfxDoneMsg;
    s32 toggleFlag;
    Gfx *gdl;
    Gfx *firstGdl;
    u32 nowCount;
    s32 i;
    struct player *localPlayer;
    s32 localSelectedNumPlayers;
    u32 pendingGfx = 0;
    u32 cpuFrameStart = 0;
    u32 mainTickElapsed;
    s32 rspReplyMsg;
    u32 roomHeapSize;


    // end declarations

    done = 0;
    nowCount = osGetCount();
    randomSetSeed(nowCount);

    // 'done' value never changes, and control never breaks -- infinite loop
    while (!done)
    {
        localGfxFrameMsg = NULL;
        localGfxDoneMsg = g_bossGfxDoneMsg;
        toggleFlag = 0;
        pendingGfx = 0;

        test_if_recording_demos_this_stage_load(g_StageNum, lvGetDifficulty());
        g_LevelMemBudget = bossFindMemBudget(g_StageNum);

        mempResetPool(MEMPOOL_STAGE);
        assetForgetSizes();

        // Counter-op: when controller 2 plays Bond (player 1), the two
        // controllers swap places for the mission. Menus use them as wired.
        g_JoySwapPads = g_CounterOpMode && (g_CounterOpBondPad != 0) && (g_StageNum != LEVELID_TITLE);

        // Co-op: two players in different parts of a level see more rooms at once than one
        // player does. When the room heap filled up, rooms stopped loading (Silo's first
        // computer room), so co-op doubles it.
        roomHeapSize = g_LevelMemBudget->roomHeapKB * 1024;

        if (g_CoopMode && (g_StageNum != LEVELID_TITLE))
        {
            roomHeapSize *= 2;
        }

        memaReset(mempAlloc(roomHeapSize, MEMPOOL_STAGE), roomHeapSize);
        playersReset();

        localSelectedNumPlayers = 0;
        if (g_StageNum != LEVELID_TITLE)
        {
            localSelectedNumPlayers = 1;
            if (frontGetPlayerCount() >= 2)
            {
                localSelectedNumPlayers = frontGetPlayerCount();
            }
        }

        playersInit(localSelectedNumPlayers);
        dynInitMemory();
        joyCheckStatusThreadSafe();
        lvStageLoad(g_StageNum);
        viInitBuffers();
        frameWaitForNext();


        while (osRecvMesg(&gfxFrameMsgQ, (OSMesg *)&localGfxFrameMsg, OS_MESG_NOBLOCK) == 0)
        {
           // empty
        }


        while (g_MainStageNum < 0 || pendingGfx != 0)
        {
            osRecvMesg(&gfxFrameMsgQ, (OSMesg *)&localGfxFrameMsg, OS_MESG_BLOCK);

            switch (localGfxFrameMsg->gen.type)
            {
                case (OS_SC_RETRACE_MSG):
                {

                    mainTickElapsed = (osGetCount() - g_FrameStartCount);
                    if (mainTickElapsed < MAIN_LOOP_TICK_INTERVAL)
                    {
                        // nothing to do.
                    }
                    else
                    {
                        if (g_MainStageNum < 0 && pendingGfx < 2U)
                        {
                            g_DbgFrameCounter++;

                            if (get_is_ramrom_flag())
                            {
                                iterate_ramrom_entries_handle_camera_out();
                            }
                            else
                            {
                                frameWaitForNext();
                            }

                            cpuFrameStart = osGetCount();
                            joyConsumeSamplesWrapper();
                            permit_stderr(0);

                            gdl = firstGdl = dynGetMasterDisplayList();

                            lvTick();
                            coopUpdateSoloView();
                            playersShuffleOrder();

                            if (g_StageNum != LEVELID_TITLE)
                            {
                                for (i = 0; i < getViewCount(); i++)
                                {
                                    playerSetCurrent(playerGetShuffled(i));

                                    localPlayer = g_CurrentPlayer;
                                    viSetViewSize(localPlayer->viewx, localPlayer->viewy);

                                    localPlayer = g_CurrentPlayer;
                                    viSetViewPosition(localPlayer->viewleft, localPlayer->viewtop);

                                    coopTankBeginPlayer();
                                    lvPlayerTick();
                                    coopTankEndPlayer(TRUE);
                                }
                            }

                            gdl = lvRender(gdl);
                            testhooksEndFrame();

                            // Lets Visualise the Coverage Value used for Scilohete Anti-Ailising (edges)
                            // (done on the VI), also produces a cool looking linemode - providing AA is working.
                            if (g_CheatLineMode)
                            {
                                gDPPipeSync(gdl++); // 0xe7000000, 0x00000000
                                gDPSetCycleType(gdl++, G_CYC_1CYCLE); // 0xba001402, 0x00000000
                                gDPSetBlendColor(gdl++, 0xff, 0xff, 0xff, 0xff); // 0xf9000000, 0xffffffff
                                gDPSetPrimDepth(gdl++, 0xffff, 0xffff); // 0xee000000, 0xffffffff
                                gDPSetDepthSource(gdl++, G_ZS_PRIM); // 0xb9000201, 0x00000004
                                gDPSetRenderMode(gdl++, G_RM_VISCVG, G_RM_VISCVG2); // 0xb900031d, 0x0fa54040
                                gDPFillRectangle(gdl++, 0, 0, VI_SX(viGetX()) - 1, viGetY() - 1);
                            }

                            gDPFullSync(gdl++); // 0xe9000000, 0x00000000
                            gSPEndDisplayList(gdl++); // 0xb8000000, 0x00000000

                            dynSwapBuffers();
                            viUpdateMode();

                            rspReplyMsg = (s32)(&localGfxDoneMsg);
                            rspGfxTaskStart(firstGdl, gdl, (s32*)rspReplyMsg);

                            pendingGfx++;
                            toggleFlag ^= 1;

                            g_PerfCpuSum += osGetCount() - cpuFrameStart;
                        }
                    }
                }
                break;

                case (OS_SC_DONE_MSG):
                    pendingGfx--;
                    break;

                case OS_SC_PRE_NMI_MSG: // message when the console's power is being cut off
                    pendingGfx = 4U;    // this stops any further graphics processing
                    break;
            }
        }

        lvStageUnload();
        stop_demo_playback();
        mempClosePool(MEMPOOL_STAGE);
        assetForgetSizes();

#if defined(VERSION_EU)
#endif

        g_StageNum = g_MainStageNum;
        g_MainStageNum = LEVELID_NONE;
    }

}


/**
 *     run title [0x5A->loaded stage#]; fry AT
 *     redirect to 70006950: A0=0x5A
 */
void bossRunTitleStage(void) {
    bossSetLoadedStage(LEVELID_TITLE);
}

/**
 *     A0->loaded stage# [800242FC]; fry AT
 *     0x5A jumps to folder select
 *     0x5B
 *     0x63
 */
void bossSetLoadedStage(LEVELID stage){
    g_MainStageNum = stage;
}

/**
 *     V0= stage# [800241A8]
 */
LEVELID bossGetStageNum() {
    return g_StageNum;
}

/**
 *     return to title screen from stage
 */
void bossReturnTitleStage(void) {
    objectivesTickStatusMessages();
    objectivesDisableStatusDisplay();
    if ((bossGetStageNum() != LEVELID_CUBA) && (objectivesAllComplete() != 0x0)) {
        fileRecordMissionEnd();
    }
    bossRunTitleStage();
}
