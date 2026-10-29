/**
 * Bullet sparks: small camera-facing sprites where a bullet hits something. sparkCreate
 * adds one; lvTick ticks them (sparksTickAll) and lvRender draws them in each view
 * (sparksRenderAll). Moving sparks fly with gravity; PAL has none, so its *All functions
 * only handle the plain sparks.
 */

#include <ultra64.h>
#include <limits.h>
#include "game/world/bg.h"
#include "game/world/roommtx.h"
#include "game/player/bondview.h"
#include "game/gfx/dyn.h"
#include "game/math/trig.h"
#include "gbi_extension.h"
#include "game/prop/glass.h"
#include "game/gfx/imagebank.h"
#include "game/level/lv.h"
#include "game/mission/objective.h"
#include "random.h"
#include "game/math/physics.h"
#include "game/hud/sparks.h"
#include "game/gfx/texcmds.h"

#define BULLET_SPARKS_MAX 20
#define BULLET_MOVING_SPARKS_MAX 50

rgba_u8 g_SparkColors[8] = {
    { {0xFF, 0xFF, 0xFF, 0xFF} },
    { {0xFF, 0xFF, 0xC8, 0xFF} },
    { {0xFF, 0x00, 0x00, 0xFF} },
    { {0xFF, 0xFF, 0xFF, 0xFF} },
    { {0xFF, 0xFF, 0xFF, 0xFF} },
    { {0xFF, 0xFF, 0xFF, 0xFF} },
    {},
    {}
};
Vtx g_SparkVtxTemplate = {};


// something explosion related
// size of each item is 0x2c (see sparkCreate)
s_bullet_spark g_Sparks[BULLET_SPARKS_MAX];

#ifndef VERSION_EU

s_moving_bullet_spark g_MovingSparks[BULLET_MOVING_SPARKS_MAX];

#endif


void sparksReset(void)
{
    s32 i;
    s32 start_index;

    if (1) { start_index = 0; }

    for (i = start_index; (i < BULLET_SPARKS_MAX) ^ 0; i++)
    {
        g_Sparks[i].images = 0;
        g_Sparks[i].lifetime = 0;
        g_Sparks[i].age = 0;
    }
}


void sparkInit(s_bullet_spark *spark, coord3d *pos, s32 type, f32 size, s16 room)
{
    f32 angle;

    angle = randomGetNext();
    angle *= (1.0f / M_U32_MAX_VALUE_F);
    angle *= M_TAU_F;

    spark->age = 0;
    spark->room = room;

    if (type == 4)
    {
        spark->lifetime = 1;
        spark->frameRate = 1.0f;
        spark->images = g_ImageFlare2;
    }
    else if (type == 1)
    {
        spark->lifetime = 11;
        spark->frameRate = 0.5f;
        spark->images = g_ImagesExplosionSmoke;
    }
    else if (type == 3)
    {
        spark->lifetime = 9;
        spark->frameRate = 0.5f;
        spark->images = g_ImagesScatteredExplosions;
    }
    else if (type == 6)
    {
        spark->lifetime = 100;
        spark->frameRate = 0.0f;
        spark->images = g_ImageFlare2;
    }
    else
    {
        spark->lifetime = 11;
        spark->frameRate = 0.5f;
        spark->images = g_ImagesExplosionSmoke;
    }

    spark->r = g_SparkColors[type].r;
    spark->g = g_SparkColors[type].g;
    spark->b = g_SparkColors[type].b;
    spark->a = g_SparkColors[type].a;

    spark->pos.x = pos->x;
    spark->pos.y = pos->y;
    spark->pos.z = pos->z;

    size *= 1.0f + ((f32)randomGetNext() * (1.0f / M_U32_MAX_VALUE_F) * 0.25f);
    size *= M_SQRT2_F;
    spark->size = size;

    spark->sizeCos = cosf(angle) * size;
    spark->sizeSin = sinf(angle) * size;
}


s_bullet_spark *sparkCreate(coord3d *pos, s32 type, f32 size, s16 room)
{
    s_bullet_spark *ptr;

    for (ptr = &g_Sparks[0]; ptr < &g_Sparks[BULLET_SPARKS_MAX]; ptr++)
    {
        if (ptr->lifetime == 0)
        {
            sparkInit(ptr, pos, type, size, room);
            return ptr;
        }
    }

    return NULL;
}


void sparksTick(void)
{
    s_bullet_spark *thing = &g_Sparks[0]; \
    s_bullet_spark *end = g_Sparks + BULLET_SPARKS_MAX;

    for (; thing < end; thing++)
    {
        if (thing->lifetime > 0)
        {
            thing->age += g_ClockTimer;

            if (thing->age >= 0 && thing->age >= thing->lifetime)
            {
                thing->lifetime = 0;
            } 
        }
    }
}


void sparkRender(s_bullet_spark *thing, Gfx **gdlptr, s32 zbufferMode)
{
    Vtx vtx;
    Mtxf *mtx;
    Gfx *gdl;
    Vtx *vertices;
    f32 z;
    f32 y;
    f32 x;
    f32 s0[3];
    f32 s1[3];
    f32 s2[3];
    f32 s3[3];
    s32 frame;
    s32 room;
    coord3d *roompos;
    
    if (thing->lifetime <= 0)
    {
        return;
    }
    
    if (thing->age < 0)
    {
        return;
    }
    
    if (!camIsPosInScreen((coord3d *) (&thing->pos.x), *((f32 *) (&thing->size))))
    {
        return;
    }

    vtx = g_SparkVtxTemplate;
    mtx = camGetViewToWorldMtxf();
    gdl = *gdlptr;
    vertices = dynAllocateVertices(4);
    room = thing->room;
    roompos = roomGetPos(room);
    vtx.v.cn[0] = ((u8 *) thing)[0x28];
    vtx.v.cn[1] = ((u8 *) thing)[0x29];
    vtx.v.cn[2] = ((u8 *) thing)[0x2a];
    vtx.v.cn[3] = ((u8 *) thing)[0x2b];
    frame = (s32) (((f32) thing->age) * (*(&thing->frameRate)));
    
    x = *((f32 *) (&thing->pos.x));
    y = *((f32 *) (&thing->pos.y));
    z = *((f32 *) (&thing->pos.z));
    
    s0[0] = mtx->m[0][0] * thing->sizeCos;
    s0[1] = mtx->m[0][1] * thing->sizeCos;
    s0[2] = mtx->m[0][2] * thing->sizeCos;
    s1[0] = mtx->m[0][0] * thing->sizeSin;
    s1[1] = mtx->m[0][1] * thing->sizeSin;
    s1[2] = mtx->m[0][2] * thing->sizeSin;
    s2[0] = mtx->m[1][0] * thing->sizeCos;
    s2[1] = mtx->m[1][1] * thing->sizeCos;
    s2[2] = mtx->m[1][2] * thing->sizeCos;
    s3[0] = mtx->m[1][0] * thing->sizeSin;
    s3[1] = mtx->m[1][1] * thing->sizeSin;
    s3[2] = mtx->m[1][2] * thing->sizeSin;

    vertices[0] = vtx;
    vertices[1] = vtx;
    vertices[2] = vtx;
    vertices[3] = vtx;
    vertices[0].v.ob[0] = (((x - s0[0]) - s3[0]) * bgGetLevelScale()) - roompos->f[0];
    vertices[0].v.ob[1] = (((y - s0[1]) - s3[1]) * bgGetLevelScale()) - roompos->f[1];
    vertices[0].v.ob[2] = (((z - s0[2]) - s3[2]) * bgGetLevelScale()) - roompos->f[2];


    vertices[0].v.tc[0] = ((struct sImageTableEntry *) (((u8 *) thing->images) + (frame * 12)))->width << 5;
    vertices[0].v.tc[1] = 0;
    vertices[1].v.ob[0] = (((x + s1[0]) - s2[0]) * bgGetLevelScale()) - roompos->f[0];
    vertices[1].v.ob[1] = (((y + s1[1]) - s2[1]) * bgGetLevelScale()) - roompos->f[1];
    vertices[1].v.ob[2] = (((z + s1[2]) - s2[2]) * bgGetLevelScale()) - roompos->f[2];
    vertices[1].v.tc[0] = 0;
    vertices[1].v.tc[1] = 0;
    vertices[2].v.ob[0] = (((x + s0[0]) + s3[0]) * bgGetLevelScale()) - roompos->f[0];
    vertices[2].v.ob[1] = (((y + s0[1]) + s3[1]) * bgGetLevelScale()) - roompos->f[1];
    vertices[2].v.ob[2] = (((z + s0[2]) + s3[2]) * bgGetLevelScale()) - roompos->f[2];
    vertices[2].v.tc[0] = 0;
    vertices[2].v.tc[1] = ((struct sImageTableEntry *) (((u8 *) thing->images) + (frame * 12)))->height << 5;
    vertices[3].v.ob[0] = (((x - s1[0]) + s2[0]) * bgGetLevelScale()) - roompos->f[0];
    vertices[3].v.ob[1] = (((y - s1[1]) + s2[1]) * bgGetLevelScale()) - roompos->f[1];
    vertices[3].v.ob[2] = (((z - s1[2]) + s2[2]) * bgGetLevelScale()) - roompos->f[2];
    vertices[3].v.tc[0] = ((struct sImageTableEntry *) (((u8 *) thing->images) + (frame * 12)))->width << 5;
    vertices[3].v.tc[1] = ((struct sImageTableEntry *) (((u8 *) thing->images) + (frame * 12)))->height << 5;
    
    gSPSetGeometryMode(gdl++, G_CULL_BACK);
    gSPMatrix(gdl++, osVirtualToPhysical((void *) camGetProjViewMtx()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gdl = roomMtxLoad(gdl, room);
    texSelect(&gdl, (struct sImageTableEntry *) (((u8 *) thing->images) + (frame * 12)), TEXBLEND_CLOUD, zbufferMode, 2);
    gSPVertex(gdl++, osVirtualToPhysical(vertices), 4, 0);
    gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPMatrix(gdl++, osVirtualToPhysical(camGetProjectionMtx()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    *gdlptr = gdl;
}


void sparksRender(Gfx **gdlptr, s32 zbufferMode)
{

    s_bullet_spark *thing = &g_Sparks[0]; \
    s_bullet_spark *end = g_Sparks + BULLET_SPARKS_MAX;

    for (; (thing < end); thing++)
    {
        sparkRender(thing, gdlptr, zbufferMode);
    }
}


#ifndef VERSION_EU
void movingSparksReset(void)
{
    s_moving_bullet_spark *ptr;

    ptr = g_MovingSparks;

    for (ptr = &g_MovingSparks[0]; ptr < &g_MovingSparks[BULLET_MOVING_SPARKS_MAX]; ptr++)
    {
        ptr->spark.lifetime = 0;
    }
}
#endif


#ifndef VERSION_EU
s_moving_bullet_spark *movingSparkCreate(coord3d *pos, coord3d *velocity, s32 type, f32 size, f32 minY, s16 room)
{
    s_moving_bullet_spark *ptr;

    for (ptr = &g_MovingSparks[0]; ptr < &g_MovingSparks[BULLET_MOVING_SPARKS_MAX]; ptr++)
    {
        if (ptr->spark.lifetime == 0)
        {
            sparkInit(&ptr->spark, pos, type, size, room);

            ptr->velocity.x = velocity->x;
            ptr->velocity.y = velocity->y;
            ptr->velocity.z = velocity->z;
            ptr->minY = minY;

            return ptr;
        }
    }

    return NULL;
}
#endif


#ifndef VERSION_EU
void movingSparksTick(void)
{
    s_moving_bullet_spark *ptr;
    s_moving_bullet_spark *end;

    ptr = &g_MovingSparks[0]; end = &g_MovingSparks[BULLET_MOVING_SPARKS_MAX];

    while (ptr < end)
    {
        if (ptr->spark.lifetime > 0)
        {
            ptr->spark.age += g_ClockTimer;
            if (ptr->spark.age >= 0)
            {
                if (ptr->spark.lifetime > ptr->spark.age)
                {
                    physMoveWithGravity(&ptr->spark.pos, &ptr->velocity, g_GlobalTimerDelta);
                    if (ptr->spark.pos.y < ptr->minY)
                    {
                        ptr->spark.lifetime = 0;
                    }
                }
                else
                {
                    ptr->spark.lifetime = 0;
                }
            }
        }
        ptr++;
    }
}
#endif


#ifndef VERSION_EU

void movingSparksRender(Gfx **gdlptr, s32 zbufferMode)
{
    s32 max_index;
    s_moving_bullet_spark *ptr;

    max_index = BULLET_MOVING_SPARKS_MAX;

    for (ptr = &g_MovingSparks[0]; ptr < (&g_MovingSparks[max_index]); ptr++)
    {
        sparkRender(&ptr->spark, gdlptr, zbufferMode);
    }

}


void sparksResetAll(void)
{
    sparksReset();
    movingSparksReset();
}


void sparksTickAll(void)
{
    sparksTick();

    // responsible for updating bullet sparks and dust clouds that spawn when shooting at other players
    // these are 2D and always facing the camera
    movingSparksTick();
}


void sparksRenderAll(Gfx **gdlptr, s32 zbufferMode)
{
    sparksRender(gdlptr, zbufferMode);
    movingSparksRender(gdlptr, zbufferMode);
}
#else
// PAL has no moving sparks.
void sparksResetAll(void)
{
    sparksReset();
}


void sparksTickAll(void)
{
    sparksTick();
}


void sparksRenderAll(Gfx **gdlptr, s32 zbufferMode)
{
    sparksRender(gdlptr, zbufferMode);
}
#endif
