#ifndef _UNK_0A1DA0_H_
#define _UNK_0A1DA0_H_
#include <ultra64.h>
#include <bondtypes.h>

typedef struct s_shattered_window_piece {
    s32 active;
    coord3d pos;        /* 0x04 */
    coord3d rot;        /* 0x10 */
    coord3d velocity;   /* 0x1c */
    coord3d angvel;     /* 0x28 */
    u32 field_0x34;

    /**
     * 3 inline 16-byte vertices start here 
     */
    s16 v1x;
    s16 v1y;
    s16 v1z;
    s16 v1flag;
    s16 v1s;
    s16 v1t;
    u8  v1r;
    u8  v1g;
    u8  v1b;
    u8  v1a;

    s16 v2x;
    s16 v2y;
    s16 v2z;
    s16 v2flag;
    s16 v2s;
    s16 v2t;
    u8  v2r;
    u8  v2g;
    u8  v2b;
    u8  v2a;

    s16 v3x;
    s16 v3y;
    s16 v3z;
    s16 v3flag;
    s16 v3s;
    s16 v3t;
    u8  v3r;
    u8  v3g;
    u8  v3b;
    u8  v3a;
} s_shattered_window_piece;

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

extern s32 g_MaxShards;
extern s_shattered_window_piece* g_Shards;
extern s32 g_NextShardNum;

extern Vtx g_SparkVtxTemplate;
extern u32 D_80040990;
extern u32 watch_screen_index;
extern u32 controller_options_index;
extern u32 game_options_index;

void sub_GAME_7F0A47FC(void);
void glassInitShards(void);
void glassResetShards(void);
void glassTickShards(void);

Gfx * glassRenderShards(Gfx *arg0);
void watchMakeArcVerts(struct WatchVertex *vtx, s32 numverts, f32 scale, s32 arg3);
Gfx *watchRenderBackground(Gfx *gdl, struct WatchVertex *watch_verts, s32 unused_arg2, s32 drawFan);
Gfx *watchRenderQuad(Gfx *gdl, s32 *arg1);
Gfx *hudRenderGaugeBar(Gfx *gdl, uintptr_t vtxaddr, s32 numvertices);
void hudMakeDamageSegments(struct damage_display_val *arg0, s32 arg1, s32 arg2, f32 arg3);
struct WatchVertex *watchMakeRectVerts(struct WatchVertex *vtx, s32 startx, s32 startz, s32 width, s32 height, s32 horizontal_offset, s32 vertical_offset);
void glassCreateShard(coord3d * pos, f32 rotX, f32 shard_size);
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
