/**
 * Level setup. setupCreateProps loads the level's setup file, turns its offsets into
 * pointers, counts the props to size the model slots, then walks the propdef records
 * twice: once to create every guard, door and object (objsetup.c), and once to link
 * records that refer to others (objects inside objects, switches, padlocks, safes).
 *
 * The propdefs are used in place: an ObjectRecord is the setup file's own record, so the
 * level's prop data lives in the setup file for the whole level. setupFreeProps frees
 * the objects again at level end. Records refer to each other by their index in the list,
 * usually relative to their own (setupGetPtrToCommandByIndex, setupCommandGetObject).
 *
 * Also here: loading a prop's model (modelLoad), finding a spare object record to reuse
 * (setupFindObjForReuse), and the per-level reset of the global prop state
 * (propsResetLevelState).
 */
#include <ultra64.h>
#include <PR/libaudio.h>
#include "str.h"
#include "game/menu/front.h"
#include "game/model/model.h"
#include "game/model/modelfile.h"
#include "game/model/chrobjdata.h"
#include "game/math/trig.h"
#include "game/nav/stan.h"
#include "game/world/bg.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/body.h"
#include "game/player/player.h"
#include "game/player/inventory.h"
#include "game/text/language.h"
#include "game/asset/asset.h"
#include "game/mission/objective.h"
#include "game/level/lv.h"
#include "game/prop/propobj.h"
#include "game/prop/alarm.h"
#include "game/prop/monitor.h"
#include "game/prop/projectile.h"
#include "game/prop/carried.h"
#include "game/prop/setup.h"
#include "game/prop/objsetup.h"
#include "game/level/gamemode.h"

/* The level's setup: the working copy, and the file as loaded */
stagesetup g_CurrentSetup;
stagesetup *g_ptrStageSetupFile;

// redeclare with the element count so ARRAYCOUNT works in setupMarkModelsUnloaded
extern ItemModelFileRecord PitemZ_entries[341];

/* Guards are placed at least this far from anything else */
#define GUARD_PLACEMENT_RADIUS 20.0f

/**
 * Whether a propdef record is an object (an ObjectRecord with a prop once created).
 */
static bool propdefIsObject(s32 type)
{
    switch (type)
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
            return TRUE;
    }

    return FALSE;
}

/**
 * The size of a propdef record, in words.
 */
s32 sizepropdef(PropDefHeaderRecord *pdef)
{
    switch (pdef->type)
    {
        case PROPDEF_GUARD:                             return sizeof(GuardRecord) / 4;
        case PROPDEF_DOOR:                              return sizeof(DoorRecord) / 4;
        case PROPDEF_DOOR_SCALE:                        return sizeof(GlobalDoorScaleRecord) / 4;
        case PROPDEF_KEY:                               return sizeof(KeyRecord) / 4;
        case PROPDEF_TINTED_GLASS:                      return sizeof(TintedGlassRecord) / 4;
        case PROPDEF_PROP:
        case PROPDEF_GLASS:
        case PROPDEF_SAFE:
        case PROPDEF_GAS_RELEASING:
        case PROPDEF_ALARM:
        case PROPDEF_RACK:
        case PROPDEF_HAT:                               return sizeof(ObjectRecord) / 4;
        case PROPDEF_CCTV:                              return 0x3b;
        case PROPDEF_MAGAZINE:                          return 0x21;
        case PROPDEF_COLLECTABLE:                       return 0x22;
        case PROPDEF_MONITOR:                           return 0x40;
        case PROPDEF_MULTI_MONITOR:                     return 0x95;
        case PROPDEF_AUTOGUN:                           return 0x36;
        case PROPDEF_AMMO:                              return 0x2d;
        case PROPDEF_ARMOUR:                            return 0x22;
        case PROPDEF_VEHICHLE:                          return 0x2c;
        case PROPDEF_AIRCRAFT:                          return 0x2d;
        case PROPDEF_TANK:                              return 0x38;
        case PROPDEF_LINK:                              return 3;
        case PROPDEF_GUARD_ATTRIBUTE:                   return 3;
        case PROPDEF_SWITCH:                            return 4;
        case PROPDEF_SAFE_ITEM:                         return 5;
        case PROPDEF_TAG:                               return 4;
        case PROPDEF_RENAME:                            return 10;
        case PROPDEF_LOCK_DOOR:                         return 4;
        case PROPDEF_CAMERAPOS:                         return 7;
        case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:         return 4;
        case PROPDEF_OBJECTIVE_START:                   return 4;
        case PROPDEF_OBJECTIVE_END:                     return 1;
        case PROPDEF_OBJECTIVE_DESTROY_OBJECT:          return 2;
        case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:      return 2;
        case PROPDEF_OBJECTIVE_FAIL_CONDITION:          return 2;
        case PROPDEF_OBJECTIVE_COLLECT_OBJECT:          return 2;
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:          return 2;
        case PROPDEF_OBJECTIVE_PHOTOGRAPH:              return 4;
        case PROPDEF_OBJECTIVE_NULL:                    return 1;
        case PROPDEF_OBJECTIVE_ENTER_ROOM:              return 4;
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:  return 5;
        case PROPDEF_OBJECTIVE_COPY_ITEM:               return 1;
        default:                                        return sizeof(PropDefHeaderRecord) / 4;
    }
}

static PropDefHeaderRecord *propdefNext(PropDefHeaderRecord *pdef)
{
    return (PropDefHeaderRecord *)((u32 *)pdef + sizepropdef(pdef));
}

/**
 * The propdef record at an index in the list, or NULL.
 */
PropDefHeaderRecord *setupGetPtrToCommandByIndex(s32 index)
{
    PropDefHeaderRecord *pdef = g_CurrentSetup.propDefs;
    s32 i;

    if (index >= 0 && pdef != NULL)
    {
        for (i = 0; pdef->type != PROPDEF_END; i++)
        {
            if (i == index)
            {
                return pdef;
            }

            pdef = propdefNext(pdef);
        }
    }

    return NULL;
}

/**
 * The index of a propdef record in the list, or -1.
 */
s32 tagGetCommandIndex(ObjectRecord *tag)
{
    PropDefHeaderRecord *pdef = g_CurrentSetup.propDefs;
    s32 i;

    if (pdef != NULL)
    {
        for (i = 0; pdef->type != PROPDEF_END; i++)
        {
            if ((void *)pdef == (void *)tag)
            {
                return i;
            }

            pdef = propdefNext(pdef);
        }
    }

    return -1;
}

/**
 * The index of the propdef record whose prop this is, or -1. Every record is checked as
 * an object, whatever its type.
 */
s32 setupGetCommandIndexByProp(PropRecord *prop)
{
    PropDefHeaderRecord *pdef = g_CurrentSetup.propDefs;
    s32 i;

    if (pdef != NULL)
    {
        for (i = 0; pdef->type != PROPDEF_END; i++)
        {
            if (((ObjectRecord *)pdef)->prop == prop)
            {
                return i;
            }

            pdef = propdefNext(pdef);
        }
    }

    return -1;
}

/**
 * The object record at an index in the list. NULL if there's none, or it's a record of a
 * kind that isn't an object (records of unused kinds are returned as they are).
 */
ObjectRecord *setupCommandGetObject(s32 index)
{
    PropDefHeaderRecord *pdef = setupGetPtrToCommandByIndex(index);

    if (pdef == NULL)
    {
        return NULL;
    }

    switch (pdef->type)
    {
        case PROPDEF_NOTHING:
        case PROPDEF_DEBRIS:
        case PROPDEF_UNK16:
            return (ObjectRecord *)pdef;
    }

    if (!propdefIsObject(pdef->type) && pdef->type < PROPDEF_END)
    {
        return NULL;
    }

    return (ObjectRecord *)pdef;
}

/**
 * Loads a prop model if it isn't already (a model's RootNode is NULL until then).
 * Returns whether it was loaded now.
 */
s32 modelLoad(s32 modelid)
{
    if (PitemZ_entries[modelid].header->RootNode != NULL)
    {
        return FALSE;
    }

    modelFileLoadToPool(PitemZ_entries[modelid].header, PitemZ_entries[modelid].filename);
    modelCalculateRwDataLen(PitemZ_entries[modelid].header);
    return TRUE;
}

static s32 setupCountPropdefs(PROPDEF_TYPE type)
{
    PropDefHeaderRecord *pdef = g_CurrentSetup.propDefs;
    s32 count = 0;

    if (pdef != NULL)
    {
        for (; pdef->type != PROPDEF_END; pdef = propdefNext(pdef))
        {
            if (pdef->type == (type & 0xff))
            {
                count++;
            }
        }
    }

    return count;
}

/**
 * Marks every prop model as not loaded, so modelLoad loads it for this level. The last
 * entry of the table is a terminator.
 */
static void setupMarkModelsUnloaded(void)
{
    ItemModelFileRecord *pitem;

    for (pitem = PitemZ_entries; pitem < &PitemZ_entries[ARRAYCOUNT(PitemZ_entries) - 1]; pitem++)
    {
        pitem->header->RootNode = NULL;
    }
}

/**
 * Loads the stage's setup file, and its text bank. The name comes from
 * setup_text_pointers, with "mp_" after the first letter for multiplayer
 * (e.g. "Ump_setuparchZ").
 */
static void setupLoadFile(enum LEVELID stageId)
{
    char name[0x100] = "";

    name[0] = setup_text_pointers[stageId][0];
    name[1] = '\0';

    if (gameIsMulti())
    {
        strcat(name, "mp_");
    }

    strcat(name, setup_text_pointers[stageId] + 1);

    g_ptrStageSetupFile = assetLoad(name, MEMPOOL_STAGE);
    langLoadToAddr(langGetBankForStage(stageId));
}

static void *setupRebase(stagesetup *file, void *offset)
{
    return (u8 *)file + (u32)offset;
}

/**
 * The setup file stores every internal reference as a byte offset from the start of the
 * file: turns them into pointers into the loaded file. Also scales the pads to the level
 * and finds their floor tiles.
 */
static void setupRebasePointers(stagesetup *file)
{
    PadRecord *pad;
    BoundPadRecord *boundpad;
    f32 scale;
    s32 i;
    s32 j;

    g_CurrentSetup.pathwaypoints = setupRebase(file, file->pathwaypoints);
    g_CurrentSetup.waypointgroups = setupRebase(file, file->waypointgroups);
    g_CurrentSetup.intro = setupRebase(file, file->intro);
    g_CurrentSetup.propDefs = setupRebase(file, file->propDefs);
    g_CurrentSetup.patrolpaths = setupRebase(file, file->patrolpaths);
    g_CurrentSetup.ailists = setupRebase(file, file->ailists);
    g_CurrentSetup.pads = setupRebase(file, file->pads);
    g_CurrentSetup.boundpads = setupRebase(file, file->boundpads);

    // pad names are optional: an offset of 0 means there are none
    g_CurrentSetup.padnames = file->padnames != 0 ? setupRebase(file, file->padnames) : NULL;
    g_CurrentSetup.boundpadnames = file->boundpadnames != 0 ? setupRebase(file, file->boundpadnames) : NULL;

    if (g_CurrentSetup.pathwaypoints != NULL)
    {
        for (i = 0; g_CurrentSetup.pathwaypoints[i].padID >= 0; i++)
        {
            g_CurrentSetup.pathwaypoints[i].neighbours = setupRebase(file, g_CurrentSetup.pathwaypoints[i].neighbours);
        }
    }

    if (g_CurrentSetup.waypointgroups != NULL)
    {
        for (i = 0; g_CurrentSetup.waypointgroups[i].neighbours != NULL; i++)
        {
            g_CurrentSetup.waypointgroups[i].neighbours = setupRebase(file, g_CurrentSetup.waypointgroups[i].neighbours);
            g_CurrentSetup.waypointgroups[i].waypoints = setupRebase(file, g_CurrentSetup.waypointgroups[i].waypoints);
        }
    }

    if (g_CurrentSetup.ailists != NULL)
    {
        for (i = 0; g_CurrentSetup.ailists[i].ailist != NULL; i++)
        {
            g_CurrentSetup.ailists[i].ailist = setupRebase(file, g_CurrentSetup.ailists[i].ailist);
        }
    }

    if (g_CurrentSetup.patrolpaths != NULL)
    {
        for (i = 0; g_CurrentSetup.patrolpaths[i].waypoints != NULL; i++)
        {
            g_CurrentSetup.patrolpaths[i].waypoints = setupRebase(file, g_CurrentSetup.patrolpaths[i].waypoints);

            for (j = 0; g_CurrentSetup.patrolpaths[i].waypoints[j] >= 0; j++)
            {
            }

            g_CurrentSetup.patrolpaths[i].len = j;
        }
    }

    if (g_CurrentSetup.pads != NULL)
    {
        scale = bgGetLevelScaleInv();

        for (pad = g_CurrentSetup.pads; pad->plink != NULL; pad++)
        {
            pad->plink = setupRebase(file, pad->plink);
            pad->pos.f[0] *= scale;
            pad->pos.f[1] *= scale;
            pad->pos.f[2] *= scale;

            stanResolvePadTile(pad, pad->plink, &pad->stan);
        }
    }

    if (g_CurrentSetup.boundpads != NULL)
    {
        scale = bgGetLevelScaleInv();

        for (boundpad = g_CurrentSetup.boundpads; boundpad->plink != NULL; boundpad++)
        {
            boundpad->plink = setupRebase(file, boundpad->plink);
            boundpad->pos.f[0] *= scale;
            boundpad->pos.f[1] *= scale;
            boundpad->pos.f[2] *= scale;
            boundpad->bbox.xmin *= scale;
            boundpad->bbox.xmax *= scale;
            boundpad->bbox.ymin *= scale;
            boundpad->bbox.ymax *= scale;
            boundpad->bbox.zmin *= scale;
            boundpad->bbox.zmax *= scale;

            stanResolvePadTile((PadRecord *)boundpad, boundpad->plink, &boundpad->stan);
        }
    }

    if (g_CurrentSetup.padnames != NULL)
    {
        for (i = 0; g_CurrentSetup.padnames[i].p != NULL; i++)
        {
            g_CurrentSetup.padnames[i].p = setupRebase(file, g_CurrentSetup.padnames[i].p);
        }
    }

    if (g_CurrentSetup.boundpadnames != NULL)
    {
        for (i = 0; g_CurrentSetup.boundpadnames[i].p != NULL; i++)
        {
            g_CurrentSetup.boundpadnames[i].p = setupRebase(file, g_CurrentSetup.boundpadnames[i].p);
        }
    }
}

/* The propdef kinds that take a model slot (guards and aircraft take animated ones) */
static const u8 s_ModelPropdefTypes[] = {
    PROPDEF_COLLECTABLE, PROPDEF_KEY, PROPDEF_HAT, PROPDEF_DOOR, PROPDEF_CCTV, PROPDEF_AUTOGUN,
    PROPDEF_RACK, PROPDEF_MONITOR, PROPDEF_MULTI_MONITOR, PROPDEF_ARMOUR, PROPDEF_PROP,
    PROPDEF_GLASS, PROPDEF_TINTED_GLASS, PROPDEF_SAFE, PROPDEF_UNK41, PROPDEF_GAS_RELEASING,
    PROPDEF_ALARM, PROPDEF_MAGAZINE, PROPDEF_AMMO, PROPDEF_VEHICHLE, PROPDEF_TANK,
};

/**
 * Allocates the guard slots, model slots and each player's inventory (one extra entry per
 * dual weapon link) for the setup's records.
 */
static void setupAllocateSlots(void)
{
    s32 numobjects = 0;
    s32 numanimated = 0;
    s32 i;

    chrsAllocateSlots(setupCountPropdefs(PROPDEF_GUARD));

    numanimated += setupCountPropdefs(PROPDEF_GUARD);

    for (i = 0; i < ARRAYCOUNT(s_ModelPropdefTypes); i++)
    {
        numobjects += setupCountPropdefs(s_ModelPropdefTypes[i]);
    }

    numanimated += setupCountPropdefs(PROPDEF_AIRCRAFT);

    modelmgrAllocateModelSlots(numobjects);
    modelmgrAllocateAnimModelSlots(numanimated);

    for (i = 0; i < getPlayerCount(); i++)
    {
        playerSetCurrent(i);
        invAllocate(setupCountPropdefs(PROPDEF_LINK));
    }
}

/**
 * The flags2 bits of objects not loaded in this game: PROPFLAG2_00000010/20/40 for
 * Agent/Secret/00, and in multiplayer bits 22-24 for 2, 3 and 4 players.
 */
static s32 setupGetSkipFlags(void)
{
    s32 flags = 1 << (lvGetDifficulty() + 4);

    if (gameIsMulti())
    {
        flags |= 1 << (getPlayerCount() + 20);
    }

    return flags;
}

static bool setupIsSkipped(PropDefHeaderRecord *pdef, s32 skipflags)
{
    return (((ObjectRecord *)pdef)->flags2 & skipflags) != 0;
}

/**
 * Creates the object from an object record that's loaded in this game.
 */
static void setupCreateObject(PropDefHeaderRecord *pdef, s32 index)
{
    switch (pdef->type)
    {
        case PROPDEF_DOOR:          setupDoor((DoorRecord *)pdef, index); break;
        case PROPDEF_COLLECTABLE:   setupWeapon((WeaponObjRecord *)pdef); break;
        case PROPDEF_HAT:           setupHat((HatRecord *)pdef); break;
        case PROPDEF_CCTV:          setupCctv((CCTVRecord *)pdef); break;
        case PROPDEF_AUTOGUN:       setupAutogun((AutogunRecord *)pdef); break;
        case PROPDEF_MONITOR:       setupSingleMonitor((MonitorObjRecord *)pdef, index); break;
        case PROPDEF_MULTI_MONITOR: setupMultiMonitor((MultiMonitorObjRecord *)pdef); break;
        case PROPDEF_ARMOUR:        setupArmour((BodyArmourRecord *)pdef); break;
        case PROPDEF_TINTED_GLASS:  setupTintedGlass((TintedGlassRecord *)pdef); break;
        case PROPDEF_TANK:          setupTank((TankRecord *)pdef); break;
        case PROPDEF_VEHICHLE:      setupVehicle((VehichleRecord *)pdef); break;
        case PROPDEF_AIRCRAFT:      setupAircraft((AircraftRecord *)pdef); break;
        default:                    setupObject((ObjectRecord *)pdef); break;
    }
}

/**
 * The first pass over the propdefs: creates guards and objects, and registers tags,
 * renames, briefing text, cutscene cameras and objectives.
 */
static void setupCreatePropdef(PropDefHeaderRecord *pdef, s32 index, s32 skipflags)
{
    switch (pdef->type)
    {
        case PROPDEF_GUARD_ATTRIBUTE:
        {
            GuardAttributeRecord *attr = (GuardAttributeRecord *)pdef;
            u8 prob = attr->GrenadeProb;
            ChrRecord *chr = chrFindByLiteralId(attr->chrnum);

            if (chr != NULL && chr->prop != NULL && chr->model != NULL)
            {
                chr->grenadeprob = prob;
            }
            break;
        }
        case PROPDEF_GUARD:
            setupGuard((GuardRecord *)pdef);
            break;
        case PROPDEF_DOOR_SCALE:
            g_DoorScale = ((GlobalDoorScaleRecord *)pdef)->Scale / 65536.0f;
            break;
        case PROPDEF_AMMO:
            // the crate's multiplayer ammo is filled in even if it's skipped
            setupAmmoCrate((MultiAmmoCrateRecord *)pdef, setupIsSkipped(pdef, skipflags));
            break;
        case PROPDEF_TAG:
        {
            TagObjectRecord *tag = (TagObjectRecord *)pdef;
            ObjectRecord *obj = setupCommandGetObject(index + (s32)tag->OffsetToObj);

            tag->TaggedObject = obj;

            if (obj != NULL)
            {
                obj->runtime_bitflags |= RUNTIMEBITFLAG_TAGGED;
            }

            tagAdd(tag);
            break;
        }
        case PROPDEF_RENAME:
        {
            RenameObjectRecord *rename = (RenameObjectRecord *)pdef;
            ObjectRecord *obj = setupCommandGetObject(rename->TagID + index);

            rename->renobj = obj;

            if (obj != NULL)
            {
                obj->runtime_bitflags |= RUNTIMEBITFLAG_RENAMED;
            }

            invAddTextOverride((struct textoverride *)rename);
            break;
        }
        case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
            briefingTextAdd((struct watchMenuObjectiveText *)pdef);
            break;
        case PROPDEF_CAMERAPOS:
        {
            // position in centimetres, angles 16.16 fixed point
            CutsceneRecord *cam = (CutsceneRecord *)pdef;

            cam->pos.f[0] = *(s32 *)&cam->pos.f[0] / 100.0f;
            cam->pos.f[1] = *(s32 *)&cam->pos.f[1] / 100.0f;
            cam->pos.f[2] = *(s32 *)&cam->pos.f[2] / 100.0f;
            cam->theta = *(s32 *)&cam->theta / 65536.0f;
            cam->verta = *(s32 *)&cam->verta / 65536.0f;
            break;
        }
        case PROPDEF_OBJECTIVE_START:
            objectiveAdd((struct objective_entry *)pdef);
            break;
        case PROPDEF_OBJECTIVE_ENTER_ROOM:
            objectiveAddRoomEntered((struct criteria_roomentered *)pdef);
            break;
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
            objectiveAddDeposit((struct criteria_deposit *)pdef);
            break;
        case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            objectiveAddPicture((struct criteria_picture *)pdef);
            break;
        default:
            if (propdefIsObject(pdef->type) && !setupIsSkipped(pdef, skipflags))
            {
                setupCreateObject(pdef, index);
            }
            break;
    }
}

/**
 * Called from setupLinkPropdef for PROPDEF_SWITCH: adds the link to g_LevelLoadPropSwitch.
 */
static void setupLinkSwitch(LinkRecord *link)
{
    link->next = g_LevelLoadPropSwitch;
    g_LevelLoadPropSwitch = link;
}

/**
 * Called from setupLinkPropdef for PROPDEF_LOCK_DOOR: adds the link to g_LevelLoadPropLockDoor.
 */
static void setupLinkLockDoor(LockDoorRecord *link)
{
    link->next = g_LevelLoadPropLockDoor;
    g_LevelLoadPropLockDoor = link;
}

/**
 * Called from setupLinkPropdef for PROPDEF_SAFE_ITEM: adds the link to g_LevelLoadPropSafeItem.
 */
static void setupLinkSafeItem(SafeObjectRecord *link)
{
    link->next = g_LevelLoadPropSafeItem;
    g_LevelLoadPropSafeItem = link;
}

/**
 * The second pass over the propdefs, once every object exists: puts objects inside
 * their owners, and links dual weapons, switches to doors, padlocks to doors and items
 * to safes. Indexes in these records are relative to the record's own.
 */
static void setupLinkPropdef(PropDefHeaderRecord *pdef, s32 index)
{
    switch (pdef->type)
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
            // an object inside another: its pad is the owner's relative index
            ObjectRecord *obj = (ObjectRecord *)pdef;
            ObjectRecord *owner;

            if (obj->prop != NULL && (obj->flags & PROPFLAG_INSIDEANOTHEROBJ))
            {
                owner = setupCommandGetObject(obj->pad + index);

                if (owner != NULL && owner->prop != NULL)
                {
                    obj->runtime_bitflags |= RUNTIMEBITFLAG_HASOWNER;
                    modelSetScale(obj->model, obj->model->scale);
                    propReparent(obj->prop, owner->prop);
                }
            }
            break;
        }
        case PROPDEF_LINK:
        {
            LinkRecord *link = (LinkRecord *)pdef;
            WeaponObjRecord *left = (WeaponObjRecord *)setupGetPtrToCommandByIndex(link->Index1 + index);
            WeaponObjRecord *right = (WeaponObjRecord *)setupGetPtrToCommandByIndex(link->Index2 + index);

            if (left != NULL && right != NULL && left->type == PROPDEF_COLLECTABLE && right->type == PROPDEF_COLLECTABLE)
            {
                weaponSetDual(left, right);
            }
            break;
        }
        case PROPDEF_SWITCH:
        {
            LinkRecord *link = (LinkRecord *)pdef;
            ObjectRecord *button = setupCommandGetObject(index + link->Index1);
            ObjectRecord *door = (ObjectRecord *)setupGetPtrToCommandByIndex(index + link->Index2);

            if (button != NULL && button->prop != NULL && door != NULL && door->type == PROPDEF_DOOR && door->prop != NULL)
            {
                link->first = button->prop;
                link->second = door->prop;
                setupLinkSwitch(link);
                button->runtime_bitflags |= RUNTIMEBITFLAG_LINKEDSWITCH;
            }
            break;
        }
        case PROPDEF_SAFE_ITEM:
        {
            SafeObjectRecord *link = (SafeObjectRecord *)pdef;
            ObjectRecord *item = setupCommandGetObject(index + link->Index1);
            SafeRecord *safe = (SafeRecord *)setupCommandGetObject(index + link->Index2);
            DoorRecord *door = (DoorRecord *)setupCommandGetObject(index + link->Index3);

            if (item != NULL && item->prop != NULL
                && safe != NULL && safe->prop != NULL && safe->type == PROPDEF_SAFE
                && door != NULL && door->prop != NULL && door->type == PROPDEF_DOOR)
            {
                link->item = item;
                link->safe = safe;
                link->door = door;
                setupLinkSafeItem(link);
                item->flags2 |= PROPFLAG2_LINKEDTOSAFE;
                door->flags2 |= PROPFLAG2_LINKEDTOSAFE;
            }
            break;
        }
        case PROPDEF_LOCK_DOOR:
        {
            LockDoorRecord *link = (LockDoorRecord *)pdef;
            DoorRecord *door = (DoorRecord *)setupCommandGetObject(index + link->Index1);
            ObjectRecord *lock = setupCommandGetObject(index + link->Index2);

            if (door != NULL && door->prop != NULL && lock != NULL && lock->prop != NULL && door->type == PROPDEF_DOOR)
            {
                link->door = door;
                link->lock = lock;
                setupLinkLockDoor(link);
                door->runtime_bitflags |= RUNTIMEBITFLAG_PADLOCKEDDOOR;
            }
            break;
        }
    }
}

/**
 * Loads a stage's setup file and creates everything in it. A stage with no setup file
 * gets an empty setup.
 */
void setupCreateProps(enum LEVELID stageId)
{
    PropDefHeaderRecord *pdef;
    s32 skipflags;
    s32 index;

    g_DoorScale = 1.0f;
    setupMarkModelsUnloaded();

    if (stageId <= LEVELID_MAX + 1 && setup_text_pointers[stageId] != NULL)
    {
        setupLoadFile(stageId);
        setupRebasePointers(g_ptrStageSetupFile);
        setupAllocateSlots();

        if (g_CurrentSetup.propDefs != NULL)
        {
            skipflags = setupGetSkipFlags();

            for (pdef = g_CurrentSetup.propDefs, index = 0; pdef->type != PROPDEF_END; pdef = propdefNext(pdef), index++)
            {
                setupCreatePropdef(pdef, index, skipflags);
            }

            for (pdef = g_CurrentSetup.propDefs, index = 0; pdef->type != PROPDEF_END; pdef = propdefNext(pdef), index++)
            {
                setupLinkPropdef(pdef, index);
            }
        }
    }
    else
    {
        g_CurrentSetup.pathwaypoints = NULL;
        g_CurrentSetup.waypointgroups = NULL;
        g_CurrentSetup.intro = NULL;
        g_CurrentSetup.propDefs = NULL;
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

/**
 * Frees every object the setup created, for good.
 */
void setupFreeProps(void)
{
    PropDefHeaderRecord *pdef = g_CurrentSetup.propDefs;

    if (pdef == NULL)
    {
        return;
    }

    for (; pdef->type != PROPDEF_END; pdef = propdefNext(pdef))
    {
        if (propdefIsObject(pdef->type))
        {
            objFreePermanently((ObjectRecord *)pdef, TRUE);
        }
    }
}

/**
 * Creates a guard at its pad, if there's room, with a random body (and head) unless the
 * record gives them.
 */
void setupGuard(GuardRecord *guard)
{
    PadRecord *pad = &g_CurrentSetup.pads[guard->PadID];
    ChrModelFileRecord *bodyfile;
    StandTile *tile;
    PropRecord *prop;
    ChrRecord *chr;
    Model *model;
    coord3d pos;
    f32 angle;
    s32 bodyid;
    s32 headid = -1;

    if (!setupCheckPlacement(&pad->pos, pad->stan, GUARD_PLACEMENT_RADIUS, &pos, &tile))
    {
        return;
    }

    bodyid = guard->BodyID == 0xffff ? bodyGetRandom() : guard->BodyID;
    bodyfile = &c_item_entries[bodyid];

    if (bodyfile->hasHead == 0)
    {
        headid = guard->HeadID >= 0 ? guard->HeadID : bodyChooseHead(bodyid);
    }

    model = bodyCreateModelWithFlags(bodyid, headid, guard->bitflags);

    if (model == NULL)
    {
        return;
    }

    angle = mathAtan2(pad->look.f[0], pad->look.f[2]);
    prop = chrAllocate(model, &pos, angle, tile, ailistFindById(guard->AIListID));

    if (prop == NULL)
    {
        return;
    }

    propActivate(prop);
    propEnable(prop);

    chr = prop->chr;
    chr->chrnum = guard->chrnum;
    chrSlotSync(chr);
    chr->hearingscale = guard->health / 1000.0f;
    chr->visionrange = guard->ReactionTime;
    chr->padpreset1 = guard->Preset;
    chr->chrpreset1 = guard->chrpreset1;
    chr->headnum = headid;
    chr->bodynum = bodyid;

    if (guard->bitflags & 4)
    {
        chr->chrflags |= CHRFLAG_CLONE;
    }

    if (guard->bitflags & 8)
    {
        chr->chrflags |= CHRFLAG_INVINCIBLE;
    }

    guard->Data = chr;
}

/**
 * Finds an object record of a kind to use for a new object. One with no prop is returned
 * straight away, unless a prop or model is wanted. Otherwise gives the first offscreen
 * one and the first of any (in *offscreenobjptr and *anyobjptr) that can be taken over:
 * not a weapon, a projectile, respawning or inside something, and (if musthavemodel)
 * with a model slot that can hold modeldef.
 */
ObjectRecord *setupFindObjForReuse(s32 wanttype, ObjectRecord **offscreenobjptr, ObjectRecord **anyobjptr, bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef)
{
    PropDefHeaderRecord *pdef = g_CurrentSetup.propDefs;
    ObjectRecord *offscreenobj = NULL;
    ObjectRecord *anyobj = NULL;
    ObjectRecord *obj;

    if (pdef != NULL)
    {
        for (; pdef->type != PROPDEF_END; pdef = propdefNext(pdef))
        {
            if ((wanttype & 0xff) != pdef->type)
            {
                continue;
            }

            obj = (ObjectRecord *)pdef;

            if (obj->prop == NULL)
            {
                if (!musthaveprop && !musthavemodel)
                {
                    return obj;
                }
            }
            else if (wanttype != PROPDEF_COLLECTABLE
                     && (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) == 0
                     && (obj->state & PROPSTATE_RESPAWN) == 0
                     && propParent(obj->prop) == NULL
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
    }

    *offscreenobjptr = offscreenobj;
    *anyobjptr = anyobj;

    return NULL;
}

void objSoundsInit(void)
{
    s32 i;

    for (i = 0; i < AI_SOUND_SLOTS; i++)
    {
        g_AiSounds[i].state = AL_STOPPED;
    }
}

void objSoundsStopAll(void)
{
    s32 i;

    for (i = 0; i < AI_SOUND_SLOTS; i++)
    {
        if (g_AiSounds[i].state != NULL && sndGetPlayingState(g_AiSounds[i].state) != AL_STOPPED)
        {
            sndDeactivate(g_AiSounds[i].state);
        }
    }
}

/**
 * Resets the global prop state for a new level: monitors, mines, alarms, carried item
 * slots, projectiles, the setup links and the cheat scales.
 */
void propsResetLevelState(void)
{
    s32 i;

    monitorsReset();
    proxminesReset();
    alarmGasTimerReset();
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

    for (i = 0; i < MAX_AMMO_CRATES; i++)
    {
        g_AmmoCrates[i].prop = NULL;
    }

    projectilesReset();

    g_LevelLoadPropSwitch = NULL;
    g_LevelLoadPropLockDoor = NULL;
    g_LevelLoadPropSafeItem = NULL;
    g_CctvAlarmTimeScale = 1.0f;
    g_CctvDamageScale = 1.0f;
    g_AutogunPendingDamageTick = 1.0f;
    g_AutogunDamageScalar = 1.0f;
    g_AutogunDamageScale = 1.0f;
    g_SoloAmmoMultiplier = 1.0f;
}
