#ifndef _CAMMODE_H_
#define _CAMMODE_H_
#include <ultra64.h>
#include <bondtypes.h>

// Camera modes: first person, the level intro (swirl and intro cameras),
// cutscenes, the death camera, and in co-op which view fills the screen

// Co-op: the player whose view fills the screen for a cinema camera mode
extern s32 g_CoopCinemaPlayer;

u32 playerGetCameraMode(void);
s32 playerPickDeathCamera(PropRecord *prop1, coord3d *pos, PropRecord *prop2, coord3d *collision_pos, StandTile *tile, f32 camera_dist);
void coopUpdateSoloView(void);
void playerSetCameraMode(s32 mode);
void playerAdvanceCameraMode(void);
void playerCalcIntroSwirlCamera(s32 index, f32 time, coord3d *pos, coord3d *lookat);
void playerTickFrozenCamera(u16 buttons, u16 oldbuttons, coord3d *pos, coord3d *pos2, StandTile **stan, coord3d *lookat);
void playerSetCameraModeRaw(s32 mode);

#endif
