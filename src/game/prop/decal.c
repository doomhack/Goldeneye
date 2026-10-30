/**
 * Decals: marks left on surfaces. Scorch marks on the floor where explosions went off
 * (a ring buffer of 20, solo only) and bullet holes and other impacts on walls and objects
 * (a ring buffer of 100). The oldest are reused when a buffer is full, and the ten oldest
 * impacts fade out ahead of that.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <gbi_extension.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "fr.h"
#include "random.h"
#include "game/world/bg.h"
#include "game/world/roommtx.h"
#include "game/player/player.h"
#include "game/menu/cheat.h"
#include "game/gfx/imagebank.h"
#include "game/gfx/texcmds.h"
#include "game/math/matrix.h"
#include "game/prop/decal.h"

/* How many of the oldest impacts fade out (the oldest fully) */
#define IMPACT_FADE_COUNT 10

/* The largest a scorch mark gets */
#define SCORCH_MAX_SIZE 200.0f

typedef struct s_impacttype {
    f32 width;                 /* half the mark's size */
    f32 height;
    u8 apptype;                /* 0 dark, 1 light, 2 a random bright colour */
    u8 renderMode;             /* TEXBLEND */
    u8 zbufferMode;            /* TEXZBUF */
} s_impacttype;

static struct Scorch *s_Scorches;
static struct Impact *s_Impacts;
static s32 s_NextScorch = 0;
static s32 s_NextImpact = 0;

/* The impact types. Their images are g_ImagesImpact, in the same order. */
static s_impacttype s_ImpactTypes[IMPACT_TYPE_COUNT] = {
    {10.0f, 10.0f, 1, 2, 8},
    { 6.0f,  6.0f, 1, 2, 8},
    { 8.0f,  8.0f, 0, 2, 8},
    {20.0f, 20.0f, 1, 2, 8},
    { 6.0f,  6.0f, 1, 2, 8},
    { 8.0f,  8.0f, 1, 2, 8},
    {12.0f, 12.0f, 1, 2, 8},
    { 6.0f,  6.0f, 1, 2, 8},
    {20.0f, 20.0f, 1, 2, 8},
    {20.0f, 20.0f, 1, 2, 8},
    {20.0f, 20.0f, 1, 2, 8},
    {20.0f, 20.0f, 1, 2, 8},
    {20.0f, 20.0f, 1, 2, 8},
    {24.0f, 24.0f, 1, 2, 8},
    { 6.0f,  6.0f, 1, 2, 1},
    { 6.0f,  6.0f, 1, 2, 1},
    {24.0f, 24.0f, 2, 2, 8},   /* paintball */
    { 6.0f,  6.0f, 1, 2, 1},   /* cracks in glass */
    { 8.0f,  8.0f, 1, 2, 1},
    {12.0f, 12.0f, 1, 2, 1},
};

static const Vtx s_DecalVertex = {{{0, 0, 0}, 0, {0, 0}, {0x0, 0x0, 0x0, 0xDC}}};

/**
 * Leaves a scorch mark of about the given size on the floor at pos (in solo only).
 * pos is changed to be relative to the room.
 */
void scorchCreate(coord3d *pos, f32 size, s16 room)
{
    f32 angle = RANDOMFRAC() * M_TAU_F;
    u8 alpha = 0xFF - (randomGetNext() % 80U);
    coord3d *roompos = roomGetPos(room);

    if (isMpGame())
    {
        return;
    }

    struct Scorch *scorch = &s_Scorches[s_NextScorch];
    s32 i;

    if (size > SCORCH_MAX_SIZE)
    {
        size = SCORCH_MAX_SIZE;
    }

    size *= 0.8f + (0.2f * RANDOMFRAC());

    pos->f[0] = (pos->f[0] * bgGetLevelScale()) - roompos->f[0];
    pos->f[1] = (pos->f[1] * bgGetLevelScale()) - roompos->f[1];
    pos->f[2] = (pos->f[2] * bgGetLevelScale()) - roompos->f[2];

    size *= bgGetLevelScale();

    f32 cosSize = cosf(angle) * size;
    f32 sinSize = sinf(angle) * size;

    // A square turned to the random angle: each corner's x and z offsets, and texture coords
    f32 dx[4] = { cosSize, sinSize, -cosSize, -sinSize };
    f32 dz[4] = { sinSize, -cosSize, -sinSize, cosSize };
    s16 s[4] = { 0, 0, g_ImageGeneric->height << 5, g_ImageGeneric->width << 5 };
    s16 t[4] = { g_ImageGeneric->width << 5, 0, 0, g_ImageGeneric->height << 5 };

    scorch->room = room;
    scorch->pos.f[0] = pos->f[0];
    scorch->pos.f[1] = pos->f[1];
    scorch->pos.f[2] = pos->f[2];
    scorch->size = size;

    for (i = 0; i < 4; i++)
    {
        scorch->vertices[i] = s_DecalVertex;
    }

    for (i = 0; i < 4; i++)
    {
        Vtx *vtx = &scorch->vertices[i];

        vtx->v.ob[0] = (s16)(s32)(pos->f[0] + dx[i]);
        vtx->v.ob[1] = (s16)(s32)(pos->f[1] + 0.5f);
        vtx->v.ob[2] = (s16)(s32)(pos->f[2] + dz[i]);
        vtx->v.tc[0] = s[i];
        vtx->v.tc[1] = t[i];

        u8 shade = randomGetNext() % 50U;

        vtx->v.cn[2] = shade;
        vtx->v.cn[1] = shade;
        vtx->v.cn[0] = shade;
        vtx->v.cn[3] = alpha;
    }

    s_NextScorch++;

    if (s_NextScorch >= SCORCH_BUFFER_LEN)
    {
        s_NextScorch = 0;
    }
}

/**
 * Draws the scorch marks in visible rooms (none in multiplayer).
 */
Gfx *scorchesRender(Gfx *gdl)
{
    s32 lastroom = -1;
    s32 i;

    if (isMpGame())
    {
        return gdl;
    }

    gSPSetGeometryMode(gdl++, G_CULL_BACK);
    gSPClearGeometryMode(gdl++, G_CULL_FRONT | G_FOG);
    gDPSetColorDither(gdl++, G_CD_NOISE);

    texSelect(&gdl, g_ImageGeneric, TEXBLEND_CLOUD, TEXZBUF_ON, 2);

    for (i = 0; i < SCORCH_BUFFER_LEN; i++)
    {
        struct Scorch *scorch = &s_Scorches[i];

        if (scorch->room < 0 || !bgRoomIsVisible(scorch->room))
        {
            continue;
        }

        if (lastroom != scorch->room)
        {
            lastroom = scorch->room;
            gdl = roomMtxLoad(gdl, scorch->room);
        }

        gSPVertex(gdl++, osVirtualToPhysical(scorch->vertices), 4, 0);
        gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
    }

    gDPSetColorDither(gdl++, G_CD_BAYER);

    return gdl;
}

/* Rounds to the nearest whole number, halves away from zero */
static s32 roundToInt(f32 value)
{
    if (value >= 0.0f)
    {
        return (s32)(value + 0.5f);
    }

    return (s32)(value - 0.5f);
}

/**
 * Removes the impacts on prop that are (or aren't) on glass.
 */
void impactsRemoveFromProp(PropRecord *prop, bool onGlass)
{
    s32 i;

    for (i = 0; i < IMPACT_BUFFER_LEN; i++)
    {
        if (prop == s_Impacts[i].prop && onGlass == s_Impacts[i].onGlass)
        {
            s_Impacts[i].room = -1;
        }
    }
}

/**
 * Removes the translucent impacts on prop (all of them, as every type is translucent):
 * for when it's damaged and changes shape.
 */
void impactsRemoveTranslucent(PropRecord *prop)
{
    s32 i;

    for (i = 0; i < IMPACT_BUFFER_LEN; i++)
    {
        if (prop == s_Impacts[i].prop && s_ImpactTypes[s_Impacts[i].type].renderMode == TEXBLEND_TRANSLUCENT)
        {
            s_Impacts[i].room = -1;
        }
    }
}

/* Fades out the oldest impacts, from index (the oldest, which goes clear) onwards */
static void impactsFadeOldest(s32 index)
{
    s32 i;

    for (i = 0; i < IMPACT_FADE_COUNT; i++)
    {
        u32 alpha = (u32)(((f32)i / 10.0f) * 255.0f);

        s_Impacts[index].vertices[3].v.cn[3] = alpha;
        s_Impacts[index].vertices[2].v.cn[3] = alpha;
        s_Impacts[index].vertices[1].v.cn[3] = alpha;
        s_Impacts[index].vertices[0].v.cn[3] = alpha;

        if (++index >= IMPACT_BUFFER_LEN)
        {
            index = 0;
        }
    }
}

/**
 * Leaves an impact mark of the given type at pos, facing along normal. On a wall, room is
 * the room it's in; on an object, prop is the object and node the model node it's on.
 * The paintball cheat swaps every mark for a splat of paint.
 */
void impactCreate(coord3d *pos, coord3d *normal, s16 type, s16 room, PropRecord *prop, s8 node, s8 onGlass)
{
    s_impacttype *impact;
    struct Impact *slot;
    coord3d centre;
    f32 halfw;
    f32 halfh;
    f32 rightX;
    f32 rightZ;
    f32 upX;
    f32 upY;
    f32 upZ;
    u8 red;
    u8 green = 0;
    u8 blue = 0;
    s32 i;

    if (cheatIsActive(CHEAT_PAINTBALL))
    {
        type = IMPACT_TYPE_PAINTBALL;
    }

    centre.f[0] = pos->f[0];
    centre.f[1] = pos->f[1];
    centre.f[2] = pos->f[2];

    impact = &s_ImpactTypes[type];
    halfw = impact->width;
    halfh = impact->height;

    // The mark's right and up directions, flat against the surface
    if (normal->f[0] == 0.0f && normal->f[2] == 0.0f)
    {
        upX = 0.0f;
        upY = 0.0f;
        rightZ = 0.0f;
        upZ = 1.0f;
        rightX = 1.0f;
    }
    else
    {
        f32 len = sqrtf((normal->f[0] * normal->f[0]) + (normal->f[1] * normal->f[1]) + (normal->f[2] * normal->f[2]));
        f32 nx = normal->f[0] / len;
        f32 ny = normal->f[1] / len;
        f32 nz = normal->f[2] / len;

        len = sqrtf((nx * nx) + (nz * nz));

        f32 hx = nx / len;
        f32 hz = nz / len;

        rightZ = -hx;
        upX = ny * hx;
        upY = -len;
        upZ = ny * hz;
        rightX = hz;
    }

    if (prop != NULL)
    {
        // On an object: sized to come out right once the node's matrix scales it
        ObjectRecord *obj = prop->obj;
        RenderPosView *renderpos = &obj->model->render_pos[node];
        coord3d right = {{ rightX, 0.0f, rightZ }};
        coord3d up = {{ upX, upY, upZ }};

        mtx4RotateVecInPlace(&renderpos->pos, &right);
        mtx4RotateVecInPlace(&renderpos->pos, &up);

        f32 rightlen = sqrtf((right.f[0] * right.f[0]) + (right.f[1] * right.f[1]) + (right.f[2] * right.f[2]));
        f32 uplen = sqrtf((up.f[0] * up.f[0]) + (up.f[1] * up.f[1]) + (up.f[2] * up.f[2]));

        halfw /= rightlen;
        halfh /= uplen;

        if (impact->zbufferMode < TEXZBUF_DECAL && impact->renderMode == TEXBLEND_TRANSLUCENT)
        {
            obj->state |= PROPSTATE_HASXLUIMPACTS;
        }
        else
        {
            obj->state |= PROPSTATE_HASIMPACTS;
        }
    }
    else
    {
        coord3d *roompos = roomGetPos(room);

        centre.f[0] = (centre.f[0] * bgGetLevelScale()) - roompos->f[0];
        centre.f[1] = (centre.f[1] * bgGetLevelScale()) - roompos->f[1];
        centre.f[2] = (centre.f[2] * bgGetLevelScale()) - roompos->f[2];
        halfw *= bgGetLevelScale();
        halfh *= bgGetLevelScale();
    }

    slot = &s_Impacts[s_NextImpact];
    slot->prop = prop;
    slot->node = node;
    slot->onGlass = onGlass;
    slot->room = room;
    slot->type = type;

    for (i = 0; i < 4; i++)
    {
        slot->vertices[i] = s_DecalVertex;
    }

    // Corners going round: left bottom, left top, right top, right bottom
    for (i = 0; i < 4; i++)
    {
        f32 sw = (i < 2) ? -1.0f : 1.0f;
        f32 sh = (i == 1 || i == 2) ? 1.0f : -1.0f;
        Vtx *vtx = &slot->vertices[i];

        vtx->v.ob[0] = roundToInt((centre.f[0] + sw * (halfw * rightX)) + sh * (halfh * upX));
        vtx->v.ob[1] = roundToInt(centre.f[1] + sh * (halfh * upY));
        vtx->v.ob[2] = roundToInt((centre.f[2] + sw * (halfw * rightZ)) + sh * (halfh * upZ));
        vtx->v.tc[0] = sw > 0.0f ? g_ImagesImpact[type].width << 5 : 0;
        vtx->v.tc[1] = sh < 0.0f ? g_ImagesImpact[type].height << 5 : 0;
    }

    for (i = 0; i < 4; i++)
    {
        switch (impact->apptype)
        {
            case 0:
            {
                u8 shade = randomGetNext() % 40U;
                red = green = blue = shade;
                break;
            }
            case 1:
            {
                u8 shade = 0xFF - (randomGetNext() % 40U);
                red = green = blue = shade;
                break;
            }
            case 2:
                red = (randomGetNext() & 1) ? 0xFF : 0;
                green = (randomGetNext() & 1) ? 0xFF : 0;
                blue = (randomGetNext() & 1) ? 0xFF : 0;
                break;
            default:
                red = 0;
                break;
        }

        slot->vertices[i].v.cn[0] = red;
        slot->vertices[i].v.cn[1] = green;
        slot->vertices[i].v.cn[2] = blue;
        slot->vertices[i].v.cn[3] = 0xFF;
    }

    s_NextImpact++;

    if (s_NextImpact >= IMPACT_BUFFER_LEN)
    {
        s_NextImpact = 0;
    }

    // The next slot is the oldest: it goes now, so it's free when it's reused
    impactsFadeOldest(s_NextImpact);
    s_Impacts[s_NextImpact].room = -1;
}

/**
 * Draws the impacts on prop (or with prop NULL, those on walls in visible rooms): in the
 * translucent pass those that are translucent and z-buffered, otherwise the rest. Clears
 * the object's flag for this pass if it has none left.
 */
Gfx *impactsRenderOnProp(Gfx *gdl, PropRecord *prop, s32 xlupass)
{
    ObjectRecord *obj = NULL;
    bool drewany = FALSE;
    s16 lastmtx = -1;
    s32 lasttype = -1;
    s32 i;

    if (prop != NULL)
    {
        obj = prop->obj;
    }

    gSPClearGeometryMode(gdl++, G_CULL_BOTH);
    gDPSetColorDither(gdl++, G_CD_NOISE);

    for (i = 0; i < IMPACT_BUFFER_LEN; i++)
    {
        struct Impact *impact = &s_Impacts[i];

        if (prop != impact->prop || impact->room < 0)
        {
            continue;
        }

        if (prop == NULL && !bgRoomIsVisible(impact->room))
        {
            continue;
        }

        s32 type = impact->type;
        bool xlu = s_ImpactTypes[type].zbufferMode < TEXZBUF_DECAL && s_ImpactTypes[type].renderMode == TEXBLEND_TRANSLUCENT;

        if (xlu != (xlupass != 0))
        {
            continue;
        }

        drewany = TRUE;

        if (prop != NULL)
        {
            if (lastmtx != impact->node)
            {
                lastmtx = impact->node;
                gSPMatrix(gdl++, osVirtualToPhysical(&obj->model->render_pos[impact->node]), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            }
        }
        else if (lastmtx != impact->room)
        {
            lastmtx = impact->room;
            gdl = roomMtxLoad(gdl, impact->room);
        }

        if (lasttype != type)
        {
            texSelect(&gdl, &g_ImagesImpact[type], s_ImpactTypes[type].renderMode, s_ImpactTypes[type].zbufferMode, 2);
            lasttype = type;
        }

        gSPVertex(gdl++, osVirtualToPhysical(impact->vertices), 4, 0);
        gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
    }

    if (prop != NULL && !drewany)
    {
        obj->state &= ~(1 << xlupass);
    }

    gDPSetColorDither(gdl++, G_CD_BAYER);

    return gdl;
}

/**
 * Draws the impacts on walls in visible rooms.
 */
Gfx *impactsRenderOnWalls(Gfx *gdl)
{
    return impactsRenderOnProp(gdl, NULL, 0);
}

/**
 * Allocates the level's scorch marks (solo only) and impacts, all free.
 */
void decalsInit(void)
{
    s32 i;

    s_NextScorch = 0;
    s_NextImpact = 0;

    if (!isMpGame())
    {
        s_Scorches = mempAlloc(SCORCH_BUFFER_LEN * sizeof(struct Scorch), MEMPOOL_STAGE);

        for (i = 0; i < SCORCH_BUFFER_LEN; i++)
        {
            s_Scorches[i].room = -1;
        }
    }

    s_Impacts = mempAlloc(IMPACT_BUFFER_LEN * sizeof(struct Impact), MEMPOOL_STAGE);

    for (i = 0; i < IMPACT_BUFFER_LEN; i++)
    {
        s_Impacts[i].room = -1;
    }
}
