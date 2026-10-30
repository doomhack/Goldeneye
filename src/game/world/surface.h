#ifndef _SURFACE_H_
#define _SURFACE_H_
#include <ultra64.h>
#include "bondconstants.h"

/**
 * What a kind of surface (HIT_TYPE) does when it's shot. Each texture's entry in
 * g_Textures says which kind it is: hitSound for the sound, hitTexture for the impact.
 */
typedef struct SurfaceHit
{
    const u16 *sfx;    /* bullet hit sounds, one picked at random */
    const u8 *impacts; /* bullet impacts (impactCreate, decal.c), one picked at random */
    s16 numSfx;
    s16 numImpacts;
} SurfaceHit;

extern const SurfaceHit g_SurfaceHits[HIT_TYPE_COUNT];

#endif
