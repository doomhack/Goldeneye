#ifndef _PLAYERMOVE_H_
#define _PLAYERMOVE_H_
#include <ultra64.h>
#include <bondtypes.h>

// ducking_height_offset when fully crouched
#define FULL_CROUCH_OFFSET -100.0f

void playerResetCollision(struct PlayerBond *col, coord3d *pos, StandTile *stan);
s32  playerTestMove(const coord3d *pos, StandTile **stan);
void playerMoveWithCollision(const coord3d *offset, s32 allow_scoot);
void playerRemoveFromRooms(struct player *player);
void playerUpdateRooms(struct player *player);

extern s32 g_PlayerCollisionOn;

void playerUpdateCollisionBounds(void);
void playerGetCollisionSize(const PropRecord* prop, f32 *collision_radius, f32 *height, f32 *always_30);
void playerSetMoving(const PropRecord *prop, s32 flag);
void playerGetPropBounds(const PropRecord *prop, coord2d **field_B0, s32 *numpoints, f32 *height_related, f32 *collision);

#endif
