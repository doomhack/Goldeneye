/**
 * Creating the level's objects from their setup file records, one function per kind of
 * record (setupObject, setupDoor, setupWeapon, setupCctv...). setupCreateProps in setup.c
 * calls these as it walks the setup file.
 *
 * Most objects stand at a pad. A plain pad gives a position and a facing. A bound pad
 * also gives a box: the object stands at the box's centre, and the FITPAD flags can scale
 * it to fill the box. Doors always use a bound pad and are scaled to fill it.
 *
 * The setup file stores fractions and angles as 16.16 fixed point in fields that are
 * floats at runtime, so each one is converted in place the first time the record is set
 * up (setupFixedToFloat).
 */
#include <ultra64.h>
#include <memp.h>
#include "game/menu/front.h"
#include "game/menu/cheat.h"
#include "game/menu/mp_weapon.h"
#include "game/model/model.h"
#include "game/model/modelfile.h"
#include "game/math/matrix.h"
#include "game/math/trig.h"
#include "game/nav/stan.h"
#include "game/world/bg.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/player/player.h"
#include "game/player/bondview_r.h"
#include "game/prop/propobj.h"
#include "game/prop/door.h"
#include "game/prop/monitor.h"
#include "game/prop/projectile.h"
#include "game/prop/carried.h"
#include "game/prop/setup.h"
#include "game/prop/objsetup.h"

/* Weapon records with these numbers take their weapon from slot (number - 0xF0) of the
   multiplayer weapon set */
#define MP_WEAPON_SET_SLOT 0xF0

#if defined(VERSION_EU)
#define AUTOGUN_SPEED_UNIT 7.5398226f // tuned for 50 Hz: 2 pi * 1.2
#define DOOR_SPEED_UNIT    1.2f
#define TANK_Y_SMOOTHING   0.2004f
#else
#define AUTOGUN_SPEED_UNIT M_TAU_F
#define DOOR_SPEED_UNIT    1.0f
#define TANK_Y_SMOOTHING   0.17000002f
#endif

/* A beam is 0x2c bytes, rounded up to 16 */
#define BEAM_ALLOC_SIZE 0x30

/* Raising the monitors on a hanging rack (21 degrees) */
#define HANGING_MONITOR_TILT 0.36651915f

f32 g_DoorScale = 1.0f;

/* The multiplayer weapon set slot of the last weapon record set up. The ammo crate that
   follows it in the setup file holds ammo for that weapon. */
static s32 s_LastMpWeaponSlot;

/**
 * Converts a 16.16 fixed point setup value, stored in the field it will be a float in,
 * to a float in the given unit.
 */
static f32 setupFixedToFloat(f32 *field, f32 unit)
{
    return (*(s32 *)field * unit) / 65536.0f;
}

/**
 * Gets where something would stand at pos. Fails if there's no floor there, or (with a
 * radius) if something is in the way.
 */
bool setupCheckPlacement(coord3d *pos, StandTile *stan, f32 radius, coord3d *posout, StandTile **stanout)
{
    *posout = *pos;
    *stanout = stan;

    if (stan == NULL)
    {
        return FALSE;
    }

    if (radius > 0.0f
        && stanTestVolume(stanout, posout->f[0], posout->f[2], radius, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f) >= 0)
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * The pad's side axis: up x look, normalised.
 */
static void padGetSide(BoundPadRecord *pad, coord3d *side)
{
    f32 scale;

    side->f[0] = (pad->up.f[1] * pad->look.f[2]) - (pad->up.f[2] * pad->look.f[1]);
    side->f[1] = (pad->up.f[2] * pad->look.f[0]) - (pad->up.f[0] * pad->look.f[2]);
    side->f[2] = (pad->up.f[0] * pad->look.f[1]) - (pad->up.f[1] * pad->look.f[0]);

    scale = 1.0f / sqrtf(((side->f[0] * side->f[0]) + (side->f[1] * side->f[1])) + (side->f[2] * side->f[2]));

    side->f[0] *= scale;
    side->f[1] *= scale;
    side->f[2] *= scale;
}

/**
 * The centre of a bound pad's box in world space. The box is in pad space: x along the
 * pad's side axis, y along up and z along look.
 */
void padGetBboxCentre(BoundPadRecord *pad, coord3d *centre)
{
    coord3d side;
    s32 i;

    padGetSide(pad, &side);

    for (i = 0; i < 3; i++)
    {
        centre->f[i] = pad->pos.f[i] + (
            (pad->bbox.xmin + pad->bbox.xmax) * side.f[i] +
            (pad->bbox.ymin + pad->bbox.ymax) * pad->up.f[i] +
            (pad->bbox.zmin + pad->bbox.zmax) * pad->look.f[i]) * 0.5f;
    }
}

/**
 * Finds the rooms on each side of a bound pad (a door's), 50 units either side of its
 * centre along its side axis. room2 is -1 if both sides are the same room.
 */
static void padGetRoomsEitherSide(BoundPadRecord *pad, s32 *room1, s32 *room2, coord3d *pos1, coord3d *pos2)
{
    coord3d centre;
    coord3d side;
    StandTile *centretile = pad->stan;
    StandTile *tile;

    padGetBboxCentre(pad, &centre);

    if (!stanWalkLine(&centretile, pad->pos.f[0], pad->pos.f[2], centre.f[0], centre.f[2]))
    {
        centretile = pad->stan;
        centre = pad->pos;
    }

    padGetSide(pad, &side);

    pos1->f[0] = centre.f[0] + (side.f[0] * 50.0f);
    pos1->f[1] = centre.f[1];
    pos1->f[2] = centre.f[2] + (side.f[2] * 50.0f);

    tile = centretile;
    stanWalkLine(&tile, centre.f[0], centre.f[2], pos1->f[0], pos1->f[2]);
    *room1 = tile->room;

    pos2->f[0] = centre.f[0] - (side.f[0] * 50.0f);
    pos2->f[1] = centre.f[1];
    pos2->f[2] = centre.f[2] - (side.f[2] * 50.0f);

    tile = centretile;
    stanWalkLine(&tile, centre.f[0], centre.f[2], pos2->f[0], pos2->f[2]);
    *room2 = tile->room;

    if (*room2 == *room1)
    {
        *room2 = -1;
    }
}

/**
 * Puts an object into the rooms it overlaps: its bounding box turned by its matrix, 30
 * units wider each way. Objects flagged to stay in one room only go in their floor tile's.
 */
void setupUpdateObjectRoomPosition(ObjectRecord *obj)
{
    PropRecord *prop = obj->prop;
    ModelRoData_BoundingBoxRecord *bbox;
    coord3d min;
    coord3d max;
    f32 radius = 0.0f;

    propRemoveFromRooms(prop);

    if (obj->flags2 & 0x20000)
    {
        if (prop->stan != NULL)
        {
            prop->rooms[0] = prop->stan->room;
            prop->rooms[1] = 0xff;
        }
        else
        {
            prop->rooms[0] = 0xff;
        }
    }
    else
    {
        bbox = objFindBbox(obj);

        if (bbox != NULL)
        {
            min.f[0] = bboxRotatedMinX(bbox, &obj->mtx) - 30.0f;
            min.f[1] = bboxRotatedMinY(bbox, &obj->mtx);
            min.f[2] = bboxRotatedMinZ(bbox, &obj->mtx) - 30.0f;

            max.f[0] = bboxRotatedMaxX(bbox, &obj->mtx) + 30.0f;
            max.f[1] = bboxRotatedMaxY(bbox, &obj->mtx);
            max.f[2] = bboxRotatedMaxZ(bbox, &obj->mtx) + 30.0f;

            // the furthest the box reaches across the floor from the object's origin
            if (radius < -min.f[0])
            {
                radius = -min.f[0];
            }

            if (radius < -min.f[2])
            {
                radius = -min.f[2];
            }

            if (radius < max.f[0])
            {
                radius = max.f[0];
            }

            if (radius < max.f[2])
            {
                radius = max.f[2];
            }

            min.f[0] += obj->runtime_pos.f[0];
            min.f[1] += obj->runtime_pos.f[1];
            min.f[2] += obj->runtime_pos.f[2];

            max.f[0] += obj->runtime_pos.f[0];
            max.f[1] += obj->runtime_pos.f[1];
            max.f[2] += obj->runtime_pos.f[2];

            propCalcRooms(prop, &min, &max, radius);
        }
    }

    propAddToRooms(prop);
}

/**
 * Scales an object to fit its bound pad's box, on the axes its FITPAD flags pick
 * (FITPADBOUNDS: all three, evenly, keeping its shape). The axes are the model's: a
 * sideways object's y and z lie along the pad's z and y. Rows of mtx take the scale on
 * each axis relative to the largest, and the model takes the largest.
 */
static void objFitToBoundPad(ObjectRecord *obj, BoundPadRecord *pad, ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    bool sideways = (obj->flags & PROPFLAG_SIDEWAYS) != 0;
    f32 padsize[3];
    f32 modelmin[3] = { bbox->Bounds.xmin, bbox->Bounds.ymin, bbox->Bounds.zmin };
    f32 modelmax[3] = { bbox->Bounds.xmax, bbox->Bounds.ymax, bbox->Bounds.zmax };
    f32 scale[3] = { 1.0f, 1.0f, 1.0f };
    f32 minscale;
    f32 maxscale;
    f32 swap;
    s32 i;

    padsize[0] = pad->bbox.xmax - pad->bbox.xmin;
    padsize[1] = sideways ? pad->bbox.zmax - pad->bbox.zmin : pad->bbox.ymax - pad->bbox.ymin;
    padsize[2] = sideways ? pad->bbox.ymax - pad->bbox.ymin : pad->bbox.zmax - pad->bbox.zmin;

    for (i = 0; i < 3; i++)
    {
        if ((obj->flags & (PROPFLAG_FITPADBOUNDS | (PROPFLAG_FITPADX << i))) && modelmin[i] < modelmax[i])
        {
            scale[i] = padsize[i] / ((modelmax[i] - modelmin[i]) * obj->model->scale);
        }
    }

    minscale = scale[0];

    if (scale[1] < minscale)
    {
        minscale = scale[1];
    }

    if (scale[2] < minscale)
    {
        minscale = scale[2];
    }

    maxscale = scale[0];

    if (maxscale < scale[1])
    {
        maxscale = scale[1];
    }

    if (maxscale < scale[2])
    {
        maxscale = scale[2];
    }

    for (i = 0; i < 3; i++)
    {
        if (obj->flags & PROPFLAG_FITPADBOUNDS)
        {
            scale[i] = minscale;
        }
        else if (!(obj->flags & (PROPFLAG_FITPADX << i)) && modelmax[i] == modelmin[i])
        {
            // a flat model takes the largest scale across its flat axis
            scale[i] = maxscale;
        }
    }

    for (i = 0; i < 3; i++)
    {
        scale[i] /= maxscale;
    }

    if (scale[0] <= 0.000001f || scale[1] <= 0.000001f || scale[2] <= 0.000001f)
    {
        scale[0] = scale[1] = scale[2] = 1.0f;
    }

    if (sideways)
    {
        swap = scale[1];
        scale[1] = scale[2];
        scale[2] = swap;
    }

    mtx4ScaleRow0(scale[0], mtx);
    mtx4ScaleRow1(scale[1], mtx);
    mtx4ScaleRow2(scale[2], mtx);

    modelSetScale(obj->model, obj->model->scale * maxscale);
}

/**
 * Creates an object at its pad (or bound pad), on the floor below it unless its flags say
 * otherwise.
 */
static void objPlaceFromSetup(ObjectRecord *obj, f32 extrascale)
{
    BoundPadRecord *boundpad = NULL;
    ModelRoData_BoundingBoxRecord *bbox;
    PropRecord *prop;
    PadRecord *pad;
    StandTile *tile;
    StandTile *floortile;
    coord3d pos;
    coord3d centre;
    coord3d floorpos;
    f32 halfheight;
    Mtxf mtx;
    s32 i;

    if (isNotBoundPad(obj->pad))
    {
        pad = &g_CurrentSetup.pads[obj->pad];
        mtx4LoadFacing(&mtx, 0.0f, 0.0f, 0.0f, -pad->look.f[0], -pad->look.f[1], -pad->look.f[2], pad->up.f[0], pad->up.f[1], pad->up.f[2]);

        pos = pad->pos;
        centre = pad->pos;
        tile = pad->stan;
    }
    else
    {
        // a bound pad: the object's centre is the box's, dropped to the box's floor
        boundpad = &g_CurrentSetup.boundpads[getBoundPadNum(obj->pad)];
        mtx4LoadFacing(&mtx, 0.0f, 0.0f, 0.0f, -boundpad->look.f[0], -boundpad->look.f[1], -boundpad->look.f[2], boundpad->up.f[0], boundpad->up.f[1], boundpad->up.f[2]);

        padGetBboxCentre(boundpad, &centre);
        pos = centre;
        halfheight = (boundpad->bbox.ymin - boundpad->bbox.ymax) * 0.5f;

        for (i = 0; i < 3; i++)
        {
            centre.f[i] += boundpad->up.f[i] * halfheight;
        }

        tile = boundpad->stan;

        // stands at the box's centre, unless flagged to stand at the pad or the centre
        // can't be reached across the floor from the pad
        if ((obj->flags2 & PROPFLAG2_ATPADPOS)
            || !stanWalkLine(&tile, boundpad->pos.f[0], boundpad->pos.f[2], pos.f[0], pos.f[2]))
        {
            pos = boundpad->pos;
            tile = boundpad->stan;
        }
    }

    if (!setupCheckPlacement(&pos, tile, 0.0f, &floorpos, &floortile))
    {
        return;
    }

    if (obj->type == PROPDEF_COLLECTABLE)
    {
        prop = weaponInitWithModelDef(obj, PitemZ_entries[obj->obj].header);
    }
    else
    {
        prop = objInitWithAutoModel(obj);
    }

    if (boundpad != NULL)
    {
        bbox = objFindBbox(obj);

        if (bbox != NULL)
        {
            objFitToBoundPad(obj, boundpad, bbox, &mtx);
        }
    }

    modelSetScale(obj->model, obj->model->scale * extrascale);
    mtx4ScaleRows012(obj->model->scale, &mtx);

    if (obj->flags & PROPFLAG_SIDEWAYS)
    {
        objPlaceAtPadSideways(obj, &floorpos, &mtx, floortile, &centre);
    }
    else
    {
        objPlaceAtPad(obj, &floorpos, &mtx, floortile, &centre);
    }

    setupUpdateObjectRoomPosition(obj);
    propActivate(prop);
    propEnable(prop);
}

/**
 * Whether an object respawns after being picked up or destroyed in multiplayer: all do,
 * except the scenario's flag or golden gun.
 */
static bool objRespawnsInMp(ObjectRecord *obj)
{
    if (frontGetMpScenario() == SCENARIO_TLD && obj->obj == PROP_FLAG)
    {
        return FALSE;
    }

    if (frontGetMpScenario() == SCENARIO_MWTGG && obj->obj == PROP_CHRGOLDEN)
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * Creates an object from its setup record. It goes inside another object (attached by
 * setupCreateProps once everything exists), into a guard's hands, or at its pad.
 */
void setupObject(ObjectRecord *obj)
{
    s32 modelnum = obj->obj;
    ModelFileHeader *header = PitemZ_entries[modelnum].header;
    PropRecord *prop;
    ChrRecord *chr;
    f32 extrascale;

    modelLoad(modelnum);

    extrascale = obj->extrascale * (1.0f / 256.0f);
    obj->health = setupFixedToFloat(&obj->health, 1.0f);

    if (isMpGame() && objRespawnsInMp(obj))
    {
        obj->state |= PROPSTATE_RESPAWN;
    }

    if (obj->flags & PROPFLAG_INSIDEANOTHEROBJ)
    {
        if (obj->type == PROPDEF_COLLECTABLE)
        {
            weaponInitWithModelDef(obj, header);
        }
        else
        {
            objInitWithModelDef(obj, header);
        }

        modelSetScale(obj->model, obj->model->scale * extrascale);
    }
    else if (obj->flags & PROPFLAG_ASSIGNEDTOCHR)
    {
        chr = chrFindByLiteralId(obj->pad);

        if (chr != NULL && chr->prop != NULL && chr->model != NULL)
        {
            if (obj->type == PROPDEF_COLLECTABLE)
            {
                prop = weaponInitWithModelDef(obj, header);
            }
            else
            {
                prop = objInitWithModelDef(obj, header);
            }

            modelSetScale(obj->model, obj->model->scale * extrascale);
            propReparent(prop, chr->prop);
        }
    }
    else
    {
        objPlaceFromSetup(obj, extrascale);
    }
}

/**
 * Creates a door at its bound pad, scaled to fill the pad's box: the model's x across the
 * pad's height, its y along the pad's look and its z across the pad's width. A door flagged
 * to hide what's behind it closes the portal between the rooms either side.
 */
void setupDoor(DoorRecord *door, s32 cmdindex)
{
    s32 modelnum = door->obj;
    BoundPadRecord *pad = &g_CurrentSetup.boundpads[door->pad];
    ModelRoData_BoundingBoxRecord *modelbbox;
    ModelFileHeader *modelfile;
    PropRecord *prop;
    StandTile *floortile;
    StandTile *postile;
    StandTile *tile;
    struct PortalMetric plane;
    s32 portalnum = -1;
    s32 room1 = -1;
    s32 room2 = -1;
    coord3d side1;
    coord3d side2;
    coord3d floorpos;
    coord3d newpos;
    coord3d pos;
    coord3d centre;
    coord3d extent;
    Mtxf padmtx;
    Mtxf mtx;
    Mtxf rotz;
    f32 dist;
    f32 xscale;
    f32 yscale;
    f32 zscale;
    f32 scale;
    s32 i;

    modelLoad(modelnum);

    if ((door->flags & PROPFLAG_CULL_BEHIND_DOOR) || (door->flags & PROPFLAG_NO_PORTAL_CLOSE))
    {
        padGetRoomsEitherSide(pad, &room1, &room2, &side1, &side2);

        if ((door->flags & PROPFLAG_CULL_BEHIND_DOOR) && room1 >= 0 && room2 >= 0)
        {
            portalnum = bgGetPortalBetweenRooms(room1, room2, &side1, &side2);
        }
    }

    if (g_DoorScale != 1.0f)
    {
        if (portalnum >= 0)
        {
            // move the pad towards (or away from) the portal so the scaled door still fills it
            bgGetPortalPlane(portalnum, &plane);
            plane.min *= bgGetLevelScaleInv();

            dist = (pad->pos.f[0] * plane.normal.f[0]) + (pad->pos.f[1] * plane.normal.f[1]) + (pad->pos.f[2] * plane.normal.f[2]);

            if (g_DoorScale < 1.0f)
            {
                dist = (dist - plane.min) * (1.0f - g_DoorScale);

                for (i = 0; i < 3; i++)
                {
                    newpos.f[i] = pad->pos.f[i] - (plane.normal.f[i] * dist);
                }
            }
            else
            {
                dist = (dist - plane.min) * (g_DoorScale - 1.0f);

                for (i = 0; i < 3; i++)
                {
                    newpos.f[i] = pad->pos.f[i] + (plane.normal.f[i] * dist);
                }
            }

            tile = pad->stan;

            if (stanWalkLine(&tile, pad->pos.f[0], pad->pos.f[2], newpos.f[0], newpos.f[2]))
            {
                pad->stan = tile;
                pad->pos = newpos;
                pad->bbox.xmin *= g_DoorScale;
                pad->bbox.xmax *= g_DoorScale;
            }
        }
        else
        {
            pad->bbox.xmin *= g_DoorScale;
            pad->bbox.xmax *= g_DoorScale;
        }
    }

    if (!setupCheckPlacement(&pad->pos, pad->stan, 0.0f, &floorpos, &floortile))
    {
        door->prop = NULL;
        return;
    }

    mtx4LoadFacing(&padmtx, 0.0f, 0.0f, 0.0f, -pad->look.f[0], -pad->look.f[1], -pad->look.f[2], pad->up.f[0], pad->up.f[1], pad->up.f[2]);
    modelfile = PitemZ_entries[modelnum].header;
    postile = floortile;

    mtx4LoadXRotation(M_HALF_PI, &mtx);
    mtx4LoadZRotation(M_HALF_PI, &rotz);
    mtx4MultInPlace(&rotz, &mtx);
    mtx4MultInPlace(&padmtx, &mtx);
    padGetBboxCentre(pad, &pos);

    modelbbox = (ModelRoData_BoundingBoxRecord *)modelfile->RootNode->Child->Data;

    xscale = (pad->bbox.ymax - pad->bbox.ymin) / (modelbbox->Bounds.xmax - modelbbox->Bounds.xmin);
    yscale = (pad->bbox.zmax - pad->bbox.zmin) / (modelbbox->Bounds.ymax - modelbbox->Bounds.ymin);
    zscale = (pad->bbox.xmax - pad->bbox.xmin) / (modelbbox->Bounds.zmax - modelbbox->Bounds.zmin);

    if (xscale <= 0.000001f || yscale <= 0.000001f || zscale <= 0.000001f)
    {
        xscale = yscale = zscale = 1.0f;
    }

    mtx4ScaleRow0(xscale, &mtx);
    mtx4ScaleRow1(yscale, &mtx);
    mtx4ScaleRow2(zscale, &mtx);

    centre = pos;

    if (door->flags2 & PROPFLAG2_ATPADPOS)
    {
        pos = floorpos;
    }
    else if (stanWalkLine(&postile, floorpos.f[0], floorpos.f[2], pos.f[0], pos.f[2]))
    {
        floortile = postile;
    }
    else
    {
        // the box's centre can't be reached across the floor: stand under it at the pad
        pos.f[0] = floorpos.f[0];
        pos.f[2] = floorpos.f[2];
    }

    // how far the door moves to open: up for vertical and fall-away doors, else sideways
    if (door->doorType == DOORTYPE_VERTICAL || door->doorType == DOORTYPE_FALLAWAY)
    {
        for (i = 0; i < 3; i++)
        {
            extent.f[i] = pad->look.f[i] * (pad->bbox.zmax - pad->bbox.zmin);
        }
    }
    else
    {
        for (i = 0; i < 3; i++)
        {
            extent.f[i] = pad->up.f[i] * (pad->bbox.ymin - pad->bbox.ymax);
        }
    }

    door->maxFrac = setupFixedToFloat(&door->maxFrac, 1.0f);
    door->perimFrac = setupFixedToFloat(&door->perimFrac, 1.0f);
    door->accel = setupFixedToFloat(&door->accel, DOOR_SPEED_UNIT);
    door->decel = setupFixedToFloat(&door->decel, DOOR_SPEED_UNIT);
    door->maxSpeed = setupFixedToFloat(&door->maxSpeed, DOOR_SPEED_UNIT);

    prop = doorInit(door, &pos, &mtx, floortile, &extent, &centre);

    if (door->flags & PROPFLAG_CULL_BEHIND_DOOR)
    {
        door->portalNumber = portalnum;

        if (portalnum >= 0 && door->openPosition == 0.0f)
        {
            doorClosePortal(door);
        }
    }

    // the door's in its floor tile's room, and the room on its other side
    prop->rooms[0] = prop->stan->room;
    propAddToRoom(prop, prop->stan->room);
    prop->rooms[1] = 0xff;
    prop->rooms[2] = 0xff;

    if ((door->flags & PROPFLAG_CULL_BEHIND_DOOR) || (door->flags & PROPFLAG_NO_PORTAL_CLOSE))
    {
        if (room1 != prop->stan->room)
        {
            if (room1 >= 0)
            {
                prop->rooms[1] = room1;
                propAddToRoom(prop, room1);
            }
        }
        else if (room2 >= 0)
        {
            prop->rooms[1] = room2;
            propAddToRoom(prop, room2);
        }
    }

    if (door->model != NULL)
    {
        scale = xscale;

        if (scale < yscale)
        {
            scale = yscale;
        }

        if (scale < zscale)
        {
            scale = zscale;
        }

        modelSetScale(door->model, door->model->scale * scale);
    }

    propActivate(prop);
    propEnable(prop);

    if (door->linkedDoorOffset != 0)
    {
        door->linkedDoor = (DoorRecord *)setupGetPtrToCommandByIndex(door->linkedDoorOffset + cmdindex);
    }
}

/**
 * Whether the Enemy Rockets cheat turns a guard's weapon into a rocket launcher: any gun
 * (or knife, mine trigger and so on), but not grenades, mines or launchers.
 */
static bool weaponBecomesRockets(s32 weaponnum)
{
    switch (weaponnum)
    {
        case ITEM_KNIFE:
        case ITEM_THROWKNIFE:
        case ITEM_WPPK:
        case ITEM_WPPKSIL:
        case ITEM_TT33:
        case ITEM_SKORPION:
        case ITEM_AK47:
        case ITEM_UZI:
        case ITEM_MP5K:
        case ITEM_MP5KSIL:
        case ITEM_SPECTRE:
        case ITEM_M16:
        case ITEM_FNP90:
        case ITEM_SHOTGUN:
        case ITEM_AUTOSHOT:
        case ITEM_SNIPERRIFLE:
        case ITEM_RUGER:
        case ITEM_GOLDENGUN:
        case ITEM_SILVERWPPK:
        case ITEM_GOLDWPPK:
        case ITEM_LASER:
        case ITEM_WATCHLASER:
        case ITEM_REMOTEMINE:
        case ITEM_TRIGGER:
        case ITEM_TASER:
            return TRUE;
    }

    return FALSE;
}

/**
 * Creates a weapon: in a guard's hands, or lying at its pad. In multiplayer, weapon set
 * slot records take their weapon from the chosen set, and the flag only appears in
 * The Living Daylights.
 */
void setupWeapon(WeaponObjRecord *weapon)
{
    struct s_mp_weapon_set *weaponset;
    ChrRecord *chr;
    bool load = TRUE;
    s32 slot;

    if (weapon->flags & PROPFLAG_ASSIGNEDTOCHR)
    {
        chr = chrFindByLiteralId(weapon->pad);

        if (chr != NULL && chr->prop != NULL && chr->model != NULL)
        {
            if (cheatIsActive(CHEAT_ENEMY_ROCKETS) && weaponBecomesRockets(weapon->weaponnum))
            {
                weapon->weaponnum = ITEM_ROCKETLAUNCH;
                weapon->obj = PROP_CHRROCKETLAUNCH;
                weapon->extrascale = 256;
            }

            weaponLoadProjectileModels(weapon->weaponnum);
            chrSetupHeldWeapon(weapon, chr);
        }

        return;
    }

    if (isMpGame())
    {
        s_LastMpWeaponSlot = -1;

        switch ((u8)weapon->weaponnum)
        {
            case MP_WEAPON_SET_SLOT + ITEM_UNARMED ... MP_WEAPON_SET_SLOT + ITEM_SKORPION:
                weaponset = mpWeaponSetGetItems();
                slot = (u8)weapon->weaponnum - MP_WEAPON_SET_SLOT;
                s_LastMpWeaponSlot = slot;

                weapon->weaponnum = weaponset[slot].itemID;
                weapon->obj = weaponset[slot].propID;
#if defined(VERSION_EU)
                weapon->extrascale = weaponset[slot].size16;
#else
                weapon->extrascale = weaponset[slot].size * 256.0f;
#endif
                load = weaponset[slot].allowpickup;
                break;

            case ITEM_TOKEN:
                load = frontGetMpScenario() == SCENARIO_TLD;
                break;
        }
    }

    if (weapon->weaponnum != ITEM_UNARMED && load)
    {
        weaponLoadProjectileModels(weapon->weaponnum);
        setupObject((ObjectRecord *)weapon);
    }
}

/**
 * Creates a hat: on a guard's head, or at its pad.
 */
void setupHat(HatRecord *hat)
{
    ChrRecord *chr;

    if (hat->flags & PROPFLAG_ASSIGNEDTOCHR)
    {
        chr = chrFindByLiteralId(hat->pad);

        if (chr != NULL && chr->prop != NULL && chr->model != NULL)
        {
            hatAssignToChr(hat, chr);
        }
    }
    else
    {
        setupObject((ObjectRecord *)hat);
    }
}

/**
 * Creates a security camera, with its head turned towards its look pad. Its sweep angles
 * and speed are converted once, as the record can be set up again on a restart.
 */
void setupCctv(CCTVRecord *cctv)
{
    coord3d *headoffset;
    PadRecord *lookpad;
    coord3d headpos;

    setupObject((ObjectRecord *)cctv);

    if (cctv->lookpad < 0)
    {
        return;
    }

    headoffset = (coord3d *)cctv->model->obj->Switches[0]->Data;
    lookpad = setupGetPad(cctv->lookpad);

    headpos = *headoffset;
    mtx4RotateVecInPlace(&cctv->mtx, &headpos);

    headpos.f[0] += cctv->prop->pos.f[0];
    headpos.f[1] += cctv->prop->pos.f[1];
    headpos.f[2] += cctv->prop->pos.f[2];

    mtx4LoadFacing(&cctv->headMtx, 0.0f, 0.0f, 0.0f, headpos.f[0] - lookpad->pos.f[0], headpos.f[1] - lookpad->pos.f[1], headpos.f[2] - lookpad->pos.f[2], 0.0f, 1.0f, 0.0f);
    mtx4ScaleRows012(cctv->model->scale, &cctv->headMtx);

    if (cctv->convert_to_f32 == 0)
    {
        cctv->convert_to_f32 = 1;
        cctv->sweepYaw1 = setupFixedToFloat(&cctv->sweepYaw1, M_TAU_F);
        cctv->sweepYaw2 = setupFixedToFloat(&cctv->sweepYaw2, M_TAU_F);
        cctv->maxYawSpeed = setupFixedToFloat(&cctv->maxYawSpeed, M_TAU_F);
        cctv->viewDist = *(s32 *)&cctv->viewDist;
    }

    cctv->towardsYaw1 = 0;
    cctv->yawSpeed = 0.0f;
    cctv->yaw = cctv->sweepYaw1;
    cctv->baseYaw = mathAtan2(headpos.f[0] - lookpad->pos.f[0], headpos.f[2] - lookpad->pos.f[2]);
    cctv->timer = 0;
}

/**
 * Creates a drone gun, resting pointed at its pad (if it has one), with a beam for its
 * tracers.
 */
void setupAutogun(AutogunRecord *autogun)
{
    BeamRecord *beam;
    PadRecord *pad;
    PropRecord *prop;
    f32 xdiff;
    f32 ydiff;
    f32 zdiff;

    setupObject((ObjectRecord *)autogun);

    autogun->speed = setupFixedToFloat(&autogun->speed, AUTOGUN_SPEED_UNIT);
    autogun->aimdist = setupFixedToFloat(&autogun->aimdist, 100.0f);
    autogun->maxYaw = setupFixedToFloat(&autogun->maxYaw, M_TAU_F);
    autogun->minYaw = setupFixedToFloat(&autogun->minYaw, M_TAU_F);

    autogun->numShots = 0;
    autogun->lastSeenTime = -1;
    autogun->onTargetTime = -1;
    autogun->nextSoundTime = -1;
    autogun->fireSound1 = 0;
    autogun->fireSound2 = 0;
    autogun->yaw = 0.0f;
    autogun->yawSpeed = 0.0f;
    autogun->homeYaw = 0.0f;
    autogun->pitch = 0.0f;
    autogun->pitchSpeed = 0.0f;
    autogun->homePitch = 0.0f;
    autogun->barrelSpinSpeed = 0.0f;
    autogun->barrelSpinAngle = 0.0f;

    beam = mempAlloc(BEAM_ALLOC_SIZE, MEMPOOL_STAGE);
    autogun->beam = beam;
    beam->age = -1;

    autogun->is_active = FALSE;
    autogun->damageAccum = 0.0f;

    if (autogun->padID >= 0)
    {
        pad = setupGetPad(autogun->padID);
        prop = autogun->prop;

        xdiff = pad->pos.x - prop->pos.x;
        ydiff = pad->pos.y - prop->pos.y;
        zdiff = pad->pos.z - prop->pos.z;

        autogun->homeYaw = mathAtan2(xdiff, zdiff);
        autogun->homePitch = mathAtan2(ydiff, sqrtf((xdiff * xdiff) + (zdiff * zdiff)));
    }
}

/**
 * Creates a monitor. One with no pad hangs from another object (a rack): its owner is
 * OwnerOffset records on, and it hangs from the owner's model part OwnerPart, tilted up.
 */
void setupSingleMonitor(MonitorObjRecord *monitor, s32 cmdindex)
{
    s32 modelnum;
    ObjectRecord *owner;
    PropRecord *prop;
    f32 scale;
    s32 part;

    monitorInit(&monitor->Monitor, monitor->ImageNum);

    if (monitor->pad < 0 && (monitor->flags & PROPFLAG_INSIDEANOTHEROBJ) == 0)
    {
        modelnum = monitor->obj;
        owner = (ObjectRecord *)setupGetPtrToCommandByIndex(cmdindex + monitor->OwnerOffset);

        modelLoad(modelnum);

        scale = monitor->extrascale * (1.0f / 256.0f);
        monitor->health = setupFixedToFloat(&monitor->health, 1.0f);

        if (isMpGame())
        {
            monitor->state |= PROPSTATE_RESPAWN;
        }

        prop = objInitWithAutoModel((ObjectRecord *)monitor);
        monitor->embedment = embedmentAllocate();

        if (prop != NULL && monitor->embedment != NULL)
        {
            monitor->runtime_bitflags |= RUNTIMEBITFLAG_EMBEDDED;
            modelSetScale(monitor->model, monitor->model->scale * scale);

            part = monitor->OwnerPart;

            if (part < 0 || part > 2)
            {
                part = 3;
            }

            monitor->model->attachedto = owner->model;
            monitor->model->attachedto_objinst = owner->model->obj->Switches[part];

            propReparent(prop, owner->prop);
            mtx4LoadXRotation(HANGING_MONITOR_TILT, &monitor->embedment->matrix);
            mtx4ScaleRows012(monitor->model->scale / owner->model->scale, &monitor->embedment->matrix);
        }
    }
    else
    {
        setupObject((ObjectRecord *)monitor);
    }

    if ((monitor->flags & PROPFLAG_MONITOR_RENDERPOSTBG) && monitor->prop != NULL)
    {
        monitor->prop->flags |= PROPFLAG_RENDERPOSTBG;
    }
}

/**
 * Creates an object with four monitor screens.
 */
void setupMultiMonitor(MultiMonitorObjRecord *monitor)
{
    s32 i;

    for (i = 0; i < 4; i++)
    {
        monitorInit(&monitor->Monitor[i], monitor->ImageNums[i]);
    }

    setupObject((ObjectRecord *)monitor);
}

/**
 * Creates body armour, full.
 */
void setupArmour(BodyArmourRecord *armour)
{
    armour->initialamount = setupFixedToFloat(&armour->initialamount, 1.0f);
    armour->amount = armour->initialamount;
    setupObject((ObjectRecord *)armour);
}

/**
 * Creates tinted glass. Glass in a portal finds the portal through its bound pad's
 * centre, so the glass can hide the room behind it once it's opaque enough.
 */
void setupTintedGlass(TintedGlassRecord *glass)
{
    BoundPadRecord *pad;
    coord3d below;
    coord3d above;
    s32 i;

    if ((glass->flags & PROPFLAG_GLASS_HASPORTAL) && !isNotBoundPad(glass->pad))
    {
        pad = &g_CurrentSetup.boundpads[getBoundPadNum(glass->pad)];
        padGetBboxCentre(pad, &below);

        for (i = 0; i < 3; i++)
        {
            above.f[i] = (10.0f * pad->up.f[i]) + below.f[i];
        }

        for (i = 0; i < 3; i++)
        {
            below.f[i] -= 10.0f * pad->up.f[i];
        }

        glass->portalnum = bgFindPortalOnSegment(&below, &above);

        // only converted for glass with a portal: the others never use it
        glass->minOpacity = setupFixedToFloat(&glass->minOpacity, 1.0f);
    }

    setupObject((ObjectRecord *)glass);
}

/**
 * Creates an ammo crate, loading the models of the ammo it holds. In multiplayer it holds
 * ammo for the weapon set's weapon placed before it; that's filled in even if the crate
 * is skipped (not loaded on this difficulty or player count).
 */
void setupAmmoCrate(MultiAmmoCrateRecord *crate, bool skipped)
{
    struct s_mp_weapon_set *mpweapon;
    s32 amount = 1;
    s32 i;

    if (isMpGame())
    {
        mpweapon = &mpWeaponSetGetItems()[s_LastMpWeaponSlot];
        amount = mpweapon->ammoamount;
        crate->slots[mpweapon->ammotype - 1].quantity = amount;
    }

    if (amount <= 0 || skipped)
    {
        return;
    }

    for (i = 0; i < AMMOTYPE_GLOBAL_MAX; i++)
    {
        if (crate->slots[i].quantity > 0 && crate->slots[i].modelnum != 0xffff)
        {
            modelLoad(crate->slots[i].modelnum);
        }
    }

    setupObject((ObjectRecord *)crate);
}

/**
 * Creates a tank, facing the way its pad does, with its turret straight ahead.
 */
void setupTank(TankRecord *tank)
{
    f32 floory = 0.0f;
    PropRecord *prop;

    weaponLoadProjectileModels(ITEM_TANKSHELLS);
    setupObject((ObjectRecord *)tank);

    tank->turret_vertical_angle = 0.0f;
    tank->turret_orientation_angle = 0.0f;
    tank->tank_orientation_angle = M_TAU_F - mathAtan2(tank->mtx.m[2][0], tank->mtx.m[2][2]);

    prop = tank->prop;

    if (prop != NULL)
    {
        floory = stanGetFloorY(prop->stan, prop->pos.f[0], prop->pos.f[2]);
    }

    tank->stan_y = floory;
    tank->smoothYSum = floory / TANK_Y_SMOOTHING;
}

/**
 * Creates a vehicle, stopped, ready to run its AI list (the setup data holds the list's
 * id). A flag hides its model's sixth part.
 */
void setupVehicle(VehichleRecord *vehicle)
{
    setupObject((ObjectRecord *)vehicle);

    if (vehicle->model != NULL && vehicle->model->obj->Switches[5] != NULL)
    {
        modelGetNodeRwData(vehicle->model, vehicle->model->obj->Switches[5])->Switch.visible = (vehicle->flags & PROPFLAG_VEHICLE_HIDEPART) == 0;
    }

    vehicle->speed = 0.0f;
    vehicle->wheelxrot = 0.0f;
    vehicle->wheelyrot = 0.0f;
    vehicle->speedaim = 0.0f;
    vehicle->turnrot60 = 0.0f;
    vehicle->roty = 0.0f;
    vehicle->speedtime60 = -1.0f;
    vehicle->ailist = ailistFindById((s32)vehicle->ailist);
    vehicle->aioffset = 0;
    vehicle->aireturnlist = -1;
    vehicle->path = 0;
    vehicle->nextstep = 0;
    vehicle->Sound = 0;
}

/**
 * Creates an aircraft, stopped with its rotors still, ready to run its AI list (the setup
 * data holds the list's id).
 */
void setupAircraft(AircraftRecord *aircraft)
{
    setupObject((ObjectRecord *)aircraft);

    aircraft->speed = 0.0f;
    aircraft->speedaim = 0.0f;
    aircraft->rotoryrot = 0.0f;
    aircraft->rotaryspeed = 0.0f;
    aircraft->rotaryspeedaim = 0.0f;
    aircraft->yrot = 0.0f;
    aircraft->speedtime60 = -1.0f;
    aircraft->rotaryspeedtime = -1.0f;
    aircraft->ailist = ailistFindById((s32)aircraft->ailist);
    aircraft->aioffset = 0;
    aircraft->aireturnlist = -1;
    aircraft->nextstep = 0;
    aircraft->path = 0;
    aircraft->Sound = 0;
}
