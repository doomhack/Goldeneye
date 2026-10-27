/**
 * Animated water: waterTick scrolls the two layers of the water texture and pulses their
 * blend each frame, and waterSetupGdl sets the render state sky.c draws the water plane
 * with.
 */

#include <ultra64.h>
#include "game/player/bondview.h"
#include "game/level/lv.h"
#include "game/world/water.h"

// bss
f32 g_WaterScrollS;
f32 g_WaterScrollT;
f32 g_WaterBlendPhase;


// data
Gfx g_WaterGdl[] = {
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 4, 0, 0, 0, G_TX_WRAP, 6, 0, G_TX_WRAP, 6, 0),
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 4, 0, 1, 0, G_TX_WRAP, 6, 0, G_TX_WRAP, 6, 0),
    gsDPSetTileSize(0, 2, 2, 0, 0),
    gsDPSetTileSize(1, 2, 2, 0, 0),
    gsDPSetPrimColor(0, 15, 255, 255, 255, 255),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,  TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsDPSetTextureLOD(G_TL_TILE),
    gsDPSetCycleType(G_CYC_2CYCLE),
    gsSPSetGeometryMode(G_CULL_BACK ),
    gsSPEndDisplayList()
};

Gfx g_WaterGdlCi[] = {
    gsDPSetTile(G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, 0, 0, G_TX_WRAP, 5, 0, G_TX_WRAP, 5, 0),
    gsDPSetTile(G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, 1, 0, G_TX_WRAP, 5, 0, G_TX_WRAP, 5, 0),
    gsDPSetTileSize(0, 2, 2, 0, 0),
    gsDPSetTileSize(1, 2, 2, 0, 0),
    gsDPSetPrimColor(0, 15, 255, 255, 255, 255),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,  TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsDPSetTextureLOD(G_TL_TILE),
    gsDPSetCycleType(G_CYC_2CYCLE),
    gsSPSetGeometryMode(G_CULL_BACK ),
    gsSPEndDisplayList()
};

f32 g_SkyCloudOffset = 0;

// Water animation controller.
void waterTick(void)
{
#ifdef VERSION_EU
    f32 delta = g_GlobalTimerDelta;
#else
    f32 delta = g_ClockTimer;
#endif

    g_WaterScrollS += delta * 0.25f;

    if (g_WaterScrollS >= 256.0f)
    {
        g_WaterScrollS -= 256.0f;
    }

    if (g_WaterScrollS < 0.0f)
    {
        g_WaterScrollS += 256.0f;
    }

    g_WaterScrollT += delta * 0.1f;

    if (g_WaterScrollT >= 256.0f)
    {
        g_WaterScrollT -= 256.0f;
    }

    if (g_WaterScrollT < 0.0f)
    {
        g_WaterScrollT += 256.0f;
    }

    g_WaterBlendPhase += delta * 0.04f;

    // 6.2831802f is not quite equal to M_TAU_F. Leave as literal value here.
    if (g_WaterBlendPhase >= 6.2831802f)
    {
        g_WaterBlendPhase -= 6.2831802f;
    }

    if (g_WaterBlendPhase < 0.0f)
    {
        g_WaterBlendPhase += 6.2831802f;
    }
    
    g_WaterGdl[2].loadtile.sl = g_WaterScrollS;
    g_WaterGdl[2].loadtile.tl = g_WaterScrollT;
    g_WaterGdl[3].loadtile.sl = ((s32)g_WaterScrollS + 90) & 0xFF;
    g_WaterGdl[3].loadtile.tl = ((s32)g_WaterScrollT + 150) & 0xFF;
    ((u32 *) g_WaterGdl)[8] = (((u32 *) g_WaterGdl)[8] & ~0xFF) | (u32) ((sinf(g_WaterBlendPhase) * 127.0f) + 128.0f);

    g_WaterGdlCi[2].loadtile.sl = g_WaterScrollS;
    g_WaterGdlCi[2].loadtile.tl = g_WaterScrollT;
    g_WaterGdlCi[3].loadtile.sl = ((s32)g_WaterScrollS + 90) & 0xFF;
    g_WaterGdlCi[3].loadtile.tl = ((s32)g_WaterScrollT + 150) & 0xFF;
    ((u32 *) g_WaterGdlCi)[8] = (((u32 *) g_WaterGdl)[8] & ~0xFF) | (u32) ((sinf(g_WaterBlendPhase) * 127.0f) + 128.0f);
}


Gfx* waterSetupGdl(Gfx *gdl, s32 arg1)
{
    if (arg1 != 0)
    {
        gSPDisplayList(gdl++, g_WaterGdl);
    }
    else
    {
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 0, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 1, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTileSize(gdl++, 0, 0, 0, 0, 0);
        gDPSetTileSize(gdl++, 1, 90, 150, 0, 0);
        gDPSetPrimColor(gdl++, 0, (sinf(g_WaterBlendPhase) * 127.0f + 128.0f), 0xFF, 0xFF, 0xFF, 0xFF);
        gDPSetTextureDetail(gdl++, G_TD_CLAMP);
        gDPSetTextureFilter(gdl++, G_TF_BILERP);
        gDPSetCombineLERP(gdl++, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetCycleType(gdl++, G_CYC_2CYCLE);
        gSPSetGeometryMode(gdl++, G_CULL_BACK);
    }
    return gdl;
}


Gfx* waterSetupGdlCi(Gfx *gdl, s32 arg1)
{
    if (arg1 != 0)
    {
        gSPDisplayList(gdl++, g_WaterGdlCi);
    }
    else
    {
        gDPSetTile(gdl++, G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, 0, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTile(gdl++, G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, 1, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTileSize(gdl++, 0, 0, 0, 0, 0);
        gDPSetTileSize(gdl++, 1, 90, 150, 0, 0);
        gDPSetPrimColor(gdl++, 0, (sinf(g_WaterBlendPhase) * 127.0f + 128.0f), 0xFF, 0xFF, 0xFF, 0xFF);
        gDPSetTextureDetail(gdl++, G_TD_CLAMP);
        gDPSetTextureFilter(gdl++, G_TF_BILERP);
        gDPSetCombineLERP(gdl++, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0); /* expands to FC272C04 1F1093FF */
        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetCycleType(gdl++, G_CYC_2CYCLE);
        gSPSetGeometryMode(gdl++, G_CULL_BACK);
    }

    return gdl;
}


void waterReset(void)
{
  g_WaterScrollS = 0.0f;
  g_WaterScrollT = 0.0f;
  g_WaterBlendPhase = 0.0f;
  return;
}
