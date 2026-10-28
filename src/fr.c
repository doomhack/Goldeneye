#include <ultra64.h>
#include "vi.h"
#include "fr.h"
#include <bondgame.h>
#include <bondconstants.h>
#include "game/player/player.h" // g_CurrentPlayer
#include "sched.h"
#include "game/gfx/rsp.h"
#include <PR/os.h>
#include "game/player/bondview.h"
#include "game/gfx/zbuf.h"
#include "game/gfx/dyn.h"

/**
 * EU .data, offset from start of data_seg : 0x2484
*/

/**
 * @file video.c
 * This file contains video handling code.
 */

// same codegen as (x & 0xffff)
#define TO_U16_A(x) ((u16)(x))
// same codegen as ((u16)(x))
#define TO_U16_B(x) (x & 0xffff)

#define TO_U16_C(x) ((u16)(x & 0xffff))

#define TO_U16_M(x) ((x) % 0xffff)

/**
 * Splits a 32 bit value into upper and lower 16, adds a 16 bit value to each, and combines back to 32 bits.
 *
 * Note: both unsigned 16 bit casts result in the same codegen, but I'm using different
 * macros to give a more "natural" explanation of the asm codegen.
 */
#define ADD_LOW_AND_HI_16_TRUNCATE(x32, add16) ((TO_U16_B(TO_U16_A((x32) >> 16) + (add16)) << 16) | TO_U16_B(TO_U16_A(x32) + (add16)))

#define ADD_LOW_AND_HI_16_MOD(x32, add16) ((TO_U16_M(TO_U16_B((x32) >> 16) + (add16)) << 16) | TO_U16_M(TO_U16_A(x32) + (add16)))

#if defined(VERSION_EU)
struct VideoSettings_s g_ViDataArray[NUM_VIDEO_SETTINGS] =
{
    {0, 0, 0, 0, 320, 272, 60.0f, 1.17647063732f, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 320, 272, 0, 0, TRUE, NULL, 1},
    {0, 0, 0, 0, 320, 272, 60.0f, 1.17647063732f, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 320, 272, 0, 0, TRUE, NULL, 1}
};
#else
struct VideoSettings_s g_ViDataArray[NUM_VIDEO_SETTINGS] =
{
    {MD_BLACK, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FOV_Y_F, ASPECT_RATIO, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, TRUE, NULL, 1},
    {MD_BLACK, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FOV_Y_F, ASPECT_RATIO, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, TRUE, NULL, 1}};
#endif

/**
 * vimode hStart horizontal offset in viUpdateMode.
 */
s32 g_viHorizontalOffset = 0;

/**
 * vimode vStart vertical offset in viUpdateMode.
 * (see g_ViShakeIntensity before changing the name)
 */
s32 g_viVerticalOffset = 0;

VideoSettings *g_ViFrontData = &g_ViDataArray[0];
VideoSettings *g_ViBackData = &g_ViDataArray[0];
s32 g_viColorOutputMode = COLORMODE_16BIT;

/**
 * g_ViShakeIntensity multiplier sign. This is only ever 1 or -1.
 */
s32 g_viVstartSign = 1;

/**
 * vimode vStart vertical offset in viVsyncRelated.
 * (see g_viVerticalOffset before changing the name)
 */
s32 g_ViShakeIntensity = 0;

/**
 * Some kind of counter for how frequently g_ViShakeIntensity is zero'd in viVsyncRelated.
 */
u32 g_ViShakeTimer = 0;

/**
 * Some kind of counter for how frequently osViBlack is called with "active" parameter.
 */
u32 g_ViUnblackTimer = 3;

/**
 * should correlate to g_schedViCurrentFrameBuffer
 */
s32 g_ViNextModeSlot = 0;

f32 g_viProjectionMatrixF[4][4];
Mtx *g_viProjectionMatrix;
u16 g_viPerspNorm;

/**
 * Original vi mode.
 * (name following n64devkit\ultra\usr\src\pr\demos\threadmonkey\block.c)
 */
OSViMode g_viOriginalVideoMode;

/**
 * cfb_16 index for g_ViFrontData.
 */
u8 g_ViFrontIndex;

/**
 * cfb_16 index for g_ViBackData and g_CurrentPlayer->viewports.
 */
u8 g_ViBackIndex;

/**
 * Current vi mode pointer.
 * EU .bss 0x80053E4C
 */
OSViMode *viMode;

/**
 * Original viMode->comRegs.hStart.
 * EU .bss 0x80053E50
 */
u32 g_viOriginalHstart;

/**
 * Original viMode->fldRegs[0].vStart.
 * EU .bss 0x80053E54
 */
u32 g_viOriginalVstart0;

/**
 * Original viMode->fldRegs[1].vStart.
 * EU .bss 0x80053E58
 */
u32 g_viOriginalVstart1;

/**
 * Const runtime supported screen widths.
 */
const s16 g_viRuntimeScreenWidths[] = {SCREEN_WIDTH_MIN, SCREEN_WIDTH, SCREEN_WIDTH_MAX};

/**
 * Const runtime supported screen heights.
 */
#ifndef VERSION_EU
const s16 g_viRuntimeScreenHeights[] = {SCREEN_HEIGHT_MIN, SCREEN_HEIGHT, SCREEN_HEIGHT_MAX};
#else
const s16 g_viRuntimeScreenHeights[] = {SCREEN_HEIGHT_272, SCREEN_HEIGHT_272, SCREEN_HEIGHT_MAX_EU};
#endif

void viInitVideoSettings(void)
{
    g_ViFrontIndex = 0;
    g_ViBackIndex = 1;

    g_ViFrontData = (VideoSettings*)((u8*)&g_ViDataArray + (g_ViFrontIndex * sizeof(VideoSettings))); //AKA front = viDataArray[0]
    g_ViFrontData->framebuf = cfb_16[g_ViFrontIndex];

    g_ViBackData = (VideoSettings*)((u8*)&g_ViDataArray + (g_ViBackIndex * sizeof(VideoSettings))); //AKA back = viDataArray[1] - was Rare mental?
    g_ViBackData->framebuf = cfb_16[g_ViBackIndex];

    g_viHorizontalOffset = 0;
    #ifndef VERSION_EU
    g_viVerticalOffset = 0;
    #else
    g_viVerticalOffset = 0xffffffe0;
    #endif
}

void viInitBuffers(void)
{
    s32 i;
    u8* p1 = cfb_16[0];
    u8* p2 = cfb_16[1];

    zbufDeallocate();

    for (i = 0; i < (s32) sizeof(cfb_16[0]); i++)
    {
        p1[i] = 0;
        p2[i] = 0;
    }
}

void frBlack(u32 black)
{
    black += 2;
    g_ViUnblackTimer = black;
}

/**
 * Calls osViSetMode with current viMode.
 * Also calls osViBlack.
 * Applies verticalOffset to vstart.
 *
 * Looks related to n64devkit\ultra\usr\src\pr\demos\blockmonkey\block.c
 * in particular, ModifyVStart
 */
void viVsyncRelated(void)
{
    s32 verticalOffset;
    s32 registerValue;

    if (g_ViShakeTimer > 0)
    {
        g_ViShakeTimer--;

        if (g_ViShakeTimer == 0)
        {
            g_ViShakeIntensity = 0;
        }
    }

    verticalOffset = g_viVstartSign * g_ViShakeIntensity;

    // note the implicit cast from unsigned to signed.
    registerValue = g_viOriginalVstart0;
    (*viMode).fldRegs[0].vStart = ADD_LOW_AND_HI_16_TRUNCATE(registerValue, verticalOffset);

    registerValue = g_viOriginalVstart1;
    (*viMode).fldRegs[1].vStart = ADD_LOW_AND_HI_16_TRUNCATE(registerValue, verticalOffset);

    osViSetMode(viMode);
    osViBlack(g_ViUnblackTimer);

    // g_ViUnblackTimer might be declared signed which means the if statement is
    //
    //    if (g_ViUnblackTimer != 0 && g_ViUnblackTimer < 3)
    //
    // but this way feels "safer", though the cast to (s32) now seems unnatural.
    // The "right" answer depends on what's supposed to happen for values like -1 or 0xffffffff ...
    if (g_ViUnblackTimer > 0 && (s32)g_ViUnblackTimer < 3)
    {
        g_ViUnblackTimer--;
    }

    //smooth dither pattern and disable weird gamma
    //(N64 was supposed to compensate for CRT's 2.2 gamma, but by this time most games were made with sRGB in mind)
    osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON | OS_VI_GAMMA_OFF);

    g_viVstartSign = -g_viVstartSign;
}


#ifndef VERSION_EU
void viUpdateMode(void)
{
    u32 packedStart;
    s32 registerValue;
    f32 calculatedYScale;
    s32 yScaleMax;
    f32 calculatedXScale;
    VideoSettings *settings;
    s32 nextMode;

    if (g_ViFrontData->mode != g_ViBackData->mode)
    {
        registerValue = g_ViBackData->mode;

        switch (registerValue)
        {
            case MD_BLACK:
            {

                osViSetYScale(1.0f);
                osViBlack(TRUE);
                break;
            }

            case MD_NORMAL:
            {
                if (g_viColorOutputMode != COLORMODE_32BIT)
                {
                    if (osTvType == TV_TYPE_MPAL)
                    {
                        g_viOriginalVideoMode = osViModeTable[OS_VI_MPAL_LAN1];
                    }
                    else
                    {
                        g_viOriginalVideoMode = osViModeTable[OS_VI_NTSC_LAN1];
                    }
                }
                else
                {
                    if (osTvType == TV_TYPE_MPAL)
                    {
                        g_viOriginalVideoMode = osViModeTable[OS_VI_MPAL_LAN2];
                    }
                    else
                    {
                        g_viOriginalVideoMode = osViModeTable[OS_VI_NTSC_LAN2];
                    }
                }

                viMode = &g_viOriginalVideoMode;
                g_viOriginalHstart = viMode->comRegs.hStart;
                g_viOriginalVstart0 = viMode->fldRegs[0].vStart;
                g_viOriginalVstart1 = viMode->fldRegs[1].vStart;

                frBlack(0);
                break;
            }

            case MD_MAXIMUM:
                if (osTvType == TV_TYPE_MPAL)
                {
                    viMode = &osViModeTable[OS_VI_MPAL_HAF1];
                }
                else
                {
                    viMode = &osViModeTable[OS_VI_NTSC_HAF1];
                }

                g_viOriginalHstart = viMode->comRegs.hStart;
                g_viOriginalVstart0 = viMode->fldRegs[0].vStart;
                g_viOriginalVstart1 = viMode->fldRegs[1].vStart;

                osViBlack(FALSE);
                break;
        }

        osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON | OS_VI_GAMMA_OFF);
    }

    calculatedXScale = (f32)g_ViBackData->x / (f32)g_ViBackData->bufx;
    calculatedYScale = (f32)g_ViBackData->y / (f32)g_ViBackData->bufy;

    /* Keep these statements on the same logical source line for IDO scheduling. */
    if (g_ViBackData->mode == MD_BLACK) \
    { \
        calculatedYScale = 1.0f; \
    } nextMode = g_ViNextModeSlot;

    yScaleMax = YSCALE_MAX;


    g_ViXScales[nextMode] = calculatedXScale;
    g_ViYScales[nextMode] = calculatedYScale;

    if (g_ViBackData->mode == VIDEOMODE_320x240)
    {
        g_ViModePtrs[nextMode] = &g_viOriginalVideoMode;

        if (g_viColorOutputMode != COLORMODE_32BIT)
        {
            if (osTvType == TV_TYPE_MPAL)
            {
                g_ViModes[nextMode] = osViModeTable[OS_VI_MPAL_LAN1];
            }
            else
            {
                g_ViModes[nextMode] = osViModeTable[OS_VI_NTSC_LAN1];
            }
        }
        else
        {
            if (osTvType == TV_TYPE_MPAL)
            {
                g_ViModes[nextMode] = osViModeTable[OS_VI_MPAL_LAN2];
            }
            else
            {
                g_ViModes[nextMode] = osViModeTable[OS_VI_NTSC_LAN2];
            }
        }

        g_ViModes[nextMode].comRegs.width = VI_SX(g_ViBackData->bufx);
        g_ViModes[nextMode].comRegs.xScale = (VI_SX(g_ViBackData->bufx) * XSCALE_MAX) / SCREEN_WIDTH_MAX;

        if (1)
        {
            g_ViModes[nextMode].fldRegs[0].yScale = (g_ViBackData->bufy * yScaleMax) / SCREEN_HEIGHT_MAX;
        }

        packedStart = g_ViModes[nextMode].comRegs.hStart;

        g_ViModes[nextMode].fldRegs[1].yScale = (g_ViBackData->bufy * yScaleMax) / SCREEN_HEIGHT_MAX;
        g_ViModes[nextMode].fldRegs[0].origin = VI_SX(g_ViBackData->bufx) * 2;
        g_ViModes[nextMode].fldRegs[1].origin = VI_SX(g_ViBackData->bufx) * 2;

        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, g_viHorizontalOffset);

        g_ViModes[nextMode].comRegs.hStart = registerValue;
        g_viOriginalHstart = registerValue;

        packedStart = g_ViModes[nextMode].fldRegs[0].vStart;
        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, g_viVerticalOffset);

        g_ViModes[nextMode].fldRegs[0].vStart = registerValue;
        g_viOriginalVstart0 = registerValue;

        packedStart = g_ViModes[nextMode].fldRegs[1].vStart;
        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, g_viVerticalOffset);

        g_ViModes[nextMode].fldRegs[1].vStart = registerValue;
        g_viOriginalVstart1 = registerValue;

        g_ViChangeVideoModes[nextMode] = 1;
    }
    else
    {
        g_ViChangeVideoModes[nextMode] = 0;
    }

    nextMode = (nextMode + 1) % NUM_VIDEO_FRAME_BUFFERS;
    g_ViNextModeSlot = nextMode;

    if (g_viColorOutputMode != COLORMODE_32BIT)
    {
        ((struct GfxInfo_s *)g_gfxTaskSettingsList)->cfb = g_ViBackData->framebuf;
    }
    else
    {
        ((struct GfxInfo_s *)g_gfxTaskSettingsList)->cfb = cfb_16[0];
    }

    settings = g_ViBackData;

    g_ViFrontIndex = (g_ViFrontIndex + 1) % NUM_VIDEO_SETTINGS;
    g_ViBackIndex = (g_ViBackIndex + 1) % NUM_VIDEO_SETTINGS;

    g_ViFrontData = g_ViDataArray + g_ViFrontIndex;
    g_ViBackData = g_ViDataArray + g_ViBackIndex;

    bcopy(settings, g_ViBackData, sizeof(VideoSettings));

    g_ViBackData->framebuf = cfb_16[g_ViBackIndex];
}
#endif
#ifdef VERSION_EU
void viUpdateMode(void)
{
    s32 origStart;
    s32 registerValue;
    f32 calculatedYScale;
    f32 calculatedXScale;
    VideoSettings *settings;
    s32 modeIndex;
    s32 upperAdjust;

    if (g_ViFrontData->mode != g_ViBackData->mode)
    {
        registerValue = g_ViBackData->mode;

        switch (registerValue)
        {
            case 0:
                osViSetYScale(1.0f);
                osViBlack(1);
                break;

            case 1:
                if (g_viColorOutputMode != 0)
                {
                    g_viOriginalVideoMode = osViModeTable[16];
                }
                else
                {
                    g_viOriginalVideoMode = osViModeTable[20];
                }

                viMode = &g_viOriginalVideoMode;
                g_viOriginalHstart = viMode->comRegs.hStart;
                g_viOriginalVstart0 = viMode->fldRegs[0].vStart;
                g_viOriginalVstart1 = viMode->fldRegs[1].vStart;

                frBlack(0);
                break;

            case 2:
                viMode = &osViModeTable[25];
                g_viOriginalHstart = viMode->comRegs.hStart;
                g_viOriginalVstart0 = viMode->fldRegs[0].vStart;
                g_viOriginalVstart1 = viMode->fldRegs[1].vStart;

                osViBlack(0);
                break;
        }
        osViSetSpecialFeatures(0x0040 | 0x0002);
    }

    calculatedXScale = ((f32)g_ViBackData->x) / ((f32)g_ViBackData->bufx);
    calculatedYScale = ((f32)g_ViBackData->y) / ((f32)g_ViBackData->bufy);

    if (g_ViBackData->mode == 0)
    {
        calculatedYScale = 1.0f;
    }

    modeIndex = g_ViNextModeSlot;

    g_ViXScales[modeIndex] = calculatedXScale;
    g_ViYScales[modeIndex] = calculatedYScale;

    if (g_ViBackData->mode == 1)
    {
        g_ViModePtrs[modeIndex] = &g_viOriginalVideoMode;

        if (g_viColorOutputMode != 0)
        {
            g_ViModes[modeIndex] = osViModeTable[16];
        }
        else
        {
            g_ViModes[modeIndex] = osViModeTable[20];
        }

        g_ViModes[modeIndex].comRegs.width = VI_SX(g_ViBackData->bufx);
        g_ViModes[modeIndex].comRegs.xScale = (VI_SX(g_ViBackData->bufx) * 0x400) / 640;
        registerValue = (g_ViBackData->bufy == 330) ? (28) : (0);
        upperAdjust = 0;
        g_ViModes[modeIndex].fldRegs[0].yScale = (g_ViBackData->bufy * 0x800) / (registerValue + 0x220);
        registerValue = (g_ViBackData->bufy == 330) ? (28) : (0);
        g_ViModes[modeIndex].fldRegs[1].yScale = (g_ViBackData->bufy * 0x800) / (registerValue + 0x220);
        g_ViModes[modeIndex].fldRegs[0].origin = VI_SX(g_ViBackData->bufx) * 2;
        g_ViModes[modeIndex].fldRegs[1].origin = VI_SX(g_ViBackData->bufx) * 2;

        origStart = g_ViModes[modeIndex].comRegs.hStart;
        registerValue = (((((origStart >> 16) & 0xffff) + g_viHorizontalOffset) % 0xffff) << 16) |
                        ((((u16)origStart) + g_viHorizontalOffset) % 0xffff);
        g_ViModes[modeIndex].comRegs.hStart = registerValue;
        g_viOriginalHstart = registerValue;

        origStart = g_ViModes[modeIndex].fldRegs[0].vStart;
        upperAdjust = (g_ViBackData->bufy == 330) ? (-14) : (0);
        registerValue =
            ((((((g_ViBackData->bufy == 330) ? (14) : (0)) + ((u16)origStart)) + g_viVerticalOffset) + 0x40) % 0xffff) |
            ((((((origStart >> 16) & 0xffff) + g_viVerticalOffset) + upperAdjust) % 0xffff) << 16);
        g_ViModes[modeIndex].fldRegs[0].vStart = registerValue;
        g_viOriginalVstart0 = registerValue;

        origStart = g_ViModes[modeIndex].fldRegs[1].vStart;
        upperAdjust = (g_ViBackData->bufy == 330) ? (-14) : (0);
        registerValue =
            ((((((g_ViBackData->bufy == 330) ? (14) : (0)) + ((u16)origStart)) + g_viVerticalOffset) + 0x40) % 0xffff) |
            ((((((origStart >> 16) & 0xffff) + g_viVerticalOffset) + upperAdjust) % 0xffff) << 16);
        g_ViModes[modeIndex].fldRegs[1].vStart = registerValue;
        g_viOriginalVstart1 = registerValue;

        g_ViChangeVideoModes[modeIndex] = 1;
    }
    else
    {
        g_ViChangeVideoModes[modeIndex] = 0;
    }

    modeIndex = (modeIndex + 1) % 2;
    g_ViNextModeSlot = modeIndex;

    if (g_viColorOutputMode != 0)
    {
        ((struct GfxInfo_s *)g_gfxTaskSettingsList)->cfb = g_ViBackData->framebuf;
    }
    else
    {
        ((struct GfxInfo_s *)g_gfxTaskSettingsList)->cfb = cfb_16[0];
    }

    settings = g_ViBackData;

    g_ViFrontIndex = (g_ViFrontIndex + 1) % 2;
    g_ViBackIndex = (g_ViBackIndex + 1) % 2;

    g_ViFrontData = g_ViDataArray + g_ViFrontIndex;
    g_ViBackData = g_ViDataArray + g_ViBackIndex;

    bcopy(settings, g_ViBackData, sizeof(VideoSettings));

    g_ViBackData->framebuf = cfb_16[g_ViBackIndex];
}
#endif


void viShake(f32 intensity)
{
    if (intensity > 14.0f)
    {
        intensity = 14.0f;
    }

    if (intensity < 0.0f)
    {
        intensity = 0.0f;
    }

    g_ViShakeIntensity = (s32)intensity;
    g_ViShakeTimer = 10;
}

void viSetVideoMode(s32 m)
{
    //if (m < 0) {
    //    assertPrint_8291E690(".\\ported\\fr.cpp",0x22a,"Assertion failed: m>=MD_BLACK");
    //}
    //if (2 < iStack00000014) {
    //    assertPrint_8291E690(".\\ported\\fr.cpp",0x22b,"Assertion failed: m<MD_MAXIMUM");
    //}
    g_ViBackData->mode = m;
    g_ViBackData->x = g_ViBackData->bufx = g_viRuntimeScreenWidths[m];
    g_ViBackData->y = g_ViBackData->bufy = g_viRuntimeScreenHeights[m];
    g_ViBackData->scalex = 1;
}

u8 *viGetFrameBuf2(void)
{
    return g_ViBackData->framebuf;
}

void viSetFrameBuf2(u8 *buf)
{
    g_ViBackData->framebuf = buf;
}

u16 viGetPerspNorm(void)
{
    return g_viPerspNorm;
}


Gfx *viSetupCurrentPlayerView(Gfx *gdl)
{
    if (g_CurrentPlayer != NULL)
    {
        g_CurrentPlayer->viewports[g_ViBackIndex].vp.vscale[0] = (VI_SX(g_ViBackData->viewx) * 2);
        g_CurrentPlayer->viewports[g_ViBackIndex].vp.vtrans[0] = (VI_SX(g_ViBackData->viewx) * 2) + (VI_SX(g_ViBackData->viewleft) * 4);
        g_CurrentPlayer->viewports[g_ViBackIndex].vp.vscale[1] = (g_ViBackData->viewy * 2);
        g_CurrentPlayer->viewports[g_ViBackIndex].vp.vtrans[1] = (g_ViBackData->viewy * 2) + (g_ViBackData->viewtop * 4);
    }

    // Make the RSP's viewport transform the current player's screen rectangle.
    gSPViewport(gdl++, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->viewports[g_ViBackIndex]));

    // Create both a floating-point matrix for the CPU side and a fixed-point matrix for the RSP.
    g_viProjectionMatrix = dynAllocateMatrix();
    guPerspectiveF(g_viProjectionMatrixF, &g_viPerspNorm, g_ViBackData->fovy, g_ViBackData->aspect, g_ViBackData->znear, g_ViBackData->zfar, 1.0f);
    guMtxF2L(g_viProjectionMatrixF, g_viProjectionMatrix);

    /** 
     * Load g_viProjectionMatrix as the current projection matrix.
     * All subsequent rendering uses this projection until a another projection matrix is loaded.
     */
    gSPMatrix(gdl++, OS_K0_TO_PHYSICAL(g_viProjectionMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPPerspNormalize(gdl++, g_viPerspNorm);

    // Store the float and non-float projection matrices so we can recall them later instead of having to rebuild them.
    camSetProjectionMtx(g_viProjectionMatrix);
    camSetProjectionMtxf(g_viProjectionMatrixF);

    // Normal rendering mode is a 16-bit RGBA image.
    if (g_viColorOutputMode != COLORMODE_32BIT)
    {
        gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, VI_SX(g_ViBackData->bufx), OS_K0_TO_PHYSICAL(g_ViBackData->framebuf));
    }
    // 32-bit mode is only used for the 32-bit screenshot routines.
    else
    {
        gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_32b, VI_SX(g_ViBackData->bufx), OS_K0_TO_PHYSICAL(cfb_16[0]));
    }

    return gdl;
}


Gfx *viClearZBufCurrentPlayer(Gfx *gdl)
{
    if (g_ViBackData->usezbuf != 0)
    {
        gdl = zbufClearCurrentPlayer(zbufInit(gdl));
    }

    return gdl;
}

Gfx *viFillScreen(Gfx *gdl)
{
	gDPSetCycleType(gdl++, G_CYC_FILL);
	gDPFillRectangle(gdl++, 0, 0, (VI_SX(g_ViBackData->bufx) - 1), (g_ViBackData->bufy - 1));
	gDPPipeSync(gdl++);

    return gdl;
}

Gfx *viSetupScreensForNumPlayers(Gfx *gdl)
{
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, VI_SX(viGetX()), viGetY());
    gDPSetFillColor(gdl++, ((GPACK_RGBA5551(0,0,0,1) << 16) | GPACK_RGBA5551(0,0,0,1)));

    if (coopSplitSideBySide())
    {
        // Each view blacks out the bands above and below it in its own half
        // (the players' screen sizes may differ). The left view also draws
        // the divider.
        s32 x1;
        s32 x2;

        if (viGetViewLeft() == 0)
        {
            x1 = 0;
            x2 = viGetViewWidth() + 1;
            gDPFillRectangle(gdl++, VI_SX(viGetViewWidth()), 0, VI_SX_INCL(x2), (viGetY() - 1));
            gDPPipeSync(gdl++);
        }
        else
        {
            x1 = viGetViewLeft();
            x2 = viGetX() - 1;
        }

        if (viGetViewTop() > 0)
        {
            gDPFillRectangle(gdl++, VI_SX(x1), 0, VI_SX_INCL(x2), (viGetViewTop() - 1));
            gDPPipeSync(gdl++);
        }

        if ((viGetViewTop() + viGetViewHeight()) < viGetY())
        {
            gDPFillRectangle(gdl++, VI_SX(x1), (viGetViewTop() + viGetViewHeight()), VI_SX_INCL(x2), (viGetY() - 1));
            gDPPipeSync(gdl++);
        }

        return gdl;
    }

    if (viGetViewTop() > 0)
    {
        if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
        {
            gDPFillRectangle(gdl++, 0, 0, VI_SX_INCL(viGetX() - 1), (viGetViewTop() - 1));
            gDPPipeSync(gdl++);
        }
    }

    if (getViewCount() == 1)
    {
        if ((viGetViewTop() + viGetViewHeight()) < viGetY())
        {
            gDPFillRectangle(gdl++, 0, (viGetViewTop() + viGetViewHeight()), VI_SX_INCL(viGetX() - 1), (viGetY() - 1));
            gDPPipeSync(gdl++);
        }
    }
    else
    {
        if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
        {
            if ((viGetViewTop() + viGetViewHeight() + viGetViewHeight()) < viGetY())
            {
                gDPFillRectangle(gdl++, 0, (viGetViewTop() + viGetViewHeight() + viGetViewHeight()), VI_SX_INCL(viGetX() - 1), (viGetY() - 1));
                gDPPipeSync(gdl++);
            }

            gDPFillRectangle(gdl++, 0, ((SCREEN_HEIGHT / 2) - 1), VI_SX_INCL(viGetX() - 1), (SCREEN_HEIGHT / 2));
            gDPPipeSync(gdl++);

            if (getPlayerCount() >= 3)
            {
                gDPFillRectangle(gdl++, VI_SX((SCREEN_WIDTH / 2) - 1), 0, VI_SX_INCL(SCREEN_WIDTH / 2), (viGetY() - 1));
                gDPPipeSync(gdl++);
            }

            if (getPlayerCount() == 3)
            {
                gDPFillRectangle(gdl++, VI_SX((SCREEN_WIDTH / 2) + 1), ((SCREEN_HEIGHT / 2) + 1), VI_SX_INCL(viGetX() - 1), (viGetY() - 1));
                gDPPipeSync(gdl++);
            }
        }
    }

    return gdl;
}

void viSetBuf(s16 x, s16 y)
{
    g_ViBackData->bufx = x;
    g_ViBackData->bufy = y;

    // Hi-res is opted into per frame by the gameplay renderer (viSetScaleX)
    g_ViBackData->scalex = 1;
}

void viSetScaleX(s32 scalex)
{
    g_ViBackData->scalex = scalex;
}

void viSetXY(s16 x, s16 y)
{
    g_ViBackData->x = x;
    g_ViBackData->y = y;
}

s16 viGetX(void)
{
    return g_ViBackData->x;
}

s16 viGetY(void)
{
    return g_ViBackData->y;
}

void viSetViewSize(s16 x, s16 y)
{
    g_ViBackData->viewx = x;
    g_ViBackData->viewy = y;
    camSetScreenSize(g_ViBackData->viewx, g_ViBackData->viewy);
    camUpdateScale();
}

s16 viGetViewWidth(void)
{
    return g_ViBackData->viewx;
}

s16 viGetViewHeight(void)
{
    return g_ViBackData->viewy;
}

void viSetViewPosition(s16 left, s16 top)
{
    g_ViBackData->viewleft = left;
    g_ViBackData->viewtop = top;
    camSetScreenPosition(g_ViBackData->viewleft, g_ViBackData->viewtop);
}

s16 viGetViewLeft(void)
{
    return g_ViBackData->viewleft;
}

s16 viGetViewTop(void)
{
    return g_ViBackData->viewtop;
}

void viSetUseZBuf(s32 usezbuf)
{
  g_ViBackData->usezbuf = usezbuf;
}

void viSetFovY(f32 fovy)
{
    g_ViBackData->fovy = fovy;
    camSetPerspective(g_ViBackData->fovy, g_ViBackData->aspect);
    camUpdateScale();
}

void viSetAspect(f32 aspect)
{
    g_ViBackData->aspect = aspect;
    camSetPerspective(g_ViBackData->fovy, g_ViBackData->aspect);
    camUpdateScale();
}

f32 viGetFovY(void)
{
    return g_ViBackData->fovy;
}

void viSetZRange(f32 near, f32 far)
{
    g_ViBackData->znear = near;
    g_ViBackData->zfar = far;
    camSetPerspective(g_ViBackData->fovy, g_ViBackData->aspect);
    camUpdateScale();
}

void viGetZRange(f32 *zrange)
{
    zrange[0] = g_ViBackData->znear;
    zrange[1] = g_ViBackData->zfar;
}

Gfx *viSetFillColor(Gfx *gdl, s32 r, s32 g, s32 b)
{
    if (g_viColorOutputMode != COLORMODE_32BIT)
    {
        gDPSetFillColor(gdl++, ((GPACK_RGBA5551(r, g, b, 1) << 16) | GPACK_RGBA5551(r, g, b, 1)));
    }
    else
    {
        gDPSetFillColor(gdl++, ((r << 24) | (g << 16) | (b << 8) | 0xFF));
    }

    return gdl;
}
