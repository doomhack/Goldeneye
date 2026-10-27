/**
 * The multiplayer pause menu: the per-player screen opened with Start in a multiplayer game
 * (pause, kills, losses, awards, exit), the end-of-game awards, and the multiplayer pause
 * and game-over state (g_MpPaused, g_MpStopPlay, g_MpGameOver). See docs/subsystems/menu.md.
 */
#include <ultra64.h>
#include <music.h>
#include <bondgame.h>
#include "game/player/bondinv.h"
#include <bondconstants.h>
#include <boss.h>
#include <joy.h>
#include <random.h>
#include "game/text/textrelated.h"
#include "game/level/frametiming.h"
#include "game/player/player.h"
#include "game/menu/mpmenu.h"
#include "game/player/gun.h"
#include "game/menu/front.h"
#include "game/level/lv.h"
#include "game/text/language.h"
#include "game/audio/mp_music.h"
#include "game/save/file.h"
#include "assets/obseg/text/LmpmenuE.h"

#ifdef REFRESH_PAL
#define MPMENU_YOFF 8   /* PAL: every text row sits 8 pixels lower */
#else
#define MPMENU_YOFF 0
#endif

// bss
s32 g_MpStopPlay;

/**
 * g_MpGameOver has two meanings. One is the standard true/false for whether the game is over.
 * The other is as a timer. Values >= 2 mean a countdown is still running.
 */
s32 g_MpGameOver;

s32 g_MpMenuChevronGlow;
s32 g_MpMenuChevronTimer;
s32 g_MpMenuAltGameOverMsg;
s32 g_MpMenuAltGameOverTimer;
s32 g_MpPaused;
s32 g_MpPausedBy;

// data
u16 g_AwardNames[] = {
    getStringID(LMPMENU, MPMENU_STR_00_LEMMINGAWARD),getStringID(LMPMENU, MPMENU_STR_01_WHERESTHEAMMO),getStringID(LMPMENU, MPMENU_STR_02_WHERESTHEARMOR),getStringID(LMPMENU, MPMENU_STR_03_AC10AWARD),getStringID(LMPMENU, MPMENU_STR_04_MARKSMANSHIPAWARD),getStringID(LMPMENU, MPMENU_STR_05_MOSTPROFESSIONAL),
    getStringID(LMPMENU, MPMENU_STR_06_MOSTDEADLY),getStringID(LMPMENU, MPMENU_STR_07_MOSTLYHARMLESS),getStringID(LMPMENU, MPMENU_STR_08_MOSTCOWARD),getStringID(LMPMENU, MPMENU_STR_09_MOSTFRANTIC),getStringID(LMPMENU, MPMENU_STR_0A_MOSTHONORABLE),getStringID(LMPMENU, MPMENU_STR_0B_MOSTDISHONORABLE),
    getStringID(LMPMENU, MPMENU_STR_0C_SHORTESTINNINGS),getStringID(LMPMENU, MPMENU_STR_0D_LONGESTINNINGS),getStringID(LMPMENU, MPMENU_STR_0E_DOUBLEKILL),getStringID(LMPMENU, MPMENU_STR_0F_TRIPLEKILL),getStringID(LMPMENU, MPMENU_STR_10_QUADKILL)
};


s32 mpMenuCanGoRight(void) 
{
    switch(g_CurrentPlayer->mpmenumode)
    {
        case MENU_GOWOC:
        case MENU_LOSSES:
        case MENU_KILLS:
        case MENU_PAUSE:
            return 1;
        case MENU_EXIT:
        case MENU_EXIT_CONFIRM:
        case MENU_FINISHED:
            return 0;
        case MENU_SCORES:
            return g_MpGameOver ? 0 : 1;
        default:
            return 0;
    }
}


s32 mpMenuCanGoLeft(void) 
{
    switch(g_CurrentPlayer->mpmenumode)
    {
        case MENU_KILLS:
        case MENU_SCORES:
        case MENU_PAUSE:
        case MENU_EXIT:
            return 1;
        case MENU_GOWOC:
        case MENU_EXIT_CONFIRM:
        case MENU_FINISHED:
            return 0;
        case MENU_LOSSES:
            return g_MpGameOver ? 1 : 0;
        default:
#ifdef DEBUG
            // kill the process
            assert(1 == 0); // mpmenu.c, line87
#endif
        return 0;
    }
}


s32 mpMenuPressedRight(s32 player)
{
    s32 iVar3 = joyGetStickXInRange(player, -2, 1);

    if ((joyGetButtonsPressedThisFrame(player, R_JPAD|R_CBUTTONS)) || ((iVar3 >= 1  && (g_CurrentPlayer->mpjoywascentre)))) 
    {
        return 1;
    }

    return 0;
}

s32 mpMenuPressedLeft(s32 player)
{
    s32 iVar3 = joyGetStickXInRange(player, -2, 1);

    if ((joyGetButtonsPressedThisFrame(player, L_JPAD|L_CBUTTONS)) || ((iVar3 < -1 && (g_CurrentPlayer->mpjoywascentre)))) 
    {
        return 1;
    }

    return 0;
}


void mpMenuBeep(void)
{
    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
}


void mpMenuUnpause(void)
{
    g_MpStopPlay = 0;
    g_MpGameOver = 0;
    g_MpPaused = 0;
}


/**
 * Returns the index (0-3) of the player with the highest value.
 */
s32 mpMenuFindMaxInt(s32 numplayers, s32 value0, s32 value1, s32 value2, s32 value3)
{
    s32 aux;
    s32 result;
 
    if ((value0 < value1) || ((value1 == value0 && ((randomGetNext() & 1))))) 
    {
        result = 1;
        aux = value1;
    }
    else 
    {
        result = 0;
        aux = value0;
    }
 
    if (numplayers >= 3) 
    {
 
        if ((aux < value2) || ((value2 == aux && ((randomGetNext() & 1))))) 
        {
            result = 2;
            aux = value2;
        }
 
        if (numplayers >= 4) 
        {
            if ((aux < value3) || ((value3 == aux && ((randomGetNext() & 1))))) {
                result = 3;
            }
        }
    }
 
    return result;
}


/**
 * Returns the index (0-3) of the player with the lowest value.
 */
s32 mpMenuFindMinInt(s32 numplayers, s32 value0, s32 value1, s32 value2, s32 value3)
{
    s32 aux;
    s32 result;
 
    if ((value1 < value0) || ((value1 == value0 && ((randomGetNext() & 1))))) 
    {
        result = 1;
        aux = value1;
    }
    else 
    {
        result = 0;
        aux = value0;
    }
 
    if (numplayers >= 3) 
    {
        if ((value2 < aux) || ((value2 == aux && ((randomGetNext() & 1)))))
        {
            result = 2;
            aux = value2;
        }
 
        if (numplayers >= 4) 
        {
            if ((value3 < aux) || ((value3 == aux && ((randomGetNext() & 1))))) 
            {
                result = 3;
            }
        }
    }
 
    return result;
}


/**
 * Returns the index of the player with the highest value.
 * 
 * @bug: aux is s32 so each time it's set to one of the values, the decimal portion gets truncated.
 * The first comparison is safe from this since it doesn't use aux, but the rest are impacted.
 */
s32 mpMenuFindMaxFloat(s32 numplayers, f32 value0, f32 value1, f32 value2, f32 value3)
{
    s32 aux;
    s32 result;
 
    if ((value0 < value1) || ((value1 == value0 && ((randomGetNext() & 1))))) 
    {
        aux = (s32) value1;
        result = 1;
    }
    else 
    {
        aux = (s32) value0;
        result = 0;
    }
 
    if (numplayers >= 3)
    {
        if ((aux < value2) || ((value2 == aux && ((randomGetNext() & 1))))) 
        {
            aux = (s32) value2;
            result = 2;
        }
 
        if (numplayers >= 4)
        {
            if ((aux < value3) || ((value3 == aux && ((randomGetNext() & 1))))) 
            {
                result = 3;
            }
        }
    }
 
    return result;
}


/**
 * Returns the index of the player with the lowest value.
 * 
 * Also suffers from the same bug as the above function.
 */
s32 mpMenuFindMinFloat(s32 numplayers, f32 value0, f32 value1, f32 value2, f32 value3)
{
    s32 aux;
    s32 result;
 
    if ((value1 < value0) || ((value1 == value0 && ((randomGetNext() & 1))))) 
    {
        aux = (s32) value1;
        result = 1;
    }
    else 
    {
        aux = (s32) value0;
        result = 0;
    }
 
    if (numplayers >= 3)
    {
        if ((value2 < aux) || ((value2 == aux && ((randomGetNext() & 1))))) 
        {
            aux = (s32) value2;
            result = 2;
        }
 
        if (numplayers >= 4)
        {
            if ((value3 < aux) || ((value3 == aux && ((randomGetNext() & 1))))) 
            {
                result = 3;
            }
        }
    }
 
    return result;
}


void mpMenuPause(void) 
{
    lvSetControlsLocked(1);
    g_MpPaused = TRUE;
}


bool mpMenuCanPlayerAct(void)
{
    if (!isMpGame())
    {
        return TRUE;
    }

    if (g_MpStopPlay)
    {
        return FALSE;
    }

    if (g_CurrentPlayer->mpmenuon)
    {
        return FALSE;
    }

    return TRUE;
}


void mpMenuStopPlay(void)
{
    g_MpStopPlay = TRUE;
}



void mpMenuCalculateAwards(bool gameoverdelay)
{
    s32 player_count;
    s32 i;
    s32 j;
    s32 weapon_choice_2;
    s32 weapon_choice_1;
    s32 prev_player_num;
    s32 duration;

    struct AwardMetrics metrics[4];

    player_count = getPlayerCount();
    duration = lvGetMissionTime60();

    sndDeactivateAllSfxByFlag_1();
    levelMusicSetState(MUSICSTATE_SILENT);

    musicTrack1ApplySeqpVol(levelMusicGetVolume());
    g_musicXTrack1Fade = 0;
    musicTrack1Play(M_INTROSWOOSH);

    mpMenuPause();

    if (gameoverdelay != 0)
    {
        g_MpGameOver = (PAL ? 250 : 300);
    }
    else
    {
        g_MpGameOver = 1;
    }

    g_MpMenuAltGameOverMsg = 1;

    // Possible copy/paste error: these are the timer values for the chevron glow
    g_MpMenuAltGameOverTimer =  (PAL ? 16 : 20);

    g_MpMenuChevronGlow = 0;
    g_MpMenuChevronTimer = 0;

    prev_player_num = playerGetCurrentNum();

    for (i = 0; i < player_count; i++)
    {

        playerSetCurrent(i);

        g_CurrentPlayer->mpmenuon = TRUE;
        g_CurrentPlayer->mpmenumode = MENU_SCORES;
        g_CurrentPlayer->ptr_text_first_mp_award = 0;
        g_CurrentPlayer->ptr_text_second_mp_award = 0;

        invGetWeaponOfChoice(&weapon_choice_1, &weapon_choice_2);
        fileSetFavWeapon((u32) weapon_choice_1, (u32) weapon_choice_2);

        metrics[i].num_shots = statsGetHits(SHOT_REGISTER_TOTAL);
        metrics[i].num_headshots = statsGetHits(SHOT_REGISTER_HEAD);
        metrics[i].num_kills = 0;
        metrics[i].num_deaths = 0;
        metrics[i].num_suicides = 0;

        for (j = 0; j < frontGetPlayerCount(); j++)
        {
            metrics[i].num_deaths += g_playerPlayerData[j].kill_counts[i];
            if (i == j)
            {
                metrics[i].num_suicides += g_playerPlayerData[i].kill_counts[j];
            }
            else
            {
                metrics[i].num_kills += g_playerPlayerData[i].kill_counts[j];
            }
        }

        metrics[i].ks_ratio = metrics[i].num_kills * 100.0f / (metrics[i].num_shots + 1.0f);
        metrics[i].kd_ratio = metrics[i].num_kills * 100.0f / (metrics[i].num_deaths + 1.0f);
        metrics[i].damage_to_backside = g_playerPlayerData[i].damage_to_backside;
        metrics[i].time_other_players_on_screen = g_playerPlayerData[i].time_other_players_on_screen;
        metrics[i].avg_km_per_hour = g_playerPlayerData[i].distance_traveled / 100000.0f / ((duration + 1) / (3600.0f * 60.0f));
        metrics[i].body_armor_pickups = g_playerPlayerData[i].body_armor_pickups;
        metrics[i].awards = 0;
        metrics[i].longest_inning = g_playerPlayerData[i].longest_inning;
        metrics[i].shortest_inning = g_playerPlayerData[i].shortest_inning;
    }

    playerSetCurrent(prev_player_num);

    // Choose which players are eligible for which awards
    i = mpMenuFindMaxInt(player_count, metrics[0].num_suicides, metrics[1].num_suicides, metrics[2].num_suicides, metrics[3].num_suicides);

    if (metrics[i].num_suicides > 0)
    {
        metrics[i].awards |= AWARD_MOSTSUICIDAL;
    }

    i = mpMenuFindMinInt(player_count, metrics[0].num_shots, metrics[1].num_shots, metrics[2].num_shots, metrics[3].num_shots);

    if (metrics[i].num_shots < 100)
    {
        metrics[i].awards |= AWARD_WHONEEDSAMMO;
    }

    i = mpMenuFindMinFloat(player_count, metrics[0].body_armor_pickups, metrics[1].body_armor_pickups, metrics[2].body_armor_pickups, metrics[3].body_armor_pickups);

    if (metrics[i].body_armor_pickups <= 2.0f)
    {
        metrics[i].awards |= AWARD_WHERESTHEARMOUR;
    }

    i = mpMenuFindMaxFloat(player_count, metrics[0].body_armor_pickups, metrics[1].body_armor_pickups, metrics[2].body_armor_pickups, metrics[3].body_armor_pickups);

    if (metrics[i].body_armor_pickups > 6.0f)
    {
        metrics[i].awards |= AWARD_ACNEGATIVE10;
    }

    i = mpMenuFindMaxInt(player_count, metrics[0].num_headshots, metrics[1].num_headshots, metrics[2].num_headshots, metrics[3].num_headshots);

    if (metrics[i].num_headshots > 0)
    {
        metrics[i].awards |= AWARD_MARKSMANSHIP;
    }

    i = mpMenuFindMaxFloat(player_count, metrics[0].ks_ratio, metrics[1].ks_ratio, metrics[2].ks_ratio, metrics[3].ks_ratio);

    if (metrics[i].ks_ratio > 0.0f)
    {
        metrics[i].awards |= AWARD_MOSTPROFESSIONAL;
    }

    i = mpMenuFindMaxFloat(player_count, metrics[0].kd_ratio, metrics[1].kd_ratio, metrics[2].kd_ratio, metrics[3].kd_ratio);

    if (metrics[i].kd_ratio > 0.0f)
    {
        metrics[i].awards |= AWARD_MOSTDEADLY;
    }

    i = mpMenuFindMinFloat(player_count, metrics[0].kd_ratio, metrics[1].kd_ratio, metrics[2].kd_ratio, metrics[3].kd_ratio);
    metrics[i].awards |= AWARD_MOSTHARMLESS;

    i = mpMenuFindMinInt(player_count, metrics[0].time_other_players_on_screen, metrics[1].time_other_players_on_screen, metrics[2].time_other_players_on_screen, metrics[3].time_other_players_on_screen);
    metrics[i].awards |= AWARD_MOSTCOWARDLY;

    i = mpMenuFindMaxFloat(player_count, metrics[0].avg_km_per_hour, metrics[1].avg_km_per_hour, metrics[2].avg_km_per_hour, metrics[3].avg_km_per_hour);

    if (metrics[i].avg_km_per_hour > 10.0f)
    {
        metrics[i].awards |= AWARD_MOSTFRANTIC;
    }

    i = mpMenuFindMinInt(player_count, metrics[0].damage_to_backside, metrics[1].damage_to_backside, metrics[2].damage_to_backside, metrics[3].damage_to_backside);
    metrics[i].awards |= AWARD_MOSTHONORABLE;

    i = mpMenuFindMaxInt(player_count, metrics[0].damage_to_backside, metrics[1].damage_to_backside, metrics[2].damage_to_backside, metrics[3].damage_to_backside);

    if (metrics[i].damage_to_backside > 0 && (metrics[i].awards & AWARD_MOSTHONORABLE) == 0)
    {
        metrics[i].awards |= AWARD_MOSTDISHONORABLE;
    }

    i = mpMenuFindMaxInt(player_count, metrics[0].longest_inning, metrics[1].longest_inning, metrics[2].longest_inning, metrics[3].longest_inning);

    if (metrics[i].longest_inning > 0)
    {
        metrics[i].awards |= AWARD_LONGESTINNINGS;
    }

    i = mpMenuFindMinInt(player_count, metrics[0].shortest_inning, metrics[1].shortest_inning, metrics[2].shortest_inning, metrics[3].shortest_inning);

    if (metrics[i].shortest_inning > 0)
    {
        metrics[i].awards |= AWARD_SHORTESTINNINGS;
    }

    for (i = 0; i < player_count; i++)
    {
        if (g_playerPlayerData[i].most_killed_one_time == 4)
        {
            metrics[i].awards |= AWARD_QUADKILL;
        }
        else if (g_playerPlayerData[i].most_killed_one_time == 3)
        {
            metrics[i].awards |= AWARD_TRIPLEKILL;
        }
        else if (g_playerPlayerData[i].most_killed_one_time == 2)
        {
            metrics[i].awards |= AWARD_DOUBLEKILL;
        }
    }

    // For each player, choose which two awards to actually give them.
    // Note that the first award checked is quad kill, but after that the awards
    // are checked randomly. So if a player has quad kill they'll definitely see
    // it on the endscreen, but this is not the case for triple kill or any
    // other awards.
    for (i = 0; i < player_count; i++)
    {
        s32 numdone = 0;
        s32 awardindex = 16;

        while (numdone == 0)
        {
            if (metrics[i].awards & (1 << awardindex))
            {
                metrics[i].awards &= ~(1 << awardindex);
                g_playerPointers[i]->ptr_text_first_mp_award = langGet(g_AwardNames[awardindex]);
                numdone = 1;
            }

            if (metrics[i].awards == 0)
            {
                numdone = 1;
            }

            awardindex = randomGetNext() % 17;
        }

        while (numdone < 2)
        {
            awardindex = randomGetNext() % 17;

            if (metrics[i].awards & (1 << awardindex))
            {
                metrics[i].awards &= ~(1 << awardindex);
                g_playerPointers[i]->ptr_text_second_mp_award = langGet(g_AwardNames[awardindex]);
                numdone = 2;
            }

            if (metrics[i].awards == 0)
            {
                numdone = 2;
            }
        }
    }
}


void mpMenuTick(void)
{
    s32 player_num;
    s32 player_count;
    s32 x_centered;
    s32 menu_count;
    s32 i;

    player_num = playerGetCurrentNum();
    player_count = getPlayerCount();
    x_centered = joyGetStickXInRange(player_num, -2, 1);

    // The player in shuffled position 0 drives down g_MpGameOver which is both a flag and a timer.
    if (!playerGetShuffledIndex(player_num) && (g_MpGameOver >= 2))
    {
        g_MpGameOver -= g_FrameDelta;

        if (g_MpGameOver <= 0) 
        { 
            g_MpGameOver = 1;
        }
    }

    if (isMpGame())
    {
        // If a player has their pause menu up when they die and the game isn't over, turn their menu off. 
        if ((g_CurrentPlayer->bonddead) && (!g_MpGameOver))
        {
            g_CurrentPlayer->mpmenuon = FALSE;
            g_CurrentPlayer->healthdisplaytime = 0;
            return;
        }

        if (g_MpGameOver < 2)
        {
            if (playerGetShuffledIndex(player_num) == 0)
            {
                g_MpMenuChevronTimer += g_FrameDelta;
                g_MpMenuAltGameOverTimer += g_FrameDelta;

                // Toggle chevron glow
                if (g_MpMenuChevronTimer >= (PAL ? 16 : 20))
                {
                    g_MpMenuChevronTimer -= (PAL ? 16 : 20);
                    g_MpMenuChevronGlow = !g_MpMenuChevronGlow;
                }

                // Flip between "GAME OVER" and "START TO EXIT" text.
                if (g_MpMenuAltGameOverTimer >= (PAL ? 100 : 120))
                {
                    g_MpMenuAltGameOverTimer -= (PAL ? 100: 120);
                    g_MpMenuAltGameOverMsg = !g_MpMenuAltGameOverMsg;
                }
            }

            if (g_playerPerm->most_killed_one_life < g_CurrentPlayer->kills_this_life)
            {
                g_playerPerm->most_killed_one_life = g_CurrentPlayer->kills_this_life;
            }

            if (g_playerPerm->longest_inning < (lvGetMissionTime60() - g_CurrentPlayer->lifestarttime60))
            {
                g_playerPerm->longest_inning = lvGetMissionTime60() - g_CurrentPlayer->lifestarttime60;
            }

            if (g_CurrentPlayer->mpmenuon != FALSE)
            {
                if (mpMenuPressedRight(player_num) && mpMenuCanGoRight())
                {
                    mpMenuBeep();
                    g_CurrentPlayer->mpmenumode++;
                }
                else if (mpMenuPressedLeft(player_num) && mpMenuCanGoLeft())
                {
                    mpMenuBeep();
                    g_CurrentPlayer->mpmenumode--;
                }
                else if (mpMenuPressedRight(player_num) && (g_CurrentPlayer->mpmenumode == MENU_EXIT_CONFIRM))
                {
                    mpMenuBeep();
                    g_CurrentPlayer->mpquitconfirm = 1;
                }
                else if (mpMenuPressedLeft(player_num) && (g_CurrentPlayer->mpmenumode == MENU_EXIT_CONFIRM))
                {
                    mpMenuBeep();
                    g_CurrentPlayer->mpquitconfirm = 0;
                }
                else if (joyGetButtonsPressedThisFrame(player_num, A_BUTTON) && (g_CurrentPlayer->mpmenumode == MENU_PAUSE))
                {
                    mpMenuBeep();
                    if (!g_MpPaused)
                    {
                        g_MpPaused = 1;
                        g_MpPausedBy = playerGetCurrentNum();
                        lvSetControlsLocked(1);
                    }
                    else if (playerGetCurrentNum() == g_MpPausedBy)
                    {
                        g_MpPaused = 0;
                        lvSetControlsLocked(0);
                    }
                }
                else if (g_CurrentPlayer->mpmenumode == MENU_FINISHED)
                {
                    if (joyGetButtonsPressedThisFrame(player_num, B_BUTTON))
                    {
                        mpMenuBeep();
                        g_CurrentPlayer->mpmenuon = TRUE;
                        g_CurrentPlayer->mpmenumode = MENU_SCORES;
                    }
                }
                else if (((joyGetButtonsPressedThisFrame(player_num, A_BUTTON | START_BUTTON)) && ((((g_CurrentPlayer->mpmenumode != MENU_EXIT)) && (g_CurrentPlayer->mpmenumode != MENU_EXIT_CONFIRM)) || ((g_CurrentPlayer->mpmenumode == MENU_EXIT_CONFIRM) && (g_CurrentPlayer->mpquitconfirm != 1)))) || (joyGetButtonsPressedThisFrame(player_num, B_BUTTON)))
                {
                    mpMenuBeep();

                    if (g_MpGameOver)
                    {
                        menu_count = 0;
                        g_CurrentPlayer->mpmenumode = MENU_FINISHED;

                        for (i = 0; i < player_count; i++)
                        {
                            if (g_playerPointers[i]->mpmenumode == MENU_FINISHED)
                            {
                                menu_count++;
                            }
                        }

                        if (menu_count == player_count)
                        {
                            bossSetLoadedStage(LEVELID_TITLE);
                        }
                    }
                    else
                    {
                        g_CurrentPlayer->mpmenuon = FALSE;
                        g_CurrentPlayer->healthdisplaytime = (PAL ? 50 : 60);

                        if (playerGetCurrentNum() == g_MpPausedBy)
                        {
                            g_MpPaused = 0;
                            lvSetControlsLocked(0);
                        }
                    }
                }
                else if ((joyGetButtonsPressedThisFrame(player_num, A_BUTTON | START_BUTTON)) && (g_CurrentPlayer->mpmenumode == MENU_EXIT))
                {
                    mpMenuBeep();
                    g_CurrentPlayer->mpmenumode = MENU_EXIT_CONFIRM;
                    g_CurrentPlayer->mpquitconfirm = 0;
                }
                else if (joyGetButtonsPressedThisFrame(player_num, A_BUTTON | START_BUTTON))
                {
                    if ((g_CurrentPlayer->mpmenumode == MENU_EXIT_CONFIRM) && (g_CurrentPlayer->mpquitconfirm == 1))
                    {
                        mpMenuBeep();
                        g_CurrentPlayer->mpmenuon = FALSE;
                        g_CurrentPlayer->healthdisplaytime = 0;
                        mpMenuCalculateAwards(FALSE);
                    }
                }

                if ((x_centered == 0) || (x_centered == -1))
                {
                    g_CurrentPlayer->mpjoywascentre = 1;
                    return;
                }

                g_CurrentPlayer->mpjoywascentre = 0;
                return;
            }

            if (joyGetButtonsPressedThisFrame(player_num, START_BUTTON))
            {
                mpMenuBeep();
                g_CurrentPlayer->mpmenuon = TRUE;
                g_CurrentPlayer->mpmenumode = MENU_SCORES;
                g_CurrentPlayer->mpjoywascentre = 1;
                g_CurrentPlayer->apparenthealth = g_CurrentPlayer->bondhealth;
                g_CurrentPlayer->apparentarmour = g_CurrentPlayer->bondarmour;
            }
        }
    }
}


Gfx *mpMenuRenderNumber(Gfx *gdl, s32 x, s32 y, s32 points, TEXTCOLORS text_color) {

    s32 textX;
    s32 textY;
    s32 textwidth;
    s32 textheight;
    s32 unused;
    u16 *text;
    s16 viX;
    s32 viY;

    sprintf(&text, "%d", points);

    textMeasure(&textheight, &textwidth, &text, g_FontBankGothicChars, g_FontBankGothic, 0);

    textX = x - (textwidth >> 1);
    textY = y;

    switch (text_color) 
    {
        case GREEN_NORMAL:
            viX = viGetX();
            viY = viGetY();
            gdl = textRender(gdl, &textX, &textY, &text, g_FontBankGothicChars, g_FontBankGothic, 0xFF00B0, viX, viY, 0, 0);
            break;

        case GREEN_HIGHLIGHT:
            viX = viGetX();
            viY = viGetY();
            gdl = textRenderOutlined(gdl, &textX, &textY, &text, g_FontBankGothicChars, g_FontBankGothic, 0xA0FFA0F0, 0x7000A0, viX, viY, 0, 0);
            break;

        case RED_NORMAL:
            viX = viGetX();
            viY = viGetY();
            gdl = textRender(gdl, &textX, &textY, &text, g_FontBankGothicChars, g_FontBankGothic, 0xFF4040B0, viX, viY, 0, 0);
            break;

        case RED_HIGHLIGHT:
            viX = viGetX();
            viY = viGetY();
            gdl = textRenderOutlined(gdl, &textX, &textY, &text, g_FontBankGothicChars, g_FontBankGothic, 0xFFA0A0F0, 0x700000A0, viX, viY, 0, 0);
            break;

        case BLUE_NORMAL:
            viX = viGetX();
            viY = viGetY();
            gdl = textRender(gdl, &textX, &textY, &text, g_FontBankGothicChars, g_FontBankGothic, 0x4040FFB0, viX, viY, 0, 0);
            break;

        case BLUE_HIGHLIGHT:
            viX = viGetX();
            viY = viGetY();
            gdl = textRenderOutlined(gdl, &textX, &textY, &text, g_FontBankGothicChars, g_FontBankGothic, 0xA0A0FFF0, 0x70A0, viX, viY, 0, 0);
            break;
    }

    return gdl;
}


//rodata
/*8005BC20*/
const char ascii_MP_watch_menu_BLANK[] = "";
const char ascii_MP_watch_menu_left_chevron[] = "<";
const char ascii_MP_watch_menu_right_chevron[] = ">";
const char ascii_pnum_KILLS[] = "%s%d %s";
const char ascii_pnum_LOSSES[] = "%s%d %s";


s32 mpGetPlayerPoints(s32 playernum)
{
    s32 team_or_token;
    s32 player_count;
    s32 i;
    s32 j;
    s32 points;

    // The have_token_or_goldengun field is also treated as a team identifier.
    team_or_token = g_playerPlayerData[playernum].have_token_or_goldengun;

    player_count = getPlayerCount();

    points = 0;

    switch (frontGetMpScenario())
    {
        case SCENARIO_NORMAL:
        case SCENARIO_MWTGG:
        case SCENARIO_LTK:
            for (i = 0; i < player_count; i++)
            {
                if (i != playernum)
                {
                    points += g_playerPlayerData[playernum].kill_counts[i];
                }
                else
                {
                    points -= g_playerPlayerData[i].kill_counts[playernum];
                }
            }

            points += g_playerPlayerData[playernum].killed_gg_owner_count * (player_count - 2);
            break;

        case SCENARIO_YOLT:
            points = MAX_PLAYER_COUNT - g_playerPlayerData[playernum].order_out_in_yolt;
            break;

        case SCENARIO_TLD:
            points = g_playerPlayerData[playernum].flag_counter;
            break;
            
        case SCENARIO_2v2:
        case SCENARIO_3v1:
        case SCENARIO_2v1:
            for (i = 0; i < player_count; i++)
            {
                if (g_playerPlayerData[i].have_token_or_goldengun == team_or_token)
                {
                    for (j = 0; j < player_count; j++)
                    {
                        if (g_playerPlayerData[j].have_token_or_goldengun != team_or_token)
                        {
                            points += g_playerPlayerData[i].kill_counts[j];
                        }
                        else
                        {
                            points -= g_playerPlayerData[i].kill_counts[j];
                        }
                    }
                }
            }
            break;

        default:
            break;
    }

    return points;
}


void mpMenuFormatRank(char *buffer, s32 playernum)
{
    s32 scenario;
    s32 count;
    s32 scores[4];
    s32 players[4];
    s32 tmp;
    s32 i;
    s32 j;

    scenario = frontGetMpScenario();
    count = getPlayerCount();

    for (i = 0; i < count; i++)
    {
        scores[i] = mpGetPlayerPoints(i);
        players[i] = i;
    }

    for (j = 0; j < count; j++)
    {
        for (i = 0; i < (count - 1); i++)
        {
            if (scores[i] < scores[i + 1])
            {
                tmp = scores[i + 1];
                scores[i + 1] = scores[i];
                scores[i] = tmp;
                tmp = players[i + 1];
                players[i + 1] = players[i];
                players[i] = tmp;
            }
        }

    }

    for (i = 0; i < count; i++)
    {
        if (playernum == players[i])
        {
            break;
        }
    }

    for (j = 0; j <= i; j++)
    {
        if (scores[j] == scores[i])
        {
            break;
        }
    }

    switch (j)
    {
        case 0:
            sprintf(buffer, langGet(getStringID(LMPMENU, MPMENU_STR_11_RANK1ST))); /* Rank: 1st */
            break;
        case 1:
            sprintf(buffer, langGet(getStringID(LMPMENU, MPMENU_STR_12_RANK2ND))); /* Rank: 2nd */
            break;
        case 2:
            if ((scenario != SCENARIO_2v2) && (scenario != SCENARIO_2v1))
            {
                sprintf(buffer, langGet(getStringID(LMPMENU, MPMENU_STR_13_RANK3RD))); /* Rank: 3rd */
            }
            else
            {
                sprintf(buffer, langGet(getStringID(LMPMENU, MPMENU_STR_12_RANK2ND))); /* Rank: 2nd */
            }
            break;
        case 3:
            if (scenario != SCENARIO_3v1)
            {
                sprintf(buffer, langGet(getStringID(LMPMENU, MPMENU_STR_14_RANK4TH))); /* Rank: 4th */
            }
            else
            {
                sprintf(buffer, langGet(getStringID(LMPMENU, MPMENU_STR_12_RANK2ND))); /* Rank: 2nd */
            }
            break;
    }

}


s32 mpMenuShowRank(s32 param_1)
{
    switch(frontGetMpScenario())
    {
        case SCENARIO_NORMAL:
        case SCENARIO_TLD:
        case SCENARIO_MWTGG:
        case SCENARIO_LTK:
        case SCENARIO_2v2:
        case SCENARIO_3v1:
        case SCENARIO_2v1:
            return 1;
        case SCENARIO_YOLT:
            return param_1 ? 0 : 1;
        default:
#ifdef DEBUG
            osSyncPrintf("Invalid scenario %d!", frontGetMpScenario());
#endif
        do {} while (1);
    }
}

s32 mpMenuShowScore(s32 param_1)
{
    switch(frontGetMpScenario())
    {
        case SCENARIO_NORMAL:
        case SCENARIO_MWTGG:
        case SCENARIO_LTK:
        case SCENARIO_2v2:
        case SCENARIO_3v1:
        case SCENARIO_2v1:
            return 1;
        break;
        case SCENARIO_YOLT:
        case SCENARIO_TLD:
            return 0;
        break;
        default:
#ifdef DEBUG
            osSyncPrintf("Invalid scenario %d!", frontGetMpScenario());
#endif
            do {} while (1);
    }
}


/**
 * Draws the text for the multiplayer pause menu, post-game screens, and the "press start" prompt shown after death.
 * 
 * When the match is running it works through a horizontal carousel of pages starting at the "Score" page.
 * Losses << Kills << Score >> Pause >> Exit
 * 
 * When the game is over it displays these screens:
 * WOC/Awards << Losses << Kills << Score
 *
 * With the menu closed, nothing is drawn unless the player is dead and their
 * death animation has finished, in which case the continue prompt is centred
 * in the viewport. That's suppressed in YOLT once two of the player's lives are
 * gone since there is nothing to continue to.
 *
 * @param gdl display list to append to
 * @return the advanced display list pointer
 */
Gfx *mpMenuRender(Gfx *gdl)
{
    s32 curplayernum;
    s32 player_count;
    s32 x;
    s32 y;
    s32 k;
    s32 textwidth;
    s32 textheight;
    s32 m;
    s32 h1;
    s32 h2;
    char rankbuffer[4];
    s32 two_player_x_offset;
    char *text;
    s32 scores[4];
    s32 i;
    TEXTCOLORS current_colour;
    TEXTCOLORS same_team_colour;
    TEXTCOLORS other_team_colour;
    MPSCENARIOS scenario;
    s32 fav_textheight;
    s32 fav_textwidth;
    s32 fav_x_offset;
    s32 x3;
    s32 y3;
    s32 textwidth3;
    s32 textheight3;
    char *text3;
    s32 self_paused;
    s32 total_kills_against_current;
    s16 x2;
    s16 viewleft;
    s32 colour;
    s32 q;
 
    curplayernum = playerGetCurrentNum();
    player_count = getPlayerCount();
    self_paused = 0;
 
    if (player_count == 1)
    {
        return gdl;
    }
 
    if (g_CurrentPlayer->mpmenuon)
    {
        gdl = textSetupDraw(gdl);
    
        if (player_count == 2)
        {
            two_player_x_offset = 80;
        }
        else
        {
            two_player_x_offset = 0;
        }

        switch (g_CurrentPlayer->mpmenumode)
        {
            case MENU_GOWOC:
            case MENU_LOSSES:
            case MENU_KILLS:
            case MENU_SCORES:
                if (!g_MpGameOver)
                {
                    text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_15_PLAY)); /* PLAY */
                }
                else
                {
                    if (g_MpMenuAltGameOverMsg)
                    {
                        text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_16_GAMEOVER)); /* GAME OVER */
                    }
                    else
                    {
                        text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_17_STARTTTOEXIT)); /* START TO EXIT */
                    }
                }
                break;
            case MENU_FINISHED:
                text = (char *) ascii_MP_watch_menu_BLANK;
                break;
            case MENU_PAUSE:
                if (g_MpPaused)
                {
                    text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_18_PAUSED)); /* PAUSED */
                    if (playerGetCurrentNum() == g_MpPausedBy)
                    {
                        self_paused = 1;
                    }
                }
                else
                {
                    text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_19_PAUSE)); /* PAUSE */
                }
                break;
            case MENU_EXIT:
            case MENU_EXIT_CONFIRM:
                text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_1A_EXIT)); /* EXIT */
                x = (viGetViewLeft() + two_player_x_offset) + 65;
                break;
        }
 
        textMeasure(&textheight, &textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);
        x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 80;
        y = (viGetViewTop() - (textheight >> 1)) + (22 + MPMENU_YOFF);
 
        if (self_paused)
        {
            viewleft = viGetX(); 
            h1 = viGetY();
            gdl = textRenderOutlined(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0xa0ffa0f0, 0x007000a0, viewleft, h1, 0, 0);
        }
        else
        {
            viewleft = viGetX(); 
            h1 = viGetY();
            gdl = textRender(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
        }
 
        if (mpMenuCanGoLeft())
        {
            viewleft = viGetViewLeft();
    
            /**
             *  Colour is reused to store a pixel offset here. Efforts to introduce a new variable to store the offset resulted in stack problem I could not solve,
             *  so it's possible Rare really did reuse one variable for this. Perhaps it was not named "colour" but something more generic like "tmp."
             */ 
            colour = g_MpGameOver ? 10 : 0, x = ((viewleft + two_player_x_offset) - colour) + 40;
    
            if (g_MpGameOver)
            {
                x -= 8;
            }
 
            y = viGetViewTop() + (22 + MPMENU_YOFF);
 
            if (!g_MpMenuChevronGlow)
            {
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, (char *) ascii_MP_watch_menu_left_chevron, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }
            else
            {
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRenderOutlined(gdl, &x, &y, (char *) ascii_MP_watch_menu_left_chevron, g_FontBankGothicChars, g_FontBankGothic, 0xa0ffa0f0, 0x007000a0, viewleft, h1, 0, 0);
            }
        }
 
        if (mpMenuCanGoRight())
        {
            viewleft = viGetViewLeft();

            // Colour is again used to store a pixel offset.
            colour = g_MpGameOver ? 10 : 0, x = ((colour + 112) + viewleft) + two_player_x_offset;
 
            if (g_MpGameOver)
            {
                x += 8;
            }
 
            y = viGetViewTop() + (22 + MPMENU_YOFF);
 
            if (!g_MpMenuChevronGlow)
            {
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, (char *) ascii_MP_watch_menu_right_chevron, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }
            else
            {
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRenderOutlined(gdl, &x, &y, (char *) ascii_MP_watch_menu_right_chevron, g_FontBankGothicChars, g_FontBankGothic, 0xa0ffa0f0, 0x007000a0, viewleft, h1, 0, 0);
            }
        }
 
        if ((g_CurrentPlayer->mpmenumode == MENU_SCORES) || (g_CurrentPlayer->mpmenumode == MENU_PAUSE))
        {
            if (player_count > 0)
            {
                i = 0;
 
                do
                {
                    scores[i] = mpGetPlayerPoints(i);
                    i++;
                }
                while (i != player_count);
            }
 
            fav_x_offset = (g_MpGameOver == 0); 
 
            if (fav_x_offset) 
            { 
                fav_x_offset = (g_MpStopPlay == 0); 
            }
 
            if (mpMenuShowRank(fav_x_offset))
            {
                mpMenuFormatRank(rankbuffer, curplayernum);
                textMeasure(&textheight, &textwidth, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0);
 
                x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 80;
                y = (viGetViewTop() - (textheight >> 1)) + (37 + MPMENU_YOFF);
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }

            fav_x_offset = (g_MpGameOver == 0); 
 
            if (fav_x_offset) 
            { 
                fav_x_offset = (g_MpStopPlay == 0); 
            }
 
            if (mpMenuShowScore(fav_x_offset))
            {
                scenario = frontGetMpScenario();
                text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_1B_SCORES)); /* SCORES */
                textMeasure(&textheight, &textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);
                x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 80;
                y = (viGetViewTop() - (textheight >> 1)) + (53 + MPMENU_YOFF);
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
 
                if (((((scenario == SCENARIO_2v2) || (scenario == SCENARIO_3v1)) || (scenario == SCENARIO_2v1)) || (scenario == SCENARIO_TLD)) || (scenario == SCENARIO_MWTGG))
                {
                    if (g_playerPlayerData[curplayernum].have_token_or_goldengun == 0)
                    {
                        current_colour = RED_HIGHLIGHT;
                        same_team_colour = RED_NORMAL;
                        other_team_colour = BLUE_NORMAL;
                    }
                    else
                    {
                        current_colour = BLUE_HIGHLIGHT;
                        same_team_colour = BLUE_NORMAL;
                        other_team_colour = RED_NORMAL;
                    }
                }
                else
                {
                    current_colour = GREEN_HIGHLIGHT;
                    same_team_colour = GREEN_NORMAL;
                    other_team_colour = GREEN_NORMAL;
                }
 
                if (player_count == 2)
                {
                    viewleft = viGetViewLeft();
                    x2 = viGetViewTop();

                    if (curplayernum == 0)
                    {
                        colour = current_colour;
                    }
                    else
                    {
                        q = g_playerPlayerData[0].have_token_or_goldengun == g_playerPlayerData[curplayernum].have_token_or_goldengun ? same_team_colour : other_team_colour;
                        colour = q;
                    }
 
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 80, x2 + (70 + MPMENU_YOFF), scores[0], colour);
                    viewleft = viGetViewLeft();
                    x2 = viGetViewTop();

                    curplayernum == 1 ? (colour = current_colour) : (q = g_playerPlayerData[1].have_token_or_goldengun == g_playerPlayerData[curplayernum].have_token_or_goldengun ? same_team_colour : other_team_colour, colour = q);

                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 80, x2 + (86 + MPMENU_YOFF), scores[1], colour);
                }
                else
                {
                    viewleft = viGetViewLeft();
                    x2 = viGetViewTop();
 
                    if (curplayernum == 0)
                    {
                        colour = current_colour;
                    }
                    else
                    {
                        q = g_playerPlayerData[0].have_token_or_goldengun == g_playerPlayerData[curplayernum].have_token_or_goldengun ? same_team_colour : other_team_colour;
                        colour = q;
                    }
 
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 64, x2 + (70 + MPMENU_YOFF), scores[0], colour);
                    viewleft = viGetViewLeft();
                    x2 = viGetViewTop();
 
                    if (curplayernum == 1)
                    {
                        colour = current_colour;
                    }
                    else
                    {
                        q = g_playerPlayerData[1].have_token_or_goldengun == g_playerPlayerData[curplayernum].have_token_or_goldengun ? same_team_colour : other_team_colour;
                        colour = q;
                    }
 
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 96, x2 + (70 + MPMENU_YOFF), scores[1], colour);
                    viewleft = viGetViewLeft();
                    x2 = viGetViewTop();
 
                    if (curplayernum == 2)
                    {
                        colour = current_colour;
                    }
                    else
                    {
                        q = g_playerPlayerData[2].have_token_or_goldengun == g_playerPlayerData[curplayernum].have_token_or_goldengun ? same_team_colour : other_team_colour;
                        colour = q;
                    }
 
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 64, x2 + (86 + MPMENU_YOFF), scores[2], colour);
 
                    if (player_count == 4)
                    {
                        viewleft = viGetViewLeft();
                        x2 = viGetViewTop();

                        curplayernum == 3 ? (colour = current_colour) : (q = g_playerPlayerData[3].have_token_or_goldengun == g_playerPlayerData[curplayernum].have_token_or_goldengun ? same_team_colour : other_team_colour, colour = q);
                        gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 96, x2 + (86 + MPMENU_YOFF), scores[3], colour);
                    }
                }
            }

            // Keep colour live through this branch to reproduce the original register allocation.
            if (colour);
        }
        else if (g_CurrentPlayer->mpmenumode == MENU_KILLS)
        {
            fav_x_offset = (g_MpGameOver == 0); 

            if (fav_x_offset) 
            { 
                fav_x_offset = (g_MpStopPlay == 0); 
            } 

            if (mpMenuShowRank(fav_x_offset))
            {
                mpMenuFormatRank(rankbuffer, curplayernum);
                textMeasure(&textheight, &textwidth, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0);
                x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 80;
                y = (viGetViewTop() - (textheight >> 1)) + (37 + MPMENU_YOFF);
                viewleft = viGetX(); h1 = viGetY();
                gdl = textRender(gdl, &x, &y, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }
 
            q = (s32) langGet(getStringID(LMPMENU, MPMENU_STR_1C_P)); /* P */
 
            // Must remain a comma expression for matching
            h2 = (s32) langGet(getStringID(LMPMENU, MPMENU_STR_1D_KILLS)), /* KILLS */
                sprintf(rankbuffer, ascii_pnum_KILLS, (char *) q, curplayernum + 1, (char *) h2); /* -> "P<n> KILLS" */
 
            textMeasure(&textheight, &textwidth, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0);
            x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 80;
            y = (viGetViewTop() - (textheight >> 1)) + (53 + MPMENU_YOFF);
            viewleft = viGetX(); 
            h1 = viGetY();
            gdl = textRender(gdl, &x, &y, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
 
            if (player_count == 2)
            {
                if (curplayernum != 0)
                {
                    viewleft = viGetViewLeft(); h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 80, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[curplayernum].kill_counts[0], GREEN_NORMAL);
                }
                if (curplayernum != 1)
                {
                    viewleft = viGetViewLeft(); h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 80, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[curplayernum].kill_counts[1], GREEN_NORMAL);
                }
            }
            else
            {
                if (curplayernum != 0)
                {
                    viewleft = viGetViewLeft(); h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 64, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[curplayernum].kill_counts[0], GREEN_NORMAL);
                }
                if (curplayernum != 1)
                {
                    viewleft = viGetViewLeft(); h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 96, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[curplayernum].kill_counts[1], GREEN_NORMAL);
                }
                if (curplayernum != 2)
                {
                    viewleft = viGetViewLeft(); h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 64, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[curplayernum].kill_counts[2], GREEN_NORMAL);
                }
                if ((player_count == 4) && (curplayernum != 3))
                {
                    viewleft = viGetViewLeft(); h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 96, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[curplayernum].kill_counts[3], GREEN_NORMAL);
                }
            }
        }
        else if (g_CurrentPlayer->mpmenumode == MENU_LOSSES)
        {
            fav_x_offset = (g_MpGameOver == 0); 

            if (fav_x_offset) 
            { 
                fav_x_offset = (g_MpStopPlay == 0); 
            } 

            if (mpMenuShowRank(fav_x_offset))
            {
                mpMenuFormatRank(rankbuffer, curplayernum);
                textMeasure(&textheight, &textwidth, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0);
                x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 80;
                y = (viGetViewTop() - (textheight >> 1)) + (37 + MPMENU_YOFF);
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }
 
            q = (s32) langGet(getStringID(LMPMENU, MPMENU_STR_1C_P)); /* P */
 
            // Must remain a comma expression for matching.
            h2 = (s32) langGet(getStringID(LMPMENU, MPMENU_STR_1E_LOSSES)), /* LOSSES */
                sprintf(rankbuffer, ascii_pnum_LOSSES, (char *) q, curplayernum + 1, (char *) h2); /* -> "P<n> LOSSES" */
 
            textMeasure(&textheight, &textwidth, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0);
            x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 80;
            y = (viGetViewTop() - (textheight >> 1)) + (53 + MPMENU_YOFF);
            viewleft = viGetX(); 
            h1 = viGetY();
            gdl = textRender(gdl, &x, &y, rankbuffer, g_FontBankGothicChars, g_FontBankGothic, 0xff4040b0, viewleft, h1, 0, 0);
 
            if (player_count == 2)
            {
                if (curplayernum != 0)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 80, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[0].kill_counts[curplayernum], GREEN_NORMAL);
                }
                else if (g_playerPlayerData[0].kill_counts[0] > 0)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 80, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[0].kill_counts[curplayernum], RED_HIGHLIGHT);
                }
                if (curplayernum != 1)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 80, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[1].kill_counts[curplayernum], GREEN_NORMAL);
                }
                else if (g_playerPlayerData[1].kill_counts[1] > 0)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 80, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[1].kill_counts[curplayernum], RED_HIGHLIGHT);
                }
            }
            else
            {
                if (curplayernum != 0)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 64, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[0].kill_counts[curplayernum], GREEN_NORMAL);
                }
                else if (g_playerPlayerData[0].kill_counts[0] > 0)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 64, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[0].kill_counts[curplayernum], RED_HIGHLIGHT);
                }

                if (curplayernum != 1)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 96, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[1].kill_counts[curplayernum], GREEN_NORMAL);
                }
                else if (g_playerPlayerData[1].kill_counts[1] > 0)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 96, h1 + (70 + MPMENU_YOFF), g_playerPlayerData[1].kill_counts[curplayernum], RED_HIGHLIGHT);
                }

                if (curplayernum != 2)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 64, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[2].kill_counts[curplayernum], GREEN_NORMAL);
                }
                else if (g_playerPlayerData[2].kill_counts[2] > 0)
                {
                    viewleft = viGetViewLeft(); 
                    h1 = viGetViewTop();
                    gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 64, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[2].kill_counts[curplayernum], RED_HIGHLIGHT);
                }

                if (player_count == 4)
                {
                    if (curplayernum != 3)
                    {
                        viewleft = viGetViewLeft(); 
                        h1 = viGetViewTop();
                        gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 96, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[3].kill_counts[curplayernum], GREEN_NORMAL);
                    }
                    else if (g_playerPlayerData[3].kill_counts[3] > 0)
                    {
                        viewleft = viGetViewLeft(); 
                        h1 = viGetViewTop();
                        gdl = mpMenuRenderNumber(gdl, (viewleft + two_player_x_offset) + 96, h1 + (86 + MPMENU_YOFF), g_playerPlayerData[3].kill_counts[curplayernum], RED_HIGHLIGHT);
                    }
                }
            }
        }
        else if (g_CurrentPlayer->mpmenumode == MENU_GOWOC)
        {
            fav_x_offset = two_player_x_offset;

            if (player_count >= 3)
            {
                if (curplayernum & 1)
                {
                    fav_x_offset = two_player_x_offset - 7;
                }
                else
                {
                    fav_x_offset = two_player_x_offset + 7;
                }
            }
 
            text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_1F_WEAPONOFCHOICE)); /* Weapon of choice: */
            textMeasure(&fav_textheight, &fav_textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);
            x = ((viGetViewLeft() + fav_x_offset) - (fav_textwidth >> 1)) + 80;
            y = (viGetViewTop() - (fav_textheight >> 1)) + (37 + MPMENU_YOFF);
            viewleft = viGetX(); h1 = viGetY();
            gdl = textRender(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            text = frontGetFavWeaponText(curplayernum, 0);
            textMeasure(&fav_textheight, &fav_textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);
            x = ((viGetViewLeft() + fav_x_offset) - (fav_textwidth >> 1)) + 80;
            x2 = viGetViewTop();
 
            if (g_TextJapanese) 
            { 
                i = 4; 
            } 
            else 
            { 
                i = 0; 
            } 

            y = ((i + (u32) x2) - (fav_textheight >> 1)) + (53 + MPMENU_YOFF);
 
            viewleft = viGetX(); 
            h1 = viGetY();
            gdl = textRender(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
 
            if (g_CurrentPlayer->ptr_text_first_mp_award)
            {
                text = (char *) g_CurrentPlayer->ptr_text_first_mp_award;
                textMeasure(&fav_textheight, &fav_textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);
                x = ((viGetViewLeft() + fav_x_offset) - (fav_textwidth >> 1)) + 80;
                y = (viGetViewTop() - (fav_textheight >> 1)) + (75 + MPMENU_YOFF);
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }

            if (g_CurrentPlayer->ptr_text_second_mp_award)
            {
                text = (char *) g_CurrentPlayer->ptr_text_second_mp_award;
                textMeasure(&fav_textheight, &fav_textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);
                x = ((viGetViewLeft() + fav_x_offset) - (fav_textwidth >> 1)) + 80;
                y = (viGetViewTop() - (fav_textheight >> 1)) + (88 + MPMENU_YOFF);
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }
        }

        if (g_CurrentPlayer->mpmenumode == MENU_EXIT_CONFIRM)
        {
            text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_20_CANCEL)); /* cancel */
            textMeasure(&textheight, &textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);
            x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 54;
            y = (viGetViewTop() - (textheight >> 1)) + (54 + MPMENU_YOFF);
 
            if (g_CurrentPlayer->mpquitconfirm == 0)
            {
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRenderOutlined(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0xa0ffa0f0, 0x007000a0, viewleft, h1, 0, 0);
            }
            else
            {
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }
 
            text = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_21_CONFIRM)); /* confirm */
            textMeasure(&textheight, &textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);
            x = ((viGetViewLeft() + two_player_x_offset) - (textwidth >> 1)) + 104;
            y = (viGetViewTop() - (textheight >> 1)) + (54 + MPMENU_YOFF);
 
            if (g_CurrentPlayer->mpquitconfirm == 1)
            {
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRenderOutlined(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0xa0ffa0f0, 0x007000a0, viewleft, h1, 0, 0);
            }
            else
            {
                viewleft = viGetX(); 
                h1 = viGetY();
                gdl = textRender(gdl, &x, &y, text, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            }
        }
        gdl = textEndDraw(gdl);
    }
    else if (((((g_CurrentPlayer->bonddead) && (g_CurrentPlayer->deathanimfinished)) && (g_CurrentPlayer->redbloodfinished)) && (!g_MpStopPlay)) && (!g_MpGameOver)
        && (!g_CoopMode || coopPartnerCanRevive())) // co-op: no respawn once both are dead, or the partner is too weak
    {
        total_kills_against_current = 0;
 
        for (m = 0; m < player_count; m++)
        {
            total_kills_against_current += g_playerPlayerData[m].kill_counts[curplayernum];
        }
 
        if ((frontGetMpScenario() != SCENARIO_YOLT) || (total_kills_against_current < 2))
        {
            gdl = bgScissorCurrentPlayerViewDefault(gdl);
            gdl = textSetupDraw(gdl);

            if (g_CoopMode)
            {
                // Co-op: a dead player's view can be empty apart from the
                // fog-colour clear (Jungle), whose pixels don't hold full
                // coverage. Antialiased text over it was smeared by the VI
                // filter, so draw it without coverage blending.
                gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            }

            text3 = (char *) langGet(getStringID(LMPMENU, MPMENU_STR_22_PRESSSTART_LF)); /* press start */
            textMeasure(&textheight3, &textwidth3, text3, g_FontBankGothicChars, g_FontBankGothic, 0);
            x2 = viGetViewLeft();
            x3 = (x2 + (viGetViewWidth() >> 1)) - (textwidth3 >> 1);
            x2 = viGetViewTop();
            y3 = (x2 + (viGetViewHeight() >> 1)) - (textheight3 >> 1);
#if !defined(VERSION_US) || defined(BACKPORT_R1)
            gdl = textRenderRect(gdl, x3 - 1, y3 - 1, x3 + textwidth3 + 1, y3 + textheight3 + 1, 0);
#endif
            viewleft = viGetX(); 
            h1 = viGetY();
            gdl = textRender(gdl, &x3, &y3, text3, g_FontBankGothicChars, g_FontBankGothic, 0x00ff00b0, viewleft, h1, 0, 0);
            gdl = textEndDraw(gdl);
        }
    }
 
    return gdl;
}


s32 mpMenuShowGauges(void)
{
    return g_MpGameOver ? FALSE : (g_CurrentPlayer->mpmenuon | (g_CurrentPlayer->healthdisplaytime > 0));
}


s32 mpIsPaused(void) 
{
    return g_MpPaused;
}
