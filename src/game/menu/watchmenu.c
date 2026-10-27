/**
 * The watch menu: the pages Bond's watch shows when the game is paused in solo and co-op
 * (mission status and abort, inventory, controls, options, briefing), how the controller
 * moves through them (watchMenuTick and the per-page watchMenu*Tick), and how each page is
 * drawn (watchMenuRender). Also the per-player settings the options pages change
 * (options*), which are kept in the player struct. See docs/subsystems/menu.md.
 */
#include <ultra64.h>
#include <bondconstants.h>
#include <boss.h>
#include <fr.h>
#include <music.h>
#include <os_extension.h>
#include <snd.h>
#include <random.h>
#include "game/menu/watchmenu.h"
#include "game/player/bondview.h"
#include "game/gfx/dyn.h"
#include "game/save/file.h"
#include "game/menu/front.h"
#include "game/text/language.h"
#include "game/player/player.h"
#include "game/text/textrelated.h"
#include "game/prop/glass.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/level/frametiming.h"
#include "assets/obseg/text/LoptionE.h"

// The controller that drives the watch: in co-op, the player who has it out.
#define WATCH_PAD (g_CoopMode ? playerGetCurrentNum() : PLAYER_1)

#define WATCH_BACKGROUND_VERTEX_COUNT 30

#define WATCH_VOL_ADJUST_STEP 1024

#if defined(VERSION_US)
#define WATCH_ROTATION_FRAMES g_FrameDelta
#else
#define WATCH_ROTATION_FRAMES g_FrameDelta60
#endif

#if defined(VERSION_EU)
#define WATCH_PERSPECTIVE_FOVY    52.5f
#define WATCH_PERSPECTIVE_ASPECT  1.283847f
#else
#define WATCH_PERSPECTIVE_FOVY    50.5f
#define WATCH_PERSPECTIVE_ASPECT  1.3333334f
#endif

#if defined(VERSION_EU)
#define OPTLABELS_ROW1_Y    0x5c
#define OPTLABELS_ROW2_Y    0x7a
#define OPTLABELS_ROW_PITCH 0x1e
#define OPTLABELS_COL_RET   0x5a
#define OPTLABELS_HINT_Y    0xe1
#else
#define OPTLABELS_ROW1_Y    0x52
#define OPTLABELS_ROW2_Y    0x6b
#define OPTLABELS_ROW_PITCH 0x19
#define OPTLABELS_COL_RET   0x4b
#define OPTLABELS_HINT_Y    0xc3
#endif

// bss
Mtx gfx_background_8007B0A0;
Mtx gfx_background_8007B0E0;

u32 D_80040990 = 0;
u32 g_WatchMenuPage = 0;
u32 g_WatchMenuControlRow = 0;
u32 g_WatchMenuOptionsRow = 0;
// data
//D:800409A0
s32 g_WatchMenuBriefPage = BRIEF_INDEX_OBJECTIVES;
//D:800409A4
s32 g_WatchMenuAbortAsk = 0;
//D:800409A8
s32 g_WatchMenuSelected = 0;
//D:800409AC
s32 D_800409AC = 0;
//D:800409B0
s32 g_WatchMenuInvTextY = 0;
//D:800409B4
s32 g_WatchMenuInvTextTargetY = 0;
//D:800409B8
s32 g_WatchMenuItem = 0;
//D:800409BC
f32 g_WatchMenuInvCursor = 0.0f;
//D:800409C0
bool g_WatchMenuInvSettled = FALSE;
//D:800409C4
s32 g_WatchMenuEquipDelay = 0;
//D:800409C8
f32 D_800409C8 = 0.0f;
//D:800409CC
f32 D_800409CC = 0.0f;
//D:800409D0
s32 D_800409D0 = -1;
//D:800409D4
f32 D_800409D4 = 0.0f;
//D:800409D8
s32 g_WatchMenuControlStyleCount = 8;
//D:800409DC
u16 game_control_styles[] = {
    /*1.1 honey*/    getStringID(LOPTIONS, OPTION_STR_09_11HONEY_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF), /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF),
    /*1.2 solitaire*/getStringID(LOPTIONS, OPTION_STR_0A_12SOLITAIRE_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF), /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF),
    /*1.3 kissy*/    getStringID(LOPTIONS, OPTION_STR_0B_13KISSY_LF),   /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF),  /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF),
    /*1.4 goodnight*/getStringID(LOPTIONS, OPTION_STR_0C_14GOODNIGHT_LF),   /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF),  /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF),
    /*2.1 plenty*/   getStringID(LOPTIONS, OPTION_STR_0D_21PLENTY_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.2 galore*/   getStringID(LOPTIONS, OPTION_STR_0E_22GALORE_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.3 domino*/   getStringID(LOPTIONS, OPTION_STR_0F_23DOMINO_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.4 goodhead*/ getStringID(LOPTIONS, OPTION_STR_10_24GOODHEAD_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF)
};

struct game_options g_Options[] = {
    { {getStringID(LOPTIONS, OPTION_STR_11_LOOKUPDOWN_LF), getStringID(LOPTIONS, OPTION_STR_1C_REVERSE_LF), getStringID(LOPTIONS, OPTION_STR_1B_UPRIGHT_LF), 0}, 0}, //look up/down, reverse, upright
    { {getStringID(LOPTIONS, OPTION_STR_12_AUTOAIM_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //autoaim, off, on
    { {getStringID(LOPTIONS, OPTION_STR_14_AIMCONTROL_LF), getStringID(LOPTIONS, OPTION_STR_1E_HOLD_LF), getStringID(LOPTIONS, OPTION_STR_1D_TOGGLE_LF), 0}, 0}, //aim control, hold, toggle
    { {getStringID(LOPTIONS, OPTION_STR_15_SIGHTONSCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //sight on screen, off, on
    { {getStringID(LOPTIONS, OPTION_STR_13_LOOKAHEAD_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //look ahead, off, on
    { {getStringID(LOPTIONS, OPTION_STR_16_AMMOONSCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //ammo on screen, off, on
    { {getStringID(LOPTIONS, OPTION_STR_17_SCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1F_FULL_LF), getStringID(LOPTIONS, OPTION_STR_20_WIDE_LF), getStringID(LOPTIONS, OPTION_STR_21_CINEMA_LF)}, 0}, //screen, full, wide, cinema
    { {getStringID(LOPTIONS, OPTION_STR_18_RATIO_LF), getStringID(LOPTIONS, OPTION_STR_22_NORMAL_LF), getStringID(LOPTIONS, OPTION_STR_23_169_LF), 0}, 0}, //ratio, normal, 16:9
    { {getStringID(LOPTIONS, OPTION_STR_3D_HIRES_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 0}, //hi-res, off, on
    { {getStringID(LOPTIONS, OPTION_STR_3E_SPLITSCREEN_LF), getStringID(LOPTIONS, OPTION_STR_40_SIDE_LF), getStringID(LOPTIONS, OPTION_STR_41_STACKED_LF), 0}, 1}, //split screen, side, stacked (default)
    { {getStringID(LOPTIONS, OPTION_STR_3F_FPSCOUNTER_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 0}, //fps counter, off, on
    { {getStringID(LOPTIONS, OPTION_STR_42_DRAWDISTANCE_LF), getStringID(LOPTIONS, OPTION_STR_1F_FULL_LF), getStringID(LOPTIONS, OPTION_STR_43_NEAR_LF), 0}, 0} //draw distance (co-op), full, near
};

//D:80040ADC
u32 g_WatchMenuStickXReady = 0;
//D:80040AE0
u32 g_WatchMenuStickYReadyFlag = 0;
//D:80040AE4
u32 g_WatchMenuStickYActive = 0;
//D:80040AE8
f32 D_80040AE8 = 0.0f;
//D:80040AEC
f32 D_80040AEC = 0.0f;
//D:80040AF0
f32 D_80040AF0 = 45.0f;
//D:80040AF4
u32 D_80040AF4 = 0xFF00A0;
//D:80040AF8
u32 D_80040AF8 = 0xA;
//D:80040AFC
u32 D_80040AFC = 0xFF;

//D:80040B00
u32 D_80040B00 = 0xA;

//D:80040B04
s32 g_WatchBackgroundGreen = 0xE0;

//D:80040B08
u32 g_WatchStaticScanlineAlpha = 0;

//D:80040B0C
u32 D_80040B0C = 0xFFA0;
//D:80040B10
u32 D_80040B10 = 0xF800;
//D:80040B14
f32 D_80040B14 = 0.0f;
//D:80040B18
f32 D_80040B18 = 0.0f;
//D:80040B1C
f32 D_80040B1C = 2.5f;
//D:80040B20
f32 g_WatchControllerPitch = 0.0f;
//D:80040B24
f32 g_WatchControllerSpinAngle = 0.0f;
//D:80040B28
f32 g_WatchControllerSpinSpeed = 0.0f;
//D:80040B2C
s32 D_80040B2C = 0;
//D:80040B30
f32 D_80040B30 = 0.0f;
//D:80040B34
f32 D_80040B34 = 0.0f;
//D:80040B38
f32 D_80040B38 = 0.0f;
//D:80040B3C
s32 D_80040B3C = 0;
//D:80040B40
s32 g_WatchStaticScanlineY = 0;
//D:80040B44
u16 D_80040B44 = 0x1;
//D:80040B48
u32 D_80040B48 = 0x32;
//D:80040B4C
u32 D_80040B4C = 0x32;
//D:80040B50
u32 D_80040B50 = 0x32;
//D:80040B54
u32 D_80040B54 = 0x32;
//D:80040B58
u16 g_OptionsMusicVolume = 0x7FFF;

//D:80040B5C
coord3d g_ControllerPos = {0.0f, 200.0f, -200.0f};

typedef struct WatchContButtonPositions {
    f32 words[55]; /* 0xdc bytes */
} WatchContButtonPositions;

//D:80040B68
// 3D positions for the buttons at the sides of the screen for the 1x control styles.
WatchContButtonPositions g_1ContButtonPositions[] = {{
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f,
    -900.0f, 200.0f, -45.0f,     // Start
    715.0f, 200.0f, 393.0f,      // Joy Stick
    -875.0f, 200.0f, -210.0f,    // D-Pad
    900.0f, 200.0f, -260.0f,     // C-Up
    900.0f, 200.0f, -160.0f,     // C-Down
    850.0f, 200.0f, -210.0f,     // C-Left
    950.0f, 200.0f, -210.0f,     // C-Right
    900.0f, 200.0f, 128.0f,      // A
    900.0f, 200.0f, -45.0f,      // B
    -820.0f, 200.0f, -389.0f,    // L
    820.0f, 200.0f, -389.0f,     // R
    -830.0f, 200.0f, 78.0f       // Z
}};

//D:80040C44
// 3D positions for the buttons of the left controller for the 2x control styles.
WatchContButtonPositions g_2ContLeftButtonPositions[] = {{
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, -600.0f, 200.0f,
    500.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, -600.0f, 200.0f,
    240.0f, -600.0f, 200.0f,
    110.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, -600.0f, 200.0f,
    320.0f
}};

//D:80040D20
// 3D positions for the buttons of the right controller for the 2x control styles.
WatchContButtonPositions g_2ContRightButtonPositions[] = {{
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    2000.0f, 0.0f, 2000.0f, 2000.0f,
    0.0f, 2000.0f, 600.0f, 200.0f,
    500.0f, 2000.0f, 0.0f, 2000.0f,
    2000.0f, 0.0f, 2000.0f, 2000.0f,
    0.0f, 2000.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f, 2000.0f,
    600.0f, 200.0f, 240.0f, 600.0f,
    200.0f, 110.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f, 2000.0f,
    600.0f, 200.0f, 320.0f
}};

//D:80058440
const char D_80058440[];
//D:80058444
const char D_80058444[];
//D:80058448
const char aC_2[];
//D:80058450
const char D_80058450[];
//D:80058454
const char D_80058454[];

// forward declarations

void watchMenuSetTabColours(s32 g_WatchMenuPage, struct WatchVertex *vertices);
Gfx *watchMenuRenderStatus(Gfx *gdl, Mtx *param_2);
Gfx *watchMenuRenderInventoryOld(Gfx *gdl, Mtx *param_2);
Gfx *watchMenuRenderInventory(Gfx *gdl, Mtx *param_2);
Gfx *watchMenuRenderControl(Gfx *gdl, Mtx *param_2);
Gfx *watchMenuRenderOptions(Gfx *gdl, Mtx *param_2);
Gfx *watchMenuRenderBriefing(Gfx *gdl, Mtx *param_2);
Gfx *watchMenuRenderBackgroundZoom(Gfx *gdl, Mtx *param_2);
Gfx *watchMenuRenderBackground(Gfx *gdl, Mtx *arg1, s32 zoom_squish);
void watchMenuOptionSet(u32 *param_1, u32 param_2);
void watchMenuAdjustVolume(u16* arg0);
Gfx *watchRenderQuad(Gfx *gdl, s32 *arg1);
void watchMenuMakeSliderVerts(struct WatchVertex *verts, f32 fill_amount, s32 transition_width);
void watchMenuSpinController(s8 contpadnum, s32 *counter, f32 *value, f32 *step);
Gfx *watchMenuRenderDualControlLabels(Gfx *gdl);
Gfx *watchMenuRenderControlLabels(Gfx *gdl);

// end forward declarations



void watchMenuNull(void)
{

}


void watchMenuReset(int stage)
{
    g_WatchMenuPage = WATCH_INDEX_MISSION_STATUS;
    g_WatchMenuControlRow = CONTROLLER_OPTIONS_INDEX_STYLE;
    g_WatchMenuOptionsRow = GAME_OPTIONS_INDEX_MUSIC;
    g_WatchMenuBriefPage = BRIEF_INDEX_OBJECTIVES;
    g_WatchMenuAbortAsk = 0;
    g_WatchMenuSelected = 0;
    D_800409AC = 0;
    g_WatchMenuInvTextY = 0;
    g_WatchMenuInvTextTargetY = 0;
    g_WatchMenuItem = 0;
    g_WatchMenuInvCursor = 0.0f;
    g_WatchMenuInvSettled = FALSE;
    g_WatchMenuEquipDelay = 0;
    D_800409C8 = 0.0f;
    D_800409CC = 0.0f;
    D_800409D0 = -1;
    D_800409D4 = 0.0f;

    if (j_text_trigger ? 1 : 0 && 1)
    {
    }

    g_CurrentPlayer->neg_vspacing_for_control_type_entry = 0;
    g_CurrentPlayer->cur_player_control_type_1 = CONTROLLER_CONFIG_HONEY;
    g_CurrentPlayer->cur_player_control_type_0 = CONTROLLER_CONFIG_HONEY;
    g_CurrentPlayer->cur_player_control_type_2 = 0.0f;
    g_CurrentPlayer->has_set_control_type_data = TRUE;
    g_WatchMenuControlStyleCount = 8;

    g_WatchMenuStickXReady = 0;
    g_WatchMenuStickYReadyFlag = 0;
    g_WatchMenuStickYActive = 0;
    D_80040AE8 = 0.0f;
    D_80040AEC = 0.0f;
    D_80040AF0 = 45.0f;
    D_80040AF4 = 0xff00a0;
    D_80040AF8 = 10;
    D_80040AFC = 0xff;
    D_80040B00 = 10;
    g_WatchBackgroundGreen = 0xe0;
    g_WatchStaticScanlineAlpha = 0;
    D_80040B0C = 0xffa0;
    D_80040B10 = 0xf800;
    D_80040B14 = 0.0f;
    D_80040B18 = 0.0f;
    D_80040B1C = 2.5f;
    g_WatchControllerPitch = 0.0f;
    g_WatchControllerSpinAngle = 0.0f;
    g_WatchControllerSpinSpeed = 0.0f;
    D_80040B2C = 0;
    D_80040B30 = 0.0f;
    D_80040B34 = 0.0f;
    D_80040B38 = 0.0f;
    D_80040B3C = 0;
    g_WatchStaticScanlineY = 0;
    D_80040B44 = 1;
    D_80040B48 = 0x32;
    D_80040B4C = 0x32;
    D_80040B50 = 0x32;
    D_80040B54 = 0x32;
    // The save folder's options belong to controller 1's player.
    optionsCoopUsePlayer(0);
    fileLoadSaveSettingsForSelectedFolder(stage);
    g_FrontMissionAborted = FALSE;
}


void watchMenuDebugNudge(void)
{
    if (10 < joyGetStickX(WATCH_PAD))
    {
        D_80040B50 = D_80040B50 + 1;
    }
    if (joyGetStickX(WATCH_PAD) < -10)
    {
        D_80040B50 = D_80040B50 + -1;
    }
    if (10 < joyGetStickY(WATCH_PAD))
    {
        D_80040B54 = D_80040B54 + -1;
    }
    if (joyGetStickY(WATCH_PAD) < -10)
    {
        D_80040B54 = D_80040B54 + 1;
    }
}


Gfx * watchMenuDebugNudgeGfx(Gfx *DL)
{
    if (10 < joyGetStickX(WATCH_PAD))
    {
        D_80040B48 += 1;
    }

    if (joyGetStickX(WATCH_PAD) < -10)
    {
        D_80040B48 -= 1;
    }

    if (10 < joyGetStickY(WATCH_PAD))
    {
        D_80040B4C -= 1;
    }

    if (joyGetStickY(WATCH_PAD) < -10)
    {
        D_80040B4C += 1;
    }

    gDPSetRenderMode(DL++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(DL++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(DL++, 0, 0, 0xFF, 0x00, 0x00, 0xFF);
    gDPFillRectangle(DL++, VI_SX(D_80040B48), D_80040B4C, VI_SX(D_80040B48+1), D_80040B4C+1);

    {
        u8 buffer [0x12];
        struct font * pFontFile;
        struct fontchar * pFontChars;
        s32 y;
        s32 x;

        pFontFile = ptrFontBankGothic;
        pFontChars = ptrFontBankGothicChars;
        sprintf(buffer,"%d, %d\n",D_80040B48,D_80040B4C);

        DL = microcode_constructor(DL++);

        textMeasure(&x, &y, buffer, pFontChars, pFontFile, 0);
        DL = textRender(DL, &D_80040B48, &D_80040B4C, buffer, pFontChars, pFontFile, 0xff0000ff, y, x, 0, 0);
        // HACK: what is this: ((s32*)pFontChars)[0x224]
        D_80040B4C = (D_80040B4C - ((s32*)pFontChars)[0x224]) + 1;
    }

    return DL;
}


u32 optionsIsDualControlStyle(void)
{
    if (joyGetControllerCount() >= 2)
    {
        if (optionsGetControlStyle() >= 4)
        {
            return 1;
        }
    }
    return 0;
}


int optionsGetControlStyle(void){
  return g_CurrentPlayer->cur_player_control_type_0;
}


void optionsSetControlStyle(int type)
{
    int langsize;

    g_CurrentPlayer->cur_player_control_type_0 = type;
    g_CurrentPlayer->cur_player_control_type_1 = type;
    g_CurrentPlayer->cur_player_control_type_2 = (float)type;

    langsize = j_text_trigger ? 14 : 10;

    g_CurrentPlayer->neg_vspacing_for_control_type_entry = -(langsize * type);
    g_CurrentPlayer->has_set_control_type_data = TRUE;

}

u32 optionsGetLookInverted(void)
{
    return g_Options[PLAYER_OPTION_LOOK].current_value;
}

void optionsSetLookInverted(u32 param_1)
{
    g_Options[PLAYER_OPTION_LOOK].current_value = param_1;
}

s32 optionsGetAutoAim(void)
{
    return g_Options[PLAYER_OPTION_AUTOAIM].current_value;
}

void optionsSetAutoAim(u32 param_1)
{
    g_Options[PLAYER_OPTION_AUTOAIM].current_value = param_1;
}

u32 optionsGetLookAhead(void)
{
    return g_Options[PLAYER_OPTION_LOOKAHEAD].current_value;
}

void optionsSetLookAhead(u32 param_1)
{
    g_Options[PLAYER_OPTION_LOOKAHEAD].current_value = param_1;
}

u32 optionsGetAimControl(void)
{
    return g_Options[PLAYER_OPTION_AIM].current_value;
}

void optionsSetAimControl(u32 param_1)
{
    g_Options[PLAYER_OPTION_AIM].current_value = param_1;
}

u32 optionsGetSightOnScreen(void)
{
    return g_Options[PLAYER_OPTION_SIGHT].current_value;
}
void optionsSetSightOnScreen(u32 param_1)
{
    g_Options[PLAYER_OPTION_SIGHT].current_value = param_1;
}

u32 optionsGetAmmoOnScreen(void)
{
    return g_Options[PLAYER_OPTION_AMMODISPLAY].current_value;
}
void optionsSetAmmoOnScreen(u32 param_1)
{
    g_Options[PLAYER_OPTION_AMMODISPLAY].current_value = param_1;
}

u32 optionsGetScreenSize(void)
{
    return g_Options[PLAYER_OPTION_SCREEN].current_value;
}
void optionsSetScreenSize(u32 param_1)
{
    g_Options[PLAYER_OPTION_SCREEN].current_value = param_1;
}

SCREEN_RATIO_OPTION optionsGetScreenRatio(void)
{
    return g_Options[PLAYER_OPTION_RATIO].current_value;
}

void optionsSetScreenRatio(SCREEN_RATIO_OPTION ratio_option)
{
    g_Options[PLAYER_OPTION_RATIO].current_value = ratio_option;
}

u32 optionsGetHiRes(void)
{
    return g_Options[PLAYER_OPTION_HIRES].current_value;
}

void optionsSetHiRes(u32 enabled)
{
    g_Options[PLAYER_OPTION_HIRES].current_value = enabled;
}

u32 optionsGetSplitStacked(void)
{
    return g_Options[PLAYER_OPTION_SPLIT].current_value;
}

void optionsSetSplitStacked(u32 stacked)
{
    g_Options[PLAYER_OPTION_SPLIT].current_value = stacked;
}

u32 optionsGetShowFps(void)
{
    return g_Options[PLAYER_OPTION_FPS].current_value;
}

void optionsSetShowFps(u32 enabled)
{
    g_Options[PLAYER_OPTION_FPS].current_value = enabled;
}

u32 optionsGetDrawNear(void)
{
    return g_Options[PLAYER_OPTION_DRAWDIST].current_value;
}

void optionsSetDrawNear(u32 near)
{
    g_Options[PLAYER_OPTION_DRAWDIST].current_value = near;
}

/**
 * Co-op: g_Options holds the current player's own options (look,
 * aim, sight, ammo, screen size). playerSetCurrent swaps them in, so the watch
 * and the options* getters see that player's values. Ratio, hi-res,
 * split screen and the fps counter are shared.
 */
#define COOP_PLAYER_OPTION_COUNT (PLAYER_OPTION_SCREEN + 1)

static u32 s_CoopOptions[2][COOP_PLAYER_OPTION_COUNT];
static s32 s_CoopOptionsPlayer = 0;

void optionsCoopUsePlayer(s32 player)
{
    s32 i;

    if (player == s_CoopOptionsPlayer)
    {
        return;
    }

    for (i = 0; i < COOP_PLAYER_OPTION_COUNT; i++)
    {
        s_CoopOptions[s_CoopOptionsPlayer][i] = g_Options[i].current_value;
        g_Options[i].current_value = s_CoopOptions[player][i];
    }

    s_CoopOptionsPlayer = player;
}

/**
 * Gives player 2 a copy of player 1's options. Called when player 2's
 * options are swapped in.
 */
void optionsCoopCopyFromPlayer1(void)
{
    s32 i;

    if (s_CoopOptionsPlayer == 0)
    {
        return;
    }

    for (i = 0; i < COOP_PLAYER_OPTION_COUNT; i++)
    {
        g_Options[i].current_value = s_CoopOptions[0][i];
    }
}


void watchMenuToggleSelect(void) {

    if (g_WatchMenuSelected == 1) {
        g_WatchMenuSelected = 0;

    } else {
        g_WatchMenuSelected = 1;
        sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
    }
}


void watchMenuClearSelect(void){
  g_WatchMenuSelected = 0;
}


u32 watchMenuStickLeft(void)
{
    return (joyGetStickX(WATCH_PAD) < -0x2d);
}


u32 watchMenuStickRight(void)
{
    return ((joyGetStickX(WATCH_PAD) < 0x2e) ^ 1);
}


u32 watchMenuStickXReady(void) {
  return g_WatchMenuStickXReady;
}


void watchMenuStickXUsed(void) {
  g_WatchMenuStickXReady = 0;
}


s32 watchMenuStickPushedLeft(void)
{
    return watchMenuStickLeft() && watchMenuStickXReady();
}


s32 watchMenuStickPushedRight(void)
{
    return watchMenuStickRight() && watchMenuStickXReady();
}


u32 watchMenuStickUp(void)
{
    return (joyGetStickY(WATCH_PAD) < 0x2e) ^ 1;
}


u32 watchMenuStickDown(void)
{
    return (joyGetStickY(WATCH_PAD) < -0x2d);
}


u32 watchMenuStickYReady(void)
{
    return g_WatchMenuStickYReadyFlag;
}


void watchMenuStickYUsed(void)
{
    g_WatchMenuStickYReadyFlag = 0;
}


s32 watchMenuStickPushedUp(void)
{
    return watchMenuStickUp() && watchMenuStickYReady();
}


s32 watchMenuStickPushedDown(void)
{
    return watchMenuStickDown() && watchMenuStickYReady();
}


u32 watchMenuStickNudgedUp(void)
{
    return (joyGetStickY(WATCH_PAD) < 0x10) ^ 1;
}


u32 watchMenuStickNudgedDown(void)
{
    return (joyGetStickY(WATCH_PAD) < -0xf);
}


u32 watchMenuStickYWasActive(void)
{
    return g_WatchMenuStickYActive;
}


void watchMenuStickYReset(void) 
{
    g_WatchMenuStickYActive = 0;
}


s32 watchMenuStickTappedUp(void)
{
    return watchMenuStickNudgedUp() && !watchMenuStickYWasActive();
}


s32 watchMenuStickTappedDown(void)
{
    return watchMenuStickNudgedDown() && !watchMenuStickYWasActive();
}


void watchMenuStatic(void)
{
    g_WatchBackgroundGreen = 0x80;
    sndPlaySfx(g_musicSfxBufferPtr, WATCH_STATIC_SFX, NULL);
    return;
}


void watchMenuPageBeep(void)
{
    watchMenuStickXUsed();
    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, NULL);
    if ((D_80040B10 << 0x10) < randomGetNext()) {
        watchMenuStatic();
    }
    return;
}


// initial pause screen: WATCH_INDEX_MISSION_STATUS
void watchMenuStatusTick(void)
{
    s32 goto_watch_screen_index_4;
    s32 goto_watch_screen_index_1;

    if (g_WatchMenuSelected == 0)
    {
        goto_watch_screen_index_4 = FALSE;
        goto_watch_screen_index_1 = FALSE;

        if (get_debug_gunwatchpos_flag() == 0)
        {
            if (joyGetButtonsPressedThisFrame(WATCH_PAD, L_TRIG|L_CBUTTONS))
            {
                goto_watch_screen_index_4 = TRUE;
            }
            if (joyGetButtonsPressedThisFrame(WATCH_PAD, R_TRIG|R_CBUTTONS))
            {
                goto_watch_screen_index_1 = TRUE;
            }
        }


        if ((joyGetButtonsPressedThisFrame(WATCH_PAD, L_JPAD)) || (watchMenuStickPushedLeft()))
        {
            goto_watch_screen_index_4 = TRUE;
        }

        if ((joyGetButtonsPressedThisFrame(WATCH_PAD, R_JPAD)) || (watchMenuStickPushedRight()))
        {
            goto_watch_screen_index_1 = TRUE;
        }

        if (goto_watch_screen_index_4)
        {
            g_WatchMenuPage = WATCH_INDEX_MISSION_BRIEFING;
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM1, 15.0f);
        }

        if (goto_watch_screen_index_1)
        {
            g_WatchMenuPage = WATCH_INDEX_INVENTORY;
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM1, 15.0f);
            return;
        }
    }
    else if ((g_WatchMenuAbortAsk) && (joyGetButtonsPressedThisFrame(WATCH_PAD, Z_TRIG|A_BUTTON)))
    {
        g_WatchMenuAbortAsk = 0;
        set_missionstate(MISSION_STATE_0);
        bossRunTitleStage();
        g_FrontMissionAborted = TRUE;
        deleteCurrentSelectedFolder();
    }
}


// pause screen: WATCH_INDEX_INVENTORY
void watchMenuInventoryTick(void)
{
    s32 goto_watch_screen_index_0;
    s32 goto_watch_screen_index_2;

    if (g_WatchMenuSelected == 0)
    {
        goto_watch_screen_index_0 = FALSE;
        goto_watch_screen_index_2 = FALSE;

        if (get_debug_gunwatchpos_flag() == FALSE)
        {
            if (joyGetButtonsPressedThisFrame(WATCH_PAD, L_TRIG|L_CBUTTONS))
            {
                goto_watch_screen_index_0 = TRUE;
            }
            if (joyGetButtonsPressedThisFrame(WATCH_PAD, R_TRIG|R_CBUTTONS))
            {
                goto_watch_screen_index_2 = TRUE;
            }
        }

        if ((joyGetButtonsPressedThisFrame(WATCH_PAD, L_JPAD)) || (watchMenuStickPushedLeft()))
        {
            goto_watch_screen_index_0 = TRUE;
        }

        if ((joyGetButtonsPressedThisFrame(WATCH_PAD, R_JPAD)) || (watchMenuStickPushedRight()))
        {
            goto_watch_screen_index_2 = TRUE;
        }

        if (goto_watch_screen_index_0)
        {
            g_WatchMenuPage = WATCH_INDEX_MISSION_STATUS;
            watchMenuResetAbort();
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM2, 15.0f);

        }

        if (goto_watch_screen_index_2)
        {
            g_WatchMenuPage = WATCH_INDEX_CONTROL_OPTIONS;
            watchMenuStickXUsed();
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM3, 15.0f);
        }
    }
}


void watchMenuUnusedTick(void) {

    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, L_CBUTTONS|L_TRIG|L_JPAD)) || (watchMenuStickPushedLeft()))
    {
        if (g_WatchMenuSelected == 0)
        {
            g_WatchMenuPage = WATCH_INDEX_INVENTORY;
            watchMenuStickXUsed();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, R_CBUTTONS|R_TRIG|R_JPAD)) || (watchMenuStickPushedRight()))
    {
        if (g_WatchMenuSelected == 0)
        {
            g_WatchMenuPage = WATCH_INDEX_CONTROL_OPTIONS;
            watchMenuResetControlRow();
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM3, 15.0f);
        }
    }
}


// WATCH_INDEX_CONTROL_OPTIONS
void watchMenuControlTick(void) {

    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, L_CBUTTONS|L_TRIG|L_JPAD)) || (watchMenuStickPushedLeft()))
    {
        if ((joyGetButtons(WATCH_PAD, Z_TRIG) == 0) && (g_WatchMenuSelected == 0))
        {
            g_WatchMenuPage = WATCH_INDEX_INVENTORY;
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM1, 15.0f);
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, R_CBUTTONS|R_TRIG|R_JPAD)) || (watchMenuStickPushedRight()))
    {
        if ((joyGetButtons(WATCH_PAD, Z_TRIG) == 0) && (g_WatchMenuSelected == 0))
        {
            g_WatchMenuPage = WATCH_INDEX_GAME_OPTIONS;
            watchMenuResetOptionsRow();
            watchMenuStickXUsed();
        }
    }
}


// WATCH_INDEX_GAME_OPTIONS
void watchMenuOptionsTick(void) {

    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, L_CBUTTONS|L_TRIG|L_JPAD)) || (watchMenuStickPushedLeft()))
    {
        if ((joyGetButtons(WATCH_PAD, Z_TRIG) == 0) && (g_WatchMenuSelected == 0))
        {
            g_WatchMenuPage = WATCH_INDEX_CONTROL_OPTIONS;
            watchMenuResetControlRow();
            watchMenuStickXUsed();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, R_CBUTTONS|R_TRIG|R_JPAD)) || (watchMenuStickPushedRight()))
    {
        if ((joyGetButtons(WATCH_PAD, Z_TRIG) == 0) && (g_WatchMenuSelected == 0))
        {
            g_WatchMenuPage = WATCH_INDEX_MISSION_BRIEFING;
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM1, 15.0f);
        }
    }
}


// WATCH_INDEX_MISSION_BRIEFING
void watchMenuBriefingTick(void) {

    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, L_CBUTTONS|L_TRIG|L_JPAD)) || (watchMenuStickPushedLeft()))
    {
        if (g_WatchMenuSelected == 0)
        {
            g_WatchMenuPage = WATCH_INDEX_GAME_OPTIONS;
            watchMenuResetOptionsRow();
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM3, 15.0f);
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, R_CBUTTONS|R_TRIG|R_JPAD)) || (watchMenuStickPushedRight()))
    {
        if (g_WatchMenuSelected == 0)
        {
            g_WatchMenuPage = WATCH_INDEX_MISSION_STATUS;
            watchMenuResetAbort();
            watchMenuPageBeep();
            watchStartZoom(WATCHZOOM2, 15.0f);
        }
    }
}


void watchMenuControlStyleTick(void)
{
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD)) || (watchMenuStickPushedUp()))
    {
        if (g_WatchMenuSelected == 0)
        {
            g_WatchMenuControlRow = CONTROLLER_OPTIONS_INDEX_INPUTS;
            watchMenuStickYUsed();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD)) || (watchMenuStickPushedDown()))
    {
        if (g_WatchMenuSelected == 0)
        {
            g_WatchMenuControlRow = CONTROLLER_OPTIONS_INDEX_INPUTS;
            watchMenuStickYUsed();
        }
    }
}


void watchMenuControlInputsTick(void)
{
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD)) || (watchMenuStickPushedUp()))
    {
        if (g_WatchMenuSelected == 0)
        {
            g_WatchMenuControlRow = CONTROLLER_OPTIONS_INDEX_STYLE;
            watchMenuStickYUsed();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD)) || (watchMenuStickPushedDown()))
    {
        if (g_WatchMenuSelected == 0)
        {
            g_WatchMenuControlRow = CONTROLLER_OPTIONS_INDEX_STYLE;
            watchMenuStickYUsed();
        }
    }
}


/**
 * Split screen and draw distance only matter in co-op, so the watch hides
 * them in solo. i is a PLAYER_OPTION index (g_WatchMenuOptionsRow - 2).
 */
static bool watchMenuOptionVisible(s32 i)
{
    if (!g_CoopMode && (i == PLAYER_OPTION_SPLIT || i == PLAYER_OPTION_DRAWDIST))
    {
        return FALSE;
    }

    return TRUE;
}

static s32 watchMenuOptionLast(void)
{
    s32 index = GAME_OPTIONS_INDEX_COUNT - 1;

    while (!watchMenuOptionVisible(index - 2))
    {
        index--;
    }

    return index;
}

void watchMenuOptionsMoveCursor(void)
{
    s32 aux;
    s32 step = 0;

    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD)) || (watchMenuStickPushedUp()))
    {
        step = -1;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }
    else if ((joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD)) || (watchMenuStickPushedDown()))
    {
        step = 1;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }

    if (step != 0)
    {
        do
        {
            g_WatchMenuOptionsRow = g_WatchMenuOptionsRow + step;
        } while (g_WatchMenuOptionsRow >= GAME_OPTIONS_INDEX_LOOK_UPDOWN
            && g_WatchMenuOptionsRow < GAME_OPTIONS_INDEX_COUNT
            && !watchMenuOptionVisible(g_WatchMenuOptionsRow - 2));
    }

    aux = g_WatchMenuOptionsRow;

    if (aux >= GAME_OPTIONS_INDEX_COUNT)
    {
        g_WatchMenuOptionsRow = GAME_OPTIONS_INDEX_MUSIC;
        return;
    }

    if (aux < 0)
    {
        g_WatchMenuOptionsRow = watchMenuOptionLast();
    }
}


void watchMenuMusicVolumeTick(void)
{
    if (joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD) || watchMenuStickPushedUp())
    {
        g_WatchMenuOptionsRow = watchMenuOptionLast();
        watchMenuStickYUsed();
        watchMenuClearSelect();
        return;
    }

    if (joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD) || watchMenuStickPushedDown())
    {
        g_WatchMenuOptionsRow = GAME_OPTIONS_INDEX_FX;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }
}


void watchMenuFxVolumeTick(void)
{
    if (joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD) || watchMenuStickPushedUp())
    {
        g_WatchMenuOptionsRow = GAME_OPTIONS_INDEX_MUSIC;
        watchMenuStickYUsed();
        watchMenuClearSelect();
        return;
    }

    if (joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD) || watchMenuStickPushedDown())
    {
        g_WatchMenuOptionsRow = GAME_OPTIONS_INDEX_LOOK_UPDOWN;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }
}


void watchMenuInventoryListTick(void)
{
    s32 count;
    s32 item_line_height;
    s32 selected_item_line_height;

    count = invCountItems();

    if (!get_debug_gunwatchpos_flag())
    {
        if (joyGetButtonsPressedThisFrame(WATCH_PAD, U_JPAD | U_CBUTTONS) || joyGetStickY(WATCH_PAD) >= 0x47)
        {
            if (((s32) g_WatchMenuInvCursor > 0) && !g_WatchMenuSelected)
            {
                g_WatchMenuInvCursor -= 1.0f;
            }
        }
        else
        {
            if (joyGetButtonsPressedThisFrame(WATCH_PAD, D_JPAD | D_CBUTTONS) || joyGetStickY(WATCH_PAD) < -0x46)
            {
                goto down_body;
            }

            goto after_updown;

down_body:
            if (((s32) g_WatchMenuInvCursor < count - 1) && !g_WatchMenuSelected)
            {
                g_WatchMenuInvCursor += 1.0f;
            }

after_updown:
            ;
        }

        if (joyGetButtons(WATCH_PAD, U_JPAD | U_CBUTTONS))
        {
            if (((s32) g_WatchMenuInvCursor > 0) && !g_WatchMenuSelected)
            {
                g_WatchMenuInvCursor -= 0.1f;
            }
        }
        else if (joyGetButtons(WATCH_PAD, D_JPAD | D_CBUTTONS))
        {
            if (((s32) g_WatchMenuInvCursor < count - 1) && !g_WatchMenuSelected)
            {
                g_WatchMenuInvCursor += 0.1f;
            }
        }
    }

    if (joyGetStickY(WATCH_PAD) >= 0x1f
        && joyGetStickY(WATCH_PAD) < 0x46
        && g_WatchMenuItem > 0
        && !g_WatchMenuSelected)
    {
        g_WatchMenuInvCursor -= (f32) joyGetStickY(WATCH_PAD) / 300.0f;
    }
    else if (joyGetStickY(WATCH_PAD) < -0x1e
        && joyGetStickY(WATCH_PAD) >= -0x45
        && (s32) g_WatchMenuInvCursor < count - 1
        && !g_WatchMenuSelected)
    {
        g_WatchMenuInvCursor -= (f32) joyGetStickY(WATCH_PAD) / 300.0f;
    }

    if (watchMenuStickTappedUp() && g_WatchMenuItem > 0 && !g_WatchMenuSelected)
    {
        g_WatchMenuInvCursor -= 1.0f;
    }
    else if (watchMenuStickTappedDown() && g_WatchMenuItem < count - 1 && !g_WatchMenuSelected)
    {
        g_WatchMenuInvCursor += 1.0f;
    }

    if (watchMenuStickNudgedUp() || watchMenuStickNudgedDown())
    {
        g_WatchMenuStickYActive = 1;
    }
    else
    {
        g_WatchMenuStickYActive = 0;
    }

    if ((f32) count - 0.5f < g_WatchMenuInvCursor)
    {
        g_WatchMenuInvCursor = (f32) count - 0.5f;
    }

    if (g_WatchMenuInvCursor < -0.5f)
    {
        g_WatchMenuInvCursor = -0.5f;
    }

    // The current item is determined by the integer part of the cursor's position.
    g_WatchMenuItem = (s32) g_WatchMenuInvCursor;

    if (j_text_trigger)
    {
        item_line_height = 14;
    }
    else
    {
        item_line_height = 12;
    }

    selected_item_line_height = j_text_trigger ? 14 : 12;
    g_WatchMenuInvTextTargetY = (2 * selected_item_line_height) + (-g_WatchMenuItem * item_line_height);

    if (g_WatchMenuInvTextTargetY < g_WatchMenuInvTextY)
    {
        g_WatchMenuInvTextY = (g_WatchMenuInvTextY - ((g_WatchMenuInvTextY - g_WatchMenuInvTextTargetY) / 3)) - 1;
        g_WatchMenuInvSettled = FALSE;
    }
    else if (g_WatchMenuInvTextY < g_WatchMenuInvTextTargetY)
    {
        g_WatchMenuInvTextY = (g_WatchMenuInvTextY + ((g_WatchMenuInvTextTargetY - g_WatchMenuInvTextY) / 3)) + ((0, 1));
        g_WatchMenuInvSettled = FALSE;
    }
    else
    {
        g_WatchMenuInvSettled = TRUE;
    }

    if (((f32) g_WatchMenuItem + 0.55f < g_WatchMenuInvCursor)
        && !joyGetButtons(WATCH_PAD, 0xffff)) // Any button
    {
        g_WatchMenuInvCursor -= 0.1f;
    }
    else if (g_WatchMenuInvCursor <= (f32) g_WatchMenuItem + 0.45f
        && !joyGetButtons(WATCH_PAD, 0xffff)) // Any button
    {
        g_WatchMenuInvCursor += 0.1f;
    }
}


void watchMenuScrollList(f32 *arg0, s32 *arg1, s32 arg2, s32 *arg3, s32 *arg4, s32 *arg5, s32 arg6, s32 arg7, s32 arg8)
{
    if (!get_debug_gunwatchpos_flag())
    {
        if (joyGetButtonsPressedThisFrame(WATCH_PAD, U_JPAD | U_CBUTTONS) || joyGetStickY(WATCH_PAD) >= 0x47)
        {
            if ((s32)*arg0 > 0 && arg7)
            {
                *arg0 -= 1.0f;
            }
        }
        else if (joyGetButtonsPressedThisFrame(WATCH_PAD, D_JPAD | D_CBUTTONS) || joyGetStickY(WATCH_PAD) < -0x46)
        {
            if ((s32)*arg0 < arg2 - 1 && arg7)
            {
                *arg0 += 1.0f;
            }
        }

        if (joyGetButtons(WATCH_PAD, U_JPAD | U_CBUTTONS))
        {
            if ((s32)*arg0 > 0 && arg7)
            {
                *arg0 -= 0.1f;
            }
        }
        else if (joyGetButtons(WATCH_PAD, D_JPAD | D_CBUTTONS))
        {
            if ((s32)*arg0 < arg2 - 1 && arg7)
            {
                *arg0 += 0.1f;
            }
        }
    }

    if (joyGetStickY(WATCH_PAD) >= 0x1f && joyGetStickY(WATCH_PAD) < 0x46 && *arg1 > 0 && arg7)
    {
        *arg0 -= (f32)joyGetStickY(WATCH_PAD) / 300.0f;
    }
    else if (joyGetStickY(WATCH_PAD) < -0x1e && joyGetStickY(WATCH_PAD) >= -0x45 && (s32)*arg0 < arg2 - 1 && arg7)
    {
        *arg0 -= (f32)joyGetStickY(WATCH_PAD) / 300.0f;
    }

    if (watchMenuStickTappedUp() && *arg1 > 0 && arg7)
    {
        *arg0 -= 1.0f;
    }
    else if (watchMenuStickTappedDown() && *arg1 < arg2 - 1 && arg7)
    {
        *arg0 += 1.0f;
    }

    if (watchMenuStickNudgedUp() || watchMenuStickNudgedDown())
    {
        g_WatchMenuStickYActive = 1;
    }
    else
    {
        g_WatchMenuStickYActive = 0;
    }

    if ((f32)arg2 - 0.5f < *arg0)
    {
        *arg0 = (f32)arg2 - 0.5f;
    }

    if (*arg0 < -0.5f)
    {
        *arg0 = -0.5f;
    }

    *arg1 = (s32)*arg0;
    *arg4 = (arg6 * arg8) + (-*arg1 * arg8);

    if (*arg4 < *arg3)
    {
        *arg3 = (*arg3 - ((*arg3 - *arg4) / 3)) - 1;
        *arg5 = 0;
    }
    else if (*arg3 < *arg4)
    {
        *arg3 = (((*arg4 - *arg3) / 3) + *arg3) + 1;
        *arg5 = 0;
    }
    else
    {
        *arg5 = 1;
    }

    if ((f32)*arg1 + 0.55f < *arg0)
    {
        if (!joyGetButtons(WATCH_PAD, ANY_BUTTON))
        {
            *arg0 -= 0.1f;
            return;
        }
    }

    if (*arg0 <= (f32)*arg1 + 0.45f)
    {
        if (!joyGetButtons(WATCH_PAD, ANY_BUTTON))
        {
            *arg0 += 0.1f;
        }
    }
}


void watchMenuBriefBackgroundTick(void)
{
    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD)) || (watchMenuStickPushedUp()))
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_OBJECTIVES;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }

    if ((joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD)) || (watchMenuStickPushedDown()))
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_M;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }
}


void watchMenuBriefMTick(void)
{
    if (joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD) || watchMenuStickPushedUp())
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_BACKGROUND;
        watchMenuStickYUsed();
        watchMenuClearSelect();
        return;
    }

    if (joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD) || watchMenuStickPushedDown())
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_Q;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }
}


void watchMenuBriefQTick(void)
{
    if (joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD) || watchMenuStickPushedUp())
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_M;
        watchMenuStickYUsed();
        watchMenuClearSelect();
        return;
    }

    if (joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD) || watchMenuStickPushedDown())
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_MONEYPENNY;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }
}

void watchMenuBriefMoneypennyTick(void)
{
    if (joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD) || watchMenuStickPushedUp())
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_Q;
        watchMenuStickYUsed();
        watchMenuClearSelect();
        return;
    }

    if (joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD) || watchMenuStickPushedDown())
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_OBJECTIVES;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }
}

void watchMenuBriefObjectivesTick(void)
{
    if (joyGetButtonsPressedThisFrame(WATCH_PAD, U_CBUTTONS|U_JPAD) || watchMenuStickPushedUp())
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_MONEYPENNY;
        watchMenuStickYUsed();
        watchMenuClearSelect();
        return;
    }

    if (joyGetButtonsPressedThisFrame(WATCH_PAD, D_CBUTTONS|D_JPAD) || watchMenuStickPushedDown())
    {
        g_WatchMenuBriefPage = BRIEF_INDEX_BACKGROUND;
        watchMenuStickYUsed();
        watchMenuClearSelect();
    }
}


void watchMenuMakeScanlineVerts(Vtx *vertices)
{
    s32 halfWidth; // Half width for the scanline
    Vtx *vertex;
    s32 zoffs;
    s32 greenChannelIndex;
    s32 side;

    // Fit the width of the scanline to the watch's green circle
    halfWidth = sqrtf(213444.0f - ((f32) (g_WatchStaticScanlineY * g_WatchStaticScanlineY)));

    for (zoffs = 0; zoffs != 8; zoffs += 4)
    {
        for (side = -1; side != 3; side += 2)
        {
            vertex = vertices;

            vertex->v.ob[0] = halfWidth * side;
            vertex->v.ob[1] = 0;
            vertex->v.ob[2] = zoffs + g_WatchStaticScanlineY;

            vertex->v.cn[1] = 0xA0;
            vertex->v.flag = 0;
            vertex->v.tc[0] = 0;
            vertex->v.tc[1] = 0;
            vertex->v.cn[0] = 0;

            greenChannelIndex = 1;
            vertex->v.cn[greenChannelIndex] = 0xA0;
            vertex->v.cn[2] = 0;
            vertex->v.cn[3] = g_WatchStaticScanlineAlpha;

            vertices++;
        }
    }
}


void watchMenuOpen(void)
{
    if (joyGetControllerCount() < 2)
    {
        g_WatchMenuControlStyleCount = 4;
    }
    else
    {
        g_WatchMenuControlStyleCount = 8;
    }
    watchMenuClearSelect();
    g_WatchMenuPage = WATCH_INDEX_MISSION_STATUS;
    g_WatchMenuBriefPage = BRIEF_INDEX_OBJECTIVES;
    D_800409C8 = 0.999f;
    D_800409CC = 0.9999f;
    invDetermineEquippedItem();
}


/**
 * Address 0x7F0A6A2C. (VERSION_US, VERSION_JP)
 * Address 0x7F0A5D78. (VERSION_EU)
*/
f32 watchMenuWrapAngle(f32 arg0)
{
    if (arg0 > M_PI_F)
    {
        arg0 = arg0 - M_TAU_F;
    }
    else if (arg0 < M_MINUS_PI_F)
    {
        arg0 = arg0 + M_TAU_F;
    }
    return arg0;
}


extern f32 g_FrameDelta60;
void watchMenuTick(void)
{
    u32 temp_1;
    s32 temp_2;
    s32 temp_3;
    u32 random_value;

    if (joyGetButtonsPressedThisFrame(WATCH_PAD, START_BUTTON))
    {
        watchRequestToggle();
    }

    if (g_WatchMenuStickXReady == 0)
    {
        if ((joyGetStickX(WATCH_PAD) >= -0xA) && (joyGetStickX(WATCH_PAD) < 0xB))
        {
            g_WatchMenuStickXReady = 1;
        }
        else if ((joyGetStickX(WATCH_PAD) < 0xB) && (joy7000C174(WATCH_PAD) >= 0xB))
        {
            g_WatchMenuStickXReady = 1;
        }
        else if ((joyGetStickX(WATCH_PAD) >= -0xA) && (joy7000C174(WATCH_PAD) < -0xA))
        {
            g_WatchMenuStickXReady = 1;
        }
    }

    if (g_WatchMenuStickYReadyFlag == 0)
    {
        if ((joyGetStickY(WATCH_PAD) >= -0xA) && (joyGetStickY(WATCH_PAD) < 0xB))
        {
            g_WatchMenuStickYReadyFlag = 1;
        }
        else if ((joyGetStickY(WATCH_PAD) < 0xB) && (joy7000C284(WATCH_PAD) >= 0xB))
        {
            g_WatchMenuStickYReadyFlag = 1;
        }
        else if ((joyGetStickY(WATCH_PAD) >= -0xA) && (joy7000C284(WATCH_PAD) < -0xA))
        {
            g_WatchMenuStickYReadyFlag = 1;
        }
    }

    temp_2 = D_80040AF8;
    if (temp_2 < 0)
    {
        D_80040AF4 = D_80040AF4 + 0xFFF00000;
    }
    D_80040AF8 = temp_2 - 1;

    if (D_80040AF4 < 0x5F00A1U)
    {
        D_80040AF4 = 0xFF00A0U;
        D_80040AF8 = 0xF;
    }

    temp_3 = D_80040B00;
    if (temp_3 < 0)
    {
        D_80040AFC = D_80040AFC - 0x10;
    }
    D_80040B00 = temp_3 - 1;

    if (D_80040AFC < 0x60U)
    {
        D_80040AFC = 0xFFU;
        D_80040B00 = 0xF;
    }
    #ifdef VERSION_US
    D_80040B14 += ((D_80040B1C * g_FrameDelta * M_TAU_F) / 360.0f);
    #else
    D_80040B14 += ((D_80040B1C * g_FrameDelta60 * M_TAU_F) / 360.0f);
    #endif

    D_80040B14 = watchMenuWrapAngle(D_80040B14);

    temp_1 = D_80040B0C << 0x10;
    if (temp_1 < randomGetNext())
    {
        watchMenuStatic();
    }

    if (g_WatchBackgroundGreen < 0xE0)
    {
        random_value = randomGetNext();
        g_WatchBackgroundGreen += (random_value >> 0x1E);
    }

    if (g_WatchBackgroundGreen > 0xe0) {
        g_WatchBackgroundGreen = 0xe0;
    }

    g_WatchStaticScanlineAlpha = ((-g_WatchBackgroundGreen * 4) + 0x380);
    g_WatchStaticScanlineY = g_WatchStaticScanlineY - 4;

    if (g_WatchStaticScanlineY >= 0x157) {
        g_WatchStaticScanlineY = -0x156;
    }

    if (g_WatchStaticScanlineY < -0x156) {
        g_WatchStaticScanlineY = 0x156;
    }

    D_80040B44 = (s16)D_80040B44 + 1;
    D_80040B44 = (s16)D_80040B44 & 1;

    switch (g_WatchMenuPage)
    {
        case WATCH_INDEX_MISSION_STATUS:
            watchMenuStatusTick();
            break;

        case WATCH_INDEX_CONTROL_OPTIONS:
            switch (g_WatchMenuControlRow)
            {
                case CONTROLLER_OPTIONS_INDEX_STYLE:
                    watchMenuControlStyleTick();
                    break;

                case CONTROLLER_OPTIONS_INDEX_INPUTS:
                    watchMenuControlInputsTick();
            }
            watchMenuControlTick();
            break;

        case WATCH_INDEX_GAME_OPTIONS:
            switch (g_WatchMenuOptionsRow)
            {
                case GAME_OPTIONS_INDEX_MUSIC:
                    watchMenuMusicVolumeTick();
                    break;

                case GAME_OPTIONS_INDEX_FX:
                    watchMenuFxVolumeTick();
                    break;

                case GAME_OPTIONS_INDEX_LOOK_UPDOWN:
                case GAME_OPTIONS_INDEX_AUTO_AIM:
                case GAME_OPTIONS_INDEX_AIM_CONTROL:
                case GAME_OPTIONS_INDEX_SIGHT_ONSCREEN:
                case GAME_OPTIONS_INDEX_LOOK_AHEAD:
                case GAME_OPTIONS_INDEX_AMMO_ONSCREEN:
                case GAME_OPTIONS_INDEX_SCREEN_SIZE:
                case GAME_OPTIONS_INDEX_RATIO:
                case GAME_OPTIONS_INDEX_HIRES:
                case GAME_OPTIONS_INDEX_SPLIT:
                case GAME_OPTIONS_INDEX_FPS:
                case GAME_OPTIONS_INDEX_DRAWDIST:
                    watchMenuOptionsMoveCursor();
            }
            watchMenuOptionsTick();
            break;

        case WATCH_INDEX_MISSION_BRIEFING:
            watchMenuBriefingTick();
            break;

        case WATCH_INDEX_INVENTORY:
            watchMenuInventoryTick();
    }
}



Gfx *watchMenuSetupDraw(Gfx *DL)
{
    gSPSetGeometryMode(DL++, G_CULL_BACK);
    gDPSetCycleType(DL++, G_CYC_1CYCLE);
    gDPPipelineMode(DL++, G_PM_1PRIMITIVE);
    gDPSetScissor(DL++, G_SC_NON_INTERLACE, 0, 0, VI_SX(viGetX()), viGetY());
    gDPSetTextureLOD(DL++, G_TL_TILE);
    gDPSetTextureLUT(DL++, G_TT_NONE);
    gDPSetTextureDetail(DL++, G_TD_CLAMP);
    gDPSetTexturePersp(DL++, G_TP_PERSP);
    gDPSetTextureFilter(DL++, G_TF_BILERP);
    gDPSetTextureConvert(DL++, G_TC_FILT);
    gDPSetCombineLERP(DL++, 0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE);
    gDPSetCombineKey(DL++, G_CK_NONE);
    gDPSetAlphaCompare(DL++, G_AC_NONE);
    gDPSetRenderMode(DL++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetColorDither(DL++, G_CD_MAGICSQ);

    return DL;
}


void watchMenuSetTabColours(s32 g_WatchMenuPage, struct WatchVertex *vertices)
{
    s32 i;

    // Unselected rectangles.
    for (i = 0; i < 20; i++)
    {
        vertices[i].color.r = 0x20;
        vertices[i].color.g = 0x70;
        vertices[i].color.b = 0x20;
    }

    // Currently selected page rectangle.
    for (i = g_WatchMenuPage * 4; i <= g_WatchMenuPage * 4 + 3; i++)
    {
        vertices[i].color.r = 0x50;
        vertices[i].color.g = 0xF0;
        vertices[i].color.b = 0x50;

        // Currently selected page rectangle, but something else is in focus e.g. toggling options or manipulating the controller on the controller screen.
        if (g_WatchMenuSelected)
        {
            vertices[i].color.r = 0x30;
            vertices[i].color.g = 0xA0;
            vertices[i].color.b = 0x30;
        }
    }
}


/**
 * @param gdl:
 * @param arg1: Something about watch view matrix.
 * @param zoom_squish: When set, will "unfold" the interior watch watch area (green + bars)
 * based on view distance during pause animation. This is used when starting to pause and bring
 * the watch up, and exiting pause menu to resume game play. If this is disabled then
 * the interior area will always be the same size as the watch container.
*/
Gfx *watchMenuRenderBackground(Gfx *gdl, Mtx *arg1, s32 zoom_squish)
{
    int i;
    struct WatchVertex *sp48;
    struct WatchVertex *sp44;
    Gfx *sp40;
    Gfx *sp3C;

    s32 stack_pad[6];

    f32 scale;

    sp48 = dynAllocateVertices(WATCH_BACKGROUND_VERTEX_COUNT);
    sp44 = dynAllocateVertices(WATCH_BACKGROUND_VERTEX_COUNT);
    sp40 = dynAllocate(0xF8);
    sp3C = dynAllocate(0xF8);

    /**
     * It seems at this point in rendering the background watch arms (hour,minute,second)
     * have already been drawn. Whatever is about to be drawn next is oriented
     * to the second hand of the watch.
     * The following commands reset the orientation for the background green
     * area and health bars. Or, commenting this out will draw the background
     * and slowly spin it around in sync with the second hand.
    */
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gDPSetPrimColor(gdl++, 0, 0, 0xE6, 0xE6, 0xE6, 0x00);
    gSPMatrix(gdl++, arg1, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    // end initial setup.

    scale = 1;

    if (watchIsZooming())
    {
        scale = (g_CurrentPlayer->zoomintime * (g_CurrentPlayer->zoominfovynew - g_CurrentPlayer->zoominfovyold))
            / g_CurrentPlayer->zoomintimemax;

        if (scale < 0.0f)
        {
            scale = -scale;
        }

        if (scale > 1)
        {
            scale = 1;
        }

        scale = scale * scale;
    }

    if (zoom_squish == 1)
    {
        scale = 0.05f;
        g_WatchBackgroundGreen = 0xE0;

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x4 || g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x6)
        {
            scale = watchGetOpenFrac();
        }
    }

    guScale(&gfx_background_8007B0A0, 0.25f, 0.25f, 0.25f);

    gSPMatrix(gdl++, &gfx_background_8007B0A0, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);

    if (zoom_squish == 0)
    {
        gSPClearGeometryMode(gdl++, G_CULL_BOTH);
        // draw body armor bars
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_body_armor_bar_gdl));
        // draw health bars
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_health_bar_gdl));
    }

    /**
     * This section renders main background, side health bar & body armor bars while zooming in or out.
    */
    guScale(&gfx_background_8007B0E0, 1, 1, scale);

    gSPMatrix(gdl++, &gfx_background_8007B0E0, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);

    if (zoom_squish == 1)
    {
        gSPClearGeometryMode(gdl++, G_CULL_BOTH);
        // draw body armor bars
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_body_armor_bar_gdl));
        // draw health bars
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_health_bar_gdl));
    }
    /**
     * End health bar zoom section
    */

    watchMakeArcVerts(sp44, WATCH_BACKGROUND_VERTEX_COUNT, 0.92f, 0);
    watchRenderBackground(sp3C, OS_PHYSICAL_TO_K0(sp44), WATCH_BACKGROUND_VERTEX_COUNT, 0);

    gDPPipeSync(gdl++);
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, 0x00, 0xFF, 0x00, 0x00);
    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(sp3C));
    gDPPipeSync(gdl++);

    /**
     * This section renders the green background area of the watch menu.
    */
    if (g_WatchBackgroundGreen < 0xE0)
    {
        watchMakeArcVerts(sp48, WATCH_BACKGROUND_VERTEX_COUNT, 0.899999976158f, 0);
        watchRenderBackground(sp40, OS_PHYSICAL_TO_K0(sp48), WATCH_BACKGROUND_VERTEX_COUNT, 0);

        gDPSetRenderMode(gdl++, G_RM_AA_PCL_SURF, G_RM_AA_PCL_SURF2);
    }
    else
    {
        watchMakeArcVerts(sp48, WATCH_BACKGROUND_VERTEX_COUNT, 0.899999976158f, 1);
        watchRenderBackground(sp40, OS_PHYSICAL_TO_K0(sp48), WATCH_BACKGROUND_VERTEX_COUNT, 1);

        gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    }

    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(sp40));
    /**
     * // end green background area.
    */

    /**
     * This section renders the green rectangles/page select at the bottom of the screen.
     * This is setup in bondview watchTrigger.
    */
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(g_CurrentPlayer->buffer_for_watch_greenbackdrop_DL));
    /**
     * // end green rectangles/page select section
    */

    for (i=0; i<WATCH_BACKGROUND_VERTEX_COUNT; i++)
    {
        sp48[i].color.a = (s8)g_WatchBackgroundGreen;
        sp44[i].color.a = (s8)g_WatchBackgroundGreen;
    }

    if (g_WatchBackgroundGreen < 0xE0)
    {
        // Create the thin green scanline that moves up the screen while the watch does static.
        watchMenuMakeScanlineVerts(g_CurrentPlayer->buffer_for_watch_static_vertices);

        gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(g_CurrentPlayer->buffer_for_watch_static_DL));
    }

    return gdl;
}


Gfx *watchMenuRenderBackgroundZoom(Gfx *gdl, Mtx *param_2)
{
    return watchMenuRenderBackground(gdl, param_2, 1);
}


Gfx *watchMenuRenderAbort(Gfx *gdl)
{
    s32 sp7C;
    s32 sp78;
    s32 sp74;
    s32 sp70;
    s32 sp6C;
    s32 sp68;
    s32 sp64;
    s32 sp60;

    s32 pFontFile;
    s32 pFontChars;
    s32 sp54;
    s32 sp50;
    s32 sp4C;

    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;
    sp54 = langGet(getStringID(LOPTIONS, OPTION_STR_24_ABORT_LF)); //abort:
    sp50 = langGet(getStringID(LOPTIONS, OPTION_STR_25_CONFIRM_LF)); //confirm
    sp4C = langGet(getStringID(LOPTIONS, OPTION_STR_26_CANCEL_LF)); //cancel
    sp7C = 0x51;

    sp78 = (j_text_trigger ? 0xF : 0) + 0xBD;

    sp74 = (j_text_trigger ? 0xA : 0) + 0x88;

    sp70 = sp6C = sp68 = (j_text_trigger ? 3 : 0) + (PAL ? 0x4E : 0x4C);

    if (g_WatchMenuSelected != 0)
    {
        if (g_WatchMenuAbortAsk == 0)
        {
            if ((joyGetStickX(WATCH_PAD) >= 0x2E) || (joyGetButtons(WATCH_PAD, 0x111) != 0))
            {
                g_WatchMenuAbortAsk = 1;
            }
        }
        else
        {
            if (g_WatchMenuAbortAsk != 0)
            {
                if ((joyGetStickX(WATCH_PAD) < -0x2D) || (joyGetButtons(WATCH_PAD, 0x222) != 0))
                {
                    g_WatchMenuAbortAsk = 0;
                }
            }
        }
    }

    if (g_WatchMenuSelected != 0)
    {
        textMeasure(&sp60, &sp64, sp54, pFontChars, pFontFile, 0);

        gdl = textRender(gdl, &sp7C, &sp70, sp54, pFontChars, pFontFile, 0xA0FFA0F0, sp64, sp60, 0, 0);

        if (g_WatchMenuAbortAsk != 0)
        {
            gdl = textRenderOutlined(gdl, &sp78, &sp6C, sp50, pFontChars, pFontFile, -1, 0x7000A0, viGetX(), viGetY(), 0, 0);
            gdl = textRender(gdl, &sp74, &sp68, sp4C, pFontChars, pFontFile, 0xFF00B0, viGetX(), viGetY(), 0, 0);
        }
        else
        {
            if (g_WatchMenuAbortAsk == 0)
            {
                gdl = textRender(gdl, &sp78, &sp6C, sp50, pFontChars, pFontFile, 0xFF00B0, viGetX(), viGetY(), 0, 0);
                gdl = textRenderOutlined(gdl, &sp74, &sp68, sp4C, pFontChars, pFontFile, -1, 0x7000A0, viGetX(), viGetY(), 0, 0);
            }
        }
    }
    else
    {
        textMeasure(&sp60, &sp64, sp54, pFontChars, pFontFile, 0);
        gdl = textRender(gdl, &sp7C, &sp70, sp54, pFontChars, pFontFile, 0x800080, sp64, sp60, 0, 0);
        gdl = textRender(gdl, &sp78, &sp6C, sp50, pFontChars, pFontFile, 0x800080, viGetX(), viGetY(), 0, 0);
        gdl = textRender(gdl, &sp74, &sp68, sp4C, pFontChars, pFontFile, 0x800080, viGetX(), viGetY(), 0, 0);
    }

    return gdl;
}


Gfx *watchMenuRenderStatusTitle(Gfx *gdl)
{
    s32 txtptr_1;
    s32 txtptr_2;
    s32 sp64;
    s32 sp60;
    s32 sp5C;
    s32 sp58;
    s32 pFontFile;
    s32 pFontChars;
    s32 sp4C;
    s32 joffset;

    txtptr_1 = langGet(getStringID(LOPTIONS, OPTION_STR_27_MISSIONSTATUS_LF)); //mission status:
    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;

    if (objectivesAllComplete())
    {
        sp4C = 0xFF00B0;
        txtptr_2 = langGet(getStringID(LOPTIONS, OPTION_STR_28_COMPLETE_LF)); //complete
    }
    else
    {
        sp4C = D_80040AF4;
        txtptr_2 = langGet(getStringID(LOPTIONS, OPTION_STR_29_INCOMPLETE_LF)); //incomplete
    }

    gdl = microcode_constructor(gdl);
    textMeasure(&sp5C, &sp58, txtptr_1, pFontChars, pFontFile, 0);
    sp64 = 0x51;
    sp60 = YOFFSET_MISSIONSTATUS;
    gdl = textRender(gdl, &sp64, &sp60, txtptr_1, pFontChars, pFontFile, 0xFF00B0, sp58, sp5C, 0, 0);

    if (j_text_trigger)
    {
        joffset = 0x22;
    }
    else
    {
        joffset = 0;
    }

    sp64 = sp64 + sp58 + joffset + 4;
    sp60 = sp60 - sp5C;
    textMeasure(&sp5C, &sp58, txtptr_2, pFontChars, pFontFile, 0);
    gdl = textRender(gdl, &sp64, &sp60, txtptr_2, pFontChars, pFontFile, sp4C, sp58, sp5C, 0, 0);
    gdl = watchMenuRenderAbort(gdl);

    return gdl;
}


Gfx *watchMenuRenderNothing(Gfx *gdl) {
  return gdl;
}


Gfx *watchMenuRenderVersion(Gfx *gdl)
{
    s32 txtptr;
    s32 sp50;
    s32 sp4C;
    s32 sp48;
    s32 sp44;
    s32 pFontFile;
    s32 pFontChars;
    s32 joffset;

    txtptr = langGet(getStringID(LOPTIONS, OPTION_STR_2B_QWATCHVERSION_LF)); //q watch v2.01 beta

    if (j_text_trigger)
    {
        joffset = -5;
    }
    else
    {
        joffset = 0;
    }
    sp50 = joffset + 0x65;
    sp4C = YOFFSET_7;
    sp48 = 0;
    sp44 = 0;
    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;
    gdl = microcode_constructor(gdl);
    textMeasure(&sp48, &sp44, txtptr, pFontChars, pFontFile, 0);
    gdl = textRender(gdl, &sp50, &sp4C, txtptr, pFontChars, pFontFile, 0xFF00B0, sp44, sp48, 0, 0);
    return gdl;
}




#ifndef _BONDWALK_H_
#define _BONDWALK_H_
typedef struct GunModelFileRecord {
    void *item_header;
    char *item_file_name;
    s32   has_no_model;
    void *item_weapon_stats;
    u16   upper_watch_text;
    u16   lower_watch_text;
    f32   watch_pos_x, watch_pos_y, watch_pos_z;
    f32   x_rotation,  y_rotation;
    u16   weapon_of_choice_text, watch_equipment_text;
    f32   equip_watch_x, equip_watch_y, equip_watch_z;
} GunModelFileRecord;
#endif
extern GunModelFileRecord gitem_structs[];
/* forward declarations so the compiler uses f32 calling convention ($f0) */
extern f32 invGetWatchPosXForIndex(s32 index);
extern f32 invGetWatchPosYForIndex(s32 index);
extern f32 invGetWatchPosZForIndex(s32 index);
extern f32 invGetWatchFovForIndex(s32 index);
extern f32 invGetWatchRotXForIndex(s32 index);
extern f32 invGetWatchRotYForIndex(s32 index);
Gfx* watchMenuRenderHeldItem(Gfx* gdl) {
    Mtx* sp114;
    u16 perspNorm;
    Mtxf matrix2;
    Mtxf matrix;
    s32 sp8C;
    s32 sp88;
    s32 sp84;
    s32 sp80;
    s32 sp7C;
    s32 sp78;
    s32 temp_v0;
    s32 sp70;
    f32 sp6C;
    f32 sp68;
    f32 sp64;
    f32 sp60;
    f32 rotx;
    f32 roty;
    s8* text;
    s8* text2;
    struct GunModelFileRecord *gitem;

    sp114 = dynAllocateMatrix();
    sp84 = 0;
    sp80 = 0;

    sp7C = ptrFontBankGothic;
    sp78 = ptrFontBankGothicChars;

    temp_v0 = invGetCurEquippedItem();
    sp70 = invGetWeaponForIndex(temp_v0);
    sp6C = invGetWatchPosXForIndex(temp_v0);
    sp68 = invGetWatchPosYForIndex(temp_v0);
    sp64 = invGetWatchPosZForIndex(temp_v0);
    sp60 = invGetWatchFovForIndex(temp_v0);
    rotx = invGetWatchRotXForIndex(temp_v0);
    roty = invGetWatchRotYForIndex(temp_v0);
    text = invGetWatchTitle1ForIndex(temp_v0);
    text2 = invGetWatchTitle2ForIndex(temp_v0);

    if (get_debug_gunwatchpos_flag() != 0) {
        gitem = &gitem_structs[gunGetWeapon(0)];

        if (joyGetButtons(0, 2) != 0) {
           gitem->watch_pos_y -= 2.0f;
        }

        if (joyGetButtons(0, 1) != 0) {
            gitem->watch_pos_y += 2.0f;
        }

        if (joyGetButtons(0, 4) != 0) {
           gitem->watch_pos_x += 2.0f;
        }

        if (joyGetButtons(0, 8) != 0) {
            gitem->watch_pos_x -= 2.0f;
        }

        if (joyGetButtons(0, 0x20) != 0) {
            gitem->watch_pos_z *= 0.98000002f;
        }

        if (joyGetButtons(0, 0x10) != 0) {
             gitem->watch_pos_z *= 1.0204082f;
        }
#ifdef DEBUG

            osSyncPrintf("gun watch pos x=%f[CL,CR] y=%f[CD,CU] z=%f[TL,TR] ", gitem->watch_pos_x, gitem->watch_pos_y, gitem->watch_pos_z);

#endif
    }


#if defined(LEFTOVERDEBUG)
    guPerspective(sp114, &perspNorm, sp60, 1.33333337f, 10.0f, 10000.0f, 1.0f);
#else
    guPerspective(sp114, &perspNorm, sp60, 1.2838470f, 10.0f, 10000.0f, 1.0f);
#endif

    gSPMatrix(gdl++, osVirtualToPhysical(sp114), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    mtx4LoadYRotation((roty * 6.2831855f) / 360.0f, &matrix2);
    mtx4LoadZRotation(6.2831855f - ((rotx * 6.2831855f) / 360.0f), &matrix);
    mtx4MultInPlace(&matrix, &matrix2);
    mtx4LoadLookAt(&matrix, sp64, sp6C, sp68, 0.0f, sp6C, sp68, 0.0f, 1.0f, 0.0f);
    mtx4MultInPlace(&matrix, &matrix2);

    gdl = watchMenuSetupDraw(gdl);

    if (g_WatchBackgroundGreen < 0xE0) {
        gdl = watchRenderItem(gdl, sp70, &matrix2, g_WatchBackgroundGreen + 1, 0x64DC6428);
    } else {
        gdl = watchRenderItem(gdl, sp70, &matrix2, 0xFF, 0x64DC6428);
    }

    gdl = microcode_constructor(gunDrawWatchAmmoDisplay(gdl));

    sp8C = 0x60;
#if defined(LEFTOVERDEBUG)
    sp88 = 0xA0;
#else
    sp88 = 0xBC;
#endif
    textMeasure(&sp84, &sp80, text, sp78, sp7C, 0);
    gdl = textRender(gdl, &sp8C, &sp88, text, sp78, sp7C, 0xFF00B0, sp80, sp84, 0, 0);

#if defined(LEFTOVERDEBUG)
    sp88 = 0xAA;
#else
    sp88 = 0xC6;
#endif
    textMeasure(&sp84, &sp80, text2, sp78, sp7C, 0);
    gdl = textRender(gdl, &sp8C, &sp88, text2, sp78, sp7C, 0xFF00B0, sp80, sp84, 0, 0);

    return gdl;
}


Gfx *watchMenuRenderStatus(Gfx *gdl, Mtx *param_2)
{
    gdl = watchMenuRenderBackground(gdl, param_2, 0);

    if (watchIsZooming() != 1)
    {
        gdl = watchMenuRenderVersion(gdl);
        gdl = watchMenuRenderStatusTitle(gdl);
        gdl = watchMenuRenderHeldItem(watchMenuRenderNothing(gdl));
    }
    else
    {
        watchIsZooming();
    }

    return gdl;
}


void watchMenuEquipSelected(void)
{
    if (joyGetButtonsPressedThisFrame(WATCH_PAD, Z_TRIG|A_BUTTON) == 0) {
        if (joyGetButtonsPressedThisFrame(WATCH_PAD, START_BUTTON) == 0)
        {
            return;
        }

        if (gunGetWeapon(0) == invGetWeaponForIndex(g_WatchMenuItem))
        {
            return;
        }
    }

    gunUnequip(0, invGetWeaponForIndex(g_WatchMenuItem));
    gunUnequip(1, 0);
    invSetCurEquippedItem(g_WatchMenuItem);
    g_WatchMenuEquipDelay = 10;
    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
}


#ifndef _BONDWALK_H_
typedef struct GunModelFileRecord {
    void *item_header;
    char *item_file_name;
    s32   has_no_model;
    void *item_weapon_stats;
    u16   upper_watch_text;
    u16   lower_watch_text;
    f32   watch_pos_x, watch_pos_y, watch_pos_z;
    f32   x_rotation,  y_rotation;
    u16   weapon_of_choice_text, watch_equipment_text;
    f32   equip_watch_x, equip_watch_y, equip_watch_z;
} GunModelFileRecord;
#endif
extern GunModelFileRecord gitem_structs[];
/* forward declarations so the compiler uses f32 calling convention ($f0) */
extern f32 invGetEquipFovForIndex(s32 index);
extern f32 invGetEquipPosXForIndex(s32 index);
extern f32 invGetEquipPosYForIndex(s32 index);
extern f32 invGetEquipPosZForIndex(s32 index);
extern f32 invGetWatchRotXForIndex(s32 index);
extern f32 invGetWatchRotYForIndex(s32 index);
extern u16 *invGetNameByIndex(s32 index);


Gfx *watchMenuRenderInventory(Gfx *gdl, Mtx *param_2)
{
    Mtx *sp924;
    u16 perspNorm;
    Mtxf sp8E0;
    Mtxf sp8A0;
    f32 temp_cos;
    f32 temp_sin;
    f32 sp894;
    f32 sp890;
    f32 sp88C;
    f32 sp888;
    s32 sp884;
    f32 sp880;
    f32 sp87C;
    s32 temp_s0_3;
    GunModelFileRecord *gitem;
    s32 x1;
    s32 y1;

    gdl = watchMenuRenderBackground(gdl, param_2, 0);

    if (watchIsZooming() != 1)
    {
        sp924 = dynAllocateMatrix();
        sp894 = invGetEquipFovForIndex(g_WatchMenuItem);
        sp890 = invGetEquipPosXForIndex(g_WatchMenuItem);
        sp88C = invGetEquipPosYForIndex(g_WatchMenuItem);
        sp888 = invGetEquipPosZForIndex(g_WatchMenuItem);
        sp884 = invGetWeaponForIndex(g_WatchMenuItem);
        sp880 = invGetWatchRotXForIndex(g_WatchMenuItem);
        sp87C = invGetWatchRotYForIndex(g_WatchMenuItem);

        if (get_debug_gunwatchpos_flag() != 0)
        {
            gitem = &gitem_structs[gunGetWeapon(0)];

            if (joyGetButtons(0, L_CBUTTONS) != 0)
            {
                gitem->equip_watch_x -= 2.0f;
            }

            if (joyGetButtons(0, R_CBUTTONS) != 0)
            {
                gitem->equip_watch_x += 2.0f;
            }

            if (joyGetButtons(0, D_CBUTTONS) != 0)
            {
                gitem->equip_watch_y += 2.0f;
            }

            if (joyGetButtons(0, U_CBUTTONS) != 0)
            {
                gitem->equip_watch_y -= 2.0f;
            }

            if (joyGetButtons(0, L_TRIG) != 0)
            {
                gitem->equip_watch_z *= 0.98000002f;
            }

            if (joyGetButtons(0, R_TRIG) != 0)
            {
                gitem->equip_watch_z *= 1.0204082f;
            }

#if defined(VERSION_US) && defined(DEBUG)
            osSyncPrintf(
                "gun list pos x=%f[CL,CR] y=%f[CD,CU] z=%f[TL,TR] ",
                gitem->equip_watch_x,
                gitem->equip_watch_y,
                gitem->equip_watch_z);
#endif
        }

        guPerspective(sp924, &perspNorm, sp894, WATCH_PERSPECTIVE_ASPECT, 10.0f, 10000.0f, 1.0f);

#undef WATCH_INV_ASPECT_RATIO

        gSPMatrix(gdl++, osVirtualToPhysical(sp924), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

        mtx4LoadYRotation((sp87C * M_TAU_F) / 360.0f, &sp8E0);
        mtx4LoadZRotation(M_TAU_F - ((sp880 * M_TAU_F) / 360.0f), &sp8A0);
        mtx4MultInPlace(&sp8A0, &sp8E0);

        temp_cos = cosf(D_80040B14) * sp888;
        temp_sin = sinf(D_80040B14) * sp888;

        mtx4LoadLookAt(&sp8A0, temp_cos, sp88C, temp_sin + sp890, 0.0f, sp88C, sp890, 0.0f, 1.0f, 0.0f);

        mtx4MultInPlace(&sp8A0, &sp8E0);

        gdl = watchRenderItem(watchMenuSetupDraw(gdl), sp884, &sp8E0, 0x40, 0xA0FFA03C);

        {
            s32 i;
            s32 textheight;
            s32 textwidth;
            s32 pFontFile2;
            s32 pFontChars2;
            char string_builder_allocation[2000];

#if defined(VERSION_EU)
            s32 pFontFile;
            s32 base_y;
            char formattedString[32];
#endif

#define LINEHEIGHT() (j_text_trigger ? 14 : 12)

#if defined(VERSION_EU)
#define WATCH_INV_BASE_Y() base_y
#else
#define WATCH_INV_BASE_Y() 0x8C
#endif

            textheight = 0;
            textwidth = 0;

#if defined(VERSION_EU)
            pFontFile2 = ptrFontBankGothic;
            pFontChars2 = ptrFontBankGothicChars;
            base_y = (j_text_trigger) ? (0x82) : (0xAA);
            string_builder_allocation[0] = 0;
#else
            string_builder_allocation[0] = 0;
            pFontFile2 = ptrFontBankGothic;
            pFontChars2 = ptrFontBankGothicChars;
#endif

            for (i = 0; i < invCountItems(); i++)
            {
                char *name = invGetNameByIndex(i);

                strcat(string_builder_allocation, name);
            }

            if (g_WatchMenuEquipDelay > 0)
            {
                g_WatchMenuEquipDelay--;
            }

            watchMenuInventoryListTick();

            x1 = 0x4E;
            y1 = WATCH_INV_BASE_Y();

            temp_s0_3 = 1;
            temp_s0_3 = (LINEHEIGHT() * 2) + WATCH_INV_BASE_Y() + temp_s0_3;

            gdl = microcode_constructor(gdl);

            textMeasure(&textheight, &textwidth, string_builder_allocation, pFontChars2, pFontFile2, LINEHEIGHT());

            gdl = microcode_constructor_related_to_menus(gdl, 0x4E, WATCH_INV_BASE_Y(), textwidth + 0x4E, (LINEHEIGHT() * 5) + WATCH_INV_BASE_Y(), 0);

            gdl = textRender(gdl, &x1, &y1, string_builder_allocation, pFontChars2, pFontFile2, 0xAA00B0, textwidth + 1, LINEHEIGHT() * 5, g_WatchMenuInvTextY, LINEHEIGHT());

            gdl = microcode_constructor_related_to_menus(gdl, 0x4B, temp_s0_3, textwidth + 0x52, (LINEHEIGHT() + temp_s0_3) - 2, 0x800050);

            {
#if !defined(VERSION_EU)
                char formattedString[32];
                s32 pFontFile;
#endif
                s32 pFontChars;
                s32 x2;
                s32 y2;
                char *invItemName;

                pFontFile = ptrFontBankGothic;
                pFontChars = ptrFontBankGothicChars;
                invItemName = invGetNameByIndex(g_WatchMenuItem);

                sprintf(formattedString, "%d, %d\n%d %f\n", g_WatchMenuInvTextY, g_WatchMenuInvTextTargetY, g_WatchMenuItem, (f64) g_WatchMenuInvCursor);

                gdl = microcode_constructor(gdl);

                textMeasure(&y2, &x2, formattedString, pFontChars, pFontFile, 0);

                if (g_WatchMenuInvSettled != 0)
                {
                    textMeasure(&y2, &x2, invItemName, pFontChars, pFontFile, LINEHEIGHT());

                    x1 = 0x4E;
                    y1 = (LINEHEIGHT() * 2) + WATCH_INV_BASE_Y();

                    if (g_WatchMenuEquipDelay == 0)
                    {
                        gdl = textRender(gdl, &x1, &y1, invItemName, pFontChars, pFontFile, 0xA0FFA0F0, x2, 0x64, 0, LINEHEIGHT());
                    }
                    else
                    {
                        gdl = textRenderOutlined(gdl, &x1, &y1, invItemName, pFontChars, pFontFile, -1, 0x7000A0, x2 + 1, 0x64, 0, LINEHEIGHT());
                    }

                    watchMenuEquipSelected();
                }
            }

#undef WATCH_INV_BASE_Y
#undef LINEHEIGHT
        }
    }

    return gdl;
}


Gfx *watchMenuRenderInventoryOld(Gfx *gdl, Mtx *param_2) {
    s32 temp_1;
    s32 sp70;
    s32 sp6C;
    s32 sp64,sp68; //unused?

    s32 sp60;
    s32 sp5C;
    s32 sp58;
    s32 sp54;
    s32 pFontFile;
    s32 pFontChars;

    u16 *long_name;
    s32 temp_2;

    sp58 = 0;
    sp54 = 0;

    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;

    long_name = invGetLongNameByIndex(g_WatchMenuItem);
    gdl = watchMenuRenderBackground(gdl, param_2, 0);

    if (watchIsZooming() != 1)
    {
        temp_1 = g_WatchMenuEquipDelay;
        if (temp_1 > 0)
        {
            g_WatchMenuEquipDelay = temp_1 - 1;
        }

        watchMenuInventoryListTick();
        gdl = microcode_constructor(gdl);

        textMeasure(&sp58, &sp54, long_name, pFontChars, pFontFile, 0);

        sp70 = ((s32) (0xAA - sp54) / 2) + 0x4B;
        temp_2 = sp70;

        sp6C = 0x1E;
        gdl = microcode_constructor_related_to_menus(gdl, temp_2, 0x1E, sp60, sp5C, 0x800050);

        if (g_WatchMenuInvSettled)
        {
            watchMenuEquipSelected();
            if (g_WatchMenuEquipDelay == 0)
            {
                gdl = textRender(gdl, &sp70, &sp6C, long_name, pFontChars, pFontFile, 0xA0FFA0F0, sp54, 0x64, 0, 0);
            }
            else
            {
                gdl = textRenderOutlined(gdl, &sp70, &sp6C, long_name, pFontChars, pFontFile, -1, 0x7000A0, sp54 + 1, 0x64, 0, 0);
            }
        }
        else
        {
            gdl = textRender(gdl, &sp70, &sp6C, long_name, pFontChars, pFontFile, 0xAA00B0, sp54, 0x64, 0, 0);
        }

    }

    return gdl;
}


/**
 * Address: 7F0A8D40
 */
void watchMenuMakeSliderVerts(struct WatchVertex *verts, f32 fill_amount, s32 transition_width)
{
    s32 i;
    struct WatchVertex *vtx;
    s32 xdiff;
    s32 filledrightx;

    xdiff = verts[2].coord1.x - verts[4].coord1.x;
    transition_width = (s32) (((f32) transition_width) * (1.2f - fill_amount));
    i = 0;
    vtx = verts;

    /**
     * Verts 0-3: unfilled right section.
     * Dark green.
     */
    do
    {
        i++;
        vtx++;
        vtx[-1].color.r = 0x20;
        vtx[-1].color.g = 0x40;
        vtx[-1].color.b = 0x20;
        vtx[-1].color.a = 0xE0;
    } while (i < 4);

    i = 4;
    vtx = &verts[4];

    /**
     * Verts 4-9: filled left section and transition.
     * Verts 10 and 11: moving boundary between transition and unfilled section.
     */
    do
    {
        filledrightx = xdiff + transition_width;

        if (i < 10)
        {
            // The filled section gets brighter as the volume increases.
            s32 rb;
            s32 g;
            rb = ((s32) (48.0f * fill_amount)) + 0x40;
            g = ((s32) (96.0f * fill_amount)) + 0x80;
            vtx->color.r = rb;
            vtx->color.g = g;
            vtx->color.b = rb;

            // Left edge of the transition band.
            if (i >= 6)
            {
                vtx->coord1.x = (s32) ((((f32) verts[4].coord1.x) + ((((f32) xdiff) + ((f32) transition_width)) * fill_amount)) - ((f32) transition_width));
                if (vtx->coord1.x < verts[4].coord1.x)
                {
                    vtx->coord1.x = verts[4].coord1.x;
                }
            }
        }
        else
        {
        // Right edge of the transition band.
        vtx->coord1.x = (s32) ((((f32) verts[4].coord1.x) + (((f32) filledrightx) * fill_amount)) + ((f32) transition_width));
            if (verts[2].coord1.x < vtx->coord1.x)
            {
                vtx->coord1.x = verts[2].coord1.x;
            }
        }

    i++;
    vtx++;

    } while (i != 12);

    // Make the unfilled section begin at the right edge of the transition area.
    filledrightx = verts[10].coord1.x;
    verts[1].coord1.x = filledrightx;
    verts[0].coord1.x = filledrightx;
}



/**
 * Address: 7F0A8ED0
 */
void watchMenuAdjustVolume(u16* outVolume) {
    s32 joy_x;
    s32 adjusted_volume;

    joy_x = joyGetStickX(WATCH_PAD);
    adjusted_volume = *outVolume;

    if (joyGetButtons(WATCH_PAD, R_CBUTTONS|R_TRIG|R_JPAD)) {
        adjusted_volume = adjusted_volume + WATCH_VOL_ADJUST_STEP;
    } else if (joyGetButtons(WATCH_PAD, L_CBUTTONS|L_TRIG|L_JPAD)) {
        adjusted_volume = adjusted_volume - WATCH_VOL_ADJUST_STEP;
    }

    // Clamp stick deflection
    if (joy_x >= 0x47) {
        joy_x = 0x46;
    } else if (joy_x < -0x46) {
        joy_x = -0x46;
    }

    // Increase volume
    if (joy_x >= 8) {
        adjusted_volume += (joy_x * 0x800 + -0x3800) / 0x46;
    // Decrease volume
    } else if (joy_x < -7) {
        adjusted_volume += (joy_x * 0x800 + 0x3800) / 0x46;
    }

    // Clamp volume between min and max allowed volume.
    if (adjusted_volume >= VOLUME_MAX + 1) {
        *outVolume = VOLUME_MAX;
    } else if (adjusted_volume < 0) {
        *outVolume = 0;
    } else {
        *outVolume = adjusted_volume;
    }
}


/**
 * Address: 7F0A8FEC
 */
Gfx *watchMenuRenderFxVolume(Gfx *gdl)
{
    u16 volume;
    f32 fvolume;
    struct WatchVertex *vtx1;
    struct WatchVertex *vtx;
    Gfx *cmd;

    vtx1 = (struct WatchVertex *)dynAllocateVertices(12);

    volume = sndGetSfxSlotFirstNaturalVolume();

    if (g_WatchMenuSelected && g_WatchMenuOptionsRow == 1)
    {
        watchMenuAdjustVolume(&volume);
    }

    fvolume = (f32)(u32)volume / 32767.0f;

    sndApplyVolumeAllSfxSlot(volume);

    if (1);

    cmd = gdl++;
    gDPSetRenderMode(cmd, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gdl = watchRenderQuad(gdl, OS_K0_TO_PHYSICAL(vtx1));
    vtx = watchMakeRectVerts(vtx1, 0, 0, 600, 20, -299, -205);

    gdl = watchRenderQuad(gdl, OS_K0_TO_PHYSICAL(vtx));
    vtx = watchMakeRectVerts(vtx, 0, 0, 600, 20, -299, -205);

    gdl = watchRenderQuad(gdl, OS_K0_TO_PHYSICAL(vtx));
    watchMakeRectVerts(vtx, 0, 0, 600, 20, -299, -205);

    watchMenuMakeSliderVerts(vtx1, fvolume, 30);

    return gdl;
}


u16 optionsGetFxVolume(void) {
    return sndGetSfxSlotFirstNaturalVolume();
}


void optionsSetFxVolume(u16 arg0) {
    sndApplyVolumeAllSfxSlot(arg0);
}


/**
 * Address: 7F0A91C8
 */
Gfx *watchMenuRenderMusicVolume(Gfx *gdl)
{
    u16 volume;
    f32 fvolume;
    struct WatchVertex *vtx1;
    struct WatchVertex *vtx;
    Gfx *cmd;

    vtx1 = (struct WatchVertex *)dynAllocateVertices(12);
    volume = optionsGetMusicVolume();

    if (g_WatchMenuSelected && g_WatchMenuOptionsRow == 0) {
        watchMenuAdjustVolume(&volume);
    }

    fvolume = (f32)(u32)volume / 32767.0f;
    optionsSetMusicVolume(volume);

    if(1);

    cmd = gdl++;
    gDPSetRenderMode(cmd, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gdl = watchRenderQuad(gdl, OS_K0_TO_PHYSICAL(vtx1));
    vtx = watchMakeRectVerts(vtx1, 0, 0, 600, 20, -299, -275);


    gdl = watchRenderQuad(gdl, OS_K0_TO_PHYSICAL(vtx));
    vtx = watchMakeRectVerts(vtx, 0, 0, 600, 20, -299, -275);

    gdl = watchRenderQuad(gdl, OS_K0_TO_PHYSICAL(vtx));
    watchMakeRectVerts(vtx, 0, 0, 600, 20, -299, -275);

    watchMenuMakeSliderVerts(vtx1, fvolume, 30);

    return gdl;
}


u16 optionsGetMusicVolume(void)
{
  return g_OptionsMusicVolume;
}


void optionsSetMusicVolume(u16 param_1)
{
    g_OptionsMusicVolume = param_1;
    musicTrack2ApplySeqpVol(g_OptionsMusicVolume);
}


/**
 * Address: 7F0A9398
 *
 * This draws the text for the toggle options (both option titles and values).
 * It also draws button names and the actions mapped to them on the controller screen.
 */
Gfx *watchMenuRenderLabel(Gfx *gdl, s32 x, s32 y, char *text, u32 colour, s32 outlined, u32 outlinecolour, s32 centre, s32 drawbg, u32 bgcolour, s32 rightalign)
{
    s32 textx;
    s32 textright;
    s32 textbottom;
    s32 textwidth;
    s32 textheight;
    struct font *font;
    struct fontchar *chars;

    font = ptrFontBankGothic;
    chars = ptrFontBankGothicChars;

    textMeasure(&textheight, &textwidth, text, chars, font, 10);

    if (centre)
    {
        textx = x - (textwidth / 2);
    }
    else if (rightalign)
    {
        textx = x - textwidth;
    }
    else
    {
        textx = x;
    }

    textright = textx + textwidth;
    textbottom = y + textheight;

    if (g_WatchBackgroundGreen < 0xe0)
    {
        /**
         * Increases the effect of fuzzy static mode on the text,
         * making text pixels more subject to disappearing or almost disappearing.
         */
        gDPSetRenderMode(gdl++, G_RM_AA_PCL_SURF, G_RM_AA_PCL_SURF2);
    }
    else
    {
        gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    }

    if (drawbg)
    {
        gdl = microcode_constructor_related_to_menus(gdl, textx - 1, (y + outlined) + 1, textright + 1, textbottom + 1, bgcolour);
    }

    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

    if (!outlined)
    {
        gdl = textRender(gdl, &textx, &y, text, chars, font, colour, textwidth, textheight, 0, 10);
    }

    if (outlined)
    {
        gdl = textRenderOutlined(gdl, &textx, &y, text, chars, font, colour, outlinecolour, textwidth + 1, textheight, 0, 10);
    }

    return gdl;
}


f32 watchMenuApproach(f32 param_1, f32 param_2, f32 param_3)
{
    if (param_1 < param_2) {
        param_1 += (param_2 - param_1) / param_3;
    } else if (param_2 < param_1) {
        param_1 -= (param_1 - param_2) / param_3;
    }

    return param_1;
}


s32 watchMenuControllerIsLevel(void) {

    if ((g_WatchControllerSpinAngle < 0.1f) &&
        (g_WatchControllerSpinAngle > -0.1f) &&
        (g_WatchControllerPitch < 0.1f) &&
        (g_WatchControllerPitch > -0.1f))
    {

        return 1;

    }
    return 0;
}


void watchMenuSpinController(s8 contpadnum, s32 *counter, f32 *value, f32 *step)
{
    s32 count;

    if ((joyGetStickX(contpadnum) >= 10) || (joyGetStickX(contpadnum) < -9))
    {
        if (g_WatchMenuSelected)
        {
            count = 0;

            if (g_WatchMenuControlRow == 1)
            {
                goto zero_done;
            }
        }
    }

    count = *counter;

    if (count < 100)
    {
        count = (*counter = count + 1);
        count = *counter;
    }

    goto counter_done;

zero_done:
    *counter = 0;

counter_done:
    if (count >= 100)
    {
        *value = watchMenuApproach(*value, (-(*step)) / 10.0f, 4.0f);
    }
    else if (g_WatchMenuSelected && (g_WatchMenuControlRow == 1))
    {
        *value = watchMenuApproach(*value, (((-((f32) joyGetStickX(contpadnum))) * 0.2f) * M_TAU_F) / 360.0f, 4.0f);
    }
}


/**
 * Address: 7F0A97D0
 */
Gfx *watchMenuRenderControlStyles(Gfx *gdl)
{
    char pad[12];
    char text[2000];
    s32 x;
    s32 y;
    s32 i;
    u16 *stringids;
    s32 textheight;
    s32 textwidth;
    struct font *font;
    struct fontchar *chars;
    s32 tmp;
    u8 *selectedtext;

    font = ptrFontBankGothic;
    chars = ptrFontBankGothicChars;
    textheight = 0;
    textwidth = 0;
    text[0] = '\0';
    i = 0;

    if (g_WatchMenuControlStyleCount > 0)
    {
        stringids = game_control_styles;

        do
        {
            strcat(text, langGet(*stringids));
            i++;
            stringids += 10;
            if (i);
        }
        while (i < g_WatchMenuControlStyleCount);
    }

    if (g_WatchMenuSelected)
    {
        if (g_WatchMenuControlRow == CONTROLLER_OPTIONS_INDEX_STYLE)
        {
            watchMenuScrollList(&g_CurrentPlayer->cur_player_control_type_2, &g_CurrentPlayer->cur_player_control_type_0, g_WatchMenuControlStyleCount, &g_CurrentPlayer->neg_vspacing_for_control_type_entry, &g_CurrentPlayer->cur_player_control_type_1, (s32 *) (&g_CurrentPlayer->has_set_control_type_data), 0, 1, (j_text_trigger) ? (0xe) : (0xa));
        }
    }

    x = 0xaa;
    tmp = j_text_trigger;
    y = 0x1a;

    textMeasure(&textheight, &textwidth, text, chars, font, (tmp) ? (0xe) : (0xa));

    i = (j_text_trigger) ? (0xe) : (0xa);

    gdl = textRender(gdl, &x, &y, text, chars, font, 0x00aa00b0, textwidth, i, g_CurrentPlayer->neg_vspacing_for_control_type_entry, (j_text_trigger) ? (0xe) : (0xa));

    if (g_CurrentPlayer->has_set_control_type_data != 0)
    {
        selectedtext = langGet(*(u16 *)((u8 *) game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20)));

        textMeasure(&textheight, &textwidth, selectedtext, chars, font, (j_text_trigger) ? (0xe) : (0xa));

        x = 0xaa;

        if (j_text_trigger ? 1 : 0)
        {
            goto selected_y_set;
        }

        goto selected_y_set;

selected_y_set:
        y = 0x1a;

        selectedtext = langGet(*(u16 *)((u8 *) game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20)));

        gdl = textRender(gdl, &x, &y, selectedtext, chars, font, 0xa0ffa0f0, textwidth, 0x64, 0, (j_text_trigger) ? (0xe) : (0xa));
    }

    return gdl;
}

 
Gfx *watchMenuRenderControlLabels(Gfx *gdl)
{
    u8 *dirtext1;
    u8 *dirtext2;
 
    if (g_Options[0].current_value == 1)
    {
        dirtext1 = langGet(getStringID(LOPTIONS, OPTION_STR_2D_UP_LF));
        dirtext2 = langGet(getStringID(LOPTIONS, OPTION_STR_2C_DOWN_LF));
    }
    else
    {
        dirtext1 = langGet(getStringID(LOPTIONS, OPTION_STR_2C_DOWN_LF));
        dirtext2 = langGet(getStringID(LOPTIONS, OPTION_STR_2D_UP_LF));
    }
 
    {
        char strA[] = "(A)\n";
        char strB[] = "(B)\n";
        char strZ[] = "(Z)\n";
        char strL[] = "(L)\n";
        char strR[] = "(R)\n";
        char strC[] = "(C)\n";
        char strPlus[] = "(+)\n";
        char strS[] = "(S)\n";
        char str3D[] = "(3D)\n";
        u8 *ctext;
        u8 *dpadtext;
        s32 buttons;
        s32 showmovesight;
        volatile unsigned int y;
        u8 pad[8];
 
        showmovesight = 0;
        gdl = microcode_constructor(gdl);
 
        if (joyGetButtons(WATCH_PAD, L_TRIG))
        {
            gdl = watchMenuRenderLabel(gdl, 0x32, OPTLABELS_ROW1_Y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 8)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0);
 
            if (*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 8) == getStringID(LOPTIONS, OPTION_STR_01_AIM_LF))
            {
                showmovesight = 1;
            }
        }
        else
        {
            gdl = watchMenuRenderLabel(gdl, 0x32, OPTLABELS_ROW1_Y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 8)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
        }
 
        y = OPTLABELS_ROW2_Y;
 
        if (g_WatchMenuControlRow != 1 || !g_WatchMenuSelected || !joyGetButtons(WATCH_PAD, U_JPAD | D_JPAD | L_JPAD | R_JPAD))
        {
            gdl = watchMenuRenderLabel(gdl, 0x32, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 14)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
        }
        else
        {
            if (joyGetButtons(WATCH_PAD, U_JPAD))
            {
                if (*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 14) == getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF))
                {
                    dpadtext = langGet(getStringID(LOPTIONS, OPTION_STR_30_FORWARD_LF));
                }
                else
                {
                    dpadtext = dirtext1;
                }
            }
            else if (joyGetButtons(WATCH_PAD, D_JPAD))
            {
                if (*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 14) == getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF))
                {
                    dpadtext = langGet(getStringID(LOPTIONS, OPTION_STR_31_BACK_LF));
                }
                else
                {
                    dpadtext = dirtext2;
                }
            }
            else if (joyGetButtons(WATCH_PAD, L_JPAD))
            {
                dpadtext = langGet(getStringID(LOPTIONS, OPTION_STR_2F_SIDESTEP_LF));
            }
            else if (joyGetButtons(WATCH_PAD, R_JPAD))
            {
                dpadtext = langGet(getStringID(LOPTIONS, OPTION_STR_2E_SIDESTEP_LF));
            }
 
            gdl = watchMenuRenderLabel(gdl, 0x32, y, dpadtext, -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0);
        }
 
        y += OPTLABELS_ROW_PITCH;
        gdl = watchMenuRenderLabel(gdl, 0x32, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 16)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
        y += OPTLABELS_ROW_PITCH;
 
        if (joyGetButtons(WATCH_PAD, Z_TRIG))
        {
            gdl = watchMenuRenderLabel(gdl, 0x32, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 6)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0);
 
            if (*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 6) == getStringID(LOPTIONS, OPTION_STR_01_AIM_LF))
            {
                showmovesight = 1;
            }
        }
        else
        {
            gdl = watchMenuRenderLabel(gdl, 0x32, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 6)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
        }
 
        y -= OPTLABELS_COL_RET;
 
        if (joyGetButtons(WATCH_PAD, R_TRIG))
        {
            gdl = watchMenuRenderLabel(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 10)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
 
            if (*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 10) == getStringID(LOPTIONS, OPTION_STR_01_AIM_LF))
            {
                showmovesight = 1;
            }
        }
        else
        {
            gdl = watchMenuRenderLabel(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 10)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
 
        y += OPTLABELS_ROW_PITCH;
 
        if (g_WatchMenuControlRow != 1 || !g_WatchMenuSelected || !joyGetButtons(WATCH_PAD, U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS))
        {
            gdl = watchMenuRenderLabel(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 12)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
        else
        {
            buttons = joyGetButtons(WATCH_PAD, U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS);
 
            /* power-of-two test: exactly one C button held */
            if ((buttons & (buttons - 1U)) == 0)
            {
                if (joyGetButtons(WATCH_PAD, U_CBUTTONS))
                {
                    if (*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 12) == getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF))
                    {
                        ctext = langGet(getStringID(LOPTIONS, OPTION_STR_30_FORWARD_LF));
                    }
                    else
                    {
                        ctext = dirtext1;
                    }
                }
                else if (joyGetButtons(WATCH_PAD, D_CBUTTONS))
                {
                    if (*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 12) == getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF))
                    {
                        ctext = langGet(getStringID(LOPTIONS, OPTION_STR_31_BACK_LF));
                    }
                    else
                    {
                        ctext = dirtext2;
                    }
                }
                else if (joyGetButtons(WATCH_PAD, L_CBUTTONS))
                {
                    ctext = langGet(getStringID(LOPTIONS, OPTION_STR_2F_SIDESTEP_LF));
                }
                else if (joyGetButtons(WATCH_PAD, R_CBUTTONS))
                {
                    ctext = langGet(getStringID(LOPTIONS, OPTION_STR_2E_SIDESTEP_LF));
                }
 
                gdl = watchMenuRenderLabel(gdl, 0x10e, y, ctext, -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
            }
            else
            {
                gdl = watchMenuRenderLabel(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 12)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
            }
        }
 
        y += OPTLABELS_ROW_PITCH;
 
        if (joyGetButtons(WATCH_PAD, B_BUTTON))
        {
            gdl = watchMenuRenderLabel(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 4)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
        }
        else
        {
            gdl = watchMenuRenderLabel(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 4)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
 
        y += OPTLABELS_ROW_PITCH;
 
        if (joyGetButtons(WATCH_PAD, A_BUTTON))
        {
            gdl = watchMenuRenderLabel(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 2)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
        }
        else
        {
            gdl = watchMenuRenderLabel(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 2)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
 
        if (showmovesight)
        {
            gdl = watchMenuRenderLabel(gdl, 0xfa, OPTLABELS_HINT_Y, langGet(getStringID(LOPTIONS, OPTION_STR_08_MOVESIGHT_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
        }
        else
        {
            gdl = watchMenuRenderLabel(gdl, 0xfa, OPTLABELS_HINT_Y, langGet(*(u16 *)((u8 *)game_control_styles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 18)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
 
        return gdl;
    }
}


Gfx *watchMenuRenderDualControlLabels(Gfx *gdl)
{
    s32 textptr_aux;

    gdl = microcode_constructor(gdl);

    if (joyGetButtons(WATCH_PAD, A_BUTTON))
    {
        gdl = watchMenuRenderLabel(gdl, 0x5A, YOFFSET_WEAPTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0); //weapon
    }
    else
    {
        gdl = watchMenuRenderLabel(gdl, 0x5A, YOFFSET_WEAPTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0); //weapon
    }

    if (joyGetButtons(WATCH_PAD, B_BUTTON))
    {
        gdl = watchMenuRenderLabel(gdl, 0x5A, YOFFSET_ACTIONTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0); //action
    }
    else
    {
        gdl = watchMenuRenderLabel(gdl, 0x5A, YOFFSET_ACTIONTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0); //action
    }

    if ((g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_PLENTY) || (g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_GALORE))
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF)); //fire
    }
    else
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_01_AIM_LF)); //aim
    }

    if (joyGetButtons(WATCH_PAD, Z_TRIG))
    {
        gdl = watchMenuRenderLabel(gdl, 0x5A, YOFFSET_5, textptr_aux, -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0);
    }
    else
    {
        gdl = watchMenuRenderLabel(gdl, 0x5A, YOFFSET_5, textptr_aux, 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
    }

    if ((g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_PLENTY) || (g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_DOMINO))
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF)); //move
    }
    else
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF)); //look
    }

    gdl = watchMenuRenderLabel(gdl, 0x5A, YOFFSET_4, textptr_aux, 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);

    if (joyGetButtons(1, A_BUTTON))
    {
        gdl = watchMenuRenderLabel(gdl, 0xE6, YOFFSET_WEAPTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1); //weapon
    }
    else
    {
        gdl = watchMenuRenderLabel(gdl, 0xE6, YOFFSET_WEAPTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1); //weapon
    }

    if (joyGetButtons(1, B_BUTTON))
    {
        gdl = watchMenuRenderLabel(gdl, 0xE6, YOFFSET_ACTIONTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1); //action
    }
    else
    {
        gdl = watchMenuRenderLabel(gdl, 0xE6, YOFFSET_ACTIONTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1); //action
    }

    if ((g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_PLENTY) || (g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_GALORE))
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_01_AIM_LF)); //aim
    }
    else
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF)); //fire
    }

    if (joyGetButtons(1, Z_TRIG))
    {
        gdl = watchMenuRenderLabel(gdl, 0xE6, YOFFSET_5, textptr_aux, -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
    }
    else
    {
        gdl = watchMenuRenderLabel(gdl, 0xE6, YOFFSET_5, textptr_aux, 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
    }

    if ((g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_PLENTY) || (g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_DOMINO))
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF)); //look
    }
    else
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF)); //move
    }

    gdl = watchMenuRenderLabel(gdl, 0xE6, YOFFSET_4, textptr_aux, 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
    return gdl;
}


/**
 * Address: 7F0AADC0
 * 
 * Draw the controller model(s) and the individual buttons.
 */
Gfx *watchMenuRenderController(Gfx *gdl)
{
    Mtx *perspmtx;
    Mtxf identity;
    Mtxf zrotmtx;
    Mtxf xrotmtx;
    Mtxf tmpmtx1;
    Mtxf tmpmtx2;
    Mtxf modelmtx;
    Mtxf finalmtx;
    coord3d pos;
    u16 perspNorm;
    Mtxf lookat1;
    Mtxf lookat2;
    void *watchTable;
    s32 green;
    s8 contpadnum0;
    s8 contpadnum1;
    WatchContButtonPositions table0;
    WatchContButtonPositions table1;
    WatchContButtonPositions table2;
    Gfx *cmd0;
    Gfx *cmd1;

    perspmtx = dynAllocateMatrix();
    pos = g_ControllerPos;
    contpadnum0 = 0;
    contpadnum1 = 1;
    table0 = g_1ContButtonPositions[0];
    table1 = g_2ContLeftButtonPositions[0];
    table2 = g_2ContRightButtonPositions[0];

    watchMenuSpinController(0, &D_80040B2C, &g_WatchControllerSpinSpeed, &g_WatchControllerSpinAngle);
    mtx4LoadIdentity(&identity);

    g_WatchControllerSpinAngle += (g_WatchControllerSpinSpeed * ((f32) WATCH_ROTATION_FRAMES)) * 0.5f;
    g_WatchControllerSpinAngle = watchMenuWrapAngle(g_WatchControllerSpinAngle);

    mtx4LoadZRotation(g_WatchControllerSpinAngle, &zrotmtx);

    if (g_WatchMenuSelected && (g_WatchMenuControlRow == CONTROLLER_OPTIONS_INDEX_INPUTS))
    {
        g_WatchControllerPitch = watchMenuApproach(g_WatchControllerPitch, (((f32) joyGetStickY(WATCH_PAD)) * M_TAU_F) / 360.0f, 4.0f);
    }
    else
    {
        g_WatchControllerPitch = watchMenuApproach(g_WatchControllerPitch, 0.0f, 4.0f);
    }

    mtx4LoadXRotation((-g_WatchControllerPitch) - 0.78539819f, &xrotmtx);
    mtx4Mult(&identity, &zrotmtx, &tmpmtx1);
    mtx4Mult(&tmpmtx1, &xrotmtx, &tmpmtx2);
    mtx4LoadTranslation(&pos, &tmpmtx1);
    mtx4Mult(&tmpmtx1, &tmpmtx2, &modelmtx);

    if (optionsIsDualControlStyle())
    {
        f32 eye = 495.0f;

        watchTable = &table1;

        if (1);

        mtx4LoadLookAt(&lookat1, eye, 2500.0f, 32.0f, eye, 0.0f, 32.0f, 0.0f, 0.0f, -1.0f);
    }
    else
    {
        watchTable = &table0;
        mtx4LoadLookAt(&lookat1, -5.0f, 2000.0f, -168.0f, -5.0f, 0.0f, -168.0f, 0.0f, 0.0f, -1.0f);
    }

    mtx4Mult(&lookat1, &modelmtx, &finalmtx);

    guPerspective(perspmtx, &perspNorm, WATCH_PERSPECTIVE_FOVY, WATCH_PERSPECTIVE_ASPECT, 1000.0f, 3000.0f, 1.0f);

    cmd0 = gdl++; \
    gSPMatrix(cmd0, osVirtualToPhysical(perspmtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    gdl = watchMenuSetupDraw(gdl);
    green = g_WatchBackgroundGreen;

    if (green < 0xe0)
    {
        gdl = watchRenderController(gdl, &finalmtx, green - 6, 1, watchTable, &contpadnum0);
    }
    else
    {
        gdl = watchRenderControllerOpaque(gdl, &finalmtx, 1, (s32) watchTable, &contpadnum0);
    }

    if (optionsIsDualControlStyle())
    {
        watchMenuSpinController(1, &D_80040B3C, &D_80040B38, &D_80040B34);
        mtx4LoadIdentity(&identity);

        D_80040B34 += (D_80040B38 * ((f32) WATCH_ROTATION_FRAMES)) * 0.5f;
        D_80040B34 = watchMenuWrapAngle(D_80040B34);

        mtx4LoadZRotation(D_80040B34, &zrotmtx);

        if (g_WatchMenuSelected && (g_WatchMenuControlRow == CONTROLLER_OPTIONS_INDEX_INPUTS))
        {
            D_80040B30 = watchMenuApproach(D_80040B30, (((f32) joyGetStickY(PLAYER_2)) * M_TAU_F) / 360.0f, 4.0f);
        }
        else
        {
            D_80040B30 = watchMenuApproach(D_80040B30, 0.0f, 4.0f);
        }

        mtx4LoadXRotation((-D_80040B30) - 0.78539819f, &xrotmtx);
        mtx4Mult(&identity, &zrotmtx, &tmpmtx1);
        mtx4Mult(&tmpmtx1, &xrotmtx, &tmpmtx2);
        mtx4LoadTranslation(&pos, &tmpmtx1);
        mtx4Mult(&tmpmtx1, &tmpmtx2, &modelmtx);
        mtx4LoadLookAt(&lookat2, -505.0f, 2500.0f, 32.0f, -505.0f, 0.0f, 32.0f, 0.0f, 0.0f, -1.0f);
        mtx4Mult(&lookat2, &modelmtx, &finalmtx);

        guPerspective(perspmtx, &perspNorm, WATCH_PERSPECTIVE_FOVY, WATCH_PERSPECTIVE_ASPECT, 1000.0f, 3000.0f, 1.0f);

        /**
         * This should be something like:
         * 
         *   cmd1 = gdl++;
         *   gSPMatrix(cmd1, osVirtualToPhysical(perspmtx),G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
         * 
         * but it doesn't match for some reason.
         */
        cmd1 = gdl++; cmd1->words.w0 = 0x01030040; cmd1->words.w1 = osVirtualToPhysical(perspmtx);

        gdl = watchMenuSetupDraw(gdl);
        green = g_WatchBackgroundGreen;

        if (green < 0xe0)
        {
            gdl = watchRenderController(gdl, &finalmtx, green - 6, 1, &table2, &contpadnum1);
        }
        else
        {
            gdl = watchRenderControllerOpaque(gdl, &finalmtx, 1, (s32) (&table2), &contpadnum1);
        }
    }

    if (optionsIsDualControlStyle())
    {
        gdl = watchMenuRenderDualControlLabels(gdl);
    }
    else
    {
        gdl = watchMenuRenderControlLabels(gdl);
    }

    return gdl;
}


void watchMenuResetControlRow(void) {
    g_WatchMenuControlRow = CONTROLLER_OPTIONS_INDEX_STYLE;
}


void watchMenuResetOptionsRow(void) {
    g_WatchMenuOptionsRow = 0;
}


void watchMenuResetAbort(void) {
    g_WatchMenuAbortAsk = 0;
}


u32 watchMenuIdentity(u32 uParm1) {
    return uParm1;
}


Gfx *watchMenuRenderControl(Gfx *gdl, Mtx *param_2) {
    s32 phi_s1;
    u16 *textptr;
    s32 sp5C;
    s32 sp58;
    s32 sp54;
    s32 sp50;
    s32 pFontFile;
    s32 pFontChars;

    gdl = watchMenuRenderBackground(gdl, param_2, 0);

    if (watchIsZooming() != 1) {

        gdl = watchMenuRenderController(gdl);
        pFontFile = ptrFontBankGothic;
        pFontChars = ptrFontBankGothicChars;

        gdl = microcode_constructor(gdl);
        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_32_CONTROLSTYLE_LF)); //control style

        sp5C = XOFFSET_1;
        sp58 = 0x1A;
        phi_s1 = 0xFF00B0;
        if (g_WatchMenuControlRow == CONTROLLER_OPTIONS_INDEX_STYLE)
        {
            phi_s1 = 0xA0FFA0F0;
            if (g_WatchMenuSelected != 0)
            {
                phi_s1 = -1;
            }
        }

        textMeasure(&sp50, &sp54, textptr, pFontChars, pFontFile, 0);

        if ((g_WatchMenuSelected != 0) && (g_WatchMenuControlRow == CONTROLLER_OPTIONS_INDEX_STYLE))
        {
            gdl = textRenderOutlined(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, 0x7000A0, sp54 + 1, sp50, 0, 0);
        } else
        {
            gdl = textRender(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, sp54, sp50, 0, 0);
        }

        gdl = watchMenuRenderControlStyles(gdl);
        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_33_CONTROLLER_LF)); //controller;

        phi_s1 = 0xFF00B0;

        if (optionsIsDualControlStyle())
        {
            textptr = langGet(getStringID(LOPTIONS, OPTION_STR_34_CONTROLLERS_LF)); //controllers;
        }
        sp5C = XOFFSET_1;
        sp58 = 0x2B;

        if (g_WatchMenuControlRow == CONTROLLER_OPTIONS_INDEX_INPUTS)
        {
            phi_s1 = 0xA0FFA0F0;
            if (g_WatchMenuSelected != 0)
            {
                phi_s1 = -1;
            }
        }

        textMeasure(&sp50, &sp54, textptr, pFontChars, pFontFile, 0);

        if ((g_WatchMenuSelected != 0) && (g_WatchMenuControlRow == CONTROLLER_OPTIONS_INDEX_INPUTS))
        {
            gdl = textRenderOutlined(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, 0x7000A0, sp54 + 1, sp50, 0, 0);
        } else
        {
            gdl = textRender(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, sp54, sp50, 0, 0);
        }

    }
    return gdl;
}


void watchMenuOptionSet(u32 *param_1, u32 param_2)
{
    *param_1 = param_2;
    watchMenuStickXUsed();
    sndPlaySfx(g_musicSfxBufferPtr, OPTION_CHOOSE_SFX, NULL);
}


void watchMenuOptionToggleTick(s32 option_index)
{
    if ( (joyGetButtonsPressedThisFrame(WATCH_PAD, L_CBUTTONS|L_TRIG|L_JPAD) || watchMenuStickPushedLeft()) && g_WatchMenuSelected )
    {
        if (g_Options[option_index].current_value == 1)
        {
            watchMenuOptionSet(&g_Options[option_index].current_value, 0);
        }
        else if (g_Options[option_index].current_value == 2)
        {
            watchMenuOptionSet(&g_Options[option_index].current_value, 1);
        }
    }
    else
    {
        if ( (joyGetButtonsPressedThisFrame(WATCH_PAD, R_CBUTTONS|R_TRIG|R_JPAD) || watchMenuStickPushedRight()) && g_WatchMenuSelected )
        {
            if (g_Options[option_index].current_value == 0)
            {
                watchMenuOptionSet(&g_Options[option_index].current_value, 1);
            }
            else if ( (g_Options[option_index].current_value == 1) && g_Options[option_index].text[3] )
            {
                watchMenuOptionSet(&g_Options[option_index].current_value, 2);
            }
        }
    }
}


/**
 * Address: 7F0AB908
 *
 * Set the color and draw the text for the values of the toggle options.
 * For example, draw the "ON" and "OFF" text for the Auto-Aim option,
 * but not the "AUTO-AIM" text itself.
 *
 * Options are highlighted by using the controller to advance up and down the toggle options list,
 * but options are not selected until the A button is pressed.
 */
Gfx *watchMenuRenderOptionValue(Gfx *gdl, s32 y, s32 option_index, u32 state)
{
    s32 colour1;
    s32 colour2;
    s32 colour3;
    s32 x1;
    s32 x2;
    struct game_options *entry;
    struct game_options *drawentry;

    colour1 = 0x00800080;
    colour2 = 0x00800080;
    colour3 = 0x00800080;

    entry = &g_Options[option_index];

    if (j_text_trigger)
    {
        x1 = 0xAA;
        if (1);
    }
    else
    {
        x1 = 0xB4;
    }

    if (j_text_trigger)
    {
        x2 = 0xDC;
    }
    else
    {
        x2 = 0xE1;
    }

    // Option is unhighlighted
    if (state == 0)
    {
        goto state_unhighlighted;
    }
    // Option is highlighted
    else if (state == 1)
    {
        goto state_highlighted;
    }
    // Option is selected
    else if (state == 2)
    {
        goto state_selected;
    }
    goto after_state;

state_unhighlighted:
    entry = &g_Options[option_index];
    if (entry->current_value == 0)
    {
        colour1 = 0x00FF00B0;
    }
    else if (entry->current_value == 1)
    {
        colour2 = 0x00FF00B0;
    }
    else if (entry->current_value == 2)
    {
        colour3 = 0x00FF00B0;
    }
    goto after_state;

/**
 * Sets color of the active value of the highlighted option.
 * These use the same colors as the active values of the unhighlighted options,
 * so changing the highlighted option has no visual effect.
 */
state_highlighted:
    entry = &g_Options[option_index];
    if (entry->current_value == 0)
    {
        colour1 = 0x00FF00B0;
    }
    else if (entry->current_value == 1)
    {
        colour2 = 0x00FF00B0;
    }
    else if (entry->current_value == 2)
    {
        colour3 = 0x00FF00B0;
    }
    goto after_state;

/**
 * Make the active value of the selected option extra bright.
 */
state_selected:
    watchMenuOptionToggleTick(option_index);
    entry = &g_Options[option_index];
    if (entry->current_value == 0)
    {
        colour1 = 0xA0FFA0F0;
    }
    else if (entry->current_value == 1)
    {
        colour2 = 0xA0FFA0F0;
    }
    else if (entry->current_value == 2)
    {
        colour3 = 0xA0FFA0F0;
    }

after_state:
    if (entry->text[3] == 0)
    {
        if (j_text_trigger)
        {
            x1 = 0xBE;
        }
        else
        {
            x1 = 0xC8;
        }

        if (j_text_trigger)
        {
            // This weird code must be kept on one line for matching.
            x2 = 0xFA; } else { x2 = 0xFA; }
        }

        drawentry = entry;

        gdl = watchMenuRenderLabel(gdl, x1, y, langGet(drawentry->text[1]), colour1, 0, -1, 1, 0, 0x3000B0, 0); // Draw text of option's first value e.g. "Full" for the Screen option.
        gdl = watchMenuRenderLabel(gdl, x2, y, langGet(drawentry->text[2]), colour2, 0, -1, 1, 0, 0x3000B0, 0); // Draw text of option's second value e.g. "Wide" for the Screen option.

        if (drawentry->text[3])
        {
            gdl = watchMenuRenderLabel(gdl, 0x10E, y, langGet(drawentry->text[3]), colour3, 0, -1, 1, 0, 0x3000B0, 0); // Draw text of option's third value e.g. "Cinema" for the Screen option.
        }

    return gdl;
}


Gfx *watchMenuRenderOptionRows(Gfx *gdl)
{
    s32 y_offset;
    s32 i;

    gdl = microcode_constructor(gdl);

    for (i = 0, y_offset = YOFFSET_1; i < PLAYER_OPTION_COUNT; i = i + 1) {

        if (!watchMenuOptionVisible(i))
        {
            continue;
        }

        if ( i == g_WatchMenuOptionsRow - 2)
        {
            // Draw option that is highlighted and selected, if there is one.
            if (g_WatchMenuSelected)
            {
                gdl = watchMenuRenderOptionValue(watchMenuRenderLabel(gdl, XOFFSET_1, y_offset, langGet(g_Options[i].text[0]), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0), y_offset, i, 2);
            }
            // Draw option that is highlighted but not selected, if there is one.
            else
            {
                gdl = watchMenuRenderOptionValue(watchMenuRenderLabel(gdl, XOFFSET_1, y_offset, langGet(g_Options[i].text[0]), 0xA0FFA0F0, 0, -1, 0, 0, 0x3000B0, 0), y_offset, i, 1);
            }
        }
        // Draw the options that are neither highlighted nor selected.
        else
        {
            gdl = watchMenuRenderOptionValue(watchMenuRenderLabel(gdl, XOFFSET_1, y_offset, langGet(g_Options[i].text[0]), 0xFF00B0, 0, -1, 0, 0, 0x3000B0, 0), y_offset, i, 0);
        }

        y_offset = y_offset + YINC_OPTIONS;
    }

    return gdl;
}


Gfx *watchMenuRenderOptions(Gfx *gdl, Mtx *param_2) {
    s32 sp5C;
    u16 *textptr;
    s32 sp54;
    s32 sp50;
    s32 sp4C;
    s32 sp48;

    s32 pFontFile;
    s32 pFontChars;

    gdl = watchMenuRenderBackground(gdl, param_2, 0);

    if (watchIsZooming() != 1)
    {
        gdl = watchMenuRenderMusicVolume(gdl);
        gdl = watchMenuRenderFxVolume(gdl);
        pFontFile = ptrFontBankGothic;
        pFontChars = ptrFontBankGothicChars;
        gdl = microcode_constructor(gdl);

        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_35_MUSIC_LF)); //music

        sp54 = XOFFSET_1;
        sp50 = YOFFSET_8;

        sp5C = 0xFF00B0;

        if (g_WatchMenuOptionsRow == 0)
        {
            sp5C = 0xA0FFA0F0;
            if (g_WatchMenuSelected != 0)
            {
                sp5C = -1;
            }
        }

        textMeasure(&sp48, &sp4C, textptr, pFontChars, pFontFile, 0);

        if ((g_WatchMenuSelected != 0) && (g_WatchMenuOptionsRow == 0))
        {
            gdl = textRenderOutlined(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, 0x7000A0, sp4C + 1, sp48, 0, 0);
        }
        else
        {
            gdl = textRender(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, sp4C, sp48, 0, 0);
        }

        sp5C = 0xFF00B0;
        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_36_FX_LF)); //fx

        sp54 = XOFFSET_1;
        sp50 = YOFFSET_9;


        if (g_WatchMenuOptionsRow == 1)
        {
            sp5C = 0xA0FFA0F0;
            if (g_WatchMenuSelected != 0)
            {
                sp5C = -1;
            }
        }

        textMeasure(&sp48, &sp4C, textptr, pFontChars, pFontFile, 0);

        if ((g_WatchMenuSelected != 0) && (g_WatchMenuOptionsRow == 1))
        {
            gdl = textRenderOutlined(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, 0x7000A0, sp4C + 1, sp48, 0, 0);
        }
        else
        {
            gdl = textRender(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, sp4C, sp48, 0, 0);
        }

        gdl = watchMenuRenderOptionRows(gdl);
    }

    return gdl;
}


int textCountLines(u8 *arg) {
    u8 cVar1;
    int count;

    cVar1 = *arg;
    count = 0;

    while (cVar1) {

        if (cVar1 == 0xA) {
            count = count + 1;
        }

        cVar1 = arg[1];
        arg += 1;
    }

    return count;
}


u8 *textLastLine(u8 *arg)
{
    u8 *ret;

    ret = arg;

    while (*arg != 0)
    {
        if (*arg == 0xa)
        {
            if (*++arg != 0)
            {
                ret = arg;
            }
        }
#if defined(VERSION_EU)
        else
        {
            arg++;
        }
#else
            arg++;
#endif
    }

    return ret;
}


#if defined(VERSION_EU)
//D:800577C0
const char D_800577C0[] = "\n";
#endif


Gfx *watchMenuRenderBriefing(Gfx *gdl, Mtx *param_2)
{
    gdl = watchMenuRenderBackground(gdl, param_2, 0);

    if (watchIsZooming() != 1)
    {
        /**
         * spDAC, spDA4, spD68 are unused.
         * Maybe vestigial tables for formatting the briefings.
         */
        s32 spDAC[15] = {0x34, 0x2f, 0x2d, 0x2a, 0x28, 0x25, 0x25, 0x28, 0x2a, 0x2d, 0x2f, 0x34, 0x37, 0x40, -1};
        s32 spDA4[2] = {0x4b, -1};
        s32 spD68[15] = {0x10e, 0x113, 0x116, 0x119, 0x11a, 0x11b, 0x11b, 0x11a, 0x119, 0x116, 0x113, 0x10e, 0x108, 0xfe, -1};

        s32 boxLeft;
        s32 boxTop;
        s32 boxRight;
        s32 boxBottom;
        s32 textHeight = 0;
        s32 textWidth = 0;

        struct font *font = ptrFontBankGothic;
        struct fontchar *chars = ptrFontBankGothicChars;

#if defined(VERSION_EU)
            char wrappedText[3000];
#else
            char wrappedText[3000] = "\n";
#endif

        char pageTitle[0x20];
        char *completeText;
        char *incompleteText;
        char *failedText;
        char *titleText;

        s32 lineCount;
        s32 objectiveRow;
        s32 objY;
        s32 objX;

        completeText = langGet(0xac28);
        incompleteText = langGet(0xac29);
        failedText = langGet(0xac37);
        titleText = briefingGetText(BRIEFING_TITLE);

#if defined(VERSION_EU)
        strcpy(wrappedText, D_800577C0);
#endif

        gdl = microcode_constructor(gdl);
        textMeasure(&textHeight, &textWidth, titleText, chars, font, 0);

        boxLeft = ((0xaa - textWidth) / 2) + 0x4b;
        boxTop = 0x1e;
        boxRight = boxLeft + textWidth;
        boxBottom = boxTop + textHeight;

        gdl = draw_blackbox_to_screen(gdl, &boxLeft, &boxTop, &boxRight, &boxBottom);
        gdl = textRender(gdl, &boxLeft, &boxTop, titleText, chars, font, 0xa0ffa0f0, textWidth, textHeight, 0, 0);

        boxLeft = 0x41;

        switch (g_WatchMenuBriefPage)
        {
            case BRIEF_INDEX_BACKGROUND:
                sprintf(pageTitle, langGet(0xac38));
                textWrap(0xd2, briefingGetText(BRIEFING_OVERVIEW), wrappedText, chars, font);
                watchMenuBriefBackgroundTick();
                break;

            case BRIEF_INDEX_M:
                sprintf(pageTitle, langGet(0xac39));
                textWrap(0xd2, briefingGetText(BRIEFING_M), wrappedText, chars, font);
                watchMenuBriefMTick();
                break;

            case BRIEF_INDEX_Q:
                sprintf(pageTitle, langGet(0xac3a));
                textWrap(0xd2, briefingGetText(BRIEFING_Q), wrappedText, chars, font);
                watchMenuBriefQTick();
                break;

            case BRIEF_INDEX_MONEYPENNY:
                sprintf(pageTitle, langGet(0xac3b));
                textWrap(0xd2, briefingGetText(BRIEFING_MONEYPENNY), wrappedText, chars, font);
                watchMenuBriefMoneypennyTick();
                break;

            case BRIEF_INDEX_OBJECTIVES:
            {
#if defined(VERSION_EU)
                char objectiveBuffer[200];
#else
                char objectiveBuffer[200] = "";
#endif
                u32 colour;
                s32 i;
                s32 j;
                char *objectiveText;
                s32 visibleObjectiveIndex;
                s32 pad;
                char *objectiveLetterPtr;

                lineCount = 1;
                objectiveRow = 0;
                visibleObjectiveIndex = 0;

                setTextOverlapCorrection((j_text_trigger) ? (1) : (5));
                sprintf(pageTitle, langGet(0xac3c));

                // Counter-op: the guard player has one objective of their own.
                s32 guardplayer = counterOpIsGuardPlayer(playerGetCurrentNum());

                for (i = 0; i < (guardplayer ? 1 : objectiveGetCount()); i++)
                {
                    if (guardplayer || (objectiveGetDifficulty(i) <= lvGetDifficulty()))
                    {
                        objectiveText = guardplayer ? "Kill Bond\n" : objectiveGetText(i);
                        objectiveBuffer[0] = '\0';

                        for (j = 0; j < lineCount; j++)
                        {
                            strcat(objectiveBuffer, D_80058440);
                        }

                        for (j = 0; j < objectiveRow; j++)
                        {
                            strcat(objectiveBuffer, D_80058444);
                        }

                        objectiveLetterPtr = objectiveBuffer + strlen(objectiveBuffer);

                        sprintf(objectiveLetterPtr, aC_2, visibleObjectiveIndex + 'a');
                        strcat(objectiveBuffer, objectiveText);

                        objY = boxTop + ((j_text_trigger) ? (1) : (5));
                        objX = 0x3c;

                        gdl = textRender(gdl, &objX, &objY, objectiveBuffer, chars, font, 0x00ff00b0, viGetX(), viGetY(), 0, 10);
                        lineCount += textCountLines(objectiveLetterPtr);
                        textMeasure(&textHeight, &textWidth, textLastLine(objectiveLetterPtr), chars, font, 10);

                        if ((textWidth + 0x3c) < (viGetX() / 2))
                        {
                            lineCount--;
                        }

                        objectiveBuffer[0] = '\0';

                        for (j = 0; j < lineCount; j++)
                        {
                            strcat(objectiveBuffer, D_80058450);
                        }
                        for (j = 0; j < objectiveRow; j++)
                        {
                            strcat(objectiveBuffer, D_80058454);
                        }

                        switch (guardplayer ? OBJECTIVESTATUS_INCOMPLETE : objectiveCheckStatus(i))
                        {
                            case OBJECTIVESTATUS_INCOMPLETE:
                                strcat(objectiveBuffer, incompleteText);
                                colour = (D_80040AFC << 16) | 0x400040ff;
                                break;

                            case OBJECTIVESTATUS_COMPLETE:
                                strcat(objectiveBuffer, completeText);
                                colour = 0xa0ffa0f0;
                                break;

                            case OBJECTIVESTATUS_FAILED:
                                strcat(objectiveBuffer, failedText);

                                if (j_text_trigger)
                                {
                                    colour = 0xa0ffa0f0;
                                }
                                else
                                {
                                    colour = 0x00ff00b0;
                                }
                                break;

                            default:
                                break;
                        }

                        textMeasure(&textHeight, &textWidth, objectiveBuffer, chars, font, 10);
                        objY = boxTop + ((j_text_trigger) ? (1) : (5));

                        if (j_text_trigger)
                        {
                            objX = 0xf5 - textWidth;
                        }
                        else
                        {
                            objX = 0xaf;
                        }

                        gdl = textRender(gdl, &objX, &objY, objectiveBuffer, chars, font, colour, 0xd2, viGetY(), 0, 10);

                        objectiveRow++;
                        visibleObjectiveIndex++;
                    }
                }

                setTextOverlapCorrection(-1);
                watchMenuBriefObjectivesTick();

                break;

            }
        }

        textMeasure(&textHeight, &textWidth, pageTitle, chars, font, 10);
        gdl = textRender(gdl, &boxLeft, &boxTop, pageTitle, chars, font, 0xa0ffa0f0, 0xd2, 0x82, 0, 10);

        boxTop += 5;
        boxLeft = 0x3c;

        textMeasure(&textHeight, &textWidth, wrappedText, chars, font, 10);
        gdl = textRender(gdl, &boxLeft, &boxTop, wrappedText, chars, font, 0x00ff00b0, viGetX(), viGetY(), 0, 10);
    }

    return gdl;
}


//D:80058440
const char D_80058440[] = " \n";
//D:80058444
const char D_80058444[] = " \n\n";
//D:80058448
const char aC_2[] = "%c: ";
//D:80058450
const char D_80058450[] = " \n";
//D:80058454
const char D_80058454[] = " \n\n";


/**
 * Address: 7F0ACA28
 */
Gfx *watchMenuRender(Gfx *gdl, Mtx *arg1, s32 watch_transitioning)
{
    watchMenuSetTabColours(g_WatchMenuPage, (struct WatchVertex *)g_CurrentPlayer->buffer_for_watch_greenbackdrop_vertices);

    if (watch_transitioning == TRUE)
    {
        playerSetOutsideWatchMenu(FALSE);
        lvSetDrawWorld(0);

        // Handle A or Z button click when in any page but inventory page
        if ((g_WatchMenuPage != WATCH_INDEX_INVENTORY) && (joyGetButtonsPressedThisFrame(WATCH_PAD, Z_TRIG|A_BUTTON)))
        {
            watchMenuToggleSelect();
        }

        switch (g_WatchMenuPage)
        {
            case WATCH_INDEX_MISSION_STATUS:
                gdl = watchMenuRenderStatus(gdl, arg1);
                break;
            case WATCH_INDEX_INVENTORY:
                gdl = watchMenuRenderInventory(gdl, arg1);
                break;
            case WATCH_INDEX_CONTROL_OPTIONS:
                gdl = watchMenuRenderControl(gdl, arg1);
                break;
            case WATCH_INDEX_GAME_OPTIONS:
                gdl = watchMenuRenderOptions(gdl, arg1);
                break;
            case WATCH_INDEX_MISSION_BRIEFING:
                gdl = watchMenuRenderBriefing(gdl, arg1);
        }
    }
    else if (watch_transitioning == FALSE)
    {
        lvSetDrawWorld(1);
        playerSetOutsideWatchMenu(TRUE);
        gdl = watchMenuRenderBackgroundZoom(gdl, arg1);
    }

    return gdl;
}
