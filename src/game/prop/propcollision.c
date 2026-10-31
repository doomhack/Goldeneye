/**
 * Collision between props, seen from above.
 *
 * Props collide on the x/z plane, each with an outline and a height range:
 *
 * - an object or door uses a convex polygon (its turned bounding box, see bboxGetHull2d),
 *   worked out when it is placed or moves (objUpdateCollisionBounds);
 * - guards and players are circles (propGetCollisionRadius) around their position.
 *
 * propIsOfCdType says whether a prop counts for a collision test's CDTYPE flags.
 * propSetMoving takes a prop out of collision tests for a moment, so that a test from
 * a prop's own position (a line of sight from a guard) does not hit itself.
 */

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/propcollision.h"
#include "game/prop/bbox.h"
#include "game/prop/prop.h"
#include "game/prop/proprooms.h"
#include "game/chr/chr.h"
#include "game/math/matrix.h"
#include "game/nav/geometry.h"
#include "game/player/player.h"

/* Aircraft collide from this far below their bounding box */
#define AIRCRAFT_COLLISION_DROP 200.0f


/**
 * Work out an object's collision outline and height range from its bounding box, its
 * matrix and its position.
 */
void objUpdateCollisionBounds(ObjectRecord *obj)
{
    collision_data *col = obj->ptr_allocated_collisiondata_block;

    if (col == NULL)
    {
        return;
    }

    ModelRoData_BoundingBoxRecord *bbox = objFindBbox(obj);
    Mtxf mtx;

    mtx4Copy(&obj->mtx, &mtx);
    mtx4SetTranslation(&obj->runtime_pos, &mtx);
    col->edges = bboxGetHull2d(bbox, &mtx, col->polygon);

    col->bottom = obj->runtime_pos.f[1] + bboxRotatedMinY(bbox, &mtx);
    col->top = obj->runtime_pos.f[1] + bboxRotatedMaxY(bbox, &mtx);

    if (obj->type == PROPDEF_AIRCRAFT)
    {
        col->bottom -= AIRCRAFT_COLLISION_DROP;
    }
}


/**
 * An object's collision outline and height range. No outline (edges 0) when it has no
 * collision data, does not collide (no PROPFLAG_00000100), or is taken out of collision
 * tests for now (PROPSTATE_NOCOLLISION).
 */
void objGetCollisionBounds(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom)
{
    ObjectRecord *obj = prop->obj;
    collision_data *col = obj->ptr_allocated_collisiondata_block;

    if (col != NULL && (obj->flags & PROPFLAG_00000100) && !(obj->state & PROPSTATE_NOCOLLISION))
    {
        *edges = col->edges;
        *polygon = col->polygon;
        *bottom = col->bottom;
        *top = col->top;
        return;
    }

    *edges = 0;
}


/* Take an object out of collision tests (collidable FALSE) or put it back */
void objSetMoving(PropRecord *prop, bool collidable)
{
    ObjectRecord *obj = prop->obj;

    if (collidable)
    {
        obj->state &= ~PROPSTATE_NOCOLLISION;
    }
    else
    {
        obj->state |= PROPSTATE_NOCOLLISION;
    }
}


/**
 * A prop's collision outline and height range (top and bottom are only set when there
 * is an outline). Guards and players without a polygon have no edges here; they collide
 * as circles (propGetCollisionRadius).
 */
void propGetCollisionPolygon(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom)
{
    *polygon = NULL;
    *edges = 0;

    if (prop->type == PROP_TYPE_CHR)
    {
        chrUpdateCollisionBounds(prop, polygon, edges, top, bottom);
    }
    else if (prop->type == PROP_TYPE_VIEWER)
    {
        playerGetPropBounds(prop, polygon, edges, top, bottom);
    }
    else if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_DOOR)
    {
        objGetCollisionBounds(prop, polygon, edges, top, bottom);
    }
}


void propGetCollisionPolygon2d(PropRecord *prop, coord2d **polygon, s32 *edges)
{
    f32 top;
    f32 bottom;

    propGetCollisionPolygon(prop, polygon, edges, &top, &bottom);
}


/**
 * A guard's or player's collision circle: radius, and the top and bottom of its height
 * range above the ground (propGetGroundY). Other props have radius 0.
 */
void propGetCollisionRadius(PropRecord *prop, f32 *radius, f32 *height, f32 *ymin)
{
    if (prop->type == PROP_TYPE_CHR)
    {
        chrGetChrWidthHeight(prop, radius, height, ymin);
        return;
    }

    if (prop->type == PROP_TYPE_VIEWER)
    {
        playerGetCollisionSize(prop, radius, height, ymin);
        return;
    }

    *radius = 0.0f;
}


/* The height of the ground under a guard or player, and 0 for other props */
f32 propGetGroundY(PropRecord *prop)
{
    if (prop->type == PROP_TYPE_CHR)
    {
        return chrGetChrGround(prop);
    }

    if (prop->type == PROP_TYPE_VIEWER)
    {
        return playerGetStanHeight(g_playerPointers[playerGetIndexForProp(prop)]);
    }

    return 0.0f;
}


/* Take a prop out of collision tests (collidable FALSE) or put it back */
void propSetMoving(PropRecord *prop, bool collidable)
{
    if (prop->type == PROP_TYPE_CHR)
    {
        chrSetMoving(prop->chr, collidable);
    }
    else if (prop->type == PROP_TYPE_VIEWER)
    {
        playerSetMoving(prop, collidable);
    }
    else if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_DOOR || prop->type == PROP_TYPE_WEAPON)
    {
        objSetMoving(prop, collidable);
    }
}


/* The CDTYPE a door counts as: closed, open or ajar, and whether guards can't use it */
s32 propDoorGetCdTypes(PropRecord *prop)
{
    DoorRecord *door = prop->door;
    s32 cdtypes;

    if (door->openPosition <= 0.0f)
    {
        cdtypes = CDTYPE_CLOSEDDOORS;
    }
    else if (door->maxFrac <= door->openPosition)
    {
        cdtypes = CDTYPE_OPENDOORS;
    }
    else
    {
        cdtypes = CDTYPE_AJARDOORS;
    }

    if (door->flags2 & PROPFLAG2_20000000)
    {
        cdtypes |= CDTYPE_DOORSLOCKEDTOAI;
    }

    return cdtypes;
}


/**
 * Whether a prop counts for a collision test that looks for cdtypes.
 *
 * Doors count for CDTYPE_DOORS, or for the door states asked for. Players count for
 * CDTYPE_PLAYERS and guards for CDTYPE_CHRS. Objects count for CDTYPE_OBJS, or
 * CDTYPE_PATHBLOCKER for free-standing glass (PROPFLAG_00000800). With CDTYPE_AIOPAQUE,
 * doors and objects guards can see through (PROPFLAG_04000000) don't count. With
 * CDTYPE_OBJSIMMUNETOEXPLOSIONS only invincible objects count.
 */
bool propIsOfCdType(PropRecord *prop, s32 cdtypes)
{
    bool result = TRUE;
    ObjectRecord *obj;

    if (prop->type == PROP_TYPE_DOOR)
    {
        obj = prop->obj;

        if ((cdtypes & CDTYPE_AIOPAQUE) && (obj->flags & PROPFLAG_04000000))
        {
            result = FALSE;
        }

        if (!(cdtypes & CDTYPE_DOORS) && !(propDoorGetCdTypes(prop) & cdtypes))
        {
            result = FALSE;
        }
    }
    else if (prop->type == PROP_TYPE_VIEWER)
    {
        if (!(cdtypes & CDTYPE_PLAYERS))
        {
            result = FALSE;
        }
    }
    else if (prop->type == PROP_TYPE_CHR)
    {
        if (!(cdtypes & CDTYPE_CHRS))
        {
            result = FALSE;
        }
    }
    else
    {
        obj = prop->obj;

        if ((cdtypes & CDTYPE_AIOPAQUE) && (obj->flags & PROPFLAG_04000000))
        {
            result = FALSE;
        }

        if ((cdtypes & CDTYPE_OBJSIMMUNETOEXPLOSIONS) && !(obj->flags & PROPFLAG_INVINCIBLE))
        {
            result = FALSE;
        }

        if (obj->flags & PROPFLAG_00000800)
        {
            if (!(cdtypes & CDTYPE_PATHBLOCKER))
            {
                result = FALSE;
            }
        }
        else if (!(cdtypes & CDTYPE_OBJS))
        {
            result = FALSE;
        }
    }

    return result;
}


/**
 * Whether a point (x and z) is inside a convex polygon: on the same side of every edge.
 * Edges the point lies on are skipped. A polygon with no edges contains nothing.
 */
bool polygonContainsPoint(coord3d *point, coord2d *polygon, s32 edges)
{
    s32 side = -1;

    if (edges <= 0)
    {
        return FALSE;
    }

    for (s32 i = 0; i < edges; i++)
    {
        coord2d *a = &polygon[i];
        coord2d *b = &polygon[(i + 1) % edges];
        f32 cross = ((b->f[1] - a->f[1]) * (point->f[0] - a->f[0])) - ((b->f[0] - a->f[0]) * (point->f[2] - a->f[1]));

        if (cross == 0.0f)
        {
            continue;
        }

        if (i == 0 || side < 0)
        {
            side = cross > 0.0f;
            continue;
        }

        if ((side && cross < 0.0f) || (!side && cross > 0.0f))
        {
            return FALSE;
        }
    }

    return TRUE;
}


/**
 * Whether an edge of poly1 separates it from poly2 (separating axis test): all of poly2
 * is beyond the edge, on the other side from the rest of poly1. A zero-length edge is
 * tested as a point instead. Uses doubles.
 */
static bool polygonExcludesPolygon(coord2d *poly1, s32 edges1, coord2d *poly2, s32 edges2)
{
    f64 other = 0.0;

    for (s32 i = 0; i < edges1; i++)
    {
        s32 next = (i + 1) % edges1;
        f64 nx = poly1[next].y - (f64)poly1[i].y;
        f64 nz = poly1[i].x - (f64)poly1[next].x;

        if (nx == 0.0f && nz == 0.0f)
        {
            coord3d point;

            point.x = poly1[i].x;
            point.y = 0.0f;
            point.z = poly1[i].y;

            if (polygonContainsPoint(&point, poly2, edges2))
            {
                return FALSE;
            }

            continue;
        }

        // The edge's line, and poly1's first point off it
        f64 line = poly1[i].x * nx + poly1[i].y * nz;
        s32 j = (next + 1) % edges1;

        while (j != i)
        {
            other = poly1[j].x * nx + poly1[j].y * nz;

            if (other != line)
            {
                break;
            }

            j = (j + 1) % edges1;
        }

        s32 k;

        for (k = 0; k < edges2; k++)
        {
            f64 value = poly2[k].x * nx + poly2[k].y * nz;

            // All of poly1 is on the line: use the mirror of poly2's point
            if (other == line)
            {
                other = line - value + line;
            }

            if (value < line && other < line)
            {
                break;
            }

            if (value > line && other > line)
            {
                break;
            }
        }

        if (k == edges2)
        {
            return TRUE;
        }
    }

    return FALSE;
}


/**
 * Whether two convex polygons touch or overlap: a corner of one is inside the other, or
 * no edge of either separates them.
 */
bool polygonsOverlap(coord2d *poly1, s32 edges1, coord2d *poly2, s32 edges2)
{
    coord3d point;

    for (s32 i = 0; i < edges1; i++)
    {
        point.f[0] = poly1[i].f[0];
        point.f[1] = 0.0f;
        point.f[2] = poly1[i].f[1];

        if (polygonContainsPoint(&point, poly2, edges2))
        {
            return TRUE;
        }
    }

    for (s32 i = 0; i < edges2; i++)
    {
        point.f[0] = poly2[i].f[0];
        point.f[1] = 0.0f;
        point.f[2] = poly2[i].f[1];

        if (polygonContainsPoint(&point, poly1, edges1))
        {
            return TRUE;
        }
    }

    if (polygonExcludesPolygon(poly1, edges1, poly2, edges2))
    {
        return FALSE;
    }

    if (polygonExcludesPolygon(poly2, edges2, poly1, edges1))
    {
        return FALSE;
    }

    return TRUE;
}


/**
 * Whether a circle (centre x and z of point) touches an edge of a polygon: it is within
 * radius of the edge's line, and of one of its ends or alongside the edge.
 */
bool polygonTouchesCircle(coord3d *point, f32 radius, coord2d *polygon, s32 edges)
{
    f32 px = point->f[0];
    f32 pz = point->f[2];

    for (s32 i = 0; i < edges; i++)
    {
        coord2d *a = &polygon[i];
        coord2d *b = &polygon[(i + 1) % edges];
        f32 linedist = geoSignedLineDist(a->f[0], a->f[1], b->f[0], b->f[1], px, pz);

        if (linedist < 0.0f)
        {
            linedist = -linedist;
        }

        f32 dista = geoDist2d(a->f[0], a->f[1], px, pz);
        f32 distb = geoDist2d(b->f[0], b->f[1], px, pz);

        if (linedist < radius
            && (dista < radius || distb < radius || geoProjectsOntoSegment(a->f[0], a->f[1], b->f[0], b->f[1], px, pz)))
        {
            return TRUE;
        }
    }

    return FALSE;
}


/* Whether a guard or player stands in the outline (with the height range) */
static bool chrBlocksOutline(PropRecord *prop, PropRecord *mover, coord2d *outline, s32 edges, f32 top, f32 bottom)
{
    ChrRecord *chr = prop->chr;
    f32 radius;
    f32 height;
    f32 ymin;

    if (chr != NULL && (chr->chrflags & CHRFLAG_HIDDEN))
    {
        return FALSE;
    }

    propGetCollisionRadius(prop, &radius, &height, &ymin);

    f32 ground = propGetGroundY(prop);

    ymin += ground;
    height += ground;

    if (!(ymin <= top) || !(bottom <= height))
    {
        return FALSE;
    }

    if (!polygonContainsPoint(&prop->pos, outline, edges) && !polygonTouchesCircle(&prop->pos, radius, outline, edges))
    {
        return FALSE;
    }

    // A door that would close on a guard: the guard moves on
    if (prop->type == PROP_TYPE_CHR && mover->type == PROP_TYPE_DOOR)
    {
        prop->chr->hidden |= CHRHIDDEN_OFFSCREEN_PATROL;
    }

    return TRUE;
}


/**
 * Whether nothing is in the way of a moving door or vehicle (mover) where it is now:
 * no guard or player in its outline, and no other object's outline overlapping it. A
 * door ignores other doors, safes and aircraft. A guard in a closing door's way is told
 * to move on.
 */
bool propIsUnblocked(PropRecord *mover)
{
    coord2d *outline;
    s32 edges;
    f32 top;
    f32 bottom;
    s32 rooms[8];

    propGetCollisionPolygon(mover, &outline, &edges, &top, &bottom);

    if (edges <= 0)
    {
        return TRUE;
    }

    propGetRoomIds(mover, rooms);
    roomGetProps(rooms);

    for (s16 *propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
    {
        PropRecord *prop = &g_Props[*propnum];

        if (prop == mover)
        {
            continue;
        }

        if (prop->type == PROP_TYPE_VIEWER || prop->type == PROP_TYPE_CHR)
        {
            if (chrBlocksOutline(prop, mover, outline, edges, top, bottom))
            {
                return FALSE;
            }
        }
        else if ((prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_DOOR)
                 && (mover->type != PROP_TYPE_DOOR
                     || (prop->type != PROP_TYPE_DOOR && prop->obj->type != PROPDEF_SAFE && prop->obj->type != PROPDEF_AIRCRAFT)))
        {
            coord2d *polygon;
            s32 numedges;
            f32 objtop;
            f32 objbottom;

            propGetCollisionPolygon(prop, &polygon, &numedges, &objtop, &objbottom);

            if (numedges > 0 && objbottom <= top && bottom <= objtop && polygonsOverlap(polygon, numedges, outline, edges))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}
