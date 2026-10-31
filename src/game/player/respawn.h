#ifndef _RESPAWN_H_
#define _RESPAWN_H_
#include <ultra64.h>
#include <bondtypes.h>

// Co-op: when a dead player can respawn (A, Z or Start once this has passed).
#define COOP_RESPAWN_DELAY60 (10 * 60)

extern s32 g_CoopDeathTime[4];

void mpRespawnPlayer(void);
s32  coopDiedLast(void);
void coopRespawnPlayer(void);

// Puts the current player at a position and resets them for a new life (also counter-op)
void coopPlaceCurrentPlayer(coord3d *posp, StandTile *stan, f32 look_angle);

extern s32 g_CoopRespawnButtonsHeld[4];

#endif
