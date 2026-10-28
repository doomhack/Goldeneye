/**
 * The intro sequence the front end plays after the Nintendo logo: the Rare logo, the gun
 * barrel and Bond shooting at the camera, with the blood running down. Also the gradient
 * background behind the folder screens. See docs/subsystems/menu.md.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <PR/gu.h>
#include <PR/os.h>
#include <assets/font_dl.h>
#include <assets/animationtable_data.h>
#include <bondgame.h>
#include <fr.h>
#include <macro.h>
#include <rom.h>
#include "game/hud/blood.h"
#include "bondtypes.h"
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/model/chrobjdata.h"
#include "game/gfx/image.h"
#include "game/model/anim.h"
#include "math.h"
#include "game/math/round.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "music.h"
#include "game/asset/fileload.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/menu/title.h"
#include "game/gfx/dyn.h"


extern signed short sins(unsigned short x);


// bss
Vtx *g_TitleBarrelVerts;
Gfx *g_TitleBarrelGfx;
Mtx *g_TitleMtxRareLogo0;
Mtx *g_TitleMtxBarrel0;
Mtx *g_TitleMtxRareLogo1;
Mtx *g_TitleMtxRareLogo2;
Mtx *g_TitleMtxBarrel1;
Mtx *g_TitleMtxBackdrop;
Mtx *g_TitleMtxBond;
f32 g_TitleX;
f32 g_TitleY;
f32 titleTransitionX;
f32 titleTransitionY;
s16 g_TitleBarrelAngle;
u8 *g_TitleBackdropPacked;
u8 *g_TitleBackdrop;
u8 *g_TitleRareLogoData;

s32 g_TitleTimer;


// data
u32 g_TitleMtxIndex = 0;
u8 g_TitleStage = 0x3;

struct FolderSelectColour g_FolderGradientBlack = { 0x00, 0x00, 0x00 };
struct FolderSelectColour g_FolderGradientWhite = { 0xFF, 0xFF, 0xFF };

Model *g_TitleBondModel = NULL;
Model *g_TitleGunModel = NULL;

ModelRenderData gunbarrelRenderData = { NULL, TRUE, 3, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}, CULLMODE_BOTH };

f32 gunbarrelPosition1[3] = {1758.2957f, 220.0f, 684.28143f};
f32 gunbarrelPosition2[3] = {-0.97f, 0.0f, 0.24f};
f32 gunbarrelPosition3[3] = {0.0f, 1.0f, -0.0f};

Lights1 gunbarrelLights = gdSPDefLights1(0xDC, 0xDC, 0xDC, 0xFF, 0xFF, 0xFF, 0x00, 0x7F, 0x00);

f32 cameraPosition1[3] = {0.0f, 0.0f, 4883.0f};
f32 cameraPosition2[3] = {0.0f, 0.0f, -1.0f};
f32 cameraPosition3[3] = {0.0f, 1.0f, 0.0f};

f32 g_TitleRareLogoAngle = 0.0f;
s32 g_TitleCounter = 0;
u32 g_TitleBloodDone = 0;


/**
 * Manipulates matrices for the gun barrel and Rareware logo during the intro sequence.
 * 
 * @param gdl The display list to append graphics commands to.
 * @return Updated display list.
 */
Gfx *titleRenderBarrel(Gfx *gdl)
{
    guTranslate(&g_TitleMtxRareLogo2[g_TitleMtxIndex], g_TitleX, g_TitleY, -5.0f);
    guTranslate(&g_TitleMtxBarrel1[g_TitleMtxIndex], titleTransitionX, titleTransitionY, -5.0f);
    gSPDisplayList(gdl++, &dlBasicGeometry);

    gdl = gunbarrelSetupDraw(screenClearBlack(gdl));

    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, 0xE6, 0xE6, 0xE6, 0x00);
    gSPDisplayList(gdl++, OS_K0_TO_PHYSICAL(g_TitleBarrelGfx));
    gSPMatrix(gdl++, osVirtualToPhysical(&g_TitleMtxBarrel1[g_TitleMtxIndex]), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW));
    gSPDisplayList(gdl++, OS_K0_TO_PHYSICAL(g_TitleBarrelGfx));

    return gdl;
}

Gfx *titleRenderBackdrop(Gfx *gdl)
{
    guTranslate(&g_TitleMtxRareLogo2[g_TitleMtxIndex], g_TitleX + 768.0f, g_TitleY - 40.0f, -5.0f);
    guScale(&g_TitleMtxBarrel1[g_TitleMtxIndex], 2.7f, 2.57f, 1.0f);
    gSPDisplayList(gdl++, &dlBasicGeometry);
    gSPDisplayList(gdl++, &dlFastPipelineSetup);

    gdl = gunbarrelSetupDraw(gdl);

    gSPMatrix(gdl++, osVirtualToPhysical(&g_TitleMtxBarrel1[g_TitleMtxIndex]), (G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW));
    gSPDisplayList(gdl++, OS_K0_TO_PHYSICAL(g_TitleBarrelGfx));

    return gdl;
}


Gfx *titleRenderFolderMenuBackground(Gfx *gdl, s32 xOffset, struct FolderSelectColour *topColour, struct FolderSelectColour *bottomColour)
{
    gDPSetRenderMode(gdl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(gdl++, G_TP_NONE);
    gDPSetTextureFilter(gdl++, G_TF_POINT);
    gDPPipeSync(gdl++);

    return titleRenderFolderMenuBackgroundLines(gdl, (u8 *)OS_K0_TO_PHYSICAL(g_TitleBackdrop), xOffset, topColour, bottomColour);
}


Gfx *titleRenderSight(Gfx *gdl)
{
    struct FolderSelectColour topGradientColour = g_FolderGradientBlack;
    struct FolderSelectColour bottomGradientColour = g_FolderGradientWhite;

    gSPDisplayList(gdl++, &dlBasicGeometry);

    gdl = screenClearBlack(gdl);

    gDPSetCombineMode(gdl++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);

    return titleRenderFolderMenuBackground(gdl, mathFloor((viGetX() * g_TitleX) / 1280.0f), &topGradientColour, &bottomGradientColour);
}


Gfx *titleRenderFade(Gfx *gdl, u32 alpha)
{
    gdl = gunbarrelSetupDraw(gdl);

    gDPSetRenderMode(gdl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, 0x00, 0x00, 0x00, alpha);
    gDPSetColorDither(gdl++, G_CD_MAGICSQ);
    gDPFillRectangle(gdl++, 0, 0, viGetX(), viGetY());

    return gdl;
}


Gfx *titleRenderBondModel(Gfx *gdl, s32 count, Mtxf *matrix)
{
#if defined(REFRESH_PAL)
    #define BOND_EYE_ANIM_START 114
    #define BOND_EYE_ANIM_SPEEDUP 176
    #define BOND_EYE_FIRE_SHOT 191
#else
    #define BOND_EYE_ANIM_START 137
    #define BOND_EYE_ANIM_SPEEDUP 212
    #define BOND_EYE_FIRE_SHOT 230
#endif
    ModelRenderData renderData;
    JointListEntry *entry;
    bool playedShot;
    s32 i;

    renderData = gunbarrelRenderData;
    playedShot = FALSE;

    for (i = 0; i < count; i++)
    {
        if (g_TitleTimer >= 0)
        {
            g_TitleTimer++;

            if (g_TitleTimer == BOND_EYE_ANIM_START)
            {
                modelSetAnimation(g_TitleBondModel, ANIM(bond_eye_fire), 0, 2.0f, 0.910000026f, 16.0f);
            }

            if (g_TitleTimer == BOND_EYE_ANIM_SPEEDUP)
            {
                modelSetAnimSpeed(g_TitleBondModel, 1.6f, 8.0f);
            }
        }

        modelTickAnim(g_TitleBondModel, 1, 1);

        if (g_TitleTimer == BOND_EYE_FIRE_SHOT)
        {
            sndPlaySfx(g_musicSfxBufferPtr, GUN_RIFLE7BIG_1_SFX, NULL);
            playedShot = TRUE;
        }
    }

    modelSetLodDisabled(1);
    modelSetShadowAlpha(80);
    modelCalcPosition(g_TitleBondModel);

    if (g_TitleGunModel->obj->Switches[0] != NULL)
    {
        modelGetNodeRwData(g_TitleGunModel, g_TitleGunModel->obj->Switches[0])->Gunfire.visible = playedShot;
    }

    if (g_TitleGunModel->obj->Switches[2] != NULL)
    {
        modelGetNodeRwData(g_TitleGunModel, g_TitleGunModel->obj->Switches[2])->Switch.visible = playedShot;
    }

    renderData.basemtx = matrix;
    renderData.mtxlist = dynAllocate(g_TitleBondModel->obj->numMatrices << 6);
    modelCalcMatrices(&renderData, g_TitleBondModel);

    renderData.basemtx = modelFindNodeMtx(g_TitleBondModel, g_TitleGunModel->attachedto_objinst, 0);
    renderData.mtxlist = dynAllocate(g_TitleGunModel->obj->numMatrices << 6);
    modelCalcMatricesNoAnim(&renderData, g_TitleGunModel);

    entry = jointListAddModel(NULL, g_TitleBondModel);
    entry = jointListAddModel(entry, g_TitleGunModel);
    jointListCalcDepths(entry);
    entry = jointListSortByDepth(entry);

    renderData.PropType = 7;
    renderData.zbufferenabled = FALSE;
    renderData.gdl = gdl;
    renderData.flags = 1;
    jointListDraw(&renderData, entry);

    renderData.flags = 2;
    jointListDraw(&renderData, entry);

    modelSetLodDisabled(0);
    jointListFree(entry);

    for (i = 0; i < g_TitleBondModel->obj->numMatrices; i++)
    {
        Mtxf bondmtx;

        mtx4Copy((Mtxf *) &((s8 *) g_TitleBondModel->render_pos)[i * sizeof(Mtxf)], &bondmtx);
        mtxF2L(&bondmtx, &((Mtxf *) g_TitleBondModel->render_pos)[i]);
    }

    for (i = 0; i < g_TitleGunModel->obj->numMatrices; i++)
    {
        Mtxf gunmtx;

        mtx4Copy((Mtxf *) &((s8 *) g_TitleGunModel->render_pos)[i * sizeof(Mtxf)], &gunmtx);
        mtxF2L(&gunmtx, &((Mtxf *) g_TitleGunModel->render_pos)[i]);
    }

    return renderData.gdl;
}


Gfx *titleRenderBond(Gfx *gdl) {
    Mtxf matrix;
    u16 perspNorm;
    guTranslate(&g_TitleMtxBackdrop[g_TitleMtxIndex], 0.0f, 0.0f, 0.0f);
    guPerspective(&g_TitleMtxBond[g_TitleMtxIndex], &perspNorm, 46.0f, (320.0f / 240.0f), 10.0f, 10000.0f, 1.0f);
    gSPPerspNormalize(gdl++, perspNorm);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(gdl++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
    gSPMatrix(gdl++, osVirtualToPhysical(&g_TitleMtxBond[g_TitleMtxIndex]), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
    gSPMatrix(gdl++, osVirtualToPhysical(&g_TitleMtxBackdrop[g_TitleMtxIndex]), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW));
    
    mtx4LoadLookAt(&matrix, gunbarrelPosition1[0], gunbarrelPosition1[1], gunbarrelPosition1[2], (gunbarrelPosition1[0] + gunbarrelPosition2[0]), (gunbarrelPosition1[1] + gunbarrelPosition2[1]), (gunbarrelPosition1[2] + gunbarrelPosition2[2]), gunbarrelPosition3[0], gunbarrelPosition3[1], gunbarrelPosition3[2]);

#if defined REFRESH_PAL
    return titleRenderBondModel(gdl, 1, &matrix);
#else
    return titleRenderBondModel(gdl, 2, &matrix);
#endif
}


extern Gfx *DL_RAREWARELOGO1 NO_SDATA;
extern Gfx *DL_RAREWARETEXT NO_SDATA;
extern Gfx *DL_RAREWARELOGO2 NO_SDATA;
extern u8 *TEX_RAREWARELOGO1 NO_SDATA;
extern u8 *TEX_RAREWARELOGO2 NO_SDATA;

Gfx *titleRenderRareLogo(Gfx *gdl, s32 cameraZ, s32 brightness) {
    cameraPosition1[2] = cameraZ;
    gSPDisplayList(gdl++, &dlBasicGeometry);
    gdl = screenClearBlack(gdl);
    {
        u16 perspNorm;
        guPerspective(&g_TitleMtxRareLogo0[g_TitleMtxIndex], &perspNorm, 60.0f, (320.0f / 240.0f), 100.0f, 5000.0f, 1.0f);
        gSPPerspNormalize(gdl++, perspNorm);
    }
    gSPMatrix(gdl++, osVirtualToPhysical(&g_TitleMtxRareLogo0[g_TitleMtxIndex]), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
    gSPClearGeometryMode(gdl++, -1);
    gSPSetGeometryMode(gdl++, (G_SHADE | G_CULL_BACK | G_LIGHTING | G_TEXTURE_GEN | G_SHADING_SMOOTH));
    guLookAt(&g_TitleMtxRareLogo1[g_TitleMtxIndex], cameraPosition1[0], cameraPosition1[1], cameraPosition1[2], (cameraPosition1[0] + cameraPosition2[0]), (cameraPosition1[1] + cameraPosition2[1]), (cameraPosition1[2] + cameraPosition2[2]), cameraPosition3[0], cameraPosition3[1], cameraPosition3[2]);
    gSPMatrix(gdl++,  osVirtualToPhysical(&g_TitleMtxRareLogo1[g_TitleMtxIndex]), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW));
    guRotate(&g_TitleMtxRareLogo2[g_TitleMtxIndex], g_TitleRareLogoAngle, 0.0f, 1.0f, 0.0f);
#if defined(REFRESH_PAL)
    g_TitleRareLogoAngle += 2.4f;
#else
    g_TitleRareLogoAngle += 2.0f;
#endif
    gSPMatrix(gdl++, osVirtualToPhysical(&g_TitleMtxRareLogo2[g_TitleMtxIndex]), (G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW));
    gSPSetLights1(gdl++, gunbarrelLights);
    gunbarrelLights.a.l.col[0] = gunbarrelLights.a.l.col[1] = gunbarrelLights.a.l.col[2] = gunbarrelLights.a.l.colc[0] = gunbarrelLights.a.l.colc[1] = gunbarrelLights.a.l.colc[2] = brightness;
    gDPPipeSync(gdl++);
    gDPPipeSync(gdl++);
    gDPSetCombineMode(gdl++, G_CC_MODULATEI, G_CC_MODULATEI);
    gDPSetTexturePersp(gdl++, G_TP_PERSP);
    gDPSetTextureDetail(gdl++, G_TD_CLAMP);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
    gDPSetTextureFilter(gdl++, G_TF_BILERP);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPPipeSync(gdl++);
    gDPPipeSync(gdl++);
    gSPTexture(gdl++, 0x0800, 0x0800, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(gdl++, &TEX_RAREWARELOGO1, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, (G_TX_NOMIRROR | G_TX_WRAP), (G_TX_NOMIRROR | G_TX_WRAP), 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(gdl++, 0, 0, brightness, brightness, brightness, 0xFF);
    gSPDisplayList(gdl++, &DL_RAREWARELOGO1);
    gSPDisplayList(gdl++, &DL_RAREWARETEXT);
    gDPLoadTextureBlock(gdl++, &TEX_RAREWARELOGO2, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, (G_TX_NOMIRROR | G_TX_WRAP), (G_TX_NOMIRROR | G_TX_WRAP), 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(gdl++, 0, 0, ((brightness * 0xF0) / 0xFF), ((brightness * 0xD0) / 0xFF), ((brightness * 0xF0) / 0xFF), 0xFF);
    gSPDisplayList(gdl++, &DL_RAREWARELOGO2);

    return gdl;
}


extern void *_rarewarelogoSegmentRomStart NO_SDATA;
extern void *_rarewarelogoSegmentStart NO_SDATA;
extern void *_rarewarelogoSegmentEnd NO_SDATA;
void titleLoadRareLogo(u8 *address) {
    g_TitleStage = 0;
    g_TitleX = 880.0f;
    g_TitleRareLogoAngle = -40.0f;
    g_TitleCounter = 0;
    g_TitleRareLogoData = address;
    romCopy(g_TitleRareLogoData, &_rarewarelogoSegmentRomStart, ALIGN64_V2((u32)&_rarewarelogoSegmentEnd - (u32)&_rarewarelogoSegmentStart));
}


Gfx *titleRenderRareIntro(Gfx *gdl)
{
#if defined(REFRESH_PAL)
#define RAREWARE_LOGO_DEN 58
#define RAREWARE_LOGO_SUB 33915
#define RAREWARE_LOGO_EYE_COUNT1 216
#define RAREWARE_LOGO_EYE_COUNT2 241
#else
#define RAREWARE_LOGO_DEN 70
#define RAREWARE_LOGO_SUB 40800
#define RAREWARE_LOGO_EYE_COUNT1 260
#define RAREWARE_LOGO_EYE_COUNT2 290
#endif

    g_TitleMtxIndex = (1 - g_TitleMtxIndex);
    gSPSegment(gdl++, SPSEGMENT_GETITLE, osVirtualToPhysical(g_TitleRareLogoData));
    if ((g_TitleStage == 0) || (g_TitleStage == 1)) {
        s32 fadeIn;
        s32 fadeOut;
        fadeIn = (g_TitleCounter * 255) / RAREWARE_LOGO_DEN;
        if (fadeIn > 255) {
            fadeIn = 255;
        }
        if (fadeIn < 0) {
            fadeIn = 0;
        }

        fadeOut = 255 - (((g_TitleCounter * 255) - RAREWARE_LOGO_SUB) / RAREWARE_LOGO_DEN);
        if (fadeOut > 255) {
            fadeOut = 255;
        }
        if (fadeOut < 0) {
            fadeOut = 0;
        }
        gdl = titleRenderRareLogo(gdl, g_TitleX, (fadeIn * fadeOut) / 255);
        if (g_TitleCounter++ >= RAREWARE_LOGO_EYE_COUNT1) {
            if (g_TitleCounter >= RAREWARE_LOGO_EYE_COUNT2) {
                g_TitleCounter = 0;
                g_TitleStage++;
                g_TitleStage++;
            }
        }
    }

    return gdl;
}


s32 titleIsRareIntroDone(void) {
    return (g_TitleStage == 2);
}


extern void *g_TitleBackdropRle NO_SDATA;
extern void *g_TitleBackdropRleEnd NO_SDATA;
void titleLoadBackdrop(u8 **addr, s32 *size) {
    g_TitleBackdrop = *addr;
    *size -= 0x40400;
    *addr += 0x40400;
    g_TitleBackdropPacked = *addr;
    romCopy(g_TitleBackdropPacked, &g_TitleBackdropRle, ALIGN64_V2(((u32)&g_TitleBackdropRleEnd - (u32)&g_TitleBackdropRle)));
    rleExpand8Bit(g_TitleBackdropPacked, g_TitleBackdrop);
}


void titleInitGunbarrel(u8 *gfxBuffer, s32 bufferSize)
{
    struct ModelAnimation *animation;
    coord3d rootpos;
    struct texpool texturePool;
    s32 size;
    s32 startframe;
    
    g_TitleStage = 2;
    
    guOrtho(g_TitleMtxBarrel0, 0.0f, 1280.0f, 0.0f, 960.0f, 1.0f, 8.0f, 256.0f);
    
    g_TitleX = -30.0f;
    g_TitleY = 482.0f;
    titleTransitionX = -100.0f;
    titleTransitionY = 482.0f;
    g_TitleBarrelAngle = 0x42;
    g_TitleBarrelVerts = (Vtx *)gfxBuffer;
    bufferSize -= 0x200;
    gfxBuffer += 0x200;
    
    titleMakeBarrelHoleVerts(&g_TitleBarrelVerts->v, 0x1E);
    
    g_TitleBarrelGfx = (Gfx*)gfxBuffer;
    bufferSize -= 0x100;
    gfxBuffer += 0x100;
    
    titleMakeBarrelHoleGfx(g_TitleBarrelGfx, (Vtx *)OS_K0_TO_PHYSICAL(g_TitleBarrelVerts), 0x1E);
    titleLoadBackdrop(&gfxBuffer, &bufferSize);

    // struct copy
    rootpos = (coord3d){0};
    
    size = 0x12C00;
    
    texInitPool(&texturePool, gfxBuffer, size);
    
    gfxBuffer += size;
    bufferSize -= size;
    
    modelFileLoad(c_item_entries[BODY_Brosnan_Tuxedo].header, c_item_entries[BODY_Brosnan_Tuxedo].filename, gfxBuffer, bufferSize, &texturePool);
    
    size = ((fileGetLoadedSizeByName(c_item_entries[BODY_Brosnan_Tuxedo].filename) + 0x3F) | 0x3F) ^ 0x3F;
    bufferSize -= size;
    gfxBuffer += size;
    
    modelFileLoad(c_item_entries[BODY_Male_Pierce_Bond_Tuxedo].header, c_item_entries[BODY_Male_Pierce_Bond_Tuxedo].filename, gfxBuffer, bufferSize, &texturePool);
    
    size = ((fileGetLoadedSizeByName(c_item_entries[BODY_Male_Pierce_Bond_Tuxedo].filename) + 0x3F) | 0x3F) ^ 0x3F;
    bufferSize -= size;
    gfxBuffer += size;
    
    g_TitleBondModel = bodyCreateModelNew(BODY_Brosnan_Tuxedo, BODY_Male_Pierce_Bond_Tuxedo, c_item_entries[BODY_Brosnan_Tuxedo].header, c_item_entries[BODY_Male_Pierce_Bond_Tuxedo].header, 0);

    modelSetScale(g_TitleBondModel, 0.18779343f);
    modelSetAnimTranslationScale(g_TitleBondModel, 1.0f);
    modelSetRootPosition(g_TitleBondModel, &rootpos);
    modelSetRootRotY(g_TitleBondModel, 0.0f);
#if defined(VERSION_EU)
    #define TITLE_BOND_ANIM_SPEED 0.6f
#else
    #define TITLE_BOND_ANIM_SPEED 0.5f
#endif
    modelSetAnimPlaySpeed(g_TitleBondModel, TITLE_BOND_ANIM_SPEED, 0.0f);
#undef TITLE_BOND_ANIM_SPEED
    
    animation = (struct ModelAnimation*)((s32)g_AnimTable + (s32)&ANIM_DATA_bond_eye_walk);
    startframe = animation->numFrames - 0x44;
    while (startframe < 0)
    {
        startframe += animation->numFrames;
    }
    
    modelSetAnimation(g_TitleBondModel, (struct ModelAnimation *) animation, 0, (f32) startframe, 0.91f, 0.0f);
    modelFileLoad(PitemZ_entries[PROP_CHRWPPK].header, PitemZ_entries[PROP_CHRWPPK].filename, gfxBuffer, bufferSize, &texturePool);
    
    size = ((fileGetLoadedSizeByName(PitemZ_entries[PROP_CHRWPPK].filename) + 0x3F) | 0x3F) ^ 0x3F;
    bufferSize -= size;
    gfxBuffer += size;
    
    modelCalculateRwDataLen(PitemZ_entries[191].header);
    
    g_TitleGunModel = modelmgrInstantiateModel(PitemZ_entries[PROP_CHRWPPK].header);
    
    modelSetScale(g_TitleGunModel, 0.18779343f);
    
    g_TitleGunModel->attachedto = g_TitleBondModel;
    g_TitleGunModel->attachedto_objinst = g_TitleBondModel->obj->Switches[3];
    g_TitleTimer = 0;    
}


void titleFreeBondModels(void)
{
    if (g_TitleBondModel)
    {
        modelFreeAnimSlot(g_TitleBondModel);
    }

    if (g_TitleGunModel)
    {
        modelFreeSlot(g_TitleGunModel);
    }
}

#ifndef REFRESH_PAL
    #define XINC 6.0f
    #define XDEC 12.0f
    #define XDEC2 6
    #define XDEC3 5.8183274f
    #define INCVAL 0x38E
    #define INTRO_EYE_COUNTER_CASE_4 108
    #define INTRO_EYE_COUNTER_CASE_5_ADD 8
    #define INTRO_EYE_COUNTER_CASE_6 0x1e
#else
    #define XINC 7.0f
    #define XDEC 14.0f
    #define XDEC2 6
    #define XDEC3 3.63643622398f
    #define INCVAL 0x444
    #define INTRO_EYE_COUNTER_CASE_4 90
    #define INTRO_EYE_COUNTER_CASE_5_ADD 9
    #define INTRO_EYE_COUNTER_CASE_6 0x19
#endif


Gfx *titleRenderGunbarrel (Gfx *gdl) {
    g_TitleMtxIndex = (1 - g_TitleMtxIndex);
    switch (g_TitleStage - 2)
    {
    case 0:
        gdl = titleRenderBarrel(gdl);
        g_TitleX += XINC;
        if (g_TitleBarrelAngle < 0) {
            g_TitleBarrelAngle = 200;
            titleTransitionX = (g_TitleX - XDEC);
        } else {
#if defined(VERSION_EU)
            g_TitleBarrelAngle -= 7;
#else
            g_TitleBarrelAngle -= 6;
#endif
        }
        if (g_TitleX > 1390.0f) {
            g_TitleStage++;
            g_TitleX = 1276.0f;
        }
        break;

    case 1:
        #if defined(VERSION_US)
        gSPDisplayList(gdl++, &dlBasicGeometry);
        gdl = screenClearBlack(gdl++);
        gdl = screenClearBlack(gdl++);
        gdl = screenClearBlack(gdl++);
        gdl = screenClearBlack(gdl++);
        gdl = screenClearBlack(gdl++);
        #endif
        gdl = titleRenderSight(gdl++);
        gdl = titleRenderBackdrop(gdl++);
        
        if (g_TitleX < 600.0f) {
            gdl = titleRenderBond(gdl);
        }
        g_TitleX -= XDEC3;
        if (g_TitleX <= -80.0f) {
            g_TitleStage++;
            g_TitleCounter = 20;
        }
        break;

    case 2:
        gdl = titleRenderSight(gdl);
        gdl = titleRenderBackdrop(gdl);
        gdl = titleRenderBond(gdl);
        g_TitleCounter--;
        if (g_TitleCounter < 0) {
            g_TitleStage++;
            bloodNextFrame(0);
            g_TitleBloodDone = 0;
            g_TitleCounter = 1;
        }
        break;

    case 3:
        g_TitleCounter--;
        if (g_TitleCounter == 0) {
            g_TitleBloodDone = bloodNextFrame(1);
            g_TitleCounter = 2;
        }
        gdl = titleRenderSight(gdl);
        gdl = titleRenderBackdrop(gdl);
        gdl = titleRenderBond(gdl);
        gdl = bloodRenderGunbarrel(gdl);
        if (g_TitleBloodDone != 0) {
            g_TitleStage++;
            g_TitleBarrelAngle = 0;
            titleTransitionX = g_TitleX;
            g_TitleCounter = 0;
        }
        break;

    case 4:
        g_TitleBarrelAngle += INCVAL;
        g_TitleCounter++;
        g_TitleX = ((sins(g_TitleBarrelAngle) * 64.0f) / 32768.0f) + titleTransitionX;
        gdl = titleRenderSight(gdl);
        gdl = titleRenderBackdrop(gdl);
        gdl = titleRenderBond(gdl);
        gdl = bloodRenderRedFill(gdl);
        if (g_TitleCounter >= INTRO_EYE_COUNTER_CASE_4)
        {
            g_TitleCounter = 0;
            g_TitleStage++;
        }
        break;

    case 5:
        g_TitleBarrelAngle += INCVAL;
        g_TitleX = ((sins(g_TitleBarrelAngle) * 64.0f) / 32768.0f) + titleTransitionX;
        gdl = titleRenderSight(gdl);
        gdl = titleRenderBackdrop(gdl);
        gdl = titleRenderBond(gdl);
        gdl = bloodRenderRedFill(gdl);
        
        g_TitleCounter += INTRO_EYE_COUNTER_CASE_5_ADD;
        
        gdl = titleRenderFade(gdl, g_TitleCounter);
        if (g_TitleCounter >= 0xF7) {
            g_TitleCounter = 0;
            g_TitleStage++;                
        }
        break;

    case 6:
        gSPDisplayList(gdl++, &dlBasicGeometry);
        gdl = screenClearBlack(gdl);
        if (g_TitleCounter++ >= INTRO_EYE_COUNTER_CASE_6) {
            g_TitleCounter = 0;
            g_TitleStage++;
        }
        break;
    };

    return gdl;
}


s32 titleIsGunbarrelDone(void) {
    return (g_TitleStage == 9);
}


/** Allocates the matrices the intro sequence (Rare logo, gun barrel, Bond) draws with. */
void titleAllocMatrices(void)
{
    g_TitleMtxIndex = 0;
    g_TitleMtxRareLogo0     = mempAlloc(0x80, MEMPOOL_STAGE);
    g_TitleMtxBarrel0    = mempAlloc(0x40, MEMPOOL_STAGE);
    g_TitleMtxRareLogo1     = mempAlloc(0x80, MEMPOOL_STAGE);
    g_TitleMtxRareLogo2     = mempAlloc(0x80, MEMPOOL_STAGE);
    g_TitleMtxBarrel1    = mempAlloc(0x80, MEMPOOL_STAGE);
    g_TitleMtxBackdrop = mempAlloc(0x80, MEMPOOL_STAGE);
    g_TitleMtxBond     = mempAlloc(0x80, MEMPOOL_STAGE);
}




/**
 * Render the gun barrel background in the folder select menu. 
 * Like all front end screens, this screen renders at the high res 440x330 resolution.
 * The 440x299 8-bit background texture is far too large to fit in the N64's 4KB texture cache (TMEM),
 * so instead the texture is read one 440 x 1 pixel row at a time. That row of pixels is then applied to a rectangle and the game
 * renders 299 one pixel tall rectangles, moving down the screen, until the screen is filled.
 * 
 * The game also gives the rectangles a color, starting darker at the top and lighter towards the bottom, creating a smooth vertical gradient.
 */
Gfx* titleRenderFolderMenuBackgroundLines(Gfx* gdl, u8 *image, s32 xOffset, struct FolderSelectColour* topColour, struct FolderSelectColour* bottomColour) 
{
    f32 topRed;
    f32 topGreen;
    f32 topBlue;
    f32 bottomRed;
    f32 bottomGreen;
    f32 bottomBlue;
    s32 i;

    topRed = topColour->red;
    topGreen = topColour->green;
    topBlue = topColour->blue;
    bottomRed = bottomColour->red;
    bottomGreen = bottomColour->green;
    bottomBlue = bottomColour->blue;

    i = 0;

    while ((i + 1) < 300) 
    {
        // Load a 440 wide by 1 pixel tall 8-bit intensity texture.
        gDPLoadTextureBlock(
            gdl++, image, G_IM_FMT_I, G_IM_SIZ_8b, 440, 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD
        );

        gDPSetPrimColor(
            gdl++, 0, 0, topRed + (((bottomRed - topRed) * i) / 299.0f),
            topGreen + (((bottomGreen - topGreen) * i) / 299.0f), topBlue + (((bottomBlue - topBlue) * i) / 299.0f), 255
        );

        if (xOffset < 0) 
        {
            gSPTextureRectangle(
                gdl++, 0, (i + 0x10) << 2, (440 << 2) - 1, ((((i + 1) + 0x10)) << 2) - 1, G_TX_RENDERTILE, (-xOffset) << 5, 0,
                1 << 10, 1 << 10
            );
        } 
        else 
        {
            gSPTextureRectangle(
                gdl++, xOffset << 2, (i + 0x10) << 2, (440 << 2) - 1, ((((i + 1) + 0x10)) << 2) - 1, G_TX_RENDERTILE, 0, 0,
                1 << 10, 1 << 10
            );
        }

        i++;
        image += 440; // Advance the image data by 440 bytes, since each texel is 1 byte.
    }

    return gdl;
}


/* Vertices of the gun barrel hole: a fan of points around a half circle, mirrored to make
 * the full circle, shaded darker towards the top. */
void titleMakeBarrelHoleVerts(Vtx_t *vtx, s32 numSteps)
{
    s16 cosval;
    f32 angle;
    s16 sinval;
    s32 step;
    u8 shade;

    for (step = 0; step <= numSteps; step += 2)
    {
        angle = ((f32) step * 3.1415927f) / (f32) numSteps;

        sinval = sinf(angle) * 64.0f;
        cosval = cosf(angle) * -64.0f;
        shade = (s8) (u32) (143.0f - (cosf(angle) * -111.0f));

        *vtx++ = (Vtx_t){ .ob = { sinval, cosval, 0 }, .cn = { shade, shade, shade, 0 } };

        if ((step != 0) && (step < numSteps))
        {
            *vtx++ = (Vtx_t){ .ob = { -sinval, cosval, 0 }, .cn = { shade, shade, shade, 0 } };
        }
    }
}


Gfx *titleMakeBarrelHoleGfx(Gfx *gdl, Vtx *vertexList, s32 numVertices)
{
    s8 j;
    
    do
    {
        j = ((numVertices >= 0x11) ? 0x10 : numVertices);

        gSPVertex(gdl++, vertexList, j, 0);

        vertexList += 14;

        j -= 3;
        
        do
        {
            gSP1Triangle(gdl++, j, j+1, j+2, 0);
        } while (--j >= 0);
        
        numVertices -= 14;        
    } while (numVertices >= 3);
    
    gSPEndDisplayList(gdl++);

    return gdl;
}
