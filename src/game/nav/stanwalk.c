/**
 * Collision on the floor tiles. There is no collision against walls as such: everything
 * that moves keeps the tile it is standing on, and every test is a walk across the tile
 * graph from there.
 *
 * walkLine follows a line from tile to tile until it reaches its end or meets a wall edge.
 * walkCircle visits every tile a circle overlaps, spreading through the edges it touches,
 * until it touches a wall edge. The tests below are those two walks, some with the props
 * in the rooms they cross added. A hit is saved in g_StanHit for stanGetCollisionEdge.
 * See docs/subsystems/nav.md.
 */

#include <ultra64.h>
#include "game/nav/stan.h"
#include "game/nav/geometry.h"
#include "game/world/bg.h"
#include "game/chr/chr.h"
#include "game/prop/prop.h"

/* walkCircle gives up once it has found this many tiles */
#define MAX_CIRCLE_TILES 41

/* The most rooms the prop tests look in */
#define MAX_TEST_ROOMS 20

StanHit g_StanHit;

/* The ladder tile stanCircleGetSpecial found last, for stanGetLadderQuad */
static StandTile *s_LadderTile;

void stanResetHits(void)
{
    g_StanHit.tile = NULL;
    g_StanHit.edge = 0;
    g_StanHit.hasLine = FALSE;
}

static void getPointWorldPos(const StandTilePoint *point, coord3d *out)
{
    f32 scale = g_StanInvScale;

    out->x = point->x * scale;
    out->y = point->y * scale;
    out->z = point->z * scale;
}

/**
 * Gets the ends of the last wall or prop edge a test hit: a tile edge with its heights, or
 * a prop edge at y 0. Returns FALSE if there is none.
 */
bool stanGetCollisionEdge(coord3d *a, coord3d *b)
{
    if (g_StanHit.tile != NULL)
    {
        getPointWorldPos(&g_StanHit.tile->points[g_StanHit.edge], a);
        getPointWorldPos(&g_StanHit.tile->points[stanNextPoint(g_StanHit.tile, g_StanHit.edge)], b);

        return TRUE;
    }

    if (g_StanHit.hasLine)
    {
        a->x = g_StanHit.lineA.f[0];
        a->y = 0.0f;
        a->z = g_StanHit.lineA.f[1];

        b->x = g_StanHit.lineB.f[0];
        b->y = 0.0f;
        b->z = g_StanHit.lineB.f[1];

        return TRUE;
    }

    return FALSE;
}

/** The rooms a walk passes through, in order */
typedef struct RoomList
{
    s32 *rooms;
    s32 count;
    s32 max;
    s32 lastRoom;
} RoomList;

/**
 * Walks the line from (x, z) to (destX, destZ), starting on *tilePtr: through each edge it
 * crosses, until the tile it ends on (returns TRUE) or a wall (returns FALSE, the hit in
 * g_StanHit). Either way *tilePtr is the last tile reached. rooms, if given, collects the
 * rooms on the way.
 */
static bool walkLine(StandTile **tilePtr, f32 x, f32 z, f32 destX, f32 destZ, RoomList *rooms)
{
    StandTile *tile = *tilePtr;
    StandTile *prev = tile;
    StandTile *prevPrev = tile;
    StandTile *next = NULL;
    s32 hitEdge = 0;
    s32 steps = 0;
    f32 lineNegDz;
    f32 lineDx;

    x *= g_StanScale;
    z *= g_StanScale;
    destX *= g_StanScale;
    destZ *= g_StanScale;

    lineNegDz = -(destZ - z);
    lineDx = destX - x;

    while (TRUE)
    {
        s32 crossings = 0;
        s32 i;

        if (rooms != NULL && tile->room != rooms->lastRoom && rooms->count < rooms->max)
        {
            rooms->rooms[rooms->count++] = tile->room;
            rooms->lastRoom = tile->room;
        }

        for (i = 0; i < tile->numPoints; i++)
        {
            const StandTilePoint *a = &tile->points[i];
            const StandTilePoint *b = &tile->points[stanNextPoint(tile, i)];
            bool isLink = (a->link >> 4) != 0;

            /* Only the edges the line could leave by: those facing along it */
            if (!(lineNegDz * (b->x - a->x) + lineDx * (b->z - a->z) <= 0.0f))
            {
                continue;
            }

            /* A wall must be crossed outright; touching a link is enough to go through it */
            if (geoSegmentsCross(x, z, destX, destZ, a->x, a->z, b->x, b->z, isLink))
            {
                StandTile *linked = stanLinkedTile(tile, i);

                crossings++;

                /* Not back the way it came */
                if (linked != prev && linked != prevPrev)
                {
                    hitEdge = i;
                    next = linked;
                }
            }
        }

        prevPrev = prev;
        prev = tile;

        /* Crossing only edges back into the last two tiles leaves next as it was: this tile */
        if (crossings == 0 || next == tile)
        {
            return TRUE;
        }

        tile = next;

        if (steps++ > 500 || next == NULL)
        {
            g_StanHit.tile = prev;
            g_StanHit.edge = hitEdge;

            return FALSE;
        }

        *tilePtr = next;
    }
}

bool stanWalkLine(StandTile **tile, f32 x, f32 z, f32 destX, f32 destZ)
{
    return walkLine(tile, x, z, destX, destZ, NULL);
}

static bool lineRooms(StandTile **tile, f32 x, f32 z, f32 destX, f32 destZ, s32 *rooms, s32 *numRooms, s32 maxRooms)
{
    RoomList list = {.rooms = rooms, .count = 0, .max = maxRooms, .lastRoom = -1};
    bool reached = walkLine(tile, x, z, destX, destZ, &list);

    *numRooms = list.count;

    return reached;
}

/**
 * Lists the rooms between two points, for a guard walking from one to the other. If
 * *destTile is NULL, it is set to the tile the walk reaches. Returns the number of rooms,
 * or 0 if the walk hits a wall or ends on another tile than *destTile.
 */
s32 stanGetRoomsBetween(StandTile *tile, f32 x, f32 z, StandTile **destTile, f32 destX, f32 destZ, s32 *rooms, s32 maxRooms)
{
    StandTile *reached = tile;
    s32 count = 0;

    if (*destTile != NULL)
    {
        if (tile->room == (*destTile)->room)
        {
            rooms[0] = tile->room;

            return 1;
        }

        if (bgRoomsSharePortal(tile->room, (*destTile)->room))
        {
            rooms[0] = tile->room;
            rooms[1] = (*destTile)->room;

            return 2;
        }
    }

    if (!lineRooms(&reached, x, z, destX, destZ, rooms, &count, maxRooms))
    {
        return 0;
    }

    if (*destTile == NULL)
    {
        *destTile = reached;
    }

    if (reached != *destTile)
    {
        return 0;
    }

    return count;
}

/**
 * Whether a polygon lies wholly to one side of the box around the line from (x, z) to
 * (destX, destZ), so none of its edges can meet the line. Cheaper than testing each edge.
 */
static bool polygonOutsideLineBox(const coord2d *polygon, s32 numEdges, f32 x, f32 z, f32 destX, f32 destZ)
{
    f32 lineMinX = x < destX ? x : destX;
    f32 lineMaxX = x < destX ? destX : x;
    f32 lineMinZ = z < destZ ? z : destZ;
    f32 lineMaxZ = z < destZ ? destZ : z;
    bool left = TRUE, right = TRUE, below = TRUE, above = TRUE;

    for (s32 i = 0; i < numEdges; i++)
    {
        left &= polygon[i].f[0] < lineMinX;
        right &= polygon[i].f[0] > lineMaxX;
        below &= polygon[i].f[1] < lineMinZ;
        above &= polygon[i].f[1] > lineMaxZ;
    }

    return left || right || below || above;
}

/**
 * Whether the line from (x, z) to (destX, destZ) is clear of walls and of props of the
 * collision types cdtypes. *tile starts as the tile at (x, z) and ends as the last tile the
 * line reaches before any hit; g_StanHit.frac is how far along the line that is.
 *
 * With fromBottom <= fromTop, props the line passes over or under don't count: the line
 * runs from that height range to toBottom..toTop, or if toBottom > toTop, rises and falls
 * with the floor, fromTop and fromBottom above it.
 */
bool stanTestLineUnobstructed(StandTile **tilePtr, f32 x, f32 z, f32 destX, f32 destZ, s32 cdtypes, f32 fromTop, f32 fromBottom, f32 toTop, f32 toBottom)
{
    StandTile *tile = *tilePtr;
    coord2d from = {.f = {x, z}};
    coord2d to = {.f = {destX, destZ}};
    s32 rooms[MAX_TEST_ROOMS + 1];
    s32 numRooms = 0;
    f32 hitFrac = 1.0f;
    bool clear;
    bool checkHeight = fromBottom <= fromTop;
    bool slopesSet = FALSE;
    f32 topSlope = 0.0f;
    f32 bottomSlope = 0.0f;

    clear = lineRooms(&tile, x, z, destX, destZ, rooms, &numRooms, MAX_TEST_ROOMS);

    if (!clear)
    {
        StandTile *hit = g_StanHit.tile;
        const StandTilePoint *a = &hit->points[g_StanHit.edge];
        const StandTilePoint *b = &hit->points[stanNextPoint(hit, g_StanHit.edge)];

        g_StanHit.hasLine = TRUE;
        g_StanHit.lineA.f[0] = a->x * g_StanInvScale;
        g_StanHit.lineA.f[1] = a->z * g_StanInvScale;
        g_StanHit.lineB.f[0] = b->x * g_StanInvScale;
        g_StanHit.lineB.f[1] = b->z * g_StanInvScale;

        hitFrac = geoSegmentHitFrac(&from, &to, &g_StanHit.lineA, &g_StanHit.lineB);
    }

    g_StanHit.prop = NULL;

    if (cdtypes != 0)
    {
        s16 *propnum;

        rooms[numRooms] = -1;
        roomGetProps(rooms);

        for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
        {
            PropRecord *prop = &g_Props[*propnum];
            coord2d *polygon;
            s32 numEdges;
            f32 propTop;
            f32 propBottom;
            s32 i;

            if (!propIsOfCdType(prop, cdtypes))
            {
                continue;
            }

            propGetCollisionPolygon(prop, &polygon, &numEdges, &propTop, &propBottom);

            if (numEdges == 0 || polygonOutsideLineBox(polygon, numEdges, x, z, destX, destZ))
            {
                continue;
            }

            for (i = 0; i < numEdges; i++)
            {
                s32 next = (i + 1) % numEdges;
                coord2d edgeA;
                coord2d edgeB;
                f32 frac;
                bool blocked = TRUE;

                /* destX and destZ can have been moved back below, from and to not */
                if (!geoSegmentsIntersect(x, z, destX, destZ, polygon[i].f[0], polygon[i].f[1], polygon[next].f[0], polygon[next].f[1]))
                {
                    continue;
                }

                edgeA = polygon[i];
                edgeB = polygon[next];
                frac = geoSegmentHitFrac(&from, &to, &edgeA, &edgeB);

                if (!(frac < hitFrac))
                {
                    continue;
                }

                if (checkHeight)
                {
                    if (!slopesSet)
                    {
                        slopesSet = TRUE;

                        if (toBottom <= toTop)
                        {
                            topSlope = toTop - fromTop;
                            bottomSlope = toBottom - fromBottom;
                        }
                        else
                        {
                            f32 startFloorY;

                            if (hitFrac < 1.0f)
                            {
                                destX = x + (destX - x) * hitFrac;
                                destZ = z + (destZ - z) * hitFrac;
                            }

                            startFloorY = stanGetFloorY(*tilePtr, x, z);
                            fromTop += startFloorY;
                            fromBottom += startFloorY;
                            topSlope = bottomSlope = (stanGetFloorY(tile, destX, destZ) - startFloorY) / hitFrac;
                        }
                    }

                    if (propTop <= bottomSlope * frac + fromBottom || topSlope * frac + fromTop <= propBottom)
                    {
                        blocked = FALSE;
                    }
                }

                if (blocked)
                {
                    clear = FALSE;
                    hitFrac = frac;
                    g_StanHit.hasLine = TRUE;
                    g_StanHit.lineA = edgeA;
                    g_StanHit.lineB = edgeB;
                    g_StanHit.tile = NULL;
                    g_StanHit.edge = 0;
                    g_StanHit.prop = prop;
                    tile = NULL;
                }
            }
        }
    }

    /* A prop was hit: the tile is the one where the line meets it */
    if (tile == NULL)
    {
        tile = *tilePtr;
        destX = x + (destX - x) * hitFrac;
        destZ = z + (destZ - z) * hitFrac;
        stanWalkLine(&tile, x, z, destX, destZ);
    }

    *tilePtr = tile;
    g_StanHit.frac = hitFrac;

    return clear;
}

/** The nearest prop of the collision types on the line, or NULL. Walls don't stop it. */
PropRecord *stanLineFindDoor(StandTile *tile, f32 x, f32 z, f32 destX, f32 destZ, s32 cdtypes)
{
    coord2d from = {.f = {x, z}};
    coord2d to = {.f = {destX, destZ}};
    s32 rooms[MAX_TEST_ROOMS + 1];
    s32 numRooms = 0;
    PropRecord *best = NULL;
    f32 bestFrac = 1.0f;
    s16 *propnum;

    lineRooms(&tile, x, z, destX, destZ, rooms, &numRooms, MAX_TEST_ROOMS);

    if (cdtypes == 0)
    {
        return NULL;
    }

    rooms[numRooms] = -1;
    roomGetProps(rooms);

    for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
    {
        PropRecord *prop = &g_Props[*propnum];
        coord2d *polygon;
        s32 numEdges;
        s32 i;

        if (!propIsOfCdType(prop, cdtypes))
        {
            continue;
        }

        propGetCollisionPolygon2d(prop, &polygon, &numEdges);

        if (numEdges == 0 || polygonOutsideLineBox(polygon, numEdges, x, z, destX, destZ))
        {
            continue;
        }

        for (i = 0; i < numEdges; i++)
        {
            s32 next = (i + 1) % numEdges;

            if (geoSegmentsIntersect(x, z, destX, destZ, polygon[i].f[0], polygon[i].f[1], polygon[next].f[0], polygon[next].f[1]))
            {
                f32 frac = geoSegmentHitFrac(&from, &to, &polygon[i], &polygon[next]);

                if (frac < bestFrac)
                {
                    bestFrac = frac;
                    best = prop;
                }
            }
        }
    }

    return best;
}

/** What a circle walk does besides looking for walls */
typedef struct CircleWalk
{
    /* Sees each tile the circle overlaps */
    void (*visitTile)(struct CircleWalk *walk, StandTile *tile);
    /* Asked first about each edge the circle touches: TRUE makes it a wall even if it links */
    bool (*edgeIsWall)(struct CircleWalk *walk, StandTile *tile, s32 edge);
    /* Keep walking past walls, counting them, to see every tile */
    bool countHits;
    s32 numHits;
    union
    {
        RoomList rooms;
        StanSpecialTiles *special;
        f32 maxY; /* in tile units */
    };
} CircleWalk;

/**
 * Visits every tile a circle at (x, z) overlaps, starting on *tile. Returns
 * STAN_COLLISION_FOUND if it touches a wall edge (in g_StanHit), STAN_COLLISION_NONE if
 * not, or STAN_COLLISION_TRAVERSAL_LIMIT if it covers too many tiles to tell.
 */
static StanCollisionResult walkCircle(StandTile **startTile, f32 x, f32 z, f32 radius, CircleWalk *walk)
{
    /* The limit is checked after each tile, which can add a tile for each of its edges */
    StandTile *tiles[MAX_CIRCLE_TILES + 15];
    s32 count = 1;
    s32 t;

    x *= g_StanScale;
    z *= g_StanScale;
    radius *= g_StanScale;

    tiles[0] = *startTile;

    for (t = 0; t < count; t++)
    {
        StandTile *tile = tiles[t];
        s32 i;

        if (walk != NULL && walk->visitTile != NULL)
        {
            walk->visitTile(walk, tile);
        }

        for (i = 0; i < tile->numPoints; i++)
        {
            const StandTilePoint *a = &tile->points[i];
            const StandTilePoint *b = &tile->points[stanNextPoint(tile, i)];
            StandTile *linked;
            s32 j;

            /* Inside the edge by more than the radius, or off both its ends */
            if (!(geoSignedLineDist(a->x, a->z, b->x, b->z, x, z) < radius
                    && (geoDist2d(a->x, a->z, x, z) < radius
                        || geoDist2d(b->x, b->z, x, z) < radius
                        || geoProjectsOntoSegment(a->x, a->z, b->x, b->z, x, z))))
            {
                continue;
            }

            if ((walk == NULL || walk->edgeIsWall == NULL || !walk->edgeIsWall(walk, tile, i))
                && (linked = stanLinkedTile(tile, i)) != NULL)
            {
                for (j = 0; j < count; j++)
                {
                    if (tiles[j] == linked)
                    {
                        break;
                    }
                }

                if (j == count)
                {
                    tiles[count++] = linked;
                }

                continue;
            }

            g_StanHit.tile = tile;
            g_StanHit.edge = i;

            if (walk == NULL || !walk->countHits)
            {
                return STAN_COLLISION_FOUND;
            }

            walk->numHits++;
        }

        if (count >= MAX_CIRCLE_TILES)
        {
            return STAN_COLLISION_TRAVERSAL_LIMIT;
        }
    }

    return STAN_COLLISION_NONE;
}

/** Whether a circle of the radius fits at (x, z) without touching a wall */
StanCollisionResult stanCircleLegal(StandTile **tile, f32 x, f32 z, f32 radius)
{
    return walkCircle(tile, x, z, radius, NULL);
}

static void addTileRoom(CircleWalk *walk, StandTile *tile)
{
    RoomList *list = &walk->rooms;
    s32 i;

    for (i = 0; i < list->count; i++)
    {
        if (list->rooms[i] == tile->room)
        {
            return;
        }
    }

    if (list->count < list->max)
    {
        list->rooms[list->count++] = tile->room;
    }
}

/**
 * Lists the rooms a circle overlaps. Walls don't stop it, but it returns
 * STAN_COLLISION_FOUND if it touched any.
 */
StanCollisionResult stanCircleRooms(StandTile **tile, f32 x, f32 z, f32 radius, s32 *rooms, s32 *numRooms, s32 maxRooms)
{
    CircleWalk walk = {
        .visitTile = addTileRoom,
        .countHits = TRUE,
        .rooms = {.rooms = rooms, .count = 0, .max = maxRooms},
    };
    StanCollisionResult result = walkCircle(tile, x, z, radius, &walk);

    *numRooms = walk.rooms.count;

    /* Rare's hit counter also ran when the walk finished, so one hit only counts then */
    if (walk.numHits > 1 || (walk.numHits == 1 && result == STAN_COLLISION_NONE))
    {
        return STAN_COLLISION_FOUND;
    }

    return result;
}

/**
 * Like stanCircleLegal, and also checks the props of the collision types in the rooms the
 * circle covers. With bottom <= top, only props overlapping that height range above the
 * floor count. Returns STAN_COLLISION_FOUND for a prop too, the edge in g_StanHit.
 */
StanCollisionResult stanTestVolume(StandTile **tile, f32 x, f32 z, f32 radius, s32 cdtypes, f32 top, f32 bottom)
{
    s32 rooms[MAX_TEST_ROOMS + 1];
    s32 numRooms = 0;
    bool checkHeight = bottom <= top;
    StanCollisionResult result;
    s16 *propnum;

    result = stanCircleRooms(tile, x, z, radius, rooms, &numRooms, MAX_TEST_ROOMS);

    if (result >= 0)
    {
        return result;
    }

    g_StanHit.prop = NULL;

    if (cdtypes == 0)
    {
        return STAN_COLLISION_NONE;
    }

    if (checkHeight)
    {
        f32 floorY = stanGetFloorY(*tile, x, z);

        top += floorY;
        bottom += floorY;
    }

    rooms[numRooms] = -1;
    roomGetProps(rooms);

    for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
    {
        PropRecord *prop = &g_Props[*propnum];
        coord2d *polygon;
        s32 numEdges;
        f32 propTop;
        f32 propBottom;
        f32 bestDist = -1.0f;
        s32 i;

        if (!propIsOfCdType(prop, cdtypes))
        {
            continue;
        }

        propGetCollisionPolygon(prop, &polygon, &numEdges, &propTop, &propBottom);

        if (numEdges <= 0 || (checkHeight && !(propBottom <= top && bottom <= propTop)))
        {
            continue;
        }

        /* Of the edges the circle touches, the hit is the one whose line is furthest away */
        for (i = 0; i < numEdges; i++)
        {
            const coord2d *a = &polygon[i];
            const coord2d *b = &polygon[(i + 1) % numEdges];
            f32 dist = geoSignedLineDist(a->f[0], a->f[1], b->f[0], b->f[1], x, z);

            if (dist < 0.0f)
            {
                dist = -dist;
            }

            if (bestDist < dist
                && dist < radius
                && (geoDist2d(a->f[0], a->f[1], x, z) < radius
                    || geoDist2d(b->f[0], b->f[1], x, z) < radius
                    || geoProjectsOntoSegment(a->f[0], a->f[1], b->f[0], b->f[1], x, z)))
            {
                bestDist = dist;
                g_StanHit.hasLine = TRUE;
                g_StanHit.lineA = *a;
                g_StanHit.lineB = *b;
                g_StanHit.tile = NULL;
                g_StanHit.edge = 0;
                g_StanHit.prop = prop;
            }
        }

        if (bestDist > -1.0f)
        {
            return STAN_COLLISION_FOUND;
        }
    }

    return STAN_COLLISION_NONE;
}

static void findCrouchTile(CircleWalk *walk, StandTile *tile)
{
    if (tile->special == STAN_SPECIAL_CROUCH)
    {
        walk->special->crouch = TRUE;
    }
}

/* A crouch tile next door is a wall (Bond has to squat to get in); a ladder is noted */
static bool checkNeighbourSpecial(CircleWalk *walk, StandTile *tile, s32 edge)
{
    StandTile *linked = stanLinkedTile(tile, edge);

    if (linked != NULL)
    {
        if (linked->special == STAN_SPECIAL_CROUCH)
        {
            walk->special->crouch = TRUE;

            return TRUE;
        }

        if (linked->special == STAN_SPECIAL_LADDER)
        {
            s_LadderTile = linked;
            walk->special->ladder = TRUE;
        }
    }

    return FALSE;
}

/** Like stanCircleLegal, and finds whether the circle is on or by crouch and ladder tiles */
StanCollisionResult stanCircleGetSpecial(StandTile **tile, f32 x, f32 z, f32 radius, StanSpecialTiles *found)
{
    CircleWalk walk = {
        .visitTile = findCrouchTile,
        .edgeIsWall = checkNeighbourSpecial,
        .special = found,
    };

    found->crouch = FALSE;
    found->ladder = FALSE;

    return walkCircle(tile, x, z, radius, &walk);
}

static bool isEdgeAboveMaxY(CircleWalk *walk, StandTile *tile, s32 edge)
{
    return walk->maxY < (f32) tile->points[edge].y
        && walk->maxY < (f32) tile->points[stanNextPoint(tile, edge)].y;
}

/** Like stanCircleLegal, with tile edges entirely above maxY walls too */
StanCollisionResult stanTestCircleMaxHeight(StandTile **tile, f32 x, f32 z, f32 radius, f32 maxY)
{
    CircleWalk walk = {
        .edgeIsWall = isEdgeAboveMaxY,
        .maxY = maxY * g_StanScale,
    };

    return walkCircle(tile, x, z, radius, &walk);
}

/**
 * Gets the ladder stanCircleGetSpecial found: *ladderTile is the tile beside it,
 * *topTile the ladder itself, and quad its corners, the bottom pair first.
 */
void stanGetLadderQuad(StandTile **ladderTile, StandTile **topTile, LadderQuad *quad)
{
    StandTile *base = s_LadderTile;
    s32 i;

    for (i = 0; i < base->numPoints; i++)
    {
        StandTile *ladder = stanLinkedTile(base, i);
        s32 baseCorner = (i + 2) % 3;
        s32 j;

        if (ladder == NULL || ladder->special != STAN_SPECIAL_LADDER)
        {
            continue;
        }

        *ladderTile = base;
        *topTile = ladder;

        /* Turn the corners round until the second pair is no lower than the first */
        for (j = 0; j < 12; j++)
        {
            coord3d *c = quad->corners;
            s32 k;

            for (k = 0; k < 3; k++)
            {
                getPointWorldPos(&ladder->points[stanTriPoint(ladder, ((j >> 2) + k) % 3)], &c[(j + k) & 3]);
            }

            getPointWorldPos(&base->points[stanTriPoint(base, baseCorner)], &c[(j + 3) & 3]);

            if (!(c[2].y < c[0].y) && !(c[2].y < c[1].y) && !(c[3].y < c[0].y) && !(c[3].y < c[1].y))
            {
                break;
            }
        }

        return;
    }
}
