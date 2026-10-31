/**
 * Explosions. An explosion is a prop of its own (PROP_TYPE_EXPLOSION), taken from a
 * buffer of 6: a cloud of up to 40 fireballs that grows for the explosion type's
 * duration. Four times over its life it damages characters and objects in range
 * (explosionInflictDamage). As it ends it leaves smoke (smoke.c) and, for some, a scorch
 * mark on the floor (decal.c), and it throws out debris (debris.c) when it starts.
 *
 * Explosions also shake the screen while any are going (explosionScreenShake).
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <gbi_extension.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "fr.h"
#include "music.h"
#include "random.h"
#include "snd.h"
#include "game/audio/sfxvolume.h"
#include "game/world/bg.h"
#include "game/world/roommtx.h"
#include "game/player/player.h"
#include "game/chr/chraction.h"
#include "game/gfx/dyn.h"
#include "game/gfx/imagebank.h"
#include "game/level/lv.h"
#include "game/nav/stan.h"
#include "game/prop/prop.h"
#include "game/prop/propobj.h"
#include "game/prop/explosion.h"
#include <assets/oddtextures.h>
#include "game/level/gamemode.h"

/* The mushroom explosion rises this long, then spreads out */
#define MUSHROOM_RISE_TICKS 32

/* Explosions damage things every quarter of their duration */
#define EXPLOSION_DAMAGE_PERIODS 4

/* A fireball's animation has this many frames */
#define EXPLOSION_FRAMES 15

/* Fireballs are drawn this far towards the eye (at most), so they aren't cut by walls */
#define EXPLOSION_PULLBACK 100.0f

f32 g_SpExplosionDamageMult = 1.0f;

static struct Explosion *s_Explosions;
static s32 s_ShakeTicks = 0;
static s32 s_ShakeBoostTicks = 0;

/* The explosion types (EXPLOSION_DEF). EU runs at 50 Hz: shorter durations and faster growth. */
#if defined(VERSION_EU)
static s_explosiontype g_ExplosionTypes[EXPLOSION_DEF_COUNT] = {
   //hrange, vrange,    hchg,               vchg,           expsize, exprang, dmgrang,   dur, proprate, flarespd, nbits,  bitsize, bitdist, bithvel, bitvvel, smoketype,             sndid, damage
    {  0.1f,   0.1f,    0.0f,               0.0f,                   0.1f,    0.0f,    0.0f,     1,        1,     1.0f,     0,     0.1f,    0.0f,    0.0f,    0.0f,         0,              0x00,   0.0f},
    {  1.0f,   1.0f,    0.0f,               0.0f,                   1.0f,    0.0f,    0.0f,    25,        1,     1.0f,    10,     5.0f,    0.0f,    2.0f,    6.0f,         7,              0x00,   0.0f},
    { 20.0f,  20.0f,    0.0f,               0.0f,                  30.0f,   50.0f,   50.0f,    67,        1,     3.0f,    40,     6.0f,    5.0f,    0.7f,    6.0f,         2,  EXPLOSION_1B_SFX, 0.125f},
    { 50.0f,  50.0f,    0.0f,               0.0f,                  50.0f,  100.0f,  100.0f,    75,        1,     4.0f,    50,     6.0f,   10.0f,    1.0f,    6.0f,         2,  EXPLOSION_1C_SFX,   0.5f},
    { 60.0f,  80.0f,    1.20000004768f,     0.360000014305f,      100.0f,  150.0f,  280.0f,   100,        2,     5.0f,    80,     8.0f,   30.0f,    2.0f,    6.0f,         1,  EXPLOSION_4A_SFX,   1.0f},
    { 60.0f, 120.0f,    1.20000004768f,     0.360000014305f,      150.0f,  200.0f,  310.0f,   100,        2,     5.0f,    80,     8.0f,   30.0f,    2.0f,    6.0f,         1,  EXPLOSION_4A_SFX,   2.0f},
    { 20.0f,  20.0f,    0.0f,               0.0f,                  22.0f,   40.0f,   40.0f,    67,        1,     3.0f,    40,     6.0f,    5.0f,    0.7f,    6.0f,         2,  EXPLOSION_1B_SFX,   0.5f},
    { 35.0f,  40.0f,    0.0f,               0.0f,                  35.0f,   70.0f,   70.0f,    75,        1,     4.0f,    50,     6.0f,   10.0f,    1.0f,    6.0f,         2,  EXPLOSION_1C_SFX,   1.0f},
    { 50.0f,  80.0f,    1.20000004768f,     0.360000014305f,       50.0f,  100.0f,  220.0f,   100,        2,     5.0f,    80,     8.0f,   30.0f,    2.0f,    6.0f,         1,  EXPLOSION_4A_SFX,   2.0f},
    { 60.0f, 120.0f,    1.20000004768f,     0.360000014305f,       50.0f,  130.0f,  230.0f,   100,        2,     5.0f,    80,     8.0f,   30.0f,    2.0f,    6.0f,         1,  EXPLOSION_4A_SFX,   2.0f},
    { 40.0f,  40.0f,    0.5f,               0.239999994635582f,    70.0f,  100.0f,  180.0f,   162,        4,     5.0f,   120,     6.0f,   30.0f,    2.5f,    6.0f,         4,  EXPLOSION_5A_SFX,   1.0f},
    { 50.0f,  50.0f,    0.699999988079071f, 0.5f,                 100.0f,  150.0f,  260.0f,   150,        1,     4.0f,   150,     6.0f,   30.0f,    3.0f,    6.0f,         4,  EXPLOSION_4A_SFX,   2.0f},
    { 70.0f,  60.0f,    1.20000004768372f,  0.699999988079071f,   150.0f,  225.0f,  320.0f,   150,        2,     5.0f,   150,     6.0f,   30.0f,    4.0f,   12.0f,         5,  EXPLOSION_4A_SFX,   4.0f},
    /* standard explosion for grenades and mines */
    { 80.0f,  60.0f,    2.40000009536743f,  0.899999976158142f,   200.0f,  300.0f,  480.0f,   150,        2,     5.0f,   200,     6.0f,   30.0f,    6.0f,   15.0f,         6,  EXPLOSION_4B_SFX,   4.0f},
    { 50.0f,  50.0f,    0.0f,               0.0f,                 120.0f,  200.0f,  400.0f,   125,        4,     4.0f,   150,     6.0f,   30.0f,    3.0f,    6.0f,         4,  EXPLOSION_4B_SFX,   4.0f},
    {  1.0f,   1.0f,    0.0f,               0.0f,                   1.0f,    0.0f,    0.0f,     1,        1,     1.0f,   150,     6.0f,   30.0f,    2.5f,    6.0f,         7,  EXPLOSION_2B_SFX,   0.0f},
    {  1.0f,   1.0f,    0.0f,               0.0f,                   1.0f,    0.0f,    0.0f,     1,        1,     1.0f,   100,     6.0f,   30.0f,    2.5f,    6.0f,         7,  EXPLOSION_2B_SFX,   0.0f},
    { 80.0f,  60.0f,   18.0f,               6.0f,                1500.0f, 2200.0f, 3600.0f,   250,        1,     2.0f,     0,     0.0f,    0.0f,    0.0f,    0.0f,         0,  EXPLOSION_4B_SFX,   4.0f},
    { 80.0f,  60.0f,    3.59999990463257f,  1.20000004768372f,    300.0f,  450.0f,  640.0f,    50,        1,     2.0f,     0,     0.0f,    0.0f,    0.0f,    0.0f,         0,  EXPLOSION_4B_SFX,   4.0f},
    /* facility remote mine */
    { 90.0f,  75.0f,    3.0f,               1.0f,                 250.0f,  375.0f,  600.0f,   150,        2,     5.0f,   200,     6.0f,   30.0f,    6.0f,   15.0f,         6,  EXPLOSION_4B_SFX,   4.0f},
    {160.0f, 120.0f,    7.19999980926514f,  2.40000009536743f,    600.0f,  450.0f,  640.0f,    50,        1,     2.0f,     0,     0.0f,    0.0f,    0.0f,    0.0f,         0,  EXPLOSION_4B_SFX,   4.0f},
};
#else
static s_explosiontype g_ExplosionTypes[EXPLOSION_DEF_COUNT] = {
   //hrange, vrange,    hchg,  vchg,  expsize, exprang, dmgrang,   dur, proprate, flarespd, nbits,  bitsize, bitdist, bithvel, bitvvel, smoketype,             sndid, damage
    {  0.1f,   0.1f,    0.0f,  0.0f,     0.1f,    0.0f,    0.0f,     1,        1,     1.0f,     0,     0.1f,    0.0f,    0.0f,    0.0f,         0,       NOTHING_SFX,   0.0f},
    {  1.0f,   1.0f,    0.0f,  0.0f,     1.0f,    0.0f,    0.0f,    30,        1,     1.0f,    10,     5.0f,    0.0f,    2.0f,    6.0f,         7,       NOTHING_SFX,   0.0f},
    { 20.0f,  20.0f,    0.0f,  0.0f,    30.0f,   50.0f,   50.0f,    80,        1,     3.0f,    40,     6.0f,    5.0f,    0.7f,    6.0f,         2,  EXPLOSION_1B_SFX, 0.125f},
    { 50.0f,  50.0f,    0.0f,  0.0f,    50.0f,  100.0f,  100.0f,    90,        1,     4.0f,    50,     6.0f,   10.0f,    1.0f,    6.0f,         2,  EXPLOSION_1C_SFX,   0.5f},
    { 60.0f,  80.0f,    1.0f,  0.3f,   100.0f,  150.0f,  280.0f,   120,        2,     5.0f,    80,     8.0f,   30.0f,    2.0f,    6.0f,         1,  EXPLOSION_4A_SFX,   1.0f},
    { 60.0f, 120.0f,    1.0f,  0.3f,   150.0f,  200.0f,  310.0f,   120,        2,     5.0f,    80,     8.0f,   30.0f,    2.0f,    6.0f,         1,  EXPLOSION_4A_SFX,   2.0f},
    { 20.0f,  20.0f,    0.0f,  0.0f,    22.0f,   40.0f,   40.0f,    80,        1,     3.0f,    40,     6.0f,    5.0f,    0.7f,    6.0f,         2,  EXPLOSION_1B_SFX,   0.5f},
    { 35.0f,  40.0f,    0.0f,  0.0f,    35.0f,   70.0f,   70.0f,    90,        1,     4.0f,    50,     6.0f,   10.0f,    1.0f,    6.0f,         2,  EXPLOSION_1C_SFX,   1.0f},
    { 50.0f,  80.0f,    1.0f,  0.3f,    50.0f,  100.0f,  220.0f,   120,        2,     5.0f,    80,     8.0f,   30.0f,    2.0f,    6.0f,         1,  EXPLOSION_4A_SFX,   2.0f},
    { 60.0f, 120.0f,    1.0f,  0.3f,    50.0f,  130.0f,  230.0f,   120,        2,     5.0f,    80,     8.0f,   30.0f,    2.0f,    6.0f,         1,  EXPLOSION_4A_SFX,   2.0f},
    { 40.0f,  40.0f,    0.4f,  0.2f,    70.0f,  100.0f,  180.0f,   170,        4,     5.0f,   120,     6.0f,   30.0f,    2.5f,    6.0f,         4,  EXPLOSION_5A_SFX,   1.0f},
    { 50.0f,  50.0f,    0.6f,  0.4f,   100.0f,  150.0f,  260.0f,   180,        1,     4.0f,   150,     6.0f,   30.0f,    3.0f,    6.0f,         4,  EXPLOSION_4A_SFX,   2.0f},
    { 70.0f,  60.0f,    1.0f,  0.6f,   150.0f,  225.0f,  320.0f,   180,        2,     5.0f,   150,     6.0f,   30.0f,    4.0f,   12.0f,         5,  EXPLOSION_4A_SFX,   4.0f},
    { 80.0f,  60.0f,    2.0f,  0.7f,   200.0f,  300.0f,  480.0f,   180,        2,     5.0f,   200,     6.0f,   30.0f,    6.0f,   15.0f,         6,  EXPLOSION_4B_SFX,   4.0f},
    { 50.0f,  50.0f,    0.0f,  0.0f,   120.0f,  200.0f,  400.0f,   150,        4,     4.0f,   150,     6.0f,   30.0f,    3.0f,    6.0f,         4,  EXPLOSION_4B_SFX,   4.0f},
    {  1.0f,   1.0f,    0.0f,  0.0f,     1.0f,    0.0f,    0.0f,     1,        1,     1.0f,   150,     6.0f,   30.0f,    2.5f,    6.0f,         7,  EXPLOSION_2B_SFX,   0.0f},
    {  1.0f,   1.0f,    0.0f,  0.0f,     1.0f,    0.0f,    0.0f,     1,        1,     1.0f,   100,     6.0f,   30.0f,    2.5f,    6.0f,         7,  EXPLOSION_2B_SFX,   0.0f},
    { 80.0f,  60.0f,   15.0f,  5.0f,  1500.0f, 2200.0f, 3600.0f,   300,        1,     2.0f,     0,     0.0f,    0.0f,    0.0f,    0.0f,         0,  EXPLOSION_4B_SFX,   4.0f},
    { 80.0f,  60.0f,    3.0f,  1.0f,   300.0f,  450.0f,  640.0f,    60,        1,     2.0f,     0,     0.0f,    0.0f,    0.0f,    0.0f,         0,  EXPLOSION_4B_SFX,   4.0f},
    { 90.0f,  75.0f,    2.5f, 0.87f,   250.0f,  375.0f,  600.0f,   180,        2,     5.0f,   200,     6.0f,   30.0f,    6.0f,   15.0f,         6,  EXPLOSION_4B_SFX,   4.0f},
    {160.0f, 120.0f,    6.0f,  2.0f,   600.0f,  450.0f,  640.0f,    60,        1,     2.0f,     0,     0.0f,    0.0f,    0.0f,    0.0f,         0,  EXPLOSION_4B_SFX,   4.0f},
};
#endif

/* A fireball's animation frames, one display list each (they set the texture) */
static Gfx *s_ExplosionFrameDLs[EXPLOSION_FRAMES] = {
    &globalDL_0x078, &globalDL_0x120, &globalDL_0x1c8, &globalDL_0x270, &globalDL_0x318,
    &globalDL_0x3c0, &globalDL_0x468, &globalDL_0x510, &globalDL_0x5b8, &globalDL_0x660,
    &globalDL_0x708, &globalDL_0x7b0, &globalDL_0x858, &globalDL_0x900, &globalDL_0x9a8,
};

static const Vtx s_FireballVertex = {{{0, 0, 0}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}};

/**
 * Starts a fireball at pos, a random size and angle.
 */
static void explosionPartStart(struct ExplosionPart *part, coord3d *pos, s_explosiontype *type)
{
    f32 size = ((RANDOMFRAC() * 0.5f) + 1.0f) * type->explosion_size;
    f32 angle = RANDOMFRAC() * M_TAU_F;

    part->size = cosf(angle) * size;
    part->rot = sinf(angle) * size;
    part->pos = *pos;
}

/**
 * Creates an explosion at pos. With makeScorch it leaves a scorch mark on the floor below
 * its source (or itself). An explosion in the air is drawn even where its floor tile's
 * room isn't. Returns whether there was a free slot for it.
 */
s32 explosionCreate(PropRecord *source, coord3d *pos, StandTile *stan, s16 type, s32 makeScorch, s32 player, u8 *rooms, s32 inAir)
{
    s_explosiontype *exptype = &g_ExplosionTypes[type];
    struct Explosion *exp = NULL;
    PropRecord *prop;
    s32 i;

    for (i = 0; i < EXPLOSION_BUFFER_LEN; i++)
    {
        if (s_Explosions[i].prop == NULL)
        {
            exp = &s_Explosions[i];
            break;
        }
    }

    if (exp == NULL)
    {
        return FALSE;
    }

    prop = propAllocate();

    if (type != EXPLOSION_DEF_DEBRIS && type != EXPLOSION_DEF_BULLETPUFF)
    {
        s_ShakeTicks = 6;
    }

    if ((s32)exptype->sndID > 0)
    {
        sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, exptype->sndID, NULL), pos);
    }

    if (prop == NULL)
    {
        return TRUE;
    }

    prop->type = PROP_TYPE_EXPLOSION;
    prop->flags |= PROPFLAG_ONSCREEN;
    prop->explosion = exp;
    prop->stan = stan;
    prop->pos = *pos;
    propSetRooms(prop, rooms);

    if (inAir)
    {
        prop->flags |= PROPFLAG_00000008;
    }

    propActivateThisFrame(prop);
    propEnable(prop);

    exp->explosion_type = type;
    exp->age = 0;
    exp->nextDamageAge = -1;
    exp->makeScorch = makeScorch;
    exp->prop = prop;
    exp->source = source;
    exp->player = player;

    if (makeScorch)
    {
        if (source != NULL && source->stan != NULL)
        {
            exp->pos.f[0] = source->pos.f[0];
            exp->pos.f[1] = stanGetFloorY(source->stan, source->pos.f[0], source->pos.f[2]) + 4.0f;
            exp->pos.f[2] = source->pos.f[2];
            exp->room = source->stan->room;
        }
        else
        {
            exp->pos.f[0] = pos->f[0];
            exp->pos.f[1] = stanGetFloorY(stan, pos->f[0], pos->f[2]) + 4.0f;
            exp->pos.f[2] = pos->f[2];
            exp->room = stan->room;
        }
    }
    else
    {
        exp->pos.f[0] = 999999.9f;
    }

    explosionPartStart(&exp->parts[0], pos, exptype);
    exp->parts[0].frame = 1;

    for (i = 0; i < exptype->numshrapnelbits; i++)
    {
        debrisCreate(pos, exptype->shrapnel_scatter_dist, exptype->shrapnel_hvel, exptype->shrapnel_vvel, exptype->shrapnel_size);
    }

    // in multiplayer, smoke stops making new clouds when another explosion starts
    if (getPlayerCount() >= 2)
    {
        smokesEndEarly(SMOKE_BUFFER_LEN);
    }

    return TRUE;
}

/**
 * Shakes the screen for a few ticks, as if an explosion went off nearby.
 */
void explosionStartShake(void)
{
    s_ShakeTicks = 6;
    s_ShakeBoostTicks = 6;
}

/**
 * The screen shake from explosions, for a player at eyepos looking along lookdir: the
 * offset to move the eye by (in result), and the screen's shake. The shake is stronger
 * the bigger and closer the explosions, and flips direction every 2 ticks.
 */
void explosionScreenShake(coord3d *eyepos, coord3d *lookdir, coord3d *result)
{
    PropRecord *prop;
    f32 angle;
    f32 xdir;
    f32 zdir;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dist;
    f32 shake;
    s32 i;

    if (s_ShakeTicks == 0)
    {
        viShake(0.0f);
        return;
    }

    // shake along the look direction turned by 0.8 radians
    angle = 0.8f;
    xdir = (cosf(angle) * lookdir->x) - (sinf(angle) * lookdir->f[2]);
    zdir = (sinf(angle) * lookdir->x) + (cosf(angle) * lookdir->f[2]);

    shake = 0.0f;

    for (i = 0; i < EXPLOSION_BUFFER_LEN; i++)
    {
        prop = s_Explosions[i].prop;

        if (prop != NULL)
        {
            dx = prop->pos.x - eyepos->x;
            dy = prop->pos.y - eyepos->y;
            dz = prop->pos.z - eyepos->z;
            dist = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

            if (dist == 0.0f)
            {
                dist = 0.0001f;
            }

            shake += (g_ExplosionTypes[s_Explosions[i].explosion_type].explosion_size / dist) * 15.0f;
        }
    }

    if (s_ShakeBoostTicks > 0)
    {
        s_ShakeBoostTicks--;
        shake++;
    }

    s_ShakeTicks--;

    if (s_ShakeTicks & 2)
    {
        result->y = shake;
        shake = -shake;
    }
    else
    {
        result->y = -shake;
    }

    result->x = shake * xdir;
    result->z = shake * zdir;

    viShake((f32)s_ShakeTicks * shake);
}

/**
 * How far into an explosion's damage box something is: 1 at the centre down to 0 at the
 * nearest face (the box is hrange across and vrange high, either side). FALSE if it's
 * outside.
 */
static bool explosionGetFalloff(f32 xdist, f32 ydist, f32 zdist, f32 hrange, f32 vrange, f32 *falloff)
{
    f32 xfrac;
    f32 yfrac;
    f32 zfrac;
    f32 frac;

    if (!(xdist <= hrange && -hrange <= xdist
          && ydist <= vrange && -vrange <= ydist
          && zdist <= hrange && -hrange <= zdist))
    {
        return FALSE;
    }

    xfrac = xdist / hrange;
    yfrac = ydist / vrange;
    zfrac = zdist / hrange;

    if (xfrac < 0.0f)
    {
        xfrac = -xfrac;
    }

    if (yfrac < 0.0f)
    {
        yfrac = -yfrac;
    }

    if (zfrac < 0.0f)
    {
        zfrac = -zfrac;
    }

    xfrac = 1.0f - xfrac;
    yfrac = 1.0f - yfrac;
    zfrac = 1.0f - zfrac;

    frac = xfrac;

    if (yfrac < frac)
    {
        frac = yfrac;
    }

    if (zfrac < frac)
    {
        frac = zfrac;
    }

    *falloff = frac;
    return TRUE;
}

/**
 * Damages the objects, characters and players in an explosion's box, if it's time to.
 * Objects take damage falling off straight with distance, characters and players with
 * its square. Explosions count as remote mine damage.
 */
static void explosionInflictDamage(PropRecord *expprop, f32 hrange, f32 vrange)
{
    struct Explosion *exp = expprop->explosion;
    s_explosiontype *type = &g_ExplosionTypes[exp->explosion_type];
    ObjectRecord *obj;
    PropRecord *prop;
    s16 *propnum;
    s32 rooms[8];
    s32 prevplayer;
    f32 xdist;
    f32 ydist;
    f32 zdist;
    f32 damage;
    f32 dist;

    if (exp->age < exp->nextDamageAge)
    {
        return;
    }

    propGetRoomIds(expprop, rooms);
    roomGetProps(rooms);

    for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
    {
        prop = &g_Props[*propnum];

        if (prop == exp->source || prop->timetoregen != 0)
        {
            continue;
        }

        if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_DOOR)
        {
            obj = prop->obj;
            xdist = obj->runtime_pos.f[0] - expprop->pos.f[0];
            ydist = obj->runtime_pos.f[1] - expprop->pos.f[1];
            zdist = obj->runtime_pos.f[2] - expprop->pos.f[2];

            if (explosionGetFalloff(xdist, ydist, zdist, hrange, vrange, &damage))
            {
                damage = damage * type->damage;

                if (!(obj->runtime_bitflags & RUNTIMEBITFLAG_WRECKED)
                    && !(obj->flags2 & (PROPFLAG2_EXPLOSIONPROOF | PROPFLAG2_LINKEDTOSAFE)))
                {
                    objDetonateRecursive(prop, ((RANDOMFRAC() * 0.5f) + 1.0f) * damage, &obj->runtime_pos, ITEM_REMOTEMINE, exp->player);
                }
            }
        }
        else if (prop->type == PROP_TYPE_CHR || prop->type == PROP_TYPE_VIEWER)
        {
            xdist = prop->pos.f[0] - expprop->pos.f[0];
            ydist = prop->pos.f[1] - expprop->pos.f[1];
            zdist = prop->pos.f[2] - expprop->pos.f[2];

            if (!explosionGetFalloff(xdist, ydist, zdist, hrange, vrange, &damage))
            {
                continue;
            }

            damage *= damage;
            damage = damage * type->damage;

            if (prop->type == PROP_TYPE_CHR)
            {
                chrExplosionDamage(prop->chr, &expprop->pos, damage, TRUE);
                continue;
            }

            // a player: knocked away from the explosion
            if (xdist != 0.0f || zdist != 0.0f)
            {
                dist = sqrtf((xdist * xdist) + (zdist * zdist));
                xdist *= 1.0f / dist;
                zdist *= 1.0f / dist;
            }

            prevplayer = playerGetCurrentNum();
            playerSetCurrent(playerGetIndexForProp(prop));

            if (!gameIsMulti())
            {
                damage *= g_SpExplosionDamageMult;
            }

            if (playerIsInTank() == 1)
            {
                damage *= 2.0f;
            }

            playerTakeDamage(damage, xdist, zdist, exp->player, 1);
            playerSetCurrent(prevplayer);
        }
    }

    exp->nextDamageAge = exp->age + (type->duration / EXPLOSION_DAMAGE_PERIODS);
}

/**
 * Leaves the explosion's smoke: at its source, if it has one on the floor, else where it
 * is.
 */
static void explosionMakeSmoke(PropRecord *expprop, struct Explosion *exp, s_explosiontype *type)
{
    PropRecord *source = exp->source;
    bool inair = (expprop->flags & PROPFLAG_00000008) != 0;

    if (source != NULL && source->stan != NULL)
    {
        if (source->type == PROP_TYPE_OBJ)
        {
            smokeCreate(&source->obj->runtime_pos, source->stan, type->smoketype, source->rooms, inair);
        }
        else
        {
            smokeCreate(&source->pos, source->stan, type->smoketype, source->rooms, inair);
        }
    }
    else
    {
        smokeCreate(&expprop->pos, expprop->stan, type->smoketype, expprop->rooms, inair);
    }
}

/**
 * Ticks an explosion: adds fireballs and deals damage while it lasts, ages it, and leaves
 * smoke and the scorch mark. Frees it once its last fireballs have played.
 */
s32 explosionTick(PropRecord *expprop)
{
    struct Explosion *exp = expprop->explosion;
    s_explosiontype *type = &g_ExplosionTypes[exp->explosion_type];
    coord3d bbmin;
    coord3d bbmax;
    coord3d partpos;
    f32 lvupdate;
    f32 hrange;
    f32 vrange;
    s32 numparts;
    s32 i;
    s32 j;

    if (g_ClockTimer == 0)
    {
        return TICKOP_NONE;
    }

    lvupdate = (g_ClockTimer < 15) ? (f32)g_ClockTimer : 15.0f;

    if (exp->age >= 8 && exp->age < type->duration)
    {
        hrange = type->hrange + (type->hchange * exp->age);
        vrange = type->vrange + (type->vchange * exp->age);

        if (exp->explosion_type == EXPLOSION_DEF_MUSHROOM)
        {
            if (exp->age < MUSHROOM_RISE_TICKS)
            {
                expprop->pos.f[1] += 10.0f * lvupdate;
            }

            if (exp->age > MUSHROOM_RISE_TICKS)
            {
                hrange = (exp->age * 3.0f) + 40.0f;

                if (hrange > 300.0f)
                {
                    hrange = 300.0f;
                }

                vrange = 20.0f;
            }
        }

        // more fireballs each tick as it goes on
        numparts = (s32)(((f32)type->propagationrate * (f32)exp->age) / (f32)type->duration) + 1;

        for (i = 0; i < numparts; i++)
        {
            for (j = 0; j < EXPLOSION_PARTS_LEN; j++)
            {
                if (exp->parts[j].frame == 0)
                {
                    exp->parts[j].frame = 1;

                    partpos.f[0] = expprop->pos.f[0] + ((RANDOMFRAC() - 0.5f) * hrange);
                    partpos.f[1] = expprop->pos.f[1] + ((RANDOMFRAC() - 0.5f) * vrange);
                    partpos.f[2] = expprop->pos.f[2] + ((RANDOMFRAC() - 0.5f) * hrange);

                    explosionPartStart(&exp->parts[j], &partpos, type);
                    break;
                }
            }
        }

        // the rooms it's in: its area, plus the size of a fireball
        hrange = (hrange * 0.5f) + type->explosion_size * 1.5f;
        vrange = (vrange * 0.5f) + type->explosion_size * 1.5f;

        bbmin.f[0] = expprop->pos.f[0] - hrange;
        bbmin.f[1] = expprop->pos.f[1] - vrange;
        bbmin.f[2] = expprop->pos.f[2] - hrange;

        bbmax.f[0] = expprop->pos.f[0] + hrange;
        bbmax.f[1] = expprop->pos.f[1] + vrange;
        bbmax.f[2] = expprop->pos.f[2] + hrange;

        propCalcRooms(expprop, &bbmin, &bbmax, hrange);

        // the damage range grows over its life
        vrange = type->explosion_range + (((type->dmg_range - type->explosion_range) * (f32)exp->age) / (f32)type->duration);
        explosionInflictDamage(expprop, vrange, vrange);
    }

    for (i = 0; i < (s32)lvupdate; i++)
    {
        exp->age++;

        for (j = 0; j < EXPLOSION_PARTS_LEN; j++)
        {
            if (exp->parts[j].frame > 0)
            {
                exp->parts[j].frame++;
            }
        }

        // smoke 20 ticks before the end (the mushroom's at 15 ticks)
        if ((exp->age == 15 && exp->explosion_type == EXPLOSION_DEF_MUSHROOM)
            || (exp->age + 20 == (s16)type->duration && exp->explosion_type != EXPLOSION_DEF_MUSHROOM))
        {
            explosionMakeSmoke(expprop, exp, type);
        }

        if (exp->age == ((s16)type->duration >> 1) && exp->makeScorch)
        {
            scorchCreate(&exp->pos, type->explosion_size * 4.0f, exp->room);
        }
    }

    if (exp->age >= type->duration + (s32)(16.0f * type->flareanimspeed))
    {
        exp->prop = NULL;
        return TICKOP_FREE;
    }

    return TICKOP_NONE;
}

/**
 * Sets the explosion's depth for drawing order: pulled 100 units nearer (or halfway, when
 * close), as its fireballs are drawn nearer the eye.
 */
u8 explosionPropTick(PropRecord *prop)
{
    Mtxf *mtx = camGetWorldToScreenMtxf();

    prop->zDepth = -((((mtx->m[0][2] * prop->pos.x) + (mtx->m[1][2] * prop->pos.y)) + (mtx->m[2][2] * prop->pos.z)) + mtx->m[3][2]);

    if (prop->zDepth < 100.0f)
    {
        prop->zDepth *= 0.5f;
    }
    else
    {
        prop->zDepth -= 100.0f;
    }

    return TICKOP_NONE;
}

/**
 * Draws a fireball: a square facing the camera, pulled towards the eye by half its
 * distance (at most EXPLOSION_PULLBACK) and shrunk to match. roompos is the room the
 * vertices are relative to.
 */
static Gfx *explosionRenderPart(struct ExplosionPart *part, Gfx *gdl, coord3d *roompos)
{
    Mtxf *cammtx = camGetViewToWorldMtxf();
    coord3d *eyepos = playerGetPos();
    Vtx *vertices = dynAllocateVertices(4);
    coord3d rightcos;
    coord3d rightsin;
    coord3d upcos;
    coord3d upsin;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dist;
    f32 pullback;
    f32 frac;
    f32 cossize;
    f32 sinsize;
    f32 cx;
    f32 cy;
    f32 cz;
    s32 i;

    dx = part->pos.f[0] - eyepos->f[0];
    dy = part->pos.f[1] - eyepos->f[1];
    dz = part->pos.f[2] - eyepos->f[2];

    dist = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    pullback = dist * 0.5f;

    if (pullback > EXPLOSION_PULLBACK)
    {
        pullback = EXPLOSION_PULLBACK;
    }

    frac = (dist == 0) ? 0.0f : (dist - pullback) / dist;

    cossize = part->size * frac;
    sinsize = part->rot * frac;

    cx = eyepos->f[0] + (dx * frac);
    cy = eyepos->f[1] + (dy * frac);
    cz = eyepos->f[2] + (dz * frac);

    for (i = 0; i < 4; i++)
    {
        vertices[i] = s_FireballVertex;
    }

    // the camera's right and up axes, scaled by the corner vector
    for (i = 0; i < 3; i++)
    {
        rightcos.f[i] = cammtx->m[0][i] * cossize;
        rightsin.f[i] = cammtx->m[0][i] * sinsize;
        upcos.f[i] = cammtx->m[1][i] * cossize;
        upsin.f[i] = cammtx->m[1][i] * sinsize;
    }

    vertices[0].v.ob[0] = (cx - rightcos.f[0] - upsin.f[0]) * bgGetLevelScale() - roompos->f[0];
    vertices[0].v.ob[1] = (cy - rightcos.f[1] - upsin.f[1]) * bgGetLevelScale() - roompos->f[1];
    vertices[0].v.ob[2] = (cz - rightcos.f[2] - upsin.f[2]) * bgGetLevelScale() - roompos->f[2];
    vertices[0].v.tc[0] = 1760;
    vertices[0].v.tc[1] = 0;

    vertices[1].v.ob[0] = (cx + rightsin.f[0] - upcos.f[0]) * bgGetLevelScale() - roompos->f[0];
    vertices[1].v.ob[1] = (cy + rightsin.f[1] - upcos.f[1]) * bgGetLevelScale() - roompos->f[1];
    vertices[1].v.ob[2] = (cz + rightsin.f[2] - upcos.f[2]) * bgGetLevelScale() - roompos->f[2];
    vertices[1].v.tc[0] = 0;
    vertices[1].v.tc[1] = 0;

    vertices[2].v.ob[0] = (cx + rightcos.f[0] + upsin.f[0]) * bgGetLevelScale() - roompos->f[0];
    vertices[2].v.ob[1] = (cy + rightcos.f[1] + upsin.f[1]) * bgGetLevelScale() - roompos->f[1];
    vertices[2].v.ob[2] = (cz + rightcos.f[2] + upsin.f[2]) * bgGetLevelScale() - roompos->f[2];
    vertices[2].v.tc[0] = 0;
    vertices[2].v.tc[1] = 1760;

    vertices[3].v.ob[0] = (cx - rightsin.f[0] + upcos.f[0]) * bgGetLevelScale() - roompos->f[0];
    vertices[3].v.ob[1] = (cy - rightsin.f[1] + upcos.f[1]) * bgGetLevelScale() - roompos->f[1];
    vertices[3].v.ob[2] = (cz - rightsin.f[2] + upcos.f[2]) * bgGetLevelScale() - roompos->f[2];
    vertices[3].v.tc[0] = 1760;
    vertices[3].v.tc[1] = 1760;

    gSPVertex(gdl++, osVirtualToPhysical(vertices), 4, 0);
    gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);

    return gdl;
}

/**
 * Draws an explosion (in the translucent pass): its fireballs, grouped by animation
 * frame, last frame first. Fireballs that have finished their animation are freed.
 */
Gfx *explosionRender(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    struct Explosion *exp = prop->explosion;
    s_explosiontype *type = &g_ExplosionTypes[exp->explosion_type];
    s32 room = prop->rooms[0];
    coord3d *roompos = roomGetPos(room);
    bbox2d bbox;
    s32 maxframe;
    s32 frame;
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

    gSPSegment(gdl++, SPSEGMENT_GETITLE, osVirtualToPhysical(g_ImageBank));

    for (frame = EXPLOSION_FRAMES - 1; frame >= 0; frame--)
    {
        gSPDisplayList(gdl++, s_ExplosionFrameDLs[frame]);

        for (i = 0; i < EXPLOSION_PARTS_LEN; i++)
        {
            if (exp->parts[i].frame > 0 && frame == (s32)((f32)(exp->parts[i].frame - 1) / type->flareanimspeed))
            {
                gdl = explosionRenderPart(&exp->parts[i], gdl, roompos);
            }
        }
    }

    gSPMatrix(gdl++, osVirtualToPhysical(camGetProjectionMtx()), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    maxframe = (s32)(type->flareanimspeed * 15.0f);

    for (i = 0; i < EXPLOSION_PARTS_LEN; i++)
    {
        if (maxframe < exp->parts[i].frame)
        {
            exp->parts[i].frame = 0;
        }
    }

    return gdl;
}

/**
 * Allocates the level's explosions, smoke, scorch marks (solo only), bullet impacts and
 * debris, all empty.
 */
void explosionsInit(void)
{
    s32 i;
    s32 j;

    s_ShakeTicks = 0;
    s_ShakeBoostTicks = 0;
    g_SpExplosionDamageMult = 1.0f;

    s_Explosions = mempAlloc(EXPLOSION_BUFFER_LEN * sizeof(struct Explosion), MEMPOOL_STAGE);

    for (i = 0; i < EXPLOSION_BUFFER_LEN; i++)
    {
        s_Explosions[i].prop = NULL;

        for (j = 0; j < EXPLOSION_PARTS_LEN; j++)
        {
            s_Explosions[i].parts[j].frame = 0;
        }
    }

    smokesInit();
    decalsInit();
    debrisInit();
}

/**
 * Removes every explosion and smoke prop, and stops the screen shake.
 */
void explosionsReset(void)
{
    s32 i;

    s_ShakeTicks = 0;
    viShake(0.0);

    if (s_Explosions != NULL)
    {
        for (i = 0; i < EXPLOSION_BUFFER_LEN; i++)
        {
            if (s_Explosions[i].prop != NULL)
            {
                propDelist(s_Explosions[i].prop);
                propDisable(s_Explosions[i].prop);
                propFree(s_Explosions[i].prop);
                s_Explosions[i].prop = NULL;
            }
        }
    }

    smokesReset();
}
