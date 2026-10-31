/**
 * Co-op and counter-op state shared by the player code: the mode flags,
 * whose controller and settings each player uses, whether a partner can
 * revive the current player, the last player to use an object, and the
 * split-screen layout. See docs/subsystems/player.md.
 */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/menu/watchmenu.h"
#include "game/player/player.h"
#include "game/player/coop.h"

/**
 * Co-op: two players share a solo mission. getPlayerCount() then returns 2,
 * so code that means "is this a deathmatch" must use isMpGame() instead.
 */
s32 g_CoopMode = FALSE;

/**
 * Counter-op: player 2 plays a guard against Bond in a solo mission, as in
 * Perfect Dark. g_CoopMode is set too, as the two modes share everything
 * about two players in a solo mission. Bond is always player 1 (index 0);
 * g_CounterOpBondPad says whose controller and settings that is.
 */
s32 g_CounterOpMode = FALSE;
s32 g_CounterOpBondPad = 0;

/**
 * Whose controller and settings a player uses: 0 is controller 1 and the
 * save folder's options, 1 is controller 2 and player 2's options. These
 * only differ from the player index in counter-op when controller 2 plays
 * Bond.
 */
s32 coopPlayerPerson(s32 player)
{
    if (g_CounterOpMode && (g_CounterOpBondPad != 0) && (player < 2))
    {
        return player ^ 1;
    }

    return player;
}

/**
 * Counter-op: TRUE for the player who plays the guards' side.
 */
s32 counterOpIsGuardPlayer(s32 player)
{
    return g_CounterOpMode && (player == COUNTEROP_GUARD);
}

/**
 * Co-op: set when player 2 had to spawn inside player 1. The two players
 * then don't collide with each other until they have moved apart.
 */
s32 g_CoopPlayersOverlap = FALSE;

/**
 * Co-op: the player whose view fills the screen on its own this frame, or
 * -1 while every player's view is shown (see coopUpdateSoloView).
 * g_CoopSoloViewIsCinema is set when that is a cutscene, intro or final
 * death replay rather than a player's watch.
 */
s32 g_CoopSoloView = -1;
s32 g_CoopSoloViewIsCinema = FALSE;

/**
 * Co-op: a dead player respawns with health taken from their partner, so the
 * partner needs at least this much health and armour together.
 */
#define COOP_REVIVE_MIN_HEALTH 0.2f

/**
 * Co-op: TRUE if a living partner has enough health (armour counts as
 * health) to share with the current, dead player.
 */
s32 coopPartnerCanRevive(void)
{
    s32 count = getPlayerCount();
    s32 cur = playerGetCurrentNum();
    s32 i;

    if (!g_CoopMode)
    {
        return FALSE;
    }

    // Counter-op: the guard player comes back in a new guard while Bond is
    // alive. Bond has one life.
    if (g_CounterOpMode)
    {
        return counterOpIsGuardPlayer(cur) && !g_playerPointers[COUNTEROP_BOND]->bonddead;
    }

    for (i = 0; i < count; i++)
    {
        if ((i != cur) && !g_playerPointers[i]->bonddead
            && ((g_playerPointers[i]->bondhealth + g_playerPointers[i]->bondarmour) >= COOP_REVIVE_MIN_HEALTH))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Co-op: the player who last used an object (a switch, a door, a computer,
 * a gadget on something). Level scripts react to the object a frame later,
 * and their messages go to this player.
 */
s32 g_CoopLastActivator = 0;

void coopNoteActivator(void)
{
    g_CoopLastActivator = playerGetCurrentNum();
}

/**
 * Co-op: TRUE if a player other than the current one is alive. While one
 * is, a player's death doesn't end the mission.
 */
s32 coopPartnerAlive(void)
{
    s32 count = getPlayerCount();
    s32 cur = playerGetCurrentNum();
    s32 i;

    if (!g_CoopMode)
    {
        return FALSE;
    }

    // Counter-op: Bond's death ends the mission whoever is left.
    if (g_CounterOpMode)
    {
        return counterOpIsGuardPlayer(cur) && !g_playerPointers[COUNTEROP_BOND]->bonddead;
    }

    for (i = 0; i < count; i++)
    {
        if ((i != cur) && !g_playerPointers[i]->bonddead)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Two views (co-op or 2 player multiplayer) shown left and right rather
 * than stacked.
 */
s32 coopSplitSideBySide(void)
{
    return (getViewCount() == 2) && !optionsGetSplitStacked();
}
