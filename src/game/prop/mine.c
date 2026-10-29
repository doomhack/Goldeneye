/**
 * Remote and proximity mines.
 *
 * A placed mine is a weapon object, ticked by objTick: its timer counts down while it
 * arms, then 1 means primed and 0 means it goes off this frame. Remote mines go off when
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
