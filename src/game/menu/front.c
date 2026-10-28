/**
 * The front end: every menu from the legal screen to the multiplayer setup. Each menu has
 * four handlers, front<Menu>Enter/Leave/Tick/Render, which frontTick and frontRender call
 * through a switch on g_FrontMenu; frontSetMenu changes menu, directly or through a few
 * black frames. Also the menu choices the game reads later (stage, difficulty, multiplayer
 * setup, 007 sliders) and the cast screen. See docs/subsystems/menu.md.
 */
#include <os_extension.h>
#include <assets/oddtextures.h>
#include "assets/image_externs.h"
#include <ultra64.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <boss.h>
#include <fr.h>
#include "game/text/language.h"
#include <joy.h>
#include <music.h>
#include <random.h>
#include <snd.h>
#include <macro.h>  
#include "game/player/bondview.h"
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/chr/chraction.h"
#include "game/menu/cheat.h"
#include "game/model/chrobjdata.h"
#include "game/save/file.h"
#include "game/menu/front.h"
#include "game/gfx/imagebank.h"
#include "game/level/lv.h"
#include "game/model/anim.h"
#include "game/menu/mp_weapon.h"
#include "game/math/round.h"
#include "game/mission/objective.h"
#include "game/player/player.h"
#include "game/text/text.h"
#include "game/hud/blood.h"
#include "game/gfx/texselect.h"
#include "game/gfx/texrect.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/gfx/dyn.h"
#include "assets/obseg/text/LtitleE.h"
#include "game/math/matrix.h"
#include "game/world/bg.h"
#include "game/chr/chrai.h"
#include "game/menu/title.h"
#include <assets/font_dl.h>
#include "game/gfx/image.h"
#include "game/asset/fileload.h"
#include "gbi_extension.h"
#include "game/model/model.h"
#include <memp.h>
#include <macro.h>
#include "string.h"
#include "game/gfx/zbuf.h"
#include "game/menu/watchmenu.h"
#include "game/player/gun.h"
#include "game/prop/propobj.h"
#include "game/replay/ramromreplay.h"
#include "str.h"


/**
 * Each character showing in the display cast screen lasts 3 seconds with a half second cross-fade
 * and the camera velocity is tuned per refresh rate so its decay per second is the
 * same on PAL (0.95 ^ (60/50) = 0.9403). 
 */
#ifdef REFRESH_PAL
#define CAST_INTRO_LEN          (50 * 3)
#define CAST_INTRO_FADE         (50 / 2)
#define CAST_DAMP               0.940299988f
#define CAST_DAMP_COMP          0.0597000122f
#else
#define CAST_INTRO_LEN          (60 * 3)
#define CAST_INTRO_FADE         (60 / 2)
#define CAST_DAMP               0.94999999f
#define CAST_DAMP_COMP          0.050000012f
#endif
#define CAST_INTRO_FADEOUT      (CAST_INTRO_LEN - CAST_INTRO_FADE + 1)
#define CAST_INTRO_LEN_F        ((f32)CAST_INTRO_LEN)
#define CAST_INTRO_FADE_F       ((f32)CAST_INTRO_FADE)

struct BriefingDataSomething
{
    u16 textid;
    u16 enabled_difficulty;
};

struct BriefingData
{
    u16 brief[4];
    struct BriefingDataSomething objective[OBJECTIVES_MAX];
};

// bss
f32 cast_camera_dist;
f32 cast_camera_dist_start;
f32 cast_camera_dist_end;
f32 cast_camera_angle;
f32 cast_camera_angle_start;
f32 cast_camera_angle_end;
f32 cast_camera_height;
f32 cast_camera_height_start;
f32 cast_camera_height_end;
struct coord3d cast_target_smoothed;
struct coord3d cast_target_accumulator;

bool cast_camera_reset;

struct coord3d cast_rootpos_smoothed;

struct coord3d cast_rootvel_smoothed;

struct coord3d cast_rootvel_accumulator;


f32 g_NintendoLogoSpin;

f32 g_NintendoLogoScale;

coord3d g_FrontFolderScreenPos[MAX_FOLDER_COUNT];

u8 g_CheatAvailable[CHEAT_MAX];

u8 g_CheatActivated[CHEAT_MAX];

#ifdef VERSION_EU

s32 g_MpCharCursor[MAX_PLAYER_COUNT];

s32 g_MpCharPrevCursor[MAX_PLAYER_COUNT];

s32 g_MpFavWeapon[MAX_PLAYER_COUNT][GUNHANDS];

#else

s32 g_MpFavWeapon[MAX_PLAYER_COUNT][GUNHANDS];

s32 g_MpCharCursor[MAX_PLAYER_COUNT];

s32 g_MpCharPrevCursor[MAX_PLAYER_COUNT];

#endif


s32 g_MpCharScroll[MAX_PLAYER_COUNT];

bool g_MpCharChosen[MAX_PLAYER_COUNT];

s32 g_MpCharPortraitSize[MAX_PLAYER_COUNT];
s32 g_MpStickHeld[MAX_PLAYER_COUNT];
char* g_FrontTabStartText;
char* g_FrontTabNextText;
char* g_FrontTabPrevText;
s32 g_MpStageCursor;
s32 g_MpScenarioCursor;
s32 g_FrontDifficultyHighlight;
s32 g_MpTeamsCursor;
struct BriefingData *g_FrontBriefing;
s32 g_NewCheatUnlocked;
s32 g_Front007HlReaction;
s32 g_Front007HlHealth;
s32 g_Front007HlDamage;
s32 g_Front007HlAccuracy;

s32 g_MpHandicap[MAX_PLAYER_COUNT];

s32 g_MpControlStyle[MAX_PLAYER_COUNT];

s32 g_MpHlPlayers;
s32 g_MpHlScenario;
s32 g_MpHlStage;
s32 g_MpHlLength;
s32 g_MpHlChar;
s32 g_MpHlWeapons;
s32 g_MpHlHandicap;
s32 g_MpHlControls;
s32 g_MpHlAim;

/**
 * EU .bss 0x80058730
*/
u32 g_CheatMenuList[CHEAT_INVALID];


MENU g_FrontMenu = MENU_INVALID;
MENU g_FrontNextMenu = MENU_INVALID;
MENU g_FrontEnterMenu = MENU_INVALID;

s32 g_FrontTimer = 0;

s32 g_FrontTabStartSelected = FALSE;
s32 g_FrontTabNextSelected = FALSE;
s32 g_FrontTabPrevSelected = FALSE;

s32 g_FrontTabStartHighlight = FALSE;
s32 g_FrontTabNextHighlight = FALSE;
s32 g_FrontTabPrevHighlight = FALSE;

s32 g_FrontFolder = FOLDER1;
s32 g_FrontFolderSaved = FOLDER1;

GAMEMODE g_FrontGameMode = GAMEMODE_INTRO;
s32 g_FrontStage = LEVELID_NONE;
s32 g_FrontMission = BRIEFING_INVALID;
DIFFICULTY g_FrontDifficulty = DIFFICULTY_MULTI;

s32 g_AppendCheatSinglePlayer = FALSE;
s32 g_AppendCheatMultiPlayer = FALSE;

f32 g_FrontCursorX = 220.0;
f32 g_FrontCursorY = 165.0;

s32 g_FrontBriefingLastPage = BRIEFING_M;
s32 g_FrontBriefingPage = BRIEFING_TITLE;

s32 g_FrontFolderIcon = 0;
s32 g_FrontFolderErase = FOLDER_INVALID;
s32 g_FrontFolderEraseChoice = FOLDER2;

s32 g_FrontMissionAborted = FALSE;
bool g_FrontBondKilled = FALSE;

s32 g_FrontFirstLegal = TRUE;
s32 g_FrontFirstBoot = TRUE;

s32 g_FrontSkipIntro = FALSE;
s32 g_FrontLogoKeyPressed = FALSE;

s32 g_FrontStartMusic = TRUE;
s32 g_FrontScreenSize = SCREEN_SIZE_320x240;

s32 g_FrontHiResPending = 0;
s32 g_FrontLowResPending = FALSE;

s32 g_CheatMenuAvailable = FALSE;

u8 * g_FrontModelBuffer = NULL;
s32 g_FrontFrameBuffer = 0;
struct Model *g_FrontLogoModel = NULL;
struct Model * g_FrontFolderModels[] = { NULL, NULL, NULL, NULL};

Lights1 g_FrontLogoLight = gdSPDefLights1(
    0x96, 0x96, 0x96,
    0xFF, 0xFF, 0xFF,
    77, 77, 46
);

Lights1 g_FrontNintendoLight = gdSPDefLights1(
    0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00,
    0, 0, 0
);


f32 g_Front007Reaction = 0.0f;
f32 g_Front007Health = 1.0f;
f32 g_Front007Damage = 1.0f;
f32 g_Front007Accuracy = 1.0f;

struct coord3d legalpage_pos = {0.0f, 0.0f, 0.0f};

struct legal_screen_text g_LegalTexts[] = {
    {220,  30, CENTER_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_07_TWY), 0}, //"TWYCROSS BOARD OF GAME CLASSIFICATION\n"
    { 34,  83, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_08_CERT), 0}, //"This is to certify\n"
    {226,  84, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_09_NINRARE), 0}, //"(c) 1997 Nintendo/Rare\n"
    {226,  97, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_10_DANJAQ), 0}, //"(c) 1962, 1995 Danjaq, LLC. &\n"
    {226, 110, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_11_UAC), 0}, //"U.A.C. All Rights Reserved\n"
    {226, 122, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_12_EON), 0}, //"(c) 1997 Eon Productions\n"
    {227, 134, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_13_MACB), 0}, //"Ltd. & Mac B. Inc.\n"
    {219, 211, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_14_PERSONS), 0}, //"Suitable only for 1-4 persons\n"
    { 60, 169, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_15_PRESIDENT), 0}, //"PRESIDENT\n"
    { 60, 201, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_16_VICE), 0}, //"VICE\n"
    { 99, 266, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_17_NORMAN), 0}, //"James Bond theme by Monty Norman.\n"
    { 80, 280, LEFT_ALIGN, CENTER_ALIGN, getStringID(LTITLE, TITLE_STR_18_EMI), 0}  //"Used by permission of EMI Unart Catalog Inc.\n"
};

ModelRenderData legalscreen_MRD = {NULL,
                                    TRUE,
                                    0x00000003,
                                    NULL,

                                    NULL,
                                    0,
                                    0,
                                    0,

                                    0,
                                    0,
                                    0,
                                    0,

                                    0,
                                    {0, 0, 0, 0},
                                    {0, 0, 0, 0},
                                    CULLMODE_BOTH};


struct coord3d nintendologo_pos = {0};

ModelRenderData       nintendologo_MRD = {NULL,
                                    TRUE,
                                    0x00000003,
                                    NULL,

                                    NULL,
                                    0,
                                    0,
                                    0,

                                    0,
                                    0,
                                    0,
                                    0,

                                    0,
                                    {0, 0, 0, 0},
                                    {0, 0, 0, 0},
                                    CULLMODE_BOTH};


struct coord3d goldeneyelogo_pos = { 0 };

ModelRenderData       goldeneyelogo_MRD = {NULL,
                                    TRUE,
                                    0x00000003,
                                    NULL,

                                    NULL,
                                    0,
                                    0,
                                    0,

                                    0,
                                    0,
                                    0,
                                    0,

                                    0,
                                    {0, 0, 0, 0},
                                    {0, 0, 0, 0},
                                    CULLMODE_BOTH};


struct coord3d g_FrontFolderPos[] = {
    {-900.0f, 800.0f, 0.0f},
    {1800.0f, 800.0f, 0.0f},
    {-1800.0f, -200.0f, 0.0f},
    {900.0f, -200.0f, 0.0f}
};

struct rectbbox folder_option_COPY_bound = { 0 };
struct rectbbox folder_option_ERASE_bound = { 0 };

struct mission_folder_setup g_FrontMissions[] = {
  /* string_ptr  folder_text_preset                                                         icon_text_preset                                            stage_id           unknown type             mission_num briefing_name_ptr */
    {"1",        getStringID(LTITLE, TITLE_STR_120_ARK)            /* Arkangelsk */,        0,                                                          LEVELID_NONE,           0, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_121_DAM)            /* Dam */,               0,                                                          LEVELID_DAM,            0, MISSION_PART,             0, "UbriefdamZ"},
    {"ii",       getStringID(LTITLE, TITLE_STR_122_FAC)            /* Facility */,          0,                                                          LEVELID_FACILITY,       0, MISSION_PART,             1, "UbriefarkZ"},
    {"iii",      getStringID(LTITLE, TITLE_STR_123_RUN)            /* Runway */,            0,                                                          LEVELID_RUNWAY,         0, MISSION_PART,             2, "UbriefrunZ"},
    {"2",        getStringID(LTITLE, TITLE_STR_124_SEV)            /* Severnaya */,         0,                                                          LEVELID_NONE,           0, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_125_SURF)           /* Surface */,           0,                                                          LEVELID_SURFACE,        0, MISSION_PART,             3, "UbriefsevxZ"},
    {"ii",       getStringID(LTITLE, TITLE_STR_126_BUNK)           /* Bunker */,            0,                                                          LEVELID_BUNKER1,        0, MISSION_PART,             4, "UbriefsevbunkerZ"},
    {"3",        getStringID(LTITLE, TITLE_STR_127_KIRG)           /* Kirghizstan */,       0,                                                          LEVELID_NONE,           1, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_128_SILO4)          /* Launch Silo #4 */,    getStringID(LTITLE, TITLE_STR_129_SILO)     /* Silo */,     LEVELID_SILO,           1, MISSION_PART,             5, "UbriefsiloZ"},
    {"4",        getStringID(LTITLE, TITLE_STR_130_MONTE)          /* Monte Carlo */,       0,                                                          LEVELID_NONE,           1, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_131_FRIG)           /* Frigate */,           0,                                                          LEVELID_FRIGATE,        1, MISSION_PART,             6, "UbriefdestZ"},
    {"5",        getStringID(LTITLE, TITLE_STR_124_SEV)            /* Severnaya */,         0,                                                          LEVELID_NONE,           1, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_125_SURF)           /* Surface */,           0,                                                          LEVELID_SURFACE2,       1, MISSION_PART,             7, "UbriefsevxbZ"},
    {"ii",       getStringID(LTITLE, TITLE_STR_126_BUNK)           /* Bunker */,            0,                                                          LEVELID_BUNKER2,        1, MISSION_PART,             8, "UbriefsevbZ"},
    {"6",        getStringID(LTITLE, TITLE_STR_132_STPETER)        /* St. Petersburg */,    0,                                                          LEVELID_NONE,           2, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_133_STATPARK)       /* Statue Park */,       getStringID(LTITLE, TITLE_STR_134_STAT)     /* Statue */,   LEVELID_STATUE,         2, MISSION_PART,             9, "UbriefstatueZ"},
    {"ii",       getStringID(LTITLE, TITLE_STR_135_MILARCH)        /* Military Archives */, getStringID(LTITLE, TITLE_STR_136_ARCH)     /* Archives */, LEVELID_ARCHIVES,       2, MISSION_PART,            10, "UbriefarchZ"},
    {"iii",      getStringID(LTITLE, TITLE_STR_137_STREETS)        /* Streets */,           0,                                                          LEVELID_STREETS,        2, MISSION_PART,            11, "UbriefpeteZ"},
    {"iv",       getStringID(LTITLE, TITLE_STR_138_DEPOT)          /* Depot */,             0,                                                          LEVELID_DEPOT,          2, MISSION_PART,            12, "UbriefdepoZ"},
    {"v",        getStringID(LTITLE, TITLE_STR_139_TRAIN)          /* Train */,             0,                                                          LEVELID_TRAIN,          2, MISSION_PART,            13, "UbrieftraZ"},
    {"7",        getStringID(LTITLE, TITLE_STR_140_CUBA)           /* Cuba */,              0,                                                          LEVELID_NONE,           3, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_141_JUN)            /* Jungle */,            0,                                                          LEVELID_JUNGLE,         3, MISSION_PART,            14, "UbriefjunZ"},
    {"ii",       getStringID(LTITLE, TITLE_STR_142_CONCENTER)      /* Control Center */,    getStringID(LTITLE, TITLE_STR_143_CON)      /* Control */,  LEVELID_CONTROL,        3, MISSION_PART,            15, "UbriefcontrolZ"},
    {"iii",      getStringID(LTITLE, TITLE_STR_144_WATERCAV)       /* Water Caverns */,     getStringID(LTITLE, TITLE_STR_145_CAV)      /* Caverns */,  LEVELID_CAVERNS,        3, MISSION_PART,            16, "UbriefcaveZ"},
    {"iv",       getStringID(LTITLE, TITLE_STR_146_ANTENNA)        /* Antenna Cradle */,    getStringID(LTITLE, TITLE_STR_147_CRADLE)   /* Cradle */,   LEVELID_CRADLE,         3, MISSION_PART,            17, "UbriefcradZ"},
    {"8",        getStringID(LTITLE, TITLE_STR_148_TEOTIHUACA)     /* Teotihuaca'n */,      0,                                                          LEVELID_NONE,           4, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_149_AZTECCOMPLEX)   /* Aztec Complex */,     getStringID(LTITLE, TITLE_STR_150_AZTEC)    /* Aztec */,    LEVELID_AZTEC,          4, MISSION_PART,            18, "UbriefaztZ"},
    {"9",        getStringID(LTITLE, TITLE_STR_151_ELSAGHIRA)      /* el-Saghira */,        0,                                                          LEVELID_NONE,           4, MISSION_HEADER,          -1, 0},
    {"i",        getStringID(LTITLE, TITLE_STR_152_EGYPTIANTEMPLE) /* Egyptian Temple */,   getStringID(LTITLE, TITLE_STR_153_EGYPTIAN) /* Egyptian */, LEVELID_EGYPT,          4, MISSION_PART,            19, "UbriefcrypZ"},
    {NULL,       0,                                                                         0,                                                          LEVELID_NONE,          -1, MISSION_PART,            -1, 0}
};






f32 g_FrontFolderCamOffsetX = 0;
f32 g_FrontFolderCamOffsetY = 190.0;
f32 g_FrontFolderCamOffsetZ = -3300.0;
s32 g_FrontMissionCursorX[] = {73, 142, 212, 282, 352};

#if defined(VERSION_EU)
s32 g_FrontMissionCursorY[] = {62, 130, 201, 270};
#else
s32 g_FrontMissionCursorY[] = {62, 131, 201, 270};
#endif

struct MP_game_length_settings g_MpGameLengths[] = {
    {getStringID(LTITLE, TITLE_STR_45_UNLIMITED), 0, 0, 0},                    /* unlimited */
    {getStringID(LTITLE, TITLE_STR_46_5MIN), 0, MINS_TO_TIMER60(5), 0},   /* 5 minutes */
    {getStringID(LTITLE, TITLE_STR_47_10MIN), 0, MINS_TO_TIMER60(10), 0},  /* 10 minutes */
    {getStringID(LTITLE, TITLE_STR_48_20MIN), 0, MINS_TO_TIMER60(20), 0},  /* 20 minutes */
    {getStringID(LTITLE, TITLE_STR_49_5PT), 0, 0, 5},                    /* first to 5 points */
    {getStringID(LTITLE, TITLE_STR_50_10PT), 0, 0, 10},                   /* first to 10 points */
    {getStringID(LTITLE, TITLE_STR_51_20PT), 0, 0, 20},                   /* first to 20 points */
    {getStringID(LTITLE, TITLE_STR_52_LASTALIVE), 0, 0, 0}                     /* last person alive wins */
};

struct mp_stage_playercount g_MpStagePlayerCounts[] = {
    {getStringID(LTITLE, TITLE_STR_53_NORMAL), 2, 4}, //"Normal"
    {getStringID(LTITLE, TITLE_STR_54_YOLT), 2, 4}, //"You Only Live Twice"
    {getStringID(LTITLE, TITLE_STR_55_FLAGTAG), 2, 4}, //"The Living Daylights [Flag Tag]"
    {getStringID(LTITLE, TITLE_STR_56_GG), 2, 4}, //"The Man With the Golden Gun"
    {getStringID(LTITLE, TITLE_STR_57_LTK), 2, 4}, //"License to Kill"
    {getStringID(LTITLE, TITLE_STR_58_2V2), 4, 4}, //"Team: 2 vs 2"
    {getStringID(LTITLE, TITLE_STR_59_3V1), 4, 4}, //"Team: 3 vs 1"
    {getStringID(LTITLE, TITLE_STR_60_2V1), 3, 3}  //"Team: 2 vs 1"
};

struct mp_stage_setup g_MpStages[] = {
    /* unlocked by default */
    {getStringID(LTITLE, TITLE_STR_154_RANDOM), getStringID(LTITLE, TITLE_STR_155_RANDOM2), IMG_MP_RANDOM, LEVELID_NONE, -1, 1, 4},
    {getStringID(LTITLE, TITLE_STR_156_TEMPLE), getStringID(LTITLE, TITLE_STR_157_TEMPLE2), IMG_MP_TEMPLE, LEVELID_TEMPLE, -1, 1, 4},
    {getStringID(LTITLE, TITLE_STR_158_COMPLEX), getStringID(LTITLE, TITLE_STR_159_COMPLEX2), IMG_MP_COMPLEX, LEVELID_COMPLEX, -1, 1, 4},
    {getStringID(LTITLE, TITLE_STR_162_CAVES), getStringID(LTITLE, TITLE_STR_163_CAVES2), IMG_MP_CAVES, LEVELID_CAVES, -1, 1, 4},
    {getStringID(LTITLE, TITLE_STR_160_LIBRARY), getStringID(LTITLE, TITLE_STR_161_LIBRARY2), IMG_MP_BASEMENT, LEVELID_LIBRARY, -1, 1, 4},
    {getStringID(LTITLE, TITLE_STR_269_BASEMENT), getStringID(LTITLE, TITLE_STR_270_BASEMENT2), IMG_MP_BASEMENT, LEVELID_BASEMENT, -1, 1, 4},
    {getStringID(LTITLE, TITLE_STR_271_STACK), getStringID(LTITLE, TITLE_STR_272_STACK2), IMG_MP_BASEMENT, LEVELID_STACK, -1, 1, 4},
    /* unlocked on level progression */
    {getStringID(LTITLE, TITLE_STR_164_FACMP), getStringID(LTITLE, TITLE_STR_165_FACMP22), IMG_MP_FACILITY, LEVELID_FACILITY, SP_LEVEL_FACILITY, 1, 4},
    {getStringID(LTITLE, TITLE_STR_166_SEVERNAYABUNKER), getStringID(LTITLE, TITLE_STR_167_BUNKER), IMG_MP_BUNKER2, LEVELID_BUNKER2, SP_LEVEL_BUNKER2, 1, 3},
    {getStringID(LTITLE, TITLE_STR_168_MILITARYARCHIVES), getStringID(LTITLE, TITLE_STR_169_ARCHIVES), IMG_MP_ARCHIVES, LEVELID_ARCHIVES, SP_LEVEL_ARCHIVES, 1, 3},
    {getStringID(LTITLE, TITLE_STR_170_WATERCAVERNS), getStringID(LTITLE, TITLE_STR_171_CAVERNS), IMG_MP_CAVERNS, LEVELID_CAVERNS, SP_LEVEL_CAVERNS, 1, 3},
    {getStringID(LTITLE, TITLE_STR_172_EGYPTIANTEMPLEMP), getStringID(LTITLE, TITLE_STR_173_EGYPTIANMP), IMG_MP_EGYPT, LEVELID_EGYPT, SP_LEVEL_EGYPT, 1, 2}
  //{getStringID(LTITLE, TITLE_STR_174_CITADEL), getStringID(LTITLE, TITLE_STR_175_CITADEL2), IMG_MP_RANDOM, LEVELID_CITADEL, -1, 1, 4}, //Citadel (old format setup)
  //{getStringID(LTITLE, TITLE_STR_176_DEST), getStringID(LTITLE, TITLE_STR_177_DEST2), IMG_MP_FRIGATE, LEVELID_FRIGATE, SP_LEVEL_FRIGATE, 1, 4}, //dest (has xbla setup)
  //{getStringID(LTITLE, TITLE_STR_178_STAT), getStringID(LTITLE, TITLE_STR_179_STAT2), IMG_MP_STATUE, LEVELID_STATUE, SP_LEVEL_STATUE, 1, 2}, //stat (works)
  //{getStringID(LTITLE, TITLE_STR_180_CRAD), getStringID(LTITLE, TITLE_STR_181_CRADLE2), IMG_MP_CRADLE, LEVELID_CRADLE, SP_LEVEL_CRADLE, 1, 2}, //crad (works)
  //{getStringID(LTITLE, TITLE_STR_182_AZT), getStringID(LTITLE, TITLE_STR_183_AZT2), IMG_MP_AZTEC, LEVELID_AZTEC, SP_LEVEL_AZTEC, 1, 4}, //azt (needs setup)
  //{getStringID(LTITLE, TITLE_STR_123_RUN), getStringID(LTITLE, TITLE_STR_123_RUN), IMG_MP_RUNWAY, LEVELID_RUNWAY, SP_LEVEL_RUN, 1, 4}, //runway (xbla setup)
  //{getStringID(LTITLE, TITLE_STR_121_DAM), getStringID(LTITLE, TITLE_STR_121_DAM), IMG_MP_DAM, LEVELID_DAM, SP_LEVEL_DAM, 1, 4}, //dam (xbla setup)
  //{getStringID(LTITLE, TITLE_STR_138_DEPOT), getStringID(LTITLE, TITLE_STR_138_DEPOT), IMG_MP_DEPOT, LEVELID_DEPOT, SP_LEVEL_DEPOT, 1, 4}, //depot (xbla setup)
};

s32 g_MpCharCount = 8;

#ifdef BUGFIX_R0
struct MP_selectable_chars g_MpChars[] = {
    {getStringID(LTITLE, TITLE_STR_184_BOND), MALE,   IMG_MPC_BROSNAN, BODY_Brosnan_Tuxedo,                   HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_191_NATALYA), FEMALE, IMG_MPC_NATALYA, BODY_Natalya_Skirt,            HEAD_Male_Brosnan_Tuxedo, 0.96609998},
    {getStringID(LTITLE, TITLE_STR_188_TREVELYAN), MALE,   IMG_MPC_TREVELYAN, BODY_Trevelyan_Janus,          HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_190_XENIA), FEMALE, IMG_MPC_XENIA, BODY_Xenia,                    HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_187_OURUMOV), MALE,   IMG_MPC_OURUMOV, BODY_Ourumov,                  HEAD_Male_Brosnan_Tuxedo, 1.0778},
    {getStringID(LTITLE, TITLE_STR_186_BORIS), MALE,   IMG_MPC_BORIS, BODY_Boris,                    HEAD_Male_Brosnan_Tuxedo, 0.9702},
    {getStringID(LTITLE, TITLE_STR_189_VALENTIN), MALE,   IMG_MPC_VALENTIN, BODY_Valentin_,                HEAD_Male_Brosnan_Tuxedo, 0.93239999},
    {getStringID(LTITLE, TITLE_STR_185_MISHKIN), MALE,   IMG_MPC_MISHKIN, BODY_Siberian_Guard_1_Mishkin, HEAD_Male_Mishkin, 1.0},
    {getStringID(LTITLE, TITLE_STR_194_MAYDAY), FEMALE, IMG_MPC_MAYDAY, BODY_Mayday,                   HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_193_JAWS), MALE,   IMG_MPC_JAWS, BODY_Jaws,                     HEAD_Male_Brosnan_Tuxedo, 1.199},
    {getStringID(LTITLE, TITLE_STR_195_ODDJOB), MALE,   IMG_MPC_ODDJOB, BODY_Oddjob,                   HEAD_Male_Brosnan_Tuxedo,0.78780001},
    {getStringID(LTITLE, TITLE_STR_192_BARONSAMEDI), MALE,   IMG_MPC_BARON, BODY_Baron_Samedi,             HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_253_RUSSIANSOLDIER), MALE,   IMG_MPC_RANDOM, BODY_Russian_Soldier,          HEAD_Male_Mark, 1.0},
    {getStringID(LTITLE, TITLE_STR_252_RUSSIANINFANTRY), MALE,   IMG_MPC_RANDOM, BODY_Russian_Infantry,         HEAD_Male_Karl, 1.0},
    {getStringID(LTITLE, TITLE_STR_263_SCIENTIST), MALE,   IMG_MPC_RANDOM, BODY_Scientist_1_Male,         HEAD_Male_Dave_Dr_Doak, 1.0},
    {getStringID(LTITLE, TITLE_STR_263_SCIENTIST), FEMALE, IMG_MPC_RANDOM, BODY_Scientist_2_Female,       HEAD_Female_Sally, 1.0},
    {getStringID(LTITLE, TITLE_STR_256_RUSSIANCOMMANDANT), MALE,   IMG_MPC_RANDOM, BODY_Russian_Commandant,       HEAD_Male_Martin, 1.0},
    {getStringID(LTITLE, TITLE_STR_254_JANUSMARINE), MALE,   IMG_MPC_RANDOM, BODY_Janus_Marine,             HEAD_Male_Steve_Ellis, 1.0},
    {getStringID(LTITLE, TITLE_STR_257_NAVALOFFICER), MALE,   IMG_MPC_RANDOM, BODY_Naval_Officer,            HEAD_Male_Duncan, 1.0},
    {getStringID(LTITLE, TITLE_STR_262_HELICOPTERPILOT), MALE,   IMG_MPC_RANDOM, BODY_Helicopter_Pilot,         HEAD_Male_Pete, 1.0},
    {getStringID(LTITLE, TITLE_STR_251_STPETERSBURGGUARD), MALE,   IMG_MPC_RANDOM, BODY_St_Petersburg_Guard,      HEAD_Male_Ken, 1.0},
    {getStringID(LTITLE, TITLE_STR_264_CIVILIAN), FEMALE, IMG_MPC_RANDOM, BODY_Civilian_1_Female,        HEAD_Female_Marion_Rosika, 1.0},
    {getStringID(LTITLE, TITLE_STR_264_CIVILIAN), MALE,   IMG_MPC_RANDOM, BODY_Civilian_2,               HEAD_Male_Graeme, 1.0},
    {getStringID(LTITLE, TITLE_STR_264_CIVILIAN), MALE,   IMG_MPC_RANDOM, BODY_Civilian_3,               HEAD_Male_Grant, 1.0},
    {getStringID(LTITLE, TITLE_STR_264_CIVILIAN), MALE,   IMG_MPC_RANDOM, BODY_Civilian_4,               HEAD_Male_Dwayne, 1.0},
    {getStringID(LTITLE, TITLE_STR_258_SIBERIANGUARD), MALE,   IMG_MPC_RANDOM, BODY_Siberian_Guard_1_Mishkin, HEAD_Male_Lee, 1.0},
    {getStringID(LTITLE, TITLE_STR_259_ARCTICCOMMANDO), MALE,   IMG_MPC_RANDOM, BODY_Arctic_Commando,          HEAD_Male_Chris, 1.0},
    {getStringID(LTITLE, TITLE_STR_258_SIBERIANGUARD), MALE,   IMG_MPC_RANDOM, BODY_Siberian_Guard_2,         HEAD_Male_Scott, 1.0},
    {getStringID(LTITLE, TITLE_STR_260_SIBERIANSPECIALFORCES), MALE,   IMG_MPC_RANDOM, BODY_Siberian_Special_Forces,  HEAD_Male_Alan, 1.0},
    {getStringID(LTITLE, TITLE_STR_250_JUNGLECOMMANDO), MALE,   IMG_MPC_RANDOM, BODY_Jungle_Commando,          HEAD_Male_Joel, 1.0},
    {getStringID(LTITLE, TITLE_STR_255_JANUSSPECIALFORCES), MALE,   IMG_MPC_RANDOM, BODY_Janus_Special_Forces,     HEAD_Male_B, 1.0},
    {getStringID(LTITLE, TITLE_STR_261_MOOKRAKERELITE), MALE,   IMG_MPC_RANDOM, BODY_Moonraker_Elite_1_Male,   HEAD_Male_Neil, 1.0},
    {getStringID(LTITLE, TITLE_STR_261_MOOKRAKERELITE), FEMALE, IMG_MPC_RANDOM, BODY_Moonraker_Elite_2_Female, HEAD_Female_Vivien, 1.0},
    {getStringID(LTITLE, TITLE_STR_196_ROSIKA), FEMALE, IMG_MPC_RANDOM, BODY_Rosika,                   HEAD_Female_Marion_Rosika, 0.88529998},
    {getStringID(LTITLE, TITLE_STR_197_KARL), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Karl, 1.0446},
    {getStringID(LTITLE, TITLE_STR_198_MARTIN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Martin, 1.0446},
    {getStringID(LTITLE, TITLE_STR_199_MARK), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Mark, 1.0446},
    {getStringID(LTITLE, TITLE_STR_200_DAVE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Dave_Dr_Doak, 1.0446},
    {getStringID(LTITLE, TITLE_STR_201_DUNCAN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Duncan, 1.0446},
    {getStringID(LTITLE, TITLE_STR_202_B), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_B, 1.0446},
    {getStringID(LTITLE, TITLE_STR_203_STEVEE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Steve_Ellis, 1.0446},
    {getStringID(LTITLE, TITLE_STR_204_GRANT), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Grant, 1.0446},
    {getStringID(LTITLE, TITLE_STR_205_GRAEME), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Graeme, 1.0446},
    {getStringID(LTITLE, TITLE_STR_206_KEN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Ken, 1.0446},
    {getStringID(LTITLE, TITLE_STR_207_ALAN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Alan, 1.0446},
    {getStringID(LTITLE, TITLE_STR_208_PETE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Pete, 1.0446},
    {getStringID(LTITLE, TITLE_STR_209_SHAUN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Shaun, 1.0446},
    {getStringID(LTITLE, TITLE_STR_210_DWAYNE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Dwayne, 1.0446},
    {getStringID(LTITLE, TITLE_STR_211_DES), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Des, 1.0446},
    {getStringID(LTITLE, TITLE_STR_212_CHRIS), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Chris, 1.0446},
    {getStringID(LTITLE, TITLE_STR_213_LEE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Lee, 1.0446},
    {getStringID(LTITLE, TITLE_STR_214_NEIL), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Neil, 1.0446},
    {getStringID(LTITLE, TITLE_STR_215_JIM), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Jim, 1.0446},
    {getStringID(LTITLE, TITLE_STR_216_ROBIN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Robin, 1.0446},
    {getStringID(LTITLE, TITLE_STR_217_STEVEH), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Steve_H, 1.0446},
    {getStringID(LTITLE, TITLE_STR_218_TERRORIST), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Terrorist, 1.0446},
    {getStringID(LTITLE, TITLE_STR_219_BIKER), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Biker, 1.0446},
    {getStringID(LTITLE, TITLE_STR_220_JOEL), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Joel, 1.0446},
    {getStringID(LTITLE, TITLE_STR_221_SCOTT), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Scott, 1.0446},
    {getStringID(LTITLE, TITLE_STR_222_JOE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Joe, 1.0446},
    {getStringID(LTITLE, TITLE_STR_223_SALLY), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Female_Sally, 1.0446},
    {getStringID(LTITLE, TITLE_STR_224_MARION), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Female_Marion_Rosika, 1.0446},
    {getStringID(LTITLE, TITLE_STR_225_MANDY), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Female_Mandy, 1.0446},
    {getStringID(LTITLE, TITLE_STR_226_VIVIEN), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Female_Vivien, 1.0446}
};
#else
struct MP_selectable_chars g_MpChars[] = {
    {getStringID(LTITLE, TITLE_STR_184_BOND), MALE,   IMG_MPC_BROSNAN, BODY_Brosnan_Tuxedo,                   HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_191_NATALYA), FEMALE, IMG_MPC_NATALYA, BODY_Natalya_Skirt,            HEAD_Male_Brosnan_Tuxedo, 0.96609998},
    {getStringID(LTITLE, TITLE_STR_188_TREVELYAN), MALE,   IMG_MPC_TREVELYAN, BODY_Trevelyan_Janus,          HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_190_XENIA), FEMALE, IMG_MPC_XENIA, BODY_Xenia,                    HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_187_OURUMOV), MALE,   IMG_MPC_OURUMOV, BODY_Ourumov,                  HEAD_Male_Brosnan_Tuxedo, 1.0778},
    {getStringID(LTITLE, TITLE_STR_186_BORIS), MALE,   IMG_MPC_BORIS, BODY_Boris,                    HEAD_Male_Brosnan_Tuxedo, 0.9702},
    {getStringID(LTITLE, TITLE_STR_189_VALENTIN), MALE,   IMG_MPC_VALENTIN, BODY_Valentin_,                HEAD_Male_Brosnan_Tuxedo, 0.93239999},
    {getStringID(LTITLE, TITLE_STR_185_MISHKIN), MALE,   IMG_MPC_MISHKIN, BODY_Siberian_Guard_1_Mishkin, HEAD_Male_Mishkin, 1.0},
    {getStringID(LTITLE, TITLE_STR_194_MAYDAY), FEMALE, IMG_MPC_MAYDAY, BODY_Mayday,                   HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_193_JAWS), MALE,   IMG_MPC_JAWS, BODY_Jaws,                     HEAD_Male_Brosnan_Tuxedo, 1.199},
    {getStringID(LTITLE, TITLE_STR_195_ODDJOB), MALE,   IMG_MPC_ODDJOB, BODY_Oddjob,                   HEAD_Male_Brosnan_Tuxedo,0.78780001},
    {getStringID(LTITLE, TITLE_STR_192_BARONSAMEDI), MALE,   IMG_MPC_BARON, BODY_Baron_Samedi,             HEAD_Male_Brosnan_Tuxedo, 1.0},
    {getStringID(LTITLE, TITLE_STR_287_RUSSIANSOLDIER), MALE,   IMG_MPC_RANDOM, BODY_Russian_Soldier,          HEAD_Male_Mark, 1.0},
    {getStringID(LTITLE, TITLE_STR_288_RUSSIANINFANTRY), MALE,   IMG_MPC_RANDOM, BODY_Russian_Infantry,         HEAD_Male_Karl, 1.0},
    {getStringID(LTITLE, TITLE_STR_289_SCIENTIST), MALE,   IMG_MPC_RANDOM, BODY_Scientist_1_Male,         HEAD_Male_Dave_Dr_Doak, 1.0},
    {getStringID(LTITLE, TITLE_STR_289_SCIENTIST), FEMALE, IMG_MPC_RANDOM, BODY_Scientist_2_Female,       HEAD_Female_Sally, 1.0},
    {getStringID(LTITLE, TITLE_STR_290_RUSSIANCOMMANDANT), MALE,   IMG_MPC_RANDOM, BODY_Russian_Commandant,       HEAD_Male_Martin, 1.0},
    {getStringID(LTITLE, TITLE_STR_291_JANUSMARINE), MALE,   IMG_MPC_RANDOM, BODY_Janus_Marine,             HEAD_Male_Steve_Ellis, 1.0},
    {getStringID(LTITLE, TITLE_STR_292_NAVALOFFICER), MALE,   IMG_MPC_RANDOM, BODY_Naval_Officer,            HEAD_Male_Duncan, 1.0},
    {getStringID(LTITLE, TITLE_STR_293_HELICOPTERPILOT), MALE,   IMG_MPC_RANDOM, BODY_Helicopter_Pilot,         HEAD_Male_Pete, 1.0},
    {getStringID(LTITLE, TITLE_STR_294_STPETERSBURGARD), MALE,   IMG_MPC_RANDOM, BODY_St_Petersburg_Guard,      HEAD_Male_Ken, 1.0},
    {getStringID(LTITLE, TITLE_STR_295_CIVILIAN), FEMALE, IMG_MPC_RANDOM, BODY_Civilian_1_Female,        HEAD_Female_Marion_Rosika, 1.0},
    {getStringID(LTITLE, TITLE_STR_295_CIVILIAN), MALE,   IMG_MPC_RANDOM, BODY_Civilian_2,               HEAD_Male_Graeme, 1.0},
    {getStringID(LTITLE, TITLE_STR_295_CIVILIAN), MALE,   IMG_MPC_RANDOM, BODY_Civilian_3,               HEAD_Male_Grant, 1.0},
    {getStringID(LTITLE, TITLE_STR_295_CIVILIAN), MALE,   IMG_MPC_RANDOM, BODY_Civilian_4,               HEAD_Male_Dwayne, 1.0},
    {getStringID(LTITLE, TITLE_STR_296_SIBERIANGUARD), MALE,   IMG_MPC_RANDOM, BODY_Siberian_Guard_1_Mishkin, HEAD_Male_Lee, 1.0},
    {getStringID(LTITLE, TITLE_STR_297_ARCTICCOMMANDO), MALE,   IMG_MPC_RANDOM, BODY_Arctic_Commando,          HEAD_Male_Chris, 1.0},
    {getStringID(LTITLE, TITLE_STR_296_SIBERIANGUARD), MALE,   IMG_MPC_RANDOM, BODY_Siberian_Guard_2,         HEAD_Male_Scott, 1.0},
    {getStringID(LTITLE, TITLE_STR_298_SIBERIANSPECIALFORCES), MALE,   IMG_MPC_RANDOM, BODY_Siberian_Special_Forces,  HEAD_Male_Alan, 1.0},
    {getStringID(LTITLE, TITLE_STR_299_JUNGLECOMMANDO), MALE,   IMG_MPC_RANDOM, BODY_Jungle_Commando,          HEAD_Male_Joel, 1.0},
    {getStringID(LTITLE, TITLE_STR_300_JANUSSPECIALFORCES), MALE,   IMG_MPC_RANDOM, BODY_Janus_Special_Forces,     HEAD_Male_B, 1.0},
    {getStringID(LTITLE, TITLE_STR_301_MOONRAKERELITE), MALE,   IMG_MPC_RANDOM, BODY_Moonraker_Elite_1_Male,   HEAD_Male_Neil, 1.0},
    {getStringID(LTITLE, TITLE_STR_301_MOONRAKERELITE), FEMALE, IMG_MPC_RANDOM, BODY_Moonraker_Elite_2_Female, HEAD_Female_Vivien, 1.0},
    {getStringID(LTITLE, TITLE_STR_196_ROSIKA), FEMALE, IMG_MPC_RANDOM, BODY_Rosika,                   HEAD_Female_Marion_Rosika, 0.88529998},
    {getStringID(LTITLE, TITLE_STR_197_KARL), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Karl, 1.0446},
    {getStringID(LTITLE, TITLE_STR_198_MARTIN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Martin, 1.0446},
    {getStringID(LTITLE, TITLE_STR_199_MARK), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Mark, 1.0446},
    {getStringID(LTITLE, TITLE_STR_200_DAVE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Dave_Dr_Doak, 1.0446},
    {getStringID(LTITLE, TITLE_STR_201_DUNCAN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Duncan, 1.0446},
    {getStringID(LTITLE, TITLE_STR_202_B), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_B, 1.0446},
    {getStringID(LTITLE, TITLE_STR_203_STEVEE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Steve_Ellis, 1.0446},
    {getStringID(LTITLE, TITLE_STR_204_GRANT), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Grant, 1.0446},
    {getStringID(LTITLE, TITLE_STR_205_GRAEME), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Graeme, 1.0446},
    {getStringID(LTITLE, TITLE_STR_206_KEN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Ken, 1.0446},
    {getStringID(LTITLE, TITLE_STR_207_ALAN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Alan, 1.0446},
    {getStringID(LTITLE, TITLE_STR_208_PETE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Pete, 1.0446},
    {getStringID(LTITLE, TITLE_STR_209_SHAUN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Shaun, 1.0446},
    {getStringID(LTITLE, TITLE_STR_210_DWAYNE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Dwayne, 1.0446},
    {getStringID(LTITLE, TITLE_STR_211_DES), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Des, 1.0446},
    {getStringID(LTITLE, TITLE_STR_212_CHRIS), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Chris, 1.0446},
    {getStringID(LTITLE, TITLE_STR_213_LEE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Lee, 1.0446},
    {getStringID(LTITLE, TITLE_STR_214_NEIL), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Neil, 1.0446},
    {getStringID(LTITLE, TITLE_STR_215_JIM), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Jim, 1.0446},
    {getStringID(LTITLE, TITLE_STR_216_ROBIN), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Robin, 1.0446},
    {getStringID(LTITLE, TITLE_STR_217_STEVEH), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Steve_H, 1.0446},
    {getStringID(LTITLE, TITLE_STR_218_TERRORIST), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Terrorist, 1.0446},
    {getStringID(LTITLE, TITLE_STR_219_BIKER), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Biker, 1.0446},
    {getStringID(LTITLE, TITLE_STR_220_JOEL), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Joel, 1.0446},
    {getStringID(LTITLE, TITLE_STR_221_SCOTT), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Scott, 1.0446},
    {getStringID(LTITLE, TITLE_STR_222_JOE), MALE,   IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Male_Joe, 1.0446},
    {getStringID(LTITLE, TITLE_STR_223_SALLY), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Female_Sally, 1.0446},
    {getStringID(LTITLE, TITLE_STR_224_MARION), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Female_Marion_Rosika, 1.0446},
    {getStringID(LTITLE, TITLE_STR_225_MANDY), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Female_Mandy, 1.0446},
    {getStringID(LTITLE, TITLE_STR_226_VIVIEN), FEMALE, IMG_MPC_RANDOM, BODY_Brosnan_Tuxedo,                   HEAD_Female_Vivien, 1.0446}
};
#endif


struct MP_handicap_menu g_MpHandicaps[] = {
    {getStringID(LTITLE, TITLE_STR_61_HERO), 0, 10.0},       // "Health -10\n(Hero)"
    {getStringID(LTITLE, TITLE_STR_62_VET4), 0, 2.8560996},  // "Health -4\n(Veteran)"
    {getStringID(LTITLE, TITLE_STR_63_VET3), 0, 2.1969998},  // "Health -3\n(Veteran)"
    {getStringID(LTITLE, TITLE_STR_64_VET2), 0, 1.6899998},  // "Health -2\n(Veteran)"
    {getStringID(LTITLE, TITLE_STR_65_VET1), 0, 1.3},        // "Health -1\n(Veteran)"
    {getStringID(LTITLE, TITLE_STR_66_HEALTH_NORMAL), 0, 1.0},        // "Health +0\n(Normal)"
    {getStringID(LTITLE, TITLE_STR_67_NOVICE1), 0, 0.76923078}, // "Health +1\n(Novice)"
    {getStringID(LTITLE, TITLE_STR_68_NOVICE2), 0, 0.59171599}, // "Health +2\n(Novice)"
    {getStringID(LTITLE, TITLE_STR_69_NOVICE3), 0, 0.45516616}, // "Health +3\n(Novice)"
    {getStringID(LTITLE, TITLE_STR_70_NOVICE4), 0, 0.35012782}, // "Health +4\n(Novice)"
    {getStringID(LTITLE, TITLE_STR_71_ROOKIE), 0, 0.1}         // "Health +10\n(Rookie)"
};

struct MP_controller_configuration_menu g_MpControlStyles[] = {
    {getStringID(LTITLE, TITLE_STR_277_11HONEY), 0, 1}, //1.1 Honey*
    {getStringID(LTITLE, TITLE_STR_278_12SOLITAIRE), 1, 1}, //1.2 Solitaire*
    {getStringID(LTITLE, TITLE_STR_279_13KISSY), 2, 1}, //1.3 Kissy*
    {getStringID(LTITLE, TITLE_STR_280_14GOODNIGHT), 3, 1}, //1.4 Goodnight*
    {getStringID(LTITLE, TITLE_STR_281_21PLENTY), 4, 2}, //2.1 Plenty*
    {getStringID(LTITLE, TITLE_STR_282_22GALORE), 5, 2}, //2.2 Galore*
    {getStringID(LTITLE, TITLE_STR_283_23DOMINO), 6, 2}, //2.3 Domino*
    {getStringID(LTITLE, TITLE_STR_284_24GOODHEAD), 7, 2}  //2.4 Goodhead*
};

struct MP_sight_aim_settings g_MpAimSettings[] = {
    {getStringID(LTITLE, TITLE_STR_72_SOFFAOFF), 0, 0}, // "Sight OFF, Auto Aim OFF"
    {getStringID(LTITLE, TITLE_STR_73_SONAOFF), 1, 0}, // "Sight ON, Auto Aim OFF"
    {getStringID(LTITLE, TITLE_STR_74_SOFFAON), 0, 1}, // "Sight OFF, Auto Aim ON"
    {getStringID(LTITLE, TITLE_STR_75_SONAON), 1, 1}  // "Sight ON, Auto Aim ON"
};

s32 g_MpPlayerCount = 0;
s32 g_MpChar[MAX_PLAYER_COUNT] = { -1, -1, -1, -1 };
s32 g_MpStage = MP_STAGE_TEMPLE;
s32 g_MpGameLength = LEN_10MIN; //10minutes
s32 g_MpAimSetting = 3; //"Sight ON, Auto Aim ON"
s32 g_MpScenario = SCENARIO_NORMAL;

s32 g_MpCanPickStage = TRUE;
s32 g_MpCanPickLength = TRUE;
s32 g_MpCanPickChar = TRUE;
s32 g_MpCanPickWeapons = TRUE;
s32 g_MpCanPickHandicap = TRUE;
s32 g_MpCanPickControls = TRUE;
s32 g_MpCanPickAim = TRUE;

u32 g_MpTeamsBlinkTimer = 0;

s16 g_FrontTargetTimes[20][3] = {
    {0, 160, 0}, // Dam
    {0, 0, 125}, // Facility
    {300, 0, 0}, // Runway
    {0, 210, 0}, // Surface 1
    {0, 0, 240}, // Bunker 1
    {180, 0, 0}, // Silo
    {0, 270, 0}, // Frigate
    {0, 0, 255}, // Surface 2
    {90, 0, 0},  // Bunker 2
    {0, 195, 0}, // Statue
    {0, 0, 80},  // Archives
    {105, 0, 0}, // Streets
    {0, 100, 0}, // Depot
    {0, 0, 325}, // Train
    {225, 0, 0}, // Jungle
    {0, 600, 0}, // Control
    {0, 0, 570}, // Caverns
    {135, 0, 0}, // Cradle
    {0, 540, 0}, // Aztec
    {0, 0, 360}  // Egyptian
};

s32 g_CheatMenuCount = 0;
s32 g_CheatMenuCursor = 0;


u32 g_CheatMenuToggle = 0;
s32 g_CastIndex = 0;
u32 g_CastAnim = ANIM_idle;
u32 g_CastAnimCount = 0;
struct Model *g_CastModel = NULL;
struct Model *g_CastWeaponModel = NULL;
u32 g_CastFull = 0;

struct intro_char g_CastChars[] = {
    {BODY_Brosnan_Tuxedo,             HEAD_Male_Brosnan_Tuxedo, getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_228_THEACTORS),             getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Special_Operations_Uniform, HEAD_Male_Brosnan_Boiler, getStringID(LTITLE, TITLE_STR_229_STARRING),      getStringID(LTITLE, TITLE_STR_232_007),                   getStringID(LTITLE, TITLE_STR_233_JAMESBOND),        0, 0},
    {BODY_Natalya_Skirt,              HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_229_STARRING),      getStringID(LTITLE, TITLE_STR_234_NATALYASIMONOVA),       getStringID(LTITLE, TITLE_STR_227_LF),               0, 0},
    {BODY_Trevelyan_006,              HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_229_STARRING),      getStringID(LTITLE, TITLE_STR_235_006),                   getStringID(LTITLE, TITLE_STR_236_ALECTREVELYAN),    0, 0},
    {BODY_Xenia,                      HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_230_ALSOFEATURING), getStringID(LTITLE, TITLE_STR_237_JANUSOPPERATIVE),       getStringID(LTITLE, TITLE_STR_238_XENIAONPTOPP),     0, 0},
    {BODY_Ourumov,                    HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_230_ALSOFEATURING), getStringID(LTITLE, TITLE_STR_239_GENERAL),               getStringID(LTITLE, TITLE_STR_240_ARKADYOURUMOV),    0, 0},
    {BODY_Boris,                      HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_230_ALSOFEATURING), getStringID(LTITLE, TITLE_STR_241_BORISGRISHENKO),        getStringID(LTITLE, TITLE_STR_227_LF),               0, 0},
    {BODY_Valentin_,                  HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_230_ALSOFEATURING), getStringID(LTITLE, TITLE_STR_242_EXKGBAGENT),            getStringID(LTITLE, TITLE_STR_243_VELENTINZUKOVSKY), 0, 0},
    {BODY_Siberian_Guard_1_Mishkin,   HEAD_Male_Mishkin,        getStringID(LTITLE, TITLE_STR_230_ALSOFEATURING), getStringID(LTITLE, TITLE_STR_244_DEFENSEMINISTER),       getStringID(LTITLE, TITLE_STR_245_DIMITRIMISHKIN),   0, 0},
    {BODY_Russian_Soldier,            HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_253_RUSSIANSOLDIER),        getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Russian_Infantry,           HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_252_RUSSIANINFANTRY),       getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Scientist_1_Male,           HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_263_SCIENTIST),             getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Scientist_2_Female,         HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_263_SCIENTIST),             getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Russian_Commandant,         HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_256_RUSSIANCOMMANDANT),     getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Janus_Marine,               HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_254_JANUSMARINE),           getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Naval_Officer,              HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_257_NAVALOFFICER),          getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Helicopter_Pilot,           HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_262_HELICOPTERPILOT),       getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_St_Petersburg_Guard,        HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_251_STPETERSBURGGUARD),     getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Civilian_1_Female,          HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_264_CIVILIAN),              getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Civilian_2,                 HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_264_CIVILIAN),              getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Civilian_3,                 HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_264_CIVILIAN),              getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Civilian_4,                 HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_264_CIVILIAN),              getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Siberian_Guard_1_Mishkin,   HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_258_SIBERIANGUARD),         getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Arctic_Commando,            HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_259_ARCTICCOMMANDO),        getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Siberian_Guard_2,           HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_258_SIBERIANGUARD),         getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Siberian_Special_Forces,    HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_260_SIBERIANSPECIALFORCES), getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Jungle_Commando,            HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_250_JUNGLECOMMANDO),        getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Janus_Special_Forces,       HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_255_JANUSSPECIALFORCES),    getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Moonraker_Elite_1_Male,     HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_261_MOOKRAKERELITE),        getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Moonraker_Elite_2_Female,   HEAD_RANDOM,              getStringID(LTITLE, TITLE_STR_227_LF),            getStringID(LTITLE, TITLE_STR_261_MOOKRAKERELITE),        getStringID(LTITLE, TITLE_STR_227_LF),               0, 1},
    {BODY_Mayday,                     HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_231_GUESTSTAR),     getStringID(LTITLE, TITLE_STR_246_MAYDAY),                getStringID(LTITLE, TITLE_STR_227_LF),               0, 0},
    {BODY_Jaws,                       HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_231_GUESTSTAR),     getStringID(LTITLE, TITLE_STR_247_JAWS),                  getStringID(LTITLE, TITLE_STR_227_LF),               0, 0},
    {BODY_Oddjob,                     HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_231_GUESTSTAR),     getStringID(LTITLE, TITLE_STR_248_ODDJOB),                getStringID(LTITLE, TITLE_STR_227_LF),               0, 0},
    {BODY_Baron_Samedi,               HEAD_FIXED,               getStringID(LTITLE, TITLE_STR_231_GUESTSTAR),     getStringID(LTITLE, TITLE_STR_249_BERONSAMEDI),           getStringID(LTITLE, TITLE_STR_227_LF),               0, 0},
    {0xFFFFFFFF,                      0,                        0,                                                0,                                                        0,                                                   0, 0}
};

struct intro_animation g_CastAnims[] = {
    {ANIM_spotting_bond, 98.0, 1.0, 0},
    {ANIM_fire_standing_draw_one_handed_weapon_fast, 21.0, 1.0, 1},
    {ANIM_fire_standing_draw_one_handed_weapon_slow, 26.0, 1.0, 1},
    {ANIM_fire_step_right_one_handed_weapon, 0.0, 1.0, 1},
    {ANIM_fire_kneel_forward_one_handed_weapon_fast, 0.0, 1.0, 1},
    {ANIM_running_one_handed_weapon, 0.0, 0.91000003, 1},
    {ANIM_draw_one_handed_weapon_and_stand_up, 31.0, 1.0, 1},
    {ANIM_aim_one_handed_weapon_left_right, 0.0, 1.0, 1},
    {ANIM_cock_one_handed_weapon_and_turn_around, 0.0, 1.0, 1},
    {ANIM_cock_one_handed_weapon_turn_around_and_stand_up, 0.0, 1.0, 1},
    {ANIM_draw_one_handed_weapon_and_turn_around, 0.0, 1.0, 1},
    {ANIM_drop_weapon_and_show_fight_stance, 248.0, 1.0, 0},
    {ANIM_laughing_in_disbelief, 150.0, 1.0, 0},
    {ANIM_fire_hip_forward_one_handed_weapon, 0.0, 0.89999998, 1},
    {ANIM_fire_standing_left_one_handed_weapon_fast, 0.0, 0.89999998, 1},
    {ANIM_fire_kneel_left_one_handed_weapon_fast, 0.0, 0.89999998, 1},
    {ANIM_draw_one_handed_weapon_and_look_around, 51.0, 1.0, 1},
    {ANIM_aim_one_handed_weapon_left, 0.0, 0.89999998, 1},
    {ANIM_aim_one_handed_weapon_right, 0.0, 0.89999998, 1},
    {ANIM_conversation, 37.0, 1.0, 2},
    {ANIM_conversation_listener, 300.0, 1.0, 2},
    {ANIM_conversation_cleaned, 120.0, 1.0, 2},
    {0xFFFFFFFF, 0.0, 0.0, 0}
};

struct coord3d zeroCoord = { 0 };

#define INTRO_RANDOM_RIFLE_LEN 6
struct intro_random_rifles
{
    PROP props[INTRO_RANDOM_RIFLE_LEN];
};

struct intro_random_rifles g_CastRifles = {
    PROP_CHRKALASH,
    PROP_CHRM16,
    PROP_CHRFNP90,
    PROP_CHRAUTOSHOT,
    PROP_CHRGRENADELAUNCH,
    PROP_CHRSNIPERRIFLE
};

#define INTRO_RANDOM_PISTO_LEN 10
struct intro_random_pistols
{
    PROP props[INTRO_RANDOM_PISTO_LEN];
};

struct intro_random_pistols g_CastPistols = {
    PROP_CHRWPPK,
    PROP_CHRWPPKSIL,
    PROP_CHRSKORPION,
    PROP_CHRUZI,
    PROP_CHRUZI,
    PROP_CHRTT33,
    PROP_CHRTT33,
    PROP_CHRRUGER,
    PROP_CHRLASER,
    PROP_CHRGOLDEN
};

struct coord3d cast_camera_pos    = { 0.0f, 0.0f, 1.0f }; // campos in frontCastRender is set to this, but is then immediately overwritten, so the values here have no effect.
struct coord3d cast_camera_target = { 0.0f, 0.0f, 0.0f };
struct coord3d cast_camera_up     = { 0.0f, 1.0f, 0.0f }; // Display cast character up vector

ModelRenderData cast_renderdata_default = { NULL,
                                            TRUE,
                                            0x00000003,
                                            NULL,

                                            NULL,
                                            0,
                                            0,
                                            0,

                                            0,
                                            0,
                                            0,
                                            0,

                                            0,
                                           {0, 0, 0, 0},
                                           {0, 0, 0, 0},
                                           CULLMODE_BOTH};


struct coord3d cast_camera_offset = { 0.0f, 40.0f, 25.0f }; // vec in frontCastRender is initialized to this, but then gets overwritten before it's read, so these values don't have any effect.


// forward declarations

Gfx* frontAddPreviousTabText(Gfx* DL);
s32 frontIsMissionCompleted(int stage, DIFFICULTY difficulty);
s32 frontGetHighestDifficulty(s32 mission);
Gfx *frontLegalRender(Gfx *DL);
Gfx *frontNintendoRender(Gfx *DL);
Gfx *frontRareRender(Gfx *DL);
Gfx *frontGunbarrelRender(Gfx *DL);
Gfx *frontLogoRender(Gfx *DL);
Gfx *frontFileSelectRender(Gfx *DL);
Gfx *frontModeSelectRender(Gfx *DL);
Gfx *frontMissionSelectRender(Gfx *DL);
Gfx *frontDifficultyRender(Gfx *DL);
Gfx *frontOptions007Render(Gfx *DL);
Gfx *frontBriefingRender(Gfx *DL);
Gfx *frontDebriefRender(Gfx *DL);
Gfx *frontStatsRender(Gfx *DL);
Gfx *frontMpOptionsRender(Gfx *DL);
Gfx *frontMpCharRender(Gfx *DL);
Gfx *frontMpHandicapRender(Gfx *DL);
Gfx *frontMpControlsRender(Gfx *DL);
Gfx *frontMpStageRender(Gfx *DL);
Gfx *frontMpScenarioRender(Gfx *DL);
Gfx *frontMpTeamsRender(Gfx *DL);
Gfx *frontCheatRender(Gfx *DL);
Gfx *frontNoControllerRender(Gfx *DL);
Gfx *frontBlankRender(Gfx *DL);
Gfx *frontCastRender(Gfx *DL);
void frontHideAllSwitches(Model *model);
void frontSetSwitchVisible(Model* objinstance, s32 item, s32 mode);
void frontSetMissionCursor(LEVEL_SOLO_SEQUENCE level);
Gfx *frontDrawAlignedText(Gfx *dl, s32 x0, s32 y0, s32 halign, s32 valign, char *text, struct fontchar *fontChars, struct font *font);
void frontSetModeCursor(int mode);
void frontSetDifficultyCursor(int difficulty);

// end forward declarations.


Gfx *frontPrintText(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *fontChars, struct font *font, s32 colour, s32 width, s32 height, s32 yOffset, s32 lineheight)
{
    gdl = textRender(
        gdl,
        x,
        y,
        text,
        fontChars,
        font,
        colour,
        width,
        height,
        yOffset,
        lineheight);

    return gdl;
}


s32 frontIsCheatUnlocked(s32 cheat)
{
    switch(cheat)
    {
        case CHEAT_EXTRA_MP_CHARS:
        case CHEAT_MAXAMMO:
        case CHEAT_DEBUG_UNK5:
        case CHEAT_DEACTIVATE_INVINCIBILITY:
        case CHEAT_LINEMODE:
        case CHEAT_2X_HEALTH:
        case CHEAT_2X_ARMOR:
        case CHEAT_EXTRA_WEAPONS:
        case CHEAT_10X_HEALTH:
        case CHEAT_BONDPHASE:
        case CHEAT_DEBUG_POS:
        case CHEAT_UNLOCK_PAINTBALL:
        case CHEAT_UNLOCK_INVINCIBLE:
        case CHEAT_UNLOCK_DKMODE:
        case CHEAT_UNLOCK_2XGL:
        case CHEAT_UNLOCK_2XRL:
        case CHEAT_UNLOCK_TURBO:
        case CHEAT_UNLOCK_NORADAR:
        case CHEAT_UNLOCK_TINY:
        case CHEAT_UNLOCK_2XTKNIFE:
        case CHEAT_UNLOCK_FAST:
        case CHEAT_UNLOCK_INVIS:
        case CHEAT_UNLOCK_ENEMYRL:
        case CHEAT_UNLOCK_SLOW:
        case CHEAT_UNLOCK_SILVERPPK:
        case CHEAT_UNLOCK_2XHKNIFE:
        case CHEAT_UNLOCK_INFAMMO:
        case CHEAT_UNLOCK_2XFNP0:
        case CHEAT_UNLOCK_GOLDPPK:
        case CHEAT_UNLOCK_2XLASER:
        case CHEAT_UNLOCK_ALLGUNS:
        case CHEAT_UNLOCK_DAM:
        case CHEAT_UNLOCK_FACILITY:
        case CHEAT_UNLOCK_RUNWAY:
        case CHEAT_UNLOCK_SURFACE:
        case CHEAT_UNLOCK_BUNKER:
        case CHEAT_UNLOCK_SILO:
        case CHEAT_UNLOCK_FRIGATE:
        case CHEAT_UNLOCK_SURFACE2:
        case CHEAT_UNLOCK_BUNKER2:
        case CHEAT_UNLOCK_STATUE:
        case CHEAT_UNLOCK_ARCHIVES:
        case CHEAT_UNLOCK_STREETS:
        case CHEAT_UNLOCK_DEPOT:
        case CHEAT_UNLOCK_TRAIN:
        case CHEAT_UNLOCK_JUNGLE:
        case CHEAT_UNLOCK_CONTROL:
        case CHEAT_UNLOCK_CAVERNS:
        case CHEAT_UNLOCK_CRADLE:
        case CHEAT_UNLOCK_AZTEK:
        case CHEAT_UNLOCK_EGYPT:
            return 0;

        case CHEAT_INVINCIBILITY:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_FACILITY);

        case CHEAT_ALLGUNS:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_EGYPT);

        case CHEAT_NO_RADAR_MP:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_FRIGATE);

        case CHEAT_FAST_ANIMATION:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_STATUE);

        case CHEAT_INVISIBILITY:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_ARCHIVES);

        case CHEAT_INFINITE_AMMO:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_CONTROL);

        case CHEAT_DK_MODE:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_RUNWAY);

        case CHEAT_TINY_BOND:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_SURFACE2);

        case CHEAT_SLOW_ANIMATION:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_DEPOT);

        case CHEAT_PAINTBALL:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_DAM);

        case CHEAT_SILVER_PP7:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_TRAIN);

        case CHEAT_GOLD_PP7:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_CRADLE);

        case CHEAT_ENEMY_ROCKETS:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_STREETS);

        case CHEAT_2X_ROCKET_LAUNCHER:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_BUNKER1);

        case CHEAT_2X_GRENADE_LAUNCHER:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_SURFACE1);

        case CHEAT_2X_RCP90:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_CAVERNS);

        case CHEAT_2X_THROWING_KNIFE:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_BUNKER2);

        case CHEAT_2X_HUNTING_KNIFE:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_JUNGLE);

        case CHEAT_2X_LASER:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_AZTEC);

        case CHEAT_TURBO_MODE:
            return fileGetIsCheatUnlocked( fileGetSaveForFoldernum(g_FrontFolder), SP_LEVEL_SILO);

        case CHEAT_MAGNUM:
            return fileIsCradleCompletedForFolder(g_FrontFolder);

        case CHEAT_LASER:
            return fileIsAztecCompletedOnSecretOr00ForFolder(g_FrontFolder);

        case CHEAT_GOLDEN_GUN:
            return fileIsEgyptCompletedOn00ForFolder(g_FrontFolder);

        default:
            do {/* WARNING: Do nothing block with infinite loop */
            } while( 1 );
        }
}


int frontGetFavWeaponText(int player,int hand) {
    return itemGetChoiceText(g_MpFavWeapon[player][hand]);
}


void frontMoveCursor(void) {
    s8 stickx = joyGetStickX(PLAYER_1);
    s8 sticky = -joyGetStickY(PLAYER_1);


    if (stickx < -5) {
        stickx = (stickx + 5);
    }
    else  if (stickx >= 6) {
        stickx = (stickx - 5);
    }
    else {
        stickx = 0;
    }

    if (stickx >= 71) {
        stickx = 70;
    }
    else {
        if (stickx < -70) {
            stickx = -70;
        }
    }

    if (sticky < -5) {
        sticky = (sticky + 5);
    }
    else  if (sticky >= 6) {
        sticky = (sticky - 5);
    }
    else {
        sticky = 0;
    }

    if (sticky >= 71) {
        sticky = 70;
    }
    else {
        if (sticky < -70) {
            sticky = -70;
        }
    }

    if (stickx > 0) {
        g_FrontCursorX += (f32)(( (f32)stickx * 0.075f + 0.5f) * g_GlobalTimerDelta);
    }
    else if (stickx < 0) {
        g_FrontCursorX += (f32)(( (f32)stickx * 0.075f - 0.5f) * g_GlobalTimerDelta);
    }

    if ((camGetScreenLeft() + camGetScreenWidth() - 20.0f) < g_FrontCursorX) {
        g_FrontCursorX = (camGetScreenLeft() + camGetScreenWidth() - 20.0f);
    }
    else {
        if (g_FrontCursorX < camGetScreenLeft() + 20.0f) {
            g_FrontCursorX = camGetScreenLeft() + 20.0f;
        }
    }

    if (sticky > 0) {
        g_FrontCursorY += (f32)(( (f32)sticky * 0.075f + 0.5f) * g_GlobalTimerDelta);
    }
    else if (sticky < 0) {
        g_FrontCursorY += (f32)(( (f32)sticky * 0.075f - 0.5f) * g_GlobalTimerDelta);
    }

    if ((camGetScreenTop() + camGetScreenHeight() - 20.0f) < g_FrontCursorY) {
        g_FrontCursorY = (camGetScreenTop() + camGetScreenHeight() - 20.0f);
        return;
    }
    if (g_FrontCursorY < (camGetScreenTop() + 20.0f)) {
        g_FrontCursorY = (camGetScreenTop() + 20.0f);
    }
}


Gfx *frontDrawCursor(Gfx *DL)
{
    f32 xypos[2];
    f32 halfedxy[2];
    sImageTableEntry *image;
    s32 option;

    option = g_FrontFolderIcon;
    if (option == 0) {
        image = g_ImageCrosshair;
    }
    else if (option == 1) {
        image = g_ImagesMainFolder + IMG_COPY; //COPY
    }
    else if (option == 2) {
        image = g_ImagesMainFolder + IMG_DEL;
    }

    texSelect(&DL, image, 4, 0, 0);

    xypos[0] = mathFloor(g_FrontCursorX + 0.5f);
    xypos[1] = mathFloor(g_FrontCursorY + 0.5f);

    halfedxy[0] = image->width * 0.5f;
    halfedxy[1] = image->height * 0.5f;

    texRectDrawImage(&DL, &xypos, &halfedxy, image->width, image->height, 0, 0, 1, 255, 255, 255, 220, (image->level > 0), 0);

    return DL;
}


//********************************************************************************************************
// TABS
//********************************************************************************************************

Gfx* frontAddStartTabText(Gfx* DL)
{
    s32 v;
    s32 h;

    s32 textWidth;
    s32 textHeight;

    g_FrontTabStartText = langGet(getStringID(LTITLE, TITLE_STR_04_START));
    textSetSpacing(TRUE);
    textHeight = 0;
    textWidth = 0;
    //text is sideways so textWidth and textHeight are flipped
    textMeasure(&textWidth, &textHeight, g_FrontTabStartText, g_FontBankGothicChars, g_FontBankGothic, 0);
    v = STARTTAB_TEXT_TOP;
    h = TABS_RIGHT_EDGE - (textWidth / 2);

    if (g_FrontTabStartHighlight)
    {
        DL = textRenderRect(DL, (h - textWidth) + 1, STARTTAB_TEXT_TOP, h, STARTTAB_TEXT_BOTTOM, 50);
    }

    textSetOrientation(ROT_90CW);

    v = STARTTAB_TEXT_HALF - (textHeight / 2);
    DL = textRender(DL, &v, &h, g_FrontTabStartText, g_FontBankGothicChars, g_FontBankGothic, 0xFF, viGetY(), viGetX(), 0, 0);
    textSetOrientation(ROT_NORMAL);
    textSetSpacing(FALSE);
    return DL;
}


s32 frontCheckCursorOnStartTab(void)
{
    if ((TABS_LEFT_EDGE < g_FrontCursorX) && (g_FrontCursorY <= STARTTAB_TAB_BOTTOM))
    {
        return TRUE;
    }

    return FALSE;
}


Gfx* frontAddPreviousTabText(Gfx* DL)
{
    s32 v;
    s32 h;

    s32 textWidth;
    s32 textHeight;

    g_FrontTabPrevText = langGet(getStringID(LTITLE, TITLE_STR_06_PREVIOUS));
    textSetSpacing(TRUE);
    textHeight = 0;
    textWidth = 0;
    //text is sideways so textWidth and textHeight are flipped
    textMeasure(&textWidth, &textHeight, g_FrontTabPrevText, g_FontBankGothicChars, g_FontBankGothic, 0);
    v = PREVTAB_TEXT_TOP;
    h = TABS_RIGHT_EDGE - (textWidth / 2);

    if (g_FrontTabPrevHighlight != 0)
    {
        DL = textRenderRect(DL, (h - textWidth) + 1, PREVTAB_TEXT_TOP, h, PREVTAB_TEXT_BOTTOM, 50);
    }

    textSetOrientation(ROT_90CW);

    v = PREVTAB_TEXT_HALF - (textHeight / 2);
    DL = textRender(DL, &v, &h, g_FrontTabPrevText, g_FontBankGothicChars, g_FontBankGothic, 0xFF, viGetY(), viGetX(), 0, 0);
    textSetOrientation(ROT_NORMAL);
    textSetSpacing(FALSE);

    return DL;
}


u32 frontCheckCursorOnPreviousTab(void)
{
    if ((TABS_LEFT_EDGE < g_FrontCursorX) && (PREVTAB_TAB_TOP < g_FrontCursorY))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void frontSetCursorPositionToNextTab(void)
{
    g_FrontCursorX = NEXTTAB_CURSOR_HPOS;
    g_FrontCursorY = NEXTTAB_CURSOR_VPOS;
}


Gfx* frontAddNextTabText(Gfx* DL)
{
    s32 v;
    s32 h;

    s32 textWidth;
    s32 textHeight;

    g_FrontTabNextText = langGet(getStringID(LTITLE, TITLE_STR_05_NEXT)); //"NEXT\n"
    textSetSpacing(TRUE);
    textHeight = 0;
    textWidth = 0;
    //text is sideways so textWidth and textHeight are flipped
    textMeasure(&textWidth, &textHeight, g_FrontTabNextText, g_FontBankGothicChars, g_FontBankGothic, 0);
    v = NEXTTAB_TEXT_TOP;
    h = TABS_RIGHT_EDGE - (textWidth / 2);

    if (g_FrontTabNextHighlight != 0)
    {
        DL = textRenderRect(DL, (h - textWidth) + 1, NEXTTAB_TEXT_TOP, h, NEXTTAB_TEXT_BOTTOM, 50);
    }

    textSetOrientation(ROT_90CW);

    v = NEXTTAB_TEXT_HALF - (textHeight / 2);
    DL = textRender(DL, &v, &h, g_FrontTabNextText, g_FontBankGothicChars, g_FontBankGothic, 0xFF, viGetY(), viGetX(), 0, 0);
    textSetOrientation(ROT_NORMAL);
    textSetSpacing(FALSE);
    return DL;
}


u32 frontCheckCursorOnNextTab(void)
{
    if (((TABS_LEFT_EDGE < g_FrontCursorX) && (NEXTTAB_TAB_TOP < g_FrontCursorY)) && (g_FrontCursorY <= PREVTAB_TAB_TOP))
    {
        return TRUE;
    }

    return FALSE;
}


//********************************************************************************************************
//BEGIN MENU SECTIONS
//********************************************************************************************************

//********************************************************************************************************
//LEGAL SCREEN
//********************************************************************************************************
void frontLegalEnter(void)
{
    struct coord3d pos;

    musicTrack1Stop();
    g_FrontStartMusic = TRUE;
    g_FrontTimer = 0;
    pos = legalpage_pos;
    modelFileLoad(PitemZ_entries[PROP_LEGALPAGE].header, PitemZ_entries[PROP_LEGALPAGE].filename, g_FrontModelBuffer, 0x3c000, 0);
    modelCalculateRwDataLen(PitemZ_entries[PROP_LEGALPAGE].header);

    g_FrontLogoModel = modelmgrInstantiateModel(PitemZ_entries[PROP_LEGALPAGE].header);
    modelSetScale(g_FrontLogoModel, 1.0f);
    modelSetRootPosition(g_FrontLogoModel, &pos);
    fileValidateSaves();
}


void frontLegalLeave(void)
{
    g_FrontFirstLegal = FALSE;
    modelFreeSlot(g_FrontLogoModel);
}


void frontLegalTick(void)
{
#ifdef REFRESH_PAL
#define MENU_LEGALSCREEN_MENU_TIMER_MAX (50*4+1)
#else
#define MENU_LEGALSCREEN_MENU_TIMER_MAX (60*4+1)
#endif

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);
    g_FrontTimer += g_ClockTimer;

    if (g_FrontTimer >= MENU_LEGALSCREEN_MENU_TIMER_MAX)
    {
        if ((joyGetControllerCount() < 1) && (g_FrontFirstLegal))
        {
            frontSetMenu(MENU_NO_CONTROLLERS, TRUE);
        }
        else
        {
            frontSetMenu(MENU_NINTENDO_LOGO, TRUE);
        }
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON) && (!g_FrontFirstLegal))
    {
        if (!g_FrontFirstBoot)
        {
            frontSetMenu(MENU_FILE_SELECT, TRUE);
        }
        else
        {
            frontSetMenu(MENU_NINTENDO_LOGO, TRUE);
        }
    }
}


/**
 * Render a string of standard text on the legal screen e.g. "TWYCROSS BOARD OF GAME CLASSIFICATION"
 */
Gfx *frontDrawAlignedText(Gfx *dl, s32 x0, s32 y0, s32 halign, s32 valign, char *text, struct fontchar *fontChars, struct font *font)
{
    s32 height;
    s32 width;
    s32 x;
    s32 y;

    width = 0;
    height = 0;
    textMeasure(&height, &width, text, g_FontZurichChars, g_FontZurich, 0);
    x = x0 - ((s32) (halign * width) / 2);
    y = y0 - ((s32) (valign * height) / 2);

    return textRender(dl, &x, &y, text, fontChars, font, -1, viGetX(), viGetY(), 0, 0);
}


Gfx *frontLegalRender(Gfx *DL)
{
    struct legal_screen_text *legal_text_end;
    struct ModelRenderData renderdata;
    Mtxf lookatmtx;
    s32 i;
    Mtxf tmpmtx;
    struct legal_screen_text *legal_text_ptr;

    renderdata = legalscreen_MRD;
    
    DL = screenClearBlack(DL);
    
    /* the original reads through legal_text_ptr before it is set; the result is always 0 */
    mtx4LoadLookAt(&lookatmtx, 0.0f, 0.0f, 4000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    
    renderdata.basemtx = &lookatmtx;
    renderdata.mtxlist = (Mtxf *) dynAllocate(g_FrontLogoModel->obj->numMatrices << 6);
    
    mtx4Copy(&lookatmtx, renderdata.mtxlist);
    
    g_FrontLogoModel->render_pos = (union RenderPosView *) renderdata.mtxlist;
    
    modelUpdateNodeRelations(g_FrontLogoModel);
    
    renderdata.flags = 3;
    renderdata.zbufferenabled = 0;
    renderdata.gdl = DL;
    
    modelRender(&renderdata, g_FrontLogoModel);
    
    DL = renderdata.gdl;
    
    for (i = 0; i < g_FrontLogoModel->obj->numMatrices; i++)
    {
        mtx4Copy((Mtxf *)&((s8 *)g_FrontLogoModel->render_pos)[i * 0x40], &tmpmtx);
        mtxF2L(&tmpmtx, &((Mtxf *) g_FrontLogoModel->render_pos)[i]);
    }
    
    DL = textSetupDraw(DL);
    
    legal_text_ptr = g_LegalTexts;
    legal_text_end = &g_LegalTexts[ARRAYCOUNT(g_LegalTexts)];
    
    do
    {
        DL = frontDrawAlignedText(DL, legal_text_ptr->h_pos, legal_text_ptr->v_pos, legal_text_ptr->halign, legal_text_ptr->valign, langGet(legal_text_ptr->txtID), g_FontZurichChars, g_FontZurich);
        legal_text_ptr++;
    }
    while (legal_text_ptr < legal_text_end);
    
    return DL;
}


void frontResetTimer(void) {
    g_FrontTimer = 0;
}


//********************************************************************************************************
//SWITCH SCREEN
//********************************************************************************************************
void frontBlankTick(void)
{
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    g_FrontTimer++;
    if ((g_FrontTimer >= 4) && (!g_FrontHiResPending) && (!g_FrontLowResPending))
    {
        g_FrontEnterMenu = g_FrontNextMenu;
        g_FrontNextMenu = MENU_INVALID;
    }
}


Gfx * frontBlankRender(Gfx * DL)
{
    return screenClearBlack(DL);
}


//********************************************************************************************************
//NINTENDO LOGO
//********************************************************************************************************
void frontNintendoEnter(void)
{
    struct coord3d pos;

    g_FrontTimer = 0;
    pos = nintendologo_pos;
    modelFileLoad(PitemZ_entries[PROP_NINTENDOLOGO].header, PitemZ_entries[PROP_NINTENDOLOGO].filename, g_FrontModelBuffer, 0x3c000, 0);
    modelCalculateRwDataLen(PitemZ_entries[PROP_NINTENDOLOGO].header);
    g_FrontLogoModel = modelmgrInstantiateModel(PitemZ_entries[PROP_NINTENDOLOGO].header);
    modelSetScale(g_FrontLogoModel, 1.0f);
    modelSetRootPosition(g_FrontLogoModel, &pos);
    musicTrack1Play(M_INTROSWOOSH);
    g_FrontStartMusic = TRUE;
    g_NintendoLogoSpin = -1.39626348019f;
    g_NintendoLogoScale = 0.0183333326131f;
}


void frontNintendoLeave(void)
{
    modelFreeSlot(g_FrontLogoModel);
}


void frontNintendoTick(void)
{
#ifdef REFRESH_PAL
/*~= 8.32 * 50 + 1*/
#define NINTENDO_TIMER_MAX 417
#else
/*~= 8.32 * 60 + 1*/
#define NINTENDO_TIMER_MAX 501
#endif

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    g_FrontTimer += g_ClockTimer;

    if (g_FrontTimer >= NINTENDO_TIMER_MAX)
    {
        frontSetMenu(MENU_RAREWARE_LOGO, TRUE);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON))
    {
        if (!g_FrontFirstBoot)
        {
            frontSetMenu(MENU_FILE_SELECT, TRUE);
        }
        else
        {
            g_FrontSkipIntro = TRUE;
            frontSetMenu(MENU_RAREWARE_LOGO, TRUE);
        }
    }
}


Gfx *frontNintendoRender(Gfx *DL)
{
    ModelRenderData ninlogo;
    s32 ambiantlight;
    Mtxf ninlogoMtxf;
    s32 i;

    ninlogo = nintendologo_MRD;

    DL = screenClearBlack(DL);

    // Lights macro? These need to be on one line.
    gSPNumLights(DL++, 1); \
    gSPLight(DL++, &g_FrontNintendoLight.l[0], 1); \
    gSPLight(DL++, &g_FrontNintendoLight, 2);

#if defined(VERSION_EU)
    // 0x100000000 ? 0xFFFECD34 = 0x132CC (78540 decimal)
    ambiantlight = 0xFF - ((s32) ((g_FrontTimer * 0xFF) + 0xFFFECD34) / 83);
#else
    // 0x100000000 ? 0xFFFE8F72 = 0x1708E (94350 decimal)
    ambiantlight = 0xFF - ((s32) ((g_FrontTimer * 0xFF) + 0xFFFE8F72) / 100);
#endif
    if (ambiantlight >= 0x100)
    {
        ambiantlight = 0xff;
    }
    if (ambiantlight < 0)
    {
        ambiantlight = 0;
    }

    g_FrontNintendoLight.a.l.colc[2] = ambiantlight;
    g_FrontNintendoLight.a.l.colc[1] = ambiantlight;
    g_FrontNintendoLight.a.l.colc[0] = ambiantlight;
    g_FrontNintendoLight.a.l.col[2] = ambiantlight;
    g_FrontNintendoLight.a.l.col[1] = ambiantlight;
    g_FrontNintendoLight.a.l.col[0] = ambiantlight;

#if defined(VERSION_EU)
    g_NintendoLogoSpin += 0.0209439527243f;
#else
    g_NintendoLogoSpin += 0.017453292f;
#endif

    {
        Mtxf tmpMtx;

        mtx4LoadYRotation(g_NintendoLogoSpin, &tmpMtx);
        mtx4ScaleColumns012(g_NintendoLogoScale, (f32*)&tmpMtx);

#if defined(VERSION_EU)
        g_NintendoLogoScale *= 1.09647190571f;
#else
        g_NintendoLogoScale *= 1.07977f;
#endif
        if (g_NintendoLogoScale > 1.1f)
        {
            g_NintendoLogoScale = 1.1f;
        }

        mtx4LoadLookAt(&ninlogoMtxf, 0.0f, 0.0f, 4000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        mtx4MultInPlace(&ninlogoMtxf, &tmpMtx);
        mtx4Copy(&tmpMtx, &ninlogoMtxf);
    }

    ninlogo.basemtx = &ninlogoMtxf;

    ninlogo.mtxlist = dynAllocate(g_FrontLogoModel->obj->numMatrices << 6);

    mtx4Copy(&ninlogoMtxf, ninlogo.mtxlist);

    g_FrontLogoModel->render_pos = (union RenderPosView*)ninlogo.mtxlist;

    modelUpdateNodeRelations(g_FrontLogoModel);

    ninlogo.flags = 3;
    ninlogo.zbufferenabled = FALSE;
    ninlogo.gdl = DL;

    modelRender(&ninlogo, g_FrontLogoModel);

    DL = ninlogo.gdl;

    for (i=0; i<g_FrontLogoModel->obj->numMatrices; i++)
    {
        Mtxf tmpMtxf;

        // hack: source address steps by sizeof(Mtxf), but can't get that to match
        mtx4Copy(&((s8*)g_FrontLogoModel->render_pos)[i*0x40], &tmpMtxf);
        mtxF2L(&tmpMtxf, &((Mtxf*)g_FrontLogoModel->render_pos)[i]);

    }

    return DL;
}


//********************************************************************************************************
//RARE LOGO
//********************************************************************************************************

void frontRareEnter(void)
{
    titleLoadRareLogo(g_FrontModelBuffer, 0x78000);
    sndPlaySfx(g_musicSfxBufferPtr, RARELOGO_SFX, 0);
}

void frontRareLeave(void) {
    return;
}

void frontRareTick(void)
{
    viSetUseZBuf(FALSE);
    if (titleIsRareIntroDone())
    {
        frontSetMenu(MENU_EYE_INTRO, TRUE);
        return;
    }
    if (joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON))
    {
        if (!g_FrontFirstBoot)
        {
            frontSetMenu(MENU_FILE_SELECT, TRUE);
            return;
        }
        g_FrontSkipIntro = TRUE;
        frontSetMenu(MENU_EYE_INTRO, TRUE);
    }
}

Gfx * frontRareRender(Gfx * DL) {
    return titleRenderRareIntro(DL);
}


//********************************************************************************************************
//GUNBARREL
//********************************************************************************************************
void frontGunbarrelEnter(void) {
    titleInitGunbarrel(g_FrontModelBuffer, 0x78000);
    musicTrack1Play(M_INTRO);
    g_FrontStartMusic = TRUE;
}

void frontGunbarrelLeave(void) {
    titleFreeBondModels();
}

void frontGunbarrelTick(void) {
    viSetUseZBuf(FALSE);
    if (titleIsGunbarrelDone()) {
        frontSetMenu(MENU_GOLDENEYE_LOGO, TRUE);
        return;
    }
    if (joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON)) {
        if (!g_FrontFirstBoot) {
            frontSetMenu(MENU_FILE_SELECT, TRUE);
            return;
        }
        g_FrontSkipIntro = TRUE;
        frontSetMenu(MENU_GOLDENEYE_LOGO, TRUE);
    }
}

Gfx * frontGunbarrelRender(Gfx * DL) {
    return titleRenderGunbarrel (DL);
}


//********************************************************************************************************
//GOLDENEYE LOGO
//********************************************************************************************************
void frontLogoEnter(void)
{
    struct coord3d pos;

    g_FrontTimer = 0;
    pos = goldeneyelogo_pos;
    modelFileLoad(PitemZ_entries[PROP_GOLDENEYELOGO].header, PitemZ_entries[PROP_GOLDENEYELOGO].filename, g_FrontModelBuffer, 0x3c000, 0);
    modelCalculateRwDataLen(PitemZ_entries[PROP_GOLDENEYELOGO].header);
    g_FrontLogoModel = modelmgrInstantiateModel(PitemZ_entries[PROP_GOLDENEYELOGO].header);

    modelSetScale(g_FrontLogoModel, 1.0f);
    modelSetRootPosition(g_FrontLogoModel, &pos);
}


void frontLogoLeave(void) {
    g_FrontFirstBoot = FALSE;
    modelFreeSlot(g_FrontLogoModel);
}

void frontLogoTick(void)
{
#ifdef REFRESH_PAL
#define GOLDENEYELOGO_TIMER_1 50*3
#define GOLDENEYELOGO_TIMER_2 (50/2)*3
#else
#define GOLDENEYELOGO_TIMER_1 60*3
#define GOLDENEYELOGO_TIMER_2 (60/2)*3
#endif

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);
    g_FrontTimer += g_ClockTimer;
    if (((!g_FrontFirstBoot) || (GOLDENEYELOGO_TIMER_1 < g_FrontTimer)) ||
       ((g_FrontLogoKeyPressed && (GOLDENEYELOGO_TIMER_2 < g_FrontTimer)))) {
        if (g_FrontTimer > GOLDENEYELOGO_TIMER_1) {
            if (g_FrontSkipIntro) {
                frontSetMenu(MENU_FILE_SELECT,TRUE);
            }
            else {
                frontSetFullCast(FALSE);
                frontSetMenu(MENU_DISPLAY_CAST,TRUE);
            }
        }
        else {
            if ((joyGetButtonsPressedThisFrame(PLAYER_1,ANY_BUTTON)) || ((g_FrontFirstBoot && g_FrontLogoKeyPressed))) {
                frontSetMenu(MENU_FILE_SELECT,TRUE);
            }
        }
    }
    else {
        if (joyGetButtonsPressedThisFrame(PLAYER_1,ANY_BUTTON)) {
            g_FrontLogoKeyPressed = TRUE;
        }
    }
}


Gfx *frontLogoRender(Gfx *DL)
{
    ModelRenderData gelogo;
    Mtxf logoMatrix;
    s32 i;
    LookAt * logoLookAt;
    Mtxf logoReflectMtx;

    gelogo = goldeneyelogo_MRD;

    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);

    logoLookAt = (LookAt *)dynAllocateLights(2);
    guLookAtReflect(&logoReflectMtx, logoLookAt, 0.0f, 0.0f, 4000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    // Lights macro? These need to be on one line.
    gSPNumLights(DL++, 1); \
    gSPLight(DL++, &g_FrontLogoLight.l[0], 1); \
    gSPLight(DL++, &g_FrontLogoLight, 2);

    // gSPLookAt macro expands to gSPLookAtX + gSPLookAtY
    gSPLookAt(DL++, logoLookAt);

    mtx4LoadLookAt(&logoMatrix, 0.0f, 0.0f, 3000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    gelogo.basemtx = &logoMatrix;
    gelogo.mtxlist = dynAllocate(g_FrontLogoModel->obj->numMatrices << 6);

    mtx4ScaleRows012(1.2f, logoMatrix.m[0]);
    mtx4Copy(&logoMatrix, gelogo.mtxlist);
    g_FrontLogoModel->render_pos = (union RenderPosView*)gelogo.mtxlist;

    modelUpdateNodeRelations(g_FrontLogoModel);

    gelogo.flags = 3;
    gelogo.zbufferenabled = FALSE;
    gelogo.gdl = DL;

    modelRender(&gelogo, g_FrontLogoModel);

    DL = gelogo.gdl;

    for (i=0; i<g_FrontLogoModel->obj->numMatrices; i++)
    {
        Mtxf tempMtxf;

        // hack: source address steps by sizeof(Mtxf), but can't get that to match
        mtx4Copy(&((s8*)g_FrontLogoModel->render_pos)[i*0x40], &tempMtxf);
        mtxF2L(&tempMtxf, &((Mtxf*)g_FrontLogoModel->render_pos)[i]);

    }

    return DL;
}


void frontHideAllSwitches(Model *model)
{
    s32 i;
    ModelNode *mnode;

    for (i = 0; i < model->obj->numSwitches; i++)
    {
        mnode = model->obj->Switches[i];

        if (mnode != NULL && (mnode->Opcode & 0xff) == MODELNODE_OPCODE_SWITCH)
        {
            union ModelRwData *unmrd;
            struct ModelRwData_SwitchRecord *srecord;

            unmrd = modelGetNodeRwData(model, mnode);
            srecord = (struct ModelRwData_SwitchRecord *)unmrd;
            srecord->visible = 0;
        }
    }
}


void frontSetSwitchVisible(Model* objinstance, s32 item, s32 mode)
{
    if (objinstance->obj->Switches[item] != NULL)
    {
        ModelRwData_SwitchRecord *rwdata = &modelGetNodeRwData(objinstance, objinstance->obj->Switches[item])->Switch;

        rwdata->visible = mode;
    }
}


//********************************************************************************************************
//WALLETINIT
//********************************************************************************************************

void frontSelectBondPicture(Model *objinstance, u32 bondID)
{
    frontSetSwitchVisible(objinstance,SW_BROSNAN,TRUE); //brosnan picture
    frontSetSwitchVisible(objinstance,SW_CONNERY,FALSE);
    frontSetSwitchVisible(objinstance,SW_DALTON,FALSE);
    frontSetSwitchVisible(objinstance,SW_MOORE,FALSE);
    frontSetSwitchVisible(objinstance,SW_BROSNANCOVER,TRUE); //bigger brosnan picture
    frontSetSwitchVisible(objinstance,SW_CONNERYCOVER,FALSE);
    frontSetSwitchVisible(objinstance,SW_DALTONCOVER,FALSE);
    frontSetSwitchVisible(objinstance,SW_MOORECOVER,FALSE);
}


void frontLoadFolders(void)
{
    ModelNode *mnode;
    s32 i;

    if (g_FrontFolderModels[0] == NULL)
    {
        modelFileLoad(
            PitemZ_entries[PROP_WALLETBOND].header,
            (s8*)PitemZ_entries[PROP_WALLETBOND].filename,
            (u8*)g_FrontModelBuffer,
            0xA000,
            0);

        modelCalculateRwDataLen(PitemZ_entries[PROP_WALLETBOND].header);

        for (i = FOLDER1; i < MAX_FOLDER_COUNT; i++)
        {
            g_FrontFolderModels[i]  = modelmgrInstantiateModelWithAnim(PitemZ_entries[PROP_WALLETBOND].header);
            modelSetScale(g_FrontFolderModels[i], 1.0f);
        }

        mnode = PitemZ_entries[PROP_WALLETBOND].header->Switches[GFXHIT0_PICS];

        if (mnode != NULL)
        {
            struct ModelRoData_DisplayList_CollisionRecord *srecord;
            struct ModelNode *b;
            Gfx *gdl;

            b = (struct ModelNode *)mnode;
            srecord = b->Data;

            gdl = (s32)srecord->BaseAddr + ((s32)srecord->Primary & 0xffffff);
            bgApplyDynamicCCRMLUT(gdl, NULL, CCRMLUT_WALLETBOND);
        }
    }
}


void frontFreeFolders(void)
{
    s32 i;

    for (i = FOLDER1; i < MAX_FOLDER_COUNT; i++)
    {
        if (g_FrontFolderModels[i] == NULL) { continue; }

        modelFreeAnimSlot(g_FrontFolderModels[i]);
        g_FrontFolderModels[i] = NULL;
    }
}


//********************************************************************************************************
//FILE SELECT
//********************************************************************************************************
void frontFileSelectEnter(void)
{
    s32 size = 0x6e000;
    Gfx* DL = (s32)(g_FrontModelBuffer) + (s32)(4096*10);
    int i;

    g_FrontSkipIntro = FALSE;


    if (g_FrontFolder < FOLDER1)
    {
        g_FrontFolder = FOLDER1;
    }
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    g_FrontFolderErase = FOLDER_INVALID;
    g_FrontFolderEraseChoice = FOLDER2;
    titleLoadBackdrop(&DL, &size);
    frontLoadFolders();
    if (g_FrontStartMusic){
        musicTrack1Play(M_FOLDERS);
        g_FrontStartMusic = FALSE;
    }
    g_FrontTimer = 0;
    for(i=1;i<CHEAT_INVALID;i++){
        g_CheatActivated[i] = FALSE;
    }
    g_AppendCheatSinglePlayer = FALSE;
    g_AppendCheatMultiPlayer = FALSE;
}


void frontFileSelectLeave(void)
{
    if ((g_FrontNextMenu == MENU_LEGAL_SCREEN) || (g_FrontEnterMenu == MENU_LEGAL_SCREEN))
    {
        frontFreeFolders();
    }
}


s32 frontGetChapterStart(s32 briefing)
{
    s32 i;

    for (i = briefing; i >= 0; i--)
    {
        if (g_FrontMissions[i].type != MISSION_PART)
        {
            return i;
        }
    }

    return -1;
}


void frontSetCursorOnFolder(int index)
{
  struct coord2d local_8;

  camViewToScreen(&g_FrontFolderScreenPos[index],&local_8);
  g_FrontCursorX = local_8.x + -1.0f;
  g_FrontCursorY = local_8.y + 20.0f;
}


/**
 * Tick function for the file folder select screen. Rebuild's each folder model's render matrix
 * then handles input. Also keeps track of the 30 second idle timeout for going back to the legal screen.
 */
void frontFileSelectTick(void)
{
    s32 walletnum;
    s32 foldernum;
    Mtxf lookatmtx;
    Mtxf walletmtx;
    struct coord3d *folderpos;

    /**
     *  Accumulate the menu timer if there's been no button inputs and no more than marginal stick input.
     *  The menu timer is checked at the end of this function to determine if we should switch to the legal screen.
     */
    if (((((joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON) != 0) || (joyGetStickX(0) < -5)) || (joyGetStickX(0) >= 6)) || (joyGetStickY(0) < -5)) || (joyGetStickY(0) >= 6))
    {
        g_FrontTimer = 0;
    }
    else
    {
        g_FrontTimer += g_ClockTimer;
    }

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    for (walletnum = 0; walletnum < MAX_FOLDER_COUNT; walletnum++)
    {
        folderpos = &g_FrontFolderPos[walletnum];

        mtx4LoadLookAt(&lookatmtx, 0.0f, 0.0f, 4000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        mtx4LoadTranslation(folderpos, &walletmtx);
        mtx4ScaleRows012(0.37f, (f32*)&walletmtx);
        mtx4MultInPlace(&lookatmtx, &walletmtx);

        g_FrontFolderModels[walletnum]->render_pos = dynAllocate(g_FrontFolderModels[walletnum]->obj->numMatrices << 6);
        mtx4Copy(&walletmtx, &g_FrontFolderModels[walletnum]->render_pos->pos);

        g_FrontFolderScreenPos[walletnum].f[0] = g_FrontFolderModels[walletnum]->render_pos->pos.m[3][0];
        g_FrontFolderScreenPos[walletnum].f[1] = g_FrontFolderModels[walletnum]->render_pos->pos.m[3][1];
        g_FrontFolderScreenPos[walletnum].f[2] = g_FrontFolderModels[walletnum]->render_pos->pos.m[3][2];

        frontHideAllSwitches(g_FrontFolderModels[walletnum]);

        frontSelectBondPicture(g_FrontFolderModels[walletnum], fileGetBondForFolder(walletnum));
        frontSetSwitchVisible(g_FrontFolderModels[walletnum], 0xE, 1);
        frontSetSwitchVisible(g_FrontFolderModels[walletnum], 0xD, 1);
    }

    if (g_FrontFolder >= FOLDER1)
    {
        if (g_FrontFolder == RAMROM_FOLDERNUM)
        {
            g_FrontFolder = g_FrontFolderSaved;
        }

        if ((g_FrontFolder < FOLDER1) || (g_FrontFolder >= MAX_FOLDER_COUNT))
        {
            g_FrontFolder = FOLDER1;
        }

        frontSetCursorOnFolder(g_FrontFolder);
        g_FrontFolder = -1;
    }

    if (g_FrontFolderErase >= 0)
    {
        if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_JPAD | L_TRIG | L_CBUTTONS) != 0) && (g_FrontFolderEraseChoice == 0))
        {
            g_FrontFolderEraseChoice = 1;
            sndPlaySfx(g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
        }
        else if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_JPAD | R_TRIG | R_CBUTTONS) != 0) && (g_FrontFolderEraseChoice != 0))
        {
            g_FrontFolderEraseChoice = 0;
            sndPlaySfx(g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
        }
        if ((joyGetStickX(0) < -0x2D) && (g_FrontFolderEraseChoice == 0))
        {
            g_FrontFolderEraseChoice = 1;
            sndPlaySfx(g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
        }
        else if ((joyGetStickX(0) >= 0x2E) && (g_FrontFolderEraseChoice != 0))
        {
            g_FrontFolderEraseChoice = 0;
            sndPlaySfx(g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
        }
        if (joyGetButtonsPressedThisFrame(PLAYER_1, A_BUTTON | Z_TRIG | START_BUTTON) != 0)
        {
            if (g_FrontFolderEraseChoice == 0)
            {
                fileDeleteSaveForFolder(g_FrontFolderErase);
                sndPlaySfx(g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
            }
            else
            {
                sndPlaySfx(g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
            }
            frontSetCursorOnFolder(g_FrontFolderErase);
            g_FrontFolderErase = -1;
        }
        else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON) != 0)
        {
            frontSetCursorOnFolder(g_FrontFolderErase);
            g_FrontFolderErase = -1;
            sndPlaySfx(g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
        }
    }
    else
    {
        for (foldernum = FOLDER1; foldernum < MAX_FOLDER_COUNT; foldernum++)
        {
            f32 xmax;
            f32 xmin;
            f32 ymax;
            f32 ymin;

            struct rectbbox folderbbox;

            LEVEL_SOLO_SEQUENCE higheststage;
            DIFFICULTY highestdifficulty;

            modelGetXYExtents(g_FrontFolderModels[foldernum], &xmax, &xmin, &ymax, &ymin);
            projectRectCornersTo2D(&g_FrontFolderScreenPos[foldernum], &xmin, &ymin, &folderbbox.right, &folderbbox.left);

            if ((folderbbox.right <= g_FrontCursorX)
                && (g_FrontCursorX <= folderbbox.left)
                && (folderbbox.down <= g_FrontCursorY)
                && (g_FrontCursorY <= folderbbox.up))
            {
                fileGetHighestStageDifficultyCompletedForFolder(foldernum, &higheststage, &highestdifficulty);

                if (joyGetButtonsPressedThisFrame(PLAYER_1, A_BUTTON | Z_TRIG | START_BUTTON) != 0)
                {
                    if (g_FrontFolderIcon == 0)
                    {
                        g_FrontFolder = foldernum;
                        sndPlaySfx(g_musicSfxBufferPtr, PAPER_TURN_SFX, NULL);
                    }
                    else if (g_FrontFolderIcon == 1)
                    {
                        fileCopyFolderToFirstFree(foldernum);
                        g_FrontFolderIcon = 0;
                        sndPlaySfx(g_musicSfxBufferPtr, COPY_FILE_SFX, NULL);
                    }
                    else if (g_FrontFolderIcon == 2)
                    {
                        if ((higheststage >= SP_LEVEL_DAM) && (highestdifficulty >= DIFFICULTY_AGENT))
                        {
                            g_FrontFolderErase = foldernum;
                            g_FrontFolderEraseChoice = 1;
                        }

                        g_FrontFolderIcon = 0;
                        sndPlaySfx(g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
                    }

                    break;
                }
            }
        }

        if (joyGetButtonsPressedThisFrame(PLAYER_1, A_BUTTON | Z_TRIG | START_BUTTON) != 0)
        {
            if ((folder_option_COPY_bound.left <= g_FrontCursorX)
                && (g_FrontCursorX <= folder_option_COPY_bound.right)
                && (folder_option_COPY_bound.up <= g_FrontCursorY)
                && (g_FrontCursorY <= folder_option_COPY_bound.down))
            {
                g_FrontFolderIcon = 1;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_LOCK_SFX, NULL);
            }
            else if ((folder_option_ERASE_bound.left <= g_FrontCursorX)
                && (g_FrontCursorX <= folder_option_ERASE_bound.right)
                && (folder_option_ERASE_bound.up <= g_FrontCursorY)
                && (g_FrontCursorY <= folder_option_ERASE_bound.down))
            {
                g_FrontFolderIcon = 2;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_LOCK_SFX, NULL);
            }
            else if (g_FrontFolderIcon != 0)
            {
                g_FrontFolderIcon = 0;
                sndPlaySfx(g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
            }
        }
        else if ((joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON) != 0)
            && (g_FrontFolderIcon != 0))
        {
            g_FrontFolderIcon = 0;
            sndPlaySfx(g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
        }

        frontMoveCursor();
    }

    if (g_FrontFolder >= FOLDER1)
    {
        frontSetMenu(MENU_MODE_SELECT, FALSE);
        frontSetModeCursor(0);

        return;
    }

    // Change to the legal screen if 30 seconds of no input have elapsed
#if defined(VERSION_EU)
    if (g_FrontTimer >= 1501) // PAL (50fps): 30 seconds + 1 frame
#else
    if (g_FrontTimer >= 1801) // NTSC (60fps): 30 seconds + 1 frame
#endif
    {
        frontSetMenu(MENU_LEGAL_SCREEN, TRUE);
    }
}


char* frontGetDifficultyName(s32 difficulty)
{
    char* text = NULL;
    switch (difficulty)
    {
    case 0:
        text = langGet(getStringID(LTITLE, TITLE_STR_19_AGENT)); //Agent
        break;
    case 1:
        text = langGet(getStringID(LTITLE, TITLE_STR_20_SECRETAGENT)); //Secret Agent
        break;
    case 2:
        text = langGet(getStringID(LTITLE, TITLE_STR_21_00AGENT)); //00 Agent
        break;
    case 3:
        text = langGet(getStringID(LTITLE, TITLE_STR_22_007)); //007
        break;
    }
    return text;
}


/**
 * Render the file folder select screen. That includes the background texture, four 3D wallet models, the progress text written on them,
 * the crosshair cursor, and the "SELECT FILE", "Copy", and "Erase" text and icons.
 *
 * difficultytext is only 4 bytes: make it bigger before writing anything longer into it.
 */
Gfx *frontFileSelectRender(Gfx *DL)
{
    s32 foldernum;
    struct FolderSelectColour topcolour;
    struct FolderSelectColour bottomcolour;
    s32 j;
    s32 i;
    u8 *langp;
    s32 chapterindex;                       // index of the chapter (MISSION_HEADER) row in g_FrontMissions
    ModelRenderData renderdata;
    s32 missionindex;                       // index of the mission (MISSION_PART) row in g_FrontMissions
    Mtxf mtx;
    f32 folderx;
    struct point2d textpos;
    struct point2d textsize;
    LEVEL_SOLO_SEQUENCE higheststage;
    DIFFICULTY highestdifficulty;
    struct coord2d folderpos2d;             // folder position projected to screen space

    topcolour = (struct FolderSelectColour){0x14, 0x14, 0x14};
    bottomcolour = (struct FolderSelectColour){0x32, 0x32, 0x32};

    /** 
     * The background image doesn't fully cover the screen so it's necessary to clear the back framebuffer to opaque black.
     * This creates the impression of the folder menu having a black border.
     */ 
    DL = screenClearBlack(DL);

    gDPSetCombineLERP(DL++, TEXEL0, PRIMITIVE, ENV_ALPHA, PRIMITIVE, 0, 0, 0, PRIMITIVE, TEXEL0, PRIMITIVE, ENV_ALPHA, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gDPSetEnvColor(DL++, 0xFF, 0xFF, 0xFF, 0x14);

    // Render the gun barrel background image.
    DL = titleRenderFolderMenuBackground(DL, (s32) mathFloor(((f32) viGetX() * -80.0f) / 1280.0f), &topcolour, &bottomcolour);

    gSPDisplayList(DL++, &dlBasicGeometry);
    gSPDisplayList(DL++, &dlFastPipelineSetup);

    for (j = 3; j >= 0; j--)
    {
        // struct copy
        renderdata = (ModelRenderData){NULL, TRUE, 3};

        renderdata.flags = 3;
        renderdata.zbufferenabled = FALSE;
        renderdata.gdl = DL;

        // Render the folder wall model.
        modelRender(&renderdata, g_FrontFolderModels[j]);

        DL = renderdata.gdl;

        for (i=0; i < g_FrontFolderModels[j]->obj->numMatrices; i++)
        {
            mtx4Copy((Mtxf*)&((s8*)g_FrontFolderModels[j]->render_pos)[i * sizeof(Mtxf)], &mtx);
            mtxF2L(&mtx, &((Mtxf*)g_FrontFolderModels[j]->render_pos)[i]);

        }
    }

    DL = textSetupDraw(DL);
    textSetSpacing(0);

    for (foldernum = FOLDER1; foldernum < MAX_FOLDER_COUNT; foldernum++)
    {
        char difficultytext[4];
        char missiontext[18];   // "Mission <chapter>.<part>\n"
        struct coord3d * folderworldpos;

        folderworldpos = &g_FrontFolderScreenPos[foldernum];

        camViewToScreen(folderworldpos, &folderpos2d);

        if (foldernum == g_FrontFolderErase)
        {
            textpos.p[1] = (s32) mathFloor(folderpos2d.f[0]) - 0x31;
            textpos.p[0] = (s32) mathFloor(folderpos2d.f[1]) + 0x19;

            DL = textRenderRect(DL, textpos.p[1], textpos.p[0], textpos.p[1] + 0x63, textpos.p[0] + 0x2A, 0x32);

            langp = langGet(getStringID(LTITLE, TITLE_STR_23_ERASEFILE)); /* Erase file? */
            textpos.p[1] = (s32) mathFloor(folderpos2d.f[0]) - 0x2F;
            textpos.p[0] = (s32) mathFloor(folderpos2d.f[1]) + 0x1E;

            DL = textRender(DL, &textpos.p[1], &textpos.p[0], langp, g_FontZurichChars, g_FontZurich, 0xEBD879FF, viGetX(), viGetY(), 0, 0);

            langp = langGet(getStringID(LTITLE, TITLE_STR_24_CANCEL)); /* cancel */
            textpos.p[1] = (s32) mathFloor(folderpos2d.f[0]) - 0x2F;
            textpos.p[0] = (s32) mathFloor(folderpos2d.f[1]) + 0x32;

            if (g_FrontFolderEraseChoice != 0)
            {
                textsize.p[0] = 0;
                textsize.p[1] = 0;
                textMeasure(&textsize.p[1], &textsize.p[0], langp, g_FontZurichChars, g_FontZurich, 0);
                DL = textRenderRect(DL, textpos.p[1] - 1, textpos.p[0] - 1, textsize.p[0] + textpos.p[1] + 3, textsize.p[1] + textpos.p[0], 0x32);
                DL = textRender(DL, &textpos.p[1], &textpos.p[0], langp, g_FontZurichChars, g_FontZurich, -1, viGetX(), viGetY(), 0, 0);
            }
            else
            {
                DL = textRender(DL, &textpos.p[1], &textpos.p[0], langp, g_FontZurichChars, g_FontZurich, 0xEBD879FF, viGetX(), viGetY(), 0, 0);
            }

            langp = langGet(getStringID(LTITLE, TITLE_STR_25_CONFIRM)); /* confirm */

            folderx = mathFloor(folderpos2d.f[0]);
            textpos.p[1] = (s32) folderx - 2;
            textpos.p[0] = (s32) mathFloor(folderpos2d.f[1]) + 0x32;

            if (g_FrontFolderEraseChoice != 0)
            {
                DL = textRender(DL, &textpos.p[1], &textpos.p[0], langp, g_FontZurichChars, g_FontZurich, 0xEBD879FF, viGetX(), viGetY(), 0, 0);
            }
            else
            {
                textsize.p[0] = 0;
                textsize.p[1] = 0;
                textMeasure(&textsize.p[1], &textsize.p[0], langp, g_FontZurichChars, g_FontZurich, 0);
                DL = textRenderRect(DL, textpos.p[1] - 1, textpos.p[0] - 1, textsize.p[0] + textpos.p[1] + 3, textsize.p[1] + textpos.p[0], 0x32);
                DL = textRender(DL, &textpos.p[1], &textpos.p[0], langp, g_FontZurichChars, g_FontZurich, -1, viGetX(), viGetY(), 0, 0);
            }

        }
        else
        {
            fileGetHighestStageDifficultyCompletedForFolder(foldernum, &higheststage, &highestdifficulty);

            if ((higheststage >= SP_LEVEL_DAM) && (highestdifficulty >= DIFFICULTY_AGENT))
            {
                langp = frontGetDifficultyName(highestdifficulty);

                if (langp != NULL)
                {
                    strcpy((char *)&difficultytext, langp);
                    strcat((char *)&difficultytext, "\n");

                    textsize.p[0] = 0;
                    textsize.p[1] = 0;

                    textMeasure(&textsize.p[1], &textsize.p[0], (char *)&difficultytext, g_FontZurichChars, g_FontZurich, 0);
                    textpos.p[1] = ((s32) mathFloor(folderpos2d.f[0]) - (textsize.p[0] / 2)) - 1;
                    textpos.p[0] = (s32) mathFloor(folderpos2d.f[1]) + 0x15;
                    DL = textRender(DL, &textpos.p[1], &textpos.p[0], (char *)&difficultytext, g_FontZurichChars, g_FontZurich, 0xEBD879FF, viGetX(), viGetY(), 0, 0);
                }

                if (highestdifficulty != DIFFICULTY_007)
                {
                    chapterindex = -1;

                    strcpy(missiontext, langGet(getStringID(LTITLE, TITLE_STR_26_MISSION))); /* Mission */

                    for (missionindex = 0; g_FrontMissions[missionindex].folder_text_preset != 0; missionindex++)
                    {
                        if (higheststage == g_FrontMissions[missionindex].mission_num)
                        {
                            chapterindex = frontGetChapterStart(missionindex);
                            break;
                        }
                    }

                    if (chapterindex >= 0)
                    {
                        strcat((char *)missiontext, (char *)g_FrontMissions[chapterindex].string_ptr);
                        strcat((char *)missiontext, ".");
                    }

                    strcat((char *)missiontext, (char *)g_FrontMissions[missionindex].string_ptr);
                    strcat((char *)missiontext, "\n");
                    textsize.p[0] = 0;
                    textsize.p[1] = 0;

                    textMeasure(&textsize.p[1], &textsize.p[0], (char *)missiontext, g_FontZurichChars, g_FontZurich, 0);
                    textpos.p[1] = ((s32) mathFloor(folderpos2d.f[0]) - (textsize.p[0] / 2)) - 1;
                    textpos.p[0] = (s32) mathFloor(folderpos2d.f[1]) + 0x2D;

                    DL = textRender(DL, &textpos.p[1], &textpos.p[0], (char *)missiontext, g_FontZurichChars, g_FontZurich, 0xEBD879FF, viGetX(), viGetY(), 0, 0);
                }
            }
        }
    }

    {
        // The above for loops makes a lot of compiler temp variables on the stack,
        // These need to be declared sometime after the above for loop.
        struct coord2d copypos;
        struct coord2d copyhalfsize;
        struct coord2d erasepos;
        struct coord2d erasehalfsize;
        struct coord2d selectpos;
        struct coord2d selecthalfsize;

        textSetSpacing(0);

        langp = langGet(getStringID(LTITLE, TITLE_STR_27_COPY)); /* Copy */

        textsize.p[0] = 0;
        textsize.p[1] = 0;

        textMeasure(&textsize.p[1], &textsize.p[0], langp, g_FontZurichChars, g_FontZurich, 0);

        textpos.p[0] = 285 - (textsize.p[1] / 2);
        textpos.p[1] = 247;

        DL = textRender(DL, &textpos.p[1], &textpos.p[0], langp, g_FontZurichChars, g_FontZurich, -1, viGetX(), viGetY(), 0, 0);
        folder_option_COPY_bound.right = (f32) (textsize.p[0] + textpos.p[1]);

        langp = langGet(getStringID(LTITLE, TITLE_STR_28_ERASE)); /* Erase */

        textsize.p[0] = 0;
        textsize.p[1] = 0;

        textMeasure(&textsize.p[1], &textsize.p[0], langp, g_FontZurichChars, g_FontZurich, 0);

        textpos.p[1] = 357;
        textpos.p[0] = 285 - (textsize.p[1] / 2);

        DL = textRender(DL, &textpos.p[1], &textpos.p[0], langp, g_FontZurichChars, g_FontZurich, -1, viGetX(), viGetY(), 0, 0);

        folder_option_ERASE_bound.right = (f32) (textsize.p[0] + textpos.p[1]);

        copypos.f[0] = 225.0f;
        copypos.f[1] = 285.0f;

        copyhalfsize.f[0] = (f32) (g_ImagesMainFolder + IMG_COPY)->width * 0.5f;
        copyhalfsize.f[1] = (f32) (g_ImagesMainFolder + IMG_COPY)->height * 0.5f;

        texSelect(&DL, g_ImagesMainFolder, 4, 0, 0);
        texRectDrawImage(&DL, &copypos.f[0], &copyhalfsize.f[0], g_ImagesMainFolder->width, g_ImagesMainFolder->height, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF, (s32) g_ImagesMainFolder->level > 0, 0);

        folder_option_COPY_bound.left = copypos.f[0] - copyhalfsize.f[0];
        folder_option_COPY_bound.up = copypos.f[1] - copyhalfsize.f[1];
        folder_option_COPY_bound.down = copypos.f[1] + copyhalfsize.f[1];

        erasepos.f[0] = 335.0f;
        erasepos.f[1] = 285.0f;

        erasehalfsize.f[0] = (g_ImagesMainFolder + IMG_DEL)->width * 0.5f;
        erasehalfsize.f[1] = (g_ImagesMainFolder + IMG_DEL)->height * 0.5f;

        texSelect(&DL, g_ImagesMainFolder + IMG_DEL, 4, 0, 0);
        texRectDrawImage(&DL, &erasepos.f[0], &erasehalfsize.f[0], (g_ImagesMainFolder + IMG_DEL)->width, (g_ImagesMainFolder + IMG_DEL)->height, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF, (s32) (g_ImagesMainFolder + IMG_DEL)->level > 0, 0);

        folder_option_ERASE_bound.left = (f32) (erasepos.f[0] - erasehalfsize.f[0]);
        folder_option_ERASE_bound.up = (f32) (erasepos.f[1] - erasehalfsize.f[1]);
        folder_option_ERASE_bound.down = (f32) (erasepos.f[1] + erasehalfsize.f[1]);

        selectpos.f[0] = 110.0f;
        selectpos.f[1] = 285.0f;

        selecthalfsize.f[0] = (g_ImagesMainFolder + IMG_SEL)->width * 0.5f;
        selecthalfsize.f[1] = (g_ImagesMainFolder + IMG_SEL)->height * 0.5f;

        texSelect(&DL, g_ImagesMainFolder + IMG_SEL, 4, 0, 0);
        texRectDrawImage(&DL, &selectpos.f[0], &selecthalfsize.f[0], (g_ImagesMainFolder + IMG_SEL)->width, (g_ImagesMainFolder + IMG_SEL)->height, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF, (s32) (g_ImagesMainFolder + IMG_SEL)->level > 0, 0);
    }

    if (g_FrontFolderErase < 0)
    {
        DL = frontDrawCursor(DL);
    }

    return DL;
}


//********************************************************************************************************
//MODE SELECT
//********************************************************************************************************
/**
 * Mode select rows: select mission, co-op, counter-op, multiplayer, cheat
 * options.
 */
#define MODESEL_ROW_MISSION   0
#define MODESEL_ROW_COOP      1
#define MODESEL_ROW_COUNTEROP 2
#define MODESEL_ROW_MULTI     3
#define MODESEL_ROW_CHEATS    4

static s32 modesel_row_y[5] = {0xC4, 0xDE, 0xF8, 0x112, 0x12C};
static s32 modesel_highlighted_row = -1;

void frontModeSelectEnter(void)
{
    // Co-op is picked again here (frontSetModeCursor has already used it
    // to put the cursor back on the co-op row).
    g_CoopMode = FALSE;
    g_CounterOpMode = FALSE;
    g_FrontGameMode = GAMEMODE_INTRO;
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    frontLoadFolders();
    fileUpdateBondInCurrentFolder();
}


void frontModeSelectLeave(void) 
{
    return;
}


void frontModeSelectTick(void)
{
    u32 i;

    g_CheatMenuAvailable = FALSE;
    for (i=CHEAT_EXTRA_MP_CHARS; i != CHEAT_INVALID; i++)
    {
        if (frontIsCheatUnlocked(i))
        {
            g_CheatAvailable[i] = TRUE;
            g_CheatMenuAvailable = TRUE;
        }
        else
        {
            g_CheatAvailable[i] = FALSE;
        }
    }
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(FALSE);

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSelectBondPicture(g_FrontFolderModels[0], fileGetBondForFolder(g_FrontFolder));
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PHOTOBOND, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_EYESONLY, 1);
    g_FrontTabPrevHighlight = FALSE;
    g_FrontDifficultyHighlight = DIFFICULTY_MULTI;
    modesel_highlighted_row = -1;
    if (frontCheckCursorOnPreviousTab())
    {
        g_FrontTabPrevHighlight = TRUE;
        if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON|Z_TRIG|A_BUTTON))
        {
            g_FrontTabPrevSelected = TRUE;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, 0);
        }
    }
    else
    {
        s32 row;

        for (row = MODESEL_ROW_MISSION; row < MODESEL_ROW_CHEATS; row++)
        {
            if (g_FrontCursorY < modesel_row_y[row + 1] - 6)
            {
                break;
            }
        }

        if ((row == MODESEL_ROW_CHEATS) && !g_CheatMenuAvailable)
        {
            row = MODESEL_ROW_MULTI;
        }

        // Co-op, counter-op and multiplayer need a second controller.
        if (((row == MODESEL_ROW_COOP) || (row == MODESEL_ROW_COUNTEROP) || (row == MODESEL_ROW_MULTI)) && (joyGetControllerCount() < 2))
        {
            row = -1;
        }

        modesel_highlighted_row = row;

        if ((row >= 0) && joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON|Z_TRIG|A_BUTTON))
        {
            switch (row)
            {
                case MODESEL_ROW_MISSION:
                    g_FrontGameMode = GAMEMODE_SOLO;
                    g_CoopMode = FALSE;
                    g_CounterOpMode = FALSE;
                    break;
                case MODESEL_ROW_COOP:
                    g_FrontGameMode = GAMEMODE_SOLO;
                    g_CoopMode = TRUE;
                    g_CounterOpMode = FALSE;
                    break;
                case MODESEL_ROW_COUNTEROP:
                    g_FrontGameMode = GAMEMODE_SOLO;
                    g_CoopMode = TRUE;
                    g_CounterOpMode = TRUE;
                    break;
                case MODESEL_ROW_MULTI:
                    g_FrontGameMode = GAMEMODE_MULTI;
                    g_CoopMode = FALSE;
                    g_CounterOpMode = FALSE;
                    break;
                case MODESEL_ROW_CHEATS:
                    g_FrontGameMode = GAMEMODE_CHEATS;
                    break;
            }

            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE_SFX, 0);
        }
    }

    // Up and down on the D-pad step the cursor between rows.
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_JPAD|D_JPAD))
    {
        s32 row = MODESEL_ROW_MISSION;
        s32 lastrow = g_CheatMenuAvailable ? MODESEL_ROW_CHEATS : MODESEL_ROW_MULTI;

        while ((row < lastrow) && (g_FrontCursorY >= modesel_row_y[row + 1] - 6))
        {
            row++;
        }

        row += joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD) ? 1 : -1;
        row = (row < MODESEL_ROW_MISSION) ? MODESEL_ROW_MISSION : ((row > lastrow) ? lastrow : row);
        g_FrontCursorX = 126.0f;
        g_FrontCursorY = modesel_row_y[row] + 6;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
    {
        g_FrontTabPrevSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, 0);
    }
    frontMoveCursor();
    if ((g_FrontGameMode == GAMEMODE_SOLO) && g_CoopMode)
    {
        frontSetMenu(MENU_COOP_SETUP, FALSE);
        return;
    }
    if (g_FrontGameMode == GAMEMODE_SOLO)
    {
        frontSetMenu(MENU_MISSION_SELECT, FALSE);
        frontSetMissionCursor(0);
        return;
    }
    if (g_FrontGameMode == GAMEMODE_MULTI)
    {
        frontSetMenu(MENU_MP_OPTIONS, FALSE);
        return;
    }
    if (g_FrontGameMode == GAMEMODE_CHEATS)
    {
        frontSetMenu(MENU_CHEAT, FALSE);
        return;
    }
    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_FILE_SELECT, FALSE);
    }
}


Gfx *frontSetupMenuBackground(Gfx *DL)
{
    f32 camX;
    f32 camY;
    ModelRenderData renderdata;
    s32 i;
    Mtxf viewMtx;
    Mtxf folderMtx;
    Mtxf tmp;

    camX = g_FrontFolderPos[g_FrontFolder].f[0];
    camY = g_FrontFolderPos[g_FrontFolder].f[1];

    renderdata = (ModelRenderData){NULL, TRUE, 3};

    camX += g_FrontFolderCamOffsetX;
    camY += g_FrontFolderCamOffsetY;

    mtx4LoadLookAt(&viewMtx, camX, camY, 4000.0f + g_FrontFolderCamOffsetZ, camX, camY, 0.0f, 0.0f, 1.0f, 0.0f);
    mtx4LoadTranslation(&g_FrontFolderPos[g_FrontFolder], &folderMtx);
    mtx4ScaleRows012(0.25f, folderMtx.m[0]);
    mtx4MultInPlace(&viewMtx, &folderMtx);

    renderdata.basemtx = &folderMtx;

    renderdata.mtxlist = dynAllocate(g_FrontFolderModels[0]->obj->numMatrices << 6);

    mtx4Copy(&folderMtx, renderdata.mtxlist);

    g_FrontFolderModels[0]->render_pos = (union RenderPosView*)renderdata.mtxlist;

    renderdata.flags = 3;
    renderdata.zbufferenabled = FALSE;
    renderdata.gdl = DL;

    modelRender(&renderdata, g_FrontFolderModels[0]);

    DL = renderdata.gdl;

    for (i=0; i<g_FrontFolderModels[0]->obj->numMatrices; i++)
    {
        // hack: source address steps by sizeof(Mtxf), but can't get that to match
        mtx4Copy(&((s8*)g_FrontFolderModels[0]->render_pos)[i*0x40], &tmp);
        mtxF2L(&tmp, &((Mtxf*)g_FrontFolderModels[0]->render_pos)[i]);
    }


    return DL;
}


/**
 * Puts the cursor on the mode select row for a game mode (GAMEMODE_SOLO
 * with g_CoopMode set is the co-op row, or the counter-op row with
 * g_CounterOpMode).
 */
void frontSetModeCursor(int mode)
{
    s32 row;

    if (mode == GAMEMODE_MULTI)
    {
        row = MODESEL_ROW_MULTI;
    }
    else if (mode == GAMEMODE_CHEATS)
    {
        row = MODESEL_ROW_CHEATS;
    }
    else if (g_CounterOpMode)
    {
        row = MODESEL_ROW_COUNTEROP;
    }
    else if (g_CoopMode)
    {
        row = MODESEL_ROW_COOP;
    }
    else
    {
        row = MODESEL_ROW_MISSION;
    }

    g_FrontCursorX = 126.0f;
    g_FrontCursorY = modesel_row_y[row] + 6;
}

/**
 * Draws one numbered row of the mode select page.
 */
static Gfx *frontDrawModeSelRow(Gfx *DL, s32 row, char *number, u8 *textstring, s32 text_color)
{
    s32 x;
    s32 y;
    s32 x2;
    s32 y2;

    x = 0x96;
    y = modesel_row_y[row];
    DL = frontPrintText(DL, &x, &y, number, g_FontZurichChars, g_FontZurich, text_color, viGetX(), viGetY(), 0, 0);

    textMeasure(&x2, &y2, textstring, g_FontZurichChars, g_FontZurich, 0);

    x = 0xAA;
    y = modesel_row_y[row];
    if (modesel_highlighted_row == row)
    {
        DL = textRenderRect(DL, 0x94, modesel_row_y[row] - 2, y2 + 0xAF, modesel_row_y[row] + 0xE, 0x32);
    }

    DL = frontPrintText(DL, &x, &y, textstring, g_FontZurichChars, g_FontZurich, text_color, viGetX(), viGetY(), 0, 0);

    return DL;
}


Gfx* frontModeSelectRender(Gfx* DL)
{
    char *textstring;
    s32 text_color;

    DL = viSetFillColor(DL,0,0,0);
    DL = viFillScreen(DL);
    #ifdef VERSION_EU
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
    #endif
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);

    text_color = (joyGetControllerCount() >= 2) ? 0xFF : 0x70;

    textstring = langGet((g_AppendCheatSinglePlayer != 0) ? getStringID(LTITLE, TITLE_STR_117_CHEATSELECTMISSION) : getStringID(LTITLE, TITLE_STR_29_SELECTMISSION));
    DL = frontDrawModeSelRow(DL, MODESEL_ROW_MISSION, "1.\n", (u8 *)textstring, 0xFF);

    DL = frontDrawModeSelRow(DL, MODESEL_ROW_COOP, "2.\n", (u8 *)"2 PLAYER CO-OP\n", text_color);
    DL = frontDrawModeSelRow(DL, MODESEL_ROW_COUNTEROP, "3.\n", (u8 *)"2 PLAYER COUNTER-OP\n", text_color);

    textstring = langGet((g_AppendCheatMultiPlayer != 0) ? getStringID(LTITLE, TITLE_STR_276_CHEATMULTIPLAYER) : getStringID(LTITLE, TITLE_STR_30_MULTIPLAYER));
    DL = frontDrawModeSelRow(DL, MODESEL_ROW_MULTI, "4.\n", (u8 *)textstring, text_color);

    if (g_CheatMenuAvailable != 0)
    {
        textstring = langGet(getStringID(LTITLE, TITLE_STR_31_CHEATOPTIONS));
        DL = frontDrawModeSelRow(DL, MODESEL_ROW_CHEATS, "5.\n", (u8 *)textstring, 0xFF);
    }

    DL = frontAddPreviousTabText(DL);
    DL = frontDrawCursor(DL);
    return DL;
}


//********************************************************************************************************
//CO-OP SETUP
//********************************************************************************************************

/**
 * Co-op setup, between mode select and mission select: player 2's
 * character and control style, and the split screen display options.
 * Point at a row and press A (or C-right / right on the D-pad) to step its
 * value forward, C-left / left on the D-pad to step it back.
 */
#define COOP_SETUP_ROW_CHAR     0
#define COOP_SETUP_ROW_CONTROLS 1
#define COOP_SETUP_ROW_SPLIT    2
#define COOP_SETUP_ROW_HIRES    3
#define COOP_SETUP_ROW_DRAWDIST 4
#define COOP_SETUP_ROW_COUNT    5

#define COOP_SETUP_ROW_Y(row) (0x79 + (row) * 0x14)
#define COOP_SETUP_VALUE_X 0xC8
#define COOP_SETUP_CURSOR_X 0x28

static char *coop_setup_row_labels[COOP_SETUP_ROW_COUNT] = {
    "Player 2:\n", // "Bond:" in counter-op
    "Player 2 Controls:\n",
    "Split Screen:\n",
    "Hi-Res:\n",
    "Draw Distance:\n",
};

static s32 coop_setup_highlighted_row = -1;

static s32 frontCoopCharCount(void);
static s32 frontCoopGetPartnerChar(void);
static u8 *frontCoopGetPartnerCharName(void);

void frontCoopSetupEnter(void)
{
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    coop_setup_highlighted_row = -1;
    g_FrontCursorX = COOP_SETUP_CURSOR_X;
    g_FrontCursorY = COOP_SETUP_ROW_Y(COOP_SETUP_ROW_CHAR) + 6;

    // Co-op gives each player one controller, so only the 1.x styles.
    if (g_MpControlStyle[PLAYER_2] >= CONTROLLER_CONFIG_PLENTY)
    {
        g_MpControlStyle[PLAYER_2] = CONTROLLER_CONFIG_HONEY;
    }

    frontLoadFolders();
}


void frontCoopSetupLeave(void)
{
    return;
}


static void frontCoopSetupStep(s32 row, s32 step)
{
    s32 value;
    s32 count;

    switch (row)
    {
        case COOP_SETUP_ROW_CHAR:
            // Counter-op: which controller plays Bond. Player 2 plays
            // whichever guard they take over, so there is no character.
            if (g_CounterOpMode)
            {
                g_CounterOpBondPad ^= 1;
                break;
            }

            count = frontCoopCharCount();
            value = (frontCoopGetPartnerChar() + step + count) % count;
            fileCoopSetPartnerChar(value);
            break;

        case COOP_SETUP_ROW_CONTROLS:
            g_MpControlStyle[PLAYER_2] = (g_MpControlStyle[PLAYER_2] + step + CONTROLLER_CONFIG_PLENTY) % CONTROLLER_CONFIG_PLENTY;
            break;

        case COOP_SETUP_ROW_SPLIT:
            optionsSetSplitStacked(!optionsGetSplitStacked());
            break;

        case COOP_SETUP_ROW_HIRES:
            optionsSetHiRes(!optionsGetHiRes());
            break;

        case COOP_SETUP_ROW_DRAWDIST:
            optionsSetDrawNear(!optionsGetDrawNear());
            break;
    }
}


void frontCoopSetupTick(void)
{
    s32 step = 0;
    s32 row;

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(FALSE);

    if (joyGetControllerCount() < 2)
    {
        g_CoopMode = FALSE;
        g_CounterOpMode = FALSE;
        frontSetMenu(MENU_MODE_SELECT, FALSE);
        frontSetModeCursor(GAMEMODE_SOLO);
        return;
    }

    g_FrontTabPrevHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    coop_setup_highlighted_row = -1;

    if (frontCheckCursorOnPreviousTab())
    {
        g_FrontTabPrevHighlight = TRUE;
    }
    else if (frontCheckCursorOnNextTab())
    {
        g_FrontTabNextHighlight = TRUE;
    }
    else
    {
        for (row = 0; row < COOP_SETUP_ROW_COUNT; row++)
        {
            if ((g_FrontCursorY >= COOP_SETUP_ROW_Y(row) - 1) && (g_FrontCursorY < COOP_SETUP_ROW_Y(row) + 0x13))
            {
                coop_setup_highlighted_row = row;
            }
        }
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON))
    {
        g_FrontTabNextSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, PAPER_TURN_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabPrevHighlight)
        {
            g_FrontTabPrevSelected = TRUE;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
        else if (g_FrontTabNextHighlight)
        {
            g_FrontTabNextSelected = TRUE;
            sndPlaySfx(g_musicSfxBufferPtr, PAPER_TURN_SFX, NULL);
        }
        else
        {
            step = 1;
        }
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_JPAD))
    {
        step = 1;
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_JPAD))
    {
        step = -1;
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
    {
        g_FrontTabPrevSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    if ((step != 0) && (coop_setup_highlighted_row >= 0))
    {
        frontCoopSetupStep(coop_setup_highlighted_row, step);
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    // Up and down on the D-pad step the cursor between rows.
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_JPAD|D_JPAD))
    {
        row = coop_setup_highlighted_row;

        if (row < 0)
        {
            row = joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD) ? -1 : COOP_SETUP_ROW_COUNT;
        }

        row += joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD) ? 1 : -1;
        row = (row < 0) ? 0 : ((row >= COOP_SETUP_ROW_COUNT) ? (COOP_SETUP_ROW_COUNT - 1) : row);
        g_FrontCursorX = COOP_SETUP_CURSOR_X;
        g_FrontCursorY = COOP_SETUP_ROW_Y(row) + 6;
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CONFIDENTIAL2, 1);
    frontMoveCursor();

    if (g_FrontTabPrevSelected || g_FrontTabNextSelected)
    {
        fileCoopSavePartnerSettings();
        fileSaveDisplaySettingsForFolder(g_FrontFolder);
    }

    if (g_FrontTabPrevSelected)
    {
        // Mode select clears co-op once it opens; the cursor goes back to
        // the co-op row.
        frontSetMenu(MENU_MODE_SELECT, FALSE);
        frontSetModeCursor(GAMEMODE_SOLO);
        return;
    }

    if (g_FrontTabNextSelected)
    {
        frontSetMenu(MENU_MISSION_SELECT, FALSE);
        frontSetMissionCursor(0);
    }
}


Gfx *frontCoopSetupRender(Gfx *DL)
{
    u8 *text;
    s32 x;
    s32 y;
    s32 textwidth;
    s32 textheight;
    s32 row;

    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
    #if defined(BUGFIX_R2)
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
    #endif
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);

    x = 0x37;
    y = 0x5F;
    DL = frontPrintText(DL, &x, &y, (u8 *)(g_CounterOpMode ? "2 Player Counter-op\n" : "2 Player Co-op\n"), g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    for (row = 0; row < COOP_SETUP_ROW_COUNT; row++)
    {
        text = (u8 *)coop_setup_row_labels[row];

        if (g_CounterOpMode && (row == COOP_SETUP_ROW_CHAR))
        {
            text = (u8 *)"Bond:\n";
        }

        textMeasure(&textheight, &textwidth, text, g_FontZurichChars, g_FontZurich, 0);

        if (coop_setup_highlighted_row == row)
        {
            DL = textRenderRect(DL, 0x37, COOP_SETUP_ROW_Y(row) - 1, textwidth + 0x3C, COOP_SETUP_ROW_Y(row) + 0xE, 0x32);
        }

        x = 0x39;
        y = COOP_SETUP_ROW_Y(row);
        DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

        switch (row)
        {
            case COOP_SETUP_ROW_CHAR:
                if (g_CounterOpMode)
                {
                    text = (u8 *)(g_CounterOpBondPad ? "Player 2\n" : "Player 1\n");
                    break;
                }

                text = frontCoopGetPartnerCharName();
                break;
            case COOP_SETUP_ROW_CONTROLS:
                text = (u8 *)langGet(g_MpControlStyles[g_MpControlStyle[PLAYER_2]].text_preset);
                break;
            case COOP_SETUP_ROW_SPLIT:
                text = (u8 *)(optionsGetSplitStacked() ? "Stacked\n" : "Side by Side\n");
                break;
            case COOP_SETUP_ROW_HIRES:
                text = (u8 *)(optionsGetHiRes() ? "On\n" : "Off\n");
                break;
            case COOP_SETUP_ROW_DRAWDIST:
                text = (u8 *)(optionsGetDrawNear() ? "Near\n" : "Full\n");
                break;
        }

        x = COOP_SETUP_VALUE_X;
        y = COOP_SETUP_ROW_Y(row);
        DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    }

    x = 0x39;
    y = COOP_SETUP_ROW_Y(COOP_SETUP_ROW_COUNT) + 0x14;
    DL = frontPrintText(DL, &x, &y, (u8 *)"A or Left/Right: change\n", g_FontZurichChars, g_FontZurich, 0x70, viGetX(), viGetY(), 0, 0);

    DL = frontAddPreviousTabText(DL);
    DL = frontAddNextTabText(DL);
    DL = frontDrawCursor(DL);
    return DL;
}


s32 frontFindMissionEntry(s32 mission)
{
    int i;

    for (i=0; g_FrontMissions[i].folder_text_preset != 0 ; i++)
    {
        if (mission == g_FrontMissions[i].mission_num)
        {
            return i;
        }
    }

    return -1;
}


s32 frontIsMissionCompleted(int stage, DIFFICULTY difficulty)
{
    s32 num;

    num = g_FrontMissions[stage].mission_num;
    if (num >= 0)
    {
        return fileIsStageUnlockedAtDifficulty(g_FrontFolder, num, difficulty);
    }
    return 0;
}


s32 frontGetHighestDifficulty(s32 mission)
{
    s32 num;
    s32 difficulty;
    s32 unlocked;

    if (g_FrontMissions[frontFindMissionEntry(mission)].stage_id >= 0)
    {
        num = DIFFICULTY_00;
        if (fileIs007ModeUnlocked(g_FrontFolder))
        {
            num = DIFFICULTY_007;
        }

        for (difficulty=num; difficulty >= 0; difficulty--)
        {
            unlocked = fileIsStageUnlockedAtDifficulty(g_FrontFolder, mission, difficulty);
            if (g_AppendCheatSinglePlayer == 0)
            {
                if (unlocked != DIFFICULTY_AGENT)
                {
                    return difficulty;
                }
            }
            else if (unlocked == DIFFICULTY_007)
            {
                return difficulty;
            }
        }
    }

    return -1;
}


//********************************************************************************************************
//MISSION SELECT
//********************************************************************************************************
void frontMissionSelectEnter(void)
{
    g_FrontStage = LEVELID_NONE;
    g_FrontMission = BRIEFING_INVALID;
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    // Co-op is chosen on the mode select page and lasts until the player
    // goes back there.
    if (joyGetControllerCount() < 2)
    {
        g_CoopMode = FALSE;
        g_CounterOpMode = FALSE;
    }
    if (g_FrontStartMusic)
    {
        musicTrack1Play(M_FOLDERS);
        g_FrontStartMusic = FALSE;
    }
    frontLoadFolders();
}


void frontMissionSelectLeave(void) 
{
    return;
}


void frontMissionSelectTick(void)
{
    s32 column;
    s32 row;
    struct ModelRwData_DisplayList_CollisionRecord *rwdata;
    s32 mission;
    s32 i;
    s32 scan;
    struct ModelRoData_DisplayList_CollisionRecord *rodata;
    ModelNode *mnode;

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);
    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], 0, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], 0x13, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], 0x14, 1);
    g_FrontTabPrevHighlight = 0;
    g_FrontDifficultyHighlight = -1;

    if (frontCheckCursorOnPreviousTab() != 0)
    {
        g_FrontTabPrevHighlight = 1;
    }
    else
    {
        for (column = 0; column < 5; )
        {
            if (column < 4)
            {
                if(!((g_FrontMissionCursorX[column] + g_FrontMissionCursorX[column+1]) * 0.5f > g_FrontCursorX))
                {
                    column++;
                    continue;
                }
            }

            break;
        }

        for (row = 0; row < 4; )
        {
            if (row < 3)
            {
                if(!((g_FrontMissionCursorY[row] + g_FrontMissionCursorY[row+1]) * 0.5f > g_FrontCursorY))
                {
                    row++;
                    continue;
                }
            }

            break;
        }

        for ( ; row > 0; row--)
        {
            for (scan = 0; scan < 5; scan++)
            {
                if (frontGetHighestDifficulty((row * 5) + scan) >= 0)
                {
                    break;
                }
            }

            if (scan < 5)
            {
                break;
            }

        }

        for ( ; column >= 0; column--)
        {
            if (frontGetHighestDifficulty((row * 5) + column) >= 0)
            {
                break;
            }
        }

        if (column < 0)
        {
            for (column = 0; column < 5; column++)
            {
                if (frontGetHighestDifficulty((row * 5) + column) >= 0)
                {
                    break;
                }
            }
        }

        g_FrontDifficultyHighlight = (row * 5) + column;
    }

    mnode = g_FrontFolderModels[0]->obj->Switches[0x15];

    if (mnode != NULL)
    {
        rodata = (struct ModelRoData_DisplayList_CollisionRecord *)mnode->Data;

        rwdata = (struct ModelRwData_DisplayList_CollisionRecord *)modelGetNodeRwData(g_FrontFolderModels[0], mnode);
        rwdata->Vertices = dynAllocateVertices(rodata->numVertices);

        for (i = 0; i < rodata->numVertices; i++)
        {
            // struct copy
            rwdata->Vertices[i] = rodata->Vertices[i];

            mission = i / 4;

            if (frontGetHighestDifficulty(mission) >= 0)
            {
                if (mission == g_FrontDifficultyHighlight)
                {
                    rwdata->Vertices[i].r = 0xff;
                    rwdata->Vertices[i].g = 0xff;
                    rwdata->Vertices[i].b = 0xff;
                    rwdata->Vertices[i].a = 0xf5;
                }
                else
                {
                    rwdata->Vertices[i].r = 0x6e;
                    rwdata->Vertices[i].g = 0x6e;
                    rwdata->Vertices[i].b = 0x6e;
                    rwdata->Vertices[i].a = 0xff;
                }
            }
            else
            {
                rwdata->Vertices[i].r = 0xf;
                rwdata->Vertices[i].g = 0xf;
                rwdata->Vertices[i].b = 0xf;
                rwdata->Vertices[i].a = 0xff;
            }
        }
    }

    if (joyGetButtonsPressedThisFrame(0, A_BUTTON | START_BUTTON | Z_TRIG) != 0)
    {
        if (g_FrontTabPrevHighlight != 0)
        {
            g_FrontTabPrevSelected = 1;

            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
        else if (g_FrontDifficultyHighlight >= 0)
        {
            g_FrontMission = frontFindMissionEntry(g_FrontDifficultyHighlight);
            g_FrontStage = g_FrontMissions[g_FrontMission].stage_id;
            g_FrontTabNextSelected = 1;

            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
    }
    else if (joyGetButtonsPressedThisFrame(0, B_BUTTON) != 0)
    {
        g_FrontTabPrevSelected = 1;

        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    frontMoveCursor();

    if (g_FrontTabNextSelected != 0)
    {
        frontSetMenu(MENU_DIFFICULTY, 0);

        frontSetDifficultyCursor(frontGetHighestDifficulty(g_FrontMissions[g_FrontMission].mission_num));
    }
    else if (g_FrontTabPrevSelected != 0)
    {
        frontSetMenu(MENU_MODE_SELECT, 0);
        frontSetModeCursor(0);
    }
}


void frontSetMissionCursor(LEVEL_SOLO_SEQUENCE level)
{
    g_FrontCursorX = g_FrontMissionCursorX[level % 5];
    g_FrontCursorY = g_FrontMissionCursorY[level / 5];
}

Gfx *frontMissionSelectRender(Gfx *DL)
{
    s32 i;
    s32 column;
    s32 height;
    s32 width;
    s32 entry;
    s32 colour;
    u8 label[24];
    s32 x;
    s32 y;
    s32 c;
    s32 mission;

    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
    gDPSetFogColor(DL++, 0xFF, 0xFF, 0xFF, 0xFF);
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);

    for (column = 0; column < 5; column++)
    {
        mission = column;

        for (i = 0; i < 4 ; i++, mission += 5)
        {
            entry = frontFindMissionEntry(mission);

            if (frontGetHighestDifficulty(mission) >= 0)
            {
                colour = 0x96969600;

                if (mission == g_FrontDifficultyHighlight)
                {
                    colour = -0x100;
                }

                if (g_FrontMissions[entry].icon_text_preset)
                {
                    strcpy(label, langGet(g_FrontMissions[entry].icon_text_preset));
                }
                else
                {
                    strcpy(label, langGet(g_FrontMissions[entry].folder_text_preset));
                }

                c = strlen(label) - 1;
                for (; c >= 0; c--)
                {
                    if (label[c] >= 'a')
                    {
                        label[c] -= 0x20;
                    }
                }

                strcat(label, "\n");

                width = 0;
                height = 0;

                textMeasure(&height, &width, label, g_FontBankGothicChars, g_FontBankGothic, 0);

                x = g_FrontMissionCursorX[column] - 0x1F;
                y = (g_FrontMissionCursorY[i] - height) + 0x1D;

                DL = textRenderRect(DL, x, y, x + width, y + height, 0);
                DL = textRender(DL, &x, &y, label, g_FontBankGothicChars, g_FontBankGothic, colour | 0xFF, viGetX(), viGetY(), 0, 0);

                x = g_FrontMissionCursorX[column] - 0x1F;
                y = (g_FrontMissionCursorY[i] - height) + 0x1D;

                DL = textRender(DL, &x, &y, label, g_FontBankGothicChars, g_FontBankGothic, colour | 0x64, viGetX(), viGetY(), 0, 0);

            }
        }
    }

    DL = frontAddPreviousTabText(DL);
    DL = frontDrawCursor(DL);
    return DL;
}


//********************************************************************************************************
//DIFFICULTY SELECT
//********************************************************************************************************
void frontDifficultyEnter(void)
{
    g_FrontDifficulty = DIFFICULTY_MULTI;
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    frontLoadFolders();
}

void frontDifficultyLeave(void) {
    return;
}


void frontDifficultyTick(void)
{
    s32 unlockedDifficulty;

    unlockedDifficulty = frontGetHighestDifficulty(g_FrontMissions[g_FrontMission].mission_num);
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(FALSE);
    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CONFIDENTIAL, 1);
    g_FrontTabPrevHighlight = FALSE;
    g_FrontDifficultyHighlight = DIFFICULTY_MULTI;

    if (frontCheckCursorOnPreviousTab())
    {
        g_FrontTabPrevHighlight = TRUE;
    }
    else if ((unlockedDifficulty >= DIFFICULTY_007) && (g_FrontCursorY >= 275.0f))
    {
        g_FrontDifficultyHighlight = DIFFICULTY_007;
    }
    else if ((unlockedDifficulty >= DIFFICULTY_00) && (g_FrontCursorY >= 243.0f))
    {
        g_FrontDifficultyHighlight = DIFFICULTY_00;
    }
    else if ((unlockedDifficulty > DIFFICULTY_AGENT) && (g_FrontCursorY >= 211.0f))
    {
        g_FrontDifficultyHighlight = DIFFICULTY_SECRET;
    }
    else
    {
        g_FrontDifficultyHighlight = DIFFICULTY_AGENT;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON|Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabPrevHighlight)
        {
            g_FrontTabPrevSelected = TRUE;

            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
        else
        {
            if (g_FrontDifficultyHighlight >= DIFFICULTY_AGENT)
            {
                g_FrontDifficulty = g_FrontDifficultyHighlight;
                g_FrontTabNextSelected = TRUE;

                sndPlaySfx(g_musicSfxBufferPtr, PAPER_TURN_SFX, NULL);
            }
        }
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
    {
        g_FrontTabPrevSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    frontMoveCursor();

    if (g_FrontTabNextSelected)
    {
        if (g_FrontDifficulty == DIFFICULTY_007)
        {
            frontSetMenu(MENU_007_OPTIONS, FALSE);
            frontSetCursorPositionToNextTab();

            return;
        }

        frontSetMenu(MENU_BRIEFING, FALSE);
        frontSetCursorPositionToNextTab();

        return;
    }
    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_MISSION_SELECT, FALSE);
        frontSetMissionCursor(g_FrontMissions[g_FrontMission].mission_num);
    }
}


Gfx * frontDrawMissionTitle(Gfx *DL, char *text)
{
    s32 x;
    s32 y;

    int chapter;

    if (g_FrontDifficulty >= DIFFICULTY_AGENT)
    {
        strcpy(text, frontGetDifficultyName(g_FrontDifficulty));
        strcat(text, langGet(getStringID(LTITLE, TITLE_STR_32_JB)));
        x = 0x37;
        y = 0x57;
        DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xff, viGetX(), viGetY(), 0, 0);
    }

    chapter = frontGetChapterStart(g_FrontMission);
    if (chapter >=0 )
    {
        strcpy(text, langGet(getStringID(LTITLE, TITLE_STR_33_MISSION2)));
        strcat(text, g_FrontMissions[chapter].string_ptr);
        strcat(text, ": ");
        strcat(text, langGet(g_FrontMissions[chapter].folder_text_preset));
        strcat(text, "\n");
        x = 0x37;
        y = 0x67;
        DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xff, viGetX(), viGetY(), 0, 0);
    }

    strcpy(text, langGet(getStringID(LTITLE, TITLE_STR_34_PART)));
    strcat(text, g_FrontMissions[g_FrontMission].string_ptr);
    strcat(text, ": ");
    strcat(text, langGet(g_FrontMissions[g_FrontMission].folder_text_preset));
    strcat(text, "\n");
    x = 0x37;
    y = 0x77;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xff, viGetX(), viGetY(), 0, 0);
    return DL;
}


void frontSetDifficultyCursor(int difficulty)
{
    g_FrontCursorX = 106.0;
    g_FrontCursorY = difficulty * 0x1e + 0xba;
}




Gfx *frontDifficultyRender(Gfx *DL)
{
    u8 *text_sp3180;
    s32 unlockedDifficulty;
    s32 x;
    s32 y;

    char stagename[3000]; /* frontDrawMissionTitle's line buffer */

    u8 *text; // sp ??
    s32 width;
    s32 height;
    f32 xypos[2];
    f32 halfedxy[2];
    struct sImageTableEntry *image;   // sp ??
    s32 i;

    unlockedDifficulty = frontGetHighestDifficulty(g_FrontMissions[g_FrontMission].mission_num);

    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
#ifdef VERSION_EU
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
#endif
    DL = frontSetupMenuBackground(DL);


    DL = textSetupDraw(DL);
    DL = frontDrawMissionTitle(DL, stagename);

    //  "DIFFICULTY:\n"
    text_sp3180 = langGet(getStringID(LTITLE, TITLE_STR_35_DIFFICULTY));
    x = 0x37;
    y = 0x8F;
    DL = frontPrintText(DL, &x, &y, text_sp3180, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    if (g_FrontDifficultyHighlight >= 0)
    {
        DL = textRenderRect(
            DL,
            0x7E,
            (g_FrontDifficultyHighlight * 0x1E) + 0xB2,
            0xF0,
            (g_FrontDifficultyHighlight * 0x1E) + 0xC3,
            0x32);
    }

    /**
     * Draw the list of available difficulties. Each item is
     * prefixed with a number.
    */
    for (i=0; i<DIFFICULTY_MAX; i++)
    {
        if (i == 0 || unlockedDifficulty >= i)
        {
            sprintf(stagename, "%d.\n", i + 1);
            text = (u8 *) stagename;

            textMeasure(&height, &width, text, g_FontZurichChars, g_FontZurich, 0);

            x = 0x82;
            y = (i * 0x1E) + 0xB4;
            DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

            switch (i)
            {
                case 0:
                    text = langGet(getStringID(LTITLE, TITLE_STR_36_AGENT2)); // "Agent\n"
                    break;
                case 1:
                    text = langGet(getStringID(LTITLE, TITLE_STR_37_SECRETAGENT2)); // "Secret Agent\n"
                    break;
                case 2:
                    text = langGet(getStringID(LTITLE, TITLE_STR_38_00AGENT2)); // "00 Agent\n"
                    break;
                case 3:
                    text = langGet(getStringID(LTITLE, TITLE_STR_39_0072)); // "007\n"
                    break;
            }

            x = 0x96;
            y = (i * 0x1E) + 0xB4;
            DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }
    }

    if (g_CoopMode)
    {
        x = 0x37;
        y = 0x12C;
        DL = frontPrintText(DL, &x, &y, (u8 *)(g_CounterOpMode ? "2 PLAYER COUNTER-OP\n" : "2 PLAYER CO-OP\n"),
            g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    }

    DL = frontAddPreviousTabText(DL);

    /**
     * Draw the checkmark for completed difficulties.
    */
    for (i=0; i<3; i++)
    {
        if ((i == 0 || unlockedDifficulty >= i)
            && (frontIsMissionCompleted(g_FrontMission, i) == 3))
        {
            image = &g_ImagesMainFolder[IMAGE_CHECK];

            xypos[0] = 280.0f;
            xypos[1] = (f32) ((i * 0x1E) + 0xBA);

            halfedxy[0] = image->width * 0.5f;
            halfedxy[1] = image->height * 0.5f;

            texSelect(&DL, image, 4, 0, 0);
            texRectDrawImage(&DL, &xypos, &halfedxy, image->width, image->height, 0, 0, 1, 0xB4, 0, 0, 0xFF, image->level > 0, 0);
        }
    }

    DL = frontDrawCursor(DL);

    return DL;
}

//********************************************************************************************************
//007 DIFFICULTY SELECT
//********************************************************************************************************

void frontOptions007Enter(void)
{
    g_FrontTabStartSelected = 0;
    g_FrontTabNextSelected = 0;
    g_FrontTabPrevSelected = 0;
    g_FrontTabPrevHighlight = 0;
    g_FrontTabNextHighlight = 0;
    g_FrontTabStartHighlight = 0;
    g_Front007HlReaction = 0;
    g_Front007HlHealth = 0;
    g_Front007HlDamage = 0;
    g_Front007HlAccuracy = 0;
    frontLoadFolders();
}


void frontOptions007Leave(void) 
{
    return;
}


void frontOptions007Tick(void)
{
    f32 frac;
    s32 cursorY;

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(FALSE);
    if (joyGetButtons(PLAYER_1, Z_TRIG|A_BUTTON) == 0)
    {
        g_FrontTabPrevHighlight = FALSE;
        g_FrontTabNextHighlight = FALSE;
        g_FrontTabStartHighlight = FALSE;
        g_Front007HlReaction = FALSE;
        g_Front007HlHealth = FALSE;
        g_Front007HlDamage = FALSE;
        g_Front007HlAccuracy = FALSE;

        if (frontCheckCursorOnPreviousTab())
        {
            g_FrontTabPrevHighlight = TRUE;
        }
        else if (frontCheckCursorOnNextTab())
        {

            g_FrontTabNextHighlight = TRUE;
        }
        else if (frontCheckCursorOnStartTab())
        {
            g_FrontTabStartHighlight = TRUE;
        }
        else
        {
            cursorY = (s32) g_FrontCursorY;
            if (cursorY >= 0x107)
            {
                g_Front007HlReaction = TRUE;
            }
            else if (cursorY >= 0xE6)
            {
                g_Front007HlAccuracy = TRUE;
            }
            else if (cursorY >= 0xC5)
            {
                g_Front007HlDamage = TRUE;
            }
            else if (cursorY >= 0xA4)
            {
                g_Front007HlHealth = TRUE;
            }
            else
            {
                g_FrontTabNextHighlight = TRUE;
            }
        }
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON))
    {
        g_FrontTabStartSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabNextHighlight)
        {
            g_FrontTabNextSelected = TRUE;
        }
        else if (g_FrontTabPrevHighlight)
        {
            g_FrontTabPrevSelected = TRUE;
        }
        else if (g_FrontTabStartHighlight)
        {
            g_FrontTabStartSelected = TRUE;
        }
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
    {
        g_FrontTabPrevSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    if (joyGetButtons(PLAYER_1, Z_TRIG|A_BUTTON))
    {
        frac = (g_FrontCursorX - 55.0f) / 300.0f;

        if (frac > 1.0f)
        {
            frac = 1.0f;
        }

        if (frac < 0.0f)
        {
            frac = 0.0f;
        }

        if (g_Front007HlReaction)
        {
            g_Front007Reaction = frac;
        }
        else if (g_Front007HlHealth)
        {
            g_Front007Health = (f32) (frac * frac * 10.0f);
        }
        else if (g_Front007HlDamage)
        {
            g_Front007Damage = (f32) (frac * frac * 10.0f);
        }
        else if (g_Front007HlAccuracy)
        {
            g_Front007Accuracy = (f32) (frac * frac * 10.0f);
        }
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CLASSIFIED, 1);
    frontMoveCursor();

    if (g_FrontTabStartSelected)
    {
        frontSetMenu(MENU_RUN_STAGE, TRUE);
        return;
    }

    if (g_FrontTabNextSelected)
    {
        frontSetMenu(MENU_BRIEFING, FALSE);
        return;
    }

    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_DIFFICULTY, FALSE);
        frontSetDifficultyCursor(g_FrontDifficulty);
    }
}



/**
 * Render the 007 Mode options. The slider bars are 300 pixels wide and
 * sit 33 pixels apart vertically.
 */
Gfx *frontOptions007Render(Gfx *DL)
{
    u8 *labeltext;
    s32 textx;
    s32 texty;
    char textbuffer[3000];
    s32 textheight;
    s32 textwidth;
    s32 barwidth;

    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
#if defined(VERSION_EU)
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
#endif
    DL = frontSetupMenuBackground(DL);

    DL = textSetupDraw(DL);
    DL = frontDrawMissionTitle(DL, textbuffer);

    labeltext = langGet(getStringID(LTITLE, TITLE_STR_40_SPECOPS)); /* SPECIAL OPTIONS */

    textx = 55;
    texty = 143;

    DL = frontPrintText(DL, &textx, &texty, (s8 *)labeltext, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    /**
     * ENEMY HEALTH
     */
    labeltext = langGet(getStringID(LTITLE, TITLE_STR_42_HEALTH)); /* Enemy health */

    textx = 57;
    texty = 164;

    // Render the bar background with a black color and an opacity of 50/255.
    DL = textRenderRect(DL, 55, 181, 355, 192, 0x00000032);

    barwidth = (sqrtf(g_Front007Health / 10.0f)) * 300.0f;

    // Render the filled portion of the bar with a black color and an opacity of 100/255.
    DL = textRenderRect(DL, 55, texty + 17, barwidth + 55, texty + 28, 0x00000064);

    if (g_Front007HlHealth != 0)
    {
        // Render a black rectangle over the "Enemy health" text to indicate it's highlighted.
        DL = textRenderRect(DL, 55, texty - 1, 199, texty + 14, 0x00000032);
    }

    // Render "Enemy health" text.
    DL = frontPrintText(DL, &textx, &texty, (s8 *)labeltext, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    sprintf(textbuffer, "%d%%\n", (s32) (g_Front007Health * 100.0f));

    textwidth = 0;
    textheight = 0;

    textMeasure(&textheight, &textwidth, textbuffer, g_FontZurichChars, g_FontZurich, 0);

    textx = 285 - textwidth;
    texty = 164;

    // Render enemy health percentage text.
    DL = frontPrintText(DL, &textx, &texty, (s8*)textbuffer, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    /**
     * ENEMY DAMAGE
     */
    labeltext = langGet(getStringID(LTITLE, TITLE_STR_43_DAMAGE)); /* Enemy damage */

    textx = 57;
    texty = 197;

    DL = textRenderRect(DL, 55, 214, 355, 225, 0x00000032);
    barwidth = (sqrtf(g_Front007Damage / 10.0f)) * 300.0f;
    DL = textRenderRect(DL, 55, texty + 17, barwidth + 55, texty + 28, 0x00000064);

    if (g_Front007HlDamage != 0)
    {
        DL = textRenderRect(DL, 55, texty - 1, 199, texty + 14, 0x00000032);
    }

    DL = frontPrintText(DL, &textx, &texty, (s8*)labeltext, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    sprintf(textbuffer, "%d%%\n", (s32) (g_Front007Damage * 100.0f));

    textwidth = 0;
    textheight = 0;

    textMeasure(&textheight, &textwidth, textbuffer, g_FontZurichChars, g_FontZurich, 0);

    textx = 285 - textwidth;
    texty = 197;

    // Render enemy damage percentage text.
    DL = frontPrintText(DL, &textx, &texty, (s8*)textbuffer, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    /**
     * ENEMY ACCURACY
     */
    labeltext = langGet(getStringID(LTITLE, TITLE_STR_44_ACCURACY)); /* Enemy accuracy */

    textx = 57;
    texty = 230;

    DL = textRenderRect(DL, 55, 247, 355, 258, 0x00000032);
    barwidth = (sqrtf(g_Front007Accuracy / 10.0f)) * 300.0f;
    DL = textRenderRect(DL, 55, texty + 17, barwidth + 55, texty + 28, 0x00000064);

    if (g_Front007HlAccuracy != 0)
    {
        DL = textRenderRect(DL, 55, texty - 1, 199, texty + 14, 0x00000032);
    }

    DL = frontPrintText(DL, &textx, &texty, (s8*)labeltext, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    sprintf(textbuffer, "%d%%\n", (s32) (g_Front007Accuracy * 10.0f));

    textwidth = 0;
    textheight = 0;

    textMeasure(&textheight, &textwidth, textbuffer, g_FontZurichChars, g_FontZurich, 0);

    textx = 285 - textwidth;
    texty = 230;

    // Render enemy accuracy percentage text.
    DL = frontPrintText(DL, &textx, &texty, (s8*)textbuffer, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    /**
     * ENEMY REACTION
     */
    labeltext = langGet(getStringID(LTITLE, TITLE_STR_41_REACTION)); /* Enemy reaction speed */

    textx = 57;
    texty = 263;

    DL = textRenderRect(DL, 55, 280, 355, 291, 0x00000032);
    DL = textRenderRect(DL, 55, texty + 17, (s32) (g_Front007Reaction * 300.0f * 1) + 55, texty + 28, 0x00000064);

    if (g_Front007HlReaction != 0)
    {
        DL = textRenderRect(DL, 55, texty - 1, 199, texty + 14, 0x00000032);
    }

    DL = frontPrintText(DL, &textx, &texty, (s8*)labeltext, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    sprintf(textbuffer, "%d%%\n", (s32) (g_Front007Reaction * 100.0f));

    textwidth = 0;
    textheight = 0;

    textMeasure(&textheight, &textwidth, textbuffer, g_FontZurichChars, g_FontZurich, 0);

    textx = 285 - textwidth;
    texty = 263;

    // Render enemy reaction speed percentage text.
    DL = frontPrintText(DL, &textx, &texty, (s8*)textbuffer, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    DL = frontAddStartTabText(DL);
    DL = frontAddPreviousTabText(DL);
    DL = frontAddNextTabText(DL);
    DL = frontDrawCursor(DL);

    return DL;
}


u32 frontGetMpControlStyle(s32 playernum)
{
  return g_MpControlStyle[playernum];
}


s32 frontIsMpStageAvailable(s32 stage)
{
    s32 players;
    if (!g_MpStages[stage].min_player)
    {
        return FALSE;
    }
    players=frontGetPlayerCount();
    if (g_MpStages[stage].max_player < players)
    {
        return FALSE;
    }
    if (g_MpStages[stage].unlock_after == -1)
    {
        return TRUE;
    }
    if ( fileIsStageUnlockedAtDifficulty(g_FrontFolder, g_MpStages[stage].unlock_after, DIFFICULTY_AGENT))
    {
        return TRUE;
    }
    return FALSE;
}


void frontStepMpGameLength(void)
{
    if (g_MpScenario != SCENARIO_TLD)
    {
        g_MpGameLength = (s32) ((s32) (g_MpGameLength + 1) % 7);
        return;
    }
    g_MpGameLength = (s32) ((s32) (g_MpGameLength + 1) % 4);
}


void frontApplyMpAimSettings(void)
{
    s32 i;
    struct player_data * p;

    for (i = 0; i < 4; i++)
    {
        p = &g_playerPlayerData[i];
        p->autoaim = g_MpAimSettings[g_MpAimSetting].autoaim;
        p->sight   = g_MpAimSettings[g_MpAimSetting].sight;
    }
}


void frontStepMpAimSettings(void)
{
    g_MpAimSetting = (s32) ((s32) (g_MpAimSetting + 1) % 4);
    frontApplyMpAimSettings();
}


void frontUnlockAllMpChars(void) {
    g_MpCharCount = 0x40;
}


s32 frontGetPlayerCount(void)
{
    if (g_FrontGameMode == GAMEMODE_MULTI)
    {
        return g_MpPlayerCount;
    }
    if (g_CoopMode)
    {
        return 2;
    }
    return 1;
}

u16 frontGetMpCharHead(s32 player)
{
    if (g_MpChar[player] < 0)
    {
        return g_MpChars[player].head;
    }
    return g_MpChars[g_MpChar[player]].head;
}

u8 frontGetMpCharGender(s32 player)
{
    if (g_MpChar[player] < 0)
    {
        return g_MpChars[player].gender;
    }
    return g_MpChars[g_MpChar[player]].gender;
}

u16 frontGetMpCharBody(s32 player)
{
    if (g_MpChar[player] < 0)
    {
        return g_MpChars[player].body;
    }
    return g_MpChars[g_MpChar[player]].body;
}

/**
 * Co-op player 2's character, saved with player 2's settings
 * (fileCoopGetPartnerChar):
 *   COOP_CHAR_BOND_AUTO        Bond, in an outfit that goes with player 1's
 *   1..COOP_BOND_OUTFIT_COUNT  Bond in that outfit (coop_bond_outfits)
 *   above that                 multiplayer character
 *                              g_MpChars[char - COOP_BOND_OUTFIT_COUNT]
 * (g_MpChars[0] is Bond in his tuxedo, which the outfits cover.)
 */
#define COOP_CHAR_BOND_AUTO 0
#define COOP_BOND_OUTFIT_COUNT 5

struct coop_bond_outfit {
    char *name;
    s32 cuff;
};

static struct coop_bond_outfit coop_bond_outfits[COOP_BOND_OUTFIT_COUNT] = {
    {"Bond: Suit\n",       CUFF_BLUE},
    {"Bond: Tuxedo\n",     CUFF_BROSNAN},
    {"Bond: Boiler Suit\n", CUFF_BOILER},
    {"Bond: Jungle\n",     CUFF_JUNGLE},
    {"Bond: Parka\n",      CUFF_SNOW},
};

/**
 * The outfits player 2's Bond can wear next to player 1's, one picked at
 * random per mission. Bond has only one cold weather outfit.
 */
static s32 coop_outfits_blue[]    = {CUFF_BROSNAN, CUFF_BOILER, -1};
static s32 coop_outfits_tuxedo[]  = {CUFF_BLUE, -1};
static s32 coop_outfits_boiler[]  = {CUFF_BLUE, CUFF_JUNGLE, -1};
static s32 coop_outfits_jungle[]  = {CUFF_BOILER, -1};
static s32 coop_outfits_snow[]    = {CUFF_BOILER, -1};

static s32 s_CoopPartnerCuff = -1;

/**
 * Number of multiplayer characters unlocked, as on the character select page.
 */
static s32 frontCoopMpCharCount(void)
{
    if (g_MpCharCount == 0x40)
    {
        return g_MpCharCount;
    }

    if (fileIsStageUnlockedAtDifficulty(g_FrontFolder, SP_LEVEL_CRADLE, DIFFICULTY_AGENT) == STAGESTATUS_COMPLETED)
    {
        return 0x21;
    }

    return 8;
}

static s32 frontCoopCharCount(void)
{
    return 1 + COOP_BOND_OUTFIT_COUNT + (frontCoopMpCharCount() - 1);
}

static s32 frontCoopGetPartnerChar(void)
{
    s32 character = fileCoopGetPartnerChar();

    if ((character < 0) || (character >= frontCoopCharCount()))
    {
        return COOP_CHAR_BOND_AUTO;
    }

    return character;
}

/**
 * The name shown on the co-op setup page.
 */
static u8 *frontCoopGetPartnerCharName(void)
{
    s32 character = frontCoopGetPartnerChar();

    if (character == COOP_CHAR_BOND_AUTO)
    {
        return (u8 *)"Bond\n";
    }

    if (character <= COOP_BOND_OUTFIT_COUNT)
    {
        return (u8 *)coop_bond_outfits[character - 1].name;
    }

    return (u8 *)langGet(g_MpChars[character - COOP_BOND_OUTFIT_COUNT].text_preset);
}

/**
 * Forget player 2's random outfit, so the next mission picks a new one.
 */
void frontCoopResetPartnerOutfit(void)
{
    s_CoopPartnerCuff = -1;
}

s32 frontCoopPartnerIsBond(void)
{
    return g_CoopMode && (frontCoopGetPartnerChar() <= COOP_BOND_OUTFIT_COUNT);
}

/**
 * Player 2's cuff type (their Bond outfit and the cuffs on their hands),
 * given player 1's. A multiplayer character keeps player 1's hands.
 */
s32 frontCoopGetPartnerCuff(s32 player1cuff)
{
    s32 character = frontCoopGetPartnerChar();
    s32 *outfits;
    s32 count;

    if (character > COOP_BOND_OUTFIT_COUNT)
    {
        return player1cuff;
    }

    if (character != COOP_CHAR_BOND_AUTO)
    {
        return coop_bond_outfits[character - 1].cuff;
    }

    if (s_CoopPartnerCuff >= 0)
    {
        return s_CoopPartnerCuff;
    }

    switch (player1cuff)
    {
        case CUFF_BLUE:    outfits = coop_outfits_blue;   break;
        case CUFF_JUNGLE:  outfits = coop_outfits_jungle; break;
        case CUFF_BOILER:  outfits = coop_outfits_boiler; break;
        case CUFF_SNOW:    outfits = coop_outfits_snow;   break;
        default:           outfits = coop_outfits_tuxedo; break;
    }

    for (count = 0; outfits[count] >= 0; count++)
    {
    }

    s_CoopPartnerCuff = outfits[randomGetNext() % (u32)count];
    return s_CoopPartnerCuff;
}

u16 frontCoopGetPartnerHead(void)
{
    return g_MpChars[frontCoopGetPartnerChar() - COOP_BOND_OUTFIT_COUNT].head;
}

u16 frontCoopGetPartnerBody(void)
{
    return g_MpChars[frontCoopGetPartnerChar() - COOP_BOND_OUTFIT_COUNT].body;
}

f32 frontGetMpHandicap(s32 player)
{
    return g_MpHandicaps[g_MpHandicap[player]].damage_modifier;
}

f32 frontGetMpCharHeight(s32 player)
{
    if (g_MpChar[player] < 0) {
        return g_MpChars[player].pov;
    }
    return g_MpChars[g_MpChar[player]].pov;
}

s32 frontGetMpTimeLimit(void) {
  return g_MpGameLengths[g_MpGameLength].time;
}

s32 frontGetMpPointLimit(void) {
  return g_MpGameLengths[g_MpGameLength].points;
}

void frontSetMpScenario(MPSCENARIOS scenarioid)
{
    g_MpScenario = scenarioid;

    g_MpCanPickStage = 1;
    g_MpCanPickLength = 1;
    g_MpCanPickChar = 1;
    g_MpCanPickWeapons = 1;
    g_MpCanPickHandicap = 1;
    g_MpCanPickControls = 1;
    g_MpCanPickAim = 1;


    switch(g_MpScenario)
    {
    case SCENARIO_NORMAL:
    case SCENARIO_2v2:
    case SCENARIO_3v1:
    case SCENARIO_2v1:
        if (6 < g_MpGameLength)
        {
            g_MpGameLength = 2;
        }
        if (0xd < mpWeaponSetGetIndex())
        {
            mpWeaponSetSelect(0xd);
        }
        break;
    case SCENARIO_YOLT:
        g_MpGameLength = 7;

        if (0xd < mpWeaponSetGetIndex())
        {
            mpWeaponSetSelect(0xd);
        }
        g_MpCanPickLength = 0;
        break;
    case SCENARIO_TLD:
        if (3 < g_MpGameLength)
        {
            g_MpGameLength = 2;
        }
        if (0xd < mpWeaponSetGetIndex())
        {
            mpWeaponSetSelect(0xd);
        }
        break;
    case SCENARIO_MWTGG:
        if (6 < g_MpGameLength)
        {
            g_MpGameLength = 2;
        }
        mpWeaponSetSelect(0xd);
        g_MpCanPickWeapons = 0;
        break;
    case SCENARIO_LTK:
        if (6 < g_MpGameLength)
        {
            g_MpGameLength = 2;
        }
        mpWeaponSetSelect(1);
        g_MpCanPickHandicap = 0;
    }
    return;
}


void frontSetMpPlayerCount(s32 numplayers)
{
    s32 i;

    if (numplayers < 2)
    {
        numplayers = 2;
    }

    g_MpPlayerCount = numplayers;

    for (i=0; i < g_MpPlayerCount; i++)
    {
        if (g_MpChar[i] == -1)
        {
            g_MpChar[i] = i;
            g_MpHandicap[i] = 5;
        }

        if ((numplayers >= 3) && (g_MpControlStyle[i] >= CONTROLLER_CONFIG_PLENTY))
        {
            g_MpControlStyle[i] = CONTROLLER_CONFIG_HONEY;
        }
    }

    if ((g_MpStagePlayerCounts[g_MpScenario].max < g_MpPlayerCount) || (g_MpPlayerCount < g_MpStagePlayerCounts[g_MpScenario].min))
    {
        frontSetMpScenario(SCENARIO_NORMAL);

    }

    if (g_MpStages[g_MpStage].max_player < g_MpPlayerCount)
    {
        g_MpStage = MP_STAGE_TEMPLE;
    }
}


//********************************************************************************************************
//MULTIPLAYER OPTIONS
//********************************************************************************************************
void frontMpOptionsEnter(void)
{
    int numplayers;

    g_FrontTabStartSelected = FALSE;
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabStartHighlight = FALSE;

    if (g_FrontStartMusic)
    {
        sndApplyVolumeAllSfxSlot(0x7fff);
        musicTrack1ApplySeqpVol(0x7fff);
        g_musicXTrack1Fade = MUSIC_FADESTATE_UNSET;
        musicTrack1Play(M_FOLDERS);
        g_FrontStartMusic = FALSE;
    }

    if (g_MpPlayerCount == 0)
    {
        numplayers = joyGetControllerCount();
        frontSetMpPlayerCount(numplayers);
    }

    if (frontIsMpStageAvailable(g_MpStage) == FALSE)
    {
        g_MpStage = MP_STAGE_TEMPLE;
    }

    g_MpHlPlayers = FALSE;
    g_MpHlScenario = FALSE;
    g_MpHlStage = FALSE;
    g_MpHlLength = FALSE;
    g_MpHlChar = FALSE;
    g_MpHlWeapons = FALSE;
    g_MpHlHandicap = FALSE;
    g_MpHlControls = FALSE;
    g_MpHlAim = FALSE;
    frontLoadFolders();
}

void frontMpOptionsLeave(void)
{
    return;
}


static s32 g_MpHlDisplay = FALSE;

/**
 * Steps the multiplayer display setting: with 2 players side by side,
 * stacked, side by side hi-res, stacked hi-res; with 3 or 4, normal or
 * hi-res.
 */
static void frontMpStepDisplaySetting(void)
{
    if (g_MpPlayerCount != 2)
    {
        optionsSetHiRes(!optionsGetHiRes());
        return;
    }

    if (!optionsGetSplitStacked())
    {
        optionsSetSplitStacked(TRUE);
        return;
    }

    optionsSetSplitStacked(FALSE);
    optionsSetHiRes(!optionsGetHiRes());
}

void frontMpOptionsTick(void)
{
    s32 players_selected = 0;
    s32 scenario_selected = 0;
    s32 gameselect_selected = 0;
    s32 gamelength_selected = 0;
    s32 character_selected = 0;
    s32 weapon_selected = 0;
    s32 health_selected = 0;
    s32 controlstyle_selected = 0;
    s32 aimadjust_selected = 0;

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    if (joyGetControllerCount() < 2)
    {
        frontSetMenu(MENU_MODE_SELECT, 0);
        frontSetModeCursor(g_FrontGameMode);
    }

    if (joyGetControllerCount() < g_MpPlayerCount)
    {
        frontSetMpPlayerCount(joyGetControllerCount());
    }

    if (joyGetButtons(PLAYER_1, Z_TRIG|A_BUTTON) == 0)
    {
        g_FrontTabPrevHighlight = 0;
        g_FrontTabNextHighlight = 0;
        g_FrontTabStartHighlight = 0;
        g_MpHlPlayers = 0;
        g_MpHlScenario = 0;
        g_MpHlStage = 0;
        g_MpHlLength = 0;
        g_MpHlChar = 0;
        g_MpHlWeapons = 0;
        g_MpHlHandicap = 0;
        g_MpHlControls = 0;
        g_MpHlAim = 0;
        g_MpHlDisplay = 0;

        if (frontCheckCursorOnPreviousTab())
        {
            g_FrontTabPrevHighlight = 1;
        }
        else if (frontCheckCursorOnStartTab())
        {
            g_FrontTabStartHighlight = 1;
        }
        else
        {
            s32 cursorY = g_FrontCursorY;
            if (cursorY >= 0x12D)
            {
                g_MpHlDisplay = 1;
            }
            else if ((cursorY >= 0x119) && (g_MpCanPickAim))
            {
                g_MpHlAim = 1;
            }
            else if ((cursorY >= 0x105) && (g_MpCanPickControls))
            {
                g_MpHlControls = 1;
            }
            else if ((cursorY >= 0xF1) && (g_MpCanPickHandicap))
            {
                g_MpHlHandicap = 1;
            }
            else if ((cursorY >= 0xDD) && (g_MpCanPickChar))
            {
                g_MpHlChar = 1;
            }
            else if ((cursorY >= 0xC9) && (g_MpCanPickWeapons))
            {
                g_MpHlWeapons = 1;
            }
            else if ((cursorY >= 0xB5) && (g_MpCanPickLength))
            {
                g_MpHlLength = 1;
            }
            else if ((cursorY >= 0xA1) && (g_MpCanPickStage))
            {
                g_MpHlStage = 1;
            }
            else if (cursorY >= 0x8D)
            {
                g_MpHlScenario = 1;
            }
            else
            {
                g_MpHlPlayers = 1;
            }
        }
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON))
    {
        g_FrontTabStartSelected = 1;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabPrevHighlight)
        {
            g_FrontTabPrevSelected = 1;
        }

        if (g_FrontTabStartHighlight)
        {
            g_FrontTabStartSelected = 1;
        }
        else if (g_MpHlPlayers)
        {
            players_selected = 1;
        }
        else if (g_MpHlScenario)
        {
            scenario_selected = 1;
        }
        else if (g_MpHlStage)
        {
            gameselect_selected = 1;
        }
        else if (g_MpHlLength)
        {
            gamelength_selected = 1;
        }
        else if (g_MpHlChar)
        {
            character_selected = 1;
        }
        else if (g_MpHlWeapons)
        {
            weapon_selected = 1;
        }
        else if (g_MpHlHandicap)
        {
            health_selected = 1;
        }
        else if (g_MpHlControls)
        {
            controlstyle_selected = 1;
        }
        else if (g_MpHlAim)
        {
            aimadjust_selected = 1;
        }
        else if (g_MpHlDisplay)
        {
            frontMpStepDisplaySetting();
        }

        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
    {
        g_FrontTabPrevSelected = 1;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CONFIDENTIAL2, 1);
    frontMoveCursor();

    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_MODE_SELECT, 0);
        frontSetModeCursor(g_FrontGameMode);
        return;
    }

    if (g_FrontTabStartSelected)
    {
        // Player 2's control style is shared with co-op.
        fileCoopSavePartnerSettings();
        fileSaveDisplaySettingsForFolder(g_FrontFolder);

        if (g_MpStages[g_MpStage].stage_id < 0)
        {
            s32 index;
            do
            {
                index = randomGetNext() % (u32)(MP_STAGE_SELECTED_MAX - 1);
                g_FrontStage = g_MpStages[1 + index].stage_id;
            } while (frontIsMpStageAvailable(index + 1) == 0);
        }
        else
        {
            g_FrontStage = g_MpStages[g_MpStage].stage_id;
        }

        g_FrontMission = -1;
        frontSetMenu(MENU_RUN_STAGE, 1);

        return;
    }

    if (players_selected)
    {
        s32 tmpNumPlayers;

        if (joyGetControllerCount() < g_MpPlayerCount + 1)
        {
            tmpNumPlayers = 2;
        }
        else
        {
            tmpNumPlayers = g_MpPlayerCount + 1;
        }

        frontSetMpPlayerCount(tmpNumPlayers);

        return;
    }
    if (scenario_selected)
    {
        frontSetMenu(MENU_MP_SCENARIO_SELECT, 0);
        return;
    }
    if (gameselect_selected)
    {
        frontSetMenu(MENU_MP_STAGE_SELECT, 0);
        return;
    }
    if (gamelength_selected)
    {
        frontStepMpGameLength();
        return;
    }
    if (character_selected)
    {
        frontSetMenu(MENU_MP_CHAR_SELECT, 0);
        return;
    }
    if (weapon_selected)
    {
        mpWeaponSetNext();
        return;
    }
    if (health_selected)
    {
        frontSetMenu(MENU_MP_HANDICAP, 0);
        return;
    }
    if (controlstyle_selected)
    {
        frontSetMenu(MENU_MP_CONTROL_STYLE, 0);
        return;
    }
    if (aimadjust_selected)
    {
        frontStepMpAimSettings();
    }
}


Gfx * frontMpOptionsRender(Gfx *DL)
{
    u8 *text;

    char acStack12 [8];

    s32 x;
    s32 y;

    int iStack24;
    int iStack28;
    u32 entry;


  DL = viSetFillColor(DL,0,0,0);
  DL = viFillScreen(DL);
  #if defined(BUGFIX_R2)
  DL = viFillScreen(DL);
  DL = viFillScreen(DL);
  #endif
  DL = frontSetupMenuBackground(DL);
  DL = textSetupDraw(DL);
  text = langGet(getStringID(LTITLE, TITLE_STR_76_MPOPTIONS));
  x = 0x37;
  y = 0x5f;
  DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xff, viGetX(), viGetY(), 0, 0);

  text = langGet(getStringID(LTITLE, TITLE_STR_77_PLAYERS));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0x79;
  if (g_MpHlPlayers) {
    DL = textRenderRect(DL,0x37,0x78,iStack28 + 0x3c,0x87,0x32);
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,0xff,viGetX(),viGetY(),0,0);

  text = langGet(getStringID(LTITLE, TITLE_STR_78_SCENARIO));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0x8d;
  if (g_MpHlScenario) {
    DL = textRenderRect(DL,0x37,0x8c,iStack28 + 0x3c,0x9b,0x32);
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,0xff,viGetX(),viGetY(),0,0);

  text = langGet(getStringID(LTITLE, TITLE_STR_79_LEVEL));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0xa1;
  if (g_MpHlStage) {
    DL = textRenderRect(DL,0x37,0xa0,iStack28 + 0x3c,0xaf,0x32);
  }
  if (g_MpCanPickStage) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  text = langGet(getStringID(LTITLE, TITLE_STR_80_GAMELENGTH));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0xb5;
  if (g_MpHlLength) {
    DL = textRenderRect(DL,0x37,0xb4,iStack28 + 0x3c,0xc3,0x32);
  }
  if (g_MpCanPickLength) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  text = langGet(getStringID(LTITLE, TITLE_STR_81_WEAPONS));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0xc9;
  if (g_MpHlWeapons) {
    DL = textRenderRect(DL,0x37,200,iStack28 + 0x3c,0xd7,0x32);
  }
  if (g_MpCanPickWeapons) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  text = langGet(getStringID(LTITLE, TITLE_STR_82_CHARACTER));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0xdd;
  if (g_MpHlChar) {
    DL = textRenderRect(DL,0x37,0xdc,iStack28 + 0x3c,0xeb,0x32);
  }
  if (g_MpCanPickChar) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  text = langGet(getStringID(LTITLE, TITLE_STR_83_HEALTH));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0xf1;
  if (g_MpHlHandicap) {
    DL = textRenderRect(DL,0x37,0xf0,iStack28 + 0x3c,0xff,0x32);
  }
  if (g_MpCanPickHandicap) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  text = langGet(getStringID(LTITLE, TITLE_STR_286_CONTROLSTYLE));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0x105;
  if (g_MpHlControls) {
    DL = textRenderRect(DL,0x37,0x104,iStack28 + 0x3c,0x113,0x32);
  }
  if (g_MpCanPickControls) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  text = langGet(getStringID(LTITLE, TITLE_STR_84_AIM));
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0x119;
  if (g_MpHlAim) {
    DL = textRenderRect(DL,0x37,0x118,iStack28 + 0x3c,0x127,0x32);
  }
  if (g_MpCanPickAim) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  sprintf(acStack12,"%d",g_MpPlayerCount);
  x = 0xa0;
  y = 0x79;
  DL = frontPrintText(DL, &x, &y, acStack12, g_FontZurichChars, g_FontZurich, 0xff, viGetX(), viGetY(), 0, 0);

  text = langGet(g_MpStagePlayerCounts[g_MpScenario].stage);
  x = 0xa0;
  y = 0x8d;
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,0xff,viGetX(),viGetY(),0,0);

  text = langGet(g_MpStages[g_MpStage].folder_text_preset);
  x = 0xa0;
  y = 0xa1;
  if (g_MpCanPickStage) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  text = langGet(g_MpGameLengths[g_MpGameLength].text_preset);
  x = 0xa0;
  y = 0xb5;
  if (g_MpCanPickLength) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);


  text = langGet(*(mpWeaponSetGetName()));
  x = 0xa0;
  y = 0xc9;
  if (g_MpCanPickWeapons) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  text = langGet(g_MpAimSettings[g_MpAimSetting].anonymous_0);
  x = 0xa0;
  y = 0x119;
  if (g_MpCanPickAim) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,entry,viGetX(),viGetY(),0,0);

  // Display: split screen layout (2 players) and hi-res, shared with co-op.
  text = (u8 *)"Display:\n";
  textMeasure(&iStack24,&iStack28,text,g_FontZurichChars,g_FontZurich,0);
  x = 0x39;
  y = 0x12d;
  if (g_MpHlDisplay) {
    DL = textRenderRect(DL,0x37,0x12c,iStack28 + 0x3c,0x13b,0x32);
  }
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,0xff,viGetX(),viGetY(),0,0);

  if (g_MpPlayerCount == 2) {
    if (optionsGetSplitStacked()) {
      text = (u8 *)(optionsGetHiRes() ? "Stacked, Hi-Res\n" : "Stacked\n");
    }
    else {
      text = (u8 *)(optionsGetHiRes() ? "Side by Side, Hi-Res\n" : "Side by Side\n");
    }
  }
  else {
    text = (u8 *)(optionsGetHiRes() ? "Hi-Res\n" : "Normal\n");
  }
  x = 0xa0;
  y = 0x12d;
  DL = frontPrintText(DL,&x,&y,text,g_FontZurichChars,g_FontZurich,0xff,viGetX(),viGetY(),0,0);

  DL = frontAddPreviousTabText(DL);
  DL = frontAddStartTabText(DL);
  DL = frontDrawCursor(DL);
  return DL;
}


bool frontIsMpCharTaken(s32 character)
{
    s32 i;
    bool char_is_selected;

    char_is_selected = FALSE;

    for (i = 0; i < g_MpPlayerCount; i++)
    {
        if (g_MpCharChosen[i] && character == g_MpChar[i])
        {
            char_is_selected = TRUE;
        }
    }

    return char_is_selected;
}

void frontMpCharEnter(void)
{
    s32 i;

    g_FrontTabStartSelected = 0;
    g_FrontTabNextSelected = 0;
    g_FrontTabPrevSelected = 0;
    g_FrontTabPrevHighlight = 0;
    g_FrontTabNextHighlight = 0;
    g_FrontTabStartHighlight = 0;

    for ( i = 0; i < 64; i += 1)
    {
        s32 selected_photo = g_MpChars[i].select_photo;

        // Multiplayer character images are composed of 4 textures.

        // upper left
        if (g_ImagesMpCharSelect[selected_photo * 4 + 0].index < (u32)(MAX_TEXTURES - 1))
        {
            texLoadFromTextureNum(g_ImagesMpCharSelect[selected_photo * 4 + 0].index, 0);
        }

        // upper right
        if (g_ImagesMpCharSelect[selected_photo * 4 + 1].index < (u32)(MAX_TEXTURES - 1))
        {
            texLoadFromTextureNum(g_ImagesMpCharSelect[selected_photo * 4 + 1].index, 0);
        }

        // lower left
        if (g_ImagesMpCharSelect[selected_photo * 4 + 2].index < (u32)(MAX_TEXTURES - 1))
        {
            texLoadFromTextureNum(g_ImagesMpCharSelect[selected_photo * 4 + 2].index, 0);
        }

        // lower right
        if (g_ImagesMpCharSelect[selected_photo * 4 + 3].index < (u32)(MAX_TEXTURES - 1))
        {
            texLoadFromTextureNum(g_ImagesMpCharSelect[selected_photo * 4 + 3].index, 0);
        }
    }

    if (g_MpCharCount != 0x40)
    {
        if (fileIsStageUnlockedAtDifficulty(g_FrontFolder, SP_LEVEL_CRADLE, DIFFICULTY_AGENT) == STAGESTATUS_COMPLETED)
        {
            g_MpCharCount = 0x21;
        }
        else
        {
            g_MpCharCount = 8;

            for ( i = 0; i < MAX_PLAYER_COUNT; i++)
            {
                if (g_MpChar[i] >= g_MpCharCount)
                {
                    s32 count = 0;

                    while (count < g_MpCharCount)
                    {
                        if (frontIsMpCharTaken(count) != 0)
                        {
                            count++;
                            continue;
                        }

                        break;
                    }

                    g_MpChar[i] = count;
                }
            }
        }
    }

    for (i = 0; i < MAX_PLAYER_COUNT; i++)
    {
        g_MpCharCursor[i] = g_MpChar[i];
        g_MpCharPrevCursor[i] = g_MpChar[i];
        g_MpCharScroll[i] = 0;
        g_MpCharChosen[i] = 0;
        g_MpCharPortraitSize[i] = 0;
    }

    frontLoadFolders();
}


//********************************************************************************************************
//MULTIPLAYER CHARACTER SELECT
//********************************************************************************************************

void frontMpCharLeave(void) 
{
    return;
}


void frontMpCharTick(void)
{
    s32 numplayers;
    s32 ready_players;
    s32 i;
    s32 scrollTarget;

    numplayers = frontGetPlayerCount();
    ready_players = 0;
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    for (i = 0; i < numplayers; i++)
    {
        if (g_MpCharChosen[i])
        {
            if ((g_MpCharPortraitSize[i] < 0xb) && (g_MpCharPrevCursor[i] == g_MpCharCursor[i]))
            {
                g_MpCharPortraitSize[i] = g_MpCharPortraitSize[i] + 1;
            }

            if (joyGetButtonsPressedThisFrame(i, B_BUTTON))
            {
                g_MpCharChosen[i] = 0;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
            }
        }

        if (g_MpCharChosen[i] == 0 && g_MpCharPortraitSize[i] > 0)
        {
            g_MpCharPortraitSize[i] = g_MpCharPortraitSize[i] - 1;
        }
        else if (g_MpCharChosen[i] == 0)
        {
            if (joyGetButtonsPressedThisFrame(i, L_JPAD|L_CBUTTONS)) // 0x202
            {
                if (g_MpCharCursor[i] > 0)
                {
                    g_MpCharCursor[i]--;
                }
            }
            else if (joyGetButtonsPressedThisFrame(i, R_JPAD|R_CBUTTONS)) // 0x101
            {
                if (g_MpCharCursor[i] < (g_MpCharCount - 1))
                {
                    g_MpCharCursor[i]++;
                }
            }
            else if (joyGetButtonsPressedThisFrame(i, A_BUTTON|Z_TRIG|START_BUTTON)) // 0xb000
            {
                if (frontIsMpCharTaken(g_MpCharCursor[i]) == 0)
                {
                    g_MpChar[i] = g_MpCharCursor[i];
                    g_MpCharPortraitSize[i] = 1;
                    g_MpCharChosen[i] = 1;

                    sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
                }
            }
            else if (joyGetStickXInRange(i, -2, 1) > 0)
            {
                if (g_MpCharScroll[i] == 0)
                {
                    if (g_MpCharCursor[i] < (g_MpCharCount - 1))
                    {
                        g_MpCharCursor[i]++;
                    }
                }
            }
            else if ((joyGetStickXInRange(i, -2, 1) < -1) && (g_MpCharScroll[i] == 0))
            {
                if (g_MpCharCursor[i] > 0)
                {
                    g_MpCharCursor[i]--;
                }
            }
        }

        scrollTarget = (g_MpCharPrevCursor[i] * 0x54) + g_MpCharScroll[i];

        if (g_MpCharCursor[i] * 0x54 < scrollTarget)
        {
            g_MpCharScroll[i] -= 0xc;
            if (g_MpCharScroll[i] < 0)
            {
                g_MpCharScroll[i] += 0x54;
                g_MpCharPrevCursor[i]--;
            }
        }
        else if (scrollTarget < g_MpCharCursor[i] * 0x54)
        {
            g_MpCharScroll[i] += 0xC;
            if (g_MpCharScroll[i] >= 0x54)
            {
                g_MpCharScroll[i] -= 0x54;
                g_MpCharPrevCursor[i]++;
            }
        }

        if (g_MpCharChosen[i] != 0 && g_MpCharPortraitSize[i] == 0xb)
        {
            ready_players++;
        }
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_BLANK, 1);
    frontMoveCursor();

    if (ready_players == numplayers)
    {
        frontSetMenu(MENU_MP_OPTIONS, 0);
    }
}


s32 frontGetPortraitAlpha(s32 x, s32 left, s32 right)
{
    if ((left >= x) || (x >= right))
    {
        return 0;
    }

    if (x < (left + 0x28))
    {
        return 0xF - ((left - x) * 6);
    }

    if ((right - 0x28) < x)
    {
        return 0xF - ((x - right) * 6);
    }

    return 0xFF;
}


Gfx *frontRenderCharacterPortrait(Gfx *DL, s32 left, s32 right, s32 centreX, s32 y, s32 character, s32 size)
{
    Mtx* proj;
    Mtx* view;
    Vertex* vertices;
    u32 row;
    u32 col;
    s32 cellW;
    s32 baseY;
    s32 cellH;
    s32 shade;
    s32 selected_photo;
    s32 vtx_index;
    struct sImageTableEntry *simage;
    s32 j;
    s32 i;

    proj = dynAllocateMatrix();
    view = dynAllocateMatrix();
    vertices = dynAllocateVertices(16);
    DL = textSetupDraw(DL);

    guOrtho(proj, 0.0f, 440.0f, 0.0f, 330.0f, 1.0f, 10.0f, 1.0f);
    guRotate(view, 3.1415927f, 1.0f, 0.0f, 0.0f);

    vtx_index = 0;

    for (i = 0; i < 4; i++)
    {
        cellW = size + 0x23;
        baseY = 0x14A - y;
        cellH = size + 0x2a;

        for (j = 0; j < 4; j++)
        {
            col = (j >= 2) ? j - 1 : j;
            row = (i >= 2) ? i - 1 : i;

            if ((frontIsMpCharTaken(character)) && (size == 0))
            {
                shade = 0x6E;
            }
            else
            {
                shade = 0xFF;
            }

            vertices[vtx_index].coord.AsArray[0] = ((col - 1) * cellW) + centreX;
            vertices[vtx_index].coord.AsArray[1] = baseY + ((row - 1) * cellH) + size;
            vertices[vtx_index].coord.AsArray[2] = -5;
            vertices[vtx_index].index = 0;

            if (j & 1)
            {
                vertices[vtx_index].s = 0x1000;
            }
            else
            {
                vertices[vtx_index].s = 0;
            }

            if (i & 1)
            {
                vertices[vtx_index].t = 0x1000;
            }
            else
            {
                vertices[vtx_index].t = 0;
            }

            vertices[vtx_index].r = shade;
            vertices[vtx_index].g = shade;
            vertices[vtx_index].b = shade;

            if (size == 0)
            {
                vertices[vtx_index].a = frontGetPortraitAlpha(vertices[vtx_index].coord.AsArray[0], left, right);
            }
            else
            {
                vertices[vtx_index].a = 0xFF;
            }

            vtx_index++;
        }
    }

    gSPMatrix(DL++, OS_PHYSICAL_TO_K0(proj), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(DL++, OS_PHYSICAL_TO_K0(view), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gSPVertex(DL++, vertices, 16, 0);

    selected_photo = g_MpChars[character].select_photo * 4;
    simage = &g_ImagesMpCharSelect[g_MpChars[character].select_photo * 4];
    texSelect(&DL, simage + 2, 2, 0, 2);
    gSP1Triangle(DL++, 0, 1, 4, 0);
    gSP1Triangle(DL++, 4, 1, 5, 0);

    selected_photo = g_MpChars[character].select_photo * 4;
    simage = &g_ImagesMpCharSelect[g_MpChars[character].select_photo * 4];
    texSelect(&DL, simage + 3, 2, 0, 2);
    gSP1Triangle(DL++, 2, 3, 6, 0);
    gSP1Triangle(DL++, 6, 3, 7, 0);

    selected_photo = g_MpChars[character].select_photo * 4;
    simage = &g_ImagesMpCharSelect[g_MpChars[character].select_photo * 4];
    texSelect(&DL, simage + 0, 2, 0, 2);
    gSP1Triangle(DL++, 8, 9, 12, 0);
    gSP1Triangle(DL++, 12, 9, 13, 0);

    selected_photo = g_MpChars[character].select_photo * 4;
    simage = &g_ImagesMpCharSelect[g_MpChars[character].select_photo * 4];
    texSelect(&DL, simage + 1, 2, 0, 2);
    gSP1Triangle(DL++, 10, 11, 14, 0);
    gSP1Triangle(DL++, 14, 11, 15, 0);

    return DL;
}


Gfx *frontMpCharRender(Gfx *DL)
{
    s32 numplayers;
    s32 playernum;
    u8 *text;
    s32 panel_left;  // X position where the portraits completely fade out on the left.
    s32 panel_top;   
    s32 panel_width; // Width of the character portrait panel (1 panel per player)
    s32 grid_offset; // Distance between the portrait panels
    s32 portrait_y_base;
    s32 select_char_height;
    s32 select_char_width;
    s32 select_char_x;
    s32 select_char_y;
    s32 panel_center_x;
    s32 name_height;
    s32 name_width;
    s32 name_x;
    s32 name_y;

    numplayers = frontGetPlayerCount();
    
    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);
    DL = textRenderRect(DL, 0x26, 0xA9, 0x184, 0xAB, 0x90);

    if (numplayers >= 3)
    {
        DL = textRenderRect(DL, 0xD4, 0x1E, 0xD6, 0x136, 0x80);
    }

    for (playernum = 0; playernum < numplayers; playernum++)
    {
        panel_top = 0;

        if (numplayers == 2)
        {
            panel_left = 0x26;
            panel_width = 0x15E;
            grid_offset = (playernum > 0) ? (0x8C) : (0);
            panel_top = grid_offset + 0x1E;
        }
        else
        {
            panel_width = 0xAF;
            grid_offset = (playernum >= 2) ? (0x8C) : (0);
            panel_top = grid_offset + 0x1E;
            grid_offset = (playernum & 1) ? (0xAF) : (0);
            panel_left = grid_offset + 0x26;
        }
        
        gDPSetScissor(DL++, G_SC_NON_INTERLACE, (panel_left + 6), panel_top + 5, (panel_left + panel_width - 6), (panel_top + 0x8C - 5));

        panel_center_x = (panel_width >> 1) + panel_left;

        if ((g_MpCharChosen[playernum] == 0) && (g_MpCharPortraitSize[playernum] == 0))
        {
            text = langGet(getStringID(LTITLE, TITLE_STR_85_SELECTCHARACTER));
            textMeasure(&select_char_height, &select_char_width, text, g_FontZurichChars, g_FontZurich, 0);
            select_char_x = panel_center_x - (select_char_width >> 1);
            select_char_y = panel_top + 5;
            DL = textSetupDraw(DL);
            DL = frontPrintText(DL, &select_char_x, &select_char_y, (s8 *) text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }
        
        text = langGet(g_MpChars[g_MpCharCursor[playernum]].text_preset);
        textMeasure(&name_height, &name_width, text, g_FontZurichChars, g_FontZurich, 0);
        portrait_y_base = panel_top;
        name_x = panel_center_x - (name_width >> 1);
        name_y = (portrait_y_base + 0x46) + 0x32;
        DL = textSetupDraw(DL);
        DL = frontPrintText(DL, &name_x, &name_y, (s8 *) text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        DL = frontRenderCharacterPortrait(DL, panel_left + 0xD, (panel_left + panel_width) - 0xE, panel_center_x - g_MpCharScroll[playernum], portrait_y_base + 0x46, g_MpCharPrevCursor[playernum], g_MpCharPortraitSize[playernum]);

        if (((g_MpCharChosen[playernum] == 0) && (g_MpCharPortraitSize[playernum] == 0)) || (g_MpCharPrevCursor[playernum] != g_MpCharCursor[playernum]))
        {
            if (g_MpCharPrevCursor[playernum] >= 3)
            {
                DL = frontRenderCharacterPortrait(DL, panel_left + 0xD, (panel_left + panel_width) - 0xE, (panel_center_x - g_MpCharScroll[playernum]) - 0xFC, portrait_y_base + 0x46, g_MpCharPrevCursor[playernum] - 3, g_MpCharPortraitSize[playernum]);
            }

            if (g_MpCharPrevCursor[playernum] >= 2)
            {
                DL = frontRenderCharacterPortrait(DL, panel_left + 0xD, (panel_left + panel_width) - 0xE, (panel_center_x - g_MpCharScroll[playernum]) - 0xA8, portrait_y_base + 0x46, g_MpCharPrevCursor[playernum] - 2, g_MpCharPortraitSize[playernum]);
            }

            if (g_MpCharPrevCursor[playernum] >= 1)
            {
                DL = frontRenderCharacterPortrait(DL, panel_left + 0xD, (panel_left + panel_width) - 0xE, (panel_center_x - g_MpCharScroll[playernum]) - 0x54, portrait_y_base + 0x46, g_MpCharPrevCursor[playernum] - 1, g_MpCharPortraitSize[playernum]);
            }

            if (g_MpCharPrevCursor[playernum] < (g_MpCharCount - 1))
            {
                DL = frontRenderCharacterPortrait(DL, panel_left + 0xD, (panel_left + panel_width) - 0xE, (panel_center_x - g_MpCharScroll[playernum]) + 0x54, portrait_y_base + 0x46, g_MpCharPrevCursor[playernum] + 1, g_MpCharPortraitSize[playernum]);
            }

            if (g_MpCharPrevCursor[playernum] < (g_MpCharCount - 2))
            {
                DL = frontRenderCharacterPortrait(DL, panel_left + 0xD, (panel_left + panel_width) - 0xE, (panel_center_x - g_MpCharScroll[playernum]) + 0xA8, portrait_y_base + 0x46, g_MpCharPrevCursor[playernum] + 2, g_MpCharPortraitSize[playernum]);
            }

            if (g_MpCharPrevCursor[playernum] < (g_MpCharCount - 3))
            {
                DL = frontRenderCharacterPortrait(DL, panel_left + 0xD, (panel_left + panel_width) - 0xE, (panel_center_x - g_MpCharScroll[playernum]) + 0xFC, portrait_y_base + 0x46, g_MpCharPrevCursor[playernum] + 3, g_MpCharPortraitSize[playernum]);
            }
        }
    }

    DL = textEndDraw(DL);

    return DL;
}


//********************************************************************************************************
//MULTIPLAYER HANDICAP
//********************************************************************************************************
void frontMpHandicapEnter(void)
{
    s32 i = 0;
    g_FrontTabStartSelected = FALSE;
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabStartHighlight = FALSE;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        g_MpCharChosen[i] = FALSE;
        g_MpStickHeld[i] = 0;
    }
    frontLoadFolders();
}

void frontMpHandicapLeave(void) 
{
    return;
}


void frontMpHandicapTick(void)
{
    s32 numplayers;
    s32 i;
    s32 numready;

    numplayers = frontGetPlayerCount();
    numready = 0;
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    for (i = 0; i < numplayers; i++)
    {
        if (g_MpCharChosen[i] && joyGetButtonsPressedThisFrame(i, B_BUTTON))
        {
            g_MpCharChosen[i] = 0;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }

        if (g_MpCharChosen[i] == 0)
        {
            // 0x202
            if (joyGetButtonsPressedThisFrame(i, L_JPAD|L_CBUTTONS) || ((joyGetStickXInRange(i, -2, 1) < -1) && g_MpStickHeld[i] != 0))
            {
                if (g_MpHandicap[i] > 0)
                {
                    g_MpHandicap[i]--;
                }
            }
            // 0x101
            else if (joyGetButtonsPressedThisFrame(i, R_JPAD|R_CBUTTONS) || ((joyGetStickXInRange(i, -2, 1) > 0) && g_MpStickHeld[i] != 0))
            {
                if (g_MpHandicap[i] < 0xA)
                {
                    g_MpHandicap[i]++;
                }
            }
            else if (joyGetButtonsPressedThisFrame(i, A_BUTTON|Z_TRIG|START_BUTTON)) // 0xb000
            {
                g_MpCharChosen[i] = 1;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
            }

            if ((joyGetStickXInRange(i, -2, 1) == -1) || (joyGetStickXInRange(i, -2, 1) == 0))
            {
                g_MpStickHeld[i] = 1;
            }
            else
            {
                g_MpStickHeld[i] = 0;
            }
        }

        if (g_MpCharChosen[i] != 0)
        {
            numready++;
        }
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_BLANK, 1);
    frontMoveCursor();

    if (numready == numplayers)
    {
        frontSetMenu(MENU_MP_OPTIONS, 0);
    }
}


Gfx * frontMpHandicapRender(Gfx *DL)
{
    s32 numplayers;
    s32 i;
    u8 *text;
    s32 panelX;
    s32 panelW;
    s32 offset;
    u8 *text2;
    s32 padding4;
    s32 padding1;
    s32 panelY;
    s32 height1;
    s32 width1;
    s32 x1;
    s32 y1;
    s32 centreX;
    s32 height2;
    s32 width2;
    s32 x2;
    s32 y2;
    s32 centreY;

    numplayers = frontGetPlayerCount();

    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);
    DL = textRenderRect(DL, 0x26, 0xA9, 0x184, 0xAB, 0x90);

    if (numplayers >= 3)
    {
        DL = textRenderRect(DL, 0xD4, 0x1E, 0xD6, 0x136, 0x80);
    }

    for (i = 0; i < numplayers; i++)
    {
        padding4 = g_MpCharChosen[i];
        padding1 = g_MpHandicap[i];

        panelY = 0;

        if (numplayers == 2)
        {
            panelX = 0x26;
            panelW = 0x15E;
            offset = (i > 0) ? 0x8C : 0;
            panelY = (offset) + 0x1E;
        }
        else
        {
            panelW = 0xAF;
            offset = (i >= 2) ? 0x8C : 0;
            panelY = (offset) + 0x1E;
            offset = (i & 1) ? 0xAF : 0;
            panelX = (offset) + 0x26;
        }

        gDPSetScissor(DL++, G_SC_NON_INTERLACE, (panelX + 6), panelY + 5, (panelX + panelW - 6), (panelY + 0x8C - 5));

        centreX = ((panelW >> 1) + panelX);
        centreY = panelY + 0x46;

        if (g_MpCharChosen[i] == 0)
        {
            text = langGet(getStringID(LTITLE, TITLE_STR_86_SELECTHANDICAP));
            textMeasure(&height1, &width1, text, g_FontZurichChars, g_FontZurich, 0);

            x1 = centreX - (width1 >> 1);
            y1 = centreY - (height1 >> 1) - 0xf;

            DL = textSetupDraw(DL);
            DL = frontPrintText(DL, &x1, &y1, (s8*)text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }

        {
            text2 = langGet(g_MpHandicaps[g_MpHandicap[i]].text_preset);
            textMeasure(&height2, &width2, text2, g_FontZurichChars, g_FontZurich, 0);
            x2 = centreX - (width2 >> 1);
            y2 = centreY - (height2 >> 1) + 0xf;

            DL = textSetupDraw(DL);
            DL = frontPrintText(DL, &x2, &y2, (s8*)text2, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }
    }

    DL = textEndDraw(DL);

    return DL;
}


//********************************************************************************************************
//MULTIPLAYER CONTROL OPTIONS
//********************************************************************************************************
void frontMpControlsEnter(void)
{
    s32 i;
    g_FrontTabStartSelected = FALSE;
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabStartHighlight = FALSE;

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        g_MpCharChosen[i] = FALSE;
        g_MpStickHeld[i] = 0;
    }

    frontLoadFolders();
}

void frontMpControlsLeave(void) {
    return;
}


// Address 0x7F013648 NTSC-J
void frontMpControlsTick(void)
{
    s32 numplayers;
    s32 i;
    s32 numready;

    numplayers = frontGetPlayerCount();
    numready = 0;
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    #define MAYBE_OR_JOYCOUNT_NOT_4 || joyGetControllerCount() != 4
    #define MAYBE_AND_JOYCOUNT_4 && joyGetControllerCount() == 4


    for (i = 0; i < numplayers; i++)
    {
        if (g_MpCharChosen[i] && joyGetButtonsPressedThisFrame(i, B_BUTTON))
        {
            g_MpCharChosen[i] = 0;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }

        if (g_MpCharChosen[i] == 0)
        {
            // 0x202
            if (joyGetButtonsPressedThisFrame(i, L_JPAD|L_CBUTTONS) || ((joyGetStickXInRange(i, -2, 1) < -1) && g_MpStickHeld[i] != 0))
            {
                // if greater than 0
                if (g_MpControlStyle[i] > CONTROLLER_CONFIG_HONEY)
                {
                    // ... OR PLAYER_1 is 1.x OR g_MpControlStyle[i] is 1 + 2.x
                    if ((numplayers != 2) || (i != PLAYER_2) MAYBE_OR_JOYCOUNT_NOT_4 || (g_MpControlStyle[PLAYER_1] < CONTROLLER_CONFIG_PLENTY) || (g_MpControlStyle[i] >= CONTROLLER_CONFIG_GALORE))
                    {
                        g_MpControlStyle[i]--;
                    }

                    // ... AND PLAYER_1 is 1.x AND PLAYER_2 is 2.x
                    if ((numplayers == 2) && (i == PLAYER_1) MAYBE_AND_JOYCOUNT_4 && (g_MpControlStyle[PLAYER_1] < CONTROLLER_CONFIG_PLENTY) && (g_MpControlStyle[PLAYER_2] >= CONTROLLER_CONFIG_PLENTY))
                    {
                        // PLAYER_2 is 1.1
                        g_MpControlStyle[PLAYER_2] = CONTROLLER_CONFIG_HONEY;
                    }
                }
            }
            // 0x101
            else if (joyGetButtonsPressedThisFrame(i, R_JPAD|R_CBUTTONS) || ((joyGetStickXInRange(i, -2, 1) > 0) && g_MpStickHeld[i] != 0))
            {
                // ... g_MpControlStyle[i] is 2.x
                if (((numplayers == 2) && (i == PLAYER_1) MAYBE_AND_JOYCOUNT_4) || g_MpControlStyle[i] >= CONTROLLER_CONFIG_PLENTY)
                {
                    // if g_MpControlStyle[i] < 2.4
                    if (g_MpControlStyle[i] < CONTROLLER_CONFIG_GOODHEAD)
                    {
                        g_MpControlStyle[i]++;

                        // ... AND PLAYER_1 is 2.1 AND PLAYER_2 is 1.x
                        if ((i == PLAYER_1) && (g_MpControlStyle[PLAYER_1] == CONTROLLER_CONFIG_PLENTY) && (g_MpControlStyle[PLAYER_2] < CONTROLLER_CONFIG_PLENTY))
                        {
                            // PLAYER_2 is 2.1
                            g_MpControlStyle[PLAYER_2] = CONTROLLER_CONFIG_PLENTY;
                        }
                    }
                }
                // else if g_MpControlStyle[i] < 1.45
                else if (g_MpControlStyle[i] < CONTROLLER_CONFIG_GOODNIGHT)
                {
                    g_MpControlStyle[i]++;
                }

            }
            else if (joyGetButtonsPressedThisFrame(i, A_BUTTON|Z_TRIG|START_BUTTON)) // 0xb000
            {
                g_MpCharChosen[i] = 1;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
            }

            if (joyGetControllerCount() != 4)
            {
                if (g_MpControlStyle[PLAYER_1] >= CONTROLLER_CONFIG_PLENTY)
                {
                    g_MpControlStyle[PLAYER_1] = CONTROLLER_CONFIG_HONEY;
                }

                if (g_MpControlStyle[PLAYER_2] >= CONTROLLER_CONFIG_PLENTY)
                {
                    g_MpControlStyle[PLAYER_2] = CONTROLLER_CONFIG_HONEY;
                }
            }

            if ((joyGetStickXInRange(i, -2, 1) == -1) || (joyGetStickXInRange(i, -2, 1) == 0))
            {
                g_MpStickHeld[i] = 1;
            }
            else
            {
                g_MpStickHeld[i] = 0;
            }
        }

        if (g_MpCharChosen[i] != 0)
        {
            numready++;
        }
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_BLANK, 1);
    frontMoveCursor();

    if (numready == numplayers)
    {
        frontSetMenu(MENU_MP_OPTIONS, 0);
    }

#undef MAYBE_OR_JOYCOUNT_NOT_4
#undef MAYBE_AND_JOYCOUNT_4
}


Gfx * frontMpControlsRender(Gfx *DL)
{
    s32 numplayers;
    s32 i;
    u8 *text;
    s32 panelX;
    s32 panelW;
    s32 offset;
    u8 *text2;
    s32 padding4;
    s32 padding1;
    s32 panelY;
    s32 height1;
    s32 width1;
    s32 x1;
    s32 y1;
    s32 centreX;
    s32 height2;
    s32 width2;
    s32 x2;
    s32 y2;
    s32 centreY;

    numplayers = frontGetPlayerCount();

    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);
    DL = textRenderRect(DL, 0x26, 0xA9, 0x184, 0xAB, 0x90);

    if (numplayers >= 3)
    {
        DL = textRenderRect(DL, 0xD4, 0x1E, 0xD6, 0x136, 0x80);
    }

    for (i = 0; i < numplayers; i++)
    {
        padding4 = g_MpCharChosen[i];
        padding1 = g_MpControlStyle[i];

        panelY = 0;

        if (numplayers == 2)
        {
            panelX = 0x26;
            panelW = 0x15E;
            offset = (i > 0) ? 0x8C : 0;
            panelY = (offset) + 0x1E;
        }
        else
        {
            panelW = 0xAF;
            offset = (i >= 2) ? 0x8C : 0;
            panelY = (offset) + 0x1E;
            offset = (i & 1) ? 0xAF : 0;
            panelX = (offset) + 0x26;
        }

        gDPSetScissor(DL++, G_SC_NON_INTERLACE, (panelX + 6), panelY + 5, (panelX + panelW - 6), (panelY + 0x8C - 5));

        centreX = ((panelW >> 1) + panelX);
        centreY = panelY + 0x46;

        if (g_MpCharChosen[i] == 0)
        {
            text = langGet(getStringID(LTITLE, TITLE_STR_285_SELECTCONTROLSTYLE));
            textMeasure(&height1, &width1, text, g_FontZurichChars, g_FontZurich, 0);

            x1 = centreX - (width1 >> 1);
            y1 = centreY - (height1 >> 1) - 0xf;

            DL = textSetupDraw(DL);
            DL = frontPrintText(DL, &x1, &y1, (s8*)text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }

        {
            text2 = langGet(g_MpControlStyles[g_MpControlStyle[i]].text_preset);
            textMeasure(&height2, &width2, text2, g_FontZurichChars, g_FontZurich, 0);
            x2 = centreX - (width2 >> 1);
            y2 = centreY - (height2 >> 1) + 0xf;

            DL = textSetupDraw(DL);
            DL = frontPrintText(DL, &x2, &y2, (s8*)text2, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }
    }

    DL = textEndDraw(DL);

    return DL;
}


//********************************************************************************************************
//MULTIPLAYER STAGE SELECT
//********************************************************************************************************
void frontMpStageEnter(void)
{
  g_FrontTabStartSelected = FALSE;
  g_FrontTabNextSelected = FALSE;
  g_FrontTabPrevSelected = FALSE;
  g_FrontTabPrevHighlight = FALSE;
  g_FrontTabNextHighlight = FALSE;
  g_FrontTabStartHighlight = FALSE;
  g_MpStageCursor = MP_STAGE_RANDOM;
  frontLoadFolders();
}

void frontMpStageLeave(void) {
return;
}


void frontMpStageTick(void)
{
    s32 row;
    s32 column;

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(FALSE);
    if (joyGetButtons(PLAYER_1, START_BUTTON|Z_TRIG|A_BUTTON) == 0)
    {
        g_FrontTabPrevHighlight = FALSE;
        g_FrontTabNextHighlight = FALSE;
        g_FrontTabStartHighlight = FALSE;
        if (frontCheckCursorOnPreviousTab())
        {
            g_FrontTabPrevHighlight = TRUE;
        }
        else
        {
            if ((s32)g_FrontCursorY >= 240)
            {
                row = 2;
            }
            else if ((s32)g_FrontCursorY >= 170)
            {
              row = 1;
            } else
            {
                row = 0;
            }

            if ((s32) g_FrontCursorX >= 292)
            {
                column = 3;
            }
            else if ((s32) g_FrontCursorX >= 207)
            {
                column = 2;
            }
            else if ((s32) g_FrontCursorX >= 122)
            {
                column = 1;
            }
            else
            {
                column = 0;
            }

            g_MpStageCursor = (row * 4) + column;
            while (frontIsMpStageAvailable(g_MpStageCursor) == 0)
            {
                g_MpStageCursor = g_MpStageCursor - 1;
            }
        }
    }
    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON|Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabPrevHighlight)
        {
            g_FrontTabPrevSelected = TRUE;
        }
        else
        {
            if (g_MpStageCursor >= MP_STAGE_RANDOM)
            {
                g_FrontTabNextSelected = TRUE;
                g_MpStage = g_MpStageCursor;
            }
        }
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
    {
        g_FrontTabPrevSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS,  1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_BLANK, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS,  1);
    frontMoveCursor();
    if (g_FrontTabNextSelected)
    {
        frontSetMenu(MENU_MP_OPTIONS, FALSE);
        return;
    }
    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_MP_OPTIONS, FALSE);
    }
}


Gfx * frontMpStageRender(Gfx *DL)
{
    s32 x;
    s32 y;
    s32 height;
    s32 width;
    struct coord2d bgPos;
    struct coord2d bgSize;
    s32 i_3;
    s32 i_2;
    s32 i_1;
    struct coord2d photoPos;
    struct coord2d photoSize;
    s32 labelX;
    s32 colour;
    struct sImageTableEntry *simage;
    s32 count;
    s32 count_2;
    s32 row;


    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);

    for (i_1 = 0; i_1 != 3; i_1++)
    {
        DL = textRenderRect(DL, 0x25, 0x6c + (i_1 * 0x46), 0x185, 0xa0 + (i_1 * 0x46), 0x101010FF);
    }

    DL = textEndDraw(DL);

    gDPSetTextureFilter(DL++, G_TF_POINT);

    simage = g_ImagesMainFolder + 5;
    texSelect(&DL, simage, 1, 0, 2); // IMAGE_DOT

    bgSize.f[0] = 176.0f;
    bgSize.f[1] = 4.0f;
    bgPos.f[0] = bgSize.f[0] + 37.0f;
    bgPos.f[1] = bgSize.f[1] + 100.0f;

    for (i_2 = 0; i_2 < 3; i_2++)
    {
        texRectDrawImage(&DL, &bgPos.f[0], &bgSize.f[0], 0x2F0, 0x12, 0, 0, 1, 0x6B, 0x67, 0x53, 0xFF, simage->level > 0, 0);
        bgPos.f[1] += 60.0f;
        texRectDrawImage(&DL, &bgPos.f[0], &bgSize.f[0], 0x2F0, 0x12, 0, 0, 1, 0x6B, 0x67, 0x53, 0xFF, simage->level > 0, 0);
        bgPos.f[1] -= 60.0f;
        bgPos.f[1] += 70.0f;
    }

    photoSize.f[0] = 34.0f;
    photoSize.f[1] = 22.0f;
    photoPos.f[1] = photoSize.f[1] + 108.0f + 4.0f;

    count = 0;

    for (row = 0; row != 3; row++)
    {
        photoPos.f[0] = 86.0f;

        for (i_3 = 0; i_3 < 4; i_3++)
        {
            if (count < 12)
            {
                simage = &g_ImagesMpStageSelect[g_MpStages[count].photo];

                texSelect(&DL, simage, 1, 0, 2);

                if (count == g_MpStageCursor)
                {
                    gDPSetCycleType(DL++, G_CYC_2CYCLE);
                    gDPSetFogColor(DL++, 0xff, 0xff, 0xff, 0x0a);
                    gDPSetRenderMode(DL++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);

                    texRectDrawImage(&DL, &photoPos.f[0], &photoSize.f[0], 0x44, 0x2C, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF, simage->level > 0, 1);
                }
                else if (frontIsMpStageAvailable(count) != 0)
                {
                    texRectDrawImage(&DL, &photoPos.f[0], &photoSize.f[0], 0x44, 0x2C, 0, 0, 1, 0x6E, 0x6E, 0x6E, 0xFF, simage->level > 0, 0);
                }
                else
                {
                    texRectDrawImage(&DL, &photoPos.f[0], &photoSize.f[0], 0x44, 0x2C, 0, 0, 1, 0xF, 0xF, 0xF, 0xFF, simage->level > 0, 0);
                }
            }

            photoPos.f[0] += 85.0f;
            count++;
        }

        photoPos.f[1] += 70.0f;
    }

    DL = textSetupDraw(DL);

    count_2 = 0;

    // not sure what's going on here, but this is required to get the stack to match.
    if(1)
    {
        s32 i_4;
        s32 labelRow;
        s32 labelY;

        labelY = 0x97;

        for (labelRow = 0; labelRow != 3; labelRow++)
        {
            labelX = 0x56;

            for (i_4 = 0; i_4 < 4; i_4++)
            {
                if (count_2 < 12 && frontIsMpStageAvailable(count_2))
                {
                    colour = 0x96969600;

                    if (count_2 == g_MpStageCursor)
                    {
                        colour = -0x100;
                    }

                    textMeasure(&height, &width, langGet(g_MpStages[count_2].select_screen_text_preset), g_FontBankGothicChars, g_FontBankGothic, 0);

                    x = labelX - 0x1F;
                    y = labelY - height;

                    DL = textRenderRect(DL, x, y, x + width, y + height, 0);

                    DL = textRender(DL, &x, &y, langGet(g_MpStages[count_2].select_screen_text_preset), g_FontBankGothicChars, g_FontBankGothic, colour | 0xFF, viGetX(), viGetY(), 0, 0);

                    x = labelX - 0x1F;
                    y = labelY - height;

                    DL = textRender(DL, &x, &y, langGet(g_MpStages[count_2].select_screen_text_preset), g_FontBankGothicChars, g_FontBankGothic, colour | 0x64, viGetX(), viGetY(), 0, 0);
                }

                labelX += 0x55;
                count_2++;
            }

            labelY += 0x46;
        }
    }

    DL = textSetupDraw(DL);
    DL = frontAddPreviousTabText(DL);
    DL = frontDrawCursor(DL);

    return DL;
}


MPSCENARIOS frontGetMpScenario(void)
{
  return g_MpScenario;
}

//********************************************************************************************************
//MULTIPLAYER SCENARIO SELECT
//********************************************************************************************************
void frontMpScenarioEnter(void)
{
  g_FrontTabStartSelected = FALSE;
  g_FrontTabNextSelected = FALSE;
  g_FrontTabPrevSelected = FALSE;
  g_FrontTabPrevHighlight = FALSE;
  g_FrontTabNextHighlight = FALSE;
  g_FrontTabStartHighlight = FALSE;
  frontLoadFolders();
}

void frontMpScenarioLeave(void) {
    return;
}


void frontMpScenarioTick(void)
{
    s32 isTeam;
    s32 scenarioid;
    s32 i;

    isTeam = FALSE;
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(FALSE);

    if (joyGetButtons(PLAYER_1, Z_TRIG|A_BUTTON) == 0) {
        g_FrontTabPrevHighlight = FALSE;
        g_FrontTabNextHighlight = FALSE;
        g_FrontTabStartHighlight = FALSE;

        if (frontCheckCursorOnPreviousTab())
        {
            g_FrontTabPrevHighlight = TRUE;
            g_MpScenarioCursor = SCENARIO_NORMAL;
        }
        else
        {
            g_MpScenarioCursor = SCENARIO_YOLT;

            for (i = 7; i != 0; i--)
            {
                if (
                    (s32)g_FrontCursorY  >=  (0x83 + (-i * -0x16))
                    && (g_MpStagePlayerCounts[i].min <= frontGetPlayerCount())
                    && (frontGetPlayerCount() <= g_MpStagePlayerCounts[i].max))
                {
                    g_MpScenarioCursor = i + 1;
                    break;
                }
            }
        }
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON|Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabPrevHighlight)
        {
            g_FrontTabPrevSelected = TRUE;
        }
        else
        {
            scenarioid = g_MpScenarioCursor - 1;

            if ((scenarioid == SCENARIO_2v2) || (scenarioid == SCENARIO_3v1) || (scenarioid == SCENARIO_2v1))
            {
                frontSetMpScenario(scenarioid);
                isTeam = TRUE;
            }
            else
            {
                frontSetMpScenario(scenarioid);
                g_FrontTabPrevSelected = TRUE;
            }
        }

        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
    {
        g_FrontTabPrevSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CLASSIFIED, 1);
    frontMoveCursor();

    if (isTeam)
    {
        frontSetMenu(MENU_MP_TEAMS, FALSE);
        return;
    }

    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_MP_OPTIONS, FALSE);
    }
}


Gfx * frontMpScenarioRender(Gfx *DL)
{
    s32 x;
    s32 y;
    s32 i;
    s32 height;
    s32 width;

    u8 *text;
    s32 colour;


    DL = viSetFillColor(DL,0,0,0);
    DL = viFillScreen(DL);
#ifdef VERSION_EU
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
#endif
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);

    text = langGet(getStringID(LTITLE, TITLE_STR_87_SCENARIO2));

    x = 0x37;
    y = 0x66;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    for (i = 0; i < 8; i++)
    {
        colour = 0xff;

        if (((s32) g_MpStagePlayerCounts[i].max < frontGetPlayerCount()) || (frontGetPlayerCount() < (s32) g_MpStagePlayerCounts[i].min))
        {
            colour = 0x70;
        }

        text = langGet(g_MpStagePlayerCounts[i].stage);

        textMeasure(&height, &width, text, g_FontZurichChars, g_FontZurich, 0);

        x = 0x39;
        y = 0x83 + (i * 0x16);
        if ((i + 1) == g_MpScenarioCursor)
        {
            DL = textRenderRect(DL, 0x37, y - 1, width + 0x3C, y + 0xE, 0x32);
        }

        DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, colour, viGetX(), viGetY(), 0, 0);
    }

    DL = frontAddPreviousTabText(DL);
    DL = frontDrawCursor(DL);

    return DL;
}


s32 frontGetMpTeam(s32 player)
{
  return g_playerPlayerData[player].have_token_or_goldengun;
}

void frontSetMpTeam(s32 player, s32 flag)
{
  g_playerPlayerData[player].have_token_or_goldengun = flag;
}


//********************************************************************************************************
//MULTIPLAYER TEAM SELECT
//********************************************************************************************************

void frontMpTeamsEnter(void)
{
    g_FrontTabStartSelected = FALSE;
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabStartHighlight = FALSE;

    if (g_MpScenario == SCENARIO_2v1) {
        g_MpTeamsCursor = 2;
    }
    else {
        g_MpTeamsCursor = 3;
    }

    g_MpTeamsBlinkTimer = 0;
    frontLoadFolders();
}

void frontMpTeamsLeave(void) 
{
    return;
}


void frontMpTeamsTick(void)
{
    s32 i;
    s32 team;

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    g_MpTeamsBlinkTimer = (s32) (g_MpTeamsBlinkTimer + 1) % 20;

    // 0x101
    if ((joyGetButtonsPressedThisFrame(0, R_JPAD|R_CBUTTONS)) || (joyGetStickXInRange(0, -2, 1) > 0))
    {
        if (g_MpScenario == SCENARIO_2v1)
        {
            if (!(g_MpTeamsCursor & 1))
            {
                g_MpTeamsCursor = 1;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
            }
        }
        else if (!(g_MpTeamsCursor & 1))
        {
            g_MpTeamsCursor += 1;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
    }
    // 0x808
    else if ((joyGetButtonsPressedThisFrame(0, U_JPAD|U_CBUTTONS)) || (joyGetStickYInRange(0, -2, 1) > 0))
    {
        if (g_MpScenario == SCENARIO_2v2)
        {
            if (g_MpTeamsCursor & 2)
            {
                g_MpTeamsCursor = 1;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
            }
        }
        else if (g_MpTeamsCursor & 2)
        {
            g_MpTeamsCursor -= 2;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
    }
    // 0x202
    else if ((joyGetButtonsPressedThisFrame(0, L_JPAD|L_CBUTTONS)) || (joyGetStickXInRange(0, -2, 1) < -1))
    {
        if (g_MpScenario == SCENARIO_2v2)
        {
            if (g_MpTeamsCursor & 1)
            {
                g_MpTeamsCursor = 2;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
            }
        }
        else if (g_MpTeamsCursor & 1)
        {
            g_MpTeamsCursor -= 1;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
    }
    // 0x404
    else if ((joyGetButtonsPressedThisFrame(0, D_JPAD|D_CBUTTONS)) || (joyGetStickYInRange(0, -2, 1) < -1))
    {
        if (g_MpScenario == SCENARIO_2v1)
        {
            if (!(g_MpTeamsCursor & 2))
            {
                g_MpTeamsCursor = 2;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
            }
        }
        else if (!(g_MpTeamsCursor & 2))
        {
            g_MpTeamsCursor += 2;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_BLANK, 1);
    frontMoveCursor();

    if (joyGetButtonsPressedThisFrame(0, A_BUTTON|Z_TRIG|START_BUTTON)) // 0xb000
    {
        for (i = 0; i < 4; i++)
        {
            if (g_MpScenario == SCENARIO_2v2)
            {

                if (i == 0 || i == g_MpTeamsCursor)
                {
                    team = 0;
                }
                else
                {
                    team = 1;
                }

                frontSetMpTeam(i, team);
            }
            else
            {
                if (i != g_MpTeamsCursor)
                {
                    team = 0;
                }
                else
                {
                    team = 1;
                }

                frontSetMpTeam(i, team);
            }
        }

        frontSetMenu(MENU_MP_OPTIONS, 0);
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
}


Gfx *frontMpTeamsRender(Gfx *DL)
{
    s32 numPlayers;
    s32 playerIndex;
    s32 layoutOffset;
    s32 useFirstTeam;
    s32 centerX;
    s32 rowTop;
    char *text;
    s32 textColour;
    s32 titleHeight;
    s32 titleWidth;
    s32 titleX;
    s32 titleYPos;
    s32 textHeight;
    s32 textWidth;
    s32 textX;
    s32 textY;
    char *titleText;

    numPlayers = frontGetPlayerCount();

    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);
    DL = textRenderRect(DL, 0x26, 0xA9, 0x184, 0xAB, 0x90);

    if (numPlayers >= 3)
    {
        DL = textRenderRect(DL, 0xD4, 0x1E, 0xD6, 0x136, 0x80);
    }

    for (playerIndex = 0; playerIndex < numPlayers; playerIndex++)
    {
        layoutOffset = (0, (playerIndex >= 2) ? 0x8C : 0);
        rowTop = layoutOffset + 0x1E;
        layoutOffset = ((playerIndex & 1) ? 0xAF : 0) + 0;

        gDPSetScissor(DL++, G_SC_NON_INTERLACE, layoutOffset + 0x2c, rowTop + 5, layoutOffset + 0xcf, rowTop + 0x87);

        centerX = layoutOffset + 0x7d;

        if (playerIndex == 0)
        {
            text = langGet(getStringID(LTITLE, TITLE_STR_88_SELECTTEAMS)); // "Select Teams"
            titleText = text;

            textMeasure(&titleHeight, &titleWidth, titleText, g_FontZurichChars, g_FontZurich, 0);

            titleYPos = rowTop + 5;
            titleX = centerX - (titleWidth >> 1);

            DL = frontPrintText(DL, &titleX, &titleYPos, titleText, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }

        if (g_MpScenario == SCENARIO_2v2)
        {
            useFirstTeam = (playerIndex == 0) || (playerIndex == g_MpTeamsCursor);
        }
        else
        {
            useFirstTeam = playerIndex != g_MpTeamsCursor;
        }

        if (useFirstTeam)
        {
            text = langGet(getStringID(LTITLE, TITLE_STR_89_RED)); // "Red Team"
        }
        else
        {
            text = langGet(getStringID(LTITLE, TITLE_STR_90_BLUE)); // "Blue Team"
        }

        if (useFirstTeam)
        {
            textColour = 0xFF0000FF;
        }
        else
        {
            textColour = 0x0000FFFF;
        }

        textMeasure(&textHeight, &textWidth, text, g_FontZurichChars, g_FontZurich, 0);

        textX = centerX - (textWidth >> 1);
        textY = (rowTop - (textHeight >> 1)) + 0x46;

        if ((playerIndex != g_MpTeamsCursor) || ((playerIndex == g_MpTeamsCursor) && (((s32) g_MpTeamsBlinkTimer) < 0xA)))
        {
            DL = frontPrintText(DL, &textX, &textY, text, g_FontZurichChars, g_FontZurich, textColour, viGetX(), viGetY(), 0, 0);
        }
    }

    DL = textEndDraw(DL);

    return DL;
}


void frontSetBriefingPage(WATCH_BRIEFING_PAGE page)
{

    // could be a dev typo
    if (g_FrontBriefingPage = page)
    {
        // maybe previous code was optimized out
    }
}


void frontLoadBriefing(void)
{
    // The briefing file and the stage's text bank go after the folder models.
    u8 *buffer = (u8 *)g_FrontModelBuffer + 4096 * 10;
    s32 size = 0x200;

    g_FrontBriefing = fileLoadByNameToAddr(g_FrontMissions[g_FrontMission].briefing_name_ptr, FILELOAD_TRIM, buffer, size);
    buffer += size;

    langLoadToBank(langGetBankForStage(g_FrontMissions[g_FrontMission].stage_id), buffer, size * 879);
}

//********************************************************************************************************
//BRIEFING SCREEN
//********************************************************************************************************
void frontBriefingEnter(void)
{
    g_FrontTabNextSelected = 0;
    g_FrontTabPrevSelected = 0;
    g_FrontBriefingLastPage = 5;
    frontSetBriefingPage(0);
    frontLoadFolders();
    frontLoadBriefing();
}


void frontBriefingLeave(void)
{
  langClearBank(langGetBankForStage(g_FrontMissions[g_FrontMission].stage_id));
  if (-1 < g_FrontNextMenu) {
    frontFreeFolders();
  }
}


void frontBriefingTick(void)
{
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(FALSE);
    g_FrontTabPrevHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabStartHighlight = FALSE;
    if (frontCheckCursorOnPreviousTab())
    {
        g_FrontTabPrevHighlight = TRUE;
    }
    else if ((frontCheckCursorOnNextTab()) && (g_FrontBriefingPage < (g_FrontBriefingLastPage - 1)))
    {
        g_FrontTabNextHighlight = TRUE;
    }
    else if (frontCheckCursorOnStartTab())
    {
        g_FrontTabStartHighlight = TRUE;
    }
    else if (g_FrontBriefingPage < (g_FrontBriefingLastPage - 1))
    {
        g_FrontTabNextHighlight = TRUE;
    }
    else
    {
        g_FrontTabStartHighlight = TRUE;
    }
    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON))
    {
        g_FrontTabNextSelected = TRUE;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabNextHighlight)
        {
            frontSetBriefingPage(g_FrontBriefingPage + 1);
        }
        else if (g_FrontTabPrevHighlight)
        {
            if (g_FrontBriefingPage > BRIEFING_TITLE)
            {
                frontSetBriefingPage(g_FrontBriefingPage - 1);
            }
            else
            {
                g_FrontTabPrevSelected = TRUE;
            }
        }
        else if (g_FrontTabStartHighlight)
        {
            g_FrontTabNextSelected = TRUE;
        }
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON) != 0)
    {
        if (g_FrontBriefingPage > BRIEFING_TITLE)
        {
            frontSetBriefingPage(g_FrontBriefingPage - 1);
        }
        else
        {
            g_FrontTabPrevSelected = TRUE;
        }
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PHOTOBRIEF, g_FrontBriefingPage == BRIEFING_TITLE);
    frontSetSwitchVisible(g_FrontFolderModels[0], g_FrontMissions[g_FrontMission].mission_num + 0x16, g_FrontBriefingPage == BRIEFING_TITLE);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CLASSIFIED, 1);
    frontMoveCursor();
    if (g_FrontTabNextSelected)
    {
        frontSetMenu(MENU_RUN_STAGE, TRUE);
        return;
    }
    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_MISSION_SELECT, FALSE);
        frontSetMissionCursor(g_FrontMissions[g_FrontMission].mission_num);
    }
}


Gfx *frontDrawObjectives(Gfx *DL, s32 y, u8 *buffer, s32 showStatus)
{
    s32 i;
    s32 index;
    s32 lineheight;
    s32 width;
    s32 x;
    s32 lineY;
    s32 colour;
    s32 lines;
    u8* text;
    s32 v1;

    index = 0;
    lines = 0;
    i = 0;
    v1 = 0;

    for (i = 0; i < OBJECTIVES_MAX; i++)
    {
        if (g_FrontBriefing->objective[i].textid != 0 && g_FrontDifficulty >= g_FrontBriefing->objective[i].enabled_difficulty)
        {
            v1 = g_FrontBriefing->objective[i].textid;
            text = langGet(v1);
            strcpy((u8*)buffer, "a.\n");
            *buffer = *buffer + index;

            lineheight = 0;
            width = 0;
            textMeasure(&lineheight, &width, (u8*)buffer, g_FontZurichChars, g_FontZurich, 0);


            x = 0x37;
            lineY = (lineheight * lines) + y + (i*0);
            DL = frontPrintText(DL, &x, &lineY, (s8*)buffer, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

            x = 0x4B;
            lineY = (lineheight * lines) + y + (i*0);

            if (showStatus != 0)
            {
                textSetWordWrap(2);
                textWrap(0xDC, (s8*)text, (s8*)buffer, g_FontZurichChars, g_FontZurich);
                textSetWordWrap(0);
            }
            else
            {
                textWrap(0x140, (s8*)text, (s8*)buffer, g_FontZurichChars, g_FontZurich);
            }

            DL = frontPrintText(DL, &x, &lineY, (s8*)buffer,  g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

            if (showStatus)
            {
                switch (objectiveCheckStatus(i))
                {
                    case 1:
                        text   = langGet(getStringID(LTITLE, TITLE_STR_91_COMPLETED));
                        colour = 0xFF;
                        break;
                    default:
                    case 0:
                    case 2:
                        text   = langGet(getStringID(LTITLE, TITLE_STR_92_FAILED));
                        colour = 0x780000FF;
                        break;
                }

                x = 0x136;
                lineY = (lineheight * lines) + y + (i*0);

                DL = frontPrintText(DL, &x, &lineY, (s8*)text, g_FontZurichChars, g_FontZurich, colour, viGetX(), viGetY(), 0, 0);
            }

            lines += textCountLines(buffer);
            index++;
        }
    }

    return DL;
}



Gfx *frontBriefingRender(Gfx *DL)
{
    u8 *text;
    s32 x;
    s32 y;
    char buffer[3000];

    DL = viSetFillColor(DL,0,0,0);
    DL = viFillScreen(DL);
#ifdef VERSION_EU
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
#endif
    DL = frontSetupMenuBackground(DL);

    DL = textSetupDraw(DL);
    DL = frontDrawMissionTitle(DL, buffer);

    switch (g_FrontBriefingPage)
    {
        case BRIEFING_TITLE:
            text = langGet(getStringID(LTITLE, TITLE_STR_93_PRIMARYOBJECTIVES ));
            break;

        case BRIEFING_OVERVIEW:
            text = langGet(getStringID(LTITLE, TITLE_STR_94_BACKGROUND));
            break;

        case BRIEFING_M:
            text = langGet(getStringID(LTITLE, TITLE_STR_95_MBRIEFING));
            break;

        case BRIEFING_Q:
            text = langGet(getStringID(LTITLE, TITLE_STR_96_QBRANCH));
            break;

        case BRIEFING_MONEYPENNY:
            text = langGet(getStringID(LTITLE, TITLE_STR_97_MONEYPENNY));
            break;
    }

    x = 0x37;
    y = 0x8F;
    DL = frontPrintText(DL, &x, &y, (s8*)text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    if (g_FrontBriefingPage == BRIEFING_TITLE)
    {
        DL = frontDrawObjectives(DL, 0xA7, buffer, 0);
    }
    else
    {
        switch (g_FrontBriefingPage)
        {
            case BRIEFING_OVERVIEW:
                text = langGet(g_FrontBriefing->brief[0]);
            break;

            case BRIEFING_M:
                text = langGet(g_FrontBriefing->brief[1]);
            break;

            case BRIEFING_Q:
                text = langGet(g_FrontBriefing->brief[2]);
            break;

            case BRIEFING_MONEYPENNY:
                text = langGet(g_FrontBriefing->brief[3]);
            break;
        }

        x = 0x37;
        y = 0xA7;
        textWrap(0x140, (s8*)text, (s8*)buffer, g_FontZurichChars, g_FontZurich);
        textSetOverlapCorrection(8);
        DL = frontPrintText(DL, &x, &y, (s8*)buffer, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        textSetOverlapCorrection(-1);
    }

    DL = frontAddStartTabText(DL);
    DL = frontAddPreviousTabText(DL);

    if (g_FrontBriefingPage < BRIEFING_MONEYPENNY)
    {
        DL = frontAddNextTabText(DL);
    }

    DL = frontDrawCursor(DL);

    return DL;
}


//********************************************************************************************************
//RUN STAGE
//********************************************************************************************************
void frontRunStageEnter(void)
{
    frontCoopResetPartnerOutfit();
    bossSetLoadedStage(g_FrontStage);
    lvSetDifficulty(g_FrontDifficulty);
}


//********************************************************************************************************
//MISSION FAILED
//********************************************************************************************************
void frontDebriefEnter(void)
{
  g_FrontTabNextSelected = FALSE;
  g_FrontTabPrevSelected = FALSE;
  frontLoadFolders();
  frontLoadBriefing();
  frontSetCursorPositionToNextTab();
  if (g_FrontStartMusic != FALSE) {
    sndApplyVolumeAllSfxSlot(0x7fff);
    musicTrack1ApplySeqpVol(0x7fff);
    g_musicXTrack1Fade = MUSIC_FADESTATE_UNSET;
    musicTrack1Play(M_FOLDERS);
    g_FrontStartMusic = FALSE;
  }
}

void frontDebriefLeave(void) {
    return;
}


void frontDebriefTick(void)
{
    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(FALSE);
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    if (frontCheckCursorOnPreviousTab())
    {
        g_FrontTabPrevHighlight = TRUE;
    }
    else
    {
        if (frontCheckCursorOnNextTab())
        {
            g_FrontTabNextHighlight = TRUE;
        }
        else
        {
            g_FrontTabNextHighlight = TRUE;
        }
    }
    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON|Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabPrevHighlight)
        {
            g_FrontTabPrevSelected = TRUE;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, 0);
        }
        else
        {
            if (g_FrontTabNextHighlight)
            {
                g_FrontTabNextSelected = TRUE;
                sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, 0);
            }
        }
    }
    else
    {
        if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
        {
            g_FrontTabPrevSelected = TRUE;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, 0);
        }
    }
    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CLASSIFIED, 1);
    frontMoveCursor();
    if (g_FrontTabNextSelected)
    {
        frontSetMenu(MENU_MISSION_COMPLETE, FALSE);
        return;
    }
    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_MISSION_SELECT, FALSE);
        frontSetMissionCursor(g_FrontMissions[g_FrontMission].mission_num);
    }
}


s32 frontIsMissionSuccess(void)
{
    s32 i;

    if (g_FrontMissionAborted || g_FrontBondKilled)
    {
        return 0;
    }

    for (i=0; i<10; i++)
    {
        if (g_FrontBriefing->objective[i].textid != 0
            && lvGetDifficulty() >= g_FrontBriefing->objective[i].enabled_difficulty
            && objectiveCheckStatus(i) != OBJECTIVESTATUS_COMPLETE)
            {
                return 0;
            }
    }

    return 1;
}


Gfx * frontDebriefRender(Gfx *DL)
{
    s32 x;
    s32 y;

    char stagename[0xbb8];
    char *text;
    s32 colour;

    s32 y2;
    s32 x2;


    DL = viSetFillColor(DL, 0, 0, 0);
    DL = viFillScreen(DL);
    #if defined(BUGFIX_R2)
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
    #endif
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);
    DL = frontDrawMissionTitle(DL, &stagename);

    text = langGet(getStringID(LTITLE, TITLE_STR_98_REPORT)); //REPORT:*
    x = 0x37;
    y = 0x8F;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    text = langGet(getStringID(LTITLE, TITLE_STR_99_MISSIONSTATUS)); //Mission status:*
    x2 = 0;
    y2 = 0;
    textMeasure(&y2, &x2, text, g_FontZurichChars, g_FontZurich, 0);
    x = 0x37;
    y = 0xA7;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    if (g_FrontBondKilled)
    {
        text = langGet(getStringID(LTITLE, TITLE_STR_100_KIA)); // KILLED IN ACTION*
        colour = 0x78000000 | 0xFF;
    }
    else if (g_FrontMissionAborted)
    {
        text = langGet(getStringID(LTITLE, TITLE_STR_101_ABORTED)); // ABORTED*
        colour = 0x78000000 | 0xFF;
    }
    else if (frontIsMissionSuccess())
    {

        text = langGet(getStringID(LTITLE, TITLE_STR_102_COMPLETED)); // Completed*
        colour = 0xFF;
    }
    else
    {
        text = langGet(getStringID(LTITLE, TITLE_STR_103_FAILED)); // FAILED*
        colour = 0x78000000 | 0xFF;
    }

    x = x2 + 0x37;
    y = 0xA7;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, colour, viGetX(), viGetY(), 0, 0);

    // Counter-op: Bond wins by completing the mission, the guard player by
    // stopping him. Name the controller too, as either can be Bond.
    if (g_CounterOpMode)
    {
        s32 bondwins = frontIsMissionSuccess();
        char winner[48];

        sprintf(winner, "Winner: %s (Player %d)\n", bondwins ? "Bond" : "Guard",
                (bondwins ? coopPlayerPerson(COUNTEROP_BOND) : coopPlayerPerson(COUNTEROP_GUARD)) + 1);
        x2 = 0;
        y2 = 0;
        textMeasure(&y2, &x2, langGet(getStringID(LTITLE, TITLE_STR_98_REPORT)), g_FontZurichChars, g_FontZurich, 0);
        x = x2 + 0x37 + 0x10;
        y = 0x8F;
        DL = frontPrintText(DL, &x, &y, winner, g_FontZurichChars, g_FontZurich, bondwins ? 0xFF : (0x78000000 | 0xFF), viGetX(), viGetY(), 0, 0);
    }

    DL = frontDrawObjectives(DL, 0xBF, &stagename, 1);
    DL = frontAddNextTabText(DL);
    DL = frontAddPreviousTabText(DL);
    DL = frontDrawCursor(DL);
    return DL;
}

//********************************************************************************************************
//MISSION COMPLETE
//********************************************************************************************************
void frontStatsEnter(void)
{
    g_FrontTabNextSelected = 0;
    g_FrontTabPrevSelected = 0;
    frontLoadFolders();
    frontLoadBriefing();
    if (g_NewCheatUnlocked)
    {
        sndPlaySfx(g_musicSfxBufferPtr, RARELOGO_SFX, 0);
    }
}

void frontStatsLeave(void) {
    return;
}


void frontStatsTick()
{
    int entry;

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);
    g_FrontTabPrevHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;

    if (frontCheckCursorOnPreviousTab())
    {
        g_FrontTabPrevHighlight = 1;
    }
    else
    {
        if (frontCheckCursorOnNextTab())
        {
            g_FrontTabNextHighlight = 1;
        }
        else
        {
            g_FrontTabNextHighlight = 1;
        }
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON|Z_TRIG|A_BUTTON))
    {
        if (g_FrontTabNextHighlight)
        {
            g_FrontTabNextSelected = 1;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
        else if (g_FrontTabPrevHighlight != FALSE)
        {
            g_FrontTabPrevSelected = 1;
            sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
        }
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
    {
        g_FrontTabPrevSelected = 1;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_PAPER, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_OHMSS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CLASSIFIED, 1);
    frontMoveCursor();

    if (g_FrontTabNextSelected)
    {
        if (frontIsMissionSuccess() && (!g_AppendCheatSinglePlayer))
        {
            if (g_FrontMissions[g_FrontMission].mission_num == SP_LEVEL_CRADLE)
            {
                frontSetMenu(MENU_RUN_STAGE, TRUE);
                g_FrontStage = LEVELID_CUBA;
            }
            else if (g_FrontMissions[g_FrontMission].mission_num >= SP_LEVEL_AZTEC)
            {
                frontSetMenu(MENU_MISSION_SELECT, FALSE);
                frontSetMissionCursor((s32)g_FrontMissions[g_FrontMission].mission_num);
            }
            else
            {
                entry = g_FrontMission + 1;
                while (g_FrontMissions[entry].folder_text_preset)
                {
                    if (g_FrontMissions[entry].stage_id >= 0)
                    {
                        break;
                    }

                    entry++;
                }

                if (g_FrontMissions[entry].folder_text_preset)
                {
                    g_FrontMission = entry;
                    g_FrontStage = g_FrontMissions[entry].stage_id;
                }

                frontSetMenu(MENU_BRIEFING, FALSE);
            }
        }
        else
        {
            frontSetMenu(MENU_BRIEFING, FALSE);
        }
    }
    else if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_MISSION_SELECT, FALSE);
        frontSetMissionCursor((s32)g_FrontMissions[g_FrontMission].mission_num);
    }

    return;
}

/**
 * Co-op mission complete: each player's accuracy, shots, head hits and kills
 * side by side, in place of the single player's detailed stats.
 */
static Gfx *frontDrawCoopMissionStats(Gfx *DL, s32 lineheight)
{
    static const s32 columns[2] = { 0xAA, 0x118 };
    struct player_data *savedperm = g_playerPerm;
    char text[64];
    s32 row;
    s32 i;
    s32 x;
    s32 y;

    // Counter-op has two more rows, so they sit closer together to fit on
    // the page.
    s32 top = g_CounterOpMode ? 0xC6 : 0xCC;
    s32 step = lineheight + (g_CounterOpMode ? 1 : 4);

    for (i = 0; i < 2; i++)
    {
        x = columns[i];
        y = top;
        if (g_CounterOpMode)
        {
            sprintf(text, (i == COUNTEROP_BOND) ? "BOND\n" : "GUARD\n");
        }
        else
        {
            sprintf(text, "PLAYER %d\n", i + 1);
        }
        DL = frontPrintText(DL, &x, &y, (s8 *)text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    }

    // Counter-op adds the guard player's bodies used and damage to Bond.
    for (row = 0; row < (g_CounterOpMode ? 6 : 4); row++)
    {
        static const s32 labels[4] = {
            TITLE_STR_106_ACCURACY,  // Accuracy:
            TITLE_STR_108_SHOTTOTAL, // Shot total:
            TITLE_STR_109_HEADHITS,  // Head hits:
            TITLE_STR_113_KILLTOTAL, // Kill total:
        };
        s32 rowy = top + 0x10 + (row * step);

        x = 0x37;
        y = rowy;
        if (row < 4)
        {
            DL = frontPrintText(DL, &x, &y, langGet(getStringID(LTITLE, labels[row])), g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }
        else
        {
            DL = frontPrintText(DL, &x, &y, (s8 *)((row == 4) ? "Bodies used:\n" : "Damage to Bond:\n"), g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }

        for (i = 0; i < 2; i++)
        {
            s32 shotsfired;
            s32 allhits;
            s32 headshots;

            g_playerPerm = &g_playerPlayerData[i];
            shotsfired = statsGetHits(0);
            headshots = statsGetHits(1);
            allhits = headshots + statsGetHits(2) + statsGetHits(3)
                + statsGetHits(4) + statsGetHits(5);

            switch (row)
            {
                case 0:
                    sprintf(text, "%.1f%%", (shotsfired > 0) ? (((allhits + statsGetHits(6)) * 100.0f) / shotsfired) : 0.0f);
                    break;
                case 1:
                    sprintf(text, "%d", shotsfired);
                    break;
                case 2:
                    sprintf(text, "%d (%d%%)", headshots, (s32)mathFloor(((headshots * 100.0f) / ((allhits > 0) ? allhits : 1)) + 0.5f));
                    break;
                case 3:
                    sprintf(text, "%d", statsGetKills());
                    break;
                case 4:
                    sprintf(text, (i == COUNTEROP_BOND) ? "-" : "%d", counterOpGetBodiesUsed());
                    break;
                default:
                    if (i == COUNTEROP_BOND)
                    {
                        sprintf(text, "-");
                    }
                    else
                    {
                        sprintf(text, "%d%%", (s32)mathFloor(counterOpGetDamageToBond() + 0.5f));
                    }
                    break;
            }

            x = columns[i];
            y = rowy;
            DL = frontPrintText(DL, &x, &y, (s8 *)text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        }
    }

    g_playerPerm = savedperm;

    return DL;
}

Gfx *frontStatsRender(Gfx *DL)
{
    s32 x;
    s32 y;
    u8 stagename[3000];
    char *text;
    s32 y2;
    s32 x2;
    s32 missiontimer;
    s32 killcount;
    s32 shotsfired;
    s32 headshots;
    s32 bodyshots;
    s32 limbshots;
    s32 reg4;
    s32 reg5;
    s32 reg6;
    s32 hitshots;
    s32 allhits;

    s32 missiontime;
    s32 temp;
    s32 besttime;
    s32 targettime;
    s32 difficulty;
    f32 hitPct;


    DL = viSetFillColor(DL, 0,0,0);
    DL = viFillScreen(DL);
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);

    missiontimer = lvGetMissionTime60();
    killcount = statsGetKills();
    shotsfired = statsGetHits(0);
    headshots = statsGetHits(1);
    bodyshots = statsGetHits(2);
    limbshots = statsGetHits(3);
    reg4 = statsGetHits(4);
    reg5 = statsGetHits(5);
    reg6 = statsGetHits(6);
    allhits = headshots + bodyshots + limbshots + reg4 + reg5;
    hitshots = allhits + reg6;
    if (allhits <= 0) {
        allhits = 1;
    }
    DL = frontDrawMissionTitle(DL, stagename);

    text = langGet(getStringID(LTITLE, TITLE_STR_104_STATS)); //STATISTICS:*
    x = 0x37;
    y = 0x8F;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    x2 = 0;
    y2 = 0;
    textMeasure(&y2, &x2, text, g_FontZurichChars, g_FontZurich, 0);

    text = langGet(getStringID(LTITLE, TITLE_STR_105_TIME)); //Time:*
    x = 0x37;
    y = 0xA7;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    besttime = fileGetSaveStageDifficultyTime( fileGetSaveForFoldernum(g_FrontFolder), g_FrontMissions[g_FrontMission].mission_num, g_FrontDifficulty);
    if ((besttime == 0) || (!(besttime < 0x3FF))) {
        besttime = -1;
    }
    if (g_FrontDifficulty >= DIFFICULTY_007) {
        difficulty = DIFFICULTY_00;
    }
    else {
        difficulty = g_FrontDifficulty;
    }
    missiontime = missiontimer / 60;
    stagename[0] = '\0';
    targettime = g_FrontTargetTimes[g_FrontMissions[g_FrontMission].mission_num][difficulty];
    sprintf(stagename, "%02d:%02d", missiontime / 60, missiontime % 60);
    x = 0x82;
    y = 0xA7;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

    if (g_NewCheatUnlocked) {
        stagename[0] = '\0';
        sprintf(stagename, "     [%s]", langGet(getStringID(LTITLE, TITLE_STR_275_NEWCHEATAVAILABLE))); //New Cheat Available
        DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xA00000FF, viGetX(), viGetY(), 0, 0);
    }


    if ((targettime > 0) && (g_FrontDifficulty != DIFFICULTY_007)) {
        text = langGet(getStringID(LTITLE, TITLE_STR_274_TARGET)); //Target:
        x = 0x37;
        y = y2 + 0xA9;
        DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        stagename[0] = '\0';
        sprintf(stagename, "%02d:%02d", targettime / 60, targettime % 60);
        x = 0x82;
        y = y2 + 0xA9;
        DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
        if (besttime >= 0) {
            if (besttime < 0x3FF) {
                stagename[0] = '\0';
                if (besttime >= 0) {
                    sprintf(stagename, "     (%s  %02d:%02d)", langGet(getStringID(LTITLE, TITLE_STR_273_BESTTIME)), besttime / 60, besttime % 60); //Best Time:
                }
                else {
                    sprintf(stagename, "");
                }
                DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
            }
        }
    }
    else {
        if (besttime >= 0) {
            if (besttime < 0x3FF) {
                text = langGet(getStringID(LTITLE, TITLE_STR_273_BESTTIME)); //Best Time:
                x = 0x37;
                y = y2 + 0xA9;
                DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
                stagename[0] = '\0';
                sprintf(stagename, "%02d:%02d", besttime / 60, besttime % 60);
                x = 0x82;
                y = y2 + 0xA9;
                DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
            }
        }
    }


    if (g_CoopMode)
    {
        DL = frontDrawCoopMissionStats(DL, y2);
        goto tabs;
    }

    text = langGet(getStringID(LTITLE, TITLE_STR_106_ACCURACY)); //Accuracy:*
    x = 0x37;
    y = 0xCC;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    if (shotsfired > 0) {
        hitPct = (hitshots * 100.0f) / shotsfired;
    }
    else {
        hitPct = 0.0f;
    }
    sprintf(stagename, "%.1f%%",  hitPct);
    x = 0x82;
    y = 0xCC;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    text = langGet(getStringID(LTITLE, TITLE_STR_107_WEAPONOFCHOICE)); //Weapon of choice:*
    x = 0x37;
    y = 0xDC;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    strcpy(stagename, frontGetFavWeaponText(0, 0));
    if ((g_MpFavWeapon[0][0] > 0) && (g_MpFavWeapon[0][1] == g_MpFavWeapon[0][0]))
    {
        temp = strlen(stagename)-1;
        sprintf(&stagename[temp], " x 2\n");
    }
    x = 0xBE;
    y = 0xDC;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    text = langGet(getStringID(LTITLE, TITLE_STR_108_SHOTTOTAL)); //Shot total:*
    x = 0x37;
    y = 0xF4;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    sprintf(stagename, "%d", shotsfired);
    x = 0x82;
    y = 0xF4;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    text = langGet(getStringID(LTITLE, TITLE_STR_109_HEADHITS)); //Head hits:*
    x = 0xB4;
    y = 0xF4;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    sprintf(stagename, "%d (%d%%)", headshots, (s32)(mathFloor(((headshots * 100.0f) / allhits) + 0.5f)));
    x = 0x12C;
    y = 0xF4;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    text = langGet(getStringID(LTITLE, TITLE_STR_110_BODYHITS)); //Body hits:*
    x = 0xB4;
    y = y2 + 0xF4;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    sprintf(stagename, "%d (%d%%)", bodyshots, (s32)mathFloor(((bodyshots * 100.0f) / allhits) + 0.5f));
    x = 0x12C;
    y = y2 + 0xF4;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    text = langGet(getStringID(LTITLE, TITLE_STR_111_LIMBHITS)); //Limb hits:*
    x = 0xB4;
    y = (y2 * 2) + 0xF4;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    sprintf(stagename, "%d (%d%%)", limbshots, (s32)mathFloor(((limbshots * 100.0f) / allhits) + 0.5f));
    x = 0x12C;
    y = (y2 * 2) + 0xF4;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    text = langGet(getStringID(LTITLE, TITLE_STR_112_OTHER)); //Others:*
    x = 0xB4;
    y = (y2 * 3) + 0xF4;
    temp = reg5 + reg4;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    sprintf(stagename, "%d (%d%%)", temp, (s32)mathFloor(((temp * 100.0f) / allhits) + 0.5f));
    x = 0x12C;
    y = (y2 * 3) + 0xF4;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);


    text = langGet(getStringID(LTITLE, TITLE_STR_113_KILLTOTAL)); //Kill total:*
    x = 0x37;
    y = y2 + 0xF4;
    DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);
    sprintf(stagename, "%d", killcount);
    x = 0x82;
    y = y2 + 0xF4;
    DL = frontPrintText(DL, &x, &y, stagename, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

tabs:
    DL = frontAddNextTabText(DL);
    DL = frontAddPreviousTabText(DL);
    DL = frontDrawCursor(DL);
    return DL;
}


//********************************************************************************************************
//CHEAT SCREEN
//********************************************************************************************************

void frontCheatEnter(void)
{
    int i;

    g_FrontTabStartSelected = FALSE;
    g_FrontTabNextSelected = FALSE;
    g_FrontTabPrevSelected = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabStartHighlight = FALSE;
    g_CheatMenuCount = 0x0;

    for (i = 1; i != CHEAT_INVALID; i++) {
        if (g_CheatAvailable[i]) {
          g_CheatMenuList[g_CheatMenuCount] = i;
          g_CheatMenuCount++;
        }
    };
    frontLoadFolders();
}


void frontCheatLeave(void)
{
  int i;
  u32 appendsp;
  u32 appendmp;

  appendsp = FALSE;
  appendmp = FALSE;

  for (i = 1; i != CHEAT_INVALID; i++) {
    if (g_CheatActivated[i]) {
        appendsp = TRUE;
        if (cheatIsMpCheat(i)) {
            appendmp = TRUE;
        }
    }
  };

  g_AppendCheatSinglePlayer = appendsp;
  g_AppendCheatMultiPlayer = appendmp;
}


void frontCheatTick(void)
{
    s32 i;

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    g_CheatMenuToggle = 0;

    if (joyGetButtons(PLAYER_1, A_BUTTON|Z_TRIG) == 0)
    {
        g_FrontTabPrevHighlight = 0;
        g_FrontTabNextHighlight = 0;
        g_FrontTabStartHighlight = 0;
        g_CheatMenuCursor = 0;

        if (frontCheckCursorOnPreviousTab())
        {
            g_FrontTabPrevHighlight = 1;
        }
        else
        {
            if (g_CheatMenuCount >= 0xD)
            {
                i = g_CheatMenuCount - 0xD;
                if ((s32) g_FrontCursorX >= 0xDC)
                {
                    for (; i >= 0; i--)
                    {
                        if ((s32) g_FrontCursorY >= (i * 0x14) + 0x35)
                        {
                            g_CheatMenuCursor = i + 0xC;
                            break;
                        }
                    }
                }
            }

            if (g_CheatMenuCursor == 0)
            {
                i = (g_CheatMenuCount >= 0xD)
                    ? 0xB
                    : g_CheatMenuCount - 1;

                for (; i >= 0; i--)
                {
                    if ((s32) g_FrontCursorY >= (i * 0x14) + 0x35)
                    {
                        g_CheatMenuCursor = i;
                        break;
                    }
                }
            }
        }
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, A_BUTTON|Z_TRIG))
    {
        if (g_FrontTabPrevHighlight != 0)
        {
            g_FrontTabPrevSelected = 1;
        }
        else
        {
            g_CheatMenuToggle = 1;
        }

        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }
    else if (joyGetButtonsPressedThisFrame(0, 0x4000U) != 0)
    {
        g_FrontTabPrevSelected = 1;
        sndPlaySfx(g_musicSfxBufferPtr, DOOR_METAL_CLOSE2_SFX, NULL);
    }

    frontHideAllSwitches(g_FrontFolderModels[0]);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_TABS, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_BLANK, 1);
    frontSetSwitchVisible(g_FrontFolderModels[0], SW_CLASSIFIED, 1);
    frontMoveCursor();

    if (g_FrontTabPrevSelected)
    {
        frontSetMenu(MENU_MODE_SELECT, 0);
        frontSetModeCursor(g_FrontGameMode);
        return;
    }

    if (g_CheatMenuToggle)
    {
    i = g_CheatMenuList[g_CheatMenuCursor];
        g_CheatActivated[i] = 1 - g_CheatActivated[i];
    }
}


Gfx * frontCheatRender(Gfx *DL)
{
    s32 strID;
    s32 i;
    s32 x;
    s32 y;
    s32 height;
    s32 width;
    u8 *text;

    DL = viSetFillColor(DL,0,0,0);
    DL = viFillScreen(DL);
#ifdef VERSION_EU
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
    DL = viFillScreen(DL);
#endif
    DL = frontSetupMenuBackground(DL);
    DL = textSetupDraw(DL);

    for (i = 0; i < ((g_CheatMenuCount >= 0xD) ? 0xC : g_CheatMenuCount); i++)
    {
        text = cheatGetName(g_CheatMenuList[i]);
        textMeasure(&height, &width, text, g_FontZurichChars, g_FontZurich, 0);

        x = 0x37;
        y = (i * 0x14) + 0x35;
        if ((i == g_CheatMenuCursor) && (frontCheckCursorOnPreviousTab() == 0))
        {
            DL = textRenderRect(DL, x - 2, y - 1, x + width + 5, y + 0xE, 0x32);
        }

        DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

        strID  = (g_CheatActivated[g_CheatMenuList[i]]) ? getStringID(LTITLE, TITLE_STR_115_ON) : getStringID(LTITLE, TITLE_STR_116_OFF);
        text = langGet(strID);
        textMeasure(&height, &width, text, g_FontZurichChars, g_FontZurich, 0);

        x = 0xB3;
        y = (i * 0x14) + 0x35;

        DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, (g_CheatActivated[g_CheatMenuList[i]]) ? 0xA00000FF : 0xFF, viGetX(), viGetY(), 0, 0);
    }

    if (g_CheatMenuCount >= 0xd)
    {
        for (i = 0; i < (g_CheatMenuCount - 0xC); i++)
        {
            text = cheatGetName(g_CheatMenuList[i + 12]);
            textMeasure(&height, &width, text, g_FontZurichChars, g_FontZurich, 0);

            x = 0xDC;
            y = (i * 0x14) + 0x35;
            if ((i + 0xc == g_CheatMenuCursor) && (frontCheckCursorOnPreviousTab() == 0))
            {
                DL = textRenderRect(DL, x - 2, y - 1, x + width + 5, y + 0xE, 0x32);
            }

            DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, 0xFF, viGetX(), viGetY(), 0, 0);

            strID  = (g_CheatActivated[g_CheatMenuList[i + 12]]) ? getStringID(LTITLE, TITLE_STR_115_ON) : getStringID(LTITLE, TITLE_STR_116_OFF);
            text = langGet(strID);
            textMeasure(&height, &width, text, g_FontZurichChars, g_FontZurich, 0);

            x = 0x158;
            y = (i * 0x14) + 0x35;
            strID = (g_CheatActivated[g_CheatMenuList[i + 12]]) ? 0xA00000FF : 0xFF;

            DL = frontPrintText(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, strID, viGetX(), viGetY(), 0, 0);
        }
    }

    DL = frontAddPreviousTabText(DL);

    DL = frontDrawCursor(DL);

    return DL;
}


//********************************************************************************************************
//NO CONTROLLER SCREEN
//********************************************************************************************************
void frontNoControllerEnter(void) {
    return;
}


void frontNoControllerLeave(void) {
    return;
}


void frontNoControllerTick(void) {
    return;
}


Gfx *frontNoControllerRender(Gfx *DL)
{
    s32 y2;
    s32 x2;
    s32 x;
    s32 y;
    s32 text;
    s32 conConnected = joyGetConnectedControllers();
    s32 numContCon = 0;

    if ((conConnected & 8)) {
        numContCon++;
    }
    if ((conConnected & 4)) {
        numContCon++;
    }
    if ((conConnected & 2)) {
        numContCon++;
    }
    DL = textSetupDraw(screenClearBlack(DL));


    if ((numContCon == 0) || (numContCon == 1) || (numContCon == 2) || (numContCon == 3)) {
        text = langGet(getStringID(LTITLE, TITLE_STR_118_NOCONT)); //NO CONTROLLER IN CONTROLLER SOCKET 1
    }
    textMeasure(&y2, &x2, text, g_FontZurichChars, g_FontZurich, 0);
    x = 0xDC - (x2 >> 1);
    y = 0x99 - (y2 >> 1);
    DL = textRender(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, -1, viGetX(), viGetY(), 0, 0);


    if ((numContCon == 0) || (numContCon == 1) || (numContCon == 2) || (numContCon == 3)) {
        text = langGet(getStringID(LTITLE, TITLE_STR_119_ATTACHCONT)); //PLEASE POWER OFF AND ATTACH A CONTROLLER
    }
    textMeasure(&y2, &x2, text, g_FontZurichChars, g_FontZurich, 0);
    x = 0xDC - (x2 >> 1);
    y = 0xB1 - (y2 >> 1);
    DL = textRender(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, -1, viGetX(), viGetY(), 0, 0);

    return DL;
}


void frontSetFullCast(bool doExtended)
{
    g_CastFull = doExtended;
    if (doExtended)
    {
        g_CastIndex = 0;
        return;
    }
    g_CastIndex = 1;
}


//********************************************************************************************************
//CAST SCREEN
//********************************************************************************************************
void frontCastEnter(void)
{
    u8 *bufferPtr;
    s32 bufferRemaining;
    s32 allocSize;
    struct coord3d subOffset;
    struct texpool texPool;
    enum BODIES body;
    enum HEADS head;
    s32 weaponProp;
    ModelFileHeader *bodyHeader;
    ModelFileHeader *headHeader;
    ModelFileHeader *weaponHeader;
    bool flip;
    s32 cameraPreset;
    struct intro_random_rifles rifles;
    struct intro_random_pistols pistols;
    
    bufferPtr = g_FrontModelBuffer;
    subOffset = zeroCoord;
    headHeader = NULL;
    flip = randomGetNext() & 1;

    if ((g_CastFull != 0) && (g_CastIndex == 0))
    {
        musicTrack1ApplySeqpVol(VOLUME_MAX);
        g_musicXTrack1Fade = 0;
        musicTrack1Play(M_INTRO);
    }

    for (g_CastAnimCount = 0; g_CastAnims[g_CastAnimCount].animID >= 0; g_CastAnimCount++)
    {
        // empty
    }

    g_CastAnim = randomGetNext() % ((u32) g_CastAnimCount);
    body = g_CastChars[g_CastIndex].body;
    head = g_CastChars[g_CastIndex].head;
    cameraPreset = g_CastAnims[g_CastAnim].camera_preset;
    
    if (body == BODY_Special_Operations_Uniform)
    {
        // Keep the multiple assignments on the same line for matching.
        switch (randomGetNext() % 5U) 
        { 
            case 0: 
                break; 
            case 1: 
                body = BODY_Formal_Wear; head = HEAD_Male_Brosnan_Default; 
                break; 
            case 2: 
                body = BODY_Jungle_Fatigues; head = HEAD_Male_Brosnan_Default; 
                break; 
            case 3: 
                body = BODY_Parka; head = HEAD_Male_Brosnan_Default;
                break; 
            case 4: 
                body = BODY_Brosnan_Tuxedo; head = HEAD_Male_Brosnan_Tuxedo; 
            break; 
        } 
    } 
    else if (body == BODY_Natalya_Skirt) 
    { 
        if (randomGetNext() & 1) 
        { 
            body = BODY_Natalya_Jungle_Fatigues; 
        } 
    } 
    else if ((body == BODY_Trevelyan_006) && (randomGetNext() & 1))
    { 
        body = BODY_Trevelyan_Janus; 
    } 

    if (head == HEAD_RANDOM) 
    { 
        head = bodyGetRandomHead(body); 
    } 

    allocSize = 0x19000; 
    bufferRemaining = 0x18160; 


    bufferRemaining = bufferRemaining + allocSize; 

    zbufSetBuffer(ALIGN64_V2(bufferPtr + bufferRemaining), Z_BUFFER_4_3_WIDTH, Z_BUFFER_4_3_HEIGHT);
    texInitPool(&texPool, bufferPtr, allocSize);

    bufferRemaining -= allocSize;
    bufferPtr += allocSize;
    bodyHeader = c_item_entries[body].header;

    modelFileLoad(c_item_entries[body].header, c_item_entries[body].filename, bufferPtr, bufferRemaining, &texPool);
    allocSize = ALIGN64_V2(fileGetLoadedSizeByName(c_item_entries[body].filename));
    bufferRemaining -= allocSize;
    bufferPtr += allocSize;

    if (head >= 0)
    {
        headHeader = c_item_entries[head].header;
        modelFileLoad(c_item_entries[head].header, c_item_entries[head].filename, bufferPtr, bufferRemaining, &texPool);
        allocSize = ALIGN64_V2(fileGetLoadedSizeByName(c_item_entries[head].filename));
        bufferRemaining -= allocSize;
        bufferPtr += allocSize;
    }

    g_CastModel = bodyCreateModelNew(body, head, bodyHeader, headHeader, 0);
    modelSetScale((Model *) g_CastModel, 0.1f);

    if (cameraPreset != INTRO_WEAPON_TYPE_NONE)
    {
        if (cameraPreset == INTRO_WEAPON_TYPE_RIFLE)
        {
            rifles = g_CastRifles;
            weaponProp = rifles.props[randomGetNext() % 6];
        }
        else
        {
            pistols = g_CastPistols;
            weaponProp = pistols.props[randomGetNext() % 10];
        }

        // Replace Cougar Magnum with PP7 if Cradle has not been completed on any folder.
        if (weaponProp == PROP_CHRRUGER)
        {
            if (fileIsCradleCompletedAnyFolder() == FALSE)
            {
                weaponProp = PROP_CHRWPPK;
            }
        }

        // Replace Laser with PP7 if Aztec has not been completed on Secret Agent or 00 Agent on any folder.
        if (weaponProp == PROP_CHRLASER)
        {
            if (fileIsAztecCompletedOnSecretOr00AnyFolder() == FALSE)
            {
                weaponProp = PROP_CHRWPPK;
            }
        }

        // Replace Golden Gun with PP7 if Egyptian has not been completed on 00 Agent on any folder.
        if (weaponProp == PROP_CHRGOLDEN)
        {
            if (fileIsEgyptCompletedOn00AnyFolder() == FALSE)
            {
                weaponProp = PROP_CHRWPPK;
            }
        }

        weaponHeader = PitemZ_entries[weaponProp].header;
        modelFileLoad(weaponHeader, PitemZ_entries[weaponProp].filename, bufferPtr, bufferRemaining, &texPool);
        allocSize = ALIGN64_V2(fileGetLoadedSizeByName(PitemZ_entries[weaponProp].filename));
        bufferRemaining -= allocSize;
        bufferPtr += allocSize;

        modelCalculateRwDataLen(weaponHeader);
        g_CastWeaponModel = modelmgrInstantiateModel(weaponHeader);
        modelSetScale(g_CastWeaponModel, 0.1f);
        g_CastWeaponModel->attachedto = g_CastModel;
        g_CastWeaponModel->attachedto_objinst = g_CastModel->obj->Switches[(flip != FALSE) ? (5) : (3)];
    }
    else
    {
        g_CastWeaponModel = NULL;
    }

    modelSetAnimTranslationScale(g_CastModel, 0.1f);
    modelSetRootPosition(g_CastModel, &subOffset);
    modelSetRootRotY(g_CastModel, 0.0f);

#if defined VERSION_EU
    modelSetAnimPlaySpeed(g_CastModel, 0.6f, 0);
#else
    modelSetAnimPlaySpeed(g_CastModel, 0.5f, 0);
#endif
    modelSetAnimation(g_CastModel, animation_table_ptrs1[g_CastAnims[g_CastAnim].animID], flip, g_CastAnims[g_CastAnim].startframeoffset, g_CastAnims[g_CastAnim].playback_speed, 0.0f);

    g_FrontTimer = 0;
    cast_camera_dist_start = ((((f32) ((u32) randomGetNext())) * (1.0f / U32_MAX)) * 80.0f) + 70.0f;
    cast_camera_dist_end = ((((f32) ((u32) randomGetNext())) * (1.0f / U32_MAX)) * 80.0f) + 70.0f;
    cast_camera_angle_start = ((((f32) ((u32) randomGetNext())) * (1.0f / U32_MAX)) - 0.5f) * M_TAU_F;
    cast_camera_angle_end = ((((f32) ((u32) randomGetNext())) * (1.0f / U32_MAX)) - 0.5f) * 2.5132742f;
    cast_camera_height_start = ((((f32) ((u32) randomGetNext())) * (1.0f / U32_MAX)) * 200.0f) + (-100.0f);
    cast_camera_height_end = ((((f32) ((u32) randomGetNext())) * (1.0f / U32_MAX)) * 200.0f) + (-100.0f);
    cast_camera_dist = cast_camera_dist_start;
    cast_camera_angle = cast_camera_angle_start;
    cast_camera_height = cast_camera_height_start;
    cast_camera_reset = TRUE;
    cast_rootpos_smoothed.f[0] = 0.0f;
    cast_rootpos_smoothed.f[1] = 0.0f;
    cast_rootpos_smoothed.f[2] = 0.0f;
}


void frontCastLeave(void) {
    if (g_CastModel != 0) {
        modelFreeAnimSlot(g_CastModel);
    }
    if (g_CastWeaponModel != 0) {
        modelFreeSlot(g_CastWeaponModel);
        return;
    }
}



void frontCastTick(void)
{
    s32 f;

    // Unlike all other menu functions, this one uses a FOV of 46.0 degrees instead of 60.0 degrees.
    viSetFovY(46.0f);
    viSetZRange(10.0f, 2000.0f);
    viSetUseZBuf(TRUE);
    viSetAspect(ASPECT_RATIO_SD);
    playerSetScreenSize(440, 330);
    viSetViewSize(440, 330);
    playerSetViewportPos(0, 0);
    viSetViewPosition(0, 0);

    g_FrontTimer += g_ClockTimer;

#if defined(REFRESH_PAL)
#define INTERFACE_MENU18_TIMER 151
#else
#define INTERFACE_MENU18_TIMER 181
#endif

    if (g_FrontTimer >= INTERFACE_MENU18_TIMER)
    {
#undef INTERFACE_MENU18_TIMER
        g_CastIndex++;

        while (1)
        {
            if (g_CastChars[g_CastIndex].flag && g_CastFull == 0 && 1)
            {
                g_CastIndex++;
                continue;
            }

            if ((g_CastChars[g_CastIndex].body == BODY_Moonraker_Elite_1_Male) && (fileIsAztecCompletedOnSecretOr00AnyFolder()) == 0)
            {
                g_CastIndex++;
                continue;
            }

            if ((g_CastChars[g_CastIndex].body == BODY_Moonraker_Elite_2_Female) && (fileIsAztecCompletedOnSecretOr00AnyFolder() == 0))
            {
                g_CastIndex++;
                continue;
            }

            if ((g_CastChars[g_CastIndex].body == BODY_Mayday) && (fileIsAztecCompletedOnSecretOr00AnyFolder() == 0 && (randomGetNext() % 0x2710U)))
            {
                g_CastIndex++;
                continue;
            }

            if ((g_CastChars[g_CastIndex].body == BODY_Jaws) && (fileIsAztecCompletedOnSecretOr00AnyFolder() == 0 && (randomGetNext() % 0x2710U)))
            {
                g_CastIndex++;
                continue;
            }

            if ((g_CastChars[g_CastIndex].body == BODY_Oddjob) && (fileIsEgyptCompletedOn00AnyFolder() == 0 && (randomGetNext() % 0x2710U)))
            {
                g_CastIndex++;
                continue;
            }

            if ((g_CastChars[g_CastIndex].body == BODY_Baron_Samedi) && (fileIsEgyptCompletedOn00AnyFolder() == 0 && (randomGetNext() % 0x2710U) ))
            {
                g_CastIndex++;
                continue;
            }

            break;
        }

        f = g_CastIndex;

        if (g_CastChars[f].body < 0)
        {
            g_CastIndex = 0;
            f = 0;
        }

        if (g_CastIndex > 0)
        {
            frontSetMenu(MENU_DISPLAY_CAST, 1);
        }
        else if (g_CastFull != 0)
        {
            frontSetMenu(MENU_MISSION_SELECT, 1);
            frontSetMissionCursor(SP_LEVEL_CRADLE);
            g_CastFull = 0;
        }
        else
        {
            select_ramrom_to_play();
        }
    }

    else if ((joyGetButtonsPressedThisFrame(PLAYER_1, 0xFFFF) != 0) && (!g_CastFull))
    {
        frontSetMenu(MENU_FILE_SELECT, TRUE);
    }
}


Gfx *frontCastRender(Gfx *DL)
{
    coord3d campos = cast_camera_pos;
    coord3d tarpos = cast_camera_target;
    coord3d up = cast_camera_up;
    Light *light;
    Mtxf mtx;
    ModelRenderData renderdata = cast_renderdata_default;
    Mtxf mtx2;
    s32 x;
    s32 y;
    s32 textwidth;
    s32 textheight;
    s32 i;
    f32 menufrac = (f32)g_FrontTimer / CAST_INTRO_LEN_F;
    f32 fade;
    char *text;
    coord3d vec = cast_camera_offset;
    coord3d suboffset;
    Mtx lookatmtx;
 
    if (g_FrontTimer < 0 || g_FrontTimer >= CAST_INTRO_LEN)
    {
        fade = 0.0f;
    }
    else if (g_FrontTimer < CAST_INTRO_FADE)
    {
        fade = (f32)g_FrontTimer / CAST_INTRO_FADE_F;
    }
    else if (g_FrontTimer >= CAST_INTRO_FADEOUT)
    {
        fade = (f32)(CAST_INTRO_LEN - g_FrontTimer) / CAST_INTRO_FADE_F;
    }
    else
    {
        fade = 1.0f;
    }
 
    DL = viFillScreen(viSetFillColor(DL, 0, 0, 0));
 
    cast_camera_dist = (cast_camera_dist_end - cast_camera_dist_start) * menufrac + cast_camera_dist_start;
    cast_camera_angle = (cast_camera_angle_end - cast_camera_angle_start) * menufrac + cast_camera_angle_start;
    cast_camera_height = (cast_camera_height_end - cast_camera_height_start) * menufrac + cast_camera_height_start;
 
    if (cast_camera_angle < 0.0f)
    {
        cast_camera_angle += M_TAU_F;
    }
 
    campos.x = cast_camera_dist * sinf(cast_camera_angle) + cosf(cast_camera_angle) * 0.2f * cast_camera_dist;
    campos.y = cast_camera_height;
    campos.z = cast_camera_dist * cosf(cast_camera_angle) - sinf(cast_camera_angle) * 0.2f * cast_camera_dist;
 
    tarpos.x += cosf(cast_camera_angle) * 0.2f * cast_camera_dist;
    tarpos.z += -sinf(cast_camera_angle) * 0.2f * cast_camera_dist;
 
    light = dynAllocateLights(2);
 
    guLookAtReflect(&lookatmtx, (LookAt *)light, 0.0f, 0.0f, 4000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
 
    gSPNumLights(DL++, 1); \
    gSPLight(DL++, &g_FrontLogoLight.l[0], 1); \
    gSPLight(DL++, &g_FrontLogoLight, 2);
 
    gSPLookAt(DL++, light);
 
    modelTickAnim(g_CastModel, g_ClockTimer, 1);
    modelSetLodDisabled(1);
    modelSetShadowAlpha(0);
    modelCalcPosition(g_CastModel);
 
    if (g_CastWeaponModel != NULL && g_CastWeaponModel->obj->Switches[0] != NULL)
    {
        modelGetNodeRwData( g_CastWeaponModel, g_CastWeaponModel->obj->Switches[0])->Gunfire.visible = 0;
    }
 
    if (g_CastWeaponModel != NULL && g_CastWeaponModel->obj->Switches[2] != NULL)
    {
        modelGetNodeRwData(g_CastWeaponModel, g_CastWeaponModel->obj->Switches[2])->Switch.visible = 0;
    }
 
    renderdata.mtxlist = (Mtxf *)dynAllocate(g_CastModel->obj->numMatrices << 6);
 
    mtx4LoadIdentity(&mtx);
 
    renderdata.basemtx = &mtx;
 
    modelCalcMatrices(&renderdata, g_CastModel);
    modelGetRootPosition(g_CastModel, &suboffset);
 
    if (cast_camera_reset)
    {
        cast_rootpos_smoothed.y = suboffset.y;
    }
 
    vec.f[0] = (suboffset.x - cast_rootpos_smoothed.x) / g_GlobalTimerDelta;
    vec.f[1] = (suboffset.y - cast_rootpos_smoothed.y) / g_GlobalTimerDelta;
    vec.f[2] = (suboffset.z - cast_rootpos_smoothed.z) / g_GlobalTimerDelta;
 
    if (cast_camera_reset)
    {
        cast_rootvel_accumulator.x = vec.f[0] / CAST_DAMP_COMP;
        cast_rootvel_accumulator.y = vec.f[1] / CAST_DAMP_COMP;
        cast_rootvel_accumulator.z = vec.f[2] / CAST_DAMP_COMP;
    }
 
    for (i = 0; i < g_ClockTimer; i++)
    {
        cast_rootvel_accumulator.x = vec.f[0] + CAST_DAMP * cast_rootvel_accumulator.x;
        cast_rootvel_accumulator.y = vec.f[1] + CAST_DAMP * cast_rootvel_accumulator.y;
        cast_rootvel_accumulator.z = vec.f[2] + CAST_DAMP * cast_rootvel_accumulator.z;
    }
 
    cast_rootvel_smoothed.x = cast_rootvel_accumulator.x * CAST_DAMP_COMP;
    cast_rootvel_smoothed.y = cast_rootvel_accumulator.y * CAST_DAMP_COMP;
    cast_rootvel_smoothed.z = cast_rootvel_accumulator.z * CAST_DAMP_COMP;
 
    cast_rootpos_smoothed.x += cast_rootvel_smoothed.x * g_GlobalTimerDelta;
    cast_rootpos_smoothed.y += cast_rootvel_smoothed.y * g_GlobalTimerDelta;
    cast_rootpos_smoothed.z += cast_rootvel_smoothed.z * g_GlobalTimerDelta;
 
    mtx4TransformVecInPlace(g_CastModel->render_pos, &vec);
 
    vec.f[0] -= cast_rootpos_smoothed.x;
    vec.f[1] -= cast_rootpos_smoothed.y;
    vec.f[2] -= cast_rootpos_smoothed.z;
 
    if (cast_camera_reset)
    {
        cast_camera_reset = FALSE;

        cast_target_accumulator.x = vec.f[0] / CAST_DAMP_COMP;
        cast_target_accumulator.y = vec.f[1] / CAST_DAMP_COMP;
        cast_target_accumulator.z = vec.f[2] / CAST_DAMP_COMP;
    }
 
    for (i = 0; i < g_ClockTimer; i++)
    {
        cast_target_accumulator.x = vec.f[0] + CAST_DAMP * cast_target_accumulator.x;
        cast_target_accumulator.y = vec.f[1] + CAST_DAMP * cast_target_accumulator.y;
        cast_target_accumulator.z = vec.f[2] + CAST_DAMP * cast_target_accumulator.z;
    }
 
    cast_target_smoothed.x = cast_target_accumulator.x * CAST_DAMP_COMP;
    cast_target_smoothed.y = cast_target_accumulator.y * CAST_DAMP_COMP;
    cast_target_smoothed.z = cast_target_accumulator.z * CAST_DAMP_COMP;
 
    vec.f[0] = cast_rootpos_smoothed.x + cast_target_smoothed.x;
    vec.f[1] = (cast_rootpos_smoothed.y + cast_target_smoothed.y) - 10.0f;
    vec.f[2] = cast_rootpos_smoothed.z + cast_target_smoothed.z;
 
    tarpos.x += vec.f[0];
    tarpos.y += vec.f[1];
    tarpos.z += vec.f[2];
 
    campos.x += cast_rootpos_smoothed.x;
    campos.y += cast_rootpos_smoothed.y + 52.5f;
    campos.z += cast_rootpos_smoothed.z;
 
    mtx4LoadLookAt(&mtx, campos.x, campos.y, campos.z, tarpos.x, tarpos.y, tarpos.z, up.x, up.y, up.z);
 
    renderdata.basemtx = &mtx;
    renderdata.mtxlist = (Mtxf *)dynAllocate(g_CastModel->obj->numMatrices << 6);
 
    modelCalcMatrices(&renderdata, g_CastModel);
 
    if (g_CastWeaponModel != NULL)
    {
        renderdata.basemtx = modelFindNodeMtx(g_CastModel, g_CastWeaponModel->attachedto_objinst, 0);
 
        if (g_CastWeaponModel->attachedto_objinst == g_CastModel->obj->Switches[5])
        {
            mtx4LoadZRotation(M_PI_F, &mtx2);
            mtx4MultInPlace(renderdata.basemtx, &mtx2);
 
            renderdata.basemtx = &mtx2;
        }
 
        renderdata.mtxlist = (Mtxf *)dynAllocate(g_CastWeaponModel->obj->numMatrices << 6);
 
        modelCalcMatricesNoAnim(&renderdata, g_CastWeaponModel);
    }
 
    renderdata.PropType = PROP_TYPE_EXPLOSION;
    renderdata.zbufferenabled = TRUE;
    renderdata.gdl = DL;
    renderdata.flags = 3;
 
    modelRender(&renderdata, g_CastModel);
 
    if (g_CastWeaponModel != NULL)
    {
        modelRender(&renderdata, g_CastWeaponModel);
    }
 
    DL = renderdata.gdl;
 
    modelSetLodDisabled(0);
 
    for (i = 0; i < g_CastModel->obj->numMatrices; i++)
    {
        mtx4Copy(&((s8 *)g_CastModel->render_pos)[i * sizeof(Mtxf)], &mtx2);
        mtxF2L(&mtx2, &((Mtxf *)g_CastModel->render_pos)[i]);
    }
 
    if (g_CastWeaponModel != NULL)
    {
        for (i = 0; i < g_CastWeaponModel->obj->numMatrices; i++)
        {
            mtx4Copy(&((s8 *)g_CastWeaponModel->render_pos)[i * sizeof(Mtxf)], &mtx2);
 
            mtxF2L(&mtx2, &((Mtxf *)g_CastWeaponModel->render_pos)[i]);
        }
    }
 
    gDPSetScissor(DL++, G_SC_NON_INTERLACE, 0, 0, 440, 330);
    gDPPipeSync(DL++);
    gDPSetCycleType(DL++, G_CYC_1CYCLE);
    gDPSetColorDither(DL++, G_CD_DISABLE);
    gDPSetTexturePersp(DL++, G_TP_NONE);
    gDPSetAlphaCompare(DL++, G_AC_NONE);
    gDPSetTextureLOD(DL++, G_TL_TILE);
    gDPSetTextureFilter(DL++, G_TF_BILERP);
    gDPSetTextureConvert(DL++, G_TC_FILT);
    gDPSetTextureLUT(DL++, G_TT_NONE);
    gDPSetRenderMode(DL++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(DL++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(DL++, 0, 0, 0, 0, 0, 0xFF - (s32)(255.0f * fade));
    gDPFillRectangle(DL++, 0, 0, 440, 330);
    gDPPipeSync(DL++);
    gDPSetColorDither(DL++, G_CD_BAYER);
    gDPSetTexturePersp(DL++, G_TP_PERSP);
    gDPSetTextureLOD(DL++, G_TL_LOD);
 
    DL = textSetupDraw(DL);
 
    if (g_CastFull == FALSE)
    {
        text = langGet( (u16)g_CastChars[g_CastIndex].text1);
 
        textMeasure(&textheight, &textwidth, text, g_FontZurichChars, g_FontZurich, 0);
 
        x = 315 - textwidth / 2;
        y = 108;
 
        DL = textRenderRect(DL, x, 108, x + textwidth + 1, textheight + 109, 0);
 
        DL = textRender(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, (u32)(255.0f * fade) | 0xFFFFFF00, viGetX(), viGetY(), 0, 0);
    }
 
    text = langGet((u16)g_CastChars[g_CastIndex].text2);
 
    textMeasure(&textheight, &textwidth, text, g_FontZurichChars, g_FontZurich, 0);
 
    x = 315 - textwidth / 2;
    y = 152;
 
    DL = textRenderRect(DL, x, 152, x + textwidth + 1, textheight + 153, 0);
 
    DL = textRender(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, (u32)(255.0f * fade) | 0xFFFFFF00, viGetX(), viGetY(), 0, 0);
 
    text = langGet((u16)g_CastChars[g_CastIndex].text3);
 
    textMeasure(&textheight, &textwidth, text, g_FontZurichChars, g_FontZurich, 0);
 
    x = 315 - textwidth / 2;
    y = 174;
 
    DL = textRenderRect(DL, x, 174, x + textwidth + 1, textheight + 175, 0);
 
    return textRender(DL, &x, &y, text, g_FontZurichChars, g_FontZurich, (u32)(255.0f * fade) | 0xFFFFFF00, viGetX(), viGetY(), 0, 0);
}


void frontSetMenu(MENU menu, s32 reload)
{
    if (menu == MENU_RUN_STAGE)
    {
        g_FrontLowResPending = TRUE;
    }

    if (reload)
    {
        g_FrontNextMenu = menu;
        return;
    }

    g_FrontEnterMenu = menu;
}


void frontTick(void)
{

    if (g_FrontMenu == MENU_SWITCH_SCREENS)
    {
        if (g_FrontHiResPending != 0)
        {
            if (viGetFrameBuf2() == (cfb_16[1]))
            {
                g_FrontScreenSize = SCREEN_SIZE_440x330;
                g_FrontHiResPending = 0;
            }
        }
        else if ((g_FrontLowResPending != 0) && (viGetFrameBuf2() == cfb_16[0]))
        {
            g_FrontScreenSize = SCREEN_SIZE_320x240;
            g_FrontLowResPending = 0;
        }
    }

#if defined(VERSION_EU)
#define MENU_INIT_ASPECT_440 ASPECT_RATIO_EU_400_330
#define MENU_INIT_ASPECT_320 ASPECT_RATIO_EU_320_272
#else
#define MENU_INIT_ASPECT_440 ASPECT_RATIO
#define MENU_INIT_ASPECT_320 ASPECT_RATIO
#endif

    if (g_FrontScreenSize != SCREEN_SIZE_320x240)
    {
        if (viGetFrameBuf2() == (cfb_16[1]))
        {
            viSetFrameBuf2(g_FrontFrameBuffer);
        }

        viSetAspect(MENU_INIT_ASPECT_440);
        viSetXY(440, 330);
        viSetBuf(440, 330);
        playerSetScreenSize(440, 330);
        viSetViewSize(440, 330);
        playerSetViewportPos(0, 0);
        viSetViewPosition(0, 0);
    }
    else
    {
        viSetAspect(MENU_INIT_ASPECT_320);
        viSetXY(320, SCREEN_HEIGHT);
        viSetBuf(320, SCREEN_HEIGHT);
        playerSetScreenSize(320, SCREEN_HEIGHT);
        viSetViewSize(320, SCREEN_HEIGHT);
        playerSetViewportPos(0, 0);
        viSetViewPosition(0, 0);
    }

#undef MENU_INIT_ASPECT_440
#undef MENU_INIT_ASPECT_320

    if (
        ((g_FrontNextMenu > MENU_INVALID) || (g_FrontEnterMenu > MENU_INVALID))
        && (g_FrontMenu != MENU_SWITCH_SCREENS))
    {
        switch(g_FrontMenu) {
            case MENU_LEGAL_SCREEN:           frontLegalLeave();          break;
            case MENU_NINTENDO_LOGO:          frontNintendoLeave();             break;
            case MENU_RAREWARE_LOGO:          frontRareLeave();             break;
            case MENU_EYE_INTRO:              frontGunbarrelLeave();                 break;
            case MENU_GOLDENEYE_LOGO:         frontLogoLeave();            break;
            case MENU_FILE_SELECT:            frontFileSelectLeave();              break;
            case MENU_MODE_SELECT:            frontModeSelectLeave();              break;
            case MENU_MISSION_SELECT:         frontMissionSelectLeave();           break;
            case MENU_DIFFICULTY:             frontDifficultyLeave();           break;
            case MENU_007_OPTIONS:            frontOptions007Leave();           break;
            case MENU_BRIEFING:               frontBriefingLeave();             break;
            case MENU_MISSION_FAILED:         frontDebriefLeave();        break;
            case MENU_MISSION_COMPLETE:       frontStatsLeave();      break;
            case MENU_MP_OPTIONS:             frontMpOptionsLeave();            break;
            case MENU_MP_SCENARIO_SELECT:     frontMpScenarioLeave();           break;
            case MENU_MP_CHAR_SELECT:         frontMpCharLeave();            break;
            case MENU_MP_TEAMS:               frontMpTeamsLeave();              break;
            case MENU_MP_HANDICAP:            frontMpHandicapLeave();           break;
            case MENU_MP_CONTROL_STYLE:       frontMpControlsLeave();           break;
            case MENU_MP_STAGE_SELECT:        frontMpStageLeave();              break;
            case MENU_CHEAT:                  frontCheatLeave();                break;
            case MENU_NO_CONTROLLERS:         frontNoControllerLeave();        break;
            case MENU_DISPLAY_CAST:           frontCastLeave();          break;
            case MENU_COOP_SETUP:             frontCoopSetupLeave();            break;
        }

        if (g_FrontNextMenu > MENU_INVALID)
        {
            g_FrontMenu = MENU_SWITCH_SCREENS;
            frontResetTimer();
        }
    }

    if (g_FrontEnterMenu > MENU_INVALID)
    {
        g_FrontMenu = g_FrontEnterMenu;
        g_FrontEnterMenu = MENU_INVALID;

        switch(g_FrontMenu) {
            case MENU_LEGAL_SCREEN:           frontLegalEnter();            break;
            case MENU_NINTENDO_LOGO:          frontNintendoEnter();               break;
            case MENU_RAREWARE_LOGO:          frontRareEnter();               break;
            case MENU_EYE_INTRO:              frontGunbarrelEnter();               break;
            case MENU_GOLDENEYE_LOGO:         frontLogoEnter();          break;
            case MENU_FILE_SELECT:            frontFileSelectEnter();             break;
            case MENU_MODE_SELECT:            frontModeSelectEnter();             break;
            case MENU_MISSION_SELECT:         frontMissionSelectEnter();          break;
            case MENU_DIFFICULTY:             frontDifficultyEnter();       break;
            case MENU_007_OPTIONS:            frontOptions007Enter();    break;
            case MENU_BRIEFING:               frontBriefingEnter();               break;
            case MENU_RUN_STAGE:              frontRunStageEnter();               break;
            case MENU_MISSION_FAILED:         frontDebriefEnter();          break;
            case MENU_MISSION_COMPLETE:       frontStatsEnter();        break;
            case MENU_MP_OPTIONS:             frontMpOptionsEnter();              break;
            case MENU_MP_SCENARIO_SELECT:     frontMpScenarioEnter();          break;
            case MENU_MP_CHAR_SELECT:         frontMpCharEnter();              break;
            case MENU_MP_TEAMS:               frontMpTeamsEnter();              break;
            case MENU_MP_HANDICAP:            frontMpHandicapEnter();             break;
            case MENU_MP_CONTROL_STYLE:       frontMpControlsEnter();              break;
            case MENU_MP_STAGE_SELECT:        frontMpStageEnter();                break;
            case MENU_CHEAT:                  frontCheatEnter();                  break;
            case MENU_NO_CONTROLLERS:         frontNoControllerEnter();           break;
            case MENU_DISPLAY_CAST:           frontCastEnter();            break;
            case MENU_COOP_SETUP:             frontCoopSetupEnter();              break;
        }
    }

    switch(g_FrontMenu) {
        case MENU_LEGAL_SCREEN:           frontLegalTick();           break;
        case MENU_SWITCH_SCREENS:         frontBlankTick();         break;
        case MENU_NINTENDO_LOGO:          frontNintendoTick();              break;
        case MENU_RAREWARE_LOGO:          frontRareTick();              break;
        case MENU_EYE_INTRO:              frontGunbarrelTick();                   break;
        case MENU_GOLDENEYE_LOGO:         frontLogoTick();         break;
        case MENU_FILE_SELECT:            frontFileSelectTick();            break;
        case MENU_MODE_SELECT:            frontModeSelectTick();               break;
        case MENU_MISSION_SELECT:         frontMissionSelectTick();            break;
        case MENU_DIFFICULTY:             frontDifficultyTick();            break;
        case MENU_007_OPTIONS:            frontOptions007Tick();            break;
        case MENU_BRIEFING:               frontBriefingTick();              break;
        case MENU_MISSION_FAILED:         frontDebriefTick();         break;
        case MENU_MISSION_COMPLETE:       frontStatsTick();       break;
        case MENU_MP_OPTIONS:             frontMpOptionsTick();             break;
        case MENU_MP_SCENARIO_SELECT:     frontMpScenarioTick();            break;
        case MENU_MP_CHAR_SELECT:         frontMpCharTick();             break;
        case MENU_MP_TEAMS:               frontMpTeamsTick();               break;
        case MENU_MP_HANDICAP:            frontMpHandicapTick();            break;
        case MENU_MP_CONTROL_STYLE:       frontMpControlsTick();            break;
        case MENU_MP_STAGE_SELECT:        frontMpStageTick();               break;
        case MENU_CHEAT:                  frontCheatTick();                 break;
        case MENU_NO_CONTROLLERS:         frontNoControllerTick();         break;
        case MENU_DISPLAY_CAST:           frontCastTick();           break;
        case MENU_COOP_SETUP:             frontCoopSetupTick();             break;
        case MENU_RUN_STAGE:
            if (frontRunStageTick())
            {
                frontSetMenu(MENU_LEGAL_SCREEN, 1);
            }
            else if (g_FrontGameMode == GAMEMODE_MULTI)
            {
                frontSetMenu(MENU_MP_OPTIONS, 1);
            }
            /**
             * The intro roster only shows main characters. The credits roster shows minor characters like guards.
             */
            else if (g_FrontStage == LEVELID_CUBA)
            {
                frontSetFullCast(TRUE);
                frontSetMenu(MENU_DISPLAY_CAST, 1);
            }
            else
            {
                frontSetMenu(MENU_MISSION_FAILED, 1);
            }
    }

}


Gfx * frontRender(Gfx *DL)
{
    switch(g_FrontMenu) {
        case MENU_LEGAL_SCREEN:
            DL = frontLegalRender(DL);
            break;
        case MENU_SWITCH_SCREENS:
            DL = frontBlankRender(DL);
            break;
        case MENU_NINTENDO_LOGO:
            DL = frontNintendoRender(DL);
            break;
        case MENU_RAREWARE_LOGO:
            DL = frontRareRender(DL);
            break;
        case MENU_EYE_INTRO:
            DL = frontGunbarrelRender(DL);
            break;
        case MENU_GOLDENEYE_LOGO:
            DL = frontLogoRender(DL);
            break;
        case MENU_FILE_SELECT:
            DL = frontFileSelectRender(DL);
            break;
        case MENU_MODE_SELECT:
            DL = frontModeSelectRender(DL);
            break;
        case MENU_MISSION_SELECT:
            DL = frontMissionSelectRender(DL);
            break;
        case MENU_DIFFICULTY:
            DL = frontDifficultyRender(DL);
            break;
        case MENU_007_OPTIONS:
            DL = frontOptions007Render(DL);
            break;
        case MENU_BRIEFING:
            DL = frontBriefingRender(DL);
            break;
        case MENU_MISSION_FAILED:
            DL = frontDebriefRender(DL);
            break;
        case MENU_MISSION_COMPLETE:
            DL = frontStatsRender(DL);
            break;
        case MENU_MP_OPTIONS:
            DL = frontMpOptionsRender(DL);
            break;
        case MENU_MP_SCENARIO_SELECT:
            DL = frontMpScenarioRender(DL);
            break;
        case MENU_MP_CHAR_SELECT:
            DL = frontMpCharRender(DL);
            break;
        case MENU_MP_TEAMS:
            DL = frontMpTeamsRender(DL);
            break;
        case MENU_MP_HANDICAP:
            DL = frontMpHandicapRender(DL);
            break;
        case MENU_MP_CONTROL_STYLE:
            DL = frontMpControlsRender(DL);
            break;
        case MENU_MP_STAGE_SELECT:
            DL = frontMpStageRender(DL);
            break;
        case MENU_CHEAT:
            DL = frontCheatRender(DL);
            break;
        case MENU_NO_CONTROLLERS:
            DL = frontNoControllerRender(DL);
            break;
        case MENU_DISPLAY_CAST:
            DL = frontCastRender(DL);
            break;
        case MENU_COOP_SETUP:
            DL = frontCoopSetupRender(DL);
    }

    return DL;
}


void frontInit(void)
{
    s32 i;
    g_FrontMenu = ~MENU_LEGAL_SCREEN;
    g_FrontEnterMenu = ~MENU_LEGAL_SCREEN;
    g_FrontScreenSize = SCREEN_SIZE_320x240;
    g_FrontHiResPending = TRUE;
    g_FrontLowResPending = FALSE;
    g_FrontFolderIcon = 0;
    g_FrontFolderErase = -1;
    g_FrontFolderEraseChoice = 1;
    g_FrontTabStartHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    g_FrontStartMusic = TRUE;

    if (g_FrontSkipIntro)
    {
        g_FrontNextMenu = MENU_FILE_SELECT;
    }

    if (g_FrontNextMenu < 0)
    {
        g_FrontNextMenu = MENU_FILE_SELECT;
    }

    g_FrontModelBuffer = (u8 *)mempAlloc(0x78000, MEMPOOL_STAGE); // allocation in bytes so cast to u8 from gfx

#if defined(VERSION_EU)
    g_FrontFrameBuffer = mempAlloc(0x55040, MEMPOOL_STAGE);
#else
    g_FrontFrameBuffer = mempAlloc(0x4b040, MEMPOOL_STAGE);
#endif

    g_FrontFrameBuffer = ALIGN64_V1(g_FrontFrameBuffer);

    for (i = 0; i < 4; i++)
    {
        g_FrontFolderModels[i] = NULL;
    }

    titleAllocMatrices();
}
