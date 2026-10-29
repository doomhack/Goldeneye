#ifndef _BG_H_
#define _BG_H_
#include <ultra64.h>
#include <bondgame.h>
#include <bondtypes.h>
#include <bondconstants.h>

struct levelentry
{
    s32 levelID;
    void *bg_seg_filename;
    void *bg_stan_filename;
    f32 levelscale;
    f32 visibility;
    f32 unknownfloat;
};
// cannon definition
#define MAXPORTALSPERROOM 20

// cannonical name
#define PORTMAX 200

#define BG_SEG_TO_PTR(base, off) ((void *) (((u32) (base)) + (((u32) (off)) + 0xF1000000)))

typedef struct RoomVtxBatchBounds {
    s16 gdlindex;    // 0x00
    s16 pad02;       // 0x02

    union {
        struct {
            s32 xmin; // 0x04
            s32 ymin; // 0x08
            s32 zmin; // 0x0c
            s32 xmax; // 0x10
            s32 ymax; // 0x14
            s32 zmax; // 0x18
        };

        struct {
            s32 min[3]; // 0x04
            s32 max[3]; // 0x10
        };
    };
} RoomVtxBatchBounds; // size = 0x1c

typedef struct BoundVec { 
    s32 x, y, z; 
} BoundVec;

typedef struct s_room_info {
    // is room being rendered? boolean
    u8 room_rendered;                       // 0x00

    // is the room a neighbor to a room being rendered? boolean
    u8 room_neighbor_to_rendered;           // 0x01

    /**
     * Acts like a small room age counter.
     * 0 = unloaded
     * 1 = loaded/used recently
     * 2-3 = loaded but aging towards unload
     * 4 = unload on tick
     */
    u8 model_bin_loaded;                    // 0x02

    /**
     * Counts how often this room has been reached during the current portal
     * visibility traversal.
     */
    u8 portal_visit_count;                  // 0x03

    Vtx *vertices;                          // 0x04
    void *ptr_expanded_mapping_info;        // 0x08
    void *ptr_secondary_expanded_mapping_info; // 0x0c

    s32 csize_point_index_binary;           // 0x10
    s32 csize_primary_DL_binary;            // 0x14
    s32 csize_secondary_DL_binary;          // 0x18

    s32 usize_point_index_binary;           // 0x1c
    s32 usize_primary_DL_binary;            // 0x20
    s32 usize_secondary_DL_binary;          // 0x24

    s32 cur_room_totalsize;                 // 0x28
    RoomVtxBatchBounds *vtx_batch_bounds;   // 0x2c

    s16 num_vtx_batch_bounds;               // 0x30
    s16 field_32;                           // 0x32

    u8 room_loaded_mask;                    // 0x34
    u8 requested; // set once the room's model has been asked for; such rooms are never unloaded
    s16 mtxIndex;                           // 0x36

    coord3d minbounds;                      // 0x38
    coord3d maxbounds;                      // 0x44
} s_room_info; 

typedef struct s_bound_info
{
    #if defined(VERSION_EU)
    //eu is 0x18 total len
    u8 roomid;
    u8 pad1;
    s16 depth; // portal depth the room was reached at; rooms draw in depth order
    u8 next;
    u8 pad2[3];
    bbox2d bbox;

    #else
    //us is 0x1C total len
    s32 roomid;
    s32 depth;
    bbox2d bbox;
    s32 next;
    #endif


} s_bound_info;

typedef struct bg_portal_entry
{
    u8 numPoints;
    u8 padding[3];
    coord3d point;
} bg_portal_entry;

typedef struct bg_portal_data_entry
{
    bg_portal_entry *offset_portal;
    u8 connectedRoom1;
    u8 connectedRoom2;
    u8 controlbytes1;
    u8 controlbytes2;
} bg_portal_data_entry;

typedef struct bg_room_data
{
    void* pPointTableBin;
    void* pPriMappingBin;
    void* pSecMappingBin;
    coord3d pos;
} bg_room_data;

typedef struct s_specialportal
{
    u8 levelid;
    u8 portallist[];
} s_specialportal;

typedef struct PortalCache {
    s32 count;
    bbox2d bbox;
} PortalCache;

typedef struct bg_queued_portal_entry {

    #if defined(VERSION_EU)
    u8 roomnum;        // 0x00
    u8 portalnum;      // 0x01
    s16 depth;         // 0x02
    bbox2d box;        // 0x04
    #else
    s32 value;         // 0x00
    s32 roomnum;       // 0x04
    s32 portalnum;     // 0x08
    s32 depth;         // 0x0c
    bbox2d box;        // 0x10
    #endif
} bg_queued_portal_entry;

extern bg_portal_data_entry *g_BgPortals;
extern PortalCache g_BgPortalCache[PORTMAX];
extern s32 g_MaxNumRooms;
extern f32 g_BgLevelScaleInv;

extern bg_room_data * g_BgRoomFiles;
extern s_room_info g_BgRoomInfo[];
extern Gfx *ptrDynamic_CC_RM_LUT[];
extern Gfx DL_LUT_PRIMARY_ADDFOG[];


void bgInit(void);

// chrSetChrPresetToNearbyChr requres arg be s32
bool bgRoomsSharePortal(s32 roomA, s32 roomB);

//f32 bgGetRoomValue(s32 room); // u8 not s32 for stanGetTileRoomValue
s32 bgCopyVisibleRoomsToList(s32 *rooms, s32 max);
bool bgTestBulletHitBackground(coord3d *from, coord3d *to, s32 roomnum, struct HitThing *hit);
void bgUnloadRoom(s32 roomID);
void bgLoad(LEVEL_INDEX stagenum);

void bgToggleDataPortalsContrlBytes1Bit1(s32 index, s32 toggle);
void bgFindVisibleRooms(void);
Gfx* bgLevelRender(Gfx *gdl);
Gfx *bgScissorCurrentPlayerView(Gfx *gdl, s32 left, s32 top, s32 right, s32 bottom);
bool bgProjectRoomCoordToScreen(coord3d *src, coord3d *dst);
Gfx* bgScissorCurrentPlayerViewDefault(Gfx* gdl);
Gfx* bgScissorCurrentPlayerViewF(Gfx* gdl, f32 left, f32 top, f32 right, f32 bottom);
f32 bgGetLevelScale(void);
u8 bgRoomIsVisible(int roomID);
s32 bgGet2dBboxByRoomId(s32 room_id, bbox2d *result);
f32 bgGetLevelVisibilityScale(void);
void bgRectOutersect(bbox2d *a, bbox2d *b);
f32 bgGetLevelScaleInv(void);
s32 bgGetPortalBetweenRooms(s32 room1, s32 room2, coord3d *start, coord3d *end);
void bgGetPortalPlane(s32 portalnum, struct PortalMetric *metric);
void bgApplyDynamicCCRMLUT(Gfx *start, Gfx *end, enum CCRMLUT lutIndex);
void bgFindRoomsInBbox(coord3d *, coord3d *, s32 *, s32 *, s32);
void bgFindRoomsAlongSegment(coord3d *pos1, coord3d *pos2, u8 *initialRooms, u8 *outRoomSet, s32 *outRoomNums, s32 *outRoomNumsCount, s32 outRoomNumsMax);
s32 bgFindPortalOnSegment(coord3d *start, coord3d *end);
void bgRoomCalcBB(s32 room);

void bgReset(void);
char *bgDebPrintROOMID(s32 roomId);
s32 bgGetConnectedRooms(s32 roomIndex, s32* list, s32 max);
s32 bgGetMaxNumRooms(void);
bool bgTestRayIntersectsBbox(coord3d *origin, coord3d *dir, s32 *bbox_min, s32 *bbox_max);

#endif
