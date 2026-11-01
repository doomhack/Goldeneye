#ifndef _EXPLOSION_H_
#define _EXPLOSION_H_
#include <ultra64.h>
#include <bondtypes.h>

#define EXPLOSION_BUFFER_LEN 6
#define EXPLOSION_PARTS_LEN 40

typedef struct s_explosiontype {
    f32 hrange;                /* the area fireballs appear in: width, and height */
    f32 vrange;
    f32 hchange;               /* how much the area grows each tick */
    f32 vchange;
    f32 explosion_size;        /* a fireball's size (it gets up to 1.5 times this) */
    f32 explosion_range;       /* the damage range at the start, growing to dmg_range at the end */
    f32 dmg_range;
    s16 duration;              /* ticks */
    s16 propagationrate;       /* fireballs added per tick, by the end */
    f32 flareanimspeed;        /* ticks per fireball animation frame */
    s16 numshrapnelbits;       /* debris thrown out */
    f32 shrapnel_size;
    f32 shrapnel_scatter_dist;
    f32 shrapnel_hvel;
    f32 shrapnel_vvel;
    u8 smoketype;              /* the smoke left behind (SMOKE_TYPE) */
    u8 sndID;
    f32 damage;
} s_explosiontype;

/* One fireball. frame counts up from 1 while it plays; 0 is a free slot. */
struct ExplosionPart
{
    coord3d pos;
    f32 size;                  /* size and rot are the fireball's corner vector: cos and sin of its angle times its size */
    f32 rot;
    s16 frame;
    u8 bb;
};

struct Explosion {
    PropRecord *prop;          /* NULL while the slot is free */
    PropRecord *source;        /* what blew up, if anything: it's not damaged, and the smoke comes from it */
    struct ExplosionPart parts[EXPLOSION_PARTS_LEN];
    s16 age;                   /* ticks */
    s16 nextDamageAge;         /* damage is dealt every duration/4 ticks */
    s8 explosion_type;         /* EXPLOSION_DEF */
    s8 makeScorch;
    s8 player;                 /* who set it off, for kill credit */
    coord3d pos;               /* where the scorch mark goes */
    s16 room;
};

extern f32 g_SpExplosionDamageMult;

s32  explosionCreate(PropRecord *source, coord3d *pos, StandTile *stan, s16 type, s32 makeScorch, s32 player, u8 *rooms, s32 inAir);
s32  explosionTick(PropRecord *prop);
u8   explosionPropTick(PropRecord *prop);
Gfx *explosionRender(PropRecord *prop, Gfx *gdl, s32 withalpha);
void explosionStartShake(void);
void explosionScreenShake(coord3d *eyepos, coord3d *lookdir, coord3d *result);
void explosionsInit(void);
void explosionsReset(void);

/* What explosions leave behind */
#include "game/prop/smoke.h"
#include "game/prop/debris.h"
#include "game/prop/decal.h"

#endif
