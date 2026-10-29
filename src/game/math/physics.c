/**
 * Simple physics for falling and thrown things: gravity, and a random launch and tumble
 * for thrown grenades, mines and other projectiles.
 *
 * Times are in game ticks (60 per second), and gravity is 1/3.6 units per tick per tick.
 */
#include <ultra64.h>
#include <math.h>
#include "random.h"
#include "game/math/matrix.h"
#include "game/math/physics.h"

/** A random fraction in [0, 1). */
static f32 physRandomFrac(void)
{
    return (f32) randomGetNext() * 2.3283064e-10f;
}

/**
 * Loads a small random rotation per tick, up to about 0.1 radians either way around each
 * axis. PAL's ticks are longer (50 per second), so it turns 1.2 times as far per tick.
 */
void physRandomTumble(Mtxf *tumble)
{
    coord3d angles;

#if defined(VERSION_US)
    angles.x = physRandomFrac() * M_TAU_F * 0.03125f - 0.09817477f;
    angles.y = physRandomFrac() * M_TAU_F * 0.03125f - 0.09817477f;
    angles.z = physRandomFrac() * M_TAU_F * 0.03125f - 0.09817477f;
#else
    angles.x = physRandomFrac() * 7.53982257f * 0.03125f - 0.11780973f;
    angles.y = physRandomFrac() * 7.53982257f * 0.03125f - 0.11780973f;
    angles.z = physRandomFrac() * 7.53982257f * 0.03125f - 0.11780973f;
#endif
    mtx4LoadRotation(&angles, tumble);
}

/**
 * Picks a random launch velocity (sideways up to 3.3 units per tick either way, upwards
 * up to 6.7) and a random tumble.
 */
void physRandomLaunch(coord3d *velocity, Mtxf *tumble)
{
    velocity->x = physRandomFrac() * 1.6666666f * 4.0f - 3.3333333f;
    velocity->y = physRandomFrac() * 1.6666666f * 4.0f;
    velocity->z = physRandomFrac() * 1.6666666f * 4.0f - 3.3333333f;

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
void physApplyTumble(Mtxf *mtx, const Mtxf *tumble, s32 ticks)
{
    for (s32 i = 0; i < ticks; i++)
    {
        mtx4MultAffineInPlace(tumble, mtx);
    }
}

