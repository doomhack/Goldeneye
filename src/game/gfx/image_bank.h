#ifndef _IMAGE_BANK_H_
#define _IMAGE_BANK_H_
#include <ultra64.h>
#include <bondtypes.h>
#include "game/player/bondview.h"

extern struct sImageTableEntry *g_ImageCrosshair;

extern struct sImageTableEntry *g_ImagesMainFolder;
extern struct sImageTableEntry *g_ImagesMpStageSelect;
extern struct sImageTableEntry *g_ImageGeneric;
extern struct sImageTableEntry *g_ImagesSkyWater;
extern struct sImageTableEntry *g_ImagesMonitor;
extern struct sImageTableEntry *g_ImagesMpCharSelect;
extern struct sImageTableEntry *g_ImagesMpRadar;
extern struct sImageTableEntry *g_ImagesImpact;
extern struct sImageTableEntry *g_ImagesExplosionSmoke;
extern struct sImageTableEntry *g_ImagesScatteredExplosions;
extern struct sImageTableEntry *g_ImageFlare2;
extern struct sImageTableEntry *g_ImageGlassOverlay;
extern struct sImageTableEntry *g_ImageFlare3;
extern struct sImageTableEntry *g_ImageFlare4;
extern struct sImageTableEntry *g_ImageFlare5;

extern u8* g_TexBitsPos;
extern s32 g_TexBitsCount;
extern s32 *g_ImageBank;
extern s32 g_ImageBankOffset;

void imageBankLoad(void);
u32 texBitsRead(s32 bitCount);
void texBitsInit(s32 pos);

#endif
