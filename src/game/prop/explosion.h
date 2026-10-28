#ifndef _explosions_H_
#define _explosions_H_
#include <ultra64.h>
#include <bondtypes.h>

// Not sure what this was originally, but it looks like there was a multiplier
// applied that compiler factors out.
#define EXPLOSION_DAMAGE_SCALER 1.0f

#define EXPLOSION_BUFFER_LEN 6
#define EXPLOSION_PARTS_LEN 40
#define SMOKE_BUFFER_LEN 20
#define SMOKE_PARTS_LEN 10
#define SCORCH_BUFFER_LEN 20
#define IMPACT_TYPE_LEN 20
#define BULLET_IMPACT_BUFFER_LEN 100
#define MAX_FLYING_PARTICLES 200

typedef struct s_smoketype {
    s16 duration;
    s16 rateappear;
    s16 ratedissolve;
    s16 size;
    f32 bg_rotrate;
    u8 r;
    u8 g;
    u8 b;
    //u8 a;
    f32 fg_rotrate;
    s16 propagated_clouds;
    //u16 field_0x16;
} s_smoketype;

typedef struct s_explosiontype {
    f32 hrange;
    f32 vrange;
    f32 hchange;
    f32 vchange;
    f32 explosion_size;
    f32 explosion_range;
    f32 dmg_range;
    s16 duration;
    s16 propagationrate;
    f32 flareanimspeed;
    s16 numshrapnelbits;
    //u16 field_0x26;
    f32 shrapnel_size;
    f32 shrapnel_scatter_dist;
    f32 shrapnel_hvel;
    f32 shrapnel_vvel;
    u8 smoketype;
    u8 sndID;
    //u16 field_0x3a;
    f32 damage;
} s_explosiontype;

typedef struct s_impacttype {
    f32 width;
    f32 height;
    u8 apptype;
    u8 renderMode; // texSetRenderMode blend mode
    u8 zbufferMode;
} s_impacttype;

struct ExplosionPart
{
    coord3d pos;
    f32 size;
    f32 rot;
    s16 frame;
    u8 bb;
};

struct Explosion {
    PropRecord *prop; // maybe explosion prop
    PropRecord *source; // maybe source prop (e.g., crate)
    struct ExplosionPart parts[EXPLOSION_PARTS_LEN];
    s16 age; // some kind of current tick + total elapsed?
    s16 nextDamageAge; // damage is dealt every duration/4 frames

    s8 explosion_type;
    s8 makeScorch;
    s8 player;
    s8 unk3CF;

    struct coord3d pos;
    s16 room;
    s16 unk3DE;
};

struct SmokePart
{
    coord3d pos;         /*0x00*/
    f32 size;                   /*0x0c*/
    f32 rot;                    /*0x10*/
    f32 deltarot;               /*0x14*/
    f32 offset1;                /*0x18*/
    f32 offset2;                /*0x1c*/
    f32 alpha;                  /*0x20*/
    s16 count;                  /*0x24*/
};

struct Smoke {
    PropRecord *prop;
    s16 duration;
    s16 smoke_type;
    struct SmokePart parts[SMOKE_PARTS_LEN]; /*0x008*/
};

struct Scorch {
    s16 roomid;
    u8 unk02;
    s8 unk03;

    struct coord3d pos;
    f32 explosion_size;
    struct Model *model;

    Vtx vertex_list[4];
};

struct BulletImpact {
    s16 room;
    s16 impact_type;
    u32 unk04;
    Vtx vertex_list[4];//cannonically vlist
    PropRecord* prop;
    s8 model_render_pos_index;
    s8 room_clear_flag;
    s16 unk4E;
};

struct FlyingParticles {
    s32 age; // 0 = free

    struct coord3d position; // 0x04
    struct coord3d rotation; // 0x10
    struct coord3d position_drift; // 0x1c
    struct coord3d rotation_drift; // 0x28

    s32 unk34;

    Vtx vertex_list[4];
};

extern f32 g_SpExplosionDamageMult;
extern Mtx g_ExplosionScaleMtx;
extern struct Smoke *g_SmokeBuffer;
extern struct Explosion *g_ExplosionBuffer;
extern s32 g_MaxParticles;
extern struct FlyingParticles *g_FlyingParticlesBuffer;
extern struct Scorch *g_ScorchBuffer;
extern struct BulletImpact *g_BulletImpactBuffer;
extern s32 g_ShakeTicks;
extern s32 g_ShakeBoostTicks;
extern s32 g_NextParticle;
extern s32 g_NextScorch;
extern s32 g_NextImpact;



s32 explosionSmokeTick(PropRecord *prop);
Gfx * explosionRenderFlyingParticles(Gfx *gdl);
Gfx * explosionCallRenderBulletImpactOnProp(Gfx *gdl);
Gfx *explosionRenderScorchBuffer(Gfx *gdl);
Gfx *explosionRenderPropExplosion(PropRecord *prop, Gfx *gdl, s32 withalpha);
Gfx *explosionRenderPropSmoke(PropRecord *prop, Gfx *gdl, s32 withalpha);


Gfx *explosionRenderBulletImpactOnProp(Gfx *gdl, PropRecord *prop, s32 pass);

void explosionCreateSmoke(coord3d *pos, StandTile *stan, s16 smoke_type, u8 *rooms, s32 flags);
void explosionUpdateFlyingParticles(void);
void explosionsInit(void);
void explosionsReset(void);
u8 smokePropTick(PropRecord* prop);
u8 explosionPropTick(PropRecord* prop);
void explosionScreenShake(coord3d* source_pos, coord3d* source_mag, coord3d* result);
void explosionCreateBulletImpact(struct coord3d *pos, struct coord3d *normal, s16 impact_type, s16 room, PropRecord *prop, s8 model_render_pos_index, s8 room_clear_flag);
s32 explosionTick(PropRecord* explosionprop);
s32 explosionCreate(PropRecord *source, struct coord3d *target_pos, StandTile *target_stan, s16 /* enum EXPLOSION_DEF */ explosion_type, s32 makeScorch, s32 player, u8 *rooms, s32 inAir);

void explosionInitScaleMtx(void);

void explosionStartShake(void);
void explosionClearBulletImpactRoom(PropRecord* prop);
void explosionClearBulletImpactRoomByFlag(PropRecord* prop, s8 flag);

#endif
