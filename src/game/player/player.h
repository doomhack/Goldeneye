#ifndef _PLAYER_H_
#define _PLAYER_H_
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player/bondview.h"

struct player_data {
    /* 0x0 */
    s32 shot_count[7];

    /* 0x1c */
    int kill_count;

    /* 0x20 */
    int killed_gg_owner_count;

    /* 0x24 */
    int kill_counts[MAX_PLAYER_COUNT];

    /* 0x34 */
    int time_other_players_on_screen;

    /* 0x38 */
    float distance_traveled;

    /* 0x3c */
    int damage_to_backside;

    /* 0x40 */
    float body_armor_pickups;

    /* 0x44 */
    int min_time_between_kills;

    /* 0x48 */
    int max_time_between_kills;

    /* 0x4c */
    int longest_inning;

    /* 0x50 */
    s32 shortest_inning;

    /* 0x54 */
    int most_killed_one_life;

    /* 0x58 */
    int most_killed_one_time;

    /* 0x5c */
    float handicap;

    /* 0x60 */
    int flag_counter;

    /* 0x64 */
    float player_perspective_height;

    /* 0x68 */
    u8 order_out_in_yolt;

    /* 0x69 */
    u8 have_token_or_goldengun;

    /* 0x6a */
    u8 autoaim;

    /* 0x6b */
    u8 sight;

    /* 0x6c */
    int killed_civilians;
};

extern struct player *g_playerPointers[4];

// extern struct player_data player1_player_data;
// extern struct player_data player2_player_data;
// extern struct player_data player3_player_data;
// extern struct player_data player4_player_data;

extern struct player_data g_playerPlayerData[4];

extern struct player *g_CurrentPlayer;
extern struct player_data *g_playerPerm;
extern s32 player_num;
extern s32 random_byte;
extern PLAYER_ID array_PLAYER_IDs[4];


void playersReset(void);
void playersInit(int playercount);
void playersShuffleOrder(void);
s32 playerGetShuffled(PLAYER_ID id);
void playerSetCurrent(s32 playernum);
s32 getPlayerCount(void);
s32 getViewCount(void);
s32 coopSplitSideBySide(void);
s32 viewIsHalfWidth(void);
s32 isMpGame(void);
s32 coopPartnerAlive(void);
s32 coopPartnerCanRevive(void);
#define COUNTEROP_BOND  0
#define COUNTEROP_GUARD 1
void coopNoteActivator(void);
extern s32 g_CoopLastActivator;
extern s32 g_CoopMode;
extern s32 g_CounterOpMode;
extern s32 g_CounterOpBondPad;
s32 coopPlayerPerson(s32 player);
s32 counterOpIsGuardPlayer(s32 player);
extern s32 g_CoopPlayersOverlap;
extern s32 g_CoopSoloView;
extern s32 g_CoopSoloViewIsCinema;
void playerSetScreenSize(u32 width, u32 height);
void playerSetViewportPos(u32 ulx, u32 uly);


s32 playerGetCurrentNum(void);
#ifdef __GNUC__
/* Inlined at call sites; player.c still provides the real function */
extern inline __attribute__((gnu_inline)) s32 playerGetCurrentNum(void) { return player_num; }
#endif
void playerSetFovY(f32 fovy);
s32 playerGetIndexForProp( PropRecord* prop);
void playerSetAspect(f32 aspect);
void playerInit(s32 player_num);
s32 playerGetShuffledIndex(s32 current_player_num);
void playerBodyRemoveGun(enum GUNHAND hand);
void playerBodyGiveGun(enum GUNHAND hand);

void playerInitHands(void);
void playersStoreFavouriteWeapons(void);

#endif
