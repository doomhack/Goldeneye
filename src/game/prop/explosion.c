/**
 * Explosions and what they leave behind. An explosion and its smoke are props of their
 * own (PROP_TYPE_EXPLOSION, PROP_TYPE_SMOKE), taken from fixed buffers (6 explosions,
 * 20 smokes) and ticked and drawn like any other prop. explosionInflictDamage hurts
 * characters and objects in range.
 *
 * Also here: debris particles, scorch marks on the floor (solo only), bullet impacts
 * on walls and props (a ring buffer of 100), and the screen shake.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/os.h>
#include <PR/gbi.h>
#include <gbi_extension.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <limits.h>
#include "game/audio/sfxvolume.h"
#include "game/world/bg.h"
#include "game/player/bondview.h"
#include "game/menu/cheat.h"
#include "game/chr/chrai.h"
#include "game/chr/chraction.h"
#include "game/prop/propobj.h"
#include "game/gfx/dyn.h"
#include "game/prop/explosion.h"
#include "fr.h"
#include "game/gfx/imagebank.h"
#include "game/gfx/texcmds.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "music.h"
#include "game/player/player.h"
#include "random.h"
#include "snd.h"
#include "game/nav/stan.h"
#include "game/world/roommtx.h"
#include <assets/oddtextures.h>

// bss
// possibly   printf("Allocating %d bytes for glass data (%d bits)\n",DAT_83bd5fb0 * 0x88 + 0xf & 0xfffffff0,         DAT_83bd5fb0);
Mtx g_ExplosionScaleMtx;

/**
 * g_SmokeBuffer = mempAlloc(0x1FE0, MEMPOOL_STAGE);
 * printf("Allocating %d bytes for smoke data\n",0x9f60);
*/
struct Smoke *g_SmokeBuffer;

/**
 * g_ExplosionBuffer = mempAlloc(0x1740, MEMPOOL_STAGE);
 * printf("Allocating %d bytes for explosion data\n",0x2e80);
*/
struct Explosion *g_ExplosionBuffer;

s32 g_MaxParticles;
// printf("Allocating %d bytes for debris data (%d bits)\n", DAT_83bd2af0 * 0xa4, DAT_83bd2af0);
struct FlyingParticles *g_FlyingParticlesBuffer;

/**
 * g_ScorchBuffer = mempAlloc(0x6E0, MEMPOOL_STAGE);
 * printf("Allocating %d bytes for scorch data\n",0xa50);
 * sizeof each entry == 0x58
*/
struct Scorch *g_ScorchBuffer;

/**
 * g_BulletImpactBuffer = mempAlloc(0x1F40, MEMPOOL_STAGE);
 * printf("Allocating %d bytes for wallhit data\n",0x3070);
 * cannonically wallhitlist
*/
struct BulletImpact *g_BulletImpactBuffer;

// data
s32 g_ShakeTicks = 0;
s32 g_ShakeBoostTicks = 0;
f32 g_SpExplosionDamageMult = 1.0;

#if defined(VERSION_EU)
s_smoketype g_SmokeTypes[] = {
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
s_smoketype g_SmokeTypes[] = {
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

#if defined(VERSION_EU)
s_explosiontype g_ExplosionTypes[] = {
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
s_explosiontype g_ExplosionTypes[] = {
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

Gfx * g_ExplosionDisplayLists[] = {
    &globalDL_0x078,
    &globalDL_0x120,
    &globalDL_0x1c8,
    &globalDL_0x270,
    &globalDL_0x318,
    &globalDL_0x3c0,
    &globalDL_0x468,
    &globalDL_0x510,
    &globalDL_0x5b8,
    &globalDL_0x660,
    &globalDL_0x708,
    &globalDL_0x7b0,
    &globalDL_0x858,
    &globalDL_0x900,
    &globalDL_0x9a8
};

s32 g_NextParticle = 0;
s32 g_NextScorch = 0;
// canonically nextwallhit
s32 g_NextImpact = 0;

s_impacttype g_ImpactTypes[] = {
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
    {24.0f, 24.0f, 2, 2, 8},
    { 6.0f,  6.0f, 1, 2, 1},
    { 8.0f,  8.0f, 1, 2, 1},
    {12.0f, 12.0f, 1, 2, 1},
};

/* Unused. Kept after the explosion types in case anything reads past their end. */
u32 g_ExplosionTypesPad = 0;

Vtx g_ExplosionRenderPartDefaultVertex = {{{0, 0, 0}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}} };
Vtx g_SmokeRenderPartDefaultVertex = {{{0, 0, 0}, 0, {0, 0}, {0x0, 0x0, 0x0, 0x0}} };
Vtx g_ScorchDefaultVertex = {{{0, 0, 0}, 0, {0, 0}, {0x0, 0x0, 0x0, 0xDC}} };
Vtx g_BulletImpactDefaultVertex = {{{0, 0, 0}, 0, {0, 0}, {0x0, 0x0, 0x0, 0xDC}} };


// rodata

/*** prototypes */

void explosionInitFlyingParticles(coord3d *spawnpos, f32 spawn_rand_scale, f32 spawn_horiz_drift_scale, f32 spawn_vert_drift_scale, f32 spawn_tex_scale);
s32 explosionRoundFloat(f32 value);
void explosionSetBulletImpactAlpha(s32 index);
void explosionInflictDamage(PropRecord *explosionprop, f32 horiz_range, f32 vert_range);
void explosionInflictDamage(struct PropRecord *explosionprop, f32 horiz_range, f32 vert_range);
void explosionScorchTick(coord3d *pos, f32 explosion_size, s16 room);
Gfx *explosionRenderPart(struct ExplosionPart *part, Gfx *gdl, coord3d *coord);

/*** *************************************************************************************************************/

/**
 * Named same as Perfect Dark.
 * NTSC 0x7F09C250
*/
s32
explosionCreate(PropRecord *source, coord3d *target_pos, StandTile *target_stan, s16 explosion_type, s32 makeScorch, s32 player, u8 *rooms, s32 inAir)
{
    s_explosiontype *type;
    struct Explosion *exp;
    f32 size;
    f32 angle;
    s32 i;
    PropRecord *prop;

    type = &g_ExplosionTypes[explosion_type];
    exp = NULL;


    for (i = 0; i < EXPLOSION_BUFFER_LEN; i++)
    {
        if (g_ExplosionBuffer[i].prop == NULL)
        {
            exp = &g_ExplosionBuffer[i];
            break;
        }
    }

    if (exp != NULL)
    {
        prop = propAllocate();

        if ((explosion_type != 0x10) && (explosion_type != 1))
        {
            g_ShakeTicks = 6;
        }

        if ((s32) type->sndID > 0)
        {
            sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, (s16) type->sndID, NULL), target_pos);
        }

        if (prop != NULL)
        {
            prop->type = PROP_TYPE_EXPLOSION;
            prop->flags |= PROPFLAG_ONSCREEN;
            prop->explosion = exp;
            i = 0;
            prop->stan = target_stan;
            prop->pos.f[0] = target_pos->f[0];
            prop->pos.f[1] = target_pos->f[1];
            prop->pos.f[2] = target_pos->f[2];

            while (rooms[i] != 0xff && i < 7)
            {
                prop->rooms[i] = rooms[i];
                i++;
            }

            prop->rooms[i] = 0xFF;

            if (inAir != 0)
            {
                prop->flags |= PROPFLAG_00000008;
            }

            propActivateThisFrame(prop);
            propEnable(prop);

            exp->explosion_type = explosion_type;
            exp->age = 0;
            exp->nextDamageAge = -1;
            exp->makeScorch = (u8) makeScorch;
            exp->prop = prop;
            exp->source = source;
            exp->player = (s8) player;

            if (makeScorch != 0)
            {
                if ((source != NULL) && (source->stan != NULL))
                {
                    exp->pos.f[0] = source->pos.f[0];
                    exp->pos.f[1] = stanGetFloorY(source->stan, source->pos.f[0], source->pos.f[2]) + 4.0f;
                    exp->pos.f[2] = source->pos.f[2];
                    exp->room = source->stan->room;
                }
                else
                {
                    exp->pos.f[0] = target_pos->f[0];
                    exp->pos.f[1] = stanGetFloorY(target_stan, target_pos->f[0], target_pos->f[2]) + 4.0f;
                    exp->pos.f[2] = target_pos->f[2];
                    exp->room = target_stan->room;
                }
            }
            else
            {
                exp->pos.f[0] = 999999.9f;
            }

            size = ((RANDOMFRAC() * 0.5f) + 1.0f) * type->explosion_size;
            angle = RANDOMFRAC() * M_TAU_F;

            exp->parts[0].size = cosf(angle) * size;
            exp->parts[0].rot = sinf(angle) * size;
            exp->parts[0].frame = 1;
            exp->parts[0].pos.f[0] = target_pos->f[0];
            exp->parts[0].pos.f[1] = target_pos->f[1];
            exp->parts[0].pos.f[2] = target_pos->f[2];

            for (i = 0; i < type->numshrapnelbits; i++)
            {
                explosionInitFlyingParticles(target_pos, type->shrapnel_scatter_dist, type->shrapnel_hvel, type->shrapnel_vvel, type->shrapnel_size);
            }

            if (getPlayerCount() >= 2)
            {
                for (i = 0; i < SMOKE_BUFFER_LEN; i++)
                {
                    if (g_SmokeBuffer[i].prop != NULL)
                    {
                        struct Smoke *smoke = &g_SmokeBuffer[i];

                        if (smoke->smoke_type != 7 && smoke->smoke_type != 8 && smoke->smoke_type != 9)
                        {
                            smoke->duration = g_SmokeTypes[smoke->smoke_type].duration;
                        }
                    }
                }
            }
        }
    }

    return exp != 0;
}


void explosionStartShake(void) {
        g_ShakeTicks = 6;
        g_ShakeBoostTicks = 6;
}


void explosionScreenShake(coord3d* source_pos, coord3d* source_mag, coord3d* result)
{
    PropRecord* explosion_prop;
    f32 angle;
    f32 diff_x;
    f32 diff_z;
    f32 mag_scalar_x;
    f32 mag_scalar_z;
    f32 diff_y;
    f32 explosion_mag;

    s32 i;
    f32 dist;
    f32 dist2;

    if (g_ShakeTicks == 0)
    {
        viShake(0.0f);
        return;
    }

    angle = 0.8f;
    mag_scalar_x = (cosf(angle) * source_mag->x) - (sinf(angle) * source_mag->f[2]);
    mag_scalar_z = (sinf(angle) * source_mag->x) + (cosf(angle) * source_mag->f[2]);

    explosion_mag = 0.0f;

    for (i = 0; i < EXPLOSION_BUFFER_LEN; i++)
    {
        explosion_prop = g_ExplosionBuffer[i].prop;
        if (explosion_prop != NULL)
        {
            diff_x = explosion_prop->pos.x - source_pos->x;
            diff_y = explosion_prop->pos.y - source_pos->y;
            diff_z = explosion_prop->pos.z - source_pos->z;
            dist = sqrtf((diff_x * diff_x) + (diff_y * diff_y) + (diff_z * diff_z));
            if (dist == 0.0f) { dist = 0.0001f; }
            dist2 = g_ExplosionTypes[g_ExplosionBuffer[i].explosion_type].explosion_size / dist;
            explosion_mag += dist2 * 15.0f;
        }
    }

    if (g_ShakeBoostTicks > 0)
    {
        g_ShakeBoostTicks--;
        explosion_mag++;
    }

    g_ShakeTicks--;
    if (g_ShakeTicks & 2)
    {
        result->y = explosion_mag;
        explosion_mag = -explosion_mag;
    }
    else
    {
        result->y = -explosion_mag;
    }

    result->x = explosion_mag * mag_scalar_x;
    result->z = explosion_mag * mag_scalar_z;

    viShake((f32) g_ShakeTicks * explosion_mag);
}

/***
 * see Perfect Dark void explosionInflictDamage(struct prop *expprop)
 * Address 0x7F09C9D8 (NTSC)
*/
void explosionInflictDamage(PropRecord *explosionprop, f32 horiz_range, f32 vert_range)
{
    s32 rooms[8];
    PropRecord *prop;
    s16 *propnum;
    s_explosiontype *type;
    struct Explosion *exp;

    exp = explosionprop->explosion;
    type = &g_ExplosionTypes[exp->explosion_type];

    if (exp->age >= exp->nextDamageAge)
    {
        propGetRoomIds(explosionprop, &rooms[0]);
        roomGetProps(&rooms[0]);

        for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
        {
            prop = &g_Props[*propnum];

            if ((prop != exp->source) && (prop->timetoregen == 0))
            {
                if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_DOOR)
                {
                    struct ObjectRecord *obj;
                    f32 xdist;
                    f32 ydist;
                    f32 zdist;

                    obj = prop->obj;
                    xdist = obj->runtime_pos.f[0] - explosionprop->pos.f[0];
                    ydist = obj->runtime_pos.f[1] - explosionprop->pos.f[1];
                    zdist = obj->runtime_pos.f[2] - explosionprop->pos.f[2];

                    if ((xdist <= horiz_range)
                        && (-horiz_range <= xdist)
                        && (ydist <= vert_range)
                        && (-vert_range <= ydist)
                        && (zdist <= horiz_range)
                        && (-horiz_range <= zdist))
                    {
                        f32 xfrac;
                        f32 yfrac;
                        f32 zfrac;
                        f32 minfrac;

                        xfrac = xdist / horiz_range;
                        yfrac = ydist / vert_range;
                        zfrac = zdist / horiz_range;

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

                        minfrac = xfrac;

                        if (yfrac < minfrac)
                        {
                            minfrac = yfrac;
                        }

                        if (zfrac < minfrac)
                        {
                            minfrac = zfrac;
                        }

                        minfrac = minfrac * EXPLOSION_DAMAGE_SCALER * type->damage;

                        if (!(obj->runtime_bitflags & 0x1000) && !(obj->flags2 & 0x200400))
                        {
                            objDetonateRecursive(
                                prop,
                                ((RANDOMFRAC() * 0.5f) + 1.0f) * minfrac,
                                &obj->runtime_pos,
                                0x1D,
                                (s32) exp->player);
                        }
                    }

                }
                else if (prop->type == PROP_TYPE_CHR || prop->type == PROP_TYPE_VIEWER)
                {
                    f32 xdist;
                    f32 ydist;
                    f32 zdist;

                    xdist = prop->pos.f[0] - explosionprop->pos.f[0];
                    ydist = prop->pos.f[1] - explosionprop->pos.f[1];
                    zdist = prop->pos.f[2] - explosionprop->pos.f[2];

                    if ((xdist <= horiz_range)
                        && (-horiz_range <= xdist)
                        && (ydist <= vert_range)
                        && (-vert_range <= ydist)
                        && (zdist <= horiz_range)
                        && (-horiz_range <= zdist))
                    {
                        f32 xfrac;
                        f32 yfrac;
                        f32 zfrac;
                        f32 minfrac;

                        xfrac = xdist / horiz_range;
                        yfrac = ydist / vert_range;
                        zfrac = zdist / horiz_range;

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

                        minfrac = xfrac;

                        if (yfrac < minfrac)
                        {
                            minfrac = yfrac;
                        }

                        if (zfrac < minfrac)
                        {
                            minfrac = zfrac;
                        }

                        minfrac *= minfrac;
                        minfrac = minfrac * EXPLOSION_DAMAGE_SCALER * type->damage;

                        if (prop->type == PROP_TYPE_CHR)
                        {
                            chrExplosionDamage(prop->chr, &explosionprop->pos, minfrac, 1);
                        }
                        else
                        {
                            s32 prevplayer;

                            if ((xdist != 0.0f) || (zdist != 0.0f))
                            {
                                f32 dist = sqrtf((xdist * xdist) + (zdist * zdist));
                                xdist *= 1.0f / dist;
                                zdist *= 1.0f / dist;
                            }

                            prevplayer = playerGetCurrentNum();
                            playerSetCurrent(playerGetIndexForProp(prop));

                            if (!isMpGame())
                            {
                                minfrac *= g_SpExplosionDamageMult;
                            }

                            if (playerIsInTank() == 1)
                            {
                                minfrac *= 2.0f;
                            }

                            playerTakeDamage(minfrac, xdist, zdist, (s32) exp->player, 1);
                            playerSetCurrent(prevplayer);
                        }
                    }
                }
            }
        }

        exp->nextDamageAge = exp->age + (type->duration >> 2);
    }
}





/***
 * see Perfect Dark u32 explosionTick(struct prop *prop)
 *
*/
s32 explosionTick(PropRecord* explosionprop)
{
    s32 i;
    s32 j;
    s32 k;

    f32 hrange;
    f32 vrange;
    f32 size;
    f32 angle;

    struct Explosion *exp;
    s_explosiontype *explosiontype;

    f32 lvupdate;
    s32 numparts;
    coord3d bbmin;
    coord3d bbmax;


    exp = explosionprop->explosion;
    explosiontype = &g_ExplosionTypes[exp->explosion_type];

    if (g_ClockTimer == 0)
    {
        return TICKOP_NONE;
    }

    lvupdate = (g_ClockTimer < 15) ? (f32) g_ClockTimer : 15.0f;

    if ((exp->age >= 8) && (exp->age < explosiontype->duration))
    {
        hrange = explosiontype->hrange + (explosiontype->hchange * exp->age);
        vrange = explosiontype->vrange + (explosiontype->vchange * exp->age);

        if (exp->explosion_type == 0xE)
        {
            if (exp->age < 0x20)
            {
                explosionprop->pos.f[1] += 10.0f * lvupdate;
            }

            if (exp->age >= 0x21)
            {
                hrange = (exp->age * 3.0f) + 40.0f;

                if (hrange > 300.0f)
                {
                    hrange = 300.0f;
                }

                vrange = 20.0f;
            }
        }

        numparts = (s32) (((f32)explosiontype->propagationrate * (f32)exp->age) / (f32)explosiontype->duration) + 1;
        for (i = 0; i < numparts; i++)
        {
            for (j=0; j<EXPLOSION_PARTS_LEN; j++)
            {
                if (exp->parts[j].frame == 0)
                {
                    exp->parts[j].frame = 1;

                    exp->parts[j].pos.f[0] = explosionprop->pos.f[0] + ((RANDOMFRAC() - 0.5f) * hrange);
                    exp->parts[j].pos.f[1] = explosionprop->pos.f[1] + ((RANDOMFRAC() - 0.5f) * vrange);
                    exp->parts[j].pos.f[2] = explosionprop->pos.f[2] + ((RANDOMFRAC() - 0.5f) * hrange);

                    size = ((RANDOMFRAC() * 0.5f) + 1.0f) * explosiontype->explosion_size;
                    angle = RANDOMFRAC() * M_TAU_F;

                    exp->parts[j].size = cosf(angle) * size;
                    exp->parts[j].rot = sinf(angle) * size;

                    break;
                }
            }
        }

        // see Perfect Dark void explosionGetBboxAtFrame(struct coord *lower, struct coord *upper, s32 frame, struct prop *prop)

        hrange = (hrange * 0.5f) + explosiontype->explosion_size * 1.5f;
        vrange = (vrange * 0.5f) + explosiontype->explosion_size * 1.5f;

        bbmin.f[0] = explosionprop->pos.f[0] - hrange;
        bbmin.f[1] = explosionprop->pos.f[1] - vrange;
        bbmin.f[2] = explosionprop->pos.f[2] - hrange;

        bbmax.f[0] = explosionprop->pos.f[0] + hrange;
        bbmax.f[1] = explosionprop->pos.f[1] + vrange;
        bbmax.f[2] = explosionprop->pos.f[2] + hrange;

        // end explosionGetBboxAtFrame.

        propCalcRooms(explosionprop, &bbmin, &bbmax, hrange);

        vrange = explosiontype->explosion_range + (((explosiontype->dmg_range - explosiontype->explosion_range) * (f32) exp->age) / (f32) explosiontype->duration);
        explosionInflictDamage(explosionprop, vrange, vrange);
    }

    for (k = 0; k < (s32) lvupdate; k++)
    {
        exp->age++;

        for (j=0; j<EXPLOSION_PARTS_LEN; j++)
        {
            if (exp->parts[j].frame > 0)
            {
                exp->parts[j].frame++;
            }
        }

        if (((exp->age == 0xF) && (exp->explosion_type == 0xE))
            || (((exp->age + 0x14) == (s16) explosiontype->duration) && (exp->explosion_type != 0xE)))
        {
            if ((exp->source != NULL) && (exp->source->stan != NULL))
            {
                if (exp->source->type == PROP_TYPE_OBJ)
                {
                    struct ObjectRecord *obj = exp->source->obj;
                    explosionCreateSmoke(&obj->runtime_pos, exp->source->stan, (s16) explosiontype->smoketype, exp->source->rooms, (explosionprop->flags & 8) != 0);
                }
                else
                {
                    explosionCreateSmoke(&exp->source->pos, exp->source->stan, (s16) explosiontype->smoketype, exp->source->rooms, (explosionprop->flags & 8) != 0);
                }
            }
            else
            {
                explosionCreateSmoke(&explosionprop->pos, explosionprop->stan, (s16) explosiontype->smoketype, explosionprop->rooms, (explosionprop->flags & 8) != 0);
            }
        }

        if ((exp->age == ((s16) explosiontype->duration >> 1)) && (exp->makeScorch != 0))
        {
            explosionScorchTick(&exp->pos, explosiontype->explosion_size * 4.0f, exp->room);
        }
    }

    if (exp->age >= explosiontype->duration + (s32) (16.0f * explosiontype->flareanimspeed))
    {
        exp->prop = NULL;

        return TICKOP_FREE;
    }

    return TICKOP_NONE;
}


u8 explosionPropTick(PropRecord* prop)
{
    Mtxf* player_matrix;

    player_matrix = camGetWorldToScreenMtxf();
    prop->zDepth = -((((player_matrix->m[0][2] * prop->pos.x) + (player_matrix->m[1][2] * prop->pos.y)) + (player_matrix->m[2][2] * prop->pos.z)) + player_matrix->m[3][2]);

    if (prop->zDepth < 100.0f)
    {
        prop->zDepth *= 0.5f;
    }
    else
    {
        prop->zDepth -= 100.0f;
    }

    if (g_ClockTimer == 0)
    {
        return TICKOP_NONE;
    }

    return TICKOP_NONE;
}


/***
 * Perfect Dark Gfx *explosionRender(struct prop *prop, Gfx *gdl, bool xlupass)
 *
*/
Gfx *explosionRenderPropExplosion(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    s32 room;
    struct Explosion *exp;
    coord3d *roompos;
    s32 frame;

    bbox2d bbox;

    s32 maxframe;

    s32 i;

    room = prop->rooms[0];
    exp = prop->explosion;
    roompos = roomGetPos((s32) room);

    if (withalpha == 0)
    {
        return gdl;
    }
    else
    {
        if (propGetRoomsBbox2d(prop, &bbox) > 0)
        {
            gdl = bgScissorCurrentPlayerViewF(gdl, bbox.min.f[0], bbox.min.f[1], bbox.max.f[0], bbox.max.f[1]);
        }
        else
        {
            gdl = bgScissorCurrentPlayerViewDefault(gdl);
        }

        gSPClearGeometryMode(gdl++, G_CULL_BOTH | G_FOG);
        gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjViewMtx()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));

        gdl = roomMtxLoad(gdl, room);

        gSPSegment(gdl++, SPSEGMENT_GETITLE, osVirtualToPhysical(g_ImageBank));

        for (frame = 14;
            frame >= 0;
            frame--)
        {
            gSPDisplayList(gdl++, g_ExplosionDisplayLists[frame]);

            for (i = 0; i < EXPLOSION_PARTS_LEN; i++)
            {
                if (exp->parts[i].frame > 0
                    && frame == (s32)( (f32)(exp->parts[i].frame - 1) / g_ExplosionTypes[exp->explosion_type].flareanimspeed ) )
                {
                    gdl = explosionRenderPart(&exp->parts[i], gdl, roompos);
                }
            }
        }

        gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjectionMtx()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));

        maxframe = (s32) (g_ExplosionTypes[exp->explosion_type].flareanimspeed * 15.0f);

        for (i = 0; i < EXPLOSION_PARTS_LEN; i++)
        {
            if (maxframe < exp->parts[i].frame)
            {
                exp->parts[i].frame = 0;
            }
        }
    }

    return gdl;
}



/***
 * Perfect Dark Gfx *explosionRenderPart(struct explosion *exp, struct explosionpart *part, Gfx *gdl, struct coord *coord, s32 arg4)
 *
*/
Gfx *explosionRenderPart(struct ExplosionPart *part, Gfx *gdl, coord3d *coord)
{
    f32 frac;

    Vtx template;
    Mtxf *camMtx;
    coord3d *eyepos;
    coord3d rightCos;
    coord3d rightSin;
    coord3d upCos;
    coord3d upSin;

    f32 dx;
    f32 dy;
    f32 dz;

    Vtx *vertices;

    f32 cosSize;
    f32 sinSize;

    f32 cx;
    f32 cy;
    f32 cz;

    f32 dist;
    f32 pullback;

    template = g_ExplosionRenderPartDefaultVertex;

    camMtx = camGetViewToWorldMtxf();
    eyepos = playerGetPos();

    dx = part->pos.f[0] - eyepos->f[0];
    dy = part->pos.f[1] - eyepos->f[1];
    dz = part->pos.f[2] - eyepos->f[2];

    dist = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    pullback = dist * 0.5f;
    if (pullback > 100.0f)
    {
        pullback = 100.0f;
    }

    if (dist == 0)
    {
        frac = 0.0f;
    }
    else
    {
        frac = (dist - pullback) / dist;
    }

    cosSize = part->size * frac;
    sinSize = part->rot * frac;

    cx = eyepos->f[0] + (dx * frac);
    cy = eyepos->f[1] + (dy * frac);
    cz = eyepos->f[2] + (dz * frac);

    vertices = dynAllocateVertices(4);

    vertices[0] = template;
    vertices[1] = template;
    vertices[2] = template;
    vertices[3] = template;

    rightCos.f[0] = camMtx->m[0][0] * cosSize;
    rightCos.f[1] = camMtx->m[0][1] * cosSize;
    rightCos.f[2] = camMtx->m[0][2] * cosSize;

    rightSin.f[0] = camMtx->m[0][0] * sinSize;
    rightSin.f[1] = camMtx->m[0][1] * sinSize;
    rightSin.f[2] = camMtx->m[0][2] * sinSize;

    upCos.f[0] = camMtx->m[1][0] * cosSize;
    upCos.f[1] = camMtx->m[1][1] * cosSize;
    upCos.f[2] = camMtx->m[1][2] * cosSize;

    upSin.f[0] = camMtx->m[1][0] * sinSize;
    upSin.f[1] = camMtx->m[1][1] * sinSize;
    upSin.f[2] = camMtx->m[1][2] * sinSize;

    vertices[0].v.ob[0] = (cx - rightCos.f[0] - upSin.f[0]) * bgGetLevelScale() - coord->f[0];
	vertices[0].v.ob[1] = (cy - rightCos.f[1] - upSin.f[1]) * bgGetLevelScale() - coord->f[1];
	vertices[0].v.ob[2] = (cz - rightCos.f[2] - upSin.f[2]) * bgGetLevelScale() - coord->f[2];
	vertices[0].v.tc[0] = 1760;
	vertices[0].v.tc[1] = 0;

	vertices[1].v.ob[0] = (cx + rightSin.f[0] - upCos.f[0]) * bgGetLevelScale() - coord->f[0];
	vertices[1].v.ob[1] = (cy + rightSin.f[1] - upCos.f[1]) * bgGetLevelScale() - coord->f[1];
	vertices[1].v.ob[2] = (cz + rightSin.f[2] - upCos.f[2]) * bgGetLevelScale() - coord->f[2];
	vertices[1].v.tc[0] = 0;
	vertices[1].v.tc[1] = 0;

	vertices[2].v.ob[0] = (cx + rightCos.f[0] + upSin.f[0]) * bgGetLevelScale() - coord->f[0];
	vertices[2].v.ob[1] = (cy + rightCos.f[1] + upSin.f[1]) * bgGetLevelScale() - coord->f[1];
	vertices[2].v.ob[2] = (cz + rightCos.f[2] + upSin.f[2]) * bgGetLevelScale() - coord->f[2];
	vertices[2].v.tc[0] = 0;
	vertices[2].v.tc[1] = 1760;

	vertices[3].v.ob[0] = (cx - rightSin.f[0] + upCos.f[0]) * bgGetLevelScale() - coord->f[0];
	vertices[3].v.ob[1] = (cy - rightSin.f[1] + upCos.f[1]) * bgGetLevelScale() - coord->f[1];
	vertices[3].v.ob[2] = (cz - rightSin.f[2] + upCos.f[2]) * bgGetLevelScale() - coord->f[2];
	vertices[3].v.tc[0] = 1760;
	vertices[3].v.tc[1] = 1760;

    gSPVertex(gdl++, osVirtualToPhysical(vertices), 4, 0);

	gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);

	return gdl;
}



/***
 * Perfect Dark Gfx *smokeRenderPart(struct smoke *smoke, struct smokepart *part, Gfx *gdl, struct coord *coord, f32 size)
 *
*/
Gfx *explosionSmokeRenderPart(struct Smoke *smoke, struct SmokePart *smoke_part, Gfx *gdl, coord3d *roompos)
{
    Vtx *vertices;
    Vtx template;
    Mtxf *mtx;
    coord3d rightCos;
    coord3d rightSin;
    coord3d upCos;
    coord3d upSin;
    f32 cosSize;
    f32 sinSize;
    f32 cx;
    f32 cy;
    f32 cz;
    u8 alpha;
    coord3d *eyepos;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dist;
    f32 range;
    f32 mult;
    f32 x0;
    f32 y0;
    f32 z0;

    template = g_SmokeRenderPartDefaultVertex;

    mtx = camGetViewToWorldMtxf();
    eyepos = playerGetPos();

    if (g_SmokeTypes[smoke->smoke_type].rateappear >= smoke_part->count)
    {
        alpha = (smoke_part->alpha / (f32) g_SmokeTypes[smoke->smoke_type].rateappear) * smoke_part->count;
    }
    else
    {
        alpha = smoke_part->alpha;
    }

    vertices = dynAllocateVertices(4);

    vertices[0] = template;
    vertices[1] = template;
    vertices[2] = template;
    vertices[3] = template;

    cosSize = cosf(smoke_part->rot) * smoke_part->size;
    sinSize = sinf(smoke_part->rot) * smoke_part->size;

    cx = smoke_part->pos.f[0] + (sinf(smoke_part->offset1) * 7.0f);
    cy = smoke_part->pos.f[1];
    cz = smoke_part->pos.f[2] + (sinf(smoke_part->offset2) * 7.0f);

    dx = cx - eyepos->f[0];
    dy = cy - eyepos->f[1];
    dz = cz - eyepos->f[2];

    dist = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

    if (dist > 30000.0f)
    {
        return gdl;
    }

    range = dist * 0.5f;

	if (range > 100.0f) {
		range = 100.0f;
	}

	if (dist == 0.0f) {
		mult = 0.0f;
	} else {
		mult = (dist - range) / dist;
	}

	cosSize = cosSize * mult;
	sinSize = sinSize * mult;

	cx = eyepos->f[0] + dx * mult;
	cy = eyepos->f[1] + dy * mult;
	cz = eyepos->f[2] + dz * mult;

	rightCos.f[0] = mtx->m[0][0] * cosSize;
	rightCos.f[1] = mtx->m[0][1] * cosSize;
	rightCos.f[2] = mtx->m[0][2] * cosSize;
	rightSin.f[0] = mtx->m[0][0] * sinSize;
	rightSin.f[1] = mtx->m[0][1] * sinSize;
	rightSin.f[2] = mtx->m[0][2] * sinSize;
	upCos.f[0] = mtx->m[1][0] * cosSize;
	upCos.f[1] = mtx->m[1][1] * cosSize;
	upCos.f[2] = mtx->m[1][2] * cosSize;
	upSin.f[0] = mtx->m[1][0] * sinSize;
	upSin.f[1] = mtx->m[1][1] * sinSize;
	upSin.f[2] = mtx->m[1][2] * sinSize;

    x0 = ((cx - rightCos.f[0] - upSin.f[0]) * bgGetLevelScale() - roompos->f[0]) * 10.0f;
	y0 = ((cy - rightCos.f[1] - upSin.f[1]) * bgGetLevelScale() - roompos->f[1]) * 10.0f;
	z0 = ((cz - rightCos.f[2] - upSin.f[2]) * bgGetLevelScale() - roompos->f[2]) * 10.0f;

    if (x0 > 30000.0f
        || x0 < -30000.0f
        || y0 > 30000.0f
        || y0 < -30000.0f
        || z0 > 30000.0f
        || z0 < -30000.0f)
    {
		return gdl;
	}

    vertices[0].v.ob[0] = x0;
	vertices[0].v.ob[1] = y0;
	vertices[0].v.ob[2] = z0;
	vertices[0].v.tc[0] = 1760;
	vertices[0].v.tc[1] = 0;
    vertices[0].v.cn[0] = g_SmokeTypes[smoke->smoke_type].r;
    vertices[0].v.cn[1] = g_SmokeTypes[smoke->smoke_type].g;
    vertices[0].v.cn[2] = g_SmokeTypes[smoke->smoke_type].b;
    vertices[0].v.cn[3] = alpha;

    vertices[1].v.ob[0] = ((((cx + rightSin.f[0]) - upCos.f[0]) * bgGetLevelScale()) - roompos->f[0]) * 10.0f;
    vertices[1].v.ob[1] = ((((cy + rightSin.f[1]) - upCos.f[1]) * bgGetLevelScale()) - roompos->f[1]) * 10.0f;
    vertices[1].v.ob[2] = ((((cz + rightSin.f[2]) - upCos.f[2]) * bgGetLevelScale()) - roompos->f[2]) * 10.0f;
    vertices[1].v.tc[0] = 0;
    vertices[1].v.tc[1] = 0;
    vertices[1].v.cn[0] = g_SmokeTypes[smoke->smoke_type].r;
    vertices[1].v.cn[1] = g_SmokeTypes[smoke->smoke_type].g;
    vertices[1].v.cn[2] = g_SmokeTypes[smoke->smoke_type].b;
    vertices[1].v.cn[3] = alpha;

    vertices[2].v.ob[0] = (((cx + rightCos.f[0] + upSin.f[0]) * bgGetLevelScale()) - roompos->f[0]) * 10.0f;
    vertices[2].v.ob[1] = (((cy + rightCos.f[1] + upSin.f[1]) * bgGetLevelScale()) - roompos->f[1]) * 10.0f;
    vertices[2].v.ob[2] = (((cz + rightCos.f[2] + upSin.f[2]) * bgGetLevelScale()) - roompos->f[2]) * 10.0f;
    vertices[2].v.tc[0] = 0;
    vertices[2].v.tc[1] = 1760;
    vertices[2].v.cn[0] = g_SmokeTypes[smoke->smoke_type].r;
    vertices[2].v.cn[1] = g_SmokeTypes[smoke->smoke_type].g;
    vertices[2].v.cn[2] = g_SmokeTypes[smoke->smoke_type].b;
    vertices[2].v.cn[3] = alpha;

    vertices[3].v.ob[0] = ((((cx - rightSin.f[0]) + upCos.f[0]) * bgGetLevelScale()) - roompos->f[0]) * 10.0f;
    vertices[3].v.ob[1] = ((((cy - rightSin.f[1]) + upCos.f[1]) * bgGetLevelScale()) - roompos->f[1]) * 10.0f;
    vertices[3].v.ob[2] = ((((cz - rightSin.f[2]) + upCos.f[2]) * bgGetLevelScale()) - roompos->f[2]) * 10.0f;
    vertices[3].v.tc[0] = 1760;
    vertices[3].v.tc[1] = 1760;
    vertices[3].v.cn[0] = g_SmokeTypes[smoke->smoke_type].r;
    vertices[3].v.cn[1] = g_SmokeTypes[smoke->smoke_type].g;
    vertices[3].v.cn[2] = g_SmokeTypes[smoke->smoke_type].b;
    vertices[3].v.cn[3] = alpha;

    gSPVertex(gdl++, osVirtualToPhysical(vertices), 4, 0);

	gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);

	return gdl;
}


void explosionCreateSmoke(coord3d *pos, StandTile *stan, s16 smoke_type, u8 *rooms, s32 flags)
{
    struct Smoke *smoke;
    struct Smoke *smoke_tmp;
    s32 i;
    s32 player_count;
    PropRecord *prop;

    smoke = NULL;
    player_count = getPlayerCount();

    for (i = 0; i < 20; i++)
    {
        if (g_SmokeBuffer[i].prop == NULL)
        {
            smoke = &g_SmokeBuffer[i];
            break;
        }
        else if (player_count >= 2)
        {
            smoke_tmp = (i + g_SmokeBuffer);
            if (((smoke_tmp->smoke_type != 7) && (smoke_tmp->smoke_type != 8)) && (smoke_tmp->smoke_type != 9))
            {
                smoke_tmp->duration = (s16) g_SmokeTypes[smoke_tmp->smoke_type].duration;
            }
        }
    }

    if (smoke == NULL) { return; }

    prop = propAllocate();
    if (prop == NULL) { return; }

    prop->type = 8;
    prop->flags |= 2;
    prop->smoke = smoke;
    prop->stan = stan;
    prop->pos.x = pos->x;
    prop->pos.y = pos->y;
    prop->pos.z = pos->z;

    for (i = 0; (rooms[i] != 0xFF) && (i < 7); i++)
    {
        prop->rooms[i] = rooms[i];
    }
    prop->rooms[i] = 0xFF;

    if (flags != 0)
    {
        prop->flags |= 8;
    }

    propActivateThisFrame(prop);
    propEnable(prop);
    smoke->prop = prop;
    smoke->duration = 0;
    smoke->smoke_type = smoke_type;
}


/***
 * Perfect Dark u32 smokeTick(struct prop *prop)
 *
*/
s32 explosionSmokeTick(PropRecord *prop)
{
    f32 dx;
    s32 i;
    s32 j;
    s32 k;
    f32 lvupdate;
    struct Smoke *smoke;
    struct SmokePart *part;
    coord3d bbmin;
	coord3d bbmax;
    f32 radius;
    s32 finished;

    smoke = prop->smoke;

	if (g_ClockTimer == 0)
    {
		return TICKOP_NONE;
	}

    lvupdate = (g_ClockTimer < 15) ? (f32) g_ClockTimer : 15.0f;

    for (i = 0; i < (s32) lvupdate; i++)
    {
        smoke->duration += 1;

		for (j = 0; j < SMOKE_PARTS_LEN; j++)
        {
            part = &smoke->parts[j];

			if (part->size != 0.0f)
            {
				part->pos.f[1] += 0.3f;
                part->size += 0.15f;

                part->alpha -= g_SmokeTypes[smoke->smoke_type].fg_rotrate;
                part->count++;
                part->rot += part->deltarot;

                part->offset1 += 0.02f + RANDOMFRAC() * 0.01f;
				part->offset2 += 0.02f + RANDOMFRAC() * 0.01f;

                if (part->alpha < 4.0f)
                {
                    part->size = 0.0f;
                }
			}
		}

        if (smoke->duration < g_SmokeTypes[smoke->smoke_type].duration)
        {
            if (smoke->duration % g_SmokeTypes[smoke->smoke_type].ratedissolve == 1)
            {
        		for (j = 0; j < SMOKE_PARTS_LEN; j++)
                {
                    if (smoke->parts[j].size == 0.0f)
                    {
                        part = &smoke->parts[j];

                        part->size = g_SmokeTypes[smoke->smoke_type].size * (RANDOMFRAC() * 0.5f + 1.0f);
                        part->alpha = (randomGetNext() % 70) + 110.0f;
						part->count = 0;
						part->rot = RANDOMFRAC() * M_TAU_F;
						part->deltarot = (0.5f - RANDOMFRAC()) * g_SmokeTypes[smoke->smoke_type].bg_rotrate;

						part->pos.x = prop->pos.x;
                        part->pos.y = prop->pos.y;
                        part->pos.z = prop->pos.z;

						part->offset1 = RANDOMFRAC() * 0.5f;
						part->offset2 = RANDOMFRAC() * 0.5f;

						if (smoke->duration > g_SmokeTypes[smoke->smoke_type].duration - g_SmokeTypes[smoke->smoke_type].propagated_clouds)
                        {
							part->alpha *= (g_SmokeTypes[smoke->smoke_type].duration - smoke->duration) / (f32)g_SmokeTypes[smoke->smoke_type].propagated_clouds;
						}

						break;
                    }
                }
            }
        }
    }

    bbmin.x = prop->pos.x - 1.0f;
	bbmin.y = prop->pos.y - 1.0f;
	bbmin.z = prop->pos.z - 1.0f;

	bbmax.x = prop->pos.x + 1.0f;
	bbmax.y = prop->pos.y + 1.0f;
	bbmax.z = prop->pos.z + 1.0f;

    radius = 0.0f;

    for (j = 0; j < SMOKE_PARTS_LEN; j++)
    {
		if (smoke->parts[j].size != 0.0f)
        {
			for (k = 0; k < 3; k++)
            {
				if (bbmin.f[k] > smoke->parts[j].pos.f[k] - smoke->parts[j].size)
                {
					bbmin.f[k] = smoke->parts[j].pos.f[k] - smoke->parts[j].size;
				}
                else if (bbmax.f[k] < smoke->parts[j].pos.f[k] - smoke->parts[j].size)
                {
					bbmax.f[k] = smoke->parts[j].pos.f[k] - smoke->parts[j].size;
				}
			}
		}
	}

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

    if (smoke->duration > g_SmokeTypes[smoke->smoke_type].ratedissolve)
    {
		finished = 1;

		for (j = 0; j < SMOKE_PARTS_LEN; j++)
        {
			if (smoke->parts[j].size > 0.0f)
            {
				finished = 0;
				break;
			}
		}
    }
    else
    {
		finished = 0;
	}

    if (finished != 0)
    {
        smoke->prop = NULL;
        return TICKOP_FREE;
    }

    return TICKOP_NONE;
}


u8 smokePropTick(PropRecord* prop)
{
    Mtxf* player_matrix;

    player_matrix = camGetWorldToScreenMtxf();
    prop->zDepth = -((((player_matrix->m[0][2] * prop->pos.x) + (player_matrix->m[1][2] * prop->pos.y)) + (player_matrix->m[2][2] * prop->pos.z)) + player_matrix->m[3][2]);

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


extern Gfx globalDL_0x000 NO_SDATA;

/***
*/
Gfx *explosionRenderPropSmoke(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    struct Smoke *smoke;
    s32 i;
    bbox2d bbox;
    coord3d *roompos;
    s32 room;


    room = prop->rooms[0];
    smoke = prop->smoke;
    roompos = roomGetPos(room);

    if (withalpha == 0)
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

    gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjViewMtx()), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    gdl = roomMtxLoad(gdl, room);

    gSPMatrix(gdl++, osVirtualToPhysical((void*)&g_ExplosionScaleMtx), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);

    gSPSegment(gdl++, SPSEGMENT_GETITLE, osVirtualToPhysical(g_ImageBank));

    gSPDisplayList(gdl++, &globalDL_0x000);

    gDPSetColorDither(gdl++, G_CD_NOISE);

    for (i = 0; i < SMOKE_PARTS_LEN; i++)
    {
        if (smoke->parts[i].size > 0.0f)
        {
            gdl = explosionSmokeRenderPart(smoke, &smoke->parts[i], gdl, roompos);
        }
        else
        {
            smoke->parts[i].size = 0.0f;
        }
    }

    gDPSetColorDither(gdl++, G_CD_BAYER);

    gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjectionMtx()), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    return gdl;
}


// https://decomp.me/scratch/RT8eu
void explosionInitFlyingParticles(coord3d *spawnpos, f32 spawn_rand_scale, f32 spawn_horiz_drift_scale, f32 spawn_vert_drift_scale, f32 spawn_tex_scale)
{
    // these are gray rectangles of dust created from shooting walls with guns that fall down with gravity
    // a bullet will create a group of them flying off the wall

    f32 rand1;
    f32 rand2;
    f32 rand3;
    s16 unk08_upper;
    s16 unk0A_upper;
    s32 rand_s8;

    rand1 = (2.0f * (((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX))) - 1.0f;
    rand2 = ((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 1.12f) - 0.12f;
    rand3 = (2.0f * (((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX))) - 1.0f;

    g_FlyingParticlesBuffer[g_NextParticle].age = 1;

    g_FlyingParticlesBuffer[g_NextParticle].position.f[0] = spawnpos->f[0] + (spawn_rand_scale * rand1);
    g_FlyingParticlesBuffer[g_NextParticle].position.f[1] = spawnpos->f[1] + (spawn_rand_scale * rand2);
    g_FlyingParticlesBuffer[g_NextParticle].position.f[2] = spawnpos->f[2] + (spawn_rand_scale * rand3);

    g_FlyingParticlesBuffer[g_NextParticle].position_drift.f[0] = rand1 * spawn_horiz_drift_scale;
    g_FlyingParticlesBuffer[g_NextParticle].position_drift.f[1] = rand2 * spawn_vert_drift_scale;
    g_FlyingParticlesBuffer[g_NextParticle].position_drift.f[2] = rand3 * spawn_horiz_drift_scale;

    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.ob[0] = (s16) ((s32) ((((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.75f) + 0.75f) * spawn_tex_scale));
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.ob[1] = 0;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.ob[2] = (s16) ((s32) ((((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.75f) + 0.75f) * spawn_tex_scale));

    if (1)
    {
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.ob[0] = (s16) ((s32) ((((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.75f) + 0.75f) * spawn_tex_scale));
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.ob[1] = 0;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.ob[2] = (s16) ((s32) ((((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.75f) + 0.75f) * (-spawn_tex_scale)));

        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.ob[0] = (s16) ((s32) ((((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.75f) + 0.75f) * (-spawn_tex_scale)));
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.ob[1] = 0;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.ob[2] = (s16) ((s32) ((((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.75f) + 0.75f) * (-spawn_tex_scale)));

        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.ob[0] = (s16) ((s32) ((((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.75f) + 0.75f) * (-spawn_tex_scale)));
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.ob[1] = 0;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.ob[2] = (s16) ((s32) ((((((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.75f) + 0.75f) * spawn_tex_scale));
    }


    unk08_upper = (randomGetNext() & 3) << 8;
    unk0A_upper = (randomGetNext() & 3) << 8;

    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.tc[0] = unk08_upper + 0xE0;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.tc[1] = unk0A_upper + 0xE0;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.tc[0] = unk08_upper + 0xE0;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.tc[1] = unk0A_upper;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.tc[0] = unk08_upper;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.tc[1] = unk0A_upper;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.tc[0] = unk08_upper;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.tc[1] = unk0A_upper + 0xE0;

    if (randomGetNext() & 1)
    {
        rand_s8 = 0xFF - (randomGetNext() & 0x3F);
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.cn[2] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.cn[1] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.cn[0] = rand_s8;
        rand_s8 = 0xFF - (randomGetNext() & 0x3F);
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.cn[2] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.cn[1] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.cn[0] = rand_s8;
        rand_s8 = 0xFF - (randomGetNext() & 0x3F);
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.cn[2] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.cn[1] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.cn[0] = rand_s8;
        rand_s8 = 0xFF - (randomGetNext() & 0x3F);
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.cn[2] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.cn[1] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.cn[0] = rand_s8;
    }
    else
    {
        rand_s8 = randomGetNext() & 0x3F;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.cn[2] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.cn[1] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.cn[0] = rand_s8;
        rand_s8 = randomGetNext() & 0x3F;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.cn[2] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.cn[1] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.cn[0] = rand_s8;
        rand_s8 = randomGetNext() & 0x3F;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.cn[2] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.cn[1] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.cn[0] = rand_s8;
        rand_s8 = randomGetNext() & 0x3F;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.cn[2] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.cn[1] = rand_s8;
        g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.cn[0] = rand_s8;
    }

    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[0].v.cn[3] = 0xdc;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[1].v.cn[3] = 0xdc;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[2].v.cn[3] = 0xdc;
    g_FlyingParticlesBuffer[g_NextParticle].vertex_list[3].v.cn[3] = 0xdc;

    g_FlyingParticlesBuffer[g_NextParticle].rotation.f[0] = (((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * M_TAU_F;
    g_FlyingParticlesBuffer[g_NextParticle].rotation.f[1] = (((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * M_TAU_F;
    g_FlyingParticlesBuffer[g_NextParticle].rotation.f[2] = (((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * M_TAU_F;

    g_FlyingParticlesBuffer[g_NextParticle].rotation_drift.f[0] = (((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.1f;
    g_FlyingParticlesBuffer[g_NextParticle].rotation_drift.f[1] = (((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.1f;
    g_FlyingParticlesBuffer[g_NextParticle].rotation_drift.f[2] = (((f32) randomGetNext()) * (1.0f / (f32)UINT_MAX)) * 0.1f;

    g_NextParticle++;
    if (g_NextParticle >= g_MaxParticles)
    {
        g_NextParticle = 0;
    }
}


void explosionUpdateFlyingParticles(void)
{
    f32 scalar;
    s32 i;
    s32 j;

    if (g_ClockTimer < 0xF)
    {
        scalar = g_ClockTimer;
    }
    else
    {
        scalar = 15.0f;
    }


    for (i = 0; i < g_MaxParticles; i++)
    {
        if (g_FlyingParticlesBuffer[i].age > 0)
        {
            g_FlyingParticlesBuffer[i].age += (s32) scalar;

            g_FlyingParticlesBuffer[i].rotation.f[0] += g_FlyingParticlesBuffer[i].rotation_drift.f[0] * scalar;
            g_FlyingParticlesBuffer[i].rotation.f[1] += g_FlyingParticlesBuffer[i].rotation_drift.f[1] * scalar;
            g_FlyingParticlesBuffer[i].rotation.f[2] += g_FlyingParticlesBuffer[i].rotation_drift.f[2] * scalar;

            g_FlyingParticlesBuffer[i].position.f[0] += g_FlyingParticlesBuffer[i].position_drift.f[0] * scalar;
            g_FlyingParticlesBuffer[i].position.f[2] += g_FlyingParticlesBuffer[i].position_drift.f[2] * scalar;

            for (j = 0; j < (s32)scalar; j++)
            {
                // initially sends particles flying up
                g_FlyingParticlesBuffer[i].position.f[1] += g_FlyingParticlesBuffer[i].position_drift.f[1];

                // applies gravity so particles fall down
                if (g_FlyingParticlesBuffer[i].position_drift.f[1] > -3.75f)
                {
                    g_FlyingParticlesBuffer[i].position_drift.f[1] -= 0.2f;
                }
            }

            // handles particles life time
            if ((g_FlyingParticlesBuffer[i].age >= 0x65) && (!(randomGetNext() & 0x1F) || (g_FlyingParticlesBuffer[i].age == 0x12C)))
            {
                g_FlyingParticlesBuffer[i].age = 0;
            }

            // position-related. deletes particles that are too low or too high.
            if ((g_FlyingParticlesBuffer[i].position.f[1] < -30000.0f) || (g_FlyingParticlesBuffer[i].position.f[1] > 30000.0f))
            {
                g_FlyingParticlesBuffer[i].age = 0;
            }
        }
    }
}


extern Gfx globalDL_0xa50 NO_SDATA;

/***
*/
Gfx *explosionRenderFlyingParticles(Gfx *gdl)
{
    Mtxf mtx;
    s32 i;
    Mtx *gbimtx;
    struct FlyingParticles *particles;

    gSPClearGeometryMode(gdl++, G_CULL_BOTH);
    gSPSegment(gdl++, SPSEGMENT_GETITLE, osVirtualToPhysical(g_ImageBank));
    gSPDisplayList(gdl++, &globalDL_0xa50);
    gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);

    for (i = 0; i < g_MaxParticles; i++)
    {
        // HACK: regalloc has instructions backwards.
        particles = (struct FlyingParticles *)(u32)g_FlyingParticlesBuffer + i;

        if (particles->age > 0)
        {
            mtx4LoadRotationAndTranslation(&particles->position, &particles->rotation, &mtx);
            mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), &mtx);

            if ((mtx.m[3][0] < 20000.0f)
                && (mtx.m[3][0] > -20000.0f)
                && (mtx.m[3][1] < 20000.0f)
                && (mtx.m[3][1] > -20000.0f)
                && (mtx.m[3][2] < 20000.0f)
                && (mtx.m[3][2] > -20000.0f))
            {
                gbimtx = dynAllocateMatrix();
                mtxF2L(&mtx, (Mtxf *)gbimtx);

                gSPMatrix(gdl++, osVirtualToPhysical((void*)gbimtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPVertex(gdl++, osVirtualToPhysical((void*)g_FlyingParticlesBuffer[i].vertex_list), 4, 0)
                gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
            }
        }
    }

    return gdl;
}




/***
*/
void explosionScorchTick(coord3d *pos, f32 explosion_size, s16 room)
{
    Vtx template;
    f32 angle;
    f32 cosSize;
    f32 sinSize;
    u8 alpha;
    coord3d *roompos;
    u32 shade;

    template = g_ScorchDefaultVertex;

    angle = RANDOMFRAC() * M_TAU_F;
    alpha = 0xFF - (randomGetNext() % 80U);

    roompos = roomGetPos((s32) room);

    if (!isMpGame())
    {
        if (explosion_size > 200.0f)
        {
            explosion_size = 200.0f;
        }

        explosion_size *= (0.8f + (0.2f * RANDOMFRAC()));

        pos->f[0] = (pos->f[0] * bgGetLevelScale()) - roompos->f[0];
        pos->f[1] = (pos->f[1] * bgGetLevelScale()) - roompos->f[1];
        pos->f[2] = (pos->f[2] * bgGetLevelScale()) - roompos->f[2];

        explosion_size *= bgGetLevelScale();

        cosSize = cosf(angle) * explosion_size;
        sinSize = sinf(angle) * explosion_size;

        g_ScorchBuffer[g_NextScorch].roomid = room;
        g_ScorchBuffer[g_NextScorch].pos.f[0] = pos->f[0];
        g_ScorchBuffer[g_NextScorch].pos.f[1] = pos->f[1];
        g_ScorchBuffer[g_NextScorch].pos.f[2] = pos->f[2];
        g_ScorchBuffer[g_NextScorch].explosion_size = explosion_size;

        g_ScorchBuffer[g_NextScorch].vertex_list[0] = template;
        g_ScorchBuffer[g_NextScorch].vertex_list[1] = template;
        g_ScorchBuffer[g_NextScorch].vertex_list[2] = template;
        g_ScorchBuffer[g_NextScorch].vertex_list[3] = template;

        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.ob[0] = (s16) (s32) (pos->f[0] + cosSize);
        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.ob[1] = (s16) (s32) (pos->f[1] + 0.5f);
        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.ob[2] = (s16) (s32) (pos->f[2] + sinSize);
        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.tc[0] = 0;
        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.tc[1] = g_ImageGeneric->width << 5;
        shade = randomGetNext() % 50U;
        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.cn[2] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.cn[1] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.cn[0] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[0].v.cn[3] = alpha;

        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.ob[0] = (s16) (s32) (pos->f[0] + sinSize);
        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.ob[1] = (s16) (s32) (pos->f[1] + 0.5f);
        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.ob[2] = (s16) (s32) (pos->f[2] - cosSize);
        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.tc[0] = 0;
        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.tc[1] = 0;
        shade = randomGetNext() % 50U;
        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.cn[2] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.cn[1] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.cn[0] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[1].v.cn[3] = alpha;

        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.ob[0] = (s16) (s32) (pos->f[0] - cosSize);
        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.ob[1] = (s16) (s32) (pos->f[1] + 0.5f);
        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.ob[2] = (s16) (s32) (pos->f[2] - sinSize);
        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.tc[0] = g_ImageGeneric->height << 5;
        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.tc[1] = 0;
        shade = randomGetNext() % 50U;
        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.cn[2] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.cn[1] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.cn[0] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[2].v.cn[3] = alpha;

        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.ob[0] = (s16) (s32) (pos->f[0] - sinSize);
        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.ob[1] = (s16) (s32) (pos->f[1] + 0.5f);
        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.ob[2] = (s16) (s32) (pos->f[2] + cosSize);
        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.tc[0] = g_ImageGeneric->width << 5;
        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.tc[1] = g_ImageGeneric->height << 5;
        shade = randomGetNext() % 50U;
        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.cn[2] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.cn[1] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.cn[0] = (u8) shade;
        g_ScorchBuffer[g_NextScorch].vertex_list[3].v.cn[3] = alpha;

        g_NextScorch++;
        if (g_NextScorch >= SCORCH_BUFFER_LEN)
        {
            g_NextScorch = 0;
        }
    }
}




Gfx *explosionRenderScorchBuffer(Gfx *gdl)
{
    s32 i;
    s32 lastroom = -1;

    if (isMpGame())
    {
        return gdl;
    }
    else
    {
        gSPSetGeometryMode(gdl++, G_CULL_BACK);
        gSPClearGeometryMode(gdl++, G_CULL_FRONT | G_FOG);
        gDPSetColorDither(gdl++, G_CD_NOISE);

        texSelect(&gdl, g_ImageGeneric, TEXBLEND_CLOUD, TEXZBUF_ON, 2);

        for (i=0; i<20; i++)
        {
            if (g_ScorchBuffer[i].roomid >= 0 && bgRoomIsVisible(g_ScorchBuffer[i].roomid))
            {
                if (lastroom != g_ScorchBuffer[i].roomid)
                {
                    lastroom = g_ScorchBuffer[i].roomid;
                    gdl = roomMtxLoad(gdl, g_ScorchBuffer[i].roomid);
                }

                /**
                 * Loads into the RSP vertex buffer the vertices that will be used by the gSP1Triangle commands that generates polygons.
                 *
                 * param v: the segment address of vertex list.
                 * param n: the number of vertices (1~32)
                 * param v0: Starting index in vertex buffer where vertices are to be loaded
                 * gSPVertex(Gfx *gdl, Vtx *v, u32 n, u32 v0)
                */
                gSPVertex(gdl++, osVirtualToPhysical((void*)g_ScorchBuffer[i].vertex_list), 4, 0);
                gSP2Triangles(gdl++,
                                0, 1, 2, 0,
                                0, 2, 3, 0);
            }
        }

        gDPSetColorDither(gdl++, G_CD_BAYER);
    }

    return gdl;
}





s32 explosionRoundFloat(f32 value)
{
    if (value >= 0.0f)
    {
        return (s32) (value + 0.5f);
    }

    return (s32) (value - 0.5f);
}



void explosionClearBulletImpactRoomByFlag(PropRecord* prop, s8 flag)
{
    s32 i;
    for (i = 0; i < BULLET_IMPACT_BUFFER_LEN; i++)
    {
        if ((prop == g_BulletImpactBuffer[i].prop) && (flag == g_BulletImpactBuffer[i].room_clear_flag))
        {
            g_BulletImpactBuffer[i].room = -1;
        }
    }
}



void explosionClearBulletImpactRoom(PropRecord* prop)
{
    s32 i;
    for (i = 0; i < BULLET_IMPACT_BUFFER_LEN; i++)
    {
        if ((prop == g_BulletImpactBuffer[i].prop) && (g_ImpactTypes[g_BulletImpactBuffer[i].impact_type].renderMode == 2))
        {
            g_BulletImpactBuffer[i].room = -1;
        }
    }
}


void explosionSetBulletImpactAlpha(s32 index)
{
    u32 val;
    s32 i;
    for (i = 0; i < SMOKE_PARTS_LEN; i++)
    {
        val = (u32) (((f32) i / 10.0f) * 255.0f);

        g_BulletImpactBuffer[index].vertex_list[3].v.cn[3] = val; // alpha
        g_BulletImpactBuffer[index].vertex_list[2].v.cn[3] = val; // alpha
        g_BulletImpactBuffer[index].vertex_list[1].v.cn[3] = val; // alpha
        g_BulletImpactBuffer[index].vertex_list[0].v.cn[3] = val; // alpha

        if (++index >= BULLET_IMPACT_BUFFER_LEN)
        {
            index = 0;
        }
    }
}


/***
*/
void explosionCreateBulletImpact(coord3d *pos, coord3d *normal, s16 impact_type, s16 room, PropRecord *prop, s8 model_render_pos_index, s8 room_clear_flag)
{
    Vtx template;
    f32 nx;
    f32 ny;
    f32 nz;
    f32 uplen;
    f32 hx;
    f32 hz;
    f32 rightX;
    coord3d *roompos;
    f32 rightZ;
    f32 upX;
    f32 upY;
    f32 upZ;
    s32 shade;
    coord3d centre;
    f32 halfw;
    f32 halfh;
    RenderPosView *renderpos;
    ObjectRecord *obj;
    f32 len;
    f32 rightlen;
    s32 i;
    coord3d right;
    coord3d up;
    s_impacttype *impact;
    u8 red;
    u8 green = 0;
    u8 blue = 0;

    template = g_BulletImpactDefaultVertex;

    if (cheatIsActive(CHEAT_PAINTBALL))
    {
        impact_type = 0x10;
    }

    centre.f[0] = pos->f[0];
    centre.f[1] = pos->f[1];
    centre.f[2] = pos->f[2];

    impact = &g_ImpactTypes[impact_type];

    halfw = impact->width;
    halfh = impact->height;

    if ((normal->f[0] == 0.0f) && (normal->f[2] == 0.0f))
    {
        upX = 0.0f;
        upY = 0.0f;
        rightZ = 0.0f;
        upZ = 1.0f;
        rightX = 1.0f;
    }
    else
    {
        len = sqrtf((normal->f[0] * normal->f[0]) + (normal->f[1] * normal->f[1]) + (normal->f[2] * normal->f[2]));

        nx = normal->f[0] / len;
        ny = normal->f[1] / len;
        nz = normal->f[2] / len;

        len = sqrtf((nx * nx) + (nz * nz));

        hx = nx / len;
        hz = nz / len;

        rightZ = -hx;
        upX = ny * hx;
        upY = -len;
        upZ = ny * hz;
        rightX = hz;
    }

    if (prop != NULL)
    {
        obj = prop->obj;
        renderpos = &obj->model->render_pos[model_render_pos_index];

        right.f[0] = rightX;
        right.f[1] = 0.0f;
        right.f[2] = rightZ;

        up.f[0] = upX;
        up.f[1] = upY;
        up.f[2] = upZ;

        mtx4RotateVecInPlace(&renderpos->pos, &right);
        mtx4RotateVecInPlace(&renderpos->pos, &up);

        rightlen = sqrtf((right.f[0] * right.f[0]) + (right.f[1] * right.f[1]) + (right.f[2] * right.f[2]));
        uplen = sqrtf((up.f[0] * up.f[0]) + (up.f[1] * up.f[1]) + (up.f[2] * up.f[2]));

        halfw /= rightlen;
        halfh /= uplen;

        if ((impact->zbufferMode < 2) && (impact->renderMode == 2))
        {
            obj->state |= PROPSTATE_2;
        }
        else
        {
            obj->state |= PROPSTATE_DAMAGED;
        }
    }
    else
    {
        roompos = roomGetPos((s32) room);
        centre.f[0] = (centre.f[0] * bgGetLevelScale()) - roompos->f[0];
        centre.f[1] = (centre.f[1] * bgGetLevelScale()) - roompos->f[1];
        centre.f[2] = (centre.f[2] * bgGetLevelScale()) - roompos->f[2];
        halfw *= bgGetLevelScale();
        halfh *= bgGetLevelScale();
    }

    g_BulletImpactBuffer[g_NextImpact].prop = prop;
    g_BulletImpactBuffer[g_NextImpact].model_render_pos_index = model_render_pos_index;
    g_BulletImpactBuffer[g_NextImpact].room_clear_flag = room_clear_flag;
    g_BulletImpactBuffer[g_NextImpact].room = room;
    g_BulletImpactBuffer[g_NextImpact].impact_type = impact_type;

    g_BulletImpactBuffer[g_NextImpact].vertex_list[0] = template;
    g_BulletImpactBuffer[g_NextImpact].vertex_list[1] = template;
    g_BulletImpactBuffer[g_NextImpact].vertex_list[2] = template;
    g_BulletImpactBuffer[g_NextImpact].vertex_list[3] = template;

    g_BulletImpactBuffer[g_NextImpact].vertex_list[0].v.ob[0] = explosionRoundFloat((centre.f[0] - (halfw * rightX)) - (halfh * upX));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[0].v.ob[1] = explosionRoundFloat(centre.f[1] - (halfh * upY));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[0].v.ob[2] = explosionRoundFloat((centre.f[2] - (halfw * rightZ)) - (halfh * upZ));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[0].v.tc[0] = 0;
    g_BulletImpactBuffer[g_NextImpact].vertex_list[0].v.tc[1] = g_ImagesImpact[impact_type].height << 5;

    g_BulletImpactBuffer[g_NextImpact].vertex_list[1].v.ob[0] = explosionRoundFloat((centre.f[0] - (halfw * rightX)) + (halfh * upX));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[1].v.ob[1] = explosionRoundFloat(centre.f[1] + (halfh * upY));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[1].v.ob[2] = explosionRoundFloat((centre.f[2] - (halfw * rightZ)) + (halfh * upZ));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[1].v.tc[0] = 0;
    g_BulletImpactBuffer[g_NextImpact].vertex_list[1].v.tc[1] = 0;

    g_BulletImpactBuffer[g_NextImpact].vertex_list[2].v.ob[0] = explosionRoundFloat(centre.f[0] + (halfw * rightX) + (halfh * upX));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[2].v.ob[1] = explosionRoundFloat(centre.f[1] + (halfh * upY));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[2].v.ob[2] = explosionRoundFloat(centre.f[2] + (halfw * rightZ) + (halfh * upZ));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[2].v.tc[0] = g_ImagesImpact[impact_type].width << 5;
    g_BulletImpactBuffer[g_NextImpact].vertex_list[2].v.tc[1] = 0;

    g_BulletImpactBuffer[g_NextImpact].vertex_list[3].v.ob[0] = explosionRoundFloat((centre.f[0] + (halfw * rightX)) - (halfh * upX));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[3].v.ob[1] = explosionRoundFloat(centre.f[1] - (halfh * upY));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[3].v.ob[2] = explosionRoundFloat((centre.f[2] + (halfw * rightZ)) - (halfh * upZ));
    g_BulletImpactBuffer[g_NextImpact].vertex_list[3].v.tc[0] = g_ImagesImpact[impact_type].width << 5;
    g_BulletImpactBuffer[g_NextImpact].vertex_list[3].v.tc[1] = g_ImagesImpact[impact_type].height << 5;

    for (i = 0; i < 4; i++)
    {
        switch (impact->apptype)
        {
            default:
                red = 0;
            break;

            case 1:
                shade = 0xFF - (randomGetNext() % 40U);
                blue = shade;
                green = shade;
                red = shade;
            break;

            case 0:
                shade = randomGetNext() % 40U;
                blue = shade;
                green = shade;
                red = shade;
            break;

            case 2:
                red = (randomGetNext() & 1) ? 0xff : 0;
                green = (randomGetNext() & 1) ? 0xff : 0;
                blue = (randomGetNext() & 1) ? 0xff : 0;
            break;
        }

        g_BulletImpactBuffer[g_NextImpact].vertex_list[i].v.cn[0] = red;
        g_BulletImpactBuffer[g_NextImpact].vertex_list[i].v.cn[1] = green;
        g_BulletImpactBuffer[g_NextImpact].vertex_list[i].v.cn[2] = blue;
        g_BulletImpactBuffer[g_NextImpact].vertex_list[i].v.cn[3] = 0xff; // alpha
    }

    g_NextImpact++;
    if (g_NextImpact >= BULLET_IMPACT_BUFFER_LEN)
    {
        g_NextImpact = 0;
    }

    explosionSetBulletImpactAlpha(g_NextImpact);

    g_BulletImpactBuffer[g_NextImpact].room = -1;
}





/***
 * Perfect Dark Gfx *smokeRender(struct prop *prop, Gfx *gdl, bool xlupass)
 *
*/
Gfx *explosionRenderBulletImpactOnProp(Gfx *gdl, PropRecord *prop, s32 pass)
{

    s32 i;
    s32 drewAny;
    ObjectRecord *obj;
    s32 lasttype;
    RenderPosView *render_pos;

    s16 lastmtx;
    s32 impact_type;
    s32 draw;

    lastmtx = -1;
    drewAny = 0;
    obj = NULL;
    lasttype = -1;

    if (prop != NULL)
    {
        obj = prop->obj;
    }

    gSPClearGeometryMode(gdl++, G_CULL_BOTH);
    gDPSetColorDither(gdl++, G_CD_NOISE);

    for (i = 0; i < BULLET_IMPACT_BUFFER_LEN; i++)
    {
        if (prop == g_BulletImpactBuffer[i].prop)
        {
            if (g_BulletImpactBuffer[i].room >= 0)
            {
                if (prop || bgRoomIsVisible(g_BulletImpactBuffer[i].room))
                {
                    impact_type = g_BulletImpactBuffer[i].impact_type;

                    if (pass)
                    {
                        draw = g_ImpactTypes[impact_type].zbufferMode < 2 && g_ImpactTypes[impact_type].renderMode == 2;
                    }
                    else
                    {
                        draw = (g_ImpactTypes[impact_type].zbufferMode >= 2) || g_ImpactTypes[impact_type].renderMode != 2;
                    }

                    if (draw)
                    {
                        drewAny = 1;

                        if (prop != NULL)
                        {
                            if (lastmtx != g_BulletImpactBuffer[i].model_render_pos_index)
                            {
                                render_pos = &obj->model->render_pos[g_BulletImpactBuffer[i].model_render_pos_index];
                                lastmtx = g_BulletImpactBuffer[i].model_render_pos_index;
                                gSPMatrix(gdl++, osVirtualToPhysical(render_pos), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                            }
                        }
                        else
                        {
                            if (lastmtx != g_BulletImpactBuffer[i].room)
                            {
                                lastmtx = g_BulletImpactBuffer[i].room;
                                gdl = roomMtxLoad(gdl, g_BulletImpactBuffer[i].room);
                            }
                        }

                        if (lasttype != impact_type)
                        {
                            texSelect(&gdl, &g_ImagesImpact[impact_type], g_ImpactTypes[impact_type].renderMode, g_ImpactTypes[impact_type].zbufferMode, 2);
                            lasttype = impact_type;
                        }

                        gSPVertex(gdl++, osVirtualToPhysical(g_BulletImpactBuffer[i].vertex_list), 4, 0);
                        gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
                    }
                }
            }
        }
    }

    if ((prop != NULL) && (drewAny == 0))
    {
        obj->state &= ~(1 << pass);
    }

    gDPSetColorDither(gdl++, G_CD_BAYER);

    return gdl;
}





Gfx * explosionCallRenderBulletImpactOnProp(Gfx *gdl)
{
    return explosionRenderBulletImpactOnProp(gdl, NULL, 0);
}


/** Called once at boot: the scale matrix explosions are drawn with. */
void explosionInitScaleMtx(void) {
    guScale((Mtx *)&g_ExplosionScaleMtx, 0.1f, 0.1f, 0.1f);
}


void explosionsInit(void)
{
    s32 i;
    s32 j;

    g_ShakeTicks = 0;
    g_ShakeBoostTicks = 0;
    g_NextParticle = 0;
    g_NextScorch = 0;
    g_NextImpact = 0;
    g_SpExplosionDamageMult = 1.0f;
    g_ExplosionBuffer = (struct Explosion *)mempAlloc(EXPLOSION_BUFFER_LEN * sizeof(struct Explosion), MEMPOOL_STAGE);

    for (i=0; i<EXPLOSION_BUFFER_LEN; i++)
    {
        g_ExplosionBuffer[i].prop = NULL;

        for (j=0; j<EXPLOSION_PARTS_LEN; j++)
        {
            g_ExplosionBuffer[i].parts[j].frame = 0;
        }
    }
    g_SmokeBuffer = (struct Smoke *)mempAlloc(SMOKE_BUFFER_LEN * sizeof(struct Smoke), MEMPOOL_STAGE);

    for (i=0; i<SMOKE_BUFFER_LEN; i++)
    {
        g_SmokeBuffer[i].prop = NULL;

        for (j=0; j<SMOKE_PARTS_LEN; j++)
        {
            g_SmokeBuffer[i].parts[j].size = 0.0f;
        }
    }

    if (!isMpGame())
    {
        // scorches are the circle burn marks left on the ground from explosions
        g_ScorchBuffer = (struct Scorch *)mempAlloc(SCORCH_BUFFER_LEN * sizeof(struct Scorch), MEMPOOL_STAGE);

        for (i=0; i<SCORCH_BUFFER_LEN; i++)
        {
            g_ScorchBuffer[i].roomid = -1;
        }
    }
    g_BulletImpactBuffer = (struct BulletImpact *)mempAlloc(BULLET_IMPACT_BUFFER_LEN * sizeof(struct BulletImpact), MEMPOOL_STAGE);

    for (i=0; i<BULLET_IMPACT_BUFFER_LEN; i++)
    {
        g_BulletImpactBuffer[i].room = -1;
    }

    g_MaxParticles = MAX_FLYING_PARTICLES / getPlayerCount();

    if ((lvGetCurrentStage() == LEVELID_STREETS) || (lvGetCurrentStage() == LEVELID_DEPOT))
    {
        g_MaxParticles = (s32) g_MaxParticles >> 1;
    }
    g_FlyingParticlesBuffer = (struct FlyingParticles *)mempAlloc(((g_MaxParticles * sizeof(struct FlyingParticles)) + 0xF) & ~0xF, MEMPOOL_STAGE);

    for (i=0; i<g_MaxParticles; i++)
    {
        g_FlyingParticlesBuffer[i].age = 0;
    }
}


void explosionsReset(void)
{
    s32 i;
  
    g_ShakeTicks = 0;
    viShake(0.0);

    if (g_ExplosionBuffer != NULL)
    {
        for (i = 0; i<EXPLOSION_BUFFER_LEN; i++)
        {
            if (g_ExplosionBuffer[i].prop != NULL)
            {
                propDelist(g_ExplosionBuffer[i].prop);
                propDisable(g_ExplosionBuffer[i].prop);
                propFree(g_ExplosionBuffer[i].prop);
                g_ExplosionBuffer[i].prop = NULL;
            }
        }
    }

  
    if (g_SmokeBuffer != NULL)
    {
        for (i = 0; i<SMOKE_BUFFER_LEN; i++)
        {
            if (g_SmokeBuffer[i].prop != NULL)
            {
                propDelist(g_SmokeBuffer[i].prop);
                propDisable(g_SmokeBuffer[i].prop);
                propFree(g_SmokeBuffer[i].prop);
                g_SmokeBuffer[i].prop = NULL;
            }
        }
    }
}
