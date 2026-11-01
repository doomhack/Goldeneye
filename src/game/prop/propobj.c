/**
 * Objects: creating, placing and freeing them.
 *
 * An object's record (ObjectRecord, or a type that starts with one) is usually its
 * propdef in the level's setup file, used in place. objInit gives it a prop and a model
 * instance. objPlaceAtPad puts it at its pad: on the floor, hanging upside-down, in the
 * air or sideways. objFree takes its prop and model back, unless it is waiting to
 * respawn.
 *
 * The rest of an object's life is in its own files: setting it up from the setup file in
 * objsetup.c, the tick in objtick.c (with cctv.c, autogun.c, vehicle.c, projectile.c and
 * mine.c), drawing in objrender.c, damage in objdamage.c and objhit.c, pickups in
 * pickup.c, slots in objslots.c, carried guns and hats in carried.c, collision in
 * propcollision.c, and doors in door.c.
 */

#include <ultra64.h>
#include <memp.h>
#include <snd.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/propobj.h"
#include "game/prop/door.h"
#include "game/prop/explosion.h"
#include "game/prop/projectile.h"
#include "game/prop/prop.h"
#include "game/gfx/vtxstore.h"
#include "game/math/matrix.h"
#include "game/model/chrobjdata.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/world/bg.h"

/* An object on the floor stands this far above it */
#define FLOOR_CLEARANCE 4.0f

/* The collision data block: sizeof(collision_data), rounded up to 16 */
#define COLLISION_DATA_ALLOC 0x50

/* The level's links between props, from the setup file */
struct LinkRecord *g_LevelLoadPropSwitch = NULL;
LockDoorRecord *g_LevelLoadPropLockDoor = NULL;
SafeObjectRecord *g_LevelLoadPropSafeItem = NULL;

/* Set by difficulty: scales the ammo in solo pickups */
f32 g_SoloAmmoMultiplier = 1.0;


/* Give a model slot back */
static void objFreeModel(ObjectRecord *obj, Model *model)
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


/**
 * Give an object a prop and a model instance, or use the ones passed. The object starts
 * at the origin, unshaded and undamaged; a collision data block is allocated if it
 * collides (PROPFLAG_00000100). NULL (and nothing kept) when there is no free prop or
 * model slot.
 */
PropRecord *objInit(ObjectRecord *obj, ModelFileHeader *header, PropRecord *prop, Model *model)
{
    if (prop == NULL)
    {
        prop = propAllocate();
    }

    if (model == NULL)
    {
        model = obj->type == PROPDEF_AIRCRAFT ? modelmgrInstantiateModelWithAnim(header) : modelmgrInstantiateModel(header);
    }

    if (prop == NULL || model == NULL)
    {
        if (model != NULL)
        {
            objFreeModel(obj, model);
        }

        if (prop != NULL)
        {
            propFree(prop);
        }

        return NULL;
    }

    obj->model = model;
    obj->ptr_allocated_collisiondata_block = NULL;

    if (obj->flags & PROPFLAG_00000100)
    {
        obj->ptr_allocated_collisiondata_block = mempAlloc(COLLISION_DATA_ALLOC, MEMPOOL_STAGE);
        obj->state |= PROPSTATE_EXT_COLISION_BLOCK;
    }
    else
    {
        obj->state &= ~PROPSTATE_EXT_COLISION_BLOCK;
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

    prop->type = PROP_TYPE_OBJ;
    prop->obj = obj;
    prop->pos.x = obj->runtime_pos.x = 0.0f;
    prop->pos.y = obj->runtime_pos.y = 0.0f;
    prop->pos.z = obj->runtime_pos.z = 0.0f;
    prop->stan = NULL;

    return prop;
}


PropRecord *objInitWithModelDef(ObjectRecord *obj, ModelFileHeader *header)
{
    return objInit(obj, header, NULL, NULL);
}


/* objInit with the object's own model */
PropRecord *objInitWithAutoModel(ObjectRecord *obj)
{
    return objInitWithModelDef(obj, PitemZ_entries[obj->obj].header);
}


/**
 * Move an object: its matrix, position and floor tile. Its shade colour jumps to the
 * new floor's (no fade).
 */
void objSetPosition(ObjectRecord *obj, coord3d *pos, Mtxf *matrix, StandTile *stan)
{
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


/**
 * Move an object to where it was placed, and work out its collision outline. If the
 * new position can't be walked to from pos on the floor tiles (or with
 * PROPFLAG2_ATPADPOS), the prop stays at pos; the object is drawn at the new position
 * only with PROPFLAG2_ATPADPOS or PROPFLAG_ABSOLUTEPOS.
 */
static void objMoveToPlacement(ObjectRecord *obj, coord3d *pos, StandTile *stan, coord3d *newpos, Mtxf *matrix)
{
    StandTile *newstan = stan;

    if (!(obj->flags2 & PROPFLAG2_ATPADPOS) && stanWalkLine(&newstan, pos->f[0], pos->f[2], newpos->x, newpos->z))
    {
        objSetPosition(obj, newpos, matrix, newstan);
    }
    else
    {
        objSetPosition(obj, pos, matrix, stan);

        if ((obj->flags2 & PROPFLAG2_ATPADPOS) || (obj->flags & PROPFLAG_ABSOLUTEPOS))
        {
            obj->runtime_pos.x = newpos->x;
            obj->runtime_pos.y = newpos->y;
            obj->runtime_pos.z = newpos->z;
        }
    }

    objUpdateCollisionBounds(obj);
}


/**
 * Place an object at its pad: pos is the floor point under it (on tile stan), centre
 * the point the object is centred on, and matrix its orientation.
 *
 * - PROPFLAG_UPSIDEDOWN: turned over, with its top at the centre;
 * - PROPFLAG_INAIR: with its bottom at the centre;
 * - otherwise standing on the floor (FLOOR_CLEARANCE above it), or on top of an object
 *   already there that it overlaps in height (it then falls if that object is
 *   destroyed: RUNTIMEBITFLAG_ONSUPPORT).
 */
void objPlaceAtPad(ObjectRecord *obj, coord3d *pos, Mtxf *matrix, StandTile *stan, coord3d *centre)
{
    ModelRoData_BoundingBoxRecord *bbox = modelFileFindBbox(obj->model->obj);
    f32 bottom = bboxGetYmin(bbox);
    f32 top = bboxGetYmax(bbox);
    coord3d newpos;
    Mtxf mtx;

    if (obj->flags & PROPFLAG_UPSIDEDOWN)
    {
        mtx4LoadZRotation(M_PI, &mtx);
        mtx4MultInPlace(matrix, &mtx);
        newpos.x = centre->f[0] - (mtx.m[1][0] * top);
        newpos.y = centre->f[1] - (mtx.m[1][1] * top);
        newpos.z = centre->f[2] - (mtx.m[1][2] * top);
    }
    else if (obj->flags & PROPFLAG_INAIR)
    {
        mtx4Copy(matrix, &mtx);
        newpos.x = centre->f[0] - (mtx.m[1][0] * bottom);
        newpos.y = centre->f[1] - (mtx.m[1][1] * bottom);
        newpos.z = centre->f[2] - (mtx.m[1][2] * bottom);
    }
    else
    {
        f32 floory = stanGetFloorY(stan, pos->f[0], pos->f[2]);
        ObjectRecord *below;

        mtx4Copy(matrix, &mtx);
        newpos.x = centre->f[0] - (mtx.m[1][0] * bottom);
        newpos.z = centre->f[2] - (mtx.m[1][2] * bottom);
        below = objFindAtPos(pos, stan->room);

        newpos.y = (floory - (mtx.m[1][1] * bottom)) + FLOOR_CLEARANCE;

        if (below != NULL)
        {
            coord2d *polygon;
            s32 edges;
            f32 belowtop;
            f32 belowbottom;

            propGetCollisionPolygon(below->prop, &polygon, &edges, &belowtop, &belowbottom);

            if (floory < belowtop && belowbottom < (mtx.m[1][1] * (top - bottom)) + floory + FLOOR_CLEARANCE)
            {
                newpos.y = belowtop - (mtx.m[1][1] * bottom);
                obj->runtime_bitflags |= RUNTIMEBITFLAG_ONSUPPORT;
            }
        }
    }

    objMoveToPlacement(obj, pos, stan, &newpos, &mtx);
}


/**
 * Place an object sideways (PROPFLAG_SIDEWAYS) at its pad: turned so its top faces the
 * pad's look direction and its front faces up, with its back (z min) at the centre.
 * Like objPlaceAtPad, but the object is always drawn at the new position.
 */
void objPlaceAtPadSideways(ObjectRecord *obj, coord3d *pos, Mtxf *padmtx, StandTile *stan, coord3d *centre)
{
    f32 back = bboxGetZmin(modelFileFindBbox(obj->model->obj));
    StandTile *newstan = stan;
    coord3d newpos;
    Mtxf matrix;
    Mtxf yrot;

    mtx4LoadXRotation(4.712389f, &matrix);
    mtx4LoadYRotation(M_PI_F, &yrot);
    mtx4MultInPlace(&yrot, &matrix);
    mtx4MultInPlace(padmtx, &matrix);

    newpos.x = centre->x - (matrix.m[2][0] * back);
    newpos.y = centre->y - (matrix.m[2][1] * back);
    newpos.z = centre->z - (matrix.m[2][2] * back);

    if (!(obj->flags2 & PROPFLAG2_ATPADPOS) && stanWalkLine(&newstan, pos->x, pos->z, newpos.x, newpos.z))
    {
        objSetPosition(obj, &newpos, &matrix, newstan);
    }
    else
    {
        objSetPosition(obj, pos, &matrix, stan);
        obj->runtime_pos.x = newpos.x;
        obj->runtime_pos.y = newpos.y;
        obj->runtime_pos.z = newpos.z;
    }

    objUpdateCollisionBounds(obj);
}


/* Stop a sound if it is playing */
static void objStopSound(ALSoundState *sound)
{
    if (sound != NULL && sndGetPlayingState(sound) != 0)
    {
        sndDeactivate(sound);
    }
}


/**
 * Free an object: stop its sounds, unlink a dual weapon, reopen a tinted glass portal,
 * and clear its bullet impacts. Unless canregen (it will respawn), also free its
 * projectile or embedment, detach it, take it out of its rooms, free its children, its
 * bent vertices and its model, and (with freeprop) its prop.
 */
void objFree(ObjectRecord *obj, s32 freeprop, s32 canregen)
{
    switch (obj->type)
    {
        case PROPDEF_AUTOGUN:
            objStopSound(((AutogunRecord *)obj)->fireSound1);
            objStopSound(((AutogunRecord *)obj)->fireSound2);
            break;
        case PROPDEF_COLLECTABLE:
        {
            WeaponObjRecord *weapon = (WeaponObjRecord *)obj;

            if (weapon->dualweapon != NULL)
            {
                weapon->dualweapon->dualweapon = NULL;
                weapon->dualweapon = NULL;
            }
            break;
        }
        case PROPDEF_DOOR:
            doorStopSounds((DoorRecord *)obj);
            break;
        case PROPDEF_TINTED_GLASS:
            if (((TintedGlassRecord *)obj)->portalnum >= 0)
            {
                bgToggleDataPortalsContrlBytes1Bit1(((TintedGlassRecord *)obj)->portalnum, 1);
            }
            break;
        case PROPDEF_AIRCRAFT:
            objStopSound(((AircraftRecord *)obj)->Sound);
            break;
        case PROPDEF_VEHICHLE:
            objStopSound(((VehichleRecord *)obj)->Sound);
            break;
    }

    PropRecord *prop = obj->prop;

    if (prop == NULL)
    {
        return;
    }

    impactsRemoveFromProp(prop, FALSE);
    impactsRemoveFromProp(prop, TRUE);

    if (canregen)
    {
        return;
    }

    objFreeEmbedmentOrProjectile(prop);

    if (propParent(prop) != NULL)
    {
        objDetach(prop);
    }

    propRemoveFromRooms(prop);

    for (PropRecord *child = propChild(prop); child != NULL;)
    {
        PropRecord *next = child->prev;

        objFreePermanently(child->obj, TRUE);
        child = next;
    }

    if (prop->type != PROP_TYPE_DOOR)
    {
        modelResetDeformedVertices(obj->model);
    }

    objFreeModel(obj, obj->model);

    if (freeprop)
    {
        propDelist(prop);
        propDisable(prop);
        propFree(prop);
    }

    obj->prop = NULL;
}


void objFreePermanently(ObjectRecord *obj, bool freeprop)
{
    objFree(obj, freeprop, FALSE);
}


/**
 * Put a model back as it was loaded: bent vertices go back to the vertex store, and the
 * level of detail, switch and head nodes are reset.
 */
void modelResetDeformedVertices(Model *model)
{
    ModelFileHeader *header = model->obj;
    ModelNode *node = header->RootNode;

    while (node != NULL)
    {
        switch (node->Opcode & 0xff)
        {
            case MODELNODE_OPCODE_DLCOLLISION:
            {
                union ModelRoData *rodata = node->Data;
                union ModelRwData *rwdata = modelGetNodeRwData(model, node);

                if (rwdata->DisplayListCollisions.Vertices != rodata->DisplayListCollisions.Vertices && objNodeCanDeform(header, node))
                {
                    vtxstoreFree(rwdata->DisplayListCollisions.Vertices);
                    rwdata->DisplayListCollisions.Vertices = rodata->DisplayListCollisions.Vertices;
                }
                break;
            }
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

        // Depth first: the child, else the next sibling of this node or an ancestor
        if (node->Child != NULL)
        {
            node = node->Child;
        }
        else
        {
            while (node != NULL)
            {
                if (node->Next != NULL)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


/**
 * When the vertex store runs out: free the first destroyed object off screen that has a
 * bent copy of its vertices, for good.
 *
 * It gives up at the first destroyed object off screen with no collision node or no
 * node data. The bent check compares the vertex pointer with the first word of the
 * node's run-time data, as the original does.
 */
void objsFreeDeformedVertices(void)
{
    for (PropRecord *prop = propsGetActiveTail(); prop != NULL; prop = prop->prev)
    {
        if (prop->type != PROP_TYPE_OBJ || (prop->flags & PROPFLAG_ONSCREEN))
        {
            continue;
        }

        ObjectRecord *obj = prop->obj;

        if (!(obj->state & PROPSTATE_DESTROYED))
        {
            continue;
        }

        Model *model = obj->model;
        ModelNode *node = objFindDlCollisionNode(obj);

        if (node == NULL)
        {
            return;
        }

        ModelRoData_DisplayList_CollisionRecord *rodata = (ModelRoData_DisplayList_CollisionRecord *)node->Data;

        if (rodata == NULL)
        {
            return;
        }

        if (objNodeCanDeform(obj->model->obj, node) && (void *)rodata->Vertices != (void *)model->datas[rodata->RwDataIndex])
        {
            objFreePermanently(obj, TRUE);
            return;
        }
    }
}
