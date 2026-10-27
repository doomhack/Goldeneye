/**
 * The background ("bg"): the level's fixed geometry, split into rooms joined by portals.
 *
 * bgLoad reads the level's bg file: the room table, the portals and the visibility
 * commands. Each frame, for each player, bgFindVisibleRooms works out the camera's room,
 * runs the visibility commands (bgParseVisCommands) and walks out through the portals
 * (bgTraversePortal), adding each room it can see to g_BgRoomsDrawn with its box on
 * screen. bgLevelRender then draws those rooms and the props in them, room by room
 * (bgRenderVisibleRooms). Room models are loaded when first seen and freed when unseen
 * for a few frames (bgRoomsTickUnload).
 *
 * Also here: rays against room geometry (bgTestBulletHitBackground), the rooms a line
 * passes through (bgFindRoomsAlongSegment), and patching room display lists for fog
 * (bgApplyDynamicCCRMLUT). Room data is in bg units; g_BgLevelScale converts world units
 * to them. See docs/subsystems/world.md.
 */

#include <ultra64.h>
#include <PR/os.h>
#include <PR/gbi.h>
#include <gbi_extension.h>
#include <bondconstants.h>
#include <fr.h>
#include <memp.h>
#include "game/asset/fileload.h"
#include "game/world/bg.h"
#include "game/player/bondview.h"
#include "game/chr/chr.h"
#include "game/asset/rzip.h"
#include "game/world/bgfog.h"
#include "game/level/lv.h"
#include "game/math/round.h"
#include "game/math/matrix.h"
#include "game/player/player.h"
#include "game/nav/stan.h"
#include "game/prop/explosion.h"
#include "game/world/roommtx.h"


#define BG_STACK_SIZE 20
#define STAGES_MAX 38

typedef struct GlobalVisCommand {
    u8 type;
    u8 length;
    u8 padding[2];
    s32 arg;
} GlobalVisCommand;

typedef struct BgVisState {
    bbox2d box;
    s32 stopExec; // set by VISOP_DONT_EXEC_COMMANDS_EVEN_ON_RETURN
    s32 unk14;
} BgVisState;

enum GlobalVisOpcode {
    GlobalVisOpcode_SIGNED_ENUM = -1,
    VISOP_END = 0x00,
    VISOP_PUSH = 0x01,
    VISOP_POP = 0x02,
    VISOP_AND = 0x03,
    VISOP_OR = 0x04,
    VISOP_NOT = 0x05,
    VISOP_XOR = 0x06,
    VISOP_PUSH_IF_ROOM_IN_RANGE = 0x14,
    VISOP_FORCE_VISIBLE = 0x1e,
    VISOP_MATCH_PORTAL_VIS = 0x1f,
    VISOP_ADD_VISIBLE_ROOM = 0x20,
    VISOP_REMOVE_VIS = 0x21,
    VISOP_VISIBLE_IF_SEEN_THROUGH_PORTAL = 0x22,
    VISOP_NOT_VISIBLE_IF_SEEN_THROUGH_PORTAL = 0x23,
    VISOP_DISABLE_ROOM = 0x24,
    VISOP_DISABLE_ROOM_RANGE = 0x25,
    VISOP_PRELOAD_ROOM = 0x26,
    VISOP_PRELOAD_ROOM_RANGE = 0x27,
    VISOP_IF_STATEMENT = 0x50,
    VISOP_DONT_EXEC_COMMANDS_EVEN_ON_RETURN = 0x51,
    VISOP_ENDIF_CONTINUE_EXEC = 0x52,
    VISOP_IF_STATEMENT_PULL_FROM_STACK = 0x5a,
    VISOP_TOGGLE_EXEC_VS_READONLY = 0x5b,
    VISOP_ENDIF = 0x5c
};

extern PortalCache g_BgPortalCache[PORTMAX];
extern s32 g_BgHeader;
extern s32 *g_BgVisCommands;
extern f32 *g_BgRoomValues;
extern s_bound_info g_BgRoomsDrawn[];

/* PAL halved this to fit 4MB; with the Expansion Pak all versions use the NTSC size. */
#define BG_PORTAL_QUEUE_LEN 500
#define BG_ROOMS_DRAWN_LEN 204

// bss
s32 g_BgData;

s32 g_StanData;

/**
 * The first word of the bg file. If it is not 0 the level is drawn as this one display
 * list (g_BgFormat 1); every real level has 0 and is drawn room by room (g_BgFormat 2).
*/
s32 g_BgSingleDl;
s32 g_BgFormat;

#ifdef VERSION_EU
char list_visible_rooms_in_cur_global_vis_packet[0x8c];
#else
char list_visible_rooms_in_cur_global_vis_packet[0x98];
#endif

s32 num_visible_rooms_in_cur_global_vis_packet;

/* D:800413F0 Level gCurrentLevel = {0, 1.0, 1.0, 1.0, 1}; cant check this
   anymore however will concede seperate vars since below gets match? */
f32 g_BgLevelScale = 1.0;
f32 g_BgLevelScaleInv = 1.0;
//D:800413FC Private member - use bgGetLevelVisibilityScale outside this file
f32 mCurrentLevelVisibilityScale = 1.0;
s32 levelentry_index = 1;

/**
 * Something related to player screen.
 * Maybe x, y, width, height.
 * DefaultScreenXMin = 1; //always 1 (related to getvideosettings) Xmin
 * DefaultScreenYMin = 1; //always 1 (related to getvideosettings)Ymin
 * SubtractFromWidth = -1; //always -1 (related to getvideosettings)
 * SubtractFromHeight = -1; //always -1 (related to getvideosettings)
 */
s32 bgViewRelated[] = { 1, 1, -1, -1 };


/**
 * Array of info about all the rooms on the level
 * Some info is different according to which player is being rendered
 * This array is also why there's a 150 is room limit
 * Canonical name: roominf
*/
s_room_info g_BgRoomInfo[MAXROOMCOUNT] = {0};
//D:800442F4 canonically roomnumber
s32 g_MaxNumRooms = MAXROOMCOUNT;

/**
 * Limits the number of rooms that can be loaded per frame.
 */
s32 g_RoomLoadBudget = 0;

/* Whether each portal is vertical (bgPortalIsVertical), worked out at level load */
u8 g_BgPortalIsVertical[PORTMAX] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


struct levelentry levelinfotable[] = {
/*  levelID;            bg_seg_filename;        bg_stan_filename;      levelscale;  visibility; unknownfloat;*/
    {LEVELID_BUNKER1,  "bg/bg_sev_all_p.seg",  "Tbg_sev_all_p_stanZ",  0.53931433,  1.0,        23.148148},
    {LEVELID_SILO,     "bg/bg_silo_all_p.seg", "Tbg_silo_all_p_stanZ", 0.47256002,  1.0,        29.069},
    {LEVELID_STATUE,   "bg/bg_stat_all_p.seg", "Tbg_stat_all_p_stanZ", 0.107202865, 1.0,        0.0801},
    {LEVELID_CONTROL,  "bg/bg_arec_all_p.seg", "Tbg_arec_all_p_stanZ", 0.49886572,  1.0,        80.645164},
    {LEVELID_ARCHIVES, "bg/bg_arch_all_p.seg", "Tbg_arch_all_p_stanZ", 0.50678575,  1.0,        54.347824},
    {LEVELID_TRAIN,    "bg/bg_tra_all_p.seg",  "Tbg_tra_all_p_stanZ",  0.15019713,  1.0,        19.53125},
    {LEVELID_FRIGATE,  "bg/bg_dest_all_p.seg", "Tbg_dest_all_p_stanZ", 0.44757429,  1.0,        36.764706},
    {LEVELID_BUNKER2,  "bg/bg_sevb_all_p.seg", "Tbg_sevb_all_p_stanZ", 0.53931433,  1.0,        23.148148},
    {LEVELID_AZTEC,    "bg/bg_azt_all_p.seg",  "Tbg_azt_all_p_stanZ",  0.35300568,  1.0,        52.083332},
    {LEVELID_STREETS,  "bg/bg_pete_all_p.seg", "Tbg_pete_all_p_stanZ", 0.34187999,  1.0,        42.372883},
    {LEVELID_DEPOT,    "bg/bg_depo_all_p.seg", "Tbg_depo_all_p_stanZ", 0.21847887,  1.0,        17.605633},
    {LEVELID_COMPLEX,  "bg/bg_ref_all_p.seg",  "Tbg_ref_all_p_stanZ",  0.94285715,  1.0,        37.878788},
    {LEVELID_EGYPT,    "bg/bg_cryp_all_p.seg", "Tbg_cryp_all_p_stanZ", 0.25608,     1.0,        23.584906},
    {LEVELID_DAM,      "bg/bg_dam_all_p.seg",  "Tbg_dam_all_p_stanZ",  0.23363999,  0.2,        100.0},
    {LEVELID_FACILITY, "bg/bg_ark_all_p.seg",  "Tbg_ark_all_p_stanZ",  1.20648,     1.0,        64.102562},
    {LEVELID_RUNWAY,   "bg/bg_run_all_p.seg",  "Tbg_run_all_p_stanZ",  0.089571431, 1.0,        4.5537338},
    {LEVELID_SURFACE,  "bg/bg_sevx_all_p.seg", "Tbg_sevx_all_p_stanZ", 0.45445713,  0.2,        22.603975},
    {LEVELID_JUNGLE,   "bg/bg_jun_all_p.seg",  "Tbg_jun_all_p_stanZ",  0.094662853, 1.0,        6.6844921},
    {LEVELID_TEMPLE,   "bg/bg_dish_all_p.seg", "Tbg_dish_all_p_stanZ", 0.47142857,  1.0,        147.05882},
    {LEVELID_CAVERNS,  "bg/bg_cave_all_p.seg", "Tbg_cave_all_p_stanZ", 0.26824287,  1.0,        13.44086},
    {LEVELID_CITADEL,  "bg/bg_cat_all_p.seg",  "Tbg_cat_all_p_stanZ",  0.76852286,  1.0,        38.461536},
    {LEVELID_CRADLE,   "bg/bg_crad_all_p.seg", "Tbg_crad_all_p_stanZ", 0.23571429,  1.0,        43.103451},
    {LEVELID_SHO,      "bg/bg_sho_all_p.seg",  "Tbg_sho_all_p_stanZ",  0.528,       1.0,        21.18644},
    {LEVELID_SURFACE2, "bg/bg_sevx_all_p.seg", "Tbg_sevx_all_p_stanZ", 0.45445713,  0.2,        22.603975},
    {LEVELID_ELD,      "bg/bg_eld_all_p.seg",  "Tbg_eld_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_BASEMENT, "bg/bg_ame_all_p.seg",  "Tbg_ame_all_p_stanZ",  0.65999997,  1.0,        37.878788},
    {LEVELID_STACK,    "bg/bg_ame_all_p.seg",  "Tbg_ame_all_p_stanZ",  0.65999997,  1.0,        37.878788},
    {LEVELID_LUE,      "bg/bg_lue_all_p.seg",  "Tbg_lue_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_LIBRARY,  "bg/bg_ame_all_p.seg",  "Tbg_ame_all_p_stanZ",  0.65999997,  1.0,        37.878788},
    {LEVELID_RIT,      "bg/bg_rit_all_p.seg",  "Tbg_rit_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_CAVES,    "bg/bg_oat_all_p.seg",  "Tbg_oat_all_p_stanZ",  0.14142857,  1.0,        10.123456},
    {LEVELID_EAR,      "bg/bg_ear_all_p.seg",  "Tbg_ear_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_LEE,      "bg/bg_lee_all_p.seg",  "Tbg_lee_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_LIP,      "bg/bg_lip_all_p.seg",  "Tbg_lip_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_CUBA,     "bg/bg_len_all_p.seg",  "Tbg_len_all_p_stanZ",  0.094662853, 1.0,        6.6844921},
    {LEVELID_WAX,      "bg/bg_wax_all_p.seg",  "Tbg_wax_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_PAM,      "bg/bg_pam_all_p.seg",  "Tbg_pam_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_MAX,      "bg/bgx.seg",           "TbgxZ",                0.94285715,  1.0,        1.0}
};

/* GCC cannot initialise flexible array members inside an array, so store the same bytes flat */
u8 specialportalarray[] = {
    0x03,
        0x2C,0x2E,0x32, 0x37,0x3E,0x3F,0x4E, 0x56,0x59,0x5D,0x72, 0x76,0x79,0x7A,0xFF,
    0x11,
        0x00,0x3A,0xFF
};

/**
 * Bond's current room.
*/
s32 g_BgCurrentRoom = 1;

/**
 * Total number of rooms drawn for the current frame.
*/
s32 g_BgNumberOfRoomsDrawn = 0;

#if defined(VERSION_EU)
/* PAL: rooms added to g_BgRoomsDrawn this frame, which can pass its length */
s32 g_BgRoomsDrawnCount = 0;
#endif

Lights1 GlobalLight = gdSPDefLights1(
    150,150,150,        /* ambient color grey */ //D:80044840
    255,255,255,
    77,77,46    /* white light from the upper west-south-west (42 up, 244') */ //D:80044848
);


/* Room models load at most one room in ten per frame: rooms whose number ends in this digit. */
s32 g_BgRoomLoadSlot = 0;
/* Set to load any room as soon as it is drawn; always set, so g_BgRoomLoadSlot is not used. */
s32 g_BgLoadAllRooms = 1;

// forward declarations

void bgUnloadAllRooms(void);
Gfx *bgScissorAndRenderRooms(Gfx *arg0);
Gfx *bgRenderVisibleRooms(Gfx *arg0);
s32 bgCheckIfRoomModelNeedsLoad(s32 roomID);
void bgLoadRoomModelData(s32 room);
void bgBuildRoomVtxBounds(s32 roomID);
Gfx *bgRenderRoomPrimary(Gfx *gdl, s32 room_index);
Gfx *bgRenderRoomSecondary(Gfx *gdl, s32 room_index);

Gfx *bgScissorCurrentPlayerView(Gfx *arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

bool bgIsRoomOnScreen(s32 roomID, struct rectbbox *screenbox);
s32 bgAddVisibleRoom(s32 curroom, s32 unk1, bbox2d *screensize, s32 next);
void bgUpdateCurrentPlayerScreenMinMax(void);
void *bgRunVisCommands(s32 *pc);
void bgDetermineVisibleRooms(void);
s32 bgGetPortalScreenBox(s32 portalnum, bbox2d *screenbox);
f32 bgGetPortalThickness(s32 portalnum);
void bgGrowRoomBoundsToPortals(s32 roomID);
#if defined(VERSION_EU)
void bgTraversePortal(s32 roomnum, s32 portalnum, s32 depth, bbox2d *parentbox);
#endif
void bgSetLevelScale(f32 arg0);
s32 bgProjectPortalPoints(s32 portalnum, f32 scale, coord3d *points);
void bgOrderPortal(s32 portalnum);

// end forward declarations


void bgInit(void)
{
}


void bgMarkSpecialPortals(void) {
    u8 *ptr;
    u8 *end;
    u8 portal;

    ptr = (u8 *)specialportalarray;
    /* the original relies on g_BgCurrentRoom being the next variable in .data */
    end = specialportalarray + sizeof(specialportalarray);

    do {
        if (levelentry_index == *ptr++) {
            do {
                portal = ptr[0];
                while (ptr[1] >= portal) {
                    ((u8 *)g_BgPortals)[(portal << 3) + 6] |= 2;
                    portal++;
                }

                ptr += 2;
            } while (ptr[0] != 0xff);
        } else {
            do {
                ptr += 2;
            } while (ptr[0] != 0xff);
        }

        ptr++;
    } while ((u32)ptr < (u32)end);
}


s32 bgAddVisibleRoom(int curroom,int unk1, bbox2d * screensize, s32 next)
{
    int i;
    int temp;

    g_BgRoomInfo[curroom].room_rendered = '\x01';

    if (g_BgRoomInfo[curroom].room_loaded_mask != '\0') {
        return 0;
    }

    // Need g_BgNumberOfRoomsDrawn in a3
    for (i = 0; i < g_BgNumberOfRoomsDrawn; i++)
    {
        if (curroom == g_BgRoomsDrawn[i].roomid) {
            if (g_BgRoomsDrawn[i].depth < unk1) {
                g_BgRoomsDrawn[i].depth = unk1;
            }
            bgRectOutersect(screensize,&g_BgRoomsDrawn[i].bbox);
            temp = g_BgRoomsDrawn[i].next;
            g_BgRoomsDrawn[i].bbox.min.x = screensize->min.x;
            g_BgRoomsDrawn[i].bbox.min.y = screensize->min.y;
            g_BgRoomsDrawn[i].bbox.max.x = screensize->max.x;
            g_BgRoomsDrawn[i].bbox.max.y = screensize->max.y;
            g_BgRoomsDrawn[i].next = temp | next;

            return temp;
        }
    }

#if defined(VERSION_EU)
    i = g_BgNumberOfRoomsDrawn;
    if (i >= 0x78) {
        i = 0x77;
    }
    g_BgRoomsDrawn[i].roomid = curroom;
    g_BgRoomsDrawn[i].depth = unk1;
    g_BgRoomsDrawn[i].bbox.min.x = screensize->min.x;
    g_BgRoomsDrawn[i].bbox.min.y = screensize->min.y;
    g_BgRoomsDrawn[i].bbox.max.x = screensize->max.x;
    g_BgRoomsDrawn[i].bbox.max.y = screensize->max.y;
    g_BgRoomsDrawn[i].next = next;
    g_BgRoomsDrawnCount++;
    if (g_BgRoomsDrawnCount < 0x78) {
        g_BgNumberOfRoomsDrawn = g_BgRoomsDrawnCount;
    }

    return 0;
#else
    i = g_BgNumberOfRoomsDrawn;
#if defined(BACKPORT_R1)
    /* PAL clamps this list so it can't overrun the table; same idea at the US size. */
    if (i >= BG_ROOMS_DRAWN_LEN) {
        i = BG_ROOMS_DRAWN_LEN - 1;
    }
#endif
    g_BgRoomsDrawn[i].roomid = curroom;
    g_BgRoomsDrawn[i].depth = unk1;
    g_BgRoomsDrawn[i].bbox.min.x = screensize->min.x;
    g_BgRoomsDrawn[i].bbox.min.y = screensize->min.y;
    g_BgRoomsDrawn[i].bbox.max.x = screensize->max.x;
    g_BgRoomsDrawn[i].bbox.max.y = screensize->max.y;
    g_BgRoomsDrawn[i].next = next;
#if defined(BACKPORT_R1)
    if (i + 1 < BG_ROOMS_DRAWN_LEN) {
        g_BgNumberOfRoomsDrawn = i + 1;
    }
#else
    g_BgNumberOfRoomsDrawn = i + 1;
#endif


    return 0;
#endif
}


void bgResetPortalVisitCounts(void)
{
  s32 i;

  g_BgNumberOfRoomsDrawn = 0;
#ifdef VERSION_EU
  g_BgRoomsDrawnCount = 0;
#endif
  i = 0;
  while (i != MAXROOMCOUNT)
  {
    g_BgRoomInfo[i].portal_visit_count = 0;
    i++;
  }
}


/**
 * Searches g_BgRoomsDrawn for matching id.
 * If found, sets result to 2d bbox and returns 1.
 * Otherwise result is set to empty 2d bbox and returns 0.
 *
 * @param room_id: room id to search for.
 * @param result: Out parameter. Contains result 2d bbox.
*/
s32 bgGet2dBboxByRoomId(s32 room_id, struct bbox2d *result)
{
    s32 i;
    for (i=0; i<g_BgNumberOfRoomsDrawn; i++)
    {
        if (room_id == g_BgRoomsDrawn[i].roomid)
        {
            result->f[0][0] = g_BgRoomsDrawn[i].bbox.f[0][0];
            result->f[0][1] = g_BgRoomsDrawn[i].bbox.f[0][1];
            result->f[1][0] = g_BgRoomsDrawn[i].bbox.f[1][0];
            result->f[1][1] = g_BgRoomsDrawn[i].bbox.f[1][1];

            return 1;
        }
    }

    // It's pointless to set those because when this function returns false, result is unused
    result->f[0][0] = 0.0f;
    result->f[0][1] = 0.0f;
    result->f[1][0] = 0.0f;
    result->f[1][1] = 0.0f;

    return 0;
}


Gfx *bgRenderVisibleRooms(Gfx *gdl)
{
#ifdef VERSION_EU
    s16 i;
#else
    s32 i;
#endif
    s32 j;
#ifdef VERSION_EU
    s16 b_max;
    s16 b_min;
#else
    s32 b_max;
    s32 b_min;
#endif
#ifdef VERSION_EU
    b_max = 0;
    b_min = 32767;
#else
    b_min = 99999999;
    b_max = 0;
#endif
 
    for (j = 0; j < g_BgNumberOfRoomsDrawn; j++)
    {
        if (b_max < g_BgRoomsDrawn[j].depth)
        {
            b_max = g_BgRoomsDrawn[j].depth;
        }
 
        if (g_BgRoomsDrawn[j].depth < b_min)
        {
            b_min=g_BgRoomsDrawn[j].depth;
        }
    }
 
    for (i = b_min; i <= b_max; i++)
    {
        for (j = 0; j < g_BgNumberOfRoomsDrawn; j++)
        {
            if (i == g_BgRoomsDrawn[j].depth)
            {
                gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjectionMtx()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogRenderClearFogMode(gdl);
 
                if (lvGetDrawWorld())
                {
                    gdl = propsRenderPass(gdl, g_BgRoomsDrawn[j].roomid, 0);
                }
 
                gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjViewMtx()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogSetRenderFogColor(
                    bgScissorCurrentPlayerViewF(
                        gdl++,
                        g_BgRoomsDrawn[j].bbox.min.x,
                        g_BgRoomsDrawn[j].bbox.min.y,
                        g_BgRoomsDrawn[j].bbox.max.x,
                        g_BgRoomsDrawn[j].bbox.max.y),
                    0);
 
                if (lvGetDrawWorld())
                {
                    gdl = bgRenderRoomPrimary(gdl, g_BgRoomsDrawn[j].roomid);
                }
 
                gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjectionMtx()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogRenderClearFogMode(gdl);
 
                if (lvGetDrawWorld())
                {
                    gdl = propsRenderPass(gdl, g_BgRoomsDrawn[j].roomid, 2);
                }
 
            }
        }
    }
 
    gdl = bgScissorCurrentPlayerViewDefault(fogRenderClearFogMode(gdl));
    gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjViewMtx()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
 
    if (lvGetDrawWorld())
    {
        gdl = explosionRenderScorchBuffer(gdl);
        gdl = explosionCallRenderBulletImpactOnProp(gdl);
    }
 
 
    for (i = b_max; i >= b_min; i--)
    {
 
        for (j = 0; j < g_BgNumberOfRoomsDrawn; j++)
        {
            if (i == g_BgRoomsDrawn[j].depth)
            {
                gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjViewMtx()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogSetRenderFogColor(
                    bgScissorCurrentPlayerViewF(
                        gdl++,
                        g_BgRoomsDrawn[j].bbox.min.x,
                        g_BgRoomsDrawn[j].bbox.min.y,
                        g_BgRoomsDrawn[j].bbox.max.x,
                        g_BgRoomsDrawn[j].bbox.max.y),
                    1);
 
                if (lvGetDrawWorld())
                {
                    gdl = bgRenderRoomSecondary(gdl, g_BgRoomsDrawn[j].roomid);
                }
 
                gSPMatrix(gdl++, osVirtualToPhysical((void*)camGetProjectionMtx()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogRenderClearFogMode(gdl);
 
                if (lvGetDrawWorld())
                {
                    gdl = propsRenderPass(gdl, g_BgRoomsDrawn[j].roomid, 1);
                }
 
            }
        }
    }
 
    return gdl;
}


s32 bgGetPriMappingBinCount(s32 room)
{
    s32 i = room;

    while (g_BgRoomFiles[i].pPriMappingBin == 0)
    {
        i++;
    }

    return i;
}


s32 bgGetSecMappingBinCount(s32 room)
{
    s32 i = room;

    while (g_BgRoomFiles[i].pSecMappingBin == 0)
    {
        i++;
    }

    return i;
}


s32 bgGetPointTableBinCount(s32 room)
{
    s32 i = room;
    
    while (g_BgRoomFiles[i].pPointTableBin == 0)
    {
        i++;
    }

    return i;
}


void bgLoad(LEVEL_INDEX levelid)
{
    typedef struct bg_envdata_entry_local {
        u8 type;
        u8 pad[3];
        s32 data;
    } bg_envdata_entry_local;
    s32 i;
    s32 size;
    s32 header[0x10];
    s32 *data;
 
    levelentry_index = 0;

    for (i = 0; i < MAXROOMCOUNT; i++) 
    {
        g_BgRoomInfo[i].vtx_batch_bounds = NULL;
    }
 
    for (i = 0; i < STAGES_MAX; i++)
    {
        if (levelinfotable[i].levelID == levelid)
        {
            levelentry_index = i;
        }
    }
 
    lightFixtureInitTables();
 
    g_BgData = (s32)header;
    fileLoadPartByName(levelinfotable[levelentry_index].bg_seg_filename, (u8 *) g_BgData, 0, 0x40);


    g_BgHeader = g_BgData;
    g_BgRoomFiles = (bg_room_data *) BG_SEG_TO_PTR(g_BgData, ((s32 *)g_BgData)[1]);
 
    size = (((((u32) g_BgRoomFiles[1].pPointTableBin) & 0x00ffffff) - 1) | 0xf) + 1;
 
    g_BgData = (s32) mempAlloc(size, MEMPOOL_STAGE);
    fileLoadPartByName(levelinfotable[levelentry_index].bg_seg_filename, (u8 *) g_BgData, 0, size);
 
    g_StanData = (s32) fileLoadByNameToPool(levelinfotable[levelentry_index].bg_stan_filename, FILELOAD_TRIM_2, 0, MEMPOOL_STAGE);
 
    stanDetermineEOF((struct StanPrefixRecord *) g_StanData, 0, (u8 *) g_StanData);
    stanLoadFile((struct StanPrefixRecord *) g_StanData);
 
    bgSetLevelScale(levelinfotable[levelentry_index].levelscale);
    stanSetLevelScale(levelinfotable[levelentry_index].levelscale);
    chrRemovedUnused(levelinfotable[levelentry_index].levelscale);
 
    mCurrentLevelVisibilityScale = levelinfotable[levelentry_index].visibility;
 
    camSetPosScale(mCurrentLevelVisibilityScale);
    mtxSetWorldScale(mCurrentLevelVisibilityScale);
 
    data = (s32 *)g_BgData;
    g_BgSingleDl = *data;
    g_BgFormat = 1;
 
    if (g_BgSingleDl == 0)
    {
        g_BgFormat = 2;
        g_BgHeader = (s32)data;
        g_BgRoomFiles = (bg_room_data *) BG_SEG_TO_PTR(data, ((s32 *)g_BgHeader)[1]);
        
 
        g_MaxNumRooms = 0;

        for (i = 1; g_BgRoomFiles[i].pPriMappingBin != NULL; i++) 
        {
            g_MaxNumRooms++;  
        }
 
        g_BgPortals = (bg_portal_data_entry *) BG_SEG_TO_PTR(data, ((s32 *)g_BgHeader)[2]);


        if (((s32 *)g_BgHeader)[3] == 0)
        {
            g_BgVisCommands = 0;
        }
        else
        {
            g_BgVisCommands = (s32 *) BG_SEG_TO_PTR(data, ((s32 *)g_BgHeader)[3]);
 
            if (((s32 *)g_BgHeader)[4] == 0)
            {
                g_BgRoomValues = NULL;
            }
            else
            {
                g_BgRoomValues = (f32 *) BG_SEG_TO_PTR(data, ((s32 *)g_BgHeader)[4]);
            }
        }
 
        for (i = 0; g_BgPortals[i].offset_portal != (NULL); i++)
        {
            g_BgPortals[i].offset_portal = (bg_portal_entry *) BG_SEG_TO_PTR(g_BgData, g_BgPortals[i].offset_portal);
        }
 
        if (g_BgVisCommands != NULL)
        {
            for (i = 0; ((bg_envdata_entry_local *)g_BgVisCommands)[i].type != 0; i++)
            {
                if (((bg_envdata_entry_local *)g_BgVisCommands)[i].type == ENVIRONMENTDATA_ALT)
                {
                    ((bg_envdata_entry_local *)g_BgVisCommands)[i].data = bgGetPortalIndex((s32) BG_SEG_TO_PTR(g_BgData, ((bg_envdata_entry_local *)g_BgVisCommands)[i].data));
                }
            }
        }
 
        for (i = 1; i < g_MaxNumRooms; i++)
        {
            g_BgRoomInfo[i].model_bin_loaded = 0;
            g_BgRoomInfo[i].requested = 0;
 
            if (g_BgRoomFiles[i].pPriMappingBin != (NULL))
            {
                s32 primaryindex;
                s32 secondaryindex;
                primaryindex = bgGetPriMappingBinCount(i + 1);
                secondaryindex = bgGetSecMappingBinCount(i);
 
                if (primaryindex <= secondaryindex)
                {
                    g_BgRoomInfo[i].csize_primary_DL_binary = ((s32) g_BgRoomFiles[primaryindex].pPriMappingBin) - ((s32) g_BgRoomFiles[i].pPriMappingBin);
                }
                else
                {
                    g_BgRoomInfo[i].csize_primary_DL_binary = ((s32) g_BgRoomFiles[secondaryindex].pSecMappingBin) - ((s32) g_BgRoomFiles[i].pPriMappingBin);
                }
            }
            else
            {
                g_BgRoomInfo[i].csize_primary_DL_binary = 0;
            }
 
            if (g_BgRoomFiles[i].pSecMappingBin != (NULL))
            {
                s32 primaryindex;
                s32 secondaryindex;
                primaryindex = bgGetPriMappingBinCount(i + 1);
                secondaryindex = bgGetSecMappingBinCount(i + 1);
 
                if (primaryindex <= secondaryindex)
                {
                    g_BgRoomInfo[i].csize_secondary_DL_binary = ((s32) g_BgRoomFiles[primaryindex].pPriMappingBin) - ((s32) g_BgRoomFiles[i].pSecMappingBin);
                }
                else
                {
                    g_BgRoomInfo[i].csize_secondary_DL_binary = ((s32) g_BgRoomFiles[secondaryindex].pSecMappingBin) - ((s32) g_BgRoomFiles[i].pSecMappingBin);
                }
            }
            else
            {
                g_BgRoomInfo[i].csize_secondary_DL_binary = 0;
            }
 
            if (g_BgRoomFiles[i].pPointTableBin != (NULL))
            {
                s32 pointindex;
                pointindex = bgGetPointTableBinCount(i + 1);
                g_BgRoomInfo[i].csize_point_index_binary = ((s32) g_BgRoomFiles[pointindex].pPointTableBin) - ((s32) g_BgRoomFiles[i].pPointTableBin);
            }
            else
            {
                g_BgRoomInfo[i].csize_point_index_binary = 0;
            }
 
            g_BgRoomInfo[i].cur_room_totalsize = -1;
        }
 
        roomMtxInit();
 
        for (i = 1; i < g_MaxNumRooms; i++)
        {
            bgRoomCalcBB(i);
        }
 
        for (i = 0; g_BgPortals[i].offset_portal != (NULL); i++)
        {
            g_BgPortalIsVertical[i] = bgPortalIsVertical(i);
        }
 
        for (i = 0; g_BgPortals[i].offset_portal != (NULL); i++)
        {
            bgOrderPortal(i);
        }
 
        for (i = 0; i < g_MaxNumRooms; i++)
        {
            bgGrowRoomBoundsToPortals(i);
        }
 
        for (i = 0; g_BgPortals[i].offset_portal != (NULL); i++)
        {
            g_BgPortals[i].controlbytes1 &= 0xfe;
        }
 
        bgMarkSpecialPortals();
    }
 
    g_RoomLoadBudget = 200;
}


void bgReset(void) {
    bgUnloadAllRooms();
    mtxSetWorldScale(1.0);
}


void bgSetLevelScale(f32 arg0) {
    g_BgLevelScale = arg0;
    g_BgLevelScaleInv = (f32) (1.0f / arg0);
}


f32 bgGetLevelScaleInv(void){
  return g_BgLevelScaleInv;
}


f32 bgGetLevelScale(void){
  return g_BgLevelScale;
}


f32 bgGetLevelVisibilityScale(void) {
    return mCurrentLevelVisibilityScale;
}


// defined later in the same file
extern void bgRoomsTickUnload(void);

void bgFindVisibleRooms(void)
{
    coord3d *pos;
    coord3d *pos3;
    s32 room;
    s32 portalnum;
    s32 lastportal;
    s32 depth;
    s32 maxdepth;
    s32 cammode;

    lastportal = -1;

    num_visible_rooms_in_cur_global_vis_packet = 0;

    if (playerGetShuffledIndex(playerGetCurrentNum()) == 0) {
        bgRoomsTickUnload();
    }

    cammode = playerGetCameraMode();

    g_RoomLoadBudget = 3;

    switch (cammode) {
    case CAMERAMODE_INTRO:
    case CAMERAMODE_FADESWIRL:
    case CAMERAMODE_SWIRL:
    case CAMERAMODE_POSEND:
    case CAMERAMODE_MP:
        g_RoomLoadBudget = 0xc8;
        break;
    case CAMERAMODE_FP:
    case CAMERAMODE_DEATH_CAM_SP:
    case CAMERAMODE_DEATH_CAM_MP:
    case CAMERAMODE_FP_NOINPUT:
        break;
    }

    room = playerGetRoom();
    g_BgCurrentRoom = room;

    pos = playerGetPos();
    pos3 = playerGetPos3();


    for (depth = 0, maxdepth = 11; depth != maxdepth; depth++) {
        for (portalnum = 0; g_BgPortals[portalnum].offset_portal != NULL; portalnum++) {

            if (g_BgPortalIsVertical[portalnum] != 0) {
                continue;
            }
            if (portalnum == lastportal) {
                continue;
            }

            if (((room == g_BgPortals[portalnum].connectedRoom1 || room == g_BgPortals[portalnum].connectedRoom2) && bgSegmentCrossesPortal(portalnum, pos, pos3))) {
                lastportal = portalnum;

                room = (room ^ g_BgPortals[portalnum].connectedRoom1) ^ g_BgPortals[portalnum].connectedRoom2;

                break;
            }
        }

        if (g_BgPortals[portalnum].offset_portal == NULL) {
            break;
        }
    }

    g_BgCurrentRoom = room;
    bgDetermineVisibleRooms();
}


void bgAddToRoomSet(u8* set, u8 newElement) {
    s32 i = 0;

    while (i < 0x10 && set[i] != 0xFF) {
        if (newElement == set[i]) {
            return;
        }
        i++;
    }

    if (i < 0xF) {
        set[i] = newElement;
        set[i+1] = -1;
    }
    return;
}


void bgFindRoomsAlongSegment(coord3d *fromPos, coord3d *toPos, u8 *fromRooms, u8 *finalRooms, s32 *traversedRooms, s32 *traversedRoomCount, s32 maxTraversedRooms)
{
    u8 sourceRooms[16];
    u8 foundRooms[16];
    u8 allRooms[16];
    s32 portalnum;
    s32 i;
    s32 count;
    u8 portalStatuses[PORTMAX];

    for (portalnum = 0; g_BgPortals[portalnum].offset_portal != 0; portalnum++)
    {
        portalStatuses[portalnum] = bgSegmentCrossesPortal(portalnum, fromPos, toPos);
    }

    for (count = 0; count < 8; count++)
    {
        sourceRooms[count] = fromRooms[count];
    }

    for (count = 0; count < 8; count++)
    {
        allRooms[count] = fromRooms[count];
    }

    count = 0;

    while ((sourceRooms[count] != 0xff) && (count < 16))
    {
        count++;
    }

    count = 0;

    do
    {
        foundRooms[0] = 0xff;

        for (portalnum = 0; g_BgPortals[portalnum].offset_portal != 0; portalnum++)
        {
            for (i = 0; (sourceRooms[i] != 0xff) && (i < 16); i++)
            {
                if (portalStatuses[portalnum] == 1)
                {
                    if (g_BgPortals[portalnum].connectedRoom1 == sourceRooms[i])
                    {
                        bgAddToRoomSet(foundRooms, g_BgPortals[portalnum].connectedRoom2);
                        bgAddToRoomSet(allRooms, g_BgPortals[portalnum].connectedRoom2);
                        portalStatuses[portalnum] = 0;
                    }
                }

                if (portalStatuses[portalnum] == 2)
                {
                    if (g_BgPortals[portalnum].connectedRoom2 == sourceRooms[i])
                    {
                        bgAddToRoomSet(foundRooms, g_BgPortals[portalnum].connectedRoom1);
                        bgAddToRoomSet(allRooms, g_BgPortals[portalnum].connectedRoom1);
                        portalStatuses[portalnum] = 0;
                    }
                }
            }
        }

        if (foundRooms[0] == 0xff)
        {
            break;
        }

        for (count = 0; count < 16; count++)
        {
            sourceRooms[count] = foundRooms[count];
        }

        count = 0;

        while ((foundRooms[count] != 0xff) && (count < 16))
        {
            count++;
        }

        count = 0;
    }
    while (foundRooms[count] != 0xff);

    for (count = 0; (count < 7) && (sourceRooms[count] != 0xff); count++)
    {
        finalRooms[count] = sourceRooms[count];
    }

    finalRooms[count] = 0xff;

    for (count = 0; (count < maxTraversedRooms) && (allRooms[count] != 0xff); count++)
    {
        traversedRooms[count] = allRooms[count];
    }

    *traversedRoomCount = count;
}


Gfx *bgLevelRender(Gfx *arg0)
{
    gSPSetLights1(arg0++, GlobalLight);
    gSPLookAt(arg0++, camGetLookAt());
    gSPSegment(arg0++, SPSEGMENT_BG_DL, g_BgData);

    if (g_BgFormat == 1)
    {
        gSPDisplayList(arg0++, g_BgSingleDl);
    }
    else
    {
        arg0 = fogRenderClearFogMode(bgScissorCurrentPlayerViewDefault(bgScissorAndRenderRooms(fogSetRenderFogColor(arg0, 0))));
    }

    gSPMatrix(arg0++, g_viProjectionMatrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    return camLoadWorldToViewMtx(arg0++);
}


/**
 * Calls @see bgScissorCurrentPlayerView with default current player values.
 */
Gfx* bgScissorCurrentPlayerViewDefault(Gfx* arg0)
{
    return bgScissorCurrentPlayerView(
        arg0,
        g_CurrentPlayer->viewleft,
        g_CurrentPlayer->viewtop,
        g_CurrentPlayer->viewleft + g_CurrentPlayer->viewx,
        g_CurrentPlayer->viewtop + g_CurrentPlayer->viewy);
}


/**
 * Same as @see bgScissorCurrentPlayerView, but accepts float parameters.
 */
Gfx* bgScissorCurrentPlayerViewF(Gfx* arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4)
{
    return bgScissorCurrentPlayerView(
        arg0,
        (s32)arg1,
        (s32)arg2,
        mathCeilToInt(arg3),
        mathCeilToInt(arg4));
}


/**
 * Specifies the drawing area (the scissoring box).
 * View is bound to current player view properties, but parameters can clip to smaller area.
 *
 * @param arg0: Display list pointer
 * @param left: Screen's left edge coordinates. Must be >= g_CurrentPlayer->viewleft otherwise ignored.
 * @param top: Screen's top edge coordinates. Must be >= g_CurrentPlayer->viewtop otherwise ignored.
 * @param width: Screen's right edge coordinates. Must be <= g_CurrentPlayer->viewleft+viewx otherwise ignored.
 * @param height: Screen's left bottom coordinates. Must be <= g_CurrentPlayer->viewtop+viewy otherwise ignored.
*/
Gfx *bgScissorCurrentPlayerView(Gfx *arg0, s32 left, s32 top, s32 width, s32 height)
{
    struct player *temp_v0;

    temp_v0 = g_CurrentPlayer;

    if (left < (s32) temp_v0->viewleft)
    {
        left = (s32) temp_v0->viewleft;
    }

    if (top < (s32) temp_v0->viewtop)
    {
        top = (s32) temp_v0->viewtop;
    }

    if (temp_v0->viewleft + temp_v0->viewx < width)
    {
        width = temp_v0->viewleft + temp_v0->viewx;
    }

    if (temp_v0->viewtop + temp_v0->viewy < height)
    {
        height = temp_v0->viewtop + temp_v0->viewy;
    }

    gDPSetScissor(arg0++, G_SC_NON_INTERLACE, VI_SX(left), top, VI_SX(width), height);

    return arg0;
}


void bgResetPortalTable(void) 
{
    s32 i;

    for (i = 0; i < PORTMAX; i++) 
    {
        g_BgPortalCache[i].count = -1;
    }
}


bool bgIsRoomOnScreen(s32 roomID, struct rectbbox *screenbox)
{
    s32 i;
    coord3d projected;
    coord3d corner;
    s32 count_z;
    s32 count_failed_projection;
    s32 count_left;
    s32 count_right;
    s32 count_top;
    s32 count_bottom;
    f32 zrange[2];

    count_z = 0;
    count_failed_projection = 0;
    count_left = 0;
    count_right = 0;
    count_top = 0;
    count_bottom = 0;

    viGetZRange(zrange);

    zrange[1] = zrange[1] / mCurrentLevelVisibilityScale;

    for (i = 0; i < 8; i++) {
        if (i & 1) {
            corner.x = g_BgRoomInfo[roomID].minbounds.x;
        } else {
            corner.x = g_BgRoomInfo[roomID].maxbounds.x;
        }

        if (i & 2) {
            corner.y = g_BgRoomInfo[roomID].minbounds.y;
        } else {
            corner.y = g_BgRoomInfo[roomID].maxbounds.y;
        }

        if (i & 4) {
            corner.z = g_BgRoomInfo[roomID].minbounds.z;
        } else {
            corner.z = g_BgRoomInfo[roomID].maxbounds.z;
        }

        if (bgProjectRoomCoordToScreen(&corner, &projected) == 0) {
            if (zrange[1] <= -projected.z) {
                count_z++;
            }

            if (screenbox->left <= projected.x) {
                count_left++;
            }

            if (projected.x <= screenbox->right) {
                count_right++;
            }

            if (screenbox->up <= projected.y) {
                count_top++;
            }

            if (projected.y <= screenbox->down) {
                count_bottom++;
            }

            count_failed_projection++;
        } else {
            if (zrange[1] <= -projected.z) {
                count_z++;
            }

            if (projected.x <= screenbox->left) {
                count_left++;
            } else if (screenbox->right <= projected.x) {
                count_right++;
            }

            if (projected.y <= screenbox->up) {
                count_top++;
            } else if (screenbox->down <= projected.y) {
                count_bottom++;
            }
        }
    }

    /**
     * If all 8 of the room's bounding box corners are behind the camera,
     * reject this room
     */
    if (count_failed_projection == 8
            || count_z == 8
            || count_left == 8
            || count_right == 8
            || count_top == 8
            || count_bottom == 8) {
        return FALSE;
    }

    return TRUE;
}


bool bgProjectRoomCoordToScreen(coord3d* src, coord3d* dst)
{
    Mtxf* temp_a0;

    temp_a0 = camGetWorldToScreenMtxf();
    dst->x = src->x * g_BgLevelScaleInv;
    dst->y = src->y * g_BgLevelScaleInv;
    dst->z = src->z * g_BgLevelScaleInv;
    mtx4TransformVecInPlace(temp_a0, dst);

    camViewToScreenSafe(dst, dst);

    if (dst->z > 0.0f)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}


s32 bgProjectPortalPoints(s32 portalnum, f32 arg1, coord3d *arg2)
{
    Mtxf *matrix;
    coord3d *point;
    s32 i;
    s32 next;
    f32 zrange[2];
    s32 allbehind;
    struct PortalMetric metric;
    s32 len;

    matrix = camGetWorldToScreenMtxf();
    allbehind = 1;
    viGetZRange(zrange);
    zrange[1] /= mCurrentLevelVisibilityScale;

    for (i = 0; i < g_BgPortals[portalnum].offset_portal->numPoints; i++) {
        point = &arg2[i];

        point->x = (&g_BgPortals[portalnum].offset_portal->point)[i].x;
        point->y = (&g_BgPortals[portalnum].offset_portal->point)[i].y;
        point->z = (&g_BgPortals[portalnum].offset_portal->point)[i].z;

        if (arg1 != 0.0f) {
            bgGetPortalPlane(portalnum, (f32 *) &metric);

            point->x += metric.normal.x * arg1;
            point->y += metric.normal.y * arg1;
            point->z += metric.normal.z * arg1;
        }

        point = &arg2[i];
        point->x *= g_BgLevelScaleInv;
        point->y *= g_BgLevelScaleInv;
        point->z *= g_BgLevelScaleInv;

        mtx4TransformVecInPlace(matrix, point);

        if (-zrange[1] * 0.9f < point->z) {
            allbehind = 0;
        }
    }

    if (allbehind) {
        return 0;
    }

    len = g_BgPortals[portalnum].offset_portal->numPoints;

    for (i = 0; i < g_BgPortals[portalnum].offset_portal->numPoints; i++) {
        point = &arg2[i];
        next = (i + 1) % g_BgPortals[portalnum].offset_portal->numPoints;

        if ((point->z > 0.0f && arg2[next].z <= 0.0f) || (point->z <= 0.0f && arg2[next].z > 0.0f)) {
            f32 scale = -point->z / (arg2[next].z - point->z);

            arg2[len].x = point->x + ((arg2[next].x - point->x) * scale);
            arg2[len].y = point->y + ((arg2[next].y - point->y) * scale);
            point = &arg2[i];
            arg2[len].z = 0.0f;
            len++;
        }
    }

    return len;
}


s32 bgGetPortalScreenBox(s32 portalnum, bbox2d *bbox)
{
    f32 scale;
    PortalCache *cache;
    s32 j;
    coord3d points[19];
    coord2d screenpos;
    bbox2d bounds;
    s32 pointcount;
    s32 onscreencount;
    s32 i;

    cache = &g_BgPortalCache[portalnum];

    if (cache->count >= 0)
    {
        bbox->min.x = cache->bbox.min.x;
        bbox->min.y = cache->bbox.min.y;
        bbox->max.x = cache->bbox.max.x;
        bbox->max.y = cache->bbox.max.y;
        
        return cache->count;
    }

    scale = bgGetPortalThickness(portalnum);
    pointcount = bgProjectPortalPoints(portalnum, scale, points);

    if (scale > 0.0f)
    {
        pointcount += bgProjectPortalPoints(portalnum, -scale, &points[pointcount]);
    }

    onscreencount = 0;
    i = 0;

    if (pointcount > 0)
    {
        j = 0;

        do
        {
            if (points[j].z <= 0.0f)
            {
                camViewToScreenSafe(&points[j], &screenpos);

                if (onscreencount == 0)
                {
                    bounds.min.x = (bounds.max.x = screenpos.x);
                    bounds.min.y = (bounds.f[1][1] = screenpos.f[1]);
                }
                else
                {
                    if (screenpos.x < bounds.min.x)
                    {
                        bounds.min.x = screenpos.x;
                    }
                    
                    if (bounds.max.x < screenpos.x)
                    {
                        bounds.max.x = screenpos.x;
                    }
                    
                    if (screenpos.y < bounds.min.y)
                    {
                        bounds.min.y = screenpos.y;
                    }
                    
                    if (bounds.max.y < screenpos.y)
                    {
                        bounds.max.y = screenpos.y;
                    }
                }

            onscreencount++;
            }

        i++;
        j++;
            
        }
        while (i != pointcount);
    }

    if (onscreencount == 0)
    {
        bounds.max.y = (bounds.min.x = 0.0f);
        bounds.min.y = 0.0f;
        bounds.max.x = 0.0f;
    }
    else
    {
        if ((bounds.max.x <= bounds.min.x) || (bounds.max.y <= bounds.min.y))
        {
            bounds.min.x = g_CurrentPlayer->screensize.min.x;
            bounds.min.y = g_CurrentPlayer->screensize.min.y;
            bounds.max.x = g_CurrentPlayer->screensize.max.x;
            bounds.max.y = g_CurrentPlayer->screensize.max.y;
        }
    }

    bbox->min.x = bounds.min.x;
    bbox->min.y = bounds.min.y;
    bbox->max.x = bounds.max.x;
    bbox->max.y = bounds.max.y;

    cache->bbox.min.x = bbox->min.x;
    cache->bbox.min.y = bbox->min.y;
    cache->bbox.max.x = bbox->max.x;
    cache->bbox.max.y = bbox->max.y;
    cache->count = onscreencount;

    return onscreencount;
}


/**
 * Determines if two rectangles overlap, adjusting first argument to be the intersection.
 *
 * Returns 0 if no intersection (or edge equality), 1 otherwise.
 */
s32 bgRectIntersect(struct bbox2d *a, struct bbox2d *b)
{
	a->min.x = a->min.x > b->min.x ? a->min.x : b->min.x;
	a->min.y = a->min.y > b->min.y ? a->min.y : b->min.y;
	a->max.x = b->max.x > a->max.x ? a->max.x : b->max.x;
	a->max.y = b->max.y > a->max.y ? a->max.y : b->max.y;

	if (a->min.x >= a->max.x) {
		a->min.x = a->max.x;
		return FALSE;
	}

	if (a->max.y <= a->min.y) {
		a->min.y = a->max.y;
		return FALSE;
	}

	return TRUE;
}


// Does a union. Increases the size of 'a' so it contains 'b'.
void bgRectOutersect(struct bbox2d *a, struct bbox2d *b)
{
    (a->min).x = ((a->min).x < (b->min).x) ? (a->min).x : (b->min).x;
    (a->min).y = ((a->min).y < (b->min).y) ? (a->min).y : (b->min).y;
    (a->max).x = ((a->max).x > (b->max).x) ? (a->max).x : (b->max).x;
    (a->max).y = ((a->max).y > (b->max).y) ? (a->max).y : (b->max).y;
}

// Does a shallow copy of 'b' into 'a'. Equivalent to '*a = *b;'.
void bbox2dCopy(struct bbox2d *a, struct bbox2d *b)
{
    (a->min).x = (b->min).x;
    (a->min).y = (b->min).y;
    (a->max).x = (b->max).x;
    (a->max).y = (b->max).y;
}


/**
 * Formats a room ID for debug output.
 */
char *bgDebPrintROOMID(s32 roomId)
{
    static char bgDebRoomOutBuffer[10][9];
    static s32 bgDebRoomOutLineNum = 0;
    char *roomIdStr;

    bgDebRoomOutLineNum = (bgDebRoomOutLineNum + 1) % 10;
    roomIdStr = bgDebRoomOutBuffer[bgDebRoomOutLineNum];

    sprintf(roomIdStr, "ROOM%d", roomId);

    return roomIdStr;
}


/**
 * These definitions must come AFTER bgDebPrintROOMID for matching.
 */

bg_queued_portal_entry g_BgPortalQueue[BG_PORTAL_QUEUE_LEN];

bg_portal_data_entry *g_BgPortals;
s32 g_BgHeader;
bg_room_data *g_BgRoomFiles;
s32 *g_BgVisCommands;
f32 *g_BgRoomValues;

#ifdef VERSION_EU
s_bound_info g_BgRoomsDrawn[124];
#else
s_bound_info g_BgRoomsDrawn[BG_ROOMS_DRAWN_LEN];
#endif

/* Portals visited this frame (counted, never read) */
s32 g_BgPortalVisits = 0;
/* How many portals deep the visibility search goes */
s32 g_BgMaxPortalDepth = 15;
s32 g_BgPortalQueueWriteIndex = 0;
s32 g_BgPortalQueueReadIndex = 0;

/**
 * Local stack.
 */
s32 g_BgStack[BG_STACK_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/**
 * Current top of the stack.
 */
s32 g_BgStackCount = 0;

s32 current_visibility = 0;

/* Set by bgSegmentCrossesPortal: how far the segment's midpoint is from the portal plane */
f32 g_BgSegmentPortalDistance = 0;



/*
    //###RenderMode / Combiner Look - Up - Tables
    The reason for this LUT is to dynamicly change the rendermode and combiner to
    FOG / NoFog or any other setting they might have wanted to test during development
    as it applies during runtime

//###Reminder:
    1cycle combiners repeat both cycles
    gDPSetCombineMode(G_CC_MODULATERGBA, G_CC_MODULATERGBA2)

    combiner macros are a list of parameters that form a mathematical sum.
                            (       -  )*     +  ,  (       -  )*     +
    G_CC_MODULATERGBA2	    COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0
*/
Gfx DL_LUT_UNKNOWN[] = {
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, 0, SCALE, 0,  TEXEL1, 0, PRIM_LOD_FRAC, 0,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    0,0
};

//D:80044940 - Primary
Gfx DL_LUT_PRIMARY_ADDFOG[] = {
    //Add FOG to all rendermodes
    //Standard HiQuality Surface to Standard Fogable HiQuality Surface
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2),
    //Terrain to Fogable Terrain
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_TERR2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_TERR2),
    //Standard DECAL to FOG DECAL
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_DECAL2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_DECAL2),
    //Transparent DECAL to  FOG Transparent DECAL
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_DECAL2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_DECAL2),
    //Transparent Surface to FOG Transparent Surface
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2),
    // Billboard Cut-out to FOG Billboard Cut-out - eg, Mario Tree or Depot lamp
    // See PGDLists\Transparent Textures.htm for more info
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2),
    //Standard Z-Less OPA to Standard FOG Z-Less OPA
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_OPA_SURF2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_OPA_SURF2),
    //Z-Less OPA Terrain to Z-Less Fog OPA Terrain
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_OPA_TERR2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_OPA_TERR2),
    0x0, 0x0
};

//D:800449C8 - Secondary
Gfx DL_LUT_SECONDARY_ADDFOG[] = {
    //Add FOG to Rendermodes
    //Transparent DECAL to  FOG Transparent DECAL
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_DECAL2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_DECAL2),
    //Transparent Surface to FOG Transparent Surface
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2),

    // Billboard Cut-out to FOG Billboard Cut-out - eg, Mario Tree or Depot lamp
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2),

    // Swap all refrences to Shade in Alpha to Environment
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE),//gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT), //gsDPSetCombineMode(G_CC_SHADE_EnvA, G_CC_SHADE_EnvA),
    // This one is an oddball... its extra here AND is weird using Tile1 only for Alpha
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    0x0,
    0x0
};

// Loaded once on first time entering level, only once ever
// Swap all refrences to Shade in Alpha to Environment
Gfx DL_LUT_PRIMARY[] = {
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT),
    0,0
};

// Loaded once on first time entering level, only once ever
// Swap all refrences to Shade in Alpha to Environment
Gfx DL_LUT_SECONDARY[] = {
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT),
    0,0
};

Gfx DL_LUT_BILLBOARD[] = {
    //Transparent 1Cycle to  BillBoard 1Cycle
    gsDPSetRenderMode(G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2), gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    //Transparent Surface to Billboard
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2), gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2),
    0x0,0x0
};

//water
Gfx DL_LUT_WATER[] = {
    0xB900031D, 0x00552078, 0xB900031D, 0x00502078,
    0xB900031D, 0x0C192078, 0xB900031D, 0x0C182078,
    /*
    //1 Cycle Opa to Particle
    gsDPSetRenderMode(RM_AA_ZB_OPA_SURF, RM_AA_ZB_OPA_SURF2), gDPSetRenderMode(G_RM_AA_ZB_PCL_SURF, G_RM_AA_ZB_PCL_SURF2),
    //2 cycle Opa to Particle
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2), gDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_PCL_SURF2),
    */
    0x0, 0
};

Gfx DL_LUT_CLOUD[] = {
    //Transparent to Cloud (Saves AA - Stops Jaggies from appearing behind BillBoard)
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2), gsDPSetRenderMode(G_RM_PASS, G_RM_ZB_CLD_SURF2),
    0,0
};

//(Wallet Bond - Main Menu)
Gfx DL_LUT_WALLETBOND[] = {
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPSetCycleType(G_CYC_2CYCLE), //1Cycle --> 2Cycle
    0xB900031D, 0x00502048,
    0xB900031D, 0x08D02048,
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    /*
    //1 Cycle particle Surface to 2 Cycle colour + 1-a*Fog ???
    gsDPSetCycleType(G_CYC_2CYCLE),
    gsDPSetRenderMode(G_RM_AA_PCL_SURF, G_RM_AA_PCL_SURF2), gDPSetRenderMode(AA_EN | IM_RD | CVG_DST_CLAMP | ALPHA_CVG_SEL | ZMODE_OPA | GBL_c1(G_BL_CLR_IN, G_BL_A_SHADE, G_BL_CLR_FOG, G_BL_1MA) | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsDPSetCombineMode(G_CC_MODULATERGBA, G_CC_PASS2), gDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATERGBA2),
    */
    0x0, 0
};

Gfx DL_LUT_FIXFOGALPHA3[] = {
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SCALE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SCALE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SCALE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SCALE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(CENTER, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT,  CENTER, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SCALE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SCALE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SCALE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SCALE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(CENTER, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    0,0
};

Gfx *ptrDynamic_CC_RM_LUT[] = {
    &DL_LUT_UNKNOWN, &DL_LUT_PRIMARY_ADDFOG, &DL_LUT_BILLBOARD, &DL_LUT_WATER, &DL_LUT_CLOUD,
    &DL_LUT_SECONDARY_ADDFOG, &DL_LUT_PRIMARY, &DL_LUT_SECONDARY, &DL_LUT_WALLETBOND, &DL_LUT_FIXFOGALPHA3
};


s32 bgGetMaxNumRooms(void) 
{
    return g_MaxNumRooms;
}


/*
 * Return butflags0 (confirmed u8)
 */
u8 bgRoomIsVisible(s32 roomID)
{
    return g_BgRoomInfo[roomID].room_rendered;
}


s32 bgGetPortalIndex(s32 portalID)
{
    s32 i;

    for(i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if (portalID == (s32)g_BgPortals[i].offset_portal)
        {
            return i;
        }
    }
    return 0;
}


u32 bgDecompress(u8* source, u8 *target)
{
    u8 buffer[0x2100];
    return rzipInflate(source, target, buffer);
}


/**
 * Load room's compressed vertex table from the bg file, decompress it
 * into dst, and store the resulting Vtx buffer in room.
 */
s32 bgLoadRoomVtxData(s32 roomnum, u8 *dst, s32 len)
{
    s_room_info *room;
    s32 alignedsize;
    s32 offset;
    s32 result;

    room = &g_BgRoomInfo[roomnum];
    alignedsize = (room->csize_point_index_binary + 0xf) & ~0xf;
    if (len < alignedsize + 0x20) {
        return -1;
    }

    /**
    * pPointTableBin is stored as a segment-0x0f bgdata address.
    * The seemingly unncessary " + g_BgData - g_BgData" likely comes from paired bgdata pointer/offset macros.
    * Adding 0xf1000000 strips the 0x0f000000 segment tag, yielding a file offset.
    */
    offset = (((u8 *)g_BgRoomFiles[roomnum].pPointTableBin + g_BgData) - g_BgData) + 0xf1000000;
    fileLoadPartByName(levelinfotable[levelentry_index].bg_seg_filename, dst + (len - alignedsize), offset, alignedsize);
    result = bgDecompress(dst + (len - alignedsize), dst);

    room->vertices = (Vtx *)dst;
    room->usize_point_index_binary = result;

    return result;
}


/**
 * Load and decompress a room's primary display list data.
 *
 * On success, roominfo->ptr_expanded_mapping_info is set to dst,
 * and roominfo->usize_primary_DL_binary is set to the returned size.
 */
s32 bgLoadRoomPrimaryGdl(s32 roomnum, u8 *dst, s32 allocsize)
{
    s_room_info *roominfo;
    s32 size;
    s32 fileoffset;
    u8 *scratch;
    s32 expanded_size;

    roominfo = &g_BgRoomInfo[roomnum];

    size = roominfo->csize_primary_DL_binary;
    size = (size + 0xf) & ~0xf; // Align to 16 bytes

    /**
     * Check if there is enough room to temporarily place the compressed data
     * at the end of the available buffer. Return -1 if there's not.
     */
    if (allocsize < size + 0x20) {
        return -1;
    }

    // Load the compressed data into the end of the buffer, starting at dst.
    scratch = dst + (allocsize - size);

    fileoffset = (s32)((u8 *)g_BgRoomFiles[roomnum].pPriMappingBin + g_BgData) - g_BgData;
    fileoffset += 0xf1000000;

    fileLoadPartByName(levelinfotable[levelentry_index].bg_seg_filename, scratch, fileoffset, size);

    // Decompress from the end-of-buffer location at dst.
    expanded_size = bgDecompress(scratch, dst);

    /**
     * Copy the decompressed GDL back to the end of the buffer as scratch.
     * texLoadFromGdl can then read from scratch and write the final
     * texture-processed GDL/data back to dst.
     */
    scratch = dst + (allocsize - expanded_size);

    texCopyGdls((Gfx *)dst, (Gfx *)scratch, expanded_size);

    lightFixtureClearRoom(roomnum);

    size = texLoadFromGdl((Gfx *)scratch, expanded_size, (Gfx *)dst, NULL);

    if (expanded_size < size) {
        expanded_size = size;
    }

    roominfo->ptr_expanded_mapping_info = dst;
    roominfo->usize_primary_DL_binary = expanded_size;

    // Return the uncompressed data size.
    return expanded_size;
}


/**
 * Load and decompress a room's secondary display list data.
 *
 * On success, roominfo->ptr_secondary_expanded_mapping_info is set to dst,
 * and roominfo->usize_secondary_DL_binary is set to the returned size.
 */
s32 bgLoadRoomSecondaryGdl(s32 roomnum, u8 *dst, s32 allocsize)
{
    s_room_info *roominfo;
    s32 size;
    s32 fileoffset;
    u8 *scratch;
    s32 expanded_size;

    roominfo = &g_BgRoomInfo[roomnum];

    size = roominfo->csize_secondary_DL_binary;
    size = (size + 0xf) & ~0xf; // Align to 16 bytes

    /**
     * Check if there is enough room to temporarily place the compressed data
     * at the end of the available buffer. Return -1 if there's not.
     */
    if (allocsize < size + 0x20) {
        return -1;
    }

    // Load the compressed data into the end of the buffer, starting at dst.
    scratch = dst + (allocsize - size);

    fileoffset = (s32)((u8 *)g_BgRoomFiles[roomnum].pSecMappingBin + g_BgData)  - g_BgData;
    fileoffset += 0xf1000000;

    fileLoadPartByName(levelinfotable[levelentry_index].bg_seg_filename, scratch, fileoffset, size);

    // Decompress from the end-of-buffer location at dst.
    expanded_size = bgDecompress(scratch, dst);

    /**
     * Copy the decompressed GDL back to the end of the buffer as scratch.
     * texLoadFromGdl can then read from scratch and write the final
     * texture-processed GDL/data back to dst.
     */
    scratch = dst + (allocsize - expanded_size);

    texCopyGdls((Gfx *)dst, (Gfx *)scratch, expanded_size);

    size = texLoadFromGdl((Gfx *)scratch, (Gfx *)expanded_size, (Gfx *)dst, NULL);

    if (expanded_size < size) {
        expanded_size = size;
    }

    roominfo->ptr_secondary_expanded_mapping_info = dst;
    roominfo->usize_secondary_DL_binary = expanded_size;

    // Return the uncompressed data size.
    return expanded_size;
}


s32 bgCheckIfRoomModelNeedsLoad(s32 roomID)
{
    g_BgRoomInfo[roomID].requested = 1;
    if (g_BgRoomInfo[roomID].model_bin_loaded == 0)
    {
        bgLoadRoomModelData(roomID);
        return 1;
    }
    return 0;
}


/*
* Allocates memory for room and update its display lists
*
* When a room is first allocated, the game will pick the largest block
* available. It doesn't know the size of the decompressed asset as its
* size is not stored as part of the GZIP format. It will then shrink
* the allocated block to the correct size. The size is cached for the
* next time the room is reloaded.
*/
void bgLoadRoomModelData(s32 roomID)
{
    /*
     * Keep this prototype visible here for IDO codegen.
     * Without it, the memaRealloc call below schedules a2 too early.
     */
    s32 memaRealloc(s32 addr, u32 oldsize, u32 newsize);

    s32 allocsize;
    s32 used;
    s32 result;
    u8 *data;

    used = 0;

    // Room ID is out of range?
    if (roomID >= g_MaxNumRooms) goto end;

    // Room is already loaded?
    if (g_BgRoomInfo[roomID].model_bin_loaded) goto end;

    // Get the cached file size. Is zero when the size is not yet known.
    allocsize = g_BgRoomInfo[roomID].cur_room_totalsize;

    if (allocsize > 0)
    {
        // Unknown debug code
    }
    else
    {
        // On a first allocation, we'll allocate the largest memory block available.
        allocsize = memaGetLongestFree();
    }

    /**
    * Allocate one contiguous block for vertices and display lists.
    */
    data = memaAlloc(allocsize);

    if (data == NULL) goto end;

    if (g_BgRoomInfo[roomID].csize_point_index_binary)
    {
        result = bgLoadRoomVtxData(roomID, data, allocsize);

        if (result >= 0)
        {
            used = result;
            lightRedarkenRoom(roomID);
        }
    }
    else
    {
        g_BgRoomInfo[roomID].vertices = NULL;
        g_BgRoomInfo[roomID].usize_point_index_binary = 0;
    }

    /**
     * Append the primary display list after the vertex data.
     */
    if (g_BgRoomInfo[roomID].csize_primary_DL_binary)
    {
        result = bgLoadRoomPrimaryGdl(roomID, data + used, allocsize - used);

        if (result >= 0)
        {
            used += result;
        }
    }

    /**
     * Append the secondary display list.
     */
    if (g_BgRoomInfo[roomID].csize_secondary_DL_binary)
    {
        result = bgLoadRoomSecondaryGdl(roomID, data + used, allocsize - used);

        if (result > 0)
        {
            used += result;
        }
    }
    else
    {
        g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info = NULL;
    }

    g_BgRoomInfo[roomID].cur_room_totalsize = ((used + 0x20) & ~0xf);
    g_BgRoomInfo[roomID].model_bin_loaded = 1;

    // If wasted space is detected, shrink allocated memory block.
    if (allocsize != ((used + 0x20) & ~0xf))
    {
        memaRealloc((s32)data, allocsize, ((used + 0x20) & ~0xf));
    }

    // Same branches, only the LUT parameter changes
    if (g_FogSkyIsEnabled)
    {
        bgApplyDynamicCCRMLUT(
            g_BgRoomInfo[roomID].ptr_expanded_mapping_info,
            (Gfx *)((u8 *)g_BgRoomInfo[roomID].ptr_expanded_mapping_info + g_BgRoomInfo[roomID].usize_primary_DL_binary),
            1);

        if (g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info)
        {
            bgApplyDynamicCCRMLUT(
                g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info,
                (Gfx *)((u8 *)g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info + g_BgRoomInfo[roomID].usize_secondary_DL_binary),
                5);
        }
    }
    else
    {
        bgApplyDynamicCCRMLUT(
            g_BgRoomInfo[roomID].ptr_expanded_mapping_info,
            (Gfx *)((u8 *)g_BgRoomInfo[roomID].ptr_expanded_mapping_info + g_BgRoomInfo[roomID].usize_primary_DL_binary),
            6);

        if (g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info)
        {
            bgApplyDynamicCCRMLUT(
                g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info,
                (Gfx *)((u8 *)g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info + g_BgRoomInfo[roomID].usize_secondary_DL_binary),
                7);
        }
    }

    bgBuildRoomVtxBounds(roomID);

end:;

}


/**
 * Given a room, frees all dynamically allocated data for that room and marks the room as unloaded.
 */
void bgUnloadRoom(s32 roomID)
{
    s_room_info *room = &g_BgRoomInfo[roomID];
    s32 size;
    s32 size2;
    Vtx *pointindex;

    if (room->vtx_batch_bounds != NULL) {
        memaFree(
            room->vtx_batch_bounds,
            ((room->num_vtx_batch_bounds * sizeof(RoomVtxBatchBounds)) + 0xf) & ~0xf);

        room->vtx_batch_bounds = NULL;
    }

    if (room->cur_room_totalsize > 0) {
        size = room->cur_room_totalsize;
        pointindex = room->vertices;

        if (pointindex != NULL)
        {
            size2 = room->cur_room_totalsize;
            memaFree(pointindex, size2);
            room->vertices = NULL;
        }
        else
        {
            memaFree(room->ptr_expanded_mapping_info, size);
            room->vertices = NULL;
        }

        room->ptr_expanded_mapping_info = NULL;
        room->ptr_secondary_expanded_mapping_info = NULL;
    }

    room->model_bin_loaded = 0;
}


/**
 * Immediately unload all loaded rooms.
 * Used for stage cleanup.
 */
void bgUnloadAllRooms(void)
{
    s32 i;

    for(i = 1; i < g_MaxNumRooms; i++)
    {
        if (g_BgRoomInfo[i].model_bin_loaded)
        {
            bgUnloadRoom(i);
        }
    }
}


/**
 * Ages loaded rooms that are no longer marked active, then unloads them
 * once their unload delay expires.
 */
void bgRoomsTickUnload(void)
{
    s32 i;

    for(i = 1; i < g_MaxNumRooms; i++)
    {
        if (g_BgRoomInfo[i].requested == 0)
        {
            if (g_BgRoomInfo[i].model_bin_loaded == 4)
            {
                bgUnloadRoom(i);
            }
            else if (g_BgRoomInfo[i].model_bin_loaded != 0)
            {
                g_BgRoomInfo[i].model_bin_loaded = g_BgRoomInfo[i].model_bin_loaded + 1;
            }
        }
    }
}


/**
 * Render a room's primary (solid) geometry.
 * Ensures the room's bg data is loaded if budget allows, then appends its display list.
 * Also resets the age of rendered rooms so bgRoomsTickUnload won't unload it.
*/
Gfx *bgRenderRoomPrimary(Gfx *gdl, s32 room_index)
{
    if (room_index >= g_MaxNumRooms)
    {
        return gdl;
    }

    if ((g_BgLoadAllRooms != 0) || (g_BgRoomLoadSlot == (room_index % 10)))
    {
        if (g_BgRoomInfo[room_index].model_bin_loaded == 0)
        {
            if (g_RoomLoadBudget > 0)
            {
                g_RoomLoadBudget--;
                bgLoadRoomModelData(room_index);
            }
        }

        if (g_BgRoomInfo[room_index].model_bin_loaded == 0)
        {
            return gdl;
        }
        else
        {
            gdl = roomMtxLoad(gdl, room_index);

            gSPSegment(gdl++, SPSEGMENT_BG_VTX, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].vertices));
            gSPDisplayList(gdl++, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].ptr_expanded_mapping_info));

            // Set the room's state to "loaded"
            g_BgRoomInfo[room_index].model_bin_loaded = 1;
        }
    }

    return gdl;
}


/**
 * Render a room's secondary (transparent) geometry.
*/
Gfx *bgRenderRoomSecondary(Gfx *gdl, s32 room_index)
{
    if (room_index >= g_MaxNumRooms)
    {
        return gdl;
    }

    // Return if the room has no secondary geometry.
    if (g_BgRoomInfo[room_index].ptr_secondary_expanded_mapping_info == 0)
    {
        return gdl;
    }
    else if ((g_BgLoadAllRooms != 0) || (g_BgRoomLoadSlot == (room_index % 10)))
    {
        if (g_BgRoomInfo[room_index].model_bin_loaded != 0)
        {
            gdl = roomMtxLoad(gdl, room_index);

            gSPSegment(gdl++, SPSEGMENT_BG_VTX, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].vertices));
            gSPDisplayList(gdl++, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].ptr_secondary_expanded_mapping_info));

            // Set the room's state to "loaded"
            g_BgRoomInfo[room_index].model_bin_loaded = 1;
        }
        else
        {
            bgLoadRoomModelData(room_index);
        }
    }

    return gdl;
}


/*
* Build world space bounds for each vertex batch loaded by the room's display list.
*/
#define ALIGN16(val) (((val) + 0xF) & ~0xF)
#define	SEGMENT_OFFSET(a)	((unsigned int)(a) & 0x00ffffff)

void bgBuildRoomVtxBounds(s32 roomID)
{
    s32 cmdindex;
    Gfx *gdl;
    Vtx *vertices;
    RoomVtxBatchBounds *points;
    s32 numpoints;
    RoomVtxBatchBounds *point;
    s32 i;
    s32 numvertices;
    Vtx *vtx;

    // Check if a cached bounding box is already present for this room
    if (g_BgRoomInfo[roomID].vtx_batch_bounds != NULL)
    {
        return;
    }

    gdl = g_BgRoomInfo[roomID].ptr_expanded_mapping_info;

    vertices = g_BgRoomInfo[roomID].vertices;
    cmdindex = 0;
    numpoints = 0;

    while (gdl[cmdindex].dma.cmd != G_ENDDL)
    {
        if (gdl[cmdindex].dma.cmd == G_VTX) 
        {
            numpoints++;
        }
        cmdindex++;
    }

    points = memaAlloc(ALIGN16(numpoints * sizeof(RoomVtxBatchBounds)));


    if (points == NULL)
    {
        return;
    }

    g_BgRoomInfo[roomID].vtx_batch_bounds = points;
    g_BgRoomInfo[roomID].num_vtx_batch_bounds = numpoints;

    numpoints = 0;
    cmdindex = 0;

    while (gdl[cmdindex].dma.cmd != G_ENDDL)
    {
        if (gdl[cmdindex].dma.cmd == G_VTX)
        {
            point = &points[numpoints];

            points[numpoints].gdlindex = cmdindex;

            for (i = 0; i < 3; i++) 
            {
                points[numpoints].min[i] = 0x7fff;
                points[numpoints].max[i] = -0x8000;
            }

            numvertices = ((gdl[cmdindex].dma.par >> 4) & 0xf) + 1;

            vtx = (Vtx *)(SEGMENT_OFFSET(gdl[cmdindex].dma.addr) + (u32)vertices);

            for (i = 0; i < numvertices; i++)
            {
                if (points[numpoints].xmin > vtx[i].v.ob[0])
                {
                    points[numpoints].xmin = vtx[i].v.ob[0];
                }

                if (points[numpoints].ymin > vtx[i].v.ob[1])
                {
                    points[numpoints].ymin = vtx[i].v.ob[1];
                }

                if (points[numpoints].zmin > vtx[i].v.ob[2])
                {
                    points[numpoints].zmin = vtx[i].v.ob[2];
                }

                if (points[numpoints].xmax < vtx[i].v.ob[0])
                {
                    points[numpoints].xmax = vtx[i].v.ob[0];
                }

                if (points[numpoints].ymax < vtx[i].v.ob[1])
                {
                    points[numpoints].ymax = vtx[i].v.ob[1];
                }

                if (points[numpoints].zmax < vtx[i].v.ob[2])
                {
                    points[numpoints].zmax = vtx[i].v.ob[2];
                }
            }


            if (points[numpoints].xmin == points[numpoints].xmax)
            {
                points[numpoints].xmax++;
            }

            if (points[numpoints].ymin == points[numpoints].ymax)
            {
                points[numpoints].ymax++;
            }

            if (points[numpoints].zmin == points[numpoints].zmax)
            {
                points[numpoints].zmax++;
            }


            points[numpoints].xmin += (s32)g_BgRoomFiles[roomID].pos.x;
            points[numpoints].ymin += (s32)g_BgRoomFiles[roomID].pos.y;
            points[numpoints].zmin += (s32)g_BgRoomFiles[roomID].pos.z;

            points[numpoints].xmax += (s32)g_BgRoomFiles[roomID].pos.x;
            points[numpoints].ymax += (s32)g_BgRoomFiles[roomID].pos.y;
            points[numpoints].zmax += (s32)g_BgRoomFiles[roomID].pos.z;
            numpoints++;
        }
        cmdindex++;
    }
}

#undef ALIGN16
#undef SEGMENT_OFFSET


/**
 * Slab method ray vs axis aligned bounding box test, written to be division-free.
 */
bool bgTestRayIntersectsBbox(coord3d *origin, coord3d *dir, s32 *bbox_min, s32 *bbox_max)
{
    coord3d bbox_min_f;
    coord3d bbox_max_f;
    f32 f0;
    f32 f0_2;
    f32 f2;
    f32 f2_2;
    f32 f6;
    f32 f10;
    f32 sp34;
    f32 sp30;
    f32 f16;
    f32 f18;
    f32 f18_2;
    f32 sp20;
    f32 f12;
    f32 f12_2;
    f32 f14;
    f32 f14_2;

    bbox_min_f.f[0] = bbox_min[0];
    bbox_min_f.f[1] = bbox_min[1];
    bbox_min_f.f[2] = bbox_min[2];

    bbox_max_f.f[0] = bbox_max[0];
    bbox_max_f.f[1] = bbox_max[1];
    bbox_max_f.f[2] = bbox_max[2];

    // x
    f18 = dir->x;
    f16 = bbox_max_f.x - origin->x;
    f14 = bbox_min_f.x - origin->x;

    if (f18 < 0.0f)
    {
        f18 = -f18;
        f14 = -f14;
        f16 = -f16;
    }

    if (f14 < 0.0f && f16 < 0.0f)
    {
        return FALSE;
    }

    if (f16 < f14)
    {
        f32 tmp = f14;
        f14 = f16;
        f16 = tmp;
    }

    // y
    f12 = dir->y;
    f2 = bbox_max_f.y - origin->y;
    f0 = bbox_min_f.y - origin->y;

    if (f12 < 0.0f)
    {
        f12 = -f12;
        f0 = -f0;
        f2 = -f2;
    }

    if (f0 < 0.0f && f2 < 0.0f)
    {
        return FALSE;
    }

    if (f2 < f0)
    {
        sp20 = f0;
        f0 = f2;
        f2 = sp20;
    }

    f6 = f14 * f12;
    f10 = f0 * f18;

    if (f10 < f6)
    {
        if (f2 * f18 < f6)
        {
            return FALSE;
        }

        sp34 = f14;
        sp30 = f18;
    }
    else
    {
        if (f16 * f12 < f10)
        {
            return FALSE;
        }

        sp34 = f0;
        sp30 = f12;
    }

    if (f16 * f12 < f2 * f18)
    {
        f0_2 = f16;
        f14_2 = f18;
    }
    else
    {
        f0_2 = f2;
        f14_2 = f12;
    }

    // z
    f2_2 = dir->z;
    f12_2 = bbox_max_f.z - origin->z;
    f18_2 = bbox_min_f.z - origin->z;

    if (f2_2 < 0.0f)
    {
        f2_2 = -f2_2;
        f18_2 = -f18_2;
        f12_2 = -f12_2;
    }

    if (f18_2 < 0.0f && f12_2 < 0.0f)
    {
        return FALSE;
    }

    if (f12_2 < f18_2)
    {
        f32 tmp = f18_2;
        f18_2 = f12_2;
        f12_2 = tmp;
    }

    if (sp34 * f2_2 < f18_2 * sp30)
    {
        if (f0_2 * f2_2 < f18_2 * f14_2)
        {
            return FALSE;
        }
    }
    else
    {
        if (f12_2 * sp30 < sp34 * f2_2)
        {
            return FALSE;
        }
    }

    return TRUE;
}


bool bgTestRayIntersectionInRoom(coord3d *from, coord3d *to, coord3d *dir, RoomVtxBatchBounds *point, s32 roomnum, struct HitThing *hitthing)
{
    Vertex *vtxbase;
    Vertex *v;

    union {
        s32 word;
        Vertex *vertices;
        s_room_info *roominfo;
    } temp;

    s32 op;
    s32 found;
    Gfx *gdl;
    HitThing hitbuf;
    Gfx *tcmd;
    s32 bestScore;
    s32 idx[3];
    s32 vtxoff;
    BoundVec bboxMin;
    BoundVec bboxMax;
    s32 i;
    s32 s2;
    s32 texnum;
    s32 dx;
    s32 dy;
    s32 dz;
    s32 dist;
    s32 idx2[3];
    s32 score;
    BoundVec bboxMin2;
    BoundVec bboxMax2;

    struct {
        s32 unused;
        s_room_info *roominfo;
        s32 padding[6];
    } local;

    gdl = (Gfx *) g_BgRoomInfo[roomnum].ptr_expanded_mapping_info;
    gdl = &gdl[point->gdlindex];
    vtxoff = ((u8 *) gdl)[1] & 0xf;
    temp.vertices = (Vertex *)g_BgRoomInfo[roomnum].vertices;
    vtxbase = (Vertex *)((s32)temp.vertices + (((u32 *)gdl)[1] & 0x00ffffff));
    temp.roominfo = &g_BgRoomInfo[roomnum];
    bestScore = 0x7FFFFFFF;
    found = 0;
    gdl++;
    op = *((s8 *) gdl);

    if ((op != G_VTX) && (op != ((s8) G_ENDDL)))
    {
        local.roominfo = temp.roominfo;

        do
        {
            if (op == ((s8) G_TRI1))
            {
                bboxMin = (BoundVec){0x7FFF, 0x7FFF, 0x7FFF};
                bboxMax = (BoundVec){-0x8000, -0x8000, -0x8000};
                score = dist;
                idx[0] = (((u8 *) gdl)[5] / 10) - vtxoff;
                idx[1] = (((u8 *) gdl)[6] / 10) - vtxoff;
                idx[2] = (((u8 *) gdl)[7] / 10) - vtxoff;
                i = 0;
                
                do
                {
                    v = vtxbase;
                    v += idx[i];

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
                    i++;
                } 
                while (i < 3);

                bboxMin.x += (s32) g_BgRoomFiles[roomnum].pos.x;
                bboxMin.y += (s32) g_BgRoomFiles[roomnum].pos.y;
                bboxMin.z += (s32) g_BgRoomFiles[roomnum].pos.z;
                bboxMax.x += (s32) g_BgRoomFiles[roomnum].pos.x;
                bboxMax.y += (s32) g_BgRoomFiles[roomnum].pos.y;
                bboxMax.z += (s32) g_BgRoomFiles[roomnum].pos.z;

                if (bgTestRayIntersectsBbox(from, dir, (s32 *) (&bboxMin), (s32 *) (&bboxMax)))
                {
                    if (intersectRayTriangle((Vertex *)((s32)vtxbase - (0 - (idx[0] << 4))), (Vertex *)((s32)vtxbase - (0 - (idx[1] << 4))), (Vertex *)((s32)vtxbase - (0 - (idx[2] << 4))), (coord3d *) (((roomnum * 24) + ((s32) g_BgRoomFiles)) + 12), from, to, dir, &hitbuf))
                    {
                        tcmd = gdl;
                        dx = ((s32) hitbuf.hitpos.x) - ((s32) from->x);
                        dy = ((s32) hitbuf.hitpos.y) - ((s32) from->y);
                        dz = ((s32) hitbuf.hitpos.z) - ((s32) from->z);
                        dist = ((dx * dx) + (dy * dy)) + (dz * dz);
                        score = dist;
                        found = 1;

                        if (((*((u8 *) gdl)) != G_SETTIMG) && (dist || vtxbase || 1) && (((Gfx *) local.roominfo->ptr_expanded_mapping_info) < gdl))
                        {
                            do
                            {
                                tcmd--;
                                if ((*((u8 *) tcmd)) == G_SETTIMG)
                                {
                                    break;
                                }
                            }
                            while (((Gfx *) local.roominfo->ptr_expanded_mapping_info) < tcmd);
                        }

                        if (tcmd == ((Gfx *) local.roominfo->ptr_expanded_mapping_info))
                        {
                            texnum = -1;
                        }
                        else
                        {
                            temp.word = ((u32 *) tcmd)[1] - 8;
                            texnum = *((u16 *) (temp.word | 0x80000000));
                        }

                        if (lightIsLightTexture(texnum))
                        {
                            score = dist - 4;
                        }


                        // Texture 0x4FD is used for the light shafts that come through windows in Archives.
                        if ((score < bestScore) && (texnum != 0x4FD))
                        {
                            bestScore = score;
                            hitthing->hitpos.x = hitbuf.hitpos.x;
                            hitthing->hitpos.y = hitbuf.hitpos.y;
                            hitthing->hitpos.z = hitbuf.hitpos.z;
                            hitthing->normal.x = hitbuf.normal.x;
                            hitthing->normal.y = hitbuf.normal.y;
                            hitthing->normal.z = hitbuf.normal.z;
                            hitthing->vtx0 = &vtxbase[idx[0]];
                            hitthing->vtx1 = &vtxbase[idx[1]];
                            hitthing->vtx2 = &vtxbase[idx[2]];
                            hitthing->texturenum = texnum;
                            hitthing->tricmd = gdl;
                            hitthing->triNum = 0;
                        }
                    }
                }
            }
            else
            {
                if (op == ((s8) G_TRI4))
                {
                    // Keep this line as-is for matching.
                    s2 = 0; do
                    {
                        bboxMin2 = (BoundVec){0x7FFF, 0x7FFF, 0x7FFF};
                        bboxMax2 = (BoundVec){-0x8000, -0x8000, -0x8000};

                        if (s2 == 0)
                        {
                            idx2[0] = (((u32 *) gdl)[1] & 0xf) - vtxoff;
                            idx2[1] = (((u32) ((u8 *) gdl)[7]) >> 4) - vtxoff;
                            idx2[2] = (((u32 *) gdl)[0] & 0xf) - vtxoff;
                        }
                        else if (s2 == 1)
                        {
                            idx2[0] = (((u8 *) gdl)[6] & 0xf) - vtxoff;
                            idx2[1] = (((u32) ((u16 *) gdl)[3]) >> 12) - vtxoff;
                            idx2[2] = (((u32) ((u8 *) gdl)[3]) >> 4) - vtxoff;
                        }
                        else if (s2 == 2)
                        {
                            idx2[0] = (((u16 *) gdl)[2] & 0xf) - vtxoff;
                            idx2[1] = (((u32) ((u8 *) gdl)[5]) >> 4) - vtxoff;
                            idx2[2] = (((u8 *) gdl)[2] & 0xf) - vtxoff;
                        }
                        else
                        {
                            idx2[0] = (((u8 *) gdl)[4] & 0xf) - vtxoff;
                            idx2[1] = (((u32 *) gdl)[1] >> 28) - vtxoff;
                            idx2[2] = (((u32) ((u16 *) gdl)[1]) >> 12) - vtxoff;
                        }

                        i = 0;

                        do
                        {
                            v = vtxbase;
                            v += idx2[i];
                            if (v->coord.x < bboxMin2.x)
                            {
                                bboxMin2.x = v->coord.x;
                            }
                            if (bboxMax2.x < v->coord.x)
                            {
                                bboxMax2.x = v->coord.x;
                            }
                            if (v->coord.y < bboxMin2.y)
                            {
                                bboxMin2.y = v->coord.y;
                            }
                            if (bboxMax2.y < v->coord.y)
                            {
                                bboxMax2.y = v->coord.y;
                            }
                            if (v->coord.z < bboxMin2.z)
                            {
                                bboxMin2.z = v->coord.z;
                            }
                            if (bboxMax2.z < v->coord.z)
                            {
                                bboxMax2.z = v->coord.z;
                            }
                            i++;
                        } 
                        while (i < 3);
                        
                        bboxMin2.x += (s32) g_BgRoomFiles[roomnum].pos.x;
                        bboxMin2.y += (s32) g_BgRoomFiles[roomnum].pos.y;
                        bboxMin2.z += (s32) g_BgRoomFiles[roomnum].pos.z;
                        bboxMax2.x += (s32) g_BgRoomFiles[roomnum].pos.x;
                        bboxMax2.y += (s32) g_BgRoomFiles[roomnum].pos.y;
                        bboxMax2.z += (s32) g_BgRoomFiles[roomnum].pos.z;

                        if (bgTestRayIntersectsBbox(from, dir, (s32 *) (&bboxMin2), (s32 *) (&bboxMax2)))
                        {
                            if (intersectRayTriangle((Vertex *)((s32)vtxbase - (0 - (idx2[0] << 4))), (Vertex *)((s32)vtxbase - (0 - (idx2[1] << 4))), (Vertex *)((s32)vtxbase - (0 - (idx2[2] << 4))), (coord3d *) (((roomnum * 24) + ((s32) g_BgRoomFiles)) + 12), from, to, dir, &hitbuf))
                            {
                                tcmd = gdl;
                                dx = ((s32) hitbuf.hitpos.x) - ((s32) from->x);
                                dy = ((s32) hitbuf.hitpos.y) - ((s32) from->y);
                                dz = ((s32) hitbuf.hitpos.z) - ((s32) from->z);
                                dist = ((dx * dx) + (dy * dy)) + (dz * dz);
                                score = dist;
                                found = 1;

                                if (((*((u8 *) gdl)) != G_SETTIMG) && (dist || vtxbase || 1) && (((Gfx *) local.roominfo->ptr_expanded_mapping_info) < gdl))
                                {
                                    do
                                    {
                                        tcmd--;
                                        if ((*((u8 *) tcmd)) == G_SETTIMG)
                                        {
                                            break;
                                        }
                                    }
                                    while (((Gfx *) local.roominfo->ptr_expanded_mapping_info) < tcmd);
                                }
                                
                                if (tcmd == ((Gfx *) local.roominfo->ptr_expanded_mapping_info))
                                {
                                    texnum = -1;
                                }
                                else
                                {
                                    temp.word = ((u32 *) tcmd)[1] - 8;
                                    texnum = *((u16 *) (temp.word | 0x80000000));
                                }

                                if (lightIsLightTexture(texnum))
                                {
                                    score = dist - 4;
                                }


                                if ((score < bestScore) && (texnum != 0x4FD))
                                {
                                    bestScore = score;
                                    hitthing->hitpos.x = hitbuf.hitpos.x;
                                    hitthing->hitpos.y = hitbuf.hitpos.y;
                                    hitthing->hitpos.z = hitbuf.hitpos.z;
                                    hitthing->normal.x = hitbuf.normal.x;
                                    hitthing->normal.y = hitbuf.normal.y;
                                    hitthing->normal.z = hitbuf.normal.z;
                                    hitthing->vtx0 = &vtxbase[idx2[0]];
                                    hitthing->vtx1 = &vtxbase[idx2[1]];
                                    hitthing->vtx2 = &vtxbase[idx2[2]];
                                    hitthing->texturenum = texnum;
                                    hitthing->tricmd = gdl;
                                    hitthing->triNum = s2 + 1;
                                }
                            }
                        }
                        s2++;
                    }
                    while (s2 != 4);
                }
            }

            gdl++;
            op = *((s8 *) gdl);
        }
        while ((op != G_VTX) && (op != ((s8) G_ENDDL)));
    }

    return found;
}


/**
 * In order to make bgTestBulletHitBackground match, 4 bytes had to dropped from the regular HitThing struct.
 * There might be an alternate way of matching this that doesn't require the sub-struct.
 */
struct HitThingSub {
    coord3d hitpos;     // 0x00
    coord3d normal;     // 0x0c

    Vertex *vtx0;       // 0x18
    Vertex *vtx1;       // 0x1c
    Vertex *vtx2;       // 0x20

    Gfx *tricmd;        // 0x24 - display-list command associated with hit triangle

    s16 triNum;         // 0x28
    s16 texturenum;     // 0x2a
};


bool bgTestBulletHitBackground(coord3d *from, coord3d *to, s32 roomnum, struct HitThing *hit)
{
    RoomVtxBatchBounds *point;
    s16 numpoints;
    coord3d fromscaled;
    coord3d toscaled;
    coord3d dir;
    s32 i;
    struct HitThingSub tmp;
    f32 scale;
    s32 bestdist;
    s32 dist;
    bool found;
    s32 score;
    s32 dx;
    s32 dy;
    s32 dz;

    found = FALSE;
    bestdist = 0x7fffffff;

    scale = g_BgLevelScale;
    fromscaled.x = from->x * scale;
    fromscaled.y = from->y * scale;
    fromscaled.z = from->z * scale;
    toscaled.x = to->x * scale;
    toscaled.y = to->y * scale;
    toscaled.z = to->z * scale;
    dir.x = toscaled.x - fromscaled.x;
    dir.y = toscaled.y - fromscaled.y;
    dir.z = toscaled.z - fromscaled.z;
    point = g_BgRoomInfo[roomnum].vtx_batch_bounds;
    point = g_BgRoomInfo[roomnum].vtx_batch_bounds;

    if (point == NULL)
    {
        return FALSE;
    }

    numpoints = g_BgRoomInfo[roomnum].num_vtx_batch_bounds;

    for (i = 0; i < numpoints; i++) 
    {
        if (!bgTestRayIntersectsBbox(&fromscaled, &dir, &point[i].xmin, &point[i].xmax)) 
        {
            continue;
        }

        if (!bgTestRayIntersectionInRoom(&fromscaled, &toscaled, &dir, &point[i], roomnum, (struct HitThing *)&tmp)) 
        {
            continue;
        }

        dx = tmp.hitpos.x - fromscaled.x;
        dy = tmp.hitpos.y - fromscaled.y;
        dz = tmp.hitpos.z - fromscaled.z;
        dist = score = dx * dx + dy * dy + dz * dz;
        found = TRUE;

        if (lightIsLightTexture(tmp.texturenum)) 
        {
            score = dist - 4;
        }

        if (score < bestdist) 
        {
            bestdist = score;
            hit->hitpos.x = tmp.hitpos.x;
            hit->hitpos.y = tmp.hitpos.y;
            hit->hitpos.z = tmp.hitpos.z;
            hit->normal.x = tmp.normal.x;
            hit->normal.y = tmp.normal.y;
            hit->normal.z = tmp.normal.z;
            hit->vtx0 = tmp.vtx0;
            hit->vtx1 = tmp.vtx1;
            hit->vtx2 = tmp.vtx2;
            hit->texturenum = tmp.texturenum;
            hit->tricmd = tmp.tricmd;
            hit->triNum = tmp.triNum;
        }
    }

    if (found) 
    {
        point = (RoomVtxBatchBounds *)hit->tricmd;

        if (((u8 *)((Gfx*)point))[0] != G_SETTILE) 
        {
            while ((Gfx *)g_BgRoomInfo[roomnum].ptr_expanded_mapping_info < ((Gfx*)point)) 
            {
                point = (RoomVtxBatchBounds *)((Gfx *)point - 1);
                if (((u8 *)((Gfx*)point))[0] == G_SETTILE) 
                {
                    break;
                }
            }
        }

        if (((Gfx*)point) == g_BgRoomInfo[roomnum].ptr_expanded_mapping_info) 
        {
            hit->tileformat = -1;
            hit->tilesize = -1;
        } 
        else 
        {
            hit->tileformat = ((u32)((u8 *)((Gfx*)point))[1]) >> 5;
            hit->tilesize = (((Gfx*)point)->words.w0 << 11) >> 30;
        }
    }

    return found;
}


void bgResetPortalQueue(void)
{
    g_BgPortalQueueWriteIndex = 0;
    g_BgPortalQueueReadIndex = 0;
}


u8 bgIncrementRoomPortalVisitCount(s32 roomnum)
{
    s_room_info* room_info;
    u8 tmp;
    u8 count;
    u8 out;

    room_info = &g_BgRoomInfo[roomnum];
    count = room_info->portal_visit_count;
    out = count;

    if ((s32) count < 0xFF)
    {
        tmp = count + 1;
        room_info->portal_visit_count = tmp;
        out = tmp & 0xFF;
    }

    return out;
}


#ifdef VERSION_EU
void bgQueuePortalTraversal(s32 arg0, s32 arg1, s32 portalnum, f32 *arg4)
{
    bg_queued_portal_entry *entry;
    entry = &g_BgPortalQueue[g_BgPortalQueueWriteIndex];
    if (portalnum >= 2)
    {
        if (bgIncrementRoomPortalVisitCount((g_BgPortals[arg1].connectedRoom2 ^ g_BgPortals[arg1].connectedRoom1) ^ arg0) >= 9)
        {
            return;
        }
    }
    entry->arg0 = arg0;
    entry->roomnum = arg1;
    entry->portalnum = portalnum;
    entry->sp4[0] = arg4[0];
    entry->sp4[1] = arg4[1];
    entry->sp4[2] = arg4[2];
    entry->sp4[3] = arg4[3];
    g_BgPortalQueueWriteIndex++;
    if (g_BgPortalQueueWriteIndex == BG_PORTAL_QUEUE_LEN)
    {
        g_BgPortalQueueWriteIndex = 0;
    }
    if (g_BgPortalQueueWriteIndex == g_BgPortalQueueReadIndex)
    {
        g_BgPortalQueueWriteIndex--;
    }
}
#else
void bgQueuePortalTraversal(s32 arg0, s32 arg1, s32 portalnum, s32 depth, f32 *arg4)
{
    bg_queued_portal_entry *entry;
    entry = &g_BgPortalQueue[g_BgPortalQueueWriteIndex];
    if (depth >= 2)
    {
        if (bgIncrementRoomPortalVisitCount((g_BgPortals[portalnum].connectedRoom2 ^ g_BgPortals[portalnum].connectedRoom1) ^ arg1) >= 9)
        {
            return;
        }
    }
    entry->arg0 = arg0;
    entry->roomnum = arg1;
    entry->portalnum = portalnum;
    entry->arg3 = depth;
    entry->sp10[0] = arg4[0];
    entry->sp10[1] = arg4[1];
    entry->sp10[2] = arg4[2];
    entry->sp10[3] = arg4[3];
    g_BgPortalQueueWriteIndex++;
    if (g_BgPortalQueueWriteIndex == BG_PORTAL_QUEUE_LEN)
    {
        g_BgPortalQueueWriteIndex = 0;
    }
    if (g_BgPortalQueueWriteIndex == g_BgPortalQueueReadIndex)
    {
        g_BgPortalQueueWriteIndex--;
    }
}
#endif


#if defined(VERSION_EU)
bool bgProcessNextQueuedPortal(void)
{
    bg_queued_portal_entry *entry;

    if (g_BgPortalQueueReadIndex == g_BgPortalQueueWriteIndex) {
        return 0;
    }

    entry = &g_BgPortalQueue[g_BgPortalQueueReadIndex];

    bgTraversePortal(entry->arg0, entry->roomnum, entry->portalnum, entry->sp4);

    g_BgPortalQueueReadIndex++;

    if (g_BgPortalQueueReadIndex == BG_PORTAL_QUEUE_LEN) {
        g_BgPortalQueueReadIndex = 0;
    }

    return 1;
}
#else
bool bgProcessNextQueuedPortal(s32 *arg0)
{
    bg_queued_portal_entry *entry;
    s32 value;

    value = *arg0;

    if (g_BgPortalQueueReadIndex == g_BgPortalQueueWriteIndex) {
        return 0;
    }

    entry = &g_BgPortalQueue[g_BgPortalQueueReadIndex];

    value = bgTraversePortal(value, entry->roomnum, entry->portalnum, entry->arg3, entry->sp10);

    g_BgPortalQueueReadIndex++;

    if (g_BgPortalQueueReadIndex == BG_PORTAL_QUEUE_LEN) {
        g_BgPortalQueueReadIndex = 0;
    }

    *arg0 = value;

    return 1;
}
#endif


/**
 * EU version drops the value parameter and is type void unlike US/JP which returns s32.
 * 
 * bgPortalDescend
 */
#if defined(VERSION_EU)
void bgTraversePortal(s32 roomnum, s32 portalnum /*canonically p*/, s32 depth, bbox2d *parentbox)
{
    bbox2d screenbox;
    coord3d *playerpos;
    s32 otherroom;
    struct PortalMetric metric;
    f32 playermetric;
    f32 portalmetric;
    s32 i;
 
    g_BgPortalVisits++;
 
    if (depth >= 101)
    {
        return;
    }
 
    if (g_BgMaxPortalDepth < depth)
    {
        return;
    }
 
    if (depth >= 16)
    {
        return;
    }
 
 

    if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_DISABLED)
    {
        return;
    }
 

 
    playerpos = playerGetPos();
    bgGetPortalPlane(portalnum, &metric);
    playermetric = ((metric.normal.z * playerpos->z) + ((metric.normal.x * playerpos->x) + (metric.normal.y * playerpos->y))) * g_BgLevelScale;
    portalmetric = bgGetPortalThickness(portalnum);
 
    if (roomnum == g_BgPortals[portalnum].connectedRoom1)
    {
        otherroom = g_BgPortals[portalnum].connectedRoom2;
 
        if (metric.max <= (playermetric - portalmetric))
        {
            return;
        }
    }
    else
    {
        otherroom = g_BgPortals[portalnum].connectedRoom1;
 
        if ((playermetric + portalmetric) <= metric.min)
        {
            return;
        }
    }
 
    if (((metric.min - portalmetric) < playermetric) && (playermetric < (metric.max + portalmetric)))
    {
        screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
        screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
        screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
        screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
    }
    else
    {
        if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL)
        {
            if (!bgGetPortalScreenBox(portalnum, &screenbox))
            {
                return;
            }
 
            otherroom = (g_BgPortals[portalnum].connectedRoom1 ^ g_BgPortals[portalnum].connectedRoom2) ^ roomnum;
 
            if (!bgIsRoomOnScreen(otherroom, (struct rectbbox *) &screenbox))
            {
                return;
            }
 
            screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
            screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
            screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
            screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
        }
        else
        {
            if (!bgGetPortalScreenBox(portalnum, &screenbox))
            {
                return;
            }
 
            bgRectIntersect(&screenbox, parentbox);
            bgRectIntersect(&screenbox, &g_CurrentPlayer->screensize);
        }
 
        if ((screenbox.max.x <= screenbox.min.x) || (screenbox.max.y <= screenbox.min.y))
        {
            return;
        }
    }
 
 
    if ((screenbox.min.x < screenbox.max.x) && (screenbox.min.y < screenbox.max.y))
    {
        if (bgAddVisibleRoom(otherroom, depth, &screenbox, g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL))
        {
            return;
        }
    }
    else
    {
        return;
    }
 
    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if (i != portalnum)
        {
            if ((otherroom == g_BgPortals[i].connectedRoom1) || (otherroom == g_BgPortals[i].connectedRoom2))
            {
                bgQueuePortalTraversal(otherroom, i, depth + 1, &screenbox);
            }
        }
    }
}
#else
s32 bgTraversePortal(s32 value, s32 roomnum, s32 portalnum /*canonically p*/, s32 depth, bbox2d *parentbox)
{
    bbox2d screenbox;
    coord3d *playerpos;
    s32 otherroom;
    struct PortalMetric metric;
    f32 playermetric;
    f32 portalmetric;
    s32 i;
 
    g_BgPortalVisits++;
 
    if (depth >= 101)
    {
        return value;
    }
 
    if (g_BgMaxPortalDepth < depth)
    {
        return value;
    }
 
    if (depth >= 16)
    {
        return value;
    }
 
 
 
    if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_DISABLED)
    {
        return value;
    }
 

 
    playerpos = playerGetPos();
    bgGetPortalPlane(portalnum, &metric);
    playermetric = ((metric.normal.z * playerpos->z) + ((metric.normal.x * playerpos->x) + (metric.normal.y * playerpos->y))) * g_BgLevelScale;
    portalmetric = bgGetPortalThickness(portalnum);
 
    if (roomnum == g_BgPortals[portalnum].connectedRoom1)
    {
        otherroom = g_BgPortals[portalnum].connectedRoom2;
 
        if (metric.max <= (playermetric - portalmetric))
        {
            return value;
        }
    }
    else
    {
        otherroom = g_BgPortals[portalnum].connectedRoom1;
 
        if ((playermetric + portalmetric) <= metric.min)
        {
            return value;
        }
    }
 
    if (((metric.min - portalmetric) < playermetric) && (playermetric < (metric.max + portalmetric)))
    {
        screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
        screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
        screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
        screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
    }
    else
    {
        if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL)
        {
            if (!bgGetPortalScreenBox(portalnum, &screenbox))
            {
                return value;
            }
 
            otherroom = (g_BgPortals[portalnum].connectedRoom1 ^ g_BgPortals[portalnum].connectedRoom2) ^ roomnum;
 
            if (!bgIsRoomOnScreen(otherroom, (struct rectbbox *) &screenbox))
            {
                return value;
            }
 
            screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
            screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
            screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
            screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
        }
        else
        {
            if (!bgGetPortalScreenBox(portalnum, &screenbox))
            {
                return value;
            }
 
            bgRectIntersect(&screenbox, parentbox);
            bgRectIntersect(&screenbox, &g_CurrentPlayer->screensize);
        }
 
        if ((screenbox.max.x <= screenbox.min.x) || (screenbox.max.y <= screenbox.min.y))
        {
            return value;
        }
    }
 
 
    if ((screenbox.min.x < screenbox.max.x) && (screenbox.min.y < screenbox.max.y))
    {
        if (bgAddVisibleRoom(otherroom, depth, &screenbox, g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL))
        {
            return value;
        }
    }
    else
    {
        return value;
    }
 
    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if (i != portalnum)
        {
            if ((otherroom == g_BgPortals[i].connectedRoom1) || (otherroom == g_BgPortals[i].connectedRoom2))
            {
                bgQueuePortalTraversal(value, otherroom, i, depth + 1, &screenbox);
            }
        }
    }
 
    return value;
}
#endif


/**
 * Add to stack. Push and then increment position. Will wrap on overflow.
 */
s32 bgStackPush(s32 arg0)
{
    g_BgStack[g_BgStackCount] = arg0;
    g_BgStackCount = (s32) (g_BgStackCount + 1) % BG_STACK_SIZE;
    return arg0;
}


/**
 * Pop from stack. Decrement position and retrieve from there. Wraps on underflow.
 */
s32 bgStackPop(void)
{
    s32 val;

    // ok, who thought this was a good idea
    val = g_BgStack[g_BgStackCount = (g_BgStackCount + (BG_STACK_SIZE-1)) % BG_STACK_SIZE];
    return val;
}


s32 bgStackGetNthValueFromEnd(s32 n) 
{
    return g_BgStack[((g_BgStackCount - n) + (BG_STACK_SIZE - 1)) % BG_STACK_SIZE];
}

GlobalVisCommand *bgParseVisCommands(GlobalVisCommand *cmd, s32 execute)
{
    static BgVisState portalBox;

    GlobalVisCommand  *ret;
    s32                value;
    bbox2d             sp68;
    bbox2d             sp58;
    u8                 preload_ok = TRUE;

    portalBox.stopExec = FALSE;

    if (cmd == NULL)
    {
        return cmd;
    }

    while (TRUE)
    {
        switch (cmd->type)
        {
            case VISOP_END:
                return cmd;

            case VISOP_PUSH:
                if (execute)
                {
                    bgStackPush(cmd->arg);
                }

                cmd += cmd->length;
                break;

            case VISOP_POP:
                if (execute)
                {
                    bgStackPop();
                }

                cmd += cmd->length;
                break;

            case VISOP_AND:
                if (execute)
                {
                    value = bgStackPop();
                    bgStackPush(bgStackPop() & value);
                }

                cmd += cmd->length;
                break;

            case VISOP_OR:
                if (execute)
                {
                    value = bgStackPop();
                    bgStackPush(bgStackPop() | value);
                }

                cmd += cmd->length;
                break;

            case VISOP_NOT:
                if (execute)
                {
                    bgStackPush(bgStackPop() == 0);
                }

                cmd += cmd->length;
                break;

            case VISOP_XOR:
                if (execute)
                {
                    value = bgStackPop();
                    bgStackPush(bgStackPop() ^ value);
                }

                cmd += cmd->length;
                break;

            case VISOP_PUSH_IF_ROOM_IN_RANGE:
                if (execute)
                {
                    s32 curroom;

                    curroom = g_BgCurrentRoom;

                    bgStackPush(cmd[1].arg <= curroom && curroom <= cmd[2].arg);
                }

                cmd += cmd->length;
                break;

            case VISOP_FORCE_VISIBLE:
                if (execute)
                {
                    portalBox.box.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
                    portalBox.box.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
                    portalBox.box.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
                    portalBox.box.f[1][1] = g_CurrentPlayer->screensize.f[1][1];

                    current_visibility = FALSE;
                }

                cmd += cmd->length;
                break;

            case VISOP_MATCH_PORTAL_VIS:
                if (execute)
                {
                    if (bgGetPortalScreenBox(cmd[1].arg, &portalBox.box) == 0)
                    {
                        current_visibility = TRUE;
                    }
                    else if (bgRectIntersect(&portalBox.box, &g_CurrentPlayer->screensize) == 0)
                    {
                        current_visibility = TRUE;
                    }
                    else
                    {
                        current_visibility = FALSE;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_VISIBLE_IF_SEEN_THROUGH_PORTAL:
                if (execute)
                {
                    if (bgGetPortalScreenBox(cmd[1].arg, &sp68) && bgRectIntersect(&sp68, &g_CurrentPlayer->screensize))
                    {
                        if (current_visibility)
                        {
                            bbox2dCopy(&portalBox.box, &sp68);
                            current_visibility = FALSE;
                        }
                        else
                        {
                            bgRectOutersect(&portalBox.box, &sp68);
                        }
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_NOT_VISIBLE_IF_SEEN_THROUGH_PORTAL:
                if (execute && !current_visibility)
                {
                    if (!bgGetPortalScreenBox(cmd[1].arg, &sp58))
                    {
                        current_visibility = TRUE;
                    }
                    else if (!bgRectIntersect(&sp58, &g_CurrentPlayer->screensize))
                    {
                        current_visibility = TRUE;
                    }
                    else if (!bgRectIntersect(&portalBox.box, &sp58))
                    {
                        current_visibility = TRUE;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_ADD_VISIBLE_ROOM:
                if (execute && !current_visibility)
                {
                    if (bgIsRoomOnScreen(cmd[1].arg, (struct rectbbox *)&portalBox.box))
                    {
                        bgAddVisibleRoom(cmd[1].arg, 0, &portalBox.box, 0);

                        list_visible_rooms_in_cur_global_vis_packet[num_visible_rooms_in_cur_global_vis_packet] = cmd[1].arg;

                        num_visible_rooms_in_cur_global_vis_packet++;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_DISABLE_ROOM:
                if (execute)
                {
                    g_BgRoomInfo[cmd[1].arg].room_loaded_mask = TRUE;
                }

                cmd += cmd->length;
                break;

            case VISOP_DISABLE_ROOM_RANGE:
                if (execute)
                {
                    s32 room;

                    room = cmd[1].arg;

                    while (room <= cmd[2].arg)
                    {
                        g_BgRoomInfo[room].room_loaded_mask = TRUE;
                        room++;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_PRELOAD_ROOM:
                if (execute && preload_ok)
                {
                    preload_ok = !bgCheckIfRoomModelNeedsLoad(cmd[1].arg);
                }

                cmd += cmd->length;
                break;

            case VISOP_PRELOAD_ROOM_RANGE:
                if (execute)
                {
                    s32 room;

                    room = cmd[1].arg;

                    while (room <= cmd[2].arg)
                    {
                        if (preload_ok)
                        {
                            preload_ok = !bgCheckIfRoomModelNeedsLoad(room);
                        }

                        room++;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_REMOVE_VIS:
                if (execute)
                {
                    current_visibility = TRUE;
                }

                cmd += cmd->length;
                break;

            case VISOP_IF_STATEMENT:
                ret = bgParseVisCommands(cmd + cmd->length, execute);
                ret += ret->length;
                cmd = ret;
                break;

            case VISOP_ENDIF_CONTINUE_EXEC:
                cmd += cmd->length;
                portalBox.stopExec = FALSE;
                return cmd;

            case VISOP_DONT_EXEC_COMMANDS_EVEN_ON_RETURN:
                cmd += cmd->length;

                if (execute)
                {
                    execute                       = FALSE;
                    portalBox.stopExec = TRUE;
                }
                else
                {
                    execute = FALSE;
                }

                break;

            case VISOP_IF_STATEMENT_PULL_FROM_STACK:
                value = bgStackPop();

                ret = bgParseVisCommands(cmd + cmd->length, value & execute);
                cmd = ret;

                if (!portalBox.stopExec)
                {
                    continue;
                }

                execute = FALSE;
                break;

            case VISOP_TOGGLE_EXEC_VS_READONLY:
                execute ^= TRUE;
                cmd += cmd->length;
                break;

            case VISOP_ENDIF:
                cmd += cmd->length;
                return cmd;

            default:
                return cmd;
        }
    }
}

PortalCache g_BgPortalCache[PORTMAX];

#define RS_STOP 0


// Something about portals. Void* are structs.
void *bgRunVisCommands(s32 *pc) 
{

    current_visibility = 0;
    if (!pc)
    {
        return pc;
    }

    bgStackGetNthValueFromEnd(0);

    return bgParseVisCommands(pc, 1);
}


void bgDetermineVisibleRooms(void) 
{
    f32 screenbounds[4];
    s32 var_s0;
    s32 temp_a0;
#if defined(VERSION_US)
    s32 sp44;
#endif
    s32 temp_v1;
    s32 i;

    bgUpdateCurrentPlayerScreenMinMax();

    screenbounds[0] = g_CurrentPlayer->screensize.min.x;
    screenbounds[1] = g_CurrentPlayer->screensize.min.y;
    screenbounds[2] = g_CurrentPlayer->screensize.max.x;
    screenbounds[3] = g_CurrentPlayer->screensize.max.y;

    bgResetPortalVisitCounts();

    for (i = 0; i < MAXROOMCOUNT; i++) 
    {
        g_BgRoomInfo[i].room_rendered = 0;
        g_BgRoomInfo[i].room_neighbor_to_rendered = 0;
        g_BgRoomInfo[i].room_loaded_mask = 0;
    }

    g_BgRoomLoadSlot = (s32) (g_BgRoomLoadSlot + 1) % 10;

#if !defined(VERSION_US)
    *(s32 *) &g_BgRoomsDrawn[120] = 0;
#endif
    g_BgPortalVisits = 0;

    bgResetPortalQueue();
    bgResetPortalTable();
    bgRunVisCommands(g_BgVisCommands);


    /**
     * If the level is Cradle, or has no portals, skip the portal occlusion culling algorithm. Just add every room in the player's
     * screen bounds to the list of rooms to draw.
     */
    if ((levelentry_index == LEVEL_INDEX_CRAD) || (g_BgPortals->offset_portal == NULL)) 
    {
        if (levelentry_index == LEVEL_INDEX_CRAD) 
        {
            bgAddVisibleRoom(9, 0, &g_CurrentPlayer->screensize, 1);
        }

        for (var_s0 = 1; var_s0 < g_MaxNumRooms; var_s0++) 
        {
            if (bgIsRoomOnScreen(var_s0, &g_CurrentPlayer->screensize) != 0) 
            {
                bgAddVisibleRoom(var_s0, 0, &g_CurrentPlayer->screensize, 1);
            }
        }
    } 
    else 
    {
        // This can never execute since the above branch is taken if the level is Cradle.
        if (levelentry_index == LEVEL_INDEX_CRAD) 
        {
            bgAddVisibleRoom(9, 0, &g_CurrentPlayer->screensize, 1);
        }

        bgAddVisibleRoom(g_BgCurrentRoom, 0, &g_CurrentPlayer->screensize, 1);

        for (var_s0 = 0; g_BgPortals[var_s0].offset_portal != NULL; var_s0++) 
        {
            if ((g_BgCurrentRoom == g_BgPortals[var_s0].connectedRoom1) || (g_BgCurrentRoom == g_BgPortals[var_s0].connectedRoom2)) 
            {
#if defined(VERSION_US)
                bgQueuePortalTraversal(0, g_BgCurrentRoom, var_s0, 1, screenbounds);
#else
                bgQueuePortalTraversal(g_BgCurrentRoom, var_s0, 1, screenbounds);
#endif
            }
        }

#if defined(VERSION_US)
        sp44 = 0;

        while (bgProcessNextQueuedPortal(&sp44) != 0) 
        {
            // empty
        }

#else
        while (bgProcessNextQueuedPortal() != 0) 
        {
            // empty
        }
#endif
    }

    for (var_s0 = 0; g_BgPortals[var_s0].offset_portal != NULL; var_s0++) 
    {
        temp_v1 = g_BgPortals[var_s0].connectedRoom1;
        temp_a0 = g_BgPortals[var_s0].connectedRoom2;

        if ((g_BgRoomInfo[temp_v1].room_rendered != 0) && (g_BgRoomInfo[temp_a0].room_rendered == 0)) 
        {
            g_BgRoomInfo[temp_a0].room_neighbor_to_rendered = 1;
        } 
        else if ((g_BgRoomInfo[temp_a0].room_rendered != 0) && (g_BgRoomInfo[temp_v1].room_rendered == 0)) 
        {
            g_BgRoomInfo[temp_v1].room_neighbor_to_rendered = 1;
        }
    }
}


Gfx *bgScissorAndRenderRooms(Gfx *arg0)
{
    s32 i;
    if (levelentry_index == LEVEL_INDEX_DAM)
    {
        for (i=0; i<g_BgNumberOfRoomsDrawn; i++)
        {
            // The lake in dam is a single giant room, id 0x23
            if (g_BgRoomsDrawn[i].roomid == 0x23)
            {
                // speculation in discord: unk1 is probably draw order or similar,
                // this is a hack to draw the lake first.
                g_BgRoomsDrawn[i].depth = 0;
                break;
            }
        }
    }

    return bgScissorCurrentPlayerViewDefault(bgRenderVisibleRooms(arg0));
}


// Copies visible rooms to a list
s32 bgCopyVisibleRoomsToList(s32 *rooms, s32 max)
{
    s32 i;

    for (i = 0; (i < num_visible_rooms_in_cur_global_vis_packet) && (i < max); i++) {
        rooms[i] = list_visible_rooms_in_cur_global_vis_packet[i];
    }

    return i;
}


/**
 * Create a list of rooms connected to roomIndex.
 * @param roomIndex    Room to query.
 * @param list         Output buffer for connected room indices.
 * @param max          Max number of entries to write.
 * @return             Number of rooms written to the list.
 */
s32 bgGetConnectedRooms(s32 roomIndex, s32* list, s32 max)
{
    s32 len = 0;
    s32 i;
    s32 p;
    s32 connectedRoom1;
    s32 connectedRoom2;

    for (p = 0; g_BgPortals[p].offset_portal != NULL; p++) {
        connectedRoom1 = g_BgPortals[p].connectedRoom1;
        connectedRoom2 = g_BgPortals[p].connectedRoom2;

        if (connectedRoom1 == roomIndex) {
            connectedRoom1 = connectedRoom2;
            connectedRoom2 = roomIndex;
        }

        if (connectedRoom2 == roomIndex) {
            for (i = 0; i < len; i++) {
                if (list[i] == connectedRoom1) {
                    goto end;
                }
            }

            list[len] = connectedRoom1;
            len++;

            if (len >= max) {
                return len;
            }
end:
        }
    }

    return len;
}


// Scan all portals to see if these rooms are connected
//
// Room data doesn't contain a list of its portals, so it goes through
// the whole list of portals which seems naive and inefficient.
//
bool bgRoomsSharePortal(s32 room1, s32 room2) 
{
    s32 i;

    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        s32 v0 = g_BgPortals[i].connectedRoom1;
        s32 v1 = g_BgPortals[i].connectedRoom2;

        if (v0 == room1 && v1 == room2)
        {
            return TRUE;
        }

        if (v1 == room1 && v0 == room2)
        {
            return TRUE;
        }
    }
    return FALSE;
}


void bgUpdateCurrentPlayerScreenMinMax(void)
{
    f32 fx;
    f32 fwidth;
    f32 fy;
    f32 fheight;

    fx = (f32) bgViewRelated[0];
    fy = (f32) bgViewRelated[1];
    fwidth = (f32) viGetX() + (f32) bgViewRelated[2];
    fheight = (f32) viGetY() + (f32) bgViewRelated[3];

    g_CurrentPlayer->screensize.min.x = (f32) viGetViewLeft();

    if (g_CurrentPlayer->screensize.min.x < fx)
    {
        g_CurrentPlayer->screensize.min.x = fx;
    }

    if (fwidth < g_CurrentPlayer->screensize.min.x)
    {
        g_CurrentPlayer->screensize.min.x = fwidth;
    }

    g_CurrentPlayer->screensize.min.y = (f32) viGetViewTop();

    if (g_CurrentPlayer->screensize.min.y < fy)
    {
        g_CurrentPlayer->screensize.min.y = fy;
    }

    if (fheight < g_CurrentPlayer->screensize.min.y)
    {
        g_CurrentPlayer->screensize.min.y = fheight;
    }

    g_CurrentPlayer->screensize.max.x = (f32) (viGetViewLeft() + viGetViewWidth());

    if (g_CurrentPlayer->screensize.max.x < fx)
    {
        g_CurrentPlayer->screensize.max.x = fx;
    }

    if (fwidth < g_CurrentPlayer->screensize.max.x)
    {
        g_CurrentPlayer->screensize.max.x = fwidth;
    }

    g_CurrentPlayer->screensize.max.y = (f32) (viGetViewTop() + viGetViewHeight());

    if (g_CurrentPlayer->screensize.max.y < fy)
    {
        g_CurrentPlayer->screensize.max.y = fy;
    }

    if (fheight < g_CurrentPlayer->screensize.max.y)
    {
        g_CurrentPlayer->screensize.max.y = fheight;
    }
}


void bgGetRoomCenter(s32 roomnum, coord3d *dst)
{
    s32 i;
    s_room_info *room = &g_BgRoomInfo[roomnum];

    for (i = 0; i < 3; i++) {
        dst->f[i] = (room->minbounds.f[i] + room->maxbounds.f[i]) * 0.5f;
    }
}


void bgRoomCalcBB(s32 room) // canonical name
{
    bg_room_data *roomdata;
    Vtx *vertices = (Vtx *) &g_StanRoomBounds[0];
    s32 j = 0;
    StanRoomBounds limits;
    u8 wasloaded;

    roomdata = (bg_room_data *) ((s32) g_BgRoomFiles + room * 24);

    if (roomdata->pPointTableBin == NULL)
    {
        if ((room < g_StanNumRooms) && ((s32) g_StanRoomFirstTile[room] != ((StanRoomBounds *) vertices)->min[j] % 1))
        {
            for (j = 0; j < 3; j++)
            {
                g_BgRoomInfo[room].minbounds.f[j] = g_StanRoomBounds[room].min[j];

                g_BgRoomInfo[room].maxbounds.f[j] = g_StanRoomBounds[room].max[j];

                g_BgRoomFiles[room].pos.f[j] = (g_StanRoomBounds[room].min[j] + g_StanRoomBounds[room].max[j]) / 2;
            }
        }

        return;
    }

    wasloaded = g_BgRoomInfo[room].model_bin_loaded;

    if (!wasloaded)
    {
        bgLoadRoomModelData(room);
    }

    vertices = g_BgRoomInfo[room].vertices;
    roomdata = (bg_room_data *) ((s32) g_BgRoomFiles + room * 24);

    limits.minX = 0x7fff;
    limits.minY = 0x7fff;
    limits.minZ = 0x7fff;
    limits.maxX = -0x7fff;
    limits.maxY = -0x7fff;
    limits.maxZ = -0x7fff;

    for (; vertices < (Vtx *) ((s32) g_BgRoomInfo[room].vertices + g_BgRoomInfo[room].usize_point_index_binary); vertices++)
    {
        for (j = 0; j < 3; j++)
        {
            if (((s16 *) vertices)[j] < limits.min[j])
            {
                limits.min[j] = ((s16 *) vertices)[j];
            }

            if (limits.max[j] < ((s16 *) vertices)[j])
            {
                limits.max[j] = ((s16 *) vertices)[j];
            }
        }
    }

    g_BgRoomInfo[room].minbounds.x = roomdata->pos.x + limits.minX;
    g_BgRoomInfo[room].minbounds.y = roomdata->pos.y + limits.minY;
    g_BgRoomInfo[room].minbounds.z = roomdata->pos.z + limits.minZ;
    g_BgRoomInfo[room].maxbounds.x = roomdata->pos.x + limits.maxX;
    g_BgRoomInfo[room].maxbounds.y = roomdata->pos.y + limits.maxY;
    g_BgRoomInfo[room].maxbounds.z = roomdata->pos.z + limits.maxZ;

    if (wasloaded == 0)
    {
        bgUnloadRoom(room);
    }
}


void bgGrowRoomBoundsToPortals(s32 roomID)
{
    s32 numupdated = 0;
    s32 i;
    s32 j;
    s32 k;
    f32 value;

    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if ((roomID == g_BgPortals[i].connectedRoom1) || (roomID == g_BgPortals[i].connectedRoom2))
        {
            for (j = 0; j < g_BgPortals[i].offset_portal->numPoints; j++)
            {
                for (k = 0; k < 3; k++)
                {
                    value = (&g_BgPortals[i].offset_portal->point)[j].f[k];

                    if (value < g_BgRoomInfo[roomID].minbounds.f[k])
                    {
                        g_BgRoomInfo[roomID].minbounds.f[k] = value;
                        numupdated++;
                    }

                    if (g_BgRoomInfo[roomID].maxbounds.f[k] < value)
                    {
                        g_BgRoomInfo[roomID].maxbounds.f[k] = value;
                        numupdated++;
                    }
                }
            }
        }
    }

}


// This assert belongs somewhere in the function
//#ifdef
//assert(levelportals[p].p->n>=3)
//#endif
void bgGetPortalPlane(s32 portalnum, f32 *out)
{
    f32 sp6c[3];
    f32 sp60[3];
    bg_portal_entry *portal;
    f32 min;
    f32 max;
    f32 dot;
    s32 i;

    for (i = 0; i < 3; i++)
    {
        sp6c[i] = (&g_BgPortals[portalnum].offset_portal->point)[0].f[i] - (&g_BgPortals[portalnum].offset_portal->point)[1].f[i];
    }

    for (i = 0; i < 3; i++)
    {
        sp60[i] = (&g_BgPortals[portalnum].offset_portal->point)[2].f[i] - (&g_BgPortals[portalnum].offset_portal->point)[1].f[i];
    }

    out[0] = (sp6c[1] * sp60[2]) - (sp6c[2] * sp60[1]);
    out[1] = (sp6c[2] * sp60[0]) - (sp6c[0] * sp60[2]);
    out[2] = (sp6c[0] * sp60[1]) - (sp6c[1] * sp60[0]);

    dot = sqrtf(((out[0] * out[0]) + (out[1] * out[1])) + (out[2] * out[2]));

    if (dot != 0.0f)
    {
        dot = 1.0f / dot;
    }

    out[0] *= dot;
    out[1] *= dot;
    out[2] *= dot;

    portal = g_BgPortals[portalnum].offset_portal;

    min = 3.4028235e38f;
    max = -3.4028235e38f;

    for (i = 0; i < portal->numPoints; i++)
    {
        min = (((((&portal->point)[i].f[0] * out[0]) + ((&portal->point)[i].f[1] * out[1])) + ((&portal->point)[i].f[2] * out[2])) < min) ? ((((&portal->point)[i].f[0] * out[0]) + ((&portal->point)[i].f[1] * out[1])) + ((&portal->point)[i].f[2] * out[2])) : (min);
        max = (((((&portal->point)[i].f[0] * out[0]) + ((&portal->point)[i].f[1] * out[1])) + ((&portal->point)[i].f[2] * out[2])) > max) ? ((((&portal->point)[i].f[0] * out[0]) + ((&portal->point)[i].f[1] * out[1])) + ((&portal->point)[i].f[2] * out[2])) : (max);
    }

    out[3] = min;
    out[4] = max;

}


/**
 * Unknown, makes use of bgGetPortalPlane.
 */
s32 bgPortalIsVertical(s32 arg0)
{
    struct PortalMetric metric;

    bgGetPortalPlane(arg0, &metric);

    if (((metric.normal.f[0] * metric.normal.f[0]) + (metric.normal.f[2] * metric.normal.f[2])) < 0.999f)
    {
        return 0;
    }

    return 1;
}


f32 bgGetPortalThickness(s32 portalnum)
{
    s32 value;
    s32 shift;
    f32 result;

    value = g_BgPortals[portalnum].controlbytes2;
    shift = (value >> 4) & 0xf;
    result = (value & 0xf) * 0.25f;

    while (shift != 0) {
        result += result;
        shift--;
    }

    return result;
}


/** Swaps the two rooms a portal connects. */
static void bgSwapConnectedRooms(s32 index)
{
    u8 t = g_BgPortals[index].connectedRoom1;
    g_BgPortals[index].connectedRoom1 = g_BgPortals[index].connectedRoom2;
    g_BgPortals[index].connectedRoom2 = t;
}

void bgOrderPortal(s32 portalnum) // canonical name
{
    coord3d room1centre;
    coord3d room2centre;
    struct PortalMetric metric;
    f32 tmp;
    f32 tmp2;
    s32 roomnum2;
    s32 swapped;
    s32 roomnum1;
    f32 tmp1;

    roomnum1 = g_BgPortals[portalnum].connectedRoom1;
    roomnum2 = g_BgPortals[portalnum].connectedRoom2;

    bgGetRoomCenter(roomnum1, &room1centre);
    bgGetRoomCenter(roomnum2, &room2centre);

    bgGetPortalPlane(portalnum, &metric);

    if (metric.max - metric.min < 0.1f) {
    }

    tmp2 = metric.normal.f[2];
    tmp1 = metric.normal.f[0] * room1centre.f[0] + metric.normal.f[1] * room1centre.f[1] + tmp2 * room1centre.f[2];

    swapped = 0;

    if (tmp1 > metric.max) {
        swapped = 1;

        bgSwapConnectedRooms(portalnum);

        metric.normal.x = -metric.normal.x;
        metric.normal.y = -metric.normal.y;
        metric.normal.z = -metric.normal.z;

        tmp = metric.min;
        metric.min = -metric.max;
        metric.max = -tmp;
    }

	tmp2 = metric.normal.f[0] * room2centre.f[0] + metric.normal.f[1] * room2centre.f[1] + metric.normal.f[2] * room2centre.f[2];

	if (tmp2 <= metric.min) {
        if (swapped) {
            bgSwapConnectedRooms(portalnum);
        }
    }

}


s32 bgGetPortalBetweenRooms(s32 room1, s32 room2, coord3d *arg2, coord3d *arg3)
{
    s32 bFoundPortal = FALSE;
    s32 i;
    s32 portalIndex = -1;

        #define osSyncPrintf(...)

    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if (((g_BgPortals[i].connectedRoom1 == room1) && (g_BgPortals[i].connectedRoom2 == room2)) ||
            ((g_BgPortals[i].connectedRoom1 == room2) && (g_BgPortals[i].connectedRoom2 == room1)))
        {
            bFoundPortal = TRUE;
            if (bgSegmentCrossesPortal(i, arg2, arg3) != 0)
            {
                if (portalIndex >= 0) osSyncPrintf("bg: bgGetPortalBetweenRooms(): Multiple portals join room \'%s\' and \'%s\'\ n", bgDebPrintROOMID(room1), bgDebPrintROOMID(room2));
                portalIndex = i;
            }
        }
    }

    if (portalIndex == -1 && !bFoundPortal) osSyncPrintf("bg: bgGetPortalBetweenRooms(): No portal joins room \'%s\' and \'%s\'\n", bgDebPrintROOMID(room1), bgDebPrintROOMID(room2));

    return portalIndex;
    #undef osSyncPrintf
}


/**
 * Toggles control bytes 1 lowest bit, based on toggle parameter.
 *
 * @param index: index into data portals.
 * @param toggle: When zero, sets lowest bit. Otherwise, clears lowest bit.
 */
void bgToggleDataPortalsContrlBytes1Bit1(s32 portal, s32 toggle)
{
    g_BgPortals[portal].controlbytes1 = (g_BgPortals[portal].controlbytes1 | 1) ^ (toggle != 0);
}


//bg_consider_window82397B18
s32 bgFindPortalOnSegment(coord3d *arg0, coord3d *arg1)
{
    s32 bestportalnum = -1;
    s32 count = 0;
    f32 bestthing = MAXFLOAT;
    f32 thisthing;
    s32 i;

    for (i = 0; g_BgPortals[i].offset_portal; i++)
    {
        if (bgSegmentCrossesPortal(i, arg0, arg1) != 0)
        {
            thisthing = g_BgSegmentPortalDistance;

            if (thisthing < 0)
            {
                thisthing = -thisthing;
            }

            if (thisthing < bestthing)
            {
                if (count)
                {
                }
                bestportalnum = i;
                bestthing = thisthing;
                count++;
            }
        }
    }

    return bestportalnum;
}


s32 bgSegmentCrossesPortal(s32 portalnum, coord3d *pos1, coord3d *pos2)
{
    f32 value1;
    f32 value2;
    coord3d diff;
    s32 next;
    f32 tmp;
    coord3d edge;
    struct PortalMetric metric;
    f32 plane_d[1];
    coord3d cross;
    s32 i;
    u8 seenA;
    u8 seenB;

    seenA = 0;
    seenB = 0;

    bgGetPortalPlane(portalnum, &metric);

    diff.f[0] = pos2->f[0] - pos1->f[0];
    diff.f[1] = pos2->f[1] - pos1->f[1];
    diff.f[2] = pos2->f[2] - pos1->f[2];

    value1 = (pos1->f[0] * metric.normal.f[0] + pos1->f[1] * metric.normal.f[1] + pos1->f[2] * metric.normal.f[2]) * g_BgLevelScale;
    value2 = (pos2->f[0] * metric.normal.f[0] + pos2->f[1] * metric.normal.f[1] + pos2->f[2] * metric.normal.f[2]) * g_BgLevelScale;

    // @bug? Using min everywhere. In PD, the second half of this statement uses max.
    if ((value1 < metric.min && value2 < metric.min) || (value1 > metric.min && value2 > metric.min))
    {
        return 0;
    }

    g_BgSegmentPortalDistance = (value1 + value2) * 0.5f - metric.min;

    for (i = 0; i < g_BgPortals[portalnum].offset_portal->numPoints; i++)
    {
        next = (i + 1) % g_BgPortals[portalnum].offset_portal->numPoints;

        edge.f[0] = (&g_BgPortals[portalnum].offset_portal->point)[next].f[0] - (&g_BgPortals[portalnum].offset_portal->point)[i].f[0];
        edge.f[1] = (&g_BgPortals[portalnum].offset_portal->point)[next].f[1] - (&g_BgPortals[portalnum].offset_portal->point)[i].f[1];
        edge.f[2] = (&g_BgPortals[portalnum].offset_portal->point)[next].f[2] - (&g_BgPortals[portalnum].offset_portal->point)[i].f[2];

        cross.f[0] = edge.f[1] * diff.f[2] - edge.f[2] * diff.f[1];
        cross.f[1] = edge.f[2] * diff.f[0] - edge.f[0] * diff.f[2];
        cross.f[2] = edge.f[0] * diff.f[1] - edge.f[1] * diff.f[0];

        tmp = cross.f[0] * cross.f[0] + cross.f[1] * cross.f[1] + cross.f[2] * cross.f[2];

        if (tmp == 0.0f)
        {
            return 0;
        }

        plane_d[0] = cross.f[0] * (&g_BgPortals[portalnum].offset_portal->point)[i].f[0]
            + cross.f[1] * (&g_BgPortals[portalnum].offset_portal->point)[i].f[1]
            + cross.f[2] * (&g_BgPortals[portalnum].offset_portal->point)[i].f[2];

        tmp = (cross.f[0] * pos1->f[0] + cross.f[1] * pos1->f[1] + cross.f[2] * pos1->f[2]) * g_BgLevelScale;

        if (tmp < plane_d[0])
        {
            if (seenB)
            {
                return 0;
            }

            seenA = 1;
        }
        else
        {
            if (seenA)
            {
                return 0;
            }

            seenB = 1;
        }
    }

    return value1 < metric.min ? 1 : 2;
}


bool bgIsBboxOverlapping(coord3d *portalbbmin, coord3d *portalbbmax, coord3d *propbbmin, coord3d *propbbmax)
{
    s32 i;

    for (i = 0; i < 3; i++)
    {
        if (propbbmin->f[i] > portalbbmax->f[i] || propbbmax->f[i] < portalbbmin->f[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}


void bgFindRoomsInBbox(coord3d *bbmin, coord3d *bbmax, s32 *room_list, s32 *count, s32 max_count)
{
    bg_portal_entry *portal_pts;
    f32 v;
    s32 cur_room;
    coord3d scaled_bbmin;
    coord3d scaled_bbmax;
    s32 cur_count;
    s32 i;
    s32 j;
    s32 k;
    s32 saved_count;
    coord3d portal_min;
    coord3d portal_max;
    s32 portal_idx;
    s32 *p;
    s32 other_room;
    
    cur_count = *count;
    i = 0;
    scaled_bbmin.x = bbmin->x * g_BgLevelScale;
    scaled_bbmin.y = bbmin->y * g_BgLevelScale;
    scaled_bbmin.z = bbmin->z * g_BgLevelScale;
    scaled_bbmax.x = bbmax->x * g_BgLevelScale;
    scaled_bbmax.y = bbmax->y * g_BgLevelScale;
    scaled_bbmax.z = bbmax->z * g_BgLevelScale;
    saved_count = cur_count;
    
    while (1)
    {
        if (i < cur_count)
        {
            p = (s32 *)((u8 *)room_list + (i << 2)); do {
            cur_room = *p;
            portal_idx = 0;
 
            if (g_BgPortals[0].offset_portal != ((void *) 0))
            {
                do
                {
                    if ((g_BgPortals[portal_idx].controlbytes1 & 1) || ((cur_room != g_BgPortals[portal_idx].connectedRoom1) && (cur_room != g_BgPortals[portal_idx].connectedRoom2)))
                    {
                        goto next_portal;
                    }
                    
                    portal_min = (coord3d){MAXFLOAT, MAXFLOAT, MAXFLOAT};
                    portal_max = (coord3d){-MAXFLOAT, -MAXFLOAT, -MAXFLOAT};
                    portal_pts = g_BgPortals[portal_idx].offset_portal;
                    
                    for (j = 0; j < portal_pts->numPoints; j++)
                    {
                        for (k = 0; k < 3; k++)
                        {
                            v = (&portal_pts->point)[j].f[k];
                            
                            if (v < portal_min.f[k])
                            {
                                portal_min.f[k] = v;
                            }
                            
                            if (portal_max.f[k] < v)
                            {
                                portal_max.f[k] = v;
                            }
                            
                            portal_pts = g_BgPortals[portal_idx].offset_portal;
                        }
 
                    }
                    
                if (bgIsBboxOverlapping(&portal_min, &portal_max, &scaled_bbmin, &scaled_bbmax))
                {
                    if (cur_room == g_BgPortals[portal_idx].connectedRoom1)
                    {
                        other_room = g_BgPortals[portal_idx].connectedRoom2;
                    }
                    else
                    {
                        other_room = g_BgPortals[portal_idx].connectedRoom1;
                    }
                    
                    for (k = 0; k < cur_count; k++)
                    {
                        if (room_list[k] == other_room)
                        {
                            break;
                        }
                    }
                    
                    if (k == cur_count)
                    {
                        if (cur_count < max_count)
                        {
                            room_list[cur_count] = other_room;
                            cur_count++;
                        }
                        
                        if (cur_count >= max_count)
                        {
                            *count = cur_count;
                            return;
                        }
                    }
                }
                    
next_portal:
                portal_idx++;
                }         
                while (g_BgPortals[portal_idx].offset_portal != NULL);
            }

            i++;
            p++;
                    
            } while (i < saved_count);
        }    
 
        if (cur_count == saved_count)
        {
            break;
        }
     
        saved_count = cur_count;
    }
    *count = cur_count;
}


/**
 * Scan the Gfx commands in the range starting at 'start'. If 'end' is non-NULL
 * treat it as a one-past-last pointer and scan [start, end). If 'end' is NULL
 * treat 'start' as a null-terminated display list and scan until the sentinel
 * command G_ENDDL is encountered.
 *
 * For each Gfx in the scanned range, look up replacement entries in the
 * runtime-selected LUT: ptrDynamic_CC_RM_LUT[lutIndex]. The LUT is organized as
 * (match,replacement) pairs (two Gfx entries per pair), terminated by an
 * entry whose words.w0 == 0. When a match is found replace the command with
 * the LUT's replacement. Replacements are counted in a static counter for
 * telemetry/debugging.
 *
 * Parameters:
 *   start   - pointer to first Gfx to scan
 *   end     - pointer one-past-last Gfx to scan, or NULL to use G_ENDDL sentinel
 *   lutIndex- index into ptrDynamic_CC_RM_LUT selecting the active LUT
 */
void bgApplyDynamicCCRMLUT(Gfx *start, Gfx *end, enum CCRMLUT lutIndex)
{
    Gfx *curGfx;
    Gfx *lutPair;

    static s32 s_bg_lut_replacement_count = 0;

    curGfx = start;

    /* Loop until end pointer or sentinel G_ENDDL (when end==NULL) */
    while (((end != NULL) && (curGfx < end)) || ((end == NULL) && (((s8*)curGfx)[0] != (s8)G_ENDDL)))
    {
        for (lutPair = ptrDynamic_CC_RM_LUT[(s32)lutIndex]; lutPair->words.w0 != 0; lutPair += 2)
        {
            if ((lutPair->words.w0 == curGfx->words.w0) && (lutPair->words.w1 == curGfx->words.w1))
            {
                s_bg_lut_replacement_count += 1;
                *curGfx = *(lutPair + 1);
            }
        }

        curGfx++;
    }
}
