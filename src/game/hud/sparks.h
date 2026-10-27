#ifndef _SPARKS_H_
#define _SPARKS_H_
#include <ultra64.h>
#include <bondtypes.h>

typedef struct s_bullet_spark {
    s32 age;
    s16 lifetime;
    s16 unk06;
    f32 unk08;
    void* unk0C;
    f32 unk10;
    f32 unk14;
    f32 unk18;
    f32 unk1c;
    f32 unk20;
    f32 unk24;
    u8 unk28;
    u8 unk29;
    u8 unk2A;
    u8 unk2B;
} s_bullet_spark;

#ifndef VERSION_EU
typedef struct s_moving_bullet_spark
{
    s_bullet_spark unk00;
    coord3d velocity;
    f32 unk38;
} s_moving_bullet_spark;
#endif

extern Vtx g_SparkVtxTemplate;
s_bullet_spark *sparkCreate(coord3d *arg0, s32 arg1, f32 arg2, s16 arg3);
void sparkInit(s_bullet_spark *arg0, coord3d *arg1, s32 arg2, f32 arg3, s16 arg4);
void sparksRender(Gfx *arg0, s32 zbufferMode);
void sparksRenderAll(Gfx *arg0, s32 zbufferMode);
void sparksResetAll(void);
void sparksTick(void);
void sparksTickAll(void);
#ifdef VERSION_EU
void movingSparkCreate(void);
#endif

#endif
