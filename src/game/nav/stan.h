#ifndef _STAN_H_
#define _STAN_H_
#include <ultra64.h>

#include <bondtypes.h>

struct StanPrefixRecord;
// RGB? I've called them 'triple' because I don't really know what RGB is
// No parens around params
#define STAN_TRIPLE_TO_PNT_INDEX(tile, tripleIndex) (tile->hdrTail >> (8 - 4*tripleIndex) & 0xF)
#define STAN_POINT_COUNT(tile) (tile->tail.half & 0xF)

#define STAN_MID_SPECIAL(tile) (tile->mid.half & 0xF)
#define STAN_MID_R(tile) ((tile->mid.half >> 0x04) & 0xF)
#define STAN_MID_G(tile) ((tile->mid.half >> 0x08) & 0xF)
#define STAN_MID_B(tile) ((tile->mid.half >> 0x0C) & 0xF)

#define STAN_TAIL_POINT_COUNT(tile) (tile->tail.half & 0xF)/*canonically getsides()*/
#define STAN_TAIL_C(tile) ((tile->tail.half >> 0x04) & 0xF)
#define STAN_TAIL_D(tile) ((tile->tail.half >> 0x08) & 0xF)
#define STAN_TAIL_E(tile) ((tile->tail.half >> 0x0C) & 0xF)

/* The ladder quad from stanGetMoveBondCollisionTiles: the shared bottom edge, then the top. */
struct move_bond_collision {
    union {
        struct {
            coord3d low0;
            coord3d low1;
            coord3d high0;
            coord3d high1;
        };
        coord3d corners[4];
    };
};

typedef enum StanCollisionResult {
    STAN_COLLISION_NONE = -2,
    STAN_COLLISION_FOUND = 2,
    STAN_COLLISION_TRAVERSAL_LIMIT = 5
} StanCollisionResult;

typedef struct StanRoomBounds {
    union {
        struct {
            s16 minX;
            s16 minY;
            s16 minZ;
            s16 maxX;
            s16 maxY;
            s16 maxZ;
        };
        struct {
            s16 min[3];
            s16 max[3];
        };
    };
} StanRoomBounds;

/////////////////
// extern

extern f32 g_StanHitFrac;
extern PropRecord *g_StanHitProp;
extern struct StandTile *g_StanTiles;
extern s32 g_StanLineLog;
extern StandTile *g_StanRoomFirstTile[139];
extern StanRoomBounds g_StanRoomBounds[139];
extern s32 g_StanNumRooms;

/////////////////
// prototypes

// Necessary forward declaration
void stanLineRoomsCallback( StandTile *tile,  StandTile *unused,  struct StandTileWalkCallbackRecord *data);
void stanSetLevelScale(f32 ls);
void stanResetHits(void);
s32 stanWalkLineNoCallback(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z);
bool stanIsNearlyInsideTri(StandTile *tile, f32 p_x, f32 p_z);
f32 stanGetPositionYValue(StandTile* tile, f32 p_x, f32 p_z);
bool stanGetCollisionEdge(coord3d *pntA, coord3d *pntB);
bool stanTileHasZeroArea(StandTile *tile);
s32 stanTestLocusEdgeAboveY(StandTile **tile, f32 target_x, f32 target_z, f32 radius, f32 yThreshold);
s32 stanCircleLegal(StandTile** tileStack, f32 target_x, f32 target_z, f32 unknown);

s32 stanTestLineUnobstructed(StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z, s32 cdtypes, f32 fromTop, f32 fromBottom, f32 toTop, f32 toBottom);
StandTile* stanFillSearch(StandTile* srcTile, tilePredicate_t tilePred);
s32 stanGetRoomsBetween(StandTile *tile, f32 start_x, f32 start_z, StandTile **destTile, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 maxBufSize);
s32 stanLineRooms(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 *rtnCountSize, s32 maxBufSize);
s32 stanTestVolume(StandTile **tile, f32 x, f32 z, f32 radius, s32 cdtypes, f32 top, f32 bottom);
s32 stanGetTileRoom(StandTile* tile);
PropRecord *stanLineFindDoor(StandTile *t, f32 start_x, f32 start_z, f32 end_x, f32 end_z, s32 cdtypes);
void stanGetTileColour(StandTile *tile, u8 *rtn);
s32 stanCircleGetSpecial(struct StandTile **tile, f32 x, f32 z, f32 radius, struct StandTileLocusCallbackRecord *records);
s32 stanLocusIsCrouch(struct StandTileLocusCallbackRecord *record);
s32 stanLocusIsLadder(struct StandTileLocusCallbackRecord *record);
f32 stanDist2d(f32 o_x,f32 o_z,f32 p_x,f32 p_z);
bool stanPointProjectsOntoEdge(f32 x1, f32 z1, f32 x2, f32 z2, f32 x3, f32 z3);
f32 stanGetSignedPointLineDistance(f32 x1, f32 z1, f32 x2, f32 z2, f32 x3, f32 z3);
void stanGetMoveBondCollisionTiles(StandTile **tile1, StandTile **tile2, struct move_bond_collision *quad);
struct StandTile *stanFindNearestLegalPos(f32 *arg_x, f32 *arg_y, f32 *arg_z, f32 radius);
bool stanSegmentsIntersect(f32 start1X, f32 start1Z, f32 end1X, f32 end1Z, f32 start2X, f32 start2Z, f32 end2X, f32 end2Z);
struct StandTile *stanMatchTileName(const char *id);
s32 stanIsInsideTriWorld(struct StandTile *tile, f32 p_x, f32 p_z);
s32 stanCircleRooms(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius, s32 *rooms, s32 *count_rtn, s32 bufMax);
StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *yRtn);
f32 stanSegmentIntersectFrac(coord2d *start1, coord2d *end1, coord2d *start2, coord2d *end2);
f32 stanPointLineDist(coord3d *point3D, coord2d *lineStart, coord2d *lineEnd);
f32 stanRaySegmentFrac(coord3d *point3D, coord2d *lineStart, coord2d *lineEnd, coord2d *direction);
s32 stanResolvePadTile(struct PadRecord *pad, char *tilename, struct StandTile **tile_stack);

void stanDetermineEOF(struct StanPrefixRecord *file , s32 origBase, u8 *newBase);
void stanLoadFile(struct StanPrefixRecord *file);

#endif
