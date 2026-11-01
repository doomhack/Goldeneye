/**
 * Bullets hitting guards: chrTestHit finds which body part a shot passes through,
 * chrHandleBulletHit applies it, and the hit makes smoke puffs (chrCreateHitPuffs) and a
 * blood stain on the body (chrCreateBloodStain).
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <macro.h>
#include <random.h>
#include <snd.h>
#include <math.h>
#include "bondaicommands.h"
#include "game/world/bg.h"
#include "game/menu/cheat.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/chraction.h"
#include "game/model/chrobjdata.h"
#include "game/gfx/dyn.h"
#include "game/gfx/vtxstore.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/save/file.h"
#include "game/prop/propobj.h"
#include "game/prop/explosion.h"
#include "game/player/gun.h"
#include "game/model/anim.h"
#include "joy.h"
#include "game/level/lv.h"
#include "game/text/language.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/player/player.h"
#include "game/nav/stan.h"
#include "game/world/surface.h"
#include "game/math/physics.h"
#include "game/world/bgfog.h"
#include "game/math/line_tri_intersect.h"


static void chrCreateHitPuffs(PropRecord *prop, s32 hitpart, coord3d *vec, coord3d *pos);


/**
 * Creates a smoke puff at the front of the character when the character is shot,
 * and also has a ~50% chance of creating a second smoke puff just behind the character.
 */
static void chrCreateHitPuffs(PropRecord *prop, s32 hitpart, coord3d *vec, coord3d *pos)
{
    s32 i;
    f32 scale;
    coord3d puffPos;
    s32 index;
    struct ChrHitReaction *entry;

    index = 0;

    for (i = 0; g_HitReactionTable[i].hitpart != -1; i++)
    {
        if (hitpart == g_HitReactionTable[i].hitpart)
        {
            index = i;
            break;
        }
    }

    entry = &g_HitReactionTable[index];

    if (entry->backImpactPuffCount)
    {
        // True when randomGetNext() bit 2 is 0, so roughly 50% chance.
        if ((randomGetNext() & 4) == 0)
        {
            scale = (42.0f / sqrtf(vec->z * vec->z + (vec->x * vec->x + vec->y * vec->y))) + 1.0f;

            puffPos.x = vec->x * scale;
            puffPos.y = vec->y * scale;
            puffPos.z = vec->z * scale;

            mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &puffPos);

            sparkCreate(&puffPos, entry->backImpactPuffCount, entry->backImpactPuffSize, prop->stan->room);
        }
    }

    if (entry->impactPuffCount)
    {
        sparkCreate(pos, entry->impactPuffCount, entry->impactPuffSize, prop->stan->room);
    }
}


/**
 * Paints blood onto the body near a hit: finds the collision vertex of the model's
 * display lists closest to pos (in the hit node's space), gives the node (and the
 * neighbouring node sharing that vertex) its own copy of its vertices, and sets the
 * alpha of every vertex that uses that point. Head and chest hits bleed more.
 */
void chrCreateBloodStain(Model *model, s32 hitpart, ModelNode *root, coord3d *pos)
{
    s32 target[3];
    s32 bestdist;
    ModelNode *bestnode;
    s32 bestindex;
    ModelNode *node;
    s32 opcode;
    ModelRoData_DisplayList_CollisionRecord *rodata;
    ModelRwData_DisplayList_CollisionRecord *rwdata;
    ModelRoData_DisplayList_CollisionRecord *relatedrodata;
    ModelRwData_DisplayList_CollisionRecord *relatedrwdata;
    ModelNode *relatednode;
    Vertex *vtx;
    Vertex *newvertices;
    s32 n;
    s32 paintval;
    s32 relatedindex;

    target[0] = pos->x;
    target[1] = pos->y;
    target[2] = pos->z;

    bestnode = NULL;
    bestindex = 0;
    bestdist = 0x7fffffff;
    node = root;

    while (node != NULL)
    {
        opcode = node->Opcode & 0xff;

        switch (opcode)
        {
            case MODELNODE_OPCODE_LOD:
                modelApplyDistanceRelations(model, node);
                break;

            case MODELNODE_OPCODE_SWITCH:
                modelApplyToggleRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;

            case MODELNODE_OPCODE_DLCOLLISION:
                rodata = &node->Data->DisplayListCollisions;
                vtx = rodata->CollisionVertices;

                for (n = 0; n < rodata->numCollisionVertices; n++, vtx++)
                {
                    s32 dx = target[0] - vtx->coord.x;
                    s32 dy = target[1] - vtx->coord.y;
                    s32 dz = target[2] - vtx->coord.z;
                    s32 dist = (dx * dx) + (dy * dy) + (dz * dz);

                    if (dist < bestdist)
                    {
                        bestdist = dist;
                        bestnode = node;
                        bestindex = n;
                    }
                }
                break;
        }

        if ((node->Child != NULL) && ((node == root) || ((opcode != MODELNODE_OPCODE_BBOX) && (opcode != MODELNODE_OPCODE_OP17))))
        {
            node = node->Child;
        }
        else
        {
            while (node != NULL)
            {
                if (node == root)
                {
                    node = NULL;
                    break;
                }

                if (node->Next != NULL)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }

    if (bestnode == NULL)
    {
        return;
    }

    rodata = &bestnode->Data->DisplayListCollisions;
    rwdata = &modelGetNodeRwData(model, bestnode)->DisplayListCollisions;
    relatedrodata = NULL;
    relatedrwdata = NULL;
    relatedindex = 0;
    paintval = (randomGetNext() % 50) + 20;

    if (hitpart == HIT_HEAD)
    {
        paintval += 100;
    }

    if (hitpart == HIT_CHEST)
    {
        paintval += 50;
    }

    relatednode = rodata->CollisionVertices[bestindex].CollisionRelatedNode;

    if (relatednode != NULL)
    {
        relatedrodata = &relatednode->Data->DisplayListCollisions;
        relatedrwdata = &modelGetNodeRwData(model, relatednode)->DisplayListCollisions;
        relatedindex = rodata->CollisionVertices[bestindex].CollisionRelatedIndex;
    }

    // Until it is first painted a node draws its read-only vertices: copy them.
    if (rwdata->Vertices == rodata->Vertices)
    {
        newvertices = vtxstoreAllocate(rodata->numVertices, VTXSTORE_CHR, NULL, 0);

        if (newvertices != NULL)
        {
            s32 j;

            rwdata->Vertices = newvertices;

            for (j = 0; j < rodata->numVertices; j++)
            {
                newvertices[j] = rodata->Vertices[j];
            }
        }
    }

    if ((relatedrwdata != NULL) && (relatedrwdata->Vertices == relatedrodata->Vertices))
    {
        newvertices = vtxstoreAllocate(relatedrodata->numVertices, VTXSTORE_CHR, NULL, 0);

        if (newvertices != NULL)
        {
            s32 j;

            relatedrwdata->Vertices = newvertices;

            for (j = 0; j < relatedrodata->numVertices; j++)
            {
                newvertices[j] = relatedrodata->Vertices[j];
            }
        }
    }

    // PointUsage chains every vertex that shares a collision point.
    if (rwdata->Vertices != rodata->Vertices)
    {
        s32 index = rodata->CollisionVertices[bestindex].index;

        while (index >= 0)
        {
            rwdata->Vertices[index].a = paintval;
            index = rodata->PointUsage[index];
        }
    }

    if ((relatedrwdata != NULL) && (relatedrwdata->Vertices != relatedrodata->Vertices))
    {
        s32 index = relatedrodata->CollisionVertices[relatedindex].index;

        while (index >= 0)
        {
            relatedrwdata->Vertices[index].a = paintval;
            index = relatedrodata->PointUsage[index];
        }
    }
}


/**
 * Tests a shot's ray against a character.
 *
 * First tests the shot against the chr's bounding sphere expanded by the
 * largest held weapon's model size. On a bounds hit, the model hit list is
 * walked for a body part. A confirmed body hit is registered via shotAddHit;
 * a near miss inside max range flags CHRFLAG_NEAR_MISS and bumps numclosearghs.
 *
 * PD: chr_test_hit
 */
void chrTestHit(PropRecord *prop, ShotData *shotdata)
{
    ChrRecord *chr;
    Model *model;
    f32 modelsize;
    s32 hitpart;
    Model *hitmodel;
    ModelNode *hitnode;
    s32 hitbounds;
    struct HitThing hitthing;
    s32 mtxindex;
    ModelNode *dlnode;
    Mtxf *submatrix;
    f32 heldmodelsize;
    Mtxf *nodemtx;
    f32 len;
    struct WeaponObjRecord *weapon;
    f32 dist;
    JointListEntry *entry;
    coord3d viewdir;
    f32 size;
    s32 i;
    coord3d diff;

    chr = prop->chr;

    if (chr->actiontype == ACT_DEAD)
    {
        return;
    }

    model = chr->model;
    modelsize = modelGetRadius(model);

    if ((prop->flags & PROPFLAG_ONSCREEN) == FALSE)
    {
        return;
    }

    hitpart = HIT_NULL_PART;

    if (!((prop->zDepth - modelsize) < shotdata->maxdist))
    {
        return;
    }

    hitmodel = NULL;
    hitnode = NULL;
    hitbounds = FALSE;
    dlnode = NULL;
    submatrix = modelGetRootMtx(model);
    heldmodelsize = 0.0f;

    for (i = 0; i < 2; i++)
    {
        if (chr->weapons_held[i] != NULL)
        {
            weapon = chr->weapons_held[i]->weapon;
            size = model->scale * modelGetRadius(weapon->model);

            if (heldmodelsize < size)
            {
                heldmodelsize = size;
            }
        }
    }

    modelsize += heldmodelsize;

    if (intersectRaySphere(&shotdata->viewOrigin, &shotdata->viewDir, (coord3d *) &submatrix->m[3][0], modelsize))
    {
        hitbounds = TRUE;
        hitpart = HIT_LEFT_FOOT;
    }

    if (hitpart != HIT_NULL_PART)
    {
        entry = chr->jointList;
        hitpart = jointListFindHit(&entry, &shotdata->viewOrigin, &shotdata->viewDir, &hitmodel, &hitnode);

        while ((hitpart == HIT_GUN) || (hitpart == HIT_HAT))
        {
            if (modelFindTriangleHit(hitmodel, hitnode, &shotdata->viewOrigin, &shotdata->viewDir, &hitthing, &mtxindex, &dlnode))
            {
                break;
            }

            hitpart = jointListFindHitNext(&entry, &shotdata->viewOrigin, &shotdata->viewDir, &hitmodel, &hitnode);
        }
    }

    if (hitpart > HIT_NULL_PART)
    {
        viewdir.x = shotdata->viewDir.x;
        viewdir.y = shotdata->viewDir.y;
        viewdir.z = shotdata->viewDir.z;

        submatrix = camGetViewToWorldMtxf();
        mtx4RotateVecInPlace(submatrix, &viewdir);
        nodemtx = modelFindNodeMtx(hitmodel, hitnode, 0);

        diff.x = nodemtx->m[3][0] - shotdata->viewOrigin.x;
        diff.y = nodemtx->m[3][1] - shotdata->viewOrigin.y;
        diff.z = nodemtx->m[3][2] - shotdata->viewOrigin.z;

        len = sqrtf((diff.z * diff.z) + ((diff.x * diff.x) + (diff.y * diff.y)));
        dist = -((shotdata->viewDir.z * len) + shotdata->viewOrigin.z);

        if (dist < shotdata->maxdist)
        {
            shotAddHit(shotdata, prop, dist, hitpart, hitnode, &hitthing, mtxindex, dlnode, hitmodel, TRUE, FALSE);
        }
    }

    if (hitbounds && (prop->zDepth <= shotdata->maxdist))
    {
        chr->chrflags |= CHRFLAG_NEAR_MISS;
        chr->numclosearghs++;
    }
}


/**
 * Resolves a known hit against a character.
 */
void chrHandleBulletHit(struct ShotData *shot, struct BulletHit *bhit)
{
    f32 scale;
    Mtxf invmtx;
    coord3d nearhitpos;
    coord3d hitpos;
    Mtxf *mtx;
    ChrRecord *chr;
    PropRecord *prop;
    const SurfaceHit *surface;
    WeaponObjRecord *weaponobj;
    s32 i;
    coord3d jointpos;

    chr = bhit->prop->chr;

    // Calculate the view space hit position for impact effects.
    hitpos.f[0] = shot->viewOrigin.x - ((bhit->dist * shot->viewDir.x) / shot->viewDir.z);
    hitpos.f[1] = shot->viewOrigin.y - ((bhit->dist * shot->viewDir.y) / shot->viewDir.z);
    hitpos.f[2] = shot->viewOrigin.z - bhit->dist;

    // A point 42 units nearer the camera, so the tracer and smoke puffs don't land
    // inside the character model.
    scale = 1.0f - (42.0f / sqrtf(SQ(hitpos.f[0]) + SQ(hitpos.f[1]) + SQ(hitpos.f[2])));

    nearhitpos.x = hitpos.f[0] * scale;
    nearhitpos.y = hitpos.f[1] * scale;
    nearhitpos.z = hitpos.f[2] * scale;

    mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &nearhitpos);

    // Point the tracer effect towards the impact position.
    gunSetTracerTarget(&nearhitpos);

    // Make a fleshy impact sound.
    shotPlayHitPropSound(shot->weapon, bhit->prop, -1);

    chrCreateHitPuffs(bhit->prop, bhit->hitpart, &hitpos, &nearhitpos);

    // Apply damage to the character.
    if (!chrTakeShot(chr, bhit->hitpart, &shot->dir, shot->weapon, TRUE))
    {
        return;
    }

    if (bhit->hitpart == HIT_GUN)
    {
        for (i = 0; i != 2; i++)
        {
            prop = chr->weapons_held[i];

            if (prop == NULL)
            {
                continue;
            }

            weaponobj = prop->weapon;

            if (weaponobj->model != bhit->model)
            {
                continue;
            }

            // A held explosive that is shot goes off.
            if (weaponobj->weaponnum == ITEM_GRENADE ||
                weaponobj->weaponnum == ITEM_GRENADEROUND ||
                weaponobj->weaponnum == ITEM_ROCKETROUND ||
                weaponobj->weaponnum == ITEM_TIMEDMINE ||
                weaponobj->weaponnum == ITEM_BOMBCASE ||
                weaponobj->weaponnum == ITEM_REMOTEMINE ||
                weaponobj->weaponnum == ITEM_PROXIMITYMINE)
            {
                objSetDropType(prop, 1);
                chr->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;

                objApplyDamage(prop->obj, weaponGetDamage(shot->weapon), &hitpos, shot->weapon, playerGetCurrentNum());
            }
            else
            {
                // A bullet hole in the held weapon.
                if (bhit->hit.texturenum < 0)
                {
                    surface = &g_SurfaceHits[HIT_DEFAULT];
                }
                else
                {
                    surface = &g_SurfaceHits[g_Textures[bhit->hit.texturenum].hitTexture];
                }

                impactCreate(&bhit->hit.hitpos, &bhit->hit.normal, surface->impacts[randomGetNext() % (s16)surface->numImpacts], 1, prop, bhit->room, 0);
            }
        }

        return;
    }

    // A bullet hole in a hat or helmet.
    if (bhit->hitpart == HIT_HAT)
    {
        if (bhit->hit.texturenum < 0)
        {
            surface = &g_SurfaceHits[HIT_DEFAULT];
        }
        else
        {
            surface = &g_SurfaceHits[g_Textures[bhit->hit.texturenum].hitTexture];
        }

        impactCreate(&bhit->hit.hitpos, &bhit->hit.normal, surface->impacts[randomGetNext() % (s16)surface->numImpacts], 1, chr->handle_positiondata_hat, bhit->room, 0);

        return;
    }

    mtx = modelFindNodeMtx(bhit->model, bhit->node, 0);

    jointpos.x = hitpos.f[0];
    jointpos.y = hitpos.f[1];
    jointpos.z = hitpos.f[2];

    jointpos.x += (jointpos.x - mtx->m[3][0]) * 0.5f;
    jointpos.y += (jointpos.y - mtx->m[3][1]) * 0.5f;
    jointpos.z += (jointpos.z - mtx->m[3][2]) * 0.5f;

    jointpos.x -= modelGetNodeRadius(bhit->model, bhit->node) * 0.5f * shot->viewDir.x;
    jointpos.y -= modelGetNodeRadius(bhit->model, bhit->node) * 0.5f * shot->viewDir.y;
    jointpos.z -= modelGetNodeRadius(bhit->model, bhit->node) * 0.5f * shot->viewDir.z;

    mtx4InvertRigid(mtx, &invmtx);
    mtx4TransformVecInPlace(&invmtx, &jointpos);

    // Create a blood stain at the impact point.
    chrCreateBloodStain(bhit->model, bhit->hitpart, bhit->node, &jointpos);
}

