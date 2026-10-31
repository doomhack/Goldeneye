/**
 * Health and damage: taking damage (playerTakeDamage), the damage flash and
 * health bars (playerTickDamageAndHealth), dying (playerDie), armour,
 * invincibility, and the explosions some levels set off round the player
 * (playerSurroundWithExplosions, playerTickExplode). See docs/subsystems/player.md.
 */
#include <joy.h>
#include <music.h>
#include "game/hud/healthbar.h"
#include "game/level/frametiming.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/menu/front.h"
#include "game/menu/mpmenu.h"
#include "game/menu/watchmenu.h"
#include "game/player/bondinv.h"
#include "game/player/bondview.h"
#include "game/player/bondview_internal.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/player/playertune.h"
#include "game/prop/alarm.h"
#include "game/prop/explosion.h"
#include "game/prop/propobj.h"
#include "game/world/bgfog.h"
#include "random.h"
#include "game/player/playerdamage.h"


/**
 * Each frame: the red damage flash, which hides the ammo, sight and messages
 * while it shows, and the health and armour bars.
 */
void playerTickDamageAndHealth(void)
{
    // update damage showtime
    if (g_CurrentPlayer->damageshowtime >= 0)
    {
        // 0: This is the first frame of damage
        if (g_CurrentPlayer->damageshowtime == 0)
        {
            gunSetGunAmmoVisible(GUNAMMOREASON_DAMAGE, FALSE);
            gunSetSightVisible(GUNSIGHTREASON_DAMAGE, FALSE);
            hudmsgsSetOff(4);
            hudmsgTopSetFlag(PLAYERFLAG_NOTIMER);
            countdownTimerSetVisible(COUNTDOWN_HIDE_DAMAGE, FALSE);

            g_CurrentPlayer->damagetype = (s32)(currentPlayerGetHealth() * 8.0f);

            if (g_CurrentPlayer->damagetype >= 8)
            {
                g_CurrentPlayer->damagetype = 7;
            }

// Ensure we don't read out of bounds of the g_DamageTypes array.
            if (g_CurrentPlayer->damagetype < 0)
            {
                g_CurrentPlayer->damagetype = 0;
            }
        }

#ifdef VERSION_US
        if (
            (g_DamageTypes[g_CurrentPlayer->damagetype].fadeEndFrame >= g_CurrentPlayer->damageshowtime)
            || (g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame >= g_CurrentPlayer->damageshowtime))
        {
            if (!g_CurrentPlayer->bonddead)
            {
#else
        if (!g_CurrentPlayer->bonddead
            && (
                (g_DamageTypes[g_CurrentPlayer->damagetype].fadeEndFrame >= g_CurrentPlayer->damageshowtime)
                || (g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame >= g_CurrentPlayer->damageshowtime)))
        {
#endif
            if (g_CurrentPlayer->damageshowtime >= g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame
                && g_CurrentPlayer->damageshowtime <= g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame)
            {
                f32 frac;
#ifdef VERSION_US
                s32 flashdoneframes;
                s32 totalframes;
                s32 flashfullframe;
#else
                f32 flashdoneframes;
                f32 totalframes;
                f32 flashfullframe;
#endif

                flashdoneframes = g_CurrentPlayer->damageshowtime - g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame;
                flashfullframe = g_DamageTypes[g_CurrentPlayer->damagetype].flashFullFrame;
                totalframes = g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame - g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame;

                if (flashdoneframes < flashfullframe)
                {
                    frac = (g_DamageTypes[g_CurrentPlayer->damagetype].maxAlpha * (f32)flashdoneframes) / (f32)flashfullframe;
                }
                else
                {
                    frac = (g_DamageTypes[g_CurrentPlayer->damagetype].maxAlpha * (f32)(totalframes - flashdoneframes)) / (f32)(totalframes - flashfullframe);
                }

                currentPlayerSetFadeColour(
                    g_DamageTypes[g_CurrentPlayer->damagetype].red,
                    g_DamageTypes[g_CurrentPlayer->damagetype].green,
                    g_DamageTypes[g_CurrentPlayer->damagetype].blue,
                    frac);
            }
#ifdef VERSION_US
            }
#endif

            if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
            {
#if defined(VERSION_US)
                g_CurrentPlayer->damageshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->damageshowtime += g_GlobalTimerDelta;
#endif
            }
            else
            {
#if defined(VERSION_US)
                g_CurrentPlayer->damageshowtime += g_FrameDelta;
#else
                g_CurrentPlayer->damageshowtime += g_FrameDelta60;
#endif
            }
        }
        else /* (damage showtime is over) */
        {
            g_CurrentPlayer->damageshowtime = -1;
            currentPlayerSetFadeColour(0xFF, 0xFF, 0xFF, 0);

            if (!g_CurrentPlayer->bonddead)
            {
                gunSetGunAmmoVisible(GUNAMMOREASON_DAMAGE, TRUE);
                gunSetSightVisible(GUNSIGHTREASON_DAMAGE, TRUE);
                hudmsgsSetOn(4);
                hudmsgTopClearFlag(PLAYERFLAG_NOTIMER);
                countdownTimerSetVisible(COUNTDOWN_HIDE_DAMAGE, TRUE);
            }
        }
    }

    // update health showtime
    if (g_CurrentPlayer->healthshowtime >= 0)
    {
        // 0: This is the first frame of damage
        if (g_CurrentPlayer->healthshowtime == 0)
        {
            g_CurrentPlayer->healthdamagetype = (s32)(currentPlayerGetHealth() * 8.0f);

            if (g_CurrentPlayer->healthdamagetype >= 8)
            {
                g_CurrentPlayer->healthdamagetype = 7;
            }

            if (g_CurrentPlayer->healthdamagetype < 0)
            {
                g_CurrentPlayer->healthdamagetype = 0;
            }
        }

        if (!g_CurrentPlayer->bonddead)
        {
            if ((g_CurrentPlayer->healthshowtime >= g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].validStartFrame)
                && (g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].updateToRealHealthFrame >= g_CurrentPlayer->healthshowtime))
            {
                g_CurrentPlayer->apparenthealth = g_CurrentPlayer->oldhealth;
                g_CurrentPlayer->apparentarmour = g_CurrentPlayer->oldarmour;
#if defined(VERSION_US)
                g_CurrentPlayer->healthshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->healthshowtime += g_GlobalTimerDelta;
#endif
            }
            else if ((g_CurrentPlayer->healthshowtime >= g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].validStartFrame)
                && (g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].hideHealthFrame >= g_CurrentPlayer->healthshowtime))
            {
                g_CurrentPlayer->apparenthealth = g_CurrentPlayer->bondhealth;
                g_CurrentPlayer->apparentarmour = g_CurrentPlayer->bondarmour;
#if defined(VERSION_US)
                g_CurrentPlayer->healthshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->healthshowtime += g_GlobalTimerDelta;
#endif
            }
            else
            {
                g_CurrentPlayer->healthshowtime = -1;
            }
        }
        else
        {
            g_CurrentPlayer->healthshowtime = -1;
        }
    }
}

/**
 * If global flag g_SurroundBondWithExplosionsFlag is set then explosions
 * will be randomly created around Bond.
*/
void playerTickExplode(void)
{
    g_PlayerTickExplodeCreatePosition++;

    if (g_SurroundBondWithExplosionsFlag
        && (g_PlayerInvincible == FALSE)
        && g_SurroundBondWithExplosionsTicks < g_GlobalTimer)
    {
        coord3d pos;

        pos.f[0] = g_CurrentPlayer->prop->pos.f[0];
        pos.f[1] = g_CurrentPlayer->prop->pos.f[1];
        pos.f[2] = g_CurrentPlayer->prop->pos.f[2];

        switch (g_PlayerTickExplodeCreatePosition % 4)
        {
            case 0: pos.x += 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 1: pos.x -= 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 2: pos.z += 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 3: pos.z -= 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
        }

        pos.y += 200.0f * RANDOMGETNEXT_F32() - 100.0f;

        explosionCreate(0, &pos, g_CurrentPlayer->prop->stan, EXPLOSION_DEF_PLAYER, 0, 0, g_CurrentPlayer->prop->rooms, 0);

        g_SurroundBondWithExplosionsTicks = (randomGetNext() % (u32)PLAYER_TICKEXPLODE_FACTOR) + g_GlobalTimer + PLAYER_TICKEXPLODE_FACTOR;
    }
}


/**
 * Kills the current player (unless invincible): closes the watch, starts the
 * death sequence, blows up the tank if they were driving it and drops their
 * guns. In co-op the mission only fails when no partner is left alive.
 */
void playerDie(void)
{
    if ((g_CurrentPlayer->cheatBondInvincible == 0) && (g_CurrentPlayer->bonddead == FALSE))
    {
        if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
        {
            watchTrigger(1);
        }

        if (g_CoopMode)
        {
            g_CoopDeathTime[playerGetCurrentNum()] = g_GlobalTimer;
        }

        // In co-op the mission carries on while the partner is alive.
        if (!coopPartnerAlive())
        {
            g_FrontBondKilled = 1;
        }

        g_CurrentPlayer->bonddead = 1;

        g_CurrentPlayer->bondDie = g_CurrentPlayer->bond;

        g_CurrentPlayer->thetadie = g_CurrentPlayer->vv_theta;
        g_CurrentPlayer->vertadie = g_CurrentPlayer->vv_verta;

        if ((g_PlayerTankProp != NULL) && !coopTankUsedByPartner())
        {
            g_ExplodeTankOnDeathFlag = 1;
        }

        // Counter-op: the guard player drops their gun like a guard.
        if (counterOpIsGuardPlayer(playerGetCurrentNum()) && (g_CurrentPlayer->prop->chr != NULL))
        {
            playerDropInventory();
        }

        gunEquip(GUNLEFT, 0);
        gunEquip(GUNRIGHT, 0);

        if ((lvGetMissionTime60() - g_CurrentPlayer->lifestarttime60) < g_playerPerm->shortest_inning)
        {
            g_playerPerm->shortest_inning = lvGetMissionTime60() - g_CurrentPlayer->lifestarttime60;
        }

        g_CurrentPlayer->lifestarttime60 = lvGetMissionTime60();
    }
}


/**
 * @param damage_amount: damage amount
 * @param vectorx: damage source x coordinate
 * @param vectorz: damage source y coordinate
 * @param playerid: player index of player causing the damage
 * @param arg4: boolean, does the damage apply to body armor (e.g. false when gas)
 */
void playerTakeDamage(f32 damage_amount, f32 vectorx, f32 vectorz, s32 playerid, s32 affects_armor) {
    f32 damage_dealt = g_playerPerm->handicap * damage_amount;
    s32 cur_player_num;
    f32 angle;
    s32 prevplayer;
    s32 hadgoldengun;

    if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
    {
        hudMakeDamageSegments(g_CurrentPlayer->armor_display_values, 1, currentPlayerGetArmor());
        hudMakeDamageSegments(g_CurrentPlayer->health_display_values, -1, currentPlayerGetHealth());
    }

    if (!isMpGame() || (g_MpStopPlay == 0 && g_MpGameOver == 0))
    {
        if (g_PlayerIsInTank == 1)
        {
            damage_dealt *= 0.25f;
        }

        if (g_CurrentPlayer->bonddead == FALSE && g_CurrentPlayer->cheatBondInvincible == FALSE)
        {
            joyRumblePakStart(playerGetCurrentNum(), 0.25);
            if (optionsGetControlStyle() >= 4)
            {
                // rumble second controller in 2.x
                joyRumblePakStart(playerGetCurrentNum() + getPlayerCount(), 0.25);
            }
        }

        if (isMpGame() && frontGetMpScenario() == SCENARIO_LTK)
        {
            // the damage dealt is always equivalent to how much health and armor the player has
            // the result of this is to always kill the player regardless of how much damage he can sustain
            damage_dealt = (g_CurrentPlayer->bondhealth * g_CurrentPlayer->actual_health) + (g_CurrentPlayer->bondarmour * g_CurrentPlayer->actual_armor);
        }

        if (g_CurrentPlayer->cheatBondInvincible == FALSE && g_CurrentPlayer->bonddead == FALSE && g_PlayerInvincible == FALSE &&
            (g_CurrentPlayer->damageshowtime < 0 || (isMpGame() && g_CurrentPlayer->damageshowtime == 0)
                // Counter-op: like the guard they took over, the guard
                // player feels every shot, not one per damage flash.
                || counterOpIsGuardPlayer(playerGetCurrentNum())))
        {
            if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x5 && g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0xc)
            {
                g_CurrentPlayer->oldhealth = g_CurrentPlayer->bondhealth;
                g_CurrentPlayer->oldarmour = g_CurrentPlayer->bondarmour;

                if (isMpGame())
                {
                    cur_player_num = playerGetCurrentNum();
                    angle = g_playerPointers[cur_player_num]->vv_theta - (360.0f - ((mathAtan2(vectorx, vectorz) * 180.0f) / 3.1415927f));

                    if (angle < 0.0f)
                    {
                        angle = -angle;
                    }

                    if (angle < 90.0f || angle > 270.0f)
                    {
                        // danger: if Bond could be damaged by toxic gas in multiplayer, playerid would be -1
                        // thus causing an out of bounds access
                        g_playerPlayerData[playerid].damage_to_backside++;
                    }
                }

                if (affects_armor && damage_dealt <= g_CurrentPlayer->bondarmour * g_CurrentPlayer->actual_armor)
                {
                    g_CurrentPlayer->bondarmour = g_CurrentPlayer->bondarmour - (damage_dealt / g_CurrentPlayer->actual_armor);
                }
                else
                {
                    if (affects_armor)
                    {
                        damage_dealt -= g_CurrentPlayer->bondarmour / g_CurrentPlayer->actual_armor;
                        g_CurrentPlayer->bondarmour = 0.0f;
                        g_CurrentPlayer->actual_armor = 1.0f;
                    }

                    g_CurrentPlayer->bondhealth = g_CurrentPlayer->bondhealth - (damage_dealt / g_CurrentPlayer->actual_health);

                    if (g_CurrentPlayer->bondhealth <= 0.0f)
                    {
                        if (isMpGame())
                        {
                            prevplayer = playerGetCurrentNum();
                            hadgoldengun = 0;

                            if (frontGetMpScenario() == 3 && invHasGoldenGun())
                            {
                                hadgoldengun = 1;
                            }

                            playerDropInventory();
                            if (prevplayer != playerid)
                            {
                                statsAddDeath();
                            }

                            playerSetCurrent(playerid);

                            if (prevplayer == playerid)
                            {
                                statsAddSuicide();
                            }
                            else
                            {
                                statsAddKill();

                                if (hadgoldengun != 0)
                                {
                                    statsAddGoldenGunKill();
                                }
                            }

                            playerSetCurrent(prevplayer);


                            g_playerPlayerData[playerid].kill_counts[prevplayer]++;
                        }
                        else if (g_CounterOpMode && (playerid >= 0) && (playerid < getPlayerCount())
                                 && (playerid != playerGetCurrentNum()))
                        {
                            // Counter-op: killing the other player counts
                            // as a kill, as killing a guard does.
                            prevplayer = playerGetCurrentNum();
                            playerSetCurrent(playerid);
                            statsAddKill();
                            playerSetCurrent(prevplayer);
                        }

                        playerDie();
                    }
                }

                if (g_CurrentPlayer->damageshowtime < 0)
                {
                    g_CurrentPlayer->bondshotspeed.x = g_CurrentPlayer->bondshotspeed.x + 2.0f * vectorx;
                    g_CurrentPlayer->bondshotspeed.z = g_CurrentPlayer->bondshotspeed.z + 2.0f * vectorz;
                }

                g_CurrentPlayer->damageshowtime = 0;
                g_CurrentPlayer->healthshowtime = 0;


                if (!lvGetControlsLocked())
                {
                    sndPlaySfx(g_musicSfxBufferPtr, BOND_GET_HIT1_SFX, 0);
                }
            }
        }
    }
}


/**
 * @param damage_amount: damage amount
 * @param rad:  damage source angle
 * @param player_id: player index of player causing the damage
 * @param affects_armor: boolean, does the damage apply to body armor (e.g. false when gas)
 */
void playerTakeDamageFromAngle(f32 damage_amount, f32 angle, s32 playerid, s32 affects_armor)
{
    playerTakeDamage(damage_amount, sinf(angle), cosf(angle), playerid, affects_armor);
}


int playerIsShowingDamage(void)
{
    return (g_CurrentPlayer->damageshowtime >= (s32)0);
}


int playerIsShowingHealth(void)
{
    return (g_CurrentPlayer->healthshowtime > (s32)0);
}


void playerSetInvincible(u32 invincible)
{
    g_CurrentPlayer->cheatBondInvincible = invincible;
}


u8 playerGetInvincible(void)
{
    return g_CurrentPlayer->cheatBondInvincible;
}


f32 currentPlayerGetHealth(void)
{
    return g_CurrentPlayer->bondhealth;
}


f32 currentPlayerGetArmor(void)
{
  return g_CurrentPlayer->bondarmour;
}


void currentPlayerAddArmor(f32 armour)
{
    g_playerPerm->body_armor_pickups += armour;
    g_CurrentPlayer->bondarmour = armour;
}


// Starts explosions round the player after delay ticks (see playerTickExplode)
void playerSurroundWithExplosions(int delay)
{
    g_SurroundBondWithExplosionsFlag = 1;
    g_SurroundBondWithExplosionsTicks = delay + g_GlobalTimer;
    g_PlayerTickExplodeCreatePosition = 0;
}
