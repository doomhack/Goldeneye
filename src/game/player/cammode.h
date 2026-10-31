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
bool coopShowPartnerInCinema(void);

extern enum CAMERAMODE g_CameraMode;
extern enum CAMERAMODE g_CameraAfterCinema;
extern s32 camera_fade_active;
extern f32 camera_transition_timer;
extern s32 intro_camera_index;
extern struct SetupIntroSwirl *g_IntroSwirl;
extern struct SetupIntroCamera *g_IntroCamera;
extern enum CAMERAMODE camera_mode;
extern s32 g_IntroAnimationIndex;

extern PadRecord *g_CameraLookAtBondPad;
extern CutsceneRecord *gBondViewCutscene;
extern enum CAMERAMODE g_CutsceneCamLookAtBond;
extern s32 g_CutsceneCamUnusedFlag;
extern f32 g_OrbitCamAngle;
extern f32 g_OrbitCamSpeed;
extern f32 g_OrbitCamDist;
extern f32 g_OrbitCamHeight;
extern f32 g_OrbitCamTargetHeight;
extern s32 g_OrbitCamPad;

#endif
