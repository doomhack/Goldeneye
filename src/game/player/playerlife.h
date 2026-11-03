#ifndef _PLAYERLIFE_H_
#define _PLAYERLIFE_H_
#include <ultra64.h>
#include <bondtypes.h>

void playerResetLife(void);
void playerStartNewLife(void);
s32 playerChooseSpawnPad(void);
s32 currentPlayerGetCrouchPos(void);
s32 playerGetCrouchPos(s32 playernum);
void playerSetPosition(const coord3d *pos, const coord3d *pos2, const coord3d *offset, StandTile *tile, const coord3d *stan_walk_start);
void playerSetLeanTarget(s32 value);
void playerAdjustCrouch(s32 value);

extern s32 startpadcount;
extern PadRecord *g_Startpad[];

extern ITEM_IDS starting_weapon[2];

#endif
