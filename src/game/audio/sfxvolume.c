/**
 * Volume for sound effects that come from a place in the level (a door, a guard, a
 * gunshot), worked out from the distance to the nearest player. See sfxvolume.h.
 */
#include <ultra64.h>
#include <limits.h>
#include <math.h>
#include <snd.h>
#include "game/audio/sfxvolume.h"
#include "game/player/player.h"

/* Closer than this, a sound plays at full volume */
#define SFX_FULL_VOLUME_DISTANCE 200.0f

/* The volume at fadeStart: the curve drops from SHRT_MAX to here, then falls in a straight line to 0 */
#define SFX_FADE_START_VOLUME 10000.0f

s32 sfxVolumeForDistanceRange(f32 distance, f32 fadeStart, f32 silentAt)
{
    if (distance <= SFX_FULL_VOLUME_DISTANCE)
    {
        return SHRT_MAX;
    }

    if (distance >= silentAt)
    {
        return 0;
    }

    if (distance >= fadeStart)
    {
        return (s32)(((silentAt - distance) * SFX_FADE_START_VOLUME) / (silentAt - fadeStart));
    }

    /* Falls off with the square root of the distance, from SHRT_MAX to SFX_FADE_START_VOLUME */
    return SHRT_MAX - (s32)((sqrtf(distance - SFX_FULL_VOLUME_DISTANCE) * (SHRT_MAX - SFX_FADE_START_VOLUME))
                            / sqrtf(fadeStart - SFX_FULL_VOLUME_DISTANCE));
}

s32 sfxVolumeAtRange(const coord3d *pos, f32 fadeStart, f32 silentAt)
{
    f32 nearest = silentAt;
    s32 count = getPlayerCount();

    for (s32 i = 0; i < count; i++)
    {
        const coord3d *playerpos = &g_playerPointers[i]->prop->pos;
        f32 dx = playerpos->x - pos->x;
        f32 dy = playerpos->y - pos->y;
        f32 dz = playerpos->z - pos->z;
        f32 distance = sqrtf(dx * dx + dy * dy + dz * dz);

        if (distance < nearest)
        {
            nearest = distance;
        }
    }

    return sfxVolumeForDistanceRange(nearest, fadeStart, silentAt);
}

s32 sfxVolumeForDistance(f32 distance)
{
    return sfxVolumeForDistanceRange(distance, SFX_DEFAULT_FADE_START, SFX_DEFAULT_SILENT_AT);
}

s32 sfxVolumeAt(const coord3d *pos)
{
    return sfxVolumeAtRange(pos, SFX_DEFAULT_FADE_START, SFX_DEFAULT_SILENT_AT);
}

/** Sets a playing sound's volume for where it is. */
void sfxSetVolumeAt(ALSoundState *sound, const coord3d *pos)
{
    sndCreatePostEvent(sound, AL_SNDP_VOL_EVT, sfxVolumeAt(pos));
}
