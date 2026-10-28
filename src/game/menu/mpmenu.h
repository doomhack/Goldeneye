#ifndef _MP_WATCH_H_
#define _MP_WATCH_H_
#include <ultra64.h>

typedef enum TEXTCOLORS {
    TEXTCOLORS_SIGNED_ENUM = -1,
    GREEN_NORMAL=0,
    GREEN_HIGHLIGHT,
    RED_NORMAL,
    RED_HIGHLIGHT,
    BLUE_NORMAL,
    BLUE_HIGHLIGHT
} TEXTCOLORS;

struct AwardMetrics {
	s32 num_shots;                    // 0x00
	s32 num_headshots;                // 0x04
	s32 num_kills;                    // 0x08
	s32 num_deaths;                   // 0x0c
	s32 num_suicides;                 // 0x10
	f32 ks_ratio;                     // 0x14
	f32 kd_ratio;                     // 0x18
	s32 damage_to_backside;           // 0x1c
	s32 time_other_players_on_screen; // 0x20
	f32 avg_km_per_hour;              // 0x24
	f32 body_armor_pickups;           // 0x28
	u32 awards;                       // 0x2c
	s32 longest_inning;               // 0x30
	s32 shortest_inning;              // 0x34
};

extern s32 g_MpStopPlay;
extern s32 g_MpGameOver;

void mpMenuUnpause(void);
Gfx *mpMenuRenderNumber(Gfx *gdl, s32 x, s32 y, s32 points, TEXTCOLORS text_color);

s32 mpIsPaused(void);

void mpMenuCalculateAwards(bool gameoverdelay);
s32 mpGetPlayerPoints(s32 playernum);
void mpMenuStopPlay(void);
Gfx * mpMenuRender(Gfx *gdl);
bool mpMenuCanPlayerAct(void);
s32 mpMenuShowGauges(void);

void mpMenuTick(void);

#endif
