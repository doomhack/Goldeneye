#ifndef _PLAYERMOVE_H_
#define _PLAYERMOVE_H_
#include <ultra64.h>
#include <bondtypes.h>

// ducking_height_offset when fully crouched
#define FULL_CROUCH_OFFSET -100.0f

void playerResetCollision(struct PlayerBond *col, coord3d *pos, StandTile *stan);
s32  playerTestMove(coord3d *pos, StandTile **stan);
void playerMoveWithCollision(coord3d *offset, s32 allow_scoot);
void playerRemoveFromRooms(struct player *player);
void playerUpdateRooms(struct player *player);

#endif
