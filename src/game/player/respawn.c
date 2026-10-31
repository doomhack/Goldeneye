/**
 * Respawning after death: in multiplayer at a spawn pad (mpRespawnPlayer), and in co-op
 * next to the partner, taking health from them (coopRespawnPlayer). See
 * docs/subsystems/player.md.
 */
#include "game/player/respawn.h"
#include "game/math/trig.h"
#include "game/player/inventory.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/player/playerintro.h"
#include "game/replay/ramromreplay.h"
#include "random.h"
#include <ultra64.h>
#include <bondtypes.h>
#include "game/level/gamemode.h"

/**
 * Co-op: set when a player respawns, until they let go of the buttons they
 * respawned with, so that pressing Start to respawn doesn't also open the watch.
 */
s32 g_CoopRespawnButtonsHeld[4];


static coord3d ZeroCoordSpawnPos = {0};


void mpRespawnPlayer(void)
{
    coord3d start_pos = ZeroCoordSpawnPos;
    f32 start_look_angle;
    StandTile *start_stan;
    f32 stan_height;
    s32 i;
    s32 padnum;
    struct SetupIntroEmpty* intro_record;

    intro_record = (struct SetupIntroEmpty *)g_CurrentSetup.intro;

    playerResetLife();
    playerStartNewLife();

    g_CurrentPlayer->bonddead = 0;
    g_CurrentPlayer->deathanimfinished = 0;
    g_CurrentPlayer->redbloodfinished = 0;
    g_CurrentPlayer->startnewbonddie = 1;
    g_CurrentPlayer->healthdamagetype = 7;
    g_CurrentPlayer->damagetype = 7;
    g_CurrentPlayer->gunammooff = 0;
    g_CurrentPlayer->gunsightmode = 2;

    hudmsgsSetOn(-1);
    hudmsgTopClearFlag(-1);


    if ((getPlayerCount() >= 2) && (startpadcount > 0))
    {
        padnum = playerChooseSpawnPad();
    }
    else
    {
        padnum = 0;
    }


    start_pos.x = g_Startpad[padnum]->pos.x;
    start_pos.z = g_Startpad[padnum]->pos.z;
    start_stan = g_Startpad[padnum]->stan;

    stan_height = playerGetFloorY(start_stan, start_pos.x, start_pos.z);

    start_pos.y = g_CurrentPlayer->eyeheight + stan_height;
    g_CurrentPlayer->manGround = stan_height;

    start_look_angle = randomGetNext() * 2.3283064e-10f * 6.2831855f;

    g_CurrentPlayer->vv_theta = (f32) ((start_look_angle * 360.0f) / 6.2831855f);
    g_CurrentPlayer->stanHeight = stan_height;
#if defined(VERSION_EU)
    g_CurrentPlayer->sumGround = (f32) (stan_height / 0.2004f);
#else
    g_CurrentPlayer->sumGround = (f32) (stan_height / 0.17000002f);
#endif

    playerResetCollision(&g_CurrentPlayer->bond, &start_pos, start_stan);

    g_CurrentPlayer->bond.theta_transform.x = -sinf(start_look_angle);
    g_CurrentPlayer->bond.theta_transform.y = 0.0f;
    g_CurrentPlayer->bond.theta_transform.z = cosf(start_look_angle);
    g_CurrentPlayer->prop->pos.x = g_CurrentPlayer->bondprevpos.x = start_pos.f[0];
    g_CurrentPlayer->prop->pos.y = g_CurrentPlayer->bondprevpos.y = start_pos.f[1];
    g_CurrentPlayer->prop->pos.z = g_CurrentPlayer->bondprevpos.z = start_pos.f[2];
    g_CurrentPlayer->prop->stan = start_stan;
#if defined(VERSION_EU)
    g_CurrentPlayer->dampEyeSum.x = (f32) (g_CurrentPlayer->bond.pos.x / 0.118799984f);
    g_CurrentPlayer->dampEyeSum.y = (f32) (g_CurrentPlayer->bond.pos.y / 0.118799984f);
    g_CurrentPlayer->dampEyeSum.z = (f32) (g_CurrentPlayer->bond.pos.z / 0.118799984f);
#else
    g_CurrentPlayer->dampEyeSum.x = (f32) (g_CurrentPlayer->bond.pos.x / 0.100000024f);
    g_CurrentPlayer->dampEyeSum.y = (f32) (g_CurrentPlayer->bond.pos.y / 0.100000024f);
    g_CurrentPlayer->dampEyeSum.z = (f32) (g_CurrentPlayer->bond.pos.z / 0.100000024f);
#endif

    invReset();
    for (i = 0; i < ARRAYCOUNT(g_CurrentPlayer->ammoheldarr); i++)
    {
        g_CurrentPlayer->ammoheldarr[i] = 0;
    }

    if (intro_record != NULL) 
    {
        while (intro_record->type != 9) // INTROTYPE_END
        { 
            switch (intro_record->type) 
            {
                case 0: // INTROTYPE_SPAWN
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSpawn));
                    break;
                case 1: // INTROTYPE_ITEM
                    if (check_ramrom_flags() == ((struct SetupIntroAmmo*)intro_record)->demoSlot) {
                        if ( ((struct SetupIntroItem*)intro_record)->item_left >= 0) {
                            invAddDualItem(((struct SetupIntroItem*)intro_record)->item_right, ((struct SetupIntroItem*)intro_record)->item_left);
                        } else {
                            invAddItem(((struct SetupIntroItem*)intro_record)->item_right);
                        }
                    }
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroItem));
                    break;
                case 2: // INTROTYPE_AMMO
                    if (check_ramrom_flags() == ((struct SetupIntroAmmo*)intro_record)->demoSlot) {
                        ammoGive(((struct SetupIntroAmmo*)intro_record)->ammo_type, ((struct SetupIntroAmmo*)intro_record)->ammo_amount);
                    }
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAmmo));
                    break;
                case 3: // INTROTYPE_SWIRL
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSwirl));
                    break;
                case 4: // INTROTYPE_ANIM
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAnim));
                    break;
                case 5: // INTROTYPE_CUFF
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCuff));
                    break;
                case 6: // INTROTYPE_CAMERA
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCamera));
                    break;
                default: // INTROTYPE_WATCH, INTROTYPE_CREDITS
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroEmpty));
                    break;
            }
        }
    }

    g_CurrentPlayer->deltaPos.x = 0.0f;
    g_CurrentPlayer->deltaPos.y = -0.0001f;
    g_CurrentPlayer->deltaPos.z = 0.0f;
    currentPlayerStartChrFade(120.0f, 1.0f);
}


/**
 * Co-op: a partner closer than this to the start pad is in the way of a respawn.
 */
#define COOP_RESPAWN_CLEAR 80.0f

s32 g_CoopDeathTime[4];

/**
 * Co-op: TRUE if no other player died after the current player.
 */
s32 coopDiedLast(void)
{
    s32 cur = playerGetCurrentNum();
    s32 i;

    // Counter-op: the mission ends with Bond, so his death is the replay.
    if (gameIsCounterOp())
    {
        return (cur == COUNTEROP_BOND);
    }

    for (i = 0; i < getPlayerCount(); i++)
    {
        if ((i != cur) && ((s32)(g_CoopDeathTime[i] - g_CoopDeathTime[cur]) > 0))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * Co-op: give the current player the level's starting loadout again, the
 * guns and ammo the setup's intro hands out, holding the starting weapon.
 * Objects they picked up (keys, mission items) are kept.
 */
static void coopGiveStartingLoadout(void)
{
    struct SetupIntroEmpty *intro_record = (struct SetupIntroEmpty *)g_CurrentSetup.intro;
    s32 i;

    invResetKeepProps();

    for (i = 0; i < 30; i++)
    {
        g_CurrentPlayer->ammoheldarr[i] = 0;
    }

    if (intro_record != NULL)
    {
        while (intro_record->type != 9) // INTROTYPE_END
        {
            switch (intro_record->type)
            {
                case 0: // INTROTYPE_SPAWN
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSpawn));
                    break;
                case 1: // INTROTYPE_ITEM
                    if (check_ramrom_flags() == ((struct SetupIntroItem*)intro_record)->demoSlot)
                    {
                        if (((struct SetupIntroItem*)intro_record)->item_left >= 0)
                        {
                            invAddDualItem(((struct SetupIntroItem*)intro_record)->item_right, ((struct SetupIntroItem*)intro_record)->item_left);
                        }
                        else
                        {
                            invAddItem(((struct SetupIntroItem*)intro_record)->item_right);
                        }
                    }
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroItem));
                    break;
                case 2: // INTROTYPE_AMMO
                    if (check_ramrom_flags() == ((struct SetupIntroAmmo*)intro_record)->demoSlot)
                    {
                        ammoGive(((struct SetupIntroAmmo*)intro_record)->ammo_type, ((struct SetupIntroAmmo*)intro_record)->ammo_amount);
                    }
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAmmo));
                    break;
                case 3: // INTROTYPE_SWIRL
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSwirl));
                    break;
                case 4: // INTROTYPE_ANIM
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAnim));
                    break;
                case 5: // INTROTYPE_CUFF
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCuff));
                    break;
                case 6: // INTROTYPE_CAMERA
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCamera));
                    break;
                case 7: // INTROTYPE_WATCH
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroWatch));
                    break;
                case 8: // INTROTYPE_CREDITS
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCredits));
                    break;
                default:
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroEmpty));
                    break;
            }
        }
    }

    invAddItem(ITEM_FIST);
    gunEquip(GUNLEFT, starting_weapon[GUNLEFT]);
    gunEquip(GUNRIGHT, starting_weapon[GUNRIGHT]);
}

/**
 * Co-op: puts the current player at pos (on the floor of stan), facing
 * look_angle (radians).
 */
void coopPlaceCurrentPlayer(coord3d *posp, StandTile *stan, f32 look_angle)
{
    coord3d pos = *posp;
    f32 stan_height;

    stan_height = playerGetFloorY(stan, pos.x, pos.z);
    pos.y = g_CurrentPlayer->eyeheight + stan_height;
    g_CurrentPlayer->manGround = stan_height;
    g_CurrentPlayer->vv_theta = (look_angle * 360.0f) / M_TAU_F;
    g_CurrentPlayer->stanHeight = stan_height;
#if defined(VERSION_EU)
    g_CurrentPlayer->sumGround = (f32) (stan_height / 0.2004f);
#else
    g_CurrentPlayer->sumGround = (f32) (stan_height / 0.17000002f);
#endif

    playerResetCollision(&g_CurrentPlayer->bond, &pos, stan);

    g_CurrentPlayer->bond.theta_transform.x = -sinf(look_angle);
    g_CurrentPlayer->bond.theta_transform.y = 0.0f;
    g_CurrentPlayer->bond.theta_transform.z = cosf(look_angle);
    g_CurrentPlayer->prop->pos.x = g_CurrentPlayer->bondprevpos.x = pos.f[0];
    g_CurrentPlayer->prop->pos.y = g_CurrentPlayer->bondprevpos.y = pos.f[1];
    g_CurrentPlayer->prop->pos.z = g_CurrentPlayer->bondprevpos.z = pos.f[2];
    g_CurrentPlayer->prop->stan = stan;
#if defined(VERSION_EU)
    g_CurrentPlayer->dampEyeSum.x = (f32) (g_CurrentPlayer->bond.pos.x / 0.118799984f);
    g_CurrentPlayer->dampEyeSum.y = (f32) (g_CurrentPlayer->bond.pos.y / 0.118799984f);
    g_CurrentPlayer->dampEyeSum.z = (f32) (g_CurrentPlayer->bond.pos.z / 0.118799984f);
#else
    g_CurrentPlayer->dampEyeSum.x = (f32) (g_CurrentPlayer->bond.pos.x / 0.100000024f);
    g_CurrentPlayer->dampEyeSum.y = (f32) (g_CurrentPlayer->bond.pos.y / 0.100000024f);
    g_CurrentPlayer->dampEyeSum.z = (f32) (g_CurrentPlayer->bond.pos.z / 0.100000024f);
#endif
    playerUpdateCollisionBounds();
}

/**
 * Co-op: bring the current (dead) player back at the level's start point
 * with the level's starting loadout, which is the penalty for dying. Only
 * happens while a partner is still alive.
 */
void coopRespawnPlayer(void)
{
    struct player *partner = NULL;
    coord3d pos;
    StandTile *stan;
    f32 look_angle;
    s32 count = getPlayerCount();
    s32 cur = playerGetCurrentNum();
    s32 i;
    s32 copiedgoldeneye;
    f32 shared;

    for (i = 0; i < count; i++)
    {
        if ((i != cur) && !g_playerPointers[i]->bonddead
            && ((g_playerPointers[i]->bondhealth + g_playerPointers[i]->bondarmour) >= 0.2f))
        {
            partner = g_playerPointers[i];
            break;
        }
    }

    if (partner == NULL)
    {
        return;
    }

    // Mission progress survives the respawn.
    copiedgoldeneye = g_CurrentPlayer->copiedgoldeneye;
    playerResetLife();
    playerStartNewLife();
    g_CurrentPlayer->copiedgoldeneye = copiedgoldeneye;

    g_CurrentPlayer->bonddead = 0;
    g_CurrentPlayer->deathanimfinished = 0;
    g_CurrentPlayer->redbloodfinished = 0;
    g_CurrentPlayer->startnewbonddie = 1;
    g_CurrentPlayer->healthdamagetype = 7;
    g_CurrentPlayer->damagetype = 7;
    g_CurrentPlayer->gunammooff = 0;
    g_CurrentPlayer->gunsightmode = 2;

    hudmsgsSetOn(-1);
    hudmsgTopClearFlag(-1);

    if (startpadcount > 0)
    {
        f32 dx;
        f32 dz;

        pos = g_Startpad[0]->pos;
        stan = g_Startpad[0]->stan;
        look_angle = M_TAU_F - mathAtan2(g_Startpad[0]->look.f[0], g_Startpad[0]->look.f[2]);

        // Step aside if the partner is standing on the start pad.
        dx = partner->prop->pos.x - pos.x;
        dz = partner->prop->pos.z - pos.z;

        if ((dx * dx + dz * dz) < (COOP_RESPAWN_CLEAR * COOP_RESPAWN_CLEAR))
        {
            coopFindPartnerSpawn(&pos, &stan, look_angle);
        }
    }
    else
    {
        pos = partner->prop->pos;
        stan = partner->prop->stan;
        look_angle = (partner->vv_theta * M_TAU_F) / 360.0f;
        coopFindPartnerSpawn(&pos, &stan, look_angle);
    }

    coopPlaceCurrentPlayer(&pos, stan, look_angle);
    coopGiveStartingLoadout();

    // Like Perfect Dark: the partner's health and armour, counted together,
    // are split evenly between the two players. Both lose their armour.
    shared = (partner->bondhealth + partner->bondarmour) * 0.5f;
    partner->bondhealth = shared;
    partner->bondarmour = 0.0f;
    g_CurrentPlayer->bondhealth = shared;
    g_CurrentPlayer->bondarmour = 0.0f;

    g_CurrentPlayer->deltaPos.x = 0.0f;
    g_CurrentPlayer->deltaPos.y = -0.0001f;
    g_CurrentPlayer->deltaPos.z = 0.0f;
    currentPlayerStartChrFade(120.0f, 1.0f);
}
