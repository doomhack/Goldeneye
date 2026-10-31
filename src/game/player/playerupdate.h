#ifndef _PLAYERUPDATE_H_
#define _PLAYERUPDATE_H_
#include <ultra64.h>
#include <bondtypes.h>

extern s32 g_bondviewForceDisarm;
extern s32 resolution;
extern f32 g_PlayerWorldScale;
extern coord3d g_ForceBondMoveOffset;
extern s32 g_CoopForceMovePlayer;
extern s32 mission_timer;
#if defined(VERSION_EU)
extern f32 watch_time_0;
#else
extern s32 watch_time_0;
#endif
void playerUpdate(s8 stick_x, s8 stick_y, u16 buttons);
s32 lvGetMissionTime60(void);
f32 playerGetFloorY(struct StandTile *tile, f32 x, f32 z);
void playerUpdateVertical(s32 use_stanHeight, f32 stanHeight_offset);
void playerSetWorldScale(f32);
void playerMoveFrozen(u16, u16);
void playerMove(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons);
void playerUpdatePosFromCollision(void);
void playerUpdateVertaTheta(void);
void playerTickMoveAnim(f32 speed, f32 speedforwards, f32 speedsideways);

#endif
