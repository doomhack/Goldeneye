#ifndef _PLAYERLIFE_H_
#define _PLAYERLIFE_H_
#include <ultra64.h>
#include <bondtypes.h>

void playerResetLife(void);
void playerStartNewLife(void);
s32 playerChooseSpawnPad(void);
s32 currentPlayerGetCrouchPos(void);
s32 playerGetCrouchPos(s32 playernum);
void playerSetPosition(coord3d *pos, coord3d *pos2, coord3d *offset, StandTile *tile, coord3d *stan_walk_start);
void playerSetLeanTarget(s32 value);
void playerAdjustCrouch(s32 value);

extern s32 startpadcount;
extern PadRecord *g_Startpad[];

extern ITEM_IDS starting_weapon[2];

#endif
