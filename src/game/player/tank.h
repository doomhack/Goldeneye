#ifndef _TANK_H_
#define _TANK_H_
#include <ultra64.h>
#include <bondtypes.h>

void tankUpdateTurretOffset(void);
void tankGetCollisionBounds(coord2d *tank_collision_bounds, coord3d *collision_position, f32 tank_orientation_angle);
s32  tankTestCollision(coord3d *collision_position, StandTile *starttile, f32 tank_orientation_angle, coord3d *hitFrom, coord3d *hitTo);
s32  tankTestCollisionAt(coord3d *collision_position, StandTile *tile, f32 tank_orientation_angle);
s32  tankTryMoveTo(coord3d *pos, f32 angle, StandTile **outTile);
void tankGetTurretDir(coord3d *dir);

bool playerIsInTank(void);
PropRecord *playerGetTank(void);

// Co-op: the player using the tank, or -1
extern s32 g_CoopTankPlayer;

s32  coopTankIsHidden(void);
s32  coopTankUsedByPartner(void);
void coopTankBeginPlayer(void);
void coopTankEndPlayer(s32 moved);

#endif
