/**
 * The per-frame tick of objects and doors.
 *
 * propsTick calls objTick for each object or door prop in the active list, on every
 * player's pass. The object's own behaviour (flying, doors opening, cameras, autoguns,
 * vehicles, aircraft) runs once a frame, on one pass: the "sim owner". That is the first
 * player in the frame's shuffled order, or for a projectile in flight the player who
 * threw it. The rest runs on every pass, as it is per view: whether the object is in
 * view, its model's matrices, and the opacity of tinted glass.
 *
 * Objects attached to another (a gun in a guard's hand, a knife stuck in a crate) are
 * ticked through their parent: objTickAttached when the parent is drawn, otherwise
 * objTickOffscreen.
 */

#include <ultra64.h>
#include <math.h>
#include <random.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/objtick.h"
#include "game/prop/autogun.h"
#include "game/prop/cctv.h"
#include "game/prop/door.h"
#include "game/prop/mine.h"
#include "game/prop/objdamage.h"
#include "game/prop/projectile.h"
#include "game/prop/prop.h"
#include "game/prop/propobj.h"
#include "game/prop/setup.h"
#include "game/prop/vehicle.h"
#include "game/gfx/dyn.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/model/chrobjdata.h"
#include "game/model/model.h"
#include "game/player/player.h"
#include "game/world/bg.h"

/* The rack model's four shelves */
#define RACK_NUM_SHELVES 4

/**
 * Work out how see-through tinted glass is from the current player: fully see-through
 * (minOpacity) within xludist, solid (255) beyond opadist, and in between in between.
 */
s32 glassCalculateOpacity(coord3d *pos, f32 xludist, f32 opadist, f32 minOpacity)
{
    coord3d *campos = &currentPlayerGetProp()->pos;
    f32 xdiff = pos->x - campos->x;
    f32 ydiff = pos->y - campos->y;
    f32 zdiff = pos->z - campos->z;
    f32 distance = sqrtf(xdiff * xdiff + ydiff * ydiff + zdiff * zdiff);

    if (distance > opadist)
    {
        return 255;
    }

    if (distance < xludist)
    {
        return minOpacity * 255;
    }

    return (((distance - xludist) * (1.0f - minOpacity)) / (opadist - xludist) + minOpacity) * 255;
}


/**
 * Work out the opacity of tinted glass and windowed doors. In solo, solid glass closes
 * its portal so the room behind isn't drawn. Returns FALSE if the object must be drawn
 * even in fog (glass is never fogged out).
 */
static bool objUpdateOpacity(PropRecord *prop, s32 playercount)
{
    ObjectRecord *obj = prop->obj;

    if (obj->type == PROPDEF_TINTED_GLASS)
    {
        TintedGlassRecord *glass = (TintedGlassRecord *)obj;

        glass->calculatedopacity = glassCalculateOpacity(&obj->runtime_pos, glass->TintDist, glass->CullDist, glass->minOpacity);

        if (glass->portalnum >= 0 && playercount == 1)
        {
            bgToggleDataPortalsContrlBytes1Bit1(glass->portalnum, glass->calculatedopacity != 0xFF);
        }

        return FALSE;
    }

    if (obj->type == PROPDEF_DOOR && (((DoorRecord *)obj)->doorFlags & DOORFLAG_WINDOWED))
    {
        DoorRecord *door = (DoorRecord *)obj;
        Model *model = obj->model;

        door->calculatedopacity = glassCalculateOpacity(&obj->runtime_pos, door->TintDist, door->CullDist, 0.0f);

        if (playercount == 1)
        {
            // Close the portal while the door is shut and solid
            bool closeportal = door->calculatedopacity == 0xFF && !(door->openPosition > 0.0f);

            if (model->obj->Skeleton == &skeleton_door && modelGetNodeRwData(model, model->obj->Switches[1])->Switch.visible == 0)
            {
                closeportal = FALSE;
            }

            if (closeportal)
            {
                doorClosePortal(door);
            }
            else
            {
                doorOpenPortal(door);
            }
        }
    }

    return TRUE;
}


/* Whether the object is drawn on this pass */
static bool objIsVisible(PropRecord *prop, bool applyfogcull)
{
    ObjectRecord *obj = prop->obj;

    // The tank the player is in, and objects flagged always visible
    if (obj->type == PROPDEF_TANK && playerGetTank() == prop)
    {
        return TRUE;
    }

    if (obj->flags2 & PROPFLAG2_04000000)
    {
        return TRUE;
    }

    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_RESPAWNING) || (obj->flags2 & PROPFLAG2_00080000))
    {
        return FALSE;
    }

    return posIsOnScreen(prop, &obj->runtime_pos, modelGetRadius(obj->model), applyfogcull);
}


/* The racks' shelves sit at their nodes */
static void rackGetModelMatrices(Model *model, Mtxf *mtxs)
{
    for (s32 i = 0; i < RACK_NUM_SHELVES; i++)
    {
        mtx4LoadTranslation(&model->obj->Switches[i]->Data->Group.Origin, &mtxs[i + 1]);
        mtx4MultAffineInPlace(&mtxs[0], &mtxs[i + 1]);
    }
}


/**
 * Work out the matrices of an object's model for this view, into mtxs. The root matrix
 * (mtxs[0]) places the object; the rest depend on the type.
 */
static void objGetModelMatrices(PropRecord *prop, Model *model, Mtxf *mtxs, f32 prevopenposition, bool issimowner)
{
    ObjectRecord *obj = prop->obj;
    Mtxf objmtx;

    if (obj->type == PROPDEF_DOOR)
    {
        doorGetModelMatrices(prop->door, model, mtxs, prevopenposition, issimowner);
        return;
    }

    mtx4Copy(&obj->mtx, &objmtx);
    mtx4SetTranslation(&obj->runtime_pos, &objmtx);
    mtx4MultAffine(camGetWorldToScreenMtxf(), &objmtx, mtxs);

    switch (obj->type)
    {
        case PROPDEF_CCTV:
            cctvGetModelMatrices(prop, model, &objmtx, mtxs);
            break;

        case PROPDEF_AUTOGUN:
            autogunGetModelMatrices(prop, model, &objmtx, mtxs);
            break;

        case PROPDEF_COLLECTABLE:
            for (s32 i = 1; i < model->obj->numMatrices; i++)
            {
                mtx4LoadIdentity(&mtxs[i]);
            }
            break;

        case PROPDEF_RACK:
            rackGetModelMatrices(model, mtxs);
            break;

        case PROPDEF_VEHICHLE:
            vehicleGetModelMatrices(prop, model, mtxs, issimowner);
            break;

        case PROPDEF_AIRCRAFT:
            aircraftGetModelMatrices(prop, model, mtxs, issimowner);
            break;

        case PROPDEF_TANK:
            tankGetModelMatrices(prop, model, mtxs);
            break;

        default:
            break;
    }
}


/**
 * Whether this pass runs the object's own behaviour (see the top of the file). In
 * multiplayer a projectile in flight is run by the player who threw it. (The original
 * game ran a projectile on its owner's pass whatever the owner was, so a guard's dropped
 * gun was never moved in multiplayer; only a player owner counts.)
 */
static bool objIsSimOwner(ObjectRecord *obj, s32 playercount)
{
    bool issimowner;

    if (playercount == 1)
    {
        return TRUE;
    }

    issimowner = playerGetShuffledIndex(playerGetCurrentNum()) == 0;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
    {
        Projectile *projectile = obj->projectile;

        if (projectile->ownerprop != NULL && playerGetIndexForProp(projectile->ownerprop) >= 0)
        {
            issimowner = projectile->ownerprop == g_CurrentPlayer->prop;
        }
    }

    return issimowner;
}


static void objTickChildren(PropRecord *prop, void (*tick)(PropRecord *child))
{
    PropRecord *child = propChild(prop);

    while (child != NULL)
    {
        PropRecord *prev = child->prev;

        tick(child);
        child = prev;
    }
}


/**
 * Tick an object or door. Returns what to do with its prop (TICKOP): TICKOP_FREE if it
 * was removed, TICKOP_RETICK to tick it again later this frame, or what its projectile
 * says.
 */
TICKOP objTick(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    Model *model = obj->model;
    bool moved = FALSE;
    TICKOP tickop = TICKOP_NONE;
    f32 prevopenposition = 0.0f;
    s32 playercount = getPlayerCount();
    bool issimowner;
    bool applyfogcull;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
    {
        objFree(obj, 0, obj->state & PROPSTATE_RESPAWN);
        return TICKOP_FREE;
    }

    // An object in flight is ticked last, after the props it might hit have moved
    if (obj->runtime_bitflags & RUNTIMEBITFLAG_ISRETICK)
    {
        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_ISRETICK;
    }
    else if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
    {
        prop->flags &= ~PROPFLAG_ONSCREEN;
        obj->runtime_bitflags |= RUNTIMEBITFLAG_ISRETICK;
        return TICKOP_RETICK;
    }

    issimowner = objIsSimOwner(obj, playercount);

    if (issimowner)
    {
        if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        {
            tickop = projectileTick(prop, &moved);
        }

        if (moved)
        {
            objUpdateCollisionBounds(obj);
            setupUpdateObjectRoomPosition(obj);
            objGetShadeColour(obj->prop, &obj->nextcol);
            proxminesDetonateNear(&obj->runtime_pos);
        }

        if (obj->type == PROPDEF_DOOR)
        {
            prevopenposition = prop->door->openPosition;
            doorTick(prop);
        }
        else if (obj->type == PROPDEF_CCTV && !(obj->flags & PROPFLAG_CCTV_DISABLED))
        {
            cctvTick(prop);
        }
        else if (obj->type == PROPDEF_AUTOGUN && !(obj->flags & PROPFLAG_AUTOGUN_DISABLED))
        {
            autogunTick(prop);
        }
        else if (obj->type == PROPDEF_VEHICHLE)
        {
            vehicleTick(prop);
        }
        else if (obj->type == PROPDEF_AIRCRAFT)
        {
            aircraftTick(prop);
        }
    }

    applyfogcull = objUpdateOpacity(prop, playercount);

    if (objIsVisible(prop, applyfogcull))
    {
        Mtxf *mtxs;

        if (issimowner)
        {
            objUpdateShading(&obj->shadecol, &obj->nextcol);
        }

        prop->flags |= PROPFLAG_ONSCREEN;
        mtxs = dynAllocate(model->obj->numMatrices * sizeof(Mtxf));
        model->render_pos = (RenderPosView *)mtxs;

        objGetModelMatrices(prop, model, mtxs, prevopenposition, issimowner);

        modelUpdateRelationsQuick(model, model->obj->RootNode);
        prop->zDepth = -((Mtxf *)model->render_pos)[0].m[3][2];

        objTickExplosive(prop);
        objTickChildren(prop, objTickAttached);
    }
    else
    {
        prop->flags &= ~PROPFLAG_ONSCREEN;
        objTickExplosive(prop);
        objTickChildren(prop, objTickOffscreen);
    }

    // Landing after being knocked off something does a little damage
    if (obj->runtime_bitflags & RUNTIMEBITFLAG_LANDDAMAGE)
    {
        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_LANDDAMAGE;
        objApplyDamage(obj, (RANDOMFRAC() * 4.0f) + 2.0f, &prop->pos, 0,
                       (s32)(((u32)(obj->runtime_bitflags & RUNTIMEBITFLAG_OWNER)) >> RUNTIMEBITSHIFT_OWNER));
    }

    if (issimowner)
    {
        if (obj->type == PROPDEF_DOOR)
        {
            doorUpdateSoundVolume((DoorRecord *)obj);
        }
        else if (obj->type == PROPDEF_AUTOGUN && !lvGetControlsLocked())
        {
            autogunTickFire(prop);
        }

        objDropRecursively(prop);
    }

    if (tickop == TICKOP_CHANGEDLIST)
    {
        prop->stan = NULL;
    }

    return tickop;
}


/* Drop everything attached to the prop, and everything attached to those */
void objDropRecursively(PropRecord *prop)
{
    PropRecord *child = propChild(prop);

    while (child)
    {
        PropRecord *prev = child->prev;

        objDropRecursively(child);
        objDrop(child);
        child = prev;
    }
}


/* Tick an attached object whose parent isn't drawn */
void objTickOffscreen(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
    {
        objFree(obj, 1, obj->state & PROPSTATE_RESPAWN);
        return;
    }

    prop->flags &= ~PROPFLAG_ONSCREEN;
    objTickExplosive(prop);
    objTickChildren(prop, objTickOffscreen);
}


/* Tick an attached object whose parent is drawn. An object stuck in its parent is
   drawn with it. */
void objTickAttached(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    Model *model = obj->model;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
    {
        objFree(obj, 1, obj->state & PROPSTATE_RESPAWN);
        return;
    }

    if (model->attachedto_objinst != NULL && (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED))
    {
        Mtxf *parentmtx = modelFindNodeMtx(model->attachedto, model->attachedto_objinst, 0);

        prop->flags |= PROPFLAG_ONSCREEN;
        model->render_pos = (RenderPosView *)dynAllocate(model->obj->numMatrices * sizeof(Mtxf));

        mtx4MultAffine(parentmtx, &obj->embedment->matrix, (Mtxf *)model->render_pos);
        modelUpdateRelationsQuick(model, model->obj->RootNode);
        objTickExplosive(prop);
        objTickChildren(prop, objTickAttached);
    }
    else
    {
        prop->flags &= ~PROPFLAG_ONSCREEN;
        objTickExplosive(prop);
        objTickChildren(prop, objTickOffscreen);
    }
}
