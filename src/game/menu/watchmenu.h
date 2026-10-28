#ifndef _WATCH_H_
#define _WATCH_H_
#include <ultra64.h>
#include <bondconstants.h>

#ifdef VERSION_EU
#define XOFFSET_1 65
#define YOFFSET_1 90
#define YOFFSET_WEAPTEXT 193
#define YOFFSET_ACTIONTEXT 172
#define YOFFSET_4 235
#define YOFFSET_5 214
#define YOFFSET_MISSIONSTATUS 0x43
#define YOFFSET_7 0x33
#define YOFFSET_8 0x26
#define YOFFSET_9 0x41
#define YINC 17
#define YINC_OPTIONS 12
#define WATCHZOOM1 4.80000019073f
#define WATCHZOOM2 6.09999990463f
#define WATCHZOOM3 4.15000009537f
#else
#define XOFFSET_1 64
#define YOFFSET_1 80
#define YINC 15
#define YINC_OPTIONS 11
#define YOFFSET_WEAPTEXT 167
#define YOFFSET_ACTIONTEXT 149
#define YOFFSET_4 203
#define YOFFSET_5 185
#define YOFFSET_MISSIONSTATUS 0x41
#define YOFFSET_7 0x31
#define YOFFSET_8 0x25
#define YOFFSET_9 0x3B
#define WATCHZOOM1 4.6f
#define WATCHZOOM2 5.9f
#define WATCHZOOM3 3.95f
#endif

/**
 * A static buffer for watch menu current screen rectangles
 * is added to the player struct. This is a compile time
 * const to know the size of the buffer.
 * Do not change this value until player struct is fully shiftable.
*/
#define WATCH_NUMBER_SCREENS 5

/**
 * Watch menu screen select menu UI layout should satisfy the following.
 * Width is the width of each rectangle, and spacer is the space
 * between the end of one rectangle and start of the next.
 * For "n" screens:
 * 
 *     (n * width) + ((n - 1) * spacer) = 600
 * 
 * Assume that all "spacer" space should add up to one rectangle. Then
 * 
 *     (n+1) * width = 600
 *     (n-1) * spacer = width
*/

#define WATCH_SCREEN_SELECT_RECTANGLE_MIN_X   (-299)
#define WATCH_SCREEN_SELECT_RECTANGLE_MAX_X   (301)
// default: 600
#define WATCH_SCREEN_SELECT_TOTAL_WIDTH       (WATCH_SCREEN_SELECT_RECTANGLE_MAX_X - WATCH_SCREEN_SELECT_RECTANGLE_MIN_X)
// default: 100
#define WATCH_SCREEN_SELECT_RECTANGLE_WIDTH   (s32)(WATCH_SCREEN_SELECT_TOTAL_WIDTH / (WATCH_NUMBER_SCREENS + 1))
// default: 25
#define WATCH_SCREEN_SELECT_SPACER_WIDTH      (s32)(WATCH_SCREEN_SELECT_RECTANGLE_WIDTH / (WATCH_NUMBER_SCREENS - 1))

/**
 * Horizontal spacing between watch menu screen select rectangles.
 * Default = 125.
*/
#define WATCH_SCREEN_SELECT_RECTANGLE_HSTEP (WATCH_SCREEN_SELECT_RECTANGLE_WIDTH + WATCH_SCREEN_SELECT_SPACER_WIDTH)

typedef enum WATCH_INDEX {
    WATCH_INDEX_SIGNED_ENUM = -1,
    WATCH_INDEX_MISSION_STATUS = 0,
    WATCH_INDEX_INVENTORY,
    WATCH_INDEX_CONTROL_OPTIONS,
    WATCH_INDEX_GAME_OPTIONS,
    WATCH_INDEX_MISSION_BRIEFING
} WATCH_INDEX;

typedef enum WATCH_CONTROLLER_OPTIONS_INDEX {
    WATCH_CONTROLLER_OPTIONS_INDEX_SIGNED_ENUM = -1,
    CONTROLLER_OPTIONS_INDEX_STYLE = 0,
    CONTROLLER_OPTIONS_INDEX_INPUTS
} WATCH_CONTROLLER_OPTIONS_INDEX;

typedef enum WATCH_GAME_OPTIONS_INDEX {
    WATCH_GAME_OPTIONS_INDEX_SIGNED_ENUM = -1,
    GAME_OPTIONS_INDEX_MUSIC = 0,
    GAME_OPTIONS_INDEX_FX,
    GAME_OPTIONS_INDEX_LOOK_UPDOWN,
    GAME_OPTIONS_INDEX_AUTO_AIM,
    GAME_OPTIONS_INDEX_AIM_CONTROL,
    GAME_OPTIONS_INDEX_SIGHT_ONSCREEN,
    GAME_OPTIONS_INDEX_LOOK_AHEAD,
    GAME_OPTIONS_INDEX_AMMO_ONSCREEN,
    GAME_OPTIONS_INDEX_SCREEN_SIZE,
    GAME_OPTIONS_INDEX_RATIO,
    GAME_OPTIONS_INDEX_HIRES,
    GAME_OPTIONS_INDEX_SPLIT,
    GAME_OPTIONS_INDEX_FPS,
    GAME_OPTIONS_INDEX_DRAWDIST,
    GAME_OPTIONS_INDEX_COUNT
} WATCH_GAME_OPTIONS_INDEX;

typedef enum WATCH_BRIEF_INDEX {
    WATCH_BRIEF_INDEX_SIGNED_ENUM = -1,
    BRIEF_INDEX_BACKGROUND = 0,
    BRIEF_INDEX_M,
    BRIEF_INDEX_Q,
    BRIEF_INDEX_MONEYPENNY,
    BRIEF_INDEX_OBJECTIVES
} WATCH_BRIEF_INDEX;

struct game_options {
    u16 text[4];
    u32 current_value;
};

extern struct game_options g_Options[];

void watchMenuResetControlRow(void);

void watchMenuResetOptionsRow(void);

void watchMenuResetAbort(void);

f32 watchMenuWrapAngle(f32 angle);

f32 watchMenuApproach(f32 value, f32 target, f32 divisor);

SCREEN_RATIO_OPTION optionsGetScreenRatio(void);

void optionsSetScreenRatio(SCREEN_RATIO_OPTION ratio_option);
u32 optionsGetHiRes(void);
void optionsSetHiRes(u32 enabled);
u32 optionsGetSplitStacked(void);
void optionsSetSplitStacked(u32 stacked);
u32 optionsGetShowFps(void);
void optionsSetShowFps(u32 enabled);
u32 optionsGetDrawNear(void);
void optionsSetDrawNear(u32 near);
void optionsCoopUsePlayer(s32 player);
void optionsCoopCopyFromPlayer1(void);

s32 optionsGetAutoAim(void);
u32 optionsGetLookAhead(void);
void optionsSetAutoAim(u32 value);
int optionsGetControlStyle(void);
u32 optionsGetSightOnScreen(void);
u32 optionsGetAmmoOnScreen(void);
u32 optionsGetAimControl(void);
u32 optionsGetScreenSize(void);
u16 optionsGetFxVolume(void);
u32 optionsGetLookInverted(void);
u16 optionsGetMusicVolume(void);
void optionsSetMusicVolume(u16 volume);
void optionsSetFxVolume(u16 volume);
void optionsSetControlStyle(int type);

// Do not declare a public prototype for this method. lvStageLoad is expecting
// this to be defined with no arguments, but the actual method is defined with
// one argument.

Gfx *watchMenuRender(Gfx *gdl, Mtx *mtx, s32 watch_transitioning);
void watchMenuOpen(void);

void watchMenuReset(void);
int textCountLines(char *arg);
void watchMenuTick(void);
void optionsSetAimControl(u32 value);
void optionsSetAmmoOnScreen(u32 value);
void optionsSetLookAhead(u32 value);
void optionsSetLookInverted(u32 value);
void optionsSetScreenSize(u32 value);
void optionsSetSightOnScreen(u32 value);

#endif


