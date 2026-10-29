#ifndef _MONITOR_H_
#define _MONITOR_H_

#include <ultra64.h>
#include <bondtypes.h>

/**
 * Monitor script commands. A script is an array of u32 words: each command is its number,
 * then 0 to 2 arguments. Positions and scales are in 1/1024ths of the image, durations in
 * ticks.
 */
enum MonitorCmd
{
    MONCMD_STOP_SCROLL,     /* stop both scrolls where they are */
    MONCMD_SCROLL_BY_X,     /* (distance, ticks) */
    MONCMD_SCROLL_BY_Y,     /* (distance, ticks) */
    MONCMD_SCROLL_TO_X,     /* (position, ticks) */
    MONCMD_SCROLL_TO_Y,     /* (position, ticks) */
    MONCMD_SCALE_X,         /* (scale, ticks) */
    MONCMD_SCALE_Y,         /* (scale, ticks) */
    MONCMD_SET_IMAGE,       /* (monitor image number, or an image pointer) */
    MONCMD_WAIT,            /* (ticks) */
    MONCMD_GOTO,            /* (script) */
    MONCMD_GOTO_CHANCE,     /* (script, chance out of 65536) */
    MONCMD_RESTART,         /* back to the start of the script */
    MONCMD_STOP,            /* stop here for good */
    MONCMD_FADE_TO,         /* (0xRRGGBBAA, ticks) */
    MONCMD_SET_ROTATION,    /* (angle, 65536 = a full turn) */
    MONCMD_ROTATE,          /* (angle per tick, 65536 = a full turn) */
};

/* The scripts numbered for setup files and AI scripts (tv_change_screen_bank) */
#define MONITOR_NUM_SCRIPTS 52
#define MONITOR_SCRIPT_BLACK 51 /* for broken screens */

extern MonitorRecord g_GunScreenTaser;

void monitorsReset(void);
void monitorInit(MonitorRecord *mon, s32 scriptNum);
void monitorSetScript(MonitorRecord *mon, s32 scriptNum);
void monitorSetImage(MonitorRecord *mon, struct sImageTableEntry *image);
Gfx *monitorTickAndDraw(Model *model, ModelNode *node, MonitorRecord *mon, Gfx *gdl, s32 zbufferMode, s32 blendMode);

#endif
