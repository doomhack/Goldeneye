#ifndef _COOP_H_
#define _COOP_H_
#include <ultra64.h>

// Counter-op: Bond is always player index 0, the guard player index 1
#define COUNTEROP_BOND  0
#define COUNTEROP_GUARD 1

extern s32 g_CoopMode;
extern s32 g_CounterOpMode;
extern s32 g_CounterOpBondPad;
extern s32 g_CoopPlayersOverlap;
extern s32 g_CoopSoloView;
extern s32 g_CoopSoloViewIsCinema;
extern s32 g_CoopLastActivator;

s32 coopPlayerPerson(s32 player);
s32 counterOpIsGuardPlayer(s32 player);
s32 coopPartnerCanRevive(void);
void coopNoteActivator(void);
s32 coopPartnerAlive(void);
s32 coopSplitSideBySide(void);

#endif
