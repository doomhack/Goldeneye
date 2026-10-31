/**
 * The game mode: solo, co-op or counter-op mission, or a multiplayer match.
 * See docs/subsystems/player.md.
 */
#include <ultra64.h>
#include "game/level/gamemode.h"
#include "game/player/player.h"

/** Set by the mode select page (frontModeSelectTick). */
GameMode g_GameMode = GAMEMODE_SOLO;

/**
 * Whether a multiplayer match is running. This goes by the player count, not
 * the mode select page, because the menus run on the title stage with no
 * players.
 */
s32 gameIsMulti(void)
{
    return !gameIsTwoPlayerMission() && getPlayerCount() >= 2;
}
