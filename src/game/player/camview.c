/**
 * The current player's view: the screen rectangle and field of view, the view
 * and projection matrices, converting between view space and screen pixels, the
 * frustum planes for on-screen tests, the viewport for the screen size and watch,
 * and setting up the camera for a frame (camSetupFrame). See
 * docs/subsystems/player.md.
 */
#include <boss.h>
#include <fr.h>
#include "game/gfx/dyn.h"
#include "game/hud/blood.h"
#include "game/math/trig.h"
#include "game/menu/watchmenu.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/prop/explosion.h"
#include "game/prop/propobj.h"
#include "game/world/bg.h"
#include "game/world/roommtx.h"
#include "game/player/camview.h"
#include "game/level/gamemode.h"

s32 cameraBufferToggle = 0;

s32 cameraFrameCounter1 = 0;

s32 cameraFrameCounter2 = 0;

f32 g_CamPosScale = 1.0;

static coord3d ZeroCoordShake = { 0 };


// The frustum planes in world space, set by camUpdateFrustumPlanes
static coord3d g_CamFrustumTopNormal;
static f32 g_CamFrustumTopOffset;
static coord3d g_CamFrustumBottomNormal;
static f32 g_CamFrustumBottomOffset;
static coord3d g_CamFrustumLeftNormal;
static f32 g_CamFrustumLeftOffset;
static coord3d g_CamFrustumRightNormal;
static f32 g_CamFrustumRightOffset;
static f32 g_CamFrustumNearOffset;

void camSetScreenSize(f32 width, f32 height)
{
    g_CurrentPlayer->c_screenwidth = width;
    g_CurrentPlayer->c_screenheight = height;
    g_CurrentPlayer->c_halfwidth = width * 0.5f;
    g_CurrentPlayer->c_halfheight = height * 0.5f;
}


void camSetScreenPosition(f32 left, f32 top)
{
    g_CurrentPlayer->c_screenleft = left;
    g_CurrentPlayer->c_screentop = top;
}


void camSetPerspective(f32 fovy, f32 aspect)
{
    g_CurrentPlayer->c_perspfovy = fovy;
    g_CurrentPlayer->c_perspaspect = aspect;
}


void camUpdateScale(void)
{
	g_CurrentPlayer->c_scaley = sinf(mDegToHalfRad(g_CurrentPlayer->c_perspfovy)) / (cosf(mDegToHalfRad(g_CurrentPlayer->c_perspfovy)) * g_CurrentPlayer->c_halfheight);
	g_CurrentPlayer->c_scalex = (g_CurrentPlayer->c_scaley * g_CurrentPlayer->c_perspaspect * g_CurrentPlayer->c_halfheight) / g_CurrentPlayer->c_halfwidth;

	g_CurrentPlayer->c_recipscalex = 1.0f / g_CurrentPlayer->c_scalex;
	g_CurrentPlayer->c_recipscaley = 1.0f / g_CurrentPlayer->c_scaley;

    g_CurrentPlayer->c_scalelod = g_CurrentPlayer->c_scaley;
    g_CurrentPlayer->c_scalelod60 = sinf(DegToRad(30)) / (cosf(DegToRad(30)) * 120.0f);
	g_CurrentPlayer->c_lodscalez = g_CurrentPlayer->c_scalelod / g_CurrentPlayer->c_scalelod60;
}


/**
 * Transforms a 2D screen coordinate to a 3D world coordinate
 *
 * 'out' looks to be a vector which probably has the length 'length'
 * It starts from the middle of the screenn.
 */
void camScreenToViewDir(coord2d *in, coord3d *out, f32 length)
{
    f32 norm;
    f32 x;
    f32 y;
    f32 z;

    y = (g_CurrentPlayer->c_halfheight - (in->y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    x = ((in->x - g_CurrentPlayer->c_screenleft) - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    z = -1.0f;
    norm = length / sqrtf((x * x) + (y * y) + (z * z));
    out->x = (x * norm);
    out->y = (y * norm);
    out->z = (-1.0f * norm);
}


void camViewToScreen(coord3d *in, coord2d *out)
{
    f32 inv_z = (1.0f / in->z);
    out->y = (in->y * inv_z * g_CurrentPlayer->c_recipscaley) + (g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight);
    out->x = (g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth) - (in->x * inv_z * g_CurrentPlayer->c_recipscalex);
}


void camViewToScreenSafe(coord3d *in, coord3d *out)
{
	f32 inv_z;

	if (in->z == 0.0f)
    {
		inv_z = -100000000000000000000.0f;
	} 
    else
    {
		inv_z = 1.0f / in->z;
	}

	out->y = in->y * inv_z * g_CurrentPlayer->c_recipscaley + (g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight);
	out->x = (g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth) - in->x * inv_z * g_CurrentPlayer->c_recipscalex;
}


void camViewDeltaToScreen(coord3d *in, f32 divisor, coord3d *out)
{
	out->y = in->y * (1.0f / divisor) * g_CurrentPlayer->c_recipscaley;
	out->x = in->x * (1.0f / divisor) * g_CurrentPlayer->c_recipscalex;
}


void camSetWorldToViewMtx(Mtx *matrix) {
    g_CurrentPlayer->worldToViewMtx = matrix;
}

Mtx *camGetWorldToViewMtx(void) {
    return g_CurrentPlayer->worldToViewMtx;
}

void camSetProjectionMtx(Mtx *matrix) {
    g_CurrentPlayer->projmatrix = matrix;
}

Mtx *camGetProjectionMtx(void) {
    return g_CurrentPlayer->projmatrix;
}

void camSetProjViewMtx(Mtx *matrix) {
    g_CurrentPlayer->projViewMtx = matrix;
}

Mtx *camGetProjViewMtx(void) {
    return g_CurrentPlayer->projViewMtx;
}

/*
 * Co-op: each view's world-to-screen matrix lives in display memory, which is
 * double buffered. A player whose view isn't drawn for a while (the other
 * player has the watch or a cutscene full-screen) keeps a pointer into a
 * buffer that has since been reused, and guards checking whether that player
 * aims at them read bad floats (a crash on hardware). Keep a copy of each
 * player's matrix from the last frame their view was drawn.
 */
static s32 s_CoopFrame;
static s32 s_CoopWorldToScreenFrame[4];
static s32 s_CoopWorldToScreenSaved[4];
static Mtxf s_CoopWorldToScreen[4];

static s32 coopCurrentPlayerIndex(void)
{
    s32 i;

    for (i = 0; i < 4; i++)
    {
        if (g_playerPointers[i] == g_CurrentPlayer)
        {
            return i;
        }
    }

    return -1;
}

void camSetWorldToScreenMtxf(Mtxf *matrix) {
    g_CurrentPlayer->worldToScreenMtxf = matrix;

    if (gameIsTwoPlayerMission())
    {
        s32 player = coopCurrentPlayerIndex();

        if (player >= 0)
        {
            s_CoopWorldToScreenFrame[player] = s_CoopFrame;
        }
    }
}

Mtxf *camGetWorldToScreenMtxf(void) {
    if (gameIsTwoPlayerMission())
    {
        s32 player = coopCurrentPlayerIndex();

        // Set this frame or last frame: still in a live display buffer.
        if ((player >= 0) && s_CoopWorldToScreenSaved[player]
            && (s_CoopFrame - s_CoopWorldToScreenFrame[player] >= 2))
        {
            return &s_CoopWorldToScreen[player];
        }
    }

    return g_CurrentPlayer->worldToScreenMtxf;
}

void camSetProjectionMtxf(Mtxf *matrix) {
    g_CurrentPlayer->projmatrixf = matrix;
}

Mtxf *camGetProjectionMtxf(void) {
    return g_CurrentPlayer->projmatrixf;
}

void camSetViewToWorldMtxf(Mtxf *matrix) {
    g_CurrentPlayer->prevViewToWorldMtxf = g_CurrentPlayer->viewtoworldmtxf;
    g_CurrentPlayer->viewtoworldmtxf = matrix;
}

Mtxf *camGetViewToWorldMtxf(void) {
    return g_CurrentPlayer->viewtoworldmtxf;
}

/* Co-op: the view-to-world matrix of the last view drawn in the previous frame. */
static Mtxf s_CoopPrevViewToWorld;
static s32 s_CoopLastViewPlayer = -1;

/**
 * Co-op: called before the views of a frame are drawn. Copies the matrix of
 * the view drawn last in the previous frame, which hasn't been replaced yet.
 */
void coopViewsBegin(void)
{
    struct player *player;

    s_CoopFrame++;

    if ((s_CoopLastViewPlayer < 0) || (s_CoopLastViewPlayer >= 4))
    {
        return;
    }

    player = g_playerPointers[s_CoopLastViewPlayer];

    if ((player != NULL) && (player->viewtoworldmtxf != NULL))
    {
        mtx4Copy(player->viewtoworldmtxf, &s_CoopPrevViewToWorld);
    }
}

/**
 * Co-op: called after the views of a frame are drawn, with the player whose
 * view was drawn last.
 */
void coopViewsEnd(s32 lastplayer)
{
    s32 i;

    s_CoopLastViewPlayer = lastplayer;

    for (i = 0; i < 4; i++)
    {
        struct player *player = g_playerPointers[i];

        if ((player != NULL) && (player->worldToScreenMtxf != NULL) && (s_CoopWorldToScreenFrame[i] == s_CoopFrame))
        {
            mtx4Copy(player->worldToScreenMtxf, &s_CoopWorldToScreen[i]);
            s_CoopWorldToScreenSaved[i] = TRUE;
        }
    }
}

/**
 * The previous frame's view-to-world matrix. Chrs use it to turn their gun's
 * model matrices, which were worked out while last frame was drawn, into
 * world space.
 *
 * Co-op: chrs think during the first view of a frame, when the other
 * players' cameras haven't moved yet, so their prevViewToWorldMtxf is two frames old
 * and points into display memory that is being reused (a crash on hardware
 * once it holds bad floats). A gun's matrices are only kept as on screen when
 * the last view drawn showed it, so that view's matrix is the right one.
 */
Mtxf *camGetPrevViewToWorldMtxf(void) {
    if (gameIsTwoPlayerMission())
    {
        return &s_CoopPrevViewToWorld;
    }

    return g_CurrentPlayer->prevViewToWorldMtxf;
}

void camSetLookAt(LookAt *lookat) {
    g_CurrentPlayer->lookAt = lookat;
}

LookAt *camGetLookAt(void)
{
    return g_CurrentPlayer->lookAt;
}

f32 camGetLodScaleZ(void)
{
    return g_CurrentPlayer->c_lodscalez;
}

f32 camGetScreenWidth(void)
{
    return g_CurrentPlayer->c_screenwidth;
}

f32 camGetScreenHeight(void)
{
    return g_CurrentPlayer->c_screenheight;
}

f32 camGetScreenLeft(void)
{
    return g_CurrentPlayer->c_screenleft;
}

f32 camGetScreenTop(void)
{
    return g_CurrentPlayer->c_screentop;
}

f32 camGetPerspAspect(void)
{
    return g_CurrentPlayer->c_perspaspect;
}


/**
 * Update the world space frustum planes used for object visibility tests.
 */
void camUpdateFrustumPlanes(void)
{
    f32 h_div;
    f32 h2;
    f32 h;
    f32 nh_div;
    f32 nh2_div;
    f32 h2_div;

    h = g_CurrentPlayer->c_halfheight * g_CurrentPlayer->c_scaley;
    h_div = 1.0f / sqrtf((h * h) + 1.0f);
    h *= h_div;
    nh_div = -h_div;

    g_CamFrustumTopNormal.x = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][0]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumTopNormal.y = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][1]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumTopNormal.z = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][2]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumTopOffset = (g_CamFrustumTopNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                          + (g_CamFrustumTopNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                          + (g_CamFrustumTopNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumBottomNormal.x = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][0]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumBottomNormal.y = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][1]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumBottomNormal.z = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][2]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumBottomOffset = (g_CamFrustumBottomNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                             + (g_CamFrustumBottomNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                             + (g_CamFrustumBottomNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    h2 = (-g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    h2_div = 1.0f / sqrtf((h2 * h2) + 1.0f);
    h2 *= h2_div;
    nh2_div = -h2_div;

    g_CamFrustumLeftNormal.x = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][0]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumLeftNormal.y = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][1]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumLeftNormal.z = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][2]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumLeftOffset = (g_CamFrustumLeftNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                           + (g_CamFrustumLeftNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                           + (g_CamFrustumLeftNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumRightNormal.x = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][0]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumRightNormal.y = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][1]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumRightNormal.z = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][2]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumRightOffset = (g_CamFrustumRightNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                            + (g_CamFrustumRightNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                            + (g_CamFrustumRightNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumNearOffset = (g_CurrentPlayer->viewtoworldmtxf->m[2][0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                           + (g_CurrentPlayer->viewtoworldmtxf->m[2][1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                           + (g_CurrentPlayer->viewtoworldmtxf->m[2][2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);
}


/**
 * Check if the 3D coordinate is within the screen
 *
 * Takes dot product of some position and compares each to an associated scalar value.
 * Returns 0 if the dot product exceeds the scalar amount, 1 otherwise.
 *
 * @param pos: Applies dot product of this position against g_CurrentPlayer->viewtoworldmtxf
 * and four coords starting at g_CamFrustumLeftNormal.
 *
 * @param margin: Value added to g_CamFrustumNearOffset to compare g_CurrentPlayer->viewtoworldmtxf,
 * and the four values starting at g_CamFrustumLeftOffset.
 */
bool camIsPosInScreen(coord3d *pos, f32 margin)
{
    if (g_CamFrustumNearOffset + margin < (g_CurrentPlayer->viewtoworldmtxf->m[2][0] * pos->f[0]) + (g_CurrentPlayer->viewtoworldmtxf->m[2][1] * pos->f[1]) + (g_CurrentPlayer->viewtoworldmtxf->m[2][2] * pos->f[2]))
    {
        return FALSE;
    }

    if (g_CamFrustumLeftOffset + margin < (g_CamFrustumLeftNormal.f[0] * pos->f[0]) + (g_CamFrustumLeftNormal.f[1] * pos->f[1]) + (g_CamFrustumLeftNormal.f[2] * pos->f[2]))
    {
        return FALSE;
    }

    if (g_CamFrustumRightOffset + margin < (g_CamFrustumRightNormal.f[0] * pos->f[0]) + (g_CamFrustumRightNormal.f[1] * pos->f[1]) + (g_CamFrustumRightNormal.f[2] * pos->f[2]))
    {
        return FALSE;
    }

    if (g_CamFrustumTopOffset + margin < (g_CamFrustumTopNormal.f[0] * pos->f[0]) + (g_CamFrustumTopNormal.f[1] * pos->f[1]) + (g_CamFrustumTopNormal.f[2] * pos->f[2]))
    {
        return FALSE;
    }

    if (g_CamFrustumBottomOffset + margin < (g_CamFrustumBottomNormal.f[0] * pos->f[0]) + (g_CamFrustumBottomNormal.f[1] * pos->f[1]) + (g_CamFrustumBottomNormal.f[2] * pos->f[2]))
    {
        return FALSE;
    }

    return TRUE;
}


/**
 * Similar to the above function but checks if the 3D point is within an arbitrary box instead of the whole screen.
 * 
 * @param pos: 3D coordinate in absolute world space.
 * 
 * @param margin: is a slack in world units applied as a sphere around the point. The point is rejected only
 * if it is more than 'margin' outside a box plane.
 * 
 * @param box: screen space rectangle with 'min' being the top-left corner and 'max' the bottom-right corner.
 */
bool camIsPosInScreenBox(coord3d *pos, f32 margin, bbox2d *box)
{
    coord3d topnormal;
    f32 topoffset;
    coord3d bottomnormal;
    f32 bottomoffset;
    coord3d leftnormal;
    f32 leftoffset;
    coord3d rightnormal;
    f32 rightoffset;
    f32 leftinvlen;
    f32 xslope;
    f32 yslope;
    f32 rightinvlen;
    f32 topinvlen;
    f32 bottominvlen;
    f32 leftneginvlen;
    f32 rightneginvlen;
    f32 topneginvlen;
    f32 bottomneginvlen;

    if (g_CamFrustumNearOffset + margin < g_CurrentPlayer->viewtoworldmtxf->m[2][0] * pos->f[0] + g_CurrentPlayer->viewtoworldmtxf->m[2][1] * pos->f[1] + g_CurrentPlayer->viewtoworldmtxf->m[2][2] * pos->f[2])
    {
        return FALSE;
    }

    xslope = (box->min.x - g_CurrentPlayer->c_screenleft - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;

    leftinvlen = 1.0f / sqrtf(xslope * xslope + 1.0f);
    xslope *= leftinvlen;
    leftneginvlen = -leftinvlen;

    leftnormal.f[0] = leftneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][0] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    leftnormal.f[1] = leftneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][1] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    leftnormal.f[2] = leftneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][2] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    leftoffset = leftnormal.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + leftnormal.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + leftnormal.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    if (leftoffset + margin < leftnormal.f[0] * pos->f[0] + leftnormal.f[1] * pos->f[1] + leftnormal.f[2] * pos->f[2])
    {
        return FALSE;
    }

    xslope = -(box->max.x - g_CurrentPlayer->c_screenleft - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    rightinvlen = 1.0f / sqrtf(xslope * xslope + 1.0f);
    xslope *= rightinvlen;
    rightneginvlen = -rightinvlen;

    rightnormal.f[0] = -rightneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][0] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    rightnormal.f[1] = -rightneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][1] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    rightnormal.f[2] = -rightneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][2] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    rightoffset = rightnormal.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + rightnormal.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + rightnormal.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    if (rightoffset + margin < rightnormal.f[0] * pos->f[0] + rightnormal.f[1] * pos->f[1] + rightnormal.f[2] * pos->f[2])
    {
        return FALSE;
    }

    yslope = (g_CurrentPlayer->c_halfheight - (box->min.y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    topinvlen = 1.0f / sqrtf(yslope * yslope + 1.0f);
    yslope *= topinvlen;
    topneginvlen = -topinvlen;

    topnormal.f[0] = -topneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][0] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    topnormal.f[1] = -topneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][1] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    topnormal.f[2] = -topneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][2] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    topoffset = topnormal.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + topnormal.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + topnormal.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    if (topoffset + margin < topnormal.f[0] * pos->f[0] + topnormal.f[1] * pos->f[1] + topnormal.f[2] * pos->f[2])
    {
        return FALSE;
    }

    yslope = -(g_CurrentPlayer->c_halfheight - (box->max.y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    bottominvlen = 1.0f / sqrtf(yslope * yslope + 1.0f);
    yslope *= bottominvlen;
    bottomneginvlen = -bottominvlen;

    bottomnormal.f[0] = bottomneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][0] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    bottomnormal.f[1] = bottomneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][1] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    bottomnormal.f[2] = bottomneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][2] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    bottomoffset = bottomnormal.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + bottomnormal.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + bottomnormal.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    if (bottomoffset + margin < bottomnormal.f[0] * pos->f[0] + bottomnormal.f[1] * pos->f[1] + bottomnormal.f[2] * pos->f[2])
    {
        return FALSE;
    }

    return TRUE;
}


s16 viGetFrameWidth(void)
{
    if (cameraBufferToggle != 0)
    {
        return SCREEN_WIDTH_440;
    }

    return SCREEN_WIDTH_320;
}


s16 viGetFrameHeight(void)
{
    if (cameraBufferToggle != 0)
    {
        return SCREEN_HEIGHT_330;
    }

    return SCREEN_HEIGHT;
}

s16 playerGetViewportWidth(void)
{
    if (viewIsHalfWidth())
    {
        return VIEWPORT_WIDTH_4P;
    }

    if (cameraBufferToggle != 0)
    {
        return SCREEN_WIDTH_440;
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
    {
        return VIEWPORT_WIDTH_WIDESCREEN;
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
    {
        return VIEWPORT_WIDTH_CINEMA;
    }

    return VIEWPORT_WIDTH_FULLSCREEN;
}

s16 playerGetViewportLeft(void)
{
    if (viewIsHalfWidth())
    {
        // Counter-op: each view stays where its controller's player sits.
        if ((coopPlayerPerson(playerGetCurrentNum()) == 1) || (playerGetCurrentNum() == 3))
        {
                return 0xa1;
        }
    }

    return 0;
}


s16 playerGetViewportHeight(void)
{
    f32 t;

    // Co-op side by side views are full height: see the single view sizes.
    if ((getViewCount() >= 2) && !coopSplitSideBySide())
    {
        return VIEWPORT_HEIGHT_4P;
    }

    if (cameraBufferToggle != 0)
    {
        if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
        {
            return VIEWPORT_HEIGHT_WIDESCREEN;
        }
        else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
        {
            return VIEWPORT_HEIGHT_CINEMA;
        }
        else
        {
            return VIEWPORT_HEIGHT_FULLSCREEN;
        }
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
    {
        t = watchGetAnimFrac();
        return (s16) ((s32) (WIDESCREEN_SCALE_FACTOR * t) + VIEWPORT_OFFSET_HEIGHT_WIDESCREEN);
    }
    else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
    {
        t = watchGetAnimFrac();
        return (s16) ((s32) (CINEMA_SCALE_FACTOR * t) + VIEWPORT_OFFSET_HEIGHT_CINEMA);
    }
    else
    {
        return VIEWPORT_HEIGHT_DEFAULT;
    }
}


s16 playerGetViewportTop(void)
{
    f32 t;

    if ((getViewCount() == 2) && !coopSplitSideBySide())
    {
        if (coopPlayerPerson(playerGetCurrentNum()) == 0)
        {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_2P_PLAYER_1;
#endif
        }

        return VIEWPORT_ULY_2P_PLAYER_2;
    }

    if (getViewCount() >= 3)
    {
        if (playerGetCurrentNum() < 2)
        {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_4P_PLAYER_12;
#endif
        }

        return VIEWPORT_ULY_4P_PLAYER_34;
    }

    if (cameraBufferToggle != 0)
    {
        if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
        {
            return VIEWPORT_ULY_CAM_WIDESCREEN;
        }
        else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
        {
            return VIEWPORT_ULY_CAM_CINEMA;
        }
        else
        {
            return VIEWPORT_ULY_CAM_FULLSCREEN;
        }
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
    {
        t = watchGetAnimFrac();
        return (s16) ((s32) (WIDESCREEN_ULY_SCALE_FACTOR * t) + VIEWPORT_ULY_WIDESCREEN_OFFSET);
    }
    else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
    {
        t = watchGetAnimFrac();
        return (s16) ((s32) (CINEMA_ULY_SCALE_FACTOR * t) + VIEWPORT_ULY_CINEMA_OFFSET);
    }
    else
    {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_DEFAULT;
#endif
    }
}


/**
 * Makes room's position the origin for drawing, so the fixed-point matrices keep
 * their precision.
 */
void camSetRoomOrigin(s32 room)
{
    roomGetWorldPos(room, &g_CurrentPlayer->current_model_pos);
    roomMtxSetOriginRoom(room);
}


void camStorePrevOrigin(void) {
    g_CurrentPlayer->previous_model_pos.f[0] = g_CurrentPlayer->current_model_pos.f[0];
    g_CurrentPlayer->previous_model_pos.f[1] = g_CurrentPlayer->current_model_pos.f[1];
    g_CurrentPlayer->previous_model_pos.f[2] = g_CurrentPlayer->current_model_pos.f[2];
    mtx4RotateVecInPlace(camGetWorldToScreenMtxf(), &g_CurrentPlayer->previous_model_pos);
}


/**
 * Builds the frame's view matrices from the camera's position, look direction and
 * up vector, relative to the room origin, then the frustum planes.
 */
void camUpdateMatrices(coord3d* cam_pos, coord3d* cam_look_dir, coord3d* cam_up)
{
    Mtx lookatmtx;
    LookAt *lookat;
    Mtxf lookmtx;
    coord3d clpos;
    coord3d scaledpos;
    f32 scale;
    Mtx *projview;
    Mtxf *projmtx;
    Mtxf projviewf;
    Mtx *worldtoview;
    Mtxf *worldtoscreen;
    Mtxf *viewtoworld;
    s32 j;
    s32 i;

    i = playerGetRoom();
    camSetRoomOrigin(i);

    worldtoview = dynAllocateMatrix();
    worldtoscreen = (Mtxf *) dynAllocateMatrix();
    viewtoworld = (Mtxf *) dynAllocateMatrix();

    lookat = (LookAt *)dynAllocateLights(2); // a LookAt is two Lights

    scale = g_CamPosScale;

    scaledpos.x = (cam_pos->x - g_CurrentPlayer->current_model_pos.x) * scale;
    scaledpos.y = (cam_pos->y - g_CurrentPlayer->current_model_pos.y) * scale;
    scaledpos.z = (cam_pos->z - g_CurrentPlayer->current_model_pos.z) * scale;

    clpos.f[0] = scaledpos.f[0] + cam_look_dir->f[0];
    clpos.f[1] = scaledpos.f[1] + cam_look_dir->f[1];
    clpos.f[2] = scaledpos.f[2] + cam_look_dir->f[2];

    mtx4LoadLookDir(&lookmtx,
        scaledpos.x, scaledpos.y, scaledpos.z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    guLookAtReflect(&lookatmtx, lookat,
        scaledpos.x, scaledpos.y, scaledpos.z,
        clpos.x, clpos.y, clpos.z,
        cam_up->x, cam_up->y, cam_up->z);

    mtx4LoadLookDir(worldtoscreen,
        cam_pos->x, cam_pos->y, cam_pos->z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    mtx4LoadFacingDir(viewtoworld,
        cam_pos->x, cam_pos->y, cam_pos->z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    projview = dynAllocateMatrix();

    projmtx = camGetProjectionMtxf();
    mtx4Mult(projmtx, &lookmtx, &projviewf);

	for (i = 0; i < 4; i++)
    {
		for (j = 0; j < 4; j++)
        {
			if (projviewf.m[i][j] > 32000.0f)
            {
				projviewf.m[i][j] = 32000.0f;
			}
            else if (projviewf.m[i][j] < -32000.0f)
            {
				projviewf.m[i][j] = -32000.0f;
			}
		}
	}

    guMtxF2L((f32 (*)[4]) &projviewf, projview);
    camSetProjViewMtx(projview);

    scale = bgGetLevelVisibilityScale();

    mtx4ScaleRows012(scale, &lookmtx);
    guMtxF2L((f32 (*)[4]) &lookmtx, worldtoview);

    camSetWorldToViewMtx(worldtoview);
    camSetWorldToScreenMtxf(worldtoscreen);
    camSetViewToWorldMtxf(viewtoworld);

    camSetLookAt(lookat);
    camUpdateFrustumPlanes();
    camStorePrevOrigin();
}


/**
 * Sets up the current player's camera for this frame: the camera position
 * (Bond's eyes with explosion shake, or the frozen camera when cameramode is 1),
 * the view matrices (camUpdateMatrices), and the body's aim pitch and yaw. PAL
 * also clears the letterbox bars on Cuba.
 */
Gfx *camSetupFrame(Gfx *gdl)
{
    coord3d cam_pos;
    coord3d cam_look;
    coord3d cam_up;
    struct PlayerBond *collision;
    coord3d shake;
    coord3d vec;
    coord3d zeropos;
    f32 vec_y;
    f32 horizontal_len;
    f32 angle;
    f32 vertical_rot;
    f32 ft4;

#if defined(VERSION_EU)
    if (bossGetStageNum() == LEVELID_CUBA)
    {
        if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
        {
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
        }
        else if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
        {
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
        }
        else
        {
            gdl = screenClearBlack(gdl);
            gdl = screenClearBlack(gdl);
        }
    }
#endif

    if (g_CurrentPlayer->cameramode == 1) {
        cam_pos.x = g_CurrentPlayer->pos.x;
        cam_pos.y = g_CurrentPlayer->pos.y;
        cam_pos.z = g_CurrentPlayer->pos.z;

        cam_look.x = g_CurrentPlayer->pos2.x - g_CurrentPlayer->pos.x;
        cam_look.y = g_CurrentPlayer->pos2.y - g_CurrentPlayer->pos.y;
        cam_look.z = g_CurrentPlayer->pos2.z - g_CurrentPlayer->pos.z;

        cam_up.x = g_CurrentPlayer->offset.x;
        cam_up.y = g_CurrentPlayer->offset.y;
        cam_up.z = g_CurrentPlayer->offset.z;
    } else {
        collision = &g_CurrentPlayer->bond;

        shake = ZeroCoordShake;

        if (!g_CurrentPlayer->bonddead) {
            explosionScreenShake(
                &collision->pos,
                &collision->applied_view,
                &shake
            );
        } else {
            viShake(0.0f);
        }

        cam_pos.x = collision->pos.x;
        cam_pos.y = collision->pos.y;
        cam_pos.z = collision->pos.z;

        cam_look.x = collision->applied_view.x;
        cam_look.y = collision->applied_view.y;
        cam_look.z = collision->applied_view.z;

        cam_up.x = collision->applied_view2.x;
        cam_up.y = collision->applied_view2.y;
        cam_up.z = collision->applied_view2.z;
    }

    camUpdateMatrices(&cam_pos, &cam_look, &cam_up);
    gunGetAimRay(&zeropos, &vec);


    vec_y = vec.y;
    horizontal_len = sqrtf((vec.z * vec.z) + (vec.x * vec.x));
    vertical_rot = bondviewGetPlayerPitchRadians();
    ft4 = mathAtan2(vec_y, horizontal_len) + vertical_rot;

    if (ft4 >= M_PI_F) {
        ft4 -= M_TAU_F;
    }
    g_CurrentPlayer->bodyAimPitch = ft4;

    angle = mathAtan2(-vec.x, -vec.z);
    if (angle >= M_PI_F) {
        angle -= M_TAU_F;
    }
    g_CurrentPlayer->bodyAimYaw = angle;

    return gdl;
}


void camSetPosScale(f32 scale)
{
  g_CamPosScale = scale;
}


Gfx* camLoadWorldToViewMtx(Gfx* gdl)
{
    gSPMatrix(gdl++, camGetWorldToViewMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    return gdl;
}


void mtxsF2L(RenderPosView * views, s32 count)
{
    Mtxf mtx;
    s32 i;

    for (i = 0; i < count; i++)
    {
        mtx4Copy(&views[i].pos, &mtx);
        mtxF2L(&mtx, &views[i].pos);
    }
}


void camMtxsToRoomSpace(Mtxf *matrices, s32 count)
{
    Mtxf mtx;
    s32 i;
    s32 j;

    for (i = 0, j = 0; i < count; i++, j += sizeof(Mtxf))
    {
        mtx4MultAffine(camGetViewToWorldMtxf(), (Mtxf *)((u32)matrices + j), &mtx);

        mtx.m[3][0] -= g_CurrentPlayer->current_model_pos.f[0];
        mtx.m[3][1] -= g_CurrentPlayer->current_model_pos.f[1];
        mtx.m[3][2] -= g_CurrentPlayer->current_model_pos.f[2];

        mtxF2L(&mtx, matrices + i);
    }
}
