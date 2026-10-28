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
extern f32 cast_camera_dist_start;
extern f32 cast_camera_dist_end;
extern f32 cast_camera_angle;
extern f32 cast_camera_angle_start;
extern f32 cast_camera_angle_end;
extern f32 cast_camera_height;
extern f32 cast_camera_height_start;
extern f32 cast_camera_height_end;
extern struct coord3d cast_target_smoothed;
extern struct coord3d cast_target_accumulator;

extern bool cast_camera_reset;

extern struct coord3d cast_rootpos_smoothed;

extern struct coord3d cast_rootvel_smoothed;
extern struct coord3d cast_rootvel_accumulator;

extern f32 g_NintendoLogoSpin;
extern f32 g_NintendoLogoScale;
extern coord3d g_FrontFolderScreenPos[MAX_FOLDER_COUNT];

extern u8 g_CheatAvailable[];

extern u8 g_CheatActivated[];


extern s32 g_MpFavWeapon[4][2];
extern s32 g_MpCharCursor[4];

extern s32 g_MpCharPrevCursor[4];

extern s32 g_MpCharScroll[4];

extern bool g_MpCharChosen[];
extern s32 g_MpCharPortraitSize[];
extern s32 g_MpHandicap[];
extern char* g_FrontTabStartText;
extern char* g_FrontTabNextText;
extern char* g_FrontTabPrevText;
extern s32 g_MpStageCursor;
extern s32 g_MpScenarioCursor;
extern s32 g_FrontDifficultyHighlight;
extern s32 g_MpTeamsCursor;
extern s32 g_NewCheatUnlocked;
extern s32 g_Front007HlReaction;
extern s32 g_Front007HlHealth;
extern s32 g_Front007HlDamage;
extern s32 g_Front007HlAccuracy;


extern s32 g_MpHandicap[];
extern s32 g_MpControlStyle[];


extern s32 g_MpHlPlayers;
extern s32 g_MpHlScenario;
extern s32 g_MpHlStage;
extern s32 g_MpHlLength;
extern s32 g_MpHlChar;
extern s32 g_MpHlWeapons;
extern s32 g_MpHlHandicap;
extern s32 g_MpHlControls;
extern s32 g_MpHlAim;

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
extern bool g_FrontBondKilled;
extern s32 g_FrontFirstLegal;
extern s32 g_FrontFirstBoot;
extern s32 g_FrontSkipIntro;
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
s32 frontFindMissionEntry(s32 mission);

void frontInit(void);

#endif
