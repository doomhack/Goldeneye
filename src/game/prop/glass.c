/**
 * Glass shards. When a window, a door's pane or a camera's lens breaks,
 * glassCreateShards fills its area with small triangles that fly out, fall and spin,
 * and vanish after 150 ticks. Broken light fixtures throw a few too. They come from one
 * ring buffer of 200 shared by all players (halved on Streets and Depot), and the
 * oldest are reused when it's full.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "music.h"
#include "random.h"
#include "snd.h"
#include "game/audio/sfxvolume.h"
#include "game/player/player.h"
#include "game/gfx/dyn.h"
#include "game/gfx/imagebank.h"
#include "game/gfx/texcmds.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/math/trig.h"
#include "game/prop/glass.h"

/* EU runs at 50 Hz, so shards move further each tick */
#ifndef VERSION_EU
#define SHARD_HORIZ_VEL_SCALE 1.5f
#define SHARD_VERT_VEL_SCALE 3.0f
#define SHARD_ROTVEL_SCALE 0.1f
#else
#define SHARD_HORIZ_VEL_SCALE 1.8f
#define SHARD_VERT_VEL_SCALE 3.6f
#define SHARD_ROTVEL_SCALE 0.12f
#endif

#define SHARD_GRAVITY 0.1f
#define SHARD_LIFETIME 150

static s32 s_MaxShards;
static s32 s_NumActiveShards; // shards with age > 0, so the per-frame passes can skip an empty pool
static struct Shard *s_Shards;
static s32 s_NextShard = 0;

/**
 * Breaks a glass pane into a grid of shards. The pane spans xmin..xmax and ymin..ymax
 * along xaxis and yaxis (unnormalised: their lengths scale the extents), facing along
 * zaxis. Enough shards are made to use about half the buffer.
 */
void glassCreateShards(coord3d *pos, coord3d *xaxis, coord3d *yaxis, coord3d *zaxis, f32 xmin, f32 xmax, f32 ymin, f32 ymax)
{
    coord3d xnorm = *xaxis;
    coord3d ynorm = *yaxis;
    coord3d basepos;
    f32 len;
    s32 x;
    s32 y;

    len = sqrtf((xnorm.x * xnorm.x) + (xnorm.y * xnorm.y) + (xnorm.z * xnorm.z));
    xnorm.x *= 1.0f / len;
    xnorm.y *= 1.0f / len;
    xnorm.z *= 1.0f / len;
    xmin *= len;
    xmax *= len;

    len = sqrtf((ynorm.x * ynorm.x) + (ynorm.y * ynorm.y) + (ynorm.z * ynorm.z));
    ynorm.x *= 1.0f / len;
    ynorm.y *= 1.0f / len;
    ynorm.z *= 1.0f / len;
    ymin *= len;
    ymax *= len;

    f32 angle = mathAtan2(zaxis->x, zaxis->z);
    f32 width = xmax - xmin;
    f32 height = ymax - ymin;
    f32 size = sqrtf((width * height) / (s_MaxShards / 2));
    s32 step = (s32)size;
    f32 half = (f32)(step >> 1);

    // The middle of the first grid square
    basepos.x = pos->x + ((xmin + half) * xnorm.x) + ((ymin + half) * ynorm.x);
    basepos.y = pos->y + ((xmin + half) * xnorm.y) + ((ymin + half) * ynorm.y);
    basepos.z = pos->z + ((xmin + half) * xnorm.z) + ((ymin + half) * ynorm.z);

    sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, HIT_GLASS_SMASH_SFX, NULL), pos);

    s32 xcount = (s32)(width / step);
    s32 ycount = (s32)(height / step);

    for (y = 0; y < ycount; y++)
    {
        for (x = 0; x < xcount; x++)
        {
            coord3d shardpos;

            shardpos.x = basepos.x + ((x * (f32)step) * xnorm.x) + ((y * (f32)step) * ynorm.x);
            shardpos.y = basepos.y + ((x * (f32)step) * xnorm.y) + ((y * (f32)step) * ynorm.y);
            shardpos.z = basepos.z + ((x * (f32)step) * xnorm.z) + ((y * (f32)step) * ynorm.z);

            glassCreateShard(&shardpos, angle, ((RANDOMFRAC() * 0.7f) + 0.1f) * size);
        }
    }
}

/* A random corner offset: 1 to 1.5 times size */
static s16 shardCornerOffset(f32 size)
{
    return ((RANDOMFRAC() * 0.5f) + 1.0f) * size;
}

/**
 * Throws out one shard of glass at pos, turned to angle about the vertical. It flies
 * out at random, mostly upwards.
 */
void glassCreateShard(coord3d *pos, f32 angle, f32 size)
{
    struct Shard *shard = &s_Shards[s_NextShard];
    f32 dx = (2.0f * RANDOMFRAC()) - 1.0f;
    f32 dy = (RANDOMFRAC() * 1.12f) - 0.12f;
    f32 dz = (2.0f * RANDOMFRAC()) - 1.0f;
    s32 i;

    if (shard->age <= 0)
    {
        s_NumActiveShards++;
    }

    shard->age = 1;
    shard->pos.x = pos->x;
    shard->pos.y = pos->y;
    shard->pos.z = pos->z;

    shard->vel.x = dx * SHARD_HORIZ_VEL_SCALE;
    shard->vel.y = dy * SHARD_VERT_VEL_SCALE;
    shard->vel.z = dz * SHARD_HORIZ_VEL_SCALE;

    // An uneven triangle
    shard->vertices[0].v.ob[0] = shardCornerOffset(size);
    shard->vertices[0].v.ob[1] = shardCornerOffset(size);
    shard->vertices[0].v.ob[2] = 0;
    shard->vertices[1].v.ob[0] = shardCornerOffset(size);
    shard->vertices[1].v.ob[1] = shardCornerOffset(-size);
    shard->vertices[1].v.ob[2] = 0;
    shard->vertices[2].v.ob[0] = shardCornerOffset(-size);
    shard->vertices[2].v.ob[1] = shardCornerOffset(-size);
    shard->vertices[2].v.ob[2] = 0;

    // The texture comes from environment mapping: the vertex colours are its normals
    static const u8 normals[3][3] = {
        { 0x05, 0x05, 0x7E },
        { 0x05, 0xFB, 0x7E },
        { 0xFB, 0xFB, 0x7E },
    };

    for (i = 0; i < 3; i++)
    {
        shard->vertices[i].v.tc[0] = 0;
        shard->vertices[i].v.tc[1] = 0;
        shard->vertices[i].v.cn[0] = normals[i][0];
        shard->vertices[i].v.cn[1] = normals[i][1];
        shard->vertices[i].v.cn[2] = normals[i][2];
        shard->vertices[i].v.cn[3] = 0xFF;
    }

    shard->rot.x = angle;
    shard->rot.y = 0.0f;
    shard->rot.z = RANDOMFRAC() * M_TAU_F;

    shard->rotvel.x = RANDOMFRAC() * SHARD_ROTVEL_SCALE;
    shard->rotvel.y = RANDOMFRAC() * SHARD_ROTVEL_SCALE;
    shard->rotvel.z = RANDOMFRAC() * SHARD_ROTVEL_SCALE;

    s_NextShard++;

    if (s_NextShard >= s_MaxShards)
    {
        s_NextShard = 0;
    }
}

/**
 * Moves all shards on by this frame's ticks (at most 15), and frees those that have
 * lived long enough or fallen out of the world.
 */
void glassTickShards(void)
{
    f32 ticks = g_ClockTimer < 15 ? (f32)g_ClockTimer : 15.0f;
    s32 i;
    s32 j;

    if (s_NumActiveShards == 0)
    {
        return;
    }

    for (i = 0; i < s_MaxShards; i++)
    {
        struct Shard *shard = &s_Shards[i];

        if (shard->age <= 0)
        {
            continue;
        }

        shard->age += (s32)ticks;

        shard->rot.x += shard->rotvel.x * ticks;
        shard->rot.y += shard->rotvel.y * ticks;
        shard->rot.z += shard->rotvel.z * ticks;
        shard->pos.x += shard->vel.x * ticks;
        shard->pos.z += shard->vel.z * ticks;

        for (j = 0; j < (s32)ticks; j++)
        {
            shard->pos.y += shard->vel.y;
            shard->vel.y -= SHARD_GRAVITY;
        }

        if (shard->age >= SHARD_LIFETIME)
        {
            shard->age = 0;
        }

        if (shard->pos.y < -30000.0f || shard->pos.y > 30000.0f)
        {
            shard->age = 0;
        }

        if (shard->age == 0)
        {
            s_NumActiveShards--;
        }
    }
}

/**
 * Draws all shards, environment mapped with the glass texture.
 */
Gfx *glassRenderShards(Gfx *gdl)
{
    Mtxf mtxf;
    s32 i;
    // With no shards out there's nothing to find, so skip walking the whole pool.
    s32 count = s_NumActiveShards != 0 ? s_MaxShards : 0;

    texSelect(&gdl, g_ImageGlassOverlay + 1, TEXBLEND_TRANSLUCENT, TEXZBUF_ON, 2);

    gSPTexture(gdl++, 0x0D80, 0x0D80, 2, G_TX_RENDERTILE, 1);
    gDPSetCycleType(gdl++, G_CYC_2CYCLE);
    gDPSetTextureLOD(gdl++, G_TL_LOD);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH);
    gDPSetTextureFilter(gdl++, G_TF_BILERP);
    gSPSetGeometryMode(gdl++, G_LIGHTING | G_TEXTURE_GEN);
    gSPMatrix(gdl++, osVirtualToPhysical(camGetProjViewMtx()), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    for (i = 0; i < count; i++)
    {
        struct Shard *shard = &s_Shards[i];

        if (shard->age <= 0)
        {
            continue;
        }

        Mtx *mtx = dynAllocateMatrix();

        // Relative to the camera, as the projection matrix includes its position
        mtx4LoadRotationAndTranslation(&shard->pos, &shard->rot, &mtxf);

        mtxf.m[3][0] -= g_CurrentPlayer->current_model_pos.x;
        mtxf.m[3][1] -= g_CurrentPlayer->current_model_pos.y;
        mtxf.m[3][2] -= g_CurrentPlayer->current_model_pos.z;

        mtxF2L(&mtxf, (Mtxf *)mtx);

        gSPMatrix(gdl++, osVirtualToPhysical(mtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(gdl++, osVirtualToPhysical(shard->vertices), 3, 0);
        gSP1Triangle(gdl++, 0, 1, 2, 0);
    }

    gSPClearGeometryMode(gdl++, G_LIGHTING | G_TEXTURE_GEN);
    gSPMatrix(gdl++, (u32)camGetProjectionMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gdl++, (u32)camGetWorldToViewMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    return gdl;
}

/**
 * Allocates the level's shards, all free.
 */
void glassInitShards(void)
{
    s32 level = lvGetCurrentStage();
    s32 i;

    s_MaxShards = SHARD_MAX / getPlayerCount();

    if (level == LEVELID_STREETS || level == LEVELID_DEPOT)
    {
        s_MaxShards >>= 1;
    }

    s_Shards = mempAlloc(((s_MaxShards * sizeof(struct Shard)) + 0xF) & ~0xF, MEMPOOL_STAGE);

    for (i = 0; i < s_MaxShards; i++)
    {
        s_Shards[i].age = 0;
    }

    s_NextShard = 0;
    s_NumActiveShards = 0;
}

/**
 * Removes all shards.
 */
void glassResetShards(void)
{
    s32 i;

    for (i = 0; i < s_MaxShards; i++)
    {
        s_Shards[i].age = 0;
    }

    s_NumActiveShards = 0;
}
