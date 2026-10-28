/**
 * Simple physics for falling and thrown things: gravity, and a random launch and tumble
 * for thrown grenades, mines and other projectiles.
 *
 * Times are in game ticks (60 per second), and gravity is 1/3.6 units per tick per tick.
 */
#include <ultra64.h>
#include "math.h"
#include "bondconstants.h"
#include "bondtypes.h"
#include "random.h"
#include "game/math/matrix.h"
#include "game/math/physics.h"

coord3d g_ZeroCoord = { 0, 0, 0 };

/** Loads a small random rotation, up to about 0.12 radians around each axis. */
void physRandomTumble(Mtxf *mtx)
{
    coord3d angles;
    angles = g_ZeroCoord;
#if defined(VERSION_US)
    angles.x = RANDOMFRAC() * M_TAU_F * 0.03125f - 0.09817477f;
    angles.y = RANDOMFRAC() * M_TAU_F * 0.03125f - 0.09817477f;
    angles.z = RANDOMFRAC() * M_TAU_F * 0.03125f - 0.09817477f;
#else
    angles.x = RANDOMFRAC() * 7.53982257f * 0.03125f - 0.11780973f;
    angles.y = RANDOMFRAC() * 7.53982257f * 0.03125f - 0.11780973f;
    angles.z = RANDOMFRAC() * 7.53982257f * 0.03125f - 0.11780973f;
#endif
    mtx4LoadRotation(&angles, mtx);
}

/**
 * Picks a random launch velocity (sideways up to 3.3 units per tick either way, upwards
 * up to 6.7) and a random tumble.
 */
void physRandomLaunch(coord3d *velocity, Mtxf *tumble)
{
    velocity->x = RANDOMFRAC() * 1.6666666f * 4.0f - 3.3333333f;
    velocity->y = RANDOMFRAC() * 1.6666666f * 4.0f;
    velocity->z = RANDOMFRAC() * 1.6666666f * 4.0f - 3.3333333f;

    physRandomTumble(tumble);
}

/**
 * Moves a height and a vertical speed on by `ticks` under gravity, using the average of
 * the old and new speed.
 */
void physFall(f32 *height, f32 *speed, f32 ticks)
{
    f32 oldspeed = *speed;
    f32 newspeed = oldspeed - (ticks * 0.2777778f);

    *height += (ticks * (oldspeed + newspeed)) * 0.5f;
    *speed = newspeed;
}

/** Moves a position and velocity on by `ticks`: straight sideways, falling vertically. */
void physMoveWithGravity(coord3d *pos, coord3d *velocity, f32 ticks)
{
    physFall(&pos->y, &velocity->y, ticks);
    pos->x += ticks * velocity->x;
    pos->z += ticks * velocity->z;
}

/** Applies a per-tick rotation `ticks` times. */
void physApplyTumble(Mtxf *mtx, Mtxf *tumble, s32 ticks)
{
    s32 i;

    for (i = 0; i < ticks; i++)
    {
        mtx4MultAffineInPlace(tumble, mtx);
    }
}

