/**
 * The global image bank: images used everywhere, such as ammo icons, the crosshair,
 * explosion smoke, bullet impacts and lens flares.
 *
 * imageBankLoad copies the bank from ROM into the stage pool on each level load, points the
 * g_Image* globals at its tables, and loads the textures that are always needed. The bank
 * is linked at 0x02000000 and is also mapped to that RSP segment (SPSEGMENT_GETITLE), so
 * its display lists can point at each other; adding g_ImageBankOffset to the address of
 * anything in it gives where it is in memory.
 */

#include <ultra64.h>
#include <rom.h>
#include <memp.h>
#include <macro.h>
#include "game/gfx/imagebank.h"
#include "game/gfx/texload.h"

s32 g_ImageBankOffset;
void *g_ImageBank;

struct sImageTableEntry *g_ImageGeneric;
struct sImageTableEntry *g_ImagesImpact;
struct sImageTableEntry *g_ImagesExplosionSmoke;
struct sImageTableEntry *g_ImagesScatteredExplosions;
struct sImageTableEntry *g_ImageFlare2;
struct sImageTableEntry *g_ImageFlare3;
struct sImageTableEntry *g_ImageFlare4;
struct sImageTableEntry *g_ImageFlare5;
struct sImageTableEntry *g_ImageCrosshair;
struct sImageTableEntry *g_ImageGlassOverlay;
struct sImageTableEntry *g_ImagesMonitor;
struct sImageTableEntry *g_ImagesSkyWater;
struct sImageTableEntry *g_ImagesMainFolder;
struct sImageTableEntry *g_ImagesMpRadar;
struct sImageTableEntry *g_ImagesMpCharSelect;
struct sImageTableEntry *g_ImagesMpStageSelect;

extern u8 _GlobalimagetableSegmentRomStart[] NO_SDATA;
extern u8 _GlobalimagetableSegmentStart[] NO_SDATA;
extern u8 _GlobalimagetableSegmentEnd[] NO_SDATA;

extern struct sImageTableEntry s_genericimage[] NO_SDATA;
extern struct sImageTableEntry s_impactimages[] NO_SDATA;
extern struct sImageTableEntry s_explosion_smokeimages[] NO_SDATA;
extern struct sImageTableEntry s_scattered_explosions[] NO_SDATA;
extern struct sImageTableEntry s_flareimage2[] NO_SDATA;
extern struct sImageTableEntry s_flareimage3[] NO_SDATA;
extern struct sImageTableEntry s_flareimage4[] NO_SDATA;
extern struct sImageTableEntry s_flareimage5[] NO_SDATA;
extern struct sImageTableEntry s_crosshairimage[] NO_SDATA;
extern struct sImageTableEntry s_glassoverlayimage[] NO_SDATA;
extern struct sImageTableEntry s_monitorimages[] NO_SDATA;
extern struct sImageTableEntry s_skywaterimages[] NO_SDATA;
extern struct sImageTableEntry s_mainfolderimages[] NO_SDATA;
extern struct sImageTableEntry s_mpradarimages[] NO_SDATA;
extern struct sImageTableEntry s_mpcharselimages[] NO_SDATA;
extern struct sImageTableEntry s_mpstageselimages[] NO_SDATA;

extern Gfx globalDL_0x000 NO_SDATA;
extern Gfx globalDL_0x078 NO_SDATA;
extern Gfx globalDL_0x120 NO_SDATA;
extern Gfx globalDL_0x1c8 NO_SDATA;
extern Gfx globalDL_0x270 NO_SDATA;
extern Gfx globalDL_0x318 NO_SDATA;
extern Gfx globalDL_0x3c0 NO_SDATA;
extern Gfx globalDL_0x468 NO_SDATA;
extern Gfx globalDL_0x510 NO_SDATA;
extern Gfx globalDL_0x5b8 NO_SDATA;
extern Gfx globalDL_0x660 NO_SDATA;
extern Gfx globalDL_0x708 NO_SDATA;
extern Gfx globalDL_0x7b0 NO_SDATA;
extern Gfx globalDL_0x858 NO_SDATA;
extern Gfx globalDL_0x900 NO_SDATA;
extern Gfx globalDL_0x9a8 NO_SDATA;
extern Gfx globalDL_0xa50 NO_SDATA;

#define IN_BANK(symbol) ((void *)(g_ImageBankOffset + (uintptr_t)(symbol)))

/* Where each image table is in the bank */
static const struct
{
    struct sImageTableEntry **table;
    struct sImageTableEntry *symbol;
} s_Tables[] = {
    { &g_ImageGeneric, s_genericimage },
    { &g_ImagesImpact, s_impactimages },
    { &g_ImagesExplosionSmoke, s_explosion_smokeimages },
    { &g_ImagesScatteredExplosions, s_scattered_explosions },
    { &g_ImageFlare2, s_flareimage2 },
    { &g_ImageFlare3, s_flareimage3 },
    { &g_ImageFlare4, s_flareimage4 },
    { &g_ImageFlare5, s_flareimage5 },
    { &g_ImageCrosshair, s_crosshairimage },
    { &g_ImageGlassOverlay, s_glassoverlayimage },
    { &g_ImagesMonitor, s_monitorimages },
    { &g_ImagesSkyWater, s_skywaterimages },
    { &g_ImagesMainFolder, s_mainfolderimages },
    { &g_ImagesMpRadar, s_mpradarimages },
    { &g_ImagesMpCharSelect, s_mpcharselimages },
    { &g_ImagesMpStageSelect, s_mpstageselimages },
};

/* The bank's display lists, whose textures are loaded up front */
static Gfx *const s_DisplayLists[] = {
    &globalDL_0x000,
    &globalDL_0x078,
    &globalDL_0x120,
    &globalDL_0x1c8,
    &globalDL_0x270,
    &globalDL_0x318,
    &globalDL_0x3c0,
    &globalDL_0x468,
    &globalDL_0x510,
    &globalDL_0x5b8,
    &globalDL_0x660,
    &globalDL_0x708,
    &globalDL_0x7b0,
    &globalDL_0x858,
    &globalDL_0x900,
    &globalDL_0x9a8,
    &globalDL_0xa50,
};

void imageBankLoad(void)
{
    u32 size = _GlobalimagetableSegmentEnd - _GlobalimagetableSegmentStart;

    // Aligned to 4 KB
    g_ImageBank = mempAlloc(size + 0x1000, MEMPOOL_STAGE);
    g_ImageBank = (void *)(((uintptr_t)g_ImageBank + 0xfff) & ~0xfff);

    romCopy(g_ImageBank, _GlobalimagetableSegmentRomStart, size);

    g_ImageBankOffset = (uintptr_t)g_ImageBank - 0x02000000;

    for (s32 i = 0; i < ARRAYCOUNT(s_Tables); i++)
    {
        *s_Tables[i].table = IN_BANK(s_Tables[i].symbol);
    }

    for (s32 i = 0; i < ARRAYCOUNT(s_DisplayLists); i++)
    {
        texLoadFromDisplayList(IN_BANK(s_DisplayLists[i]), NULL);
    }

    texLoad(&g_ImageGeneric->index, NULL);

    for (s32 i = 0; i < 6; i++)
    {
        texLoad(&g_ImagesExplosionSmoke[i].index, NULL);
    }

    for (s32 i = 0; i < 5; i++)
    {
        texLoad(&g_ImagesScatteredExplosions[i].index, NULL);
    }
}
