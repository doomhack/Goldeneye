/**
 * Stan: the floor. Every level has a stan file, a list of flat polygons ("tiles") that
 * cover everywhere a character can stand, grouped by room. Each tile edge either links to
 * the neighbouring tile or is a wall.
 *
 * Collision and movement are walks over this graph instead of tests against the room
 * geometry: stanWalkLine follows a line from tile to tile until it reaches its end or hits
 * a wall, and stanWalkCircle visits every tile a circle touches. The other tests here
 * (stanCircleLegal, stanLineRooms, stanTestLineUnobstructed, stanTestVolume) are those
 * two walks with different callbacks. A hit is saved in g_StanHit* for the caller to read.
 *
 * Tile coordinates are 16-bit integers; g_StanScale converts world units to them.
 * See docs/subsystems/nav.md.
 */

#include <ultra64.h>
//#include <bondtypes.h>
#include "game/nav/stan.h"
#include "game/world/bg.h"
#include "game/chr/chrai.h"
#include "game/chr/chr.h"
#include "game/nav/stan.h"
#include "assert.h"
#include "limits.h"
#include "str.h"
#include "token.h"

void stanGetTileMidPoint(StandTile *tile, coord3d *out);

// bss
struct StanPrefixRecord {
    s32 stanfile;
    StandTile *ptr_firstroom;    // read as offset 4, hence the struct
};

struct StanPrefixRecord *g_StanFile;

StandTile *g_StanRoomFirstTile[139];
StanRoomBounds g_StanRoomBounds[139];
s32 g_StanNumRooms; //region?

// All relating to a saved collision, but not one struct
StandTile *g_StanHitTile;
s32 g_StanHitEdge;
struct coord2d g_StanHitLineA;
struct coord2d g_StanHitLineB;
f32 g_StanHitFrac;

PropRecord * g_StanHitProp;

StandTile * g_StanLadderTile;
StandTile *g_StanFillStack[352];


// data

// Indexed by StandTile.mid.headerMid.special.
u8 g_StanTileSpecialFlags[] = {
    0x8D, 0x86, 0x04, 0xC5,
    0x9D, 0xA4, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};


f32 g_StanScale = 1.0;
f32 g_StanInvScale = 1.0;
u8 g_StanTileSizes[] = {
    0x20,0x20,0x20,0x20,
    0x28,0x30,0x38,0x40,
    0x48,0x50,0x58,0x00
};
struct StandTile * g_StanTiles = NULL;
s32 g_StanFirstTileAddr = 0;
struct StandTile* g_StanTilesEnd = NULL;
s32 g_StanDebugRegion = 0;
s32 g_StanLineLog = 0;

s32 g_StanSavedCollHasLine =  0;


const char aStanlinelog[] = "-stanlinelog";

// forward declarations

s32 stanCircleCrouchCallback(StandTile *tile, struct StandTileLocusCallbackRecord* record);
s32 stanCircleLadderCallback(StandTile *tile, s32 pointIdx, f32 edgeDist, f32 pointDistA, f32 pointDistB, struct StandTileLocusCallbackRecord *record);
s32 stanCircleRooms(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius, s32 *rooms, s32 *count_rtn, s32 bufMax);
f32 stanTriEdgeDist(StandTile *tile, s32 start3index, f32 p_x, f32 p_z);
StanCollisionResult stanWalkCircle(struct StandTile**, f32, f32, f32, standTileLocusCallback_A_t, standTileLocusCallback_B_t, standTileLocusCallback_C_t, struct StandTileLocusCallbackRecord*);
s32 stanLocusAddTileRoomIfNew(StandTile *tile, struct StandTileLocusCallbackRecord *rec);
s32 stanLocusIsCrouch(struct StandTileLocusCallbackRecord *record);
s32 stanGetLocusCount(struct StandTileLocusCallbackRecord *record);
s32 stanLocusEdgeIsAboveY(StandTile *tile, s32 edgeIndex, f32 edgeDist, f32 distToPointA, f32 distToPointB, struct StandTileLocusCallbackRecord *record);

// end forward declarations


void stanBuildRoomData(void)
{
    StandTile *tile;
    u8 lastRoom;
    s32 i;
    s32 j;
    s32 k;

    lastRoom = 0xff;
    g_StanNumRooms = 0;

    // Must remain on one line for matching.
    for (k = 0; k < 139; k++) g_StanRoomFirstTile[k] = NULL;

    tile = g_StanFile->ptr_firstroom;

    while (*(u32 *)tile)
    {
        if (tile->room != lastRoom)
        {
            lastRoom = tile->room;

            if (g_StanNumRooms <= lastRoom)
            {
                g_StanNumRooms = lastRoom + 1;
            }

            g_StanRoomFirstTile[lastRoom] = tile;

            g_StanRoomBounds[lastRoom].min[0] = g_StanRoomBounds[lastRoom].min[1] = g_StanRoomBounds[lastRoom].min[2] = 0x7fff;
            g_StanRoomBounds[lastRoom].max[0] = g_StanRoomBounds[lastRoom].max[1] = g_StanRoomBounds[lastRoom].max[2] = -0x8000;
        }

        for (i = 0; i < (tile->tail.hdrTail.pointCount & 0xf); i++)
        {
            for (j = 0; j < 3; j++)
            {
                if (tile->points[i].AsArray[j] < g_StanRoomBounds[lastRoom].min[j])
                {
                    g_StanRoomBounds[lastRoom].min[j] = tile->points[i].AsArray[j];
                }

                if (tile->points[i].AsArray[j] > g_StanRoomBounds[lastRoom].max[j])
                {
                    g_StanRoomBounds[lastRoom].max[j] = tile->points[i].AsArray[j];
                }
            }
        }

        tile = (StandTile *)(((u8 *)tile) + g_StanTileSizes[tile->tail.hdrTail.pointCount & 0xf]);
    }
}


/**
 * Finds the highest stan file beneath pos. If a tile is found, yRtn is set to the tile
 * height beneath pos.
 * @returns NULL if no suitable tile is found.
 */
StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *yRtn)
{
    StandTile *firstTile;
    f32 scaled[3];
    StandTile *tile;
    s16 scaledShort[3];
    coord3d *midPointPtr;
    StandTile *tileStack;
    StandTile *bestTile;
    f32 maxY;
    f32 bestY;
    f32 edgeDist;
    f32 tileY;
    s32 nearEdge;
    coord3d midPoint;
    s32 tailhalf;
    s32 room;
    StandTile **roomFirstTiles;
    s32 i;

    maxY = 32767.0f;
    bestY = -3.4028235e38f;
    bestTile = NULL;

    scaled[0] = pos->x * g_StanScale;
    scaled[1] = pos->y * g_StanScale;
    scaled[2] = pos->z * g_StanScale;

    if (maxY < scaled[1]) 
    {
        scaled[1] = maxY;
    }

    if (scaled[1] < -32767.0f) 
    {
        scaled[1] = -32767.0f;
    }

    scaledShort[0] = scaled[0];
    scaledShort[1] = scaled[1];
    scaledShort[2] = scaled[2];

    midPointPtr = &midPoint;
    room = 0; \
    if (g_StanNumRooms > 0) \
    { \
        roomFirstTiles = (StandTile **) &g_StanRoomFirstTile; \
        do 
        {
            firstTile = *roomFirstTiles;
            
            if (firstTile != NULL)
            {
                if (scaledShort[0] < (&((StanRoomBounds *) g_StanRoomBounds)[room])->minX)
                {
                    goto next_room;
                }

                if ((&((StanRoomBounds *) g_StanRoomBounds)[room])->maxX < scaledShort[0])
                {
                    goto next_room;
                }

                if (scaledShort[2] < (&((StanRoomBounds *) g_StanRoomBounds)[room])->minZ)
                {
                    goto next_room;
                }

                if ((&((StanRoomBounds *) g_StanRoomBounds)[room])->maxZ < scaledShort[2])
                {
                    goto next_room;
                }

                if (scaledShort[1] < (&((StanRoomBounds *) g_StanRoomBounds)[room])->minY)
                {
                    goto next_room;
                }

                if (rooms != NULL)
                {
                    for (i = 0; (rooms[i] != 0xff) && (i != 4); i++)
                    {
                        if (room == rooms[i])
                        {
                            goto found_room;
                        }
                    }
                    goto next_room;
found_room:
                    ;
                }

                tile = firstTile;
                firstTile = *roomFirstTiles;

                while (((*((u32 *) tile)) != 0) && (tile->room == room))
                {
                    for (i = 0; i < 3; i++)
                    {
                        edgeDist = stanTriEdgeDist(tile, i, scaled[0], scaled[2]);

                        if (edgeDist < -2.0f)
                        {
                            goto nexttile;
                        }
                        
                        if (edgeDist < 2.0f)
                        {
                            nearEdge = 1;
                        }
                    }
                    
                    if (stanTileHasZeroArea(tile))
                    {
                        goto nexttile;
                    }
                    
                    if (nearEdge)
                    {
                        stanGetTileMidPoint(tile, midPointPtr);
                        tileStack = tile;
                        
                        if (!stanWalkLineNoCallback(&tileStack, midPointPtr->x, midPointPtr->z, pos->x, pos->z)) 
                        {
                            goto nexttile;
                        }
                        
                        if (tileStack != tile) 
                        {
                            goto nexttile;
                        }
                    }
                    
                    tileY = stanGetPositionYValue(tile, pos->x, pos->z);
                    
                    if (pos->y < tileY)
                    {
                        goto nexttile;
                    }
                    
                    if (bestY < tileY)
                    {
                        bestTile = tile;
                        bestY = tileY;
                    }
nexttile:
                    tailhalf = tile->tail.half;
                    tile = (StandTile *) (((u8 *) tile) + g_StanTileSizes[(tailhalf >> 12) & 0xf]); \
                } \
next_room:
                ;
            }
            
            room++;
            roomFirstTiles++;
        } while (room < g_StanNumRooms);
    }
    
    if ((bestTile != NULL) && (yRtn != NULL))
    {
        *yRtn = bestY;
    }
        
    return bestTile;
}


void stanLoadFile(struct StanPrefixRecord *file)
{
    g_StanDebugRegion = 1;
    g_StanFile = file;
    g_StanTiles = (StandTile *)(((u8 *)file->ptr_firstroom) - 0x80);

    if (tokenFind(1, aStanlinelog))
    {
        g_StanLineLog = 1;
    }

    stanBuildRoomData();
    stanSetLevelScale(1.0f);
}


/**
 * Returns true if x/z coords from the three point indices out of tile->tail.half are colinear i.e. the triangle has zero horizontal area.
 */
bool stanTileHasZeroArea(StandTile *tile)
{
    s32 AB[3];
    s32 AC[3];
    u32 crossStore[2];
    s32 temp1, temp2, temp3;
    

    temp1 = (tile->tail.half >> 8) & 0xf;
    temp2 = (tile->tail.half >> 4) & 0xf;
    temp3 = (tile->tail.half) & 0xf;

    AB[0] = tile->points[temp2].x - tile->points[temp1].x;
    AB[2] = tile->points[temp2].z - tile->points[temp1].z;
    
    AC[0] = tile->points[temp3].x - tile->points[temp1].x;
    AC[2] = tile->points[temp3].z - tile->points[temp1].z;

    crossStore[0] = (AB[2] * AC[0]) - (AB[0] * AC[2]);

    return crossStore[0] == 0;
}


void stanGetTileMidPoint(StandTile *tile, coord3d *out)
{
    u16 tail;
    u8 indexA;
    u32 indexB;
    u32 indexC;
    StandTilePoint *pointA;
    StandTilePoint *pointB;
    unsigned int shifted;
    s16 *pointCx;

    tail = (indexC = tile->tail.half);
    shifted = (tail & 0xFFFF) >> 4;
    indexA = (tail >> 8) & 0xf;
    indexB = shifted & 0xf;
    indexC = indexC & 0xf;
    pointCx = &(&tile->points[indexC])->x;
    pointA = &tile->points[indexA];
    pointB = &tile->points[indexB];
    out->x = (((((f32) pointA->x) + ((f32) pointB->x)) + ((f32) (*pointCx))) / 3.0f) * g_StanInvScale;
    out->y = (((((f32) (&tile->points[indexA])->y) + ((f32) pointB->y)) + ((f32) (&tile->points[indexC])->y)) / 3.0f) * g_StanInvScale;
    out->z = (((((f32) (&tile->points[indexA])->z) + ((f32) pointB->z)) + ((f32) ((float) (&tile->points[indexC])->z))) / 3.0f) * g_StanInvScale;
}


void stanGetPointInsideCorner(StandTile *tile, s32 tripleIndex /*canonically c */, coord3d *out)
{
    coord3d midPoint;
    s32 pntIndex;


    pntIndex = (tile->tail.half >> (8 - (tripleIndex * 4))) & 0xf;
    
    
    out->x = ((f32) tile->points[pntIndex].x) * g_StanInvScale;
    out->y = ((f32) tile->points[pntIndex].y) * g_StanInvScale;
    out->z = ((f32) tile->points[pntIndex].z) * g_StanInvScale;
    
    stanGetTileMidPoint(tile, &midPoint);
    
    // 10% of the way from the actual tile point towards the tile's centre.
    out->x = (midPoint.x * 0.1f) + (0.9f * out->x);
    out->y = (midPoint.y * 0.1f) + (0.9f * out->y);
    out->z = (midPoint.z * 0.1f) + (0.9f * out->z);
}


f32 stanDistSquared(coord3d *p,coord3d *q)
{
    // Should be a coord3d or vec3d, but they used an array which
    // causes lots of data reads and writes to the stack.
    f32 components[3];

    components[0] = q->x - p->x;
    components[1] = q->y - p->y;
    components[2] = q->z - p->z;

    return components[0]*components[0] + components[1]*components[1] + components[2]*components[2];
}


StandTile *stanFindNearestLegalPos(f32 *x, f32 *y, f32 *z, f32 radius)
{
    StandTile *tile;
    s32 tileTail;
    StandTile *stack[1];
    StandTile *bestTile;
    s32 i;
    s32 midpointIndex;
    coord3d original;
    coord3d candidate;
    f32 bestDist;
    f32 dist;

    bestTile = NULL;
    original.x = *x;
    original.y = *y;
    original.z = *z;
    midpointIndex = 3;
    bestDist = M_U32_MAX_VALUE_F;


    tile = g_StanFile->ptr_firstroom;

    if (*((u32 *) tile))
    {
        do
        {
            if (((((u16 *) tile)[0] >> 15) & 1) != 1)
            {
                if (stanTileHasZeroArea(tile) == FALSE)
                {
                    for (i = 0; i != 4; i++)
                    {
                        if (i == midpointIndex)
                        {
                            stanGetTileMidPoint(tile, &candidate);
                        }
                        else
                        {
                            stanGetPointInsideCorner(tile, i, &candidate);
                        }

                        stack[0] = tile;

                        if (stanCircleLegal(stack, candidate.x, candidate.z, radius) < 0)
                        {

                            dist = stanDistSquared(&candidate, &original);

                            if (dist < bestDist)
                            {
                                bestTile = tile;
                                bestDist = dist;
                                *x = candidate.x;
                                *y = candidate.y;
                                *z = candidate.z;
                            }
                        }
                    }
                }
            }

            tileTail = tile->tail.half;
            tile = (StandTile *) (((u8 *) tile) + g_StanTileSizes[(tileTail >> 12) & 0xf]);
            
        } while (*((u32 *) tile));
    }

    return bestTile;
}


// Returns the shortest distance from (p_x,p_z) to the infinite extention of tile's index-th edge, projected into XZ.
// Where the edge is vertical (or degenerate) they just return the distance between the points.
// cannonically tile is sf and index is ei
f32 stanEdgeDist(StandTile *tile,s32 index,f32 p_x,f32 p_z)
{
    s32 nextIndex;
    f32 edge_x;
    f32 edge_z;
    f32 edge_len; //canonically d

    f32 v_x;
    f32 v_z;
    f32 crossProduct;

    // 3 unused. We use 2 for the points to make our code cleaner,
    //   though it seems much more likely that the variables were used in the else clause.
    struct StandTilePoint* currPnt;
    struct StandTilePoint* nextPnt;


    // Omiting the '& 0xF' is equivalent, but keeping it is necessary to match.
    // Perhaps the structure isn't correct but this seems much cleaner than doing an explicit >> 0xC.
    nextIndex = (index + 1) % STAN_TAIL_E(tile);

    nextPnt = &tile->points[nextIndex];
    currPnt = &tile->points[index];
    edge_x = (f32)(nextPnt->x - currPnt->x);
    edge_z = (f32)(nextPnt->z - currPnt->z);

    edge_len = sqrtf(edge_x * edge_x + edge_z * edge_z);

    if (edge_len == 0) {
        // Degenerate case, edge is vertical
        // They just return the distance between the points, which is sensible and the correct value in 3 dimensions.
        v_x = p_x - (f32)tile->points[nextIndex].x;
        v_z = p_z - (f32)tile->points[nextIndex].z;
        return sqrtf(v_x * v_x + v_z * v_z);
    }
    else
    {

        // | (AP x AB) / ||AB|| | = ||PA|| sin(a),
        // so we're returning the SIGNED displacement
        crossProduct = (
            edge_z * (p_x - (f32)tile->points[index].x)
            +
            -edge_x * (p_z - (f32)tile->points[index].z)
        );
        return crossProduct / edge_len;
    }

}


f32 stanTriEdgeDist(StandTile *tile, s32 start3index, f32 p_x, f32 p_z)
{
    f32 dx;
    f32 edgeX;
    f32 edgeZ;
    f32 edgeLen;
    f32 dz;
    f32 crossProduct;
    s32 end3index;
    s32 nextPntI;


    nextPntI = 2;

    if (start3index != nextPntI) {
        end3index = start3index + 1;
    } else {
        end3index = 0;
    }

    start3index = (tile->tail.half >> (8 - (start3index << nextPntI))) & 0xf;
    end3index = (tile->tail.half >> (8 - (end3index << nextPntI))) & 0xf;

    edgeX = tile->points[end3index].x - tile->points[start3index].x;
    edgeZ = tile->points[end3index].z - tile->points[start3index].z;
    edgeLen = sqrtf((edgeX * edgeX) + (edgeZ * edgeZ));

    if (edgeLen == 0.0f) {
        dx = p_x - tile->points[end3index].x;
        dz = p_z - tile->points[end3index].z;
        return sqrtf((dx * dx) + (dz * dz));
    }


    crossProduct = (edgeZ * (p_x - tile->points[start3index].x)) + (-edgeX * (p_z - tile->points[start3index].z));
    return crossProduct / edgeLen;
}


f32 stanTriEdgeDistWorld(StandTile *tile,s32 start3index,f32 p_x,f32 p_z)
{
  f32 disp;

  disp = stanTriEdgeDist(tile, start3index, p_x * g_StanScale, p_z * g_StanScale);
  return disp * g_StanInvScale;
}


f32 stanPointDist(StandTile *tile,int pntI,f32 p_x,f32 p_z)
{

  p_x -= (f32)tile->points[pntI].x;
  p_z -= (f32)tile->points[pntI].z;
  return sqrtf(p_x * p_x + p_z * p_z);
}


/**
 * Returns true if the perpendicular projection of the X/Z point onto the
 * tile edge's infinite line falls between the edge endpoints.
 *
 * Example:
 *
 *     A -------- B
 *          |
 *          |
 *          P
 *
 * P is not on the edge, but its projection lands between A and B.
 */
bool stanPointProjectsOntoTileEdge(StandTile *tile, s32 edgeIndex, f32 p_x, f32 p_z)
{
    StandTilePoint *point;
    f32 edgeXCopy;
    f32 startX;
    f32 startZ;
    f32 edgeX;
    f32 edgeZ;
    StandTilePoint *nextPoint;

    point = &tile->points[edgeIndex];

    startX = point->x;
    startZ = point->z;

    edgeIndex = (edgeIndex + 1) % ((tile->tail.half >> 12) & 0xf);

    point = (nextPoint = &tile->points[edgeIndex]);

    edgeX = point->x;
    edgeX = edgeX - startX;

    edgeZ = point->z;
    edgeZ = edgeZ - startZ;

    p_x -= startX;
    p_z -= startZ;

    edgeXCopy = edgeX;

    startZ = (edgeXCopy * edgeXCopy) + (edgeZ * edgeZ);
    startX = (p_x * edgeXCopy) + (p_z * edgeZ);

    edgeZ = startX;

    return ((startZ < edgeZ) && (edgeZ < 0.0f))
        || ((0.0f < edgeZ) && (edgeZ < startZ));
}


s32 stanIsInsideTriWorld(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 disp;
    s32 i;

    for (i = 0; i != 3; i++)
    {
        disp = stanTriEdgeDistWorld(tile,i,p_x,p_z);
        if (disp < 0) {
            return 0;
        }
    }

    return 1;
}


f32 stanTriEdgeDist2(StandTile *tile, s32 start3index, f32 p_x, f32 p_z)
{
    f32 len;
    f32 dx;
    f32 dz;
    s32 endindex;

    f32 cross;


    endindex = (start3index != 2) ? start3index + 1 : 0;

    start3index = (tile->tail.half >> (8 - (start3index << 2))) & 0xF;
    endindex = (tile->tail.half >> (8 - (endindex << 2))) & 0xF;

    dx = (f32)(tile->points[endindex].x - tile->points[start3index].x);
    dz = (f32)(tile->points[endindex].z - tile->points[start3index].z);

    len = sqrtf((dx * dx) + (dz * dz));

    if (len == 0.0f) {
        return 0.0f;
    }


    cross = (dz * (p_x - tile->points[start3index].x)) + ((p_z - tile->points[start3index].z) * -dx);
    return cross / len;
}


bool stanIsNearlyInsideTri(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 unk;
    s32 i;

    p_x *= g_StanScale;
    p_z *= g_StanScale;

    for (i = 0; i != 3; i++)
    {
        unk = stanTriEdgeDist2(tile, i, p_x, p_z);

        if (unk < -2)
        {
            return FALSE;
        }
    }

    return TRUE;
}


// A->B ACWS returns 1, CWS (including opposite) returns -1.
// Identical direction and |A| >= |B| returns 0
int stanCrossSign(f32 a_x,f32 a_z,f32 b_x,f32 b_z)
{
    // The main 2 cases : return the sign of AxB where it's non-zero
    if (a_z * b_x < a_x * b_z) {
        return 1;
    }
    if (a_x * b_z < a_z * b_x) {
        return -1;
    }

    // [AxB == 0 now]

    // If the vectors are opposite, default to clockwise
    if ((a_x * b_x < 0) || (a_z * b_z < 0)) {
      return -1;
    }

    // If A is shorter, return anti-clockwise
    if (a_x * a_x + a_z * a_z < b_x * b_x + b_z * b_z) {
        return 1;   // ACWS
    }

    // Identical direction, |A| >= |B|
    return 0;
}


/**
 * Test if two 2D line segments intersect.
 * 
 * Segment 1 runs from start1 -> end1. Segment 2 runs from start2 -> end2.
 * 
 * This function acts as a gate for stanSegmentIntersectFrac,
 * so callers can decide if it's worth computing where along segment 1 the crossing lands.
 */
bool stanSegmentsIntersect(f32 start1X, f32 start1Z, f32 end1X, f32 end1Z, f32 start2X, f32 start2Z, f32 end2X, f32 end2Z)
{
    f32 start1RelX;
    f32 start1RelZ;
    f32 seg1Dx;
    f32 seg1Dz;
 
    start1RelX = start1X - start2X;
    start1RelZ = start1Z - start2Z;
    seg1Dx = end1X - start1X;
    seg1Dz = end1Z - start1Z;
 
    return
        (
            (stanCrossSign(seg1Dx, seg1Dz, -start1RelX, -start1RelZ)
            * stanCrossSign(seg1Dx, seg1Dz, end2X - start1X, end2Z - start1Z)) < 1)
        &&
        (
            (stanCrossSign(end2X - start2X, end2Z - start2Z, start1RelX, start1RelZ)
            * stanCrossSign(end2X - start2X, end2Z - start2Z, end1X - start2X, end1Z - start2Z)) < 1)
        ;
}


#if defined(VERSION_US)
s32 stanSegmentsCross(f32 x1, f32 z1, f32 x2, f32 z2,
                      f32 x3, f32 z3, f32 x4, f32 z4, s32 limit) {
    f32 a_x;
    f32 a_z;
    f32 b_x;
    f32 b_z;
    f32 c_x;
    f32 c_z;

    s32 val1;
    s32 rc;
    s32 val2;
    s32 tmp;

    rc = 1;

    b_x = x1 - x3;
    b_z = z1 - z3;
    a_x = x2 - x1;
    a_z = z2 - z1;

    tmp = stanCrossSign(a_x, a_z, -b_x, -b_z);
    val1 = tmp * stanCrossSign(a_x, a_z, x4 - x1, z4 - z1);

    c_x = x4 - x3;
    c_z = z4 - z3;

    tmp = stanCrossSign(c_x, c_z, b_x, b_z);
    val2 = tmp * stanCrossSign(c_x, c_z, x2 - x3, z2 - z3);

    if (val1 >= limit) {
        rc = 0;
    }

    if (val2 >= limit) {
        rc = 0;
    }

    return rc;
}
#else
s32 stanSegmentsCross(f32 x1, f32 z1, f32 x2, f32 z2, f32 x3, f32 z3, f32 x4, f32 z4, s32 limit) {
    f32 a_z;
    f32 a_x;
    f32 b_z;
    f32 b_x;
    f32 c_x;

    s32 val2;
    s32 val1;
	f32 c_z;
    s32 rc;

    rc = 1;

    b_x = -(x1 - x3);
    b_z = -(z1 - z3);
	a_x = (x2 - x1);
    a_z = (z2 - z1);
    c_x = x4 - x1;
    c_z = z4 - z1;
    val1 = stanCrossSign(a_x, a_z, b_x, b_z) * stanCrossSign(a_x, a_z, c_x, c_z);

	a_x = (x4 - x3);
    a_z = (z4 - z3);
    b_x = (x1 - x3);
    b_z = (z1 - z3);
    c_x = x2 - x3;
    c_z = z2 - z3;

    val2 = stanCrossSign(a_x, a_z, b_x, b_z) * stanCrossSign(a_x, a_z, c_x, c_z);

    if (val1 >= limit) {
        rc = 0;
    }

    if (val2 >= limit) {
        rc = 0;
    }
    return rc;
}
#endif


bool stanWalkLine(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z, standTileWalkCallback_t callback, struct StandTileWalkCallbackRecord *callbackData)
{
    StandTile *tile;
    StandTile *previousTile;
    StandTile *previousPreviousTile;
    StandTile *linkedTile;
    StandTile *nextTile;
    f32 lineNegDz;
    f32 lineDx;
    s32 uninitialized;
    s32 edgeIndex;
    s32 crossings;
    s32 iterationCount;
    s32 savedPointIndex;
    StandTilePoint *nextPoint;
    StandTilePoint *curPoint;
    s32 nextPointIndex;
    s32 hasLink;

    start_x *= g_StanScale;
    start_z *= g_StanScale;
    dest_x *= g_StanScale;
    dest_z *= g_StanScale;

    tile = *tileStack;
    previousTile = *tileStack;
    previousPreviousTile = *tileStack;
    lineNegDz = -(dest_z - start_z);
    crossings = 0;
    nextTile = NULL;
    iterationCount = 0;
    lineDx = dest_x - start_x;

    savedPointIndex = uninitialized;\
    while (1)
    {
        crossings = 0;

        if (callback)
        {
            callback(tile, previousTile, callbackData);
        }

        curPoint = (StandTilePoint *) tile;

        for (edgeIndex = 0; edgeIndex < (tile->tail.hdrTail.pointCount & 0xF); edgeIndex++, curPoint++)
        {
            nextPointIndex = (edgeIndex + 1) % (tile->tail.hdrTail.pointCount & 0xF);
            nextPoint = &((StandTilePoint *) tile)[nextPointIndex];

            if (((lineNegDz * (nextPoint[1].x - curPoint[1].x)) + (lineDx * (nextPoint[1].z - curPoint[1].z))) <= 0.0f)
            {
                hasLink = curPoint[1].link >> 4 != 0;

                if (stanSegmentsCross(start_x, start_z, dest_x, dest_z, curPoint[1].x, curPoint[1].z, nextPoint[1].x, nextPoint[1].z, hasLink))
                {
                    linkedTile = &g_StanTiles[curPoint[1].link];
                    crossings++;

                    if (previousTile != linkedTile && previousPreviousTile != linkedTile)
                    {
                        savedPointIndex = edgeIndex;
                        nextTile = curPoint[1].link >> 4 != 0 ? linkedTile : NULL;
                    }

                    // Optimized away but required to allocate edgeIndex to the target register.
                    if (edgeIndex && edgeIndex && edgeIndex)
                    {
                    }
                }
            }
        }


        previousPreviousTile = previousTile;
        previousTile = tile;

        if (tile == nextTile)
        {
            crossings = 0;
        }

        tile = nextTile;

        if (crossings == 0)
        {
            return TRUE;
        }

        if (iterationCount++ >= 0x1f5 || nextTile == NULL || crossings == 0)
        {
            g_StanHitTile = previousTile;
            g_StanHitEdge = savedPointIndex;
            return FALSE;
        }

        *tileStack = nextTile;
    }
}


/**
 * Name: stanWalkLineNoCallback
*/
s32 stanWalkLineNoCallback(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z)
{
    return stanWalkLine(tileStack, start_x, start_z, dest_x, dest_z, 0, 0);
}


/**
 * Name: walkTilesBetweenPoints_NotingRooms
*/
s32 stanLineRooms(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 *rtnCountSize, s32 maxBufSize)
{
    struct StandTileWalkCallbackRecord callbackData;
    s32 rtn;


    callbackData.roomBuf = roomBuffer;
    callbackData.count = 0;
    callbackData.bufMax = maxBufSize;
    callbackData.lastRoom = -1;

    rtn = stanWalkLine(tileStack, start_x, start_z, dest_x, dest_z, stanLineRoomsCallback, &callbackData);

    *rtnCountSize = callbackData.count;
    return rtn;
}


/*
* Address: 0x0x7f0b0c98
*/
void stanLineRoomsCallback(StandTile *tile, StandTile *unused, struct StandTileWalkCallbackRecord *data)
{
    s32 newRoom;

    if (tile->room != data->lastRoom && data->count < data->bufMax)
    {
        newRoom = (s32)tile->room;
        *data->roomBuf = newRoom;
        data->lastRoom = newRoom;
        data->roomBuf += 1;
        data->count += 1;
    }

    return;
}


/**
 * Builds a list of room IDs between a start position and a destination position.
 * Only used for objects on set paths e.g. patrolling guards.
 */
s32 stanGetRoomsBetween(StandTile *tile, f32 start_x, f32 start_z, StandTile **destTile, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 maxBufSize)
{
    StandTile *savedTile;
    s32 count;

    savedTile = tile;
    count = 0;

    if (*destTile != NULL) {
        u8 roomA; // Source tile's room
        u8 roomB; // Destination tile's room

        roomA = tile->room;
        roomB = (*destTile)->room;

        // Fast path: start and destination tiles are in the same room, return a count of 1.
        if (roomB == roomA) {  
            roomBuffer[0] = roomA;
            return 1;
        }

        // Next fastest case: both rooms directly connected by a portal, write two room IDs and return a count of 2.
        if (bgRoomsSharePortal(roomA & 0xff, roomB & 0xff)) { 
            roomBuffer[0] = tile->room;
            roomBuffer[1] = (*destTile)->room;
            return 2;
        }
    }

    /**
     * Full path check needed. Find the rooms between the points, store them in roomBuffer, and save the number of rooms in count.
     * If the path check fails, return 0.
     */
    if (!stanLineRooms(&savedTile, start_x, start_z, dest_x, dest_z, roomBuffer, &count, maxBufSize)) {
        return 0;
    }

    if (maxBufSize < count) {
        count = maxBufSize;
    }

    if (*destTile == NULL) {
        *destTile = savedTile;
    }

    if (savedTile != *destTile) {
        return 0;
    }

    return count;
}


/**
 * Can change global variables:
 *
 * - g_StanSavedCollHasLine
 * - g_StanHitLineA
 * - g_StanHitLineB
 * - g_StanHitTile
 * - g_StanHitEdge
 * - g_StanHitProp
 *
 * US address 7F0B0E24.
 *
 * 'testLineUnobstructed'
*/
s32 stanTestLineUnobstructed(StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z, s32 cdtypes, f32 fromTop, f32 fromBottom, f32 toTop, f32 toBottom)
{
    struct PropRecord *prop;
    s32 retval;
    StandTile *tile;
    struct coord2d from;
    struct coord2d to;
    f32 hitFrac;
    s32 point_index;
    struct coord2d edgeA;
    struct coord2d edgeB;
    s32 blocked;
    s32 numrooms;
    s32 rooms[0x14];
    s32 checkHeight;
    s32 next;
    f32 topSlope;
    f32 bottomSlope;
    f32 startGroundY;
    s16 *propnum;
    coord2d *polygon;
    s32 numvertices0;
    //f32 unused2;
    s32 i;
    f32 frac;
    f32 propTop;
    f32 propBottom;
    f32 slope;
    s32 already_set;

    hitFrac = 1.0f;
    numrooms = 0;
    checkHeight = (fromBottom <= fromTop);
    already_set = 0;

    tile = *pTile;
    from.f[0] = p_x;
    from.f[1] = p_z;
    to.f[0] = dest_x;
    to.f[1] = dest_z;

    retval = stanLineRooms(&tile, p_x, p_z, dest_x, dest_z, &rooms[0], &numrooms, 0x14);


    if (numrooms > 20)
    {

        numrooms = 20;
    }

    if (retval == 0)
    {

        point_index = (g_StanHitEdge + 1) % (s32)((g_StanHitTile->tail.half >> 0xC) & 0xF);
        g_StanSavedCollHasLine = 1;

        g_StanHitLineA.f[0] = (f32) g_StanHitTile->points[g_StanHitEdge].x * g_StanInvScale;
        g_StanHitLineA.f[1] = (f32) g_StanHitTile->points[g_StanHitEdge].z * g_StanInvScale;

        g_StanHitLineB.f[0] = (f32) g_StanHitTile->points[point_index].x * g_StanInvScale;
        g_StanHitLineB.f[1] = (f32) g_StanHitTile->points[point_index].z * g_StanInvScale;

        hitFrac = stanSegmentIntersectFrac(&from, &to, &g_StanHitLineA, &g_StanHitLineB);
    }
    else
    {
        //
    }

    g_StanHitProp = NULL;

    if (cdtypes != 0)
    {
        rooms[numrooms] = -1;
        roomGetProps((s32 *)&rooms);

        for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
        {
            prop = &g_Props[*propnum];

            if (propIsOfCdType(prop, cdtypes) != 0)
            {
                propGetCollisionPolygon(prop, &polygon, &numvertices0, &propTop, &propBottom);

                if (numvertices0 > 0)
                {
                    for (i = 0; i < numvertices0; i++)
                    {
                        next = (i + 1) % numvertices0;

                        if (stanSegmentsIntersect(p_x, p_z, dest_x, dest_z, polygon[i].f[0], polygon[i].f[1], polygon[next].f[0], polygon[next].f[1]) != 0)
                        {
                            edgeA.f[0] = polygon[i].f[0];
                            edgeA.f[1] = polygon[i].f[1];
                            edgeB.f[0] = polygon[next].f[0];
                            edgeB.f[1] = polygon[next].f[1];

                            frac = stanSegmentIntersectFrac(&from, &to, &edgeA, &edgeB);

                            if (frac < hitFrac)
                            {
                                blocked = 1;

                                if (checkHeight != 0)
                                {
                                    if (already_set == 0)
                                    {
                                        already_set = 1;

                                        if (toBottom <= toTop)
                                        {
                                            topSlope = toTop - fromTop;
                                            bottomSlope = toBottom - fromBottom;
                                        }
                                        else
                                        {
                                            if (hitFrac < 1.0f)
                                            {
                                                dest_x -= p_x;
                                                dest_x *= hitFrac;
                                                dest_x = p_x + dest_x;

                                                dest_z -= p_z;
                                                dest_z *= hitFrac;
                                                dest_z = p_z + dest_z;
                                            }

                                            startGroundY = stanGetPositionYValue(*pTile, p_x, p_z);
                                            fromTop += startGroundY;
                                            fromBottom += startGroundY;
                                            slope = (stanGetPositionYValue(tile, dest_x, dest_z) - startGroundY) / hitFrac;
                                            bottomSlope = slope;
                                            topSlope = slope;
                                        }
                                    }

                                    if ((propTop <= ((bottomSlope * frac) + fromBottom)) || (((topSlope * frac) + fromTop) <= propBottom))
                                    {
                                        blocked = 0;
                                    }
                                }

                                if (blocked != 0)
                                {
                                    retval = 0;
                                    hitFrac = frac;
                                    g_StanSavedCollHasLine = 1;
                                    g_StanHitLineA = edgeA;
                                    g_StanHitLineB = edgeB;
                                    g_StanHitTile = NULL;
                                    g_StanHitEdge = 0;
                                    g_StanHitProp = prop;
                                    tile = NULL;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (tile == NULL)
    {
        tile = *pTile;

        dest_x -= p_x;
        dest_x *= hitFrac;
        dest_x = p_x + dest_x;

        dest_z -= p_z;
        dest_z *= hitFrac;
        dest_z = p_z + dest_z;
        /*stanlineret = */ stanWalkLineNoCallback(&tile, p_x, p_z, dest_x, dest_z);
    }

    *pTile = tile;
    g_StanHitFrac = hitFrac;

    return retval;
}


PropRecord *stanLineFindDoor(StandTile *t, f32 start_x, f32 start_z, f32 end_x, f32 end_z, s32 cdtypes)
{
    f32 frac;
    PropRecord *prop;
    coord2d lineStart;
    coord2d lineEnd;
    struct coord2d *tmp;
    coord2d edgeStart;
    coord2d edgeEnd;
    s32 i;
    StandTile *tile;
    s32 roomCount;
    s32 roomBuffer[21];
    s16 *propIndexPtr;
    coord2d *polygon;
    s32 numEdges;
    PropRecord *bestProp;
    s32 next;
    f32 bestFrac;

    bestProp = NULL;
    bestFrac = 1.0f;

    tile = t;
    roomCount = 0;

    stanLineRooms(&tile, start_x, start_z, end_x, end_z, roomBuffer, &roomCount, 20);

    if (roomCount >= 21)
    {
        // The comment below was in the unmatched function. I have left it where I think it was most likely meant to go.
        //osSyncPrintf("stanLineDoor: %d rooms is more than %d\n");
        roomCount = 20;
    }

    lineStart.f[0] = start_x;
    lineStart.f[1] = start_z;
    lineEnd.f[0] = end_x;
    lineEnd.f[1] = end_z;

    if (cdtypes != 0)
    {
        roomBuffer[roomCount] = -1;
        roomGetProps(roomBuffer);

        propIndexPtr = g_RoomPropsFound;


        if (*propIndexPtr >= 0)
        {
            do
            {
                prop = &g_Props[*propIndexPtr];

                if (propIsOfCdType(prop, cdtypes))
                {
                    propGetCollisionPolygon2d(prop, &polygon, &numEdges);

                    if (numEdges > 0)
                    {
                        i = 0;

                        while (i < numEdges)
                        {
                            next = (i + 1) % numEdges;

                            if (stanSegmentsIntersect(start_x, start_z, end_x, end_z, polygon[i].f[0], polygon[i].f[1], polygon[next].f[0], polygon[next].f[1]))
                            {
                                edgeStart.f[0] = polygon[i].f[0];
                                tmp = &polygon[i];
                                edgeStart.f[1] = (*tmp).f[1];

                                edgeEnd.f[0] = polygon[next].f[0];
                                edgeEnd.f[1] = polygon[next].f[1];

                                frac = stanSegmentIntersectFrac(&lineStart, &lineEnd, &edgeStart, &edgeEnd);

                                if (frac < bestFrac)
                                {
                                    bestFrac = frac;
                                    bestProp = prop;
                                }
                            }

                            i++;
                        }
                    }
                }

                propIndexPtr++;
            }
            while (*propIndexPtr >= 0);
        }
    }

    return bestProp;
}


/**
 * Computes the signed perpendicular distance from point P to the infinite
 * line that goes through point A and point B.
 * The sign indicates which side of the line the point is on.
 */
f32 stanGetSignedPointLineDistance(f32 a_x, f32 a_z, f32 b_x, f32 b_z, f32 p_x, f32 p_z)
{
    f32 result; //d

    result = sqrtf((b_x - a_x) * (b_x - a_x) + (b_z - a_z) * (b_z - a_z));

    if (result == 0.0f)
    {
        return sqrtf((p_x - b_x) * (p_x - b_x) + (p_z - b_z) * (p_z - b_z));
    }
    return ((b_z - a_z) * (p_x - a_x) + -(b_x - a_x) * (p_z - a_z)) / result;
}


f32 stanDist2d(f32 o_x,f32 o_z,f32 p_x,f32 p_z)
{
    p_x -= o_x;
    p_z -= o_z;
    return sqrtf(p_x * p_x + p_z * p_z);
}


/**
 * Tests whether a point P's perpendicular projection onto the infinite line
 * going through points A and B falls inside the finite edge segment.
 */
bool stanPointProjectsOntoEdge(f32 a_x, f32 a_z, f32 b_x, f32 b_z, f32 p_x, f32 p_z)
{
    f32 f0;
    f32 f2;
    f32 f16;
    f32 f18;

    p_x -= a_x;
    p_z -= a_z;

    f0 = b_x - a_x;
    f2 = b_z - a_z;

    f16 = p_x * f0 + p_z * f2;
    f18 = f0 * f0 + f2 * f2;

    return (f18 < f16 && f16 < 0) || (f16 > 0 && f16 < f18);
}


/**
 * Can change global variables
 * - g_StanSavedCollHasLine
 * - g_StanHitLineA
 * - g_StanHitLineB
 * - g_StanHitTile
 * - g_StanHitEdge
 * - g_StanHitProp
 *
 * US address 7F0B18B8.
 * Perfect Dark cdTestVolume (from context)
*/
s32 stanTestVolume(StandTile **tile, f32 x, f32 z, f32 radius, s32 cdtypes, f32 top, f32 bottom)
{
    s32 i;
    f32 dist;
    f32 bestDist;
    s32 result;
    s32 next;

    s32 checkHeight;
    f32 groundY;
    s16 *propnum;
    s32 numrooms;
    struct PropRecord *prop;
    s32 rooms[0x14];
    coord2d *polygon;
    s32 numvertices0;
    f32 distB;
    f32 distA;
    f32 propTop;
    f32 propBottom;


    checkHeight = (bottom <= top);

    numrooms = 0;

    result = stanCircleRooms(tile, x, z, radius, &rooms[0], &numrooms, 20);
    if (result >= 0)
    {
        return result;
    }


    if (numrooms > 20)
    {
        numrooms = 20;
    }

    g_StanHitProp = NULL;

    if (cdtypes)
    {
        if (checkHeight)
        {
            groundY = stanGetPositionYValue(*tile, x, z);
            top += groundY;
            bottom += groundY;
        }

        rooms[numrooms] = -1;
        roomGetProps(&rooms[0]);

        for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
        {
            prop = &g_Props[*propnum];

            if (propIsOfCdType(prop, cdtypes) != 0)
            {
                propGetCollisionPolygon(prop, &polygon, &numvertices0, &propTop, &propBottom);
                if ((numvertices0 > 0) && ((checkHeight == 0) || ((propBottom <= top) && (bottom <= propTop))))
                {
                    bestDist = -1.0f;

                    i=0;
                    while(1)
                    {
                        next = (i + 1) % numvertices0;

                        dist = stanGetSignedPointLineDistance(polygon[i].f[0], polygon[i].f[1], polygon[next].f[0], polygon[next].f[1], x, z);

                        if (dist < 0.0f)
                        {
                            dist = -dist;
                        }

                        if (bestDist < dist)
                        {
                            distA = stanDist2d(polygon[i].f[0], polygon[i].f[1], x, z);
                            distB = stanDist2d(polygon[next].f[0], polygon[next].f[1], x, z);

                            if ((dist < radius)
                                && (
                                    (distA < radius)
                                    || (distB < radius)
                                    || (stanPointProjectsOntoEdge(polygon[i].f[0], polygon[i].f[1], polygon[next].f[0], polygon[next].f[1], x, z) != 0)))
                            {
                                g_StanSavedCollHasLine = 1;
                                bestDist = dist;

                                g_StanHitLineA.f[0] = polygon[i].f[0];
                                g_StanHitLineA.f[1] = polygon[i].f[1];
                                g_StanHitLineB.f[0] = polygon[next].f[0];
                                g_StanHitLineB.f[1] = polygon[next].f[1];
                                g_StanHitTile = NULL;
                                g_StanHitEdge = 0;
                                g_StanHitProp = prop;
                            }
                        }

                        if (next == 0)
                        {
                            break;
                        }

                        i = next;
                    }

                    if (bestDist > -1.0f)
                    {
                        return 2;
                    }
                }
            }
        }
    }

    return -2;
}


//stanResetHits
void stanResetHits(void) {
    g_StanHitTile = 0;
    g_StanHitEdge = 0;
    g_StanSavedCollHasLine = 0;
}


void stanGetEdgePoints(StandTile *tile, s32 pointI, coord3d *currPntRtn, coord3d *nextPointRtn)
{
    f32 scale;

    scale = g_StanInvScale;

    currPntRtn->x = tile->points[pointI].x * scale;
    currPntRtn->y = tile->points[pointI].y * scale;
    currPntRtn->z = tile->points[pointI].z * scale;

    /**
     * This line could potentially become:
     * pointI = (pointI + 1) % STAN_POINT_COUNT(tile);
     * 
     * If STAN_POINT_COUNT were redefined as:
     * #define STAN_POINT_COUNT(tile) (((tile)->tail.half >> 12) & 0xf)
     * Something to consider?
     */
    pointI = (pointI + 1) % ((tile->tail.half >> 12) & 0xf);

    nextPointRtn->x = tile->points[pointI].x * scale;
    nextPointRtn->y = tile->points[pointI].y * scale;
    nextPointRtn->z = tile->points[pointI].z * scale;
}


/**
 * Traverses linked stan tiles that intersect a circle at the given X/Z
 * position. The callbacks can observe visited tiles, reject linked edges, or
 * allow traversal to continue after encountering a blocking edge.
 *
 * @return STAN_COLLISION_FOUND when a blocking edge is found,
 * STAN_COLLISION_TRAVERSAL_LIMIT when the tile stack limit is reached, or
 * STAN_COLLISION_NONE when traversal completes without a collision.
 */
StanCollisionResult stanWalkCircle(StandTile **startTile, f32 x, f32 z, f32 radius, standTileLocusCallback_A_t callbackA, standTileLocusCallback_B_t callbackB, standTileLocusCallback_C_t callbackC, struct StandTileLocusCallbackRecord *record)
{
    s32 i;
    // cat is only checked against 41 after a whole tile is walked, and each tile can push up to 15 more
    StandTile *tileStack[41 + 15];
    StandTile *tile;
    StandTile *linkedTile;
    s32 visitedCount;
    s32 cat;
    s32 pointI;
    s32 pointINext;
    s32 nextPointI;
    f32 edgeDist;
    f32 pointDistA;
    f32 pointDistB;

    x *= g_StanScale;
    z *= g_StanScale;
    radius *= g_StanScale;
    visitedCount = 0;
    cat = 1;
    tileStack[0] = *startTile;

    do
    {
        tile = tileStack[visitedCount++];
        pointI = 0;

        if (callbackA != NULL)
        {
            callbackA(tile, record);
        }

        if (((tile->tail.half >> 12) & 0xf) > 0)
        {
            do
            {
                pointINext = pointI;
                do
                {
                    i = cat;
                    pointINext = pointINext + 1;

                    nextPointI = pointINext;
                    nextPointI %= (tile->tail.half >> 12) & 0xf;
                    edgeDist = stanEdgeDist(tile, pointI, x, z);
                    pointDistA = stanPointDist(tile, pointI, x, z);
                    pointDistB = stanPointDist(tile, nextPointI, x, z);

                    if (edgeDist < radius && (pointDistA < radius || pointDistB < radius || stanPointProjectsOntoTileEdge(tile, pointI, x, z)))
                    {
                        if ((callbackB == NULL || !callbackB(tile, pointI, edgeDist, pointDistA, pointDistB, record)) && (tile->points[pointI].link >> 4))
                        {
                            linkedTile = (StandTile *)((u8 *)g_StanTiles + (tile->points[pointI].link << 3));
                            for (i = cat - 1; i >= 0; i--)
                            {
                                if (linkedTile == tileStack[i])
                                {
                                    goto next_point;
                                }
                            }
                            tileStack[cat] = linkedTile;
                            cat++;
                            goto next_point;
                        }

                        g_StanHitTile = tile;
                        g_StanHitEdge = pointI;
                        if (callbackC != NULL && callbackC(tileStack, cat, record) == 1)
                        {
                            goto next_point;
                        }
                        return STAN_COLLISION_FOUND;
                    }
                    next_point:
                    pointI = pointINext;
                }
                while (FALSE);
            }
            while (pointINext < (((*tile).tail.half >> 12) & 0xf));
        }

        // if (cat >= 0x320)
        // {
        //     return 5; //error value?
        // }

        if (((u32) cat) >= 41)
        {
            return STAN_COLLISION_TRAVERSAL_LIMIT;
        }
    }
    while (visitedCount < cat);

    visitedCount = radius;

    if (callbackC != NULL)
    {
        callbackC(tileStack, cat, record);
    }

    return STAN_COLLISION_NONE;
}


s32 stanCircleLegal(StandTile **tileStack, f32 target_x, f32 target_z, f32 unknown) {
    return stanWalkCircle(tileStack, target_x, target_z, unknown, NULL, NULL, NULL, NULL);
}


/**
 * Callback for stan locus traversal.
 * 
 * Adds the current tile's room ID to the caller-provided room list if it
 * has not already been recorded. The callback always returns 0 so traversal
 * continues.
 */
s32 stanLocusAddTileRoomIfNew(StandTile *tile, struct StandTileLocusCallbackRecord *rec)
{
    s32 roomCount;
    s32 room;
    StandTile *t;
    struct StandTileLocusCallbackRecord *record;
    s32 i;
    s32 *ptr;
    
    record = rec;
    i = 0;
    roomCount = record->count;
    t = tile;
    
    // Only search for duplicates if more than 0 rooms have been collected.
    if (roomCount > 0)
    {
        room = t->room;
        ptr = record->rooms;
        
        //If the tile's room is already in the room list, return immediately.
        do
        {
            if (room == (*ptr))
            {
                return 0;
            }
            
            i++;
            ptr++;
        }
        while (i < record->count);
    }
    
    // The room has not been collected yet so append it.
    if (roomCount < rec->bufMax)
    {
        rec->rooms[roomCount] = tile->room;
        rec->count = rec->count + 1;
    }
    
    return 0;
}


s32 stanCircleRoomsHitCallback(StandTile **tileStack, s32 stackHeight, struct StandTileLocusCallbackRecord* data) {
    data->nearEdgeCount += 1;
    return 1;
}


s32 stanCircleRooms(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius, s32 *rooms, s32 *count_rtn, s32 bufMax)
{
    struct StandTileLocusCallbackRecord data;
    s32 rtn;

    data.rooms = rooms;
    data.count = 0;
    data.bufMax = bufMax;
    data.nearEdgeCount = 0;

    rtn = stanWalkCircle(tileStack, target_x, target_z, radius,
        stanLocusAddTileRoomIfNew, NULL, stanCircleRoomsHitCallback, &data
    );

    *count_rtn = data.count;

    if (1 < data.nearEdgeCount) {
        return 2;
    }

    return rtn;
}


s32 stanCircleCrouchCallback(StandTile *tile, struct StandTileLocusCallbackRecord *record)
{
    s32 val = tile->mid.half >> 0xC;
    if (g_StanTileSpecialFlags[val] & STANTILEFLAG_FORCECROUCH)
    {
        record->crouch = 1;
    }

    return 0;
}


s32 stanCircleLadderCallback(StandTile *tile, s32 pointIdx, f32 edgeDist, f32 pointDistA, f32 pointDistB, struct StandTileLocusCallbackRecord *record)
{
    u16 link;
    StandTile *target;
    s32 mid;

    link = tile->points[pointIdx].link;

    if ((link >> 4) != 0) {
        target = (StandTile *)(link + (StandTile *)g_StanTiles);

        mid = target->mid.half;

        if (g_StanTileSpecialFlags[mid >> 0xc] & STANTILEFLAG_FORCECROUCH) {
            record->crouch = 1;
            return 1;
        }

        mid = target->mid.half;

        if (g_StanTileSpecialFlags[mid >> 0xc] & STANTILEFLAG_LADDER) {
            g_StanLadderTile = target;
            record->ladder = 1;
            return 0;
        }
    }

    return 0;
}


s32 stanCircleGetSpecial(StandTile **tile, f32 x, f32 z, f32 radius, struct StandTileLocusCallbackRecord *records)
{
    *records = (struct StandTileLocusCallbackRecord){0};

    return stanWalkCircle(tile, x, z, radius, stanCircleCrouchCallback, stanCircleLadderCallback, NULL, records);
}


s32 stanLocusIsCrouch(struct StandTileLocusCallbackRecord *record)
{
    return record->crouch;
}


s32 stanLocusIsLadder(struct StandTileLocusCallbackRecord *record)
{
    return record->ladder;
}


void stanGetTileOrderedPointWorldPos(StandTile *tile, s32 pointnum, coord3d *out)
{
    StandTilePoint *point;
    f32 scale;

    pointnum = tile->tail.half >> (8 - (pointnum * 4));

    point = &tile->points[pointnum & 0xf];

    scale = g_StanInvScale;

    out->x = point->x * scale;
    out->y = point->y * scale;
    out->z = point->z * scale;
}


void stanGetMoveBondCollisionTiles(StandTile **tile1, StandTile **tile2, struct move_bond_collision *quad)
{
    coord3d *coords = quad->corners;
    StandTile *curtileStore;
    StandTile *baseTile;
    StandTile *linktile;
    s32 curtilePointI;
    s32 i;
    s32 j;
    s32 k;
    s32 target;

    baseTile = g_StanLadderTile;

    target = baseTile->tail.hdrTail.pointCount & 0xf;

    i = 0;

    if (i < target)
    {
        do
        {
            linktile = (StandTile *)((u8 *)g_StanTiles + (baseTile->points[i].link << 3));

            if ((baseTile->points[i].link >> 4) != 0)
            {
                s32 linkTileMid;
            
                linkTileMid = linktile->mid.half;
            
                if (g_StanTileSpecialFlags[linkTileMid >> 12] & STANTILEFLAG_LADDER)
                {
                    curtilePointI = (i + 2) % 3;
            
                    *tile1 = baseTile;
                    *tile2 = linktile;
            
                    j = 0;
                    curtileStore = baseTile;

                    while (1)
                    {
                        for (k = 0; k < 3; k++)
                        {
                            stanGetTileOrderedPointWorldPos(
                                linktile,
                                ((j >> 2) + k) % 3,
                                &coords[(j + k) & 3]);
                        }

                        stanGetTileOrderedPointWorldPos(
                            curtileStore,
                            curtilePointI,
                            &coords[(j + 3) & 3]);

                        j++;

                        if (j == 12)
                        {
                            break;
                        }

                        if (!(coords[2].y < coords[0].y)
                                && !(coords[2].y < coords[1].y)
                                && !(coords[3].y < coords[0].y)
                                && !(coords[3].y < coords[1].y))
                        {
                            break;
                        }
                    }

                    return;
                }
            }

            i++;

            if (i < target)
            {
                continue;
            }

            break;
        }
        while (TRUE);
    }
}


/**
 * Callback function.
 * 
 * For a given edge, return true if the edge is vertically above yThreshold.
 */
s32 stanLocusEdgeIsAboveY(StandTile *tile, s32 edgeIndex, f32 edgeDist, f32 distToPointA, f32 distToPointB, struct StandTileLocusCallbackRecord *record)
{
    s32 pointCount = (tile->tail.half >> 12) & 0xf;
    s32 nextIndex = (edgeIndex + 1) % pointCount;

    return record->yThreshold < (f32)tile->points[edgeIndex].y
        && record->yThreshold < (f32)tile->points[nextIndex].y;
}


/**
 * US address 7F0B26B8.
*/
s32 stanTestLocusEdgeAboveY(StandTile **tile, f32 target_x, f32 target_z, f32 radius, f32 yThreshold)
{
    struct StandTileLocusCallbackRecord record = { .yThreshold = yThreshold * g_StanScale };

    return stanWalkCircle(tile, target_x, target_z, radius, NULL, stanLocusEdgeIsAboveY, NULL, &record);
}


typedef struct BfsSearchLocals {
    StandTile **stackptr;
    s32 pad48;
    s32 pad4c;
    s32 pad50;
    s32 pad54;
    s32 lastnumtiles;
} BfsSearchLocals;


// Horrifc BFS on tiles
//
// Four things look like they could be improved
// 1. The outer loop always restarts at g_StanFillStack[0].
//    The next 'wave' will process tiles it already scanned in the second wave
//    Neighbors are checked again needlessly
//    This has a high cost if the waypoint the game is trying to find
//    is far away on the stans
//    Should reset to the previous value of loc.lastnumtiles
// 2. 'seenCount' can become really big because it's all the stans discovered so far
//    Neighbor checks thus become exponentially expensive
//    There could be a faster way to check that a stan was already visited
// 3. The closest pad to a tile is something that could be precomputed
//    because it's static and never changes at run-time
// 4. This function is called at one single location and the second arg
//    is always the same. It could return the result from tilePred already.
StandTile *stanFillSearch(StandTile *starttile, tilePredicate_t predicate) // stanFillSearch is the canonical name for this function
{
    StandTile **stackbase;
    BfsSearchLocals loc;
    StandTile **tileStartAddr;
    StandTilePoint *point;
    StandTile *linkedtile;
    s32 seenCount;
    s32 pointindex;
    s32 pointcount;
    s32 link;
    s32 i;
    s32 stackindex;
    StandTile **tileStack;

    if (predicate(starttile))
    {
        return starttile;
    }

    tileStartAddr = &g_StanTiles;

    // Add the starting tile to the discovered tile stack.
    g_StanFillStack[0] = starttile;
    seenCount = 1;

    // Process the discovered tiles in waves. loc.lastnumtiles is the number of tiles that existed at the beginning of the current wave.
    for (loc.lastnumtiles = 1, tileStack = g_StanFillStack, stackbase = tileStack;; loc.lastnumtiles = seenCount)
    {
        stackindex = 0;

        if (seenCount > 0)
        {
            // This restarts at the beginning of the stack for every wave rather than beginning at the new frontier.
            loc.stackptr = tileStack;

            do
            {
                pointindex = 0;
                starttile = *loc.stackptr;
                point = (StandTilePoint *)starttile;
                pointcount = (starttile->tail.half >> 12) & 0xf;

                // Loop over the tile's linked points.
                if (pointcount > 0)
                {
                    do
                    {
                        link = point[1].link;

                        // This assignment is logically redundant but needed for matching.
                        tileStartAddr = &g_StanTiles;

                        linkedtile = (StandTile *)(((u8 *)(*tileStartAddr)) + (link << 3));

                        // A zero high portion indicates that there is no valid linked tile to visit.
                        if ((link >> 4) != 0)
                        {
                            // Loop to see if this is a new tile (disgusting)
                            for (i = 0; i < seenCount; i++)
                            {
                                if (linkedtile == tileStack[i])
                                {
                                    goto nextpoint;
                                }
                            }

                            // Return as soon as a matching tile is found.
                            if (predicate(linkedtile))
                            {
                                return linkedtile;
                            }

                            // Add the newly discovered tile to the stack.
                            stackbase[seenCount] = linkedtile;
                            seenCount++;

                            if ((u32)seenCount >= 351)
                            {
                                return 0;
                            }

                            pointcount = (starttile->tail.half >> 12) & 0xf;
                        }

nextpoint:
                        pointindex++;
                        point++;
                    }
                    while (pointindex < pointcount);
                }

                stackindex++;
                loc.stackptr++;
            }
            while (stackindex < loc.lastnumtiles);
        }


        // We only continue if we made progress with this iteration
        if (seenCount != loc.lastnumtiles)
        {
            // This is logically redundant because both pointers refer to g_StanFillStack, but it's still needed for matching.
            stackbase = tileStack;

            continue;
        }

        // No new tiles were discovered so the search is exhausted.
        return 0;
    }
}


/**
 * @param pntA: out parameter, will contain g_StanHitLineA (x,z)
 * @param pntB: out parameter, will contain g_StanHitLineB (x,z)
 */
bool stanGetCollisionEdge(coord3d *pntA, coord3d *pntB)
{
    if (g_StanHitTile)
    {
        stanGetEdgePoints(g_StanHitTile, g_StanHitEdge, pntA, pntB);

        return TRUE;
    }
    else
    {
        if (g_StanSavedCollHasLine)
        {
            pntA->x = g_StanHitLineA.f[0];
            pntA->y = 0;
            pntA->z = g_StanHitLineA.f[1];

            pntB->x = g_StanHitLineB.f[0];
            pntB->y = 0;
            pntB->z = g_StanHitLineB.f[1];

            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
}


void stanSetLevelScale(f32 ls)
{
    g_StanScale = ls;
    g_StanInvScale = (1.0f / ls);
    return;
}


/**
 * Calculates y value on a tile, according to (x,z) position.
 */
f32 stanGetPositionYValue(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 a[3]; // edge vector a
    f32 b[3]; // edge vector b
    s64 cp[3]; // cross product a x b (the plane normal)
    s64 rsum;
    s32 indexE;
    s32 indexD;
    s32 indexC;

    p_x *= g_StanScale;
    indexD = STAN_TAIL_D(tile);
    indexC = STAN_TAIL_C(tile);
    indexE = STAN_TAIL_POINT_COUNT(tile);
    p_z *= g_StanScale;

    a[0] = (f32) (tile->points[indexC].x - tile->points[indexD].x);
    a[1] = (f32) (tile->points[indexC].y - tile->points[indexD].y);
    a[2] = (f32) (tile->points[indexC].z - tile->points[indexD].z);

    b[0] = (f32) (tile->points[indexE].x - tile->points[indexD].x);
    b[1] = (f32) (tile->points[indexE].y - tile->points[indexD].y);
    b[2] = (f32) (tile->points[indexE].z - tile->points[indexD].z);

    // implicit call to __f_to_ll
    // This is the cross product, a x b
    cp[0] = (s64)((a[1] * b[2]) - (a[2] * b[1]));
    cp[1] = (s64)((a[2] * b[0]) - (a[0] * b[2]));
    cp[2] = (s64)((a[0] * b[1]) - (a[1] * b[0]));

    // implicit call to __ll_mul
    rsum = ((s64)cp[0] * (s64)tile->points[indexD].x)
        + ((s64)cp[1] * (s64)tile->points[indexD].y)
        + ((s64)cp[2] * (s64)tile->points[indexD].z);

    // don't divide by zero
    if (cp[1] == 0)
    {
        return (f32) tile->points[indexD].y * g_StanInvScale;
    }

    return (f32) ((((f64)(rsum) - ((f64) p_x * (f64)cp[0])) - ((f64) p_z * (f64)cp[2])) / (f64)(cp[1])) * g_StanInvScale;
}


void stanGetTileColour(StandTile *tile, f32 p_x, f32 p_z, u8 *rtn)
{
    u8 B = (tile->mid.half >> 0x8) & 0xF;
    u8 C = (tile->mid.half >> 0x4) & 0xF;
    u8 D = (tile->mid.half >> 0x0) & 0xF;
    rtn[0] = (B << 0x4) | B;
    rtn[1] = (C << 0x4) | C;
    rtn[2] = (D << 0x4) | D;
}


 /**
 * Get 24bit id stanIdHi from id string
 * @param stanIdHi: 1bit Type, 15bit Integer ID.
 * @param stanIdLo: 5bit stanIdLo File (a-z) and 3bit subtri 0-7
 * canonically Named
 */
void stanPackId(const char *id, u16 *stanIdHi, u8 *stanIdLo)
{
    u32   bitsnumber;
    char *str_end;
    s32   bitsletter;
    s32   bitsfile;
    s32   bitssubtri;
    s32   var1;

    var1 = id[0] - 'p';

    if (var1 < 0 || var1 > 1) // is p or q the first char (q never used?)
    {
    }
    else
    {
        bitsletter = id[0] - 'p'; // yes this is right, Im duplicated.
        bitsnumber = strtol(id + 1, &str_end, 10);

        if (id == str_end - 1)
        {
        }
        else if (bitsnumber <= 32767)
        {
            if (str_end[0] - 'a' < 0 || str_end[0] - 'a' >= 26)
            {
            }
            else
            {
                bitsfile = str_end[0] - 'a';
                bitssubtri = str_end[1];

                if (bitssubtri != 0 && bitssubtri != '0')
                {
                    bitssubtri -= '0';
                }

                if (bitssubtri < 0 || bitssubtri >= 8)
                {
                }
                else
                {
                    if (str_end[1] == 0 || str_end[2] == 0)
                    {
                        *stanIdHi = bitsletter << 0xf | bitsnumber;
                        *stanIdLo = bitsfile << 3 | bitssubtri;
                        return;
                    }
                    else
                    {
                    }
                }
            }
        }
        else
        {
        }
    }
    *stanIdHi = -1;
    *stanIdLo = -1;
    return;
}


StandTile *stanMatchTileName(const char *id)
{
    StandTile *tile;
    u16 stanIdHi;
    u8 stanIdLo;

    if (*id == '\0') {
        return NULL;
    }

    stanPackId(id, &stanIdHi, &stanIdLo);

    for (tile = g_StanFile->ptr_firstroom; *(u32 *)tile != 0;
         tile = (StandTile *)((u8 *)tile + g_StanTileSizes[(tile->tail.half >> 12) & 0xf])) {
        if ((tile->id >> 8) == stanIdHi && (u8)tile->id == stanIdLo) {
            return tile;
        }
    }

    return NULL;
}


void stanDetermineEOF(struct StanPrefixRecord *file /* canonically r */, s32 origBase, u8 *newBase)
{
    s32 delta;
    void **roomPtr;
    StandTile *tile;
    
    delta = ((s32) newBase) - origBase;
    g_StanFile = file;
    
  
    g_StanTiles = (StandTile *)(((s32)file->ptr_firstroom + delta) - 0x80);
    g_StanFirstTileAddr = (s32)file->ptr_firstroom + delta;
    
    roomPtr = (void **)&file->ptr_firstroom;
    
    if (file->ptr_firstroom != NULL)
    {
        do
        {
            *roomPtr = (void *) ((s32) (*roomPtr) + delta);
            roomPtr++;
        }
        while (*roomPtr != NULL);
    }
    
    tile = (StandTile *) (roomPtr + 1);
    
    if ((*(s32 *) tile) != 0)
    {
        do
        {
            g_StanTilesEnd = tile;
            tile = (StandTile *)((u8 *)tile + g_StanTileSizes[(tile->tail.half >> 0xc) & 0xf]);
        } 
        while (*(s32 *) tile != 0);
    }
    
    g_StanFile = file;
}


/**
 Get the room the tile belongs to
 @param tile: Tile to quiry
 @return the room number the tile is located in
 @exception Although room is u8, this needs to be s32 for matching ai.
 */
s32 stanGetTileRoom(StandTile *tile)
{
    return tile->room;
}


//incorrect here so that both this and bgGetRoomValue match

Gfx *stanDebugDrawTile(Gfx *ptrdl, StandTile *tile, u32 RGBAColor) {
    return ptrdl;
}

Gfx * stanDebugBeginDraw(Gfx *gdl) {
    return gdl;
}


f32 stanSegmentIntersectFrac(coord2d *start1, coord2d *end1, coord2d *start2, coord2d *end2)
{
    f32 deltaX = start2->f[1] - end2->f[1];
    f32 deltaY = end2->f[0] - start2->f[0];
    f32 intersectionFactor = (start2->f[1] - start1->f[1]) * deltaY + (start2->f[0] - start1->f[0]) * deltaX;
    f32 totalLineLength = (end1->f[1] - start1->f[1]) * deltaY + (end1->f[0] - start1->f[0]) * deltaX;

    if (totalLineLength == 0.0f)
    {
        return 1.0f;
    }

    intersectionFactor /= totalLineLength;

    if (intersectionFactor < 0.0f || intersectionFactor > 1.0f)
    {
        return 1.0f;
    }

    return intersectionFactor;
}


f32 stanPointLineDist(struct coord3d *point3D, coord2d *lineStart, coord2d *lineEnd)
{
    f32 projectedDistance;
    f32 perpendicularDistance;
    f32 distanceSquared;
    f32 deltaX;
    f32 deltaY;

    deltaX = lineEnd->x - point3D->y;
    deltaY = lineEnd->y - point3D->z;

    perpendicularDistance = deltaY * lineStart->x - deltaX * lineStart->y;
    projectedDistance = deltaX * lineStart->x + deltaY * lineStart->y;

    distanceSquared = (point3D->x - perpendicularDistance) * (point3D->x + perpendicularDistance);

    if (distanceSquared < 0.0f)
    {
        return FLT_MAX;
    }

    projectedDistance -= sqrtf(distanceSquared);

    if (projectedDistance < 0.0f)
    {
        if (projectedDistance * projectedDistance + perpendicularDistance * perpendicularDistance <= point3D->x * point3D->x)
        {
            return 0.0f;
        }

        return FLT_MAX;
    }

    return projectedDistance;
}


f32 stanRaySegmentFrac(coord3d *point3D, coord2d *lineStart, coord2d *lineEnd, coord2d *direction)
{
    f32 directionLength ;
    f32 tempCrossProduct;
    coord2d normalizedDir;
    f32 deltaY;
    f32 deltaX;
    f32 lineLength;
    f32 inverseLineLength ;
    f32 normalY;
    f32 normalX;
    f32 proj1;
    f32 proj2;
    f32 parallelLineStartY;
    f32 parallelLineStartX;
    f32 parallelLineEndY;
    f32 parallelLineEndX;
    f32 crossProductPoint;
    f32 crossProductStart;
    f32 crossProductEnd;
    f32 intersectionFactor;
    f32 intersectionFactorLineEnd;
    f32 startProjection;
    f32 endProjection;

    directionLength  = sqrtf(direction->x * direction->x + direction->y * direction->y);

    if (directionLength  == 0.0f)
    {
        return 1.0f;
    }

    normalizedDir.x = direction->x * (1.0f / directionLength );
    normalizedDir.y = direction->y * (1.0f / directionLength );

    deltaX = lineEnd->x - lineStart->x;
    deltaY = lineEnd->y - lineStart->y;

    lineLength = sqrtf(deltaX * deltaX + deltaY * deltaY);

    if (lineLength == 0.0f)
    {
        goto handlezero;
    }

    inverseLineLength  = 1.0f / lineLength;
    normalX = deltaY * inverseLineLength ;
    normalY = -deltaX * inverseLineLength ;

    proj1 = point3D->x * normalX;
    proj2 = point3D->x * normalY;

    if (proj1 * (point3D->y - lineStart->x) + proj2 * (point3D->z - lineStart->y) < 0.0f)
    {
        proj1 = -proj1;
        proj2 = -proj2;
    }

    parallelLineStartX = lineStart->x + proj1;
    parallelLineStartY = lineStart->y + proj2;
    parallelLineEndX = lineEnd->x + proj1;
    parallelLineEndY = lineEnd->y + proj2;

    crossProductStart = (direction->y * parallelLineStartX) - (parallelLineStartY * direction->x);
    crossProductPoint = (point3D->y * direction->y) - (point3D->z * direction->x);
    crossProductEnd = (direction->y * parallelLineEndX) - (parallelLineEndY * direction->x);

    if (crossProductEnd < crossProductStart)
    {
        coord2d *tmp;

        tempCrossProduct = crossProductStart;
        crossProductStart = crossProductEnd;
        crossProductEnd = tempCrossProduct;

        tmp = lineStart;
        lineStart = lineEnd;
        lineEnd = tmp;

        normalX = -normalX;
        normalY = -normalY;
    }

    if (crossProductEnd == crossProductStart)
    {
        intersectionFactor = stanPointLineDist(point3D, &normalizedDir, lineStart);
        intersectionFactorLineEnd = stanPointLineDist(point3D, &normalizedDir, lineEnd);

        if (intersectionFactorLineEnd < intersectionFactor)
        {
            intersectionFactor = intersectionFactorLineEnd;
        }
    }
    else if (crossProductEnd < crossProductPoint)
    {
handlezero:
        intersectionFactor = stanPointLineDist(point3D, &normalizedDir, lineEnd);
    }
    else if (crossProductPoint < crossProductStart)
    {
        intersectionFactor = stanPointLineDist(point3D, &normalizedDir, lineStart);
    }
    else
    {
        startProjection = normalX * (point3D->y - lineStart->x) + normalY * (point3D->z - lineStart->y);
        endProjection = normalX * (point3D->y + direction->x - lineStart->x) + normalY * (point3D->z + direction->y - lineStart->y);

        if (startProjection == endProjection)
        {
            return 1.0f;
        }

        intersectionFactor = (startProjection - point3D->x) * directionLength  / (startProjection - endProjection);
    }

    if (directionLength  < intersectionFactor)
    {
        return 1.0f;
    }

    if (intersectionFactor < 0.0f)
    {
        return 0.0f;
    }

    return (f32) intersectionFactor * (1.0f / directionLength );
}


/**
 * Finds the floor tile under a pad from the tile name the setup gives it. If the name is
 * missing or the pad is outside that tile, moves to the nearest place on any tile where a
 * point fits and walks back towards the pad.
 * Returns 1 if the named tile was right, 2 if another tile was found, 0 if none.
 */
s32 stanResolvePadTile(struct PadRecord *pad, char *tilename, struct StandTile **tile_stack)
{
    struct coord3d coord;
   
    *tile_stack = stanMatchTileName(tilename);
    
    if ((*tile_stack == NULL) || (stanIsInsideTriWorld(*tile_stack, pad->pos.f[0], pad->pos.f[2]) == 0))
    {
        coord.f[0] = pad->pos.f[0];
        coord.f[1] = pad->pos.f[1];
        coord.f[2] = pad->pos.f[2];
        
        *tile_stack = stanFindNearestLegalPos(&coord.f[0], &coord.f[1], &coord.f[2], 0);
        
        if ((*tile_stack != NULL) && (stanWalkLineNoCallback(tile_stack, coord.f[0], coord.f[2], pad->pos.f[0], pad->pos.f[2]) != 0))
        {
            return 2;
        }
        
        *tile_stack = NULL;
        
        return 0;
    }
    
    return 1;
}
 
