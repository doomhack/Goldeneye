/**
 * Objects in flight: thrown and dropped weapons, mines, knives, rockets, grenades, things
 * knocked off tables, and props launched by AI scripts.
 *
 * - A flying or landed object has a Projectile from a pool of 20; when the pool is full
 *   the oldest one is taken over (its object is removed). An object stuck to another (a
 *   mine on a door, say) has an Embedment from a pool of 40 instead.
 * - Each tick (projectileTick, from objTick) a flying object falls, tumbles and moves.
 *   With PROJECTILEFLAG_COLLIDE3D it moves against the level and props in 3D
 *   (projectileMove); otherwise only along the floor plan (objMoveAlongFloor). It bounces
 *   until it is slow, then settles: it turns to lie flat and slides to a stop, and its
 *   projectile is freed.
 * - Mines, bugs and plastique stick to what they hit; throwing knives hurt guards and
 *   players; rockets and grenade rounds go off when they hit something.
 *
 * The game's state depends on the exact results here, so the float sums keep the order of
 * the originals.
 */

#include <ultra64.h>
#include <math.h>
#include <bondgame.h>
#include <bondaicommands.h>
#include <music.h>
#include <snd.h>
#include "game/prop/projectile.h"
#include "game/prop/propobj.h"
#include "game/prop/prop.h"
#include "game/prop/setup.h"
#include "game/prop/explosion.h"
#include "game/audio/sfxvolume.h"
#include "game/chr/chr.h"
#include "game/chr/chraction.h"
#include "game/chr/chrai.h"
#include "game/level/lv.h"
#include "game/math/line_tri_intersect.h"
#include "game/math/matrix.h"
#include "game/math/physics.h"
#include "game/math/quaternion.h"
#include "game/math/trig.h"
#include "game/mission/objective.h"
#include "game/model/jointlist.h"
#include "game/model/model.h"
#include "game/nav/geometry.h"
#include "game/nav/stan.h"
#include "game/player/bondview.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/world/bg.h"
#include "random.h"

#if defined(VERSION_EU)
#define PROJECTILE_LIFETIME_TICKS 2000
#define ROCKET_FUEL_TICKS 251
#else
#define PROJECTILE_LIFETIME_TICKS 2400 // 40 seconds
#define ROCKET_FUEL_TICKS 301
#endif

// Once a rocket's speed squared passes this (about 167 units per tick), it stops speeding up
#define ROCKET_TOP_SPEED_SQ 27777.773f

// A rocket rises for up to this many ticks' worth of gravity (also the gravity per tick)
#define ROCKET_RISE_TIME 0.27777779f

// A landed object's speed is multiplied by this each tick, unless it has its own friction
#define SLIDE_FRICTION 0.9f

// A bounce slower than this (units per tick, upwards) settles
#define MIN_BOUNCE_SPEED 2.2222223f

// How far a moved object is held off the floor
#define FLOOR_CLEARANCE 4.0f

// moveResult values, from projectileMove and objMoveAlongFloor
#define MOVE_BLOCKED 0
#define MOVE_CLEAR   1
#define MOVE_STUCK   2 // blocked, and the caller asked to stick

Projectile g_Projectiles[PROJECTILES_ARR_MAX];
Embedment g_Embedments[EMBEDMENT_ARR_MAX];

// What the last projectile move hit, if anything (props only)
static PropRecord *s_HitProp;
static s32 s_HitPart = -1; // the body part for guards and players (HIT_), -1 for objects
static Model *s_HitModel;
static ModelNode *s_HitNode;
// The direction of the move that hit, in the world and on the screen
static coord3d s_HitDir;
static coord3d s_HitScreenDir;

/** Called on level load: empties both pools. */
void projectilesReset(void)
{
    for (s32 i = 0; i < PROJECTILES_ARR_MAX; i++)
    {
        g_Projectiles[i].flags = PROJECTILEFLAG_FREE;
        g_Projectiles[i].sounds[0] = NULL;
        g_Projectiles[i].sounds[1] = NULL;
    }

    for (s32 i = 0; i < EMBEDMENT_ARR_MAX; i++)
    {
        g_Embedments[i].flags = EMBEDMENTFLAG_FREE;
    }

    s_HitProp = NULL;
    s_HitPart = -1;
}

/* ---- The pools ---- */

static void projectileStopSound(ALSoundState *sound)
{
    if (sound != NULL && sndGetPlayingState(sound))
    {
        sndDeactivate(sound);
    }
}

void projectileFree(Projectile *projectile)
{
    if (projectile->flags & PROJECTILEFLAG_LAUNCHING)
    {
        projectileStopSound(projectile->sounds[0]);
        projectileStopSound(projectile->sounds[1]);
    }

    projectile->flags |= PROJECTILEFLAG_FREE;
}

static void projectileReset(Projectile *projectile)
{
    projectile->flags = 0;
    projectile->speed = (coord3d) {{0.0f, 0.0f, 0.0f}};
    projectile->accel = (coord3d) {{0.0f, 0.0f, 0.0f}};
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
    projectile->obj = NULL;
    projectile->launchTick = 0;
}

/**
 * Takes a free projectile, or if there is none, the one launched longest ago (removing
 * its object). NULL only if no projectile has an object.
 */
static Projectile *projectileAllocate(void)
{
    s32 oldest = -1;

    for (s32 i = 0; i < PROJECTILES_ARR_MAX; i++)
    {
        if (g_Projectiles[i].flags & PROJECTILEFLAG_FREE)
        {
            projectileReset(&g_Projectiles[i]);
            return &g_Projectiles[i];
        }
    }

    for (s32 i = 0; i < PROJECTILES_ARR_MAX; i++)
    {
        if (g_Projectiles[i].obj && (oldest < 0 || g_Projectiles[i].launchTick < g_Projectiles[oldest].launchTick))
        {
            oldest = i;
        }
    }

    if (oldest < 0)
    {
        return NULL;
    }

    objFreeEmbedmentOrProjectile(g_Projectiles[oldest].obj->prop);
    g_Projectiles[oldest].obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;

    projectileReset(&g_Projectiles[oldest]);
    return &g_Projectiles[oldest];
}

void embedmentFree(Embedment *embedment)
{
    embedment->flags |= EMBEDMENTFLAG_FREE;
}

Embedment *embedmentAllocate(void)
{
    for (s32 i = 0; i < EMBEDMENT_ARR_MAX; i++)
    {
        if (g_Embedments[i].flags & EMBEDMENTFLAG_FREE)
        {
            g_Embedments[i].flags = 0;
            g_Embedments[i].projectile = NULL;
            return &g_Embedments[i];
        }
    }

    return NULL;
}

/** An object's projectile: its own, or its embedment's. NULL if it has none. */
static Projectile *objGetProjectile(ObjectRecord *obj)
{
    if (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED)
    {
        return obj->embedment->projectile;
    }

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
    {
        return obj->projectile;
    }

    return NULL;
}

/**
 * Gives an object a projectile, if it can get one. An embedded object always gets a new
 * one (the old one is not freed).
 */
void objEnsureProjectile(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED)
    {
        obj->embedment->projectile = projectileAllocate();
    }
    else if (!(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
    {
        obj->projectile = projectileAllocate();

        if (obj->projectile)
        {
            obj->runtime_bitflags |= RUNTIMEBITFLAG_HASPROJECTILE;
        }
    }
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

/**
 * Makes an object's projectile move against the level and props in 3D, starting from the
 * room it is in.
 */
void projectileEnableCollide3D(PropRecord *prop)
{
    Projectile *projectile = objGetProjectile(prop->obj);

    if (projectile)
    {
        projectile->flags |= PROJECTILEFLAG_COLLIDE3D;

        if (prop->stan)
        {
            projectile->rooms[0] = prop->stan->room;
            projectile->rooms[1] = 0xff;
            return;
        }

        projectile->rooms[0] = 0xff;
    }
}

/** The radius an object collides with when it moves along the floor. */
f32 objGetWidth(ObjectRecord *obj)
{
    // Original bug: compares a PROPDEF_ type to a PROP_TYPE_ value
    if (obj->type == PROP_TYPE_WEAPON)
    {
        return 20.0f;
    }

    return 10.0f;
}

/* ---- Ray tests against props, for 3D moves ---- */

/**
 * Tests a ray (in screen space) against an object's model, node by node. On a hit, gives
 * the world position and the normal facing back along the ray.
 */
static bool projectileTestModel(ObjectRecord *obj, coord3d *screenStart, coord3d *screenDir, coord3d *hitPos, coord3d *hitNormal, Model **hitModel, ModelNode **hitNode)
{
    Model *model = obj->model;
    ModelNode *node = NULL;
    ModelNode *hitnode = NULL;
    struct HitThing hitthing;
    s32 mtxindex;
    s32 found = 0;

    if (obj->type == PROPDEF_DOOR)
    {
        // A door's bounding box first
        found = modelTestRayIntersectsTransformedBBox(&((DoorRecord *) obj)->bbox, &model->render_pos[0].pos, screenStart, screenDir);
        node = model->obj->RootNode;

        if (found > 0 && !modelFindTriangleHit(model, node, screenStart, screenDir, &hitthing, &mtxindex, &hitnode))
        {
            found = 0;
        }
    }
    else
    {
        do
        {
            found = modelFindRayHitNext(model, screenStart, screenDir, &node);
        } while (found > 0 && !modelFindTriangleHit(model, node, screenStart, screenDir, &hitthing, &mtxindex, &hitnode));
    }

    if (found <= 0)
    {
        return FALSE;
    }

    Mtxf *mtx = &model->render_pos[mtxindex].pos;

    *hitPos = hitthing.hitpos;
    mtx4TransformVecInPlace(mtx, hitPos);
    mtx4TransformVecInPlace(camGetViewToWorldMtxf(), hitPos);

    *hitNormal = hitthing.normal;
    mtx4RotateVecInPlace(mtx, hitNormal);

    if (hitNormal->f[0] * screenDir->f[0] + hitNormal->f[1] * screenDir->f[1] + hitNormal->f[2] * screenDir->f[2] > 0.0f)
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

/** The normal of a hit on a prop's side: back along the ray, flat. */
static void projectileSideNormal(coord3d *rayDir, coord3d *hitNormal)
{
    hitNormal->x = -rayDir->x;
    hitNormal->y = 0.0f;
    hitNormal->z = -rayDir->z;

    if (hitNormal->x != 0.0f || hitNormal->z != 0.0f)
    {
        guNormalize(&hitNormal->x, &hitNormal->y, &hitNormal->z);
    }
    else
    {
        hitNormal->z = 1.0f;
    }
}

/**
 * Tests a ray against a prop's collision polygon (its outline on the floor plan, between
 * its top and bottom). If it hits closer than *hitDist, updates the hit.
 */
static bool projectileTestPolygon(PropRecord *prop, coord3d *rayStart, coord3d *rayEnd, coord3d *rayDir, coord3d *hitPos, coord3d *hitNormal, f32 *hitDist)
{
    coord2d *polygon;
    s32 numedges;
    f32 ymax;
    f32 ymin;
    f32 bestfrac = 1.0f;
    s32 bestedge = -1;

    propGetCollisionPolygon(prop, &polygon, &numedges, &ymax, &ymin);

    if (numedges <= 0
        || (ymax < rayStart->y && ymax < rayEnd->y)
        || (rayStart->y < ymin && rayEnd->y < ymin))
    {
        return FALSE;
    }

    coord2d rayStart2d = {{rayStart->x, rayStart->z}};
    coord2d rayEnd2d = {{rayEnd->x, rayEnd->z}};

    for (s32 i = 0; i < numedges; i++)
    {
        s32 next = (i + 1) % numedges;

        if (geoSegmentsIntersect(rayStart->x, rayStart->z, rayEnd->x, rayEnd->z, polygon[i].x, polygon[i].y, polygon[next].x, polygon[next].y))
        {
            coord2d edgeStart = polygon[i];
            coord2d edgeEnd = polygon[next];
            f32 frac = geoSegmentHitFrac(&rayStart2d, &rayEnd2d, &edgeStart, &edgeEnd);

            if (frac < bestfrac)
            {
                bestfrac = frac;
                bestedge = i;
            }
        }
    }

    // Original bug: a hit on edge 0 is missed
    if (bestedge > 0)
    {
        s32 next = (bestedge + 1) % numedges;
        coord3d edgeStart = {{polygon[bestedge].x, 0.0f, polygon[bestedge].y}};
        coord3d edgeEnd = {{polygon[next].x, 0.0f, polygon[next].y}};
        coord3d intersection;

        chrLineLineIntersection(&edgeStart, &edgeEnd, rayStart, rayDir, &intersection);

        f32 dist = (rayDir->z * (intersection.z - rayStart->z)) + (((intersection.x - rayStart->x) * rayDir->x) + ((intersection.y - rayStart->y) * rayDir->y));

        if (dist < *hitDist)
        {
            *hitDist = dist;
            *hitPos = intersection;
            projectileSideNormal(rayDir, hitNormal);

            s_HitProp = prop;
            s_HitPart = HIT_NULL_PART;
            s_HitModel = NULL;
            s_HitNode = NULL;
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Tests a ray against one object: its model if it is on screen, otherwise its bounding
 * sphere and collision polygon. The hit must be within maxDist.
 */
static bool projectileTestObject(ObjectRecord *obj, coord3d *rayStart, coord3d *rayEnd, coord3d *rayDir, f32 maxDist, coord3d *screenStart, coord3d *screenDir, coord3d *hitPos, coord3d *hitNormal, f32 *hitDist, Model **hitModel, ModelNode **hitNode)
{
    Model *model = obj->model;
    f32 radius = modelGetRadius(model);
    PropRecord *prop = obj->prop;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 along = 0.0f;

    // How far along the ray the object is (attached objects count as at the start)
    if (prop->parent == NULL)
    {
        dx = obj->runtime_pos.x - rayStart->x;
        dy = obj->runtime_pos.y - rayStart->y;
        dz = obj->runtime_pos.z - rayStart->z;
        along = (dz * rayDir->z) + ((dx * rayDir->x) + (dy * rayDir->y));
    }

    if (!(-radius <= along && along <= maxDist + radius))
    {
        return FALSE;
    }

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        if (projectileTestModel(obj, screenStart, screenDir, hitPos, hitNormal, hitModel, hitNode))
        {
            dx = hitPos->x - rayStart->x;
            dy = hitPos->y - rayStart->y;
            dz = hitPos->z - rayStart->z;
            along = (dz * rayDir->z) + ((dx * rayDir->x) + (dy * rayDir->y));

            if (0.0f <= along && along <= maxDist)
            {
                *hitDist = along;
                return TRUE;
            }
        }
    }
    else
    {
        radius = modelGetRadius(model);

        if (intersectRaySphere(rayStart, rayDir, &obj->runtime_pos, radius))
        {
            *hitDist = maxDist;

            if (projectileTestPolygon(prop, rayStart, rayEnd, rayDir, hitPos, hitNormal, hitDist))
            {
                *hitModel = model;
                *hitNode = model->obj->RootNode;
                return TRUE;
            }
        }
    }

    return FALSE;
}

/**
 * Tests a ray against an object and, if it is on screen, the objects attached to it. If
 * one is hit closer than *bestDist, updates the hit.
 */
static bool projectileTestObjectTree(ObjectRecord *obj, coord3d *rayStart, coord3d *rayEnd, coord3d *rayDir, f32 maxDist, coord3d *screenStart, coord3d *screenDir, coord3d *bestHitPos, coord3d *bestHitNormal, f32 *bestDist)
{
    PropRecord *prop = obj->prop;
    coord3d hitPos;
    coord3d hitNormal;
    f32 hitDist;
    Model *hitModel;
    ModelNode *hitNode;
    bool found = FALSE;

    if (projectileTestObject(obj, rayStart, rayEnd, rayDir, maxDist, screenStart, screenDir, &hitPos, &hitNormal, &hitDist, &hitModel, &hitNode)
        && hitDist < *bestDist)
    {
        *bestDist = hitDist;
        *bestHitPos = hitPos;
        *bestHitNormal = hitNormal;

        s_HitProp = obj->prop;
        s_HitPart = -1;
        s_HitModel = hitModel;
        s_HitNode = hitNode;
        found = TRUE;
    }

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        for (PropRecord *child = prop->child; child != NULL; child = child->prev)
        {
            if ((child->flags & PROPFLAG_ONSCREEN)
                && projectileTestObjectTree(child->obj, rayStart, rayEnd, rayDir, maxDist, screenStart, screenDir, bestHitPos, bestHitNormal, bestDist))
            {
                found = TRUE;
            }
        }
    }

    return found;
}

/**
 * Tests a ray against a guard's or player's body parts, if they are on screen. If one is
 * hit closer than *bestDist, updates the hit; its position is on the ray.
 */
static bool projectileTestChr(ChrRecord *chr, coord3d *rayStart, coord3d *rayDir, f32 rayLen, coord3d *screenStart, coord3d *screenDir, coord3d *hitPos, coord3d *hitNormal, f32 *bestDist)
{
    PropRecord *prop = chr->prop;
    f32 radius = modelGetRadius(chr->model);
    f32 dx = prop->pos.x - rayStart->x;
    f32 dy = prop->pos.y - rayStart->y;
    f32 dz = prop->pos.z - rayStart->z;
    f32 along = (dz * rayDir->z) + ((dx * rayDir->x) + (dy * rayDir->y));

    if (!(-radius <= along && along <= rayLen + radius && (prop->flags & PROPFLAG_ONSCREEN)))
    {
        return FALSE;
    }

    JointListEntry *entry = chr->jointList;
    Model *model;
    ModelNode *node;
    s32 bodyPart = jointListFindHit(&entry, screenStart, screenDir, &model, &node);

    if (bodyPart <= 0)
    {
        return FALSE;
    }

    Mtxf *mtx = modelFindNodeMtx(model, node, 0);

    dx = mtx->m[3][0] - screenStart->x;
    dy = mtx->m[3][1] - screenStart->y;
    dz = mtx->m[3][2] - screenStart->z;

    f32 hitDist = (dz * screenDir->z) + ((dx * screenDir->x) + (dy * screenDir->y));

    if (!(hitDist < *bestDist))
    {
        return FALSE;
    }

    *bestDist = hitDist;
    hitPos->x = (rayDir->x * hitDist) + rayStart->x;
    hitPos->y = (rayDir->y * hitDist) + rayStart->y;
    hitPos->z = (rayDir->z * hitDist) + rayStart->z;
    projectileSideNormal(rayDir, hitNormal);

    s_HitProp = prop;
    s_HitPart = bodyPart;
    s_HitModel = model;
    s_HitNode = node;
    return TRUE;
}

/**
 * Finds the closest prop of the given CDTYPEs that the segment from rayStart to rayEnd
 * hits, in the given rooms (-1 terminated), ignoring `ignoreProp`. On a hit, sets
 * hitPos, hitNormal and the s_Hit variables, and returns TRUE.
 */
static bool projectileFindHitProp(PropRecord *ignoreProp, coord3d *rayStart, coord3d *rayEnd, u32 cdtypes, coord3d *hitPos, coord3d *hitNormal, s32 *rooms)
{
    PropRecord *playerstank = playerGetTank();
    bool found = FALSE;
    coord3d dir;

    dir.x = rayEnd->x - rayStart->x;
    dir.y = rayEnd->y - rayStart->y;
    dir.z = rayEnd->z - rayStart->z;

    f32 dist = sqrtf(dir.f[0] * dir.f[0] + dir.f[1] * dir.f[1] + dir.f[2] * dir.f[2]);

    if (dist == 0.0f)
    {
        return FALSE;
    }

    dir.x *= 1.0f / dist;
    dir.y *= 1.0f / dist;
    dir.z *= 1.0f / dist;

    coord3d screenStart = *rayStart;
    mtx4TransformVecInPlace(camGetWorldToScreenMtxf(), &screenStart);

    coord3d screenDir = dir;
    mtx4RotateVecInPlace(camGetWorldToScreenMtxf(), &screenDir);

    f32 bestDist = dist;

    if (cdtypes == 0)
    {
        return FALSE;
    }

    roomGetProps(rooms);

    for (s16 *propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
    {
        PropRecord *prop = &g_Props[*propnum];

        if (prop == ignoreProp)
        {
            continue;
        }

        if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_DOOR)
        {
            ObjectRecord *obj = prop->obj;

            if ((obj->runtime_bitflags & RUNTIMEBITFLAG_ISRETICK) || (obj->flags2 & PROPFLAG2_THROWTHROUGH))
            {
                continue;
            }

            if (prop->type == PROP_TYPE_DOOR)
            {
                if (!(cdtypes & CDTYPE_DOORS) && !(propDoorGetCdTypes(prop) & cdtypes))
                {
                    continue;
                }
            }
            else if (!(cdtypes & CDTYPE_OBJS))
            {
                continue;
            }

            // The tank the player is driving
            if (prop == playerstank && (obj->state & PROPSTATE_NOCOLLISION))
            {
                continue;
            }

            if (projectileTestObjectTree(obj, rayStart, rayEnd, &dir, dist, &screenStart, &screenDir, hitPos, hitNormal, &bestDist))
            {
                found = TRUE;
            }
        }
        else if (prop->type == PROP_TYPE_CHR || (prop->type == PROP_TYPE_VIEWER && prop->chr))
        {
            if (prop->type == PROP_TYPE_VIEWER)
            {
                if (!g_playerPointers[playerGetIndexForProp(prop)]->perimEnabled || !(cdtypes & CDTYPE_PLAYERS))
                {
                    continue;
                }
            }
            else if ((prop->chr->hidden & CHRHIDDEN_MOVING) || !(cdtypes & CDTYPE_CHRS))
            {
                continue;
            }

            if (projectileTestChr(prop->chr, rayStart, &dir, dist, &screenStart, &screenDir, hitPos, hitNormal, &bestDist))
            {
                found = TRUE;
            }
        }
        else if (prop->type == PROP_TYPE_VIEWER && g_playerPointers[playerGetIndexForProp(prop)]->perimEnabled)
        {
            if (projectileTestPolygon(prop, rayStart, rayEnd, &dir, hitPos, hitNormal, &bestDist))
            {
                found = TRUE;
            }
        }
    }

    if (found)
    {
        s_HitDir = dir;
        s_HitScreenDir = screenDir;
    }

    return found;
}

/* ---- Moving ---- */

static bool isBetween(f32 a, f32 b, f32 x)
{
    return (a <= b && x <= b && a <= x) || (b <= a && b <= x && x <= a);
}

/**
 * Moves an object towards targetpos in 3D, against the level and props (in the rooms
 * along the way and those on screen). Keeps its floor tile and room list up to date.
 *
 * Returns MOVE_CLEAR if nothing was in the way; MOVE_STUCK if something was and canEmbed
 * is set; otherwise MOVE_BLOCKED. Blocked by the level, it stops just short of the wall
 * (unless moveOnlyIfClear); blocked by a prop, it stays where it is but takes the target
 * height. hitpos and hitnormal give the hit.
 */
static s32 projectileMove(ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal, bool canEmbed, bool moveOnlyIfClear)
{
    PropRecord *prop = obj->prop;
    Projectile *projectile = obj->projectile;
    s32 result = MOVE_CLEAR;
    coord3d endpos;
    s32 roomNums[121];
    s32 roomCount;
    u8 roomSet[8];

    s_HitProp = NULL;

    if (obj->runtime_pos.x == targetpos->x && obj->runtime_pos.y == targetpos->y && obj->runtime_pos.z == targetpos->z)
    {
        return result;
    }

    endpos = *targetpos;

    // The rooms along the way, then those on screen
    roomCount = 0;
    bgFindRoomsAlongSegment(&obj->runtime_pos, &endpos, projectile->rooms, roomSet, roomNums, &roomCount, 20);

    if (roomCount > 20)
    {
        roomCount = 20;
    }

    roomCount += bgCopyVisibleRoomsToList(&roomNums[roomCount], 100);
    roomNums[roomCount] = -1;

    // The level. The hit in the last room listed wins, not the nearest.
    for (s32 *room = roomNums; *room >= 0; room++)
    {
        HitThing hit;

        if (!bgTestBulletHitBackground(&obj->runtime_pos, &endpos, *room, &hit))
        {
            continue;
        }

        f32 scale = bgGetLevelScaleInv();

        hit.hitpos.x *= scale;
        hit.hitpos.y *= scale;
        hit.hitpos.z *= scale;

        if (isBetween(obj->runtime_pos.x, endpos.x, hit.hitpos.x)
            && isBetween(obj->runtime_pos.y, endpos.y, hit.hitpos.y)
            && isBetween(obj->runtime_pos.z, endpos.z, hit.hitpos.z)
            && !(obj->runtime_pos.x == hit.hitpos.x && obj->runtime_pos.y == hit.hitpos.y && obj->runtime_pos.z == hit.hitpos.z))
        {
            result = MOVE_BLOCKED;
            *hitpos = hit.hitpos;
            *hitnormal = hit.normal;

            if (hitnormal->x != 0.0f || hitnormal->y != 0.0f || hitnormal->z != 0.0f)
            {
                guNormalize(&hitnormal->x, &hitnormal->y, &hitnormal->z);
            }
            else
            {
                hitnormal->z = 1.0f;
            }
        }
    }

    f32 y;

    if (result == MOVE_BLOCKED)
    {
        // Stop 0.1 units short of the wall (half way back if the move is that short)
        coord3d diff;
        f32 factor;

        diff.x = targetpos->x - obj->runtime_pos.x;
        diff.y = targetpos->y - obj->runtime_pos.y;
        diff.z = targetpos->z - obj->runtime_pos.z;

        f32 dist = sqrtf((diff.z * diff.z) + ((diff.x * diff.x) + (diff.y * diff.y)));

        if (0.1f < dist)
        {
            factor = 0.1f / dist;
        }
        else
        {
            factor = 0.5f;
        }

        hitpos->x -= factor * diff.x;
        hitpos->y -= factor * diff.y;
        hitpos->z -= factor * diff.z;

        endpos = *hitpos;
    }

    // Then props, up to the wall
    if (!projectileFindHitProp(prop, &obj->runtime_pos, &endpos, CDTYPE_ALL & ~CDTYPE_BG, hitpos, hitnormal, roomNums))
    {
        if (result == MOVE_BLOCKED && canEmbed)
        {
            result = MOVE_STUCK;

            if (!moveOnlyIfClear)
            {
                bgFindRoomsAlongSegment(&obj->runtime_pos, hitpos, projectile->rooms, roomSet, roomNums, &roomCount, 20);
            }
        }
        else if (result == MOVE_CLEAR || !moveOnlyIfClear)
        {
            if (result != MOVE_CLEAR)
            {
                bgFindRoomsAlongSegment(&obj->runtime_pos, &endpos, projectile->rooms, roomSet, roomNums, &roomCount, 20);
            }

            obj->runtime_pos.x = endpos.x;
            obj->runtime_pos.z = endpos.z;
            y = endpos.y;
            prop->pos.y = y;
            obj->runtime_pos.y = y;
        }
    }
    else if (!moveOnlyIfClear)
    {
        // Hit a prop: only the height changes
        endpos.x = obj->runtime_pos.x;
        endpos.z = obj->runtime_pos.z;

        bgFindRoomsAlongSegment(&obj->runtime_pos, &endpos, projectile->rooms, roomSet, roomNums, &roomCount, 20);

        y = endpos.y;
        prop->pos.y = y;
        obj->runtime_pos.y = y;

        result = canEmbed ? MOVE_STUCK : MOVE_BLOCKED;
    }

    if (result != MOVE_CLEAR && moveOnlyIfClear)
    {
        return result;
    }

    // The floor tile below it: walk there, or look for one if it has left the floor plan
    if (!(projectile->flags & PROJECTILEFLAG_OFFGRID))
    {
        StandTile *tile = prop->stan;

        if (stanWalkLine(&tile, prop->pos.x, prop->pos.z, obj->runtime_pos.x, obj->runtime_pos.z) == TRUE && tile != NULL)
        {
            prop->stan = tile;
            prop->pos.x = obj->runtime_pos.x;
            prop->pos.z = obj->runtime_pos.z;
        }
        else
        {
            projectile->flags |= PROJECTILEFLAG_OFFGRID;
            prop->flags |= PROPFLAG_00000008;
        }
    }

    if (projectile->flags & PROJECTILEFLAG_OFFGRID)
    {
        StandTile *tile = stanFindTileBelowPos(&obj->runtime_pos, roomSet, NULL);

        if (tile != NULL)
        {
            prop->stan = tile;
            prop->pos.x = obj->runtime_pos.x;
            prop->pos.z = obj->runtime_pos.z;
            projectile->flags &= ~PROJECTILEFLAG_OFFGRID;
            prop->flags &= ~PROPFLAG_00000008;
        }
    }

    s32 i;

    for (i = 0; i < 7 && roomSet[i] != 0xff; i++)
    {
        projectile->rooms[i] = roomSet[i];
    }

    projectile->rooms[i] = 0xff;

    return result;
}

/**
 * Moves an object towards targetpos along the floor plan, as a circle of objGetWidth(),
 * colliding with walls and props. If something is in the way, it goes as far as it can
 * (stopping a little short), and returns MOVE_BLOCKED with hitpos and hitnormal set (the
 * normal is flat). Either way it takes the target height.
 */
static s32 objMoveAlongFloor(ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal)
{
    const u32 cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
    PropRecord *prop = obj->prop;
    StandTile *stan = prop->stan;
    f32 width = objGetWidth(obj);
    coord3d target;
    f32 ymax;
    f32 ymin;
    f32 y;

    s_HitProp = NULL;

    if (targetpos->f[0] == prop->pos.f[0] && targetpos->f[1] == prop->pos.f[1] && targetpos->f[2] == prop->pos.f[2])
    {
        return MOVE_CLEAR;
    }

    target = *targetpos;

    if (!(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
    {
        return MOVE_CLEAR;
    }

    // The heights to test, above the floor (an empty range tests all heights)
    f32 floorY = stanGetFloorY(prop->stan, prop->pos.x, prop->pos.z);

    if (!(obj->projectile->flags & PROJECTILEFLAG_TESTHEIGHT))
    {
        ymax = 0.0f;
        ymin = 1.0f;
    }
    else if (target.f[1] < prop->pos.y)
    {
        ymax = prop->pos.y - floorY;
        ymin = target.f[1] - floorY;
    }
    else
    {
        ymax = target.f[1] - floorY;
        ymin = prop->pos.y - floorY;
    }

    stanResetHits();

    if (stanTestLineUnobstructed(&stan, prop->pos.x, prop->pos.z, target.f[0], target.f[2], cdtypes, ymax, ymin, 0.0f, 1.0f)
        && stan != NULL
        && stanTestVolume(&stan, target.f[0], target.f[2], width, cdtypes, ymax, ymin) < 0)
    {
        obj->runtime_pos.x = target.f[0];
        obj->runtime_pos.z = target.f[2];
        prop->stan = stan;
        prop->pos.x = target.f[0];
        prop->pos.z = target.f[2];
        y = target.f[1];
        prop->pos.y = y;
        obj->runtime_pos.y = y;
        return MOVE_CLEAR;
    }

    // Blocked: the normal of the edge it hit
    coord3d edgeA;
    coord3d edgeB;

    stanGetCollisionEdge(&edgeA, &edgeB);

    hitnormal->x = edgeB.z - edgeA.z;
    hitnormal->y = 0.0f;
    hitnormal->z = edgeA.x - edgeB.x;

    if (hitnormal->x != 0.0f || hitnormal->z != 0.0f)
    {
        guNormalize(&hitnormal->x, &hitnormal->y, &hitnormal->z);
    }
    else
    {
        hitnormal->z = 1.0f;
    }

    coord3d dir;

    dir.x = target.f[0] - prop->pos.x;
    dir.y = target.f[1] - prop->pos.y;
    dir.z = target.f[2] - prop->pos.z;

    chrStanLineDirIntersection(&prop->pos, &dir, hitpos);

    // Go 99% of the way to where the circle touches the edge, if that is clear
    coord2d centre = {{prop->pos.x, prop->pos.z}};
    coord2d edgeStart = {{edgeA.x, edgeA.z}};
    coord2d edgeEnd = {{edgeB.x, edgeB.z}};
    coord2d move = {{target.f[0] - prop->pos.x, target.f[2] - prop->pos.z}};
    f32 frac = geoCircleSweepFrac(width, &centre, &move, &edgeStart, &edgeEnd);
    coord3d partial;

    stan = prop->stan;

    partial.x = prop->pos.x + ((move.x * frac) * 0.99000001f);
    partial.y = target.f[1];
    partial.z = prop->pos.z + ((move.y * frac) * 0.99000001f);

    if (stanTestLineUnobstructed(&stan, prop->pos.x, prop->pos.z, partial.x, partial.z, cdtypes, ymax, ymin, 0.0f, 1.0f)
        && stan != NULL
        && stanTestVolume(&stan, partial.x, partial.z, width, cdtypes, ymax, ymin) < 0)
    {
        obj->runtime_pos.x = partial.x;
        obj->runtime_pos.z = partial.z;
        prop->stan = stan;
        prop->pos.x = partial.x;
        prop->pos.z = partial.z;
    }

    y = partial.y;
    prop->pos.y = y;
    obj->runtime_pos.y = y;

    return MOVE_BLOCKED;
}

static s32 objMove(ObjectRecord *obj, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal, bool canEmbed, bool moveOnlyIfClear)
{
    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) && (obj->projectile->flags & PROJECTILEFLAG_COLLIDE3D))
    {
        return projectileMove(obj, targetpos, hitpos, hitnormal, canEmbed, moveOnlyIfClear);
    }

    return objMoveAlongFloor(obj, targetpos, hitpos, hitnormal);
}

/** objMove, without colliding with whoever threw it. */
static s32 projectileMoveObj(ObjectRecord *obj, Projectile *projectile, coord3d *targetpos, coord3d *hitpos, coord3d *hitnormal, bool canEmbed, bool moveOnlyIfClear)
{
    if (projectile->ownerprop != NULL)
    {
        propSetMoving(projectile->ownerprop, FALSE);
    }

    s32 result = objMove(obj, targetpos, hitpos, hitnormal, canEmbed, moveOnlyIfClear);

    if (projectile->ownerprop != NULL)
    {
        propSetMoving(projectile->ownerprop, TRUE);
    }

    return result;
}

/* ---- Coming to rest ---- */

/**
 * Called when an object stops bouncing: it will turn from how it is now to lie flat
 * (keeping its heading, and staying upside down if it is), then slide to a stop. A door
 * just loses its projectile. prevXAxis is the object's x axis before this tick.
 */
static void objSettle(ObjectRecord *obj, coord3d *prevXAxis)
{
    Projectile *projectile;
    coord3d angles;
    Mtxf rotmtx;
    Mtxf restmtx;
    Mtxf scalemtx;
    f32 x;
    f32 z;

    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_NOLANDDAMAGE;

    if (!(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
    {
        return;
    }

    projectile = obj->projectile;

    if (obj->type == PROPDEF_DOOR)
    {
        projectileFree(projectile);
        obj->projectile = NULL;
        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_HASPROJECTILE;
        return;
    }

    projectile->ownerprop = NULL;
    projectile->flags &= ~PROJECTILEFLAG_AIRBORNE;
    projectile->flags &= ~PROJECTILEFLAG_COLLIDE3D;

    // Where it turns from, and its scale along each axis
    mtx4GetRotation(&obj->mtx, &angles);
    mtx4LoadRotation(&angles, &rotmtx);
    quatFromAngles(&angles, &projectile->startRot);

    mtx4TransposeRotation(&rotmtx, &restmtx);
    mtx4Mult(&restmtx, &obj->mtx, &scalemtx);

    projectile->scaleX = sqrtf(((scalemtx.m[0][0] * scalemtx.m[0][0]) + (scalemtx.m[0][1] * scalemtx.m[0][1])) + (scalemtx.m[0][2] * scalemtx.m[0][2]));
    projectile->scaleY = sqrtf(((scalemtx.m[1][0] * scalemtx.m[1][0]) + (scalemtx.m[1][1] * scalemtx.m[1][1])) + (scalemtx.m[1][2] * scalemtx.m[1][2]));
    projectile->scaleZ = sqrtf(((scalemtx.m[2][0] * scalemtx.m[2][0]) + (scalemtx.m[2][1] * scalemtx.m[2][1])) + (scalemtx.m[2][2] * scalemtx.m[2][2]));

    // Where it turns to: its x axis flattened, y straight up (or down if it is upside down)
    x = obj->mtx.m[0][0];
    z = obj->mtx.m[0][2];

    if (x != 0.0f || z != 0.0f)
    {
        f32 invlen = 1.0f / sqrtf((x * x) + (z * z));

        x *= invlen;
        z *= invlen;
    }
    else
    {
        z = 1.0f;
        x = 0.0f;
    }

    f32 up = obj->mtx.m[1][1] >= 0.0f ? 1.0f : -1.0f;

    restmtx.m[0][0] = x;
    restmtx.m[0][1] = 0.0f;
    restmtx.m[0][2] = z;
    restmtx.m[0][3] = 0.0f;
    restmtx.m[1][0] = 0.0f;
    restmtx.m[1][1] = up;
    restmtx.m[1][2] = 0.0f;
    restmtx.m[1][3] = 0.0f;
    restmtx.m[2][0] = up < 0.0f ? z : -z;
    restmtx.m[2][1] = 0.0f;
    restmtx.m[2][2] = up < 0.0f ? -x : x;
    restmtx.m[2][3] = 0.0f;
    restmtx.m[3][0] = 0.0f;
    restmtx.m[3][1] = 0.0f;
    restmtx.m[3][2] = 0.0f;
    restmtx.m[3][3] = 1.0f;

    mtx4GetRotation(&restmtx, &angles);
    quatFromAngles(&angles, &projectile->restRot);
    quatMakeShortestPath(&projectile->startRot, &projectile->restRot);

    projectile->rotFrac = 0.0f;

    // How fast it turns: a quarter turn in 20 ticks if it is still tipping the same way
    // as last tick, otherwise as fast as it was turning
    f32 angle = mathAcos(((restmtx.m[0][0] * rotmtx.m[0][0]) + (restmtx.m[0][1] * rotmtx.m[0][1])) + (restmtx.m[0][2] * rotmtx.m[0][2]));

    if (angle > 0.0f && obj->mtx.m[0][1] > 0.0f && prevXAxis->y < obj->mtx.m[0][1])
    {
        projectile->rotSpeed = 0.050000001f / ((angle * 4.0f) / M_TAU_F);
    }
    else if (angle > 0.0f && obj->mtx.m[0][1] < 0.0f && obj->mtx.m[0][1] < prevXAxis->y)
    {
        projectile->rotSpeed = 0.050000001f / ((angle * 4.0f) / M_TAU_F);
    }
    else
    {
        f32 modelscale = obj->model->scale;
        f32 turned = mathAcos((((prevXAxis->x * obj->mtx.m[0][0]) + (prevXAxis->y * obj->mtx.m[0][1])) + (obj->mtx.m[0][2] * prevXAxis->z)) / (modelscale * modelscale));

        turned = turned / g_GlobalTimerDelta;

        if (angle != 0.0f)
        {
            projectile->rotSpeed = turned / angle;
        }
        else
        {
            projectile->rotSpeed = 1.0f;
        }
    }

    if (projectile->rotSpeed < 0.0f)
    {
        projectile->rotSpeed = -projectile->rotSpeed;
    }

    if (projectile->rotSpeed < 0.029999999f)
    {
        projectile->rotSpeed = 0.029999999f;
    }
    else if (0.15000001f < projectile->rotSpeed)
    {
        projectile->rotSpeed = 0.15000001f;
    }
}

/**
 * Plays the whoosh of a throwing knife spinning through the air, while it is still
 * deadly (flying, not bounced yet). Otherwise stops it, and the knife is no longer deadly.
 */
void projectileUpdateKnifeSound(ObjectRecord *obj)
{
    static const s16 knifeSounds[] = {0x5f, 0x60, 0x61};

    if (!(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
    {
        return;
    }

    Projectile *projectile = obj->projectile;

    if ((projectile->flags & PROJECTILEFLAG_AIRBORNE) && (s32) projectile->numBounces <= 0 && (obj->runtime_bitflags & RUNTIMEBITFLAG_THROWING_KNIFE_RELATED))
    {
        s32 slot = projectile->soundSlot;
        s32 sfxindex = randomGetNext() % 3;

        // Two sounds take turns, each cut off after a few ticks
#if defined(VERSION_US)
        if ((s32) projectile->lastSfxTimer < g_GlobalTimer - 6)
#else
        if ((s32) projectile->lastSfxTimer < g_GlobalTimer - 5)
#endif
        {
            projectileStopSound(projectile->sounds[slot]);
        }

        if (projectile->sounds[slot] != NULL)
        {
            return;
        }

        if (!lvGetControlsLocked())
        {
            sndPlaySfx(g_musicSfxBufferPtr, knifeSounds[sfxindex], &projectile->sounds[slot]);
            sfxSetVolumeAt(projectile->sounds[slot], &obj->prop->pos);

            projectile->lastSfxTimer = g_GlobalTimer;
            projectile->soundSlot = 1 - slot;
        }
    }
    else
    {
        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_THROWING_KNIFE_RELATED;
        projectileStopSound(projectile->sounds[0]);
        projectileStopSound(projectile->sounds[1]);
    }
}

/** A rotation whose y axis is along `normal`. */
static void mtxAlignToNormal(coord3d *normal, Mtxf *out)
{
    f32 fwdX;
    f32 fwdY;
    f32 fwdZ;
    f32 sideX;
    f32 sideZ;
    Mtxf unyaw;
    Mtxf yawmtx;
    Mtxf pitchmtx;
    coord3d fwd;

    f32 len = sqrtf(normal->f[0] * normal->f[0] + normal->f[1] * normal->f[1] + normal->f[2] * normal->f[2]);
    f32 nx = normal->x / len;
    f32 ny = normal->y / len;
    f32 nz = normal->z / len;

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
        f32 flatLen = sqrtf(nx * nx + nz * nz);
        f32 b = nx / flatLen;

        sideX = nz / flatLen;
        sideZ = -b;

        fwdX = ny * b;
        fwdY = -flatLen;
        fwdZ = ny * sideX;
    }

    f32 yaw = mathAtan2(sideX, sideZ);

    mtx4LoadYRotation(-yaw, &unyaw);

    fwd.x = fwdX;
    fwd.y = fwdY;
    fwd.z = fwdZ;

    mtx4RotateVecInPlace(&unyaw, &fwd);

    f32 pitch = mathAtan2(fwd.x, fwd.y);

    mtx4LoadYRotation(-1.5707964f + yaw, &yawmtx);
    mtx4LoadXRotation(-1.5707964f - pitch, &pitchmtx);

    mtx4Mult(&yawmtx, &pitchmtx, out);
}

/** Stands an object on a surface at pos, with its base on it, facing out along `normal`. */
static void objPlaceOnSurface(ObjectRecord *obj, coord3d *pos, StandTile *stan, coord3d *normal)
{
    Mtxf mtx;

    mtxAlignToNormal(normal, &mtx);
    mtx4ScaleRows012(obj->model->scale, &mtx);
    objSetPosition(obj, pos, &mtx, stan);

    f32 ymin = bboxGetYmin(modelFileFindBbox(obj->model->obj));

    obj->runtime_pos.f[0] -= ymin * obj->mtx.m[1][0];
    obj->runtime_pos.f[1] -= ymin * obj->mtx.m[1][1];
    obj->runtime_pos.f[2] -= ymin * obj->mtx.m[1][2];

    objUpdateCollisionBounds(obj);
}

/**
 * Attaches an object to a node of another prop's model (it must be on screen), keeping
 * where it is in the world. Returns FALSE if there is no free embedment.
 */
static bool objEmbed(PropRecord *prop, PropRecord *parent, Model *model, ModelNode *node)
{
    if (!(parent->flags & PROPFLAG_ONSCREEN))
    {
        return FALSE;
    }

    ObjectRecord *obj = prop->obj;

    obj->embedment = embedmentAllocate();

    if (!obj->embedment)
    {
        return FALSE;
    }

    Mtxf objmtx;
    Mtxf nodeworld;
    Mtxf worldtonode;
    Mtxf *nodemtx = modelFindNodeMtx(model, node, 0);

    obj->runtime_bitflags |= RUNTIMEBITFLAG_EMBEDDED;

    propRemoveFromRooms(prop);
    propDelist(prop);
    propDisable(prop);

    obj->model->attachedto = model;
    obj->model->attachedto_objinst = node;

    propReparent(prop, parent);

    // Its matrix relative to the node
    mtx4Copy(&obj->mtx, &objmtx);
    mtx4SetTranslation(&obj->runtime_pos, &objmtx);
    mtx4MultAffine(camGetViewToWorldMtxf(), nodemtx, &nodeworld);
    mtx4InvertAffine(&nodeworld, &worldtonode);
    mtx4MultAffine(&worldtonode, &objmtx, &obj->embedment->matrix);

    return TRUE;
}

/* ---- The tick ---- */

/** Bounces the projectile's speed off a surface: speed += -(1 + bounciness) (speed . normal) normal */
static void projectileReflect(Projectile *projectile, coord3d *normal)
{
    f32 impulse = ((projectile->speed.f[0] * normal->f[0]) + (projectile->speed.f[1] * normal->f[1])) + (projectile->speed.f[2] * normal->f[2]);

    impulse *= -(projectile->bounciness + 1.0f);
    projectile->speed.f[0] += impulse * normal->f[0];
    projectile->speed.f[1] += impulse * normal->f[1];
    projectile->speed.f[2] += impulse * normal->f[2];
}

/** Frees an object's projectile. A prop off the floor plan keeps its room list. */
static void objLoseProjectile(PropRecord *prop, Projectile *projectile)
{
    ObjectRecord *obj = prop->obj;

    projectileFree(projectile);
    obj->projectile = NULL;
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_HASPROJECTILE;

    if (prop->flags & PROPFLAG_00000008)
    {
        prop->flags |= PROPFLAG_00000010;
    }
}

/**
 * Fired from a gun: first moves straight out to targetPos. A rocket that can't is set to
 * go off.
 */
static void projectileMoveToTarget(ObjectRecord *obj, Projectile *projectile)
{
    WeaponObjRecord *weapon = NULL;
    bool moveOnlyIfClear = TRUE;
    coord3d hitpos;
    coord3d hitnormal;

    if (obj->type == PROPDEF_COLLECTABLE)
    {
        weapon = (WeaponObjRecord *) obj;

        if (weapon->weaponnum == ITEM_ROCKETROUND)
        {
            moveOnlyIfClear = FALSE;
        }
    }

    s32 result = projectileMoveObj(obj, projectile, &projectile->targetPos, &hitpos, &hitnormal, FALSE, moveOnlyIfClear);

    if (result != MOVE_CLEAR && weapon != NULL && weapon->weaponnum == ITEM_ROCKETROUND)
    {
        weapon->timer = 0;
    }

    projectile->flags &= ~PROJECTILEFLAG_MOVETOTARGET;
}

/**
 * Works out where a flying object goes this tick: a rocket's rise, gravity (none under
 * thrust), and its speed; and turns it by its tumble.
 */
static void projectileFly(ObjectRecord *obj, Projectile *projectile, coord3d *newpos)
{
    if (projectile->flags & PROJECTILEFLAG_RISING)
    {
        // A rocket rises towards a height that itself rises (accelerating), then levels
        // off under thrust once it gets there
        if (projectile->riseTime < ROCKET_RISE_TIME)
        {
            projectile->riseSpeed += projectile->accel.f[1] * g_GlobalTimerDelta;
            projectile->riseY += projectile->riseSpeed * g_GlobalTimerDelta;
            projectile->riseTime += (1.0f / 90.0f) * g_GlobalTimerDelta;

            if (projectile->riseTime > ROCKET_RISE_TIME)
            {
                projectile->riseTime = ROCKET_RISE_TIME;
            }
        }
        else if (newpos->f[1] < projectile->riseY)
        {
            projectile->riseSpeed += projectile->accel.f[1] * g_GlobalTimerDelta;
            projectile->riseY += projectile->riseSpeed * g_GlobalTimerDelta;
            newpos->f[1] += (0.07f * (projectile->riseY - newpos->f[1])) * g_GlobalTimerDelta;
        }
        else
        {
            newpos->f[1] = projectile->riseY;
            projectile->flags &= ~PROJECTILEFLAG_RISING;
            projectile->riseTime = 0.0f;
            projectile->flags |= PROJECTILEFLAG_POWERED;
            projectile->speed.f[1] = projectile->riseSpeed;
        }
    }

    projectile->speed.f[1] += (projectile->accel.f[1] + projectile->riseTime) * g_GlobalTimerDelta;

    if (!(projectile->flags & PROJECTILEFLAG_POWERED))
    {
        physFall(&newpos->f[1], &projectile->speed.f[1], g_GlobalTimerDelta);
    }
    else
    {
        newpos->f[1] += projectile->speed.f[1] * g_GlobalTimerDelta;
    }

    projectile->speed.f[0] += projectile->accel.f[0] * g_GlobalTimerDelta;
    projectile->speed.f[2] += projectile->accel.f[2] * g_GlobalTimerDelta;
    newpos->f[0] += projectile->speed.f[0] * g_GlobalTimerDelta;
    newpos->f[2] += projectile->speed.f[2] * g_GlobalTimerDelta;

    physApplyTumble(&obj->mtx, &projectile->mtx, g_ClockTimer);
}

static bool weaponCanStick(ITEM_IDS weaponnum)
{
    return weaponnum == ITEM_REMOTEMINE
        || weaponnum == ITEM_TIMEDMINE
        || weaponnum == ITEM_PROXIMITYMINE
        || weaponnum == ITEM_BOMBCASE
        || weaponnum == ITEM_BUG
        || weaponnum == ITEM_MICROCAMERA
        || weaponnum == ITEM_PLASTIQUE;
}

/**
 * A mine (or the like) that hit something sticks to it: it loses its projectile, stands
 * on the surface, and if it hit a prop, is attached to it. Not to guards, players or
 * other flying things. Returns TRUE if it was attached to a prop.
 */
static bool projectileTryStick(PropRecord *prop, Projectile *projectile, coord3d *hitpos, coord3d *hitnormal)
{
    ObjectRecord *obj = prop->obj;
    PropRecord *hitprop = s_HitProp;

    if (hitprop != NULL
        && (hitprop->type == PROP_TYPE_CHR || hitprop->type == PROP_TYPE_VIEWER || (hitprop->obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)))
    {
        return FALSE;
    }

    objLoseProjectile(prop, projectile);

    sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, ATTACH_MINE_SFX, NULL), &prop->pos);
    objectivesCheckDeposit(((WeaponObjRecord *) obj)->weaponnum, prop->stan->room);
    objPlaceOnSurface(obj, hitpos, prop->stan, hitnormal);

    if (s_HitProp != NULL)
    {
        StandTile *stan = prop->stan;

        if (objEmbed(prop, s_HitProp, s_HitModel, s_HitNode))
        {
            prop->stan = stan;
            return TRUE;
        }
    }

    return FALSE;
}

/** Leaves a blood stain on the body part a knife hit, half way in from where it hit. */
static void projectileKnifeBloodStain(coord3d *hitpos)
{
    Mtxf *nodemtx = modelFindNodeMtx(s_HitModel, s_HitNode, 0);
    Mtxf worldtonode;
    coord3d pos = *hitpos;

    mtx4TransformVecInPlace(camGetWorldToScreenMtxf(), &pos);

    pos.f[0] += (pos.f[0] - nodemtx->m[3][0]) * 0.5f;
    pos.f[1] += (pos.f[1] - nodemtx->m[3][1]) * 0.5f;
    pos.f[2] += (pos.f[2] - nodemtx->m[3][2]) * 0.5f;

    pos.f[0] -= (modelGetNodeRadius(s_HitModel, s_HitNode) * 0.5f) * s_HitScreenDir.f[0];
    pos.f[1] -= (modelGetNodeRadius(s_HitModel, s_HitNode) * 0.5f) * s_HitScreenDir.f[1];
    pos.f[2] -= (modelGetNodeRadius(s_HitModel, s_HitNode) * 0.5f) * s_HitScreenDir.f[2];

    mtx4InvertRigid(nodemtx, &worldtonode);
    mtx4TransformVecInPlace(&worldtonode, &pos);
    chrCreateBloodStain(s_HitModel, s_HitPart, s_HitNode, &pos);
}

/**
 * A throwing knife hit a guard, or another player: while it is still deadly, it shoots
 * them, bounces off and leaves a blood stain. Returns TRUE if it hurt them.
 */
static bool projectileKnifeHit(ObjectRecord *obj, Projectile *projectile, coord3d *hitpos, coord3d *hitnormal)
{
    PropRecord *hitprop = s_HitProp;
    ITEM_IDS weaponnum = ((WeaponObjRecord *) obj)->weaponnum;

    if (!(hitprop->type == PROP_TYPE_CHR || (hitprop->type == PROP_TYPE_VIEWER && hitprop->obj != NULL && playerGetIndexForProp(hitprop) != playerGetCurrentNum())))
    {
        return FALSE;
    }

    if (!(projectile->flags & PROJECTILEFLAG_AIRBORNE)
        || (s32) projectile->numBounces > 0
        || !(obj->runtime_bitflags & RUNTIMEBITFLAG_THROWING_KNIFE_RELATED)
        || !chrTakeShot(hitprop->chr, s_HitPart, &s_HitDir, weaponnum, TRUE))
    {
        return FALSE;
    }

    if (projectile->bounciness > 0.0f)
    {
        projectileReflect(projectile, hitnormal);
    }

    if (!(projectile->flags & PROJECTILEFLAG_NOTUMBLE))
    {
        physRandomTumble(&projectile->mtx);
    }

    projectile->numBounces++;
    shotPlayHitPropSound(weaponnum, s_HitProp, -1);

    if ((s_HitProp->flags & PROPFLAG_ONSCREEN) && s_HitPart != HIT_GUN && s_HitPart != HIT_HAT)
    {
        projectileKnifeBloodStain(hitpos);
    }

    return TRUE;
}

/** A rocket that hit a prop damages it, and is set to go off. */
static void projectileRocketHit(ObjectRecord *obj)
{
    PropRecord *hitprop = s_HitProp;

    if (hitprop->type == PROP_TYPE_CHR)
    {
        chrExplosionDamage(hitprop->chr, &obj->runtime_pos, 2.0f, 1);
    }
    else if (hitprop->type == PROP_TYPE_OBJ || hitprop->type == PROP_TYPE_WEAPON)
    {
        objApplyDamage(hitprop->obj, 100.0f, &obj->runtime_pos, ITEM_ROCKETROUND, (obj->runtime_bitflags & RUNTIMEBITFLAG_OWNER) >> RUNTIMEBITSHIFT_OWNER);
    }

    ((WeaponObjRecord *) obj)->timer = 0;
}

/**
 * After a flying object's move: bounces it off what it hit, and off the floor if it has
 * gone through it, settling it when it is slow. Returns whether it hit the floor, and in
 * *bounced whether it bounced up off something.
 */
static bool projectileBounce(PropRecord *prop, Projectile *projectile, s32 moveResult, f32 startY, coord3d *prevXAxis, coord3d *hitpos, coord3d *hitnormal, bool *bounced)
{
    ObjectRecord *obj = prop->obj;
    ModelRoData_BoundingBoxRecord *bbox = objFindBbox(obj);
    f32 floorY = 0.0f;
    f32 bottom = 1.0f;
    bool hitFloor = FALSE;

    *bounced = FALSE;

    if (moveResult == MOVE_BLOCKED && projectile->bounciness > 0.0f)
    {
        f32 prevSpeedY = projectile->speed.f[1];

        projectileReflect(projectile, hitnormal);

        // It was falling, and now goes up (or at least isn't below where it started)
        if (prevSpeedY <= 0.0f && (projectile->speed.f[1] >= 0.0f || startY <= obj->runtime_pos.f[1]))
        {
            *bounced = TRUE;
        }
    }

    if (!(projectile->flags & PROJECTILEFLAG_OFFGRID))
    {
        floorY = stanGetFloorY(prop->stan, prop->pos.f[0], prop->pos.f[2]);
        bottom = bboxRotatedMinY(bbox, &obj->mtx);
        hitFloor = prop->pos.f[1] < floorY - bottom;
    }

    if (hitFloor || moveResult == MOVE_BLOCKED)
    {
        if (!(projectile->flags & PROJECTILEFLAG_NOTUMBLE))
        {
            physRandomTumble(&projectile->mtx);
        }

        projectile->numBounces++;
    }

    if (hitFloor || *bounced)
    {
        // Rest on what it bounced off
        f32 y = ((hitFloor ? floorY : hitpos->f[1]) - bottom) + FLOOR_CLEARANCE;

        prop->pos.f[1] = y;
        obj->runtime_pos.f[1] = y;

        if (!(obj->runtime_bitflags & RUNTIMEBITFLAG_NOLANDDAMAGE))
        {
            obj->runtime_bitflags |= RUNTIMEBITFLAG_LANDDAMAGE;
        }

        if (projectile->bounciness > 0.0f)
        {
            projectile->speed.f[1] *= -projectile->bounciness;

            if (projectile->speed.f[1] < MIN_BOUNCE_SPEED)
            {
                if ((projectile->flags & PROJECTILEFLAG_BOUNCEONCE) && projectile->numBounces == 1)
                {
                    projectile->speed.f[1] = MIN_BOUNCE_SPEED;
                }
                else
                {
                    objSettle(obj, prevXAxis);
                }
            }
        }
        else
        {
            objSettle(obj, prevXAxis);
        }
    }

    return hitFloor;
}

/**
 * A flying weapon's own effects after its move: the knife's whoosh, a rocket's thrust and
 * smoke, a grenade round's smoke (and going off when they hit), and the sound of hitting
 * a wall or the floor.
 */
static void projectileWeaponEffects(PropRecord *prop, Projectile *projectile, s32 moveResult, bool hitFloor, bool bounced)
{
    WeaponObjRecord *weapon = (WeaponObjRecord *) prop->obj;

    if (weapon->weaponnum == ITEM_THROWKNIFE)
    {
        projectileUpdateKnifeSound((ObjectRecord *) weapon);
    }
    else if (weapon->weaponnum == ITEM_ROCKETROUND)
    {
        if (moveResult == MOVE_BLOCKED || hitFloor)
        {
            weapon->timer = 0;
        }
        else
        {
            f32 speedSq = ((projectile->speed.f[0] * projectile->speed.f[0]) + (projectile->speed.f[1] * projectile->speed.f[1])) + (projectile->speed.f[2] * projectile->speed.f[2]);

            if (speedSq > ROCKET_TOP_SPEED_SQ)
            {
                projectile->accel.f[0] = 0.0f;
                projectile->accel.f[1] = 0.0f;
                projectile->accel.f[2] = 0.0f;
            }

            if ((s32) projectile->airTime >= ROCKET_FUEL_TICKS)
            {
                // Out of fuel: falls
                projectile->riseTime = 0.0f;
                projectile->flags &= ~(PROJECTILEFLAG_POWERED | PROJECTILEFLAG_RISING);
            }
            else
            {
                explosionCreateSmoke(&weapon->runtime_pos, prop->stan, 8, prop->rooms, (prop->flags & PROPFLAG_00000008) != 0);
            }
        }
    }
    else if (weapon->weaponnum == ITEM_GRENADEROUND)
    {
        if (hitFloor || bounced)
        {
            weapon->timer = 0;
        }
        else
        {
            explosionCreateSmoke(&weapon->runtime_pos, prop->stan, 9, prop->rooms, (prop->flags & PROPFLAG_00000008) != 0);
        }
    }

    if (moveResult == MOVE_BLOCKED || hitFloor)
    {
        // At most one hit sound every 3 ticks
        if ((s32) projectile->lastHitSoundTick < (s32) g_LvTickCount - 2)
        {
            ALSoundState *sound;

            if (weapon->weaponnum == ITEM_THROWKNIFE || weapon->weaponnum == ITEM_KNIFE)
            {
                sound = sndPlaySfx(g_musicSfxBufferPtr, KNIFE_HIT_WALL_SFX, NULL);
            }
            else
            {
                sound = sndPlaySfx(g_musicSfxBufferPtr, DROP_GUN_SFX, NULL);
            }

            sfxSetVolumeAt(sound, &prop->pos);
        }

        projectile->lastHitSoundTick = g_LvTickCount;
    }
}

/** An object launched by an AI script trails smoke from far below it (it is high up). */
static void projectileSmokeTrail(ObjectRecord *obj)
{
    PropRecord *prop = obj->prop;
    coord3d pos;

    pos.f[0] = obj->runtime_pos.f[0] + 400.0f;
    pos.f[1] = obj->runtime_pos.f[1] - 1800.0f;
    pos.f[2] = obj->runtime_pos.f[2];

    if (!(g_LvTickCount & 0xf))
    {
        pos.f[2] += 400.0f;
    }
    else
    {
        pos.f[2] -= 400.0f;
    }

    explosionCreate(NULL, &pos, prop->stan, 0x14, 0, 0, prop->rooms, 0);

    if ((s32) g_LvTickCount % 40 == 0)
    {
        explosionCreateSmoke(&pos, prop->stan, 10, prop->rooms, 1);
    }
}

/** One tick of a flying object. */
static TICKOP projectileTickAirborne(PropRecord *prop, Projectile *projectile, coord3d *newpos)
{
    ObjectRecord *obj = prop->obj;
    bool isWeapon = obj->type == PROPDEF_COLLECTABLE;
    ITEM_IDS weaponnum = isWeapon ? ((WeaponObjRecord *) obj)->weaponnum : ITEM_UNARMED;
    f32 startY = obj->runtime_pos.f[1];
    coord3d prevXAxis;
    coord3d hitpos = {{0.0f, 0.0f, 0.0f}};
    coord3d hitnormal = {{0.0f, 0.0f, 0.0f}};
    TICKOP tickop = TICKOP_NONE;
    bool stopped = FALSE;

    projectile->airTime += g_ClockTimer;

    prevXAxis.f[0] = obj->mtx.m[0][0];
    prevXAxis.f[1] = obj->mtx.m[0][1];
    prevXAxis.f[2] = obj->mtx.m[0][2];

    projectileFly(obj, projectile, newpos);

    s32 moveResult = projectileMoveObj(obj, projectile, newpos, &hitpos, &hitnormal, isWeapon && weaponCanStick(weaponnum), FALSE);

    if (moveResult == MOVE_STUCK && projectileTryStick(prop, projectile, &hitpos, &hitnormal))
    {
        // Now attached to the prop: it moves with it
        tickop = TICKOP_CHANGEDLIST;
        stopped = TRUE;
    }

    if (!stopped && s_HitProp != NULL && isWeapon)
    {
        if (weaponnum == ITEM_THROWKNIFE)
        {
            stopped = projectileKnifeHit(obj, projectile, &hitpos, &hitnormal);
        }
        else if (weaponnum == ITEM_ROCKETROUND)
        {
            projectileRocketHit(obj);
            stopped = TRUE;
        }
    }

    // A mine that stuck to a wall carries on here with its freed projectile, as it did
    // originally (it can still bounce off the floor)
    if (!stopped)
    {
        bool bounced;
        bool hitFloor = projectileBounce(prop, projectile, moveResult, startY, &prevXAxis, &hitpos, &hitnormal, &bounced);

        if (isWeapon)
        {
            projectileWeaponEffects(prop, projectile, moveResult, hitFloor, bounced);
        }
    }

    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) && (projectile->flags & PROJECTILEFLAG_SMOKETRAIL) && !(g_LvTickCount & 7))
    {
        projectileSmokeTrail(obj);
    }

    return tickop;
}

/**
 * One tick of a landed object: it turns to lie flat, and slides (with friction once it
 * is flat). When it has stopped, or is off the floor plan, its projectile is freed.
 */
static void projectileTickLanded(PropRecord *prop, Projectile *projectile, coord3d *newpos, bool *moved)
{
    ObjectRecord *obj = prop->obj;
    bool done = TRUE;

    if (projectile->rotFrac < 1.0f)
    {
        quatf rot;

        projectile->rotFrac += projectile->rotSpeed * g_GlobalTimerDelta;

        if (g_ClockTimer > 0)
        {
            projectile->rotSpeed *= 1.1f;
        }

        if (projectile->rotFrac > 1.0f || (projectile->flags & PROJECTILEFLAG_OFFGRID))
        {
            projectile->rotFrac = 1.0f;
        }

        quatSlerp(&projectile->startRot, &projectile->restRot, projectile->rotFrac, &rot);
        quatToMtx4(&rot, &obj->mtx);
        mtx4ScaleRow0(projectile->scaleX, &obj->mtx);
        mtx4ScaleRow1(projectile->scaleY, &obj->mtx);
        mtx4ScaleRow2(projectile->scaleZ, &obj->mtx);
        done = FALSE;
    }

    if ((projectile->speed.f[0] != 0.0f || projectile->speed.f[2] != 0.0f || projectile->rotFrac < 1.0f) && !(projectile->flags & PROJECTILEFLAG_OFFGRID))
    {
        ModelRoData_BoundingBoxRecord *bbox = objFindBbox(obj);
        coord3d hitpos;
        coord3d hitnormal;

        done = FALSE;

        for (s32 i = 0; i < g_ClockTimer; i++)
        {
            newpos->f[0] += projectile->speed.f[0];
            newpos->f[2] += projectile->speed.f[2];

            if (projectile->rotFrac >= 1.0f)
            {
                if (projectile->groundFriction > 0.0f)
                {
                    f32 friction = (projectile->groundFriction * g_GlobalTimerDelta) / sqrtf((projectile->speed.f[0] * projectile->speed.f[0]) + (projectile->speed.f[2] * projectile->speed.f[2]));

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
                    projectile->speed.f[0] *= SLIDE_FRICTION;
                    projectile->speed.f[2] *= SLIDE_FRICTION;
                }
            }
        }

        objMove(obj, newpos, &hitpos, &hitnormal, FALSE, FALSE);
        *moved = TRUE;

        f32 y = (stanGetFloorY(prop->stan, prop->pos.f[0], prop->pos.f[2]) - bboxRotatedMinY(bbox, &obj->mtx)) + FLOOR_CLEARANCE;

        prop->pos.f[1] = y;
        obj->runtime_pos.f[1] = y;

        if (projectile->speed.f[0] < 0.1f && projectile->speed.f[0] > -0.1f
            && projectile->speed.f[2] < 0.1f && projectile->speed.f[2] > -0.1f)
        {
            projectile->speed.f[2] = 0.0f;
            projectile->speed.f[0] = 0.0f;
        }
    }

    if (done || (projectile->flags & PROJECTILEFLAG_OFFGRID))
    {
        objLoseProjectile(prop, projectile);

        if (obj->type == PROPDEF_COLLECTABLE)
        {
            objectivesCheckDeposit(((WeaponObjRecord *) obj)->weaponnum, prop->stan->room);
        }
    }
}

/**
 * One tick of an object with a projectile (from objTick, once per frame). Sets *moved if
 * it moved. Returns TICKOP_CHANGEDLIST if it stuck to another prop.
 */
TICKOP projectileTick(PropRecord *prop, bool *moved)
{
    ObjectRecord *obj = prop->obj;
    Projectile *projectile = obj->projectile;
    coord3d newpos;

    projectile->age += g_ClockTimer;

    if ((s32) projectile->age > PROJECTILE_LIFETIME_TICKS)
    {
        obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
    }

    if (projectile->flags & PROJECTILEFLAG_MOVETOTARGET)
    {
        projectileMoveToTarget(obj, projectile);
    }

    newpos = obj->runtime_pos;

    if (projectile->refreshrate > 0)
    {
        projectile->refreshrate -= g_ClockTimer;
    }

    if (projectile->flags & PROJECTILEFLAG_AIRBORNE)
    {
        *moved = TRUE;
        return projectileTickAirborne(prop, projectile, &newpos);
    }

    projectileTickLanded(prop, projectile, &newpos, moved);
    return TICKOP_NONE;
}
