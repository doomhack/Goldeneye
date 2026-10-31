/**
 * The HUD drawn over each player's view every frame (hudRender): gun hands, sight, blood,
 * casings, the watch, health bars, ammo, messages, fades and the end-of-level credits.
 * hudRender also runs the death and co-op respawn sequence, since it is timed by what is
 * on screen. See docs/subsystems/player.md.
 */
#include "game/hud/hud.h"
#include "game/audio/levelmusic.h"
#include "game/gfx/dyn.h"
#include "game/hud/blood.h"
#include "game/hud/healthbar.h"
#include "game/hud/hudmsg.h"
#include "game/hud/radar.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/math/trig.h"
#include "game/menu/cheat.h"
#include "game/menu/front.h"
#include "game/menu/mpmenu.h"
#include "game/mission/objective.h"
#include "game/model/model.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/prop/alarm.h"
#include "game/text/language.h"
#include "game/text/text.h"
#include "game/world/bg.h"
#include <ultra64.h>
#include <bondtypes.h>
#include <boss.h>
#include <fr.h>
#include <joy.h>
#include <math.h>
#include <music.h>
#include <str.h>

s32 g_CreditsScrollFrame = 0;

s32 credits_state = 0;

CreditsEntry *credits_pointer = NULL;


/**
 * Renders the in-game health and armor gauges.
 * The watch menu gauges are handled by watchTrigger().
 */
Gfx *hudRenderHealthBars(Gfx *gdl)
{
    Mtx *lookatmtx;
    Mtx *orthomtx;
    Mtxf lookatmtxf;

    //Set up armor bars.
    hudMakeDamageSegments(g_CurrentPlayer->armor_display_values, 1, g_CurrentPlayer->apparentarmour);
    hudRenderGaugeBar(g_CurrentPlayer->watchArmorGdl, g_CurrentPlayer->armor_display_values, 0x2e);

    // Set up health bars.
    hudMakeDamageSegments(g_CurrentPlayer->health_display_values, -1, g_CurrentPlayer->apparenthealth);
    hudRenderGaugeBar(g_CurrentPlayer->watchHealthGdl, g_CurrentPlayer->health_display_values, 0x2e);

    // Create an orthographic render state for the gauge.
    lookatmtx = dynAllocateMatrix();
    orthomtx = dynAllocateMatrix();

    guOrtho(orthomtx, -800.0f * g_CamPosScale, 800.0f * g_CamPosScale, -600.0f * g_CamPosScale, 600.0f * g_CamPosScale, -100.0f, 1000.0f, 1.0f);

    gSPMatrix(gdl++, osVirtualToPhysical(orthomtx), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    mtx4LoadLookAt(
        &lookatmtxf,
        0.0f, 500.0f, 0.0f,
        0.0f, 0.0f,   0.0f,
        0.0f, 0.0f,  -1.0f
    );

    mtxF2L(&lookatmtxf, (Mtxf *)lookatmtx);

    gSPMatrix(gdl++, osVirtualToPhysical(lookatmtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);

    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gDPSetPrimColor(gdl++, 0, 0, 0xe6, 0xe6, 0xe6, 0x00);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH);

    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(g_CurrentPlayer->watchArmorGdl));
    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(g_CurrentPlayer->watchHealthGdl));

    gSPMatrix(gdl++, osVirtualToPhysical(camGetProjectionMtx()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    return gdl;
}


Gfx *hudRenderCredits(Gfx *gdl)
{
    s32 frame;
    s32 start;
    s32 x;
    s32 y;
    s32 xpos1;
    s32 xpos2;
    s32 textheight;
    s32 textwidth;
    s32 i;
    s32 end;
    s32 x2;
    s32 entrysize;
    s32 align1;
    s32 align2;
    s16 viewheight;
    char *text;

    if (bossGetStageNum() == LEVELID_CUBA && credits_state == 1 && credits_pointer != NULL)
    {
        xpos1 = 0xdc;
        xpos2 = 0xdc;
        align1 = CREDITS_ALIGN_RIGHT;
        align2 = CREDITS_ALIGN_RIGHT;
        g_CreditsScrollFrame++;
        frame = g_CreditsScrollFrame;
        gdl = textSetupDraw(gdl);
        start = (frame - viGetViewHeight()) / 16;
        end = (frame / 16) + 1;

        if (start < 0)
        {
            start = 0;
        }

        for (i = 0; i < start; i++)
        {
            if (credits_pointer[i].TextId1 == 0 && credits_pointer[i].TextId2 == 0)
            {
                end = i;
                start = i;
                credits_state = 2;
                break;
            }

            if (credits_pointer[i].TextId1 != 0x5011)
            {
                if (credits_pointer[i].Position1 >= 0)
                {
                    xpos1 = credits_pointer[i].Position1;
                }

                if ((s16) credits_pointer[i].Alignment1 >= CREDITS_ALIGN_RIGHT)
                {
                    align1 = (s16) credits_pointer[i].Alignment1;
                }
            }

            if (credits_pointer[i].TextId2 != 0x5011)
            {
                if (credits_pointer[i].Position2 >= 0)
                {
                    xpos2 = credits_pointer[i].Position2;
                }

                if ((s16) credits_pointer[i].Alignment2
                    >= CREDITS_ALIGN_RIGHT)
                {
                    align2 = (s16) credits_pointer[i].Alignment2;
                }
            }
        }

        for (i = start; i < end && (credits_pointer[i].TextId1 || credits_pointer[i].TextId2); i++)
        {
            if ((u32) credits_pointer[i].TextId1 != 0x5011)
            {
                text = langGet(credits_pointer[i].TextId1);

                if (credits_pointer[i].Position1 >= 0)
                {
                    xpos1 = credits_pointer[i].Position1;
                }

                if ((s16) credits_pointer[i].Alignment1 >= CREDITS_ALIGN_RIGHT)
                {
                    align1 = (s16) credits_pointer[i].Alignment1;
                }

                viewheight = viGetViewHeight();
                y = ((viGetViewTop() + (i * 16)) - frame) + viewheight;
                textheight = 0;
                textwidth = 0;

                textMeasure(&textheight, &textwidth, text, g_FontZurichChars, g_FontZurich, 0);

                entrysize = y + textheight;

                if (align1 == CREDITS_ALIGN_LEFT)
                {
                    x = xpos1 - textwidth;
                    x2 = xpos1;
                }
                else if (align1 == CREDITS_ALIGN_CENTER)
                {
                    x = xpos1 - (textwidth >> 1);
                    x2 = x + textwidth;
                }
                else
                {
                    x = xpos1;
                    x2 = xpos1 + textwidth;
                }

                gdl = textRenderRect(gdl, x, y - 1, x2 + 1, entrysize + 1, 0);

                gdl = textRender(gdl, &x, &y, text, g_FontZurichChars, g_FontZurich, -1, viGetX(), viGetY(), 0, 0);
            }

            if (credits_pointer[i].TextId2 != 0x5011)
            {
                text = langGet(credits_pointer[i].TextId2);

                if (credits_pointer[i].Position2 >= 0)
                {
                    xpos2 = credits_pointer[i].Position2;
                }

                if ((s16) credits_pointer[i].Alignment2 >= CREDITS_ALIGN_RIGHT)
                {
                    align2 = (s16) credits_pointer[i].Alignment2;
                }

                viewheight = viGetViewHeight();
                y = ((viGetViewTop() + (i * 16)) - frame) + viewheight;
                textheight = 0;
                textwidth = 0;

                textMeasure(&textheight, &textwidth, text, g_FontZurichChars, g_FontZurich, 0);

                entrysize = y + textheight;

                if (align2 == CREDITS_ALIGN_LEFT)
                {
                    x = xpos2 - textwidth;
                    x2 = xpos2;
                }
                else if (align2 == CREDITS_ALIGN_CENTER)
                {
                    x = xpos2 - (textwidth >> 1);
                    x2 = x + textwidth;
                }
                else
                {
                    x = xpos2;
                    x2 = xpos2 + textwidth;
                }

                gdl = textRenderRect(gdl, x, y - 1, x2 + 1, entrysize + 1, 0);

                gdl = textRender(gdl, &x, &y, text, g_FontZurichChars, g_FontZurich, -1, viGetX(), viGetY(), 0, 0);
            }
        }

        gdl = textEndDraw(gdl);
    }

    return gdl;
}


Gfx *hudRender(Gfx *gdl)
{
    s32 ulx;
    s32 uly;
    s32 lrx;
    s32 lry;
    s32 doblood;
    s32 scenario;
    s32 cur;
    s32 count;
    s32 i;
    s32 total;

    if (g_CurrentPlayer->cameramode == CAMERAMODE_INTRO)
    {
        hudmsgTickIntroText();
        gdl = hudmsgBottomRender(gdl);
        hudmsgTopTick();
        gdl = hudmsgTopRender(gdl);
        gdl = countdownTimerRender(gdl);
        gdl = currentPlayerDrawFade(gdl);
        return hudRenderCredits(gdl);
    }

    if (g_CounterOpMode)
    {
        counterOpTick();
    }

    gunUpdateAndFireBothHands();
    casingsRender(&gdl);
    gunRenderHands(&gdl);
    gdl = watchRender(gdl);

    if (g_CurrentPlayer->mpmenuon != 0)
    {
        ulx = viGetViewLeft();
        uly = viGetViewTop();
        lrx = viGetViewLeft() + viGetViewWidth();
        lry = viGetViewTop() + viGetViewHeight();
        gdl = textSetupDraw(gdl);
        gdl = textRenderRect(gdl, ulx, uly, lrx, lry, 160);
    }

    if (playerIsShowingHealth() &&
        (g_CurrentPlayer->watch_animation_state == 0))
    {
        gdl = hudRenderHealthBars(gdl);
    }
    else if (mpMenuShowGauges())
    {
        gdl = hudRenderHealthBars(gdl);
        if (g_CurrentPlayer->healthdisplaytime > 0)
        {
            g_CurrentPlayer->healthdisplaytime -= g_ClockTimer;
        }
        if (g_CurrentPlayer->healthdisplaytime < 0)
        {
            g_CurrentPlayer->healthdisplaytime = 0;
        }
    }

    if (!isMpGame())
    {
        objectivesTickStatusMessages();
    }

    if (g_CurrentPlayer->bonddead != 0)
    {
        if (g_CurrentPlayer->deathanimfinished == 0)
        {
            doblood = 0;
            if (g_CurrentPlayer->bonddead == 1)
            {
                doblood                   = 1;
                g_CurrentPlayer->bonddead = 2;
            }
            if (doblood)
            {
                bloodNextFrame(0);
                if (coopPartnerAlive())
                {
                    // Co-op: the partner plays on, so keep the music and
                    // mission going.
                }
                else if (!isMpGame())
                {
                    // Solo death: stop the level's music and play the death tune
                    levelMusicSlotStop(-1);
                    levelMusicSetState(0);
                    musicTrack1ApplySeqpVol(levelMusicGetVolume());
                    g_musicXTrack1Fade = 0;
                    musicTrack2ApplySeqpVol(0);
                    g_musicXTrack2Fade = 0;
                    musicTrack1Play(27);
                }
                else
                {
                    levelMusicSetState(6);
                }
            }
            else
            {
                if (g_CurrentPlayer->redbloodfinished)
                {
                    currentPlayerSetFadeColour(150, 0, 0, 0.7058824f);
                }
                else
                {
                    if (g_ClockTimer > 0)
                    {
                        doblood = 1;
                    }
                    else
                    {
                        doblood = 2;
                    }
                    if (bloodNextFrame(doblood))
                    {
                        g_CurrentPlayer->redbloodfinished = TRUE;
                    }
                    gdl = bloodRender(gdl);
                }
            }
        }
        if (modelGetAnimFrame(&g_CurrentPlayer->model) >=
            modelGetAnimEndFrame(&g_CurrentPlayer->model))
        {
            if (g_CurrentPlayer->redbloodfinished)
            {
                if (!g_CurrentPlayer->deathanimfinished)
                {
                    g_CurrentPlayer->deathanimfinished = TRUE;
                    currentPlayerAdjustFade(60.0f, 0, 0, 0, 1.0f);
                    currentPlayerStartChrFade(120.0f, 0.0f);
                }
                if (currentPlayerIsFadeComplete())
                {
                    if (coopPartnerAlive())
                    {
                        if (((g_GlobalTimer - g_CoopDeathTime[playerGetCurrentNum()]) >= COOP_RESPAWN_DELAY60)
                            && coopPartnerCanRevive()
                            && joyGetButtons(playerGetCurrentNum(), 0xB000))
                        {
                            if (!g_CounterOpMode)
                            {
                                coopRespawnPlayer();
                                g_CoopRespawnButtonsHeld[playerGetCurrentNum()] = TRUE;
                            }
                            else if (counterOpTakeOverGuard())
                            {
                                g_CoopRespawnButtonsHeld[playerGetCurrentNum()] = TRUE;
                            }
                        }
                    }
                    else if (!isMpGame())
                    {
                        // Co-op: the replay shows the player who died last,
                        // so wait for their death to play out.
                        if (!g_CoopMode || coopDiedLast())
                        {
                            playerSetCameraMode(CAMERAMODE_DEATH_CAM_SP);
                        }
                    }
                    else
                    {
                        scenario = frontGetMpScenario();
                        cur      = playerGetCurrentNum();
                        count    = getPlayerCount();
                        total    = 0;
                        for (i = 0; i < count; i++)
                        {
                            total += g_playerPlayerData[i].kill_counts[cur];
                        }
                        if ((scenario != SCENARIO_YOLT) || (total < 2))
                        {
                            if (joyGetButtons(playerGetCurrentNum(), 0xB000))
                            {
                                mpRespawnPlayer();
                            }
                        }
                    }
                }
            }
        }
    }

    hudmsgTickIntroText();
    gdl = hudmsgBottomRender(gdl);
    hudmsgTopTick();
    gdl = hudmsgTopRender(gdl);
    gunDrawSight(&gdl);
    gdl = hudRenderAmmo(gdl);
    gdl = countdownTimerRender(gdl);
    gdl = radarRender(gdl);
    return currentPlayerDrawFade(gdl);
}
