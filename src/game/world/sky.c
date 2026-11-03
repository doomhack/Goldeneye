/**
 * The sky, drawn first each frame: a clear to the environment colour, or a textured cloud
 * plane above the level and, below the horizon, a water plane or a flat fill. The planes'
 * screen-edge crossings are worked out on the CPU and drawn as RDP triangles.
 */

#include <ultra64.h>
#include <memp.h>
#include "game/world/sky.h"
#include "game/player/player.h"
#include "game/world/water.h"
#include "game/level/lv.h"
#include "game/world/bgfog.h"
#include "game/world/bg.h"
#include "game/gfx/texcmds.h"
#include "fr.h"
#include "game/gfx/imagebank.h"
#include "game/level/gamemode.h"
#include "game/gfx/dyn.h"
#include "game/math/matrix.h"

#define SKYABS(val) (val >= 0.0f ? (val) : -(val))

// bss
s32 g_SkyStageNum;

Mtxf g_SkyInvScaleMtx;


/**
 * offset_x/offset_y are relative to the current player's view. (Stock GE
 * passed some absolute positions here too, which put the sky's horizon
 * edges in the wrong place in every split-screen view but the top-left one.)
 */
void skyGetWorldPosFromScreenPos(f32 offset_x, f32 offset_y, coord3d* out) {
    const Mtxf* player_mtxf;
    coord2d coords;
    f32 screen_top;

    player_mtxf = camGetViewToWorldMtxf();
    coords.x = camGetScreenLeft() + offset_x;
    screen_top = camGetScreenTop();
    coords.y = fogGetCurrentEnvironmentp()->WaterConcavity + (offset_y + screen_top);
    camScreenToViewDir(&coords, out, 100.0f);
    mtx4RotateVecInPlace(player_mtxf, out);
}

bool skyIsScreenCornerInSky(const coord3d *corner3dpos, coord3d *dstpos, f32 *dstfrac)
{
    const coord3d *eye = playerGetPos();
    f32 elevation = 2.0f * corner3dpos->y / sqrtf(corner3dpos->f[0] * corner3dpos->f[0] + corner3dpos->f[2] * corner3dpos->f[2] + 0.0001f);
    f32 dist;
    f32 horizDist;
    f32 dirY;

    if (elevation > 1.0f)
    {
        elevation = 1.0f;
    }

    *dstfrac = 1.0f - elevation;

    if (corner3dpos->y == 0.0f)
    {
        dirY = 0.01f;
    }
    else
    {
        dirY = corner3dpos->y;
    }

    if (dirY > 0.0f)
    {
        dist = (fogGetCurrentEnvironmentp()->CloudRepeat - eye->y) / dirY;
        horizDist = sqrtf(corner3dpos->f[0] * corner3dpos->f[0] + corner3dpos->f[2] * corner3dpos->f[2]) * dist;

        if (horizDist > 300000)
        {
            dist *= 300000 / horizDist;
        }

        dstpos->x = eye->x + dist * corner3dpos->f[0];
        dstpos->y = eye->y + dist * dirY;
        dstpos->z = eye->z + dist * corner3dpos->f[2];

        return TRUE;
    }

    return FALSE;
}


bool skyIsCornerInWater(const coord3d *corner3dpos, coord3d *dstpos, f32 *dstfrac)
{
    const coord3d *eye = playerGetPos();
    f32 elevation = -2.0f * corner3dpos->y / sqrtf(corner3dpos->f[0] * corner3dpos->f[0] + corner3dpos->f[2] * corner3dpos->f[2] + 0.0001f);
    f32 dist;
    f32 horizDist;
    f32 dirY;

    if (elevation > 1.0f)
    {
        elevation = 1.0f;
    }

    *dstfrac = 1.0f - elevation;

    if (corner3dpos->y == 0.0f)
    {
        dirY = -0.01f;
    }
    else
    {
        dirY = corner3dpos->y;
    }

    if (dirY < 0.0f)
    {
        dist = (fogGetCurrentEnvironmentp()->WaterRepeat - eye->y) / dirY;
        horizDist = sqrtf(corner3dpos->f[0] * corner3dpos->f[0] + corner3dpos->f[2] * corner3dpos->f[2]) * dist;

        if (horizDist > 300000)
        {
            dist *= 300000 / horizDist;
        }

        dstpos->x = eye->x + dist * corner3dpos->f[0];
        dstpos->y = eye->y + dist * dirY;
        dstpos->z = eye->z + dist * corner3dpos->f[2];

        return TRUE;
    }

    return FALSE;
}

void skyCalculateEdgeVertex(const coord3d *base, const coord3d* ref, coord3d* out)
{
    f32 mult;

    mult = base->y / (base->y - ref->y);
    out->x = ((ref->x - base->x) * mult) + base->x;
    out->y = 0.0f;
    out->z = ((ref->z - base->z) * mult) + base->z;
}

f32 skyClamp(f32 a, f32 b, f32 c)
{
    if (a < b)
    {
        return b;
    }

    if (c < a)
    {
        return c;
    }

    return a;
}


f32 skyRound(f32 value)
{
    return (f32) (s32) (value + 0.5f);
}


void skyChooseCloudVtxColour(SkyVtx *vtx, f32 frac)
{
    f32 r = fogGetCurrentEnvironmentp()->Red;
    f32 g = fogGetCurrentEnvironmentp()->Green;
    f32 b = fogGetCurrentEnvironmentp()->Blue;

    vtx->r = r + fogGetCurrentEnvironmentp()->CloudRed   * (1.0f - r / 255.0f) * (1.0f - frac);
    vtx->g = g + fogGetCurrentEnvironmentp()->CloudGreen * (1.0f - g / 255.0f) * (1.0f - frac);
    vtx->b = b + fogGetCurrentEnvironmentp()->CloudBlue  * (1.0f - b / 255.0f) * (1.0f - frac);

    vtx->a = 0xff;
}

void skyChooseWaterVtxColour(SkyVtx *vtx, f32 frac)
{
    f32 r = fogGetCurrentEnvironmentp()->Red;
    f32 g = fogGetCurrentEnvironmentp()->Green;
    f32 b = fogGetCurrentEnvironmentp()->Blue;

    vtx->r = r + fogGetCurrentEnvironmentp()->WaterRed   * (1.0f - r / 255.0f) * (1.0f - frac);
    vtx->g = g + fogGetCurrentEnvironmentp()->WaterGreen * (1.0f - g / 255.0f) * (1.0f - frac);
    vtx->b = b + fogGetCurrentEnvironmentp()->WaterBlue  * (1.0f - b / 255.0f) * (1.0f - frac);
    vtx->a = 0xff;
}

/*
* Has to do with converting a float to a 32-bits value that's later read in two 16-bits parts
* Possibly converting floats to fixed point
*/
u32 skyFloatToFixed(f32 value)
{
    u32 result;

    if (value > 32767.9f) { value = 32767.9f; }
    if (value < -32767.9f) { value = -32767.9f; }

    if (value < 0)
    {
        result = value * -65536;
        result = -result;
    }
    else
    {
        result = 65536 * value;
    }

    return result;
}


void skySetStageNum(s32 stagenum) {
  g_SkyStageNum = stagenum;
}


void skyTick(void)
{
    #if defined(VERSION_EU)
    g_SkyCloudOffset += g_GlobalTimerDelta;
    #else
    g_SkyCloudOffset += g_ClockTimer;
    #endif
    if ( g_SkyCloudOffset > 4096.0f)
    {
        g_SkyCloudOffset -= 4096.0f;
    }
}

/*
 * How far past each edge of the view the sky and water planes are worked out, in pixels.
 * The RSP projects the corners again and lands them a fraction of a pixel either side of
 * where the CPU put them. Inside the view, the polygon would only partly cover the view's
 * top row and left column, which the rooms' scissor leaves to the sky, and the blended
 * edge shows as a dark line that flickers as the camera bobs. The scissor cuts the overhang
 * off.
 */
#define SKY_SCREEN_MARGIN 1.0f

/*
 * Drawing the sky with F3DEX3.
 *
 * F3DEX3 can't send the RDP raw triangles, so instead of rasterising the cloud and water
 * polygons on the CPU, skyDrawPlane sends the polygon skyRender has worked out (the part of
 * the plane that's on screen, 3 to 5 corners) to the RSP as ordinary vertices.
 *
 * The polygon reaches up to 300000 units from the eye, while a vertex's texture coordinate
 * (10.5 fixed point, one unit of the sky's s and t per 1/32 texel) can only reach 32768
 * units. So the polygon is cut into the part within SKY_NEAR_RANGE of the eye, drawn as
 * it is, and the parts beyond, drawn with texture coordinates divided by 2^SKY_FAR_SHIFT
 * and the tiles scaling them back up (that far away a texel is much smaller than a pixel,
 * so the lost precision doesn't show).
 */

/** A corner of a sky or water polygon */
typedef struct SkyPolyVtx
{
    f32 pos[3]; /* as in SkyVtx: world position times the sky's scale */
    f32 st[2];  /* texture coordinates, 10.5 fixed point units */
    f32 rgb[3];
} SkyPolyVtx;

/* Clipping a 5 sided polygon to a square adds at most 4 corners */
#define SKY_POLY_MAX 9

/* How far from the eye (in texture coordinate units) the polygon is drawn at full precision */
#define SKY_NEAR_RANGE 28000.0f
#define SKY_FAR_SHIFT  4

/* Texture coordinates are measured from a multiple of this, which every sky texture repeats in */
#define SKY_TEX_PERIOD 4096.0f

/* Vertex positions are scaled to fit in this */
/* The largest clip coordinate to give the RSP (it keeps them as s15.16) */
#define SKY_CLIP_RANGE 8192.0f
#define SKY_POS_RANGE 30000.0f

/** Sets the tiles to draw with texture coordinates scaled up by 2^shift */
typedef Gfx *(*SkyRetileFunc)(Gfx *gdl, sImageTableEntry *image, s32 shift);

/** Sets a corner's colour from how far it is from the horizon (skyChooseCloudVtxColour etc.) */
typedef void (*SkyColourFunc)(SkyVtx *vtx, f32 frac);

/**
 * Sets the colour of each corner of a piece from the direction it's in from the eye, as
 * skyRender does for the screen's corners. The RDP shades in screen space, as the raw
 * triangles were, so the corners clipping adds have to be coloured for where they land on
 * screen: blending the polygon's corners along its edges in world space would give the near
 * part (a few percent of a plane reaching 300000 units) almost the colour of the near edge.
 * `elevationSign` is 1 for the clouds (above the eye) and -1 for the water.
 */
static void skyColourPiece(SkyPolyVtx *poly, s32 n, const f32 *originPos, f32 elevationSign,
                           SkyColourFunc colour)
{
    for (s32 i = 0; i < n; i++)
    {
        f32 dx = poly[i].pos[0] - originPos[0];
        f32 dy = poly[i].pos[1] - originPos[1];
        f32 dz = poly[i].pos[2] - originPos[2];
        f32 elevation = elevationSign * 2.0f * dy / sqrtf(dx * dx + dz * dz + 0.0001f);
        SkyVtx vtx;

        if (elevation > 1.0f)
        {
            elevation = 1.0f;
        }

        colour(&vtx, 1.0f - elevation);

        poly[i].rgb[0] = vtx.r;
        poly[i].rgb[1] = vtx.g;
        poly[i].rgb[2] = vtx.b;
    }
}

/**
 * Keeps the part of polygon `in` where sign * (st[axis] - limit) <= 0, in `out`.
 * Returns the number of corners kept.
 */
static s32 skyClipPolygon(const SkyPolyVtx *in, s32 n, SkyPolyVtx *out, s32 axis, f32 sign, f32 limit)
{
    s32 count = 0;

    for (s32 i = 0; i < n; i++)
    {
        const SkyPolyVtx *a = &in[i];
        const SkyPolyVtx *b = &in[(i + 1) % n];
        f32 da = sign * (a->st[axis] - limit);
        f32 db = sign * (b->st[axis] - limit);

        if (da <= 0.0f)
        {
            out[count++] = *a;
        }

        if ((da <= 0.0f) != (db <= 0.0f))
        {
            f32 frac = da / (da - db);
            SkyPolyVtx *v = &out[count++];

            for (s32 k = 0; k < 3; k++)
            {
                v->pos[k] = a->pos[k] + (b->pos[k] - a->pos[k]) * frac;
                v->rgb[k] = a->rgb[k] + (b->rgb[k] - a->rgb[k]) * frac;
            }

            for (s32 k = 0; k < 2; k++)
            {
                v->st[k] = a->st[k] + (b->st[k] - a->st[k]) * frac;
            }
        }
    }

    return count;
}

static s16 skyToS16(f32 value)
{
    return (s16)skyClamp(value, -32767.0f, 32767.0f);
}

/**
 * Draws a convex polygon as a triangle fan, with positions measured from `originPos` and
 * divided by `posDiv`, and texture coordinates measured from `originSt` and divided by
 * `stDiv`.
 */
static Gfx *skyDrawPiece(Gfx *gdl, const SkyPolyVtx *poly, s32 n, const f32 *originPos, f32 posDiv,
                         const f32 *originSt, f32 stDiv)
{
    Vtx *vtx;
    s32 i;

    if (n < 3)
    {
        return gdl;
    }

    vtx = dynAllocateVertices(n);

    for (i = 0; i < n; i++)
    {
        for (s32 k = 0; k < 3; k++)
        {
            vtx[i].v.ob[k] = skyToS16((poly[i].pos[k] - originPos[k]) / posDiv);
            vtx[i].v.cn[k] = (u8)skyClamp(poly[i].rgb[k] + 0.5f, 0.0f, 255.0f);
        }

        vtx[i].v.flag = 0;
        vtx[i].v.tc[0] = skyToS16((poly[i].st[0] - originSt[0]) / stDiv);
        vtx[i].v.tc[1] = skyToS16((poly[i].st[1] - originSt[1]) / stDiv);
        vtx[i].v.cn[3] = 0xff;
    }

    gSPVertex(gdl++, osVirtualToPhysical(vtx), n, 0);

    for (i = 1; i + 2 < n; i += 2)
    {
        gSP2Triangles(gdl++, 0, i, i + 1, 0, 0, i + 1, i + 2, 0);
    }

    if (i + 1 < n)
    {
        gSP1Triangle(gdl++, 0, i, i + 1, 0);
    }

    return gdl;
}

/**
 * Loads `full` as the projection for some polygons (with positions as skyDrawPiece makes
 * them), scaled down as a whole if their clip coordinates would be too big for the RSP:
 * F3DEX3 keeps them as s15.16, and the sky reaches far further than anything else. Scaling
 * x, y, z and w alike leaves the screen positions and depths as they are.
 */
static Gfx *skyLoadProjection(Gfx *gdl, const Mtxf *full, const SkyPolyVtx (*polys)[SKY_POLY_MAX],
                              const s32 *counts, s32 numpolys, const f32 *originPos, f32 posDiv)
{
    Mtxf scaled = *full;
    Mtx *projection;
    f32 maxabs = 1.0f;

    for (s32 p = 0; p < numpolys; p++)
    {
        for (s32 i = 0; i < counts[p]; i++)
        {
            f32 local[3];

            for (s32 k = 0; k < 3; k++)
            {
                local[k] = (polys[p][i].pos[k] - originPos[k]) / posDiv;
            }

            for (s32 j = 0; j < 4; j++)
            {
                f32 c = local[0] * full->m[0][j] + local[1] * full->m[1][j] + local[2] * full->m[2][j] + full->m[3][j];

                if (SKYABS(c) > maxabs)
                {
                    maxabs = SKYABS(c);
                }
            }
        }
    }

    if (maxabs > SKY_CLIP_RANGE)
    {
        f32 k = SKY_CLIP_RANGE / maxabs;

        for (s32 i = 0; i < 4; i++)
        {
            for (s32 j = 0; j < 4; j++)
            {
                scaled.m[i][j] *= k;
            }
        }
    }

    projection = dynAllocateMatrix();
    guMtxF2L(scaled.m, projection);
    gSPMatrix(gdl++, osVirtualToPhysical(projection), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    return gdl;
}

/**
 * Draws a cloud or water polygon from skyRender: `numverts` corners (3, 4 or 5; four are a
 * quad in the order 0 1 3 2) in the space `mtx` projects. The texture and render mode must
 * be set up already; `retile` sets the texture's tiles for the far parts.
 */
static Gfx *skyDrawPlane(Gfx *gdl, const SkyVtx *verts, s32 numverts, const Mtxf *mtx, f32 scale,
                         sImageTableEntry *image, SkyRetileFunc retile,
                         f32 elevationSign, SkyColourFunc colour)
{
    static const u8 quadorder[4] = {0, 1, 3, 2};
    SkyPolyVtx poly[5];
    SkyPolyVtx strip[SKY_POLY_MAX];
    SkyPolyVtx pieces[5][SKY_POLY_MAX];
    s32 counts[5];
    SkyPolyVtx tmp[SKY_POLY_MAX];
    const coord3d *eye = playerGetPos();
    f32 eyeSt[2];
    f32 originSt[2];
    f32 originPos[3];
    f32 posDiv;
    f32 maxabs = 0.0f;
    f32 concavity;
    Mtxf pre;
    Mtxf full;
    Mtx *modelview;
    s32 n;
    s32 i;

    if (numverts < 3)
    {
        return gdl;
    }

    for (i = 0; i < numverts; i++)
    {
        const SkyVtx *src = &verts[numverts == 4 ? quadorder[i] : i];

        poly[i].pos[0] = src->x;
        poly[i].pos[1] = src->y;
        poly[i].pos[2] = src->z;
        poly[i].st[0] = src->s;
        poly[i].st[1] = src->t;
        poly[i].rgb[0] = src->r;
        poly[i].rgb[1] = src->g;
        poly[i].rgb[2] = src->b;
    }

    /* Positions are measured from the eye, scaled down to fit */
    originPos[0] = eye->x * scale;
    originPos[1] = eye->y * scale;
    originPos[2] = eye->z * scale;

    for (i = 0; i < numverts; i++)
    {
        for (s32 k = 0; k < 3; k++)
        {
            f32 d = SKYABS(poly[i].pos[k] - originPos[k]);

            if (d > maxabs)
            {
                maxabs = d;
            }
        }
    }

    posDiv = maxabs / SKY_POS_RANGE;

    if (posDiv < 1.0f / 64.0f)
    {
        posDiv = 1.0f / 64.0f;
    }

    /* Texture coordinates are measured from the texture repeat nearest the eye */
    eyeSt[0] = eye->x;
    eyeSt[1] = eye->z + g_SkyCloudOffset;
    originSt[0] = SKY_TEX_PERIOD * (s32)(eyeSt[0] / SKY_TEX_PERIOD);
    originSt[1] = SKY_TEX_PERIOD * (s32)(eyeSt[1] / SKY_TEX_PERIOD);

    /*
     * The matrix: vertex * posDiv + originPos, then `mtx`. Then the water's concavity, which
     * moves the planes up the screen by that many pixels (as skyProjectVertex does).
     */
    mtx4LoadIdentity(&pre);
    pre.m[0][0] = pre.m[1][1] = pre.m[2][2] = posDiv;
    pre.m[3][0] = originPos[0];
    pre.m[3][1] = originPos[1];
    pre.m[3][2] = originPos[2];
    mtx4Mult(mtx, &pre, &full);

    concavity = 2.0f * fogGetCurrentEnvironmentp()->WaterConcavity / camGetScreenHeight();

    for (i = 0; i < 4; i++)
    {
        full.m[i][1] += concavity * full.m[i][3];
    }

    modelview = dynAllocateMatrix();
    guMtxIdent(modelview);

    // The planes reach SKY_SCREEN_MARGIN past the view: keep that out of the black bands
    // round it (above and below the picture on NTSC, between the co-op views)
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, VI_SX(viGetViewLeft()), viGetViewTop(),
            VI_SX(viGetViewLeft() + viGetViewWidth()), viGetViewTop() + viGetViewHeight());

    gSPMatrix(gdl++, osVirtualToPhysical(modelview), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(gdl++, G_SHADE | G_SHADING_SMOOTH);

    /* The far parts: right of, left of, then in line with but in front of and behind the near square */
    counts[0] = skyClipPolygon(poly, numverts, pieces[0], 0, -1.0f, eyeSt[0] + SKY_NEAR_RANGE);
    counts[1] = skyClipPolygon(poly, numverts, pieces[1], 0, 1.0f, eyeSt[0] - SKY_NEAR_RANGE);

    n = skyClipPolygon(poly, numverts, tmp, 0, 1.0f, eyeSt[0] + SKY_NEAR_RANGE);
    n = skyClipPolygon(tmp, n, strip, 0, -1.0f, eyeSt[0] - SKY_NEAR_RANGE);

    counts[2] = skyClipPolygon(strip, n, pieces[2], 1, -1.0f, eyeSt[1] + SKY_NEAR_RANGE);
    counts[3] = skyClipPolygon(strip, n, pieces[3], 1, 1.0f, eyeSt[1] - SKY_NEAR_RANGE);

    /* The near part */
    n = skyClipPolygon(strip, n, tmp, 1, 1.0f, eyeSt[1] + SKY_NEAR_RANGE);
    counts[4] = skyClipPolygon(tmp, n, pieces[4], 1, -1.0f, eyeSt[1] - SKY_NEAR_RANGE);

    for (i = 0; i < 5; i++)
    {
        skyColourPiece(pieces[i], counts[i], originPos, elevationSign, colour);
    }

    if (counts[0] >= 3 || counts[1] >= 3 || counts[2] >= 3 || counts[3] >= 3)
    {
        gdl = retile(gdl, image, SKY_FAR_SHIFT);
        gdl = skyLoadProjection(gdl, &full, pieces, counts, 4, originPos, posDiv);

        for (i = 0; i < 4; i++)
        {
            gdl = skyDrawPiece(gdl, pieces[i], counts[i], originPos, posDiv, originSt, (f32)(1 << SKY_FAR_SHIFT));
        }

        gdl = retile(gdl, image, 0);
    }

    gdl = skyLoadProjection(gdl, &full, &pieces[4], &counts[4], 1, originPos, posDiv);
    gdl = skyDrawPiece(gdl, pieces[4], counts[4], originPos, posDiv, originSt, 1.0f);

    // Back to the whole framebuffer, as viSetupScreensForNumPlayers left it
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, VI_SX(viGetX()), viGetY());

    return gdl;
}

/** skyDrawPlane's retile for the clouds: the tiles texSelect sets, scaled */
static Gfx *skyRetileClouds(Gfx *gdl, sImageTableEntry *image, s32 shift)
{
    texSelectScaled(&gdl, image, 2, shift);
    return gdl;
}

/** skyDrawPlane's retile for the water: the tiles waterSetupGdl sets, scaled */
static Gfx *skyRetileWater(Gfx *gdl, sImageTableEntry *image, s32 shift)
{
    s32 tileshift = shift > 0 ? 16 - shift : 0;

    (void)image;

    gDPPipeSync(gdl++);
    gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 0, 0, 0, 5, tileshift, 0, 5, tileshift);
    gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 1, 0, 0, 5, tileshift, 0, 5, tileshift);

    return gdl;
}

Gfx* waterSetupGdl(Gfx*, s32);

Gfx *skyRender(Gfx *gdl)
{
    coord3d tlDir;
    coord3d trDir;
    coord3d blDir;
    coord3d brDir;
    coord3d topDir;
    coord3d bottomDir;
    coord3d leftDir;
    coord3d rightDir;
    coord3d tlSky;
    coord3d trSky;
    coord3d blSky;
    coord3d brSky;
    coord3d topSky;
    coord3d bottomSky;
    coord3d leftSky;
    coord3d rightSky;
    coord3d tlWater;
    coord3d trWater;
    coord3d blWater;
    coord3d brWater;
    coord3d topWater;
    coord3d bottomWater;
    coord3d leftWater;
    coord3d rightWater;
    f32 tlSkyFrac;
    f32 trSkyFrac;
    f32 blSkyFrac;
    f32 brSkyFrac;
    f32 topSkyFrac;
    f32 bottomSkyFrac;
    f32 leftSkyFrac;
    f32 rightSkyFrac;
    f32 tlWaterFrac;
    f32 trWaterFrac;
    f32 blWaterFrac;
    f32 brWaterFrac;
    f32 topWaterFrac;
    f32 bottomWaterFrac;
    f32 leftWaterFrac;
    f32 rightWaterFrac;
    f32 leftHorizonY;
    f32 rightHorizonY;
    s32 numverts;
    s32 j;
    s32 tlInSky;
    s32 trInSky;
    s32 blInSky;
    s32 brInSky;
    SkyVtx skyverts[5];
    SkyVtx waterverts[5];
    f32 tmp;
    f32 scale;
    [[maybe_unused]] bool nudgeWater;
    struct CurrentEnvironmentRecord *env;

    scale = bgGetLevelScale() / 30.0f;
    nudgeWater = FALSE;
    env = fogGetCurrentEnvironmentp();

    if (!fogGetCurrentEnvironmentp()->Clouds)
    {
        // Co-op plays solo levels, so clear each view to the fog colour too.
        if ((getPlayerCount() == 1) || gameIsTwoPlayerMission())
        {
            gDPSetCycleType(gdl++, G_CYC_FILL);

            gdl = viSetFillColor(gdl, env->Red, env->Green, env->Blue);

            gDPFillRectangle(gdl++, VI_SX(viGetViewLeft()), viGetViewTop(),
                    VI_SX_INCL(viGetViewLeft() + viGetViewWidth() - 1),
                    viGetViewTop() + viGetViewHeight() - 1);

            gDPPipeSync(gdl++);
            return gdl;
        }

        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_FILL);

        gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);

        gDPFillRectangle(gdl++,
                VI_SX(g_CurrentPlayer->viewleft), g_CurrentPlayer->viewtop,
                VI_SX_INCL((g_CurrentPlayer->viewleft + g_CurrentPlayer->viewx) - 1),
                (g_CurrentPlayer->viewtop + g_CurrentPlayer->viewy) - 1);

        gDPPipeSync(gdl++);
        return gdl;
    }

    gdl = viSetFillColor(gdl, env->Red, env->Green, env->Blue);


    // The corners of the screen the planes are worked out for
    f32 edgeLeft = -SKY_SCREEN_MARGIN;
    f32 edgeTop = -SKY_SCREEN_MARGIN;
    f32 edgeRight = camGetScreenWidth() - 0.1f + SKY_SCREEN_MARGIN;
    f32 edgeBottom = camGetScreenHeight() - 0.1f + SKY_SCREEN_MARGIN;

    skyGetWorldPosFromScreenPos(edgeLeft, edgeTop, &tlDir);
    skyGetWorldPosFromScreenPos(edgeRight, edgeTop, &trDir);
    skyGetWorldPosFromScreenPos(edgeLeft, edgeBottom, &blDir);
    skyGetWorldPosFromScreenPos(edgeRight, edgeBottom, &brDir);

    tlInSky = skyIsScreenCornerInSky(&tlDir, &tlSky, &tlSkyFrac);
    trInSky = skyIsScreenCornerInSky(&trDir, &trSky, &trSkyFrac);
    blInSky = skyIsScreenCornerInSky(&blDir, &blSky, &blSkyFrac);
    brInSky = skyIsScreenCornerInSky(&brDir, &brSky, &brSkyFrac);

    skyIsCornerInWater(&tlDir, &tlWater, &tlWaterFrac);
    skyIsCornerInWater(&trDir, &trWater, &trWaterFrac);
    skyIsCornerInWater(&blDir, &blWater, &blWaterFrac);
    skyIsCornerInWater(&brDir, &brWater, &brWaterFrac);

    if (tlInSky != blInSky)
    {
        leftHorizonY = camGetScreenTop() + edgeTop + (camGetScreenHeight() + 2 * SKY_SCREEN_MARGIN) * (tlDir.f[1] / (tlDir.f[1] - blDir.f[1]));

        skyGetWorldPosFromScreenPos(edgeLeft, leftHorizonY - camGetScreenTop(), &leftDir);
        skyCalculateEdgeVertex(&tlDir, &blDir, &leftDir);
        skyIsScreenCornerInSky(&leftDir, &leftSky, &leftSkyFrac);
        skyIsCornerInWater(&leftDir, &leftWater, &leftWaterFrac);
    }
    else
    {
        leftHorizonY = 0.0f;
    }

    if (trInSky != brInSky)
    {
        rightHorizonY = camGetScreenTop() + edgeTop + (camGetScreenHeight() + 2 * SKY_SCREEN_MARGIN) * (trDir.f[1] / (trDir.f[1] - brDir.f[1]));

        skyGetWorldPosFromScreenPos(edgeRight, rightHorizonY - camGetScreenTop(), &rightDir);
        skyCalculateEdgeVertex(&trDir, &brDir, &rightDir);
        skyIsScreenCornerInSky(&rightDir, &rightSky, &rightSkyFrac);
        skyIsCornerInWater(&rightDir, &rightWater, &rightWaterFrac);
    }
    else
    {
        rightHorizonY = 0.0f;
    }

    if (tlInSky != trInSky)
    {
        skyGetWorldPosFromScreenPos(edgeLeft + (camGetScreenWidth() + 2 * SKY_SCREEN_MARGIN) * (tlDir.f[1] / (tlDir.f[1] - trDir.f[1])), edgeTop, &topDir);
        skyCalculateEdgeVertex(&tlDir, &trDir, &topDir);
        skyIsScreenCornerInSky(&topDir, &topSky, &topSkyFrac);
        skyIsCornerInWater(&topDir, &topWater, &topWaterFrac);
    }

    if (blInSky != brInSky)
    {
        tmp = camGetScreenLeft() + edgeLeft + (camGetScreenWidth() + 2 * SKY_SCREEN_MARGIN) * (blDir.f[1] / (blDir.f[1] - brDir.f[1]));

        skyGetWorldPosFromScreenPos(tmp - camGetScreenLeft(), edgeBottom, &bottomDir);
        skyCalculateEdgeVertex(&blDir, &brDir, &bottomDir);
        skyIsScreenCornerInSky(&bottomDir, &bottomSky, &bottomSkyFrac);
        skyIsCornerInWater(&bottomDir, &bottomWater, &bottomWaterFrac);
    }

    switch ((tlInSky << 3) | (trInSky << 2) | (blInSky << 1) | brInSky)
    {
        case 15:
            numverts = 0;
            break;

        case 0:
            numverts = 4;
            waterverts[0].x = tlWater.f[0] * scale;
            waterverts[0].y = tlWater.f[1] * scale;
            waterverts[0].z = tlWater.f[2] * scale;
            waterverts[1].x = trWater.f[0] * scale;
            waterverts[1].y = trWater.f[1] * scale;
            waterverts[1].z = trWater.f[2] * scale;
            waterverts[2].x = blWater.f[0] * scale;
            waterverts[2].y = blWater.f[1] * scale;
            waterverts[2].z = blWater.f[2] * scale;
            waterverts[3].x = brWater.f[0] * scale;
            waterverts[3].y = brWater.f[1] * scale;
            waterverts[3].z = brWater.f[2] * scale;
            waterverts[0].s = tlWater.f[0];
            waterverts[0].t = tlWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = trWater.f[0];
            waterverts[1].t = trWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = blWater.f[0];
            waterverts[2].t = blWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = brWater.f[0];
            waterverts[3].t = brWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], tlWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], trWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], blWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], brWaterFrac);
            break;

        case 3:
            numverts = 4;
            waterverts[0].x = tlWater.f[0] * scale;
            waterverts[0].y = tlWater.f[1] * scale;
            waterverts[0].z = tlWater.f[2] * scale;
            waterverts[1].x = trWater.f[0] * scale;
            waterverts[1].y = trWater.f[1] * scale;
            waterverts[1].z = trWater.f[2] * scale;
            waterverts[2].x = leftWater.f[0] * scale;
            waterverts[2].y = leftWater.f[1] * scale;
            waterverts[2].z = leftWater.f[2] * scale;
            waterverts[3].x = rightWater.f[0] * scale;
            waterverts[3].y = rightWater.f[1] * scale;
            waterverts[3].z = rightWater.f[2] * scale;
            waterverts[0].s = tlWater.f[0];
            waterverts[0].t = tlWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = trWater.f[0];
            waterverts[1].t = trWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = leftWater.f[0];
            waterverts[2].t = leftWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = rightWater.f[0];
            waterverts[3].t = rightWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], tlWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], trWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], leftWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], rightWaterFrac);
            break;

        case 12:
            numverts = 4;
            nudgeWater = TRUE;
            waterverts[0].x = brWater.f[0] * scale;
            waterverts[0].y = brWater.f[1] * scale;
            waterverts[0].z = brWater.f[2] * scale;
            waterverts[1].x = blWater.f[0] * scale;
            waterverts[1].y = blWater.f[1] * scale;
            waterverts[1].z = blWater.f[2] * scale;
            waterverts[2].x = rightWater.f[0] * scale;
            waterverts[2].y = rightWater.f[1] * scale;
            waterverts[2].z = rightWater.f[2] * scale;
            waterverts[3].x = leftWater.f[0] * scale;
            waterverts[3].y = leftWater.f[1] * scale;
            waterverts[3].z = leftWater.f[2] * scale;
            waterverts[0].s = brWater.f[0];
            waterverts[0].t = brWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = blWater.f[0];
            waterverts[1].t = blWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = rightWater.f[0];
            waterverts[2].t = rightWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = leftWater.f[0];
            waterverts[3].t = leftWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], brWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], blWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], rightWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], leftWaterFrac);
            break;

        case 10:
            numverts = 4;
            waterverts[0].x = trWater.f[0] * scale;
            waterverts[0].y = trWater.f[1] * scale;
            waterverts[0].z = trWater.f[2] * scale;
            waterverts[1].x = brWater.f[0] * scale;
            waterverts[1].y = brWater.f[1] * scale;
            waterverts[1].z = brWater.f[2] * scale;
            waterverts[2].x = topWater.f[0] * scale;
            waterverts[2].y = topWater.f[1] * scale;
            waterverts[2].z = topWater.f[2] * scale;
            waterverts[3].x = bottomWater.f[0] * scale;
            waterverts[3].y = bottomWater.f[1] * scale;
            waterverts[3].z = bottomWater.f[2] * scale;
            waterverts[0].s = trWater.f[0];
            waterverts[0].t = trWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = brWater.f[0];
            waterverts[1].t = brWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = topWater.f[0];
            waterverts[2].t = topWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = bottomWater.f[0];
            waterverts[3].t = bottomWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], trWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], brWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], topWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], bottomWaterFrac);
            break;

        case 5:
            numverts = 4;
            waterverts[0].x = blWater.f[0] * scale;
            waterverts[0].y = blWater.f[1] * scale;
            waterverts[0].z = blWater.f[2] * scale;
            waterverts[1].x = tlWater.f[0] * scale;
            waterverts[1].y = tlWater.f[1] * scale;
            waterverts[1].z = tlWater.f[2] * scale;
            waterverts[2].x = bottomWater.f[0] * scale;
            waterverts[2].y = bottomWater.f[1] * scale;
            waterverts[2].z = bottomWater.f[2] * scale;
            waterverts[3].x = topWater.f[0] * scale;
            waterverts[3].y = topWater.f[1] * scale;
            waterverts[3].z = topWater.f[2] * scale;
            waterverts[0].s = blWater.f[0];
            waterverts[0].t = blWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = tlWater.f[0];
            waterverts[1].t = tlWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = bottomWater.f[0];
            waterverts[2].t = bottomWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = topWater.f[0];
            waterverts[3].t = topWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], blWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], tlWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], bottomWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], topWaterFrac);
            break;

        case 14:
            numverts = 3;
            waterverts[0].x = brWater.f[0] * scale;
            waterverts[0].y = brWater.f[1] * scale;
            waterverts[0].z = brWater.f[2] * scale;
            waterverts[1].x = bottomWater.f[0] * scale;
            waterverts[1].y = bottomWater.f[1] * scale;
            waterverts[1].z = bottomWater.f[2] * scale;
            waterverts[2].x = rightWater.f[0] * scale;
            waterverts[2].y = rightWater.f[1] * scale;
            waterverts[2].z = rightWater.f[2] * scale;
            waterverts[0].s = brWater.f[0];
            waterverts[0].t = brWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = bottomWater.f[0];
            waterverts[1].t = bottomWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = rightWater.f[0];
            waterverts[2].t = rightWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], brWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], bottomWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], rightWaterFrac);
            break;

        case 13:
            numverts = 3;
            waterverts[0].x = blWater.f[0] * scale;
            waterverts[0].y = blWater.f[1] * scale;
            waterverts[0].z = blWater.f[2] * scale;
            waterverts[1].x = leftWater.f[0] * scale;
            waterverts[1].y = leftWater.f[1] * scale;
            waterverts[1].z = leftWater.f[2] * scale;
            waterverts[2].x = bottomWater.f[0] * scale;
            waterverts[2].y = bottomWater.f[1] * scale;
            waterverts[2].z = bottomWater.f[2] * scale;
            waterverts[0].s = blWater.f[0];
            waterverts[0].t = blWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = leftWater.f[0];
            waterverts[1].t = leftWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = bottomWater.f[0];
            waterverts[2].t = bottomWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], blWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], leftWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], bottomWaterFrac);
            break;

        case 11:
            numverts = 3;
            waterverts[0].x = trWater.f[0] * scale;
            waterverts[0].y = trWater.f[1] * scale;
            waterverts[0].z = trWater.f[2] * scale;
            waterverts[1].x = rightWater.f[0] * scale;
            waterverts[1].y = rightWater.f[1] * scale;
            waterverts[1].z = rightWater.f[2] * scale;
            waterverts[2].x = topWater.f[0] * scale;
            waterverts[2].y = topWater.f[1] * scale;
            waterverts[2].z = topWater.f[2] * scale;
            waterverts[0].s = trWater.f[0];
            waterverts[0].t = trWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = rightWater.f[0];
            waterverts[1].t = rightWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = topWater.f[0];
            waterverts[2].t = topWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], trWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], rightWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], topWaterFrac);
            break;

        case 7:
            numverts = 3;
            waterverts[0].x = tlWater.f[0] * scale;
            waterverts[0].y = tlWater.f[1] * scale;
            waterverts[0].z = tlWater.f[2] * scale;
            waterverts[1].x = topWater.f[0] * scale;
            waterverts[1].y = topWater.f[1] * scale;
            waterverts[1].z = topWater.f[2] * scale;
            waterverts[2].x = leftWater.f[0] * scale;
            waterverts[2].y = leftWater.f[1] * scale;
            waterverts[2].z = leftWater.f[2] * scale;
            waterverts[0].s = tlWater.f[0];
            waterverts[0].t = tlWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = topWater.f[0];
            waterverts[1].t = topWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = leftWater.f[0];
            waterverts[2].t = leftWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], tlWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], topWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], leftWaterFrac);
            break;

        case 1:
            numverts = 5;
            waterverts[0].x = blWater.f[0] * scale;
            waterverts[0].y = blWater.f[1] * scale;
            waterverts[0].z = blWater.f[2] * scale;
            waterverts[1].x = tlWater.f[0] * scale;
            waterverts[1].y = tlWater.f[1] * scale;
            waterverts[1].z = tlWater.f[2] * scale;
            waterverts[2].x = trWater.f[0] * scale;
            waterverts[2].y = trWater.f[1] * scale;
            waterverts[2].z = trWater.f[2] * scale;
            waterverts[3].x = rightWater.f[0] * scale;
            waterverts[3].y = rightWater.f[1] * scale;
            waterverts[3].z = rightWater.f[2] * scale;
            waterverts[4].x = bottomWater.f[0] * scale;
            waterverts[4].y = bottomWater.f[1] * scale;
            waterverts[4].z = bottomWater.f[2] * scale;
            waterverts[0].s = blWater.f[0];
            waterverts[0].t = blWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = tlWater.f[0];
            waterverts[1].t = tlWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = trWater.f[0];
            waterverts[2].t = trWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = rightWater.f[0];
            waterverts[3].t = rightWater.f[2] + g_SkyCloudOffset;
            waterverts[4].s = bottomWater.f[0];
            waterverts[4].t = bottomWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], blWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], tlWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], trWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], rightWaterFrac);
            skyChooseWaterVtxColour(&waterverts[4], bottomWaterFrac);
            break;

        case 2:
            numverts = 5;
            waterverts[0].x = tlWater.f[0] * scale;
            waterverts[0].y = tlWater.f[1] * scale;
            waterverts[0].z = tlWater.f[2] * scale;
            waterverts[1].x = trWater.f[0] * scale;
            waterverts[1].y = trWater.f[1] * scale;
            waterverts[1].z = trWater.f[2] * scale;
            waterverts[2].x = brWater.f[0] * scale;
            waterverts[2].y = brWater.f[1] * scale;
            waterverts[2].z = brWater.f[2] * scale;
            waterverts[3].x = bottomWater.f[0] * scale;
            waterverts[3].y = bottomWater.f[1] * scale;
            waterverts[3].z = bottomWater.f[2] * scale;
            waterverts[4].x = leftWater.f[0] * scale;
            waterverts[4].y = leftWater.f[1] * scale;
            waterverts[4].z = leftWater.f[2] * scale;
            waterverts[0].s = tlWater.f[0];
            waterverts[0].t = tlWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = trWater.f[0];
            waterverts[1].t = trWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = brWater.f[0];
            waterverts[2].t = brWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = bottomWater.f[0];
            waterverts[3].t = bottomWater.f[2] + g_SkyCloudOffset;
            waterverts[4].s = leftWater.f[0];
            waterverts[4].t = leftWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], tlWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], trWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], brWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], bottomWaterFrac);
            skyChooseWaterVtxColour(&waterverts[4], leftWaterFrac);
            break;

        case 4:
            numverts = 5;
            waterverts[0].x = brWater.f[0] * scale;
            waterverts[0].y = brWater.f[1] * scale;
            waterverts[0].z = brWater.f[2] * scale;
            waterverts[1].x = blWater.f[0] * scale;
            waterverts[1].y = blWater.f[1] * scale;
            waterverts[1].z = blWater.f[2] * scale;
            waterverts[2].x = tlWater.f[0] * scale;
            waterverts[2].y = tlWater.f[1] * scale;
            waterverts[2].z = tlWater.f[2] * scale;
            waterverts[3].x = topWater.f[0] * scale;
            waterverts[3].y = topWater.f[1] * scale;
            waterverts[3].z = topWater.f[2] * scale;
            waterverts[4].x = rightWater.f[0] * scale;
            waterverts[4].y = rightWater.f[1] * scale;
            waterverts[4].z = rightWater.f[2] * scale;
            waterverts[0].s = brWater.f[0];
            waterverts[0].t = brWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = blWater.f[0];
            waterverts[1].t = blWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = tlWater.f[0];
            waterverts[2].t = tlWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = topWater.f[0];
            waterverts[3].t = topWater.f[2] + g_SkyCloudOffset;
            waterverts[4].s = rightWater.f[0];
            waterverts[4].t = rightWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], brWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], blWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], tlWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], topWaterFrac);
            skyChooseWaterVtxColour(&waterverts[4], rightWaterFrac);
            break;

        case 8:
            numverts = 5;
            waterverts[0].x = trWater.f[0] * scale;
            waterverts[0].y = trWater.f[1] * scale;
            waterverts[0].z = trWater.f[2] * scale;
            waterverts[1].x = brWater.f[0] * scale;
            waterverts[1].y = brWater.f[1] * scale;
            waterverts[1].z = brWater.f[2] * scale;
            waterverts[2].x = blWater.f[0] * scale;
            waterverts[2].y = blWater.f[1] * scale;
            waterverts[2].z = blWater.f[2] * scale;
            waterverts[3].x = leftWater.f[0] * scale;
            waterverts[3].y = leftWater.f[1] * scale;
            waterverts[3].z = leftWater.f[2] * scale;
            waterverts[4].x = topWater.f[0] * scale;
            waterverts[4].y = topWater.f[1] * scale;
            waterverts[4].z = topWater.f[2] * scale;
            waterverts[0].s = trWater.f[0];
            waterverts[0].t = trWater.f[2] + g_SkyCloudOffset;
            waterverts[1].s = brWater.f[0];
            waterverts[1].t = brWater.f[2] + g_SkyCloudOffset;
            waterverts[2].s = blWater.f[0];
            waterverts[2].t = blWater.f[2] + g_SkyCloudOffset;
            waterverts[3].s = leftWater.f[0];
            waterverts[3].t = leftWater.f[2] + g_SkyCloudOffset;
            waterverts[4].s = topWater.f[0];
            waterverts[4].t = topWater.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&waterverts[0], trWaterFrac);
            skyChooseWaterVtxColour(&waterverts[1], brWaterFrac);
            skyChooseWaterVtxColour(&waterverts[2], blWaterFrac);
            skyChooseWaterVtxColour(&waterverts[3], leftWaterFrac);
            skyChooseWaterVtxColour(&waterverts[4], topWaterFrac);
            break;

        default:
            return gdl;
    }

    if (numverts > 0)
    {
        Mtxf waterViewProj;
        Mtxf waterMtx;
        SkyScreenVtx waterScreen[5];
        s32 i;

        mtx4Mult(camGetProjectionMtxf(), camGetWorldToScreenMtxf(), &waterViewProj);
        guScaleF(g_SkyInvScaleMtx.m, 1.0f / scale, 1.0f / scale, 1.0f / scale);
        mtx4Mult(&waterViewProj, &g_SkyInvScaleMtx, &waterMtx);

        for (i = 0; i < numverts; i++)
        {
            skyProjectVertex(&waterverts[i], &waterMtx, 130, 65535.0f, 65535.0f, &waterScreen[i]);

            waterScreen[i].screenX = skyClamp(waterScreen[i].screenX, VI_SX(camGetScreenLeft()) * 4.0f, VI_SX(camGetScreenLeft() + camGetScreenWidth()) * 4.0f - 1.0f);
            waterScreen[i].screenY = skyClamp(waterScreen[i].screenY, camGetScreenTop() * 4.0f, (camGetScreenTop() + camGetScreenHeight()) * 4.0f - 1.0f);

            if (waterScreen[i].screenY > camGetScreenTop() * 4.0f + 4.0f
                    && waterScreen[i].screenY < (camGetScreenTop() + camGetScreenHeight()) * 4.0f - 4.0f)
            {
                waterScreen[i].screenY -= 4.0f;
            }
        }

        if (!fogGetCurrentEnvironmentp()->IsWater)
        {
            f32 minX = 1279.0f;
            f32 maxX = 0.0f;
            f32 minY = 959.0f;
            f32 maxY = 0.0f;

            for (j = 0; j < numverts; j++)
            {
                if (waterScreen[j].screenX < minX) { minX = waterScreen[j].screenX; }
                if (waterScreen[j].screenX > maxX) { maxX = waterScreen[j].screenX; }

                if (waterScreen[j].screenY < minY) { minY = waterScreen[j].screenY; }
                if (waterScreen[j].screenY > maxY) { maxY = waterScreen[j].screenY; }
            }

            gDPPipeSync(gdl++);
            gDPSetCycleType(gdl++, G_CYC_FILL);
            gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
            gDPSetTexturePersp(gdl++, G_TP_NONE);
            gDPFillRectangle(gdl++, (s32)(minX * 0.25f), (s32)(minY * 0.25f), (s32)(maxX * 0.25f), (s32)(maxY * 0.25f));
            gDPPipeSync(gdl++);
            gDPSetTexturePersp(gdl++, G_TP_PERSP);
        }
        else
        {
            gDPPipeSync(gdl++);

            texSelect(&gdl, &g_ImagesSkyWater[fogGetCurrentEnvironmentp()->WaterImageId], TEXBLEND_OPAQUE, TEXZBUF_OFF, 2);
            gdl = waterSetupGdl(gdl, 0); // ???
            gDPSetRenderMode(gdl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);

            gdl = skyDrawPlane(gdl, waterverts, numverts, &waterMtx, scale, NULL, skyRetileWater,
                    -1.0f, skyChooseWaterVtxColour);
        }
    }

    switch ((tlInSky << 3) | (trInSky << 2) | (blInSky << 1) | brInSky)
    {
        case 0:
            return gdl;

        case 15:
            numverts = 4;
            skyverts[0].x = tlSky.f[0] * scale;
            skyverts[0].y = tlSky.f[1] * scale;
            skyverts[0].z = tlSky.f[2] * scale;
            skyverts[1].x = trSky.f[0] * scale;
            skyverts[1].y = trSky.f[1] * scale;
            skyverts[1].z = trSky.f[2] * scale;
            skyverts[2].x = blSky.f[0] * scale;
            skyverts[2].y = blSky.f[1] * scale;
            skyverts[2].z = blSky.f[2] * scale;
            skyverts[3].x = brSky.f[0] * scale;
            skyverts[3].y = brSky.f[1] * scale;
            skyverts[3].z = brSky.f[2] * scale;
            skyverts[0].s = tlSky.f[0] * 0.1f;
            skyverts[0].t = tlSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = trSky.f[0] * 0.1f;
            skyverts[1].t = trSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = blSky.f[0] * 0.1f;
            skyverts[2].t = blSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = brSky.f[0] * 0.1f;
            skyverts[3].t = brSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], tlSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], trSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], blSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], brSkyFrac);
            break;

        case 12:
            numverts = 4;
            skyverts[0].x = tlSky.f[0] * scale;
            skyverts[0].y = tlSky.f[1] * scale;
            skyverts[0].z = tlSky.f[2] * scale;
            skyverts[1].x = trSky.f[0] * scale;
            skyverts[1].y = trSky.f[1] * scale;
            skyverts[1].z = trSky.f[2] * scale;
            skyverts[2].x = leftSky.f[0] * scale;
            skyverts[2].y = leftSky.f[1] * scale;
            skyverts[2].z = leftSky.f[2] * scale;
            skyverts[3].x = rightSky.f[0] * scale;
            skyverts[3].y = rightSky.f[1] * scale;
            skyverts[3].z = rightSky.f[2] * scale;
            skyverts[0].s = tlSky.f[0] * 0.1f;
            skyverts[0].t = tlSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = trSky.f[0] * 0.1f;
            skyverts[1].t = trSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = leftSky.f[0] * 0.1f;
            skyverts[2].t = leftSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = rightSky.f[0] * 0.1f;
            skyverts[3].t = rightSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], tlSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], trSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], leftSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], rightSkyFrac);
            break;

        case 3:
            numverts = 4;
            skyverts[0].x = brSky.f[0] * scale;
            skyverts[0].y = brSky.f[1] * scale;
            skyverts[0].z = brSky.f[2] * scale;
            skyverts[1].x = blSky.f[0] * scale;
            skyverts[1].y = blSky.f[1] * scale;
            skyverts[1].z = blSky.f[2] * scale;
            skyverts[2].x = rightSky.f[0] * scale;
            skyverts[2].y = rightSky.f[1] * scale;
            skyverts[2].z = rightSky.f[2] * scale;
            skyverts[3].x = leftSky.f[0] * scale;
            skyverts[3].y = leftSky.f[1] * scale;
            skyverts[3].z = leftSky.f[2] * scale;
            skyverts[0].s = brSky.f[0] * 0.1f;
            skyverts[0].t = brSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = blSky.f[0] * 0.1f;
            skyverts[1].t = blSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = rightSky.f[0] * 0.1f;
            skyverts[2].t = rightSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = leftSky.f[0] * 0.1f;
            skyverts[3].t = leftSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], brSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], blSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], rightSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], leftSkyFrac);
            break;

        case 5:
            numverts = 4;
            skyverts[0].x = trSky.f[0] * scale;
            skyverts[0].y = trSky.f[1] * scale;
            skyverts[0].z = trSky.f[2] * scale;
            skyverts[1].x = brSky.f[0] * scale;
            skyverts[1].y = brSky.f[1] * scale;
            skyverts[1].z = brSky.f[2] * scale;
            skyverts[2].x = topSky.f[0] * scale;
            skyverts[2].y = topSky.f[1] * scale;
            skyverts[2].z = topSky.f[2] * scale;
            skyverts[3].x = bottomSky.f[0] * scale;
            skyverts[3].y = bottomSky.f[1] * scale;
            skyverts[3].z = bottomSky.f[2] * scale;
            skyverts[0].s = trSky.f[0] * 0.1f;
            skyverts[0].t = trSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = brSky.f[0] * 0.1f;
            skyverts[1].t = brSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = topSky.f[0] * 0.1f;
            skyverts[2].t = topSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = bottomSky.f[0] * 0.1f;
            skyverts[3].t = bottomSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], trSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], brSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], topSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], bottomSkyFrac);
            break;

        case 10:
            numverts = 4;
            skyverts[0].x = blSky.f[0] * scale;
            skyverts[0].y = blSky.f[1] * scale;
            skyverts[0].z = blSky.f[2] * scale;
            skyverts[1].x = tlSky.f[0] * scale;
            skyverts[1].y = tlSky.f[1] * scale;
            skyverts[1].z = tlSky.f[2] * scale;
            skyverts[2].x = bottomSky.f[0] * scale;
            skyverts[2].y = bottomSky.f[1] * scale;
            skyverts[2].z = bottomSky.f[2] * scale;
            skyverts[3].x = topSky.f[0] * scale;
            skyverts[3].y = topSky.f[1] * scale;
            skyverts[3].z = topSky.f[2] * scale;
            skyverts[0].s = blSky.f[0] * 0.1f;
            skyverts[0].t = blSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = tlSky.f[0] * 0.1f;
            skyverts[1].t = tlSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = bottomSky.f[0] * 0.1f;
            skyverts[2].t = bottomSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = topSky.f[0] * 0.1f;
            skyverts[3].t = topSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], blSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], tlSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], bottomSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], topSkyFrac);
            break;

        case 1:
            numverts = 3;
            skyverts[0].x = brSky.f[0] * scale;
            skyverts[0].y = brSky.f[1] * scale;
            skyverts[0].z = brSky.f[2] * scale;
            skyverts[1].x = bottomSky.f[0] * scale;
            skyverts[1].y = bottomSky.f[1] * scale;
            skyverts[1].z = bottomSky.f[2] * scale;
            skyverts[2].x = rightSky.f[0] * scale;
            skyverts[2].y = rightSky.f[1] * scale;
            skyverts[2].z = rightSky.f[2] * scale;
            skyverts[0].s = brSky.f[0] * 0.1f;
            skyverts[0].t = brSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = bottomSky.f[0] * 0.1f;
            skyverts[1].t = bottomSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = rightSky.f[0] * 0.1f;
            skyverts[2].t = rightSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], brSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], bottomSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], rightSkyFrac);
            break;

        case 2:
            numverts = 3;
            skyverts[0].x = blSky.f[0] * scale;
            skyverts[0].y = blSky.f[1] * scale;
            skyverts[0].z = blSky.f[2] * scale;
            skyverts[1].x = leftSky.f[0] * scale;
            skyverts[1].y = leftSky.f[1] * scale;
            skyverts[1].z = leftSky.f[2] * scale;
            skyverts[2].x = bottomSky.f[0] * scale;
            skyverts[2].y = bottomSky.f[1] * scale;
            skyverts[2].z = bottomSky.f[2] * scale;
            skyverts[0].s = blSky.f[0] * 0.1f;
            skyverts[0].t = blSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = leftSky.f[0] * 0.1f;
            skyverts[1].t = leftSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = bottomSky.f[0] * 0.1f;
            skyverts[2].t = bottomSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], blSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], leftSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], bottomSkyFrac);
            break;

        case 4:
            numverts = 3;
            skyverts[0].x = trSky.f[0] * scale;
            skyverts[0].y = trSky.f[1] * scale;
            skyverts[0].z = trSky.f[2] * scale;
            skyverts[1].x = rightSky.f[0] * scale;
            skyverts[1].y = rightSky.f[1] * scale;
            skyverts[1].z = rightSky.f[2] * scale;
            skyverts[2].x = topSky.f[0] * scale;
            skyverts[2].y = topSky.f[1] * scale;
            skyverts[2].z = topSky.f[2] * scale;
            skyverts[0].s = trSky.f[0] * 0.1f;
            skyverts[0].t = trSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = rightSky.f[0] * 0.1f;
            skyverts[1].t = rightSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = topSky.f[0] * 0.1f;
            skyverts[2].t = topSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], trSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], rightSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], topSkyFrac);
            break;

        case 8:
            numverts = 3;
            skyverts[0].x = tlSky.f[0] * scale;
            skyverts[0].y = tlSky.f[1] * scale;
            skyverts[0].z = tlSky.f[2] * scale;
            skyverts[1].x = topSky.f[0] * scale;
            skyverts[1].y = topSky.f[1] * scale;
            skyverts[1].z = topSky.f[2] * scale;
            skyverts[2].x = leftSky.f[0] * scale;
            skyverts[2].y = leftSky.f[1] * scale;
            skyverts[2].z = leftSky.f[2] * scale;
            skyverts[0].s = tlSky.f[0] * 0.1f;
            skyverts[0].t = tlSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = topSky.f[0] * 0.1f;
            skyverts[1].t = topSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = leftSky.f[0] * 0.1f;
            skyverts[2].t = leftSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], tlSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], topSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], leftSkyFrac);
            break;

        case 14:
            numverts = 5;
            skyverts[0].x = blSky.f[0] * scale;
            skyverts[0].y = blSky.f[1] * scale;
            skyverts[0].z = blSky.f[2] * scale;
            skyverts[1].x = tlSky.f[0] * scale;
            skyverts[1].y = tlSky.f[1] * scale;
            skyverts[1].z = tlSky.f[2] * scale;
            skyverts[2].x = trSky.f[0] * scale;
            skyverts[2].y = trSky.f[1] * scale;
            skyverts[2].z = trSky.f[2] * scale;
            skyverts[3].x = rightSky.f[0] * scale;
            skyverts[3].y = rightSky.f[1] * scale;
            skyverts[3].z = rightSky.f[2] * scale;
            skyverts[4].x = bottomSky.f[0] * scale;
            skyverts[4].y = bottomSky.f[1] * scale;
            skyverts[4].z = bottomSky.f[2] * scale;
            skyverts[0].s = blSky.f[0] * 0.1f;
            skyverts[0].t = blSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = tlSky.f[0] * 0.1f;
            skyverts[1].t = tlSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = trSky.f[0] * 0.1f;
            skyverts[2].t = trSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = rightSky.f[0] * 0.1f;
            skyverts[3].t = rightSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[4].s = bottomSky.f[0] * 0.1f;
            skyverts[4].t = bottomSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], blSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], tlSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], trSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], rightSkyFrac);
            skyChooseCloudVtxColour(&skyverts[4], bottomSkyFrac);
            break;

        case 13:
            numverts = 5;
            skyverts[0].x = tlSky.f[0] * scale;
            skyverts[0].y = tlSky.f[1] * scale;
            skyverts[0].z = tlSky.f[2] * scale;
            skyverts[1].x = trSky.f[0] * scale;
            skyverts[1].y = trSky.f[1] * scale;
            skyverts[1].z = trSky.f[2] * scale;
            skyverts[2].x = brSky.f[0] * scale;
            skyverts[2].y = brSky.f[1] * scale;
            skyverts[2].z = brSky.f[2] * scale;
            skyverts[3].x = bottomSky.f[0] * scale;
            skyverts[3].y = bottomSky.f[1] * scale;
            skyverts[3].z = bottomSky.f[2] * scale;
            skyverts[4].x = leftSky.f[0] * scale;
            skyverts[4].y = leftSky.f[1] * scale;
            skyverts[4].z = leftSky.f[2] * scale;
            skyverts[0].s = tlSky.f[0] * 0.1f;
            skyverts[0].t = tlSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = trSky.f[0] * 0.1f;
            skyverts[1].t = trSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = brSky.f[0] * 0.1f;
            skyverts[2].t = brSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = bottomSky.f[0] * 0.1f;
            skyverts[3].t = bottomSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[4].s = leftSky.f[0] * 0.1f;
            skyverts[4].t = leftSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], tlSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], trSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], brSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], bottomSkyFrac);
            skyChooseCloudVtxColour(&skyverts[4], leftSkyFrac);
            break;

        case 11:
            numverts = 5;
            skyverts[0].x = brSky.f[0] * scale;
            skyverts[0].y = brSky.f[1] * scale;
            skyverts[0].z = brSky.f[2] * scale;
            skyverts[1].x = blSky.f[0] * scale;
            skyverts[1].y = blSky.f[1] * scale;
            skyverts[1].z = blSky.f[2] * scale;
            skyverts[2].x = tlSky.f[0] * scale;
            skyverts[2].y = tlSky.f[1] * scale;
            skyverts[2].z = tlSky.f[2] * scale;
            skyverts[3].x = topSky.f[0] * scale;
            skyverts[3].y = topSky.f[1] * scale;
            skyverts[3].z = topSky.f[2] * scale;
            skyverts[4].x = rightSky.f[0] * scale;
            skyverts[4].y = rightSky.f[1] * scale;
            skyverts[4].z = rightSky.f[2] * scale;
            skyverts[0].s = brSky.f[0] * 0.1f;
            skyverts[0].t = brSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = blSky.f[0] * 0.1f;
            skyverts[1].t = blSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = tlSky.f[0] * 0.1f;
            skyverts[2].t = tlSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = topSky.f[0] * 0.1f;
            skyverts[3].t = topSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[4].s = rightSky.f[0] * 0.1f;
            skyverts[4].t = rightSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], brSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], blSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], tlSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], topSkyFrac);
            skyChooseCloudVtxColour(&skyverts[4], rightSkyFrac);
            break;

        case 7:
            numverts = 5;
            skyverts[0].x = trSky.f[0] * scale;
            skyverts[0].y = trSky.f[1] * scale;
            skyverts[0].z = trSky.f[2] * scale;
            skyverts[1].x = brSky.f[0] * scale;
            skyverts[1].y = brSky.f[1] * scale;
            skyverts[1].z = brSky.f[2] * scale;
            skyverts[2].x = blSky.f[0] * scale;
            skyverts[2].y = blSky.f[1] * scale;
            skyverts[2].z = blSky.f[2] * scale;
            skyverts[3].x = leftSky.f[0] * scale;
            skyverts[3].y = leftSky.f[1] * scale;
            skyverts[3].z = leftSky.f[2] * scale;
            skyverts[4].x = topSky.f[0] * scale;
            skyverts[4].y = topSky.f[1] * scale;
            skyverts[4].z = topSky.f[2] * scale;
            skyverts[0].s = trSky.f[0] * 0.1f;
            skyverts[0].t = trSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[1].s = brSky.f[0] * 0.1f;
            skyverts[1].t = brSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[2].s = blSky.f[0] * 0.1f;
            skyverts[2].t = blSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[3].s = leftSky.f[0] * 0.1f;
            skyverts[3].t = leftSky.f[2] * 0.1f + g_SkyCloudOffset;
            skyverts[4].s = topSky.f[0] * 0.1f;
            skyverts[4].t = topSky.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&skyverts[0], trSkyFrac);
            skyChooseCloudVtxColour(&skyverts[1], brSkyFrac);
            skyChooseCloudVtxColour(&skyverts[2], blSkyFrac);
            skyChooseCloudVtxColour(&skyverts[3], leftSkyFrac);
            skyChooseCloudVtxColour(&skyverts[4], topSkyFrac);
            break;

        default:
            return gdl;
    }

    gDPPipeSync(gdl++);

    texSelect(&gdl, &g_ImagesSkyWater[fogGetCurrentEnvironmentp()->SkyImageId], TEXBLEND_OPAQUE, TEXZBUF_OFF, 2);


    gDPSetEnvColor(gdl++, fogGetCurrentEnvironmentp()->Red, fogGetCurrentEnvironmentp()->Green, fogGetCurrentEnvironmentp()->Blue, 0xff);
    gDPSetCombineLERP(gdl++,
            SHADE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, SHADE,
            SHADE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, SHADE);

    {
        Mtxf skyViewProj;
        Mtxf skyMtx;

        mtx4Mult(camGetProjectionMtxf(), camGetWorldToScreenMtxf(), &skyViewProj);
        guScaleF(g_SkyInvScaleMtx.m, 1.0f / scale, 1.0f / scale, 1.0f / scale);
        mtx4Mult(&skyViewProj, &g_SkyInvScaleMtx, &skyMtx);


        gdl = skyDrawPlane(gdl, skyverts, numverts, &skyMtx, scale,
                &g_ImagesSkyWater[fogGetCurrentEnvironmentp()->SkyImageId], skyRetileClouds,
                1.0f, skyChooseCloudVtxColour);
    }

    return gdl;
}


void skyProjectVertex(const SkyVtx *vtx, const Mtxf *mtx, u16 perspScale, f32 sScale, f32 tScale, SkyScreenVtx *out)
{
    f32 clip[4];
    f32 t;
    f32 s;
    f32 invW;
    f32 clampedInvW;
    f32 ndc[4];
    f32 screen[4];
    f32 mult;

    mult = perspScale / 65536.0f;

    clip[0] = (vtx->x * mtx->m[0][0] + vtx->y * mtx->m[1][0] + vtx->z * mtx->m[2][0]) + mtx->m[3][0];
    clip[1] = (vtx->x * mtx->m[0][1] + vtx->y * mtx->m[1][1] + vtx->z * mtx->m[2][1]) + mtx->m[3][1];
    clip[2] = (vtx->x * mtx->m[0][2] + vtx->y * mtx->m[1][2] + vtx->z * mtx->m[2][2]) + mtx->m[3][2];
    clip[3] = (vtx->x * mtx->m[0][3] + vtx->y * mtx->m[1][3] + vtx->z * mtx->m[2][3]) + mtx->m[3][3];

    s = vtx->s * (sScale / 65536.0f);
    t = vtx->t * (tScale / 65536.0f);

    if (clip[3] == 0.0f)
    {
        invW = 32767.0f;
    }
    else
    {
        invW = 1.0f / (clip[3] * mult);
    }

    clampedInvW = invW;
    if (clampedInvW < 0.0f) { clampedInvW = 32767.0f; }

    ndc[0] = clip[0] * clampedInvW * mult;
    ndc[1] = clip[1] * clampedInvW * mult;
    ndc[2] = clip[2] * clampedInvW * mult;
    ndc[3] = clip[3] * clampedInvW * mult;

    // Screen x in physical (hi-res) quarter pixels: the RDP triangles are built from it
    screen[0] = ndc[0] * (VI_SX(camGetScreenWidth()) * 2) + (VI_SX(camGetScreenWidth()) * 2 + VI_SX(camGetScreenLeft()) * 4);
    screen[1] = -ndc[1] * (camGetScreenHeight() * 2) + (camGetScreenHeight() * 2 + camGetScreenTop() * 4);

    screen[2] = ndc[2] * 511.0f + 511.0f;
    screen[3] = ndc[3] * 0 + 0;

    screen[0] = skyClamp(screen[0], -4090.0f, 4090.0f);
    screen[1] = skyClamp(screen[1], -4090.0f, 4090.0f);
    screen[2] = skyClamp(screen[2], 0.0f, 32767.0f);
    screen[3] = skyClamp(screen[3], 0.0f, 32767.0f);

    out->clipX = clip[0];
    out->clipY = clip[1];
    out->clipZ = clip[2];
    out->clipW = clip[3];
    out->s = s;
    out->t = t;
    out->screenX = screen[0];
    out->screenY = screen[1] - fogGetCurrentEnvironmentp()->WaterConcavity * 4.0f;
    out->screenZ = screen[2];
    out->invW = invW;

    out->r = vtx->r;
    out->g = vtx->g;
    out->b = vtx->b;
    out->a = vtx->a;
}

bool skyVerticesAreTheSame(const SkyScreenVtx *a, const SkyScreenVtx *b)
{
    f32 dx;
    f32 dy;

    dx = a->screenX - b->screenX;
    dy = a->screenY - b->screenY;
    return sqrtf((dx * dx) + (dy * dy)) < 1.0f ? TRUE : FALSE;
}


