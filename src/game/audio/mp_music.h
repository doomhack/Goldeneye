#ifndef _MP_MUSIC_H_
#define _MP_MUSIC_H_
#include <ultra64.h>

#include <bondconstants.h>

void             levelMusicResetSlots(void);
void             levelMusicStart(s32 stagenum);
void             levelMusicSetState(MUSICSTATE arg0);
MUSICSTATE levelMusicGetState(void);
void             levelMusicTick(void);
void             levelMusicSlotStop(s32 slot);
void             levelMusicSlotStart(s32 slot, s32 min, s32 sec);
void levelMusicResume(void);
u16 levelMusicGetVolume(void);

#endif
