/**
 * Smoke. Smoke is a prop of its own (PROP_TYPE_SMOKE), taken from a buffer of 20: for
 * its type's duration it makes a new cloud every few ticks, up to 10 at a time, which
 * rise, grow, spin and fade. Explosions leave smoke as they end, and rockets and
 * launched grenades trail it.
 *
 * In multiplayer, starting an explosion or more smoke makes existing smoke stop making
 * new clouds (smokesEndEarly), to keep the numbers down.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <gbi_extension.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "random.h"
#include "game/world/bg.h"
#include "game/world/roommtx.h"
#include "game/player/player.h"
#include "game/gfx/dyn.h"
#include "game/gfx/imagebank.h"
#include "game/level/lv.h"
#include "game/prop/prop.h"
#include "game/prop/propobj.h"
#include "game/prop/explosion.h"
#include <assets/oddtextures.h>

extern Gfx globalDL_0x000 NO_SDATA;

/* Clouds are drawn this far towards the eye (at most), so they aren't cut by walls */
#define SMOKE_PULLBACK 100.0f

/* Smoke vertices are in tenths of a unit (drawn with s_SmokeScaleMtx) */
#define SMOKE_VERTEX_SCALE 10.0f

/* Clouds further than this, or with a corner out of vertex range, aren't drawn */
#define SMOKE_MAX_DIST 30000.0f

/* The smoke types (SMOKE_TYPE). EU runs at 50 Hz: shorter times. */
#if defined(VERSION_EU)
static s_smoketype s_SmokeTypes[SMOKE_TYPE_COUNT] = {
   // dur, appr, dis, size, bgrate,    r,  g,    b, fgrate, propclouds
    {   1,   50,  99,    0,   0.0f, 128, 128, 128,   0.3f,     150 },
    { 400,   50,  37,   60,  0.02f,  80,  80,  96,   0.3f,     150 },
    { 400,   50,  42,   20,  0.01f, 128, 128, 128,   0.3f,     150 },
    { 525,   50, 100,  100,  0.01f, 192, 192, 192,   0.3f,     150 },
    { 525,   50,  50,   80,  0.02f,  64,  64,  64,   0.3f,     150 },
    { 640,   50,  42,  190,  0.15f,  64,  64,  64,   0.3f,     150 },
    { 750,   50,  58,  300,  0.01f,  64,  64,  64,   0.3f,     150 },
    {  50,   50,   7,   15,  0.03f, 255, 255, 255,   0.3f,     150 },
    {  17,    1,   5,   30,  0.03f, 255, 255, 255,   2.0f,      25 },
    {  21,    1,   6,   16,  0.03f, 224, 224, 224,   3.0f,      25 },
    { 750,   50,  58,  900,  0.01f,  64,  64,  64,   0.3f,     150 }
};
#else
static s_smoketype s_SmokeTypes[SMOKE_TYPE_COUNT] = {
   // dur, appr, dis,size, bgrate,   r,  g,    b, fgrate, propclouds
    {   1,   60,  99,   0,   0.0f, 128, 128, 128,   0.3f,     180},
    { 480,   60,  45,  60,  0.02f,  80,  80,  96,   0.3f,     180},
    { 480,   60,  50,  20,  0.01f, 128, 128, 128,   0.3f,     180},
    { 640,   60, 120, 100,  0.01f, 192, 192, 192,   0.3f,     180},
    { 640,   60,  60,  80,  0.02f,  64,  64,  64,   0.3f,     180},
    { 770,   60,  50, 190,  0.15f,  64,  64,  64,   0.3f,     180},
    { 900,   60,  70, 300,  0.01f,  64,  64,  64,   0.3f,     180},
    {  60,   60,   8,  15,  0.03f, 255, 255, 255,   0.3f,     180},
    {  20,    1,   6,  30,  0.03f, 255, 255, 255,   2.0f,      30},
    {  25,    1,   7,  16,  0.03f, 224, 224, 224,   3.0f,      30},
    { 900,   60,  70, 900,  0.01f,  64,  64,  64,   0.3f,     180}
};
#endif

static struct Smoke *s_Smokes;
static Mtx s_SmokeScaleMtx;

static const Vtx s_CloudVertex = {{{0, 0, 0}, 0, {0, 0}, {0x0, 0x0, 0x0, 0x0}}};

/**
 * Called once at boot: the scale matrix smoke is drawn with.
 */
void smokeInitScaleMtx(void)
{
    guScale(&s_SmokeScaleMtx, 0.1f, 0.1f, 0.1f);
}

/**
 * Makes the first count smokes stop making new clouds, except the short puffs and
 * trails.
 */
void smokesEndEarly(s32 count)
{
    struct Smoke *smoke;
    s32 i;

    for (i = 0; i < count; i++)
    {
        smoke = &s_Smokes[i];

        if (smoke->prop != NULL
            && smoke->smoke_type != SMOKE_TYPE_PUFF
            && smoke->smoke_type != SMOKE_TYPE_ROCKET
            && smoke->smoke_type != SMOKE_TYPE_GRENADEROUND)
        {
            smoke->duration = s_SmokeTypes[smoke->smoke_type].duration;
        }
    }
}

/**
 * Creates smoke at pos, if there's a free slot. Smoke in the air is drawn even where its
 * floor tile's room isn't.
 */
void smokeCreate(coord3d *pos, StandTile *stan, s16 type, u8 *rooms, s32 inAir)
{
    struct Smoke *smoke;
    PropRecord *prop;
    s32 i;

    for (i = 0; i < SMOKE_BUFFER_LEN; i++)
    {
        if (s_Smokes[i].prop == NULL)
        {
            break;
        }
    }

    // in multiplayer, the smokes before the free slot stop making new clouds
    if (getPlayerCount() >= 2)
    {
        smokesEndEarly(i);
    }

    if (i == SMOKE_BUFFER_LEN)
    {
        return;
    }

    smoke = &s_Smokes[i];
    prop = propAllocate();

    if (prop == NULL)
    {
        return;
    }

    prop->type = PROP_TYPE_SMOKE;
    prop->flags |= PROPFLAG_ONSCREEN;
    prop->smoke = smoke;
    prop->stan = stan;
    prop->pos = *pos;
    propSetRooms(prop, rooms);

    if (inAir)
    {
        prop->flags |= PROPFLAG_00000008;
    }

    propActivateThisFrame(prop);
    propEnable(prop);

    smoke->prop = prop;
    smoke->duration = 0;
    smoke->smoke_type = type;
}

/**
 * Moves a cloud on a tick: it rises, grows, fades and spins, and wobbles sideways.
 */
static void smokePartTick(struct SmokePart *part, s_smoketype *type)
{
    part->pos.f[1] += 0.3f;
    part->size += 0.15f;
    part->alpha -= type->fg_rotrate;
    part->count++;
    part->rot += part->deltarot;
    part->offset1 += 0.02f + RANDOMFRAC() * 0.01f;
    part->offset2 += 0.02f + RANDOMFRAC() * 0.01f;

    if (part->alpha < 4.0f)
    {
        part->size = 0.0f;
    }
}

/**
 * Starts a cloud at the smoke's position: a random size, opacity, angle and spin. The
 * clouds made near the end start fainter.
 */
static void smokePartStart(struct Smoke *smoke, struct SmokePart *part, s_smoketype *type)
{
    part->size = type->size * (RANDOMFRAC() * 0.5f + 1.0f);
    part->alpha = (randomGetNext() % 70) + 110.0f;
    part->count = 0;
    part->rot = RANDOMFRAC() * M_TAU_F;
    part->deltarot = (0.5f - RANDOMFRAC()) * type->bg_rotrate;
    part->pos = smoke->prop->pos;
    part->offset1 = RANDOMFRAC() * 0.5f;
    part->offset2 = RANDOMFRAC() * 0.5f;

    if (smoke->duration > type->duration - type->propagated_clouds)
    {
        part->alpha *= (type->duration - smoke->duration) / (f32)type->propagated_clouds;
    }
}

/**
 * Ticks smoke: moves its clouds and makes new ones while it lasts, and updates its rooms.
 * Frees it once it's done and its clouds have gone.
 */
s32 smokeTick(PropRecord *prop)
{
    struct Smoke *smoke = prop->smoke;
    s_smoketype *type = &s_SmokeTypes[smoke->smoke_type];
    struct SmokePart *part;
    coord3d bbmin;
    coord3d bbmax;
    f32 lvupdate;
    f32 radius;
    f32 dx;
    bool finished;
    s32 i;
    s32 j;
    s32 k;

    if (g_ClockTimer == 0)
    {
        return TICKOP_NONE;
    }

    lvupdate = (g_ClockTimer < 15) ? (f32)g_ClockTimer : 15.0f;

    for (i = 0; i < (s32)lvupdate; i++)
    {
        smoke->duration++;

        for (j = 0; j < SMOKE_PARTS_LEN; j++)
        {
            if (smoke->parts[j].size != 0.0f)
            {
                smokePartTick(&smoke->parts[j], type);
            }
        }

        // a new cloud every ratedissolve ticks, in the first free slot
        if (smoke->duration < type->duration && smoke->duration % type->ratedissolve == 1)
        {
            for (j = 0; j < SMOKE_PARTS_LEN; j++)
            {
                if (smoke->parts[j].size == 0.0f)
                {
                    smokePartStart(smoke, &smoke->parts[j], type);
                    break;
                }
            }
        }
    }

    // the rooms it's in. The max side takes pos - size like the min side, so the box
    // only grows downwards (as in the original game).
    bbmin.x = prop->pos.x - 1.0f;
    bbmin.y = prop->pos.y - 1.0f;
    bbmin.z = prop->pos.z - 1.0f;

    bbmax.x = prop->pos.x + 1.0f;
    bbmax.y = prop->pos.y + 1.0f;
    bbmax.z = prop->pos.z + 1.0f;

    for (j = 0; j < SMOKE_PARTS_LEN; j++)
    {
        part = &smoke->parts[j];

        if (part->size != 0.0f)
        {
            for (k = 0; k < 3; k++)
            {
                if (bbmin.f[k] > part->pos.f[k] - part->size)
                {
                    bbmin.f[k] = part->pos.f[k] - part->size;
                }
                else if (bbmax.f[k] < part->pos.f[k] - part->size)
                {
                    bbmax.f[k] = part->pos.f[k] - part->size;
                }
            }
        }
    }

    radius = 0.0f;
    dx = prop->pos.f[0] - bbmin.f[0];

    if (dx > 0.0f)
    {
        radius = dx;
    }

    if (radius < prop->pos.f[2] - bbmin.f[2])
    {
        radius = prop->pos.f[2] - bbmin.f[2];
    }

    if (radius < bbmax.f[0] - prop->pos.f[0])
    {
        radius = bbmax.f[0] - prop->pos.f[0];
    }

    if (radius < bbmax.f[2] - prop->pos.f[2])
    {
        radius = bbmax.f[2] - prop->pos.f[2];
    }

    propCalcRooms(prop, &bbmin, &bbmax, radius);

    // finished once its clouds have gone (checked once it's older than the time
    // between clouds)
    finished = FALSE;

    if (smoke->duration > type->ratedissolve)
    {
        finished = TRUE;

        for (j = 0; j < SMOKE_PARTS_LEN; j++)
        {
            if (smoke->parts[j].size > 0.0f)
            {
                finished = FALSE;
                break;
            }
        }
    }

    if (finished)
    {
        smoke->prop = NULL;
        return TICKOP_FREE;
    }

    return TICKOP_NONE;
}

/**
 * Sets the smoke's depth for drawing order, as for an explosion.
 */
u8 smokePropTick(PropRecord *prop)
{
    return explosionPropTick(prop);
}

/**
 * Draws a cloud: a square facing the camera, pulled towards the eye like an explosion's
 * fireball, wobbling sideways, and fading in over its first rateappear ticks.
 */
static Gfx *smokeRenderPart(struct Smoke *smoke, struct SmokePart *part, Gfx *gdl, coord3d *roompos)
{
    s_smoketype *type = &s_SmokeTypes[smoke->smoke_type];
    Mtxf *mtx = camGetViewToWorldMtxf();
    coord3d *eyepos = playerGetPos();
    Vtx *vertices;
    coord3d rightcos;
    coord3d rightsin;
    coord3d upcos;
    coord3d upsin;
    f32 cossize;
    f32 sinsize;
    f32 cx;
    f32 cy;
    f32 cz;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dist;
    f32 pullback;
    f32 frac;
    f32 x0;
    f32 y0;
    f32 z0;
    u8 alpha;
    s32 i;

    if (type->rateappear >= part->count)
    {
        alpha = (part->alpha / (f32)type->rateappear) * part->count;
    }
    else
    {
        alpha = part->alpha;
    }

    vertices = dynAllocateVertices(4);

    for (i = 0; i < 4; i++)
    {
        vertices[i] = s_CloudVertex;
    }

    cossize = cosf(part->rot) * part->size;
    sinsize = sinf(part->rot) * part->size;

    cx = part->pos.f[0] + (sinf(part->offset1) * 7.0f);
    cy = part->pos.f[1];
    cz = part->pos.f[2] + (sinf(part->offset2) * 7.0f);

    dx = cx - eyepos->f[0];
    dy = cy - eyepos->f[1];
    dz = cz - eyepos->f[2];

    dist = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

    if (dist > SMOKE_MAX_DIST)
    {
        return gdl;
    }

    pullback = dist * 0.5f;

    if (pullback > SMOKE_PULLBACK)
    {
        pullback = SMOKE_PULLBACK;
    }

    frac = (dist == 0.0f) ? 0.0f : (dist - pullback) / dist;

    cossize = cossize * frac;
    sinsize = sinsize * frac;

    cx = eyepos->f[0] + dx * frac;
    cy = eyepos->f[1] + dy * frac;
    cz = eyepos->f[2] + dz * frac;

    for (i = 0; i < 3; i++)
    {
        rightcos.f[i] = mtx->m[0][i] * cossize;
        rightsin.f[i] = mtx->m[0][i] * sinsize;
        upcos.f[i] = mtx->m[1][i] * cossize;
        upsin.f[i] = mtx->m[1][i] * sinsize;
    }

    x0 = ((cx - rightcos.f[0] - upsin.f[0]) * bgGetLevelScale() - roompos->f[0]) * SMOKE_VERTEX_SCALE;
    y0 = ((cy - rightcos.f[1] - upsin.f[1]) * bgGetLevelScale() - roompos->f[1]) * SMOKE_VERTEX_SCALE;
    z0 = ((cz - rightcos.f[2] - upsin.f[2]) * bgGetLevelScale() - roompos->f[2]) * SMOKE_VERTEX_SCALE;

    if (x0 > SMOKE_MAX_DIST || x0 < -SMOKE_MAX_DIST
        || y0 > SMOKE_MAX_DIST || y0 < -SMOKE_MAX_DIST
        || z0 > SMOKE_MAX_DIST || z0 < -SMOKE_MAX_DIST)
    {
        return gdl;
    }

    vertices[0].v.ob[0] = x0;
    vertices[0].v.ob[1] = y0;
    vertices[0].v.ob[2] = z0;
    vertices[0].v.tc[0] = 1760;
    vertices[0].v.tc[1] = 0;

    vertices[1].v.ob[0] = ((cx + rightsin.f[0] - upcos.f[0]) * bgGetLevelScale() - roompos->f[0]) * SMOKE_VERTEX_SCALE;
    vertices[1].v.ob[1] = ((cy + rightsin.f[1] - upcos.f[1]) * bgGetLevelScale() - roompos->f[1]) * SMOKE_VERTEX_SCALE;
    vertices[1].v.ob[2] = ((cz + rightsin.f[2] - upcos.f[2]) * bgGetLevelScale() - roompos->f[2]) * SMOKE_VERTEX_SCALE;
    vertices[1].v.tc[0] = 0;
    vertices[1].v.tc[1] = 0;

    vertices[2].v.ob[0] = ((cx + rightcos.f[0] + upsin.f[0]) * bgGetLevelScale() - roompos->f[0]) * SMOKE_VERTEX_SCALE;
    vertices[2].v.ob[1] = ((cy + rightcos.f[1] + upsin.f[1]) * bgGetLevelScale() - roompos->f[1]) * SMOKE_VERTEX_SCALE;
    vertices[2].v.ob[2] = ((cz + rightcos.f[2] + upsin.f[2]) * bgGetLevelScale() - roompos->f[2]) * SMOKE_VERTEX_SCALE;
    vertices[2].v.tc[0] = 0;
    vertices[2].v.tc[1] = 1760;

    vertices[3].v.ob[0] = ((cx - rightsin.f[0] + upcos.f[0]) * bgGetLevelScale() - roompos->f[0]) * SMOKE_VERTEX_SCALE;
    vertices[3].v.ob[1] = ((cy - rightsin.f[1] + upcos.f[1]) * bgGetLevelScale() - roompos->f[1]) * SMOKE_VERTEX_SCALE;
    vertices[3].v.ob[2] = ((cz - rightsin.f[2] + upcos.f[2]) * bgGetLevelScale() - roompos->f[2]) * SMOKE_VERTEX_SCALE;
    vertices[3].v.tc[0] = 1760;
    vertices[3].v.tc[1] = 1760;

    for (i = 0; i < 4; i++)
    {
        vertices[i].v.cn[0] = type->r;
        vertices[i].v.cn[1] = type->g;
        vertices[i].v.cn[2] = type->b;
        vertices[i].v.cn[3] = alpha;
    }

    gSPVertex(gdl++, osVirtualToPhysical(vertices), 4, 0);
    gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);

    return gdl;
}

/**
 * Draws smoke (in the translucent pass): each of its clouds.
 */
Gfx *smokeRender(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    struct Smoke *smoke = prop->smoke;
    s32 room = prop->rooms[0];
    coord3d *roompos = roomGetPos(room);
    bbox2d bbox;
    s32 i;

    if (!withalpha)
    {
        return gdl;
    }

    if (propGetRoomsBbox2d(prop, &bbox) > 0)
    {
        gdl = bgScissorCurrentPlayerViewF(gdl, bbox.min.f[0], bbox.min.f[1], bbox.max.f[0], bbox.max.f[1]);
    }
    else
    {
        gdl = bgScissorCurrentPlayerViewDefault(gdl);
    }

    gSPClearGeometryMode(gdl++, G_CULL_BOTH | G_FOG);
    gSPMatrix(gdl++, osVirtualToPhysical(camGetProjViewMtx()), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    gdl = roomMtxLoad(gdl, room);

    gSPMatrix(gdl++, osVirtualToPhysical(&s_SmokeScaleMtx), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPSegment(gdl++, SPSEGMENT_GETITLE, osVirtualToPhysical(g_ImageBank));
    gSPDisplayList(gdl++, &globalDL_0x000);
    gDPSetColorDither(gdl++, G_CD_NOISE);

    for (i = 0; i < SMOKE_PARTS_LEN; i++)
    {
        if (smoke->parts[i].size > 0.0f)
        {
            gdl = smokeRenderPart(smoke, &smoke->parts[i], gdl, roompos);
        }
        else
        {
            smoke->parts[i].size = 0.0f;
        }
    }

    gDPSetColorDither(gdl++, G_CD_BAYER);
    gSPMatrix(gdl++, osVirtualToPhysical(camGetProjectionMtx()), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    return gdl;
}

/**
 * Allocates the level's smoke, all free.
 */
void smokesInit(void)
{
    s32 i;
    s32 j;

    s_Smokes = mempAlloc(SMOKE_BUFFER_LEN * sizeof(struct Smoke), MEMPOOL_STAGE);

    for (i = 0; i < SMOKE_BUFFER_LEN; i++)
    {
        s_Smokes[i].prop = NULL;

        for (j = 0; j < SMOKE_PARTS_LEN; j++)
        {
            s_Smokes[i].parts[j].size = 0.0f;
        }
    }
}

/**
 * Removes every smoke prop.
 */
void smokesReset(void)
{
    s32 i;

    if (s_Smokes == NULL)
    {
        return;
    }

    for (i = 0; i < SMOKE_BUFFER_LEN; i++)
    {
        if (s_Smokes[i].prop != NULL)
        {
            propDelist(s_Smokes[i].prop);
            propDisable(s_Smokes[i].prop);
            propFree(s_Smokes[i].prop);
            s_Smokes[i].prop = NULL;
        }
    }
}
