/**
 * Level setup for props. setupCreateProps loads the setup file, fixes up its pointers,
 * counts the props to size the model slots, then walks the propdef list and creates
 * every door, object, weapon, monitor and so on (setupObject, setupDoor, setupWeapon...).
 * The propdefs are used in place: an ObjectRecord is the setup file's own record, so the
 * level's prop data lives in the setup file for the whole level. setupFreeProps frees
 * them again at level end.
 *
 * Also here: helpers for finding setup records by index (setupCommandGetObject,
 * tagGetCommandIndex), loading a prop's model (modelLoad), placing a prop in its rooms
 * (setupUpdateObjectRoomPosition), and the per-level reset of the global prop state
 * (propsResetLevelState).
 */
#include <ultra64.h>
#include "game/model/chrobjdata.h"
#include <PR/libaudio.h>
#include <memp.h>
#include "game/menu/mp_weapon.h"
#include "game/player/bondview_r.h"
#include "game/world/bg.h"
#include "game/player/bondview_r.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/chraction.h"
#include "game/prop/propobj.h"
#include "game/player/bondinv.h"
#include "game/nav/stan.h"
#include "limits.h"
#include "game/text/language.h"
#include "game/math/trig.h"
#include "game/math/matrix.h"
#include "game/menu/mp_weapon.h"
#include "game/asset/fileload.h"
#include "game/mission/objective.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/player/player.h"
#include "game/prop/setup.h"
#include "game/nav/stan.h"
#include "game/model/model.h"
#include "token.h"

/**
 * EU .bss 0x80068480
*/
ITEM_IDS lastmpweaponnum;

// redeclare with the element count so ARRAYCOUNT works in setupCreateProps
extern ItemModelFileRecord PitemZ_entries[341];

// forward declarations

s32 setupCountPropdefs(PROPDEF_TYPE type);
void padGetBboxCentre(struct BoundPadRecord *pad, struct coord3d *centre);
void setupObject(s32 stageid, ObjectRecord *obj, s32 cmdindex);
void setupWeapon(s32 stageid, WeaponObjRecord* weapon, s32 cmdindex);
void setupHat(s32 stageid, ObjectRecord* hat, s32 cmdindex);
void setupKey(s32 stageid, ObjectRecord* key, s32 cmdindex);
void setupCctv(s32 stageid, CCTVRecord *cctv, s32 cmdindex);
void setupAutogun(s32 stageID, AutogunRecord *autogun, s32 cmdindex);
void setupHangingMonitors(s32 stageid, ObjectRecord* rack, s32 cmdindex);
void setupSingleMonitor(s32 stageID, MonitorObjRecord *monitor, s32 cmdindex);
void setupMultiMonitor(s32 stageID, MultiMonitorObjRecord* monitor, s32 cmdindex);
void padGetRoomsEitherSide(struct BoundPadRecord *pad, s32 *room1, s32 *room2, struct coord3d *pos1, struct coord3d *pos2);
void setupDoor(s32 stageid, struct DoorRecord *door, s32 cmdindex);


s32 setupCountPropdefs(PROPDEF_TYPE type)
{
    PropDefHeaderRecord *propdef = (PropDefHeaderRecord *) g_CurrentSetup.propDefs;
    s32 count = 0;

    if (propdef != NULL)
    {
        while (propdef->type != PROPDEF_END)
        {
            if (propdef->type == (type & 0xFF))
            {
                count ++;
            }
            propdef = &propdef[sizepropdef((PropDefHeaderRecord* ) propdef)];
        }
    }
    return count;
}


/**
 * perfect dark padGetCentre (pad.c)
 *
*/
void padGetBboxCentre(struct BoundPadRecord *pad, struct coord3d *centre)
{
    struct coord3d normal;
    f32 scale;
    struct bbox bb;
    f32 temp;

    bb.zmax = pad->bbox.xmin;
    bb.zmin = pad->bbox.xmax;
    bb.ymax = pad->bbox.ymin;
    bb.ymin = pad->bbox.ymax;
    bb.xmax = pad->bbox.zmin;
    bb.xmin = pad->bbox.zmax;

    normal.f[0] = (pad->up.f[1] * pad->look.f[2]) - (pad->up.f[2] * pad->look.f[1]);
    normal.f[1] = (pad->up.f[2] * pad->look.f[0]) - (pad->up.f[0] * pad->look.f[2]);
    normal.f[2] = (pad->up.f[0] * pad->look.f[1]) - (pad->up.f[1] * pad->look.f[0]);

    temp = (normal.f[0] * normal.f[0]) + (normal.f[1] * normal.f[1]) + (normal.f[2] * normal.f[2]);
    scale = 1.0f / sqrtf(temp);

    normal.f[0] *= scale;
    normal.f[1] *= scale;
    normal.f[2] *= scale;

    centre->f[0] = pad->pos.f[0] + (
			(bb.zmax + bb.zmin) * normal.f[0] +
			(bb.ymax + bb.ymin) * pad->up.f[0] +
			(bb.xmax + bb.xmin) * pad->look.f[0]) * 0.5f;

	centre->f[1] = pad->pos.f[1] + (
			(bb.zmax + bb.zmin) * normal.f[1] +
			(bb.ymax + bb.ymin) * pad->up.f[1] +
			(bb.xmax + bb.xmin) * pad->look.f[1]) * 0.5f;

	centre->f[2] = pad->pos.f[2] + (
			(bb.zmax + bb.zmin) * normal.f[2] +
			(bb.ymax + bb.ymin) * pad->up.f[2] +
			(bb.xmax + bb.xmin) * pad->look.f[2]) * 0.5f;

}

/**
*/
void setupObject(s32 stageid, ObjectRecord *obj, s32 cmdindex)
{
    s32 modelnum;
    f32 minscale;
    struct coord3d floorpos;
    struct StandTile *floortile;
    struct coord3d pos;
    StandTile *tile;
    Mtxf mtx;
    struct coord3d centre;
    struct PropRecord *chrprop;
    f32 extrascale;
    s32 canRespawn;
    struct BoundPadRecord *boundpad;
    ChrRecord *chr;
    ModelRoData_BoundingBoxRecord *bbox;
    struct PadRecord *pad;
    struct PropRecord *prop;
    f32 xscale;
    f32 yscale;
    f32 zscale;
    f32 maxscale;

    modelnum = obj->obj;
    boundpad = NULL;

    modelLoad(modelnum);

    extrascale = obj->extrascale * 0.00390625f;

    obj->damage = *(s32*)&obj->damage / 65536.0f;

    if (isMpGame())
    {
        canRespawn = 1;

        if ((frontGetMpScenario() == SCENARIO_TLD) && (obj->obj == PROP_FLAG))
        {
            canRespawn = 0;
        }
        else if ((frontGetMpScenario() == SCENARIO_MWTGG) && (obj->obj == PROP_CHRGOLDEN))
        {
            canRespawn = 0;
        }

        if (canRespawn != 0)
        {
            obj->state |= PROPSTATE_RESPAWN; // respawn enabled
        }
    }

    if (obj->flags & PROPFLAG_INSIDEANOTHEROBJ)
    {
        if (obj->type == PROP_TYPE_SMOKE)
        {
            weaponInitWithModelDef(obj, PitemZ_entries[modelnum].header);
        }
        else
        {
            objInitWithModelDef(obj, PitemZ_entries[modelnum].header);
        }

        modelSetScale(obj->model, obj->model->scale * extrascale);
    }
    else if (obj->flags & PROPFLAG_ASSIGNEDTOCHR)
    {
        chr = chrFindByLiteralId(obj->pad);

        if ((chr != NULL) && (chr->prop != NULL) && (chr->model != NULL))
        {
            if (obj->type == 8)
            {
                chrprop = weaponInitWithModelDef(obj, PitemZ_entries[modelnum].header);
            }
            else
            {
                chrprop = objInitWithModelDef(obj, PitemZ_entries[modelnum].header);
            }

            modelSetScale(obj->model, obj->model->scale * extrascale);
            propReparent(chrprop, chr->prop);
        }
    }
    else
    {
        if (isNotBoundPad(obj->pad))
        {
            pad = &g_CurrentSetup.pads[obj->pad];

            mtx4LoadFacing(&mtx, 0.0f, 0.0f, 0.0f, -pad->look.f[0], -pad->look.f[1], -pad->look.f[2], pad->up.f[0], pad->up.f[1], pad->up.f[2]);

            pos.f[0] = pad->pos.f[0];
            pos.f[1] = pad->pos.f[1];
            pos.f[2] = pad->pos.f[2];

            if (obj->flags & PROPFLAG_ONSCREEN)
            {
                centre.f[0] = pad->pos.f[0];
                centre.f[1] = pad->pos.f[1];
                centre.f[2] = pad->pos.f[2];
            }
            else
            {
                // same as above?

                centre.f[0] = pad->pos.f[0];
                centre.f[1] = pad->pos.f[1];
                centre.f[2] = pad->pos.f[2];
            }

            tile = pad->stan;
        }
        else
        {
            boundpad = &g_CurrentSetup.boundpads[getBoundPadNum(obj->pad)];

            mtx4LoadFacing(&mtx, 0.0f, 0.0f, 0.0f, -boundpad->look.f[0], -boundpad->look.f[1], -boundpad->look.f[2], boundpad->up.f[0], boundpad->up.f[1], boundpad->up.f[2]);

            if (!(obj->flags2 & PROPFLAG2_00000001))
            {
                padGetBboxCentre(boundpad, &pos);

                centre.f[0] = pos.f[0] + (boundpad->up.f[0] * ((boundpad->bbox.ymin - boundpad->bbox.ymax) * 0.5f));
                centre.f[1] = pos.f[1] + (boundpad->up.f[1] * ((boundpad->bbox.ymin - boundpad->bbox.ymax) * 0.5f));
                centre.f[2] = pos.f[2] + (boundpad->up.f[2] * ((boundpad->bbox.ymin - boundpad->bbox.ymax) * 0.5f));

                tile = boundpad->stan;

                if (stanWalkLineNoCallback(&tile, boundpad->pos.f[0], boundpad->pos.f[2], pos.f[0], pos.f[2]) == 0)
                {
                    pos.f[0] = boundpad->pos.f[0];
                    pos.f[1] = boundpad->pos.f[1];
                    pos.f[2] = boundpad->pos.f[2];

                    tile = boundpad->stan;

                    if (!(obj->flags & PROPFLAG_ONSCREEN) && !(obj->flags & PROPFLAG_00001000))
                    {
                        // removed
                    }
                }
            }
            else
            {
                pos.f[0] = boundpad->pos.f[0];
                pos.f[1] = boundpad->pos.f[1];
                pos.f[2] = boundpad->pos.f[2];

                tile = boundpad->stan;

                padGetBboxCentre(boundpad, &centre);

                centre.f[0] += (boundpad->bbox.ymin - boundpad->bbox.ymax) * 0.5f * boundpad->up.f[0];
                centre.f[1] += (boundpad->bbox.ymin - boundpad->bbox.ymax) * 0.5f * boundpad->up.f[1];
                centre.f[2] += (boundpad->bbox.ymin - boundpad->bbox.ymax) * 0.5f * boundpad->up.f[2];
            }
        }

        if (getposstan(&pos, tile, 0.0f, &floorpos, &floortile) != 0)
        {
            if (obj->type == PROP_TYPE_SMOKE)
            {
                prop = weaponInitWithModelDef(obj, PitemZ_entries[modelnum].header);
            }
            else
            {
                prop = objInitWithAutoModel(obj);
            }

            if (boundpad != NULL)
            {
                bbox = chrobjGetBboxFromObjectRecord(obj);
                if (bbox != NULL)
                {
                    xscale = 1.0f;
                    yscale = 1.0f;
                    zscale = 1.0f;

                    if (obj->flags & (PROPFLAG_00000010 | PROPFLAG_00000020))
                    {
                        if (bbox->Bounds.xmin < bbox->Bounds.xmax)
                        {
                            if (obj->flags & PROPFLAG_ONSCREEN)
                            {
                                xscale = (boundpad->bbox.xmax - boundpad->bbox.xmin) / ((bbox->Bounds.xmax - bbox->Bounds.xmin) * obj->model->scale);
                            }
                            else
                            {
                                xscale = (boundpad->bbox.xmax - boundpad->bbox.xmin) / ((bbox->Bounds.xmax - bbox->Bounds.xmin) * obj->model->scale);
                            }
                        }
                    }

                    if (obj->flags & (PROPFLAG_00000010 | PROPFLAG_00000040))
                    {
                        if (bbox->Bounds.ymin < bbox->Bounds.ymax)
                        {
                            if (obj->flags & PROPFLAG_ONSCREEN)
                            {
                                zscale = (boundpad->bbox.zmax - boundpad->bbox.zmin) / ((bbox->Bounds.ymax - bbox->Bounds.ymin) * obj->model->scale);
                            }
                            else
                            {
                                yscale = (boundpad->bbox.ymax - boundpad->bbox.ymin) / ((bbox->Bounds.ymax - bbox->Bounds.ymin) * obj->model->scale);
                            }
                        }
                    }

                    if (obj->flags & (PROPFLAG_00000010 | PROPFLAG_00000080))
                    {
                        if (bbox->Bounds.zmin < bbox->Bounds.zmax)
                        {
                            if (obj->flags & PROPFLAG_ONSCREEN)
                            {
                                yscale = (boundpad->bbox.ymax - boundpad->bbox.ymin) / ((bbox->Bounds.zmax - bbox->Bounds.zmin) * obj->model->scale);
                            }
                            else
                            {
                                zscale = (boundpad->bbox.zmax - boundpad->bbox.zmin) / ((bbox->Bounds.zmax - bbox->Bounds.zmin) * obj->model->scale);
                            }
                        }
                    }

                    minscale = xscale;

                    if (yscale < minscale)
                    {
                        minscale = yscale;
                    }

                    if (zscale < minscale)
                    {
                        minscale = zscale;
                    }

                    maxscale = xscale;

                    if (xscale < yscale)
                    {
                        maxscale = yscale;
                    }

                    if (maxscale < zscale)
                    {
                        maxscale = zscale;
                    }

                    if (obj->flags & PROPFLAG_00000010)
                    {
                        zscale = minscale;
                        yscale = minscale;
                        xscale = minscale;
                    }
                    else
                    {
                        if (!(obj->flags & PROPFLAG_00000020))
                        {
                            if (obj->flags & PROPFLAG_ONSCREEN)
                            {
                                if (bbox->Bounds.xmax == bbox->Bounds.xmin)
                                {
                                    xscale = maxscale;
                                }
                            }
                            else if (bbox->Bounds.xmax == bbox->Bounds.xmin)
                            {
                                xscale = maxscale;
                            }
                        }

                        if (!(obj->flags & PROPFLAG_00000040))
                        {
                            if (obj->flags & PROPFLAG_ONSCREEN)
                            {
                                if (bbox->Bounds.ymax == bbox->Bounds.ymin)
                                {
                                    zscale = maxscale;
                                }
                            }
                            else if (bbox->Bounds.ymax == bbox->Bounds.ymin)
                            {
                                yscale = maxscale;
                            }
                        }

                        if (!(obj->flags & PROPFLAG_00000080))
                        {
                            if (obj->flags & PROPFLAG_ONSCREEN)
                            {
                                if (bbox->Bounds.zmax == bbox->Bounds.zmin)
                                {
                                    yscale = maxscale;
                                }
                            }
                            else if (bbox->Bounds.zmax == bbox->Bounds.zmin)
                            {
                                zscale = maxscale;
                            }
                        }
                    }

                    xscale /= maxscale;
                    yscale /= maxscale;
                    zscale /= maxscale;

                    if ((xscale <= 0.000001f) || (yscale <= 0.000001f) || (zscale <= 0.000001f))
                    {
                        zscale = 1.0f;
                        yscale = 1.0f;
                        xscale = 1.0f;
                    }

                    mtx4ScaleRow0(xscale, mtx.m[0]);
                    mtx4ScaleRow1(yscale, mtx.m[0]);
                    mtx4ScaleRow2(zscale, mtx.m[0]);

                    modelSetScale(obj->model, obj->model->scale * maxscale);
                }
            }

            modelSetScale(obj->model, obj->model->scale * extrascale);
            mtx4ScaleRows012(obj->model->scale, mtx.m[0]);

            if (obj->flags & PROPFLAG_ONSCREEN)
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
    }
}

/**
*/
void setupWeapon(s32 stageid, WeaponObjRecord* weapon, s32 cmdindex)
{
    bool hastoken;
    ChrRecord* chr;
    bool giveweapon;
    s32 setindex;
    struct s_mp_weapon_set* weapon_set;

    if ((weapon->flags & PROPFLAG_ASSIGNEDTOCHR))
    {
        chr = chrFindByLiteralId(weapon->pad);

        if (chr && chr->prop && chr->model)
        {
            if (cheatIsActive(CHEAT_ENEMY_ROCKETS))
            {
                switch ((s8)weapon->weaponnum)
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
                        weapon->weaponnum = ITEM_ROCKETLAUNCH;
                        weapon->obj = PROP_CHRROCKETLAUNCH;
                        weapon->extrascale = 256;
                        break;
                }
            }

            weaponLoadProjectileModels((s8)weapon->weaponnum);
            chrSetupHeldWeapon(weapon, chr);
        }
    }
    else
    {
        hastoken = 1;
        giveweapon = 1;

        if (isMpGame())
        {
            lastmpweaponnum = -1;

            switch ((u8)weapon->weaponnum)
            {
                case ITEM_UNARMED + 0xF0:
                case ITEM_FIST + 0xF0:
                case ITEM_KNIFE + 0xF0:
                case ITEM_THROWKNIFE + 0xF0:
                case ITEM_WPPK + 0xF0:
                case ITEM_WPPKSIL + 0xF0:
                case ITEM_TT33 + 0xF0:
                case ITEM_SKORPION + 0xF0:
                    weapon_set = mpWeaponSetGetItems();

                    setindex = (u8)weapon->weaponnum - 0xF0;
                    lastmpweaponnum = setindex;

                    weapon->weaponnum = weapon_set[setindex].itemID;
                    weapon->obj = weapon_set[setindex].propID;
#if defined(VERSION_EU)
                    weapon->extrascale = (weapon_set[setindex].size16);
#else
                    weapon->extrascale = (weapon_set[setindex].size * 256.0f);
#endif

                    giveweapon = weapon_set[setindex].allowpickup;

                    break;

                case ITEM_TOKEN:

                    hastoken = 1;
                    giveweapon = 1;

                    if (frontGetMpScenario() != SCENARIO_TLD)
                    {
                        giveweapon = 0;
                    }
                    break;
            }
        }

        if ((weapon->weaponnum != ITEM_UNARMED) && giveweapon)
        {
            weaponLoadProjectileModels(weapon->weaponnum);
            setupObject(stageid, (struct ObjectRecord*)weapon, cmdindex);
        }
    }
}

//i should be object hat
void setupHat(s32 stageid, ObjectRecord* hat, s32 cmdindex)
{
    if (hat->flags & PROPFLAG_ASSIGNEDTOCHR) {
        ChrRecord* chr = chrFindByLiteralId(hat->pad);
        if (chr && chr->prop && chr->model) {
            hatAssignToChr(hat, chr);
        }
    } else {
        setupObject(stageid, hat, cmdindex);
    }
}

//i should be object key
void setupKey(s32 stageid, ObjectRecord* key, s32 cmdindex)
{
    setupObject(stageid, key, cmdindex);
}


/**
*/
void setupCctv(s32 stageid, CCTVRecord *cctv, s32 cmdindex)
{
    struct coord3d *headoffset;
    struct PadRecord *lookpad;
    struct coord3d headpos;
    Mtxf *headmtx;

    setupObject(stageid, (struct ObjectRecord*)cctv, cmdindex);

    if (cctv->lookpad >= 0)
    {
        headoffset = (struct coord3d*)cctv->model->obj->Switches[0]->Data;

        if (isNotBoundPad(cctv->lookpad))
        {
            lookpad = &g_CurrentSetup.pads[cctv->lookpad];
        }
        else
        {
            lookpad = (struct PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(cctv->lookpad)];
        }

        headpos.f[0] = headoffset->f[0];
        headpos.f[1] = headoffset->f[1];
        headpos.f[2] = headoffset->f[2];

        mtx4RotateVecInPlace(&cctv->mtx, &headpos);

        headmtx = &cctv->headMtx;

        headpos.f[0] += cctv->prop->pos.f[0];
        headpos.f[1] += cctv->prop->pos.f[1];
        headpos.f[2] += cctv->prop->pos.f[2];

        mtx4LoadFacing(headmtx, 0.0f, 0.0f, 0.0f, headpos.f[0] - lookpad->pos.f[0], headpos.f[1] - lookpad->pos.f[1], headpos.f[2] - lookpad->pos.f[2], 0.0f, 1.0f, 0.0f);
        mtx4ScaleRows012(cctv->model->scale, headmtx->m[0]);

        if (cctv->convert_to_f32 == 0)
        {
            cctv->convert_to_f32 = 1;
            cctv->sweepYaw1 = (*(s32*)&cctv->sweepYaw1 * M_TAU_F) / 65536.0f;
            cctv->sweepYaw2 = (*(s32*)&cctv->sweepYaw2 * M_TAU_F) / 65536.0f;
            cctv->maxYawSpeed = (*(s32*)&cctv->maxYawSpeed * M_TAU_F) / 65536.0f;
            cctv->viewDist = *(s32*)&cctv->viewDist;
        }

        cctv->towardsYaw1 = 0;
        cctv->yawSpeed = 0.0f;
        cctv->yaw = cctv->sweepYaw1;
        cctv->baseYaw = mathAtan2(headpos.f[0] - lookpad->pos.f[0], headpos.f[2] - lookpad->pos.f[2]);
        cctv->timer = 0;
    }
}

void setupAutogun(s32 stageID, AutogunRecord *autogun, s32 cmdindex)
{
    s8 *beam;

    setupObject(stageID, (ObjectRecord *) autogun, cmdindex);

#ifdef VERSION_EU
    autogun->speed = ((*((s32 *) (&autogun->speed))) * 7.5398226f) / 65536.0f;
    autogun->aimdist = ((*((s32 *) (&autogun->aimdist))) * 100.0f) / 65536.0f;
    autogun->maxYaw = ((*((s32 *) (&autogun->maxYaw))) * M_TAU_F) / 65536.0f;
    autogun->minYaw = ((*((s32 *) (&autogun->minYaw))) * M_TAU_F) / 65536.0f;
#endif

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

#ifndef VERSION_EU
    autogun->speed = ((*((s32 *) (&autogun->speed))) * M_TAU_F) / 65536.0f;
    autogun->aimdist = ((*((s32 *) (&autogun->aimdist))) * 100.0f) / 65536.0f;
    autogun->maxYaw = ((*((s32 *) (&autogun->maxYaw))) * M_TAU_F) / 65536.0f;
    autogun->minYaw = ((*((s32 *) (&autogun->minYaw))) * M_TAU_F) / 65536.0f;
#endif

    beam          = mempAlloc(0x30U, MEMPOOL_STAGE);
    autogun->beam = beam;
    *beam = -1;

    autogun->is_active = FALSE;
    autogun->damageAccum = 0.0f;

    if (autogun->padID >= 0)
    {
        f32 xdiff;
        f32 ydiff;
        f32 zdiff;
        PadRecord *pad;
        PropRecord *prop;

        if (autogun->padID < 0x2710)
        {
            pad = &g_CurrentSetup.pads[autogun->padID];
        }
        else
        {
            pad = &g_CurrentSetup.boundpads[getBoundPadNum(autogun->padID)];
        }

        prop = autogun->prop;

        xdiff = pad->pos.x - prop->pos.x;
        ydiff = pad->pos.y - prop->pos.y;
        zdiff = pad->pos.z - prop->pos.z;

        autogun->homeYaw = mathAtan2(xdiff, zdiff);
        autogun->homePitch = mathAtan2(ydiff, sqrtf((xdiff * xdiff) + (zdiff * zdiff)));
    }
}


//i should be object rack
void setupHangingMonitors(s32 stageid, ObjectRecord* rack, s32 cmdindex)
{
    setupObject(stageid, rack, cmdindex);
}


void setupSingleMonitor(s32 stageID, MonitorObjRecord *monitor, s32 cmdindex)
{
    MonitorRecord *record;
    s32 modelnum;
    ObjectRecord *owner;
    PropRecord *prop;
    f32 scale;

    monitor->Monitor = g_MonitorAnimController;
    record = &monitor->Monitor;
    monitorSetImageByNum(&monitor->Monitor, monitor->ImageNum);

    if (monitor->pad < 0 && (monitor->flags & PROPFLAG_INSIDEANOTHEROBJ) == 0)
    {
        modelnum = monitor->obj;
        owner = (struct ObjectRecord *)setupGetPtrToCommandByIndex(cmdindex + monitor->OwnerOffset);

        modelLoad(modelnum);

        scale = monitor->extrascale * (1.0f / 256.0f);
        monitor->damage = *(s32*)&monitor->damage / M_U16_MAX_VALUE_F;

        if (isMpGame())
        {
            monitor->state |= PROPSTATE_RESPAWN;
        }

        prop = objInitWithAutoModel((ObjectRecord*)monitor);
        monitor->embedment = embedmentAllocate();

        if (prop && monitor->embedment)
        {
            monitor->runtime_bitflags |= RUNTIMEBITFLAG_EMBEDDED;
            modelSetScale(monitor->model, monitor->model->scale * scale);
            monitor->model->attachedto = owner->model;

            if (monitor->OwnerPart == 0)
            {
                monitor->model->attachedto_objinst = owner->model->obj->Switches[0];
            }
            else if (monitor->OwnerPart == 1)
            {
                monitor->model->attachedto_objinst = owner->model->obj->Switches[1];
            }
            else if (monitor->OwnerPart == 2)
            {
                monitor->model->attachedto_objinst = owner->model->obj->Switches[2];
            }
            else
            {
                monitor->model->attachedto_objinst = owner->model->obj->Switches[3];;
            }

            propReparent(prop, owner->prop);
            mtx4LoadXRotation(0.36651915f, (Mtxf*)&monitor->embedment->matrix);
            mtx4ScaleRows012(monitor->model->scale / owner->model->scale, (f32*)&monitor->embedment->matrix);
        }
    }
    else
    {
        setupObject(stageID, (ObjectRecord*)monitor, cmdindex);
    }

    if ((monitor->flags & PROPFLAG_MONITOR_RENDERPOSTBG) && monitor->prop)
    {
        monitor->prop->flags |= PROPFLAG_RENDERPOSTBG;
    }
}


void setupMultiMonitor(s32 stageID, MultiMonitorObjRecord* monitor, s32 cmdindex)
{
    monitor->Monitor[0] = g_MonitorAnimController;
    monitorSetImageByNum(&monitor->Monitor[0], monitor->ImageNums[0]);

    monitor->Monitor[1] = g_MonitorAnimController;
    monitorSetImageByNum(&monitor->Monitor[1], monitor->ImageNums[1]);

    monitor->Monitor[2] = g_MonitorAnimController;
    monitorSetImageByNum(&monitor->Monitor[2], monitor->ImageNums[2]);

    monitor->Monitor[3] = g_MonitorAnimController;
    monitorSetImageByNum(&monitor->Monitor[3], monitor->ImageNums[3]);

    setupObject(stageID, monitor, cmdindex);
}

void padGetRoomsEitherSide(struct BoundPadRecord *pad, s32 *room1, s32 *room2, struct coord3d *pos1, struct coord3d *pos2)
{
    StandTile *tile;
    struct coord3d normal;
    struct coord3d center;
    StandTile *centretile;
    f32 scale;

    padGetBboxCentre(pad, &center);
    centretile = (StandTile *)pad->stan;

    if (stanWalkLineNoCallback(&centretile, pad->pos.f[0], pad->pos.f[2], center.f[0], center.f[2]) == 0)
    {
        centretile = (StandTile *)pad->stan;
        center.f[0] = pad->pos.f[0];
        center.f[1] = pad->pos.f[1];
        center.f[2] = pad->pos.f[2];
    }

    normal.f[0] = (pad->up.f[1] * pad->look.f[2]) - (pad->up.f[2] * pad->look.f[1]);
    normal.f[1] = (pad->up.f[2] * pad->look.f[0]) - (pad->up.f[0] * pad->look.f[2]);
    normal.f[2] = (pad->up.f[0] * pad->look.f[1]) - (pad->up.f[1] * pad->look.f[0]);

    scale = 1.0f / sqrtf(((normal.f[0] * normal.f[0]) + (normal.f[1] * normal.f[1])) + (normal.f[2] * normal.f[2]));
    tile = centretile;

    normal.f[0] *= scale;
    normal.f[1] *= scale;
    normal.f[2] *= scale;

    pos1->f[0] = center.f[0] + (normal.f[0] * 50.0f);
    pos1->f[1] = center.f[1];
    pos1->f[2] = center.f[2] + (normal.f[2] * 50.0f);


    stanWalkLineNoCallback(&tile, center.f[0], center.f[2], pos1->f[0], pos1->f[2]);


    *room1 = (s32) tile->room;
    tile = centretile;

    pos2->f[0] = center.f[0] - (normal.f[0] * 50.0f);
    pos2->f[1] = center.f[1];
    pos2->f[2] = center.f[2] - (normal.f[2] * 50.0f);

    stanWalkLineNoCallback(&tile, center.f[0], center.f[2], pos2->f[0], pos2->f[2]);


    *room2 = (s32) tile->room;

    if (*room2 == *room1)
    {
        *room2 = -1;
    }
}


extern f32 g_DoorScale;
/**
 * NTSC ADDRESS: 7F003480
 * PAL ADDRESS: 7F0033F0
 * Perfect Dark: void setupCreateDoor(struct doorobj *door, s32 cmdindex)
*/
void setupDoor(s32 stageid, struct DoorRecord *door, s32 cmdindex)
{
    s32 modelnum;
    struct BoundPadRecord *pad;
    StandTile *floortile;
    PropRecord *prop;
    struct coord3d floorpos;
    s32 portalnum;
    s32 room1;
    s32 room2;
    struct coord3d side1;
    struct coord3d side2;
    struct PortalMetric plane;
    struct ModelRoData_BoundingBoxRecord *modelbbox;
    struct coord3d newpos;
    StandTile *tile;
    Mtxf padmtx;
    f32 dist; // no sp
    ModelFileHeader *modelfile;
    struct coord3d pos;                           /* compiler-managed */
    StandTile *postile;
    Mtxf mtx;
    struct coord3d centre;
    Mtxf rotz;
    struct coord3d extent;
    struct bbox bb2;
    f32 xscale;
    f32 yscale;
    f32 zscale;
    f32 scale;
    //StandTile *stan;

    modelnum = door->obj;

    portalnum = -1;
    room1 = -1;
    room2 = -1;

    modelLoad(modelnum);

    pad = &(g_CurrentSetup.boundpads[door->pad]);

    if ((door->flags & PROPFLAG_CULL_BEHIND_DOOR) || (door->flags & PROPFLAG_NO_PORTAL_CLOSE))
    {
        padGetRoomsEitherSide(pad, &room1, &room2, &side1, &side2);

        if ((door->flags & PROPFLAG_CULL_BEHIND_DOOR) && (room1 >= 0) && (room2 >= 0))
        {
            portalnum = bgGetPortalBetweenRooms(room1, room2, &side1, &side2);
        }
    }

    if (g_DoorScale != 1.0f)
    {
        if (portalnum >= 0)
        {
            bgGetPortalPlane(portalnum, &plane);
            plane.min *= bgGetLevelScaleInv();

            dist = (pad->pos.f[0] * plane.normal.f[0]) + (pad->pos.f[1] * plane.normal.f[1]) + (pad->pos.f[2] * plane.normal.f[2]);

            if (g_DoorScale < 1.0f)
            {
                dist = (dist - plane.min) * (1.0f - g_DoorScale);
                newpos.f[0] = pad->pos.f[0] - (plane.normal.f[0] * dist);
                newpos.f[1] = pad->pos.f[1] - (plane.normal.f[1] * dist);
                newpos.f[2] = pad->pos.f[2] - (plane.normal.f[2] * dist);
            }
            else
            {
                dist = (dist - plane.min) * (g_DoorScale - 1.0f);
                newpos.f[0] = pad->pos.f[0] + (plane.normal.f[0] * dist);
                newpos.f[1] = pad->pos.f[1] + (plane.normal.f[1] * dist);
                newpos.f[2] = pad->pos.f[2] + (plane.normal.f[2] * dist);
            }

            tile = pad->stan;
            if (stanWalkLineNoCallback(&tile, pad->pos.f[0], pad->pos.f[2], newpos.f[0], newpos.f[2]) != 0)
            {
                pad->stan = tile;
                pad->pos.f[0] = newpos.f[0];
                pad->pos.f[1] = newpos.f[1];
                pad->pos.f[2] = newpos.f[2];
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

    if (getposstan(&pad->pos, pad->stan, 0.0f, &floorpos, &floortile) != 0)
    {
        mtx4LoadFacing(&padmtx, 0, 0, 0, -pad->look.f[0], -pad->look.f[1], -pad->look.f[2], pad->up.f[0], pad->up.f[1], pad->up.f[2]);
        modelfile = PitemZ_entries[modelnum].header;
        postile = floortile;

        bb2.zmax = pad->bbox.xmin;
        bb2.zmin = pad->bbox.xmax; //78
        bb2.ymax = pad->bbox.ymin; //74
        bb2.ymin = pad->bbox.ymax; //70
        bb2.xmax = pad->bbox.zmin; //6c
        bb2.xmin = pad->bbox.zmax; //68

        mtx4LoadXRotation(M_HALF_PI, &mtx);
        mtx4LoadZRotation(M_HALF_PI, &rotz);
        mtx4MultInPlace(&rotz, &mtx);
        mtx4MultInPlace(&padmtx, &mtx);
        padGetBboxCentre(pad, &pos);

        modelbbox = (struct ModelRoData_BoundingBoxRecord *)modelfile->RootNode->Child->Data;

        xscale = (bb2.ymin - bb2.ymax) / (modelbbox->Bounds.xmax - modelbbox->Bounds.xmin);
        yscale = (bb2.xmin - bb2.xmax) / (modelbbox->Bounds.ymax - modelbbox->Bounds.ymin);
        zscale = (bb2.zmin - bb2.zmax) / (modelbbox->Bounds.zmax - modelbbox->Bounds.zmin);

        if ((xscale <= 0.000001f) || (yscale <= 0.000001f) || (zscale <= 0.000001f))
        {
            xscale =
                yscale =
                zscale = 1.0f;
        }

        mtx4ScaleRow0(xscale, mtx.m[0]);
        mtx4ScaleRow1(yscale, mtx.m[0]);
        mtx4ScaleRow2(zscale, mtx.m[0]);

        centre.f[0] = pos.f[0];
        centre.f[1] = pos.f[1];
        centre.f[2] = pos.f[2];

        if (!(door->flags2 & 1))
        {
            if (stanWalkLineNoCallback(&postile, floorpos.f[0], floorpos.f[2], pos.f[0], pos.f[2]) != 0)
            {
                floortile = postile;
            }
            else
            {
                pos.f[0] = floorpos.f[0];
                pos.f[2] = floorpos.f[2];

                if (!(door->flags & 0x1000)) // prop flag PROPFLAG_00001000 "Absolute Position"
                {

                }
            }
        }
        else
        {
            pos.f[0] = floorpos.f[0];
            pos.f[1] = floorpos.f[1];
            pos.f[2] = floorpos.f[2];
        }

        if ((door->doorType == DOORTYPE_VERTICAL) || (door->doorType == DOORTYPE_FALLAWAY))
        {
            extent.f[0] = pad->look.f[0] * (bb2.xmin - bb2.xmax);
            extent.f[1] = pad->look.f[1] * (bb2.xmin - bb2.xmax);
            extent.f[2] = pad->look.f[2] * (bb2.xmin - bb2.xmax);
        }
        else
        {
            extent.f[0] = pad->up.f[0] * (bb2.ymax - bb2.ymin);
            extent.f[1] = pad->up.f[1] * (bb2.ymax - bb2.ymin);
            extent.f[2] = pad->up.f[2] * (bb2.ymax - bb2.ymin);
        }

        // These values are stored in the setup files as integers, but at
		// runtime they are floats. Hence reading a "float" as an integer,
		// converting it to a float and writing it back to the same property.
		door->maxFrac = *(s32 *) &door->maxFrac / 65536.0f;
		door->perimFrac = *(s32 *) &door->perimFrac / 65536.0f;
#if defined(VERSION_EU)
        door->accel = (*(s32 *) &door->accel) * 1.2f / 65536.0f;
		door->decel = (*(s32 *) &door->decel) * 1.2f / 65536.0f;
		door->maxSpeed = (*(s32 *) &door->maxSpeed) * 1.2f / 65536.0f;
#else
		door->accel = (*(s32 *) &door->accel) / 65536.0f;
		door->decel = (*(s32 *) &door->decel) / 65536.0f;
		door->maxSpeed = (*(s32 *) &door->maxSpeed) / 65536.0f;
#endif

        prop = doorInit(door, &pos, &mtx, floortile, &extent, &centre);
        if (door->flags & PROPFLAG_CULL_BEHIND_DOOR)
        {
            door->portalNumber = portalnum;
            if ((portalnum >= 0) && (door->openPosition == 0.0f))
            {
                doorDeactivatePortal(door);
            }
        }

        prop->rooms[0] = prop->stan->room;
        propAddToRoom(prop, prop->stan->room);
        prop->rooms[1] = 0xFFU;
        prop->rooms[2] = 0xFFU;

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

            if (prop->rooms[1] != 0xff && 1)
            {
                if (!prop->stan->room)
                {
                }

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
            door->linkedDoor = (struct DoorRecord *)setupGetPtrToCommandByIndex(door->linkedDoorOffset + cmdindex);
        }
    }
    else
    {
        door->prop = NULL;
    }

}


// Perfect Dark void setupLoadFiles(s32 stagenum)
void setupCreateProps(enum LEVELID stageId)
{
    ItemModelFileRecord *pitem;
    s32 withchrs;
    s32 withobjs;

    withchrs = (((void *) tokenFind(1, "-nochr")) == NULL) && (((void *) tokenFind(1, "-noprop")) == NULL);
    withobjs = (((void *) tokenFind(1, "-noobj")) == NULL) && (((void *) tokenFind(1, "-noprop")) == NULL);

    g_DoorScale = 1.0f;

    /**
     * Mark every prop model as "not resident" so the model loads later in this function actually fetch data. Essentially
     * RootNode is doubling as a loaded flag for function modelLoad().
     * 
     * The last entry in the PitemZ_entries table is a terminator which is why 1 is subtracted from the loop length.
     */
    for (pitem = PitemZ_entries; pitem < &PitemZ_entries[ARRAYCOUNT(PitemZ_entries) - 1]; pitem++)
    {
        pitem->header->RootNode = NULL;
    }

    if ((stageId <= (LEVELID_MAX + 1)) && setup_text_pointers[stageId])
    {
        char strResource[0x100] = ""; // Scratch buffer for synthesizing the setup file's name at runtime.
        s32 numAnimatedObjects = 0;
        s32 numObjects = 0;
        s32 i1 = 0;
        s32 i2 = 0;
        s32 i3 = 0;
        f32 roompos_1;
        s32 i5 = 0;
        s32 i8;
        f32 roompos_2;
        struct stagesetup *local_stage;

        strResource[0] = setup_text_pointers[stageId][0]; // 'U' -> "U"
        strResource[1] = 0; // Terminate so strcat has a valid string.

        /**
         * There are no slots for the mp stages in setup_text_pointers. The name is created
         * by adding "mp_" after the "U" e.g. "Ump_setuparchZ"
         */
        if (isMpGame())
        {
            strcat(strResource, "mp_"); // -> "Ump_"
        }

        strcat(strResource, setup_text_pointers[stageId] + 1); // Add remaining text back U[mp_] + setupxxxZ

        g_ptrStageSetupFile = fileLoadByNameToPool(strResource, FILELOAD_TRIM, 256, MEMPOOL_STAGE);

        local_stage = g_ptrStageSetupFile;
        langLoadToAddr(langGetBankForStage(stageId));

        /**
         * The setup file stores every internal reference as a byte offset from the start of the file,
         * so rebase them all onto the RAM copy at local_stage.
         */
        g_CurrentSetup.pathwaypoints = (void *) (((u32) local_stage) + ((u32) local_stage->pathwaypoints));
        g_CurrentSetup.waypointgroups = (void *) (((u32) local_stage) + ((u32) local_stage->waypointgroups));
        g_CurrentSetup.intro = (void *) (((u32) local_stage) + ((u32) local_stage->intro));
        g_CurrentSetup.propDefs = (void *) (((u32) local_stage) + ((u32) local_stage->propDefs));
        g_CurrentSetup.patrolpaths = (void *) (((u32) local_stage) + ((u32) local_stage->patrolpaths));
        g_CurrentSetup.ailists = (void *) (((u32) local_stage) + ((u32) local_stage->ailists));
        g_CurrentSetup.pads = (void *) (((u32) local_stage) + ((u32) local_stage->pads));
        g_CurrentSetup.boundpads = (void *) (((u32) local_stage) + ((u32) local_stage->boundpads));

        // Pad names and bound names are optional. An offset of 0 means absent.
        if (local_stage->padnames != 0)
        {
            g_CurrentSetup.padnames = (void *) (((u32) local_stage) + ((u32) local_stage->padnames));
        }
        else
        {
            g_CurrentSetup.padnames = NULL;
        }

        if (local_stage->boundpadnames != 0)
        {
            g_CurrentSetup.boundpadnames = (void *) (((u32) local_stage) + ((u32) local_stage->boundpadnames));
        }
        else
        {
            g_CurrentSetup.boundpadnames = NULL;
        }

        if (g_CurrentSetup.pathwaypoints)
        {
            for (i1 = 0; g_CurrentSetup.pathwaypoints[i1].padID >= 0; i1++)
            {
                g_CurrentSetup.pathwaypoints[i1].neighbours = (void *) (((u32) g_CurrentSetup.pathwaypoints[i1].neighbours) + ((u32) local_stage));
            }
        }

        if (g_CurrentSetup.waypointgroups)
        {
            for (i2 = 0; g_CurrentSetup.waypointgroups[i2].neighbours; i2++)
            {
                g_CurrentSetup.waypointgroups[i2].neighbours = (void *) (((u32) g_CurrentSetup.waypointgroups[i2].neighbours) + ((u32) local_stage));
                g_CurrentSetup.waypointgroups[i2].waypoints = (void *) (((u32) g_CurrentSetup.waypointgroups[i2].waypoints) + ((u32) local_stage));
            }
        }

        // Convert ailist pointers a.k.a. Action Blocks from file-local to proper pointers
        {
            AIListRecord *ailists = g_CurrentSetup.ailists;
            if (ailists)
            {
                for (i3 = 0; g_CurrentSetup.ailists[i3].ailist != 0; i3++)
                {
                    g_CurrentSetup.ailists[i3].ailist = (void *) (((u32) g_CurrentSetup.ailists[i3].ailist) + ((u32) local_stage));
                }
            }
        }

        if (g_CurrentSetup.patrolpaths)
        {
            for (i3 = 0; g_CurrentSetup.patrolpaths[i3].waypoints != NULL; i3++)
            {
                g_CurrentSetup.patrolpaths[i3].waypoints = (void *) (((u32) g_CurrentSetup.patrolpaths[i3].waypoints) + ((u32) local_stage));

                for (i5 = 0; g_CurrentSetup.patrolpaths[i3].waypoints[i5] >= 0; i5++)
                {
                    // Empty
                }

                g_CurrentSetup.patrolpaths[i3].len = i5;
            }
        }

        if (g_CurrentSetup.pads)
        {
            struct PadRecord *pad;
    
            roompos_1 = bgGetLevelScaleInv();
            pad = g_CurrentSetup.pads;

            for (; pad->plink != NULL; pad++)
            {
                pad->plink = (void *) (((u32) local_stage) + ((u32) pad->plink));
                pad->pos.f[0] *= roompos_1;
                pad->pos.f[1] *= roompos_1;
                pad->pos.f[2] *= roompos_1;
        
                stanResolvePadTile(pad, pad->plink, &pad->stan);

            }
        }

        if (g_CurrentSetup.boundpads)
        {
            struct BoundPadRecord *vol;

            roompos_2 = bgGetLevelScaleInv();
            vol = g_CurrentSetup.boundpads;
            
            for (; vol->plink != NULL; vol++)
            {
                /** Ugly matching hack. 
                *   TODO: investigate if there's a way to get rid of this.
                */

                vol->plink = (void *) (((u32) local_stage) + ((u32)vol->plink));
                vol->pos.f[0] *= roompos_2;
                vol->pos.f[1] *= roompos_2;
                vol->pos.f[2] *= roompos_2;
                vol->bbox.xmin *= roompos_2;
                vol->bbox.xmax *= roompos_2;
                vol->bbox.ymin *= roompos_2;
                vol->bbox.ymax *= roompos_2;
                vol->bbox.zmin *= roompos_2;
                vol->bbox.zmax *= roompos_2;

                stanResolvePadTile((struct PadRecord *) vol, vol->plink, &vol->stan);

            }
        }

        if (g_CurrentSetup.padnames)
        {
            for (i1 = 0; g_CurrentSetup.padnames[i1].p; i1++)
            {
                g_CurrentSetup.padnames[i1].p = (void *) (((u32) g_CurrentSetup.padnames[i1].p) + ((u32) local_stage));
            }
        }

        if (g_CurrentSetup.boundpadnames)
        {

            for (i1 = 0; g_CurrentSetup.boundpadnames[i1].p; i1++)
            {
                g_CurrentSetup.boundpadnames[i1].p = (void *) (((u32) g_CurrentSetup.boundpadnames[i1].p) + ((u32) local_stage));
            }
        }

        // PD rejoins here

        if (withchrs)
        {
            chrsAllocateSlots(setupCountPropdefs(PROPDEF_GUARD));
            numAnimatedObjects += setupCountPropdefs(PROPDEF_GUARD);
            numObjects += setupCountPropdefs(PROPDEF_COLLECTABLE);
            numObjects += setupCountPropdefs(PROPDEF_KEY);
            numObjects += setupCountPropdefs(PROPDEF_HAT);
        }
        else
        {
            chrsAllocateSlots(0); // chrmgrConfigure
        }

        if (withobjs)
        {
            // load std props for all stages
            numObjects += setupCountPropdefs(PROPDEF_DOOR);
            numObjects += setupCountPropdefs(PROPDEF_CCTV);
            numObjects += setupCountPropdefs(PROPDEF_AUTOGUN);
            numObjects += setupCountPropdefs(PROPDEF_RACK);
            numObjects += setupCountPropdefs(PROPDEF_MONITOR);
            numObjects += setupCountPropdefs(PROPDEF_MULTI_MONITOR);
            numObjects += setupCountPropdefs(PROPDEF_ARMOUR);
            numObjects += setupCountPropdefs(PROPDEF_PROP);
            numObjects += setupCountPropdefs(PROPDEF_GLASS);
            numObjects += setupCountPropdefs(PROPDEF_TINTED_GLASS);
            numObjects += setupCountPropdefs(PROPDEF_SAFE);
            numObjects += setupCountPropdefs(PROPDEF_UNK41);
            numObjects += setupCountPropdefs(PROPDEF_GAS_RELEASING);
            numObjects += setupCountPropdefs(PROPDEF_ALARM);
            numObjects += setupCountPropdefs(PROPDEF_MAGAZINE);
            numObjects += setupCountPropdefs(PROPDEF_AMMO);
            numObjects += setupCountPropdefs(PROPDEF_VEHICHLE);
            numObjects += setupCountPropdefs(PROPDEF_TANK);
            numAnimatedObjects += setupCountPropdefs(PROPDEF_AIRCRAFT);
        }

        modelmgrAllocateModelSlots(numObjects);
        modelmgrAllocateAnimModelSlots(numAnimatedObjects);

        for (i8 = 0; i8 < getPlayerCount(); i8++)
        {
            playerSetCurrent(i8);
            invAllocate(setupCountPropdefs(PROPDEF_LINK));
        }

        if (g_CurrentSetup.propDefs)
        {
            PropDefHeaderRecord *phead;
            s32 flags;
            s32 pdefIndex;

            // per-difficulty "Don't Load" mask: PROPFLAG2_00000010/20/40 for Agent/Secret/00
            flags = 1 << (lvGetDifficulty() + 4);

            /**
             * Complete the skip loading mask started on the line above. Checks for:
             * - don't load on 2 players
             * - don't load on 3 players
             * - don't load on 4 players
             * - don't load in multiplayer
             */
            if (isMpGame())
            {
                flags |= 1 << (getPlayerCount() + 20);
            }

            phead = g_CurrentSetup.propDefs;
            pdefIndex = 0;

            while ((i1 = phead->type) != PROPDEF_END)
            {
                switch (phead->type)
                {
                    case PROPDEF_GUARD_ATTRIBUTE:
                    {
                        GuardAttributeRecord *pdef_guarda;
                        u8 prob;
                        ChrRecord *chr;
                        pdef_guarda = (GuardAttributeRecord *) phead;
                        prob = (u8) pdef_guarda->GrenadeProb;
                        chr = chrFindByLiteralId(pdef_guarda->chrnum);
                        if ((chr && chr->prop) && chr->model)
                        {
                            chr->grenadeprob = prob;
                        }
                        break;
                    }
                    case PROPDEF_GUARD:
                        if (withchrs)
                        {
                            setupGuard(stageId, (struct GuardRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_DOOR:
                        if (withobjs && (!(((struct DoorRecord *) phead)->flags2 & flags)))
                        {
                            setupDoor(stageId, (struct DoorRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_DOOR_SCALE:
                        g_DoorScale = ((struct GlobalDoorScaleRecord *) phead)->Scale / M_U16_MAX_VALUE_F;
                        break;
                    case PROPDEF_COLLECTABLE:
                        if (withchrs && (!(((struct WeaponObjRecord *) phead)->flags2 & flags)))
                        {
                            setupWeapon(stageId, (struct WeaponObjRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_KEY:
                        if (withchrs && (!(((struct KeyRecord *) phead)->flags2 & flags)))
                        {
                            setupKey(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_HAT:
                        if (withchrs && (!(((struct ObjectRecord *) phead)->flags2 & flags)))
                        {
                            setupHat(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_CCTV:
                        if (withobjs && (!(((struct CCTVRecord *) phead)->flags2 & flags)))
                        {
                            setupCctv(stageId, (struct CCTVRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_AUTOGUN:
                        if (withobjs && (!(((struct AutogunRecord *) phead)->flags2 & flags)))
                        {
                            setupAutogun(stageId, (struct AutogunRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_RACK:
                        if (withobjs && (!(((struct ObjectRecord *) phead)->flags2 & flags)))
                        {
                            setupHangingMonitors(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_MONITOR:
                        if (withobjs && (!(((struct MonitorObjRecord *) phead)->flags2 & flags)))
                        {
                            setupSingleMonitor(stageId, (struct MonitorObjRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_MULTI_MONITOR:
                        if (withobjs && (!(((struct MultiMonitorObjRecord *) phead)->flags2 & flags)))
                        {
                            setupMultiMonitor(stageId, (struct MultiMonitorObjRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_ARMOUR:
                    {
                        struct BodyArmourRecord *pdef_ba = (struct BodyArmourRecord *) phead;
                        if (withobjs && ((pdef_ba->flags2 & flags) == 0))
                        {
                            pdef_ba->initialamount = (*((s32 *) (&pdef_ba->initialamount))) / M_U16_MAX_VALUE_F;
                            pdef_ba->amount = pdef_ba->initialamount;
                            setupObject(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    }
                    case PROPDEF_TINTED_GLASS:
                    {
                        if (withobjs && (!(((struct TintedGlassRecord *) phead)->flags2 & flags)))
                        {
                            if (((struct TintedGlassRecord *) phead)->flags & PROPFLAG_GLASS_HASPORTAL)
                            {
                                if (!(((struct TintedGlassRecord *) phead)->pad < 10000))
                                {
                                    struct coord3d up;
                                    struct coord3d up2;
                                    BoundPadRecord *pad3d;

                                    pad3d = &g_CurrentSetup.boundpads[((struct TintedGlassRecord *) phead)->pad - 10000];
                                    padGetBboxCentre(pad3d, &up);
                                    up2.x = (10.0f * pad3d->up.x) + up.x;
                                    up2.y = (10.0f * pad3d->up.y) + up.y;
                                    up2.z = (10.0f * pad3d->up.z) + up.z;
                                    up.x -= 10.0f * pad3d->up.x;
                                    up.y -= 10.0f * pad3d->up.y;
                                    up.z -= 10.0f * pad3d->up.z;

                                    ((struct TintedGlassRecord *) phead)->portalnum = bgFindPortalOnSegment(&up, &up2);
                                    ((struct TintedGlassRecord *) phead)->minOpacity = (*((s32 *) (&((struct TintedGlassRecord *) phead)->minOpacity))) / M_U16_MAX_VALUE_F;
                                }
                            }
                            setupObject(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    }
                    case PROPDEF_PROP:
                    case PROPDEF_ALARM:
                    case PROPDEF_MAGAZINE:
                    case PROPDEF_GAS_RELEASING:
                    case PROPDEF_UNK41:
                    case PROPDEF_GLASS:
                    case PROPDEF_SAFE:
                        if (withobjs && (!(((ObjectRecord *) phead)->flags2 & flags)))
                        {
                            setupObject(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_AMMO:
                    {
                        struct MultiAmmoCrateRecord *pdef_macr = (struct MultiAmmoCrateRecord *) phead;
                        s32 ammoqty = 1;
                        s32 i9;

                        if (isMpGame())
                        {
                            struct s_mp_weapon_set *mpweapon = &mpWeaponSetGetItems()[lastmpweaponnum];
                            
                            ammoqty = mpweapon->ammoamount;
                            pdef_macr->slots[mpweapon->ammotype - 1].quantity = ammoqty;
                        }

                        if (((ammoqty > 0) && withobjs) && (!(pdef_macr->flags2 & flags)))
                        {
                            for (i9 = 0; i9 < AMMOTYPE_GLOBAL_MAX; i9++)
                            {
                                if ((pdef_macr->slots[i9].quantity > 0) && (pdef_macr->slots[i9].modelnum != 0xFFFF))
                                {
                                    modelLoad(pdef_macr->slots[i9].modelnum);
                                }
                            }

                            setupObject(stageId, (struct ObjectRecord *) pdef_macr, pdefIndex);
                        }
                        break;
                    }
                    case PROPDEF_TANK:
                        if (withobjs && (!(((struct TankRecord *) phead)->flags2 & flags)))
                        {
                            struct TankRecord *pdef_tank = (struct TankRecord *) phead;
                            struct PropRecord *tank_prop;

                            f32 stan_y = 0.0f;

                            weaponLoadProjectileModels(ITEM_TANKSHELLS);
                            setupObject(stageId, (struct ObjectRecord *) pdef_tank, pdefIndex);
                            pdef_tank->turret_vertical_angle = 0.0f;
                            pdef_tank->turret_orientation_angle = 0.0f;
                            pdef_tank->tank_orientation_angle = M_TAU_F - mathAtan2(pdef_tank->mtx.m[2][0], pdef_tank->mtx.m[2][2]);
                            tank_prop = pdef_tank->prop;

                            if (tank_prop)
                            {
                                stan_y = stanGetPositionYValue(tank_prop->stan, tank_prop->pos.f[0], tank_prop->pos.f[2]);
                            }

                            pdef_tank->stan_y = stan_y;
#ifdef VERSION_EU
                            pdef_tank->smoothYSum = stan_y / 0.2004f; // EU-tuned constant
#else
                            pdef_tank->smoothYSum = stan_y / 0.17000002f;
#endif
                        }
                        break;
                    case PROPDEF_VEHICHLE:
                        if (withobjs && (!(((struct VehichleRecord *) phead)->flags2 & flags)))
                        {
                            struct VehichleRecord *pdef_veh = (struct VehichleRecord *) phead;

                            setupObject(stageId, (struct ObjectRecord *) pdef_veh, pdefIndex);

                            if (pdef_veh->model != NULL)
                            {
                                if (pdef_veh->model->obj->Switches[5] != NULL)
                                {
                                    modelGetNodeRwData(pdef_veh->model, pdef_veh->model->obj->Switches[5])->Switch.visible = (pdef_veh->flags & 0x10000000) == 0;
                                }
                            }

                            pdef_veh->speed        = 0.0f;
                            pdef_veh->wheelxrot    = 0.0f;
                            pdef_veh->wheelyrot    = 0.0f;
                            pdef_veh->speedaim     = 0.0f;
                            pdef_veh->turnrot60    = 0.0f;
                            pdef_veh->roty         = 0.0f;
                            pdef_veh->speedtime60  = -1.0f;
                            pdef_veh->ailist       = ailistFindById(pdef_veh->ailist);
                            pdef_veh->aioffset     = 0;
                            pdef_veh->aireturnlist = -1;
                            pdef_veh->path         = 0;
                            pdef_veh->nextstep     = 0;
                            pdef_veh->Sound        = 0;
                        }
                        break;
                    case PROPDEF_AIRCRAFT:
                        if (withobjs && (!(((struct AircraftRecord *) phead)->flags2 & flags)))
                        {
                            struct AircraftRecord *pdef_air = (struct AircraftRecord *) phead;

                            setupObject(stageId, (struct ObjectRecord *) pdef_air, pdefIndex);
                            pdef_air->speed           = 0.0f;
                            pdef_air->speedaim        = 0.0f;
                            pdef_air->rotoryrot       = 0.0f;
                            pdef_air->rotaryspeed     = 0.0f;
                            pdef_air->rotaryspeedaim  = 0.0f;
                            pdef_air->yrot            = 0.0f;
                            pdef_air->speedtime60     = -1.0f;
                            pdef_air->rotaryspeedtime = -1.0f;
                            pdef_air->ailist          = ailistFindById(pdef_air->ailist);
                            pdef_air->aioffset        = 0;
                            pdef_air->aireturnlist    = -1;
                            pdef_air->nextstep        = 0;
                            pdef_air->path            = 0;
                            pdef_air->Sound           = 0;
                        }
                        break;
                    case PROPDEF_TAG:
                    {
                        struct TagObjectRecord *pdef_tag;
                        struct ObjectRecord *taggedobj;

                        pdef_tag = (struct TagObjectRecord *) phead;
                        taggedobj = setupCommandGetObject(stageId, pdefIndex + ((s32) pdef_tag->OffsetToObj));
                        pdef_tag->TaggedObject = taggedobj;

                        if (taggedobj)
                        {
                            taggedobj->runtime_bitflags |= RUNTIMEBITFLAG_TAGGED;
                        }

                        tagAdd(pdef_tag);
                        break;
                    }
                    case PROPDEF_RENAME:
                    {
                        struct RenameObjectRecord *pdef_ren;
                        struct ObjectRecord *targetobj;

                        pdef_ren = (struct RenameObjectRecord *) phead;
                        i3 = pdef_ren->TagID + pdefIndex;
                        targetobj = setupCommandGetObject(stageId, i3);
                        pdef_ren->renobj = targetobj;

                        if (targetobj)
                        {
                            targetobj->runtime_bitflags |= RUNTIMEBITFLAG_DESTROYED;
                        }

                        invAddTextOverride((struct textoverride *) pdef_ren);
                        break;
                    }
                    case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
                        briefingTextAdd((struct setup_objective_text *) phead);
                        break;
                    case PROPDEF_CAMERAPOS:
                    {
                        struct CutsceneRecord *pdef_cam = (struct CutsceneRecord *) phead;

                        pdef_cam->pos.f[0] = (*((s32 *) (&pdef_cam->pos.f[0]))) / 100.0f;
                        pdef_cam->pos.f[1] = (*((s32 *) (&pdef_cam->pos.f[1]))) / 100.0f;
                        pdef_cam->pos.f[2] = (*((s32 *) (&pdef_cam->pos.f[2]))) / 100.0f;
                        pdef_cam->theta = (*((s32 *) (&pdef_cam->theta))) / M_U16_MAX_VALUE_F;
                        pdef_cam->verta = (*((s32 *) (&pdef_cam->verta))) / M_U16_MAX_VALUE_F;
                        break;
                    }
                    case PROPDEF_OBJECTIVE_START:
                        objectiveAdd((struct objective_entry *) phead);
                        break;
                    case PROPDEF_OBJECTIVE_ENTER_ROOM:
                        objectiveAddRoomEntered((struct criteria_roomentered *) phead);
                        break;
                    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
                        objectiveAddDeposit((struct criteria_deposit *) phead);
                        break;
                    case PROPDEF_OBJECTIVE_PHOTOGRAPH:
                        objectiveAddPicture((struct criteria_picture *) phead);
                        break;
                }

                phead = (PropDefHeaderRecord *) (((u32 *) phead) + sizepropdef(phead));
                pdefIndex++;
            }

            phead = g_CurrentSetup.propDefs;
            pdefIndex = 0;

            while (phead->type != PROPDEF_END)
            {
                switch (phead->type)
                {
                    case PROPDEF_PROP:
                    case PROPDEF_KEY:
                    case PROPDEF_MAGAZINE:
                    case PROPDEF_COLLECTABLE:
                    case PROPDEF_MONITOR:
                    case PROPDEF_AMMO:
                    case PROPDEF_ARMOUR:
                    case PROPDEF_GAS_RELEASING:
                    case PROPDEF_UNK41:
                    case PROPDEF_GLASS:
                    case PROPDEF_SAFE:
                    case PROPDEF_TINTED_GLASS:
                    {
                        struct ObjectRecord *pdef_obj = (struct ObjectRecord *) phead;

                        if (pdef_obj->prop && (pdef_obj->flags & PROPFLAG_INSIDEANOTHEROBJ))
                        {
                            u32 offset = pdef_obj->pad;
                            struct ObjectRecord *inobj = setupCommandGetObject(stageId, offset + pdefIndex);

                            if (inobj && inobj->prop)
                            {
                                pdef_obj->runtime_bitflags |= RUNTIMEBITFLAG_HASOWNER;
                                modelSetScale(pdef_obj->model, pdef_obj->model->scale);
                                propReparent(pdef_obj->prop, inobj->prop);
                            }


                        }
                        break;
                    }
                    case PROPDEF_LINK:
                    {
                        struct LinkRecord *pdef_link = (struct LinkRecord *) phead;
                        struct WeaponObjRecord *guna = (struct WeaponObjRecord *) setupGetPtrToCommandByIndex(pdef_link->Index1 + pdefIndex);
                        struct WeaponObjRecord *gunb = (struct WeaponObjRecord *) setupGetPtrToCommandByIndex(pdef_link->Index2 + pdefIndex);

                        if (guna && gunb)
                        {
                            if ((guna->type == PROPDEF_COLLECTABLE) && (gunb->type == PROPDEF_COLLECTABLE))
                            {
                                propweaponSetDual(guna, gunb);
                            }

                        }

                        break;
                    }
                    case PROPDEF_SWITCH:
                    {
                        struct LinkRecord *pdef_switch;
                        struct ObjectRecord *doorA;
                        struct ObjectRecord *doorB;
                        s32 index1;
                        s32 index2;

                        pdef_switch = (struct LinkRecord *) phead;
                        index1 = pdef_switch->Index1;
                        index2 = pdef_switch->Index2;
                        doorA = (struct ObjectRecord *) setupCommandGetObject(stageId, pdefIndex + index1);
                        doorB = (struct ObjectRecord *) setupGetPtrToCommandByIndex(pdefIndex + index2);

                        if ((((doorA && doorA->prop) && doorB) && (doorB->type == PROPDEF_DOOR)) && doorB->prop)
                        {
                            pdef_switch->first = doorA->prop;
                            pdef_switch->second = doorB->prop;
                            setupLinkSwitch(pdef_switch);
                            doorA->runtime_bitflags |= RUNTIMEBITFLAG_00000001; // linked door
                        }


                        break;
                    }
                    case PROPDEF_SAFE_ITEM:
                    {
                        s32 index1;
                        struct SafeObjectRecord *pdef_safe;
                        s32 index2;
                        s32 index3;
                        struct ObjectRecord *safe_item;
                        struct SafeRecord *safe;
                        struct DoorRecord *door;

                        pdef_safe = (struct SafeObjectRecord *) phead;
                        index1 = pdef_safe->Index1;
                        index2 = pdef_safe->Index2;
                        index3 = pdef_safe->Index3;
                        safe_item = setupCommandGetObject(stageId, pdefIndex + index1);
                        safe = (struct SafeRecord *) setupCommandGetObject(stageId, pdefIndex + index2);
                        door = (struct DoorRecord *) setupCommandGetObject(stageId, pdefIndex + index3);

                        if (((((((safe_item && safe_item->prop) && safe) && safe->prop) && (safe->type == PROPDEF_SAFE)) && door) && door->prop) && (door->type == PROPDEF_DOOR))
                        {
                            pdef_safe->item = safe_item;
                            pdef_safe->safe = safe;
                            pdef_safe->door = door;
                            setupLinkSafeItem((struct ObjectRecord *) pdef_safe);
                            safe_item->flags2 |= PROPFLAG2_LINKEDTOSAFE;
                            door->flags2 |= PROPFLAG2_LINKEDTOSAFE;
                        }
                        break;
                    }
                    case PROPDEF_LOCK_DOOR:
                    {
                        struct LockDoorRecord *pdef_lock_door;
                        struct DoorRecord *door;
                        struct ObjectRecord *lock;
                        s32 index1;
                        s32 index2;

                        pdef_lock_door = (struct LockDoorRecord *) phead;

                        index1 = pdef_lock_door->Index1;
                        index2 = pdef_lock_door->Index2;

                        door = (struct DoorRecord *) setupCommandGetObject(stageId, pdefIndex + index1);
                        lock = setupCommandGetObject(stageId, pdefIndex + index2);

                        if ((((door && door->prop) && lock) && lock->prop) && (door->type == PROPDEF_DOOR))
                        {
                            pdef_lock_door->door = door;
                            pdef_lock_door->lock = lock;
                            setupLinkLockDoor(pdef_lock_door);
                            door->runtime_bitflags |= RUNTIMEBITFLAG_PADLOCKEDDOOR;
                        }
                        break;
                    }
                }

                phead = (PropDefHeaderRecord *) (((u32 *) phead) + sizepropdef(phead));
                pdefIndex += 1;
            }
        }
    }
    else
    {
        g_CurrentSetup.pathwaypoints = NULL;
        g_CurrentSetup.waypointgroups = NULL;
        g_CurrentSetup.intro = 0;
        g_CurrentSetup.propDefs = 0;
        g_CurrentSetup.patrolpaths = NULL;
        g_CurrentSetup.ailists = NULL;
        g_CurrentSetup.pads = NULL;
        g_CurrentSetup.boundpads = NULL;
        g_CurrentSetup.padnames = NULL;
        g_CurrentSetup.boundpadnames = NULL;
        chrsAllocateSlots(0);
        modelmgrAllocateModelSlots(0);
        modelmgrAllocateAnimModelSlots(0);
    }

    bgChrsInit();
}


MonitorRecord g_InitialMonitorAnimController = {&monAnim00Bond, 0, 0xFFFF, 0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.5, 0.0, 0.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 1.0, 0.0};
struct object_animation_controller g_InitialUnknownAnimController = {&monAnim34, 0, 0xFFFF, 0, 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.5, 0.0, 0.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1.0};
struct object_animation_controller g_InitialTaserAnimController = {&monAnim35Taser, 0, 0xFFFF, 0, 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.5, 0.0, 0.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1.0};

f32 g_DoorScale = 1.0f;

// forward declarations

void monitorsInitAnimations();
void proxminesInit();

// end forward declarations


void objSoundsInit(void)
{
    s32 i;

    for (i=0; i<SFX_RELATED_LEN; i++)
    {
        sfx_related[i].state = AL_STOPPED;
    }

}


void propsResetLevelState(void)
{
    s32 i;

    monitorsInitAnimations();
    proxminesInit();
    alarm_timer = 0;
    g_AlarmSound = 0;
    toxic_gas_sound_timer = 0.0f;
    activate_gas_sound_timer = 0;
    gasLeakSource.f[0] = 0.0f;
    gasLeakSource.f[1] = 0.0f;
    gasLeakSource.f[2] = 0.0f;
    g_GasLastCoughTime = 0;
    gasLeakTimer = 0.0f;
    g_GasSound = 0;
    clock_drawn_flag = 1;
    clock_enable = 0;
    clock_time = 0.0f;
    g_RemoteMineOwnerTriggerFlag = 0;

    for (i = 0; i < MAX_WEAPON_SLOTS; i++)
    {
        g_WeaponSlots[i].prop = NULL;
    }

    g_NextWeaponSlot = 0;

    for (i = 0; i < MAX_HAT_SLOTS; i++)
    {
        g_HatSlots[i].prop = NULL;
    }
    
    g_NextHatSlot = 0;

    for (i=0; i < MAX_AMMO_CRATES; i++)
    {
        g_AmmoCrates[i].prop = NULL;
    }

    for (i = 0; i < PROJECTILES_ARR_MAX; i++)
    {
        g_Projectiles[i].flags = 0x80000000;
        g_Projectiles[i].sounds[0] = NULL;
        g_Projectiles[i].sounds[1] = NULL;
    }

    for (i = 0; i < EMBEDMENT_ARR_MAX; i++)
    {
        g_Embedments[i].flags = 1;
    }

    g_LevelLoadPropSwitch = NULL;
    g_LevelLoadPropLockDoor = NULL;
    g_LevelLoadPropSafeItem = NULL;
    g_ProjectileHitProp = NULL;
    bodypartshot = -1;
    g_CctvAlarmTimeScale = 1.0f;
    g_CctvDamageScale = 1.0f;
    g_AutogunPendingDamageTick = 1.0f;
    g_AutogunDamageScalar = 1.0f;
    g_AutogunDamageScale = 1.0f;
    g_SoloAmmoMultiplier = 1.0f;
}

/**
 * Called from setupCreateProps when PROPDEF type is PROPDEF_SWITCH.
*/
void setupLinkSwitch(struct LinkRecord *link)
{
    link->next = g_LevelLoadPropSwitch;
    g_LevelLoadPropSwitch = link;
}


/**
 * Called from setupCreateProps when PROPDEF type is PROPDEF_LOCK_DOOR.
*/
void setupLinkLockDoor(struct LockDoorRecord *link)
{
    link->next = g_LevelLoadPropLockDoor;
    g_LevelLoadPropLockDoor = link;
}


/**
 * Called from setupCreateProps when PROPDEF type is PROPDEF_SAFE_ITEM.
*/
void setupLinkSafeItem(struct ObjectRecord *obj)
{
    obj->prop = g_LevelLoadPropSafeItem;
    g_LevelLoadPropSafeItem = obj;
}


void monitorsInitAnimations(void)
{
    g_MonitorAnimController = g_InitialMonitorAnimController;
    g_UnknownAnimController = g_InitialUnknownAnimController;
    g_TaserAnimController = g_InitialTaserAnimController;
}


void proxminesInit(void)
{
    s32 i;

    for (i=0; i<30; i++)
    {
        proxy_mine_table[i] = NULL;
    }
}


void objSoundsStopAll(void) {
    s32 i;

    for (i = 0; i < SFX_RELATED_LEN; i++)
    {
        if (sfx_related[i].state != NULL)
        {
            if (sndGetPlayingState(sfx_related[i].state) != AL_STOPPED)
            {
                sndDeactivate(sfx_related[i].state);
            }
        }
    }
}


void setupFreeProps(s32 stage)
{
    u32 *obj = (u32)g_CurrentSetup.propDefs;
    
    if (obj)
    {
        while ((u8)obj[0] != PROPDEF_END)
        {
            switch ((u8)obj[0])
            {
                case PROPDEF_DOOR:
                case PROPDEF_PROP:
                case PROPDEF_KEY:
                case PROPDEF_ALARM:
                case PROPDEF_CCTV:
                case PROPDEF_MAGAZINE:
                case PROPDEF_COLLECTABLE:
                case PROPDEF_MONITOR:
                case PROPDEF_MULTI_MONITOR:
                case PROPDEF_RACK:
                case PROPDEF_AUTOGUN:
                case PROPDEF_HAT:
                case PROPDEF_AMMO:
                case PROPDEF_ARMOUR:
                case PROPDEF_GAS_RELEASING:
                case PROPDEF_VEHICHLE:
                case PROPDEF_AIRCRAFT:
                case PROPDEF_UNK41:
                case PROPDEF_GLASS:
                case PROPDEF_SAFE:
                case PROPDEF_TANK:
                case PROPDEF_TINTED_GLASS:
                    objFreePermanently((ObjectRecord *)obj, 1);
                    break;
                case PROPDEF_DOOR_SCALE:
                case PROPDEF_GUARD:
                case PROPDEF_LINK:
                case PROPDEF_DEBRIS:
                case PROPDEF_UNK16:
                case PROPDEF_GUARD_ATTRIBUTE:
                case PROPDEF_SWITCH:
                case PROPDEF_TAG:
                case PROPDEF_OBJECTIVE_START:
                case PROPDEF_OBJECTIVE_END:
                case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
                case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
                case PROPDEF_OBJECTIVE_FAIL_CONDITION:
                case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
                case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
                case PROPDEF_OBJECTIVE_PHOTOGRAPH:
                case PROPDEF_OBJECTIVE_NULL:
                case PROPDEF_OBJECTIVE_ENTER_ROOM:
                case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
                case PROPDEF_OBJECTIVE_COPY_ITEM:
                case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
                case PROPDEF_RENAME:
                case PROPDEF_LOCK_DOOR:
                case PROPDEF_SAFE_ITEM:
                case PROPDEF_CAMERAPOS:
                
                    break;
                
            }

            obj =  obj + sizepropdef(obj) ;
        }
    }
}

/**
 * @brief getposstan
*/
s32 getposstan(struct coord3d *pos, StandTile *stan, f32 radius, struct coord3d *posReturn, StandTile **stanReturn)
{
    posReturn->f[0] = pos->f[0];
    posReturn->f[1] = pos->f[1];
    posReturn->f[2] = pos->f[2];

    *stanReturn = stan;

    if (stan == 0)
    {
        return 0;
    }

    if ((radius > 0.0f) && (stanTestVolume(stanReturn, posReturn->f[0], posReturn->f[2], radius, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f) >= 0))
    {
        return 0;
    }

    return 1;
}

//Todo: finish this func
/**
 * Get Size of Prop Definition
 * @param pdef:  Prop Defenition to get size of
 * @return Size of prop in Words (32bit)
*/
s32 sizepropdef(PropDefHeaderRecord *pdef)
{
    switch (pdef->type)
    {
        case PROPDEF_GUARD:
            return sizeof(GuardRecord) / 4;
        case PROPDEF_DOOR:
            return sizeof(DoorRecord) / 4;
        case PROPDEF_DOOR_SCALE:
            return sizeof(GlobalDoorScaleRecord) / 4;
        case PROPDEF_PROP:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_GLASS:
            return sizeof(ObjectRecord) / 4; // Rare typo?
        case PROPDEF_TINTED_GLASS:
            return sizeof(TintedGlassRecord) / 4;
        case PROPDEF_SAFE:
            return sizeof(ObjectRecord) / 4; // wrong
        case PROPDEF_GAS_RELEASING:
            return sizeof(ObjectRecord) / 4; // also wrong, gas is only 2 words
        case PROPDEF_KEY:
            return sizeof(KeyRecord) / 4;
        case PROPDEF_ALARM:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_CCTV:
            return 0x3b;//return sizeof(GlassRecord) / 4;
        case PROPDEF_MAGAZINE:
            return 0x21;//return sizeof(GlassRecord) / 4;
        case PROPDEF_COLLECTABLE:
            return 0x22;//return sizeof(GlassRecord) / 4;
        case PROPDEF_MONITOR:
            return 0x40;//return sizeof(GlassRecord) / 4;
        case PROPDEF_MULTI_MONITOR:
            return 0x95;//return sizeof(GlassRecord) / 4;
        case PROPDEF_RACK:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_AUTOGUN:
            return 0x36;//return sizeof(GlassRecord) / 4;
        case PROPDEF_LINK:
            return 3;//return sizeof(GlassRecord) / 4;
        case PROPDEF_HAT:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_GUARD_ATTRIBUTE:
            return 3;//return sizeof(GlassRecord) / 4;
        case PROPDEF_SWITCH:
            return 4;//return sizeof(GlassRecord) / 4;
        case PROPDEF_SAFE_ITEM:
            return 5;//return sizeof(GlassRecord) / 4;
        case PROPDEF_AMMO:
            return 0x2d;//return sizeof(GlassRecord) / 4;
        case PROPDEF_ARMOUR:
            return 0x22;//return sizeof(GlassRecord) / 4;
        case PROPDEF_TAG:
            return 4;//return sizeof(GlassRecord) / 4;
        case PROPDEF_RENAME:
            return 10;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_START:
            return 4;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_END:
            return 1;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
            return 2;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
            return 2;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_FAIL_CONDITION:
            return 2;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
            return 2;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
            return 2;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            return 4;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_NULL:
            return 1;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_ENTER_ROOM:
            return 4;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
            return 5;//return sizeof(GlassRecord) / 4;
        case PROPDEF_OBJECTIVE_COPY_ITEM:
            return 1;//return sizeof(GlassRecord) / 4;
        case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
            return 4;//return sizeof(GlassRecord) / 4;
        case PROPDEF_LOCK_DOOR:
            return 4;//return sizeof(GlassRecord) / 4;
        case PROPDEF_VEHICHLE:
            return 0x2c;//return sizeof(GlassRecord) / 4;
        case PROPDEF_AIRCRAFT:
            return 0x2d;//return sizeof(GlassRecord) / 4;
        case PROPDEF_TANK:
            return 0x38;//return sizeof(GlassRecord) / 4;
        case PROPDEF_CAMERAPOS:
            return 7;//return sizeof(GlassRecord) / 4;
        default:
            return sizeof(PropDefHeaderRecord) / 4;;
    }
}


/*
 *Return Item by Setup index
 *Setup Array is most likley PropDefHeaderRecord since size was 4
 */
ObjectRecord *setupGetPtrToCommandByIndex(s32 index) //#MATCH
{
    PropDefHeaderRecord *object = g_CurrentSetup.propDefs; //wow, first use of header, cool

    if (index >= 0 && object)
    {
        s32 i;
        for (i = 0; object->type != PROPDEF_END; i++)
        {
            if (i == index)
            {
                return object;
            }

            object = sizepropdef(object) + object; //This is correct order, using += swaps t7/s1
        }
    }

    return NULL;
}


s32 tagGetCommandIndex(struct ObjectRecord *tag)
{
    PropDefHeaderRecord *object;
    s32 i;

    object = g_CurrentSetup.propDefs;

    if (object != NULL)
    {
        for (i = 0; object->type != PROPDEF_END; i++)
        {
            if ((void*)object == (void*)tag)
            {
                return i;
            }

            object = sizepropdef(object) + object;
        }
    }

    return -1;
}


s32 setupGetCommandIndexByProp(struct PropRecord *prop)
{
    PropDefHeaderRecord *object;
    s32 i;

    object = g_CurrentSetup.propDefs;

    if (object != NULL)
    {
        for (i = 0; object->type != PROPDEF_END; i++)
        {
            if ((void*)((struct ObjectRecord *)object)->prop == (void*)prop)
            {
                return i;
            }

            object = sizepropdef(object) + object;
        }
    }

    return -1;
}


s32 modelLoad(s32 modelid)
{
    if (PitemZ_entries[modelid].header->RootNode == NULL)
    {
        modelFileLoadToPool(PitemZ_entries[modelid].header,PitemZ_entries[modelid].filename/*, "prop"*/);
        modelCalculateRwDataLen(PitemZ_entries[modelid].header);
        return TRUE;
    }
    return FALSE;
}


void setupUpdateObjectRoomPosition(ObjectRecord *obj)
{
    PropRecord *prop;
    struct coord3d min;
    struct coord3d max;
    struct ModelRoData_BoundingBoxRecord *bbox;
    f32 radius;

    prop = obj->prop;
    radius = 0.0f;
    propRemoveFromRooms(prop);

    if (obj->flags2 & 0x20000)
    {
        if (prop->stan != NULL)
        {
            prop->rooms[0] = prop->stan->room;
            prop->rooms[1] = (u8)-1;
        }
        else
        {
            prop->rooms[0] = (u8)-1;
        }
    }
    else
    {
        bbox = chrobjGetBboxFromObjectRecord(obj);

        if (bbox != NULL)
        {
            min.f[0] = bboxRotatedMinX(bbox, &obj->mtx) - 30.0f;
            min.f[1] = bboxRotatedMinY(bbox, &obj->mtx);
            min.f[2] = bboxRotatedMinZ(bbox, &obj->mtx) - 30.0f;

            max.f[0] = bboxRotatedMaxX(bbox, &obj->mtx) + 30.0f;
            max.f[1] = bboxRotatedMaxY(bbox, &obj->mtx);
            max.f[2] = bboxRotatedMaxZ(bbox, &obj->mtx) + 30.0f;

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


 ObjectRecord *setupCommandGetObject(s32 stageID, s32 index)
{
     PropDefHeaderRecord *obj;

    obj = setupGetPtrToCommandByIndex(index);

    if (obj != NULL)
    {
        switch (obj->type)
        {
            case PROPDEF_DOOR:    //1
            case PROPDEF_PROP: //3:
            case PROPDEF_KEY:  //4:
            case PROPDEF_ALARM: // 5:
            case PROPDEF_CCTV: //6:
            case PROPDEF_MAGAZINE: //7:
            case PROPDEF_COLLECTABLE: // 8:
            case PROPDEF_MONITOR: //10:
            case PROPDEF_MULTI_MONITOR:// 11:
            case PROPDEF_RACK: //12:
            case PROPDEF_AUTOGUN: //13:
            case PROPDEF_HAT: //17:
            case PROPDEF_AMMO: //20:
            case PROPDEF_ARMOUR: //21:
            case PROPDEF_GAS_RELEASING: //36:
            case PROPDEF_VEHICHLE: //39:
            case PROPDEF_AIRCRAFT: //40:
            case PROPDEF_UNK41: //41:
            case PROPDEF_GLASS: //42:
            case PROPDEF_SAFE: //43:
            case PROPDEF_TANK: //45:
            case PROPDEF_TINTED_GLASS:                 //47:
                return obj;
            break;

            case PROPDEF_DOOR_SCALE: //2
            case PROPDEF_GUARD:           //9 :
            case PROPDEF_LINK:            //14:
            case PROPDEF_GUARD_ATTRIBUTE: //18:
            case PROPDEF_SWITCH:          //19:
            case PROPDEF_TAG:             //22:
            case PROPDEF_OBJECTIVE_START: //23
            case PROPDEF_OBJECTIVE_END:
            case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
            case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
            case PROPDEF_OBJECTIVE_FAIL_CONDITION:
            case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
            case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
            case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            case PROPDEF_OBJECTIVE_NULL:
            case PROPDEF_OBJECTIVE_ENTER_ROOM:
            case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
            case PROPDEF_OBJECTIVE_COPY_ITEM:
            case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT: //35
            case PROPDEF_RENAME:                    //37:
            case PROPDEF_LOCK_DOOR:                 //38:
            case PROPDEF_SAFE_ITEM:                 //44:
            case PROPDEF_CAMERAPOS:                 //46:
                return NULL;
            break;

            // not included:
            //case PROPDEF_DEBRIS: //15:
            //case PROPDEF_UNK16: //16:
            // return ???
        }
    }
    return obj;
}


ObjectRecord *setupFindObjForReuse(s32 wanttype, ObjectRecord **offscreenobjptr, ObjectRecord **anyobjptr, bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef)
{
    ObjectRecord *offscreenobj = NULL;
    ObjectRecord *anyobj = NULL;

    u32 *cmd = g_CurrentSetup.propDefs;

    if (cmd)
    {
        while ((u8)cmd[0] != PROPDEF_END)
        {
            if ((wanttype & 0xff) == (u8)cmd[0])
            {
                ObjectRecord *obj = (ObjectRecord *)cmd;

                if (obj->prop == NULL)
                {
                    if (!musthaveprop && !musthavemodel)
                    {
                        return obj;
                    }
                }
                else if (wanttype != PROP_TYPE_SMOKE
                        && (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) == 0
                        && (obj->state & PROPSTATE_RESPAWN) == 0
                        && obj->prop->parent == NULL
                        && (!musthavemodel || modelmgrCanSlotFitRwdata(obj->model, modeldef)))
                {
                    if (offscreenobj == NULL && (obj->prop->flags & PROPFLAG_ONSCREEN) == 0)
                    {
                        offscreenobj = obj;
                    }

                    if (anyobj == NULL)
                    {
                        anyobj = obj;
                    }
                }
            }

            cmd = cmd + sizepropdef(cmd);
        }
    }

    *offscreenobjptr = offscreenobj;
    *anyobjptr = anyobj;

    return NULL;
}


void setupGuard(s32 stageid, GuardRecord *guard, s32 cmdindex)
{
    struct PadRecord *pad;
    struct ChrRecord *chr;
    struct StandTile *tile; // 84
    struct coord3d pos; // 72
    struct PropRecord *prop;
    struct ChrModelFileRecord *cmfr;
    f32 angle; // 60
    struct Model *model; //56
    s32 bodyid;
    s32 headid;

    pad = &g_CurrentSetup.pads[guard->PadID];

    if (getposstan(&pad->pos, pad->stan, 20.0f, &pos, &tile) != 0)
    {
        headid = -1;
        bodyid = (guard->BodyID == 0xFFFF)
            ? bodyGetRandom()
            : guard->BodyID;

        cmfr = &c_item_entries[bodyid];
        if (cmfr->hasHead == 0)
        {
            headid = (guard->HeadID >= 0)
                ? guard->HeadID
                : bodyChooseHead(bodyid);
        }

        model = bodyCreateModelWithFlags(bodyid, headid, (u32) guard->bitflags);

        if (model != 0)
        {
            angle = mathAtan2(pad->look.f[0], pad->look.f[2]);
            prop = chrAllocate(model, &pos, angle, tile, ailistFindById(guard->AIListID));

            if (prop != 0)
            {
                propActivate(prop);
                propEnable(prop);

                chr = prop->chr;
                chr->chrnum = (s16) guard->chrnum;
                chr->hearingscale = ((f32)guard->health) / 1000.0f;
                chr->visionrange = (f32)guard->ReactionTime;
                chr->padpreset1 = (s16) guard->Preset;
                chr->chrpreset1 = (s16) guard->chrpreset1;
                chr->headnum = (s8) headid;
                chr->bodynum = (s8) bodyid;

                if ((guard->bitflags & 4) != 0)
                {
                    chr->chrflags |= CHRFLAG_CLONE;
                }

                if ((guard->bitflags & 8) != 0)
                {
                    chr->chrflags |= CHRFLAG_INVINCIBLE;
                }

                guard->Data = chr;
            }
        }
    }
}
