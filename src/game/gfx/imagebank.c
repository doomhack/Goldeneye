/**
 * The global image bank: images used everywhere, such as ammo icons, the crosshair,
 * explosion smoke, bullet impacts and lens flares.
 *
 * imageBankLoad copies the bank from ROM into the stage pool on each level load, points the
 * g_Image* globals at its entries, and loads the textures of the images that are always
 * needed. The bank is also mapped to its own RSP segment (SPSEGMENT_GETITLE).
 */

#include <ultra64.h>
#include <rom.h>
#include <memp.h>
#include "game/gfx/imagebank.h"

// bss
//8008D0B0;
s32 g_ImageBankOffset;
//8008D0B4;
s32 *g_ImageBank;
//8008D0B8;
struct sImageTableEntry *g_ImageGeneric;
struct sImageTableEntry *g_ImagesImpact;
struct sImageTableEntry *g_ImagesExplosionSmoke;
struct sImageTableEntry *g_ImagesScatteredExplosions;
struct sImageTableEntry *g_ImageFlare1;
struct sImageTableEntry *g_ImageFlare2;
struct sImageTableEntry *g_ImageFlare3;
struct sImageTableEntry *g_ImageFlare4;
struct sImageTableEntry *g_ImageFlare5;
struct sImageTableEntry *g_ImageAmmo9mm;
struct sImageTableEntry *g_ImageAmmoRifle;
struct sImageTableEntry *g_ImageAmmoShotgun;
struct sImageTableEntry *g_ImageAmmoKnife;
struct sImageTableEntry *g_ImageAmmoGrenadeLauncher;
struct sImageTableEntry *g_ImageAmmoRocket;
struct sImageTableEntry *g_ImageAmmoMine;
struct sImageTableEntry *g_ImageAmmoGrenade;
struct sImageTableEntry *g_ImageAmmoMagnum;
struct sImageTableEntry *g_ImageAmmoGoldenGun;
struct sImageTableEntry *g_ImageAmmoRemoteMine;
struct sImageTableEntry *g_ImageAmmoTimedMine;
struct sImageTableEntry *g_ImageAmmoProxMine;
struct sImageTableEntry *g_ImageAmmoTank;
//8008D114;
struct sImageTableEntry *g_ImageCrosshair;
struct sImageTableEntry *g_ImageCrosshairBeta;
struct sImageTableEntry *g_ImageGlassOverlay;
struct sImageTableEntry *g_ImagesMonitor;
struct sImageTableEntry *g_ImagesSkyWater;
struct sImageTableEntry *g_ImagesMainFolder;
struct sImageTableEntry *g_ImagesMpRadar;
struct sImageTableEntry *g_ImagesMpCharSelect;
struct sImageTableEntry *g_ImagesMpStageSelect;


extern u8* _GlobalimagetableSegmentRomStart NO_SDATA;


extern u32* _GlobalimagetableSegmentStart NO_SDATA;
extern u32* _GlobalimagetableSegmentEnd NO_SDATA;
extern void* s_genericimage NO_SDATA;
extern void* s_impactimages NO_SDATA;
extern void* s_explosion_smokeimages NO_SDATA;
extern void* s_scattered_explosions NO_SDATA;
extern void* s_flareimage1 NO_SDATA;
extern void* s_flareimage2 NO_SDATA;
extern void* s_flareimage3 NO_SDATA;
extern void* s_flareimage4 NO_SDATA;
extern void* s_flareimage5 NO_SDATA;
extern void* s_ammo9mmimage NO_SDATA;
extern void* s_rifleammoimage NO_SDATA;
extern void* s_shotgunammoimage NO_SDATA;
extern void* s_knifeammoimage NO_SDATA;
extern void* s_glammoimage NO_SDATA;
extern void* s_rocketammoimage NO_SDATA;
extern void* s_genericmineammoimage NO_SDATA;
extern void* s_grenadeammoimage NO_SDATA;
extern void* s_magnumammoimage NO_SDATA;
extern void* s_goldengunammoimage NO_SDATA;
extern void* s_remotemineammoimage NO_SDATA;
extern void* s_timedmineammoimage NO_SDATA;
extern void* s_proxmineammoimage NO_SDATA;
extern void* s_tankammoimage NO_SDATA;
extern void* s_crosshairimage NO_SDATA;
extern void* s_betacrosshairimage NO_SDATA;
extern void* s_glassoverlayimage NO_SDATA;
extern void* s_monitorimages NO_SDATA;
extern void* s_skywaterimages NO_SDATA;
extern void* s_mainfolderimages NO_SDATA;
extern void* s_mpradarimages NO_SDATA;
extern void* s_mpcharselimages NO_SDATA;
extern void* s_mpstageselimages NO_SDATA;

extern Gfx* globalDL_0x000 NO_SDATA;
extern Gfx* globalDL_0x078 NO_SDATA;
extern Gfx* globalDL_0x120 NO_SDATA;
extern Gfx* globalDL_0x1c8 NO_SDATA;
extern Gfx* globalDL_0x270 NO_SDATA;
extern Gfx* globalDL_0x318 NO_SDATA;
extern Gfx* globalDL_0x3c0 NO_SDATA;
extern Gfx* globalDL_0x468 NO_SDATA;
extern Gfx* globalDL_0x510 NO_SDATA;
extern Gfx* globalDL_0x5b8 NO_SDATA;
extern Gfx* globalDL_0x660 NO_SDATA;
extern Gfx* globalDL_0x708 NO_SDATA;
extern Gfx* globalDL_0x7b0 NO_SDATA;
extern Gfx* globalDL_0x858 NO_SDATA;
extern Gfx* globalDL_0x900 NO_SDATA;
extern Gfx* globalDL_0x9a8 NO_SDATA;
extern Gfx* globalDL_0xa50 NO_SDATA;

void imageBankLoad(void)
{
    u32 size;
    s32 i;

    size = (u32)&_GlobalimagetableSegmentEnd - (u32)&_GlobalimagetableSegmentStart;
    g_ImageBank = mempAlloc(size + 0x1000, MEMPOOL_STAGE);
    g_ImageBank = ((u32)g_ImageBank + 0xFFFU) & 0xFFFFF000;

    romCopy(g_ImageBank, &_GlobalimagetableSegmentRomStart, size);

    g_ImageBankOffset = (u32)g_ImageBank + 0xFE000000;
    g_ImageGeneric = (void *) (g_ImageBankOffset + (u32)&s_genericimage);
    g_ImagesImpact = (void *) (g_ImageBankOffset + (u32)&s_impactimages);
    g_ImagesExplosionSmoke = (void *) (g_ImageBankOffset + (u32)&s_explosion_smokeimages);
    g_ImagesScatteredExplosions = (void *) (g_ImageBankOffset + (u32)&s_scattered_explosions);
    g_ImageFlare1 = (void *) (g_ImageBankOffset + (u32)&s_flareimage1);
    g_ImageFlare2 = (void *) (g_ImageBankOffset + (u32)&s_flareimage2);
    g_ImageFlare3 = (void *) (g_ImageBankOffset + (u32)&s_flareimage3);
    g_ImageFlare4 = (void *) (g_ImageBankOffset + (u32)&s_flareimage4);
    g_ImageFlare5 = (void *) (g_ImageBankOffset + (u32)&s_flareimage5);
    g_ImageAmmo9mm = (void *) (g_ImageBankOffset + (u32)&s_ammo9mmimage);
    g_ImageAmmoRifle = (void *) (g_ImageBankOffset + (u32)&s_rifleammoimage);
    g_ImageAmmoShotgun = (void *) (g_ImageBankOffset + (u32)&s_shotgunammoimage);
    g_ImageAmmoKnife = (void *) (g_ImageBankOffset + (u32)&s_knifeammoimage);
    g_ImageAmmoGrenadeLauncher = (void *) (g_ImageBankOffset + (u32)&s_glammoimage);
    g_ImageAmmoRocket = (void *) (g_ImageBankOffset + (u32)&s_rocketammoimage);
    g_ImageAmmoMine = (void *) (g_ImageBankOffset + (u32)&s_genericmineammoimage);
    g_ImageAmmoGrenade = (void *) (g_ImageBankOffset + (u32)&s_grenadeammoimage);
    g_ImageAmmoMagnum = (void *) (g_ImageBankOffset + (u32)&s_magnumammoimage);
    g_ImageAmmoGoldenGun = (void *) (g_ImageBankOffset + (u32)&s_goldengunammoimage);
    g_ImageAmmoRemoteMine = (void *) (g_ImageBankOffset + (u32)&s_remotemineammoimage);
    g_ImageAmmoTimedMine = (void *) (g_ImageBankOffset + (u32)&s_timedmineammoimage);
    g_ImageAmmoProxMine = (void *) (g_ImageBankOffset + (u32)&s_proxmineammoimage);
    g_ImageAmmoTank = (void *) (g_ImageBankOffset + (u32)&s_tankammoimage);
    g_ImageCrosshair = (void *) (g_ImageBankOffset + (u32)&s_crosshairimage);
    g_ImageCrosshairBeta = (void *) (g_ImageBankOffset + (u32)&s_betacrosshairimage);
    g_ImageGlassOverlay = (void *) (g_ImageBankOffset + (u32)&s_glassoverlayimage);
    g_ImagesMonitor = (void *) (g_ImageBankOffset + (u32)&s_monitorimages);
    g_ImagesSkyWater = (void *) (g_ImageBankOffset + (u32)&s_skywaterimages);
    g_ImagesMainFolder = (void *) (g_ImageBankOffset + (u32)&s_mainfolderimages);
    g_ImagesMpRadar = (void *) (g_ImageBankOffset + (u32)&s_mpradarimages);
    g_ImagesMpCharSelect = (void *) (g_ImageBankOffset + (u32)&s_mpcharselimages);
    g_ImagesMpStageSelect = (void *) (g_ImageBankOffset + (u32)&s_mpstageselimages);

    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x000, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x078, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x120, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x1c8, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x270, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x318, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x3c0, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x468, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x510, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x5b8, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x660, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x708, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x7b0, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x858, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x900, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0x9a8, 0);
    texLoadFromDisplayList(g_ImageBankOffset + (u32)&globalDL_0xa50, 0);

    texLoad(g_ImageGeneric, 0);

    for (i=0; i < 6; i++)
    {
        texLoad(&g_ImagesExplosionSmoke[i], 0);
    }

    for (i=0; i < 5; i++)
    {
        texLoad(&g_ImagesScatteredExplosions[i], 0);
    }
}
