/**
 * Objects at run time: everything a prop does after it is created. Doors are in door.c.
 *
 * - objTick: the per-frame tick for every object and door (doorTick): flying objects
 *   (projectileTick, in projectile.c), CCTV, autoguns, vehicles, aircraft, the tank,
 *   weapons such as mines (chrobjWeaponTick). It is about 1,600 lines long.
 * - Drawing (chrobjRenderProp, objRenderModel); monitor screens are in monitor.c.
 * - Damage is in objdamage.c, and shots hitting objects in objhit.c.
 * - Pickups: collecting weapons, ammo, keys and hats (propPickupByPlayer).
 * - Weapon and hat slots: pools of props created at run time for dropped guns and hats.
 */

#include <ultra64.h>
#include <math.h>
#include <PR/libaudio.h>
#include <assets/oddtextures.h>
#include <bondgame.h>
#include <boss.h>
#include <limits.h>
#include <music.h>
#include <memp.h>
#include <snd.h>
#include <gbi_extension.h>
#include "game/menu/front.h"
#include "game/prop/propobj.h"
#include "assets/obseg/text/LpropobjE.h"
#include "game/world/bg.h"
#include "game/world/bgfog.h"
#include "bondaicommands.h"
#include "game/player/bondinv.h"
#include "game/player/bondview.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/chraction.h"
#include "game/model/chrobjdata.h"
#include "game/prop/explosion.h"
#include "fr.h"
#include "game/prop/glass.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/player/gun.h"
#include "game/gfx/imagebank.h"
#include "game/level/lv.h"
#include "game/text/language.h"
#include "game/math/round.h"
#include "game/math/trig.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/mission/objective.h"
#include "game/player/player.h"
#include "game/math/quaternion.h"
#include "random.h"
#include "game/nav/geometry.h"
#include "game/nav/stan.h"
#include "game/gfx/texcmds.h"
#include "game/world/surface.h"
#include "game/text/text.h"
#include "game/gfx/vtxstore.h"
#include "game/math/physics.h"
#include "game/gfx/dyn.h"
#include "game/math/line_tri_intersect.h"
#include "game/prop/prop.h"
#include "game/prop/setup.h"
#include "game/prop/monitor.h"
#include "str.h"
#include "game/prop/alarm.h"
#include "game/prop/door.h"
#include "game/audio/sfxvolume.h"
#include "game/prop/projectile.h"
#include "game/prop/objdamage.h"
#include "game/prop/objhit.h"



struct LinkRecord *g_LevelLoadPropSwitch = NULL;
LockDoorRecord *g_LevelLoadPropLockDoor = NULL;
SafeObjectRecord *g_LevelLoadPropSafeItem = NULL;

/*
* Set on level load.
*/
f32 g_SoloAmmoMultiplier = 1.0;


// Forward declarations.

void objRenderModel(PropRecord *prop, ModelRenderData *, s32 xlupass);
bool chrobjSeparatingAxisTheorem(coord2d* rect1, s32 numvertices0, coord2d* rect2, s32 numvertices1);
AmmoCrateRecord *ammocrateAllocate(void);

// End forward declarations.

ModelRoData_BoundingBoxRecord *chrobjGetBboxFromObjFile(ModelFileHeader *obj)
{
    ModelNode *mdlnext;

    if (obj->RootNode->Child)
    {
        //for each next node, check for BBox
        for (mdlnext = obj->RootNode->Child; mdlnext; mdlnext = mdlnext->Next)
        {
            if (mdlnext->Opcode == MODELNODE_OPCODE_BBOX)
            {
                return &mdlnext->Data->BoundingBox;
            }
        }

        //none found, check FIRST child
        if (obj->RootNode->Child->Child)
        {
            //for each next node, check for BBox
            for (mdlnext = obj->RootNode->Child->Child; mdlnext; mdlnext = mdlnext->Next)
            {
                if (mdlnext->Opcode == MODELNODE_OPCODE_BBOX)
                {
                    return &mdlnext->Data->BoundingBox;
                }
            }
        }
    }
    return NULL;
}


struct ModelRoData_BoundingBoxRecord* chrobjGetBboxFromObjectRecord(ObjectRecord *obj)
{
    return (struct ModelRoData_BoundingBoxRecord *)chrobjGetBboxFromObjFile(obj->model->obj);
}


void objGetTileShading(PropRecord *prop, rgba_u8 *colour)
{
    u8 *col = colour->rgba;
    s32 tmp;
    s32 min;
    s32 med;
    s32 max;
    s32 tmp2;
    s32 range;

    stanGetTileColour(prop->stan, col);

    tmp = (col[0] * 79 + col[1] * 156 + col[2] * 21) >> 8;
    col[3] = (255 - tmp) * 0.75f;

    max = 0;
    min = 0;
	med = 0;

	if (col[1] > col[0]) {
		max = 1;
	} else {
		min = 1;
	}

	if (col[2] > col[max]) {
		med = max;
		max = 2;
	} else if (col[2] > col[min]) {
		med = 2;
	} else {
		med = min;
		min = 2;
	}

	if (col[max] > 0) {
		tmp2 = col[med] * (col[max] - col[min]) / col[max];
		range = col[max] - col[min];
		col[min] = 0;
		col[med] = tmp2;
		col[max] = range;
	}

    col[0] >>= 1;
    col[1] >>= 1;
    col[2] >>= 1;
}


/**
 * The colour an object (or door) is shaded towards: its floor tile's light, at half
 * brightness. An illuminated object keeps the colour it has.
 */
void objGetShadeColour(PropRecord *prop, rgba_u8 *color)
{
    if (prop->obj->flags & PROPFLAG_ILLUMINATED)
    {
        return;
    }

    objGetTileShading(prop, color);
    color->r >>= 1;
    color->g >>= 1;
    color->b >>= 1;
}


void objUpdateShading(rgba_u8 *dest, rgba_u8 *src)
{
    s32 val_diff;
    s32 val_new;
    s32 i;

    for (i = 0; i < 4; i++)
    {
        val_diff = (src->rgba[i] - dest->rgba[i]);
        val_new = dest->rgba[i] + ((val_diff + 7) >> 3);
        dest->rgba[i] = val_new;
    }
}


void shadingLerp(rgba_s32* dest, s32 enable, rgba_f32* src)
{
    if (enable == 1)
    {
        src->r *= 255.0f;
        src->g *= 255.0f;
        src->b *= 255.0f;

        if (1) { dest->r = (s32)((src->a * (src->r - dest->r)) + dest->r); }
        if (1) { dest->g = (s32)((src->a * (src->g - dest->g)) + dest->g); }
        if (1) { dest->b = (s32)((src->a * (src->b - dest->b)) + dest->b); }
        dest->a = (s32)((src->a * (255.0f - dest->a)) + dest->a);
    }
}


void chrobjCollisionRelated(ObjectRecord *obj)
{
    struct ModelRoData_BoundingBoxRecord *bbox;
    Mtxf mtx;

    if (obj->ptr_allocated_collisiondata_block != NULL)
    {
        bbox = chrobjGetBboxFromObjectRecord(obj);
        mtx4Copy(&obj->mtx, &mtx);
        mtx4SetTranslation(&obj->runtime_pos, &mtx);
        obj->ptr_allocated_collisiondata_block->edges = bboxGetHull2d(bbox, &mtx, obj->ptr_allocated_collisiondata_block->polygon);

        obj->ptr_allocated_collisiondata_block->bottom = obj->runtime_pos.f[1] + bboxRotatedMinY(bbox, &mtx);
        obj->ptr_allocated_collisiondata_block->top = obj->runtime_pos.f[1] + bboxRotatedMaxY(bbox, &mtx);

        if (obj->type == PROPDEF_AIRCRAFT)
        {
            obj->ptr_allocated_collisiondata_block->bottom -= 200.0f;
        }
    }
}


PropRecord* objInit(ObjectRecord* obj, ModelFileHeader* model_header, PropRecord* prop, Model* model)
{
    if (prop == NULL)
    {
        prop = propAllocate();
    }

    if (model == NULL)
    {
        if (obj->type == PROPDEF_AIRCRAFT)
        {
            model = modelmgrInstantiateModelWithAnim(model_header);
        }
        else
        {
            model = modelmgrInstantiateModel(model_header);
        }
    }

    if ((prop != NULL) && (model != NULL))
    {
        obj->model = model;
        obj->ptr_allocated_collisiondata_block = NULL;

        if (obj->flags & 0x100)
        {
            obj->ptr_allocated_collisiondata_block = mempAlloc(0x50U, MEMPOOL_STAGE);
            obj->state = (u8) (obj->state | PROPSTATE_EXT_COLISION_BLOCK);
        }
        else
        {
            obj->state = (u8) (obj->state & 0xFFF7);
        }

        obj->prop = prop;
        obj->projectile = NULL;

        obj->shadecol.r = 0;
        obj->shadecol.g = 0;
        obj->shadecol.b = 0;
        obj->shadecol.a = 0;

        obj->nextcol.r = 0;
        obj->nextcol.g = 0;
        obj->nextcol.b = 0;
        obj->nextcol.a = 0;

        obj->damageTaken = 0.0f;
        obj->model->chr = NULL;
        modelSetScale(obj->model, PitemZ_entries[obj->obj].scale);
        prop->type = 1;
        prop->obj = obj;
        prop->pos.x = 0.0f;
        obj->runtime_pos.x = 0.0f;
        prop->pos.y = 0.0f;
        obj->runtime_pos.y = 0.0f;
        prop->pos.z = 0.0f;
        obj->runtime_pos.z = 0.0f;
        prop->stan = NULL;
    }
    else
    {
        if (model != NULL)
        {
            if (obj->type == PROPDEF_AIRCRAFT)
            {
                modelFreeAnimSlot(model);
            }
            else
            {
                modelFreeSlot(model);
            }
        }

        if (prop != NULL)
        {
            propFree(prop);
            prop = NULL;
        }
    }

    return prop;
}


PropRecord* objInitWithModelDef(ObjectRecord* object, ModelFileHeader* header)
{
  return objInit(object, header, 0, 0);
}


PropRecord* objInitWithAutoModel(ObjectRecord* obj)
{
    return objInitWithModelDef(obj, PitemZ_entries[obj->obj].header);
}


// Changes the color shade on the object, e.g. when walking in a darker area or under a colored light.
void objChangeShading(ObjectRecord* obj, coord3d* pos, Mtxf* matrix, StandTile* stan) {

    PropRecord *prop = obj->prop;

    mtx4Copy(matrix, &obj->mtx);

    obj->runtime_pos.x = prop->pos.x = pos->x;
    obj->runtime_pos.y = prop->pos.y = pos->y;
    obj->runtime_pos.z = prop->pos.z = pos->z;

    prop->stan = stan;

    objGetShadeColour(obj->prop, &obj->nextcol);

    obj->shadecol.r = obj->nextcol.r;
    obj->shadecol.g = obj->nextcol.g;
    obj->shadecol.b = obj->nextcol.b;
    obj->shadecol.a = obj->nextcol.a;
}


//moveToPad
void objPlaceAtPad(ObjectRecord *baseobj, coord3d *pos, Mtxf *matrix, StandTile *stan, coord3d *pos2)
{
    ModelRoData_BoundingBoxRecord *modelBoundingBox;
    f32 xmax;
    f32 ymin;
    coord3d newPos;
    StandTile *mStan;
    Mtxf mtxcopy;

    modelBoundingBox = chrobjGetBboxFromObjFile(baseobj->model->obj);
    xmax = bboxGetYmin(modelBoundingBox);
    ymin = bboxGetYmax(modelBoundingBox);
    mStan = stan;

    if (baseobj->flags & 4)
    {
        mtx4LoadZRotation(M_PI, &mtxcopy);
        mtx4MultInPlace(matrix, &mtxcopy);
        newPos.x = pos2->f[0] - (mtxcopy.m[1][0] * ymin);
        newPos.y = pos2->f[1] - (mtxcopy.m[1][1] * ymin);
        newPos.z = pos2->f[2] - (mtxcopy.m[1][2] * ymin);
    }
    else if (baseobj->flags & 8)
    {
        mtx4Copy(matrix, &mtxcopy);
        newPos.x = pos2->f[0] - (mtxcopy.m[1][0] * xmax);
        newPos.y = pos2->f[1] - (mtxcopy.m[1][1] * xmax);
        newPos.z = pos2->f[2] - (mtxcopy.m[1][2] * xmax);
    }
    else
    {
        ObjectRecord *roomObj;
        f32 distfromTileCenter;
        coord2d *roomObjPolygon;
        s32 roomObjEdges;
        f32 roomObjTop;
        f32 roomObjBottom;

        distfromTileCenter = stanGetFloorY(mStan, pos->f[0], pos->f[2]);

        mtx4Copy(matrix, &mtxcopy);
        newPos.x = pos2->f[0] - (mtxcopy.m[1][0] * xmax);
        newPos.z = pos2->f[2] - (mtxcopy.m[1][2] * xmax);
        roomObj  = objFindAtPos(pos, stan->room);

        if (roomObj)
        {
            PropRecord *roomObjProp = roomObj->prop;
            propGetCollisionPolygon(roomObjProp, &roomObjPolygon, &roomObjEdges, &roomObjTop, &roomObjBottom);

            if ((distfromTileCenter < roomObjTop) && (roomObjBottom < ((mtxcopy.m[1][1] * (ymin - xmax)) + distfromTileCenter + 4.0f)))
            {
                newPos.y = roomObjTop - (mtxcopy.m[1][1] * xmax);
                baseobj->runtime_bitflags |= RUNTIMEBITFLAG_ONSUPPORT;
            }
            else
            {
                newPos.y = (distfromTileCenter - (mtxcopy.m[1][1] * xmax)) + 4.0f;
            }
        }
        else
        {
            newPos.y = (distfromTileCenter - (mtxcopy.m[1][1] * xmax)) + 4.0f;
        }
    }

    if (!(baseobj->flags2 & 1) && stanWalkLine(&mStan, pos->f[0], pos->f[2], newPos.x, newPos.z))
    {
        objChangeShading(baseobj, &newPos, &mtxcopy, mStan);
    }
    else
    {
        objChangeShading(baseobj, pos, &mtxcopy, stan);
        if ((baseobj->flags2 & 1) || (baseobj->flags & 0x1000))
        {
            baseobj->runtime_pos.x = newPos.x;
            baseobj->runtime_pos.y = newPos.y;
            baseobj->runtime_pos.z = newPos.z;
        }
    }

    chrobjCollisionRelated(baseobj);
}


void objPlaceAtPadSideways(ObjectRecord *obj, coord3d *pos, Mtxf *padmtx, StandTile *stan2, coord3d *pos2)
{
    f32 zmin;
    coord3d posdiff;
    StandTile *stan;
    Mtxf matrix;
    Mtxf yrot;

    zmin = bboxGetZmin(chrobjGetBboxFromObjFile(obj->model->obj));
    stan = stan2;

    mtx4LoadXRotation(4.712389f, &matrix);
    mtx4LoadYRotation(M_PI_F, &yrot);
    mtx4MultInPlace(&yrot, &matrix);
    mtx4MultInPlace(padmtx, &matrix);

    posdiff.x = pos2->x - (matrix.m[2][0] * zmin);
    posdiff.y = pos2->y - (matrix.m[2][1] * zmin);
    posdiff.z = pos2->z - (matrix.m[2][2] * zmin);

    if ((!(((s32) obj->flags2) & 1)) && (stanWalkLine(&stan, pos->x, pos->z, posdiff.x, posdiff.z) != 0))
    {
        objChangeShading(obj, &posdiff, &matrix, stan);
    }
    else
    {
        objChangeShading(obj, pos, &matrix, stan2);
        obj->runtime_pos.x = posdiff.x;
        obj->runtime_pos.y = posdiff.y;
        obj->runtime_pos.z = posdiff.z;
    }

    chrobjCollisionRelated(obj);
}


void objFree(ObjectRecord* obj, s32 freeprop, s32 canregen)
{
    PropRecord *child;

    if (obj->type == PROPDEF_AUTOGUN)
    {
        AutogunRecord* record = (AutogunRecord*)obj;
        if ((record->fireSound1 != NULL) && (sndGetPlayingState(record->fireSound1) != 0))
        {
            sndDeactivate(record->fireSound1);
        }

        if ((record->fireSound2 != NULL) && (sndGetPlayingState(record->fireSound2) != 0))
        {
            sndDeactivate(record->fireSound2);
        }
    }
    else if (obj->type == PROPDEF_COLLECTABLE)
    {
        WeaponObjRecord* record = (WeaponObjRecord*) obj;
        WeaponObjRecord* record2 = record->dualweapon;
        if (record2 != NULL)
        {
            record2->dualweapon = NULL;
            record->dualweapon = NULL;
        }
    }
    else if (obj->type == PROPDEF_DOOR)
    {
        doorStopSounds((DoorRecord *)obj);
    }
    else if (obj->type == PROPDEF_TINTED_GLASS)
    {
        TintedGlassRecord* record = (TintedGlassRecord*) obj;
        if (record->portalnum >= 0)
        {
            bgToggleDataPortalsContrlBytes1Bit1(record->portalnum, 1);
        }
    }
    else if (obj->type == PROPDEF_AIRCRAFT)
    {
        AircraftRecord* record = (AircraftRecord*) obj;
        if ((record->Sound != NULL) && (sndGetPlayingState(record->Sound) != 0))
        {
            sndDeactivate(record->Sound);
        }
    }
    else if (obj->type == PROPDEF_VEHICHLE)
    {
        VehichleRecord* record = (VehichleRecord*) obj;
        if ((record->Sound != NULL) && (sndGetPlayingState(record->Sound) != 0))
        {
            sndDeactivate(record->Sound);
        }
    }

    if (obj->prop != NULL)
    {
        explosionClearBulletImpactRoomByFlag(obj->prop, FALSE);
        explosionClearBulletImpactRoomByFlag(obj->prop, TRUE);

        if (canregen == 0)
        {
            objFreeEmbedmentOrProjectile(obj->prop);

            if (obj->prop->parent != NULL)
            {
                objDetach(obj->prop);
            }

            propRemoveFromRooms(obj->prop);

            child = obj->prop->child;
            while (child)
            {
                PropRecord* next = child->prev;
                objFreePermanently(child->obj, TRUE);
                child = next;
            }

            if (obj->prop->type != PROP_TYPE_DOOR)
            {
                modelResetDeformedVertices(obj->model);
            }

            if (obj->type == PROPDEF_AIRCRAFT)
            {
                modelFreeAnimSlot(obj->model);
            }
            else
            {
                modelFreeSlot(obj->model);
            }

            if (freeprop != 0)
            {
                propDelist(obj->prop);
                propDisable(obj->prop);
                propFree(obj->prop);
            }

            obj->prop = NULL;
        }
    }
}


void objFreePermanently(struct ObjectRecord * obj, bool freeprop)
{
    objFree(obj, freeprop, 0);
}


/**
 * Description: Separating Axis Theorem
 *
 * Return true if both blocks are not intersecting on the X/Z plane.
 * PD: cdBlockExcludesBlockLaterally
 */
bool chrobjSeparatingAxisTheorem(coord2d* rect1, s32 numvertices0, coord2d* rect2, s32 numvertices1)
{
    f64 diff2;
    f64 diff1;
    s32 j;
    s32 k;
    s32 next;
    s32 i;
    f64 sum3;
    f64 sum2 = 0.0;
    f64 sum1;
    coord3d tmp;

    for (i = 0; i < numvertices0; i++)
    {
        next = (i + 1) % numvertices0;
        diff1 = rect1[next].y - (f64)rect1[i].y;
        diff2 = rect1[i].x - (f64)rect1[next].x;

        if (diff1 == 0.0f && diff2 == 0.0f)
        {
            tmp.x = rect1[i].x;
            tmp.y = 0.0f;
            tmp.z = rect1[i].y;
            if (polygonContainsPoint(&tmp, rect2, numvertices1))
            {
                return FALSE;
            }
        }
        else
        {
            sum1 = rect1[i].x * diff1 + rect1[i].y * diff2;
            j = (next + 1) % numvertices0;

            while (j != i)
            {
                sum2 = rect1[j].x * diff1 + rect1[j].y * diff2;

                if (sum2 != sum1) { break; }

                j = (j + 1) % numvertices0;
            }

            for (k = 0; k < numvertices1; k++)
            {
                sum3 = rect2[k].x * diff1 + rect2[k].y * diff2;

                if (sum2 == sum1)
                {
                    sum2 = sum1 - sum3 + sum1;
                }

                if (sum3 < sum1 && sum2 < sum1) { break; }
                if (sum3 > sum1 && sum2 > sum1) { break; }
            }

            if (k == numvertices1)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}


/**
 * Address 0x7F0446B8 (NTSC)
 * Address 0x7F0449A0 (NTSC-J)
 *
 * Description: Does a 2D collision check between two (convex?) polygons.
 *
 * Note: The NTSC version is 7 to 8 times faster than the others.
 *       Was this an attempt at optimization or to fix a bug?
 *
 * Deepseek says JP/EU's new code will detect edges cases such as a polygon
 * fully contained into another. NTSC's only check is SAT, which misses when
 * the polygons have edges that don’t overlap. NTSC's code handles 95% of
 * collisions so it should be called first.
 *
 * So they fixed a bug, but didn't do it the right way so it wouldn't affect performance.
*/
s32 chrobjTestPolygonsTouchingOrOverlap2D(coord2d *poly1, s32 edges1, coord2d *poly2, s32 edges2)
{
    s32 i;
    coord3d point;

    for (i=0; i<edges1; i++)
    {
        point.f[0] = poly1[i].f[0];
        point.f[1] = 0.0f;
        point.f[2] = poly1[i].f[1];

        if (polygonContainsPoint(&point, poly2, edges2) != 0)
        {
            return 1;
        }
    }

    for (i=0; i<edges2; i++)
    {
        point.f[0] = poly2[i].f[0];
        point.f[1] = 0.0f;
        point.f[2] = poly2[i].f[1];

        if (polygonContainsPoint(&point, poly1, edges1) != 0)
        {
            return 1;
        }
    }

    if (chrobjSeparatingAxisTheorem(poly1, edges1, poly2, edges2))
    {
        return 0;
    }

    if (chrobjSeparatingAxisTheorem(poly2, edges2, poly1, edges1))
    {
        return 0;
    }

    return 1;
}


/**
 * Checks whether a point collision with a convex polygon is within the specified radius.
 * @param point: 3d point to test collision with polygon. Only (x,z) are used.
 * @param collision_radius: Collision radius of point to test.
 * @param polygon: Convex polygon.
 * @param edges: Number of edges to test in polygon.
*/
s32 chrobjTestPointPolygonCollision(coord3d *point, f32 collision_radius, coord2d *polygon, s32 edges)
{
    f32 lineDist;
    f32 dist1;
    f32 px;
    f32 pz;
    f32 dist2;
    s32 i;
    coord2d *next;

    px = point->f[0];
    pz = point->f[2];

    for (i=0; i<edges; i++)
    {
        next = &polygon[(i+1) % edges];

        lineDist = geoSignedLineDist(polygon[i].f[0], polygon[i].f[1], next->f[0], next->f[1], px, pz);

        if (lineDist < 0.0f)
        {
            lineDist = -lineDist;
        }

        dist1 = geoDist2d(polygon[i].f[0], polygon[i].f[1], px, pz);
        dist2 = geoDist2d(next->f[0], next->f[1], px, pz);

        if ((lineDist < collision_radius)
            && ((dist1 < collision_radius)
                || (dist2 < collision_radius)
                || geoProjectsOntoSegment(polygon[i].f[0], polygon[i].f[1], next->f[0], next->f[1], px, pz)
            )
        )
        {
            return 1;
        }
    }

    return 0;
}


/**
*/
s32 vehicleFindBlockingProp(struct PropRecord *argProp)
{
    s32 clear;
    coord2d *polygon2;
    s32 edges2;
    f32 chrTop;
    f32 chrBottom;
    s32 roomids[8];
    s16 *propnum;
    f32 height;
    f32 arbitratyNumber;
    f32 radius;
    f32 ground;
    PropRecord *propss;
    coord2d *polygon;
    s32 edges;
    f32 top;
    f32 bottom;

    propGetCollisionPolygon(argProp, &polygon2, &edges2, &chrTop, &chrBottom);

    if (edges2 <= 0)
    {
        return 1;
    }

    propGetRoomIds(argProp, (s32*)&roomids);
    roomGetProps((s32*)&roomids);

    propss = (PropRecord *)&g_Props;

    for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
    {
        PropRecord *prop = &propss[*propnum];

        if (prop != argProp)
        {
            if ((prop->type == PROP_TYPE_VIEWER) || (prop->type == PROP_TYPE_CHR))
            {
                ChrRecord *chr = prop->chr;

                if ((chr == NULL) || !(chr->chrflags & CHRFLAG_HIDDEN))
                {
                    propGetCollisionRadius(prop, &radius, &height, &arbitratyNumber);

                    ground = propGetGroundY(prop);
                    arbitratyNumber += ground;
                    height += ground;

                    if (arbitratyNumber <= chrTop)
                    {
                        clear = 1;

                        if (chrBottom <= height)
                        {
                            if (polygonContainsPoint(&prop->pos, polygon2, edges2) != 0)
                            {
                                clear = 0;
                            }

                            // 'else if' avoided here
                            if ((clear != 0) && (chrobjTestPointPolygonCollision(&prop->pos, radius, polygon2, edges2) != 0))
                            {
                                clear = 0;
                            }

                            if (clear == 0)
                            {
                                if ((prop->type == PROP_TYPE_CHR) && (argProp->type == PROP_TYPE_DOOR))
                                {
                                    prop->chr->hidden |= CHRHIDDEN_OFFSCREEN_PATROL;
                                }

                                return 0;
                            }
                        }
                    }
                }
            }
            else if (
                ((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_WEAPON) || (prop->type == PROP_TYPE_DOOR))
                && (
                    (argProp->type != PROP_TYPE_DOOR)
                    || ((prop->type != PROP_TYPE_DOOR) && ((prop->obj->type != PROPDEF_SAFE)) && (prop->obj->type != PROPDEF_AIRCRAFT))))
            {
                propGetCollisionPolygon(prop, &polygon, &edges, &top, &bottom);

                if ((edges > 0)
                    && (bottom <= chrTop)
                    && (chrBottom <= top)
                    && (chrobjTestPolygonsTouchingOrOverlap2D(polygon, edges, polygon2, edges2) != 0))
                {
                    return 0;
                }
            }
        }
    }

    return 1;
}


/**
 * Draws tracers for characters other than the player, and draws tracers for drone guns as well.
 */
Gfx *weaponRenderTracers(Gfx *gdl)
{
    ChrRecord *chr;
    ChrRecord *chr2;
    PropRecord *prop;
    ObjectRecord *obj;
    s32 playernum;
    s32 type;
    s32 type_viewer;
    s32 obj_type_0d;
    s32 match;
    s32 one;
    s32 type_chr;

    prop = propsGetActiveTail();
    if (prop != NULL)
    {
        type_viewer = PROP_TYPE_VIEWER;
        obj_type_0d = 0x0d;
        one = 1;
        type_chr = PROP_TYPE_CHR;

        do
        {
            type = prop->type;
            if (type_chr == type)
            {
                chr = prop->chr;
                gdl = beamRender(gdl, &chr->beams[0], one);
                gdl = beamRender(gdl, &chr->beams[1], one);
            }
            else if (one == type)
            {
                obj = prop->obj;
                match = obj_type_0d == obj->type;
                if (match)
                {
                    gdl = beamRender(gdl, ((AutogunRecord *)obj)->beam, one);
                }
            }
            else if (type_viewer == type)
            {
                if (prop->voidp != NULL)
                {
                    playernum = playerGetIndexForProp(prop);
                    if (playerGetCurrentNum() != playernum)
                    {
                        chr2 = prop->chr;
                        gdl = beamRender(gdl, &chr2->beams[0], one);
                        gdl = beamRender(gdl, &chr2->beams[1], one);
                    }
                }
            }
            prop = prop->prev;
        }
        while (prop != NULL);
    }
    return gdl;
}



void objRenderModel(PropRecord *prop, ModelRenderData *mrData, s32 xlupass)
{
    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        ObjectRecord *obj;
        Model *model;
        s32 alpha;
        PropRecord *child;
        Gfx *gdl;
        s32 mN;
        ObjectRecord *o;
        DoorRecord *door;
        s32 recolor;
        s32 v;
        ModelNode *node;
        Gfx *g3;
        Gfx *g2;
        bool destroyed;
        union ModelRwData *rwdataSlot;

        obj = prop->obj;
        model = obj->model;
        destroyed = (obj->flags & PROPFLAG_00000200) != FALSE;

        if (destroyed)
        {
            destroyed = camGetProjViewMtx() != NULL;
        }

        gdl = mrData->gdl;

        if (obj->type == PROPDEF_MONITOR)
        {
            if (mrData->flags & 1)
            {
                if (obj->flags2 & PROPFLAG2_00010000)
                {
                    mN = 0;
                }
                else if (obj->flags & PROPFLAG_FIXED_MONITOR)
                {
                    mN = 8;
                }
                else
                {
                    mN = 1;
                }

                gdl = monitorTickAndDraw(model, model->obj->Switches[0], &((MonitorObjRecord *)prop->obj)->Monitor, gdl, mN, 1);
            }
        }
        else if (obj->type == PROPDEF_MULTI_MONITOR)
        {
            if (mrData->flags & 1)
            {
                o = prop->obj;

                if (obj->flags2 & PROPFLAG2_00010000)
                {
                    mN = 0;
                }
                else if (obj->flags & PROPFLAG_FIXED_MONITOR)
                {
                    mN = 8;
                }
                else
                {
                    mN = 1;
                }

                gdl = monitorTickAndDraw(model, model->obj->Switches[0], &((MultiMonitorObjRecord *)o)->Monitor[0], gdl, mN, 1);

                if (obj->flags2 & PROPFLAG2_00010000)
                {
                    mN = 0;
                }
                else if (obj->flags & (PROPFLAG_FIXED_MONITOR | PROPFLAG_SPECIAL_FUNC))
                {
                    mN = 8;
                }
                else
                {
                    mN = 1;
                }

                gdl = monitorTickAndDraw(model, model->obj->Switches[1], &((MultiMonitorObjRecord *)o)->Monitor[1], gdl, mN, 1);

                gdl = monitorTickAndDraw(model, model->obj->Switches[2], &((MultiMonitorObjRecord *)o)->Monitor[2], gdl, mN, 1);

                gdl = monitorTickAndDraw(model, model->obj->Switches[3], &((MultiMonitorObjRecord *)o)->Monitor[3], gdl, mN, 1);
            }
        }

        if (obj->type == PROPDEF_DOOR)
        {
            door = prop->door;

            gSPClearGeometryMode(gdl++, G_CULL_BOTH);

            if (door->doorFlags & DOORFLAG_FLIP)
            {
                mrData->cullmode = CULLMODE_FRONT;
            }
            else
            {
                mrData->cullmode = CULLMODE_BACK;
            }

            if (mrData->PropType == PROP_TYPE_MAX)
            {
                mrData->envcolour.word &= 0xffffff00;
            }
        }
        else
        {
            node = objFindDlCollisionNode(obj);
            recolor = 0;

            if (node != NULL)
            {
                union ModelRoData *nodedata;

                nodedata = node->Data;

                if (nodedata != NULL)
                {
                    rwdataSlot = (union ModelRwData *)&obj->model->datas[nodedata->DisplayListCollisions.RwDataIndex];

                    if (nodedata->DisplayListCollisions.Vertices != rwdataSlot->DisplayListCollisions.Vertices)
                    {
                        recolor = 1;
                    }
                }
            }

            v = objGetDestroyedLevel(obj);

            if ((v == 0) || !recolor)
            {
                mrData->cullmode = CULLMODE_BACK;

                if (mrData->PropType == PROP_TYPE_MAX)
                {
                    mrData->envcolour.word &= 0xffffff00;
                }
            }
            else
            {
                v = objGetDestroyedLevel(obj);
                mrData->cullmode = CULLMODE_NONE;

                if (mrData->PropType == PROP_TYPE_MAX)
                {
                    alpha = v * 50 + 100;

                    if (alpha >= 256)
                    {
                        alpha = 255;
                    }

                    mrData->envcolour.word &= 0xffffff00;
                    mrData->envcolour.word |= alpha;
                }
                else if (v > 0)
                {
                    mrData->envcolour.word |= 0xff00;
                }
            }
        }

        if (destroyed)
        {
            // Keep on one line for matching.
            g3 = gdl++; gSPMatrix(g3, camGetProjViewMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
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
            gdl = explosionRenderBulletImpactOnProp(gdl, prop, xlupass);
        }

        if (destroyed)
        {
            // Keep on one line for matching.
            g2 = gdl++; gSPMatrix(g2, camGetProjectionMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        }

        mrData->gdl = gdl;

        for (child = prop->child; child != NULL; child = child->prev)
        {
            objRenderModel(child, mrData, xlupass);
        }

        if (xlupass)
        {
            if (destroyed != FALSE)
            {
                camMtxsToRoomSpace((Mtxf *)model->render_pos, model->obj->numMatrices);
            }
            else
            {
                mtxsF2L(model->render_pos, model->obj->numMatrices);
            }
        }
    }
}



Gfx *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 xlupass)
{
    rgba_f32 fogcolour;
    s32 fogfrac;
    ModelRenderData mrData;
    bbox2d screenbox;
    rgba_s32 shade;
    s32 renderflags;
    ObjectRecord *obj;
    s32 objAlpha;
    f32 visfrac;
    s32 shots;
    s32 brightness;

    obj = prop->obj;

    mrData = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};

    objAlpha = 0xFF;
    fogfrac = fogGetPropDistColor(prop, &fogcolour);

    if (fogfrac == 0)
    {
        return gdl;
    }

    if ((u8) obj->type != PROPDEF_TINTED_GLASS)
    {
        visfrac = chrobjFogVisRangeRelated(prop, modelGetRadius(obj->model));

        if (((s32) prop->timetoregen > 0) && ((s32) prop->timetoregen < CHROBJ_TIMETOREGEN))
        {
            visfrac *= ((CHROBJ_TIMETOREGEN_F - (f32) prop->timetoregen) / CHROBJ_TIMETOREGEN_F);
        }

        objAlpha = (s32) (visfrac * 255.0f);

        if (objAlpha <= 0)
        {
            return gdl;
        }
    }

    if ((objAlpha < 0xFF) || (obj->flags2 & PROPFLAG2_00010000))
    {
        if (xlupass == 0)
        {
            return gdl;
        }

        renderflags = 3;
    }
    else
    {

        renderflags = (xlupass == 0) ? 1 : 2;
    }

    if ((propGetRoomsBbox2d(prop, &screenbox) > 0) && !(obj->flags2 & PROPFLAG2_04000000))
    {
        gdl = bgScissorCurrentPlayerViewF(gdl, screenbox.min.x, screenbox.min.y, screenbox.max.x, screenbox.max.y);
    }
    else
    {
        gdl = bgScissorCurrentPlayerViewDefault(gdl);
    }

    mrData.flags = renderflags;
    mrData.zbufferenabled = (obj->flags2 & PROPFLAG2_00010000) == 0;

    mrData.gdl = gdl;

    if (objAlpha < 0xFF)
    {
        mrData.PropType = 5;
        mrData.envcolour.word = objAlpha;
    }
    else
    {
        mrData.PropType = 9;

        if (obj->type == PROPDEF_TINTED_GLASS)
        {
            mrData.envcolour.word = ((struct TintedGlassRecord*)obj)->calculatedopacity << 8;
        }
        else if ((obj->type == PROPDEF_DOOR) && (((struct DoorRecord*)obj)->doorFlags & DOORFLAG_WINDOWED))
        {
            mrData.envcolour.word = ((struct DoorRecord*)obj)->calculatedopacity << 8;
        }
        else
        {
            mrData.envcolour.word = 0;
        }
    }

    shots = objGetShotsTaken(obj);
    brightness = 0xFF - (shots * 0x15);

    if (brightness < 0)
    {
        brightness = 0;
    }

    shade.r = (s32) (obj->shadecol.rgba[0] * brightness) >> 8;
    shade.g = (s32) (obj->shadecol.rgba[1] * brightness) >> 8;
    shade.b = (s32) (obj->shadecol.rgba[2] * brightness) >> 8;
    shade.a = obj->shadecol.rgba[3] + shots * 0xF;

    if (shade.a >= 0x100)
    {
        shade.a = 0xFF;
    }

    shadingLerp(&shade, fogfrac, &fogcolour);

    mrData.fogcolour.word = (shade.rgba[0] << 0x18) | (shade.rgba[1] << 0x10) | (shade.rgba[2] << 0x08) | (shade.rgba[3] << 0x00);

    objRenderModel(prop, &mrData, xlupass);

    return mrData.gdl;
}

void objSetMoving(PropRecord* prop, s32 moving) {
    ChrRecord* chr;
    chr = prop->chr;

    if (moving != 0)
    {
        chr->accuracyrating = (u8) chr->accuracyrating & ~0x20;
    }
    else
    {
        chr->accuracyrating = (u8) chr->accuracyrating | 0x20;
    }
}


void objGetCollisionBounds(PropRecord* prop, coord2d** polygon, s32* edges, f32* top, f32* bottom)
{
    ObjectRecord* obj;
    obj = prop->obj;

    if ((obj->ptr_allocated_collisiondata_block != NULL) && (obj->flags & PROPFLAG_00000100) && !(obj->state & PROPSTATE_20))
    {
        *edges = obj->ptr_allocated_collisiondata_block->edges;
        *polygon = obj->ptr_allocated_collisiondata_block->polygon;
        *bottom = obj->ptr_allocated_collisiondata_block->bottom;
        *top = obj->ptr_allocated_collisiondata_block->top;
        return;
    }

    *edges = 0;
}


bool objGetOnscreenRenderBounds(PropRecord *prop, coord3d *screenpos, coord2d *max, coord2d *min)
{
    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        ObjectRecord *obj = prop->obj;
        Mtxf *matrix = modelGetRootMtx(obj->model);

        screenpos->z = matrix->m[3][2];

        if (screenpos->z < 0)
        {
            screenpos->x = matrix->m[3][0];
            screenpos->y = matrix->m[3][1];

            min->f[0] = 0;
            min->f[1] = 0;

            max->f[0] = 0;
            max->f[1] = 0;

            modelGetXYExtents(obj->model, &max->f[1], &max->f[0], &min->f[1], &min->f[0]);

            return TRUE;
        }
    }

    return FALSE;
}


void modelResetDeformedVertices(Model* model)
{
    ModelNode* node;
    ModelFileHeader* header;
    union ModelRoData* rodata;
    union ModelRwData* rwdata;

    header = model->obj;
    node = header->RootNode;

    while (node != NULL)
    {
        switch (node->Opcode & 0xFF)
        {
            case MODELNODE_OPCODE_DLCOLLISION:
                rodata = node->Data;
                rwdata = modelGetNodeRwData(model, node);

                if ((rwdata->DisplayListCollisions.Vertices != rodata->DisplayListCollisions.Vertices) && (objNodeCanDeform(header, node) != 0))
                {
                    vtxstoreFree(rwdata->DisplayListCollisions.Vertices);
                    rwdata->DisplayListCollisions.Vertices = rodata->DisplayListCollisions.Vertices;
                }
                break;
            case MODELNODE_OPCODE_LOD:
                modelApplyDistanceRelations(model, node);
                break;
            case MODELNODE_OPCODE_SWITCH:
                modelApplyToggleRelations(model, node);
                break;
            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}



/*
* Description: Computes the 2D bounding box for every room the prop is in
*              so it can be used for scissors. Returns true when the prop
*              has at least one room bounding box.
*/
s32 propGetRoomsBbox2d(PropRecord *prop, bbox2d *bbox)
{
    s32 room_ids[8];
    s32 *rooms;
    bool result = FALSE;
    s32 room_id;
    bbox2d bbox2;

    propGetRoomIds(prop, room_ids);
    rooms = room_ids;
    room_id = *rooms;

    while (room_id >= 0)
    {
        if (bgGet2dBboxByRoomId(room_id, &bbox2))
        {
            if (result)
            {
                if (bbox->min.x > bbox2.min.x)
                {
                    bbox->min.x = bbox2.min.x;
                }
                if (bbox->min.y > bbox2.min.y)
                {
                    bbox->min.y = bbox2.min.y;
                }
                if (bbox->max.x < bbox2.max.x)
                {
                    bbox->max.x = bbox2.max.x;
                }
                if (bbox->max.y < bbox2.max.y)
                {
                    bbox->max.y = bbox2.max.y;
                }
            }
            else
            {
                bbox->min.x = bbox2.min.x;
                bbox->min.y = bbox2.min.y;
                bbox->max.x = bbox2.max.x;
                bbox->max.y = bbox2.max.y;
            }
            result = TRUE;
        }
        rooms++;
        room_id = *rooms;
    }

    return result;
}


f32 chrobjFogVisRangeRelated(PropRecord *prop, f32 size)
{
    f32 ret;
    NearFogValues *nfd;
    f32 dist;

    ret = 1.0f;
    nfd = fogGetNearFogValuesP();

    if ((nfd != NULL) && (nfd->MaxObfuscationRange < prop->zDepth))
    {
        dist = camGetLodScaleZ();
        dist = ((((prop->zDepth - nfd->MaxObfuscationRange) * 100.0f) / size) + nfd->MaxObfuscationRange) * dist;

        if (nfd->MaxVisRange <= dist)
        {
            ret = 0.0f; //im invisible
        }
        else
        {
            if (nfd->NearFog < dist)
            {
                ret = (nfd->MaxVisRange - dist) / (nfd->MaxVisRange - nfd->NearFog);// power of fog (0 - 1 ) where 0 is full fog, and 1 is no fog
            }
        }
    }

    return ret;
}


bool posIsBeforeFogEnd(coord3d *coord, f32 size)
{
    bool result = TRUE;
    coord3d *ptr = (coord3d*)fogGetNearFogValuesP();
    coord3d tmp;
    f32 depth;

    if (ptr != NULL)
    {
        coord3d *campos = playerGetPos();
        Mtxf *mtx = camGetWorldToScreenMtxf();

        tmp.x = coord->x - campos->x;
        tmp.y = coord->y - campos->y;
        tmp.z = coord->z - campos->z;

        depth = tmp.f[0] * mtx->m[0][0] + tmp.f[1] * mtx->m[0][1] + tmp.f[2] * mtx->m[0][2];

        if (depth > ptr->z)
        {
            f32 scalez = camGetLodScaleZ();
            depth = ((depth - ptr->z) * 100 / size + ptr->z) * scalez;

            if (depth >= ptr->y)
            {
                result = FALSE;
            }
        }
    }

    return result;
}


bool posIsOnScreen(PropRecord *prop, coord3d *pos, f32 radius, bool checkFogEnd)
{
    s32 room_ids[8];
    s32 *rooms;
    s32 roomnum;
    bool result;
    bbox2d bbox;

    result = FALSE;
    propGetRoomIds(prop, room_ids);
    rooms = room_ids;
    roomnum = *rooms;

    while (roomnum >= 0)
    {
        if (bgRoomIsVisible(roomnum) != 0)
        {
            if (fogPositionIsVisibleThroughFog(pos, radius) && (!checkFogEnd || posIsBeforeFogEnd(pos, radius)))
            {
                if (propGetRoomsBbox2d(prop, &bbox) != 0)
                {
                    result = camIsPosInScreenBox(pos, radius, &bbox);
                }
                else
                {
                    result = camIsPosInScreen(pos, radius);
                }

                if (result)
                {
                    coord3d *campos = playerGetPos();
                    f32 xdiff = pos->x - campos->x;
                    f32 ydiff = pos->y - campos->y;
                    f32 zdiff = pos->z - campos->z;

                    if (xdiff * xdiff + ydiff * ydiff + zdiff * zdiff > 32000 * 32000)
                    {
                        result = FALSE;
                    }
                }
            }

            break;
        }

        rooms++;
        roomnum = *rooms;
        result = FALSE;
    }

    return result;
}


void objsFreeDeformedVertices(void)
{
    Model *model;
    PropRecord *prop;
    ObjectRecord *obj;
    ModelNode *node;
    ModelRoData_DisplayList_CollisionRecord *rodata;

    for (prop = propsGetActiveTail(); prop != NULL; prop = prop->prev)
    {
        if ((prop->type == PROP_TYPE_OBJ) && !(prop->flags & PROPFLAG_ONSCREEN))
        {
            obj = prop->obj;
            if (obj->state & PROPSTATE_DESTROYED)
            {
                model = obj->model;
                node = objFindDlCollisionNode(obj);
                if (node == NULL)
                {
                    return;
                }
                rodata = (ModelRoData_DisplayList_CollisionRecord *)node->Data;
                if (rodata == NULL)
                {
                    return;
                }
                if (objNodeCanDeform(obj->model->obj, node) != 0)
                {
                    // The model has its own deformed copy of the vertices.
                    if ((void *)rodata->Vertices != (void *)model->datas[rodata->RwDataIndex])
                    {
                        objFreePermanently(obj, 1);
                        return;
                    }
                }
            }
        }
    }
}
