/**
 * Objects and doors at run time: everything a prop does after it is created.
 *
 * - Projectiles: objects that fly (thrown, dropped, knocked off) get a Projectile from a
 *   pool of 20; objects stuck to something get an Embedment. objTick moves them with
 *   projectileMove, bouncing off walls and props.
 * - objTick: the per-frame tick for every object and door: falling and flying, CCTV,
 *   autoguns, vehicles, aircraft, the tank, weapons such as mines (chrobjWeaponTick).
 *   It is about 2,400 lines long.
 * - Drawing (chrobjRenderProp, objRenderModel) and the monitor screen animations, a
 *   small bytecode interpreted by monitorTickAnimation.
 * - Damage: shot tests (objTestShot), damage and destroyed levels (objApplyDamage),
 *   deforming the model (objDeform), exploding (objExplode), shattering glass.
 * - Pickups: collecting weapons, ammo, keys and hats (propPickupByPlayer).
 * - Weapon and hat slots: pools of props created at run time for dropped guns and hats.
 * - Doors: opening and closing, sounds, portals, and which side the player is on.
 * - Alarm, gas leak and the countdown timer.
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
#include "str.h"


#if defined(VERSION_EU)
#define MONITOR_TIMER_DELTA g_GlobalTimerDeltaRaw
#else
#define MONITOR_TIMER_DELTA g_GlobalTimerDelta
#endif

#define OBJECT_INTERACTION_TIMER_DELTA g_GlobalTimerDelta

#define PROXIMITY_MINE_TRIGGER_DISTANCE 62500.0f

// aprox 135 deg/s/s divided by 60fps
// (135/60/60/(180/M_PI_F))
#define CAM_ACCEL 0.00065449846f 

// 34.3 deg/s/s
#define AUTOGUN_SPIN_ACCEL_PER_FRAME    0.009973311f

// 2058 deg/s or 5.7 rotations per second
#define AUTOGUN_SPIN_MAX_SPEED          0.5983986f

//If squared speed exceeds this, the rocket stops accelerating. This corresponds to ~166.6 units/s.
#define ROCKET_SPEED_BREAK_THRESHOLD    27777.773f

//Horizontal friction applied when projectile is sliding.
#define PROJECTILE_FRICTION_FACTOR      0.9f

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

#define PROJECTILE_LIFETIME_FRAMES 2000

#define GRENADE_SMOKE_FRAMES 0xFB

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

// 40 seconds
#define PROJECTILE_LIFETIME_FRAMES 2400

#define GRENADE_SMOKE_FRAMES 301

#define CCTV_ALARM_FRAMES 300.0f

#endif


/* From the decomp.me ctx -- not present in any repo header. Without the macro,
 * C89 turns U32_TO_F32(...) into an implicit function call (5 sites, ~31 insns
 * short, wrong FP allocation everywhere downstream). */
#define U32MAX 4294967295
#define U32_TO_F32(x) (x*(1.0f/U32MAX))

/* The two named pool constants from the retired GLOBAL_ASM block. Both are the
 * compiler's own literal pool regenerated from these literals (menu18 precedent);
 * value 0x3e8e38e4 = 0.27777779f in both slots of the shipped pool. */
#define ROCKET_INITIAL_GRAVITY_MODIFIER  0.27777779f
#define PROP_PROJECTILE_GRAVITY_MODIFIER 0.27777779f

s32 alarm_timer = 0;
ALSoundState *g_AlarmSound = NULL;
f32 toxic_gas_sound_timer = 0.0;
s32 activate_gas_sound_timer = FALSE;
coord3d gasLeakSource = { {0.0f, 0.0f, 0.0f} };
s32 g_GasLastCoughTime = 0;
f32 gasLeakTimer = 0.0f;
ALSoundState *g_GasSound = NULL;
s32 clock_drawn_flag = 1;
s32 clock_enable = 0;
f32 clock_time = 0;
s32 g_RemoteMineOwnerTriggerFlag = 0;
s32 g_NextWeaponSlot = 0; // numbers between 0 and 30
s32 g_NextHatSlot = 0;
struct LinkRecord *g_LevelLoadPropSwitch = NULL;
LockDoorRecord *g_LevelLoadPropLockDoor = NULL;
SafeObjectRecord *g_LevelLoadPropSafeItem = NULL;
struct PropRecord * g_ProjectileHitProp = NULL;
s32 bodypartshot = 0xFFFFFFFF;
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

// One command of a monitor (TV screen) animation script.
struct tvcmd {
    u32 type;
    s32 value;    // target position/scale (1/1024 units), image, pause frames, script pointer or colour
    u32 duration; // frames to reach the target (for the random jump: the chance, out of 65536)
};

// Forward declarations.

s32 doorUpdateDisplacement(DoorRecord* door);
s32 objGetShotsTaken(ObjectRecord *);
void objRenderModel(PropRecord *prop, ModelRenderData *, s32 xlupass);
bool chrobjSeparatingAxisTheorem(coord2d* rect1, s32 numvertices0, coord2d* rect2, s32 numvertices1);
void chrobjSndCreatePostEvent(ALSoundState *state, coord3d *pos, f32 low, f32 high);
void proxmineRemove(WeaponObjRecord* proxy);
void proxmineAdd(WeaponObjRecord* proxy);
s32 objMove(struct ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal, s32 canEmbed, s32 moveOnlyIfClear);
s32 objTryMovePropWithCollision(ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal);
s32 projectileMove(struct ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal, s32 canEmbed, s32 moveOnlyIfClear);
void objSettle(struct ObjectRecord *obj, coord3d *up);
void doorsUpdatePositions(struct DoorRecord *door);
void doorGetMtx(DoorRecord *door, Mtxf *rhs);
void objBreakCCTVGlass(ObjectRecord *obj);
void monitorSetImage(MonitorRecord *mon, struct sImageTableEntry *image);
void monitorSetAnimation(MonitorRecord *mon, void *image);
s32 jointListFindHit(JointListEntry **entryptr, coord3d *modelRayStart, coord3d *modelRayDir, Model **outModel, ModelNode **outNode);
AmmoCrateRecord *ammocrateAllocate(void);
ModelNode* objFindDlCollisionNode(ObjectRecord* obj);
bool objNodeCanDeform(ModelFileHeader* modeldef, ModelNode* node);
void proxminesDetonateNear(coord3d *pos);
void doorDeactivatePortal(DoorRecord *door);
void doorSetOpenState(DoorRecord *door, s32 state);
s32  sndVolumeForPos(coord3d *pos, f32 low, f32 high);
void doorUpdateSoundVolume(DoorRecord *door);

// End forward declarations.

/* PD: projectileFree (similar but not the same structure) */
void projectileFree(Projectile* projectile)
{
    ALSoundState* sound1;
    ALSoundState* sound2;

    if (projectile->flags & PROJECTILEFLAG_LAUNCHING)
    {
        sound1 = projectile->sounds[0];
        if ((sound1 != 0) && (sndGetPlayingState((ALSoundState* ) sound1) != 0))
        {
            sndDeactivate((ALSoundState* ) projectile->sounds[0]);
        }

        sound2 = projectile->sounds[1];
        if ((sound2 != 0) && (sndGetPlayingState((ALSoundState* ) sound2) != 0))
        {
            sndDeactivate((ALSoundState* ) projectile->sounds[1]);
        }
    }
    projectile->flags |= PROJECTILEFLAG_FREE;
}


void projectileReset(Projectile *projectile)
{
    projectile->flags = 0;

    projectile->speed.x = 0.0f;
    projectile->speed.y = 0.0f;
    projectile->speed.z = 0.0f;

    projectile->accel.x = 0.0f;
    projectile->accel.y = 0.0f;
    projectile->accel.z = 0.0f;

    projectile->riseTime = 0.0f;

    projectile->rotFrac = 1.0f;
    projectile->ownerprop = NULL;
    projectile->bounciness = 0.05f;
    projectile->numBounces = 0;
    projectile->groundFriction = 0.0f;
    projectile->lastSfxTimer = -1;
    projectile->soundSlot = 0;
    projectile->airTime = 0;
    projectile->lastHitSoundTick = -1;
    projectile->droptype = DROPTYPE_DEFAULT;
    projectile->refreshrate = 0;
    projectile->scaleX = 1.0f;
    projectile->scaleY = 1.0f;
    projectile->scaleZ = 1.0f;
    projectile->age = 0;
    projectile->obj = 0;
    projectile->launchTick = 0;
}


Projectile *projectileAllocate(void)
{
    s32 bestindex;
    s32 i;

    bestindex = -1;

    // Happy path - find one that is already free
    for (i = 0; i < PROJECTILES_ARR_MAX; i++)
    {
        if (g_Projectiles[i].flags & PROJECTILEFLAG_FREE)
        {
            projectileReset(g_Projectiles + i);
            return (g_Projectiles + i);
        }
    }

    // Find one with the lowest unkE8 (some kind of age/timer?)
    // and some other conditions
    for (i = 0; i < PROJECTILES_ARR_MAX; i++)
    {
        if (g_Projectiles[i].obj && (bestindex < 0 || g_Projectiles[i].launchTick < g_Projectiles[bestindex].launchTick))
        {
            bestindex = i;
        }
    }

    if (bestindex >= 0)
    {
        // Reset and return it
        objFreeEmbedmentOrProjectile(g_Projectiles[bestindex].obj->prop);
        g_Projectiles[bestindex].obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;

        projectileReset(g_Projectiles + bestindex);
        return (g_Projectiles + bestindex);
    }
    else
    {
        return NULL;
    }
}


void objEnsureProjectile(PropRecord *prop)
{
    ObjectRecord *po = prop->obj; //canonical name
    if (po->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED)
    {
        po->embedment->projectile = projectileAllocate();
    }
    else if ((po->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) == 0)
    {
        po->projectile = projectileAllocate();

        if (po->projectile)
        {
            po->runtime_bitflags |= RUNTIMEBITFLAG_HASPROJECTILE;
        }
    }
}


void projectileSetSticky(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    Projectile *projectile = NULL;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED)
    {
        projectile = obj->embedment->projectile;
    }
    else if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
    {
        projectile = obj->projectile;
    }

    if (projectile)
    {
        projectile->flags |= PROJECTILEFLAG_STICKY;
        if (prop->stan)
        {
            projectile->rooms[0] = prop->stan->room;
            projectile->rooms[1] = 0xFF;
            return;
        }
        projectile->rooms[0] = 0xFFU;
    }
}


void embedmentFree(Embedment *embedment)
{
    embedment->flags |= EMBEDMENTFLAG_FREE;
}


Embedment *embedmentAllocate(void)
{
    s32 i;

    for (i = 0; i < EMBEDMENT_ARR_MAX; i++)
    {
        if (g_Embedments[i].flags & 1)
        {
            g_Embedments[i].flags = 0;
            g_Embedments[i].projectile = NULL;
            return &g_Embedments[(u32)i];
        }
    }

    return NULL;
}


/**
 * This doesn't exactly return the number of shots taken but it's the best way
 * to describe the behaviour of the function without writing a novel into the
 * function's name.
 *
 * The number returned is 0 when at full health and only ever increments as the
 * object takes damage. While healthy, the number scales from 0 to 4 based on
 * how close it is to being destroyed, where 4 is destroyed. After being
 * destroyed, the number increments at 1 per shot up to a max of 12.
 */

s32 objGetShotsTaken(ObjectRecord *obj)
{
    if (!(obj->state & PROPSTATE_DESTROYED))
    {
        return (obj->maxdamage * 3.0f) / obj->damage;
    }

    return obj->maxdamage + 4.0f;
}


/**
 * Return 0 if not destroyed
 * Return 1 if at destroyed level 1
 * Return 2 if at destroyed level 2
 * Return 3 if at destroyed level 3
 *
 * Each destroyed level is a new phase of visual brokenness. Typically the
 * object is destroyed and it looks broken (level 1), then after a couple of
 * shots it enters level 2, and a few shots later level 3.
 *
 * While healthy, damage goes from 0 to maxdamage (eg. 1000) but this function
 * returns 0 due to the if statement.
 *
 * When destroyed, damage is reset to 0 then incremented at one unit per shot,
 * so four shots causes it to enter a new destroyed level.
 */

s32 objGetDestroyedLevel(ObjectRecord *obj)
{
    if (!(obj->state & PROPSTATE_DESTROYED))
    {
        return 0; //if Not Dead
    }
    return ((s32) obj->maxdamage >> 2) + 1;
}


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


void doorGetTileShading(PropRecord *prop, rgba_u8 *color)
{
    struct DoorRecord *door = prop->door;
    if (door->flags & 0x400 ){ return; }

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

        obj->maxdamage = 0.0f;
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

    doorGetTileShading(obj->prop, &obj->nextcol);

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
                baseobj->runtime_bitflags |= RUNTIMEBITFLAG_00008000;
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


void objFreeEmbedmentOrProjectile(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    if (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED)
    {
        if (obj->embedment)
        {
            if (obj->embedment->projectile)
            {
                projectileFree(obj->embedment->projectile);
            }

            embedmentFree(obj->embedment);
        }
        obj->embedment = NULL;
        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_EMBEDDED;
    }
    else if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
    {
        projectileFree(obj->projectile);
        obj->projectile = NULL;
        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_HASPROJECTILE;
    }
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
        DoorRecord* record = (DoorRecord*) obj;
        if ((record->openSoundState != NULL) && (sndGetPlayingState(record->openSoundState) != 0))
        {
            sndDeactivate(record->openSoundState);
        }

        if ((record->closeSoundState != NULL) && (sndGetPlayingState(record->closeSoundState) != 0))
        {
            sndDeactivate(record->closeSoundState);
        }
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


float objGetWidth(struct ObjectRecord * obj)
{
    if (obj->type == PROP_TYPE_WEAPON)
    {
        return 20.0f;
    }
    return 10.0f;
}


bool projectileTestPropBoundingSphere(coord3d *zeropos, coord3d *pos, coord3d *vec, f32 scale)
{
    coord3d vector;
    f32 dist2rd;

    vector.x = vec->x - zeropos->x;
    vector.y = vec->y - zeropos->y;
    vector.z = vec->z - zeropos->z;

    dist2rd = pos->f[0] * vector.f[0] + pos->f[1] * vector.f[1] + pos->f[2] * vector.f[2];

    if (dist2rd > 0) {
        f32 a = pos->f[0] * pos->f[0] + pos->f[1] * pos->f[1] + pos->f[2] * pos->f[2];
        f32 b = vector.f[0] * vector.f[0] + vector.f[1] * vector.f[1] + vector.f[2] * vector.f[2];

        if ((b - scale * scale) * a <= dist2rd * dist2rd) {
            return TRUE;
        }
    }

    return FALSE;
}


bool projectileLineTestModel(ObjectRecord *obj, coord3d *modelRayOrigin, coord3d *modelRayDir, coord3d *hitPos, coord3d *hitNormal, Model **hitModel, ModelNode **hitNode)
{
    s32 found;
    Mtxf *mtx;
    ModelNode *node;
    struct HitThing hitthing;
    s32 mtxindex;
    ModelNode *hitnode;
    Model *model;

    model = obj->model;
    found = 0;
    node = NULL;
    hitnode = NULL;

    // Fast path: test door bounding box.
    if (obj->type == PROPDEF_DOOR) 
    {
        found = modelTestRayIntersectsTransformedBBox(&((DoorRecord *)obj)->bbox, &model->render_pos[0].pos, modelRayOrigin, modelRayDir);
        node = model->obj->RootNode;
        if (found > 0) 
        {
            if (propobjFindHit(model, node, modelRayOrigin, modelRayDir, &hitthing, &mtxindex, &hitnode) == 0) 
            {
                found = 0;
            }
        }
    // General case: traverse model's nodes for collision.
    } 
    else 
    {
        do 
        {
            found = modelFindRayHitNext(model, modelRayOrigin, modelRayDir, &node);

            if (found > 0) 
            {
                bool hit = propobjFindHit(model, node, modelRayOrigin, modelRayDir, &hitthing, &mtxindex, &hitnode);
                if (hit != 0) 
                {
                    break;
                }
            }
        } while (found > 0);
    }

    if (found > 0) 
    {
        mtx = &model->render_pos[mtxindex].pos;

        hitPos->x = hitthing.hitpos.x;
        hitPos->y = hitthing.hitpos.y;
        hitPos->z = hitthing.hitpos.z;

        mtx4TransformVecInPlace(mtx, hitPos);
        mtx4TransformVecInPlace(camGetViewToWorldMtxf(), hitPos);

        hitNormal->x = hitthing.normal.x;
        hitNormal->y = hitthing.normal.y;
        hitNormal->z = hitthing.normal.z;

        mtx4RotateVecInPlace(mtx, hitNormal);

        if (hitNormal->f[0] * modelRayDir->f[0] + hitNormal->f[1] * modelRayDir->f[1] + hitNormal->f[2] * modelRayDir->f[2] > 0.0f) 
        {
            hitNormal->x = -hitNormal->x;
            hitNormal->y = -hitNormal->y;
            hitNormal->z = -hitNormal->z;
        }

        mtx4RotateVecInPlace(camGetViewToWorldMtxf(), hitNormal);

        if (hitNormal->x != 0.0f || hitNormal->y != 0.0f || hitNormal->z != 0.0f) 
        {
            guNormalize(&hitNormal->x, &hitNormal->y, &hitNormal->z);
        } 
        else 
        {
            hitNormal->z = 1.0f;
        }

        *hitModel = model;
        *hitNode = hitnode;
        return TRUE;
    }

    return FALSE;
}


bool projectileTestPropPolygon(PropRecord *prop, coord3d *rayStart, coord3d *rayEnd, coord3d *rayDir, coord3d *hitPos, coord3d *hitNormal, f32 *hitDist)
{
    coord2d *polygon;
    s32 numedges;
    f32 ymax;
    f32 ymin;
    s32 i;
    f32 bestfrac;
    coord2d edgeStart2d;
    coord2d edgeEnd2d;
    coord2d rayStart2d;
    coord2d rayEnd2d;
    s32 bestedge;
    coord3d edgeStart3d;
    coord3d edgeEnd3d;
    f32 dist;
    coord3d intersection;
    s32 next;

    bestfrac = 1.0f;
    bestedge = -1;
    propGetCollisionPolygon(prop, &polygon, &numedges, &ymax, &ymin);

    if (numedges > 0)
    {
        if (!(((ymax < rayStart->y) && (ymax < rayEnd->y)) || ((rayStart->y < ymin) && (rayEnd->y < ymin))))
        {
            rayStart2d.x = rayStart->x;
            rayStart2d.y = rayStart->z;
            rayEnd2d.x = rayEnd->x;
            rayEnd2d.y = rayEnd->z;
            for (i = 0; i < numedges; i++)
            {
                next = (i + 1) % numedges;
                if (geoSegmentsIntersect(rayStart->x, rayStart->z, rayEnd->x, rayEnd->z, polygon[i].x, polygon[i].y, polygon[next].x, polygon[next].y))
                {
                    edgeStart2d.x = polygon[i].x;
                    edgeStart2d.y = polygon[i].y;
                    edgeEnd2d.x = polygon[next].x;
                    edgeEnd2d.y = polygon[next].y;
                    dist = geoSegmentHitFrac(&rayStart2d, &rayEnd2d, &edgeStart2d, &edgeEnd2d);

                    if (dist < bestfrac)
                    {
                        bestfrac = dist;
                        bestedge = i;
                    }
                }
            }
            if (bestedge > 0)
            {
                next = (bestedge + 1) % numedges;
                edgeStart3d.x = polygon[bestedge].x;
                edgeStart3d.y = 0.0f;
                edgeStart3d.z = polygon[bestedge].y;
                edgeEnd3d.x = polygon[next].x;
                edgeEnd3d.y = 0.0f;
                edgeEnd3d.z = polygon[next].y;

                chrLineLineIntersection(&edgeStart3d, &edgeEnd3d, rayStart, rayDir, &intersection);
                dist = (rayDir->z * (intersection.z - rayStart->z)) + (((intersection.x - rayStart->x) * rayDir->x) + ((intersection.y - rayStart->y) * rayDir->y));

                if (dist < (*hitDist))
                {
                    *hitDist = dist;
                    hitPos->x = intersection.x;
                    hitPos->y = intersection.y;
                    hitPos->z = intersection.z;
                    hitNormal->x = -rayDir->x;
                    hitNormal->y = 0.0f;
                    hitNormal->z = -rayDir->z;

                    if ((hitNormal->x != 0.0f) || (hitNormal->z != 0.0f))
                    {
                        guNormalize(&hitNormal->x, &hitNormal->y, &hitNormal->z);
                    } else
                    {
                        hitNormal->z = 1.0f;
                    }

                    g_ProjectileHitProp = prop;
                    bodypartshot = HIT_NULL_PART;
                    g_CurrentProjectileModel = NULL;
                    g_ProjectileHitNode = NULL;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}


/**
 * Test a single object for collision between the object and a projectile.
 * If there is a collision, update hit parameters with collision data.
 * @return TRUE if the object and projectile collide, FALSE otherwise.
 */
bool projectileTestObjectCollision(ObjectRecord *obj, coord3d *worldRayOrigin, coord3d *worldRayEnd, coord3d *worldRayDir, f32 maxDist, coord3d *modelRayOrigin, coord3d *modelRayDir, coord3d *hitPos, coord3d *hitNormal, f32 *hitDist, Model **hitModel, ModelNode **hitNode)
{
    Model *modelstack[1];
    f32 instsize;
    f32 value;
    f32 dx;
    f32 dy;
    f32 dz;
    PropRecord *prop;

    instsize = modelGetRadius(modelstack[0] = obj->model);
    prop = obj->prop;
    value = 0.0f;

    if (prop->parent == NULL) {
        dx = obj->runtime_pos.x - worldRayOrigin->x;
        dy = obj->runtime_pos.y - worldRayOrigin->y;
        dz = obj->runtime_pos.z - worldRayOrigin->z;
        value = (dz * worldRayDir->z) + ((dx * worldRayDir->x) + (dy * worldRayDir->y));
    }

    if (-instsize <= value) {
        if (value <= maxDist + instsize) {
            // Precise line test for on screen props.
            if (prop->flags & PROPFLAG_ONSCREEN) {
                if (projectileLineTestModel(obj, modelRayOrigin, modelRayDir, hitPos, hitNormal, hitModel, hitNode)) {
                    dx = hitPos->x - worldRayOrigin->x;
                    dy = hitPos->y - worldRayOrigin->y;
                    dz = hitPos->z - worldRayOrigin->z;
                    value = (dz * worldRayDir->z) + ((dx * worldRayDir->x) + (dy * worldRayDir->y));

                    if (0.0f <= value) {
                        if (value <= maxDist) {
                            *hitDist = value;
                            return TRUE;
                        }
                    }
                }
            // Cheaper bounds test for off screen props.
            } else {
                prop = obj->prop;
                instsize = modelGetRadius(modelstack[0]);

                if (projectileTestPropBoundingSphere(worldRayOrigin, worldRayDir, &obj->runtime_pos, instsize)) {
                    *hitDist = maxDist;

                    if (projectileTestPropPolygon(prop, worldRayOrigin, worldRayEnd, worldRayDir, hitPos, hitNormal, hitDist))
                    {
                        *hitModel = modelstack[0];
                        *hitNode = *(ModelNode **)modelstack[0]->obj;
                        return TRUE;
                    }
                }
            }
        }
    }

    return FALSE;
}


/**
 * Tests an object and its on screen child hierarchy for projectile collision.
 * If a closer hit is found, updates the caller's hit collision data.
 * @returns TRUE if this object or one of its recursive children produced a closer hit, FALSE otherwise.
 */
bool projectileTestObjectCollisionRecursive(ObjectRecord *obj, coord3d *worldRayOrigin, coord3d *worldRayEnd, coord3d *worldRayDir, f32 maxDist, coord3d *modelRayOrigin, coord3d *modelRayDir, coord3d *bestHitPos, coord3d *bestHitNormal, f32 *bestHitDist)
{
    coord3d hitPos;
    coord3d hitNormal;
    f32 hitDist;
    Model *hitModel;
    ModelNode *hitNode;
    bool found;
    PropRecord *prop;
    PropRecord *child;

    prop = obj->prop;
    found = FALSE;

    if (projectileTestObjectCollision(obj, worldRayOrigin, worldRayEnd, worldRayDir, maxDist, modelRayOrigin, modelRayDir, &hitPos, &hitNormal, &hitDist, &hitModel, &hitNode))
    {
        if (hitDist < *bestHitDist)
        {
            *bestHitDist = hitDist;

            bestHitPos->x = hitPos.x;
            bestHitPos->y = hitPos.y;
            bestHitPos->z = hitPos.z;

            bestHitNormal->x = hitNormal.x;
            bestHitNormal->y = hitNormal.y;
            bestHitNormal->z = hitNormal.z;

            g_ProjectileHitProp = obj->prop;
            bodypartshot = -1;
            g_CurrentProjectileModel = hitModel;
            g_ProjectileHitNode = hitNode;

            found = TRUE;
        }
    }

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        child = prop->child;

        while (child != NULL)
        {
            if (child->flags & PROPFLAG_ONSCREEN)
            {
                if (projectileTestObjectCollisionRecursive(child->obj, worldRayOrigin, worldRayEnd, worldRayDir, maxDist, modelRayOrigin, modelRayDir, bestHitPos, bestHitNormal, bestHitDist))
                {
                    found = TRUE;
                }
            }

            child = child->prev;
        }
    }

    return found;
}


bool projectileTestChr(ChrRecord *chr, coord3d *rayStart, coord3d *rayDir, f32 rayLen, coord3d *screenStart, coord3d *screenDir, coord3d *hitpos, coord3d *hitnormal, f32 *bestdist) {
    f32 instSize;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 planeDist;
    f32 hitDist;
    s32 bodyPart;
    Model *model;
    ModelNode *node;
    JointListEntry *entry;
    Mtxf *mtx;
    PropRecord *prop;

    prop = chr->prop;
    instSize = modelGetRadius(chr->model);
    dx = prop->pos.x - rayStart->x;
    dy = prop->pos.y - rayStart->y;
    dz = prop->pos.z - rayStart->z;
    planeDist = (dz * rayDir->z) + ((dx * rayDir->x) + (dy * rayDir->y));

    if ((-instSize <= planeDist) && (planeDist <= (rayLen + instSize)) && (prop->flags & PROPFLAG_ONSCREEN)) {
        entry = chr->jointList;
        bodyPart = jointListFindHit(&entry, screenStart, screenDir, &model, &node);
        if (bodyPart > 0) {
            mtx = modelFindNodeMtx(model, node, 0);
            dx = mtx->m[3][0] - screenStart->x;
            dy = mtx->m[3][1] - screenStart->y;
            dz = mtx->m[3][2] - screenStart->z;
            hitDist = (dz * screenDir->z) + ((dx * screenDir->x) + (dy * screenDir->y));
            if (hitDist < *bestdist) {
                *bestdist = hitDist;
                hitpos->x = (rayDir->x * hitDist) + rayStart->x;
                hitpos->y = (rayDir->y * hitDist) + rayStart->y;
                hitpos->z = (rayDir->z * hitDist) + rayStart->z;
                hitnormal->x = -rayDir->x;
                hitnormal->y = 0.0f;
                hitnormal->z = -rayDir->z;
                if ((hitnormal->x != 0.0f) || (hitnormal->z != 0.0f)) {
                    guNormalize(&hitnormal->x, &hitnormal->y, &hitnormal->z);
                } else {
                    hitnormal->z = 1.0f;
                }
                g_ProjectileHitProp = prop;
                bodypartshot = bodyPart;
                g_CurrentProjectileModel = model;
                g_ProjectileHitNode = node;
                return 1;
            }
        }
    }
    return 0;
}


bool projectileFindCollidingProp(PropRecord *ignoreProp, coord3d *worldRayStart, coord3d *worldRayEnd, u32 cdtypes, coord3d *outHitPos, coord3d *outHitNormal, s32 *rooms)
{
    bool result;
    f32 dist;
    s16 *propnumptr;
    f32 bestdist;
    bool found_collision;
    coord3d dir;
    ChrRecord *chr;
    coord3d screenStart;
    coord3d screenDir;
    PropRecord *iterprop;
    PropRecord *playerstank;
    ObjectRecord *obj;

    result = FALSE;
    found_collision = FALSE;
    playerstank = playerGetTank();

    dir.x = worldRayEnd->x - worldRayStart->x;
    dir.y = worldRayEnd->y - worldRayStart->y;
    dir.z = worldRayEnd->z - worldRayStart->z;

    dist = sqrtf(dir.f[0] * dir.f[0] + dir.f[1] * dir.f[1] + dir.f[2] * dir.f[2]);

    if (dist == 0.0f)
    {
        return FALSE;
    }

    dir.x *= (1.0f / dist);
    dir.y *= (1.0f / dist);
    dir.z *= (1.0f / dist);

    screenStart.x = worldRayStart->x;
    screenStart.y = worldRayStart->y;
    screenStart.z = worldRayStart->z;

    mtx4TransformVecInPlace(camGetWorldToScreenMtxf(), &screenStart);

    screenDir.x = dir.x;
    screenDir.y = dir.y;
    screenDir.z = dir.z;

    mtx4RotateVecInPlace(camGetWorldToScreenMtxf(), &screenDir);

    bestdist = dist;

    if (cdtypes != 0)
    {
        roomGetProps(rooms);

        for (propnumptr = g_RoomPropsFound; *propnumptr >= 0; propnumptr++)
        {
            iterprop = &g_Props[*propnumptr];

            if (iterprop != ignoreProp)
            {
                if (iterprop->type == PROP_TYPE_OBJ
                        || iterprop->type == PROP_TYPE_WEAPON
                        || iterprop->type == PROP_TYPE_DOOR)
                {
                    obj = iterprop->obj;

                    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_ISRETICK) == 0 && (obj->flags2 & PROPFLAG2_THROWTHROUGH) == 0) {
                        if (iterprop->type == PROP_TYPE_DOOR)
                        {
                            if ((cdtypes & CDTYPE_DOORS) == 0 && (propDoorGetCdTypes(iterprop) & cdtypes) == 0)
                            {
                                continue;
                            }
                        }
                        else
                        {
                            if ((cdtypes & CDTYPE_OBJS) == 0)
                            {
                                continue;
                            }
                        }

                        if ((iterprop != playerstank) || !(obj->state & PROPSTATE_20))
                        {
                            if (projectileTestObjectCollisionRecursive(obj, worldRayStart, worldRayEnd, &dir, dist, &screenStart, &screenDir, outHitPos, outHitNormal, &bestdist))
                            {
                                found_collision = TRUE;
                            }
                        }
                    }
                } else if (iterprop->type == PROP_TYPE_CHR
                        || (iterprop->type == PROP_TYPE_VIEWER && iterprop->chr))
                {
                    chr = iterprop->chr;

                    if (iterprop->type == PROP_TYPE_VIEWER)
                    {
                        if (!g_playerPointers[playerGetIndexForProp(iterprop)]->perimEnabled || (cdtypes & CDTYPE_PLAYERS) == 0)
                        {
                            continue;
                        }
                    }
                    else if (iterprop->type == PROP_TYPE_CHR)
                    {
                        if ((chr->hidden & CHRHIDDEN_MOVING) || (cdtypes & CDTYPE_CHRS) == 0)
                        {
                            continue;
                        }
                    }

                    if (projectileTestChr(chr, worldRayStart, &dir, dist, &screenStart, &screenDir, outHitPos, outHitNormal, &bestdist))
                    {
                        found_collision = TRUE;
                    }
                }
                else if (iterprop->type == PROP_TYPE_VIEWER && g_playerPointers[playerGetIndexForProp(iterprop)]->perimEnabled)
                {
                    if (projectileTestPropPolygon(iterprop, worldRayStart, worldRayEnd, &dir, outHitPos, outHitNormal, &bestdist))
                    {
                        found_collision = TRUE;
                    }
                }
            }
        }
    }

    if (found_collision)
    {
        result = TRUE;

        g_ProjectileHitPos.x = dir.x;
        g_ProjectileHitPos.y = dir.y;
        g_ProjectileHitPos.z = dir.z;

        g_ProjectileHitNormal.x = screenDir.x;
        g_ProjectileHitNormal.y = screenDir.y;
        g_ProjectileHitNormal.z = screenDir.z;

    }

    return result;
}


/**
 * this function contains 
 * osSyncPrintf("stanLineObjGfx: %d rooms is more than %d\n",arg0+0x58,20);
 */
s32 projectileMove(struct ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal, s32 canEmbed, s32 moveOnlyIfClear)
{
    PropRecord *prop;
    StandTile *tile;
    s32 result;
    HitThing hit;
    coord3d endpos;
    s32 i;
    s32 roomCount;
    s32 roomNums[121];
    u8 roomSet[8];
    coord3d diff;
    s32 *roomPtr;
    f32 dist;
    f32 factor;
    f32 scale;

    prop = obj->prop;
    tile = prop->stan;
    result = 1;
    g_ProjectileHitProp = NULL;

    if (((obj->runtime_pos.x == targetpos->x) && (obj->runtime_pos.y == targetpos->y)) && (obj->runtime_pos.z == targetpos->z))
    {
        goto end;
    }

    endpos.x = targetpos->x;
    endpos.y = targetpos->y;
    endpos.z = targetpos->z;

    if ((!(obj->runtime_bitflags & RUNTIMEBITFLAG_00000080)) || (!(obj->projectile->flags & PROPFLAG_ENABLED)))
    {
        goto end;
    }

    roomCount = 0;
    bgFindRoomsAlongSegment(&obj->runtime_pos, &endpos, obj->projectile->rooms, roomSet, roomNums, &roomCount, 20);

    if (roomCount > 20)
    {
        roomCount = 20;
    }

    i = bgCopyVisibleRoomsToList(&roomNums[roomCount], 100);
    roomCount = roomCount + i;
    roomNums[roomCount] = -1;

    if (roomNums[0] < 0)
    {
        goto after_bg_loop;
    }

    roomPtr = roomNums;

bg_loop:
    if (bgTestBulletHitBackground(&obj->runtime_pos, &endpos, *roomPtr, &hit))
    {
        scale = bgGetLevelScaleInv();


        hit.hitpos.x *= scale;
        hit.hitpos.y *= scale;
        hit.hitpos.z *= scale;

        if ((((((obj->runtime_pos.x <= endpos.x) && (hit.hitpos.x <= endpos.x)) && (obj->runtime_pos.x <= hit.hitpos.x))
           || (((endpos.x <= obj->runtime_pos.x) && (endpos.x <= hit.hitpos.x)) && (hit.hitpos.x <= obj->runtime_pos.x)))
           && ((((obj->runtime_pos.y <= endpos.y) && (hit.hitpos.y <= endpos.y)) && (obj->runtime_pos.y <= hit.hitpos.y))
           || (((endpos.y <= obj->runtime_pos.y) && (endpos.y <= hit.hitpos.y)) && (obj->runtime_pos.y >= hit.hitpos.y))))
           && ((((obj->runtime_pos.z <= endpos.z) && (hit.hitpos.z <= endpos.z)) && (obj->runtime_pos.z <= hit.hitpos.z))
           || (((endpos.z <= obj->runtime_pos.z) && (endpos.z <= hit.hitpos.z)) && (hit.hitpos.z <= obj->runtime_pos.z))))
        {
            if (!(((obj->runtime_pos.x == hit.hitpos.x)  && (obj->runtime_pos.y == hit.hitpos.y)) && (obj->runtime_pos.z == hit.hitpos.z)))
            {
                result = 0;

                hitpos->x = hit.hitpos.x;
                hitpos->y = hit.hitpos.y;
                hitpos->z = hit.hitpos.z;

                hitnormal->x = hit.normal.x;
                hitnormal->y = hit.normal.y;
                hitnormal->z = hit.normal.z;

                if (((hitnormal->x != 0.0f) || (hitnormal->y != 0.0f)) || (hitnormal->z != 0.0f))
                {
                    guNormalize(&hitnormal->x, &hitnormal->y, &hitnormal->z);
                }
                else
                {
                    hitnormal->z = 1.0f;
                }
            }
        }
    }

    roomPtr++;

    if ((*roomPtr) >= 0)
    {
        goto bg_loop;
    }

after_bg_loop:
    if (result == 0)
    {
        diff.x = targetpos->x - obj->runtime_pos.x;
        diff.y = targetpos->y - obj->runtime_pos.y;
        diff.z = targetpos->z - obj->runtime_pos.z;

        dist = sqrtf((diff.z * diff.z) + ((diff.x * diff.x) + (diff.y * diff.y)));


        if (0.1f < dist)
        {
            factor = 0.1f;
            factor = factor / dist;
        }
        else
        {
            factor = 0.5f;
        }

        hitpos->x -= factor * diff.x;
        hitpos->y -= factor * diff.y;
        hitpos->z -= factor * diff.z;

        endpos.x = hitpos->x;
        endpos.y = hitpos->y;
        endpos.z = hitpos->z;
    }

    if (!projectileFindCollidingProp(prop, &obj->runtime_pos, &endpos, CDTYPE_ALL & ~CDTYPE_BG, hitpos, hitnormal, roomNums))
    {
        if ((result == 0) && (canEmbed != 0))
        {
            result = 2;

            if (moveOnlyIfClear == 0)
            {
                bgFindRoomsAlongSegment(&obj->runtime_pos, hitpos, obj->projectile->rooms, roomSet, roomNums, &roomCount, 20);
            }
        }
        else if ((result == 1) || (moveOnlyIfClear == 0))
        {
            if (result != 1)
            {
                bgFindRoomsAlongSegment(&obj->runtime_pos, &endpos, obj->projectile->rooms, roomSet, roomNums, &roomCount, 20);
            }

            obj->runtime_pos.x = endpos.x;
            obj->runtime_pos.z = endpos.z;
            prop->pos.y = (dist = endpos.y);
            obj->runtime_pos.y = dist;
        }
    }
    else if (moveOnlyIfClear == 0)
    {
        // Keep if (1) for matching.
        if (1)
        {
            endpos.x = obj->runtime_pos.x;
            endpos.z = obj->runtime_pos.z;

            bgFindRoomsAlongSegment(&obj->runtime_pos, &endpos, obj->projectile->rooms, roomSet, roomNums, &roomCount, 20);

            dist = endpos.y;
        }

        prop->pos.y = dist;
        obj->runtime_pos.y = dist;

        if (canEmbed)
        {
            result = 2;
        }
        else
        {
            result = 0;
        }
    }

    if ((result != 1) && (moveOnlyIfClear != 0))
    {
        goto end;
    }

    i = 0;

    if (!(obj->projectile->flags & PROPFLAG_00000008))
    {
        tile = prop->stan;

        if ((stanWalkLine(&tile, prop->pos.x, prop->pos.z, obj->runtime_pos.x, obj->runtime_pos.z) == TRUE) && (tile != NULL))
        {
            prop->stan = tile;
            prop->pos.x = obj->runtime_pos.x;
            prop->pos.z = obj->runtime_pos.z;
        }
        else
        {
            obj->projectile->flags |= PROPFLAG_00000008;
            prop->flags |= PROPFLAG_00000008;
        }
    }

    if (obj->projectile->flags & PROPFLAG_00000008)
    {
        tile = stanFindTileBelowPos(&obj->runtime_pos, roomSet, NULL);

        if (tile != NULL)
        {
            prop->stan = tile;
            prop->pos.x = obj->runtime_pos.x;
            prop->pos.z = obj->runtime_pos.z;
            obj->projectile->flags &= ~PROPFLAG_00000008;
            prop->flags &= ~PROPFLAG_00000008;
        }
    }

    if (roomSet[0] != 0xff)
    {
        do
        {
            obj->projectile->rooms[i] = roomSet[i];
            i++;
        }
        while ((roomSet[i] != 0xff) && (i != 7));
    }

    obj->projectile->rooms[i] = 0xff;

end:
    return result;
}


s32 objTryMovePropWithCollision(ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal)
{
    PropRecord *prop;
    StandTile *stan;
    f32 width;
    s32 result;
    coord3d dir;
    coord3d target;
    f32 tmp;
    f32 ymax;
    f32 ymin;
    coord3d edgeA;
    coord3d edgeB;
    coord3d partialpos = {};
    coord2d centre;
    coord2d edgeStart;
    coord2d edgeEnd;
    coord2d dir2d;

    prop = obj->prop;
    stan = prop->stan;
    width = objGetWidth(obj);
    result = 1;
    g_ProjectileHitProp = NULL;

    // Early return if target position and object's current position are the same.
    if (((targetpos->f[0] == prop->pos.f[0]) && (targetpos->f[1] == prop->pos.f[1])) && (targetpos->f[2] == prop->pos.f[2]))
    {
        goto done;
    }

    target.f[0] = targetpos->f[0];
    target.f[1] = targetpos->f[1];
    target.f[2] = targetpos->f[2];

    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_00000080) == 0)
    {
        goto done;
    }

    tmp = stanGetFloorY(prop->stan, prop->pos.x, prop->pos.z);

    if ((obj->embedment->flags & PROJECTILEFLAG_00000040) == 0)
    {
        ymax = 0.0f;
        ymin = 1.0f;
    }
    else if (target.f[1] < prop->pos.y)
    {
        ymax = prop->pos.y - tmp;
        ymin = target.f[1] - tmp;
    }
    else
    {
        ymax = target.f[1] - tmp;
        ymin = prop->pos.y - tmp;
    }

    stanResetHits();

    if ((stanTestLineUnobstructed(&stan, prop->pos.x, prop->pos.z, target.f[0], target.f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, ymax, ymin, 0.0f, 1.0f) != 0) && (stan != NULL))
    {
        if (stanTestVolume(&stan, target.f[0], target.f[2], width, 0x1f, ymax, ymin) < 0)
        {
            obj->runtime_pos.x = target.f[0];
            obj->runtime_pos.z = target.f[2];
            prop->stan = stan;
            prop->pos.x = target.f[0];
            prop->pos.z = target.f[2];
            tmp = target.f[1];
            prop->pos.y = tmp;
            obj->runtime_pos.y = tmp;
            goto done;
        }
    }

    stanGetCollisionEdge(&edgeA, &edgeB);

    hitnormal->x = edgeB.z - edgeA.z;
    hitnormal->y = 0.0f;
    hitnormal->z = edgeA.x - edgeB.x;

    if (hitnormal->x != 0.0f)
    {
        goto normalize;
    }

    if (hitnormal->z == 0.0f)
    {
        goto unitz;
    }

normalize:
    guNormalize(&hitnormal->x, &hitnormal->y, &hitnormal->z);
    goto normaldone;

unitz:
    hitnormal->z = 1.0f;

normaldone:
    if (((target.f[0] != prop->pos.f[0]) || (target.f[1] != prop->pos.f[1])) || (target.f[2] != prop->pos.f[2]))
    {
        dir.x = target.f[0] - prop->pos.x;
        dir.y = target.f[1] - prop->pos.y;
        dir.z = target.f[2] - prop->pos.z;

        chrStanLineDirIntersection(&prop->pos, &dir, hitpos);

        centre.x = prop->pos.x;
        centre.y = prop->pos.z;

        edgeStart.x = edgeA.x;
        edgeStart.y = edgeA.z;
        edgeEnd.x = edgeB.x;
        edgeEnd.y = edgeB.z;

        dir2d.x = target.f[0] - prop->pos.x;
        dir2d.y = target.f[2] - prop->pos.z;

        tmp = geoCircleSweepFrac(width, &centre, &dir2d, &edgeStart, &edgeEnd);

        stan = prop->stan;

        partialpos.x = prop->pos.x + ((dir2d.x * tmp) * 0.99000001f);
        partialpos.y = target.f[1];
        partialpos.z = prop->pos.z + ((dir2d.y * tmp) * 0.99000001f);

        if ((stanTestLineUnobstructed(&stan, prop->pos.x, prop->pos.z, partialpos.x, partialpos.z, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, ymax, ymin, 0.0f, 1.0f) != 0) && (stan != NULL))
        {
            if (stanTestVolume(&stan, partialpos.x, partialpos.z, width, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, ymax, ymin) < 0)
            {
                obj->runtime_pos.x = partialpos.x;
                obj->runtime_pos.z = partialpos.z;
                prop->stan = stan;
                prop->pos.x = partialpos.x;
                prop->pos.z = partialpos.z;
            }
        }
    }
    else
    {
        hitpos->x = target.f[0];
        hitpos->y = target.f[1];
        hitpos->z = target.f[2];
    }

    tmp = partialpos.y;
    prop->pos.y = tmp;
    obj->runtime_pos.y = tmp;
    result = 0;

done:
    return result;
}


/**
 * US address 7F042EB4.
*/
s32 objMove(struct ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal, s32 canEmbed, s32 moveOnlyIfClear)
{
    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) && (obj->projectile->flags & PROJECTILEFLAG_STICKY))
    {
        return projectileMove(obj, targetpos, hitpos, hitnormal, canEmbed, moveOnlyIfClear);
    }

    return objTryMovePropWithCollision(obj, targetpos, hitpos, hitnormal);
}


/**
 * Update a speed and distance travelled, factoring in acceleration,
 * deceleration and the global update multiplier.
 *
 * The new speed and distance done are written back to those pointers.
 *
 * offsets: 077A48, 7F042F18
 * (copied from Perfect Dark)
 */
void chrobjApplySpeed(f32 *openPosition, f32 maxFrac, f32 *speedPtr, f32 accel, f32 decel, f32 maxSpeed)
{
    f32 speed = *speedPtr;
    s32 i;

    for (i = 0; i < g_ClockTimer; i++)
    {
        f32 limit = speed * speed * 0.5f / decel;
        f32 distRemaining = maxFrac - *openPosition;
        if (distRemaining > 0.0f)
        {
            if (speed > 0.0f && distRemaining <= limit)
            {
                // Slow down for end
                speed -= decel;

                if (speed < decel)
                {
                    speed = decel;
                }
            }
            else if (speed < maxSpeed)
            {
                // Accelerate
                if (speed < 0.0f)
                {
                    speed += decel;
                }
                else
                {
                    speed += accel;
                }

                if (speed > maxSpeed)
                {
                    speed = maxSpeed;
                }
            }

            if (speed >= distRemaining)
            {
                *openPosition = maxFrac;
                break;
            }

            *openPosition += speed;
        }
        else
        {
            if (speed < 0.0f && -distRemaining <= limit)
            {
                speed += decel;

                if (speed > -decel)
                {
                    speed = -decel;
                }
            }
            else if (speed > -maxSpeed)
            {
                if (speed > 0.0f)
                {
                    speed -= decel;
                }
                else
                {
                    speed -= accel;
                }

                if (speed < -maxSpeed)
                {
                    speed = -maxSpeed;
                }
            }

            if (speed <= distRemaining)
            {
                *openPosition = maxFrac;
                break;
            }

            *openPosition += speed;
        }
    }
    *speedPtr = speed;
}


void chrobjCallsApplySpeed(f32 *openPosition, f32 maxFrac, f32 *speedPtr, f32 accel, f32 decel, f32 maxSpeed)
{
    if (maxFrac - *openPosition < -M_PI_F)
    {
        maxFrac += M_TAU_F;
    }
    else if (maxFrac - *openPosition >= M_PI_F)
    {
        maxFrac -= M_TAU_F;
    }

    chrobjApplySpeed(openPosition, maxFrac, speedPtr, accel, decel, maxSpeed);

    if (*openPosition < 0.0f)
    {
        *openPosition = *openPosition + M_TAU_F;
    }

    if (*openPosition >= M_TAU_F)
    {
        *openPosition = *openPosition - M_TAU_F;
    }
}


/**
 * This function handles an object's transition from the bouncing state to the rest state.
 */
void objSettle(ObjectRecord *obj, coord3d *up)
{
    coord3d angles;
    Mtxf rotmtx;
    Mtxf aimmtx;
    Mtxf scalemtx;
    f32 x;
    f32 z;
    f32 invlen;
    f32 tmp;
    f32 modelscale;
    f32 angle;
    Projectile *projectile;

    if (((obj->runtime_bitflags &= ~0x00010000) & 0x80) == 0) {
        return;
    }

    projectile = obj->projectile;

    if (((u8 *)obj)[3] == 1) {
        projectileFree(projectile);
        obj->projectile = 0;
        obj->runtime_bitflags &= ~0x80;
        return;
    }

    projectile->ownerprop = 0;
    projectile->flags &= ~PROJECTILEFLAG_AIRBORNE;
    projectile->flags &= ~PROJECTILEFLAG_STICKY;

    mtx4GetRotation(&obj->mtx, &angles);
    mtx4LoadRotation(&angles, &rotmtx);
    quatFromAngles(&angles, &projectile->startRot);

    mtx4TransposeRotation(&rotmtx, &aimmtx);
    mtx4Mult(&aimmtx, &obj->mtx, &scalemtx);

    projectile->scaleX = sqrtf(
        ((scalemtx.m[0][0] * scalemtx.m[0][0])
        + (scalemtx.m[0][1] * scalemtx.m[0][1]))
        + (scalemtx.m[0][2] * scalemtx.m[0][2]));

    projectile->scaleY = sqrtf(
        ((scalemtx.m[1][0] * scalemtx.m[1][0])
        + (scalemtx.m[1][1] * scalemtx.m[1][1]))
        + (scalemtx.m[1][2] * scalemtx.m[1][2]));

    projectile->scaleZ = sqrtf(
        ((scalemtx.m[2][0] * scalemtx.m[2][0])
        + (scalemtx.m[2][1] * scalemtx.m[2][1]))
        + (scalemtx.m[2][2] * scalemtx.m[2][2]));

    x = obj->mtx.m[0][0];
    z = obj->mtx.m[0][2];

    if ((x != 0.0f) || (z != 0.0f)) {
        invlen = 1.0f / sqrtf((x * x) + (z * z));
        x *= invlen;
        z *= invlen;
    } else {
        z = 1.0f;
        x = 0.0f;
    }

    aimmtx.m[0][0] = x;
    aimmtx.m[0][1] = 0.0f;
    aimmtx.m[0][2] = z;
    aimmtx.m[0][3] = 0.0f;

    if (obj->mtx.m[1][1] >= 0.0f) {
        aimmtx.m[1][0] = 0.0f;
        aimmtx.m[1][1] = 1.0f;
        aimmtx.m[1][2] = 0.0f;
        aimmtx.m[1][3] = 0.0f;

        aimmtx.m[2][0] = -z;
        aimmtx.m[2][1] = 0.0f;
        aimmtx.m[2][2] = x;
        aimmtx.m[2][3] = 0.0f;
    } else {
        aimmtx.m[1][0] = 0.0f;
        aimmtx.m[1][1] = -1.0f;
        aimmtx.m[1][2] = 0.0f;
        aimmtx.m[1][3] = 0.0f;

        aimmtx.m[2][0] = z;
        aimmtx.m[2][1] = 0.0f;
        aimmtx.m[2][2] = -x;
        aimmtx.m[2][3] = 0.0f;
    }

    aimmtx.m[3][0] = 0.0f;
    aimmtx.m[3][1] = 0.0f;
    aimmtx.m[3][2] = 0.0f;
    aimmtx.m[3][3] = 1.0f;

    mtx4GetRotation(&aimmtx, &angles);
    quatFromAngles(&angles, &projectile->restRot);
    quatMakeShortestPath(&projectile->startRot, &projectile->restRot);

    projectile->rotFrac = 0.0f;

    angle = mathAcos(
        ((aimmtx.m[0][0] * rotmtx.m[0][0])
        + (aimmtx.m[0][1] * rotmtx.m[0][1]))
        + (aimmtx.m[0][2] * rotmtx.m[0][2]));

    if (((angle > 0.0f) && (obj->mtx.m[0][1] > 0.0f))
        && (up->y < obj->mtx.m[0][1]))
    {
        projectile->rotSpeed = 0.050000001f / ((angle * 4.0f) / M_TAU_F);
    } else if (((angle > 0.0f) && (obj->mtx.m[0][1] < 0.0f))
        && (obj->mtx.m[0][1] < up->y))
    {
        projectile->rotSpeed = 0.050000001f / ((angle * 4.0f) / M_TAU_F);
    } else {
        modelscale = obj->model->scale;

        tmp = mathAcos(
            (((up->x * obj->mtx.m[0][0])
            + (up->y * obj->mtx.m[0][1]))
            + (obj->mtx.m[0][2] * up->z))
            / (modelscale * modelscale));

        tmp = tmp / g_GlobalTimerDelta;

        if (angle != 0.0f) {
            projectile->rotSpeed = tmp / angle;
        } else {
            projectile->rotSpeed = 1.0f;
        }
    }

    if (projectile->rotSpeed < 0.0f) {
        projectile->rotSpeed = -projectile->rotSpeed;
    }

    if (projectile->rotSpeed < 0.029999999f) {
        projectile->rotSpeed = 0.029999999f;
    } else if (0.15000001f < projectile->rotSpeed) {
        projectile->rotSpeed = 0.15000001f;
    }
}


/**
 * Plays and maintains the sound of the whoosh of a throwing knife spinning through the air.
 */
void objUpdateThrowKnifeSound(ObjectRecord *obj) 
{
    if (!(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)) 
    {
        return;
    }

    if ((obj->projectile->flags & PROJECTILEFLAG_AIRBORNE) && ((s32)obj->projectile->numBounces <= 0) && (obj->runtime_bitflags & RUNTIMEBITFLAG_THROWING_KNIFE_RELATED)) 
    {

        s16 Throwing_knife_SFX[] = {0x5F, 0x60, 0x61};
        s32 slot;
        s32 sfxindex;

        slot = obj->projectile->soundSlot;
        sfxindex = randomGetNext() % 3;

#if defined(VERSION_US)
        if ((s32)obj->projectile->lastSfxTimer < g_GlobalTimer - 6) {
#else
        if ((s32)obj->projectile->lastSfxTimer < g_GlobalTimer - 5) 
        {
#endif
            if (obj->projectile->sounds[slot] != NULL) 
            {
                if (sndGetPlayingState(obj->projectile->sounds[slot])) 
                {
                    sndDeactivate(obj->projectile->sounds[slot]);
                }
            }
        }

        if (obj->projectile->sounds[slot] != NULL) 
        {
            return;
        }

        if (!lvGetControlsLocked()) 
        {
            sndPlaySfx(g_musicSfxBufferPtr, Throwing_knife_SFX[sfxindex], &obj->projectile->sounds[slot]);

            chrobjSndCreatePostEventDefault(obj->projectile->sounds[slot], &obj->prop->pos);

            obj->projectile->lastSfxTimer = g_GlobalTimer;
            obj->projectile->soundSlot = 1 - slot;
        }
    } 
    else 
    {
        obj->runtime_bitflags &= ~PROJECTILEFLAG_00000020;

        if (obj->projectile->sounds[0] != NULL)
        {
            if (sndGetPlayingState(obj->projectile->sounds[0])) 
            {
                sndDeactivate(obj->projectile->sounds[0]);
            }
        }

        if (obj->projectile->sounds[1] != NULL) 
        {
            if (sndGetPlayingState(obj->projectile->sounds[1])) 
            {
                sndDeactivate(obj->projectile->sounds[1]);
            }
        }
    }
}




//[80030B74	00	Bond]
u32 monAnim00Bond[] = {
    MONUSEIMAGE(IMGBOND),
    MONHORZSCROLL(0x400, 20),
    MONHOLDTIME(20),
    MONVERTSCROLL(0x400, 20),
    MONRGBA(COLOR_BLACK, 20),
    MONHOLDTIME(20),
    MONZOOMSQUARE(0x200, 20),
    MONRGBA(COLOR_WHITE, 20),
    MONHOLDTIME(20),
    MONZOOMSQUARE(0x400, 20),
    MONHOLDTIME(20),
    MONLOOP()
};

//[80030C00	01	Desktops, Satellite]
u32 monAnim01DesktopsSatellite[] = {
     MONUSEIMAGE(IMG2DMATH),
     MONHORZSCROLL(0x400, 20),
     MONHOLDTIME(20),
     MONVERTSCROLL(0x400, 20),
     MONRGBA(COLOR_BLACK, 20),
     MONHOLDTIME(20),
     MONZOOMSQUARE(0x200, 20),
     MONRGBA(COLOR_WHITE, 20),
     MONHOLDTIME(20),
     MONZOOMSQUARE(0x400, 20),
     MONHOLDTIME(20),
     MONUSEIMAGE(IMGSATELLITE),
     MONHORZSCROLL(0x400, 20),
     MONHOLDTIME(20),
     MONVERTSCROLL(0x400, 20),
     MONRGBA(COLOR_BLACK, 20),
     MONHOLDTIME(20),
     MONZOOMSQUARE(0x200, 20),
     MONRGBA(COLOR_WHITE, 20),
     MONHOLDTIME(20),
     MONZOOMSQUARE(0x400, 20),
     MONHOLDTIME(20),
     MONUSEIMAGE(IMGDESKTOP),
     MONHORZSCROLL(0x400, 20),
     MONHOLDTIME(20),
     MONVERTSCROLL(0x400, 20),
     MONRGBA(COLOR_BLACK, 20),
     MONHOLDTIME(20),
     MONZOOMSQUARE(0x200, 20),
     MONRGBA(COLOR_WHITE, 20),
     MONHOLDTIME(20),
     MONZOOMSQUARE(0x400, 20),
     MONHOLDTIME(20),
     MONUSEIMAGE(IMGDESKTOPSTAGGERED),
     MONHORZSCROLL(0x400, 20),
     MONHOLDTIME(20),
     MONVERTSCROLL(0x400, 20),
     MONRGBA(COLOR_BLACK, 20),
     MONHOLDTIME(20),
     MONZOOMSQUARE(0x200, 20),
     MONRGBA(COLOR_WHITE, 20),
     MONHOLDTIME(20),
     MONZOOMSQUARE(0x400, 20),
     MONHOLDTIME(20),
     MONLOOP(),
};

//[80030E24	02	10 screens: astrological]
u32 monAnim02Astrological[] = {
     MONUSEIMAGE(IMGSHUTTLE1), MONHOLDTIME(80),
     MONUSEIMAGE(IMGSHUTTLE2), MONHOLDTIME(80),
     MONUSEIMAGE(IMGEARTHFULL1), MONHOLDTIME(80),
     MONUSEIMAGE(IMGEARTHFULL2), MONHOLDTIME(80),
     MONUSEIMAGE(IMGBLUESTARS), MONHOLDTIME(80),
     MONUSEIMAGE(IMGGALAXY1), MONHOLDTIME(80),
     MONUSEIMAGE(IMGGALAXY2), MONHOLDTIME(80),
     MONUSEIMAGE(IMGEARTHTEXT), MONHOLDTIME(80),
     MONUSEIMAGE(IMGTARGETEARTH), MONHOLDTIME(80),
     MONUSEIMAGE(IMGGALAXY3), MONHOLDTIME(80),
     MONLOOP(),
};

//[80030EC8	0F	7 screens: satellite, targetting, ]
u32 monAnim0FSatelliteTargeting[] = {
     MONUSEIMAGE(IMGEARTH), MONHOLDTIME(80),
     MONUSEIMAGE(IMGDESKTOPBANG), MONHOLDTIME(80),
     MONUSEIMAGE(IMGHEATMAP), MONHOLDTIME(80),
     MONUSEIMAGE(IMG2DMATH), MONHOLDTIME(80),
     MONUSEIMAGE(IMGSATELLITE), MONHOLDTIME(80),
                        MONHOLDTIME(80),
     MONUSEIMAGE(IMGTARGETEARTH), MONHOLDTIME(80),
     MONUSEIMAGE(IMGEARTHFULL2), MONHOLDTIME(80),
     MONLOOP()
};

//[80030F44	03	3 wave patterns]
u32 monAnim03ThreeWavePattern[] = {
     MONRGBA(COLOR_MINESHAFT3, 1),
     MONUSEIMAGE(IMGSINE),
     MONHORZSCROLL(0x800, 120),
     MONHOLDTIME(120),
     MONZOOMWIDTH(0x100, 1),
     MONZOOMHEIGHT(0x200, 60),
     MONHORZSCROLL(0xFFFFE000, 120),
     MONHOLDTIME(120),
     MONZOOMWIDTH(0x400, 1),
     MONZOOMHEIGHT(0x400, 60),
     MONZOOMHEIGHT(0x400, 60),
     MONHORZSCROLL(0x800, 120),
     MONHOLDTIME(120),
     MONZOOMWIDTH(0x80, 1),
     MONZOOMHEIGHT(0x800, 60),
     MONZOOMHEIGHT(0x400, 120),
     MONVERTSCROLL(0x400, 60),
     MONHORZSCROLL(0x200, 120),
     MONHOLDTIME(120),
     MONLOOP()
};

//[80031018	04	wave pattern]
u32 monAnim04WavePattern[] = {
     MONRGBA(COLOR_MINESHAFT3, 1),
     MONUSEIMAGE(IMGSINE),
     MONZOOMWIDTH(0x80, 1),
     MONZOOMHEIGHT(0x800, 60),
     MONZOOMHEIGHT(0x400, 120),
     MONVERTSCROLL(0x400, 10),
     MONHORZSCROLL(0x200, 40),
     MONHOLDTIME(120),
     MONLOOP()
};

//[80031074	05	green text up]
u32 monAnim05GreenTextUp[] = {
     MONUSEIMAGE(IMGTEXT),
     MONRGBA(COLOR_BARELYGREENOPAQUE, 1),
     MONVERTSCROLL(0xFFFFFE00, 80),
     MONHOLDTIME(120),
     MONVERTSCROLL(0xFFFFFF00, 20),
     MONHOLDTIME(120),
     MONVERTSCROLL(0xFFFFFF80, 10),
     MONHOLDTIME(40),
     MONVERTSCROLL(0xFFFFFE00, 40),
     MONHOLDTIME(60),
     MONVERTSCROLL(0xFFFFFFC0, 30),
     MONHOLDTIME(120),
     MONLOOP()
};

//[800310F0	06	red text down]
u32 monAnim06RedTextDown[] = {
     MONUSEIMAGE(IMGTEXT),
     MONRGBA(COLOR_DIESEL, 1),
     MONVERTSCROLL(0x200, 80),
     MONHOLDTIME(120),
     MONVERTSCROLL(0x100, 20),
     MONHOLDTIME(120),
     MONVERTSCROLL(0x80, 10),
     MONHOLDTIME(40),
     MONVERTSCROLL(0x200, 40),
     MONHOLDTIME(60),
     MONVERTSCROLL(0x40, 30),
     MONHOLDTIME(120),
     MONVERTSCROLL(0x100, 20),
     MONHOLDTIME(120),
     MONVERTSCROLL(0x80, 10),
     MONLOOP()
};

//[8003118C	07	d. green text down]
u32 monAnim07GreenTextDown[] = {
     MONUSEIMAGE(IMGTEXT),
     MONRGBA(COLOR_DEEPFIR, 1),
     MONVERTSCROLL(0x200, 80),
     MONHOLDTIME(120),
     MONVERTSCROLL(0x80, 10),
     MONHOLDTIME(40),
     MONVERTSCROLL(0x100, 20),
     MONHOLDTIME(120),
     MONVERTSCROLL(0x80, 10),
     MONHOLDTIME(40),
     MONVERTSCROLL(0x200, 40),
     MONHOLDTIME(60),
     MONVERTSCROLL(0x40, 30),
     MONHOLDTIME(120),
     MONLOOP()
};

//[8003121C	08	red bar graph +]
u32 monAnim08RedBarGraph[] = {
     MONUSEIMAGE(IMGBARS),
     MONRGBA(COLOR_VERDUNGREEN, 1),
     MONHORZSCROLL(0x280, 1),
     MONHOLDTIME(10),
     MONLOOP()
};

//[80031248	09	blue bar graph +]
u32 monAnim09BlueBarGraph[] = {
     MONUSEIMAGE(IMGBARS),
     MONRGBA(COLOR_CYPRUS, 1),
     MONHORZSCROLL(0x280, 1),
     MONHOLDTIME(10),
     MONLOOP()
};

//[80031274	0A	green bar graph -]
u32 monAnim0AGreenBarGraph[] = {
     MONUSEIMAGE(IMGBARS),
     MONRGBA(COLOR_TOMTHUMB, 1),
     MONHORZSCROLL(0xFFFFFD80, 1),
     MONHOLDTIME(10),
     MONLOOP()
};

//[800312A0	subroutine	used by radar]
u32 monAnimRadarSub1[] = {
     MONRGBA(COLOR_GREEN, 20),
     MONJUMPTO(monAnimRadarSub3)
};

//[800312B4	subroutine	used by radar]
u32 monAnimRadarSub2[] = {
     MONRGBA(COLOR_SANFELIX, 20),
     MONJUMPTO(monAnimRadarSub3)
};

//[800312C8	subroutine	used by radar]
u32 monAnimRadarSub3[] = {
     MONROTATEIMAGE(0xB6),
     MONHOLDTIME(1),
     MONJUMPCHANCE(monAnimRadarSub1, TWO_PERCENT_CHANCE),
     MONJUMPCHANCE(monAnimRadarSub2, 0x147A),
     MONLOOP()
};

//[800312F4	0B	radar]
u32 monAnim0BRadar[] = {
     MONUSEIMAGE(IMGTRIANGLE),
     MONRGBA(COLOR_ALMOSTDARKGREEN, 1),
     MONJUMPTO(monAnimRadarSub2)
};

//[80031310	0C	spinning cube]
u32 monAnim0CSpinningCube[] = {
     MONUSEIMAGE(IMGCUBE1),
     MONRGBA(COLOR_MINSK, 30),
     MONHOLDTIME(5),
     MONUSEIMAGE(IMGCUBE2),
     MONHOLDTIME(5),
     MONUSEIMAGE(IMGCUBE3),
     MONHOLDTIME(5),
     MONUSEIMAGE(IMGCUBE4),
     MONHOLDTIME(5),
     MONLOOP()
};

//[80031360	10	global map]
u32 monAnim10GlobalMap[] = {
     MONUSEIMAGE(IMGWORLDMAP),
     MONRGBA(COLOR_SEAGREEN, 30),
     MONHORZSCROLL(0xFFFFFC00, 1024),
     MONHOLDTIME(1440),
     MONHORZSCROLLNA(0x288, 360),
     MONVERTSCROLLNA(0x3AA, 360),
     MONZOOMSQUARE(0x80, 300),
     MONRGBA(COLOR_BLACK, 60),
     MONHOLDTIME(60),
     MONRGBA(COLOR_GRAY, 10),
     MONHOLDTIME(90),
     MONRGBA(COLOR_APPLE2, 30),
     MONHOLDTIME(30),
     MONRGBA(COLOR_LOTUS, 60),
     MONHOLDTIME(60),
     MONRGBA(COLOR_GRAY, 60),
     MONHOLDTIME(60),
     MONHORZSCROLLNA(0x200, 360),
     MONVERTSCROLLNA(0x200, 360),
     MONZOOMSQUARE(0x400, 720),
     MONHOLDTIME(300),
     MONUSEIMAGE(IMGWORLDMAP),
     MONHOLDTIME(420),
     MONRGBA(COLOR_STRONGGREEN, 30),
     MONHOLDTIME(30),
     MONRGBA(COLOR_GREENKELP, 60),
     MONHOLDTIME(60),
     MONLOOP()
};

//[80031490	0D	3 screens: location, weapon armed, ]
u32 monAnim0DLocWeapArmed[] = {
     MONRGBA(COLOR_BLACK, 1),
     MONRGBA(COLOR_SILVER, 400),
     MONUSEIMAGE(1),
     MONHOLDTIME(680),
     MONUSEIMAGE(2),
     MONHOLDTIME(680),
     MONUSEIMAGE(4),
     MONHOLDTIME(180),
     MONRGBA(COLOR_PESTO, 1),
     MONUSEIMAGE(4),
     MONHOLDTIME(200),
     MONLOOP()
};

//[800314F8	0E	red target]
u32 monAnim0ERedTarget[] = {
     MONZOOMSQUARE(0x400, 1),
     MONRGBA(COLOR_THUNDERBIRD, 1),
     MONUSEIMAGE(6),
     MONHOLDTIME(600),
     MONRGBA(COLOR_SILVER, 5),
     MONHOLDTIME(5),
     MONRGBA(COLOR_MINESHAFT, 60),
     MONUSEIMAGE(IMGSTATIC),
     MONRGBA(COLOR_CODGRAY, 100),
     MONHOLDTIME(400),
     MONLOOP()
};

//[8003156C	11	Karl yelling]
u32 monAnim11KarlYelling[] = {
     MONRGBA(COLOR_DARKGREEN, 0),
     MONUSEIMAGE(IMGTALK1),
     MONHOLDTIME(5),
     MONUSEIMAGE(IMGTALK2),
     MONHOLDTIME(5),
     MONUSEIMAGE(IMGTALK3),
     MONHOLDTIME(5),
     MONUSEIMAGE(IMGTALK4),
     MONHOLDTIME(10),
     MONUSEIMAGE(IMGTALK2),
     MONHOLDTIME(5),
     MONLOOP()
};

//[800315CC	12	skateboard]
u32 monAnim12Skateboard[] = {
     MONUSEIMAGE(IMGSKATEBOARD4),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONHORZSCROLL(0x264, 30),
     MONUSEIMAGE(IMGSKATEBOARD1),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONUSEIMAGE(IMGSKATEBOARD2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONUSEIMAGE(IMGSKATEBOARD3),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONHORZSCROLL(0x19C, 40),
     MONUSEIMAGE(IMGSKATEBOARD2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONUSEIMAGE(IMGSKATEBOARD1),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONRGBA(COLOR_DARKGREEN, 0),
     MONHOLDTIME(3),
     MONRGBA(COLOR_DARKERGREEN, 0),
     MONHOLDTIME(2),
     MONLOOP()
};

//[80031848	13	police guy]
u32 monAnim13PoliceGuy[] = {
    MONRGBA(COLOR_DARKGREEN2, 0),
    MONUSEIMAGE(IMGFIST1),
    MONHOLDTIME(5),
    MONUSEIMAGE(IMGFIST2),
    MONHOLDTIME(5),
    MONUSEIMAGE(IMGFIST3),
    MONHOLDTIME(5),
    MONUSEIMAGE(IMGFIST4),
    MONHOLDTIME(5),
    MONLOOP()
};

//[80031898	14	'off']
u32 monAnim14Off[] = {
    MONUSEIMAGE(IMGSINE),
    MONRGBA(COLOR_BARELYGREEN, 1),
    MONHOLDTIME(5),
    MONLOOP()
};

//[800318B8	15	randomly select one of seven animations]
u32 monAnim15RandomSeven[] = {
    MONJUMPCHANCE(monAnim04WavePattern, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim11KarlYelling, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim08RedBarGraph, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim09BlueBarGraph, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim0AGreenBarGraph, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim06RedTextDown, TWENTY_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim07GreenTextDown, FOURTY_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim05GreenTextUp, HUNDRED_PERCENT_CHANCE),
    MONLOOP()
};

//[8003191C	16	randomly select random screens + random effects or boring]
u32 monAnim16RandomFour[] = {
    MONJUMPCHANCE(monAnim03ThreeWavePattern, TWO_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim08RedBarGraph, TWO_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim05GreenTextUp, TWO_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim17RandImageEffect, SIXTY_PERCENT_CHANCE),
    MONLOOP()
};

//[80031950	17	Base Function for random screens + random effects]
u32 monAnim17RandImageEffect[] = {
    MONJUMPCHANCE(monRandEffectChanceSHUTTLE1, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceSHUTTLE2, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceEARTHFULL1, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceEARTHFULL2, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceBLUESTARS, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceGALAXY1, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceGALAXY2, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceEARTHTEXT, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceTARGETEARTH, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandEffectChanceGALAXY3, TEN_PERCENT_CHANCE),
    MONHOLDTIME(100),
    MONLOOP()
};

//[800319D4	18	random screens + random effects - set image]
u32 monRandEffectChanceSHUTTLE1[] = {
    MONUSEIMAGE(IMGSHUTTLE1),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[800319F0	19	random screens + random effects - set image]
u32 monRandEffectChanceSHUTTLE2[] = {
    MONUSEIMAGE(IMGSHUTTLE2),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031A0C	1A	random screens + random effects - set image]
u32 monRandEffectChanceEARTHFULL1[] = {
    MONUSEIMAGE(IMGEARTHFULL1),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031A28	1B	random screens + random effects - set image]
u32 monRandEffectChanceEARTHFULL2[] = {
    MONUSEIMAGE(IMGEARTHFULL2),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031A44	1C	random screens + random effects - set image]
u32 monRandEffectChanceBLUESTARS[] = {
    MONUSEIMAGE(IMGBLUESTARS),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031A60	1D	random screens + random effects - set image]
u32 monRandEffectChanceGALAXY1[] = {
    MONUSEIMAGE(IMGGALAXY1),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031A7C	1E	random screens + random effects - set image]
u32 monRandEffectChanceGALAXY2[] = {
    MONUSEIMAGE(IMGGALAXY2),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031A98	1F	random screens + random effects - set image]
u32 monRandEffectChanceEARTHTEXT[] = {
    MONUSEIMAGE(IMGEARTHTEXT),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031AB4	20	random screens + random effects - set image]
u32 monRandEffectChanceTARGETEARTH[] = {
    MONUSEIMAGE(IMGTARGETEARTH),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031AD0	21	random screens + random effects - set image]
u32 monRandEffectChanceGALAXY3[] = {
    MONUSEIMAGE(IMGGALAXY3),
    MONHOLDTIME(20),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, HUNDRED_PERCENT_CHANCE)
};

//[80031AEC	22	random screens + random effects - colourizer]
u32 monRandChanceScrollOrZoomRandRGBN[] = {
    MONJUMPCHANCE(monRandChanceScrollOrZoomRed, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandChanceScrollOrZoomGreen, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monRandChanceScrollOrZoomBlue, TEN_PERCENT_CHANCE),
    MONRGBA(COLOR_SILVER, 60),
    MONJUMPTO(monRandChanceScrollOrZoom)
};

//[80031B24	23	random screens + random effects - colourizer]
u32 monRandChanceScrollOrZoomRed[] = {
    MONRGBA(COLOR_PERSIANRED, 60),
    MONJUMPTO(monRandChanceScrollOrZoom)
};

u32 monRandChanceScrollOrZoomGreen[] = {
    MONRGBA(COLOR_APPLE, 60),
    MONJUMPTO(monRandChanceScrollOrZoom)
};

u32 monRandChanceScrollOrZoomBlue[] = {
    MONRGBA(COLOR_GOVERNORBAY, 60),
    MONJUMPTO(monRandChanceScrollOrZoom)
};

u32 monRandChanceScrollOrZoom[] = {
    MONHOLDTIME(50),
    MONJUMPCHANCE(monAnim27RandomEffectScrollRight, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim28RandomEffectScrollUpFast, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim29RandomEffectScrollUp, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim2ARandEffectScrollZoom1, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim2ARandEffectScrollZoom2, TEN_PERCENT_CHANCE),
    MONHOLDTIME(300),
    MONJUMPTO(monAnim2CRandEffectWaitRoute)
};


//[80031BB4	27	random screens + random effects - scroll right]
u32 monAnim27RandomEffectScrollRight[] = {
    MONHORZSCROLL(0x800, 120),
    MONHOLDTIME(120),
    MONJUMPTO(monAnim2CRandEffectWaitRoute)
};

//[80031BD0	28	random screens + random effects - scroll up fast]
u32 monAnim28RandomEffectScrollUpFast[] = {
    MONVERTSCROLL(0x2000, 50),
    MONHOLDTIME(200),
    MONJUMPTO(monAnim2CRandEffectWaitRoute)
};

//[80031BEC	29	random screens + random effects - scroll up]
u32 monAnim29RandomEffectScrollUp[] = {
    MONVERTSCROLL(0x2000, 200),
    MONHOLDTIME(200),
    MONJUMPTO(monAnim2CRandEffectWaitRoute)
};

//[80031C08	2A	random screens + random effects - scroll and zoom]
u32 monAnim2ARandEffectScrollZoom1[] = {
    MONHORZSCROLLNA(0x288, 300),
    MONVERTSCROLLNA(0x3AA, 300),
    MONZOOMSQUARE(0x80, 200),
    MONHOLDTIME(300),
    MONHORZSCROLLNA(0x200, 50),
    MONVERTSCROLLNA(0x200, 200),
    MONZOOMSQUARE(0x400, 720),
    MONHOLDTIME(600),
    MONJUMPTO(monAnim2CRandEffectWaitRoute)
};

//[80031C80	2B	random screens + random effects - scroll and zoom]
u32 monAnim2ARandEffectScrollZoom2[] = {
    MONHORZSCROLLNA(0x320, 400),
    MONVERTSCROLLNA(0x190, 400),
    MONZOOMSQUARE(0x80, 200),
    MONHOLDTIME(300),
    MONHORZSCROLLNA(0xC8, 200),
    MONVERTSCROLLNA(0x190, 800),
    MONZOOMSQUARE(0x200, 720),
    MONHOLDTIME(800),
    MONZOOMSQUARE(0x400, 720),
    MONHORZSCROLLNA(0x200, 100),
    MONVERTSCROLLNA(0x200, 60),
    MONHOLDTIME(500),
    MONJUMPTO(monAnim2CRandEffectWaitRoute)
};

//[80031D30	2C	random screens + random effects - wait and route]
u32 monAnim2CRandEffectWaitRoute[] = {
    MONHOLDTIME(50),
    MONJUMPCHANCE(monRandChanceScrollOrZoomRandRGBN, TEN_PERCENT_CHANCE),
    MONJUMPCHANCE(monAnim2DRandEffectFlash, TWENTY_PERCENT_CHANCE),
    MONJUMPTO(monAnim17RandImageEffect)
};

//[80031D58	2D	random screens + random effects - flash]
u32 monAnim2DRandEffectFlash[] = {
    MONHOLDTIME(50),
    MONRGBA(COLOR_WHITE, 10),
    MONRGBA(COLOR_BLACK, 5),
    MONRGBA(COLOR_WHITE, 10),
    MONHOLDTIME(25),
    MONRGBA(COLOR_BLACK, 200),
    MONHOLDTIME(500),
    MONJUMPTO(monAnim17RandImageEffect)
};

//[80031DA8	2E	red brightening screen]
u32 monAnim2ERedBrightening[] = {
     MONUSEIMAGE(IMGKEYBOARDKEY),
     MONZOOMSQUARE(0x200, 0),
     MONRGBA(COLOR_ALIZARINCRIMSON, 60),
     MONHOLDTIME(60),
     MONRGBA(COLOR_MINESHAFT2, 10),
     MONHOLDTIME(10),
     MONLOOP()
};

//[80031DF4	2F	green brightening screen]
u32 monAnim2FGreenBrightening[] = {
     MONUSEIMAGE(IMGKEYBOARDKEY),
     MONZOOMSQUARE(0x200, 0),
     MONRGBA(COLOR_APPLE, 60),
     MONHOLDTIME(60),
     MONRGBA(COLOR_MINESHAFT2, 10),
     MONHOLDTIME(10),
     MONLOOP()
};

//[80031E40	30	grey solid]
u32 monAnim30GreySolid[] = {
     MONUSEIMAGE(IMGKEYBOARDKEY),
     MONZOOMSQUARE(0x200, 0),
     MONRGBA(COLOR_MINESHAFT2, 10),
     MONHOLDTIME(10),
     MONLOOP()
};

//[80031E78	31	red solid]
u32 monAnim31RedSolid[] = {
     MONUSEIMAGE(IMGKEYBOARDKEY),
     MONZOOMSQUARE(0x200, 0),
     MONRGBA(COLOR_ALIZARINCRIMSON, 10),
     MONHOLDTIME(10),
     MONLOOP()
};

//[80031EB0	32	green solid]
u32 monAnim32GreenSolid[] = {
     MONUSEIMAGE(IMGKEYBOARDKEY),
     MONZOOMSQUARE(0x200, 0),
     MONRGBA(COLOR_APPLE, 10),
     MONHOLDTIME(10),
     MONLOOP()
};

//[80031EE8	33	black solid]
u32 monAnim33BlackSolid[] = {
     MONUSEIMAGE(0),
     MONRGBA(COLOR_BLACK, 0),
     MONSTOPANIM()
};

//[80031F00	34	???	Not Included in Normal List - linked @ 0x9544]
u32 monAnim34[] = {
     MONZOOMSQUARE(0x400, 0),
     MONHOLDTIME(1),
     MONZOOMSQUARE(0x1000, 20),
     MONHOLDTIME(20),
     MONLOOP()
};

//[80031F44	35	Taser	Not Included in Normal List!]
u32 monAnim35Taser[] = {
     MONUSEIMAGE(IMGBOND),
     MONHORZSCROLL(0x400, 20), MONHOLDTIME(20),
     MONVERTSCROLL(0x400, 20), MONRGBA(COLOR_BLACK, 20), MONHOLDTIME(20),
     MONZOOMSQUARE(0x200, 20), MONRGBA(COLOR_WHITE, 20), MONHOLDTIME(20),
     MONZOOMSQUARE(0x400, 20), MONHOLDTIME(20),
     MONLOOP()
};


void mtxAlignToNormal(coord3d *normal, Mtxf *out)
{
    f32 fwdX;
    f32 fwdY;
    f32 fwdZ;
    f32 sideX;
    f32 sideZ;
    f32 f0;
    f32 nx;
    f32 ny;
    f32 nz;
    f32 a;
    f32 b;
    f32 yaw;
    f32 pitch;
    Mtxf unyaw;
    Mtxf yawmtx;
    Mtxf pitchmtx;
    coord3d fwd;

    f0 = sqrtf(normal->f[0] * normal->f[0] + normal->f[1] * normal->f[1] + normal->f[2] * normal->f[2]);

    nx = normal->x / f0;
    ny = normal->y / f0;
    nz = normal->z / f0;

    if (nx == 0.0f && nz == 0.0f)
    {
        fwdX = 0.0f;
        fwdY = 0.0f;
        fwdZ = ny;
        sideX = 1.0f;
        sideZ = 0.0f;
    }
    else
    {
        a = sqrtf(nx * nx + nz * nz);
        b = nx / a;

        sideX = nz / a;
        sideZ = -b;

        fwdX = ny * b;
        fwdY = -a;
        fwdZ = ny * sideX;
    }

    yaw = mathAtan2(sideX, sideZ);

    mtx4LoadYRotation(-yaw, &unyaw);

    fwd.x = fwdX;
    fwd.y = fwdY;
    fwd.z = fwdZ;

    mtx4RotateVecInPlace(&unyaw, &fwd);

    pitch = mathAtan2(fwd.x, fwd.y);

    mtx4LoadYRotation(-1.5707964f + yaw, &yawmtx);
    mtx4LoadXRotation(-1.5707964f - pitch, &pitchmtx);

    mtx4Mult(&yawmtx, &pitchmtx, out);
}

void objPlaceOnSurface(ObjectRecord* obj, coord3d* pos, StandTile* stan, coord3d* normal)
{
    Mtxf matrix;
    f32 ymin;

    mtxAlignToNormal(normal, &matrix);
    mtx4ScaleRows012(obj->model->scale, &matrix);
    objChangeShading(obj, pos, &matrix, stan);

    ymin = bboxGetYmin(chrobjGetBboxFromObjFile(obj->model->obj));

    obj->runtime_pos.f[0] -= ymin * obj->mtx.m[1][0];
    obj->runtime_pos.f[1] -= ymin * obj->mtx.m[1][1];
    obj->runtime_pos.f[2] -= ymin * obj->mtx.m[1][2];

    chrobjCollisionRelated(obj);
}


bool objEmbed(PropRecord *prop, PropRecord *parent, Model *model, ModelNode *node)
{
    if (parent->flags & PROPFLAG_ONSCREEN)
    {
        ObjectRecord *obj = prop->obj;

        Mtxf mtx1;
        Mtxf mtx2;
        Mtxf mtx3;
        Mtxf* nodemtx;

        obj->embedment = embedmentAllocate();

        if (obj->embedment)
        {
            nodemtx = modelFindNodeMtx(model, node, 0);

            obj->runtime_bitflags |= RUNTIMEBITFLAG_EMBEDDED;

            propRemoveFromRooms(prop);
            propDelist(prop);
            propDisable(prop);

            obj->model->attachedto = model;
            obj->model->attachedto_objinst = node;

            propReparent(prop, parent);

            mtx4Copy(&obj->mtx, &mtx1);
            mtx4SetTranslation(&obj->runtime_pos, &mtx1);
            mtx4MultAffine(camGetViewToWorldMtxf(), nodemtx, &mtx2);
            mtx4InvertAffine(&mtx2, &mtx3);
            mtx4MultAffine((Mtxf* ) &mtx3.m, &mtx1, &obj->embedment->matrix);

            return TRUE;
        }
    }

    return FALSE;
}


/**
 * Named same as Perfect Dark.
*/
s32 propExplode(PropRecord *prop, s32 /* enum EXPLOSION_DEF */ explosionType)
{
    ObjectRecord *prop_obj;
    s32 playernum;
    s32 ret;
    struct PropRecord *parent;
    struct StandTile *stan;
    coord3d pos;
    Mtxf *mtx;

    prop_obj = prop->obj;
    playernum = (prop_obj->runtime_bitflags & RUNTIMEBITFLAG_OWNER) >> RUNTIMEBITSHIFT_OWNER;

    if (prop->parent)
    {
        parent = prop->parent;

        while (parent->parent)
        {
			parent = parent->parent;
		}

        stan = parent->stan;

        if (prop->flags & PROPFLAG_ONSCREEN)
        {
            mtx = modelGetRootMtx(prop_obj->model);

            pos.x = mtx->m[3][0];
			pos.y = mtx->m[3][1];
			pos.z = mtx->m[3][2];

            mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &pos);
        }
        else
        {
            pos.x = parent->pos.x;
			pos.y = parent->pos.y;
			pos.z = parent->pos.z;
        }

        if ((parent->flags & PROPFLAG_00000008) == 0
            && stanWalkLine(&stan, parent->pos.f[0], parent->pos.f[2], pos.x, pos.z))
        {
    ret =
            explosionCreate(0, &pos, stan, (s16) explosionType, (prop_obj->flags & 0xE) == 0, playernum, parent->rooms, 0);
        }
        else
        {
    ret =
            explosionCreate(0, &pos, stan, (s16) explosionType, 0, playernum, parent->rooms, 1);
        }
    }
    else
    {
    ret =
        explosionCreate(
            0,
            &prop_obj->runtime_pos,
            prop->stan,
            (s16) explosionType,
            (prop_obj->flags & 0xE) == 0 && (prop->flags & PROPFLAG_00000008) == 0,
            playernum,
            prop->rooms,
            (prop->flags & PROPFLAG_00000008) != 0);
    }

    return ret;
}


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
                countdownTimerSetVisible(2, FALSE);
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

                if ((diff_x * diff_x) + (diff_y * diff_y) + (diff_z * diff_z) < PROXIMITY_MINE_TRIGGER_DISTANCE)
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
	struct WeaponObjRecord *weaponObj = NULL;
	struct WeaponObjRecord *airborneWeapon;
	coord3d newpos;
	s32 objMovedThisFrame;
	bool moveOnlyIfPathClear;
	bool canEmbed;

    /**
     * 0 - the move was blocked by geometry
     * 1 - the move completed unobstructed
     * 2 - the move was blocked and the caller asked for embedding
     */
	s32 moveResult;

	coord3d collisionPoint;
	coord3d collisionNormal;
	struct PropRecord *hitprop;
	struct StandTile *savedStan;
	struct ModelRoData_BoundingBoxRecord *projectileBBox;
	struct ModelRoData_BoundingBoxRecord *objectBBox;
	Mtxf *objectMatrix;
	Mtxf *projectileMatrix;
	f32 bboxBottomOffset;
	coord3d previousXAxis; // snapshot of the object's local X axis before this tick's rotation
	s32 hitGround;
	s32 bounceCondition;
	s32 projectileStopped;
	s32 projectileAlive;
	f32 startY;
	f32 floorY;
	f32 gravity;
	f32 speedY;
	f32 nextVerticalSpeed;
	f32 previousVerticalSpeed;
	f32 impulse;
	f32 friction;
	quatf rot;
	coord3d smokepos;
	coord3d bloodStainPos;
	Mtxf inverseNodeMatrix;
	struct ALSoundState_s *sfx_state;

	// Doors and glass
	struct DoorRecord *door;
	struct DoorRecord *linked;
	struct TintedGlassRecord *glass;
	f32 previousOpenPosition; // Start-of-tick snapshot for how open a door is.
	s32 allLinkedClosed;
	s32 closePortal;
	Mtxf *lidmtx;
	f32 lidAngle;
	f32 bladeAngle;
	f32 openAngle;
	f32 bladeStart;

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

	objMovedThisFrame = 0;
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
			projectile = obj->projectile;
			projectile->age += g_ClockTimer;

			if (((s32) projectile->age) > PROJECTILE_LIFETIME_FRAMES)
			{
				obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
			}

			if (projectile->flags & PROJECTILEFLAG_00000100)
			{
				moveOnlyIfPathClear = TRUE;

				if (obj->type == PROPDEF_COLLECTABLE)
				{
					weaponObj = (struct WeaponObjRecord *) obj;
                    
					if (weaponObj->weaponnum == ITEM_ROCKETROUND)
					{
						moveOnlyIfPathClear = FALSE;
					}
				}

				if (projectile->ownerprop != NULL)
				{
					propSetMoving(projectile->ownerprop, 0);
				}

				moveResult = objMove(obj, &projectile->targetPos, &collisionPoint, &collisionNormal, 0, moveOnlyIfPathClear);

				if (projectile->ownerprop != NULL)
				{
					propSetMoving(projectile->ownerprop, 1);
				}

                /*
                * Fragile code: PROJECTILEFLAG_00000100 is only set by gunLaunchProjectileFromPlayer()
                * whose callers always pass a PROPDEF_COLLECTABLE WeaponObjRecord.
                * Therefore weaponObj is initialized above on every valid path reaching this point.
                * But if this flag is ever used with another object type, the NULL check
                * below would read an uninitialized pointer.
                */
				if (((moveResult != 1) && (weaponObj != NULL)) && (weaponObj->weaponnum == ITEM_ROCKETROUND))
				{
					weaponObj->timer = 0;
				}

				projectile->flags &= ~PROJECTILEFLAG_00000100;
			}

			newpos.f[0] = obj->runtime_pos.f[0];
			newpos.f[1] = obj->runtime_pos.f[1];
			newpos.f[2] = obj->runtime_pos.f[2];

			if (projectile->refreshrate > 0)
			{
				projectile->refreshrate -= g_ClockTimer;
			}

			if (obj->projectile->flags & PROJECTILEFLAG_AIRBORNE)
			{
				airborneWeapon = (struct WeaponObjRecord *) obj;
				projectileBBox = chrobjGetBboxFromObjectRecord(obj);
				hitGround = 0;
				bounceCondition = 0;
				projectileStopped = 0;
				bboxBottomOffset = 1.0f;
				startY = obj->runtime_pos.f[1];
				canEmbed = FALSE;
				projectile->airTime += g_ClockTimer;
				previousXAxis.f[0] = obj->mtx.m[0][0];
				previousXAxis.f[1] = obj->mtx.m[0][1];
				previousXAxis.f[2] = obj->mtx.m[0][2];

				if (projectile->flags & PROJECTILEFLAG_00000020)
				{
					if (projectile->riseTime < ROCKET_INITIAL_GRAVITY_MODIFIER)
					{
						projectile->riseSpeed += projectile->accel.f[1] * g_GlobalTimerDelta;
						projectile->riseY += projectile->riseSpeed * g_GlobalTimerDelta;
                        // Gravity modifier increases at 1/90 per frame until reaching ROCKET_INITIAL_GRAVITY_MODIFIER
                        // I would have thought this is somehow related to turning 9.8m/s/s into per frame accel? 
						projectile->riseTime += (1.0f / 90.0f) * g_GlobalTimerDelta;
						if (projectile->riseTime > ROCKET_INITIAL_GRAVITY_MODIFIER)
						{
							projectile->riseTime = ROCKET_INITIAL_GRAVITY_MODIFIER;
						}
					}
					else if (newpos.f[1] < projectile->riseY)
					{
						projectile->riseSpeed += projectile->accel.f[1] * g_GlobalTimerDelta;
						projectile->riseY += projectile->riseSpeed * g_GlobalTimerDelta;
                        //Smooth vertical interpolation when rocket is rising toward its ballistic apex.
						newpos.f[1] += (0.07f * (projectile->riseY - newpos.f[1])) * g_GlobalTimerDelta;
					}
					else
					{
						newpos.f[1] = projectile->riseY;
						projectile->flags &= ~PROJECTILEFLAG_00000020;
						projectile->riseTime = 0.0f;
						projectile->flags |= PROJECTILEFLAG_POWERED;
						projectile->speed.f[1] = projectile->riseSpeed;
					}
				}

				gravity = PROP_PROJECTILE_GRAVITY_MODIFIER;

				if (!(projectile->flags & PROJECTILEFLAG_POWERED))
				{
					projectile->speed.f[1] += (projectile->accel.f[1] + projectile->riseTime) * g_GlobalTimerDelta;
					speedY = projectile->speed.f[1];
					nextVerticalSpeed = speedY - (gravity * g_GlobalTimerDelta);
                    //apparently Standard trapezoidal integrator. (y += (dt * (v + v_next)) * 0.5f)
					newpos.f[1] += (g_GlobalTimerDelta * (speedY + nextVerticalSpeed)) * 0.5f;
					projectile->speed.f[1] = nextVerticalSpeed;
				}
				else
				{
					projectile->speed.f[1] += (projectile->accel.f[1] + projectile->riseTime) * g_GlobalTimerDelta;
					newpos.f[1] += projectile->speed.f[1] * g_GlobalTimerDelta;
				}

				objectMatrix = &obj->mtx;
				projectileMatrix = &projectile->mtx;
				projectile->speed.f[0] += projectile->accel.f[0] * g_GlobalTimerDelta;
				projectile->speed.f[2] += projectile->accel.f[2] * g_GlobalTimerDelta;
				newpos.f[0] += projectile->speed.f[0] * g_GlobalTimerDelta;
				newpos.f[2] += projectile->speed.f[2] * g_GlobalTimerDelta;
				physApplyTumble(objectMatrix, projectileMatrix, g_ClockTimer);

                // Determine which projectiles can stick to surfaces.
				if ((obj->type == PROPDEF_COLLECTABLE) && (((((((airborneWeapon->weaponnum == ITEM_REMOTEMINE) || (airborneWeapon->weaponnum == ITEM_TIMEDMINE)) || (airborneWeapon->weaponnum == ITEM_PROXIMITYMINE)) || (airborneWeapon->weaponnum == ITEM_BOMBCASE)) || (airborneWeapon->weaponnum == ITEM_BUG)) || (airborneWeapon->weaponnum == ITEM_MICROCAMERA)) || (airborneWeapon->weaponnum == ITEM_PLASTIQUE)))
				{
					canEmbed = TRUE;
				}

				if (projectile->ownerprop != NULL)
				{
					propSetMoving(projectile->ownerprop, 0);
				}

				moveResult = objMove(obj, &newpos, &collisionPoint, &collisionNormal, canEmbed, 0);

				if (projectile->ownerprop != NULL)
				{
					propSetMoving(projectile->ownerprop, 1);
				}

				objMovedThisFrame = 1;

				hitprop = g_ProjectileHitProp;

				// Stick to whatever was hit, unless it's a guard, a player or
				// another projectile in flight.
				if ((moveResult == 2) && ((hitprop == NULL) || ((hitprop->type != PROP_TYPE_CHR) && (hitprop->type != PROP_TYPE_VIEWER))))
				{
					if ((hitprop == NULL) || !(hitprop->obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
					{
						projectileFree(projectile);
						obj->projectile = NULL;
						obj->runtime_bitflags &= ~RUNTIMEBITFLAG_HASPROJECTILE;
						if (prop->flags & PROPFLAG_00000008)
						{
							prop->flags |= PROPFLAG_00000010;
						}

						chrobjSndCreatePostEventDefault(sndPlaySfx(g_musicSfxBufferPtr, ATTACH_MINE_SFX, NULL), &prop->pos);
						objectivesCheckDeposit(((struct WeaponObjRecord *) obj)->weaponnum, prop->stan->room);
						objPlaceOnSurface(obj, &collisionPoint, prop->stan, &collisionNormal);
						if (g_ProjectileHitProp != NULL)
						{
							savedStan = prop->stan;
							if (objEmbed(prop, g_ProjectileHitProp, g_CurrentProjectileModel, g_ProjectileHitNode) != 0)
							{
								prop->stan = savedStan;
								tickop = TICKOP_CHANGEDLIST;
								projectileStopped = 1;
							}
						}
					}
				}

				if (projectileStopped == 0)
				{
					hitprop = g_ProjectileHitProp;

					if ((hitprop != NULL) && (obj->type == PROPDEF_COLLECTABLE))
					{
						if (airborneWeapon->weaponnum == ITEM_THROWKNIFE)
						{
							if ((hitprop->type == PROP_TYPE_CHR) || (((hitprop->type == PROP_TYPE_VIEWER) && (hitprop->obj != NULL)) && (playerGetIndexForProp(hitprop) != playerGetCurrentNum())))
							{
								if ((((projectile->flags & PROJECTILEFLAG_AIRBORNE) && (((s32) projectile->numBounces) <= 0)) && (obj->runtime_bitflags & RUNTIMEBITFLAG_THROWING_KNIFE_RELATED)) && (chrTakeShot(hitprop->chr, bodypartshot, &g_ProjectileHitPos, ((struct WeaponObjRecord *) obj)->weaponnum, 1) != 0))
								{
									projectileStopped = 1;

									if (projectile->bounciness > 0.0f)
									{
										impulse = ((projectile->speed.f[0] * collisionNormal.f[0]) + (projectile->speed.f[1] * collisionNormal.f[1])) + (projectile->speed.f[2] * collisionNormal.f[2]);
										impulse *= -(projectile->bounciness + 1.0f);
										projectile->speed.f[0] += impulse * collisionNormal.f[0];
										projectile->speed.f[1] += impulse * collisionNormal.f[1];
										projectile->speed.f[2] += impulse * collisionNormal.f[2];
									}

									if (!(projectile->flags & PROJECTILEFLAG_00000200))
									{
										physRandomTumble(projectileMatrix);
									}

									projectile->numBounces += 1;
									shotPlayHitPropSound(((struct WeaponObjRecord *) obj)->weaponnum, g_ProjectileHitProp, -1);

									if (((g_ProjectileHitProp->flags & PROPFLAG_ONSCREEN) && (bodypartshot != HIT_GUN)) && (bodypartshot != HIT_HAT))
									{
										nodemtx = modelFindNodeMtx(g_CurrentProjectileModel, g_ProjectileHitNode, 0);

										bloodStainPos.f[0] = collisionPoint.f[0];
										bloodStainPos.f[1] = collisionPoint.f[1];
										bloodStainPos.f[2] = collisionPoint.f[2];

										mtx4TransformVecInPlace(camGetWorldToScreenMtxf(), &bloodStainPos);

										bloodStainPos.f[0] += (bloodStainPos.f[0] - nodemtx->m[3][0]) * 0.5f;
										bloodStainPos.f[1] += (bloodStainPos.f[1] - nodemtx->m[3][1]) * 0.5f;
										bloodStainPos.f[2] += (bloodStainPos.f[2] - nodemtx->m[3][2]) * 0.5f;

										bloodStainPos.f[0] -= (modelGetNodeRadius(g_CurrentProjectileModel, g_ProjectileHitNode) * 0.5f) * g_ProjectileHitNormal.f[0];
										bloodStainPos.f[1] -= (modelGetNodeRadius(g_CurrentProjectileModel, g_ProjectileHitNode) * 0.5f) * g_ProjectileHitNormal.f[1];
										bloodStainPos.f[2] -= (modelGetNodeRadius(g_CurrentProjectileModel, g_ProjectileHitNode) * 0.5f) * g_ProjectileHitNormal.f[2];
    
										mtx4InvertRigid(nodemtx, &inverseNodeMatrix);
										mtx4TransformVecInPlace(&inverseNodeMatrix, &bloodStainPos);
										chrCreateBloodStain(g_CurrentProjectileModel, bodypartshot, g_ProjectileHitNode, &bloodStainPos);
									}
								}
							}
						}
						else if (airborneWeapon->weaponnum == ITEM_ROCKETROUND)
						{
							projectileStopped = 1;
							if (hitprop->type == PROP_TYPE_CHR)
							{
								chrExplosionDamage((ChrRecord *) hitprop->chr, &obj->runtime_pos, 2.0f, 1);
							}
							else if ((hitprop->type == PROP_TYPE_OBJ) || (hitprop->type == PROP_TYPE_WEAPON))
							{
								objApplyDamage(hitprop->obj, 100.0f, &obj->runtime_pos, ITEM_ROCKETROUND, (obj->runtime_bitflags & RUNTIMEBITFLAG_OWNER) >> RUNTIMEBITSHIFT_OWNER);
							}

							((struct WeaponObjRecord *) obj)->timer = 0;
						}
					}
				}

				if (projectileStopped == 0)
				{
					if (moveResult == 0)
					{
						if (projectile->bounciness > 0.0f)
						{
							previousVerticalSpeed = projectile->speed.f[1];
							impulse = ((projectile->speed.f[0] * collisionNormal.f[0]) + (projectile->speed.f[1] * collisionNormal.f[1])) + (projectile->speed.f[2] * collisionNormal.f[2]);
							impulse *= -(projectile->bounciness + 1.0f);
							projectile->speed.f[0] += impulse * collisionNormal.f[0];
							projectile->speed.f[1] += impulse * collisionNormal.f[1];
							projectile->speed.f[2] += impulse * collisionNormal.f[2];

							if ((previousVerticalSpeed <= 0.0f) && ((projectile->speed.f[1] >= 0.0f) || (startY <= obj->runtime_pos.f[1])))
							{
								bounceCondition = 1;
							}
						}
					}

					floorY = 0.0f;

					if (!(projectile->flags & PROJECTILEFLAG_00000008))
					{
						floorY = stanGetFloorY(prop->stan, prop->pos.f[0], prop->pos.f[2]);
						bboxBottomOffset = bboxRotatedMinY(projectileBBox, &objectMatrix[0]);
						hitGround = (prop->pos.f[1] < (floorY - bboxBottomOffset));
					}

					if ((hitGround) || (moveResult == 0))
					{
						if (!(projectile->flags & PROJECTILEFLAG_00000200))
						{
							physRandomTumble(projectileMatrix);
						}

						projectile->numBounces += 1;
					}

					if ((hitGround) || (bounceCondition))
					{
						if (hitGround)
						{
                            // The projectile has hit the ground so raise it 4 units above the ground.
							obj->runtime_pos.f[1] = (prop->pos.f[1] = (floorY - bboxBottomOffset) + 4.0f);
						}
						else
						{
							obj->runtime_pos.f[1] = (prop->pos.f[1] = (collisionPoint.f[1] - bboxBottomOffset) + 4.0f);
						}

						if (!(obj->runtime_bitflags & RUNTIMEBITFLAG_00010000))
						{
							obj->runtime_bitflags |= RUNTIMEBITFLAG_00000100;
						}

						if (projectile->bounciness > 0.0f)
						{
							projectile->speed.f[1] *= -projectile->bounciness;

							if (projectile->speed.f[1] < 2.2222223f)
							{
								if ((projectile->flags & PROJECTILEFLAG_00000002) && (projectile->numBounces == 1))
								{
									projectile->speed.f[1] = 2.2222223f;
								}
								else
								{
									objSettle(obj, &previousXAxis);
								}
							}
						}
						else
						{
							objSettle(obj, &previousXAxis);
						}
					}

					if (obj->type == PROPDEF_COLLECTABLE)
					{
						if (airborneWeapon->weaponnum == ITEM_THROWKNIFE)
						{
							objUpdateThrowKnifeSound(airborneWeapon);
						}
						else if (airborneWeapon->weaponnum == ITEM_ROCKETROUND)
						{
							if ((moveResult == 0) || (hitGround))
							{
								airborneWeapon->timer = 0;
							}
							else
							{
								nextVerticalSpeed = ((projectile->speed.f[0] * projectile->speed.f[0]) + (projectile->speed.f[1] * projectile->speed.f[1])) + (projectile->speed.f[2] * projectile->speed.f[2]);

								if (nextVerticalSpeed > ROCKET_SPEED_BREAK_THRESHOLD)
								{
									projectile->accel.f[0] = 0.0f;
									projectile->accel.f[1] = 0.0f;
									projectile->accel.f[2] = 0.0f;
								}

								if (((s32) projectile->airTime) >= GRENADE_SMOKE_FRAMES)
								{
									projectile->riseTime = 0.0f;
									projectile->flags &= ~(PROJECTILEFLAG_POWERED | PROJECTILEFLAG_00000020);
								}
								else
								{
									explosionCreateSmoke(&airborneWeapon->runtime_pos, prop->stan, 8, prop->rooms, (prop->flags & PROPFLAG_00000008) != 0);
								}
							}
						}
						else if (airborneWeapon->weaponnum == ITEM_GRENADEROUND)
						{
							if ((hitGround != 0) || (bounceCondition != 0))
							{
								airborneWeapon->timer = 0;
							}
							else
							{
								explosionCreateSmoke(&obj->runtime_pos, prop->stan, 9, prop->rooms, (prop->flags & PROPFLAG_00000008) != 0);
							}
						}

						if ((moveResult == 0) || (hitGround != 0))
						{
							if (((s32) projectile->lastHitSoundTick) < (((s32) g_LvTickCount) - 2))
							{
								if ((airborneWeapon->weaponnum == ITEM_THROWKNIFE) || (airborneWeapon->weaponnum == ITEM_KNIFE))
								{
									sfx_state = sndPlaySfx(g_musicSfxBufferPtr, KNIFE_HIT_WALL_SFX, NULL);
								}
								else
								{
									sfx_state = sndPlaySfx(g_musicSfxBufferPtr, DROP_GUN_SFX, NULL);
								}

								chrobjSndCreatePostEventDefault(sfx_state, &prop->pos);
							}

							projectile->lastHitSoundTick = g_LvTickCount;
						}
					}
				}

				if (((airborneWeapon->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) && (projectile->flags & PROJECTILEFLAG_FALLING)) && (!(g_LvTickCount & 7)))
				{
					smokepos.f[0] = airborneWeapon->runtime_pos.f[0] + 400.0f;
					smokepos.f[1] = airborneWeapon->runtime_pos.f[1] - 1800.0f;
					smokepos.f[2] = airborneWeapon->runtime_pos.f[2];

					if (!(g_LvTickCount & 0xF))
					{
						smokepos.f[2] += 400.0f;
					}
					else
					{
						smokepos.f[2] -= 400.0f;
					}

					explosionCreate(NULL, &smokepos, airborneWeapon->prop->stan, 0x14, 0, 0, airborneWeapon->prop->rooms, 0);
                    //Spawn smoke every 40 frames
					if ((((s32) g_LvTickCount) % 40) == 0)
					{
						explosionCreateSmoke(&smokepos, airborneWeapon->prop->stan, 0xA, airborneWeapon->prop->rooms, 1);
					}
				}
			}
			else
			{
				projectileAlive = 1;

				if (projectile->rotFrac < 1.0f)
				{
					projectile->rotFrac += projectile->rotSpeed * g_GlobalTimerDelta;

					if (g_ClockTimer > 0)
					{
						projectile->rotSpeed *= 1.1f;
					}

					if ((projectile->rotFrac > 1.0f) || (projectile->flags & PROJECTILEFLAG_00000008))
					{
						projectile->rotFrac = 1.0f;
					}

					quatSlerp(&projectile->startRot, &projectile->restRot, projectile->rotFrac, &rot);
					objectMatrix = &obj->mtx;
					quatToMtx4(&rot, &obj->mtx);
					mtx4ScaleRow0(projectile->scaleX, objectMatrix);
					mtx4ScaleRow1(projectile->scaleY, objectMatrix);
					mtx4ScaleRow2(projectile->scaleZ, objectMatrix);
					projectileAlive = 0;
				}

                // Apply horizontal slide and friction to a projectile that has landed.
				if ((((projectile->speed.f[0] != 0.0f) || (projectile->speed.f[2] != 0.0f)) || (projectile->rotFrac < 1.0f)) && (!(projectile->flags & PROJECTILEFLAG_00000008)))
				{
					objectMatrix = &obj->mtx;
					objectBBox = chrobjGetBboxFromObjectRecord(obj);
					projectileAlive = 0;

					for (i = 0; i < g_ClockTimer; i++)
					{
						newpos.f[0] += projectile->speed.f[0];
						newpos.f[2] += projectile->speed.f[2];
						if (projectile->rotFrac >= 1.0f)
						{
							if (projectile->groundFriction > 0.0f)
							{
								friction = (projectile->groundFriction * g_GlobalTimerDelta) / sqrtf((projectile->speed.f[0] * projectile->speed.f[0]) + (projectile->speed.f[2] * projectile->speed.f[2]));
								if (friction >= 1.0f)
								{
									projectile->speed.f[0] = 0.0f;
									projectile->speed.f[2] = 0.0f;
								}
								else
								{
									projectile->speed.f[0] -= projectile->speed.f[0] * friction;
									projectile->speed.f[2] -= projectile->speed.f[2] * friction;
								}
							}
							else
							{
								projectile->speed.f[0] *= PROJECTILE_FRICTION_FACTOR;
								projectile->speed.f[2] *= PROJECTILE_FRICTION_FACTOR;
							}
						}
					}

					objMove(obj, &newpos, &collisionPoint, &collisionNormal, 0, 0);
					objMovedThisFrame = 1;
					floorY = stanGetFloorY(prop->stan, prop->pos.f[0], prop->pos.f[2]);
					obj->runtime_pos.f[1] = prop->pos.f[1] = (floorY - bboxRotatedMinY(objectBBox, objectMatrix)) + 4.0f;
					if ((projectile->speed.f[0] < 0.1f) && (projectile->speed.f[0] > (-0.1f)))
					{
						if ((projectile->speed.f[2] < 0.1f) && (projectile->speed.f[2] > (-0.1f)))
						{
							projectile->speed.f[2] = 0.0f;
							projectile->speed.f[0] = 0.0f;
						}
					}
				}

				if ((projectileAlive != 0) || (projectile->flags & PROJECTILEFLAG_00000008))
				{
					projectileFree(projectile);
					obj->projectile = NULL;
					obj->runtime_bitflags &= ~RUNTIMEBITFLAG_HASPROJECTILE;
					if (prop->flags & PROPFLAG_00000008)
					{
						prop->flags |= PROPFLAG_00000010;
					}

					if (obj->type == PROPDEF_COLLECTABLE)
					{
						objectivesCheckDeposit(((struct WeaponObjRecord *) obj)->weaponnum, prop->stan->room);
					}
				}
			}
		}

		if (objMovedThisFrame != 0)
		{
			chrobjCollisionRelated(obj);
			setupUpdateObjectRoomPosition(obj);
			doorGetTileShading(obj->prop, &obj->nextcol);
			proxminesDetonateNear(&obj->runtime_pos);
		}

		if (obj->type == PROPDEF_DOOR)
		{
			door = (struct DoorRecord *) prop->obj;
			previousOpenPosition = door->openPosition;
#if defined(VERSION_EU)
			if ((((((s32) door->openedTime) > 0) && (door->openstate == DOORSTATE_STATIONARY)) && (!(door->flags & PROPFLAG_DOOR_KEEPOPEN))) && (((s32) door->openedTime) < (((s32) g_GlobalTimer) - ((((s32) door->autoCloseFrames) * 50) / 60))))
#else
			if ((((((s32) door->openedTime) > 0) && (((s32) door->openedTime) < (((s32) g_GlobalTimer) - ((s32) door->autoCloseFrames)))) && (door->openstate == DOORSTATE_STATIONARY)) && (!(door->flags & PROPFLAG_DOOR_KEEPOPEN)))
#endif
			{
				doorActivate(door, DOORSTATE_CLOSING);
			}

			if (door->openstate == DOORSTATE_WAITING)
			{
				linked = door->linkedDoor;
				allLinkedClosed = 1;
				while ((linked != NULL) && (linked != door))
				{
					if ((linked->openstate != DOORSTATE_STATIONARY) || (linked->openPosition > 0.0f))
					{
						allLinkedClosed = 0;
					}

					linked = linked->linkedDoor;
				}


				if (allLinkedClosed != 0)
				{
					doorSetOpenState(door, 1);
				}
			}

			if (((door->doorType == DOORTYPE_FALLAWAY) && (doorIsClosed(door) != 0)) && (doorIsPadlockFree(door) != 0))
			{
				doorActivateWrapper(prop);
			}

			if ((door->lastcalc60i < g_GlobalTimer) || (g_ClockTimer == 0))
			{
				doorsUpdatePositions(door);
			}
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

				chrobjCallsApplySpeed(&autogun->yaw, autogun->homeYaw, &autogun->yawSpeed, AUTOGUN_YAW_ACCEL_PER_FRAME, AUTOGUN_YAW_ACCEL_PER_FRAME, AUTOGUN_YAW_MAX_SPEED);
				chrobjCallsApplySpeed(&autogun->pitch, autogun->homePitch, &autogun->pitchSpeed, AUTOGUN_PITCH_ACCEL_PER_FRAME, AUTOGUN_PITCH_ACCEL_PER_FRAME, AUTOGUN_PITCH_MAX_SPEED);
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

				chrobjCallsApplySpeed(&autogun->yaw, aimYaw, &autogun->yawSpeed, AUTOGUN_ALERT_ACCEL_PER_FRAME  , AUTOGUN_ALERT_ACCEL_PER_FRAME  , autogun->speed);
				chrobjCallsApplySpeed(&autogun->pitch, aimPitch, &autogun->pitchSpeed, AUTOGUN_ALERT_ACCEL_PER_FRAME  , AUTOGUN_ALERT_ACCEL_PER_FRAME  , autogun->speed);
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
				engineVolume = sndVolumeForPos(&vehicle->runtime_pos, 2000.0f, 3000.0f);
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

				chrobjCallsApplySpeed(&vehicle->roty, targetYaw, &vehicle->turnrot60, TRUCK_TURN_ACCEL_PER_FRAME, TRUCK_TURN_DECEL_PER_FRAME, TRUCK_TURN_MAX_SPEED);
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
						doorGetTileShading(prop, &vehicle->nextcol);
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
				engineVolume = sndVolumeForPos(&aircraft->runtime_pos, 5000.0f, 6000.0f);
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
				doorDeactivatePortal(door);
			}
			else
			{
				doorActivatePortal(door);
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
		visible = ((!(obj->runtime_bitflags & RUNTIMEBITFLAG_00000800)) && (!(obj->flags2 & PROPFLAG2_00080000))) ? (posIsOnScreen(prop, &obj->runtime_pos, modelGetRadius(model), applyFogCull)) : (0);
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
			door = (struct DoorRecord *) prop->obj;
			doorGetMtx(door, mtxs);
			mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), mtxs);

			if (model->obj->Skeleton == (&skeleton_eyelid_door))
			{
				lidAngle = M_TAU_F - ((door->openPosition * M_TAU_F) / 360.0f);
				lidmtx = &mtxs[1];
				nodepos = &model->obj->Switches[1]->Data->Group.Origin;
				mtx4LoadXRotation(lidAngle, lidmtx);
				mtx4SetTranslation((coord3d *) nodepos, lidmtx);
				mtx4MultInPlace(mtxs, lidmtx);
				lidmtx = &mtxs[2];
				nodepos = &model->obj->Switches[2]->Data->Group.Origin;
				mtx4LoadXRotation(M_TAU_F - lidAngle, lidmtx);
				mtx4SetTranslation((coord3d *) nodepos, lidmtx);
				mtx4MultInPlace(mtxs, lidmtx);
			}
			else if (model->obj->Skeleton == (&skeleton_iris_door))
			{
				bladeAngle = 0.0f;
				openAngle = (door->openPosition * M_TAU_F) / 360.0f;
				bladeStart = door->maxFrac * 0.3f;
				if (bladeStart < door->openPosition)
				{
					bladeAngle = (((door->maxFrac * (door->openPosition - bladeStart)) / (door->maxFrac - bladeStart)) * M_TAU_F) / 360.0f;
					if (isSimOwner)
					{
						if (previousOpenPosition <= bladeStart)
						{
							chrobjSndCreatePostEventDefault(sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_OPEN_SFX, NULL), &prop->pos);
						}
					}
				}
				else if (isSimOwner)
				{
					if (bladeStart < previousOpenPosition)
					{
						chrobjSndCreatePostEventDefault(sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_CLOSE_SFX, NULL), &prop->pos);
					}
				}

				// Six blades, each a pivot node (tilted by bladeAngle) with a
				// child node (turned by openAngle).
				for (i = 0; i < 6; i++)
				{
					Mtxf *blademtx = &mtxs[i * 2 + 1];
					Mtxf *tipmtx = &mtxs[i * 2 + 2];

					mtx4LoadZRotation(bladeAngle, blademtx);
					mtx4SetTranslation(&model->obj->Switches[i * 2 + 1]->Data->Group.Origin, blademtx);
					mtx4MultInPlace(mtxs, blademtx);
					mtx4LoadZRotation(openAngle, tipmtx);
					mtx4SetTranslation(&model->obj->Switches[i * 2 + 2]->Data->Group.Origin, tipmtx);
					mtx4MultInPlace(blademtx, tipmtx);
				}
			}
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

	if (obj->runtime_bitflags & RUNTIMEBITFLAG_00000100)
	{
		obj->runtime_bitflags &= ~RUNTIMEBITFLAG_00000100;
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
						chrobjSndCreatePostEventDefault(autogun->fireSound1, &prop->pos);
					}
					else if (autogun->fireSound2 == NULL)
					{
						sndPlaySfx(g_musicSfxBufferPtr, GUN_B9_CANNON_SHORT_SFX, &autogun->fireSound2);
						chrobjSndCreatePostEventDefault(autogun->fireSound2, &prop->pos);
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


void monitorSetAnimation(MonitorRecord *mon, void *image)
{
    mon->cmdlist  = image;
    mon->offset = 0;
}


void monitorSetImageByNum(MonitorRecord *mon, s32 monAnimID)
{
    u32 *image = monAnim00Bond;
    switch (monAnimID)
    {
         default:
         case 0:
            break;
         case 1:
            image = monAnim01DesktopsSatellite;
            break;
        case 2:
            image = monAnim02Astrological;
            break;
        case 3:
            image = monAnim03ThreeWavePattern;
            break;
        case 4:
            image = monAnim04WavePattern;
            break;
        case 5:
            image = monAnim05GreenTextUp;
            break;
        case 6:
            image = monAnim06RedTextDown;
            break;
        case 7:
            image = monAnim07GreenTextDown;
            break;
        case 8:
            image = monAnim08RedBarGraph;
            break;
        case 9:
            image = monAnim09BlueBarGraph;
            break;
        case 10:
            image = monAnim0AGreenBarGraph;
            break;
        case 11:
            image = monAnim0BRadar;
            break;
        case 12:
            image = monAnim0CSpinningCube;
            break;
        case 13:
            image = monAnim0DLocWeapArmed;
            break;
        case 14:
            image = monAnim0ERedTarget;
            break;
        case 15:
            image = monAnim0FSatelliteTargeting;
            break;
        case 16:
            image = monAnim10GlobalMap;
            break;
        case 17:
            image = monAnim11KarlYelling;
            break;
        case 18:
            image = monAnim12Skateboard;
            break;
        case 19:
            image = monAnim13PoliceGuy;
            break;
        case 20:
            image = monAnim14Off;
            break;
        case 21:
            image = monAnim15RandomSeven;
            break;
        case 22:
            image = monAnim16RandomFour;
            break;
        case 23:
            image = monAnim17RandImageEffect;
            break;
        case 24:
            image = monRandEffectChanceSHUTTLE1;
            break;
        case 25:
            image = monRandEffectChanceSHUTTLE2;
            break;
        case 26:
            image = monRandEffectChanceEARTHFULL1;
            break;
        case 27:
            image = monRandEffectChanceEARTHFULL2;
            break;
        case 28:
            image = monRandEffectChanceBLUESTARS;
            break;
        case 29:
            image = monRandEffectChanceGALAXY1;
            break;
        case 30:
            image = monRandEffectChanceGALAXY2;
            break;
        case 31:
            image = monRandEffectChanceEARTHTEXT;
            break;
        case 32:
            image = monRandEffectChanceTARGETEARTH;
            break;
        case 33:
            image = monRandEffectChanceGALAXY3;
            break;
        case 34:
            image = monRandChanceScrollOrZoomRandRGBN;
            break;
        case 35:
            image = monRandChanceScrollOrZoomRed;
            break;
        case 36:
            image = monRandChanceScrollOrZoomGreen;
            break;
        case 37:
            image = monRandChanceScrollOrZoomBlue;
            break;
        case 38:
            image = monRandChanceScrollOrZoom;
            break;
        case 39:
            image = monAnim27RandomEffectScrollRight;
            break;
        case 40:
            image = monAnim28RandomEffectScrollUpFast;
            break;
        case 41:
            image = monAnim29RandomEffectScrollUp;
            break;
        case 42:
            image = monAnim2ARandEffectScrollZoom1;
            break;
        case 43:
            image = monAnim2ARandEffectScrollZoom2;
            break;
        case 44:
            image = monAnim2CRandEffectWaitRoute;
            break;
        case 45:
            image = monAnim2DRandEffectFlash;
            break;
        case 46:
            image = monAnim2ERedBrightening;
            break;
        case 47:
            image = monAnim2FGreenBrightening;
            break;
        case 48:
            image = monAnim30GreySolid;
            break;
        case 49:
            image = monAnim31RedSolid;
            break;
        case 50:
            image = monAnim32GreenSolid;
            break;
        case 51:
            image = monAnim33BlackSolid;
            break;
    }
    monitorSetAnimation(mon,  image);
}


void monitorSetImage(MonitorRecord *mon, struct sImageTableEntry *image)
{
    mon->tconfig = image;
}


Gfx *monitorTickAnimation(Model *model, ModelNode *node, MonitorRecord *screen, Gfx *gdl, s32 zbufferMode, s32 blendMode)
{
    if (node && (node->Opcode & 0xff) == MODELNODE_OPCODE_DLCOLLISION) 
    {
        Vertex *vertices = dynAllocateVertices(4);
        Gfx *savedgdl = gdl++;
        union ModelRoData *rodata = node->Data;
        union ModelRwData *rwdata = modelGetNodeRwData(model, node);
        sImageTableEntry *tconfig;
        bool yielding = FALSE;

        while (!yielding) 
        {
            struct tvcmd *m = (struct tvcmd *) &screen->cmdlist[screen->offset];

            switch (m->type) 
            {
            case TVCMD_STOPSCROLL:
                screen->xmidinc = 0.0f;
                screen->ymidinc = 0.0f;
                screen->offset++;
                break;
            case TVCMD_SCROLLRELX:
                screen->xmidfrac = 0.0f;
                screen->xmidinc = 1.0f / m->duration;
                screen->xmidold = screen->xmid;
                screen->xmidnew = screen->xmid + m->value * (1.0f / 1024.0f);
                screen->offset += 3;
                break;
            case TVCMD_SCROLLRELY:
                screen->ymidfrac = 0.0f;
                screen->ymidinc = 1.0f / m->duration;
                screen->ymidold = screen->ymid;
                screen->ymidnew = screen->ymid + m->value * (1.0f / 1024.0f);
                screen->offset += 3;
                break;
            case TVCMD_SCROLLABSX:
                screen->xmidfrac = 0.0f;
                screen->xmidinc = 1.0f / m->duration;
                screen->xmidold = screen->xmid;
                screen->xmidnew = m->value * (1.0f / 1024.0f);
                screen->offset += 3;
                break;
            case TVCMD_SCROLLABSY:
                screen->ymidfrac = 0.0f;
                screen->ymidinc = 1.0f / m->duration;
                screen->ymidold = screen->ymid;
                screen->ymidnew = m->value * (1.0f / 1024.0f);
                screen->offset += 3;
                break;
            case TVCMD_SCALEABSX:
                screen->xscalefrac = 0.0f;
                screen->xscaleinc = 1.0f / m->duration;
                screen->xscaleold = screen->xscale;
                screen->xscalenew = m->value * (1.0f / 1024.0f);
                screen->offset += 3;
                break;
            case TVCMD_SCALEABSY:
                screen->yscalefrac = 0.0f;
                screen->yscaleinc = 1.0f / m->duration;
                screen->yscaleold = screen->yscale;
                screen->yscalenew = m->value * (1.0f / 1024.0f);
                screen->offset += 3;
                break;
            case TVCMD_SETTEXTURE:
                monitorSetImage(screen, (struct sImageTableEntry *)m->value); // the command holds the image pointer
                screen->offset += 2;
                break;
            case TVCMD_PAUSE:
                if (screen->pause60 >= 0) 
                {
                    screen->pause60 -= g_ClockTimer;

                    if (screen->pause60 >= 0) 
                    {
                        yielding = TRUE;
                    } 
                    else 
                    {
                        screen->offset += 2;
                    }
                } 
                else 
                {

                    yielding = TRUE;
                    screen->pause60 = m->value;
                }
                break;
            case TVCMD_SETCMDLIST:
                monitorSetAnimation(screen, (u32 *) m->value);
                break;
            case TVCMD_RANDSETCMDLIST:
                if ((randomGetNext() >> 16) < m->duration) 
                {
                    monitorSetAnimation(screen, (u32 *) m->value);
                } 
                else 
                {
                    screen->offset += 3;
                }
                break;
            case TVCMD_RESTART:
                screen->offset = 0;
                break;
            case TVCMD_YIELD:
                yielding = TRUE;
                break;
            case TVCMD_SETCOLOUR:
                screen->colfrac = 0.0f;
                screen->colinc = 1.0f / m->duration;

                screen->redold = screen->red;
                screen->rednew = ((u32)m->value >> 24) & 0xff;

                screen->greenold = screen->green;
                screen->greennew = ((u32)m->value >> 16) & 0xff;

                screen->blueold = screen->blue;
                screen->bluenew = ((u32)m->value >> 8) & 0xff;

                screen->alphaold = screen->alpha;
                screen->alphanew = m->value & 0xff;

                screen->offset += 3;
                break;
            case TVCMD_ROTATEABS:
                screen->rot = m->value * M_TAU_F / M_U16_MAX_VALUE_F;
                screen->offset += 2;
                break;
            case TVCMD_ROTATEREL:
                screen->rot += MONITOR_TIMER_DELTA * m->value * M_TAU_F / M_U16_MAX_VALUE_F;

                if (screen->rot >= M_TAU_F) 
                {
                    screen->rot -= M_TAU_F;
                }

                if (screen->rot < 0.0f) 
                {
                    screen->rot += M_TAU_F;
                }

                screen->offset += 2;
                break;
            }
        }


        // Increment X scale
        if (screen->xscaleinc > 0.0f) 
        {
            screen->xscalefrac += screen->xscaleinc * MONITOR_TIMER_DELTA;

            if (screen->xscalefrac < 1.0f) 
            {
                screen->xscale = screen->xscaleold + (screen->xscalenew - screen->xscaleold) * screen->xscalefrac;
            } 
            else 
            {
                screen->xscalefrac = 1.0f;
                screen->xscaleinc = 0.0f;
                screen->xscale = screen->xscalenew;
            }
        }

        // Increment Y scale
        if (screen->yscaleinc > 0.0f) 
        {
            screen->yscalefrac += screen->yscaleinc * MONITOR_TIMER_DELTA;

            if (screen->yscalefrac < 1.0f) 
            {
                screen->yscale = screen->yscaleold + (screen->yscalenew - screen->yscaleold) * screen->yscalefrac;
            } 
            else 
            {
                screen->yscalefrac = 1.0f;
                screen->yscaleinc = 0.0f;
                screen->yscale = screen->yscalenew;
            }
        }

        // Increment X scroll
        if (screen->xmidinc > 0.0f) 
        {
            screen->xmidfrac += screen->xmidinc * MONITOR_TIMER_DELTA;

            if (screen->xmidfrac < 1.0f) 
            {
                screen->xmid = screen->xmidold + (screen->xmidnew - screen->xmidold) * screen->xmidfrac;
            } 
            else 
            {
                screen->xmidfrac = 1.0f;
                screen->xmidinc = 0.0f;
                screen->xmid = screen->xmidnew;
            }
        }

        // Increment Y scroll
        if (screen->ymidinc > 0.0f) 
        {
            screen->ymidfrac += screen->ymidinc * MONITOR_TIMER_DELTA;

            if (screen->ymidfrac < 1.0f) 
            {
                screen->ymid = screen->ymidold + (screen->ymidnew - screen->ymidold) * screen->ymidfrac;
            } 
            else 
            {
                screen->ymidfrac = 1.0f;
                screen->ymidinc = 0.0f;
                screen->ymid = screen->ymidnew;
            }
        }

        // Increment colour change
        if (screen->colinc > 0.0f) 
        {
            screen->colfrac += screen->colinc * MONITOR_TIMER_DELTA;

            if (screen->colfrac < 1.0f) 
            {
                screen->red = screen->redold + (s32) ((screen->rednew - screen->redold) * screen->colfrac);
                screen->green = screen->greenold + (s32) ((screen->greennew - screen->greenold) * screen->colfrac);
                screen->blue = screen->blueold + (s32) ((screen->bluenew - screen->blueold) * screen->colfrac);
                screen->alpha = screen->alphaold + (s32) ((screen->alphanew - screen->alphaold) * screen->colfrac);
            } 
            else 
            {
                screen->colfrac = 1.0f;
                screen->colinc = 0.0f;
                screen->red = screen->rednew;
                screen->green = screen->greennew;
                screen->blue = screen->bluenew;
                screen->alpha = screen->alphanew;
            }
        }

        // Set up everything for rendering
        rwdata->DisplayListCollisions.gdl = gdl;
        rwdata->DisplayListCollisions.Vertices = vertices;

        vertices[0] = rodata->DisplayListCollisions.Vertices[0];
        vertices[1] = rodata->DisplayListCollisions.Vertices[1];
        vertices[2] = rodata->DisplayListCollisions.Vertices[2];
        vertices[3] = rodata->DisplayListCollisions.Vertices[3];

        if ((u32)screen->tconfig < 100) 
        {
            tconfig = &g_ImagesMonitor[(s32)screen->tconfig];
        } 
        else 
        {
            tconfig = screen->tconfig;
        }

        {

            if (tconfig != NULL) 
            {
                f32 xfrac1;
                f32 yfrac1;
                f32 xfrac2;
                f32 yfrac2;
                f32 cosrot;
                f32 sinrot;
                f32 rotscale;

                xfrac1 = screen->xscale / 2.0f;
                yfrac1 = screen->yscale / 2.0f;
                xfrac2 = xfrac1;
                yfrac2 = yfrac1;



                if (screen->rot != 0.0f) 
                {
                    cosrot = cosf(screen->rot);
                    rotscale = 1.4141999f;
                    cosrot *= rotscale;
                    sinrot = sinf(screen->rot);


                    xfrac1 *= cosrot;
                    sinrot *= rotscale;
                    yfrac1 *= sinrot;
                    xfrac2 *= sinrot;
                    yfrac2 *= cosrot;
                }



                vertices[0].s = tconfig->width  * (screen->xmid + xfrac1) * 32.0f;
                vertices[0].t = tconfig->height * (screen->ymid + yfrac1) * 32.0f;

                vertices[1].s = tconfig->width  * (screen->xmid - xfrac2) * 32.0f;
                vertices[1].t = tconfig->height * (screen->ymid + yfrac2) * 32.0f;

                vertices[2].s = tconfig->width  * (screen->xmid - xfrac1) * 32.0f;
                vertices[2].t = tconfig->height * (screen->ymid - yfrac1) * 32.0f;

                vertices[3].s = tconfig->width  * (screen->xmid + xfrac2) * 32.0f;
                vertices[3].t = tconfig->height * (screen->ymid - yfrac2) * 32.0f;
            }

        }

        if (1) 
        {
            u8 tmpc;
            u8 tmpc2;
            tmpc = screen->red;
            vertices[3].r = tmpc;
            vertices[2].r = tmpc;
            vertices[1].r = tmpc;
            vertices[0].r = tmpc;

            tmpc = screen->green;
            vertices[3].g = tmpc;
            vertices[2].g = tmpc;
            vertices[1].g = tmpc;
            vertices[0].g = tmpc;

            tmpc2 = screen->blue;
            vertices[3].b = tmpc2;
            vertices[2].b = tmpc2;
            vertices[1].b = tmpc2;
            vertices[0].b = tmpc2;

            tmpc = screen->alpha;
            vertices[3].a = tmpc;
            vertices[2].a = tmpc;
            vertices[1].a = tmpc;
            vertices[0].a = tmpc;
        }

        if (screen->alpha < 255) 
        {
            blendMode = 2;
        }

        // Render the image
        gSPSetGeometryMode(gdl++, G_CULL_BACK);

        texSelect(&gdl, tconfig, blendMode, zbufferMode, 2);

        gSPMatrix(gdl++, osVirtualToPhysical(model->render_pos), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(gdl++, SPSEGMENT_MODEL_VTX, osVirtualToPhysical(vertices));
        gSPVertex(gdl++, 0x04000000, 4, 0);
        gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPEndDisplayList(gdl++);

        gSPBranchList(savedgdl++, gdl);
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

                gdl = monitorTickAnimation(model, model->obj->Switches[0], &((MonitorObjRecord *)prop->obj)->Monitor, gdl, mN, 1);
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

                gdl = monitorTickAnimation(model, model->obj->Switches[0], &((MultiMonitorObjRecord *)o)->Monitor[0], gdl, mN, 1);

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

                gdl = monitorTickAnimation(model, model->obj->Switches[1], &((MultiMonitorObjRecord *)o)->Monitor[1], gdl, mN, 1);

                gdl = monitorTickAnimation(model, model->obj->Switches[2], &((MultiMonitorObjRecord *)o)->Monitor[2], gdl, mN, 1);

                gdl = monitorTickAnimation(model, model->obj->Switches[3], &((MultiMonitorObjRecord *)o)->Monitor[3], gdl, mN, 1);
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


ModelNode* objFindDlCollisionNode(ObjectRecord* obj)
{
    ModelFileHeader* header = obj->model->obj;
    ModelNode *node = header->RootNode;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
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


bool objNodeCanDeform(ModelFileHeader* modeldef, ModelNode* node)
{
    ModelSkeleton* onescreen = &skeleton_console_one_screen;
    ModelSkeleton* fourscreen = &skeleton_console_four_screen;

    if ((onescreen == modeldef->Skeleton))
    {
        if (node == modeldef->Switches[0])
        {
            return FALSE;
        }
    }

    if (fourscreen == modeldef->Skeleton)
    {
        if ((node == modeldef->Switches[0]) || (node == modeldef->Switches[1]) || (node == modeldef->Switches[2]) || (node == modeldef->Switches[3]))
        {
            return FALSE;
        }
    }

    return TRUE;
}


typedef struct Word4 { u32 w0; u32 w1; u32 w2; u32 w3; } Word4;


/*
*   objDeform - Deform an object due to it being destroyed.
*   PD has a very similar function of the same name
*/
void objDeform(ObjectRecord *obj, E_EXPLOSIONTYPE explosiontype)
{
    ModelNode *node;
    ModelRoData_DisplayList_CollisionRecord *rodata;
    Model *model;
    Vertex **vtxslot;
    Vertex *newverts;
    s32 adjust_height;
    s32 ymid;
    s32 deformseed;
    f32 shrink;
    bool allow_blackening;
    ModelNode *nodeCopy;
    f32 yscale;
    s32 chance;
    s32 i;
    f32 modelscale;
    s32 offset;
    s32 ymin;
    s32 ymax;
    
    model = obj->model;

    // Keep on one line for matching.
    ymin = 99999; ymax = -99999;
    
    node = objFindDlCollisionNode(obj);
    nodeCopy = node;
    
    if (nodeCopy == NULL)
    {
        return;
    }
    
    rodata = (ModelRoData_DisplayList_CollisionRecord *) nodeCopy->Data;
    
    if (rodata == NULL)
    {
        return;
    }
    
    if (!objNodeCanDeform(obj->model->obj, nodeCopy))
    {
        return;
    }
    
    vtxslot = (Vertex **) (&model->datas[rodata->RwDataIndex]);
    
#ifdef VERSION_EU
    if (obj->obj < PROP_WINDOW)
    {
        if (randomGetNext() & 1)
        {
            deformseed = (u16) object_explosion_details.seeds[obj->obj][explosiontype];
        }
        else
        {
            deformseed = (u16) object_explosion_details.seeds[obj->obj][explosiontype + 3];
        }
    }
    else
    {
        deformseed = 0;
    }
#else
    if (randomGetNext() & 1)
    {
        deformseed = (u16) object_explosion_details[obj->obj].Seed[explosiontype];
    }
    else
    {
        deformseed = (u16) object_explosion_details[obj->obj].Seed[ymid = explosiontype + 3];
    }
#endif
    
    if (deformseed == 0)
    {
        deformseed = randomGetNext();
    }
    
    explosionClearBulletImpactRoom(obj->prop);
    
    if (obj->obj == PROP_TV_HOLDER)
    {
        adjust_height = 0;
    }
    else
    {
        adjust_height = 1;
    }
    
    newverts = vtxstoreAllocate(rodata->numVertices, VTXSTORE_OBJ, model->obj, objGetDestroyedLevel(obj));
    
    if (newverts != NULL)
    {
        if ((*vtxslot) != rodata->Vertices)
        {
            for (i = 0, offset = 0; i < rodata->numVertices; i++, offset += sizeof(Vertex))
            {
                *((Word4 *) (((u8 *) newverts) + offset)) = *((Word4 *) (((u8 *) (*vtxslot)) + offset));
            }

            vtxstoreFree(*vtxslot);
        }
        else
        {
            for (i = 0, offset = 0; i < rodata->numVertices; i++, offset += sizeof(Vertex))
            {
                *((Word4 *) (((u8 *) newverts) + offset)) = *((Word4 *) (((u8 *) rodata->Vertices) + offset));
            }
        }
        
        *vtxslot = newverts;
    }
    else
    {
        if ((*vtxslot) != rodata->Vertices)
        {
            vtxstoreFree(*vtxslot);
            *vtxslot = rodata->Vertices;
            obj->runtime_bitflags |= 4;
        }
        
        shrink = 0.85000002f;
        obj->mtx.m[1][0] *= shrink;
        obj->mtx.m[1][1] *= shrink;
        obj->mtx.m[1][2] *= shrink;
        
        if (!adjust_height)
        {
            return;
        }
        
        node = (ModelNode *) obj->model->obj;
        modelscale = obj->model->scale;
        node = (ModelNode *) chrobjGetBboxFromObjFile((ModelFileHeader *) node);
        obj->prop->pos.y += (modelscale * bboxGetYmin((ModelRoData_BoundingBoxRecord *) node)) * 0.15000001f;
        obj->runtime_pos.y += (modelscale * bboxGetYmin((ModelRoData_BoundingBoxRecord *) node)) * 0.15000001f;
        return;
    }
    
    // The gray desks and wood table are exempt from blackening. DESK2 is not used in the vanilla game.
    if (((obj->obj == PROP_DESK1) || (obj->obj == PROP_DESK2)) || (obj->obj == PROP_WOODEN_TABLE1))
    {
        allow_blackening = FALSE;
    }
    else
    {
        allow_blackening = TRUE;
    }
    
    if (rodata->numVertices > 0)
    {
        i = 0;
        offset = 0;
        
        do
        {
            if (((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y < ymin)
            {
                ymin = ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y;
            }
            
            if (ymax < ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y)
            {
                ymax = ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y;
            }
            
            i++;
            offset += sizeof(Vertex);
            
        } while (offset < (rodata->numVertices << 4));
    }
    
    i = 0;
    ymid = (ymin + ymax) >> 1;

#ifdef VERSION_EU
    if (explosiontype == EXPLOSION_MEDIUM2)
#else
    if ((explosiontype * 2) == EXPLOSION_SMALL)
#endif
    {
        if ((ymid - ymin) >= 41)
        {
            ymid = ymin + 40;
        }
    }

    if ((ymax - ymin) >= 61)
    {
#ifdef VERSION_EU
        if (explosiontype < EXPLOSION_MEDIUM2)
#else
        if ((explosiontype * 2) < EXPLOSION_SMALL)
#endif
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

    if (rodata->numVertices <= 0)
    {
        return;
    }
    
    offset = 0;
    
    do
    {
        chrObjRandomSetSeed(((((Vertex *) (((u8 *) rodata->Vertices) + offset))->coord.z + ((Vertex *) (((u8 *) rodata->Vertices) + offset))->coord.x) + ((Vertex *) (((u8 *) rodata->Vertices) + offset))->coord.y) + deformseed);
        chance = 0;
        
        if (allow_blackening)
        {
            if (obj->mtx.m[1][1] >= 0.0f)
            {
                if (((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y >= ymid)
                {
                    if (adjust_height)
                    {
                        chance = 90;
                    }
                    else
                    {
                        chance = 20;
                    }
                }
                else if (adjust_height)
                {
                    chance = 20;
                }
                else
                {
                    chance = 90;
                }
            }
            else if ((ymid ^ 0) >= ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y)
            {
                if (adjust_height)
                {
                    chance = 90;
                }
                else
                {
                    chance = 20;
                }
            }
            else if (adjust_height)
            {
                chance = 20;
            }
            else
            {
                chance = 90;
            }
        }
        else
        {
            chance = 0;
        }
            
        if (1)
        {
            if (((s32) (chrObjRandomGetNext() % 100)) < chance)
            {
                ((Vertex *) (((u8 *) (*vtxslot)) + offset))->r = 0;
                ((Vertex *) (((u8 *) (*vtxslot)) + offset))->g = 0;
                ((Vertex *) (((u8 *) (*vtxslot)) + offset))->b = 0;
                ((Vertex *) (((u8 *) (*vtxslot)) + offset))->a = 255;
            }
            else
#ifdef VERSION_EU
            if (explosiontype == EXPLOSION_BREAK_OBJECT2)
#else
            if ((explosiontype * 2) == EXPLOSION_MEDIUM)
#endif
            {
                ((Vertex *) (((u8 *) (*vtxslot)) + offset))->a = 0;
            }
            
            ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y = (((f32) (((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y - ymin)) * yscale) + ((f32) ymin);
            ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.x += (chrObjRandomGetNext() % 80) - 40;
            ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y += (chrObjRandomGetNext() % 80) - 40;
            ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.z += (chrObjRandomGetNext() % 80) - 40;
                
            if (((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y < ymin)
            {
                ((Vertex *) (((u8 *) (*vtxslot)) + offset))->coord.y = ymin;
            }

            i++;
            offset += sizeof(Vertex);
        }
    } while (i < rodata->numVertices);
}


void objBounce(ObjectRecord *obj, coord3d *velocity)
{
    coord3d dir;
    coord3d rot = {{0, 0, 0}};
    Projectile *projectile = NULL;

    objEnsureProjectile(obj->prop);

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED) {
        projectile = obj->embedment->projectile;
    } else if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) {
        projectile = obj->projectile;
    }

    if (projectile) {
        projectile->speed.x = (RANDOMFRAC() * 1.6666666f * 4.0f) - 3.3333333f;
        projectile->speed.y = (RANDOMFRAC() * 1.6666666f * 2.0f) + 3.3333333f;
        projectile->speed.z = (RANDOMFRAC() * 1.6666666f * 4.0f) - 3.3333333f;

#ifdef VERSION_EU
        rot.x = (RANDOMFRAC() * 7.53982257843f * 0.015625f) - 0.058904863894f;
        rot.y = (RANDOMFRAC() * 7.53982257843f * 0.015625f) - 0.058904863894f;
        rot.z = (RANDOMFRAC() * 7.53982257843f * 0.015625f) - 0.058904863894f;
#else
        rot.x = (RANDOMFRAC() * M_TAU_F * 0.015625f) - 0.049087387f;
        rot.y = (RANDOMFRAC() * M_TAU_F * 0.015625f) - 0.049087387f;
        rot.z = (RANDOMFRAC() * M_TAU_F * 0.015625f) - 0.049087387f;
#endif

        mtx4LoadRotation(&rot, &projectile->mtx);

        projectile->flags |= PROJECTILEFLAG_AIRBORNE;

        dir.x = velocity->x;
        dir.y = velocity->y;
        dir.z = velocity->z;

        mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &dir);

        projectile->speed.x += 3.3333333f * dir.x;
        projectile->speed.z += 3.3333333f * dir.z;
        projectile->ownerprop = currentPlayerGetProp();
        projectile->numBounces = 1;
    }
}


void propobjSetDropped(PropRecord *prop, DROPTYPE droptype)
{
    PropRecord *parent = prop->parent;

    if (parent)
    {
        ObjectRecord *obj = prop->obj;

        objEnsureProjectile(prop);

        if ((obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED) && obj->embedment->projectile)
        {
            obj->embedment->projectile->droptype = droptype;
        }
        else if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        {
            obj->projectile->droptype = droptype;
        }
    }
}


void objDetach(PropRecord *prop)
{
    PropRecord *parent = prop->parent;

    if (parent)
    {
        ObjectRecord *obj = prop->obj;
        Model *model = obj->model;

        propDetach(prop);

        model->attachedto_objinst = NULL;

        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_HASOWNER;

        if (parent->type == PROP_TYPE_CHR || parent->type == PROP_TYPE_VIEWER)
        {
            ChrRecord *chr = parent->chr;

            if (chr)
            {
                if (prop == chr->handle_positiondata_hat)
                {
                    chr->handle_positiondata_hat = NULL;
                }
                else if (prop == chr->weapons_held[GUNRIGHT])
                {
                    chrSetFiring(chr, GUNRIGHT, FALSE);
                    chr->weapons_held[GUNRIGHT] = NULL;
                }
                else if (prop == chr->weapons_held[GUNLEFT])
                {
                    chrSetFiring(chr, GUNLEFT, FALSE);
                    chr->weapons_held[GUNLEFT] = NULL;
                }
            }
        }
    }
}


s32 objDrop(PropRecord *prop)
{
    PropRecord *parent = prop->parent;
    Projectile *projectile;
    ObjectRecord *obj = prop->obj;
    Model *model;
    Mtxf rootmtx;
    PropRecord *root;
    StandTile* rootstan;

    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED) && obj->embedment->projectile)
    {
        Projectile* projectile2 = obj->embedment->projectile;
        embedmentFree(obj->embedment);

        obj->projectile = projectile2;
        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_EMBEDDED;
        obj->runtime_bitflags |= RUNTIMEBITFLAG_HASPROJECTILE;
    }

    if (parent && (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
    {
        model = obj->model;
        projectile = obj->projectile;
        root = parent;

        projectile->flags |= PROJECTILEFLAG_AIRBORNE;
        projectile->ownerprop = parent;

        if (projectile->droptype == DROPTYPE_SURRENDER && parent->type == PROP_TYPE_CHR)
        {
            ChrRecord* chr = parent->chr;
            Model *chrmodel = chr->model;
            coord3d rot = { {0.0f, 0.0f, 0.0f} };
            f32 angle = modelGetRootRotY(chrmodel);

            projectile->speed.x = sinf(angle) * 1.6666666f;
            projectile->speed.y = -RANDOMFRAC() * 1.6666666f * 0.5f;
            projectile->speed.z = cosf(angle) *  1.6666666f;

#ifdef VERSION_EU
            rot.x = (RANDOMFRAC() * 7.53982257843f * 0.0078125f) - 0.029452431947f;
            rot.y = (RANDOMFRAC() * 7.53982257843f * 0.0078125f) - 0.029452431947f;
            rot.z = (RANDOMFRAC() * 7.53982257843f * 0.0078125f) - 0.029452431947f;
#else
            rot.x = (RANDOMFRAC() * M_TAU_F * 0.0078125f) - 0.024543693f;
            rot.y = (RANDOMFRAC() * M_TAU_F * 0.0078125f) - 0.024543693f;
            rot.z = (RANDOMFRAC() * M_TAU_F * 0.0078125f) - 0.024543693f;
#endif

            mtx4LoadRotation(&rot, &projectile->mtx);
        }
        else if (projectile->droptype == DROPTYPE_THROWGRENADE && parent->type == PROP_TYPE_CHR)
        {
            ChrRecord* chr = parent->chr;
            Model *chrmodel = chr->model;
            coord3d rot = { {0.0f, 0.0f, 0.0f} };
            f32 angle = modelGetRootRotY(chrmodel);

            projectile->speed.x = sinf(angle) * 13.333333f;
            projectile->speed.y = 6.6666665f;
            projectile->speed.z = cosf(angle) * 13.333333f;

#ifdef VERSION_EU
            rot.x = (RANDOMFRAC() * 7.53982257843f * 0.0078125f) - 0.029452431947f;
            rot.y = (RANDOMFRAC() * 7.53982257843f * 0.0078125f) - 0.029452431947f;
            rot.z = (RANDOMFRAC() * 7.53982257843f * 0.0078125f) - 0.029452431947f;
#else
            rot.x = (RANDOMFRAC() * M_TAU_F * 0.0078125f) - 0.024543693f;
            rot.y = (RANDOMFRAC() * M_TAU_F * 0.0078125f) - 0.024543693f;
            rot.z = (RANDOMFRAC() * M_TAU_F * 0.0078125f) - 0.024543693f;
#endif

            mtx4LoadRotation(&rot, &projectile->mtx);
            projectile->flags |= 0x40;

        }
        else if (projectile->droptype == DROPTYPE_HAT)
        {
            coord3d rot = { {0.0f, 0.0f, 0.0f} };
            PropRecord *playerprop = currentPlayerGetProp();
            f32 x = parent->pos.x - playerprop->pos.x;
            f32 z = parent->pos.z - playerprop->pos.z;
            f32 angle = mathAtan2(x, z);

            projectile->speed.x = ((2.0f * (RANDOMFRAC() * 1.6666666f)) + 3.3333333f) * sinf(angle);
            projectile->speed.y = 2.0f * (RANDOMFRAC() * 1.6666666f);
            projectile->speed.z = ((2.0f * (RANDOMFRAC() * 1.6666666f)) + 3.3333333f) * cosf(angle);

#ifdef VERSION_EU
            rot.x = (RANDOMFRAC() * 7.53982257843f * 0.03125f) - 0.117809727788f;
            rot.y = (RANDOMFRAC() * 7.53982257843f * 0.03125f) - 0.117809727788f;
            rot.z = (RANDOMFRAC() * 7.53982257843f * 0.03125f) - 0.117809727788f;
#else
            rot.x = (RANDOMFRAC() * M_TAU_F * 0.03125f) - 0.09817477f;
            rot.y = (RANDOMFRAC() * M_TAU_F * 0.03125f) - 0.09817477f;
            rot.z = (RANDOMFRAC() * M_TAU_F * 0.03125f) - 0.09817477f;
#endif

            mtx4LoadRotation(&rot, &projectile->mtx);
        }
        else
        {
            // DROPTYPE_OWNERREAP ?
            physRandomLaunch(&projectile->speed, &projectile->mtx);
        }

        while (root->parent != NULL)
        {
            root = root->parent;
        }

        rootstan = root->stan;

        if (prop->flags & PROPFLAG_ONSCREEN)
        {
            // Do collision checks
            f32 objwidth = objGetWidth(obj);
            Mtxf *rendermtx = modelGetRootMtx(model);
            s32 cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;

            mtx4MultAffine(camGetViewToWorldMtxf(), rendermtx, &rootmtx);

            if (projectile->flags & 0x40)
            {
                cdtypes = CDTYPE_OBJS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
            }

            propSetMoving(root, FALSE);

            if ((stanTestLineUnobstructed(&rootstan, root->pos.f[0], root->pos.f[2], rootmtx.m[3][0], rootmtx.m[3][2], cdtypes, 0.0f, 1.0f, 0.0f, 1.0f) != 0)
                && (stanTestVolume(&rootstan, rootmtx.m[3][0], rootmtx.m[3][2], objwidth, cdtypes, 0.0f, 1.0f) < 0))
            {
                prop->stan = rootstan;

            }
            else
            {
                prop->stan = root->stan;
                rootmtx.m[3][0] = root->pos.x;
                rootmtx.m[3][2] = root->pos.z;
            }

            propSetMoving(root, TRUE);
            prop->zDepth = -rendermtx->m[3][2];

        }
        else
        {
            // No collision checks
            // Helpful for throwing mines through doors during speedruns
            prop->stan = root->stan;
            mtx4LoadIdentity(&rootmtx);
            mtx4ScaleRows012(model->scale, &rootmtx);
            mtx4SetTranslation(&root->pos, &rootmtx);
        }

        objDetach(prop);
        propActivate(prop);
        propEnable(prop);

        obj->runtime_pos.x = prop->pos.x = rootmtx.m[3][0];
        obj->runtime_pos.y = prop->pos.y = rootmtx.m[3][1];
        obj->runtime_pos.z = prop->pos.z = rootmtx.m[3][2];

        rootmtx.m[3][0] = 0.0f;
        rootmtx.m[3][1] = 0.0f;
        rootmtx.m[3][2] = 0.0f;

        mtx4Copy(&rootmtx, &obj->mtx);

        doorGetTileShading(obj->prop, &obj->nextcol);

        obj->shadecol.r = obj->nextcol.r;
        obj->shadecol.g = obj->nextcol.g;
        obj->shadecol.b = obj->nextcol.b;
        obj->shadecol.a = obj->nextcol.a;

        setupUpdateObjectRoomPosition(obj);

        return TRUE;
    }

    return FALSE;
}


/**
 * Make an object fall. Eg. due to it sitting on a table which is now destroyed,
 * or because it was a chopper that is now destroyed.
 */
void objFall(ObjectRecord *obj, s32 playernum)
{
    obj->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
    obj->runtime_bitflags |= (playernum << RUNTIMEBITSHIFT_OWNER);

    if ((obj->flags2 & PROPFLAG2_NOFALL) == 0
            && (obj->flags & PROPFLAG_RENDERPOSTBG)
            && (obj->runtime_bitflags & (RUNTIMEBITFLAG_EMBEDDED | RUNTIMEBITFLAG_HASPROJECTILE)) == 0)
    {

        coord3d rot = {{0, 0, 0}};
        Projectile *projectile = NULL;

        objEnsureProjectile(obj->prop);

        if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        {
            projectile = obj->projectile;
        }

        if (projectile)
        {
            projectile->speed.x = RANDOMFRAC() * 1.6666666f - 0.8333333f;
            projectile->speed.y = RANDOMFRAC() * 1.6666666f * 2.0f + 1.6666666f;
            projectile->speed.z = RANDOMFRAC() * 1.6666666f - 0.8333333f;

            if ((obj->flags2 & PROPFLAG2_FALLWITHOUTROTATION) == 0)
            {
#ifdef VERSION_EU
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
            obj->runtime_bitflags &= ~RUNTIMEBITFLAG_00008000;
        }
    }
}


/**
 * Destroy the objects that the given prop is supporting.
 *
 * For example, destroying a table will also destroy all the props that are
 * sitting on that table.
 */
void objDestroySupportedObjects(PropRecord* tableprop, s32 playernum)
{
    ObjectRecord* obj;
    ObjectRecord* tableobj;
    PropRecord* prop;
    coord2d* rect;
    s32 edges;
    u8 room;

    tableobj = tableprop->obj;
    room = tableprop->stan->room;

    propGetCollisionPolygon2d(tableprop, &rect, &edges);

    if (edges > 0)
    {
        prop = propsGetActiveTail();
        while (prop)
        {
            if (((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_WEAPON)) && (prop->stan->room == room))
            {
                obj = prop->obj;
                if ((tableobj->runtime_pos.y < obj->runtime_pos.y)
                        && ((s32) obj->runtime_bitflags & RUNTIMEBITFLAG_00008000)
                        && (polygonContainsPoint(&obj->runtime_pos, rect, edges) != 0))
                {
                    objFall(obj, playernum);
                }
            }
            prop = prop->prev;
        }
    }
}


void objExplode(ObjectRecord *obj, coord3d *target_pos, s32 playernum)
{
    PropRecord *prop;
    PropRecord *tailprop;
    s16 explosion_type;
    StandTile *stan;
    s32 shots;

    if (!(obj->damage < obj->maxdamage))
    {
        if (objGetDestroyedLevel(obj) == 0)
        {
            return;
        }
    }

    prop = obj->prop;
#if defined(VERSION_EU)
    explosion_type = ((s8 *) object_explosion_details.typeids)[obj->obj];
#else
    explosion_type = object_explosion_details[obj->obj].TypeID;
#endif
    tailprop = prop;

    if (tailprop->parent != (NULL))
    {
        do
        {
            tailprop = tailprop->parent;
        }
        while (tailprop->parent != NULL);
    }

    stan = tailprop->stan;

    if (objGetDestroyedLevel(obj) == 0)
    {
        ((PropDefHeaderRecord *)obj)->state |= PROPSTATE_DESTROYED;
        obj->maxdamage = 0.0f;

        if (stan != NULL)
        {
            if ((!(tailprop->flags & PROPFLAG_00000008)) && stanWalkLine(&stan, tailprop->pos.x, tailprop->pos.z, target_pos->x, target_pos->z))
            {
                explosionCreate(prop, target_pos, stan, explosion_type, (obj->flags & 0xe) == 0, playernum, tailprop->rooms, 0);
            }
            else
            {
                explosionCreate(prop, target_pos, tailprop->stan, explosion_type, 0, playernum, tailprop->rooms, 1);
            }
        }

        if (obj->flags2 & PROPFLAG2_00002000)
        {
            obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
            return;
        }

        objDeform(obj, 1);

        if (tailprop != prop)
        {
            return;
        }

        objDestroySupportedObjects(prop, playernum);

        if (obj->runtime_bitflags & RUNTIMEBITFLAG_00008000)
        {
            if ((randomGetNext() % 3) != 0)
            {
                return;
            }
        }

        obj->runtime_bitflags |= RUNTIMEBITFLAG_00010000;
        objFall(obj, playernum);
        return;
    }

    shots = objGetShotsTaken(obj);

    if ((shots & 3) == 0)
    {
        objDeform(obj, (shots >> 2) + 1);

        if (stan != NULL)
        {
            if ((!(tailprop->flags & PROPFLAG_00000008)) && stanWalkLine(&stan, tailprop->pos.x, tailprop->pos.z, target_pos->x, target_pos->z))
            {
                explosionCreate(prop, target_pos, stan, 0x10, (obj->flags & (PROPFLAG_ONSCREEN | PROPFLAG_ENABLED | PROPFLAG_00000008)) == 0, playernum, tailprop->rooms, 0);
            }
            else
            {
                explosionCreate(prop, target_pos, tailprop->stan, 0x10, 0, playernum, tailprop->rooms, 1);
            }
        }
    }

    if ((0 < objGetDestroyedLevel(obj)) && (((PropDefHeaderRecord *)obj)->state & PROPSTATE_RESPAWN))
    {
        if (obj->runtime_bitflags & RUNTIMEBITFLAG_00001000)
        {
            ((PropDefHeaderRecord *)obj)->state |= PROPSTATE_10;
        }
        else
        {
            ((PropDefHeaderRecord *)obj)->state &= ~PROPSTATE_10;
        }
#if defined(VERSION_EU)
        prop->timetoregen = 1000;
#else
        prop->timetoregen = 1200;
#endif
    }

    if (shots >= 12)
    {
        obj->runtime_bitflags |= RUNTIMEBITFLAG_00001000;
        obj->flags &= ~RUNTIMEBITFLAG_00000100;
    }
}




bool bgTestHitOnObj(coord3d *rayStart, coord3d *rayEnd, coord3d *rayDir, Gfx *gdl, Gfx *gdl2, Vertex *vertices, struct HitThing *hitthing)
{
    Vertex *vtxbase = NULL;
    HitThing hitbuf;
    Vertex *pt0;
    Vertex *pt1;
    s32 result[1];
    s32 idx[3];
    s32 padC;
    BoundVec bboxMin;
    BoundVec bboxMax;
    coord3d zero;
    f32 bestDist;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 d;
    s32 idx2[3];
    Vertex *pt2;
    BoundVec bboxMin2;
    BoundVec bboxMax2;
    Gfx *cmdStart;
    Gfx *tcmd;
    Vertex *v;
    coord3d zero2;
    s32 s2;
    s32 i;
    s32 op;
    s32 texnum;
    
    bestDist = M_U32_MAX_VALUE_F;
    result[0] = 0;
    cmdStart = gdl;

    while (1)
    {
        op = *((s8 *) gdl);
        if (op == (s8)G_ENDDL)
        {
            if (gdl2 == 0)
            {
                break;
            }

            cmdStart = gdl2;


            gdl = gdl2;
            gdl2 = 0;

            continue;
        }

        if (op == G_VTX)
        {
            op = ((u8 *) gdl)[1] & 0xf;
            padC = ((u32 *) gdl)[1] & 0x00ffffff;
            vtxbase = (Vertex *) ((((s32) vertices) + padC) - (op << 4));
            gdl++;

            continue;
        }

        if (op == (s8)G_TRI1)
        {
            bboxMin = (BoundVec){0x7FFF, 0x7FFF, 0x7FFF};
            bboxMax = (BoundVec){-0x8000, -0x8000, -0x8000};

            idx[0] = ((u8 *) gdl)[5] / 10;
            idx[1] = ((u8 *) gdl)[6] / 10;
            idx[2] = ((u8 *) gdl)[7] / 10;

            for (i = 0; i < 3; i++)
            {
                v = vtxbase;
                v += idx[i];

                if (v->coord.x < bboxMin.x)
                {
                    bboxMin.x = v->coord.x;
                }
                if (bboxMax.x < v->coord.x)
                {
                    bboxMax.x = v->coord.x;
                }
                if (v->coord.y < bboxMin.y)
                {
                    bboxMin.y = v->coord.y;
                }
                if (bboxMax.y < v->coord.y)
                {
                    bboxMax.y = v->coord.y;
                }
                if (v->coord.z < bboxMin.z)
                {
                    bboxMin.z = v->coord.z;
                }
                if (bboxMax.z < v->coord.z)
                {
                    bboxMax.z = v->coord.z;
                }
            }

            if (bgTestRayIntersectsBbox(rayStart, rayDir, (s32 *) (&bboxMin), (s32 *) (&bboxMax)))
            {
                zero = (coord3d){0};
                pt0 = vtxbase; pt0 += idx[0]; pt1 = vtxbase; pt1 += idx[1]; pt2 = vtxbase; pt2 += idx[2];

                if (intersectRayTriangle(&pt0->coord, &pt1->coord, &pt2->coord, &zero, rayStart, rayEnd, rayDir, &hitbuf.hitpos, &hitbuf.normal))
                {
                    dx = (f32) (((s32) hitbuf.hitpos.x) - ((s32) rayStart->x));
                    dy = (f32) (((s32) hitbuf.hitpos.y) - ((s32) rayStart->y));
                    dz = (f32) (((s32) hitbuf.hitpos.z) - ((s32) rayStart->z));
                    tcmd = gdl;
                    if (((*((u8 *) gdl)) != 253) && (cmdStart < gdl))
                    {
                        do
                        {
                            tcmd--;

                            if ((*((u8 *) tcmd)) == 253)
                            {
                                break;
                            }
                        } while (cmdStart < tcmd);

                    }

                    if (tcmd == cmdStart)
                    {
                        texnum = -1;
                    }
                    else
                    {
                        padC = ((u32 *) tcmd)[1] - 8;
                        texnum = *((u16 *) (padC | 0x80000000));
                    }

                    d = ((dx * dx) + (dy * dy)) + (dz * dz);

                    if (d < bestDist)
                    {
                        bestDist = d;
                        hitthing->hitpos.x = hitbuf.hitpos.x;
                        hitthing->hitpos.y = hitbuf.hitpos.y;
                        hitthing->hitpos.z = hitbuf.hitpos.z;
                        hitthing->normal.x = hitbuf.normal.x;
                        hitthing->normal.y = hitbuf.normal.y;
                        hitthing->normal.z = hitbuf.normal.z;
                        hitthing->vtx0 = &vtxbase[idx[0]];
                        hitthing->vtx1 = &vtxbase[idx[1]];
                        hitthing->vtx2 = &vtxbase[idx[2]];
                        hitthing->texturenum = texnum;
                        hitthing->tricmd = gdl;
                        hitthing->triNum = 0;
                        result[0] = 1;
                    }
                }
            }
        }
        else if (op == (s8)G_TRI4)
        {
            s2 = 0;
            do
            {
                bboxMin2 = (BoundVec){0x7FFF, 0x7FFF, 0x7FFF};
                bboxMax2 = (BoundVec){-0x8000, -0x8000, -0x8000};

                if (s2 == 0)
                {
                    idx2[0] = ((u32 *) gdl)[1] & 0xf;
                    idx2[1] = ((u32) ((u8 *) gdl)[7]) >> 4;
                    idx2[2] = ((u32 *) gdl)[0] & 0xf;
                }
                else if (s2 == 1)
                {
                    idx2[0] = ((u8 *) gdl)[6] & 0xf;
                    idx2[1] = ((u32) ((u16 *) gdl)[3]) >> 12;
                    idx2[2] = ((u32) ((u8 *) gdl)[3]) >> 4;
                }
                else if (s2 == 2)
                {
                    idx2[0] = ((u16 *) gdl)[2] & 0xf;
                    idx2[1] = ((u32) ((u8 *) gdl)[5]) >> 4;
                    idx2[2] = ((u8 *) gdl)[2] & 0xf;
                }
                else
                {
                    idx2[0] = ((u8 *) gdl)[4] & 0xf;
                    idx2[1] = ((u32 *) gdl)[1] >> 28;
                    idx2[2] = ((u32) ((u16 *) gdl)[1]) >> 12;
                }

                for (i = 0; i < 3; i++)
                {
                    v = vtxbase;
                    v += idx2[i];

                    if (v->coord.x < bboxMin2.x)
                    {
                        bboxMin2.x = v->coord.x;
                    }
                    if (bboxMax2.x < v->coord.x)
                    {
                        bboxMax2.x = v->coord.x;
                    }
                    if (v->coord.y < bboxMin2.y)
                    {
                        bboxMin2.y = v->coord.y;
                    }
                    if (bboxMax2.y < v->coord.y)
                    {
                        bboxMax2.y = v->coord.y;
                    }
                    if (v->coord.z < bboxMin2.z)
                    {
                        bboxMin2.z = v->coord.z;
                    }
                    if (bboxMax2.z < v->coord.z)
                    {
                        bboxMax2.z = v->coord.z;
                    }
                }

                if (bgTestRayIntersectsBbox(rayStart, rayDir, (s32 *) (&bboxMin2), (s32 *) (&bboxMax2)))
                {
                    zero2 = (coord3d){0};
                    pt0 = vtxbase; pt0 += idx2[0]; pt1 = vtxbase; pt1 += idx2[1]; pt2 = vtxbase; pt2 += idx2[2];

                    if (intersectRayTriangle(&pt0->coord, &pt1->coord, &pt2->coord, &zero2, rayStart, rayEnd, rayDir, &hitbuf.hitpos, &hitbuf.normal))
                    {
                        dx = (f32) (((s32) hitbuf.hitpos.x) - ((s32) rayStart->x));
                        dy = (f32) (((s32) hitbuf.hitpos.y) - ((s32) rayStart->y));
                        dz = (f32) (((s32) hitbuf.hitpos.z) - ((s32) rayStart->z));
                        tcmd = gdl;

                        if (((*((u8 *) gdl)) != G_SETTIMG) && (cmdStart < gdl))
                        {
                            do
                            {
                                tcmd--;
                                if ((*((u8 *) tcmd)) == G_SETTIMG)
                                {
                                    break;
                                }
                            } while (cmdStart < tcmd);

                        }

                        if (tcmd == cmdStart)
                        {
                            texnum = -1;
                        }
                        else
                        {
                            padC = ((u32 *) tcmd)[1] - 8;
                            texnum = *((u16 *) (padC | 0x80000000));
                        }

                        d = ((dx * dx) + (dy * dy)) + (dz * dz);

                        if (d < bestDist)
                        {
                            bestDist = d;
                            hitthing->hitpos.x = hitbuf.hitpos.x;
                            hitthing->hitpos.y = hitbuf.hitpos.y;
                            hitthing->hitpos.z = hitbuf.hitpos.z;
                            hitthing->normal.x = hitbuf.normal.x;
                            hitthing->normal.y = hitbuf.normal.y;
                            hitthing->normal.z = hitbuf.normal.z;
                            hitthing->vtx0 = &vtxbase[idx2[0]];
                            hitthing->vtx1 = &vtxbase[idx2[1]];
                            hitthing->vtx2 = &vtxbase[idx2[2]];
                            hitthing->texturenum = texnum;
                            hitthing->tricmd = gdl;
                            hitthing->triNum = s2 + 1;
                            result[0] = 1;
                        }
                    }
                }
                s2++;
            } while (s2 != 4);
        }
        gdl++;
    }
 
    return result[0];
}


// PD: obj_find_hitthing_by_gfx_tris
bool propobjFindHit(Model *model, ModelNode *startNode, coord3d *rayPos, coord3d *rayDir, struct HitThing *hitthing, s32 *dstmtxindex, ModelNode **dstnode)
{
    coord3d localStart;
    coord3d localEnd;
    coord3d localDir;
    Mtxf *lastmtx;
    bool done;
    ModelNode *node;
    Vertex *vertices;

    lastmtx = NULL;
    done = FALSE;
    node = startNode;
    vertices = NULL;

    while (node && !done)
    {
        u32 type = node->Opcode & 0xff;
        Gfx *gdl = NULL;
        void *gdl2 = NULL;

        switch (type)
        {
            case MODELNODE_OPCODE_DLCOLLISION:
                {
                    ModelRoData_DisplayList_CollisionRecord *rodata = &node->Data->DisplayListCollisions;
                    ModelRwData_DisplayList_CollisionRecord *rwdata = &modelGetNodeRwData(model, node)->DisplayListCollisions;

                    if (rwdata->gdl != NULL)
                    {
                        if (rwdata->gdl == rodata->Primary)
                        {
                            gdl = (Gfx *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Primary & 0xffffff));
                        }
                        else
                        {
                            gdl = rwdata->gdl;
                        }

                        if (rodata->Secondary != NULL)
                        {
                            gdl2 = (void *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Secondary & 0xffffff));
                        }

                        vertices = rwdata->Vertices;
                    }
                }
                break;

            case MODELNODE_OPCODE_DL:
                {
                    ModelRoData_DisplayListRecord *rodata = &node->Data->DisplayList;

                    if (rodata->Primary != NULL)
                    {
                        gdl = (Gfx *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Primary & 0xffffff));

                        if (rodata->Secondary != NULL)
                        {
                            gdl2 = (Gfx *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Secondary & 0xffffff));
                        }

                        vertices = (void *)(uintptr_t)rodata->BaseAddr;
                    }
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

        if (gdl != NULL)
        {
            s32 mtxindex = modelFindNodeMtxIndex(node, 0);
            Mtxf *mtx = NULL;
            Mtxf invmtx;

            if (mtxindex >= 0)
            {
                mtx = (Mtxf *)&model->render_pos[mtxindex]; // TODO: adjust
            }

            if (mtx && mtx != lastmtx)
            {
                lastmtx = mtx;

                mtx4InvertAffine(mtx, &invmtx);

                localStart.x = rayPos->x;
                localStart.y = rayPos->y;
                localStart.z = rayPos->z;

                mtx4TransformVecInPlace(&invmtx, &localStart);

                localDir.x = rayDir->x;
                localDir.y = rayDir->y;
                localDir.z = rayDir->z;

                mtx4RotateVecInPlace(&invmtx, &localDir);

                localEnd.x = localDir.x * 32767.0f + localStart.x;
                localEnd.y = localDir.y * 32767.0f + localStart.y;
                localEnd.z = localDir.z * 32767.0f + localStart.z;
            }

            if (bgTestHitOnObj(&localStart, &localEnd, &localDir, gdl, gdl2, vertices, hitthing))
            {
                *dstmtxindex = mtxindex;
                *dstnode = node;
                done = TRUE;
            }
        }

        if (node->Child)
        {
            node = node->Child;
        } else {
            while (node)
            {
                if (node == startNode)
                {
                    node = NULL;
                    break;
                }

                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }

    return done;
}


void glassShatter(ObjectRecord* obj)
{
    PropRecord* prop;
    struct ModelRoData_BoundingBoxRecord *bbox;

    prop = obj->prop;
    bbox = chrobjGetBboxFromObjectRecord(obj);
    explosionClearBulletImpactRoomByFlag(prop, FALSE);
    explosionClearBulletImpactRoomByFlag(prop, TRUE);

    glassCreateShards(&obj->runtime_pos,
        (coord3d *)obj->mtx.m[0], (coord3d *)obj->mtx.m[1], (coord3d *)obj->mtx.m[2],
        bbox->Bounds.xmin, bbox->Bounds.xmax,
        bbox->Bounds.ymin, bbox->Bounds.ymax);

    obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
    obj->state |= PROPSTATE_DESTROYED;
    obj->maxdamage = 0.0f;
}


void doorShatterGlass(DoorRecord *door)
{
    PropRecord *prop;
    Model *model;
    struct ModelRoData_BoundingBoxRecord *bbox;
    struct ModelRwData_SwitchRecord *switchdata;
    Mtxf mtx;

    prop = door->prop;
    model = door->model;
    bbox = (struct ModelRoData_BoundingBoxRecord *) model->obj->Switches[2]->Data;

    doorGetMtx(door, &mtx);
    glassCreateShards((coord3d *)mtx.m[3], (coord3d *)mtx.m[0], (coord3d *)mtx.m[1], (coord3d *)mtx.m[2], bbox->Bounds.xmin, bbox->Bounds.xmax, bbox->Bounds.ymin, bbox->Bounds.ymax);

    explosionClearBulletImpactRoomByFlag(prop, 1);

    switchdata = &modelGetNodeRwData(model, model->obj->Switches[1])->Switch;
    switchdata->visible = FALSE;
}


void objBreakCCTVGlass(ObjectRecord *obj)
{
    PropRecord *prop;
    union ModelRoData *rodata;
    Mtxf *node_mtx;
    Mtxf glassNodeWorldMtx;
    Model *model;

    prop = obj->prop;
    model = obj->model;

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        rodata = model->obj->Switches[2]->Data;

        node_mtx = modelFindNodeMtx(model, model->obj->Switches[1], 0);

        mtx4MultAffine(camGetViewToWorldMtxf(), node_mtx, &glassNodeWorldMtx);

        glassCreateShards((coord3d *)glassNodeWorldMtx.m[3], (coord3d *)glassNodeWorldMtx.m[0], (coord3d *)glassNodeWorldMtx.m[1], (coord3d *)glassNodeWorldMtx.m[2], ((f32 *)rodata)[1], ((f32 *)rodata)[2], ((f32 *)rodata)[3], ((f32 *)rodata)[4]);
    }

    explosionClearBulletImpactRoomByFlag(prop, 1);

    *(s32 *)modelGetNodeRwData(model, model->obj->Switches[3]) = 0;
}


void objDetonateRecursive(PropRecord *prop, f32 damage, coord3d *pos, s32 weaponnum, s32 owner)
{
    PropRecord *node;
    ObjectRecord *prop_obj;

    prop_obj = prop->obj;

    prop_obj->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
    prop_obj->runtime_bitflags |= (owner << RUNTIMEBITSHIFT_OWNER);

    if ((s32)(prop_obj->runtime_bitflags << 0xc) >= 0)
    {
        node = prop->child;
        while (node != NULL)
        {
            PropRecord *iter_next = node->prev;
            // recursive call:
            objDetonateRecursive(node, damage, pos, weaponnum, owner);
            node = iter_next;
        }

        objApplyDamage(prop->obj, damage, pos, weaponnum, owner);
    }
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


bool objIsMortal(ObjectRecord* obj)
{
    if (obj->type == PROPDEF_DOOR)
    {
        return FALSE;
    }
    if ((objIsCollectable((PropDefHeaderRecord* ) obj) != 0) && (obj->type != PROPDEF_ARMOUR))
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


void chrobjMaybeDetonateObjectIfFlags(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS item, s32 owner)
{
    if ((obj->flags2 & 0x4000) == 0)
    {
        objApplyDamage(obj, damage, pos, item, owner);
    }
}


ObjectRecord blank_07_object = {
    {0x0100, //extrascale
    0x0, //state
    0x07}, //type
    0, //obj
    0xFFFF, //pad
    0x00000001, //flags
    0, //flags2
    NULL, //prop
    NULL, //model
    {
       {{1.0f, 0.0f, 0.0f, 0.0f},
       {0.0f, 1.0f, 0.0f, 0.0f},
       {0.0f, 0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 0.0f, 1.0f}}
    }, //mtx
    {{0.0, 0.0, 0.0}},//runtime_pos
    {0x00000000}, //runtime_bitflags
    NULL, //ptr_allocated_collisiondata_block
    {NULL}, //projectile/embedment
    0.0f, //maxdamage
    1000.0f, //damage
    {{0xFF, 0xFF, 0xFF, 0x00}}, //shadecol
    {{0xFF, 0xFF, 0xFF, 0x00}}, //nextcol
};


// Formerly maybe_detonate_object
void objApplyDamage(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS itemnum, s32 playernum)
{
    s32 flags;
    s32 shotsmod;
    f32 maxdamage;
    PropRecord *child;
    MultiAmmoCrateRecord *slotview;
    s32 startslot;
    s32 slot;
    AmmoCrateRecord *crate;
    ObjectRecord tmpobj;
    
    /**
     * Set the OWNER field (bits 17-18) to the player index; clear-then-set, equivalent to obj->owner = playernum.
     * Used to record which player is responsible for the damage.
     */ 
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
    obj->runtime_bitflags |= playernum << RUNTIMEBITSHIFT_OWNER;

    if (obj->type == PROPDEF_GAS_RELEASING)
    {
        if (objGetDestroyedLevel(obj) == 1)
        {
            return;
        }
    }

    if (itemnum == ITEM_UNARMED)
    {
        if (objIsCollectable((PropDefHeaderRecord *)obj))
        {
            // Equivalent to: if (!(obj->flags & PROPFLAG_00800000)) keep like this for matching
            if ((s32)(obj->flags << 8) >= 0)
            {
                return;
            }
            goto apply_damage;
        }

        // Equivalent to: if (obj->flags & PROPFLAG_01000000) keep like this for matching
        if ((s32)(obj->flags << 7) < 0)
        {
            return;
        }

        goto apply_damage;
    }

    flags = obj->flags;

    if (flags & PROPFLAG_INVINCIBLE)
    {
        return;
    }

    if (obj->type == PROPDEF_COLLECTABLE)
    {
        s32 weaponnum = ((WeaponObjRecord *)obj)->weaponnum;

        // For these items, if they take damage, set their timers to 0, effectively causing them to immediately detonate.
        if (weaponnum == ITEM_GRENADE
                || weaponnum == ITEM_TIMEDMINE
                || weaponnum == ITEM_REMOTEMINE
                || weaponnum == ITEM_PROXIMITYMINE
                || weaponnum == ITEM_ROCKETROUND
                || weaponnum == ITEM_GRENADEROUND
                || weaponnum == ITEM_BOMBCASE
                || weaponnum == ITEM_PLASTIQUE)
        {
            ((WeaponObjRecord *)obj)->timer = 0;
        }

        return;
    }

    if (obj->type == PROPDEF_MAGAZINE)
    {
        s32 ammotype = ((AmmoCrateRecord *)obj)->ammoType;

        // slot and startslot are double purposed here for matching.
        slot = AMMO_REMOTEMINE;
        startslot = AMMO_PROXMINE;

        if (ammotype == AMMO_GRENADE
                || ammotype == AMMO_ROCKETS
                || slot == ammotype
                || startslot == ammotype
                || ammotype == AMMO_TIMEDMINE
                || ammotype == AMMO_GRENADEROUND
                || ammotype == AMMO_EXPLOSIVEPEN
                || ammotype == AMMO_BOMBCASE
                || ammotype == AMMO_DYNAMITE)
        {
            obj->flags = flags | 0x10000000;
        }

        return;
    }

    if (!objIsMortal(obj))
    {
        return;
    }

apply_damage:
    // If the object is not yet destroyed, accumulate damage into the object's maxdamage field.
    if (objGetDestroyedLevel(obj) == 0)
    {
        obj->maxdamage += damage * 250.0f; 
    }
    /**
     * If already destroyed, further damage is based on number of shots taken, not how much damage is taken.
     * maxdamage is repurposed to to hold the number of shots the object has taken since it reached the current destroyed level.
     * When 4 shots are taken the object moves to the next destroyed level.
     */
    else
    {
        shotsmod = objGetShotsTaken(obj) % 4;
        maxdamage = (f32)(4 - shotsmod);

        if (maxdamage < damage)
        {
            damage = maxdamage;
        }
        else if (damage < 1.0f)
        {
            damage = 1.0f;
        }

        obj->maxdamage += damage;
    }

    if (obj->type == PROPDEF_GLASS || obj->type == PROPDEF_TINTED_GLASS)
    {
        if (obj->damage <= obj->maxdamage)
        {
            glassShatter(obj);
        }
    }
    else
    {
        propobjSetDropped(obj->prop, DROPTYPE_DEFAULT);
        objExplode(obj, pos, playernum);
    }

    // What is this?
    if (obj->type == PROPDEF_AMMO)
    {
        if (objGetDestroyedLevel(obj) == 1)
        {
            startslot = randomGetNext() % AMMOTYPE_GLOBAL_MAX;
            slot = startslot;

            do
            {
                s32 off = slot * sizeof(struct multiammocrateslot);
                slotview = (MultiAmmoCrateRecord *)((u8 *)obj + off);

                if (slotview->slots[0].quantity > 0 && slotview->slots[0].modelnum != 0xffff)
                {
                    crate = ammocrateAllocate();

                    if (crate != NULL)
                    {
                        u16 modelnum = slotview->slots[0].modelnum;

                        tmpobj = blank_07_object;
                        *(ObjectRecord *)crate = tmpobj;

                        // Matching trick.
                        slotview++;
                        slotview--;

                        crate->obj = modelnum;
                        crate->ammoType = slot + 1;

                        if (crate->ammoType == AMMO_9MM_2)
                        {
                            crate->ammoType = AMMO_9MM;
                        }

                        if (objInitWithModelDef((ObjectRecord *)crate, PitemZ_entries[modelnum].header) != NULL)
                        {
                            modelSetScale(crate->model, crate->model->scale);
                            propReparent(crate->prop, obj->prop);
                        }

                        break;
                    }
                }

                slot++;
                slot %= AMMOTYPE_GLOBAL_MAX;
            }
            while (slot != startslot);
        }
    }

    /**
     * For drone guns, CCTV cameras, and monitors, cease unique functionality when destroyed, set monitors to black.
     */
    if (obj->type == PROPDEF_AUTOGUN)
    {
        obj->flags |= PROPFLAG_NO_AMMO;

        if (objGetDestroyedLevel(obj) == 1)
        {
            obj->flags |= PROPFLAG_IS_DRONE_GUN;
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
            monitorSetAnimation(&((MonitorObjRecord *)obj)->Monitor, monAnim33BlackSolid);
        }
    }
    else if (obj->type == PROPDEF_MULTI_MONITOR)
    {
        if (objGetDestroyedLevel(obj) == 1)
        {
            monitorSetAnimation(&((MultiMonitorObjRecord *)obj)->Monitor[0], monAnim33BlackSolid);
            monitorSetAnimation(&((MultiMonitorObjRecord *)obj)->Monitor[1], monAnim33BlackSolid);
            monitorSetAnimation(&((MultiMonitorObjRecord *)obj)->Monitor[2], monAnim33BlackSolid);
            monitorSetAnimation(&((MultiMonitorObjRecord *)obj)->Monitor[3], monAnim33BlackSolid);
        }
    }
    else if (obj->type == PROPDEF_GAS_RELEASING)
    {
        if (objGetDestroyedLevel(obj) == 1)
        {
            gasStart(&obj->runtime_pos);
        }
    }
    // Damage done to an armor gets subtracted from the amount of armor it provides when picked up.
    else if (obj->type == PROPDEF_ARMOUR)
    {
        if (objGetDestroyedLevel(obj) == 0)
        {
            ((BodyArmourRecord *)obj)->amount = ((BodyArmourRecord *)obj)->initialamount
                * (obj->damage - obj->maxdamage) / obj->damage;
        }
        else
        {
            ((BodyArmourRecord *)obj)->amount = 0.0f;
        }
    }

    // If an object is destroyed drop its children, for example the monitor holders in Bunker.
    if (objGetDestroyedLevel(obj) == 1)
    {
        child = obj->prop->child;

        while (child != NULL)
        {
            PropRecord *nextchild = child->prev;

            propobjSetDropped(child, DROPTYPE_DEFAULT);
            child = nextchild;
        }
    }
}


/**
 * This is the former non-matching block for objTestShotHit. These asserts belong somewhere in objTestShotHit
 *
 *   void objTestShotHit(PropRecord* prop, struct ShotData* hitinfo) {
 *      mtx4TransformVecInPlace(?, hitinfo.hitpos);
 *  assert(!IsBadVec3d( (coord3d*)hitinfo.hitpos  );
 *  mtx4TransformVecInPlace(?, hitinfo.normal);
 *         assert(!IsBadVec3d( (coord3d*)hitinfo.normal  );
 * }
 */

void objTestShotHit(PropRecord *prop, struct ShotData *hitinfo)
{
    ObjectRecord *obj = prop->obj;
    Model *model;
    PropRecord *child;
    PropRecord *next;
    s32 hitpart;
    ModelNode *node;
    HitThing hit;
    s32 mtxindex;
    coord3d pos;
    ModelNode *hitnode;


    if (((obj->runtime_bitflags & RUNTIMEBITFLAG_00001000) == FALSE) && (prop->flags & PROPFLAG_ONSCREEN))
    {
        // Dummy goto is needed for matching.
        goto dummy_label_995911;

dummy_label_995911:
        child = prop->child;

        while (child != NULL)
        {
            next = child->prev;
            objTestShotHit(child, hitinfo);
            child = next;
        }

        model = obj->model;
        node = NULL;

        if ((obj->type == PROPDEF_DOOR) && (((((DoorRecord *) obj)->doorFlags & DOORFLAG_CLIP_TO_BBOX) || (((DoorRecord *) obj)->doorType == DOORTYPE_EYE)) || (((DoorRecord *) obj)->doorType == DOORTYPE_IRIS)))
        {
            hitpart = modelTestRayIntersectsTransformedBBox(&((DoorRecord *) obj)->bbox, &model->render_pos->pos, &hitinfo->viewOrigin, &hitinfo->viewDir);
            node = model->obj->RootNode;

            if (hitpart > HIT_NULL_PART)
            {
                if (!propobjFindHit(model, node, &hitinfo->viewOrigin, &hitinfo->viewDir, &hit, &mtxindex, &hitnode))
                {
                    hitpart = HIT_NULL_PART;
                }
            }
        }
        else
        {
            do
            {
                hitpart = modelFindRayHitNext(model, &hitinfo->viewOrigin, &hitinfo->viewDir, &node);

                if (hitpart > HIT_NULL_PART)
                {
                    if (propobjFindHit(model, node, &hitinfo->viewOrigin, &hitinfo->viewDir, &hit, &mtxindex, &hitnode))
                    {
                        break;
                    }
                }
            }
            while (hitpart > HIT_NULL_PART);
        }

        if (hitpart > HIT_NULL_PART)
        {
            bool penetrates;
            Mtxf *mtx;

            mtx = &model->render_pos[mtxindex].pos;
            pos.x = hit.hitpos.x;
            pos.y = hit.hitpos.y;
            pos.z = hit.hitpos.z;

            mtx4TransformVecInPlace(mtx, &pos);

            if ((-pos.z) <= hitinfo->maxdist)
            {
                penetrates = TRUE;

                if ((obj->flags & PROPFLAG2_00020000) == FALSE)
                {
                    if ((obj->type == PROPDEF_GLASS) || (obj->type == PROPDEF_TINTED_GLASS))
                    {
                        penetrates = FALSE;
                    }
                    else if ((obj->model->obj->Skeleton == (&skeleton_door)) && (hitnode == obj->model->obj->Switches[3]))
                    {
                        penetrates = FALSE;
                    }
                }

                shotAddHit(hitinfo, prop, -pos.z, hitpart, node, &hit, mtxindex, hitnode, model, penetrates, (obj->flags2 & PROPFLAG2_00100000) != FALSE);
            }
        }
    }
}


// PD: obj_test_hit
void objTestShot(PropRecord* prop, struct ShotData* shotdata)
{
    ObjectRecord *obj;
    f32 tmp;
    Model *model;
    struct ModelRoData_BoundingBoxRecord *bbox;

    obj = prop->obj;
    model = obj->model;
    bbox = chrobjGetBboxFromObjectRecord(obj);

    if ((prop->flags & PROPFLAG_ONSCREEN)
            && (obj->runtime_bitflags & RUNTIMEBITFLAG_00001000) == 0
            && (obj->flags2 & PROPFLAG2_SHOOTTHROUGH) == 0) {
        tmp = -(model->render_pos->pos.m[3][2] + bboxRotatedMaxZ(bbox, (Mtxf*)model->render_pos));

        if (tmp <= shotdata->maxdist) {
            objTestShotHit(prop, (void*)shotdata);
        }
    }
}


void objHit(ShotData *shotdata, BulletHit *hit)
{
    ObjectRecord *obj;
    coord3d pos;
    PropRecord *rootprop;

    rootprop = hit->prop;

    while (rootprop->parent != NULL)
    {
        rootprop = rootprop->parent;
    }

    obj = hit->prop->obj;

    pos.x = shotdata->viewOrigin.x - ((hit->dist * shotdata->viewDir.x) / shotdata->viewDir.z);
    pos.y = shotdata->viewOrigin.y - ((hit->dist * shotdata->viewDir.y) / shotdata->viewDir.z);
    pos.z = shotdata->viewOrigin.z - hit->dist;

    pos.x -= 26.0f * shotdata->viewDir.x;
    pos.y -= 26.0f * shotdata->viewDir.y;
    pos.z -= 26.0f * shotdata->viewDir.z;

    mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &pos);

    if (hit->countsAsPenetration != 0)
    {
        gunSetTracerTarget(&pos);
    }

    sparkCreate(&pos, 1, 26.0f, rootprop->stan->room);

    if ((objIsHealthy(obj) && objIsMortal(obj)) && (hit->countsAsPenetration != 0))
    {
        statsAddHit(shotdata->weapon, SHOT_REGISTER_OBJECT);
    }

    if (hit->countsAsPenetration == 0)
    {
        shotPlayGlassSound(&hit->prop->pos);
    }
    else
    {
        shotPlayHitPropSound(shotdata->weapon, hit->prop, hit->hit.texturenum);
    }

    if (shotdata->weapon != ITEM_WATCHLASER)
    {
        if (hit->countsAsPenetration == 0)
        {
            PropRecord *hitprop;
            s8 room_clear_flag;
            s16 impact_type;

            hitprop = hit->prop;
            room_clear_flag = 0;

            if (obj->model->obj->Skeleton == &skeleton_door)
            {
                room_clear_flag = 1;
            }

            impact_type = (randomGetNext() % 3) + 0x11;

            explosionCreateBulletImpact(&hit->hit.hitpos, &hit->hit.normal, impact_type, 1, hitprop, hit->room, room_clear_flag);
        }
        else
        {
            const SurfaceHit *surface;
            s32 impact_index;
            s8 room_clear_flag;
            s16 texturenum;

            texturenum = hit->hit.texturenum;
            room_clear_flag = 0;

            if (texturenum < 0)
            {
                surface = &g_SurfaceHits[HIT_DEFAULT];
            }
            else
            {
                surface = &g_SurfaceHits[g_Textures[texturenum].hitTexture];
            }

            impact_index = randomGetNext() % surface->numImpacts;

            if (((obj->model->obj->Skeleton == &skeleton_door) && (hit->bboxNode == obj->model->obj->Switches[3])) || ((obj->model->obj->Skeleton == &skeleton_cctv) && (hit->bboxNode == obj->model->obj->Switches[1])))
            {
                room_clear_flag = 1;
            }

            explosionCreateBulletImpact(&hit->hit.hitpos, &hit->hit.normal, surface->impacts[impact_index], 1, hit->prop, hit->room, room_clear_flag);
        }
    }

    {
        f32 damage;

        damage = weaponGetDamage(shotdata->weapon);

        // On Agent player shots to drone guns do 2x normal damage.
        if (obj->type == PROPDEF_AUTOGUN)
        {
            damage *= g_AutogunDamageScale;
        }
        // Shots to CCTV camera glass do 100x normal damage.
        else if (obj->type == PROPDEF_CCTV)
        {
            if (obj->model->obj->Skeleton == &skeleton_cctv)
            {
                if (hit->bboxNode == obj->model->obj->Switches[1])
                {
                    damage *= 100.0f;
                    objBreakCCTVGlass(obj);
                }
            }

            damage *= g_CctvDamageScale;
        }

        chrobjMaybeDetonateObjectIfFlags(obj, damage, &pos, shotdata->weapon, playerGetCurrentNum());
    }

    if ((obj->model->obj->Skeleton == &skeleton_door) && (hit->countsAsPenetration == 0))
    {
        ((DoorRecord *)obj)->numHits++;

        if (((DoorRecord *)obj)->numHits >= 3)
        {
            doorShatterGlass((DoorRecord *)obj);
        }
    }

    objDropRecursively(hit->prop);

    {
        bool do_bounce;

        do_bounce = FALSE;

        if (objIsCollectable(obj))
        {
            if (!(obj->flags & PROPFLAG_00400000))
            {
                do_bounce = TRUE;
            }
        }
        else if (obj->flags & PROPFLAG_00200000)
        {
            do_bounce = TRUE;
        }

        if (obj->flags2 & PROPFLAG2_00000002)
        {
            if (!objIsHealthy(obj))
            {
                do_bounce = TRUE;
            }
        }

        if (obj->flags2 & PROPFLAG2_LINKEDTOSAFE)
        {
            do_bounce = FALSE;
        }

        if (do_bounce)
        {
            objBounce(obj, &shotdata->viewDir);
        }
    }
}


bool objIsHealthy(ObjectRecord *self) //#MATCH
{
    return objGetDestroyedLevel(self) == 0;
}


bool objTestForInteract(PropRecord* prop)
{
    f32 xdiff;
    ObjectRecord *obj;
    PropRecord *player;
    f32 maxdistSq;
    f32 ydiff;
    f32 zdiff;
    f32 maxYdiff;
    f32 anglediff;
    f32 playerangle;
    f32 maxAngle;
    StandTile *stan;
    f32 xzdiff;
    f32 angle;

    obj = prop->obj;

    // Counter-op: of the objects, the guard player can only use alarms.
    // Consoles, switches and mission objects are Bond's.
    if (counterOpIsGuardPlayer(playerGetCurrentNum()) && (obj->type != PROPDEF_ALARM))
    {
        return TRUE;
    }

    if (((obj->type == PROP_TYPE_PLAYER)
            || (obj->flags & PROPFLAG_00080000)
            || (obj->runtime_bitflags & (RUNTIMEBITFLAG_00000001 | RUNTIMEBITFLAG_00000002 | RUNTIMEBITFLAG_TAGGED))))
    {
        if ((prop->flags & PROPFLAG_ONSCREEN)
                && (objIsHealthy(obj) != 0)
                && !(obj->flags & PROPFLAG_CANNOT_ACTIVATE))
        {

            player = currentPlayerGetProp();

            xdiff = obj->runtime_pos.x - player->pos.x;
            ydiff = obj->runtime_pos.y - player->pos.y;
            zdiff = obj->runtime_pos.z - player->pos.z;

            stan = player->stan;

            if ((obj->type == PROPDEF_AIRCRAFT) && (obj->flags & PROPFLAG_DOOR_OPENTOFRONT))
            {
                maxYdiff = 400.0f;
                maxdistSq = 160000.0f;
                maxAngle = 2.0943952f;
            }
            else
            {
                maxYdiff = 200.0f;
                maxdistSq = 40000.0f;
                maxAngle = 0.3926991f;
            }

            xzdiff = ((xdiff * xdiff) + (zdiff * zdiff));

            if ((xzdiff < maxdistSq) && (ydiff < maxYdiff) && (-maxYdiff < ydiff))
            {

                angle = mathAtan2(xdiff, zdiff);
                playerangle = bondviewGetPlayerYawRadians();
                anglediff = angle - playerangle;

                if (angle < playerangle)
                {
                    anglediff += M_TAU_F;
                }

                if (anglediff > M_PI_F)
                {
                    anglediff = M_TAU_F - anglediff;
                }

                if (anglediff <= maxAngle)
                {
                    if (!(obj->flags2 & PROPFLAG2_INTERACTCHECKLOS) || (stanWalkLine(&stan, player->pos.x, player->pos.z, prop->pos.x, prop->pos.z) != 0))
                    {
                        g_InteractProp = prop;
                    }
                }
            }
        }
    }
    return TRUE;
}


/*
 * Return TYPE if Collected or Interacted (except for Alarm which always returns False)
 */
TICKOP propobjInteract(PropRecord *prop)
{
    ObjectRecord *obj        = prop->obj;
    TICKOP op = TICKOP_NONE;

    if (obj->type == PROPDEF_ALARM)
    {
        sndPlaySfx(g_musicSfxBufferPtr, ALARM_SWITCH_SFX, 0);
        if (alarmIsActive())
        {
            alarmDeactivate();
        }
        else
        {
            alarmActivate();
        }
    }

    if (obj->flags & PROPFLAG_00080000)
    {
        op = propPickupByPlayer(prop, TRUE);
    }

    obj->runtime_bitflags |= RUNTIMEBITFLAG_ACTIVATED;
    coopNoteActivator();
    switchActivateLinkedDoors(prop);

    return op;
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


void pickupAppendPickedUp(char *buffer)
{
  char *str;

  str = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_00_PICKEDUP)); //Picked up
  strcat(buffer,str);
  return;
}


void pickupAppendAmmoAmount(char *buffer, AMMOTYPE ammotype,u32 amount)
{
    char *textfiletext;

    switch(ammotype) {
    case AMMO_9MM:
    case AMMO_9MM_2:
    case AMMO_RIFLE:
    case AMMO_PLASTIQUE:
        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_01_SOME)); //some
        strcat(buffer,textfiletext);
        break;
    case AMMO_SHOTGUN:
    case AMMO_GRENADE:
    case AMMO_ROCKETS:
    case AMMO_REMOTEMINE:
    case AMMO_PROXMINE:
    case AMMO_TIMEDMINE:
    case AMMO_KNIFE:
    case AMMO_GRENADEROUND:
    case AMMO_MAGNUM:
    case AMMO_GGUN:
    case AMMO_DARTS:
    case AMMO_FLARE:
    case AMMO_PITON:
    case AMMO_DYNAMITE:
    case AMMO_BUG:
    case AMMO_MICRO_CAMERA:
        if (amount == 1) {
            textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_02_A)); //a
            strcat(buffer,textfiletext);
        }
        else {
            textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_01_SOME)); //some
            strcat(buffer,textfiletext);
        }
        break;
    case AMMO_EXPLOSIVEPEN:
    case AMMO_BOMBCASE:
        if (amount == 1) {
            textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_03_AN)); //an
            strcat(buffer,textfiletext);
        }
        else {
            textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_01_SOME)); //some
            strcat(buffer,textfiletext);
        }
        break;
    case AMMO_GEKEY:
    case AMMO_TOKEN:
        if (amount == 1) {
            textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_04_THE)); //the
            strcat(buffer,textfiletext);
        }
        else {
            textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_01_SOME)); //some
            strcat(buffer,textfiletext);
        }

    default:
        break;
    }
    return;
}


void pickupAppendAmmoType(char *buffer, AMMOTYPE ammotype, s32 amount)
{
    char *textfiletext;
    if (((ammotype == AMMO_9MM) || (ammotype == AMMO_9MM_2)) || (ammotype == AMMO_RIFLE))
    {
        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_05_AMMO)); //ammo
        strcat(buffer,textfiletext);
    }
    else
    {
        if (ammotype == AMMO_KNIFE)
        {
            textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_0F_THROWING)); //throwing
            strcat(buffer,textfiletext);
            if (amount == 1)
            {
                textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_10_KNIFE)); //knife
                strcat(buffer,textfiletext);
            }
            else
            {
                textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_11_KNIVES)); //knives
                strcat(buffer,textfiletext);
            }
        }
        else
        {
            if (ammotype == AMMO_DYNAMITE)
            {
                if (amount == 1)
                {
                    textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_19_STICK)); //stick
                    strcat(buffer,textfiletext);
                }
                else
                {
                    textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_1A_STICKS)); //sticks
                    strcat(buffer,textfiletext);
                }
                textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_18_OFDYNAMITE)); //of dynamite
                strcat(buffer,textfiletext);
            }
            else
            {
                switch(ammotype)
                {
                    case AMMO_SHOTGUN:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_06_SHOTGUNCARTRIDGE)); //shotgun cartridge
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_MAGNUM:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_07_MAGNUMBULLET)); //magnum bullet
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_GGUN:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_08_GOLDENBULLET)); //golden bullet
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_GRENADE:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_09_HANDGRENADE)); //hand grenade
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_GRENADEROUND:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_0A_GRENADEROUND)); //grenade round
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_ROCKETS:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_0B_ROCKET)); //rocket
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_REMOTEMINE:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_0C_REMOTEMINE)); //remote mine
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_PROXMINE:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_0D_PROXIMITYMINE)); //proximity mine
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_TIMEDMINE:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_0E_TIMEDMINE)); //timed mine
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_DARTS:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_13_DART)); //dart
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_EXPLOSIVEPEN:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_14_EXPLOSIVEPEN)); //explosive pen
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_BOMBCASE:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_15_EXPLOSIVECASE)); //explosive case
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_FLARE:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_16_FLARE)); //flare
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_PITON:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_17_PITON)); //piton
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_BUG:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_1B_BUG)); //bug
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_MICRO_CAMERA:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_1C_MICROCAMERA)); //micro camera
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_GEKEY:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_1D_GOLDENEYEKEY)); //GoldenEye key
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_TOKEN:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_1E_TOKEN)); //token
                        strcat(buffer,textfiletext);
                        break;
                    case AMMO_PLASTIQUE:
                        textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_1F_PLASTIQUE)); //plastique
                        strcat(buffer,textfiletext);
                        break;

                    default:
                        break;
                }
                if (1 < amount)
                {
                    textfiletext = langGet(getStringID(LPROPOBJ,PROPOBJ_STR_12_S)); //s
                    strcat(buffer,textfiletext);
                }
            }
        }
    }
}


void pickupPlayAmmoSound(AMMOTYPE ammotype)
{
    switch(ammotype) {
        case AMMO_9MM:
        case AMMO_9MM_2:
        case AMMO_RIFLE:
        case AMMO_SHOTGUN:
        case AMMO_GRENADE:
        case AMMO_ROCKETS:
        case AMMO_GRENADEROUND:
        case AMMO_MAGNUM:
        case AMMO_GGUN:
        case AMMO_DARTS:
        case AMMO_EXPLOSIVEPEN:
        case AMMO_FLARE:
        case AMMO_PITON:
        case AMMO_DYNAMITE:
        case AMMO_GEKEY:
        case AMMO_TOKEN:
            sndPlaySfx(g_musicSfxBufferPtr,PICKUP_AMMO_SFX,0);
            break;
        case AMMO_REMOTEMINE:
        case AMMO_PROXMINE:
        case AMMO_TIMEDMINE:
        case AMMO_BOMBCASE:
        case AMMO_BUG:
        case AMMO_MICRO_CAMERA:
        case AMMO_PLASTIQUE:
            sndPlaySfx(g_musicSfxBufferPtr,PICKUP_MINE_SFX,0);
            break;
        case AMMO_KNIFE:
            sndPlaySfx(g_musicSfxBufferPtr,PICKUP_KNIFE_SFX,0);

        default:
            break;
    }
}


void pickupPlayWeaponSound(ITEM_IDS weapontype)
{
    if ((weapontype == ITEM_KNIFE) || (weapontype == ITEM_THROWKNIFE))
    {
        sndPlaySfx(g_musicSfxBufferPtr,PICKUP_KNIFE_SFX,0);
    }
    else
    {
        if ((weapontype == ITEM_REMOTEMINE) || (weapontype == ITEM_PROXIMITYMINE) || (weapontype == ITEM_TIMEDMINE) ||
            (weapontype == ITEM_BOMBCASE) || (weapontype == ITEM_BUG) || (weapontype == ITEM_MICROCAMERA) ||
            (weapontype == ITEM_PLASTIQUE))
        {
            sndPlaySfx(g_musicSfxBufferPtr,PICKUP_MINE_SFX,0);
        }
        else
        {
            if ((weapontype == ITEM_GRENADE) || (weapontype == ITEM_GRENADEROUND ) || (weapontype == ITEM_ROCKETROUND))
            {
                sndPlaySfx(g_musicSfxBufferPtr,PICKUP_AMMO_SFX,0);
            }
            else
            {
                if (weapontype == ITEM_LASER)
                {
                    sndPlaySfx(g_musicSfxBufferPtr,PICKUP_LASER_SFX,0);
                }
                else
                {
                    sndPlaySfx(g_musicSfxBufferPtr,PICKUP_GUN_SFX,0);
                }
            }
        }
    }
}


//!FIXME, i need to be properly split from chrai.c
void pickupBuildAmmoText(char *finaltext, AMMOTYPE ammotype, u32 quantity)
{
    *finaltext = 0;
    if (!viewIsHalfWidth())
    {
        pickupAppendPickedUp(finaltext);
        pickupAppendAmmoAmount(finaltext, ammotype, quantity);
    }
    pickupAppendAmmoType(finaltext, ammotype, quantity);
    strcat(finaltext, ".\n");
}


void pickupShowAmmoText(s32 ammotype, s32 quantity)
{
    char buffer[100] = "";
    pickupBuildAmmoText(buffer, ammotype, quantity);
#ifdef VERSION_US
    hudmsgBottomShow(buffer);
#else
    hudmsgBottomShowText(buffer);
#endif
}

void pickupAddAmmo(AMMOTYPE ammotype,int amount,int doplaysound,int dodisplaytext)
{
    int curammo;
    int maxammo;

    if (0 < amount)
    {
        curammo = ammoGetHeld(ammotype);
        maxammo = ammoGetMax(ammotype);
        if (curammo < maxammo)
        {
            curammo = ammoGetHeld(ammotype);
            ammoGive(ammotype,curammo + amount);
        }
            if (dodisplaytext != 0)
            {
                pickupShowAmmoText(ammotype,amount);
            }

            if (doplaysound != 0)
            {
                pickupPlayAmmoSound(ammotype);
            }

            if (ammotype == AMMO_GRENADE)
            {
                invAddItem(ITEM_GRENADE);
            }
            else if (ammotype == AMMO_REMOTEMINE)
            {
                invAddItem(ITEM_REMOTEMINE);
                invAddItem(ITEM_TRIGGER);
            }
            else if (ammotype == AMMO_PROXMINE)
            {
                invAddItem(ITEM_PROXIMITYMINE);
            }
            else if (ammotype == AMMO_TIMEDMINE)
            {
                invAddItem(ITEM_TIMEDMINE);
            }
            else if (ammotype == AMMO_KNIFE)
            {
                invAddItem(ITEM_THROWKNIFE);
            }
            else if (ammotype == AMMO_BOMBCASE)
            {
                invAddItem(ITEM_BOMBCASE);
            }
            else if (ammotype == AMMO_BUG)
            {
                invAddItem(ITEM_BUG);
            }
            else if (ammotype == AMMO_MICRO_CAMERA)
            {
                invAddItem(ITEM_MICROCAMERA);
            }
            else if (ammotype == AMMO_GEKEY)
            {
                invAddItem(ITEM_GOLDENEYEKEY);
            }
            else if (ammotype == AMMO_TOKEN)
            {
                invAddItem(ITEM_TOKEN);
            }
            else if (ammotype == AMMO_PLASTIQUE)
            {
                invAddItem(ITEM_PLASTIQUE);
            }
    }
}


s32 weaponGetAmmoInMagazine(AmmoCrateRecord *crate)
{
    s32 qty = 1;

    switch (crate->ammoType)
    {
        case AMMO_9MM:     qty = 10; break;
        case AMMO_9MM_2:   qty = 10; break;
        case AMMO_RIFLE:   qty = 10; break;
        case AMMO_SHOTGUN: qty =  5; break;
        case AMMO_MAGNUM:  qty =  5; break;
        case AMMO_GGUN:    qty =  3; break;
        case AMMO_DARTS:   qty =  4; break;

        default:
            break;
    }

    if (qty > 1 && !isMpGame())
    {
        qty *= g_SoloAmmoMultiplier;
    }

    return qty;
}

s32 pickupAmmoFromWeapon(WeaponObjRecord *weapon)
{
    s32 ammotype;
    s32 qty;

    ammotype = weaponGetAmmoType(weapon->weaponnum);
    qty = 1;

    if (weapon->flags & PROPFLAG_NO_AMMO)
    {
        return 0;
    }

    switch (ammotype)
    {
        case AMMO_9MM:          qty = 10; break;
        case AMMO_9MM_2:        qty = 10; break;
        case AMMO_RIFLE:        qty = 10; break;
        case AMMO_SHOTGUN:      qty =  5; break;
        case AMMO_MAGNUM:       qty =  5; break;
        case AMMO_GGUN:         qty =  3; break;
        case AMMO_DARTS:        qty =  4; break;
        case AMMO_GRENADEROUND: qty =  3; break;
    }

    if (qty > 1 && !isMpGame())
    {
        qty *= g_SoloAmmoMultiplier;
    }

    return qty;
}


void pickupBuildWeaponText(char *finalstring, ITEM_IDS itemtype)
{
    /* Half-width views skip "Picked up" and strcat onto this, so it must start empty
       (the original left the caller's stack buffer uninitialised) */
    finalstring[0] = '\0';

    if (!viewIsHalfWidth())
    {
        //Picked up
        strcpy(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_00_PICKEDUP)));
    }

    switch(itemtype)
    {
        case ITEM_THROWKNIFE:
        case ITEM_GRENADE:
        case ITEM_TIMEDMINE:
        case ITEM_PROXIMITYMINE:
        case ITEM_REMOTEMINE:
        case ITEM_BOMBCASE:
        case ITEM_PLASTIQUE:
        case ITEM_BUG:
        case ITEM_MICROCAMERA:
        case ITEM_GOLDENEYEKEY:
        case ITEM_ROCKETROUND:
        case ITEM_GRENADEROUND :
        case ITEM_TOKEN:
            pickupBuildAmmoText(finalstring,weaponGetAmmoType(itemtype),1);
            return;
        case ITEM_KNIFE:
            //a hunting knife.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_20_AHUNTINGKNIFE)));
            break;
        case ITEM_WPPK:
            //a PP7.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_21_APPK)));
            break;
        case ITEM_WPPKSIL:
            //a silenced PP7.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_22_ASILENCEDPPK)));
            break;
        case ITEM_TT33:
            //a DD44 Dostovei.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_23_ATT33)));
            break;
        case ITEM_SKORPION:
            //a Klobb.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_24_ASPKORPION)));
            break;
        case ITEM_AK47:
            //a KF7 Soviet.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_25_ANAK47)));
            break;
        case ITEM_UZI:
            //a ZMG (9mm).
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_26_ANUZI)));
            break;
        case ITEM_MP5K:
            //a D5K Deutsche.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_27_ANMP5K)));
            break;
        case ITEM_MP5KSIL:
            //a silenced D5K.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_28_ASILENCEDMP5)));
            break;
        case ITEM_SPECTRE:
            //a Phantom.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_29_ASPECTRE)));
            break;
        case ITEM_M16:
            //an AR33 assault rifle.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_2A_ANM16)));
            break;
        case ITEM_FNP90:
            //an RC-P90.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_2B_ANFNP90)));
            break;
        case ITEM_SHOTGUN:
            //a shotgun.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_2C_ASHOTGUN)));
            break;
        case ITEM_AUTOSHOT:
            //an automatic shotgun.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_2D_ANAUTOSHOTGUN)));
            break;
        case ITEM_SNIPERRIFLE:
            //a sniper rifle.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_2E_ASNIPERRIFLE)));
            break;
        case ITEM_GRENADELAUNCH:
            //a grenade launcher.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_2F_AGRENADELAUNCHER)));
            break;
        case ITEM_ROCKETLAUNCH:
            //a rocket launcher.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_30_AROCKETLAUNCHER)));
            break;
        case ITEM_RUGER:
            //a Cougar Magnum.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_31_ARUGERMAGNUM)));
            break;
        case ITEM_GOLDENGUN:
            //the Golden Gun.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_32_THEGOLDENGUN)));
            break;
        case ITEM_LASER:
            //a Moonraker laser.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_33_AMOOKRAKERLASER)));
            break;
        case ITEM_FLAREPISTOL:
            //a flare pistol.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_34_AFLAREPISTOL)));
            break;
        case ITEM_PITONGUN:
            //a piton gun.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_35_APITONGUN)));
            break;
        case ITEM_SILVERWPPK:
            //a silver PP7.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_36_ASILVERPPK)));
            break;
        case ITEM_GOLDWPPK:
            //a gold PP7.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_37_AGOLDPPK)));
            break;
        case ITEM_KEYCARD:
            //a keycard.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_38_AKEYCARD)));
            break;
        case ITEM_KEYYALE:
            //a yale key.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_39_AYALEKEY)));
            break;
        case ITEM_KEYBOLT:
            //a bolt key.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_3A_ABOLTKEY)));
            break;
        default:
            //a new weapon.
            strcat(finalstring, langGet(getStringID(LPROPOBJ,PROPOBJ_STR_3B_ANEWWEAPON)));
            break;
    }


}


void pickupShowWeaponText(ITEM_IDS weaponid)
{
    char acStack100 [100];

    pickupBuildWeaponText(acStack100,weaponid);
    HUDMESSAGEBOTTOM(acStack100);
    return;
}


// Perfect Dark propobj.c: s32 propPickupByPlayer(struct prop *prop, bool showhudmsg)
TICKOP propPickupByPlayer(PropRecord *prop, bool showstring)
{
    ObjectRecord *obj;
    TICKOP op;

    op = TICKOP_NONE;
    obj = prop->obj;

    if (g_CurrentPlayer->bonddead || g_ClockTimer == 0)
    {
        return TICKOP_NONE;
    }

    // Counter-op: the guard player only has their guard's gun, so they
    // can't take Bond's weapons or ammo.
    if (counterOpIsGuardPlayer(playerGetCurrentNum()))
    {
        return TICKOP_NONE;
    }

    switch (obj->type)
    {
        case PROPDEF_KEY:
        {
            sndPlaySfx(g_musicSfxBufferPtr, KEYCARD_SFX, 0);

            if (showstring)
            {
                char *text = invGetActivatedTextObject(obj);

                if (text == NULL)
                {
                    text = langGet(getStringID(LPROPOBJ, PROPOBJ_STR_3C_PICKEDUPAKEY)); // "Picked up a key.\n",
                }

#if defined(VERSION_EU)
                hudmsgBottomShowText(text);
#else
                hudmsgBottomShow(text);
#endif
            }

            op = TICKOP_GIVETOPLAYER;

            break;
        }

        case PROPDEF_MAGAZINE:
        {
            struct AmmoCrateRecord *crate;
            s32 amount;

            crate = (struct AmmoCrateRecord *)prop->obj;

            amount = weaponGetAmmoInMagazine(crate);
            pickupAddAmmo(crate->ammoType, amount, 1, showstring);

            op = TICKOP_FREE;

            break;
        }

        case PROPDEF_AMMO:
        {
            struct MultiAmmoCrateRecord *multicrate;
            s32 i;
            s32 ammoquantity;
            AMMOTYPE ammotype;

            multicrate = (struct MultiAmmoCrateRecord *)prop->obj;

            for (i = AMMO_NONE; i < AMMOTYPE_GLOBAL_MAX; i++)
            {
                ammotype = i + 1;

                if (ammotype == AMMO_9MM_2)
                {
                    ammotype = AMMO_9MM;
                }

                ammoquantity = multicrate->slots[i].quantity;

                if (!isMpGame())
                {
                    ammoquantity *= g_SoloAmmoMultiplier;
                }

                pickupAddAmmo(ammotype, ammoquantity, 0, showstring);
            }

            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_AMMO_SFX, 0);

            op = TICKOP_FREE;

            break;
        }

        case PROPDEF_COLLECTABLE: // weapon
        {
            WeaponObjRecord* wep;
            bool collected;
            s32 ammo_type;

            collected = 0;
            wep = (WeaponObjRecord *)prop->obj;

            pickupPlayWeaponSound(wep->weaponnum);

            if (wep->weaponnum == ITEM_REMOTEMINE)
            {
                invAddItem(ITEM_TRIGGER);
            }
            else if (wep->weaponnum == ITEM_TOKEN)
            {
                gunEquip(GUNRIGHT, ITEM_TOKEN);
            }

            if (obj->runtime_bitflags & RUNTIMEBITFLAG_DESTROYED)
            {
                if (wep->weaponnum < ITEM_BOMBCASE)
                {
                    invAddWeaponByProp(prop);
                }

                if (showstring)
                {
                    char *text = invGetActivatedTextObject(obj);

                    if (text)
                    {
#if defined(VERSION_EU)
                        hudmsgBottomShowText(text);
#else
                        hudmsgBottomShow(text);
#endif
                    }
                    else
                    {
                        pickupShowWeaponText(wep->weaponnum);
                    }

                    collected = 1;
                }

                op = TICKOP_GIVETOPLAYER;
            }
            else
            {
                if (invAddWeaponByProp(prop) != 0)
                {
                    collected = 1;
                }

                if (showstring)
                {
                    char *text = invGetActivatedTextWeapon(wep->weaponnum);

                    if (text)
                    {
                        collected = 1;

#if defined(VERSION_EU)
                        hudmsgBottomShowText(text);
#else
                        hudmsgBottomShow(text);
#endif
                    }
                    else if (collected)
                    {
                        pickupShowWeaponText(wep->weaponnum);
                    }
                }

                op = TICKOP_FREE;
            }

            ammo_type = weaponGetAmmoType(wep->weaponnum);

            if (ammo_type)
            {
                s32 pickupqty = pickupAmmoFromWeapon(wep);

                if (pickupqty > 0)
                {
                    if (ammoGetHeld(ammo_type) < ammoGetMax(ammo_type))
                    {
                        s32 heldqty = ammoGetHeld(ammo_type);

                        ammoGive(ammo_type, heldqty + pickupqty);

                        if ((collected == 0) && showstring)
                        {
                            pickupShowAmmoText(ammo_type, pickupqty);
                        }
                    }
                }
            }

            break;
        }

        case PROPDEF_ARMOUR: // Body armor
        {
            currentPlayerAddArmor(((struct BodyArmourRecord *)prop->obj)->amount);
            sndPlaySfx(g_musicSfxBufferPtr, ARMOUR_COLLECT_SFX, 0);

            if (showstring)
            {
                char *text = invGetActivatedTextObject(obj);

                if (text == NULL)
                {
                    if (!viewIsHalfWidth())
                    {
                        text = langGet(getStringID(LPROPOBJ, PROPOBJ_STR_3D_PICKEDUPSOMEBODEYARMOUR)); // "Picked up some body armor.\n",
                    }
                    else
                    {
                        text = langGet(getStringID(LPROPOBJ, PROPOBJ_STR_3E_BODYARMOUR)); // "body armor.\n",
                    }

                }

#if defined(VERSION_EU)
                hudmsgBottomShowText(text);
#else
                hudmsgBottomShow(text);
#endif
            }

            op = TICKOP_FREE;

            break;
        }

        case PROPDEF_PROP:
        //case PROPDEF_ALARM:
        //case PROPDEF_CCTV:
        //case PROPDEF_GUARD:
        //case PROPDEF_MONITOR:
        //case PROPDEF_MULTI_MONITOR:
        //case PROPDEF_RACK:
        //case PROPDEF_AUTOGUN:
        //case PROPDEF_LINK:
        //case PROPDEF_DEBRIS:
        //case PROPDEF_UNK16:
        //case PROPDEF_HAT:
        //case PROPDEF_GUARD_ATTRIBUTE:
        //case PROPDEF_SWITCH:
        case PROPDEF_TAG:
        //case PROPDEF_OBJECTIVE_START:
        //case PROPDEF_OBJECTIVE_END:
        //case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
        //case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
        //case PROPDEF_OBJECTIVE_FAIL_CONDITION:
        //case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
        //case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
        //case PROPDEF_OBJECTIVE_PHOTOGRAPH:
        //case PROPDEF_OBJECTIVE_NULL:
        //case PROPDEF_OBJECTIVE_ENTER_ROOM:
        //case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
        //case PROPDEF_OBJECTIVE_COPY_ITEM:
        //case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
        //case PROPDEF_GAS_RELEASING:
        //case PROPDEF_RENAME:
        //case PROPDEF_LOCK_DOOR:
        //case PROPDEF_VEHICHLE:
        //case PROPDEF_AIRCRAFT:
        //case PROPDEF_UNK41:
        //case PROPDEF_GLASS:
        //case PROPDEF_SAFE:
        //case PROPDEF_SAFE_ITEM:
        //case PROPDEF_TANK:
        //case PROPDEF_CAMERAPOS:
        case PROPDEF_TINTED_GLASS:
        default:
        {
            sndPlaySfx(g_musicSfxBufferPtr, KEYCARD_SFX, 0);

            if (showstring)
            {
                char *text = invGetActivatedTextObject(obj);
                if (text == NULL)
                {
                    text = langGet(getStringID(LPROPOBJ, PROPOBJ_STR_3F_PICKEDUPSOMETHING)); // "Picked up something.\n",
                }

#if defined(VERSION_EU)
                hudmsgBottomShowText(text);
#else
                hudmsgBottomShow(text);
#endif
            }

            op = TICKOP_GIVETOPLAYER;

            break;
        }
    }

    if ((op == TICKOP_FREE) && ((obj->runtime_bitflags & RUNTIMEBITFLAG_TAGGED) == 0))
    {
        objFree(obj, 0, (obj->state & RUNTIMEBITFLAG_REMOVE));

        return TICKOP_FREE;
    }

    if (op != TICKOP_NONE)
    {
        invAddProp(prop);

        return TICKOP_GIVETOPLAYER;
    }

    return TICKOP_NONE;
}


// They're missing from .h associated with them, meaning they're only found in the .c file.
// Move them to a .h to fix the issue. This will also fix the issue with other functions
// which couldn't be brought from decomp.me

extern bool objCanPickupFromSafe(ObjectRecord *obj);
extern s32 invHasItem(ITEM_IDS weapon);
extern s32 weaponGetAmmoType(ITEM_IDS weapon);
extern s32 weaponGetMaxAmmo(ITEM_IDS weapon);
extern s32 weaponGetHeldAmmo(ITEM_IDS weapon);
extern bool invHasDualWeapon(ITEM_IDS right, ITEM_IDS left);
extern s32 ammoGetHeld(AMMOTYPE type);

TICKOP objTickPlayer(struct PropRecord* prop)
{
    struct ObjectRecord* obj;

    obj = prop->obj;

    if ((objIsCollectable(obj) != 0) && (obj->type != PROPDEF_HAT))
    {
        if (obj->flags & PROPFLAG_UNCOLLECTABLE)
        {
            return TICKOP_NONE;
        }
    } 
    else 
    {
        if (!(obj->flags & PROPFLAG_00040000))
        {
            return TICKOP_NONE;
        }
    }

    if (obj->flags & PROPFLAG_00080000)
    {
        return TICKOP_NONE;
    }

    if (obj->runtime_bitflags & 0x80) 
    {
        if (((s32)obj->projectile->refreshrate > 0) && (obj->projectile->numBounces == 0)) 
        {
            return TICKOP_NONE;
        }
    }

    if (objCanPickupFromSafe(obj) == 0) 
    {
        return TICKOP_NONE;
    }

    if (obj->type == PROPDEF_COLLECTABLE) 
    {
        struct WeaponObjRecord* weaponObj;
        s32 leftWeapon;
        s32 rightWeapon;

        weaponObj = (WeaponObjRecord*)prop->obj;

        if (((weaponObj->weaponnum == ITEM_GRENADE) || (weaponObj->weaponnum == ITEM_GRENADEROUND))
            && ((weaponObj->timer >= 0) || (obj->runtime_bitflags & 4)))
        {
            return TICKOP_NONE;
        }

        if (((weaponObj->weaponnum == ITEM_REMOTEMINE)
                || (weaponObj->weaponnum == ITEM_PROXIMITYMINE)
                || (weaponObj->weaponnum == ITEM_TIMEDMINE)
                || (weaponObj->weaponnum == ITEM_BOMBCASE)
                || (weaponObj->weaponnum == ITEM_BUG)
                || (weaponObj->weaponnum == ITEM_MICROCAMERA)
                || (weaponObj->weaponnum == ITEM_PLASTIQUE))
            && ((weaponObj->timer >= 0) || (obj->runtime_bitflags & 4)))
        {
            return TICKOP_NONE;
        }

        if ((weaponObj->weaponnum == ITEM_ROCKETROUND) && (obj->runtime_bitflags & 0x80))
        {
            return TICKOP_NONE;
        }

        if (invHasItem(weaponObj->weaponnum) != 0) 
        {
            if (weaponGetAmmoType(weaponObj->weaponnum) != 0) 
            {
                if (weaponGetHeldAmmo(weaponObj->weaponnum) >= weaponGetMaxAmmo(weaponObj->weaponnum)) 
                {
                    if ((weaponObj->dualweapon != NULL) || (weaponObj->LinkedWeaponType >= 0)) 
                    {
                        if (weaponObj->dualweapon != NULL) 
                        {
                            leftWeapon = weaponObj->dualweapon->weaponnum;
                            rightWeapon = leftWeapon;
                        } 
                        else 
                        {
                            leftWeapon = weaponObj->LinkedWeaponType;
                            rightWeapon = leftWeapon;
                        }

                        if (weaponObj->flags & 0x10000000) 
                        {
                            leftWeapon = weaponObj->weaponnum;
                        } 
                        else 
                        {
                            rightWeapon = weaponObj->weaponnum;
                        }

                        if (invHasDualWeapon(rightWeapon, leftWeapon) != 0) 
                        {
                            return TICKOP_NONE;
                        }
                    } 
                    else 
                    {
                        return TICKOP_NONE;
                    }
                }
            }
        }
    }
    else if (obj->type == PROPDEF_MAGAZINE)
    {
        struct AmmoCrateRecord* ammoCrateObj;

        ammoCrateObj = (AmmoCrateRecord*)prop->obj;

        if (ammoGetHeld(ammoCrateObj->ammoType) >= ammoGetMax(ammoCrateObj->ammoType)
            && !(
                ((ammoCrateObj->ammoType == AMMO_GRENADE) && (!invHasItem(ITEM_GRENADE)))
                || ((ammoCrateObj->ammoType == AMMO_REMOTEMINE) && (!invHasItem(ITEM_REMOTEMINE)))
                || ((ammoCrateObj->ammoType == AMMO_PROXMINE) && (!invHasItem(ITEM_PROXIMITYMINE)))
                || ((ammoCrateObj->ammoType == AMMO_TIMEDMINE) && (!invHasItem(ITEM_TIMEDMINE)))
                || ((ammoCrateObj->ammoType == AMMO_KNIFE) && (!invHasItem(ITEM_THROWKNIFE)))
            )
        ) {
            return TICKOP_NONE;
        }
    } 
    else if (obj->type == PROPDEF_AMMO) 
    {
        struct MultiAmmoCrateRecord* multiAmmoCrateObj;
        s32 full;
        s32 i;

        multiAmmoCrateObj = (MultiAmmoCrateRecord*)prop->obj;
        full = 1;

        if (objGetDestroyedLevel(obj) != 0)
        {
            return 0;
        }

        for (i = 0; i < AMMOTYPE_GLOBAL_MAX; i++)
        {
            s32 ammotype;

            ammotype = i + 1;

            if (i == 1) {
                ammotype = 1;
            }

            if (multiAmmoCrateObj->slots[i].quantity > 0) {
                if (ammoGetHeld(ammotype) < ammoGetMax(ammotype)) {
                    full = 0;
                    break;
                }

                if (((ammotype == AMMO_GRENADE) && (!invHasItem(ITEM_GRENADE)))
                    || ((ammotype == AMMO_REMOTEMINE) && (!invHasItem(ITEM_REMOTEMINE)))
                    || ((ammotype == AMMO_PROXMINE) && (!invHasItem(ITEM_PROXIMITYMINE)))
                    || ((ammotype == AMMO_TIMEDMINE) && (!invHasItem(ITEM_TIMEDMINE)))
                    || ((ammotype == AMMO_KNIFE) && (!invHasItem(ITEM_THROWKNIFE))))
                {
                    full = 0;
                    break;
                }
            }
        }

        if (full) 
        {
            return TICKOP_NONE;
        }
    } 
    else if (obj->type == PROPDEF_ARMOUR) 
    {
        struct BodyArmourRecord* armorObj;
        s32 ignore;
        s32 scenario;

        armorObj = (BodyArmourRecord*)prop->obj;
        ignore = 0;

        if (armorObj->amount <= currentPlayerGetArmor()) 
        {
            ignore = 1;
        } 
        else if (isMpGame()) 
        {
            scenario = frontGetMpScenario();

            if ((scenario == SCENARIO_TLD) && (invHasFlagToken() != 0)) 
            {
                ignore = 1;
            } 
            else if ((scenario == SCENARIO_MWTGG) && (invHasGoldenGun() != 0)) 
            {
                ignore = 1;
            }
        }

        if (ignore != 0) 
        {
            return TICKOP_NONE;
        }
    }

    if ((bondviewGetPlayerPitchRadians() < -0.7853982f) && (g_CurrentPlayer->magnetattracttime < 0))
    {
        return TICKOP_NONE;
    }

    {
        f32 xdiff;
        f32 ydiff;
        f32 zdiff;
#if defined(VERSION_US)
#endif
        struct PropRecord* playerprop;
#if defined(VERSION_EU)
#endif
        s32 pickup;

        playerprop = currentPlayerGetProp();

        xdiff = obj->runtime_pos.x - playerprop->pos.x;
        ydiff = obj->runtime_pos.y - playerprop->pos.y;
        zdiff = obj->runtime_pos.z - playerprop->pos.z;

        if (g_CurrentPlayer->magnetattracttime >= 0x3C) 
        {
            pickup = (((xdiff * xdiff) + (zdiff * zdiff)) <= 122500.0f)
                && (ydiff >= -500.0f)
                && (ydiff <= 500.0f);
        } 
        else 
        {
            pickup = (((xdiff * xdiff) + (zdiff * zdiff)) <= 10000.0f)
                && (ydiff >= -200.0f)
                && (ydiff <= 200.0f);
        }

        if ((pickup != 0) && !(obj->flags2 & 0x1000)) 
        {
            struct StandTile* stan = playerprop->stan;

            if ((stanTestLineUnobstructed(&stan, playerprop->pos.x, playerprop->pos.z, prop->pos.x, prop->pos.z, 2, 30.0f, 30.0f, 0.0f, 1.0f) == 0) || (stan != prop->stan))
            {
                pickup = 0;
            }
        }

        if (pickup != 0) 
        {
            return propPickupByPlayer(prop, TRUE);
        }

        return TICKOP_NONE;
    }
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


PropRecord *hatApplyToChr(HatRecord *hat, ChrRecord *chr, ModelFileHeader *filedata, PropRecord *prop, Model *model)
{
    prop = objInit((ObjectRecord*)hat, filedata, prop, model);

    if (prop && hat->model)
    {
        f32 scale = hat->extrascale * (1.0f / 256.0f);

        modelSetScale(hat->model, hat->model->scale * scale);
        hat->model->attachedto = chr->model;
        hat->model->attachedto_objinst = chr->model->obj->Switches[6];

        propReparent(prop, chr->prop);
        chr->handle_positiondata_hat = prop;
    }

    return prop;
}


void hatLoadAndApplyToChr(HatRecord *hat, ChrRecord *chr)
{
    s32 obj_idx;
    obj_idx = (u32) hat->obj;
    modelLoad(obj_idx);
    hatApplyToChr(hat, chr, PitemZ_entries[obj_idx].header, NULL, 0);
}


void hatAssignToChr(HatRecord* hat, ChrRecord* chr)
{
    hat->damage = (*(s32*)&hat->damage / M_U16_MAX_VALUE_F);
    hatLoadAndApplyToChr(hat, chr);
}


PropRecord *hatCreateForChr(ChrRecord *chr, s32 modelnum, u32 flags)
{
    ModelFileHeader *modeldef;
    PropRecord *prop;
    Model *model;
    HatRecord *hat;

    modeldef = PitemZ_entries[modelnum].header;

    modelLoad(modelnum);
    prop = propAllocate();
    model = modelmgrInstantiateModel(modeldef);
    hat = hatCreate(prop == NULL, model == NULL, modeldef);

    if (prop == NULL)
    {
        prop = propAllocate();
    }

    if (model == NULL)
    {
        model = modelmgrInstantiateModel(modeldef);
    }

    if (hat && prop && model)
    {
        HatRecord tmp = {
            .extrascale = 0x0100,
            .type       = PROPDEF_HAT,
            .flags      = 0x00004000,
            .mtx        = {.m = {{1.0f, 0.0f, 0.0f, 0.0f},
                                 {0.0f, 1.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, 1.0f, 0.0f},
                                 {0.0f, 0.0f, 0.0f, 1.0f}}},
            .damage     = 1000.0f,
            .shadecol   = {{0xFF, 0xFF, 0xFF, 0x00}},
            .nextcol    = {{0xFF, 0xFF, 0xFF, 0x00}},
        };

        *hat = tmp;

        hat->obj = modelnum;
        hat->flags = flags | PROPFLAG_ASSIGNEDTOCHR;
        hat->pad = chr->chrnum;

        prop = hatApplyToChr(hat, chr, modeldef, prop, model);
    }
    else
    {
        if (model)
        {
            modelFreeSlot(model);
        }

        if (prop)
        {
            propFree(prop);
            prop = NULL;
        }
    }

    return prop;
}


// PD: weaponCreate
WeaponObjRecord* weaponCreate(bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef)
{
    s32 i;
    WeaponObjRecord *tmp;
    WeaponObjRecord *offscreenobj = NULL;
    WeaponObjRecord *anyobj = NULL;
    s32 freeslot = -1;
    s32 offscreenslot = -1;
    s32 anyslot = -1;

    for (i = g_NextWeaponSlot; TRUE; )
    {

        if (g_WeaponSlots[i].prop == NULL)
        {
            if (!musthaveprop && !musthavemodel)
            {
                freeslot = i;
                break;
            }
        }  else if ((g_WeaponSlots[i].runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) == 0 && (g_WeaponSlots[i].state & PROPSTATE_RESPAWN) == 0)
        {
            WeaponObjRecord* slot = &g_WeaponSlots[i];
            if (((slot->timer <= 0) && (slot->prop->parent == NULL))
                    || (((slot->weaponnum == ITEM_REMOTEMINE) || (slot->weaponnum == ITEM_PROXIMITYMINE) || (slot->weaponnum == ITEM_TIMEDMINE))
                            && ((slot->prop->parent == NULL) || (slot->prop->parent->type == PROP_TYPE_OBJ) || (slot->prop->parent->type == PROP_TYPE_DOOR) || (slot->prop->parent->type == PROP_TYPE_WEAPON))))
            {
                if (!musthavemodel || modelmgrCanSlotFitRwdata(slot->model, modeldef))
                {
                    if ((slot->prop->flags & PROPFLAG_ONSCREEN) == 0 && offscreenslot < 0)
                    {
                        offscreenslot = i;
                    }

                    if (anyslot < 0)
                    {
                        anyslot = i;
                    }
                }
            }
        }

        i = (i + 1) % MAX_WEAPON_SLOTS;

        if (i == g_NextWeaponSlot)
        {
            break;
        }
    }

    if (freeslot >= 0)
    {
        g_NextWeaponSlot = (freeslot + 1) % MAX_WEAPON_SLOTS;
        return &g_WeaponSlots[freeslot];
    }

    tmp = (WeaponObjRecord *)setupFindObjForReuse(PROPDEF_COLLECTABLE, (ObjectRecord **)&offscreenobj, (ObjectRecord **)&anyobj, musthaveprop, musthavemodel, modeldef);

    if (tmp)
    {
        return tmp;
    }

    if (offscreenslot >= 0)
    {
        if (g_WeaponSlots[offscreenslot].prop)
        {
            objFreePermanently((ObjectRecord *)&g_WeaponSlots[offscreenslot], TRUE);
        }

        g_NextWeaponSlot = (offscreenslot + 1) % MAX_WEAPON_SLOTS;
        return (g_WeaponSlots + offscreenslot);
    }

    if (offscreenobj)
    {
        if (offscreenobj->prop)
        {
            objFreePermanently((ObjectRecord *)offscreenobj, TRUE);
        }

        return offscreenobj;
    }

    if (anyslot >= 0)
    {
        if (g_WeaponSlots[anyslot].prop)
        {
            objFreePermanently((ObjectRecord *)&g_WeaponSlots[anyslot], TRUE);
        }

        g_NextWeaponSlot = (anyslot + 1) % MAX_WEAPON_SLOTS;
        return (g_WeaponSlots + anyslot);
    }

    if (anyobj)
    {
        if (anyobj->prop)
        {
            objFreePermanently((ObjectRecord *)anyobj, TRUE);
        }

        return anyobj;
    }

    return NULL;
}


HatRecord *hatCreate(bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef)
{
	s32 i;
	HatRecord *tmp;
	HatRecord *offscreenobj = NULL;
	HatRecord *anyobj = NULL;
	s32 freeslot = -1;
	s32 offscreenslot = -1;
	s32 anyslot = -1;

	for (i = g_NextHatSlot; TRUE; )
	{
		if (g_HatSlots[i].prop == NULL)
		{
			if (!musthaveprop && !musthavemodel)
			{
				freeslot = i;
				break;
			}
		}
		else if ((g_HatSlots[i].runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) == 0
				&& g_HatSlots[i].prop->parent == NULL
				&& (!musthavemodel || modelmgrCanSlotFitRwdata(g_HatSlots[i].model, modeldef)))
		{
			if ((g_HatSlots[i].prop->flags & PROPFLAG_ONSCREEN) == 0 && offscreenslot < 0)
			{
				offscreenslot = i;
			}

			if (anyslot < 0)
			{
				anyslot = i;
			}
		}

		i = (i + 1) % MAX_HAT_SLOTS;

		if (i == g_NextHatSlot)
		{
			break;
		}
	}

	if (freeslot >= 0)
	{
		g_NextHatSlot = (freeslot + 1) % MAX_HAT_SLOTS;
		return (g_HatSlots + freeslot);
	}

	tmp = (HatRecord *)setupFindObjForReuse(PROPDEF_HAT, (ObjectRecord **)&offscreenobj, (ObjectRecord **)&anyobj, musthaveprop, musthavemodel, modeldef);

	if (tmp)
	{
		return tmp;
	}

	if (offscreenslot >= 0)
	{
		if (g_HatSlots[offscreenslot].prop)
		{
			objFreePermanently((ObjectRecord*)&g_HatSlots[offscreenslot], TRUE);
		}

		g_NextHatSlot = (offscreenslot + 1) % MAX_HAT_SLOTS;
		return (g_HatSlots + offscreenslot);
	}

	if (offscreenobj)
	{
		if (offscreenobj->prop)
		{
			objFreePermanently((ObjectRecord*)offscreenobj, TRUE);
		}

		return offscreenobj;
	}

	if (anyslot >= 0)
	{
		if (g_HatSlots[anyslot].prop)
		{
			objFreePermanently((ObjectRecord*)&g_HatSlots[anyslot], TRUE);
		}

		g_NextHatSlot = (anyslot + 1) % MAX_HAT_SLOTS;
		return (g_HatSlots + anyslot);
	}

	if (anyobj)
	{
		if (anyobj->prop)
		{
			objFreePermanently((ObjectRecord*)anyobj, TRUE);
		}

		return anyobj;
	}

	return NULL;
}


AmmoCrateRecord *ammocrateAllocate(void)
{
    s32 i;

    // Try to find a free one
    for (i = 0; i < MAX_AMMO_CRATES; i++)
    {
        if (g_AmmoCrates[i].prop == NULL)
        {
            return (g_AmmoCrates + i);
        }
    }

    // Find one that can be freed off-screen
    for (i = 0; i < MAX_AMMO_CRATES; i++)
    {
        if ((g_AmmoCrates[i].runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) == 0
                && (g_AmmoCrates[i].state & PROPSTATE_RESPAWN) == 0
                && g_AmmoCrates[i].prop->parent == NULL
                && (g_AmmoCrates[i].prop->flags & 0x02) == 0)
        {
            objFreePermanently(&g_AmmoCrates[i], TRUE);
            return (g_AmmoCrates + i);
        }
    }

    // Find one that can be freed on-screen
    for (i = 0; i < MAX_AMMO_CRATES; i++)
    {
        if ((g_AmmoCrates[i].runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) == 0
                && (g_AmmoCrates[i].state & PROPSTATE_RESPAWN) == 0
                && g_AmmoCrates[i].prop->parent == NULL)
        {
            objFreePermanently(&g_AmmoCrates[i], TRUE);
            return (g_AmmoCrates + i);
        }
    }

    return NULL;
}


void remoteMinesDetonate(void)
{
    u32 uVar1 = 1 << (playerGetCurrentNum());
    g_RemoteMineOwnerTriggerFlag = uVar1 | g_RemoteMineOwnerTriggerFlag;
    sndPlaySfx(g_musicSfxBufferPtr, WATCH_DETONATE_MINE_SFX, NULL);
}


/**
 * Find a weapon prop of the given weapon number: the prop itself or any of its children.
 */
WeaponObjRecord *weaponFindInProp(s32 weaponnum, PropRecord *prop)
{
    WeaponObjRecord *weapon;
    PropRecord *p;

    if (prop->type == PROP_TYPE_WEAPON)
    {
        weapon = prop->weapon;
        if (weaponnum == weapon->weaponnum)
        {
            return weapon;
        }
    }

    for (p = prop->child; p; p = p->prev)
    {
        weapon = weaponFindInProp(weaponnum, p);
        if (weapon)
        {
            return weapon;
        }
    }
    return NULL;
}


/**
 * Find a weapon of the given weapon number lying in the level (not in flight as a
 * projectile). The AI command "if key dropped" uses this for keys and other items.
 */
WeaponObjRecord *weaponFindThrown(s32 weaponnum)
{
    WeaponObjRecord *weapon;
    PropRecord *prop;

    for (prop = propsGetActiveTail(); prop; prop = prop->prev)
    {
        weapon = weaponFindInProp(weaponnum, prop);
        if (weapon && (!(weapon->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)))
        {
            return weapon;
        }
    }

    return NULL;
}

void proxmineAdd(WeaponObjRecord* proxy)
{
    s32 i = 0;

    while (1) {
        if (proxy_mine_table[i] == NULL)
        {
            proxy_mine_table[i] = proxy;

            return;
        }
        i++;
        if (i == 30)
        {
            return;
        }
    }
}


void proxmineRemove(WeaponObjRecord* proxy)
{
    s32 i = 0;

    while (1)
    {
        if (proxy_mine_table[i] == proxy)
        {
            proxy_mine_table[i] = NULL;
            return;
        }
        i++;
        if (i == 30)
        {
            return;
        }
    }
}

void proxminesDetonateNear(coord3d* pos)
{
    s32 i;
    for (i = 0; i < 30; i++)
    {
        WeaponObjRecord* obj = proxy_mine_table[i];

        if (obj && (obj->timer == 1))
        {
            f32 diff_x;
            f32 diff_z;
            f32 diff_y;
            f32 dist_sqr;
            diff_x = pos->x - obj->runtime_pos.x;
            diff_y = pos->y - obj->runtime_pos.y;
            diff_z = pos->z - obj->runtime_pos.z;
            dist_sqr = (diff_x * diff_x) + (diff_y * diff_y) + (diff_z * diff_z);

            if (dist_sqr < PROXIMITY_MINE_TRIGGER_DISTANCE)
            {
                obj->timer = 0;
            }
        }
    }
}


void proxminesTickGuards(void)
{
    ChrRecord* guard;
    s32 numslots;
    s32 i;

    numslots = chrsGetNumSlots();

    for (i = 0; i < numslots; i++)
    {
        guard = &g_ChrSlots[i];
        if ((guard->model != NULL) && (guard->hidden & CHRHIDDEN_BACKGROUND_AI))
        {
            coord3d pos;
            chrGetMovementTarget(guard, &pos);
            proxminesDetonateNear(&pos);
        }
    }
}


void propweaponSetDual(WeaponObjRecord *leftweapon, WeaponObjRecord *rightweapon) //#MATCH
{
    leftweapon->LinkedWeaponType  = rightweapon->weaponnum;
    leftweapon->dualweapon        = rightweapon;
    rightweapon->LinkedWeaponType = leftweapon->weaponnum;
    rightweapon->dualweapon       = leftweapon;
}


PropRecord* weaponInit(WeaponObjRecord* obj, ModelFileHeader* model_header, PropRecord* prop, Model* model)
{
    prop = objInit((ObjectRecord*)obj, model_header, prop, model);
    if (prop != NULL)
    {
        prop->type = 4;
        weaponSetGunfireVisible(prop, 0);
    }

    return prop;
}


PropRecord* weaponInitWithModelDef(struct ObjectRecord* obj, ModelFileHeader* modeldef)
{
    PropRecord* prop;

    prop = objInitWithModelDef(obj, modeldef);
    if (prop != NULL)
    {
        prop->type = PROP_TYPE_WEAPON;
        weaponSetGunfireVisible(prop, 0);
    }
    return prop;
}


bool chrEquipWeapon(WeaponObjRecord *wep, ChrRecord *chr)
{
    WeaponObjRecord *wep2;
    GUNHAND hand = wep->flags & PROPFLAG_WEAPON_LEFTHANDED;

    if (wep->flags & PROPFLAG_WEAPON_LEFTHANDED)
    {
        hand = GUNLEFT;
    }
    else
    {
        hand = GUNRIGHT;
    }

    wep2 = wep;
    if (wep2->prop && wep2->model)
    {
        if (!(wep2->flags & PROPFLAG_CONCEAL_GUN))
        {
            if (!chr->weapons_held[hand])
            {
                wep2->model->attachedto = chr->model;

                if (hand == GUNRIGHT)
                {
                    wep2->model->attachedto_objinst = chr->model->obj->Switches[3];
                }
                else
                {
                    wep2->model->attachedto_objinst = chr->model->obj->Switches[5];
                }

                chr->weapons_held[hand] = wep2->prop;

                if (wep2->flags & PROPFLAG_IS_DOUBLE && chr->weapons_held[1 - hand])
                {
                    propweaponSetDual(wep2, chr->weapons_held[1 - hand]->weapon);
                }
            }
            else
            {
                return FALSE;
            }
        }
        propReparent(wep2->prop, chr->prop);
    }
    return TRUE;
}


PropRecord *chrInitHeldWeapon(WeaponObjRecord *weapon, ChrRecord *chr, ModelFileHeader *modeldef, PropRecord *prop, Model *model)
{
	prop = weaponInit(weapon, modeldef, prop, model);

	if (prop && weapon->model)
	{
		f32 scale = weapon->extrascale * (1.0f / 256.0f);

		modelSetScale(weapon->model, weapon->model->scale * scale);
        chrEquipWeapon(weapon, chr);
	}

	return prop;
}


void chrLoadHeldWeapon(WeaponObjRecord *weapon, ChrRecord *chr)
{
	s32 modelnum = weapon->obj;

	modelLoad(modelnum);
	chrInitHeldWeapon(weapon, chr, PitemZ_entries[modelnum].header, 0, 0);
}


void chrSetupHeldWeapon(WeaponObjRecord* weapon, ChrRecord* chr)
{
    weapon->damage = (*(s32*)&weapon->damage) / M_U16_MAX_VALUE_F;
    chrLoadHeldWeapon(weapon, chr);
}


WeaponObjRecord blank_08_object_preset_1 = {
    {{0x0100, //extrascale
    0x0, //state
    0x08}, //type
    0, //obj
    1, //pad
    0x00000000, //flags
    0, //flags2
    NULL, // prop
    NULL, // model
    {
        {{1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}}
    }, //mtx
    { {0.0, 0.0, 0.0} }, //runtime_pos
    {0x00000000 }, //runtime_bitflags
    NULL, //ptr_allocated_collisiondata_block
    {NULL}, //projectile/embedment
    0.0f, //maxdamage
    1000.0f,//damage
    { {0xFF, 0xFF, 0xFF, 0x00} }, // shadecol
    { {0xFF, 0xFF, 0xFF, 0x00} }}, // nextcol
    ITEM_UNARMED, //weaponnum
    -1, //LinkedWeaponType
    -1, //timer
    NULL //dualweapon
};


/**
 * @param modelnum: index into PitemZ_entries, which is enum PROP
 * @param weaponid: object_weapon.gun_pickup value
 */
WeaponObjRecord *weaponCreateFromModel(PROP modelnum, s32 weaponid)
{
    ModelFileHeader *modeldef;
    PropRecord *prop;
    Model *model;
    WeaponObjRecord *obj;

    modeldef = PitemZ_entries[modelnum].header;

    modelLoad(modelnum);

    prop = propAllocate();

    model = modelmgrInstantiateModel(modeldef);

    obj = weaponCreate(prop == NULL, model == NULL, modeldef);

    if (prop == NULL)
    {
        prop = propAllocate();
    }

    if (model == NULL)
    {
        model = modelmgrInstantiateModel(modeldef);
    }

    if (obj != NULL && prop != NULL && model != NULL)
    {
        WeaponObjRecord tmp = blank_08_object_preset_1;

        *obj = tmp;

        obj->weaponnum = weaponid;
        obj->obj = modelnum;

        weaponInit(obj, modeldef, prop, model);
    }
    else
    {
        obj = NULL;

        if (model != NULL)
        {
            modelFreeSlot(model);
        }

        if (prop != NULL)
        {
            propFree(prop);
        }
    }

    return obj;
}


/**
 * Set removed flag on hand
 */
void chrRemoveHeldWeapon(ChrRecord *chr, GUNHAND hand) //#MATCH
{
    if (chr->weapons_held[hand])
    {
        chr->weapons_held[hand]->weapon->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
    }
}

WeaponObjRecord blank_08_object_preset_4001 = {
    {{0x0100, //extrascale
    0x0, //state
    0x08}, //type
    0, //obj
    0x4001, //pad
    0x00000000, //flags
    0, //flags2
    NULL, //prop
    NULL, //model
    {
       {{1.0f, 0.0f, 0.0f, 0.0f},
       {0.0f, 1.0f, 0.0f, 0.0f},
       {0.0f, 0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 0.0f, 1.0f}}
    }, //mtx
    {{0.0, 0.0, 0.0}},//runtime_pos
    {0x00000000}, //runtime_bitflags
    NULL, //ptr_allocated_collisiondata_block
    {NULL}, //projectile/embedment
    0.0f, //maxdamage
    1000.0f, //damage
    { {0xFF, 0xFF, 0xFF, 0x00} }, // shadecol
    { {0xFF, 0xFF, 0xFF, 0x00} }}, // nextcol
    ITEM_UNARMED, //weaponnu
    -1, //LinkedWeaponType
    -1, //timer
    NULL //dualweapon
};

/**
*/
PropRecord *chrGiveWeaponEx(ChrRecord *self, s32 propid, ITEM_IDS itemid, s32 flags, WeaponObjRecord *weapon, ModelFileHeader *modeldef)
{
    Model *objinst;
    PropRecord *lastobjentry;

    if (!modeldef)
    {
        modeldef = PitemZ_entries[propid].header;
        modelLoad(propid);
    }

    lastobjentry = propAllocate();
    objinst = modelmgrInstantiateModel(modeldef);

    if (!weapon)
    {
        weapon = weaponCreate(lastobjentry == NULL, objinst == NULL, modeldef);
    }

    if (!lastobjentry)
    {
        lastobjentry = propAllocate();
    }

    if (!objinst)
    {
        objinst = modelmgrInstantiateModel(modeldef);
    }

    if (weapon && lastobjentry && objinst)
    {
        WeaponObjRecord new_weapon = blank_08_object_preset_4001;
        *weapon = new_weapon;

        weapon->weaponnum = itemid;
        weapon->obj = propid;
        weapon->flags = flags | 0x4000;

        // pad = chrnum ???
        weapon->pad = self->chrnum;

        lastobjentry = chrInitHeldWeapon(weapon, self, modeldef, lastobjentry, objinst);
    }
    else
    {
        if (objinst)
        {
            modelFreeSlot(objinst);
        }

        if (lastobjentry)
        {
            propFree(lastobjentry);
            lastobjentry = NULL;
        }
    }

    return lastobjentry; //should be new weapon
}


/**
 * Add New Weapon to chr
 */
PropRecord *chrGiveWeapon(ChrRecord *self, s32 PropID, ITEM_IDS ItemID, s32 flags) //#MATCH
{
    return chrGiveWeaponEx(self, PropID, ItemID, flags, NULL, NULL);
}


/**
 * Render the weapon(s) characters are holding including the muzzle flash.
 */
void chrRenderHeldWeapon(PropRecord *chrprop, GUNHAND hand, JointListEntry **jointList)
{
    ChrRecord *chr;
    PropRecord *prop;
    ObjectRecord *weaponObj;
    Model *heldModel;
    ModelRenderData renderData;
    Model *chrModel;
    Mtxf rotationMtx;

    chr = chrprop->chr;
    prop = chrGetEquippedWeaponProp(chr, hand);

    if (prop != NULL) {

        weaponObj = prop->obj;

        if (!(weaponObj->runtime_bitflags & RUNTIMEBITFLAG_00000800))
        {
            if ((s32)(weaponObj->flags2 << 12) >= 0) 
            {
                heldModel = weaponObj->model;
                renderData = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};

                chrModel = chr->model;
                prop->flags |= PROPFLAG_ONSCREEN;

                renderData.basemtx = modelFindNodeMtx(chrModel, heldModel->attachedto_objinst, 0);

                if (hand == GUNLEFT) 
                {
                    mtx4LoadZRotation(3.1415927f, &rotationMtx);
                    mtx4MultInPlace(renderData.basemtx, &rotationMtx);
                    renderData.basemtx = &rotationMtx;
                }

                renderData.mtxlist = dynAllocate(heldModel->obj->numMatrices * sizeof(Mtxf));
                modelCalcMatricesNoAnim(&renderData, heldModel);

                if (jointList != NULL)
                {
                    if (!(weaponObj->runtime_bitflags & RUNTIMEBITFLAG_00000080)) 
                    {
                        *jointList = jointListAddModel(*jointList, heldModel);
                    }
                }

                return;
            }
        }

        prop->flags &= ~PROPFLAG_ONSCREEN;
    }
}


TICKOP weaponTickPlayer(struct PropRecord* prop)
{
    return objTickPlayer(prop);
}


void weaponSetGunfireVisible(PropRecord *prop, s32 firing)
{
    ObjectRecord *obj = prop->obj;
    Model *model = obj->model;
    ModelNode *node;

    if (model && model->obj->Skeleton == &skeleton_prop_weapon) {
        node = model->obj->Switches[0];
        if (node) {
            struct ModelRwData_GunfireRecord *rwdata = &modelGetNodeRwData(model, node)->Gunfire;
            rwdata->visible = firing;
        }

        node = model->obj->Switches[2];
        if (node) {
            struct ModelRwData_BSPRecord *rwdata = &modelGetNodeRwData(model, node)->BSP;
            rwdata->visible = firing;
        }
    }
}


/*
* Alternative name: getHatType
*/
HATTYPE hatGetModel(PropRecord *prop) //#MATCH
{
    ObjectRecord *objinst = prop->obj;
    switch (objinst->obj)
    {
        case PROP_HATFURRY:
        case PROP_HATFURRYBROWN:
        case PROP_HATFURRYBLACK:
        {
            return HATTYPE_FURRY;
        }

        case PROP_HATTBIRD:
        case PROP_HATTBIRDBROWN:
        {
            return HATTYPE_BIRD;
        }

        case PROP_HATHELMET:
        case PROP_HATHELMETGREY:
        {
            return HATTYPE_HELMATE;
        }

        case PROP_HATMOON:

        {
            return HATTYPE_MOON;
        }
        case PROP_HATBERET:
        case PROP_HATBERETBLUE:
        case PROP_HATBERETRED:
        {
            return HATTYPE_BERRET;
        }

        case PROP_HATPEAKED:
        {
            return HATTYPE_PEAKED;
        }

        default:
        {
            return HATTYPE_OTHER;
        }
    }
}


/**
 * US address 7F0526EC.
*/
void doorGetMtx(DoorRecord *door, Mtxf *rhs)
{
    Mtxf lhs;
    coord3d pivot;
    coord3d offset;
    BoundPadRecord* pad;
    coord3d side;
    coord3d pos;

    if ((door->doorType == DOORTYPE_SWINGING) || (door->doorType == DOORTYPE_AZTECCHAIR))
    {
        pad = &g_CurrentSetup.boundpads[door->pad];

        side.f[0] = (pad->up.f[1] * pad->look.f[2]) - (pad->up.f[2] * pad->look.f[1]); // cross product
        side.f[1] = (pad->up.f[2] * pad->look.f[0]) - (pad->up.f[0] * pad->look.f[2]); // cross product
        side.f[2] = (pad->up.f[0] * pad->look.f[1]) - (pad->up.f[1] * pad->look.f[0]); // cross product

        pivot.f[0] = pad->pos.f[0] + (pad->up.f[0] * pad->bbox.ymin);
        pivot.f[1] = pad->pos.f[1] + (pad->up.f[1] * pad->bbox.ymin);
        pivot.f[2] = pad->pos.f[2] + (pad->up.f[2] * pad->bbox.ymin);

        if (door->doorType == DOORTYPE_AZTECCHAIR)
        {
            pivot.f[0] += side.f[0] * pad->bbox.xmax;
            pivot.f[1] += side.f[1] * pad->bbox.xmax;
            pivot.f[2] += side.f[2] * pad->bbox.xmax;
        }
        else if (door->flags & PROPFLAG_DOOR_OPENTOFRONT)
        {
            pivot.f[0] += side.f[0] * pad->bbox.xmax;
            pivot.f[1] += side.f[1] * pad->bbox.xmax;
            pivot.f[2] += side.f[2] * pad->bbox.xmax;
        }
        else
        {
            pivot.f[0] += side.f[0] * pad->bbox.xmin;
            pivot.f[1] += side.f[1] * pad->bbox.xmin;
            pivot.f[2] += side.f[2] * pad->bbox.xmin;
        }

        offset.f[0] = door->runtime_pos.f[0] - pivot.f[0];
        offset.f[1] = door->runtime_pos.f[1] - pivot.f[1];
        offset.f[2] = door->runtime_pos.f[2] - pivot.f[2];

        mtx4Copy(&door->mtx, rhs);
        mtx4LoadTranslation(&offset, &lhs);
        mtx4MultInPlace(&lhs, rhs);

        if (door->doorType == DOORTYPE_AZTECCHAIR)
        {
            if (door->flags & PROPFLAG_DOOR_OPENTOFRONT)
            {
                mtx4LoadZRotation(M_TAU_F - ((door->openPosition * M_TAU_F) / 360.0f), &lhs);
            }
            else
            {
                mtx4LoadZRotation((door->openPosition * M_TAU_F) / 360.0f, &lhs);
            }
        }
        else if (door->flags & PROPFLAG_DOOR_OPENTOFRONT)
        {
            mtx4LoadYRotation(M_TAU_F - ((door->openPosition * M_TAU_F) / 360.0f), &lhs);
        }
        else
        {
            mtx4LoadYRotation((door->openPosition * M_TAU_F) / 360.0f, &lhs);
        }

        mtx4MultInPlace(&lhs, rhs);
        mtx4LoadTranslation(&pivot, &lhs);
        mtx4MultInPlace(&lhs, rhs);
    }
    else if ((door->doorType == DOORTYPE_EYE) || (door->doorType == DOORTYPE_IRIS))
    {
        mtx4Copy(&door->mtx, rhs);
        mtx4SetTranslation(&door->runtime_pos, rhs);
    }
    else
    {
        pos.f[0] = (door->slideDir.x * door->openPosition) + door->runtime_pos.x;
        pos.f[1] = (door->slideDir.y * door->openPosition) + door->runtime_pos.y;
        pos.f[2] = (door->slideDir.z * door->openPosition) + door->runtime_pos.z;

        mtx4Copy(&door->mtx, rhs);
        mtx4SetTranslation(&pos, rhs);
    }

    if (door->doorFlags & DOORFLAG_FLIP)
    {
        mtx4ScaleRow2(-1.0f, rhs);
    }
}


/**
*/
void doorUpdateBbox(DoorRecord *door)
{
    struct ModelRoData_BoundingBoxRecord *door_bb;
    Mtxf mtx;

    door_bb = (struct ModelRoData_BoundingBoxRecord *)door->model->obj->RootNode->Child->Data;

    // struct copy
    door->bbox = *door_bb;

    // Resize the door's bounding box.
    if (door->doorFlags & DOORFLAG_CLIP_TO_BBOX)
    {
        if (door->doorType == DOORTYPE_VERTICAL)
        {
            door->bbox.Bounds.ymax = door_bb->Bounds.ymax + (door_bb->Bounds.ymin - door_bb->Bounds.ymax) * door->openPosition;
        }
        else
        {
            door->bbox.Bounds.xmin = door_bb->Bounds.xmin + (door_bb->Bounds.xmax - door_bb->Bounds.xmin) * door->openPosition;
        }
    }

    if (door->perimFrac <= door->openPosition)
    {
        door->ptr_allocated_collisiondata_block->edges = 0;

        return;
    }

    doorGetMtx(door, &mtx);
    door->ptr_allocated_collisiondata_block->edges = bboxGetHull2d(&door->bbox, &mtx, door->ptr_allocated_collisiondata_block->polygon);

    if (door->doorType == DOORTYPE_VERTICAL)
    {
        door->ptr_allocated_collisiondata_block->bottom = door->runtime_pos.f[1] + bboxRotatedMinY(&door->bbox, &mtx);
    }
    else if (door->doorType == DOORTYPE_FALLAWAY)
    {
        door->ptr_allocated_collisiondata_block->bottom = door->runtime_pos.f[1] - 10000.0f;
    }
    else
    {
        door->ptr_allocated_collisiondata_block->bottom = mtx.m[3][1] + bboxRotatedMinY(&door->bbox, &mtx);

        if (door->doorFlags & DOORFLAG_EXTENDEDY)
        {
            door->ptr_allocated_collisiondata_block->bottom -= 1000.0f;
        }
    }

    if (((door->doorType == DOORTYPE_EYE) && (0 < door->openPosition - (0.4f * door->maxFrac)))
        || ((door->doorType == DOORTYPE_IRIS) && (0 < door->openPosition - (0.4f * door->maxFrac)))
        )
    {

        door->ptr_allocated_collisiondata_block->top = door->ptr_allocated_collisiondata_block->bottom + 50.0f;
    }
    else if (door->doorType == DOORTYPE_FALLAWAY)
    {
        door->ptr_allocated_collisiondata_block->top = door->runtime_pos.f[1] + 1000.0f;
    }
    else
    {
        door->ptr_allocated_collisiondata_block->top = mtx.m[3][1] + bboxRotatedMaxY(&door->bbox, &mtx);

        if (door->doorFlags & DOORFLAG_EXTENDEDY)
        {
            door->ptr_allocated_collisiondata_block->top += 1000.0f;
        }
    }

}


/**
 * This function physically resizes sliding doors with DOORFLAG_CLIP_TO_BBOX as they move.
 * When the door is opening, it shrinks. When the door is closing, it expands.
 * This prevents z-fighting if the door's face is parallel with the suface it's sliding into.
 * Examples include the brown security doors in Facility and the sliding stone doors in Egyptian and Temple.
 */
void doorBuildClippedVertices(DoorRecord *inDoor)
{
    Model *mdl;
    ModelNode *mdlDLCNode;
    struct ModelRoData_DisplayList_CollisionRecord *src;
    DoorRecord *door;
    s16 cutoff;
    s32 i;
    s32 j;
    Vertex *cur;
    Vertex *n1;
    Vertex *n2;
    Vertex *n3;
    Vertex *dcur;
    Vertex *d1;
    Vertex *d2;
    Vertex *d3;

    if (inDoor->doorFlags & DOORFLAG_CLIP_TO_BBOX)
    {
        door = inDoor;
        mdl = inDoor->model;
        mdlDLCNode = mdl->obj->RootNode->Child->Child;
        src = (struct ModelRoData_DisplayList_CollisionRecord *)mdlDLCNode->Data;
        inDoor = (DoorRecord *)modelGetNodeRwData(mdl, mdlDLCNode);

        if (door->doorType == DOORTYPE_VERTICAL)
        {
            cutoff = door->bbox.Bounds.ymax + 0.5f;
        }
        else
        {
            cutoff = door->bbox.Bounds.xmin + 0.5f;
        }

        ((struct ModelRwData_DisplayList_CollisionRecord *)inDoor)->Vertices = dynAllocateVertices(src->numVertices);

        for (i = 0; i < src->numVertices / 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                dcur = &(&((struct ModelRwData_DisplayList_CollisionRecord *)inDoor)->Vertices[i * 4])[j];
                d1 = &(&((struct ModelRwData_DisplayList_CollisionRecord *)inDoor)->Vertices[i * 4])[(j + 1) % 4];
                d2 = &(&((struct ModelRwData_DisplayList_CollisionRecord *)inDoor)->Vertices[i * 4])[(j + 2) % 4];
                d3 = &(&((struct ModelRwData_DisplayList_CollisionRecord *)inDoor)->Vertices[i * 4])[(j + 3) % 4];
                cur = &(&src->Vertices[i * 4])[j];
                n1 = &(&src->Vertices[i * 4])[(j + 1) % 4];
                n2 = &(&src->Vertices[i * 4])[(j + 2) % 4];
                n3 = &(&src->Vertices[i * 4])[(j + 3) % 4];

                if (j == 0)
                {
                    *dcur = *cur;
                    *d1 = *n1;
                    *d2 = *n2;
                    *d3 = *n3;
                }

                // Rebuild the S/T coords for vertically sliding doors to mask the door's resizing.
                if (door->doorType == DOORTYPE_VERTICAL)
                {
                    if (cur->coord.y >= cutoff)
                    {
                        if (cur->coord.x == n1->coord.x && cur->coord.z == n1->coord.z && cur->coord.y != n1->coord.y)
                        {
                            dcur->s = ((cur->coord.y - cutoff) * (n1->s - cur->s) / (cur->coord.y - n1->coord.y)) + cur->s;
                            dcur->t = ((cur->coord.y - cutoff) * (n1->t - cur->t) / (cur->coord.y - n1->coord.y)) + cur->t;
                        }
                        else if (cur->coord.x == n2->coord.x && cur->coord.z == n2->coord.z && cur->coord.y != n2->coord.y)
                        {
                            dcur->s = ((cur->coord.y - cutoff) * (n2->s - cur->s) / (cur->coord.y - n2->coord.y)) + cur->s;
                            dcur->t = ((cur->coord.y - cutoff) * (n2->t - cur->t) / (cur->coord.y - n2->coord.y)) + cur->t;
                        }
                        else if (cur->coord.x == n3->coord.x && cur->coord.z == n3->coord.z && cur->coord.y != n3->coord.y)
                        {
                            dcur->s = ((cur->coord.y - cutoff) * (n3->s - cur->s) / (cur->coord.y - n3->coord.y)) + cur->s;
                            dcur->t = ((cur->coord.y - cutoff) * (n3->t - cur->t) / (cur->coord.y - n3->coord.y)) + cur->t;
                        }

                        dcur->coord.y = cutoff;
                    }
                }
                // Rebuild the S/T coords for horizontally sliding doors.
                else
                {
                    if (cur->coord.x <= cutoff)
                    {
                        if (cur->coord.y == n1->coord.y && cur->coord.z == n1->coord.z && cur->coord.x != n1->coord.x)
                        {
                            dcur->s = ((cutoff - cur->coord.x) * (n1->s - cur->s) / (n1->coord.x - cur->coord.x)) + cur->s;
                            dcur->t = ((cutoff - cur->coord.x) * (n1->t - cur->t) / (n1->coord.x - cur->coord.x)) + cur->t;
                        }
                        else if (cur->coord.y == n2->coord.y && cur->coord.z == n2->coord.z && cur->coord.x != n2->coord.x)
                        {
                            dcur->s = ((cutoff - cur->coord.x) * (n2->s - cur->s) / (n2->coord.x - cur->coord.x)) + cur->s;
                            dcur->t = ((cutoff - cur->coord.x) * (n2->t - cur->t) / (n2->coord.x - cur->coord.x)) + cur->t;
                        }
                        else if (cur->coord.y == n3->coord.y && cur->coord.z == n3->coord.z && cur->coord.x != n3->coord.x)
                        {
                            dcur->s = ((cutoff - cur->coord.x) * (n3->s - cur->s) / (n3->coord.x - cur->coord.x)) + cur->s;
                            dcur->t = ((cutoff - cur->coord.x) * (n3->t - cur->t) / (n3->coord.x - cur->coord.x)) + cur->t;
                        }

                        dcur->coord.x = cutoff;
                    }
                }
            }
        }
    }
}


/**
 * objToggleDoorPortal / doorActivatePortal
 * Toggles (Open/Closed) the portal linked with door
 * @param door: Door to toggle portal on
 */
void doorActivatePortal(DoorRecord *door)
{
    if (door->portalNumber >= 0)
    {
        bgToggleDataPortalsContrlBytes1Bit1(door->portalNumber, TRUE);
    }
}


/**
 * objToggleDoorPortal / doorDeactivatePortal
 * Toggles (Open/Closed) the portal linked with door
 * @param door: Door to toggle portal on
 */
void doorDeactivatePortal(DoorRecord *door) {
    if (door->portalNumber >= 0)
    {
        bgToggleDataPortalsContrlBytes1Bit1(door->portalNumber, FALSE);
    }
}


PropRecord* doorInit(DoorRecord* door, coord3d* pos, Mtxf* mtx, StandTile* stan, coord3d* coord, coord3d* centre) {
    PropRecord* prop;
    f32 scale;

    prop = objInitWithAutoModel((ObjectRecord* ) door);
    scale = PitemZ_entries[door->obj].scale;
    door->ptr_allocated_collisiondata_block = mempAlloc(0x50U, MEMPOOL_STAGE);

    mtx4Copy(mtx, &door->mtx);
    mtx4ScaleRows012(scale, &door->mtx);

    door->slideDir.x = (f32) coord->x;
    door->slideDir.y = (f32) coord->y;
    door->slideDir.z = (f32) coord->z;

    if (door->flags & PROPFLAG_80000000) {
        door->openPosition = door->maxFrac;
    } else {
        door->openPosition = 0.0f;
    }

    door->speed = 0.0f;
    door->openstate = DOORSTATE_STATIONARY;
    door->numHits = 0;
    door->linkedDoor = NULL;

    if (door->doorFlags & DOORFLAG_CLIP_TO_BBOX) {
        union ModelRoData *rodata = door->model->obj->RootNode->Child->Child->Data;
        door->clipVtx = mempAlloc(rodata->DisplayListCollisions.numVertices * sizeof(Vertex), MEMPOOL_STAGE);
    } else {
        door->clipVtx = NULL;
    }

    door->portalNumber = -1;
    door->openSoundState = 0;
    door->closeSoundState = 0;

    prop->type = PROP_TYPE_DOOR;
    prop->door = door;
    prop->pos.x = pos->x;
    prop->pos.y = pos->y;
    prop->pos.z = pos->z;
    prop->stan = stan;

    door->runtime_pos.x = centre->x;
    door->runtime_pos.y = centre->y;
    door->runtime_pos.z = centre->z;
    door->flags |= PROPFLAG_00000100;

    doorUpdateBbox(door);
    doorBuildClippedVertices(door);
    doorGetTileShading(door->prop, &door->nextcol);

    door->shadecol.r = door->nextcol.r;
    door->shadecol.g = door->nextcol.g;
    door->shadecol.b = door->nextcol.b;
    door->shadecol.a = door->nextcol.a;

    return prop;
}


s32 sndVolumeForDistance(f32 distance, f32 min, f32 max)
{
    s32 retval;

    if (distance <= 200.0f)
    {
        retval = SHRT_MAX;
    }
    else if (max <= distance)
    {
        retval = 0.0f;
    }
    else if (min <= distance)
    {
        retval = ((max - distance) * 10000.0f) / (max - min);
    }
    else
    {
        retval = SHRT_MAX - (s32)((sqrtf(distance - 200.0f) * 22767.0f) / sqrtf(min - 200.0f));
    }

    return retval;
}


s32 sndVolumeForPos(coord3d *pos, f32 low, f32 high)
{
    PropRecord *prop;
    s32 index;
    f32 shortest_distance;
    f32 diffx;
    f32 diffy;
    f32 diffz;
    f32 distance;
    s32 count;

    shortest_distance = high;
    count = getPlayerCount();

    for (index = 0; index < count; index++)
    {
        prop  = g_playerPointers[index]->prop;
        diffx = prop->pos.x - pos->x;
        diffy = prop->pos.y - pos->y;
        diffz = prop->pos.z - pos->z;
        distance = sqrtf(diffx * diffx + diffy * diffy + diffz * diffz);

        if (distance < shortest_distance)
        {
            shortest_distance = distance;
        }
    }
    return sndVolumeForDistance(shortest_distance, low, high);
}


void chrobjSndCreatePostEvent(ALSoundState *state, coord3d *pos, f32 low, f32 high)
{
    sndCreatePostEvent(state, 8, sndVolumeForPos(pos, low, high));
}


s32 sndVolumeForDistanceDefault(f32 vol)
{
    return sndVolumeForDistance(vol, 5000.0f, 6000.0f);
}


s32 sndVolumeForPosDefault(coord3d *pos)
{
    return sndVolumeForPos(pos, 5000.0f, 6000.0f);
}


void chrobjSndCreatePostEventDefault(ALSoundState *state, coord3d *pos)
{
    chrobjSndCreatePostEvent(state, pos, 5000.0f,  6000.0f);
}


void doorUpdateSoundVolume(DoorRecord* door)
{
    s32 open_playing;
    s32 close_playing;
    s32 volume;

    open_playing = (door->openSoundState != NULL) && (sndGetPlayingState(door->openSoundState) != 0);
    close_playing = (door->closeSoundState != NULL) && (sndGetPlayingState(door->closeSoundState) != 0);

    if ((open_playing != 0) || (close_playing != 0))
    {

        volume = sndVolumeForPosDefault(&door->prop->pos);

        if (lvGetControlsLocked() != 0)
        {
            volume = 0;
        }

        if (open_playing != 0)
        {
            sndCreatePostEvent(door->openSoundState, 8, volume);
        }

        if (close_playing != 0)
        {
            sndCreatePostEvent(door->closeSoundState, 8, volume);
        }
    }
}


void doorStopSounds(DoorRecord *door) //#MATCH
{
    if (door->openSoundState && sndGetPlayingState(door->openSoundState))
    {
        sndDeactivate(door->openSoundState);
    }

    if (door->closeSoundState && sndGetPlayingState(door->closeSoundState))
    {
        sndDeactivate(door->closeSoundState);
    }
}


void doorPlayOpenSound0(DoorRecord *door) {
    ALSoundState *soundState = NULL;
    ALSoundState **pendingState = NULL;

    doorStopSounds(door);

    if (door->openSoundState == NULL)
    {
        pendingState = &door->openSoundState;
    }
    else if (door->closeSoundState == NULL)
    {
        pendingState = &door->closeSoundState;
    }

    switch (door->doorOpenSound)
    {
    case DOOR_OPEN_SOUND_01:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_SLIDE1_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_02:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_METAL:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_OPEN_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_LOOP_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_04:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SLIDE_OPEN_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SINGLE_LOOP_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_WOOD:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_WOOD_OPEN_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_06:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_WOOD_2:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_CATCH_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_WOOD_SLIDE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_WOOD_3:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_WOOD_OPEN_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_09:
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_SHUTTER_OPEN_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_METAL_2:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_OPEN_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_11:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL_3:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_OPEN3_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_13:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_HYDROLIC:
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_HYDRAL_CLOSE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_STONE:
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_SLIDE_STONE_OPEN_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_16:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SLIDE_OPEN_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL_4:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        if (soundState != NULL)
        {
            chrobjSndCreatePostEventDefault(soundState, &door->prop->pos);
        }
        soundState = sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_OPEN_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_LOOP_SFX, pendingState);
        }
        break;
    }

    if (soundState != NULL)
    {
        chrobjSndCreatePostEventDefault(soundState, &door->prop->pos);
    }

    doorUpdateSoundVolume(door);
}


void doorPlayOpenSound1(DoorRecord *door) {
    ALSoundState *soundState = NULL;
    ALSoundState **pendingState = NULL;

    doorStopSounds(door);

    if (door->openSoundState == NULL)
    {
        pendingState = &door->openSoundState;
    }
    else if (door->closeSoundState == NULL)
    {
        pendingState = &door->closeSoundState;
    }

    switch (door->doorOpenSound)
    {
    case DOOR_OPEN_SOUND_01:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_SLIDE1_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_02:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_METAL:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_OPEN_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_LOOP_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_04:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SLIDE_OPEN_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SINGLE_LOOP_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_WOOD_2:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_CATCH_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_WOOD_SLIDE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_WOOD_3:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_WOOD_OPEN_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_09:
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_SHUTTER_OPEN_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_13:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_HYDROLIC:
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_HYDRAL_CLOSE_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_STONE:
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_SLIDE_STONE_OPEN_SFX, pendingState);
        }
        break;
    case DOOR_OPEN_SOUND_16:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SLIDE_OPEN_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL_4:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        if (soundState != NULL)
        {
            chrobjSndCreatePostEventDefault(soundState, &door->prop->pos);
        }
        soundState = sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_OPEN_SFX, NULL);
        if (pendingState != NULL)
        {
            sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_LOOP_SFX, pendingState);
        }
        break;
    }

    if (soundState != NULL)
    {
        chrobjSndCreatePostEventDefault(soundState, &door->prop->pos);
    }

    doorUpdateSoundVolume(door);
}


void doorPlayCloseSound0(DoorRecord *door) {
    ALSoundState *soundState = NULL;

    doorStopSounds(door);

    switch (door->doorOpenSound)
    {
    case DOOR_OPEN_SOUND_01:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_02:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_04:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SLIDE_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_WOOD_2:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_WOOD_3:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_WOOD_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_09:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SHUTTER_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_13:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_HYDROLIC:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_HYDRAL_OPEN_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_STONE:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SLIDE_STONE_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_16:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SLIDE_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL_4:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_CLOSE_SFX, NULL);
        break;
    }


    if (soundState != NULL)
    {
        chrobjSndCreatePostEventDefault(soundState, &door->prop->pos);
    }

    doorUpdateSoundVolume(door);
}


void doorPlayCloseSound1(DoorRecord *door)
{
    ALSoundState *soundState = NULL;

    doorStopSounds(door);

    switch (door->doorOpenSound)
    {
    case DOOR_OPEN_SOUND_01:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_02:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_04:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SLIDE_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_WOOD:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_WOOD_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_06:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_WOOD_2:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SMART_CATCH1_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_WOOD_3:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_WOOD_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_09:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SHUTTER_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL_2:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_11:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL_3:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE3_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_13:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, TRAIN_SLIDE_DOOR_SLIDE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_HYDROLIC:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_HYDRAL_OPEN_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_STONE:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, DOOR_SLIDE_STONE_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_16:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, HEAVY_SLIDE_CLOSE_SFX, NULL);
        break;
    case DOOR_OPEN_SOUND_METAL_4:
        soundState = sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_CLOSE_SFX, NULL);
        break;
    }


    if (soundState != NULL) {
        chrobjSndCreatePostEventDefault(soundState, &door->prop->pos);
    }

    doorUpdateSoundVolume(door);
}


/**
 * Play the door open sound and activate the door's portal,
 */
void doorStartOpen(DoorRecord *door)
{
    door->flags &= ~PROPFLAG_DOOR_KEEPOPEN;
    door->runtime_bitflags |= RUNTIMEBITFLAG_BEENOPENED;

    doorPlayOpenSound0(door);
    doorActivatePortal(door);

    if (door->doorType == DOORTYPE_FALLAWAY)
    {
        struct collision_data *col = door->ptr_allocated_collisiondata_block;
        door->flags |= PROPFLAG_CANNOT_ACTIVATE;
        door->perimFrac = 0;

        if (col) { col->edges = 0; }
        door->flags &= ~PROPFLAG_00000100;
    }
}


/**
 * Play the door close sound
 */
void doorStartClose(DoorRecord *door)
{
    door->flags &= ~PROPFLAG_DOOR_KEEPOPEN;
    doorPlayOpenSound1(door);
}


void doorFinishOpen(DoorRecord *door)
{
    doorPlayCloseSound0(door);

    if (door->doorType == DOORTYPE_FALLAWAY)
    {
        objEnsureProjectile(door->prop);

        if (door->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        {
            door->projectile->flags |= 1;
            mtx4LoadIdentity(&door->projectile->mtx);
        }
    }
}


void doorFinishClose(DoorRecord* door)
{
    doorPlayCloseSound1(door);
    doorDeactivatePortal(door);
}


/**
 * Apply the given state to an individual door (not its siblings).
 *
 * Handles playing door open/close sounds and activating the portal if opening.
 */
void doorSetOpenState(DoorRecord *door, s32 newstate)
{
    if (newstate == DOORSTATE_OPENING)
    {
        if (door->openstate == DOORSTATE_STATIONARY || door->openstate == DOORSTATE_WAITING)
        {
            doorStartOpen(door);
        }

        door->openstate = newstate;
    }
    else if (newstate == DOORSTATE_CLOSING)
    {
        if (door->openstate == DOORSTATE_STATIONARY && door->openPosition > 0)
        {
            doorStartClose(door);
        }

        if ((door->openstate != DOORSTATE_STATIONARY && door->openstate != DOORSTATE_WAITING) || door->openPosition > 0)
        {
            door->openstate = newstate;
        }
        else if (door->openstate == DOORSTATE_WAITING)
        {
            door->openstate = DOORSTATE_STATIONARY;
        }
    }
    else
    {
        door->openstate = newstate;
    }
}


void doorActivate(DoorRecord *door, DOORSTATE State) //#MATCH
{
    DoorRecord *linkeddoor;
    DOORSTATE   LinkedState = State;

    if (door->flags2 & 0x40000000) //Close first door before opening second
    {
        if (State == DOORSTATE_OPENING)
        {
            LinkedState = DOORSTATE_CLOSING;
            if (door->openstate == DOORSTATE_STATIONARY)
            {
                State = DOORSTATE_WAITING;
            }
        }
    }

    doorSetOpenState(door, State);

    linkeddoor = door->linkedDoor;

    while (linkeddoor && linkeddoor != door)
    {
        doorSetOpenState(linkeddoor, LinkedState);
        linkeddoor = linkeddoor->linkedDoor;
    };
}


bool doorIsClosed(DoorRecord *door)
{
    return ((door->openstate == DOORSTATE_STATIONARY) || (door->openstate == DOORSTATE_WAITING)) && (door->openPosition <= 0.0f);
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


/**
* Loaded to 7F054EA8.
*/
s32 doorUpdateDisplacement(DoorRecord* door)
{
    int isMoving = 0;

    if (door->openstate == DOORSTATE_OPENING)
    {
        chrobjApplySpeed(&door->openPosition, door->maxFrac, &door->speed, door->accel, door->decel, door->maxSpeed);

        if (door->maxFrac <= door->openPosition)
        {
            door->openPosition = door->maxFrac;
        }
        else
        {
            if (door->openPosition <= 0.0f)
            {
                door->openPosition = 0.0f;
            }
        }

        isMoving = 1;
    }
    else if (door->openstate == DOORSTATE_CLOSING)
    {
        chrobjApplySpeed(&door->openPosition, 0.0f, &door->speed, door->accel, door->decel, door->maxSpeed);

        if (door->maxFrac <= door->openPosition)
        {
            door->openPosition = door->maxFrac;
        }
        else
        {
            if (door->openPosition <= 0.0f)
            {
                door->openPosition = 0.0f;
            }
        }

        isMoving = 1;
    }

    return isMoving;
}


/**
*/
void doorsUpdatePositions(DoorRecord *door)
{
    Model *model;
    ModelNode *node;
    s32 moved;
    DoorRecord *sibling;
    s32 canMove;
    s32 i;

    struct ModelRoData_DisplayList_CollisionRecord *rodata;
    struct ModelRwData_DisplayList_CollisionRecord *rwdata;

    moved = 0;
    canMove = 1;

    sibling = door;
    while (sibling != NULL)
    {
        sibling->lastcalc60f = sibling->openPosition;
        if (doorUpdateDisplacement(sibling) != 0)
        {
            moved = 1;
        }

        sibling = sibling->linkedDoor;

        if (sibling == door)
        {
            break;
        }
    }

    sibling = door;
    if ((moved != 0))
    {
        while (sibling != NULL)
        {
            doorUpdateBbox(sibling);
            canMove = vehicleFindBlockingProp(sibling->prop);

            if (canMove == 0)
            {
                break;
            }

            sibling = sibling->linkedDoor;

            if (sibling == door)
            {
                break;
            }
        }
    }

    sibling = door;
    while (sibling != NULL)
    {
        if (moved)
        {
            if (canMove != 0)
            {
                if (sibling->openstate == DOORSTATE_OPENING)
                {
                    if (sibling->maxFrac <= sibling->openPosition)
                    {
                        sibling->openstate = DOORSTATE_STATIONARY;
                        sibling->speed = 0.0f;
                        sibling->openedTime = (u32) g_GlobalTimer;

                        doorFinishOpen(sibling);
                    }
                }
                else if ((sibling->openstate == DOORSTATE_CLOSING) && (sibling->openPosition <= 0.0f))
                {
                    sibling->openstate = DOORSTATE_STATIONARY;
                    sibling->speed = 0.0f;
                    sibling->openedTime = 0;

                    doorFinishClose(sibling);
                }

                doorGetTileShading(sibling->prop, &sibling->nextcol);
            }
            else
            {
                sibling->speed = 0.0f;
                sibling->openPosition = sibling->lastcalc60f;

                doorUpdateBbox(sibling);
            }

            doorBuildClippedVertices(sibling);
        }
        else if  (sibling->doorFlags & DOORFLAG_CLIP_TO_BBOX)
        {
            model = sibling->model;
            node = model->obj->RootNode->Child->Child;
            rodata = (struct ModelRoData_DisplayList_CollisionRecord *)node->Data;
            rwdata = &modelGetNodeRwData(model, node)->DisplayListCollisions;

            if (rwdata->Vertices != sibling->clipVtx)
            {
                for (i = 0; i < rodata->numVertices; i++)
                {
                    // struct copy
                    sibling->clipVtx[i] = rwdata->Vertices[i];
                }
            }

            rwdata->Vertices = sibling->clipVtx;
        }

        sibling->lastcalc60i = g_GlobalTimer;

        sibling = sibling->linkedDoor;

        if (sibling == door)
        {
            break;
        }
    }
}


// PD: door0f08f604
void doorGetAnglesToPlayer(DoorRecord *door, f32 *minangle, f32 *maxangle, s32 altcoordsystem)
{
    f32 anglediff;
    PropRecord *playerprop;
    BoundPadRecord *pad;
    coord3d normal;
    f32 xmin;
    f32 xmax;
    coord3d playerpos;
    f32 angle2;
    f32 cosine;
    f32 sine;
    f32 angle;
    f32 y1;
    f32 x1;
    f32 playerangle;
    f32 anglediff2;
    f32 scale;
    f32 xbound;

    pad = &g_CurrentSetup.boundpads[door->pad];
    playerprop = currentPlayerGetProp();

    if (1) { scale = 1.0f; }
    playerpos.f[0] = (((g_CurrentPlayer->bond.theta_transform.x * 30.0f) * scale) * 0.75f) + playerprop->pos.x;
    playerpos.f[1] = playerprop->pos.y;
    playerpos.f[2] = (((g_CurrentPlayer->bond.theta_transform.z * 30.0f) * scale) * 0.75f) + playerprop->pos.z;

    if (altcoordsystem != 0)
    {
        xmin = pad->bbox.xmin;
        xmax = pad->bbox.xmax;
        normal.f[0] = (pad->up.y * pad->look.z) - (pad->look.y * pad->up.z);
        normal.f[1] = (pad->up.z * pad->look.x) - (pad->look.z * pad->up.x);
        normal.f[2] = (pad->up.x * pad->look.y) - (pad->look.x * pad->up.y);
    }
    else
    {
        xmin = pad->bbox.ymin;
        xmax = pad->bbox.ymax;
        normal.f[0] = pad->up.x;
        normal.f[1] = pad->up.y;
        normal.f[2] = pad->up.z;
    }

    x1 = (pad->pos.x + (normal.x * xmin)) - playerpos.x;
    y1 = (pad->pos.z + (normal.z * xmin)) - playerpos.z;
    angle = mathAtan2(x1, y1);

    playerangle = bondviewGetPlayerYawRadians();
    anglediff = angle - playerangle;

    scale = (angle - playerangle) + M_TAU_F;
    if (angle < playerangle)
    {
        anglediff = scale;
    }

    if (anglediff > M_PI_F)
    {
        anglediff = anglediff - M_TAU_F;
    }

    if (door->doorType == DOORTYPE_SWINGING)
    {
        angle2 = (door->openPosition * M_TAU_F) / 360.0f;

        if (door->flags & 0x20000000)
        {
            angle2 = M_TAU_F - angle2;
        }

        cosine = cosf(angle2);
        sine = sinf(angle2);

        xbound = xmax - xmin;
        x1 = ((pad->pos.x + (normal.x * xmin)) + (xbound * ((normal.x * cosine) + (normal.z * sine)))) - playerpos.x;
        y1 = ((pad->pos.z + (normal.z * xmin)) + (xbound * (((-normal.x) * sine) + (normal.z * cosine)))) - playerpos.z;

        angle = mathAtan2(x1, y1);
        playerangle = bondviewGetPlayerYawRadians();

        anglediff2 = angle - playerangle;
        if (angle < playerangle)
        {
            anglediff2 += M_TAU_F;
        }
        if (anglediff2 > M_PI_F)
        {
            anglediff2 -= M_TAU_F;
        }
    }
    else
    {
        x1 = (pad->pos.x + (normal.x * xmax)) - playerpos.x;
        y1 = (pad->pos.z + (normal.z * xmax)) - playerpos.z;

        angle = mathAtan2(x1, y1);
        playerangle = bondviewGetPlayerYawRadians();

        anglediff2 = angle - playerangle;


        if (angle < playerangle)
        {
            anglediff2 += M_TAU_F;
        }

        if (anglediff2 > M_PI_F)
        {
            anglediff2 -= M_TAU_F;
        }
    }

    if (anglediff < anglediff2)
    {
        *minangle = anglediff;
        *maxangle = anglediff2;
    }
    else
    {
        *minangle = anglediff2;
        *maxangle = anglediff;
    }
}


// PD: func0f08f968
bool doorTestInteractAngle(DoorRecord *door, bool altcoordsystem)
{
    bool checkmore;
    f32 minangle;
    f32 maxangle;
    DoorRecord *sibling;
    f32 limit;
    f32 siblingmin;
    f32 siblingmax;

    checkmore = TRUE;
    limit = 0.34906587f;

    if (g_InteractProp == NULL)
    {
        doorGetAnglesToPlayer(door, &minangle, &maxangle, altcoordsystem);

        if ((minangle >= -limit) && (minangle <= limit) && (maxangle >= -limit) && (maxangle <= limit))
        {
            g_InteractProp = door->prop;
            checkmore = FALSE;
        }
        else
        {
            sibling = door->linkedDoor;

            while (sibling != NULL && sibling != door && (minangle >= 0.0f || maxangle < 0.0f))
            {
                doorGetAnglesToPlayer(sibling, &siblingmin, &siblingmax, altcoordsystem);

                if ((minangle > 0.0f) && (siblingmin < minangle))
                {
                    minangle = siblingmin;
                }

                if ((maxangle < 0.0f) && (maxangle < siblingmax))
                {
                    maxangle = siblingmax;
                }

                sibling = sibling->linkedDoor;
            }

            if ((maxangle - minangle) < M_PI_F && (minangle < 0.0f) && (maxangle > 0.0f))
            {
                g_InteractProp = door->prop;
                checkmore = FALSE;
            }
        }
    }

    return checkmore;
}


bool doorTestForInteract(PropRecord *prop)
{
	bool checkmore;
	DoorRecord *door;
    bool maybe;
    PropRecord *playerprop;
    f32 xdiff;
    f32 ydiff;
    f32 zdiff;
    BoundPadRecord *boundpads;
    u8 rooms1[32];
    u8 rooms2[32];

    checkmore = TRUE;
    door = prop->door;

	if ((door->flags & PROPFLAG_CANNOT_ACTIVATE) == 0
			&& door->maxFrac > 0
			&& (prop->flags & PROPFLAG_ONSCREEN))
    {
		maybe = FALSE;
		playerprop = currentPlayerGetProp();

		xdiff = door->runtime_pos.x - playerprop->pos.x;
		ydiff = door->runtime_pos.y - playerprop->pos.y;
		zdiff = door->runtime_pos.z - playerprop->pos.z;

		if (xdiff * xdiff + zdiff * zdiff < 40000.0f && ydiff < 200.0f && ydiff > -200.0f)
        {
			maybe = TRUE;
		}
        else
        {
            propGetRoomIds(prop, (s32*)rooms1);
            propGetRoomIds(playerprop, (s32*)rooms2);
            if (roomListsOverlap((s32*)rooms1, (s32*)rooms2) != 0)
            {
                boundpads = &g_CurrentSetup.boundpads[door->pad];
                if (padBoundsContainPoint(&playerprop->pos, 150.0f, boundpads))
                {
                    maybe = TRUE;
                }
            }
		}

		if (maybe)
        {
            checkmore = doorTestInteractAngle(door, FALSE);

            if (checkmore && (door->flags2 & PROPFLAG2_DOOR_ALTCOORDSYSTEM))
            {
                checkmore = doorTestInteractAngle(door, TRUE);
            }
		}
	}

	return checkmore;
}


void doorActivateWrapper(PropRecord *prop) //#MATCH
{
    DoorRecord *door = prop->door;

    if ((door->openstate == DOORSTATE_OPENING) || (door->openstate == DOORSTATE_WAITING))
    {
        doorActivate(door, DOORSTATE_CLOSING);
    }
    else if (door->openstate == DOORSTATE_CLOSING)
    {
        doorActivate(door, DOORSTATE_OPENING);
    }
    else if (door->openstate == DOORSTATE_STATIONARY)
    {
        if (door->openPosition > 0.5f)
        {
            doorActivate(door, DOORSTATE_CLOSING);
        }
        else
        {
            doorActivate(door, DOORSTATE_OPENING);
        }
    }
    door->runtime_bitflags |= RUNTIMEBITFLAG_ACTIVATED;
    coopNoteActivator();
    door->flags2 &= ~8;
    switchActivateLinkedDoors(prop);
}


bool posIsInFrontOfDoor(PropRecord *prop, DoorRecord *door)
{
    BoundPadRecord *pad;
    coord3d diff;
    coord3d normal;
    f32 dot;
    f32 side;

    pad = &g_CurrentSetup.boundpads[((ObjectRecord *) door)->pad];

    normal.f[0] = (pad->up.y * pad->look.z) - (pad->look.y * pad->up.z);
    normal.f[1] = (pad->up.z * pad->look.x) - (pad->look.z * pad->up.x);
    normal.f[2] = (pad->up.x * pad->look.y) - (pad->look.x * pad->up.y);

    diff.x = prop->pos.x - pad->pos.x;
    diff.y = prop->pos.y - pad->pos.y;
    diff.z = prop->pos.z - pad->pos.z;

    dot = (side = ((diff.x * normal.f[0]) + (diff.y * normal.f[1])) + (diff.z * normal.f[2]));

    if (door->doorFlags & DOORFLAG_FLIP)
    {
        side = -dot;
    }

    if (side < 0.0f)
    {
        return FALSE;
    }
    else if (side > 0.0f)
    {
        return TRUE;
    }
    else
    {
        return TRUE;
    }
}


void doorsChooseSwingDirection(PropRecord *chrprop, DoorRecord *door)
{
    if ((door->flags & PROPFLAG_DOOR_TWOWAY) && door->openstate == PROPSTATE_NONE && door->openPosition == 0.0f)
    {
        bool infront = posIsInFrontOfDoor(chrprop, door);
        u32 wantflag = 0;

        if ((door->doorFlags & DOORFLAG_FLIP) == 0)
        {
            if (!infront)
            {
                wantflag = PROPFLAG_DOOR_OPENTOFRONT;
            }
        }
        else
        {
            if (infront)
            {
                wantflag = PROPFLAG_DOOR_OPENTOFRONT;
            }
        }

        // If flags are different
        if ((s32)((door->flags ^ wantflag) << 2) < 0)
        {
            // Toggle direction on door and siblings
            DoorRecord *sibling = door;

            do
            {
                sibling->flags ^= PROPFLAG_DOOR_OPENTOFRONT;
                sibling = sibling->linkedDoor;
            } while (sibling && sibling != door);
        }
    }
}


TICKOP propdoorInteract(PropRecord* doorprop)
{
    s32 canOpen;
    PropRecord* playerprop;
    DoorRecord* door;
    textoverride* txt;

    door = doorprop->door;
    canOpen = 0;
    playerprop = currentPlayerGetProp();

    if (door->keyflags == 0)
    {
        canOpen = 1;
    }
    else if (invCheckHasKeyFlags(door->keyflags) != 0)
    {
        canOpen = 1;
    }
    else if (posIsInFrontOfDoor(playerprop, door) != 0)
    {
        if ((door->flags2 & PROPFLAG2_10000000) && !(door->flags2 & PROPFLAG2_08000000))
        {
            canOpen = 1;
        }
    }
    else if (!(door->flags2 & PROPFLAG2_10000000) && (door->flags2 & PROPFLAG2_08000000))
    {
        canOpen = 1;
    }

    if (doorIsPadlockFree(door) == 0)
    {
        canOpen = 0;
    }

    // Counter-op: a door whose opening is Bond's progress (Depot's train)
    // stays shut for the guard player, showing as locked.
    if (counterOpIsGuardPlayer(playerGetCurrentNum()) && counterOpDoorIsScripted(door))
    {
        canOpen = 0;
    }

    if (canOpen != 0)
    {
        doorsChooseSwingDirection(playerprop, door);
        doorActivateWrapper(doorprop);
    }
    else if ((door->openstate == DOORSTATE_STATIONARY) && (door->openPosition < 0.5f))
    {
        if (!(door->flags2 & PROPFLAG2_00000004))
        {
            txt = invGetOverrideForObj((ObjectRecord*)door);
            if ((txt != NULL) && (txt->pickuptext != 0))
            {
#ifdef VERSION_US
                hudmsgBottomShow(langGet((s32) txt->pickuptext));
#else
                hudmsgBottomShowText(langGet((s32) txt->pickuptext));
#endif
            }
            else
            {
#ifdef VERSION_US
                hudmsgBottomShow(langGet(0xA440));
#else
                hudmsgBottomShowText(langGet(0xA440));
#endif
            }
        }
        // Counter-op: the guard player trying a locked door isn't news to
        // the level scripts.
        if (!counterOpIsGuardPlayer(playerGetCurrentNum()))
        {
            door->runtime_bitflags |= RUNTIMEBITFLAG_ACTIVATED;
            coopNoteActivator();
        }
        door->flags2 |= PROPFLAG2_00000008;
    }

    return TICKOP_NONE;
}


void alarmActivate(void)
{
    if (alarm_timer < 1) {
        alarm_timer = 1;
    }
    return;
}


void alarmStopSound(void)
{
    if ((g_AlarmSound != 0) && (sndGetPlayingState(g_AlarmSound) != AL_STOPPED)) {
        sndDeactivate(g_AlarmSound);
    }
    return;
}


void alarmDeactivate(void)
{
  alarm_timer = 0;
  alarmStopSound();
  return;
}


bool alarmIsActive(void)
{
  return (0 < alarm_timer);
}


void gasStart(coord3d *source) //#MATCH
{
    activate_gas_sound_timer = TRUE;
    gasLeakTimer               = 0.0f;
    gasLeakSource.x             = source->x;
    gasLeakSource.y             = source->y;
    gasLeakSource.z             = source->z;
    if (bossGetStageNum() == LEVELID_EGYPT)
    {
        gasTimeToFullOpacity = 120.0f;
        gasDoesDamageFlag = FALSE;
        return;
    }
    gasTimeToFullOpacity = 3600.0f;
    gasDoesDamageFlag = TRUE;
}


void gasStopSound(void)
{
    if ((g_GasSound != NULL) && (sndGetPlayingState(g_GasSound) != AL_STOPPED)) {
        sndDeactivate(g_GasSound);
    }
    return;
}


bool gasIsActive() //#MATCH
{
    return (toxic_gas_sound_timer > 0);
}


void gasTick(void)
{
    if (activate_gas_sound_timer != 0)
    {
        toxic_gas_sound_timer += g_GlobalTimerDelta;
        if (gasTimeToFullOpacity <= toxic_gas_sound_timer)
        {
            toxic_gas_sound_timer = gasTimeToFullOpacity;
            activate_gas_sound_timer = 0;
        }
    }

    if (toxic_gas_sound_timer > 0.0f && g_PlayerInvincible == FALSE)
    {
        fogSwitchToSolosky2(toxic_gas_sound_timer / gasTimeToFullOpacity);

        if (gasDoesDamageFlag == 0) { return; }

#ifdef VERSION_EU
        if (g_GasLastCoughTime < (g_GlobalTimer - 0xBB))
#else
        if (g_GasLastCoughTime < (g_GlobalTimer - 0xE1))
#endif
        {
            g_GasLastCoughTime = g_GlobalTimer;
            if (toxic_gas_sound_timer >= 600.0f)
            {
                sndPlaySfx(g_musicSfxBufferPtr, COUGH_SFX, NULL);
            }
            if (toxic_gas_sound_timer >= 1800.0f)
            {
                playerTakeDamage(0.125f, 0.0f, 0.0f, -1, 0);
            }
        }

        if (gasLeakTimer < gasTimeToFullOpacity)
        {
            gasLeakTimer = gasLeakTimer + g_GlobalTimerDelta;
            if ((g_GasSound == NULL) && (lvGetControlsLocked() == 0))
            {
                sndPlaySfx(g_musicSfxBufferPtr, GAS_HISS_SFX, &g_GasSound);
            }
            if (g_GasSound != NULL)
            {
                chrobjSndCreatePostEventDefault(g_GasSound, &gasLeakSource);
            }
        }
        else
        {
            if ((g_GasSound != NULL) && (sndGetPlayingState(g_GasSound) != 0))
            {
                sndDeactivate(g_GasSound);
            }
        }
    }
}


void countdownTimerSetVisible(int clocklockbits, bool unset)
{
    if (unset)
    {
        clock_drawn_flag &= ~clocklockbits;
        return;
    }
    clock_drawn_flag |= clocklockbits;
}

bool countdownTimerIsVisible(void)
{
    return clock_drawn_flag == FALSE;
}

void countdownTimerSetValue(f32 time)
{
    clock_time = time;
}

f32 countdownTimerGetValue(void)
{
    return clock_time;
}

void countdownTimerSetRunning(bool enable)
{
    clock_enable = enable;
}

bool countdownTimerIsRunning(void)
{
    return clock_enable;
}

void countdownTimerTick(void)
{
    if (clock_enable != 0) {
        clock_time = clock_time - g_GlobalTimerDelta;
    }
}

char g_CountdownColon[] = ":\n";

/*
    Renders the on-screen countdown timer
    using minutes, seconds and milliseconds
    in the following format

    00 : 00 : 00

    Timer value is set using countdownTimerSetValue()
*/
Gfx *countdownTimerRender(Gfx *DL)
{
    s32 mins;
    s32 secs;
    s32 ms;
    s32 valign_offset;
    f32 time;

    if (clock_drawn_flag == 0) {

        time = clock_time;
        if (time < 0.0f) {
            time = -time;
        }

        mins = (s32) mathFloor(time / 3600.0f);
        secs = (s32) mathFloor(time / 60.0f) - (mins * 60);
        ms = ((s32) mathFloor((time * 100.0f) / 60.0f) - (mins * 6000)) - (secs * 100);

        DL = textSetupDraw(DL);

        #if defined(VERSION_US)
            valign_offset = 18;
        #else
            valign_offset = 28;
        #endif

        // Minutes
        DL = gunDrawHudInteger(DL, (mins % 100) / 10, 0x82, HUDHALIGN_MIDDLE, ( viGetViewTop() + viGetViewHeight()) - valign_offset, HUDVALIGN_MIDDLE, 1);
        DL = gunDrawHudInteger(DL, mins % 10, 0x8A, HUDHALIGN_MIDDLE, ( viGetViewTop() + viGetViewHeight()) - valign_offset, HUDVALIGN_MIDDLE, 1);

        // :
        DL = gunDrawHudString(DL, g_CountdownColon, 0x93, HUDHALIGN_MIDDLE, (viGetViewTop() + viGetViewHeight()) - valign_offset, HUDVALIGN_MIDDLE, 1);

        // Seconds
        DL = gunDrawHudInteger(DL, (secs % 60) / 10, 0x9C, HUDHALIGN_MIDDLE, (viGetViewTop() + viGetViewHeight()) - valign_offset, HUDVALIGN_MIDDLE, 1);
        DL = gunDrawHudInteger(DL, secs % 10, 0xA4, HUDHALIGN_MIDDLE, (viGetViewTop() + viGetViewHeight()) - valign_offset, HUDVALIGN_MIDDLE, 1);

        // :
        DL = gunDrawHudString(DL, g_CountdownColon, 0xAD, HUDHALIGN_MIDDLE, (viGetViewTop() + viGetViewHeight()) - valign_offset, HUDVALIGN_MIDDLE, 1);

        // Milliseconds
        DL = gunDrawHudInteger(DL, (ms % 100) / 10, 0xB6, HUDHALIGN_MIDDLE, (viGetViewTop() + viGetViewHeight()) - valign_offset, HUDVALIGN_MIDDLE, 1);
        DL = gunDrawHudInteger(DL, ms % 10, 0xBE, HUDHALIGN_MIDDLE, (viGetViewTop() + viGetViewHeight()) - valign_offset, HUDVALIGN_MIDDLE, 1);

        DL = textEndDraw(DL);
    }

    return DL;
}


void alarmGasTimerTick(void)
{
    if (alarmIsActive() != 0)
    {
        if ((g_AlarmSound == 0) && (lvGetControlsLocked() == 0))
        {
            sndPlaySfx(g_musicSfxBufferPtr, ALARM3_SFX, &g_AlarmSound);
        }

        alarm_timer = alarm_timer + g_ClockTimer;

        if (CHROBJ_GAS_TIMER < alarm_timer)
        {
            alarmDeactivate();
        }
    }

    gasTick();
    countdownTimerTick();
    proxminesTickGuards();
    g_RemoteMineOwnerTriggerFlag = 0;

    return;
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


void playerDropInventory(void)
{
    ChrRecord *playerchr;
    PropRecord *prop;
    enum ITEM_IDS item;
    enum PROP propid;

    playerchr = g_CurrentPlayer->prop->chr;

    chrRemoveHeldWeapon(playerchr, GUNRIGHT);
    chrRemoveHeldWeapon(playerchr, GUNLEFT);

    for (item = ITEM_FIST; item != ITEM_IDS_MAX; item++)
    {
        propid = weaponGetChrModel(item);

        if ((propid >= 0) && (invHasItem(item) != 0))
        {
            prop = chrGiveWeaponEx(playerchr, propid, item, 0x20000000, NULL, NULL);

            if (prop != NULL)
            {
                propobjSetDropped(prop, DROPTYPE_DEFAULT);
                objDrop(prop);
            }
        }
    }
}


void alarmsReset(void) {
    alarmDeactivate();
    gasStopSound();
}
