/**
 * Per-player mission stats shown on the end screen: shots fired, hits by
 * body part, kills, and so on (stats*).
 */
#include "assets/obseg/text/LgunE.h"
#include "game/menu/watchmenu.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/text/language.h"
#include "game/player/playerstats.h"


/**
 * Counts a hit on a body part (or object) by the current player.
 */
void statsAddHit(ITEM_IDS item, SHOT_REGISTER shot_register) {

    if (weaponHasFlag(item, WEAPONSTATBITFLAG_PLAYER_STAT_HIT)) {
        g_playerPerm->shot_count[shot_register] = g_playerPerm->shot_count[shot_register]+1;
    }
}


/**
 * Hits on one body part.
 */
s32 statsGetHits(SHOT_REGISTER shot_register)
{
  return g_playerPerm->shot_count[shot_register];
}


/**
 * Counts an innocent killed.
 */
void statsAddCivilianKill(void)
{
    g_playerPerm->killed_civilians++;
}


/**
 * Innocents killed.
 */
s32 statsGetCivilianKills(void)
{
    return g_playerPerm->killed_civilians;
}


/**
 * Counts a kill. In multiplayer it also shows the kill count and any
 * double/triple kill messages.
 */
void statsAddKill(void)
{
    char buffer[256];
    s32 time_since_kill;
    s32 recent_kill_count;
    s32 mission_time;

    g_playerPerm->kill_count += 1;
    g_CurrentPlayer->kills_this_life += 1;

    if (!isMpGame()) { return; }

    mission_time = lvGetMissionTime60();
    sprintf(buffer, "%s: %d\n", langGet(getStringID(LGUN, GUN_STR_DA_KILLCOUNT)), g_playerPerm->kill_count); // "kill count"

#if defined(VERSION_US)
    hudmsgBottomShow(buffer);
#elif defined(VERSION_EU)
    hudmsgBottomShowText(buffer);
#endif

    if (g_playerPerm->kill_count >= 2)
    {
        time_since_kill = mission_time - g_CurrentPlayer->last_kill_time[0];
        if (g_playerPerm->max_time_between_kills < time_since_kill)
        {
            g_playerPerm->max_time_between_kills = time_since_kill;
        }

        if (time_since_kill < g_playerPerm->min_time_between_kills)
        {
            g_playerPerm->min_time_between_kills = time_since_kill;
        }
    }

    recent_kill_count = 1;
    g_CurrentPlayer->last_kill_time[3] = g_CurrentPlayer->last_kill_time[2];
    g_CurrentPlayer->last_kill_time[2] = g_CurrentPlayer->last_kill_time[1];
    g_CurrentPlayer->last_kill_time[1] = g_CurrentPlayer->last_kill_time[0];
    g_CurrentPlayer->last_kill_time[0] = mission_time;

    // Count the earlier kills (up to three) within 2 seconds of this one
    if (g_CurrentPlayer->last_kill_time[1] != -1 && (g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[1]) < 0x78)
    {
        recent_kill_count++;
        if ((g_CurrentPlayer->last_kill_time[2] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[2]) < 0x78))
        {
            recent_kill_count++;
            if ((g_CurrentPlayer->last_kill_time[3] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[3]) < 0x78))
            {
                recent_kill_count++;
            }
        }
    }

    if (g_playerPerm->most_killed_one_time < recent_kill_count)
    {
        g_playerPerm->most_killed_one_time = recent_kill_count;
    }
}


/**
 * Kills so far.
 */
s32 statsGetKills(void) {
    return g_playerPerm->kill_count;
}


/**
 * Counts a kill of the Golden Gun holder (multiplayer).
 */
void statsAddGoldenGunKill(void){
    g_playerPerm->killed_gg_owner_count++;
}


/**
 * Counts a death, and in multiplayer shows how many times you have died.
 */
void statsAddDeath(void)
{
	char buffer[256];
    g_CurrentPlayer->deathcount = (s32) (g_CurrentPlayer->deathcount + 1);
    if (isMpGame())
    {
        if (g_CurrentPlayer->deathcount == 1)
        {
            sprintf(buffer, langGet(getStringID(LGUN, GUN_STR_DB_DIEDONCE_LF))); //died once
        }
        else
        {
            sprintf(buffer, "%s %d %s\n", langGet(getStringID(LGUN, GUN_STR_DC_DIED)), g_CurrentPlayer->deathcount, langGet(getStringID(LGUN, GUN_STR_DD_TIMES))); //died times
        }
#if defined(VERSION_EU)
		hudmsgBottomShowText(buffer);
#else
		hudmsgBottomShow(buffer);
#endif
    }
}


/**
 * Counts a suicide. In multiplayer it also shows a message.
 */
void statsAddSuicide(void) {
    char buffer[256];
    s32 time_diff;
    s32 recent_kill_count;
    s32 currentTime;

    g_CurrentPlayer->num_suicides += 1;
    if (isMpGame()) {

        currentTime = lvGetMissionTime60();

        sprintf(buffer, "%s: %d\n", langGet(getStringID(LGUN, GUN_STR_DE_SUICIDECOUNT)), g_CurrentPlayer->num_suicides); // "suicide count"

#if defined(VERSION_EU)
		hudmsgBottomShowText(buffer);
#else
		hudmsgBottomShow(buffer);
#endif

        if (g_playerPerm->kill_count >= 2) {
            time_diff = currentTime - g_CurrentPlayer->last_kill_time[0];
            if (g_playerPerm->max_time_between_kills < time_diff) {
                g_playerPerm->max_time_between_kills = time_diff;
            }
            if (time_diff < g_playerPerm->min_time_between_kills) {
                g_playerPerm->min_time_between_kills = time_diff;
            }
        }
        recent_kill_count = 1;
        g_CurrentPlayer->last_kill_time[3] = g_CurrentPlayer->last_kill_time[2];
        g_CurrentPlayer->last_kill_time[2] = g_CurrentPlayer->last_kill_time[1];
        g_CurrentPlayer->last_kill_time[1] = g_CurrentPlayer->last_kill_time[0];
        g_CurrentPlayer->last_kill_time[0] = currentTime;

        if ( g_CurrentPlayer->last_kill_time[1] != -1) {

            if ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[1]) < 0x78) {

                recent_kill_count += 1;

                if ((g_CurrentPlayer->last_kill_time[2] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[2]) < 0x78)) {

                    recent_kill_count += 1;

                    if ((g_CurrentPlayer->last_kill_time[3] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[3]) < 0x78)) {
                        recent_kill_count += 1;
                    }
                }
            }
        }

        if (g_playerPerm->most_killed_one_time < recent_kill_count) {
            g_playerPerm->most_killed_one_time = recent_kill_count;
        }
    }
}
