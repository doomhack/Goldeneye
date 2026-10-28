#ifndef _INTRO_LOGOS_H_
#define _INTRO_LOGOS_H_
#include "bondtypes.h"
#include "include/PR/gbi.h"
#include "ultra64.h"
#include <ultra64.h>

extern Vtx *g_TitleBarrelVerts;
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
extern u8 *g_TitleBackdropPacked;
extern u8 *g_TitleBackdrop;
extern u8 *g_TitleRareLogoData;
extern s32 g_TitleTimer;

extern u32 g_TitleMtxIndex;

Gfx *titleRenderFolderMenuBackground(Gfx *gdl, s32 xOffset, struct FolderSelectColour *topColour, struct FolderSelectColour *bottomColour);
void titleAllocMatrices(void);

Gfx* titleRenderFolderMenuBackgroundLines(Gfx* gdl, u8 *image, s32 x, struct FolderSelectColour* topColour, struct FolderSelectColour* bottomColour);

Gfx *titleMakeBarrelHoleGfx(Gfx *gdl, Vtx *vertexList, s32 numVertices);
void titleMakeBarrelHoleVerts(Vtx_t *vtx, s32 numSteps);

void titleFreeBondModels(void);
void titleInitGunbarrel(u8 *gfxBuffer, s32 bufferSize);
s32 titleIsGunbarrelDone(void);
s32 titleIsRareIntroDone(void);
void titleLoadBackdrop(u8 **addr, s32 *size);
void titleLoadRareLogo(u8 *address);
Gfx *titleRenderGunbarrel (Gfx *gdl);
Gfx *titleRenderRareIntro(Gfx *gdl);

#endif
