/**
 * Stan: the floor. Every level has a stan file, a list of flat polygons ("tiles") that
 * cover everywhere a character can stand, grouped by room. Each tile edge either links to
 * the neighbouring tile or is a wall.
 *
 * This file loads the tiles and answers questions about them: which tile is below a
 * point, how high the floor is, which tile a pad stands on. The collision tests that walk
 * from tile to tile are in stanwalk.c. See docs/subsystems/nav.md.
 *
 * Tile coordinates are 16-bit integers; g_StanScale converts world units to them.
 */

#include <ultra64.h>
#include "game/nav/stan.h"
#include "game/nav/geometry.h"
#include "limits.h"
#include "math.h"
#include "str.h"

/* The most tiles stanSearchOutward looks at */
#define MAX_SEARCH_TILES 351

/* The tiles start here, and links count from here (the first tile is link 16) */
StandTile *g_StanTiles;
f32 g_StanScale = 1.0f;
f32 g_StanInvScale = 1.0f;

/* Each room's tiles are together in the file: its first tile, and its bounding box */
StandTile *g_StanRoomFirstTile[139];
StanRoomBounds g_StanRoomBounds[139];
s32 g_StanNumRooms;

static StandTile *s_FirstTile;
static StandTile *s_SearchQueue[MAX_SEARCH_TILES];

static bool isListEnd(const StandTile *tile)
{
    return tile->id == 0 && tile->room == 0;
}

/** The tile after this one in the file. Every tile has room for at least three points. */
StandTile *stanNextTile(StandTile *tile)
{
    s32 numPoints = tile->numPoints < 3 ? 3 : tile->numPoints;

    return (StandTile *) ((u8 *) tile + sizeof(StandTile) + numPoints * sizeof(StandTilePoint));
}

static void buildRoomData(void)
{
    StandTile *tile;
    s32 room = -1;
    s32 i;
    s32 j;

    g_StanNumRooms = 0;

    for (i = 0; i < ARRAYCOUNT(g_StanRoomFirstTile); i++)
    {
        g_StanRoomFirstTile[i] = NULL;
    }

    for (tile = s_FirstTile; !isListEnd(tile); tile = stanNextTile(tile))
    {
        StanRoomBounds *bounds;

        if (tile->room != room)
        {
            room = tile->room;

            if (g_StanNumRooms <= room)
            {
                g_StanNumRooms = room + 1;
            }

            g_StanRoomFirstTile[room] = tile;

            for (j = 0; j < 3; j++)
            {
                g_StanRoomBounds[room].min[j] = 0x7fff;
                g_StanRoomBounds[room].max[j] = -0x8000;
            }
        }

        bounds = &g_StanRoomBounds[room];

        for (i = 0; i < tile->numPoints; i++)
        {
            const s16 pos[3] = {tile->points[i].x, tile->points[i].y, tile->points[i].z};

            for (j = 0; j < 3; j++)
            {
                if (pos[j] < bounds->min[j])
                {
                    bounds->min[j] = pos[j];
                }

                if (pos[j] > bounds->max[j])
                {
                    bounds->max[j] = pos[j];
                }
            }
        }
    }
}

/**
 * Takes a level's stan file, loaded at file, and scale, the world units per tile unit.
 * The file's pointers count from 0; the tiles are used where they are.
 */
void stanLoadFile(StandFileHeader *file, f32 scale)
{
    StandTile **pointer;

    for (pointer = &file->firstTile; *pointer != NULL; pointer++)
    {
        *pointer = (StandTile *) ((u8 *) *pointer + (uintptr_t) file);
    }

    s_FirstTile = file->firstTile;
    g_StanTiles = (StandTile *) ((u8 *) s_FirstTile - 16 * sizeof(StandTilePoint));
    g_StanScale = scale;
    g_StanInvScale = 1.0f / scale;

    buildRoomData();
}

/** The distance in from edge corner->corner+1 of the tile's triangle; negative if outside */
static f32 triEdgeDist(const StandTile *tile, s32 corner, f32 x, f32 z)
{
    const StandTilePoint *a = &tile->points[stanTriPoint(tile, corner)];
    const StandTilePoint *b = &tile->points[stanTriPoint(tile, corner == 2 ? 0 : corner + 1)];

    return geoSignedLineDist(a->x, a->z, b->x, b->z, x, z);
}

/** Whether the point (in tile units) is no more than margin outside the tile's triangle */
static bool isInsideTri(const StandTile *tile, f32 x, f32 z, f32 margin)
{
    s32 i;

    for (i = 0; i < 3; i++)
    {
        if (triEdgeDist(tile, i, x, z) < -margin)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/** Whether a world point is on the tile, or within 2 tile units of it */
bool stanIsNearlyInsideTri(StandTile *tile, f32 x, f32 z)
{
    return isInsideTri(tile, x * g_StanScale, z * g_StanScale, 2.0f);
}

/** Whether the tile's triangle is flat seen from above */
static bool hasZeroArea(const StandTile *tile)
{
    const StandTilePoint *a = &tile->points[tile->tri0];
    const StandTilePoint *b = &tile->points[tile->tri1];
    const StandTilePoint *c = &tile->points[tile->tri2];

    return (b->z - a->z) * (c->x - a->x) == (b->x - a->x) * (c->z - a->z);
}

static void getPointWorldPos(const StandTilePoint *point, coord3d *out)
{
    out->x = point->x * g_StanInvScale;
    out->y = point->y * g_StanInvScale;
    out->z = point->z * g_StanInvScale;
}

/** The middle of the tile's triangle, in world units */
static void getTileMidPoint(const StandTile *tile, coord3d *out)
{
    const StandTilePoint *a = &tile->points[tile->tri0];
    const StandTilePoint *b = &tile->points[tile->tri1];
    const StandTilePoint *c = &tile->points[tile->tri2];

    out->x = (((f32) a->x + (f32) b->x + (f32) c->x) / 3.0f) * g_StanInvScale;
    out->y = (((f32) a->y + (f32) b->y + (f32) c->y) / 3.0f) * g_StanInvScale;
    out->z = (((f32) a->z + (f32) b->z + (f32) c->z) / 3.0f) * g_StanInvScale;
}

/** A point just inside one of the triangle's corners: a tenth of the way to the middle */
static void getPointInsideCorner(const StandTile *tile, s32 corner, coord3d *out)
{
    coord3d mid;

    getPointWorldPos(&tile->points[stanTriPoint(tile, corner)], out);
    getTileMidPoint(tile, &mid);

    out->x = mid.x * 0.1f + 0.9f * out->x;
    out->y = mid.y * 0.1f + 0.9f * out->y;
    out->z = mid.z * 0.1f + 0.9f * out->z;
}

static bool isRoomInList(const u8 *rooms, s32 room)
{
    s32 i;

    for (i = 0; i < 4 && rooms[i] != 0xff; i++)
    {
        if (rooms[i] == room)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Finds the highest tile at or below pos, searching every room whose bounds hold it (or
 * only those in rooms, a list of up to 4 ended by 0xff). Slow: used for objects that have
 * no tile yet. Sets *floorY, if given, to the floor height there.
 */
StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *floorY)
{
    f32 x = pos->x * g_StanScale;
    f32 y = pos->y * g_StanScale;
    f32 z = pos->z * g_StanScale;
    s16 tileX;
    s16 tileY;
    s16 tileZ;
    StandTile *best = NULL;
    f32 bestY = -FLT_MAX;
    s32 room;

    if (y > 32767.0f)
    {
        y = 32767.0f;
    }

    if (y < -32767.0f)
    {
        y = -32767.0f;
    }

    tileX = (s32) x;
    tileY = (s32) y;
    tileZ = (s32) z;

    for (room = 0; room < g_StanNumRooms; room++)
    {
        const StanRoomBounds *bounds = &g_StanRoomBounds[room];
        StandTile *tile;

        if (g_StanRoomFirstTile[room] == NULL
            || tileX < bounds->min[0] || tileX > bounds->max[0]
            || tileZ < bounds->min[2] || tileZ > bounds->max[2]
            || tileY < bounds->min[1])
        {
            continue;
        }

        if (rooms != NULL && !isRoomInList(rooms, room))
        {
            continue;
        }

        for (tile = g_StanRoomFirstTile[room]; !isListEnd(tile) && tile->room == room; tile = stanNextTile(tile))
        {
            coord3d mid;
            StandTile *reached = tile;
            f32 tileFloorY;

            if (!isInsideTri(tile, x, z, 2.0f) || hasZeroArea(tile))
            {
                continue;
            }

            /*
             * The triangle is only most of the tile: walk from its middle to make sure
             * the point is on this tile. (Rare meant to do this only near the triangle's
             * edges, but the flag that said so was never cleared, and GCC always walks.)
             */
            getTileMidPoint(tile, &mid);

            if (!stanWalkLine(&reached, mid.x, mid.z, pos->x, pos->z) || reached != tile)
            {
                continue;
            }

            tileFloorY = stanGetFloorY(tile, pos->x, pos->z);

            if (pos->y < tileFloorY)
            {
                continue;
            }

            if (bestY < tileFloorY)
            {
                best = tile;
                bestY = tileFloorY;
            }
        }
    }

    if (best != NULL && floorY != NULL)
    {
        *floorY = bestY;
    }

    return best;
}

static f32 distSquared(const coord3d *a, const coord3d *b)
{
    f32 dx = b->x - a->x;
    f32 dy = b->y - a->y;
    f32 dz = b->z - a->z;

    return dx * dx + dy * dy + dz * dz;
}

/**
 * Moves pos to the nearest place where a circle of the radius fits, trying just inside
 * each tile's triangle corners and its middle. Returns that tile, or NULL (pos unchanged).
 */
StandTile *stanFindNearestLegalPos(coord3d *pos, f32 radius)
{
    StandTile *best = NULL;
    coord3d original = *pos;
    f32 bestDist = 4294967296.0f;
    StandTile *tile;

    for (tile = s_FirstTile; !isListEnd(tile); tile = stanNextTile(tile))
    {
        s32 i;

        /* Tiles named q... are skipped */
        if ((tile->id >> 23) != 0 || hasZeroArea(tile))
        {
            continue;
        }

        for (i = 0; i < 4; i++)
        {
            coord3d candidate;
            StandTile *start = tile;

            if (i == 3)
            {
                getTileMidPoint(tile, &candidate);
            }
            else
            {
                getPointInsideCorner(tile, i, &candidate);
            }

            if (stanCircleLegal(&start, candidate.x, candidate.z, radius) < 0)
            {
                f32 dist = distSquared(&candidate, &original);

                if (dist < bestDist)
                {
                    best = tile;
                    bestDist = dist;
                    *pos = candidate;
                }
            }
        }
    }

    return best;
}

/**
 * Packs a tile name as StandTile.id, or returns -1 if it isn't one. A name is p or q, a
 * number up to 32767, a letter a-z, and an optional digit 0-7: "p123a4".
 */
static s32 packTileName(const char *name)
{
    char *end;
    u32 number;
    s32 letter;
    s32 digit = 0;

    if (name[0] != 'p' && name[0] != 'q')
    {
        return -1;
    }

    number = strtol(name + 1, &end, 10);

    if (end == name + 1 || number > 32767 || end[0] < 'a' || end[0] > 'z')
    {
        return -1;
    }

    letter = end[0] - 'a';

    if (end[1] != '\0')
    {
        if (end[1] < '0' || end[1] > '7' || end[2] != '\0')
        {
            return -1;
        }

        digit = end[1] - '0';
    }

    return ((name[0] - 'p') << 23) | (number << 8) | (letter << 3) | digit;
}

static StandTile *findTileByName(const char *name)
{
    s32 id = packTileName(name);
    StandTile *tile;

    if (id < 0)
    {
        return NULL;
    }

    for (tile = s_FirstTile; !isListEnd(tile); tile = stanNextTile(tile))
    {
        if (tile->id == (u32) id)
        {
            return tile;
        }
    }

    return NULL;
}

/**
 * Finds the floor tile under a pad from the tile name the setup gives it. If the name is
 * missing or the pad is outside that tile, finds the nearest place on any tile and walks
 * back to the pad from there.
 * Returns 1 if the named tile was right, 2 if another tile was found, 0 if none.
 */
s32 stanResolvePadTile(struct PadRecord *pad, const char *tilename, StandTile **tile)
{
    coord3d pos;

    *tile = tilename[0] != '\0' ? findTileByName(tilename) : NULL;

    if (*tile != NULL
        && isInsideTri(*tile, pad->pos.x * g_StanScale, pad->pos.z * g_StanScale, 0.0f))
    {
        return 1;
    }

    pos = pad->pos;
    *tile = stanFindNearestLegalPos(&pos, 0.0f);

    if (*tile != NULL && stanWalkLine(tile, pos.x, pos.z, pad->pos.x, pad->pos.z))
    {
        return 2;
    }

    *tile = NULL;

    return 0;
}

/** The height of the tile's floor at (x, z), in world units */
f32 stanGetFloorY(StandTile *tile, f32 x, f32 z)
{
    const StandTilePoint *a = &tile->points[tile->tri0];
    const StandTilePoint *b = &tile->points[tile->tri1];
    const StandTilePoint *c = &tile->points[tile->tri2];
    f32 ab[3];
    f32 ac[3];
    s64 normal[3];
    s64 planeDist;

    x *= g_StanScale;
    z *= g_StanScale;

    ab[0] = b->x - a->x;
    ab[1] = b->y - a->y;
    ab[2] = b->z - a->z;

    ac[0] = c->x - a->x;
    ac[1] = c->y - a->y;
    ac[2] = c->z - a->z;

    /* The plane through the triangle: normal . p = planeDist, solved for y */
    normal[0] = ab[1] * ac[2] - ab[2] * ac[1];
    normal[1] = ab[2] * ac[0] - ab[0] * ac[2];
    normal[2] = ab[0] * ac[1] - ab[1] * ac[0];

    planeDist = normal[0] * a->x + normal[1] * a->y + normal[2] * a->z;

    if (normal[1] == 0)
    {
        return a->y * g_StanInvScale;
    }

    return (f32) (((f64) planeDist - (f64) x * (f64) normal[0] - (f64) z * (f64) normal[2]) / (f64) normal[1]) * g_StanInvScale;
}

/** The colour a character standing on the tile is lit by: r, g, b from 0 to 255 */
void stanGetTileColour(StandTile *tile, u8 *rgb)
{
    rgb[0] = tile->red * 0x11;
    rgb[1] = tile->green * 0x11;
    rgb[2] = tile->blue * 0x11;
}

static bool isQueued(const StandTile *tile, s32 count)
{
    s32 i;

    for (i = 0; i < count; i++)
    {
        if (s_SearchQueue[i] == tile)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Searches the tiles outward from start, nearest first in steps from tile to tile, for one
 * the predicate is true of. Gives up, returning NULL, after MAX_SEARCH_TILES tiles.
 */
StandTile *stanSearchOutward(StandTile *start, bool (*predicate)(StandTile *tile))
{
    s32 count = 1;
    s32 i;

    if (predicate(start))
    {
        return start;
    }

    s_SearchQueue[0] = start;

    for (i = 0; i < count; i++)
    {
        StandTile *tile = s_SearchQueue[i];
        s32 p;

        for (p = 0; p < tile->numPoints; p++)
        {
            StandTile *neighbour = stanLinkedTile(tile, p);

            if (neighbour == NULL || isQueued(neighbour, count))
            {
                continue;
            }

            if (predicate(neighbour))
            {
                return neighbour;
            }

            s_SearchQueue[count++] = neighbour;

            if (count >= MAX_SEARCH_TILES)
            {
                return NULL;
            }
        }
    }

    return NULL;
}
