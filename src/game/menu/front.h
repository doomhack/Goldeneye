#ifndef _MAINMENU_H_
#define _MAINMENU_H_
#include <ultra64.h>
#include <bondgame.h>
#include <bondconstants.h>


#define TABS_LEFT_EDGE 390.0f
#define TABS_RIGHT_EDGE 411

#define NEXTTAB_CURSOR_HPOS 399.0f
#define NEXTTAB_CURSOR_VPOS 144.0f

#define STARTTAB_TAB_TOP 40.0f
#define STARTTAB_TEXT_TOP 51
//#define STARTTAB_TEXT_HALF 84
#define STARTTAB_TEXT_HALF (STARTTAB_TEXT_TOP + ((STARTTAB_TEXT_BOTTOM-STARTTAB_TEXT_TOP)/2))
#define STARTTAB_TEXT_BOTTOM 117
#define STARTTAB_TAB_BOTTOM 130.5f

#define NEXTTAB_TAB_TOP 130.5f
#define NEXTTAB_TEXT_TOP 144
//#define NEXTTAB_TEXT_HALF 177
#define NEXTTAB_TEXT_HALF (NEXTTAB_TEXT_TOP + ((NEXTTAB_TEXT_BOTTOM-NEXTTAB_TEXT_TOP)/2))
#define NEXTTAB_TEXT_BOTTOM 210
#define NEXTTAB_TAB_BOTTOM 223.0f

#define PREVTAB_TAB_TOP 223.0f
#define PREVTAB_TEXT_TOP 236
//#define PREVTAB_TEXT_HALF 269
#define PREVTAB_TEXT_HALF (PREVTAB_TEXT_TOP + ((PREVTAB_TEXT_BOTTOM-PREVTAB_TEXT_TOP)/2))
#define PREVTAB_TEXT_BOTTOM 302
#define PREVTAB_TAB_BOTTOM 315.0f


struct legal_screen_text {
    s32 h_pos;
    s32 v_pos;
    s32 halign;
    s32 valign;
    u16 txtID;
    u16 anonymous_5;
};

struct mission_folder_setup {
    void * string_ptr;
    u16 folder_text_preset;
    u16 icon_text_preset;
    s32 stage_id;
    s32 unknown; 
    s32 type;
    s32 mission_num;
    void * briefing_name_ptr;
};

struct MP_game_length_settings {
    u16 text_preset;
    u16 padding;
    s32 time;
    s32 points;
};

struct mp_stage_playercount {
    u16 stage;
    u8 min;
    u8 max;
};

struct mp_stage_setup {
    u16 folder_text_preset;
    u16 select_screen_text_preset;
    s32 photo;
    s32 stage_id;
    s32 unlock_after;
    s32 min_player;
    s32 max_player;
};

struct MP_selectable_chars {
    u16 text_preset;
    u8 gender;
    u8 select_photo;
    s16 body;
    s16 head;
    f32 pov;
};

struct MP_handicap_menu {
    u16 text_preset;
    u16 padding;
    f32 damage_modifier;
};

struct MP_controller_configuration_menu {
    u16 text_preset;
    //char field_1;
    u8 field_2;
    u8 field_3;
};

struct MP_sight_aim_settings {
    u16 anonymous_0;
    u8 sight;
    u8 autoaim;
};

struct intro_char {
    enum BODIES body;
    enum HEADS head;
    s16 text1;
    s16 text2;
    s16 text3;
    s16 RESERVED;
    s32 flag;
};

struct intro_animation {
    s32 animID;
    f32 startframeoffset;
    f32 playback_speed;
    s32 camera_preset;
};

struct solo_target_times {
    u16 agent_time;
    u16 secret_agent_time;
    u16 OO_agent_time;
};



extern f32 cast_camera_dist;
//CODE.bss:800695A4
extern f32 cast_camera_dist_start;
//CODE.bss:800695A8
extern f32 cast_camera_dist_end;
//CODE.bss:800695AC
extern f32 cast_camera_angle;
//CODE.bss:800695B0
extern f32 cast_camera_angle_start;
//CODE.bss:800695B4
extern f32 cast_camera_angle_end;
//CODE.bss:800695B8
extern f32 cast_camera_height;
//CODE.bss:800695BC
extern f32 cast_camera_height_start;
//CODE.bss:800695C0
extern f32 cast_camera_height_end;
//CODE.bss:800695C4                     .align 3
//CODE.bss:800695C8
extern struct coord3d cast_target_smoothed;
//CODE.bss:800695D4                     .align 3
//CODE.bss:800695D8
extern struct coord3d cast_target_accumulator;

//CODE.bss:800695E4
extern s32 cast_camera_reset;

//CODE.bss:800695E8
extern struct coord3d cast_rootpos_smoothed;

//CODE.bss:800695F4                     .align 3
//CODE.bss:800695F8
extern struct coord3d cast_rootvel_smoothed;
//CODE.bss:80069604                     .align 3
//CODE.bss:80069608
extern struct coord3d cast_rootvel_accumulator;

//CODE.bss:80069614
extern f32 g_NintendoLogoSpin;
//CODE.bss:80069618
extern f32 g_NintendoLogoScale;
//CODE.bss:80069620
extern coord3d g_FrontFolderScreenPos[MAX_FOLDER_COUNT];

//CODE.bss:80069650
extern u8 g_CheatAvailable[];

//CODE.bss:800696A0
extern u8 g_CheatActivated[];


//CODE.bss:800696F0
extern s32 g_MpFavWeapon[4][2];
//CODE.bss:80069710
extern s32 g_MpCharCursor[4];

//CODE.bss:80069720
extern s32 g_MpCharPrevCursor[4];

//CODE.bss:80069730
extern s32 g_MpCharScroll[4];

//CODE.bss:80069740
extern s32 g_MpCharChosen[];
//CODE.bss:80069750
extern s32 g_MpCharPortraitSize[];
//CODE.bss:80069760
extern s32 g_MpHandicap[];
//CODE.bss:80069770
extern char* g_FrontTabStartText;
//CODE.bss:80069774
extern char* g_FrontTabNextText;
//CODE.bss:80069778
extern char* g_FrontTabPrevText;
//CODE.bss:8006977C
extern s32 g_MpStageCursor;
//CODE.bss:80069780
extern s32 g_MpScenarioCursor;
//CODE.bss:80069784
extern s32 g_FrontDifficultyHighlight;
//CODE.bss:80069788
extern s32 g_MpTeamsCursor;
//CODE.bss:80069790
extern s32 g_NewCheatUnlocked;
//CODE.bss:80069794
extern s32 g_Front007HlReaction;
//CODE.bss:80069798
extern s32 g_Front007HlHealth;
//CODE.bss:8006979C
extern s32 g_Front007HlDamage;
//CODE.bss:800697A0
extern s32 g_Front007HlAccuracy;

//CODE.bss:800697A4                     .align 3

//CODE.bss:800697A8
extern s32 g_MpHandicap[];
//CODE.bss:800697B8
extern s32 g_MpControlStyle[];


//CODE.bss:800697C8
extern s32 g_MpHlPlayers;
//CODE.bss:800697CC
extern s32 g_MpHlScenario;
//CODE.bss:800697D0
extern s32 g_MpHlStage;
//CODE.bss:800697D4
extern s32 g_MpHlLength;
//CODE.bss:800697D8
extern s32 g_MpHlChar;
//CODE.bss:800697DC
extern s32 g_MpHlWeapons;
//CODE.bss:800697E0
extern s32 g_MpHlHandicap;
//CODE.bss:800697E4
extern s32 g_MpHlControls;
//CODE.bss:800697E8
extern s32 g_MpHlAim;
//CODE.bss:800697EC                     .align 4
/*
CODE.bss:800697F0     dword_CODE_bss_800697F0:.space 0x130
*/

extern MENU g_FrontMenu;
extern MENU g_FrontNextMenu;
extern MENU g_FrontEnterMenu;
extern s32 g_FrontTimer;
extern s32 g_FrontTabStartSelected;
extern s32 g_FrontTabNextSelected;
extern s32 g_FrontTabPrevSelected;
extern s32 g_FrontTabStartHighlight;
extern s32 g_FrontTabNextHighlight;
extern s32 g_FrontTabPrevHighlight;

extern f32 tab2_first_x_coord;
extern f32 tab1_max_x_coord;
extern f32 tab2_bottom_y_coord;

extern s32 g_FrontFolder;
extern s32 g_FrontFolderSaved;
extern GAMEMODE g_FrontGameMode;
extern s32 g_FrontStage;
extern s32 g_FrontMission;
extern DIFFICULTY g_FrontDifficulty;
extern s32 g_AppendCheatSinglePlayer;
extern s32 g_AppendCheatMultiPlayer;
extern f32 g_FrontCursorX;
extern f32 g_FrontCursorY;
extern s32 g_FrontBriefingLastPage;
extern s32 g_FrontBriefingPage;
extern s32 g_FrontFolderIcon;
extern s32 g_FrontFolderErase;
extern s32 g_FrontFolderEraseChoice;
extern s32 g_FrontMissionAborted;
extern s32 g_FrontBondKilled;
extern s32 g_FrontFirstLegal;
extern s32 g_FrontFirstBoot;
extern s32 g_FrontSkipIntro;
extern s32 dword_ge_logo_bool;
extern s32 g_FrontStartMusic;
extern s32 g_FrontScreenSize;
extern s32 g_FrontHiResPending;
extern s32 g_FrontLowResPending;
extern s32 g_CheatMenuAvailable;
extern u8 * g_FrontModelBuffer;
extern s32 g_FrontFrameBuffer;
extern  Model * g_FrontFolderModels[];
extern Lights1 g_FrontLogoLight;
extern Lights1 g_FrontNintendoLight;

extern f32 g_Front007Reaction;
extern f32 g_Front007Health;
extern f32 g_Front007Accuracy;
extern f32 g_Front007Damage;


extern struct mission_folder_setup g_FrontMissions[];

extern s32 g_MpPlayerCount;
extern s32 g_MpChar[];
extern s32 g_MpStage;
extern s32 g_MpGameLength;
extern s32 g_MpAimSetting;
extern s32 g_MpScenario;
extern s32 g_MpCanPickStage;
extern s32 g_MpCanPickLength;
extern s32 g_MpCanPickChar;
extern s32 g_MpCanPickWeapons;
extern s32 g_MpCanPickHandicap;
extern s32 g_MpCanPickControls;
extern s32 g_MpCanPickAim;
extern s16 g_FrontTargetTimes[20][3];

extern s32 g_CastIndex;
extern u32 g_CastAnim;
extern u32 g_CastAnimCount;
extern struct Model *g_CastModel;
extern struct Model *g_CastWeaponModel;
extern u32 g_CastFull;

void frontSetMenu(MENU menu, s32 reload);
s32 frontGetPlayerCount(void);
void frontCoopResetPartnerOutfit(void);
s32 frontCoopPartnerIsBond(void);
s32 frontCoopGetPartnerCuff(s32 player1cuff);
u16 frontCoopGetPartnerHead(void);
u16 frontCoopGetPartnerBody(void);
void frontSetFullCast(bool doExtended);
MPSCENARIOS frontGetMpScenario(void);
f32 frontGetMpHandicap(int player);
f32 frontGetMpCharHeight(int player);
s32 frontGetMpTimeLimit(void);
s32 frontGetMpPointLimit(void);
void frontSetMpScenario(MPSCENARIOS scenarioid);
void frontApplyMpAimSettings(void);
void frontTick(void);
Gfx * frontRender(Gfx *DL);
void frontUnlockAllMpChars(void);
u8 frontGetMpCharGender(int player);
s32 frontFindMissionEntry(s32 arg0);

void frontInit(void);

#endif
