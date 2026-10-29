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


#define OBJECT_INTERACTION_TIMER_DELTA g_GlobalTimerDelta


// aprox 135 deg/s/s divided by 60fps
// (135/60/60/(180/M_PI_F))
#define CAM_ACCEL 0.00065449846f 

// 34.3 deg/s/s
#define AUTOGUN_SPIN_ACCEL_PER_FRAME    0.009973311f

// 2058 deg/s or 5.7 rotations per second
#define AUTOGUN_SPIN_MAX_SPEED          0.5983986f

#if defined(VERSION_EU)

#define AUTOGUN_YAW_MAX_SPEED          0.0008377581f

#define AUTOGUN_YAW_ACCEL_PER_FRAME    0.0000139626345f

#define AUTOGUN_PITCH_ACCEL_PER_FRAME  0.0000069813173f

#define AUTOGUN_PITCH_MAX_SPEED        0.00041887906f

#define AUTOGUN_ALERT_ACCEL_PER_FRAME  0.0010471976f

#define AUTOGUN_TRACKING_FRAMES 100

#define TRUCK_TURN_ACCEL_PER_FRAME     0.0001308997f

#define TRUCK_TURN_DECEL_PER_FRAME     0.0002617994f

#define TRUCK_TURN_MAX_SPEED           0.007853982f

#define CCTV_ALARM_FRAMES 250.0f

#else

// 2.4 deg/s (2.4/60/(180/M_PI_F}))
#define AUTOGUN_YAW_MAX_SPEED          0.00069813174f

// 2.4 deg/s/s (2.4/60/60/(180/M_PI_F))
#define AUTOGUN_YAW_ACCEL_PER_FRAME    0.000011635529f

// 1.2 deg/s 
#define AUTOGUN_PITCH_ACCEL_PER_FRAME  0.0000058177643f

// 1.2 deg/s/s 
#define AUTOGUN_PITCH_MAX_SPEED        0.00034906587f

//Faster rotation when autogun is actively tracking. 3 deg/s
#define AUTOGUN_ALERT_ACCEL_PER_FRAME  0.0008726647f

#define AUTOGUN_TRACKING_FRAMES 120

//0.375 deg/s/s - hmm, wheels maybe?
#define TRUCK_TURN_ACCEL_PER_FRAME     0.000109083085f

//0.75 deg/s/s
#define TRUCK_TURN_DECEL_PER_FRAME     0.00021816617f

// 22.5 deg/s 
#define TRUCK_TURN_MAX_SPEED           0.006544985f

#define CCTV_ALARM_FRAMES 300.0f

#endif


/* From the decomp.me ctx -- not present in any repo header. Without the macro,
 * C89 turns U32_TO_F32(...) into an implicit function call (5 sites, ~31 insns
 * short, wrong FP allocation everywhere downstream). */
#define U32MAX 4294967295
#define U32_TO_F32(x) (x*(1.0f/U32MAX))

struct LinkRecord *g_LevelLoadPropSwitch = NULL;
LockDoorRecord *g_LevelLoadPropLockDoor = NULL;
SafeObjectRecord *g_LevelLoadPropSafeItem = NULL;
f32 g_CctvAlarmTimeScale = 1.0;
f32 g_CctvDamageScale = 1.0;
f32 g_AutogunPendingDamageTick = 1.0;
f32 g_AutogunDamageScalar = 1.0;
f32 g_AutogunDamageScale = 1.0;

/*
* Set on level load.
*/
f32 g_SoloAmmoMultiplier = 1.0;

extern struct ModelAnimation *animation_table_ptrs2[];

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
 * Named same as Perfect Dark.
*/
/**
 * US address 7F043D70.
 * JP address 7F044074.
 * EU address 7F043E34.
 *
 * Seems to be a subset of Perfect Dark weaponTick.
*/
void chrobjWeaponTick(struct PropRecord* prop)
{
    struct ObjectRecord* obj;
    struct WeaponObjRecord *weapon;
    s32 exp_result;
    u32 owner_player_number;
    s32 p1;
    u32 owner_player_as_bitflag;
    struct PropRecord* player_prop;
    f32 diff_x;
    f32 diff_z;
    f32 diff_y;

    obj = prop->obj;

    if (playerGetShuffledIndex(playerGetCurrentNum()) != 0)
    {
        return;
    }

    if (obj->type == PROP_TYPE_EXPLOSION) // 7
    {
        if (obj->flags & PROPFLAG_IS_DRONE_GUN)
        {
            propExplode(prop, EXPLOSION_DEF_DRONE);
            obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
        }

        return;
    }

    if (obj->type == PROP_TYPE_SMOKE) // 8
    {
        weapon = prop->weapon;

        if (((weapon->weaponnum == ITEM_GRENADE) || (weapon->weaponnum == ITEM_GRENADEROUND )) && (weapon->timer >= 0))
        {
            weapon->timer -= g_ClockTimer;

            if (weapon->timer < 0)
            {
                propExplode(prop, (obj->flags2 & PROPFLAG2_DOOR_ALTCOORDSYSTEM) ? EXPLOSION_DEF_MASSIVE : EXPLOSION_DEF_STANDARD);
                obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
            }
        }
        else if (weapon->weaponnum == ITEM_ROCKETROUND)
        {
            if (weapon->timer == 0)
            {
                propExplode(prop, (obj->flags2 & PROPFLAG2_DOOR_ALTCOORDSYSTEM) ? EXPLOSION_DEF_MASSIVE : EXPLOSION_DEF_STANDARD);
                obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
            }
        }
        else if (weapon->weaponnum == ITEM_PLASTIQUE)
        {
            if (weapon->timer == 0)
            {
                propExplode(prop, EXPLOSION_DEF_MASSIVE);
                obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
                playerSurroundWithExplosions(PLASTIQUE_EXPLOSION_DELAY_TICKS);
                countdownTimerSetVisible(COUNTDOWN_HIDE_PLASTIQUE, FALSE);
            }
        }
        else if (((weapon->weaponnum == ITEM_TIMEDMINE) || (weapon->weaponnum == ITEM_BOMBCASE)) && (weapon->timer >= 0))
        {
            weapon->timer -= g_ClockTimer;

            if (weapon->timer < 0)
            {
                exp_result = propExplode(prop, (obj->flags2 & PROPFLAG2_DOOR_ALTCOORDSYSTEM) ? EXPLOSION_DEF_MASSIVE : EXPLOSION_DEF_STANDARD);
                if (exp_result == 0)
                {
                    return;
                }
                weapon->timer = -1;
                obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;

            }
        }
        else if (weapon->weaponnum == ITEM_REMOTEMINE)
        {
            if (g_RemoteMineOwnerTriggerFlag)
            {
                owner_player_number = (obj->runtime_bitflags & RUNTIMEBITFLAG_OWNER) >> RUNTIMEBITSHIFT_OWNER;
                owner_player_as_bitflag = (1 << owner_player_number);
                if (g_RemoteMineOwnerTriggerFlag & owner_player_as_bitflag)
                {
                    weapon->timer = 0;
                }
            }

            if (weapon->timer > 1)
            {
                weapon->timer -= g_ClockTimer;

                if (weapon->timer < 2)
                {
                    weapon->timer = 1;
                }
            }
            else if (weapon->timer == 0)
            {
                if (obj->flags2 & PROPFLAG2_DOOR_ALTCOORDSYSTEM)
                {
                    p1 = EXPLOSION_DEF_MASSIVE;
                }
                else
                {
                    p1 = EXPLOSION_DEF_STANDARD;

                    if (bossGetStageNum() == LEVELID_FACILITY)
                    {
                        p1 = EXPLOSION_DEF_FACILITY_REMOTE;
                    }
                }

                exp_result = propExplode(prop, p1);
                if (exp_result == 0)
                {
                    return;
                }
                weapon->timer = -1;
                obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
            }

        }
        else if (weapon->weaponnum == ITEM_PROXIMITYMINE)
        {
            if (weapon->timer > 1)
            {
                weapon->timer -= g_ClockTimer;

                if (weapon->timer < 2)
                {
                    weapon->timer = 1;
                    proxmineAdd(weapon);
                }
            }
            else if (weapon->timer == 1)
            {
                player_prop = currentPlayerGetProp();

                diff_x = player_prop->pos.f[0] - prop->pos.f[0];
                diff_y = player_prop->pos.f[1] - prop->pos.f[1];
                diff_z = player_prop->pos.f[2] - prop->pos.f[2];

                if ((diff_x * diff_x) + (diff_y * diff_y) + (diff_z * diff_z) < PROXMINE_TRIGGER_DIST_SQ)
                {
                    weapon->timer = 0;
                }
            }

            if (weapon->timer == 0)
            {
                exp_result = propExplode(prop, (obj->flags2 & PROPFLAG2_DOOR_ALTCOORDSYSTEM) ? EXPLOSION_DEF_MASSIVE : EXPLOSION_DEF_STANDARD);
                if (exp_result == 0)
                {
                    return;
                }

                weapon->timer = -1;
                obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
                proxmineRemove(weapon);
            }
        }
    }
}


void objDropRecursively(PropRecord *prop)
{
	PropRecord *child = prop->child;

	while (child)
    {
		PropRecord *prev = child->prev;
		objDropRecursively(child);
		objDrop(child);
		child = prev;
	}
}


void objTickOffscreen(PropRecord* prop)
{
    ObjectRecord* obj;
    PropRecord* next;
    PropRecord* child;

    obj = prop->obj;
    if (obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
    {
        objFree(obj, 1, obj->state & PROPSTATE_RESPAWN);
        return;
    }

    prop->flags &= ~(PROPFLAG_ONSCREEN);
    chrobjWeaponTick(prop);

    child = prop->child;
    while (child != NULL)
    {
        next = child->prev;
        objTickOffscreen(child);
        child = next;
    }
}


void objTickAttached(PropRecord* prop)
{
    ObjectRecord* obj;
    Model* model;
    PropRecord* child;
    PropRecord* prev;
    Mtxf* mtx;

    obj = prop->obj;
    model = obj->model;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
    {
        objFree(obj, 1, (obj->state & PROPSTATE_RESPAWN));
        return;
    }

    if ((model->attachedto_objinst != NULL) && (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED))
    {
        mtx = modelFindNodeMtx(model->attachedto, model->attachedto_objinst, 0);
        prop->flags |= PROPFLAG_ONSCREEN;
        model->render_pos = (RenderPosView*)dynAllocate(model->obj->numMatrices << 6);

        mtx4MultAffine(mtx, &obj->embedment->matrix, (Mtxf*)model->render_pos);
        modelUpdateRelationsQuick(model, model->obj->RootNode);
        chrobjWeaponTick(prop);

        child = prop->child;
        while (child != NULL)
        {
            prev = child->prev;
            objTickAttached(child);
            child = prev;
        }
    }
    else
    {
        prop->flags &= ~(PROPFLAG_ONSCREEN);
        chrobjWeaponTick(prop);

        child = prop->child;
        while (child != NULL)
        {
            prev = child->prev;
            objTickOffscreen(child);
            child = prev;
        }
    }
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


s32 vehicleAlignToFloor(ObjectRecord *obj)
{
    Model *model;
    PropRecord *prop;
    f32 *normalzptr;
    coord3d *modelpoint0;
    coord3d *modelpoint1;
    coord3d *modelpoint2;
    coord3d *modelpoint3;
    coord3d point0;
    coord3d point1;
    coord3d point2;
    coord3d point3;
    s32 result;
    Mtxf mtx;
    StandTile *tile;
    ModelRoData_BoundingBoxRecord *bbox;
    coord3d edge0;
    coord3d edge1;
    coord3d edge2;
    coord3d edge3;
    f32 cross2y;
    f32 cross0y;
    f32 cross1y;
    f32 cross3y;
    coord3d normal;
    coord3d yawvec;
    f32 ypos;

    model = obj->model;
    prop = obj->prop;
    modelpoint0 = (coord3d *) model->obj->Switches[1]->Data;
    modelpoint1 = (coord3d *) model->obj->Switches[2]->Data;
    modelpoint2 = (coord3d *) model->obj->Switches[3]->Data;
    modelpoint3 = (coord3d *) model->obj->Switches[4]->Data;
    result = 1;
    bbox = (ModelRoData_BoundingBoxRecord *) model->obj->Switches[6]->Data;

    mtx4LoadYRotation(((VehichleRecord *)obj)->roty, &mtx);
    mtx4ScaleRows012(model->scale, &mtx);
    mtx4SetTranslation(&obj->runtime_pos, &mtx);

    mtx4TransformVec(&mtx, modelpoint0, &point0);
    mtx4TransformVec(&mtx, modelpoint1, &point1);
    mtx4TransformVec(&mtx, modelpoint2, &point2);
    mtx4TransformVec(&mtx, modelpoint3, &point3);

    tile = prop->stan;
    if (stanWalkLine(&tile, prop->pos.x, prop->pos.z, point0.x, point0.z))
    {
        point0.y = stanGetFloorY(tile, point0.x, point0.z);
    }
    else
    {
        result = 0;
    }

    tile = prop->stan;
    if (stanWalkLine(&tile, prop->pos.x, prop->pos.z, point1.x, point1.z))
    {
        point1.y = stanGetFloorY(tile, point1.x, point1.z);
    }
    else
    {
        result = 0;
    }

    tile = prop->stan;
    if (stanWalkLine(&tile, prop->pos.x, prop->pos.z, point2.x, point2.z))
    {
        point2.y = stanGetFloorY(tile, point2.x, point2.z);
    }
    else
    {
        result = 0;
    }

    tile = prop->stan;
    if (stanWalkLine(&tile, prop->pos.x, prop->pos.z, point3.x, point3.z))
    {
        point3.y = stanGetFloorY(tile, point3.x, point3.z);
    }
    else
    {
        result = 0;
    }

    if (result)
    {
        edge0.x = point1.x - point0.x;
        edge0.y = point1.y - point0.y;
        edge0.z = point1.z - point0.z;
        guNormalize(&edge0.x, &edge0.y, &edge0.z);

        edge1.x = point3.x - point1.x;
        edge1.y = point3.y - point1.y;
        edge1.z = point3.z - point1.z;
        guNormalize(&edge1.x, &edge1.y, &edge1.z);

        edge2.x = point2.x - point3.x;
        edge2.y = point2.y - point3.y;
        edge2.z = point2.z - point3.z;
        guNormalize(&edge2.x, &edge2.y, &edge2.z);

        edge3.x = point0.x - point2.x;
        edge3.y = point0.y - point2.y;
        edge3.z = point0.z - point2.z;
        guNormalize(&edge3.x, &edge3.y, &edge3.z);

        normalzptr = &edge1.z;

        cross0y = (edge0.z * edge1.x) - ((*normalzptr) * edge0.x);
        cross1y = ((*normalzptr) * edge2.x) - (edge2.z * edge1.x);
        cross2y = (edge2.z * edge3.x) - (edge3.z * edge2.x);
        cross3y = (edge3.z * edge0.x) - (edge0.z * edge3.x);

        if (((cross1y <= cross0y) && (cross2y <= cross0y)) && (cross3y <= cross0y))
        {
            normal.x = (edge0.y * edge1.z) - (edge1.y * edge0.z);
            normal.y = cross0y;
            normal.z = (edge0.x * edge1.y) - (edge1.x * edge0.y);
        }
        else if ((cross2y <= cross1y) && (cross3y <= cross1y))
        {
            normal.x = (edge1.y * edge2.z) - (edge2.y * edge1.z);
            normal.y = cross1y;
            normal.z = (edge1.x * edge2.y) - (edge2.x * edge1.y);
        }
        else if (cross3y <= cross2y)
        {
            normal.x = (edge2.y * edge3.z) - (edge3.y * edge2.z);
            normal.y = cross2y;
            normal.z = (edge2.x * edge3.y) - (edge3.x * edge2.y);
        }
        else
        {
            normal.x = (edge3.y * edge0.z) - (edge0.y * edge3.z);
            normal.y = cross3y;
            normal.z = (edge3.x * edge0.y) - (edge0.x * edge3.y);
        }

        normalzptr = &normal.z;

        yawvec.x = sinf(((VehichleRecord *)obj)->roty);
        yawvec.y = 0.0f;
        yawvec.z = cosf(((VehichleRecord *)obj)->roty);

        mtx4LoadIdentity(&obj->mtx);

        if (obj->mtx.m[0][0])
        {
            // empty
        }

        if (obj->mtx.m[1][1])
        {
            // empty
        }

        obj->mtx.m[1][0] = normal.x;

        if (obj->mtx.m[1][0])
        {
            // empty
        }

        obj->mtx.m[1][1] = normal.y;

        obj->mtx.m[1][2] = ((f32*)&normal)[2];

        if (normal.x)
        {
            // empty
        }

        obj->mtx.m[0][0] = (obj->mtx.m[1][1] * yawvec.f[2]) - (obj->mtx.m[1][2] * yawvec.f[1]);
        obj->mtx.m[0][1] = (obj->mtx.m[1][2] * yawvec.f[0]) - (obj->mtx.m[1][0] * yawvec.f[2]);
        obj->mtx.m[0][2] = (obj->mtx.m[1][0] * (f32)(yawvec.y)) - (obj->mtx.m[1][1] * yawvec.f[0]);

        obj->mtx.m[2][0] = (((f32 *)(&obj->mtx))[1] * ((f32 *)(&obj->mtx))[6]) - (((f32 *)(&obj->mtx))[2] * obj->mtx.m[1][1]);
        obj->mtx.m[2][1] = (((f32 *)(&obj->mtx))[2] * ((f32 *)(&obj->mtx))[4]) - (((f32 *)(&obj->mtx))[0] * ((f32 *)(&obj->mtx))[6]);
        obj->mtx.m[2][2] = (((f32 *)(&obj->mtx))[0] * obj->mtx.m[1][1]) - (((f32 *)(&obj->mtx))[1] * ((f32 *)(&obj->mtx))[4]);

        mtx4ScaleRows012(model->scale, &obj->mtx);
        mtx4TransformVec(&obj->mtx, modelpoint0, &point1);

        ypos = (point0.y - (bboxGetYmin(bbox) * model->scale)) - point1.y;
        prop->pos.y = ypos;
        obj->runtime_pos.y = ypos;
    }
    else
    {
        mtx4LoadYRotation(((VehichleRecord *)obj)->roty, &obj->mtx);
        mtx4ScaleRows012(model->scale, &obj->mtx);

        ypos = stanGetFloorY(prop->stan, prop->pos.x, prop->pos.z) - ((bboxGetYmin(bbox) + modelpoint0->f[1]) * model->scale);

        prop->pos.y = ypos;
        obj->runtime_pos.y = ypos;
    }

    return result;
}


s32 glassCalculateOpacity(coord3d *pos, f32 xludist, f32 opadist, f32 minOpacity)
{
    coord3d *campos = &currentPlayerGetProp()->pos;
    s32 opacity;
    f32 xdiff = pos->x - campos->x;
    f32 ydiff = pos->y - campos->y;
    f32 zdiff = pos->z - campos->z;

    f32 distance = sqrtf(xdiff * xdiff + ydiff * ydiff + zdiff * zdiff);

    if (distance > opadist)
    {
        opacity = 255;
    } else if (distance < xludist)
    {
        opacity = minOpacity * 255;
    }
    else
    {
        opacity = (((distance - xludist) * (1.0f - minOpacity)) / (opadist - xludist) + minOpacity) * 255;
    }

    return opacity;
}


/**
 * Co-op: TRUE if a camera or drone gun may watch the player. Counter-op:
 * only Bond, as they are the guards' side's own. Co-op: any living player.
 */
static s32 coopObjMayWatchPlayer(s32 player)
{
    if (g_CounterOpMode)
    {
        return player == COUNTEROP_BOND;
    }

    return !g_playerPointers[player]->bonddead;
}

/**
 * Co-op: the player a drone gun aims at and shoots this frame, or -1 to
 * keep the current one (outside co-op, or with nobody to watch). Objects
 * only tick on one player's pass a frame, and which player that is changes
 * from frame to frame, so the gun takes the nearest player it may watch
 * instead and keeps one target.
 */
static s32 coopAutogunTargetPlayer(PropRecord *prop)
{
    s32 best = -1;
    f32 bestdist = 0.0f;
    s32 i;

    if (!g_CoopMode)
    {
        return -1;
    }

    for (i = 0; i < getPlayerCount(); i++)
    {
        PropRecord *pp = g_playerPointers[i]->prop;
        f32 x;
        f32 y;
        f32 z;
        f32 dist;

        if ((pp == NULL) || !coopObjMayWatchPlayer(i))
        {
            continue;
        }

        x = pp->pos.f[0] - prop->pos.f[0];
        y = pp->pos.f[1] - prop->pos.f[1];
        z = pp->pos.f[2] - prop->pos.f[2];
        dist = (x * x) + (y * y) + (z * z);

        if ((best < 0) || (dist < bestdist))
        {
            best = i;
            bestdist = dist;
        }
    }

    return best;
}

s32 objTick(struct PropRecord *prop)
{
	struct ObjectRecord *obj;
	struct Model *model;
	Mtxf *mtxs;
	TICKOP tickop;
	s32 playerCount;
	bool applyFogCull;
	s32 i;
	s32 visible;

    /**
     * Relevant only for MP. TRUE on the one pass per frame that advances this object's
	 * shared state (projectile physics, door sounds) -- things you want to calculate
     * only once per frame, not for every player per frame. Normally applies to
	 * first player in the shuffled order determined by playerGetShuffledIndex().
     * But for an in-flight projectile it applies to the player who fired it.
	 * Code outside this guard is per-viewport render setup and runs every pass.
     */
	bool isSimOwner;

	// Projectiles
	struct Projectile *projectile;
	bool objMovedThisFrame;
	coord3d newpos; // vehicles

	// Doors and glass
	struct DoorRecord *door;
	struct TintedGlassRecord *glass;
	f32 previousOpenPosition; // Start-of-tick snapshot for how open a door is.
	s32 closePortal;

	// Cameras
	struct CCTVRecord *cctv;
	struct PropRecord *playerProp;
	struct StandTile *losTile;
	s32 cctvSeesPlayer;
	s32 cctvWatch;
	s32 cctvWatchEnd;
	s32 cctvSawPlayer;
	f32 xdiff;
	f32 ydiff;
	f32 zdiff;
	f32 viewDist;
	f32 angleToPlayer;
	f32 camYaw;
	f32 angleDelta;
	f32 sweepTarget;
	f32 newSpeed;
	coord3d headpos;

	// Autoguns
	struct AutogunRecord *autogun;
	struct PropRecord *targetProp;
	s32 coopTarget;
	s32 coopPrevPlayer;
	coord3d playerDirVec;
	f32 horizontalDistSq;
	f32 horizontalDist;
	f32 distanceToPlayer;
	f32 playerYaw;
	f32 playerPitch;
	f32 yawToPlayer;
	f32 yawFromHome;
	f32 aimLimit;
	f32 wideAimLimit;
	f32 aimYaw;
	f32 aimPitch;
	f32 yawToGo;
	f32 pitchToGo;
	s32 seesPlayer;
	s32 isTracking;
	s32 hasLineOfSight;
	coord3d gunpos;
	f32 gunYaw;
	f32 gunPitch;
	s32 leftFlash;
	s32 rightFlash;
	s32 flashNode;
	s32 showBeam;
	s32 hitWall;
	struct StandTile *shotTile;
	struct StandTile *hitTile;
	coord3d shotFrom;
	coord3d shotDir;
	coord3d shotTo;
	Mtxf nodeWorldMtx;
	f32 targetDistSq;
	f32 targetDist;
	f32 damage;
	BeamRecord *beam;
	f32 beamDist;
	f32 invDist;

	// Vehicles and aircraft
	struct VehichleRecord *vehicle;
	struct AircraftRecord *aircraft;
	struct Model *aircraftModel;
	struct PadRecord *pad;
	waypoint *currentWaypoint;
	coord3d *waypointPosition;
	struct StandTile *currentTile;
	struct StandTile *previousTile;
	coord3d previousPos;
	coord3d forwardDir;
	coord3d xminDir;
	coord3d xmaxDir;
	coord3d zminDir;
	coord3d zmaxDir;
	struct ModelRoData_BoundingBoxRecord *bbox;
	coord3d *axlePos;
	s32 canMove;
	s32 engineVolume;
	f32 targetYaw;
	f32 previousYaw;
	f32 previousTurnSpeed;
	f32 truckAngularVelocity;
	f32 sideSlip;
	f32 timeLeft;
	coord3d *wheel1Pos;
	coord3d *wheel2Pos;
	coord3d *wheel3Pos;
	coord3d *wheel4Pos;
	struct ModelRoData_BoundingBoxRecord *wheelBbox;
	f32 wheelDiameter;
	f32 wheelbase;
	f32 wheelSpin;
	f32 absTurn;
	f32 sideOffset;
	Mtxf wheelRollMtx;
	Mtxf wheelSteerMtx;
	coord3d *rotorPos;
	ModelRenderData renderdata;
	Mtxf rotorMtx;

	// Tank
	struct ModelNode **switches;
	coord3d *turretPos;
	coord3d *gunPos;
	coord3d *gunTipPos;
	coord3d *turretTopPos;
	struct ModelRoData_BoundingBoxRecord *turretBbox;
	f32 turretPitch;
	f32 turretYaw;
	Mtxf turretWorldMtx;

	// Model matrices
	Mtxf objmtx;
	Mtxf yawmtx;
	Mtxf *nodemtx;
	coord3d *nodepos;
	struct PropRecord *prev;
    
	obj = prop->obj;
	model = obj->model;

	objMovedThisFrame = FALSE;
	tickop = TICKOP_NONE;

	previousOpenPosition = 0.0f;
	playerCount = getPlayerCount();
	applyFogCull = TRUE;

	if (obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
	{
		objFree(obj, 0, obj->state & PROPSTATE_RESPAWN);
		return 1;
	}
	else if (obj->runtime_bitflags & RUNTIMEBITFLAG_ISRETICK)
	{
		obj->runtime_bitflags &= ~RUNTIMEBITFLAG_ISRETICK;
	}
	else if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
	{
		prop->flags &= 0xFFFD;
		obj->runtime_bitflags |= RUNTIMEBITFLAG_ISRETICK;
		return 3;
	}

	if (playerCount == 1)
	{
		isSimOwner = TRUE;
	}
	else
	{
		isSimOwner = playerGetShuffledIndex(playerGetCurrentNum()) == 0;

		if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
		{
			projectile = obj->projectile;

			// JP/EU added the player check. Without it a guard's dropped gun
			// (owner = the guard) is never simulated, so in co-op it floats.
			if (projectile->ownerprop != NULL
				&& playerGetIndexForProp(projectile->ownerprop) >= 0
			)
			
            isSimOwner = projectile->ownerprop == g_CurrentPlayer->prop;
			
		}
	}

	if (isSimOwner)
	{
		if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
		{
			tickop = projectileTick(prop, &objMovedThisFrame);
		}

		if (objMovedThisFrame)
		{
			chrobjCollisionRelated(obj);
			setupUpdateObjectRoomPosition(obj);
			objGetShadeColour(obj->prop, &obj->nextcol);
			proxminesDetonateNear(&obj->runtime_pos);
		}

		if (obj->type == PROPDEF_DOOR)
		{
			previousOpenPosition = prop->door->openPosition;
			doorTick(prop);
		}
		else if ((obj->type == PROPDEF_CCTV) && (!(obj->flags & PROPFLAG_IS_DRONE_GUN)))
		{
			cctv = (struct CCTVRecord *) prop->obj;

			if (cctv->towardsYaw1 != 0)
			{
				sweepTarget = cctv->sweepYaw1;
			}
			else
			{
				sweepTarget = cctv->sweepYaw2;
			}

			// Co-op: the camera watches every player it may, and an alarm
			// needs it to see any of them for long enough.
			cctvWatch = g_CoopMode ? 0 : playerGetCurrentNum();
			cctvWatchEnd = g_CoopMode ? getPlayerCount() : (cctvWatch + 1);
			cctvSawPlayer = FALSE;

			for (; cctvWatch < cctvWatchEnd; cctvWatch++)
			{
				if (g_CoopMode && (!coopObjMayWatchPlayer(cctvWatch) || (g_playerPointers[cctvWatch]->prop == NULL)))
				{
					continue;
				}

				playerProp = g_CoopMode ? g_playerPointers[cctvWatch]->prop : currentPlayerGetProp();
				xdiff = playerProp->pos.f[0] - obj->runtime_pos.f[0];
				viewDist = cctv->viewDist;
				ydiff = playerProp->pos.f[1] - obj->runtime_pos.f[1];
				zdiff = playerProp->pos.f[2] - obj->runtime_pos.f[2];
				cctvSeesPlayer = 1;

				if ((viewDist > 0.0f) && ((viewDist * viewDist) < (((xdiff * xdiff) + (ydiff * ydiff)) + (zdiff * zdiff))))
				{
					cctvSeesPlayer = 0;
				}

				if (obj->flags & PROPFLAG_INMOTION)
				{
					cctvSeesPlayer = 0;
				}

				if (cctvSeesPlayer != 0)
				{
					angleToPlayer = mathAtan2(xdiff, zdiff);
					camYaw = cctv->yaw;

					if (camYaw < 0.0f)
					{
						camYaw += M_TAU_F;
					}
					else if (camYaw >= M_TAU_F)
					{
						camYaw -= M_TAU_F;
					}

					camYaw += cctv->baseYaw;

					if (camYaw >= M_TAU_F)
					{
						camYaw -= M_TAU_F;
					}

					angleDelta = angleToPlayer - camYaw;

					if (angleToPlayer < camYaw)
					{
						angleDelta += M_TAU_F;
					}

					angleDelta -= M_PI_F;

					if (angleDelta < 0.0f)
					{
						angleDelta += M_TAU_F;
					}

					if (angleDelta > M_PI_F)
					{
						angleDelta -= M_TAU_F;
					}

					if ((angleDelta > DegToRad(45)) || (angleDelta < DegToRad(-45)))
					{
						cctvSeesPlayer = 0;
					}
				}

				if (cctvSeesPlayer != 0)
				{
					losTile = prop->stan;
					playerSetMoving(playerProp, 0);
					if (stanTestLineUnobstructed(&losTile, prop->pos.f[0], prop->pos.f[2], playerProp->pos.f[0], playerProp->pos.f[2], 0x1B, 100.0f, 100.0f, 0.0f, 1.0f) != 0)
					{
						cctvSawPlayer = TRUE;
					}

					playerSetMoving(playerProp, 1);
				}
			}

			if (cctvSawPlayer)
			{
				cctv->timer += g_ClockTimer;
				if (cctv->timer >= ((s32) (CCTV_ALARM_FRAMES * g_CctvAlarmTimeScale)))
				{
					alarmActivate();
					cctv->timer = 0;
				}
			}

			if (cctv->yaw < sweepTarget)
			{
				if ((sweepTarget - (((cctv->yawSpeed * cctv->yawSpeed) * 0.5f) / CAM_ACCEL)) <= cctv->yaw)
				{
					cctv->yawSpeed = (f32) (cctv->yawSpeed - (CAM_ACCEL * g_GlobalTimerDelta));
					if (cctv->yawSpeed < CAM_ACCEL)
					{
						cctv->yawSpeed = CAM_ACCEL;
					}
				}
				else if (cctv->yawSpeed < cctv->maxYawSpeed)
				{
					newSpeed = cctv->yawSpeed + (CAM_ACCEL * g_GlobalTimerDelta);
					if (cctv->maxYawSpeed < newSpeed)
					{
						newSpeed = cctv->maxYawSpeed;
					}

					if (cctv->yaw < (sweepTarget - (((newSpeed * newSpeed) * 0.5f) / CAM_ACCEL)))
					{
						cctv->yawSpeed = newSpeed;
					}
				}

				cctv->yaw += cctv->yawSpeed * g_GlobalTimerDelta;
				if (sweepTarget <= cctv->yaw)
				{
					cctv->yaw = sweepTarget;
					cctv->yawSpeed = 0.0f;
					cctv->towardsYaw1 = 0;
				}
			}
			else
			{
				if (cctv->yaw <= (sweepTarget + (((cctv->yawSpeed * cctv->yawSpeed) * 0.5f) / CAM_ACCEL)))
				{
					cctv->yawSpeed = (f32) (cctv->yawSpeed - (CAM_ACCEL * g_GlobalTimerDelta));
					if (cctv->yawSpeed < CAM_ACCEL)
					{
						cctv->yawSpeed = CAM_ACCEL;
					}
				}
				else if (cctv->yawSpeed < cctv->maxYawSpeed)
				{
					newSpeed = cctv->yawSpeed + (CAM_ACCEL * g_GlobalTimerDelta);
					if (cctv->maxYawSpeed < newSpeed)
					{
						newSpeed = cctv->maxYawSpeed;
					}

					if (cctv->yaw > (sweepTarget + (((newSpeed * newSpeed) * 0.5f) / CAM_ACCEL)))
					{
						cctv->yawSpeed = newSpeed;
					}
				}

				cctv->yaw -= cctv->yawSpeed * g_GlobalTimerDelta;
				if (cctv->yaw <= sweepTarget)
				{
					cctv->yaw = sweepTarget;
					cctv->yawSpeed = 0.0f;
					cctv->towardsYaw1 = 1;
				}
			}
		}
		else if ((obj->type == PROPDEF_AUTOGUN) && (!(obj->flags & PROPFLAG_IS_DRONE_GUN)))
		{
			autogun = (struct AutogunRecord *) prop->obj;
			coopTarget = coopAutogunTargetPlayer(prop);
			targetProp = (coopTarget >= 0) ? g_playerPointers[coopTarget]->prop : currentPlayerGetProp();
			seesPlayer = 0;
			isTracking = 0;
			hasLineOfSight = 0;
			if (obj->flags2 & PROPFLAG_IS_DOUBLE)
			{
				if (obj->flags2 & PROPFLAG2_40000000)
				{
					autogun->homePitch = autogun->pitch;
					autogun->homeYaw = autogun->yaw;
				}
				else if ((autogun->yaw == autogun->homeYaw) && (autogun->pitch == autogun->homePitch))
				{
					autogun->homePitch = (((U32_TO_F32(randomGetNext()) * 39.0f) + 1.0f) * M_TAU_F) / 360.0f; //degtorad 
					autogun->homeYaw = U32_TO_F32(randomGetNext()) * M_TAU_F;
				}

				physTurnTowards(&autogun->yaw, autogun->homeYaw, &autogun->yawSpeed, AUTOGUN_YAW_ACCEL_PER_FRAME, AUTOGUN_YAW_ACCEL_PER_FRAME, AUTOGUN_YAW_MAX_SPEED, g_ClockTimer);
				physTurnTowards(&autogun->pitch, autogun->homePitch, &autogun->pitchSpeed, AUTOGUN_PITCH_ACCEL_PER_FRAME, AUTOGUN_PITCH_ACCEL_PER_FRAME, AUTOGUN_PITCH_MAX_SPEED, g_ClockTimer);
			}
			else
			{
				yawFromHome = targetProp->pos.f[0] - obj->runtime_pos.f[0];
				playerDirVec.f[1] = (targetProp->pos.f[1] - obj->runtime_pos.f[1]) - 20.0f;//Aim 20 units below player’s head
				zdiff = targetProp->pos.f[2] - obj->runtime_pos.f[2];
				horizontalDistSq = (yawFromHome * yawFromHome) + (zdiff * zdiff);
				playerDirVec.f[2] = yawFromHome;
				playerDirVec.f[0] = zdiff;
				horizontalDist = sqrtf(horizontalDistSq);
				distanceToPlayer = horizontalDist;
				if (obj->flags & PROPFLAG_DOOR_TWOWAY)
				{
					horizontalDistSq += playerDirVec.f[1] * playerDirVec.f[1];
					distanceToPlayer = sqrtf(horizontalDistSq);
				}

				aimLimit = chrGetAimLimitAngle(horizontalDistSq);
				aimYaw = autogun->homeYaw;
				aimPitch = autogun->homePitch;
				if (distanceToPlayer <= autogun->aimdist)
				{
					playerYaw = mathAtan2(playerDirVec.f[2], playerDirVec.f[0]);
					playerPitch = mathAtan2(playerDirVec.f[1], horizontalDist);
					if ((obj->flags & PROPFLAG_NO_AMMO) || (obj->flags & PROPFLAG_INMOTION))
					{
						seesPlayer = 1;
					}
					else
					{
						yawToPlayer = playerYaw - autogun->yaw;
						if (yawToPlayer < 0.0f)
						{
							yawToPlayer += M_TAU_F;
						}

						if (yawToPlayer > M_PI_F)
						{
							yawToPlayer -= M_TAU_F;
						}

						if ((yawToPlayer < DegToRad(70)) && (yawToPlayer > DegToRad(-70)))
						{
							seesPlayer = 1;
						}
					}

					if (seesPlayer != 0)
					{
						yawFromHome = playerYaw - autogun->homeYaw;
						losTile = prop->stan;
						if (yawFromHome < (-M_PI_F))
						{
							yawFromHome += M_TAU_F;
						}
						else if (yawFromHome >= M_PI_F)
						{
							yawFromHome -= M_TAU_F;
						}

						playerSetMoving(targetProp, 0);
						if ((((yawFromHome <= autogun->maxYaw) && (autogun->minYaw <= yawFromHome)) && (stanTestLineUnobstructed(&losTile, prop->pos.f[0], prop->pos.f[2], targetProp->pos.f[0], targetProp->pos.f[2], 0x1B, prop->pos.f[1], prop->pos.f[1], targetProp->pos.f[1], targetProp->pos.f[1]) != 0)) && ((losTile) == targetProp->stan))
						{
							obj->flags |= PROPFLAG_INMOTION;
							hasLineOfSight = 1;
							aimYaw = playerYaw;
							aimPitch = playerPitch;
						}
						else if ((autogun->lastSeenTime >= 0) && ((g_GlobalTimer - AUTOGUN_TRACKING_FRAMES) < autogun->lastSeenTime)) //cooldown 2 seconds
						{
							aimYaw = autogun->yaw;
							aimPitch = autogun->pitch;
						}
						else
						{
							seesPlayer = 0;
						}

						playerSetMoving(targetProp, 1);
					}
				}

				if (seesPlayer != 0)
				{
					aimLimit = chrGetAimLimitAngle(horizontalDistSq);
				}

				if (autogun->is_active != 0)
				{
                    //Sway once every 2 seconds while firing
					aimYaw += (aimLimit * 0.8f) * sinf((((f32) (((s32) g_GlobalTimer) % AUTOGUN_TRACKING_FRAMES)) * M_TAU_F) / (f32) AUTOGUN_TRACKING_FRAMES);
					if (aimYaw < 0.0f)
					{
						aimYaw += M_TAU_F;
					}

					if (aimYaw >= M_TAU_F)
					{
						aimYaw -= M_TAU_F;
					}
				}

				yawFromHome = aimYaw - autogun->homeYaw;
				if (yawFromHome < (-M_PI_F))
				{
					yawFromHome += M_TAU_F;
				}
				else if (yawFromHome >= M_PI_F)
				{
					yawFromHome -= M_TAU_F;
				}

				if (autogun->maxYaw < yawFromHome)
				{
					aimYaw = autogun->homeYaw + autogun->maxYaw;
				}
				else if (yawFromHome < autogun->minYaw)
				{
					aimYaw = autogun->homeYaw + autogun->minYaw;
				}

				if (aimYaw < 0.0f)
				{
					aimYaw += M_TAU_F;
				}

				if (aimYaw >= M_TAU_F)
				{
					aimYaw -= M_TAU_F;
				}

				physTurnTowards(&autogun->yaw, aimYaw, &autogun->yawSpeed, AUTOGUN_ALERT_ACCEL_PER_FRAME  , AUTOGUN_ALERT_ACCEL_PER_FRAME  , autogun->speed, g_ClockTimer);
				physTurnTowards(&autogun->pitch, aimPitch, &autogun->pitchSpeed, AUTOGUN_ALERT_ACCEL_PER_FRAME  , AUTOGUN_ALERT_ACCEL_PER_FRAME  , autogun->speed, g_ClockTimer);
				yawToGo = aimYaw - autogun->yaw;
				if (yawToGo < 0.0f)
				{
					yawToGo += M_TAU_F;
				}

				if (yawToGo > M_PI_F)
				{
					yawToGo -= M_TAU_F;
				}

				pitchToGo = aimPitch - autogun->pitch;
				if (pitchToGo < 0.0f)
				{
					pitchToGo += M_TAU_F;
				}

				if (pitchToGo > M_PI_F)
				{
					pitchToGo -= M_TAU_F;
				}

				autogun->is_active = 0;
				if (seesPlayer != 0)
				{
					if ((((yawToGo < aimLimit) && ((-aimLimit) < yawToGo)) && (pitchToGo < aimLimit)) && ((-aimLimit) < pitchToGo))
					{
						autogun->is_active = 1;
						isTracking = 1;
						if (hasLineOfSight != 0)
						{
							autogun->lastSeenTime = (s32) g_GlobalTimer;
							autogun->onTargetTime = (s32) g_GlobalTimer;
						}
					}
					else
					{
						wideAimLimit = 2.0f * aimLimit;
						if ((((yawToGo < wideAimLimit) && ((-wideAimLimit) < yawToGo)) && (pitchToGo < wideAimLimit)) && ((-wideAimLimit) < pitchToGo))
						{
							autogun->is_active = 1;
							isTracking = 1;
							if (hasLineOfSight != 0)
							{
								autogun->lastSeenTime = (s32) g_GlobalTimer;
							}
						}
						else if ((autogun->lastSeenTime >= 0) && ((g_GlobalTimer - AUTOGUN_TRACKING_FRAMES) < autogun->lastSeenTime))
						{
							autogun->is_active = 1;
							isTracking = 1;
						}
					}
				}

				if (isTracking != 0) //firing
				{
					autogun->barrelSpinSpeed += AUTOGUN_SPIN_ACCEL_PER_FRAME * g_GlobalTimerDelta;
					if (autogun->barrelSpinSpeed > AUTOGUN_SPIN_MAX_SPEED)
					{
						autogun->barrelSpinSpeed = AUTOGUN_SPIN_MAX_SPEED;
					}
				}
				else if (autogun->barrelSpinSpeed > 0.0f)
				{
					for (i = 0; i < g_ClockTimer; i++)
					{
						autogun->barrelSpinSpeed *= 0.99f; //barrel loses 45% of its spin per second when idle
					}


					if (autogun->barrelSpinSpeed <= 0.0001f)
					{
						autogun->barrelSpinSpeed = 0.0f;
					}
				}

				if (autogun->barrelSpinSpeed > 0.0f)
				{
					autogun->barrelSpinAngle += autogun->barrelSpinSpeed * g_GlobalTimerDelta;
					while (autogun->barrelSpinAngle >= M_TAU_F)
					{
						autogun->barrelSpinAngle -= M_TAU_F;
					}

				}
			}
		}
		else if (obj->type == PROPDEF_VEHICHLE)
		{
			vehicle = (struct VehichleRecord *) obj;
			targetYaw = vehicle->roty;
			waypointPosition = NULL;

			aiForObject((PropDefHeaderRecord *) vehicle);

			if (vehicle->speedtime60 >= 0.0f)
			{
				if (vehicle->speedtime60 <= g_GlobalTimerDelta)
				{
					vehicle->speed = vehicle->speedaim;
				}
				else
				{
					vehicle->speed += ((vehicle->speedaim - vehicle->speed) * g_GlobalTimerDelta) / vehicle->speedtime60;
				}

				vehicle->speedtime60 -= g_GlobalTimerDelta;
			}

			engineVolume = 0;

			if (((!(obj->flags2 & PROPFLAG2_00080000)) && (objIsHealthy(obj) != 0)) && ((vehicle->speed > 0.0f) || (vehicle->speedaim > 0.0f)))
			{
				engineVolume = sfxVolumeAtRange(&vehicle->runtime_pos, 2000.0f, 3000.0f);
			}

			if (engineVolume > 0)
			{
				if (((vehicle->Sound == NULL) || (sndGetPlayingState(vehicle->Sound) == 0)) && (lvGetControlsLocked() == 0))
				{
					sndPlaySfx(g_musicSfxBufferPtr, TRUCK_RUN_SFX, &vehicle->Sound);
				}

				if (vehicle->Sound != NULL)
				{
					sndCreatePostEvent(vehicle->Sound, 8, engineVolume);
				}
			}
			else if ((vehicle->Sound != NULL) && (sndGetPlayingState(vehicle->Sound) != 0))
			{
				sndDeactivate(vehicle->Sound);
			}

			if (vehicle->path != NULL)
			{
				currentWaypoint = &g_CurrentSetup.pathwaypoints[vehicle->path->waypoints[vehicle->nextstep]];
				waypointPosition = &g_CurrentSetup.pads[currentWaypoint->padID].pos;
				targetYaw = mathAtan2(waypointPosition->f[0] - vehicle->runtime_pos.f[0], waypointPosition->f[2] - vehicle->runtime_pos.f[2]);
				if (vehicle->flags & PROPFLAG_INMOTION)
				{
					vehicle->roty = targetYaw;
					obj->flags &= ~PROPFLAG_INMOTION;
					vehicleAlignToFloor(vehicle);
				}
			}
			else if (vehicle->flags & PROPFLAG_INMOTION) 
			{
				vehicle->roty = mathAtan2(vehicle->mtx.m[2][0], vehicle->mtx.m[2][2]);
				vehicle->flags &= ~PROPFLAG_INMOTION;
				vehicleAlignToFloor(vehicle);
			}

			if (vehicle->speed > 0.0f)
			{
				truckAngularVelocity = 0.0f;
				currentTile = prop->stan;
				previousYaw = vehicle->roty;
				previousTurnSpeed = vehicle->turnrot60;
				if (waypointPosition != NULL)
				{
					truckAngularVelocity = 0.0f;
					forwardDir.f[0] = sinf(vehicle->roty);
					forwardDir.f[1] = 0.0f;
					forwardDir.f[2] = cosf(vehicle->roty);
					if (rayPassesNearPoint2d(&vehicle->runtime_pos, &forwardDir, waypointPosition, 10.0f) != 0)
					{
						targetYaw = vehicle->roty;
					}
				}

				physTurnTowards(&vehicle->roty, targetYaw, &vehicle->turnrot60, TRUCK_TURN_ACCEL_PER_FRAME, TRUCK_TURN_DECEL_PER_FRAME, TRUCK_TURN_MAX_SPEED, g_ClockTimer);
				while (vehicle->roty >= M_TAU_F)
				{
					vehicle->roty -= M_TAU_F;
				}


				while (vehicle->roty < 0.0f)
				{
					vehicle->roty += M_TAU_F;
				}


				if (targetYaw == vehicle->roty)
				{
					if ((vehicle->turnrot60 <= TRUCK_TURN_DECEL_PER_FRAME) && (vehicle->turnrot60 >= (-TRUCK_TURN_DECEL_PER_FRAME)))
					{
						vehicle->turnrot60 = 0.0f;
					}
				}

				axlePos = &model->obj->Switches[3]->Data->Group.Origin;
				if (g_GlobalTimerDelta > 0.0f)
				{
					truckAngularVelocity = (vehicle->roty - previousYaw) / g_GlobalTimerDelta;
				}

				if (truckAngularVelocity < 0.0f)
				{
					truckAngularVelocity += M_TAU_F;
				}

				sideSlip = ((axlePos->f[2] * model->scale) * sinf(truckAngularVelocity)) * g_GlobalTimerDelta;
				forwardDir.f[0] = sinf(vehicle->roty);
				forwardDir.f[1] = 0.0f;
				forwardDir.f[2] = cosf(vehicle->roty);
				newpos.f[0] = (vehicle->runtime_pos.f[0] + ((vehicle->speed * g_GlobalTimerDelta) * forwardDir.f[0])) - (forwardDir.f[2] * sideSlip);
				newpos.f[1] = vehicle->runtime_pos.f[1];
				newpos.f[2] = (vehicle->runtime_pos.f[2] + ((vehicle->speed * g_GlobalTimerDelta) * forwardDir.f[2])) + (forwardDir.f[0] * sideSlip);
				if ((stanTestLineUnobstructed(&currentTile, prop->pos.f[0], prop->pos.f[2], newpos.f[0], newpos.f[2], 0x1F, 0.0f, 1.0f, 0.0f, 1.0f) != 0) && (stanTestVolume(&currentTile, newpos.f[0], newpos.f[2], 10.0f, 0x1F, 0.0f, 1.0f) < 0))
				{
					previousTile = prop->stan;
					previousPos.f[0] = prop->pos.f[0];
					previousPos.f[1] = prop->pos.f[1];
					previousPos.f[2] = prop->pos.f[2];
					prop->stan = currentTile;
					vehicle->runtime_pos.f[0] = (prop->pos.f[0] = newpos.f[0]);
					vehicle->runtime_pos.f[2] = (prop->pos.f[2] = newpos.f[2]);
					chrobjCollisionRelated(obj);
					setupUpdateObjectRoomPosition(obj);
					canMove = vehicleFindBlockingProp(prop);
					if (canMove != 0)
					{
						bbox = &model->obj->Switches[10]->Data->BoundingBox;
						xminDir.f[0] = bbox->Bounds.xmin * vehicle->mtx.m[0][0];
						xminDir.f[2] = bbox->Bounds.xmin * vehicle->mtx.m[0][2];
						xmaxDir.f[0] = bbox->Bounds.xmax * vehicle->mtx.m[0][0];
						xmaxDir.f[2] = bbox->Bounds.xmax * vehicle->mtx.m[0][2];
						zminDir.f[0] = bbox->Bounds.zmin * vehicle->mtx.m[2][0];
						zminDir.f[2] = bbox->Bounds.zmin * vehicle->mtx.m[2][2];
						zmaxDir.f[0] = bbox->Bounds.zmax * vehicle->mtx.m[2][0];
						zmaxDir.f[2] = bbox->Bounds.zmax * vehicle->mtx.m[2][2];
						currentTile = prop->stan;
						if (((((stanWalkLine(&currentTile, prop->pos.f[0], prop->pos.f[2], (prop->pos.f[0] + xminDir.f[0]) + zminDir.f[0], (prop->pos.f[2] + xminDir.f[2]) + zminDir.f[2]) == 0) || (stanWalkLine(&currentTile, (prop->pos.f[0] + xminDir.f[0]) + zminDir.f[0], (prop->pos.f[2] + xminDir.f[2]) + zminDir.f[2], (prop->pos.f[0] + xmaxDir.f[0]) + zminDir.f[0], (prop->pos.f[2] + xmaxDir.f[2]) + zminDir.f[2]) == 0)) || (stanWalkLine(&currentTile, (prop->pos.f[0] + xmaxDir.f[0]) + zminDir.f[0], (prop->pos.f[2] + xmaxDir.f[2]) + zminDir.f[2], (prop->pos.f[0] + xmaxDir.f[0]) + zmaxDir.f[0], (prop->pos.f[2] + xmaxDir.f[2]) + zmaxDir.f[2]) == 0)) || (stanWalkLine(&currentTile, (prop->pos.f[0] + xmaxDir.f[0]) + zmaxDir.f[0], (prop->pos.f[2] + xmaxDir.f[2]) + zmaxDir.f[2], (prop->pos.f[0] + xminDir.f[0]) + zmaxDir.f[0], (prop->pos.f[2] + xminDir.f[2]) + zmaxDir.f[2]) == 0)) || (stanWalkLine(&currentTile, (prop->pos.f[0] + xminDir.f[0]) + zmaxDir.f[0], (prop->pos.f[2] + xminDir.f[2]) + zmaxDir.f[2], (prop->pos.f[0] + xminDir.f[0]) + zminDir.f[0], (prop->pos.f[2] + xminDir.f[2]) + zminDir.f[2]) == 0))
						{
							canMove = 0;
						}
					}

					if (canMove != 0)
					{
						vehicleAlignToFloor(vehicle);
						objGetShadeColour(prop, &vehicle->nextcol);
						proxminesDetonateNear(&vehicle->runtime_pos);
						if ((waypointPosition != NULL) && (chrIsArrivingLaterallyAtPos(&previousPos, &newpos, waypointPosition, 100.0f) != 0))
						{
							vehicle->nextstep++;
							if (vehicle->path->waypoints[vehicle->nextstep] < 0)
							{
								vehicle->path = NULL;
								vehicle->speedaim = 0.0f;
								vehicle->speedtime60 = 60.0f;
							}
						}
					}
					else
					{
						if (vehicle->speedtime60 < 0.0f)
						{
							vehicle->speedaim = (f32) vehicle->speed;
							vehicle->speedtime60 = 60.0f;
						}

						vehicle->speed = 0.0f;
						vehicle->roty = previousYaw;
						vehicle->turnrot60 = previousTurnSpeed;
						prop->stan = previousTile;
						obj->runtime_pos.f[0] = (prop->pos.f[0] = previousPos.f[0]);
						obj->runtime_pos.f[1] = (prop->pos.f[1] = previousPos.f[1]);
						obj->runtime_pos.f[2] = (prop->pos.f[2] = previousPos.f[2]);
						chrobjCollisionRelated(obj);
						setupUpdateObjectRoomPosition(obj);
					}
				}
				else
				{
					if (vehicle->speedtime60 < 0.0f)
					{
						vehicle->speedaim = (f32) vehicle->speed;
						vehicle->speedtime60 = 60.0f;
					}

					vehicle->speed = 0.0f;
					vehicle->roty = previousYaw;
					vehicle->turnrot60 = previousTurnSpeed;
				}
			}
			else if (vehicle->flags & PROPFLAG_INMOTION)
			{
				vehicle->roty = mathAtan2(vehicle->mtx.m[2][0], vehicle->mtx.m[2][2]);
				vehicle->flags &= ~PROPFLAG_INMOTION;
				vehicleAlignToFloor(vehicle);
			}
		}
		else if (obj->type == PROPDEF_AIRCRAFT)
		{
			aircraft = (struct AircraftRecord *) obj;

			aiForObject((PropDefHeaderRecord *) aircraft);

			aircraftModel = aircraft->model;

			if (aircraftModel->anim != NULL)
			{
				modelSetRootPosition(aircraftModel, &aircraft->runtime_pos);
#if defined(VERSION_EU)
				modelSetAnimPlaySpeed(aircraft->model, 1.2f, 0.0f);
#endif
				aircraftModel = aircraft->model;

				if (aircraftModel->anim == animation_table_ptrs2[1])
				{
					modelSetAnimTranslationScale(aircraftModel, 10.438f);
					modelSetRootRotY(aircraft->model, M_PI_F);
				}
				else if (bossGetStageNum() == 22)
				{
					modelSetAnimTranslationScale(aircraft->model, 1.0438f);
					modelSetRootRotY(aircraft->model, 2.3561945f);
				}
				else if (bossGetStageNum() == 26)
				{
					modelSetAnimTranslationScale(aircraft->model, 1.0438f);
					modelSetRootRotY(aircraft->model, 3.9269907f);
				}
				else
				{
					modelSetAnimTranslationScale(aircraft->model, 1.0438f);
					modelSetRootRotY(aircraft->model, 0.0f);
				}

				modelTickAnim(aircraft->model, g_ClockTimer, 1);
				modelCalcPosition(aircraft->model);
				modelGetRootPosition(aircraft->model, &aircraft->runtime_pos);
				prop->pos.f[0] = aircraft->runtime_pos.f[0];
				prop->pos.f[2] = aircraft->runtime_pos.f[2];
				if (aircraft->pad < 10000)
				{
					pad = &g_CurrentSetup.pads[aircraft->pad];
				}
				else
				{
					pad = (PadRecord *) (&g_CurrentSetup.boundpads[aircraft->pad - 10000]);
				}

				prop->pos.f[1] = pad->pos.f[1] + aircraft->runtime_pos.f[1];
				aircraft->runtime_pos.f[1] = prop->pos.f[1];
				modelSetRootPosition(aircraft->model, &aircraft->runtime_pos);
			}

			timeLeft = aircraft->speedtime60;
			if (timeLeft >= 0.0f)
			{
				if (timeLeft <= g_GlobalTimerDelta)
				{
					aircraft->speed = (f32) aircraft->speedaim;
				}
				else
				{
					aircraft->speed += ((aircraft->speedaim - aircraft->speed) * g_GlobalTimerDelta) / timeLeft;
				}

				aircraft->speedtime60 = (f32) (timeLeft - g_GlobalTimerDelta);
			}

			if (aircraft->rotaryspeedtime >= 0.0f)
			{
				if (aircraft->rotaryspeedtime <= g_GlobalTimerDelta)
				{
					aircraft->rotaryspeed = (f32) aircraft->rotaryspeedaim;
				}
				else
				{
					aircraft->rotaryspeed += ((aircraft->rotaryspeedaim - aircraft->rotaryspeed) * g_GlobalTimerDelta) / aircraft->rotaryspeedtime;
				}

				aircraft->rotaryspeedtime -= g_GlobalTimerDelta;
			}

			engineVolume = 0;
			if ((((!(aircraft->flags2 & PROPFLAG2_00080000)) && (objIsHealthy(obj) != 0)) && (aircraft->rotaryspeed != 0.0f)) && (!(aircraft->flags & PROPFLAG_INMOTION)))
			{
				engineVolume = sfxVolumeAt(&aircraft->runtime_pos);
			}

			if (engineVolume > 0)
			{
				if (((aircraft->Sound == NULL) || (sndGetPlayingState(aircraft->Sound) == 0)) && (lvGetControlsLocked() == 0))
				{
					sndPlaySfx(g_musicSfxBufferPtr, HELI_RUN_SFX, &aircraft->Sound);
				}

				if (aircraft->Sound != NULL)
				{
					sndCreatePostEvent(aircraft->Sound, 8, engineVolume);
				}
			}
			else if ((aircraft->Sound != NULL) && (sndGetPlayingState(aircraft->Sound) != 0))
			{
				sndDeactivate(aircraft->Sound);
			}
		}
	}

	if (obj->type == PROPDEF_TINTED_GLASS)
	{
		glass = (struct TintedGlassRecord *) prop->obj;
		glass->calculatedopacity = glassCalculateOpacity(&obj->runtime_pos, glass->TintDist, glass->CullDist, glass->minOpacity);
		if ((glass->portalnum >= 0) && (playerCount == 1))
		{
			if (glass->calculatedopacity == 0xFF)
			{
				bgToggleDataPortalsContrlBytes1Bit1(glass->portalnum, 0);
			}
			else
			{
				bgToggleDataPortalsContrlBytes1Bit1(glass->portalnum, 1);
			}
		}

		applyFogCull = FALSE;
	}
	else if ((obj->type == PROPDEF_DOOR) && (((struct DoorRecord *) obj)->doorFlags & DOORFLAG_WINDOWED))
	{
		door = (struct DoorRecord *) prop->obj;
		closePortal = 1;
		door->calculatedopacity = glassCalculateOpacity(&obj->runtime_pos, door->TintDist, door->CullDist, 0.0f);
		if (playerCount == 1)
		{
			if ((door->calculatedopacity != 0xFF) || (door->openPosition > 0.0f))
			{
				closePortal = 0;
			}

			if ((model->obj->Skeleton == (&skeleton_door)) && (modelGetNodeRwData(model, model->obj->Switches[1])->Switch.visible == 0))
			{
				closePortal = 0;
			}

			if (closePortal != 0)
			{
				doorClosePortal(door);
			}
			else
			{
				doorOpenPortal(door);
			}
		}
	}

	if ((obj->type == PROPDEF_TANK) && (playerGetTank() == prop))
	{
		visible = 1;
	}
	else if (obj->flags2 & PROPFLAG2_04000000)
	{
		visible = 1;
	}
	else
	{
		visible = ((!(obj->runtime_bitflags & RUNTIMEBITFLAG_RESPAWNING)) && (!(obj->flags2 & PROPFLAG2_00080000))) ? (posIsOnScreen(prop, &obj->runtime_pos, modelGetRadius(model), applyFogCull)) : (0);
	}

	if (visible != 0)
	{
		if (isSimOwner)
		{
			objUpdateShading(&obj->shadecol, &obj->nextcol);
		}

		prop->flags |= PROPFLAG_ONSCREEN;
		mtxs = dynAllocate(model->obj->numMatrices << 6);
		model->render_pos = (RenderPosView *) mtxs;

		if (obj->type == PROPDEF_DOOR)
		{
			doorGetModelMatrices(prop->door, model, mtxs, previousOpenPosition, isSimOwner);
		}
		else
		{
			mtx4Copy(&obj->mtx, &objmtx);
			mtx4SetTranslation(&obj->runtime_pos, &objmtx);
			mtx4MultAffine(camGetWorldToScreenMtxf(), &objmtx, mtxs);

			if (obj->type == PROPDEF_CCTV)
			{
				cctv = (struct CCTVRecord *) prop->obj;
				camYaw = cctv->yaw;
				nodepos = (coord3d *) model->obj->Switches[0]->Data;

				if (camYaw < 0.0f)
				{
					camYaw += M_TAU_F;
				}
				else if (camYaw >= M_TAU_F)
				{
					camYaw -= M_TAU_F;
				}

				mtx4LoadYRotation(camYaw, &yawmtx);
				mtx4Mult(&yawmtx, &cctv->headMtx, &mtxs[1]);
				headpos.f[0] = nodepos->f[0];
				headpos.f[1] = nodepos->f[1];
				headpos.f[2] = nodepos->f[2];
				mtx4TransformVecInPlace(&objmtx, &headpos);
				mtx4SetTranslation(&headpos, &mtxs[1]);
				mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), &mtxs[1]);
			}
			else if (obj->type == PROPDEF_AUTOGUN)
			{
				autogun = (struct AutogunRecord *) prop->obj;
				gunYaw = autogun->yaw + M_PI_2F;
				gunPitch = -autogun->pitch;

				if (gunYaw >= M_TAU_F)
				{
					gunYaw -= M_TAU_F;
				}

				nodepos = &model->obj->Switches[1]->Data->Group.Origin;
				gunpos.f[0] = nodepos->f[0];
				gunpos.f[1] = nodepos->f[1];
				gunpos.f[2] = nodepos->f[2];

				mtx4TransformVecInPlace(&objmtx, &gunpos);
				mtx4LoadYRotation(gunYaw, &mtxs[1]);
				mtx4SetTranslation(&gunpos, &mtxs[1]);
				mtx4ScaleRows012(autogun->model->scale, &mtxs[1]);
				mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), &mtxs[1]);
				nodepos = (coord3d *) model->obj->Switches[2]->Data;
				mtx4LoadZRotation(gunPitch, &mtxs[2]);
				mtx4SetTranslation(nodepos, &mtxs[2]);
				mtx4MultAffineInPlace(&mtxs[1], &mtxs[2]);

				if (model->obj->Switches[3] != NULL)
				{
					nodemtx = modelFindNodeMtx(model, model->obj->Switches[3], 0);
					nodepos = (coord3d *) model->obj->Switches[3]->Data;
					mtx4LoadXRotation(autogun->barrelSpinAngle, nodemtx);
					mtx4SetTranslation(nodepos, nodemtx);
					mtx4MultAffineInPlace(&mtxs[2], nodemtx);
				}

				if (model->obj->Switches[4] != NULL)
				{
					nodemtx = modelFindNodeMtx(model, model->obj->Switches[4], 0);
					nodepos = (coord3d *) model->obj->Switches[4]->Data;
					mtx4LoadTranslation(nodepos, nodemtx);
					mtx4MultAffineInPlace(&mtxs[2], nodemtx);
				}

				if (model->obj->Switches[6] != NULL)
				{
					nodemtx = modelFindNodeMtx(model, model->obj->Switches[6], 0);
					nodepos = (coord3d *) model->obj->Switches[6]->Data;
					mtx4LoadXRotation(autogun->barrelSpinAngle, nodemtx);
					mtx4SetTranslation(nodepos, nodemtx);
					mtx4MultAffineInPlace(&mtxs[2], nodemtx);
				}
			}
			else if (obj->type == PROPDEF_COLLECTABLE)
			{
				for (i = 1; i < model->obj->numMatrices; i++)
				{
					mtx4LoadIdentity(&mtxs[i]);
				}
			}
			else if (obj->type == PROPDEF_RACK)
			{
				mtx4LoadTranslation(&model->obj->Switches[0]->Data->Group.Origin, &mtxs[1]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[1]);
				mtx4LoadTranslation(&model->obj->Switches[1]->Data->Group.Origin, &mtxs[2]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[2]);
				mtx4LoadTranslation(&model->obj->Switches[2]->Data->Group.Origin, &mtxs[3]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[3]);
				mtx4LoadTranslation(&model->obj->Switches[3]->Data->Group.Origin, &mtxs[4]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[4]);
			}
			else if (obj->type == PROPDEF_VEHICHLE)
			{
				vehicle = (struct VehichleRecord *) obj;

				wheelSpin = 0.0f;
				switches = model->obj->Switches;
				wheel1Pos = &switches[1]->Data->Group.Origin;
				wheel2Pos = &switches[2]->Data->Group.Origin;
				wheel3Pos = &switches[3]->Data->Group.Origin;
				wheel4Pos = &switches[4]->Data->Group.Origin;
				wheelBbox = &switches[6]->Data->BoundingBox;
				wheelDiameter = (wheelBbox->Bounds.ymax - wheelBbox->Bounds.ymin) * model->scale;

				if (isSimOwner)
				{
					wheelSpin = ((vehicle->speed * g_GlobalTimerDelta) * M_TAU_F) / ((wheelDiameter * M_TAU_F) * 0.5f);
					vehicle->wheelxrot += wheelSpin;
					while (vehicle->wheelxrot >= M_TAU_F)
					{
						vehicle->wheelxrot -= M_TAU_F;
					}


					while (vehicle->wheelxrot < 0.0f)
					{
						vehicle->wheelxrot += M_TAU_F;
					}

				}

				vehicle->wheelxrot += wheelSpin;
				while (vehicle->wheelxrot >= M_TAU_F)
				{
					vehicle->wheelxrot -= M_TAU_F;
				}


				while (vehicle->wheelxrot < 0.0f)
				{
					vehicle->wheelxrot += M_TAU_F;
				}


				mtx4LoadXRotation(vehicle->wheelxrot, &wheelRollMtx);
				if (vehicle->speed > 0.0f)
				{
					absTurn = vehicle->turnrot60;
					wheelbase = (wheel3Pos->f[2] - wheel1Pos->f[2]) * model->scale;
					if (absTurn < 0.0f)
					{
						absTurn = -absTurn;
					}

					sideOffset = sinf(absTurn) * wheelbase;
					vehicle->wheelyrot = mathAtan2(sideOffset, (cosf(absTurn) * wheelbase) - (wheelbase - vehicle->speed));
					if (vehicle->wheelyrot < absTurn)
					{
						vehicle->wheelyrot = absTurn;
					}

					if (vehicle->turnrot60 > 0.0f)
					{
						vehicle->wheelyrot = M_TAU_F - vehicle->wheelyrot;
					}
				}

				mtx4LoadYRotation(vehicle->wheelyrot, &wheelSteerMtx);
				mtx4Copy(&wheelRollMtx, &mtxs[3]);
				mtx4SetTranslation(wheel3Pos, &mtxs[3]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[3]);
				mtx4Copy(&wheelRollMtx, &mtxs[4]);
				mtx4SetTranslation(wheel4Pos, &mtxs[4]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[4]);
				mtx4MultAffineInPlace(&wheelSteerMtx, &wheelRollMtx);
				mtx4Copy(&wheelRollMtx, &mtxs[1]);
				mtx4SetTranslation(wheel1Pos, &mtxs[1]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[1]);
				mtx4Copy(&wheelRollMtx, &mtxs[2]);
				mtx4SetTranslation(wheel2Pos, &mtxs[2]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[2]);
			}
			else if (obj->type == PROPDEF_AIRCRAFT)
			{
				aircraft = (struct AircraftRecord *) obj;

				rotorPos = &model->obj->Switches[2]->Data->Group.Origin;

				if ((g_ClockTimer > 0) && (isSimOwner))
				{
					aircraft->rotoryrot += aircraft->rotaryspeed;

					while (aircraft->rotoryrot >= M_TAU_F)
					{
						aircraft->rotoryrot -= M_TAU_F;
					}

					while (aircraft->rotoryrot < 0.0f)
					{
						aircraft->rotoryrot += M_TAU_F;
					}
				}

				if (aircraft->model->anim != NULL)
				{
					renderdata = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};
					renderdata.basemtx = camGetWorldToScreenMtxf();
					renderdata.mtxlist = &mtxs[0];
					modelCalcMatrices(&renderdata, aircraft->model);
				}
				else
				{
					mtx4Copy(&mtxs[0], &mtxs[1]);
				}

				if (aircraft->flags & PROPFLAG_INMOTION)
				{
					mtx4LoadZRotation(aircraft->rotoryrot, &rotorMtx);
				}
				else
				{
					mtx4LoadYRotation(aircraft->rotoryrot, &rotorMtx);
				}

				mtx4Copy(&rotorMtx, &mtxs[2]);
				mtx4SetTranslation(rotorPos, &mtxs[2]);
				mtx4MultAffineInPlace(&mtxs[1], &mtxs[2]);

				if (model->obj->Switches[3] != NULL)
				{
					nodepos = (coord3d *) model->obj->Switches[3]->Data;
					nodemtx = modelFindNodeMtx(model, model->obj->Switches[3], 0);
					mtx4LoadXRotation(aircraft->rotoryrot, &rotorMtx);
					mtx4Copy(&rotorMtx, nodemtx);
					mtx4SetTranslation(nodepos, nodemtx);
					mtx4MultAffineInPlace(&mtxs[1], nodemtx);
				}

				if (model->obj->Switches[4] != NULL)
				{
					nodepos = (coord3d *) model->obj->Switches[4]->Data;
					nodemtx = modelFindNodeMtx(model, model->obj->Switches[4], 0);
					mtx4LoadTranslation(nodepos, nodemtx);
					mtx4MultAffineInPlace(&mtxs[1], nodemtx);
				}
			}
			else if (obj->type == PROPDEF_TANK)
			{
				struct TankRecord *tank_render = (struct TankRecord *) obj;

				switches = model->obj->Switches;
				turretPos = &switches[1]->Data->Group.Origin;
				gunPos = &switches[3]->Data->Group.Origin;
				gunTipPos = &switches[4]->Data->Group.Origin;
				turretTopPos = &switches[2]->Data->Group.Origin;
				turretBbox = &switches[6]->Data->BoundingBox;
				turretPitch = -tank_render->turret_vertical_angle;

				if (turretPitch < 0.0f)
				{
					turretPitch += M_TAU_F;
				}

				turretYaw = -tank_render->turret_orientation_angle;

				if (turretYaw < 0.0f)
				{
					turretYaw += M_TAU_F;
				}

				mtx4LoadYRotation(turretYaw, &mtxs[1]);
				mtx4SetTranslation(turretPos, &mtxs[1]);
				mtx4MultAffineInPlace(&mtxs[0], &mtxs[1]);
				mtx4LoadXRotation(turretPitch, &mtxs[3]);
				mtx4SetTranslation(gunPos, &mtxs[3]);
				mtx4MultAffineInPlace(&mtxs[1], &mtxs[3]);
				mtx4LoadYRotation(M_PI_2F, &mtxs[4]);
				mtx4SetTranslation(gunTipPos, &mtxs[4]);
				mtx4MultAffineInPlace(&mtxs[3], &mtxs[4]);
				mtx4LoadTranslation(turretTopPos, &mtxs[2]);
				mtx4MultAffineInPlace(&mtxs[1], &mtxs[2]);
				mtx4MultAffine(camGetViewToWorldMtxf(), &mtxs[1], &turretWorldMtx);
				tank_render->hullEdges = bboxGetHull2d(turretBbox, &turretWorldMtx, tank_render->hull);

				if (model->obj->Switches[7] != NULL)
				{
					modelGetNodeRwData(model, model->obj->Switches[7])->Gunfire.visible = (s16) tank_render->is_firing_tank;
				}

				if (model->obj->Switches[8] != NULL)
				{
					modelGetNodeRwData(model, model->obj->Switches[8])->Switch.visible = 0;
				}
			}
		}

		modelUpdateRelationsQuick(model, model->obj->RootNode);

		prop->zDepth = -((Mtxf *) model->render_pos)[0].m[3][2];

		chrobjWeaponTick(prop);

		{
			struct PropRecord *current = prop->child;
			while (current != NULL)
			{
				prev = current->prev;
				objTickAttached(current);
				current = prev;
			}

		}
	}
	else
	{
		prop->flags &= ~PROPFLAG_ONSCREEN;
		chrobjWeaponTick(prop);
		{
			struct PropRecord *current = prop->child;
			while (current != NULL)
			{
				prev = current->prev;
				objTickOffscreen(current);
				current = prev;
			}

		}
	}

	if (obj->runtime_bitflags & RUNTIMEBITFLAG_LANDDAMAGE)
	{
		obj->runtime_bitflags &= ~RUNTIMEBITFLAG_LANDDAMAGE;
		objApplyDamage(obj, (U32_TO_F32(randomGetNext()) * 4.0f) + 2.0f, &prop->pos, 0, (s32) (((u32) (obj->runtime_bitflags & RUNTIMEBITFLAG_OWNER)) >> RUNTIMEBITSHIFT_OWNER));
	}

	if (isSimOwner)
	{
		if (obj->type == PROPDEF_DOOR)
		{
			doorUpdateSoundVolume((struct DoorRecord *) prop->obj);
		}
		else if ((obj->type == PROPDEF_AUTOGUN) && (lvGetControlsLocked() == 0))
		{
			autogun = (struct AutogunRecord *) prop->obj;

			leftFlash = 0;
			rightFlash = 0;

			if ((autogun->is_active != 0) && (!(obj->flags & PROPFLAG_IS_DRONE_GUN)))
			{
				autogun->numShots = autogun->numShots + 1;
				leftFlash = (autogun->numShots & 1) == 0;

				if (model->obj->Switches[5] != 0)
				{
					rightFlash = (autogun->numShots & 1) == 1;
				}

				if (autogun->nextSoundTime < g_GlobalTimer)
				{
					if ((autogun->fireSound1 != NULL) && (sndGetPlayingState(autogun->fireSound1) != 0))
					{
						sndDeactivate(autogun->fireSound1);
					}

					if ((autogun->fireSound2 != NULL) && (sndGetPlayingState(autogun->fireSound2) != 0))
					{
						sndDeactivate(autogun->fireSound2);
					}

					if (autogun->fireSound1 == NULL)
					{
						sndPlaySfx(g_musicSfxBufferPtr, GUN_B9_CANNON_SHORT_SFX, &autogun->fireSound1);
						sfxSetVolumeAt(autogun->fireSound1, &prop->pos);
					}
					else if (autogun->fireSound2 == NULL)
					{
						sndPlaySfx(g_musicSfxBufferPtr, GUN_B9_CANNON_SHORT_SFX, &autogun->fireSound2);
						sfxSetVolumeAt(autogun->fireSound2, &prop->pos);
					}

					autogun->nextSoundTime = (s32) (g_GlobalTimer + 2);
				}

				if ((leftFlash != 0) || (rightFlash != 0))
				{
					hitWall = 1;
					hitTile = NULL;
					shotTile = prop->stan;
					showBeam = (autogun->numShots & 3) == 0;
					// Co-op: the shots are for the gun's target, wherever this
					// pass's player is.
					coopTarget = coopAutogunTargetPlayer(prop);
					targetProp = (coopTarget >= 0) ? g_playerPointers[coopTarget]->prop : currentPlayerGetProp();
					flashNode = 5;

					if ((model->obj->Switches[7] != 0) && (!(autogun->numShots & 7)))
					{
						flashNode = 7;
					}

					if ((prop->flags & PROPFLAG_ONSCREEN) && (model->obj->Switches[flashNode] != NULL))
					{
						nodemtx = modelFindNodeMtx(model, model->obj->Switches[flashNode], 0);
						nodepos = &model->obj->Switches[flashNode]->Data->Group.Origin;
						shotFrom.f[0] = nodepos->f[0];
						shotFrom.f[1] = nodepos->f[1];
						shotFrom.f[2] = nodepos->f[2];
						mtx4MultAffine(camGetViewToWorldMtxf(), nodemtx, &nodeWorldMtx);
						mtx4TransformVecInPlace(&nodeWorldMtx, &shotFrom);
						if (stanWalkLine(&shotTile, prop->pos.f[0], prop->pos.f[2], shotFrom.f[0], shotFrom.f[2]) == 0)
						{
							shotFrom.f[0] = prop->pos.f[0];
							shotFrom.f[1] = prop->pos.f[1];
							shotFrom.f[2] = prop->pos.f[2];
						}
					}
					else
					{
						shotFrom.f[0] = prop->pos.f[0];
						shotFrom.f[1] = prop->pos.f[1];
						shotFrom.f[2] = prop->pos.f[2];
					}

					shotDir.f[0] = cosf(autogun->pitch) * sinf(autogun->yaw);
					shotDir.f[1] = sinf(autogun->pitch);
					shotDir.f[2] = cosf(autogun->pitch) * cosf(autogun->yaw);
					shotTo.f[0] = shotFrom.f[0] + (shotDir.f[0] * 65536.0f);
					shotTo.f[1] = shotFrom.f[1] + (shotDir.f[1] * 65536.0f);
					shotTo.f[2] = shotFrom.f[2] + (shotDir.f[2] * 65536.0f);

					stanResetHits();

					if (stanTestLineUnobstructed(&shotTile, shotFrom.f[0], shotFrom.f[2], shotTo.f[0], shotTo.f[2], 2, 100.0f, 100.0f, 0.0f, 1.0f) == 0)
					{
						chrStanLineDirIntersection(&shotFrom, &shotDir, &shotTo);
						hitTile = shotTile;
						shotTo.f[0] -= 26.0f * shotDir.f[0];
						shotTo.f[1] -= 26.0f * shotDir.f[1];
						shotTo.f[2] -= 26.0f * shotDir.f[2];
					}

					coopPrevPlayer = playerGetCurrentNum();

					if (coopTarget >= 0)
					{
						playerSetCurrent(coopTarget);
					}

					if (g_GlobalTimer == ((s32) autogun->onTargetTime))
					{
						xdiff = targetProp->pos.f[0] - shotFrom.f[0];
						ydiff = targetProp->pos.f[1] - shotFrom.f[1];
						zdiff = targetProp->pos.f[2] - shotFrom.f[2];
						targetDistSq = ((xdiff * xdiff) + (ydiff * ydiff)) + (zdiff * zdiff);
						xdiff = shotTo.f[0] - shotFrom.f[0];
						ydiff = shotTo.f[1] - shotFrom.f[1];
						zdiff = shotTo.f[2] - shotFrom.f[2];
						if ((targetDistSq <= (((xdiff * xdiff) + (ydiff * ydiff)) + (zdiff * zdiff))) && (playerIsShowingDamage() == 0))
						{
							targetDist = sqrtf(targetDistSq);
							damage = (0.16f * OBJECT_INTERACTION_TIMER_DELTA) * g_AutogunPendingDamageTick;
							if (targetDist > 200.0f)
							{
								damage *= 200.0f / targetDist;
							}

							autogun->damageAccum += damage;
							if (autogun->damageAccum >= 1.0f)
							{
								playerTakeDamageFromAngle((weaponGetDamage(14) * 0.125f) * g_AutogunDamageScalar, autogun->yaw, -1, 1);
								autogun->damageAccum = 0.0f;
								if (playerIsShowingDamage() != 0)
								{
									hitWall = 0;
								}
							}
						}
					}

					if (coopTarget >= 0)
					{
						playerSetCurrent(coopPrevPlayer);
					}

					if (hitWall != 0)
					{
						if (hitTile != NULL)
						{
							sparkCreate(&shotTo, 1, 26.0f, (s16) hitTile->room);
						}

						shotPlayHitSound(14, &shotTo, -1);
					}
					else
					{
						shotTo.f[0] = targetProp->pos.f[0];
						shotTo.f[1] = targetProp->pos.f[1];
						shotTo.f[2] = targetProp->pos.f[2];
						shotPlayHitPropSound(14, targetProp, -1);
					}

					if (showBeam != 0)
					{
						beam = autogun->beam;
						beam->from.f[0] = shotFrom.f[0];
						beam->from.f[1] = shotFrom.f[1];
						beam->from.f[2] = shotFrom.f[2];
						beam->dir.f[0] = shotTo.f[0] - beam->from.f[0];
						beam->dir.f[1] = shotTo.f[1] - beam->from.f[1];
						beam->dir.f[2] = shotTo.f[2] - beam->from.f[2];
						beamDist = sqrtf(((beam->dir.f[0] * beam->dir.f[0]) + (beam->dir.f[1] * beam->dir.f[1])) + (beam->dir.f[2] * beam->dir.f[2]));
						invDist = 1.0f / beamDist;
						beam->dir.f[0] = (f32) (beam->dir.f[0] * invDist);
						beam->dir.f[1] = (f32) (beam->dir.f[1] * invDist);
						beam->dir.f[2] = (f32) (beam->dir.f[2] * invDist);

						if (beamDist > 10000.0f)
						{
							beamDist = 10000.0f;
						}

						beam->age = 0;
						beam->weaponnum = ITEM_FNP90;
						beam->maxdist = beamDist;

						if (beamDist < 500.0f)
						{
							beamDist = 500.0f;
						}

						if (beam->weaponnum == ITEM_LASER)
						{
							beam->speed = 0.25f * beamDist;
							beam->length = 0.6f * beamDist;

							if (beam->length > 3000.0f)
							{
								beam->length = 3000.0f;
							}

							beam->dist = ((-0.1f) - (U32_TO_F32(randomGetNext()) * 0.3f)) * beamDist;
						}
						else
						{
							beam->speed = 0.2f * beamDist;
							beam->length = 0.2f * beamDist;

							if (beam->length > 3000.0f)
							{
								beam->length = 3000.0f;
							}

							beam->dist = ((2.0f * U32_TO_F32(randomGetNext())) - 1.0f) * beam->speed;
						}
					}
				}
			}

			if (model->obj->Switches[5] != NULL)
			{
				modelGetNodeRwData(model, model->obj->Switches[5])->Gunfire.visible = (s16) leftFlash;
			}

			if (model->obj->Switches[7] != NULL)
			{
				modelGetNodeRwData(model, model->obj->Switches[7])->Gunfire.visible = (s16) rightFlash;
			}
		}

		objDropRecursively(prop);
	}

	if (tickop == TICKOP_CHANGEDLIST)
	{
		prop->stan = NULL;
	}

	return tickop;
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
