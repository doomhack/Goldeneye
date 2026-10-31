/**
 * Debris: small grey chips of rubble that tumble out of explosions and bullet holes and
 * fall away under gravity. They're kept in a ring buffer (200 between all players, half
 * that on Streets and Depot) and the oldest are reused when it's full.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <gbi_extension.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "random.h"
#include "game/player/player.h"
#include "game/gfx/dyn.h"
#include "game/gfx/imagebank.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/prop/debris.h"

extern Gfx globalDL_0xa50 NO_SDATA;

/* Debris lives at least this long, then goes at random (1 in 32 a tick) */
#define DEBRIS_MIN_AGE 101
#define DEBRIS_MAX_AGE 300

#define DEBRIS_GRAVITY 0.2f
#define DEBRIS_MAX_FALL_SPEED 3.75f

static s32 s_MaxDebris;
static struct Debris *s_Debris;
static s32 s_NextDebris = 0;

/* A random corner offset: 0.75 to 1.5 times size */
static s16 debrisCornerOffset(f32 size)
{
    return (s16)(s32)(((RANDOMFRAC() * 0.75f) + 0.75f) * size);
}

/**
 * Throws out one chip of debris from near pos: up to scatter away from it, moving
 * outwards at up to hvel and upwards at up to vvel. size is the chip's size.
 */
void debrisCreate(coord3d *pos, f32 scatter, f32 hvel, f32 vvel, f32 size)
{
    struct Debris *debris = &s_Debris[s_NextDebris];
    f32 dx = (2.0f * RANDOMFRAC()) - 1.0f;
    f32 dy = (RANDOMFRAC() * 1.12f) - 0.12f;
    f32 dz = (2.0f * RANDOMFRAC()) - 1.0f;
    s16 u;
    s16 v;
    s32 i;

    debris->age = 1;

    debris->pos.f[0] = pos->f[0] + (scatter * dx);
    debris->pos.f[1] = pos->f[1] + (scatter * dy);
    debris->pos.f[2] = pos->f[2] + (scatter * dz);

    debris->vel.f[0] = dx * hvel;
    debris->vel.f[1] = dy * vvel;
    debris->vel.f[2] = dz * hvel;

    // An uneven quad, a corner in each quadrant
    debris->vertices[0].v.ob[0] = debrisCornerOffset(size);
    debris->vertices[0].v.ob[1] = 0;
    debris->vertices[0].v.ob[2] = debrisCornerOffset(size);
    debris->vertices[1].v.ob[0] = debrisCornerOffset(size);
    debris->vertices[1].v.ob[1] = 0;
    debris->vertices[1].v.ob[2] = debrisCornerOffset(-size);
    debris->vertices[2].v.ob[0] = debrisCornerOffset(-size);
    debris->vertices[2].v.ob[1] = 0;
    debris->vertices[2].v.ob[2] = debrisCornerOffset(-size);
    debris->vertices[3].v.ob[0] = debrisCornerOffset(-size);
    debris->vertices[3].v.ob[1] = 0;
    debris->vertices[3].v.ob[2] = debrisCornerOffset(size);

    // One of the 4x4 tiles of the debris texture
    u = (randomGetNext() & 3) << 8;
    v = (randomGetNext() & 3) << 8;

    debris->vertices[0].v.tc[0] = u + 0xE0;
    debris->vertices[0].v.tc[1] = v + 0xE0;
    debris->vertices[1].v.tc[0] = u + 0xE0;
    debris->vertices[1].v.tc[1] = v;
    debris->vertices[2].v.tc[0] = u;
    debris->vertices[2].v.tc[1] = v;
    debris->vertices[3].v.tc[0] = u;
    debris->vertices[3].v.tc[1] = v + 0xE0;

    // Light or dark grey, a little different at each corner
    bool light = randomGetNext() & 1;

    for (i = 0; i < 4; i++)
    {
        u8 shade = randomGetNext() & 0x3F;

        if (light)
        {
            shade = 0xFF - shade;
        }

        debris->vertices[i].v.cn[2] = shade;
        debris->vertices[i].v.cn[1] = shade;
        debris->vertices[i].v.cn[0] = shade;
    }

    for (i = 0; i < 4; i++)
    {
        debris->vertices[i].v.cn[3] = 0xDC;
    }

    debris->rot.f[0] = RANDOMFRAC() * M_TAU_F;
    debris->rot.f[1] = RANDOMFRAC() * M_TAU_F;
    debris->rot.f[2] = RANDOMFRAC() * M_TAU_F;

    debris->rotvel.f[0] = RANDOMFRAC() * 0.1f;
    debris->rotvel.f[1] = RANDOMFRAC() * 0.1f;
    debris->rotvel.f[2] = RANDOMFRAC() * 0.1f;

    s_NextDebris++;

    if (s_NextDebris >= s_MaxDebris)
    {
        s_NextDebris = 0;
    }
}

/**
 * Moves all debris on by this frame's ticks (at most 15), and frees chips that have
 * lived long enough or fallen out of the world.
 */
void debrisTick(void)
{
    f32 ticks = g_ClockTimer < 15 ? g_ClockTimer : 15.0f;
    s32 i;
    s32 j;

    for (i = 0; i < s_MaxDebris; i++)
    {
        struct Debris *debris = &s_Debris[i];

        if (debris->age <= 0)
        {
            continue;
        }

        debris->age += (s32)ticks;

        debris->rot.f[0] += debris->rotvel.f[0] * ticks;
        debris->rot.f[1] += debris->rotvel.f[1] * ticks;
        debris->rot.f[2] += debris->rotvel.f[2] * ticks;

        debris->pos.f[0] += debris->vel.f[0] * ticks;
        debris->pos.f[2] += debris->vel.f[2] * ticks;

        for (j = 0; j < (s32)ticks; j++)
        {
            debris->pos.f[1] += debris->vel.f[1];

            if (debris->vel.f[1] > -DEBRIS_MAX_FALL_SPEED)
            {
                debris->vel.f[1] -= DEBRIS_GRAVITY;
            }
        }

        if (debris->age >= DEBRIS_MIN_AGE && ((randomGetNext() & 0x1F) == 0 || debris->age == DEBRIS_MAX_AGE))
        {
            debris->age = 0;
        }

        if (debris->pos.f[1] < -30000.0f || debris->pos.f[1] > 30000.0f)
        {
            debris->age = 0;
        }
    }
}

/**
 * Draws all debris that's near enough to the camera.
 */
Gfx *debrisRender(Gfx *gdl)
{
    Mtxf mtx;
    s32 i;

    gSPClearGeometryMode(gdl++, G_CULL_BOTH);
    gSPSegment(gdl++, SPSEGMENT_GETITLE, osVirtualToPhysical(g_ImageBank));
    gSPDisplayList(gdl++, &globalDL_0xa50);
    gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);

    for (i = 0; i < s_MaxDebris; i++)
    {
        struct Debris *debris = &s_Debris[i];

        if (debris->age <= 0)
        {
            continue;
        }

        mtx4LoadRotationAndTranslation(&debris->pos, &debris->rot, &mtx);
        mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), &mtx);

        if (mtx.m[3][0] < 20000.0f && mtx.m[3][0] > -20000.0f
            && mtx.m[3][1] < 20000.0f && mtx.m[3][1] > -20000.0f
            && mtx.m[3][2] < 20000.0f && mtx.m[3][2] > -20000.0f)
        {
            Mtx *gbimtx = dynAllocateMatrix();

            mtxF2L(&mtx, (Mtxf *)gbimtx);

            gSPMatrix(gdl++, osVirtualToPhysical(gbimtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPVertex(gdl++, osVirtualToPhysical(debris->vertices), 4, 0);
            gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
        }
    }

    return gdl;
}

/**
 * Allocates the level's debris, all free.
 */
void debrisInit(void)
{
    s32 i;

    s_NextDebris = 0;
    s_MaxDebris = DEBRIS_MAX / getPlayerCount();

    if (lvGetCurrentStage() == LEVELID_STREETS || lvGetCurrentStage() == LEVELID_DEPOT)
    {
        s_MaxDebris >>= 1;
    }

    s_Debris = mempAlloc(((s_MaxDebris * sizeof(struct Debris)) + 0xF) & ~0xF, MEMPOOL_STAGE);

    for (i = 0; i < s_MaxDebris; i++)
    {
        s_Debris[i].age = 0;
    }
}
