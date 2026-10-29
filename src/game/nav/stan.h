#ifndef _STAN_H_
#define _STAN_H_
#include <ultra64.h>
#include <bondtypes.h>

/*
 * The floor tiles ("stan") and the collision tests that walk them.
 * See docs/subsystems/nav.md.
 */

/** A tile's special type */
enum StanSpecial
{
    STAN_SPECIAL_NONE,
    STAN_SPECIAL_CROUCH, /* Bond squats to move onto it */
    STAN_SPECIAL_2,      /* in Egypt and Bunker 1, with no effect */
    STAN_SPECIAL_LADDER,
};

/** A corner of a tile, and the start of the edge to the next corner */
typedef struct StandTilePoint
{
    s16 x;
    s16 y;
    s16 z;
    /* the tile across the edge, in 8-byte steps from g_StanTiles; below 16 if the edge is a wall */
    u16 link;
} StandTilePoint;

typedef struct StandTile
{
    u32 id : 24; /* the tile's name, see stanResolvePadTile */
    u8 room;
    u16 special : 4; /* enum StanSpecial */
    u16 red : 4;     /* the light a character standing on it gets, stanGetTileColour */
    u16 green : 4;
    u16 blue : 4;
    u16 numPoints : 4;
    /* Three corners whose triangle covers most of the tile: the quick "on the tile" tests and the floor height use it */
    u16 tri0 : 4;
    u16 tri1 : 4;
    u16 tri2 : 4;
    StandTilePoint points[]; /* anticlockwise seen from above; always room for at least 3 */
} StandTile;

/* A level's stan file. The asset files in assets/obseg/stan build it from these. */
typedef struct StandFileHeader
{
    void *unused;
    StandTile *firstTile; /* then more pointers, up to a NULL, then the tiles */
    u8 pointers[];
} StandFileHeader;

typedef struct StandFileFooter
{
    char strictstring[8];
    void *unused[4];
} StandFileFooter;

/* Citadel's file is the earlier format, kept to build it; the game doesn't load it */
typedef struct BetaStandFilePoint
{
    f32 x;
    f32 y;
    f32 z;
    u32 link;
} BetaStandFilePoint;

typedef struct BetaStandTile
{
    const char *id;
    u16 special : 4;
    u16 red : 4;
    u16 green : 4;
    u16 blue : 4;
    u16 room;
    u8 numPoints;
    u8 tri0;
    u8 tri1;
    u8 tri2;
    struct BetaStandFilePoint points[];
} BetaStandTile;

/* What a collision test hit, for stanGetCollisionEdge and for the callers below */
typedef struct StanHit
{
    StandTile *tile; /* the tile whose wall edge was hit, or NULL */
    s32 edge;
    bool hasLine;    /* line holds the last prop edge hit */
    coord2d lineA;
    coord2d lineB;
    f32 frac;        /* stanTestLineUnobstructed: how far along the line the hit was */
    PropRecord *prop; /* the prop hit, or NULL */
} StanHit;

/* The walks' results */
typedef enum StanCollisionResult
{
    STAN_COLLISION_NONE = -2,
    STAN_COLLISION_FOUND = 2,
    STAN_COLLISION_TRAVERSAL_LIMIT = 5, /* the circle covered too many tiles to finish */
} StanCollisionResult;

/** What stanCircleGetSpecial found under a circle */
typedef struct StanSpecialTiles
{
    bool crouch;
    bool ladder;
} StanSpecialTiles;

/** A ladder, from stanGetLadderQuad: the bottom edge, then the top */
typedef struct LadderQuad
{
    union
    {
        struct
        {
            coord3d low0;
            coord3d low1;
            coord3d high0;
            coord3d high1;
        };
        coord3d corners[4];
    };
} LadderQuad;

typedef struct StanRoomBounds
{
    s16 min[3];
    s16 max[3];
} StanRoomBounds;

extern StandTile *g_StanTiles;
extern f32 g_StanScale;
extern f32 g_StanInvScale;
extern StandTile *g_StanRoomFirstTile[139];
extern StanRoomBounds g_StanRoomBounds[139];
extern s32 g_StanNumRooms;
extern StanHit g_StanHit;

/** The tile across a tile's edge (from point i to the next), or NULL for a wall */
static inline StandTile *stanLinkedTile(const StandTile *tile, s32 i)
{
    u16 link = tile->points[i].link;

    return (link >> 4) != 0 ? (StandTile *) ((u8 *) g_StanTiles + link * 8) : NULL;
}

/** The index of corner 0, 1 or 2 of a tile's triangle */
static inline s32 stanTriPoint(const StandTile *tile, s32 corner)
{
    return corner == 0 ? tile->tri0 : corner == 1 ? tile->tri1 : tile->tri2;
}

static inline s32 stanNextPoint(const StandTile *tile, s32 i)
{
    return (i + 1) % tile->numPoints;
}

/* stan.c: the tiles */
void stanLoadFile(StandFileHeader *file, f32 scale);
StandTile *stanNextTile(StandTile *tile);
StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *yRtn);
StandTile *stanFindNearestLegalPos(coord3d *pos, f32 radius);
s32 stanResolvePadTile(struct PadRecord *pad, const char *tilename, StandTile **tile);
f32 stanGetFloorY(StandTile *tile, f32 x, f32 z);
bool stanIsNearlyInsideTri(StandTile *tile, f32 x, f32 z);
void stanGetTileColour(StandTile *tile, u8 *rgb);
StandTile *stanSearchOutward(StandTile *start, bool (*predicate)(StandTile *tile));

/* stanwalk.c: the collision tests */
void stanResetHits(void);
bool stanGetCollisionEdge(coord3d *a, coord3d *b);
bool stanWalkLine(StandTile **tile, f32 x, f32 z, f32 destX, f32 destZ);
s32 stanGetRoomsBetween(StandTile *tile, f32 x, f32 z, StandTile **destTile, f32 destX, f32 destZ, s32 *rooms, s32 maxRooms);
bool stanTestLineUnobstructed(StandTile **tile, f32 x, f32 z, f32 destX, f32 destZ, s32 cdtypes, f32 fromTop, f32 fromBottom, f32 toTop, f32 toBottom);
PropRecord *stanLineFindDoor(StandTile *tile, f32 x, f32 z, f32 destX, f32 destZ, s32 cdtypes);
StanCollisionResult stanCircleLegal(StandTile **tile, f32 x, f32 z, f32 radius);
StanCollisionResult stanCircleRooms(StandTile **tile, f32 x, f32 z, f32 radius, s32 *rooms, s32 *numRooms, s32 maxRooms);
StanCollisionResult stanTestVolume(StandTile **tile, f32 x, f32 z, f32 radius, s32 cdtypes, f32 top, f32 bottom);
StanCollisionResult stanCircleGetSpecial(StandTile **tile, f32 x, f32 z, f32 radius, StanSpecialTiles *found);
StanCollisionResult stanTestCircleMaxHeight(StandTile **tile, f32 x, f32 z, f32 radius, f32 maxY);
void stanGetLadderQuad(StandTile **ladderTile, StandTile **topTile, LadderQuad *quad);

#endif
