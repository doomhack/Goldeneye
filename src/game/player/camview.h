#ifndef _CAMVIEW_H_
#define _CAMVIEW_H_
#include <ultra64.h>
#include <bondtypes.h>

// The view: screen rectangle, field of view, view matrices, frustum tests,
// the viewports and setting up the camera for a frame

void camSetScreenSize(f32 width, f32 height);
void camSetScreenPosition(f32 left, f32 top);
void camSetPerspective(f32 fovy, f32 aspect);
void camUpdateScale(void);
void camScreenToViewDir(coord2d *in, coord3d *out, f32 length);
void camViewToScreen(coord3d *in, coord2d *out);
void camViewToScreenSafe(coord3d *in, coord3d *out);
void camViewDeltaToScreen(coord3d *in, f32 divisor, coord3d *out);
void camSetWorldToViewMtx(Mtx *matrix);
Mtx *camGetWorldToViewMtx(void);
void camSetProjectionMtx(Mtx *matrix);
Mtx *camGetProjectionMtx(void);
void camSetProjViewMtx(Mtx *matrix);
Mtx *camGetProjViewMtx(void);
void camSetWorldToScreenMtxf(Mtxf *matrix);
Mtxf *camGetWorldToScreenMtxf(void);
void camSetProjectionMtxf(Mtxf *matrix);
Mtxf *camGetProjectionMtxf(void);
void camSetViewToWorldMtxf(Mtxf *matrix);
Mtxf *camGetViewToWorldMtxf(void);
void coopViewsBegin(void);
void coopViewsEnd(s32 lastplayer);
Mtxf *camGetPrevViewToWorldMtxf(void);
void camSetLookAt(LookAt *lookat);
LookAt *camGetLookAt(void);
f32 camGetLodScaleZ(void);
f32 camGetScreenWidth(void);
f32 camGetScreenHeight(void);
f32 camGetScreenLeft(void);
f32 camGetScreenTop(void);
f32 camGetPerspAspect(void);
void camUpdateFrustumPlanes(void);
bool camIsPosInScreen(coord3d *pos, f32 margin);
bool camIsPosInScreenBox(coord3d *pos, f32 margin, bbox2d *box);
s16 viGetFrameWidth(void);
s16 viGetFrameHeight(void);
s16 playerGetViewportWidth(void);
s16 playerGetViewportLeft(void);
s16 playerGetViewportHeight(void);
s16 playerGetViewportTop(void);
void camSetRoomOrigin(s32 room);
void camStorePrevOrigin(void);
void camUpdateMatrices(coord3d*cam_pos, coord3d*cam_look_dir, coord3d*cam_up);
Gfx *camSetupFrame(Gfx *gdl);
void camSetPosScale(f32 scale);
Gfx*camLoadWorldToViewMtx(Gfx*gdl);
void mtxsF2L(RenderPosView *views, s32 count);
void camMtxsToRoomSpace(Mtxf *matrices, s32 count);

#endif
