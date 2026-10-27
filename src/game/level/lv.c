/**
 * The level: one stage from load to unload, inside the main loop in boss.c.
 *
 * lvStageLoad loads the stage. Each frame the main loop calls lvTick (the parts that run
 * once: the clock, multiplayer rules, things that move unseen), lvPlayerTick for each view
 * (the player's input and movement), then lvRender, which for each view sets the camera,
 * finds the visible rooms, ticks and draws the props, fires the guns and draws the HUD.
 * lvStageUnload frees it all again.
 *
 * Also here: the game timers (g_ClockTimer is how far the game moves this frame, 0 while
 * paused), difficulty, the multiplayer time and point limits, the level music and the fps
 * counter. See docs/subsystems/level.md.
 */

#include <ultra64.h>
#include <math.h>
#include <os_extension.h>
#include <PR/libaudio.h>
#include <assets/font_dl.h>
#include <memp.h>
#include <music.h>
#include <fr.h>
#include <snd.h>
#include <rom.h>
#include <random.h>
#include <joy.h>
#include <token.h>
#include "game/level/lv.h"
#include "game/text/language.h"
#include "game/menu/front.h"
#include "game/player/bondinv.h"
#include "game/player/player.h"
#include "game/player/bondhead.h"
#include "game/prop/propobj.h"
#include "game/prop/explosion.h"
#include "game/chr/chrai.h"
#include "game/audio/levelmusic.h"
#include "game/model/model.h"
#include "game/gfx/image.h"
#include "game/prop/glass.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/gfx/imagebank.h"
#include "game/text/text.h"
#include "game/menu/watchmenu.h"
#include "sched.h"
#include "game/menu/cheat.h"
#include "game/world/bg.h"
#include "game/mission/objective.h"
#include "game/menu/mpmenu.h"
#include "game/gfx/vtxstore.h"
#include "game/model/jointlist.h"
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/prop/setup.h"
#include "game/nav/waypoint.h"
#include "game/player/bondview.h"
#include "game/player/bondview_r.h"
#include "game/gfx/zbuf.h"
#include "game/nav/stan.h"
#include "game/player/gun.h"
#include "game/world/roommtx.h"
#include "game/world/water.h"
#include "game/level/frametiming.h"
#include "game/chr/chr.h"

// bss
u32 *g_FontDl;
char ramrom_data_target[0x380];
s32 record_slot_num;
u8 * address_demo_loaded;


// data
s32 g_CurrentStageToLoad = 0;
s32 g_ControlsLockedFlag = 0;
s32 g_ClockTimer = 0;


#if defined (BUGFIX_R1)
// addresses updated, per build\ge007.j.map
f32 g_GlobalTimerDeltaRaw = 0;
s32 g_GlobalTimer = 0;
s32 g_LvTickCount = 0;
f32 g_GlobalTimerDelta = 0;
#else
f32 g_GlobalTimerDelta = 0;
s32 g_GlobalTimer = 0;
s32 g_LvTickCount = 0;
#endif
/*
* Selected difficulty mode.
* 0x80048384
*/
s32 g_SelectedDifficulty = DIFFICULTY_AGENT;

s32 g_LvIdle = 0;
s32 g_LvIdleTime = 0;
s32 g_LvTimeSinceInput = 0;
s32 g_MissionTime = 0;

s32 g_MpTime = 0x8CA0;

s32 g_MpPoint = 0xA;

ALSoundState * g_MpSoundStateRelated = NULL;

f32 g_MissionTimeSec = 0.0;

s32 g_SystemPowerTime = 0;

f32 g_SystemPowerTimeSec = 0.0;

s32 g_StageTime = 0;

f32 g_StageTimeSec = 0;

s32 g_PowerOnTime = 0;

/**
 * Power on time in seconds.
 */
f32 g_PowerOnTimeSec = 0;

/**
 * Debug variable, seems to track whether user input has changed since
 * the last time the method was entered.
 *
 * Addres 0x800483C0.
 */
s32 g_LvDrawWorld = 1;

s32 g_DebugHitTexNum = 0xFFFFFFFF;

struct LvlMpUnknown *g_DebugDetailEdit = NULL;


extern u8* _fontdlSegmentRomStart NO_SDATA;
extern u8* _fontdlSegmentRomEnd NO_SDATA;


// forward declarations


// end forward declarations

s32 lvGetDrawWorld(void)
{
    return g_LvDrawWorld;
}

void lvSetDrawWorld(s32 arg0)
{
    g_LvDrawWorld = arg0;
}

void lvInit(void)
{
    s32 size;

    size = (s32)&_fontdlSegmentRomEnd - (s32)&_fontdlSegmentRomStart;
    g_FontDl = mempAlloc(size, MEMPOOL_PERMANENT);
    romCopy(g_FontDl, &_fontdlSegmentRomStart, size);
}

/**
 * Stage load method.
 * Title screen is handled as a special case.
 * First half of method resets stage and player values (including mutliplayer values) to defaults.
 * Second part loads stage data (init guards, init guard heads, etc).
 *
 * NTSC Address: 0x7F0BDAB0.
 * NTSC-J Address: 7F0BE660.
 * PAL Address: 7F0BCE60.
 **/
void lvStageLoad(s32 stage)
{
    s32 i;
    struct player_data *player_data;

    g_CurrentStageToLoad = stage;

    // this if block pushes where g_CurrentStageToLoad gets loaded to the
    // top of the method. Maybe a debug log about which level is loaded.
    if(0)
    {
    }

    g_LvDrawWorld = 1;
    g_ControlsLockedFlag = 0;
    g_ClockTimer = 1;

#ifdef VERSION_US
    g_GlobalTimerDelta = 1.0f;
#endif
#if defined(VERSION_EU)
    g_GlobalTimerDeltaRaw = 1.0f;
#endif

    g_LvTickCount = 0;
    g_GlobalTimer = 0;

#if defined(VERSION_EU)
    g_GlobalTimerDelta = 1.20000004768f;
#endif

    g_LvIdle = 0;
    g_LvIdleTime = 0;
    g_LvTimeSinceInput = 0;
    g_MissionTime = 0;
    g_MissionTimeSec = 0.0f;
    g_StageTime = 0;
    g_StageTimeSec = 0.0f;
    g_MpSoundStateRelated = 0;

    sndSetScalerApplyVolumeAllSfxSlot(1.0f);
    musicTrack1ApplySeqpVol(VOLUME_MAX);
    musicTrack2ApplySeqpVol(VOLUME_MAX);
    musicTrack3ApplySeqpVol(VOLUME_MAX);
    levelMusicResetSlots();
    modelmgrSetLevelResetting(TRUE);
    texInitSharedPool();
#ifdef VERSION_EU
    movingSparksReset();
#else
    sparksResetAll();
#endif
    imageBankLoad();
    textLoadFonts();

    /* If title screen, initialize screen and folder setup.
    * Otherwise:
    * - enable cheats for player
    * - init watch
    * - reset some player values, like view height
    * - reset multiplayer stats
    */
    if (stage == LEVELID_TITLE)
    {
        frontInit();
    }
    else
    {
        g_NewCheatUnlocked = 0;

        if ((g_CurrentStageToLoad != LEVELID_TITLE) && (g_MissionTime == 0) && (g_ClockTimer > 0))
        {
            if (g_AppendCheatSinglePlayer != 0)
            {
                s32 s0 = 1;

                for (s0 = 1; s0 != CHEAT_INVALID; s0++)
                {
                    if (g_CheatActivated[s0] && cheatIsEnemyRockets(s0))
                    {
                        cheatActivate(s0);
                    }
                }
            }
        }

        bgLoad(g_CurrentStageToLoad);
        skySetStageNum(g_CurrentStageToLoad);

        // HACK: This method call is wrong. The function takes one argument, but the asm calls it without
        // any arguments here.
        watchMenuReset();

        levelMusicStart(stage);

        for (i=0; i<4; i++)
        {
            s32 s3;
            player_data = (struct player_data *)&g_playerPlayerData[i];

            if (!isMpGame())
            {
                // s4 variable
                player_data->autoaim = 0;
                player_data->sight = 0;
                player_data->handicap = 1.0f;
                player_data->player_perspective_height = 1.0f;
            }
            else
            {

                // why is this looping from g_playerPlayerData again, this inner block
                // gets executed 16 times in multiplayer.
                for (s3 = 0; s3 < 4; s3++)
                {
                    if (frontGetMpScenario() == SCENARIO_LTK)
                    {
                        g_playerPlayerData[s3].handicap = 200.0f;
                    }
                    else
                    {
                        g_playerPlayerData[s3].handicap = frontGetMpHandicap(s3);
                    }

                    g_playerPlayerData[s3].player_perspective_height = frontGetMpCharHeight(s3);
                }

                lvSetMpTimeLimit(frontGetMpTimeLimit());
                lvSetMpPointLimit(frontGetMpPointLimit());
                frontApplyMpAimSettings();
            }

            // g_playerPlayerData s4 variable
            player_data->time_other_players_on_screen = 0;
            player_data->damage_to_backside = 0;
            player_data->min_time_between_kills = S32_MAX;
            player_data->max_time_between_kills = 0;
            player_data->most_killed_one_life = 0;
            player_data->most_killed_one_time = 0;
            player_data->longest_inning = 0;
            player_data->shortest_inning = S32_MAX;
            player_data->order_out_in_yolt = 0;
            player_data->flag_counter = 0;
            player_data->distance_traveled = 0.0f; // one kind of float zero
            player_data->body_armor_pickups = 0.f; // a different kind of float zero

            // g_playerPlayerData s2, different than above
            for (s3 = 0; s3 < 4; s3++)
            {
                player_data->kill_counts[s3] = 0;
            }
        }
    }

    objectivesReset();
    mpMenuUnpause();
    vtxstoreReset();
    jointListInit();
    modelmgrResetSlotCounts();
    propsInit();
    propsResetLevelState();
    objSoundsInit();
    chrsInit();
    bodiesReset(stage);
    setupCreateProps(stage);
    explosionsInit();
    glassInitShards();
    waterReset();
    cheatResetPlayerMasks();

    if (g_CurrentStageToLoad == LEVELID_TITLE)
    {
        cheatResetInput();
    }
    else
    {
        s32 s0;

        waypointsValidate();
        casingsLoadModels();
        counterOpResetForStage();

        for (s0 = 0; s0 < getPlayerCount(); s0++)
        {
            playerSetCurrent(s0);
            invResetHeldTimes();
            playerInitHands();
            playerResetLife();
            playerApplyIntro();
            playerStartNewLife();
            bheadInit();
            cheatResetInput();
        }

        playerSetCurrent(0);
    }

    /**
     * Leave stage load allocation mode.
     * From this point on, model creation should try to reuse existing slots.
     */
    modelmgrSetLevelResetting(FALSE);
    
    zbufDeallocate();
    viSetVideoMode(MD_NORMAL);
    lvSetControlsLocked(0);
}


s32 lvGetCurrentStage(void)
{
    return g_CurrentStageToLoad;
}


static u32 s_FpsLastCount;
static s32 s_FpsFrames;
static s32 s_FpsTenths;
static u32 s_FpsLastCpu;
static u32 s_FpsLastRsp;
static u32 s_FpsLastRdp;
static u32 s_FpsLastTasks;
static s32 s_FpsCpuMs;
static s32 s_FpsRspMs;
static s32 s_FpsRdpMs;

/**
 * Draws the frame rate (the watch's fps counter option) in the top right
 * corner of the screen, averaged over about a second. The second line is
 * the average milliseconds per frame the CPU spent building it, and the
 * graphics task took on the RSP and until the RDP finished drawing.
 */
static Gfx *lvRenderFpsCounter(Gfx *DL)
{
    char text[40];
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    u32 now = osGetCount();
    u32 ms = (now - s_FpsLastCount) / (OS_CPU_COUNTER / 1000);

    s_FpsFrames++;

    if (ms >= 1000)
    {
        u32 cpu = g_PerfCpuSum;
        u32 rsp = g_PerfRspSum;
        u32 rdp = g_PerfRdpSum;
        u32 tasks = g_PerfGfxTasks;
        u32 ntasks = tasks - s_FpsLastTasks;

        s_FpsTenths = (ms < 10000) ? ((s_FpsFrames * 10000) / ms) : 0;
        s_FpsCpuMs = ((cpu - s_FpsLastCpu) / (OS_CPU_COUNTER / 1000)) / s_FpsFrames;
        s_FpsRspMs = (ntasks != 0) ? (((rsp - s_FpsLastRsp) / (OS_CPU_COUNTER / 1000)) / ntasks) : 0;
        s_FpsRdpMs = (ntasks != 0) ? (((rdp - s_FpsLastRdp) / (OS_CPU_COUNTER / 1000)) / ntasks) : 0;

        s_FpsFrames = 0;
        s_FpsLastCount = now;
        s_FpsLastCpu = cpu;
        s_FpsLastRsp = rsp;
        s_FpsLastRdp = rdp;
        s_FpsLastTasks = tasks;
    }

    sprintf(text, "%d.%d FPS\nCPU %d RSP %d RDP %d\n", s_FpsTenths / 10, s_FpsTenths % 10, s_FpsCpuMs, s_FpsRspMs, s_FpsRdpMs);

    width = 0;
    height = 0;
    textMeasure(&height, &width, text, g_FontBankGothicChars, g_FontBankGothic, 0);

    x = viGetX() - width - 0x14;
    y = 0x10;

    DL = textSetupDraw(DL);
    DL = textRenderRect(DL, x - 2, y - 1, x + width + 2, y + height + 1, 0x64);
    DL = textRender(DL, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, -1, viGetX(), viGetY(), 0, 0);
    DL = textEndDraw(DL);

    return DL;
}

/**
 * Graphics render method.
 * Also sets player max ammo if infinite ammo cheat is enabled.
 *
 * Address 0x7F0BE30C (VERSION_US).
 */

Gfx* lvRender(Gfx* DL)
{
    gSPSegment(DL++, SPSEGMENT_PHYSICAL, NULL);
    gSPSegment(DL++, SPSEGMENT_UNKNOWN, osVirtualToPhysical(g_FontDl));

    gSPDisplayList(DL++, &dlFastPipelineSetup);
    gSPDisplayList(DL++, &dlZBufferGeometry);

    if (g_CurrentStageToLoad == LEVELID_TITLE)
    {
        DL = viClearZBufCurrentPlayer(DL);
        DL = viSetupCurrentPlayerView(DL);
        gDPSetScissor(DL++, G_SC_NON_INTERLACE, 0, 0, (s16)VI_SX(viGetX()), (s16)viGetY());
        DL = frontRender(DL);
    }
    else
    {
        s32 i;
        s32 pcount;

        pcount = getViewCount();

        gSPClipRatio(DL++, FRUSTRATIO_2);

        if (g_CoopMode)
        {
            coopViewsBegin();
        }

        for(i = 0; i < pcount; i++)
        {
            playerSetCurrent(playerGetShuffled(i));
            coopTankBeginPlayer();

            viSetViewSize(g_CurrentPlayer->viewx, g_CurrentPlayer->viewy);
            viSetViewPosition(g_CurrentPlayer->viewleft, g_CurrentPlayer->viewtop);
            viSetFovY(g_CurrentPlayer->fovy);
            viSetAspect(g_CurrentPlayer->aspect);

            DL = viClearZBufCurrentPlayer(DL);
            DL = viSetupCurrentPlayerView(DL);


            DL = camSetupFrame(DL);

            DL = viSetupScreensForNumPlayers(DL);
            DL = skyRender(DL);
            bgFindVisibleRooms();
            propsTickPerPlayer();
            propsSortOnscreen();
            autoaimUpdateTarget();
            shotFireHands();

            if (playerPressedActivate() && playerInteract())
            {
                gunTryReload(GUNRIGHT);
                gunTryReload(GUNLEFT);
            }

            propsTickPickups();
            DL = bgLevelRender(DL);


            if (tokenFind(1, "-stanshow_"))
            {
                StandTilePoint *tile1 = stanMatchTileName(tokenFind(1, "-stanshow_"));
                if (tile1)
                {
                    DL = stanDebugBeginDraw(DL);
                    DL = stanDebugDrawTile(DL, tile1, 0xFF0000FF);
                }
            }

            if (tokenFind(2, "-stanshow_"))
            {
                StandTilePoint *tile2 = stanMatchTileName(tokenFind(2, "-stanshow_"));
                if (tile2)
                {
                    DL = stanDebugBeginDraw(DL);
                    DL = stanDebugDrawTile(DL, tile2, 0xFF00FF);
                }
            }

            if (tokenFind(3, "-stanshow_"))
            {
                StandTilePoint *tile3 = stanMatchTileName(tokenFind(3, "-stanshow_"));
                if (tile3)
                {
                    DL = stanDebugBeginDraw(DL);
                    DL = stanDebugDrawTile(DL, tile3, 0xFFFF);
                }
            }

            DL = weaponRenderTracers(DL);

#if defined(VERSION_EU)
            movingSparksTick(&DL, ZBUF_SURFACE);
#else
            sparksRenderAll(&DL, ZBUF_SURFACE);
#endif
            DL = glassRenderShards(DL);
            DL = explosionRenderFlyingParticles(DL);

            if (

                cheatIsActive(CHEAT_INFINITE_AMMO) != 0
                && (
                    (gunGetWeapon(GUNRIGHT) != ITEM_WATCHLASER)
                    || (g_CurrentPlayer->trigger_down == 0)
                )
                )
            {
                ammoGiveMaxAll();
            }

            DL = hudRender(DL);

            DL = mpMenuRender(DL);
            coopTankEndPlayer(FALSE);
        }

        if (g_CoopMode && (pcount > 0))
        {
            coopViewsEnd(playerGetShuffled(pcount - 1));
        }
    }

    gDPSetScissor(DL++, G_SC_NON_INTERLACE, 0, 0, VI_SX(viGetX()), viGetY());

    if ((g_CurrentStageToLoad != LEVELID_TITLE) && coopSplitSideBySide())
    {
        // The divider between side by side views, drawn last so neither
        // view's sky can paint over it.
        gDPPipeSync(DL++);
        gDPSetCycleType(DL++, G_CYC_FILL);
        gDPSetRenderMode(DL++, G_RM_NOOP, G_RM_NOOP2);
        gDPSetFillColor(DL++, ((GPACK_RGBA5551(0,0,0,1) << 16) | GPACK_RGBA5551(0,0,0,1)));
        gDPFillRectangle(DL++, VI_SX(VIEWPORT_WIDTH_4P), 0, VI_SX_INCL(VIEWPORT_WIDTH_4P + 1), (viGetY() - 1));
        gDPPipeSync(DL++);
    }

    if ((g_CurrentStageToLoad != LEVELID_TITLE) && !coopSplitSideBySide() && (getViewCount() >= 2))
    {
        // The dividers between stacked or 4 way views, drawn again last: in
        // hi-res the views' sky and fills can reach over the ones each view
        // draws first.
        gDPPipeSync(DL++);
        gDPSetCycleType(DL++, G_CYC_FILL);
        gDPSetRenderMode(DL++, G_RM_NOOP, G_RM_NOOP2);
        gDPSetFillColor(DL++, ((GPACK_RGBA5551(0,0,0,1) << 16) | GPACK_RGBA5551(0,0,0,1)));
        gDPFillRectangle(DL++, 0, ((SCREEN_HEIGHT / 2) - 1), VI_SX_INCL(viGetX() - 1), (SCREEN_HEIGHT / 2));

        if (getViewCount() >= 3)
        {
            gDPFillRectangle(DL++, VI_SX((SCREEN_WIDTH / 2) - 1), 0, VI_SX_INCL(SCREEN_WIDTH / 2), (viGetY() - 1));
        }

        if (getViewCount() == 3)
        {
            gDPFillRectangle(DL++, VI_SX((SCREEN_WIDTH / 2) + 1), ((SCREEN_HEIGHT / 2) + 1), VI_SX_INCL(viGetX() - 1), (viGetY() - 1));
        }

        gDPPipeSync(DL++);
    }

    if ((g_CurrentStageToLoad != LEVELID_TITLE) && optionsGetShowFps())
    {
        DL = lvRenderFpsCounter(DL);
    }

    return DL;
}


/**
 * Sets the modifier values for the level being loaded.
 * This covers the enemy accuracy, reaction speed, and similar values.
 */
void lvApplyDifficulty(void)
{
    if (g_SelectedDifficulty == DIFFICULTY_AGENT)
    {
        f32 armorDiff = currentPlayerGetHealth() + currentPlayerGetArmor();
        f32 damageMultiplier = 1.0f;

        if (armorDiff <= 0.125f)
        {
            damageMultiplier = 0.5f;
        }
        else if (armorDiff <= 0.6f)
        {
            damageMultiplier = (((armorDiff - 0.125f) * 0.5f) / 0.47500002f) + 0.5f;
        }

        g_CctvAlarmTimeScale = 2.0f;
        g_CctvDamageScale = 2.0f;
        g_AutogunPendingDamageTick = (0.5f * damageMultiplier);
        g_AutogunDamageScalar = (0.5f * damageMultiplier);
        g_AutogunDamageScale = 2.0f;
        g_AiAccuracyModifier = DEFAULT_AGENT_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = (DEFAULT_AGENT_AI_DAMAGE_MODIFIER * damageMultiplier);
        g_AiHealthModifier = 2.0f;
        g_SpExplosionDamageMult = (f32) (0.25f * damageMultiplier);
        difficulty = 1.5f;
        g_SoloAmmoMultiplier = DEFAULT_AGENT_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_AGENT_AI_REACTION_SPEED;
    }
    else if (g_SelectedDifficulty == DIFFICULTY_SECRET)
    {
        g_CctvAlarmTimeScale = 1.0f;
        g_CctvDamageScale = 1.0f;
        g_AutogunPendingDamageTick = 0.75f;
        g_AutogunDamageScalar = 0.75f;
        g_AutogunDamageScale = 1.0f;
        g_AiAccuracyModifier = DEFAULT_SECRET_AGENT_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = DEFAULT_SECRET_AGENT_AI_DAMAGE_MODIFIER;
        g_AiHealthModifier = 1.0f;
        g_SpExplosionDamageMult = 0.75f;

        difficulty = 0.75f;

        g_SoloAmmoMultiplier = DEFAULT_SECRET_AGENT_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_SECRET_AGENT_AI_REACTION_SPEED;
    }
    else if (g_SelectedDifficulty == DIFFICULTY_00)
    {
        g_CctvAlarmTimeScale = 1.0f;
        g_CctvDamageScale = 1.0f;
        g_AutogunPendingDamageTick = 1.0f;
        g_AutogunDamageScalar = 1.0f;
        g_AutogunDamageScale = 1.0f;
        g_AiAccuracyModifier = DEFAULT_00_AGENT_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = DEFAULT_00_AGENT_AI_DAMAGE_MODIFIER;
        g_AiHealthModifier = 1.0f;
        g_SpExplosionDamageMult = 1.0f;

        difficulty = 0.2f;

        g_SoloAmmoMultiplier = DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_00_AGENT_AI_REACTION_SPEED;
    }
    else if (g_SelectedDifficulty == DIFFICULTY_007)
    {
        g_CctvAlarmTimeScale = 1.0f;
        g_CctvDamageScale = 1.0f;
        g_AutogunPendingDamageTick = 1.0f;
        g_AutogunDamageScalar = 1.0f;
        g_AutogunDamageScale = 1.0f;
        g_AiAccuracyModifier = DEFAULT_007_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = DEFAULT_007_AI_DAMAGE_MODIFIER;
        g_AiHealthModifier = 1.0f;
        g_SpExplosionDamageMult = 1.0f;
        difficulty = 1.0f;
        g_SoloAmmoMultiplier = DEFAULT_007_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_007_AI_REACTION_SPEED;
    }
}


/**
 * Multiplayer method. Manages a lot of stuff.
 * Tracks you-only-live-twice kills/deaths.
 * Lots of debug code.
 *
 * Address: 0x7F0BEB88 (NTSC).
 * Address: 0x7F0BF7AC (NTSC-J).
 * Address: 0x7F0BDFAC (PAL).
 */
void lvTick(void)
{
    if (g_ControlsLockedFlag != 0)
    {
        g_ClockTimer = 0;
    }
    else if (mpIsPaused() != 0)
    {
        g_ClockTimer = 0;
    }
    else
    {
        g_ClockTimer = g_FrameDelta;
        g_LvTickCount += 1;
    }

#ifdef VERSION_US
    g_GlobalTimerDelta = (f32) g_ClockTimer;
#else
    g_GlobalTimerDeltaRaw = (f32) g_ClockTimer;
#ifdef VERSION_EU
    g_GlobalTimerDelta = g_GlobalTimerDeltaRaw * 1.2f;
#else
    g_GlobalTimerDelta = g_GlobalTimerDeltaRaw;
#endif
#endif
    g_GlobalTimer += g_ClockTimer;
    if ((g_CurrentStageToLoad != LEVELID_TITLE) && (g_MissionTime == 0) && (g_ClockTimer > 0))
    {
        if (g_AppendCheatSinglePlayer != 0)
        {
            s32 i;
            for (i = 1; i != CHEAT_INVALID; i++)
            {
                if (g_CheatActivated[i] && !cheatIsEnemyRockets(i))
                {
                    cheatActivate(i);
                }
            }
        }
    }

    if ((isMpGame()) && (g_CurrentStageToLoad != LEVELID_TITLE))
    {
        if (levelMusicGetState() == MUSICSTATE_MP_DEATH)
        {
            s32 i;
            s32 mp_alive_count;
            s32 mp_player_field424_count;

            mp_alive_count = 0;
            mp_player_field424_count = 0;

            for (i=0; i<getPlayerCount(); i++)
            {
                if (g_playerPointers[i]->bonddead != FALSE)
                {
                    mp_alive_count++;
                    if (g_playerPointers[i]->redbloodfinished)
                    {
                        mp_player_field424_count++;
                    }
                }
            }

            if ((mp_alive_count > 0) && (mp_alive_count == mp_player_field424_count))
            {
                levelMusicSetState(MUSICSTATE_LEVEL);
            }
        }

        if (g_MpTime > 0)
        {
            s32 current_time;
            s32 sp180;
            s32 i;
            current_time = g_MissionTime;
            sp180 = g_ClockTimer + g_MissionTime;

#ifdef VERSION_EU
            if ((g_MissionTime < (g_MpTime - 0xBB8)) && (sp180 >= (g_MpTime - 0xBB8)))
#else
            if ((g_MissionTime < (g_MpTime - 0xE10)) && (sp180 >= (g_MpTime - 0xE10)))
#endif
            {
                for (i = 0; i < getPlayerCount(); i++)
                {
                    playerSetCurrent(i);
#ifdef VERSION_US
                    HUDMESSAGEBOTTOM("One minute left");
#else
                    HUDMESSAGEBOTTOM(langGet(0xB044));
#endif
                }
            }

            // sound alarm when game is about to end (10 seconds before end)
#ifdef VERSION_EU
            if ((sp180 >= (g_MpTime - 0x1F4)) && (g_MpSoundStateRelated == 0) && (lvGetControlsLocked() == 0))
#else
            if ((sp180 >= (g_MpTime - 0x258)) && (g_MpSoundStateRelated == 0) && (lvGetControlsLocked() == 0))
#endif
            {
                sndPlaySfx(g_musicSfxBufferPtr, ALARM1_SFX, &g_MpSoundStateRelated);
            }

            // stop alarm
            if (lvGetControlsLocked() != 0)
            {
                if ((g_MpSoundStateRelated != NULL) && (sndGetPlayingState(g_MpSoundStateRelated) != 0))
                {
                    sndDeactivate(g_MpSoundStateRelated);
                }
            }

            if ((current_time < g_MpTime) && (sp180 >= g_MpTime))
            {
                mpMenuCalculateAwards(FALSE);
            }
        }

        // when playing with a kill limit, g_MpPoint is not zero
        if ((g_MpPoint > 0) && (g_ClockTimer != 0))
        {
            s32 var_player_count1;
            s32 i;
            s32 mp_player_currently_in_dying_animation;
            s32 mp_players_over_point_limit;

            var_player_count1 = getPlayerCount();
            mp_player_currently_in_dying_animation = 0;
            mp_players_over_point_limit = 0;

            for (i = 0; i < var_player_count1; i++)
            {
                if (g_playerPointers[i]->bonddead != FALSE &&
                    (g_playerPointers[i]->redbloodfinished == FALSE || g_playerPointers[i]->deathanimfinished == FALSE || g_playerPointers[i]->colourfadetimemax60 >= 0.0f))
                {
                    mp_player_currently_in_dying_animation++;
                }

                if (mpGetPlayerPoints(i) >= g_MpPoint)
                {
                    // counts players over kill limit
                    mp_players_over_point_limit++;
                }
            }

            if (mp_players_over_point_limit > 0)
            {
                if (mp_player_currently_in_dying_animation == 0)
                {
                    // end game after dying players are finished dying
                    mpMenuCalculateAwards(FALSE);
                }
                else
                {
                    // this will cause the game to freeze players, to stop them from moving once game ended
                    mpMenuStopPlay();
                }
            }
        }


        // YOLT scenario: end-of-game tracking.
        if ((frontGetMpScenario() == SCENARIO_YOLT) && (g_ClockTimer != 0))
        {
            s32 player_count;
            s32 killed_count;
            s32 not_dead_count;
            s32 fully_dead_total;
            s32 killed_total;
            s32 i;
            s32 j;

            player_count = getPlayerCount();
            killed_total = 0;
            fully_dead_total = 0;

            for (i = 0; i < player_count; i++)
            {
                killed_count = 0;
                not_dead_count = 0;

                for (j = 0; j < player_count; j++)
                {
                    if (g_playerPointers[j]->bonddead == 0)
                    {
                        not_dead_count++;
                    }
                    killed_count += g_playerPlayerData[j].kill_counts[i];
                }

                if (killed_count >= 2)
                {
                    if (g_playerPlayerData[i].order_out_in_yolt == 0)
                    {
                        g_playerPlayerData[i].order_out_in_yolt = (u8) (not_dead_count + 1);
                    }

                    killed_total++;

                    if (g_playerPointers[i]->redbloodfinished
                        && g_playerPointers[i]->deathanimfinished
                        && g_playerPointers[i]->colourfadetimemax60 < 0.0f)
                    {
                        fully_dead_total++;
                    }
                }
            }

            if (fully_dead_total >= player_count - 1)
            {
                mpMenuCalculateAwards(FALSE);
            }
            else if (killed_total >= player_count - 1)
            {
                mpMenuStopPlay();
            }
        } // end YOLT

        if (0)
        {
            char debug_buf[268];
            sprintf(debug_buf, "setdetail %d %d %d %d %d %d %d %d %d", 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
    }

    g_MissionTime = g_MissionTime + g_ClockTimer;
#ifdef VERSION_EU
    g_MissionTimeSec = (f32) (g_MissionTime) / 50.0f;
#else
    g_MissionTimeSec = (f32) (g_MissionTime) / 60.0f;
#endif
    g_SystemPowerTime = g_SystemPowerTime + g_ClockTimer;
#ifdef VERSION_EU
    g_SystemPowerTimeSec = (f32) (g_SystemPowerTime) / 50.0f;
#else
    g_SystemPowerTimeSec = (f32) (g_SystemPowerTime) / 60.0f;
#endif

    if (joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON))
    {
        g_LvIdle = 0;
        g_LvTimeSinceInput = 0;
    }
    else
    {
        g_LvTimeSinceInput = g_LvTimeSinceInput + g_ClockTimer;

#ifdef VERSION_EU
        if (g_LvTimeSinceInput >= 0x5DC)
#else
        if (g_LvTimeSinceInput >= 0x708)
#endif
        {
            g_LvIdle = 1;
        }
    }

    if (g_LvIdle != 0)
    {
        g_LvIdleTime += g_ClockTimer;
    }
    else
    {
        g_StageTime = g_StageTime + g_ClockTimer;
#ifdef VERSION_EU
        g_StageTimeSec = (f32) (g_StageTime) / 50.0f;
#else
        g_StageTimeSec = (f32) (g_StageTime) / 60.0f;
#endif
        g_PowerOnTime = g_PowerOnTime + g_ClockTimer;
#ifdef VERSION_EU
        g_PowerOnTimeSec = (f32) (g_PowerOnTime) / 50.0f;
#else
        g_PowerOnTimeSec = (f32) (g_PowerOnTime) / 60.0f;
#endif
    }

    viSetUseZBuf(1);

    if (g_CurrentStageToLoad == LEVELID_TITLE)
    {
        cheatTick();
        frontTick();
    }
    else
    {
        vtxstoreTick();
        lvApplyDifficulty();
        roomMtxTick();
        waterTick();
        skyTick();
#ifdef VERSION_EU
        movingSparkCreate();
#else
        sparksTickAll();
#endif
        casingsTick();
        glassTickShards();
        explosionUpdateFlyingParticles();

        // Co-op (first step): guards and level scripts only deal with player 1.
        if (g_CoopMode)
        {
            playerSetCurrent(0);
        }

        propsTickOnce();
        levelMusicTick();
    }
}


/**
 * Moves the current player from their controller and runs their multiplayer menu.
 *
 * Multiplayer:
 * Updates distance_traveled and possibly (depending on the scenario) have_token_or_goldengun.
 *
 * US Address 0x7F0BF800.
 * EU address 7F0BEC44.
 */
void lvPlayerTick(void)
{
    s8 local_player_number;
    f32 temp_f0;
    f32 temp_f2;

    local_player_number = playerGetCurrentNum();
    cheatTick();

    playerUpdate(joyGetStickX(local_player_number), joyGetStickY(local_player_number), joyGetButtons(local_player_number, ANY_BUTTON));
    mpMenuTick();

    temp_f0 = g_CurrentPlayer->prop->pos.x - g_CurrentPlayer->bondprevpos.x;
    temp_f2 = g_CurrentPlayer->prop->pos.z - g_CurrentPlayer->bondprevpos.z;

    g_playerPerm->distance_traveled += sqrtf((temp_f0 * temp_f0) + (temp_f2 * temp_f2));

    if (frontGetMpScenario() == SCENARIO_TLD)
    {
        if (invHasFlagToken())
        {
            if (gunGetWeapon(GUNRIGHT) != ITEM_TOKEN)
            {
                gunEquip(GUNRIGHT, ITEM_TOKEN);


                if (g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state == GUN_ANIM_STATE_FIRE)
                {
                    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_SWITCH_LOWER;
                }
            }

            g_playerPerm->flag_counter += g_ClockTimer;
            g_playerPerm->have_token_or_goldengun = 1;
        }
        else
        {
            g_playerPerm->have_token_or_goldengun = 0;
        }

        return;
    }

    if (frontGetMpScenario() == SCENARIO_MWTGG)
    {
        if (invHasGoldenGun())
        {
            g_playerPerm->have_token_or_goldengun = 1;
        }
        else
        {
            g_playerPerm->have_token_or_goldengun = 0;
        }

        return;
    }
}


void lvStageUnload(void)
{
    if (g_MpSoundStateRelated != NULL)
    {
        if (sndGetPlayingState(g_MpSoundStateRelated) != AL_STOPPED)
        {
            sndDeactivate(g_MpSoundStateRelated);
        }
    }

    if (g_CurrentStageToLoad != LEVELID_TITLE)
    {
        langClearBank(langGetBankForStage(g_CurrentStageToLoad));
        playersStoreFavouriteWeapons();
    }

    cheatTurnAllOff();
    chrsFreeAll();
    objSoundsStopAll();
    explosionsReset();
    glassResetShards();
    cleanup_REMOVED_();
    alarmsReset();
    setupFreeProps(g_CurrentStageToLoad);
    objectivesFinish();
    levelStopTankSfx();
    gunsStopSounds();
    levelMusicStop();
    bgReset();
}


void lvSetControlsLocked(s32 arg0)
{
    if ((arg0 != 0) && (g_ControlsLockedFlag == 0))
    {
        joyRumblePakStop();
    }

    g_ControlsLockedFlag = arg0;
}


s32 lvGetControlsLocked(void)
{
    return g_ControlsLockedFlag;
}


DIFFICULTY lvGetDifficulty(void)
{
    return g_SelectedDifficulty;
}


void lvSetDifficulty(DIFFICULTY arg0)
{
    g_SelectedDifficulty = arg0;
}

void lvSetMpTimeLimit(s32 arg0)
{
    g_MpTime = arg0;
}


void lvSetMpPointLimit(s32 arg0)
{
    g_MpPoint = arg0;
}


f32 lvGetMissionTimeSec(void)
{
    return g_MissionTimeSec;
}


f32 lvGetSystemPowerTimeSec(void)
{
    return g_SystemPowerTimeSec;
}


void cleanup_REMOVED_(void){

}
