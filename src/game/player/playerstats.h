#ifndef _PLAYERSTATS_H_
#define _PLAYERSTATS_H_
#include <ultra64.h>
#include <bondtypes.h>

// Per-player stats for the end screen: hits by body part, kills and deaths

void statsAddHit(ITEM_IDS item, SHOT_REGISTER shot_register);
s32 statsGetHits(SHOT_REGISTER shot_register);
void statsAddCivilianKill(void);
s32 statsGetCivilianKills(void);
void statsAddKill(void);
s32 statsGetKills(void);
void statsAddGoldenGunKill(void);
void statsAddDeath(void);
void statsAddSuicide(void);

#endif
