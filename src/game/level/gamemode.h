#ifndef _GAMEMODE_H_
#define _GAMEMODE_H_
#include <ultra64.h>

/**
 * Which kind of mission the players picked on the mode select page.
 * Multiplayer is not one of these: see gameIsMulti().
 */
typedef enum GameMode
{
    GAMEMODE_SOLO,      // one player, or a multiplayer match
    GAMEMODE_COOP,      // two players working together on a mission
    GAMEMODE_COUNTEROP  // player 2 plays a guard against Bond
} GameMode;

extern GameMode g_GameMode;

/** Co-op only (not counter-op). */
static inline s32 gameIsCoop(void)
{
    return g_GameMode == GAMEMODE_COOP;
}

static inline s32 gameIsCounterOp(void)
{
    return g_GameMode == GAMEMODE_COUNTEROP;
}

/**
 * Co-op or counter-op: two players in a solo mission. getPlayerCount() is
 * then 2, but it is not a multiplayer match.
 */
static inline s32 gameIsTwoPlayerMission(void)
{
    return gameIsCoop() || gameIsCounterOp();
}

s32 gameIsMulti(void);

#endif
