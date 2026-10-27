#ifndef _INTRO_LOGOS_H_
#define _INTRO_LOGOS_H_
#include <ultra64.h>

extern s32 g_TitleBarrelVerts;
extern Gfx *g_TitleBarrelGfx;
extern Mtx *g_TitleMtxRareLogo0;
extern Mtx *g_TitleMtxBarrel0;
extern Mtx *g_TitleMtxRareLogo1;
extern Mtx *g_TitleMtxRareLogo2;
extern Mtx *g_TitleMtxBarrel1;
extern Mtx *g_TitleMtxBackdrop;
extern Mtx *g_TitleMtxBond;
extern f32 x;
extern f32 y;
extern f32 titleTransitionX;
extern f32 titleTransitionY;
extern s16 g_TitleBarrelAngle;
extern s32 g_TitleBackdropPacked;
extern s32 g_TitleBackdrop;
extern s32 g_TitleRareLogoData;
extern s32 g_TitleTimer;

extern u32 g_TitleMtxIndex;

Gfx *titleRenderFolderMenuBackground(Gfx *gdl, s32 xOffset, struct FolderSelectColour *topColour, struct FolderSelectColour *bottomColour);
void titleAllocMatrices(void);

#endif
