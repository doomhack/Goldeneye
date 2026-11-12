#ifndef _BG_H_
#define _BG_H_
#include <ultra64.h>
#include <bondgame.h>
#include <bondtypes.h>
#include <bondconstants.h>

struct levelentry
{
    s32 levelID;
    const char *bg_seg_filename;
    const char *bg_stan_filename;
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

/**
 * The per-room flags that the visibility and unload passes walk for every room each frame,
 * kept apart from s_room_info so a pass over all rooms touches 6 bytes per room.
 */
typedef struct RoomState {
    /**
     * The room's age, counted in frames since it was last drawn.
     * 0 = unloaded
     * 1 = loaded and drawn recently
     * 2 and up = loaded, not drawn for a while (stops at 255); from 4 the room can be
     * freed when the room heap needs space (bgEvictOldestRoom)
     */
    u8 model_bin_loaded;

    /**
     * Counts how often this room has been reached during the current portal
     * visibility traversal.
     */
    u8 portal_visit_count;

    u8 room_loaded_mask;
    u8 requested; // set once the room's model has been asked for; such rooms are never unloaded
    s16 mtxIndex;
} RoomState;

static_assert(sizeof(RoomState) == 6, "RoomState packs into 6 bytes");

/**
 * A room's model data. What rendering and collision read comes first; the load sizes,
 * used only when the room is loaded or unloaded, come last.
 */
typedef struct s_room_info {
    coord3d minbounds;
    coord3d maxbounds;

    Vtx *vertices;
    void *ptr_expanded_mapping_info;
    void *ptr_secondary_expanded_mapping_info;
    RoomVtxBatchBounds *vtx_batch_bounds;
    s32 num_vtx_batch_bounds;

    s32 csize_point_index_binary;
    s32 csize_primary_DL_binary;
    s32 csize_secondary_DL_binary;

    s32 usize_point_index_binary;
    s32 usize_primary_DL_binary;
    s32 usize_secondary_DL_binary;

    s32 cur_room_totalsize;
} s_room_info; 

typedef struct s_bound_info
{
    s32 roomid;
    s32 depth;
    bbox2d bbox;
    s32 next;
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
    const void* pPointTableBin;
    void* pPriMappingBin;
    void* pSecMappingBin;
    coord3d pos;
} bg_room_data;

typedef struct s_specialportal
{
    u8 levelid;
    u8 portallist[];
} s_specialportal;

/* A portal's screen box, valid for the visibility pass numbered pass (see bgResetPortalTable) */
typedef struct PortalCache {
    s32 count;
    bbox2d bbox;
    u32 pass;
} PortalCache;

typedef struct bg_queued_portal_entry {

    s32 roomnum;       // 0x04
    s32 portalnum;     // 0x08
    s32 depth;         // 0x0c
    bbox2d box;        // 0x10
} bg_queued_portal_entry;

extern bg_portal_data_entry *g_BgPortals;
extern PortalCache g_BgPortalCache[PORTMAX];
extern s32 g_MaxNumRooms;
extern f32 g_BgLevelScaleInv;

extern bg_room_data * g_BgRoomFiles;
extern s_room_info g_BgRoomInfo[];
extern RoomState g_BgRoomState[];
extern Gfx *const ptrDynamic_CC_RM_LUT[];
extern Gfx DL_LUT_PRIMARY_ADDFOG[];


void bgInit(void);

// chrSetChrPresetToNearbyChr requres arg be s32
bool bgRoomsSharePortal(s32 roomA, s32 roomB);

//f32 bgGetRoomValue(s32 room); // u8 not s32 for stanGetTileRoomValue
s32 bgCopyVisibleRoomsToList(s32 *rooms, s32 max);
bool bgTestBulletHitBackground(const coord3d *from, const coord3d *to, s32 roomnum, struct HitThing *hit);
void bgUnloadRoom(s32 roomID);
void bgLoad(LEVEL_INDEX stagenum);

void bgToggleDataPortalsContrlBytes1Bit1(s32 index, s32 toggle);
void bgFindVisibleRooms(void);
Gfx* bgLevelRender(Gfx *gdl);
Gfx *bgScissorCurrentPlayerView(Gfx *gdl, s32 left, s32 top, s32 right, s32 bottom);
Gfx* bgScissorCurrentPlayerViewDefault(Gfx* gdl);
Gfx* bgScissorCurrentPlayerViewF(Gfx* gdl, f32 left, f32 top, f32 right, f32 bottom);
f32 bgGetLevelScale(void);
u8 bgRoomIsVisible(int roomID);
s32 bgGet2dBboxByRoomId(s32 room_id, bbox2d *result);
f32 bgGetLevelVisibilityScale(void);
void bgRectOutersect(bbox2d *a, const bbox2d *b);
f32 bgGetLevelScaleInv(void);
s32 bgGetPortalBetweenRooms(s32 room1, s32 room2, const coord3d *start, const coord3d *end);
void bgGetPortalPlane(s32 portalnum, struct PortalMetric *metric);
void bgApplyDynamicCCRMLUT(Gfx *start, const Gfx *end, enum CCRMLUT lutIndex);
void bgFindRoomsInBbox(const coord3d *, const coord3d *, s32 *, s32 *, s32);
void bgFindRoomsAlongSegment(const coord3d *pos1, const coord3d *pos2, const u8 *initialRooms, u8 *outRoomSet, s32 *outRoomNums, s32 *outRoomNumsCount, s32 outRoomNumsMax);
s32 bgFindPortalOnSegment(const coord3d *start, const coord3d *end);
void bgRoomCalcBB(s32 room);

void bgReset(void);
const char *bgDebPrintROOMID(s32 roomId);
s32 bgGetConnectedRooms(s32 roomIndex, s32* list, s32 max);
s32 bgGetMaxNumRooms(void);
bool bgTestRayIntersectsBbox(const coord3d *origin, const coord3d *dir, const s32 *bbox_min, const s32 *bbox_max);

#endif
