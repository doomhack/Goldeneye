#ifndef _TANK_H_
#define _TANK_H_
#include <ultra64.h>
#include <bondtypes.h>

/**
 * The tank's state while a player drives it. All angles are radians unless
 * named Deg. The *Sum fields are running sums that smooth turning: each frame
 * sum = sum * scale + target, and the angle used is sum * (1 - scale).
 */
struct Tank
{
    ALSoundState *sfx[2];   // [0] the engine, [1] the tracks while moving
    f32 turnSpeed;          // body turn speed, -3.75 to 3.75
    f32 yaw;                // body facing
    f32 turretPitch;
    f32 turretPitchSum;
    f32 turretYaw;          // turret facing in the world
    f32 turretYawSum;
    f32 turretTurnSpeed;
    f32 turretAngle;        // where the stick is turning the turret to
    f32 turretTurn;         // change to turretAngle this frame
    s32 damagePenaltyTicks; // ticks of half speed after the tank smashes a prop
    coord3d turretOffset;   // where the turret sits on the body (tankUpdateTurretOffset)
    s32 engineVolume;

    /* Climbing in: the camera moves from where the player stood to the seat */
    s32 enterAudioState;    // 0 begin, 1 seated (queue the sound), 2 done
    f32 enterProgress;      // 0 to 1 over 45 ticks
    f32 enterRemain;        // eased 1 to 0
    f32 enterYawDeg;        // the player's look angles when they climbed in
    f32 enterPitchDeg;
    coord3d enterPos;       // where the player stood
};

extern struct Tank g_Tank;

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

extern s32 g_PlayerIsInTank;
extern struct PropRecord *g_WorldTankProp;
extern struct PropRecord *g_PlayerTankProp;
extern f32 g_PlayerTankYOffset;
extern s32 g_BondCanEnterTank;
extern s32 g_ExplodeTankOnDeathFlag;

#endif
