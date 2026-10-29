#ifndef _SFXVOLUME_H_
#define _SFXVOLUME_H_

#include <ultra64.h>
#include <bondtypes.h>
#include <snd.h>

/*
 * Volume for a sound effect heard from somewhere in the level: full volume up to 2 metres
 * from the nearest player, then fading out until it is silent at silentAt. Most sounds
 * use the default range, which fades to a third of full volume at 50 metres and is silent
 * at 60 metres.
 */
#define SFX_DEFAULT_FADE_START 5000.0f
#define SFX_DEFAULT_SILENT_AT 6000.0f

s32 sfxVolumeForDistanceRange(f32 distance, f32 fadeStart, f32 silentAt);
s32 sfxVolumeAtRange(const coord3d *pos, f32 fadeStart, f32 silentAt);

s32 sfxVolumeForDistance(f32 distance);
s32 sfxVolumeAt(const coord3d *pos);
void sfxSetVolumeAt(ALSoundState *sound, const coord3d *pos);

#endif
