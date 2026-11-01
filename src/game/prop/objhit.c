/**
 * Shots and rays against objects' models: which triangle of a model a ray hits (also used
 * for thrown things and guards' held guns), testing the player's shots against objects,
 * and what a hit does (sparks, impact marks, sounds, damage, knocking light things about).
 */

#include <ultra64.h>
#include <bondgame.h>
#include <gbi_extension.h>
#include "game/gfx/gfxdl.h"
#include "game/prop/objhit.h"
#include "game/prop/objdamage.h"
#include "game/prop/propobj.h"
#include "game/prop/prop.h"
#include "game/prop/projectile.h"
#include "game/prop/door.h"
#include "game/prop/explosion.h"
#include "game/prop/glass.h"
#include "game/hud/sparks.h"
#include "game/math/line_tri_intersect.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/chrobjdata.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/world/bg.h"
#include "game/nav/stan.h"
#include "game/world/surface.h"
#include "game/gfx/texcmds.h"
#include "random.h"

/* ---- Ray against a model's triangles ---- */

/**
 * Tests a ray against one triangle (vertex indexes into vtxbase) of a display list. If it
 * hits closer than *bestDistSq, records the hit, with the texture last set before the
 * triangle command.
 */
static bool objTestTriangle(coord3d *rayStart, coord3d *rayEnd, coord3d *rayDir, Vertex *vtxbase, s32 idx[3], Gfx *tricmd, s32 triNum, Gfx *cmdStart, f32 *bestDistSq, HitThing *hitthing)
{
    BoundVec bboxMin = {0x7fff, 0x7fff, 0x7fff};
    BoundVec bboxMax = {-0x8000, -0x8000, -0x8000};
    HitThing hit;

    for (s32 i = 0; i < 3; i++)
    {
        Vertex *v = &vtxbase[idx[i]];

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

    if (!bgTestRayIntersectsBbox(rayStart, rayDir, (s32 *) &bboxMin, (s32 *) &bboxMax))
    {
        return FALSE;
    }

    coord3d zero = {{0.0f, 0.0f, 0.0f}};

    if (!intersectRayTriangle(&vtxbase[idx[0]].coord, &vtxbase[idx[1]].coord, &vtxbase[idx[2]].coord, &zero, rayStart, rayEnd, rayDir, &hit.hitpos, &hit.normal))
    {
        return FALSE;
    }

    f32 dx = (f32) ((s32) hit.hitpos.x - (s32) rayStart->x);
    f32 dy = (f32) ((s32) hit.hitpos.y - (s32) rayStart->y);
    f32 dz = (f32) ((s32) hit.hitpos.z - (s32) rayStart->z);

    s32 texnum = gfxFindTextureNum(tricmd, cmdStart);

    f32 distSq = ((dx * dx) + (dy * dy)) + (dz * dz);

    if (!(distSq < *bestDistSq))
    {
        return FALSE;
    }

    *bestDistSq = distSq;
    hitthing->hitpos = hit.hitpos;
    hitthing->normal = hit.normal;
    hitthing->vtx0 = &vtxbase[idx[0]];
    hitthing->vtx1 = &vtxbase[idx[1]];
    hitthing->vtx2 = &vtxbase[idx[2]];
    hitthing->texturenum = texnum;
    hitthing->tricmd = tricmd;
    hitthing->triNum = triNum;
    return TRUE;
}

/**
 * Tests a ray (in model space) against the triangles of a display list and the one it
 * continues into (gdl2, may be NULL), using `vertices` for its G_VTX loads. Fills in the
 * nearest hit.
 */
static bool objTestDisplayList(coord3d *rayStart, coord3d *rayEnd, coord3d *rayDir, Gfx *gdl, Gfx *gdl2, Vertex *vertices, HitThing *hitthing)
{
    Vertex *vtxbase = NULL;
    Gfx *cmdStart = gdl;
    f32 bestDistSq = M_U32_MAX_VALUE_F;
    bool found = FALSE;

    while (TRUE)
    {
        if (gfxIsEnd(gdl))
        {
            if (gdl2 == NULL)
            {
                break;
            }

            cmdStart = gdl = gdl2;
            gdl2 = NULL;
            continue;
        }

        if (gfxIsVtx(gdl))
        {
            // The vertices are loaded from the address's offset into the buffer at the first index
            vtxbase = (Vertex *) (((uintptr_t) vertices + (gfxVtxAddr(gdl) & 0x00ffffff)) - (gfxVtxFirst(gdl) << 4));
            gdl++;
            continue;
        }

        for (s32 tri = gfxFirstTri(gdl); tri < gfxFirstTri(gdl) + gfxTriCount(gdl); tri++)
        {
            s32 idx[3];

            gfxGetTri(gdl, tri, idx);

            if (objTestTriangle(rayStart, rayEnd, rayDir, vtxbase, idx, gdl, tri, cmdStart, &bestDistSq, hitthing))
            {
                found = TRUE;
            }
        }

        gdl++;
    }

    return found;
}

/**
 * Tests a ray (in screen space) against the triangles of a model, from startNode down.
 * Stops at the first display list that is hit (not necessarily the nearest), and gives
 * the hit (in the node's model space), the node's matrix index and the node.
 */
bool modelFindTriangleHit(Model *model, ModelNode *startNode, coord3d *rayPos, coord3d *rayDir, HitThing *hitthing, s32 *dstmtxindex, ModelNode **dstnode)
{
    coord3d localStart;
    coord3d localEnd;
    coord3d localDir;
    Mtxf *lastmtx = NULL;
    Vertex *vertices = NULL;
    ModelNode *node = startNode;
    bool done = FALSE;

    while (node && !done)
    {
        Gfx *gdl = NULL;
        Gfx *gdl2 = NULL;

        switch (node->Opcode & 0xff)
        {
            case MODELNODE_OPCODE_DLCOLLISION:
            {
                ModelRoData_DisplayList_CollisionRecord *rodata = &node->Data->DisplayListCollisions;
                ModelRwData_DisplayList_CollisionRecord *rwdata = &modelGetNodeRwData(model, node)->DisplayListCollisions;

                if (rwdata->gdl != NULL)
                {
                    if (rwdata->gdl == rodata->Primary)
                    {
                        gdl = (Gfx *) ((uintptr_t) rodata->BaseAddr + ((u32) rodata->Primary & 0xffffff));
                    }
                    else
                    {
                        gdl = rwdata->gdl;
                    }

                    if (rodata->Secondary != NULL)
                    {
                        gdl2 = (Gfx *) ((uintptr_t) rodata->BaseAddr + ((u32) rodata->Secondary & 0xffffff));
                    }

                    vertices = rwdata->Vertices;
                }
                break;
            }
            case MODELNODE_OPCODE_DL:
            {
                ModelRoData_DisplayListRecord *rodata = &node->Data->DisplayList;

                if (rodata->Primary != NULL)
                {
                    gdl = (Gfx *) ((uintptr_t) rodata->BaseAddr + ((u32) rodata->Primary & 0xffffff));

                    if (rodata->Secondary != NULL)
                    {
                        gdl2 = (Gfx *) ((uintptr_t) rodata->BaseAddr + ((u32) rodata->Secondary & 0xffffff));
                    }

                    vertices = (Vertex *) (uintptr_t) rodata->BaseAddr;
                }
                break;
            }
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
            Mtxf *mtx = mtxindex >= 0 ? &model->render_pos[mtxindex].pos : NULL;

            // The ray in the node's space (worked out again only when the matrix changes)
            if (mtx && mtx != lastmtx)
            {
                Mtxf invmtx;

                lastmtx = mtx;
                mtx4InvertAffine(mtx, &invmtx);

                localStart = *rayPos;
                mtx4TransformVecInPlace(&invmtx, &localStart);

                localDir = *rayDir;
                mtx4RotateVecInPlace(&invmtx, &localDir);

                localEnd.x = localDir.x * 32767.0f + localStart.x;
                localEnd.y = localDir.y * 32767.0f + localStart.y;
                localEnd.z = localDir.z * 32767.0f + localStart.z;
            }

            if (objTestDisplayList(&localStart, &localEnd, &localDir, gdl, gdl2, vertices, hitthing))
            {
                *dstmtxindex = mtxindex;
                *dstnode = node;
                done = TRUE;
            }
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
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

/* ---- The player's shots ---- */

/**
 * Tests a shot against an object's model, and those attached to it (they are tested
 * first). Adds a hit to the shot if it is within range. Glass and the window of a door
 * stop the shot.
 */
static void objTestShotModel(PropRecord *prop, ShotData *shot)
{
    ObjectRecord *obj = prop->obj;

    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_WRECKED) || !(prop->flags & PROPFLAG_ONSCREEN))
    {
        return;
    }

    PropRecord *child = propChild(prop);

    while (child != NULL)
    {
        PropRecord *next = child->prev;

        objTestShotModel(child, shot);
        child = next;
    }

    Model *model = obj->model;
    ModelNode *node = NULL;
    ModelNode *hitnode;
    HitThing hit;
    s32 mtxindex;
    s32 hitpart;

    if (obj->type == PROPDEF_DOOR
        && ((((DoorRecord *) obj)->doorFlags & DOORFLAG_CLIP_TO_BBOX) || ((DoorRecord *) obj)->doorType == DOORTYPE_EYE || ((DoorRecord *) obj)->doorType == DOORTYPE_IRIS))
    {
        // These doors are tested against their bounding box first
        hitpart = modelTestRayIntersectsTransformedBBox(&((DoorRecord *) obj)->bbox, &model->render_pos->pos, &shot->viewOrigin, &shot->viewDir);
        node = model->obj->RootNode;

        if (hitpart > 0 && !modelFindTriangleHit(model, node, &shot->viewOrigin, &shot->viewDir, &hit, &mtxindex, &hitnode))
        {
            hitpart = 0;
        }
    }
    else
    {
        do
        {
            hitpart = modelFindRayHitNext(model, &shot->viewOrigin, &shot->viewDir, &node);
        } while (hitpart > 0 && !modelFindTriangleHit(model, node, &shot->viewOrigin, &shot->viewDir, &hit, &mtxindex, &hitnode));
    }

    if (hitpart <= 0)
    {
        return;
    }

    coord3d pos = hit.hitpos;

    mtx4TransformVecInPlace(&model->render_pos[mtxindex].pos, &pos);

    if (-pos.z <= shot->maxdist)
    {
        bool penetrates = TRUE;

        // Original: tests a flags2 bit in flags (PROPFLAG_INVINCIBLE)
        if (!(obj->flags & PROPFLAG2_00020000))
        {
            if (obj->type == PROPDEF_GLASS || obj->type == PROPDEF_TINTED_GLASS)
            {
                penetrates = FALSE;
            }
            else if (obj->model->obj->Skeleton == &skeleton_door && hitnode == obj->model->obj->Switches[3])
            {
                penetrates = FALSE;
            }
        }

        shotAddHit(shot, prop, -pos.z, hitpart, node, &hit, mtxindex, hitnode, model, penetrates, (obj->flags2 & PROPFLAG2_BULLETPROOFGLASS) != 0);
    }
}

/** Tests the player's shot against an object, if it is on screen and in range. */
void objTestShot(PropRecord *prop, ShotData *shot)
{
    ObjectRecord *obj = prop->obj;
    Model *model = obj->model;
    ModelRoData_BoundingBoxRecord *bbox = objFindBbox(obj);

    if ((prop->flags & PROPFLAG_ONSCREEN)
        && !(obj->runtime_bitflags & RUNTIMEBITFLAG_WRECKED)
        && !(obj->flags2 & PROPFLAG2_SHOOTTHROUGH))
    {
        // The distance to its nearest point
        f32 nearest = -(model->render_pos->pos.m[3][2] + bboxRotatedMaxZ(bbox, &model->render_pos->pos));

        if (nearest <= shot->maxdist)
        {
            objTestShotModel(prop, shot);
        }
    }
}

/**
 * Knocks a shot object into the air, away from the player (who is then its owner): a
 * random hop and spin, plus the direction of the shot.
 */
static void objBounce(ObjectRecord *obj, coord3d *shotDir)
{
    coord3d rot = {{0, 0, 0}};
    Projectile *projectile = NULL;
    coord3d dir;

    objEnsureProjectile(obj->prop);

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED)
    {
        projectile = obj->embedment->projectile;
    }
    else if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
    {
        projectile = obj->projectile;
    }

    if (projectile == NULL)
    {
        return;
    }

    projectile->speed.x = (RANDOMFRAC() * 1.6666666f * 4.0f) - 3.3333333f;
    projectile->speed.y = (RANDOMFRAC() * 1.6666666f * 2.0f) + 3.3333333f;
    projectile->speed.z = (RANDOMFRAC() * 1.6666666f * 4.0f) - 3.3333333f;

#if defined(VERSION_EU)
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

    dir = *shotDir;
    mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &dir);

    projectile->speed.x += 3.3333333f * dir.x;
    projectile->speed.z += 3.3333333f * dir.z;
    projectile->ownerprop = currentPlayerGetProp();
    projectile->numBounces = 1;
}

/** A shot hits a CCTV camera's lens: it breaks into shards and is hidden. */
static void objBreakCCTVGlass(ObjectRecord *obj)
{
    PropRecord *prop = obj->prop;
    Model *model = obj->model;

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        // Switch 2's data holds the lens's extent
        f32 *bounds = (f32 *) model->obj->Switches[2]->Data;
        Mtxf *nodemtx = modelFindNodeMtx(model, model->obj->Switches[1], 0);
        Mtxf lensmtx;

        mtx4MultAffine(camGetViewToWorldMtxf(), nodemtx, &lensmtx);

        glassCreateShards((coord3d *) lensmtx.m[3], (coord3d *) lensmtx.m[0], (coord3d *) lensmtx.m[1], (coord3d *) lensmtx.m[2], bounds[1], bounds[2], bounds[3], bounds[4]);
    }

    impactsRemoveFromProp(prop, TRUE);

    // Hide the lens
    *(s32 *) modelGetNodeRwData(model, model->obj->Switches[3]) = 0;
}

/** Whether a shot knocks an object about. */
static bool objBouncesWhenShot(ObjectRecord *obj)
{
    bool bounce = FALSE;

    if (objIsCollectable((PropDefHeaderRecord *) obj))
    {
        bounce = !(obj->flags & PROPFLAG_NOBOUNCEWHENSHOT);
    }
    else if (obj->flags & PROPFLAG_BOUNCEWHENSHOT)
    {
        bounce = TRUE;
    }

    if ((obj->flags2 & PROPFLAG2_LIGHTWEIGHT) && !objIsHealthy(obj))
    {
        bounce = TRUE;
    }

    if (obj->flags2 & PROPFLAG2_LINKEDTOSAFE)
    {
        bounce = FALSE;
    }

    return bounce;
}

/** Leaves a bullet hole or impact mark where a shot hit an object. */
static void objHitImpact(ObjectRecord *obj, BulletHit *hit)
{
    s8 clearWithRoom = 0;

    if (!hit->countsAsPenetration)
    {
        // A hit that didn't go in (glass): a crack
        if (obj->model->obj->Skeleton == &skeleton_door)
        {
            clearWithRoom = 1;
        }

        s16 impact = (randomGetNext() % 3) + 0x11;

        impactCreate(&hit->hit.hitpos, &hit->hit.normal, impact, 1, hit->prop, hit->room, clearWithRoom);
        return;
    }

    s16 texturenum = hit->hit.texturenum;
    const SurfaceHit *surface = texturenum < 0 ? &g_SurfaceHits[HIT_DEFAULT] : &g_SurfaceHits[g_Textures[texturenum].hitTexture];
    s32 impact = randomGetNext() % surface->numImpacts;

    // A door's window and a camera's lens
    if ((obj->model->obj->Skeleton == &skeleton_door && hit->bboxNode == obj->model->obj->Switches[3])
        || (obj->model->obj->Skeleton == &skeleton_cctv && hit->bboxNode == obj->model->obj->Switches[1]))
    {
        clearWithRoom = 1;
    }

    impactCreate(&hit->hit.hitpos, &hit->hit.normal, surface->impacts[impact], 1, hit->prop, hit->room, clearWithRoom);
}

/**
 * One of the player's shots hits an object: sparks, the tracer, the hit sound and mark,
 * damage (double to drone guns on Agent, a hundred times to a camera's lens), a door
 * window cracking, and light objects jumping.
 */
void objHit(ShotData *shot, BulletHit *hit)
{
    ObjectRecord *obj = hit->prop->obj;
    PropRecord *rootprop = hit->prop;
    coord3d pos;

    while (propParent(rootprop) != NULL)
    {
        rootprop = propParent(rootprop);
    }

    // Where it hit, 26 units back towards the player, in the world
    pos.x = shot->viewOrigin.x - ((hit->dist * shot->viewDir.x) / shot->viewDir.z);
    pos.y = shot->viewOrigin.y - ((hit->dist * shot->viewDir.y) / shot->viewDir.z);
    pos.z = shot->viewOrigin.z - hit->dist;

    pos.x -= 26.0f * shot->viewDir.x;
    pos.y -= 26.0f * shot->viewDir.y;
    pos.z -= 26.0f * shot->viewDir.z;

    mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &pos);

    if (hit->countsAsPenetration)
    {
        gunSetTracerTarget(&pos);
    }

    sparkCreate(&pos, 1, 26.0f, rootprop->stan->room);

    if (objIsHealthy(obj) && objIsMortal(obj) && hit->countsAsPenetration)
    {
        statsAddHit(shot->weapon, SHOT_REGISTER_OBJECT);
    }

    if (!hit->countsAsPenetration)
    {
        shotPlayGlassSound(&hit->prop->pos);
    }
    else
    {
        shotPlayHitPropSound(shot->weapon, hit->prop, hit->hit.texturenum);
    }

    if (shot->weapon != ITEM_WATCHLASER)
    {
        objHitImpact(obj, hit);
    }

    f32 damage = weaponGetDamage(shot->weapon);

    if (obj->type == PROPDEF_AUTOGUN)
    {
        damage *= g_AutogunDamageScale;
    }
    else if (obj->type == PROPDEF_CCTV)
    {
        if (obj->model->obj->Skeleton == &skeleton_cctv && hit->bboxNode == obj->model->obj->Switches[1])
        {
            damage *= 100.0f;
            objBreakCCTVGlass(obj);
        }

        damage *= g_CctvDamageScale;
    }

    objApplyShotDamage(obj, damage, &pos, shot->weapon, playerGetCurrentNum());

    // Three hits crack a door's window
    if (obj->model->obj->Skeleton == &skeleton_door && !hit->countsAsPenetration)
    {
        ((DoorRecord *) obj)->numHits++;

        if (((DoorRecord *) obj)->numHits >= 3)
        {
            doorShatterGlass((DoorRecord *) obj);
        }
    }

    objDropRecursively(hit->prop);

    if (objBouncesWhenShot(obj))
    {
        objBounce(obj, &shot->viewDir);
    }
}
