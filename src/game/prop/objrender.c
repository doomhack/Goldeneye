/**
 * Drawing objects and doors, and whether things are in view.
 *
 * propRender calls objRenderProp for each object, weapon or door prop, once for the
 * opaque pass and once for the translucent pass. objRenderProp:
 *
 * - fades the object out in near fog, and while it is about to respawn. A faded object
 *   (and one flagged always in front) is drawn only on the translucent pass;
 * - scissors it to the screen area of the rooms it is in;
 * - darkens it a little for each shot it has taken, and tints it towards the fog colour;
 * - draws its model, then its children (a gun on a table, a knife in a crate).
 *
 * Monitors draw their screens first, doors draw with the cull mode for the side they
 * open to, and a damaged object (whose vertices have been bent) draws both sides of each
 * face and is shaded darker the more damaged it is.
 *
 * Also here: the gun beams of guards, drone guns and the other players
 * (weaponRenderTracers); whether a position is in view (posIsOnScreen); and the colour
 * an object takes from the light of the floor it stands on.
 */

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/objrender.h"
#include "game/gfx/texcmds.h"
#include "game/prop/explosion.h"
#include "game/prop/monitor.h"
#include "game/prop/objdamage.h"
#include "game/prop/prop.h"
#include "game/prop/propobj.h"
#include "game/chr/chr.h"
#include "game/model/chrobjdata.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/world/bg.h"
#include "game/world/bgfog.h"

/* Which passes ModelRenderData.flags draws */
#define RENDERPASS_OPA 1
#define RENDERPASS_XLU 2

/**
 * How model.c combines an object's colours (ModelRenderData.PropType).
 *
 * FADING: the low byte of the env colour is the alpha of the whole object; bit 0xff00 set
 * also uses the vertex shading (a damaged object).
 * SOLID: the low byte of the env colour darkens a damaged object; bits 8-15 are the
 * opacity of glass.
 */
#define OBJRENDER_FADING (PROP_TYPE_WEAPON + 1)
#define OBJRENDER_SOLID  (PROP_TYPE_SMOKE + 1)

/* Each shot taken darkens an object by this much (of 255) and adds this to its shade
   alpha */
#define SHOT_DARKEN      0x15
#define SHOT_SHADE_ALPHA 0xf

/* Nothing further than this from the camera is drawn */
#define MAX_DRAW_DIST 32000


/**
 * Draw the gun beams (tracers) of the guards, the drone guns and the other players. The
 * current player's own beams are drawn with their gun.
 */
Gfx *weaponRenderTracers(Gfx *gdl)
{
    for (PropRecord *prop = propsGetActiveTail(); prop != NULL; prop = prop->prev)
    {
        if (prop->type == PROP_TYPE_CHR)
        {
            gdl = beamRender(gdl, &prop->chr->beams[0], TRUE);
            gdl = beamRender(gdl, &prop->chr->beams[1], TRUE);
        }
        else if (prop->type == PROP_TYPE_OBJ)
        {
            if (prop->obj->type == PROPDEF_AUTOGUN)
            {
                gdl = beamRender(gdl, ((AutogunRecord *)prop->obj)->beam, TRUE);
            }
        }
        else if (prop->type == PROP_TYPE_VIEWER)
        {
            if (prop->voidp != NULL && playerGetIndexForProp(prop) != playerGetCurrentNum())
            {
                gdl = beamRender(gdl, &prop->chr->beams[0], TRUE);
                gdl = beamRender(gdl, &prop->chr->beams[1], TRUE);
            }
        }
    }

    return gdl;
}


/* How a monitor screen uses the z-buffer: not at all for an object always drawn in
   front, and as a decal on a fixed monitor (decalflags) */
static s32 monitorGetZbufferMode(ObjectRecord *obj, u32 decalflags)
{
    if (obj->flags2 & PROPFLAG2_00010000)
    {
        return TEXZBUF_OFF;
    }

    if (obj->flags & decalflags)
    {
        return 8;
    }

    return TEXZBUF_ON;
}


/* Run and draw a monitor's screens. A multi-screen monitor has four; its last three are
   decals on a special-function monitor too. */
static Gfx *objRenderScreens(ObjectRecord *obj, Gfx *gdl)
{
    Model *model = obj->model;
    ModelNode **screens = model->obj->Switches;

    if (obj->type == PROPDEF_MONITOR)
    {
        MonitorRecord *mon = &((MonitorObjRecord *)obj)->Monitor;

        gdl = monitorTickAndDraw(model, screens[0], mon, gdl, monitorGetZbufferMode(obj, PROPFLAG_FIXED_MONITOR), TEXBLEND_OPAQUE);
    }
    else if (obj->type == PROPDEF_MULTI_MONITOR)
    {
        MonitorRecord *mons = ((MultiMonitorObjRecord *)obj)->Monitor;
        s32 zbuffermode = monitorGetZbufferMode(obj, PROPFLAG_FIXED_MONITOR);

        gdl = monitorTickAndDraw(model, screens[0], &mons[0], gdl, zbuffermode, TEXBLEND_OPAQUE);

        zbuffermode = monitorGetZbufferMode(obj, PROPFLAG_FIXED_MONITOR | PROPFLAG_SPECIAL_FUNC);

        gdl = monitorTickAndDraw(model, screens[1], &mons[1], gdl, zbuffermode, TEXBLEND_OPAQUE);
        gdl = monitorTickAndDraw(model, screens[2], &mons[2], gdl, zbuffermode, TEXBLEND_OPAQUE);
        gdl = monitorTickAndDraw(model, screens[3], &mons[3], gdl, zbuffermode, TEXBLEND_OPAQUE);
    }

    return gdl;
}


/* Whether the object's vertices have been bent by damage: its model uses its own copy */
static bool objIsDeformed(ObjectRecord *obj)
{
    ModelNode *node = objFindDlCollisionNode(obj);

    if (node == NULL || node->Data == NULL)
    {
        return FALSE;
    }

    union ModelRoData *rodata = node->Data;
    union ModelRwData *rwdata = (union ModelRwData *)&obj->model->datas[rodata->DisplayListCollisions.RwDataIndex];

    return rodata->DisplayListCollisions.Vertices != rwdata->DisplayListCollisions.Vertices;
}


/* Set the cull mode and the damage shading */
static void objSetCullAndDamage(PropRecord *prop, ModelRenderData *mrData)
{
    ObjectRecord *obj = prop->obj;

    if (obj->type == PROPDEF_DOOR)
    {
        mrData->cullmode = (prop->door->doorFlags & DOORFLAG_FLIP) ? CULLMODE_FRONT : CULLMODE_BACK;

        if (mrData->PropType == OBJRENDER_SOLID)
        {
            mrData->envcolour.word &= 0xffffff00;
        }

        return;
    }

    bool deformed = objIsDeformed(obj);
    s32 damage = objGetDestroyedLevel(obj);

    if (damage == 0 || !deformed)
    {
        mrData->cullmode = CULLMODE_BACK;

        if (mrData->PropType == OBJRENDER_SOLID)
        {
            mrData->envcolour.word &= 0xffffff00;
        }

        return;
    }

    // Bent faces can show their backs
    mrData->cullmode = CULLMODE_NONE;

    if (mrData->PropType == OBJRENDER_SOLID)
    {
        s32 alpha = damage * 50 + 100;

        if (alpha >= 256)
        {
            alpha = 255;
        }

        mrData->envcolour.word &= 0xffffff00;
        mrData->envcolour.word |= alpha;
    }
    else if (damage > 0)
    {
        mrData->envcolour.word |= 0xff00;
    }
}


/**
 * Draw an object's model, then its children. Objects with PROPFLAG_00000200 (glass with
 * environment mapping) are drawn in the world's frame with the background's projection,
 * so their reflections stay fixed to the world.
 */
static void objRenderModel(PropRecord *prop, ModelRenderData *mrData, s32 xlupass)
{
    if (!(prop->flags & PROPFLAG_ONSCREEN))
    {
        return;
    }

    ObjectRecord *obj = prop->obj;
    Model *model = obj->model;
    bool worldspace = (obj->flags & PROPFLAG_00000200) && camGetProjViewMtx() != NULL;
    Gfx *gdl = mrData->gdl;

    if (mrData->flags & RENDERPASS_OPA)
    {
        gdl = objRenderScreens(obj, gdl);
    }

    if (obj->type == PROPDEF_DOOR)
    {
        gSPClearGeometryMode(gdl++, G_CULL_BOTH);
    }

    objSetCullAndDamage(prop, mrData);

    if (worldspace)
    {
        gSPMatrix(gdl++, camGetProjViewMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    }

    mrData->gdl = gdl;
    modelRender(mrData, model);
    gdl = mrData->gdl;

    if (obj->type == PROPDEF_DOOR)
    {
        gSPClearGeometryMode(gdl++, G_CULL_BOTH);
    }

    if (obj->state & (1 << xlupass))
    {
        gdl = impactsRenderOnProp(gdl, prop, xlupass);
    }

    if (worldspace)
    {
        gSPMatrix(gdl++, camGetProjectionMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    }

    mrData->gdl = gdl;

    for (PropRecord *child = prop->child; child != NULL; child = child->prev)
    {
        objRenderModel(child, mrData, xlupass);
    }

    // The display lists only point at the matrices, and the RSP reads them after the
    // frame is built. So after the last pass they are converted to its fixed point in
    // place.
    if (xlupass)
    {
        if (worldspace)
        {
            camMtxsToRoomSpace((Mtxf *)model->render_pos, model->obj->numMatrices);
        }
        else
        {
            mtxsF2L(model->render_pos, model->obj->numMatrices);
        }
    }
}


/* The object's shading: its floor's colour, darker for each shot taken, then tinted
   towards the fog colour (by fogfrac) */
static u32 objGetRenderShade(ObjectRecord *obj, s32 fogfrac, rgba_f32 *fogcolour)
{
    s32 shots = objGetShotsTaken(obj);
    s32 brightness = 0xff - shots * SHOT_DARKEN;
    rgba_s32 shade;

    if (brightness < 0)
    {
        brightness = 0;
    }

    shade.r = (obj->shadecol.r * brightness) >> 8;
    shade.g = (obj->shadecol.g * brightness) >> 8;
    shade.b = (obj->shadecol.b * brightness) >> 8;
    shade.a = obj->shadecol.a + shots * SHOT_SHADE_ALPHA;

    if (shade.a >= 0x100)
    {
        shade.a = 0xff;
    }

    shadingLerp(&shade, fogfrac, fogcolour);

    return (shade.r << 24) | (shade.g << 16) | (shade.b << 8) | shade.a;
}


/**
 * Draw an object, weapon or door prop on the opaque (xlupass 0) or translucent (1) pass.
 */
Gfx *objRenderProp(PropRecord *prop, Gfx *gdl, s32 xlupass)
{
    ObjectRecord *obj = prop->obj;
    ModelRenderData mrData = {.zbufferenabled = TRUE, .flags = RENDERPASS_OPA | RENDERPASS_XLU};
    rgba_f32 fogcolour;
    s32 fogfrac = fogGetPropDistColor(prop, &fogcolour);
    s32 alpha = 0xff;
    bbox2d screenbox;

    if (fogfrac == 0)
    {
        return gdl;
    }

    // Fade with near fog, and while respawning
    if ((u8)obj->type != PROPDEF_TINTED_GLASS)
    {
        f32 visfrac = propGetFogFade(prop, modelGetRadius(obj->model));

        if ((s32)prop->timetoregen > 0 && (s32)prop->timetoregen < CHROBJ_TIMETOREGEN)
        {
            visfrac *= (CHROBJ_TIMETOREGEN_F - (f32)prop->timetoregen) / CHROBJ_TIMETOREGEN_F;
        }

        alpha = (s32)(visfrac * 255.0f);

        if (alpha <= 0)
        {
            return gdl;
        }
    }

    // A faded object, or one always in front, is drawn whole on the translucent pass
    if (alpha < 0xff || (obj->flags2 & PROPFLAG2_00010000))
    {
        if (xlupass == 0)
        {
            return gdl;
        }

        mrData.flags = RENDERPASS_OPA | RENDERPASS_XLU;
    }
    else
    {
        mrData.flags = xlupass == 0 ? RENDERPASS_OPA : RENDERPASS_XLU;
    }

    if (propGetRoomsBbox2d(prop, &screenbox) && !(obj->flags2 & PROPFLAG2_04000000))
    {
        gdl = bgScissorCurrentPlayerViewF(gdl, screenbox.min.x, screenbox.min.y, screenbox.max.x, screenbox.max.y);
    }
    else
    {
        gdl = bgScissorCurrentPlayerViewDefault(gdl);
    }

    mrData.zbufferenabled = !(obj->flags2 & PROPFLAG2_00010000);
    mrData.gdl = gdl;

    if (alpha < 0xff)
    {
        mrData.PropType = OBJRENDER_FADING;
        mrData.envcolour.word = alpha;
    }
    else
    {
        mrData.PropType = OBJRENDER_SOLID;

        if (obj->type == PROPDEF_TINTED_GLASS)
        {
            mrData.envcolour.word = ((TintedGlassRecord *)obj)->calculatedopacity << 8;
        }
        else if (obj->type == PROPDEF_DOOR && (((DoorRecord *)obj)->doorFlags & DOORFLAG_WINDOWED))
        {
            mrData.envcolour.word = ((DoorRecord *)obj)->calculatedopacity << 8;
        }
        else
        {
            mrData.envcolour.word = 0;
        }
    }

    mrData.fogcolour.word = objGetRenderShade(obj, fogfrac, &fogcolour);

    objRenderModel(prop, &mrData, xlupass);

    return mrData.gdl;
}


/**
 * Where an object on screen is, in the view's frame: its root position (viewpos) and the
 * x and y ranges of its model (min, max). FALSE when it is off screen or behind the camera.
 */
bool objGetOnscreenRenderBounds(PropRecord *prop, coord3d *viewpos, coord2d *xrange, coord2d *yrange)
{
    if (!(prop->flags & PROPFLAG_ONSCREEN))
    {
        return FALSE;
    }

    ObjectRecord *obj = prop->obj;
    Mtxf *matrix = modelGetRootMtx(obj->model);

    viewpos->z = matrix->m[3][2];

    if (!(viewpos->z < 0))
    {
        return FALSE;
    }

    viewpos->x = matrix->m[3][0];
    viewpos->y = matrix->m[3][1];

    yrange->f[0] = 0;
    yrange->f[1] = 0;
    xrange->f[0] = 0;
    xrange->f[1] = 0;

    modelGetXYExtents(obj->model, &xrange->f[1], &xrange->f[0], &yrange->f[1], &yrange->f[0]);

    return TRUE;
}


/**
 * The screen area covered by the rooms the prop is in, to scissor its drawing to. FALSE
 * when none of its rooms has one.
 */
bool propGetRoomsBbox2d(PropRecord *prop, bbox2d *bbox)
{
    s32 rooms[8];
    bool found = FALSE;
    bbox2d roombox;

    propGetRoomIds(prop, rooms);

    for (s32 *room = rooms; *room >= 0; room++)
    {
        if (!bgGet2dBboxByRoomId(*room, &roombox))
        {
            continue;
        }

        if (!found)
        {
            *bbox = roombox;
        }
        else
        {
            if (bbox->min.x > roombox.min.x)
            {
                bbox->min.x = roombox.min.x;
            }

            if (bbox->min.y > roombox.min.y)
            {
                bbox->min.y = roombox.min.y;
            }

            if (bbox->max.x < roombox.max.x)
            {
                bbox->max.x = roombox.max.x;
            }

            if (bbox->max.y < roombox.max.y)
            {
                bbox->max.y = roombox.max.y;
            }
        }

        found = TRUE;
    }

    return found;
}


/* How far into near fog a prop's distance is, scaled up for a small prop (size is its
   radius): beyond the start of the fog, the distance grows by 100 / size */
static f32 fogScaleDist(NearFogValues *fog, f32 dist, f32 size)
{
    return (((dist - fog->MaxObfuscationRange) * 100.0f) / size + fog->MaxObfuscationRange) * camGetLodScaleZ();
}


/**
 * How visible a prop is through near fog: 1 when clear of it, fading to 0 at the
 * level's furthest view distance. Small props fade out sooner.
 */
f32 propGetFogFade(PropRecord *prop, f32 size)
{
    NearFogValues *fog = fogGetNearFogValuesP();

    if (fog == NULL || !(fog->MaxObfuscationRange < prop->zDepth))
    {
        return 1.0f;
    }

    f32 dist = fogScaleDist(fog, prop->zDepth, size);

    if (fog->MaxVisRange <= dist)
    {
        return 0.0f;
    }

    if (fog->NearFog < dist)
    {
        return (fog->MaxVisRange - dist) / (fog->MaxVisRange - fog->NearFog);
    }

    return 1.0f;
}


/* Whether a position of the given size is nearer than the level's furthest view
   distance, measured the same way as propGetFogFade */
static bool posIsBeforeFogEnd(coord3d *pos, f32 size)
{
    NearFogValues *fog = fogGetNearFogValuesP();

    if (fog == NULL)
    {
        return TRUE;
    }

    coord3d *campos = playerGetPos();
    Mtxf *mtx = camGetWorldToScreenMtxf();
    coord3d rel;

    rel.x = pos->x - campos->x;
    rel.y = pos->y - campos->y;
    rel.z = pos->z - campos->z;

    f32 depth = rel.f[0] * mtx->m[0][0] + rel.f[1] * mtx->m[0][1] + rel.f[2] * mtx->m[0][2];

    if (depth > fog->MaxObfuscationRange)
    {
        f32 scalez = camGetLodScaleZ();

        depth = ((depth - fog->MaxObfuscationRange) * 100 / size + fog->MaxObfuscationRange) * scalez;

        if (depth >= fog->MaxVisRange)
        {
            return FALSE;
        }
    }

    return TRUE;
}


/* Whether any of the rooms the prop is in is visible this frame */
static bool propIsInVisibleRoom(PropRecord *prop)
{
    s32 rooms[8];

    propGetRoomIds(prop, rooms);

    for (s32 *room = rooms; *room >= 0; room++)
    {
        if (bgRoomIsVisible(*room))
        {
            return TRUE;
        }
    }

    return FALSE;
}


/**
 * Whether a sphere around pos is in view: the prop is in a visible room, the sphere is
 * not hidden by fog (nor, with checkFogEnd, beyond the furthest view distance), it is
 * within the screen area of the prop's rooms, and it is no more than MAX_DRAW_DIST away.
 */
bool posIsOnScreen(PropRecord *prop, coord3d *pos, f32 radius, bool checkFogEnd)
{
    bbox2d bbox;
    bool onscreen;

    if (!propIsInVisibleRoom(prop))
    {
        return FALSE;
    }

    if (!fogPositionIsVisibleThroughFog(pos, radius) || (checkFogEnd && !posIsBeforeFogEnd(pos, radius)))
    {
        return FALSE;
    }

    if (propGetRoomsBbox2d(prop, &bbox))
    {
        onscreen = camIsPosInScreenBox(pos, radius, &bbox);
    }
    else
    {
        onscreen = camIsPosInScreen(pos, radius);
    }

    if (onscreen)
    {
        coord3d *campos = playerGetPos();
        f32 xdiff = pos->x - campos->x;
        f32 ydiff = pos->y - campos->y;
        f32 zdiff = pos->z - campos->z;

        if (xdiff * xdiff + ydiff * ydiff + zdiff * zdiff > MAX_DRAW_DIST * MAX_DRAW_DIST)
        {
            onscreen = FALSE;
        }
    }

    return onscreen;
}


/**
 * The light of the floor tile the prop stands on, as a shade colour: the colour keeps
 * its hue and saturation (its lowest channel is taken off all three) at half brightness,
 * and the alpha is 3/4 of how dark the tile is.
 */
void objGetTileShading(PropRecord *prop, rgba_u8 *colour)
{
    u8 *col = colour->rgba;
    s32 lo = 0;
    s32 mid = 0;
    s32 hi = 0;

    stanGetTileColour(prop->stan, col);

    s32 luma = (col[0] * 79 + col[1] * 156 + col[2] * 21) >> 8;
    col[3] = (255 - luma) * 0.75f;

    // Sort the channels
    if (col[1] > col[0])
    {
        hi = 1;
    }
    else
    {
        lo = 1;
    }

    if (col[2] > col[hi])
    {
        mid = hi;
        hi = 2;
    }
    else if (col[2] > col[lo])
    {
        mid = 2;
    }
    else
    {
        mid = lo;
        lo = 2;
    }

    if (col[hi] > 0)
    {
        s32 newmid = col[mid] * (col[hi] - col[lo]) / col[hi];
        s32 newhi = col[hi] - col[lo];

        col[lo] = 0;
        col[mid] = newmid;
        col[hi] = newhi;
    }

    col[0] >>= 1;
    col[1] >>= 1;
    col[2] >>= 1;
}


/**
 * The colour an object (or door) is shaded towards: its floor tile's light, at half
 * brightness. An illuminated object keeps the colour it has.
 */
void objGetShadeColour(PropRecord *prop, rgba_u8 *colour)
{
    if (prop->obj->flags & PROPFLAG_ILLUMINATED)
    {
        return;
    }

    objGetTileShading(prop, colour);
    colour->r >>= 1;
    colour->g >>= 1;
    colour->b >>= 1;
}


/* Move a shade colour an eighth of the way towards another (rounding up), so a moving
   object's shade changes smoothly */
void objUpdateShading(rgba_u8 *dest, rgba_u8 *src)
{
    for (s32 i = 0; i < 4; i++)
    {
        dest->rgba[i] += (src->rgba[i] - dest->rgba[i] + 7) >> 3;
    }
}


/**
 * Tint a shade colour towards a fog colour (0 to 1 channels) by the fog colour's alpha.
 * Only when enable is 1 (fogGetPropDistColor's result when there is fog to tint by).
 * The fog colour is scaled to 0-255 in place.
 */
void shadingLerp(rgba_s32 *dest, s32 enable, rgba_f32 *src)
{
    if (enable != 1)
    {
        return;
    }

    src->r *= 255.0f;
    src->g *= 255.0f;
    src->b *= 255.0f;

    dest->r = (s32)(src->a * (src->r - dest->r) + dest->r);
    dest->g = (s32)(src->a * (src->g - dest->g) + dest->g);
    dest->b = (s32)(src->a * (src->b - dest->b) + dest->b);
    dest->a = (s32)(src->a * (255.0f - dest->a) + dest->a);
}
