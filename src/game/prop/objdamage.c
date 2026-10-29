/**
 * Damage to objects: how damaged they are, destroying and exploding them, bending their
 * models as they break, and what being destroyed does to each kind of object.
 *
 * An object has `health` (from its setup) and `damageTaken`. Damage adds 250 times the
 * weapon's damage to damageTaken; when that passes health, the object is destroyed: it
 * explodes, its model is bent and blackened (objDeform), and what stands on it falls.
 * damageTaken then starts again from 0 and counts shots: every 4 shots the object breaks
 * further (destroyed levels 1 to 3), and after 12 it is wrecked and shots go through it.
 *
 * Shots hitting objects are in objhit.c. The game's state depends on the exact results
 * here (the model bending uses its own random numbers), so the float sums keep the order
 * of the originals.
 */

#include <ultra64.h>
#include <bondgame.h>
#include "game/prop/objdamage.h"
#include "game/prop/propobj.h"
#include "game/prop/prop.h"
#include "game/prop/projectile.h"
#include "game/prop/explosion.h"
#include "game/prop/monitor.h"
#include "game/prop/alarm.h"
#include "game/gfx/vtxstore.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/chrobjdata.h"
#include "game/nav/stan.h"
#include "game/player/bondview.h"
#include "game/nav/geometry.h"
#include "random.h"

// Objects placed in the air (setup flags): their explosions leave no scorch mark
#define OBJFLAGS_IN_AIR (PROPFLAG_ONSCREEN | PROPFLAG_ENABLED | PROPFLAG_00000008)

// An explosion of small bits, for an already destroyed object breaking further
#define EXPLOSIONTYPE_BITS 0x10

#if defined(VERSION_EU)
#define RESPAWN_TICKS 1000
#else
#define RESPAWN_TICKS 1200 // 20 seconds
#endif

/**
 * How far gone an object is: 0 to 3 while healthy (in quarters of its health, rounded
 * down), then 4 plus the shots taken since it was destroyed.
 */
s32 objGetShotsTaken(ObjectRecord *obj)
{
    if (!(obj->state & PROPSTATE_DESTROYED))
    {
        return (obj->damageTaken * 3.0f) / obj->health;
    }

    return obj->damageTaken + 4.0f;
}

/**
 * 0 while healthy, then 1 when destroyed, 2 after 4 more shots, 3 after 8 (and more after
 * 12, when it is wrecked). Each level is drawn more broken.
 */
s32 objGetDestroyedLevel(ObjectRecord *obj)
{
    if (!(obj->state & PROPSTATE_DESTROYED))
    {
        return 0;
    }

    return ((s32) obj->damageTaken >> 2) + 1;
}

bool objIsHealthy(ObjectRecord *obj)
{
    return objGetDestroyedLevel(obj) == 0;
}

bool objIsCollectable(PropDefHeaderRecord *obj)
{
    switch (obj->type)
    {
        case PROPDEF_KEY:
        case PROPDEF_MAGAZINE:
        case PROPDEF_COLLECTABLE:
        case PROPDEF_HAT:
        case PROPDEF_AMMO:
        case PROPDEF_ARMOUR:
            return TRUE;
        default:
            return FALSE;
    }
}

/**
 * Whether shots can damage an object: not doors, not invincible objects, and not things
 * you can pick up (except body armour) unless they are flagged mortal.
 */
bool objIsMortal(ObjectRecord *obj)
{
    if (obj->type == PROPDEF_DOOR)
    {
        return FALSE;
    }

    if (objIsCollectable((PropDefHeaderRecord *) obj) && obj->type != PROPDEF_ARMOUR)
    {
        if (!(obj->flags & PROPFLAG_FORCEMORTAL))
        {
            return FALSE;
        }
    }
    else if (obj->flags & PROPFLAG_INVINCIBLE)
    {
        return FALSE;
    }

    return TRUE;
}

static s16 objGetExplosionType(ObjectRecord *obj)
{
#if defined(VERSION_EU)
    return ((s8 *) object_explosion_details.typeids)[obj->obj];
#else
    return object_explosion_details[obj->obj].TypeID;
#endif
}

/**
 * Makes an explosion at a prop (an attached prop explodes where it is drawn, or at the
 * prop it is attached to if it is off screen). Its owner is the player who set it off.
 */
s32 propExplode(PropRecord *prop, s32 explosionType)
{
    ObjectRecord *obj = prop->obj;
    s32 playernum = (obj->runtime_bitflags & RUNTIMEBITFLAG_OWNER) >> RUNTIMEBITSHIFT_OWNER;

    if (prop->parent == NULL)
    {
        return explosionCreate(NULL, &obj->runtime_pos, prop->stan, explosionType,
            (obj->flags & OBJFLAGS_IN_AIR) == 0 && (prop->flags & PROPFLAG_00000008) == 0,
            playernum, prop->rooms, (prop->flags & PROPFLAG_00000008) != 0);
    }

    PropRecord *root = prop->parent;
    coord3d pos;

    while (root->parent)
    {
        root = root->parent;
    }

    StandTile *stan = root->stan;

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        Mtxf *mtx = modelGetRootMtx(obj->model);

        pos.x = mtx->m[3][0];
        pos.y = mtx->m[3][1];
        pos.z = mtx->m[3][2];

        mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &pos);
    }
    else
    {
        pos = root->pos;
    }

    if (!(root->flags & PROPFLAG_00000008) && stanWalkLine(&stan, root->pos.f[0], root->pos.f[2], pos.x, pos.z))
    {
        return explosionCreate(NULL, &pos, stan, explosionType, (obj->flags & OBJFLAGS_IN_AIR) == 0, playernum, root->rooms, 0);
    }

    return explosionCreate(NULL, &pos, stan, explosionType, 0, playernum, root->rooms, 1);
}

/* ---- Bending the model ---- */

/** The model's collision display list node, applying the model's switches on the way. */
ModelNode *objFindDlCollisionNode(ObjectRecord *obj)
{
    ModelNode *node = obj->model->obj->RootNode;

    while (node)
    {
        switch (node->Opcode & 0xff)
        {
            case MODELNODE_OPCODE_DLCOLLISION:
                return node;
            case MODELNODE_OPCODE_LOD:
                modelApplyDistanceRelations(obj->model, node);
                break;
            case MODELNODE_OPCODE_SWITCH:
                modelApplyToggleRelations(obj->model, node);
                break;
            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(obj->model, node);
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

    return NULL;
}

/** The consoles' screens stay whole. */
bool objNodeCanDeform(ModelFileHeader *modeldef, ModelNode *node)
{
    if (modeldef->Skeleton == &skeleton_console_one_screen && node == modeldef->Switches[0])
    {
        return FALSE;
    }

    if (modeldef->Skeleton == &skeleton_console_four_screen
        && (node == modeldef->Switches[0] || node == modeldef->Switches[1] || node == modeldef->Switches[2] || node == modeldef->Switches[3]))
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * Bends and blackens an object's model as it breaks: `stage` is 1 when it is destroyed,
 * then 2, 3 and 4 as it breaks further. The model gets its own copy of the vertices from
 * the vertex store, squashed down and jittered, with most of the top (or bottom, for a TV
 * holder) blackened. Each object type has its own random seeds, so it breaks the same way
 * each time. If the vertex store is full it is squashed instead.
 */
void objDeform(ObjectRecord *obj, s32 stage)
{
    Model *model = obj->model;
    ModelNode *node = objFindDlCollisionNode(obj);
    s32 deformseed;

    if (node == NULL)
    {
        return;
    }

    ModelRoData_DisplayList_CollisionRecord *rodata = (ModelRoData_DisplayList_CollisionRecord *) node->Data;

    if (rodata == NULL || !objNodeCanDeform(obj->model->obj, node))
    {
        return;
    }

    Vertex **vtxslot = (Vertex **) &model->datas[rodata->RwDataIndex];

#if defined(VERSION_EU)
    if (obj->obj < PROP_WINDOW)
    {
        if (randomGetNext() & 1)
        {
            deformseed = (u16) object_explosion_details.seeds[obj->obj][stage];
        }
        else
        {
            deformseed = (u16) object_explosion_details.seeds[obj->obj][stage + 3];
        }
    }
    else
    {
        deformseed = 0;
    }
#else
    // Original bug: stage 4 reads past the seeds, into the next type's explosion type
    if (randomGetNext() & 1)
    {
        deformseed = (u16) object_explosion_details[obj->obj].Seed[stage];
    }
    else
    {
        deformseed = (u16) object_explosion_details[obj->obj].Seed[stage + 3];
    }
#endif

    if (deformseed == 0)
    {
        deformseed = randomGetNext();
    }

    explosionClearBulletImpactRoom(obj->prop);

    // A TV holder hangs from the ceiling, so it blackens and sinks the other way
    bool adjustHeight = obj->obj != PROP_TV_HOLDER;

    Vertex *newverts = vtxstoreAllocate(rodata->numVertices, VTXSTORE_OBJ, model->obj, objGetDestroyedLevel(obj));

    if (newverts == NULL)
    {
        if (*vtxslot != rodata->Vertices)
        {
            vtxstoreFree(*vtxslot);
            *vtxslot = rodata->Vertices;
            // Original: this sets RUNTIMEBITFLAG_REMOVE, so the object goes
            obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
        }

        f32 shrink = 0.85000002f;

        obj->mtx.m[1][0] *= shrink;
        obj->mtx.m[1][1] *= shrink;
        obj->mtx.m[1][2] *= shrink;

        if (adjustHeight)
        {
            f32 modelscale = obj->model->scale;
            ModelRoData_BoundingBoxRecord *bbox = chrobjGetBboxFromObjFile(obj->model->obj);

            obj->prop->pos.y += (modelscale * bboxGetYmin(bbox)) * 0.15000001f;
            obj->runtime_pos.y += (modelscale * bboxGetYmin(bbox)) * 0.15000001f;
        }

        return;
    }

    // Copy the current vertices (already bent, or the model's own)
    Vertex *from = *vtxslot;

    for (s32 i = 0; i < rodata->numVertices; i++)
    {
        newverts[i] = from[i];
    }

    if (from != rodata->Vertices)
    {
        vtxstoreFree(from);
    }

    *vtxslot = newverts;

    // The grey desks and the wooden table don't blacken
    bool allowBlackening = !(obj->obj == PROP_DESK1 || obj->obj == PROP_DESK2 || obj->obj == PROP_WOODEN_TABLE1);
    s32 ymin = 99999;
    s32 ymax = -99999;

    for (s32 i = 0; i < rodata->numVertices; i++)
    {
        if (newverts[i].coord.y < ymin)
        {
            ymin = newverts[i].coord.y;
        }

        if (ymax < newverts[i].coord.y)
        {
            ymax = newverts[i].coord.y;
        }
    }

    s32 ymid = (ymin + ymax) >> 1;
    f32 yscale;

    if (stage == 3 && ymid - ymin >= 41)
    {
        ymid = ymin + 40;
    }

    // Squash it down by up to 60 units (10% for the first breaks)
    if (ymax - ymin >= 61)
    {
        if (stage < 3)
        {
            yscale = 0.89999998f;
        }
        else
        {
            yscale = ((f32) ((ymax - ymin) - 60)) / ((f32) (ymax - ymin));
        }
    }
    else
    {
        yscale = 1.0f;
    }

    for (s32 i = 0; i < rodata->numVertices; i++)
    {
        Vertex *orig = &rodata->Vertices[i];
        Vertex *vtx = &newverts[i];
        s32 chance = 0;

        chrObjRandomSetSeed(((orig->coord.z + orig->coord.x) + orig->coord.y) + deformseed);

        if (allowBlackening)
        {
            // The top half (bottom if it is upside down) mostly blackens
            bool isTop = obj->mtx.m[1][1] >= 0.0f ? vtx->coord.y >= ymid : ymid >= vtx->coord.y;

            chance = isTop == adjustHeight ? 90 : 20;
        }

        if ((s32) (chrObjRandomGetNext() % 100) < chance)
        {
            vtx->r = 0;
            vtx->g = 0;
            vtx->b = 0;
            vtx->a = 255;
        }
        else if (stage == 1)
        {
            vtx->a = 0;
        }

        vtx->coord.y = (((f32) (vtx->coord.y - ymin)) * yscale) + ((f32) ymin);
        vtx->coord.x += (chrObjRandomGetNext() % 80) - 40;
        vtx->coord.y += (chrObjRandomGetNext() % 80) - 40;
        vtx->coord.z += (chrObjRandomGetNext() % 80) - 40;

        if (vtx->coord.y < ymin)
        {
            vtx->coord.y = ymin;
        }
    }
}

/* ---- Destroying ---- */

/**
 * Knocks an object off what it stands on: it gets a projectile with a small random hop
 * and spin. Not objects that never fall, or that are already flying or stuck.
 */
static void objFall(ObjectRecord *obj, s32 playernum)
{
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
    obj->runtime_bitflags |= playernum << RUNTIMEBITSHIFT_OWNER;

    if ((obj->flags2 & PROPFLAG2_NOFALL)
        || !(obj->flags & PROPFLAG_RENDERPOSTBG)
        || (obj->runtime_bitflags & (RUNTIMEBITFLAG_EMBEDDED | RUNTIMEBITFLAG_HASPROJECTILE)))
    {
        return;
    }

    coord3d rot = {{0, 0, 0}};
    Projectile *projectile = NULL;

    objEnsureProjectile(obj->prop);

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
    {
        projectile = obj->projectile;
    }

    if (projectile == NULL)
    {
        return;
    }

    projectile->speed.x = RANDOMFRAC() * 1.6666666f - 0.8333333f;
    projectile->speed.y = RANDOMFRAC() * 1.6666666f * 2.0f + 1.6666666f;
    projectile->speed.z = RANDOMFRAC() * 1.6666666f - 0.8333333f;

    if (!(obj->flags2 & PROPFLAG2_FALLWITHOUTROTATION))
    {
#if defined(VERSION_EU)
        rot.x = ((RANDOMFRAC() * 7.5398226f) / 320.0f) - 0.011780973f;
        rot.y = ((RANDOMFRAC() * 7.5398226f) / 320.0f) - 0.011780973f;
        rot.z = ((RANDOMFRAC() * 7.5398226f) / 320.0f) - 0.011780973f;
#else
        rot.x = ((RANDOMFRAC() * M_TAU_F) / 320.0f) - 0.009817477f;
        rot.y = ((RANDOMFRAC() * M_TAU_F) / 320.0f) - 0.009817477f;
        rot.z = ((RANDOMFRAC() * M_TAU_F) / 320.0f) - 0.009817477f;
#endif
    }

    mtx4LoadRotation(&rot, &projectile->mtx);

    projectile->flags |= PROJECTILEFLAG_AIRBORNE;

    obj->flags &= ~PROPFLAG_00000100;
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_ONSUPPORT;
}

/**
 * Knocks off the objects standing on a destroyed one: those above it, in the same room,
 * inside its outline.
 */
static void objDestroySupportedObjects(PropRecord *tableprop, s32 playernum)
{
    ObjectRecord *tableobj = tableprop->obj;
    u8 room = tableprop->stan->room;
    coord2d *polygon;
    s32 edges;

    propGetCollisionPolygon2d(tableprop, &polygon, &edges);

    if (edges <= 0)
    {
        return;
    }

    for (PropRecord *prop = propsGetActiveTail(); prop; prop = prop->prev)
    {
        if ((prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON) && prop->stan->room == room)
        {
            ObjectRecord *obj = prop->obj;

            if (tableobj->runtime_pos.y < obj->runtime_pos.y
                && (obj->runtime_bitflags & RUNTIMEBITFLAG_ONSUPPORT)
                && polygonContainsPoint(&obj->runtime_pos, polygon, edges))
            {
                objFall(obj, playernum);
            }
        }
    }
}

/** An explosion at targetPos, with a scorch mark if the object's root prop is on the floor plan. */
static void objExplosionAt(ObjectRecord *obj, PropRecord *root, coord3d *targetPos, s16 type, s32 playernum)
{
    StandTile *stan = root->stan;

    if (stan == NULL)
    {
        return;
    }

    if (!(root->flags & PROPFLAG_00000008) && stanWalkLine(&stan, root->pos.x, root->pos.z, targetPos->x, targetPos->z))
    {
        explosionCreate(obj->prop, targetPos, stan, type, (obj->flags & OBJFLAGS_IN_AIR) == 0, playernum, root->rooms, 0);
    }
    else
    {
        explosionCreate(obj->prop, targetPos, root->stan, type, 0, playernum, root->rooms, 1);
    }
}

/**
 * After damage: destroys an object whose damage has passed its health (it explodes, bends,
 * and drops what stands on it, and may fall itself), or breaks a destroyed one further
 * every 4 shots.
 */
static void objExplode(ObjectRecord *obj, coord3d *targetPos, s32 playernum)
{
    PropRecord *prop = obj->prop;
    PropRecord *root = prop;

    if (!(obj->health < obj->damageTaken) && objGetDestroyedLevel(obj) == 0)
    {
        return;
    }

    s16 explosionType = objGetExplosionType(obj);

    while (root->parent != NULL)
    {
        root = root->parent;
    }

    if (objGetDestroyedLevel(obj) == 0)
    {
        obj->state |= PROPSTATE_DESTROYED;
        obj->damageTaken = 0.0f;

        objExplosionAt(obj, root, targetPos, explosionType, playernum);

        if (obj->flags2 & PROPFLAG2_REMOVEWHENDESTROYED)
        {
            obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
            return;
        }

        objDeform(obj, 1);

        if (root != prop)
        {
            return;
        }

        objDestroySupportedObjects(prop, playernum);

        // Two in three objects standing on something stay where they are
        if ((obj->runtime_bitflags & RUNTIMEBITFLAG_ONSUPPORT) && (randomGetNext() % 3) != 0)
        {
            return;
        }

        obj->runtime_bitflags |= RUNTIMEBITFLAG_NOLANDDAMAGE;
        objFall(obj, playernum);
        return;
    }

    s32 shots = objGetShotsTaken(obj);

    if ((shots & 3) == 0)
    {
        objDeform(obj, (shots >> 2) + 1);
        objExplosionAt(obj, root, targetPos, EXPLOSIONTYPE_BITS, playernum);
    }

    // A respawning object remembers whether it was wrecked
    if (objGetDestroyedLevel(obj) > 0 && (obj->state & PROPSTATE_RESPAWN))
    {
        if (obj->runtime_bitflags & RUNTIMEBITFLAG_WRECKED)
        {
            obj->state |= PROPSTATE_10;
        }
        else
        {
            obj->state &= ~PROPSTATE_10;
        }

        prop->timetoregen = RESPAWN_TICKS;
    }

    if (shots >= 12)
    {
        obj->runtime_bitflags |= RUNTIMEBITFLAG_WRECKED;
        obj->flags &= ~RUNTIMEBITFLAG_LANDDAMAGE; // original bug: clears the bit in flags, not runtime_bitflags
    }
}

/** Glass breaks into shards and is removed. */
static void glassShatter(ObjectRecord *obj)
{
    PropRecord *prop = obj->prop;
    ModelRoData_BoundingBoxRecord *bbox = chrobjGetBboxFromObjectRecord(obj);

    explosionClearBulletImpactRoomByFlag(prop, FALSE);
    explosionClearBulletImpactRoomByFlag(prop, TRUE);

    glassCreateShards(&obj->runtime_pos,
        (coord3d *) obj->mtx.m[0], (coord3d *) obj->mtx.m[1], (coord3d *) obj->mtx.m[2],
        bbox->Bounds.xmin, bbox->Bounds.xmax,
        bbox->Bounds.ymin, bbox->Bounds.ymax);

    obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
    obj->state |= PROPSTATE_DESTROYED;
    obj->damageTaken = 0.0f;
}

/**
 * Damages a prop and everything attached to it (children first), unless it has an owner
 * set already. The damage is credited to `owner`.
 */
void objDetonateRecursive(PropRecord *prop, f32 damage, coord3d *pos, s32 weaponnum, s32 owner)
{
    ObjectRecord *obj = prop->obj;

    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
    obj->runtime_bitflags |= owner << RUNTIMEBITSHIFT_OWNER;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASOWNER)
    {
        return;
    }

    PropRecord *child = prop->child;

    while (child != NULL)
    {
        PropRecord *next = child->prev;

        objDetonateRecursive(child, damage, pos, weaponnum, owner);
        child = next;
    }

    objApplyDamage(prop->obj, damage, pos, weaponnum, owner);
}

/** objApplyDamage, for gunfire: objects immune to gunfire take none. */
void objApplyShotDamage(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS item, s32 owner)
{
    if (!(obj->flags2 & PROPFLAG2_IMMUNETOGUNFIRE))
    {
        objApplyDamage(obj, damage, pos, item, owner);
    }
}

// What a destroyed multi ammo crate drops one of
static const ObjectRecord s_AmmoCrateTemplate = {
    {0x0100, 0, PROPDEF_MAGAZINE}, // extrascale 1, state, type
    0, // obj
    0xffff, // pad
    PROPFLAG_RENDERPOSTBG, // flags
    0, // flags2
    NULL, // prop
    NULL, // model
    {{{1.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f}}}, // mtx
    {{0.0, 0.0, 0.0}}, // runtime_pos
    {0}, // runtime_bitflags
    NULL, // ptr_allocated_collisiondata_block
    {NULL}, // projectile/embedment
    0.0f, // damageTaken
    1000.0f, // health
    {{0xff, 0xff, 0xff, 0x00}}, // shadecol
    {{0xff, 0xff, 0xff, 0x00}}, // nextcol
};

/** A destroyed multi ammo crate drops a crate of one of its ammo types, starting from a random one. */
static void objDropAmmoCrate(ObjectRecord *obj)
{
    MultiAmmoCrateRecord *multi = (MultiAmmoCrateRecord *) obj;
    s32 startslot = randomGetNext() % AMMOTYPE_GLOBAL_MAX;
    s32 slot = startslot;

    do
    {
        if (multi->slots[slot].quantity > 0 && multi->slots[slot].modelnum != 0xffff)
        {
            AmmoCrateRecord *crate = ammocrateAllocate();

            if (crate != NULL)
            {
                u16 modelnum = multi->slots[slot].modelnum;

                *(ObjectRecord *) crate = s_AmmoCrateTemplate;

                crate->obj = modelnum;
                crate->ammoType = slot + 1;

                if (crate->ammoType == AMMO_9MM_2)
                {
                    crate->ammoType = AMMO_9MM;
                }

                if (objInitWithModelDef((ObjectRecord *) crate, PitemZ_entries[modelnum].header) != NULL)
                {
                    modelSetScale(crate->model, crate->model->scale);
                    propReparent(crate->prop, obj->prop);
                }

                break;
            }
        }

        slot = (slot + 1) % AMMOTYPE_GLOBAL_MAX;
    } while (slot != startslot);
}

static bool weaponIsExplosive(s32 weaponnum)
{
    return weaponnum == ITEM_GRENADE
        || weaponnum == ITEM_TIMEDMINE
        || weaponnum == ITEM_REMOTEMINE
        || weaponnum == ITEM_PROXIMITYMINE
        || weaponnum == ITEM_ROCKETROUND
        || weaponnum == ITEM_GRENADEROUND
        || weaponnum == ITEM_BOMBCASE
        || weaponnum == ITEM_PLASTIQUE;
}

static bool ammoIsExplosive(s32 ammotype)
{
    return ammotype == AMMO_GRENADE
        || ammotype == AMMO_ROCKETS
        || ammotype == AMMO_REMOTEMINE
        || ammotype == AMMO_PROXMINE
        || ammotype == AMMO_TIMEDMINE
        || ammotype == AMMO_GRENADEROUND
        || ammotype == AMMO_EXPLOSIVEPEN
        || ammotype == AMMO_BOMBCASE
        || ammotype == AMMO_DYNAMITE;
}

/** Whether `damage` (from itemnum) can hurt the object, handling explosive pickups on the way. */
static bool objTakesDamage(ObjectRecord *obj, ITEM_IDS itemnum)
{
    // Explosions (no weapon): pickups only if flagged, other objects unless flagged
    if (itemnum == ITEM_UNARMED)
    {
        if (objIsCollectable((PropDefHeaderRecord *) obj))
        {
            return (obj->flags & PROPFLAG_00800000) != 0;
        }

        return !(obj->flags & PROPFLAG_01000000);
    }

    if (obj->flags & PROPFLAG_INVINCIBLE)
    {
        return FALSE;
    }

    if (obj->type == PROPDEF_COLLECTABLE)
    {
        // A shot explosive goes off now
        if (weaponIsExplosive(((WeaponObjRecord *) obj)->weaponnum))
        {
            ((WeaponObjRecord *) obj)->timer = 0;
        }

        return FALSE;
    }

    if (obj->type == PROPDEF_MAGAZINE)
    {
        if (ammoIsExplosive(((AmmoCrateRecord *) obj)->ammoType))
        {
            obj->flags |= 0x10000000;
        }

        return FALSE;
    }

    return objIsMortal(obj);
}

/**
 * Damages an object (`damage` is the weapon's damage; the item is ITEM_UNARMED for
 * explosions) and credits it to `playernum`. Destroys or breaks it as it goes, and stops
 * what it does once destroyed: drone guns and cameras stop, monitors go black, a gas
 * container leaks, body armour gives less, a multi ammo crate drops a crate, and attached
 * objects drop.
 */
void objApplyDamage(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS itemnum, s32 playernum)
{
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
    obj->runtime_bitflags |= playernum << RUNTIMEBITSHIFT_OWNER;

    if (obj->type == PROPDEF_GAS_RELEASING && objGetDestroyedLevel(obj) == 1)
    {
        return;
    }

    if (!objTakesDamage(obj, itemnum))
    {
        return;
    }

    if (objGetDestroyedLevel(obj) == 0)
    {
        obj->damageTaken += damage * 250.0f;
    }
    else
    {
        // Once destroyed, each hit counts as at least one shot, and never past the next level
        f32 shotsToNextLevel = (f32) (4 - objGetShotsTaken(obj) % 4);

        if (shotsToNextLevel < damage)
        {
            damage = shotsToNextLevel;
        }
        else if (damage < 1.0f)
        {
            damage = 1.0f;
        }

        obj->damageTaken += damage;
    }

    if (obj->type == PROPDEF_GLASS || obj->type == PROPDEF_TINTED_GLASS)
    {
        if (obj->health <= obj->damageTaken)
        {
            glassShatter(obj);
        }
    }
    else
    {
        objSetDropType(obj->prop, DROPTYPE_DEFAULT);
        objExplode(obj, pos, playernum);
    }

    if (obj->type == PROPDEF_AMMO && objGetDestroyedLevel(obj) == 1)
    {
        objDropAmmoCrate(obj);
    }

    if (obj->type == PROPDEF_AUTOGUN)
    {
        obj->flags |= PROPFLAG_NO_AMMO;

        if (objGetDestroyedLevel(obj) == 1)
        {
            obj->flags |= PROPFLAG_AUTOGUN_DISABLED;
        }
    }
    else if (obj->type == PROPDEF_CCTV)
    {
        if (objGetDestroyedLevel(obj) == 1)
        {
            obj->flags |= PROPFLAG_CCTV_DISABLED;
        }
    }
    else if (obj->type == PROPDEF_MONITOR)
    {
        if (objGetDestroyedLevel(obj) == 1)
        {
            monitorSetScript(&((MonitorObjRecord *) obj)->Monitor, MONITOR_SCRIPT_BLACK);
        }
    }
    else if (obj->type == PROPDEF_MULTI_MONITOR)
    {
        if (objGetDestroyedLevel(obj) == 1)
        {
            for (s32 i = 0; i < 4; i++)
            {
                monitorSetScript(&((MultiMonitorObjRecord *) obj)->Monitor[i], MONITOR_SCRIPT_BLACK);
            }
        }
    }
    else if (obj->type == PROPDEF_GAS_RELEASING)
    {
        if (objGetDestroyedLevel(obj) == 1)
        {
            gasStart(&obj->runtime_pos);
        }
    }
    else if (obj->type == PROPDEF_ARMOUR)
    {
        BodyArmourRecord *armour = (BodyArmourRecord *) obj;

        if (objGetDestroyedLevel(obj) == 0)
        {
            armour->amount = armour->initialamount * (obj->health - obj->damageTaken) / obj->health;
        }
        else
        {
            armour->amount = 0.0f;
        }
    }

    // Destroyed: what is attached to it drops (the monitors on Bunker's holders, say)
    if (objGetDestroyedLevel(obj) == 1)
    {
        PropRecord *child = obj->prop->child;

        while (child != NULL)
        {
            PropRecord *next = child->prev;

            objSetDropType(child, DROPTYPE_DEFAULT);
            child = next;
        }
    }
}
