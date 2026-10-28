#ifndef _LEVELMUSIC_H_
#define _LEVELMUSIC_H_
#include <ultra64.h>

#include <bondconstants.h>

void             levelMusicResetSlots(void);
void             levelMusicStart(s32 stagenum);
void             levelMusicSetState(MUSICSTATE state);
MUSICSTATE levelMusicGetState(void);
void             levelMusicTick(void);
void             levelMusicSlotStop(s32 slot);
void             levelMusicSlotStart(s32 slot, s32 min, s32 sec);
void levelMusicResume(void);
u16 levelMusicGetVolume(void);
s32 levelMusicGetAmbientTrack(s32 stageID);
s32 levelMusicGetXTrack(s32 stageID);
s32 levelMusicGetTrack(s32 stage);
void levelStopTankSfx(void);

void levelMusicStop(void);
void levelMusicPause(void);

#endif
