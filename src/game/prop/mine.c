/**
 * Live explosives: thrown grenades, launcher rounds, rockets, plastique and mines.
 *
 * Each is a weapon object whose timer objTickExplosive counts down. Grenades and timed
 * mines go off when it runs out; rockets and plastique when something sets it to 0. A
 * placed remote or proximity mine counts down while it arms, then 1 means primed and 0
 * that it goes off this frame. Remote mines go off when
 * their owner uses the detonator (remoteMinesDetonate sets the owner's bit in
 * g_RemoteMineOwnerTriggerFlag, which is cleared at the end of the frame). Primed
 * proximity mines are kept in a list so that guards and moving objects (thrown, falling
 * or driven) can set them off as well as the player.
 */

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <music.h>
#include <snd.h>
#include "game/prop/mine.h"
#include "game/chr/chr.h"
#include "game/chr/chraction.h"
#include "game/chr/chrai.h"
#include "game/player/player.h"
#include "game/prop/alarm.h"
#include "game/prop/objdamage.h"
#include "game/prop/propobj.h"
#include "game/level/lv.h"
#include <boss.h>

/* An explosive with this in flags2 makes a massive explosion */
#define EXPLOSIVE_FLAG2_MASSIVE PROPFLAG2_DOOR_ALTCOORDSYSTEM

s32 g_RemoteMineOwnerTriggerFlag = 0;

WeaponObjRecord *g_ProxMines[MAX_PROXMINES];

/* The current player sets off their remote mines */
void remoteMinesDetonate(void)
{
    g_RemoteMineOwnerTriggerFlag |= 1 << playerGetCurrentNum();
    sndPlaySfx(g_musicSfxBufferPtr, WATCH_DETONATE_MINE_SFX, NULL);
}


void proxminesReset(void)
{
    for (s32 i = 0; i < MAX_PROXMINES; i++)
    {
        g_ProxMines[i] = NULL;
    }
}


/* Add a primed proximity mine to the list (if the list is full, only the player can set
   it off) */
void proxmineAdd(WeaponObjRecord *mine)
{
    for (s32 i = 0; i < MAX_PROXMINES; i++)
    {
        if (g_ProxMines[i] == NULL)
        {
            g_ProxMines[i] = mine;
            return;
        }
    }
}


void proxmineRemove(WeaponObjRecord *mine)
{
    for (s32 i = 0; i < MAX_PROXMINES; i++)
    {
        if (g_ProxMines[i] == mine)
        {
            g_ProxMines[i] = NULL;
            return;
        }
    }
}


/* Set off the primed proximity mines near a position */
void proxminesDetonateNear(coord3d *pos)
{
    for (s32 i = 0; i < MAX_PROXMINES; i++)
    {
        WeaponObjRecord *mine = g_ProxMines[i];

        if (mine && mine->timer == 1)
        {
            f32 xdiff = pos->x - mine->runtime_pos.x;
            f32 ydiff = pos->y - mine->runtime_pos.y;
            f32 zdiff = pos->z - mine->runtime_pos.z;

            if ((xdiff * xdiff) + (ydiff * ydiff) + (zdiff * zdiff) < PROXMINE_TRIGGER_DIST_SQ)
            {
                mine->timer = 0;
            }
        }
    }
}


/* Active guards (running an AI script) set off proximity mines at the spot they are
   moving to */
void proxminesTickGuards(void)
{
    s32 numslots = chrsGetNumSlots();

    for (s32 i = 0; i < numslots; i++)
    {
        ChrRecord *guard = &g_ChrSlots[i];

        if (guard->model != NULL && (guard->hidden & CHRHIDDEN_BACKGROUND_AI))
        {
            coord3d pos;

            chrGetMovementTarget(guard, &pos);
            proxminesDetonateNear(&pos);
        }
    }
}


static s32 explosiveGetSize(ObjectRecord *obj)
{
    return (obj->flags2 & EXPLOSIVE_FLAG2_MASSIVE) ? EXPLOSION_DEF_MASSIVE : EXPLOSION_DEF_STANDARD;
}


/* Blow up a weapon. Returns FALSE if there was no room for the explosion, to try again
   next frame. */
static bool explosiveDetonate(PropRecord *prop, s32 explosiontype)
{
    ObjectRecord *obj = prop->obj;
    WeaponObjRecord *weapon = (WeaponObjRecord *)obj;

    if (!propExplode(prop, explosiontype))
    {
        return FALSE;
    }

    weapon->timer = -1;
    obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;

    return TRUE;
}


static void remoteMineTick(PropRecord *prop, WeaponObjRecord *weapon)
{
    ObjectRecord *obj = prop->obj;

    if (g_RemoteMineOwnerTriggerFlag)
    {
        u32 owner = (obj->runtime_bitflags & RUNTIMEBITFLAG_OWNER) >> RUNTIMEBITSHIFT_OWNER;

        if (g_RemoteMineOwnerTriggerFlag & (1 << owner))
        {
            weapon->timer = 0;
        }
    }

    if (weapon->timer > 1)
    {
        weapon->timer -= g_ClockTimer;

        if (weapon->timer < 2)
        {
            weapon->timer = 1;
        }
    }
    else if (weapon->timer == 0)
    {
        s32 explosiontype;

        if (obj->flags2 & EXPLOSIVE_FLAG2_MASSIVE)
        {
            explosiontype = EXPLOSION_DEF_MASSIVE;
        }
        else
        {
            explosiontype = EXPLOSION_DEF_STANDARD;

            if (bossGetStageNum() == LEVELID_FACILITY)
            {
                explosiontype = EXPLOSION_DEF_FACILITY_REMOTE;
            }
        }

        explosiveDetonate(prop, explosiontype);
    }
}


static void proxmineTick(PropRecord *prop, WeaponObjRecord *weapon)
{
    if (weapon->timer > 1)
    {
        weapon->timer -= g_ClockTimer;

        if (weapon->timer < 2)
        {
            weapon->timer = 1;
            proxmineAdd(weapon);
        }
    }
    else if (weapon->timer == 1)
    {
        // Primed: the current player sets it off (guards and moving objects do through
        // the list)
        PropRecord *playerprop = currentPlayerGetProp();
        f32 xdiff = playerprop->pos.f[0] - prop->pos.f[0];
        f32 ydiff = playerprop->pos.f[1] - prop->pos.f[1];
        f32 zdiff = playerprop->pos.f[2] - prop->pos.f[2];

        if ((xdiff * xdiff) + (ydiff * ydiff) + (zdiff * zdiff) < PROXMINE_TRIGGER_DIST_SQ)
        {
            weapon->timer = 0;
        }
    }

    if (weapon->timer == 0 && explosiveDetonate(prop, explosiveGetSize(prop->obj)))
    {
        proxmineRemove(weapon);
    }
}


/**
 * Count down a live explosive and blow it up when it is time. Runs once a frame, on the
 * first player's pass, for every object (including those carried or out of view).
 */
void objTickExplosive(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    WeaponObjRecord *weapon;

    if (playerGetShuffledIndex(playerGetCurrentNum()) != 0)
    {
        return;
    }

    // (An ammo crate flagged 0x10000000 blows up at once)
    if (obj->type == PROPDEF_MAGAZINE)
    {
        if (obj->flags & 0x10000000)
        {
            propExplode(prop, EXPLOSION_DEF_DRONE);
            obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
        }

        return;
    }

    if (obj->type != PROPDEF_COLLECTABLE)
    {
        return;
    }

    weapon = prop->weapon;

    switch (weapon->weaponnum)
    {
        case ITEM_GRENADE:
        case ITEM_GRENADEROUND:
            // Not live (picked up, or not thrown yet) when the timer is negative
            if (weapon->timer >= 0)
            {
                weapon->timer -= g_ClockTimer;

                if (weapon->timer < 0)
                {
                    propExplode(prop, explosiveGetSize(obj));
                    obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
                }
            }
            break;

        case ITEM_ROCKETROUND:
            if (weapon->timer == 0)
            {
                propExplode(prop, explosiveGetSize(obj));
                obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
            }
            break;

        case ITEM_PLASTIQUE:
            if (weapon->timer == 0)
            {
                propExplode(prop, EXPLOSION_DEF_MASSIVE);
                obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
                playerSurroundWithExplosions(PLASTIQUE_EXPLOSION_DELAY_TICKS);
                countdownTimerSetVisible(COUNTDOWN_HIDE_PLASTIQUE, FALSE);
            }
            break;

        case ITEM_TIMEDMINE:
        case ITEM_BOMBCASE:
            if (weapon->timer >= 0)
            {
                weapon->timer -= g_ClockTimer;

                if (weapon->timer < 0)
                {
                    explosiveDetonate(prop, explosiveGetSize(obj));
                }
            }
            break;

        case ITEM_REMOTEMINE:
            remoteMineTick(prop, weapon);
            break;

        case ITEM_PROXIMITYMINE:
            proxmineTick(prop, weapon);
            break;

        default:
            break;
    }
}
