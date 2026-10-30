#ifndef _SMOKE_H_
#define _SMOKE_H_
#include <ultra64.h>
#include <bondtypes.h>

#define SMOKE_BUFFER_LEN 20
#define SMOKE_PARTS_LEN 10

/* Index into the smoke types (smoke.c). 1 to 6 are left by explosions, by size. */
typedef enum SMOKE_TYPE
{
    SMOKE_TYPE_NONE,
    SMOKE_TYPE_PUFF = 7,       // a bullet puff's
    SMOKE_TYPE_ROCKET,         // a rocket's trail
    SMOKE_TYPE_GRENADEROUND,   // a launched grenade's trail
    SMOKE_TYPE_HIGHTRAIL,      // the trail of an object high in the sky
    SMOKE_TYPE_COUNT
} SMOKE_TYPE;

typedef struct s_smoketype {
    s16 duration;              /* ticks it keeps making clouds */
    s16 rateappear;            /* ticks a cloud takes to fade in */
    s16 ratedissolve;          /* ticks between new clouds */
    s16 size;
    f32 bg_rotrate;            /* a cloud's spin speed (random up to half this, either way) */
    u8 r;
    u8 g;
    u8 b;
    f32 fg_rotrate;            /* how fast a cloud fades out */
    s16 propagated_clouds;     /* the clouds made in the last this many ticks start fainter */
} s_smoketype;

/* One cloud. size 0 is a free slot. */
struct SmokePart
{
    coord3d pos;
    f32 size;
    f32 rot;
    f32 deltarot;
    f32 offset1;               /* phases of its sideways wobble */
    f32 offset2;
    f32 alpha;
    s16 count;                 /* ticks since it appeared */
};

struct Smoke {
    PropRecord *prop;          /* NULL while the slot is free */
    s16 duration;              /* ticks so far */
    s16 smoke_type;            /* SMOKE_TYPE */
    struct SmokePart parts[SMOKE_PARTS_LEN];
};

void smokeCreate(coord3d *pos, StandTile *stan, s16 type, u8 *rooms, s32 inAir);
void smokesEndEarly(s32 count);
s32  smokeTick(PropRecord *prop);
u8   smokePropTick(PropRecord *prop);
Gfx *smokeRender(PropRecord *prop, Gfx *gdl, s32 withalpha);
void smokeInitScaleMtx(void);
void smokesInit(void);
void smokesReset(void);

#endif
