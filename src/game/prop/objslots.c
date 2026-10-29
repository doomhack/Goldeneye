/**
 * Slots for objects made during play.
 *
 * Guns dropped by guards, thrown weapons, placed mines, knocked-off hats and the ammo
 * crates guards drop need objects that weren't in the level's setup file. They come from
 * fixed pools: 30 weapon slots, 10 hat slots and 20 ammo crates. When a pool is full, an
 * object lying on the floor is freed and its slot reused, preferring one out of view.
 */

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/objslots.h"
#include "game/prop/propobj.h"
#include "game/prop/setup.h"
#include "game/chr/chrai.h"
#include "game/model/model.h"

s32 g_NextWeaponSlot = 0;
s32 g_NextHatSlot = 0;

typedef bool (*SlotReusableFn)(ObjectRecord *obj);

/* Free an object so its record can be used for something new */
static ObjectRecord *objTakeForReuse(ObjectRecord *obj)
{
    if (obj->prop)
    {
        objFreePermanently(obj, TRUE);
    }

    return obj;
}

#define SLOT(i) ((ObjectRecord *)(slots + (i) * slotsize))

/**
 * Take an object for a new weapon or hat. In order: an empty slot (if the caller has a
 * prop and model to give it), a setup object of the same type that can be reused, a slot
 * whose object is lying out of view, a setup object out of view, then any reusable slot
 * or setup object. The search starts after the last slot taken, so slots are reused in
 * turn. Returns NULL if nothing can be reused.
 *
 * musthaveprop and musthavemodel say the caller couldn't allocate a prop or model of its
 * own, so the object taken must free one up (a slot already in use).
 */
static ObjectRecord *slotAllocate(u8 *slots, s32 slotsize, s32 numslots, s32 *nextslot, PROPDEF_TYPE type,
                                  SlotReusableFn reusable, bool musthaveprop, bool musthavemodel,
                                  ModelFileHeader *modeldef)
{
    ObjectRecord *offscreenobj = NULL;
    ObjectRecord *anyobj = NULL;
    ObjectRecord *setupobj;
    s32 freeslot = -1;
    s32 offscreenslot = -1;
    s32 anyslot = -1;
    s32 i = *nextslot;

    do
    {
        ObjectRecord *obj = SLOT(i);

        if (obj->prop == NULL)
        {
            if (!musthaveprop && !musthavemodel)
            {
                freeslot = i;
                break;
            }
        }
        else if (reusable(obj) && (!musthavemodel || modelmgrCanSlotFitRwdata(obj->model, modeldef)))
        {
            if (!(obj->prop->flags & PROPFLAG_ONSCREEN) && offscreenslot < 0)
            {
                offscreenslot = i;
            }

            if (anyslot < 0)
            {
                anyslot = i;
            }
        }

        i = (i + 1) % numslots;
    }
    while (i != *nextslot);

    if (freeslot >= 0)
    {
        *nextslot = (freeslot + 1) % numslots;
        return SLOT(freeslot);
    }

    setupobj = setupFindObjForReuse(type, &offscreenobj, &anyobj, musthaveprop, musthavemodel, modeldef);

    if (setupobj)
    {
        return setupobj;
    }

    if (offscreenslot >= 0)
    {
        *nextslot = (offscreenslot + 1) % numslots;
        return objTakeForReuse(SLOT(offscreenslot));
    }

    if (offscreenobj)
    {
        return objTakeForReuse(offscreenobj);
    }

    if (anyslot >= 0)
    {
        *nextslot = (anyslot + 1) % numslots;
        return objTakeForReuse(SLOT(anyslot));
    }

    if (anyobj)
    {
        return objTakeForReuse(anyobj);
    }

    return NULL;
}


/* A weapon can be reused if it is lying still: not flying, not waiting to respawn, and
   either loose and not a live grenade, or a mine (placed on the floor, an object or a
   door) */
static bool weaponSlotReusable(ObjectRecord *obj)
{
    WeaponObjRecord *weapon = (WeaponObjRecord *)obj;
    PropRecord *parent = obj->prop->parent;

    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) || (obj->state & PROPSTATE_RESPAWN))
    {
        return false;
    }

    if (weapon->timer <= 0 && parent == NULL)
    {
        return true;
    }

    return (weapon->weaponnum == ITEM_REMOTEMINE
            || weapon->weaponnum == ITEM_PROXIMITYMINE
            || weapon->weaponnum == ITEM_TIMEDMINE)
        && (parent == NULL
            || parent->type == PROP_TYPE_OBJ
            || parent->type == PROP_TYPE_DOOR
            || parent->type == PROP_TYPE_WEAPON);
}


/* A hat can be reused once it has landed on the floor */
static bool hatSlotReusable(ObjectRecord *obj)
{
    return !(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) && obj->prop->parent == NULL;
}


WeaponObjRecord *weaponCreate(bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef)
{
    return (WeaponObjRecord *)slotAllocate((u8 *)g_WeaponSlots, sizeof(g_WeaponSlots[0]), MAX_WEAPON_SLOTS,
                                           &g_NextWeaponSlot, PROPDEF_COLLECTABLE, weaponSlotReusable,
                                           musthaveprop, musthavemodel, modeldef);
}


HatRecord *hatCreate(bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef)
{
    return (HatRecord *)slotAllocate((u8 *)g_HatSlots, sizeof(g_HatSlots[0]), MAX_HAT_SLOTS,
                                     &g_NextHatSlot, PROPDEF_HAT, hatSlotReusable,
                                     musthaveprop, musthavemodel, modeldef);
}


/**
 * Take an ammo crate for a guard to drop: a free one, else one lying out of view, else
 * one lying in view.
 */
AmmoCrateRecord *ammocrateAllocate(void)
{
    s32 i;

    for (i = 0; i < MAX_AMMO_CRATES; i++)
    {
        if (g_AmmoCrates[i].prop == NULL)
        {
            return &g_AmmoCrates[i];
        }
    }

    for (s32 pass = 0; pass < 2; pass++)
    {
        for (i = 0; i < MAX_AMMO_CRATES; i++)
        {
            AmmoCrateRecord *crate = &g_AmmoCrates[i];

            if (!(crate->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
                    && !(crate->state & PROPSTATE_RESPAWN)
                    && crate->prop->parent == NULL
                    && (pass == 1 || !(crate->prop->flags & PROPFLAG_ONSCREEN)))
            {
                objFreePermanently(crate, TRUE);
                return crate;
            }
        }
    }

    return NULL;
}
