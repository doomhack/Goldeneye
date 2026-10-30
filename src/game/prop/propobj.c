/**
 * Objects at run time: creating, placing and freeing them, and their collision outlines.
 *
 * The rest of an object's life is in its own files: the tick in objtick.c (with
 * cctv.c, autogun.c, vehicle.c, projectile.c and mine.c), drawing in objrender.c, damage
 * in objdamage.c and objhit.c, pickups in pickup.c, slots in objslots.c, carried guns and
 * hats in carried.c, and doors in door.c.
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

    modelBoundingBox = modelFileFindBbox(baseobj->model->obj);
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

    objUpdateCollisionBounds(baseobj);
}


void objPlaceAtPadSideways(ObjectRecord *obj, coord3d *pos, Mtxf *padmtx, StandTile *stan2, coord3d *pos2)
{
    f32 zmin;
    coord3d posdiff;
    StandTile *stan;
    Mtxf matrix;
    Mtxf yrot;

    zmin = bboxGetZmin(modelFileFindBbox(obj->model->obj));
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

    objUpdateCollisionBounds(obj);
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
