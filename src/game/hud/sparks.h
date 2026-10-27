#ifndef _SPARKS_H_
#define _SPARKS_H_
#include <ultra64.h>
#include <bondtypes.h>

typedef struct s_bullet_spark {
    s32 age;
    s16 lifetime;
    s16 room;
    f32 frameRate; // animation frames per tick
    void* images;
    coord3d pos;
    f32 sizeCos; // size * cos(angle): rotates the quad
    f32 sizeSin;
    f32 size;
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} s_bullet_spark;

#ifndef VERSION_EU
typedef struct s_moving_bullet_spark
{
    s_bullet_spark spark;
    coord3d velocity;
    f32 minY; // the spark dies when it falls below this
} s_moving_bullet_spark;
#endif

extern Vtx g_SparkVtxTemplate;
s_bullet_spark *sparkCreate(coord3d *pos, s32 type, f32 size, s16 room);
void sparkInit(s_bullet_spark *spark, coord3d *pos, s32 type, f32 size, s16 room);
void sparksRender(Gfx *gdl, s32 zbufferMode);
void sparksRenderAll(Gfx *gdl, s32 zbufferMode);
void sparksResetAll(void);
void sparksTick(void);
void sparksTickAll(void);
#ifdef VERSION_EU
void movingSparkCreate(void);
#endif

#endif
