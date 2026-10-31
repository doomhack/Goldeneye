/**
 * Co-op and counter-op support for the AI script interpreter (chrai.c).
 *
 * Solo AI lists only know one player. In co-op, commands that look at
 * "Bond" are re-run for the other player where one player isn't enough
 * (coopAiBegin/coopAiEnd). In counter-op, level scripts stay about Bond,
 * but their messages go to whoever used the object they checked.
 * See docs/subsystems/guards.md.
 */
#include "game/chr/chraicoop.h"
#include "game/chr/chrai.h"
#include "game/mission/objective.h"
#include "game/player/inventory.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include <bondaicommands.h>
#include <bondgame.h>
#include <bondtypes.h>
#include <boss.h>
#include "game/level/gamemode.h"

/**
 * Co-op: how an AI command that looks at "Bond" treats the two players.
 *
 * Solo AI lists only know one player. In co-op each chr runs its list with
 * its target player current (see coopChrSelectTarget), and the commands
 * below are re-run for the other player where one player isn't enough.
 */
enum CoopAiMode
{
    COOP_AI_ONE,      // current player only
    COOP_AI_ANY_SEEN, // true if true for any living player; the chr then targets that player
    COOP_AI_ANY,      // true if true for any player (level conditions)
    COOP_AI_ALL,      // true only if true for every player
    COOP_AI_EACH      // action applied to every player
};

#define COOP_AI_TARGET_LOCK60 (20 * 60)

static s32 coopAiCommandMode(u8 cmd)
{
    // Counter-op: level scripts and guards only know Bond. Only a cutscene's
    // control lock reaches the guard player as well.
    if (gameIsCounterOp())
    {
        if ((cmd == AI_BondDisableControl) || (cmd == AI_BondEnableControl))
        {
            return COOP_AI_EACH;
        }

        return COOP_AI_ONE;
    }

    switch (cmd)
    {
        case AI_IFISeeBond:
        case AI_IFIHeardBond:
        case AI_IFICouldSeeBond:
        case AI_IFICouldSeeBondsStan:
        case AI_IFImTargetedByBond:
        case AI_IFMyDistanceToBondLessThanDecimeter:
            return COOP_AI_ANY_SEEN;

        case AI_IFBondDistanceToPadLessThanDecimeter:
        case AI_IFBondInRoomWithPad:
        case AI_IFBondCollectedObject:
        case AI_IFBondHasItemEquipped:
        case AI_IFBondYPosLessThan:
        case AI_IFBondInTank:
        case AI_IFBondHealthLessThan:
            return COOP_AI_ANY;

        case AI_IFBondDistanceToPadGreaterThanDecimeter:
        case AI_IFMyDistanceToBondGreaterThanDecimeter:
        case AI_IFBondHealthGreaterThan:
        case AI_IFBondIsDead:
        case AI_IFBondItemTotalAmmoLessThan:
            return COOP_AI_ALL;

        case AI_BondDisableControl:
        case AI_BondEnableControl:
        case AI_BondEquipItem:
        case AI_BondEquipItemCinema:
        case AI_BondHideWeapons:
            return COOP_AI_EACH;
    }

    return COOP_AI_ONE;
}

/**
 * Co-op: TRUE if any player can still use an item, i.e. carries it with
 * ammo for it if it takes ammo. Each player starts with their own copy of a
 * level's single-use mission item (Dam's covert modem, Frigate's tracker
 * bug, Surface 2's timed mine).
 */
s32 coopAnyPlayerStillHasItem(ITEM_IDS item)
{
    s32 prevplayer = playerGetCurrentNum();
    s32 ammotype = weaponGetAmmoType(item);
    s32 found = FALSE;
    s32 i;

    for (i = 0; (i < getPlayerCount()) && !found; i++)
    {
        playerSetCurrent(i);

        if (invHasItem(item) && ((ammotype == AMMO_NONE) || (ammoGetTotal(ammotype) > 0)))
        {
            found = TRUE;
        }
    }

    playerSetCurrent(prevplayer);
    return found;
}

/**
 * Counter-op: the player who used the object a level script just checked,
 * for the rest of this aiRunList() run, or -1.
 */
static s32 s_CounterOpTextPlayer = -1;

/**
 * Called at the start of each aiRunList() run.
 */
void coopAiReset(struct CoopAiState *st)
{
    st->start = -1;
    s_CounterOpTextPlayer = -1;
}

/**
 * Counter-op: switches to the player a script message is for. Returns the
 * player to switch back to afterwards.
 */
s32 counterOpTextBegin(void)
{
    s32 prev = playerGetCurrentNum();

    if (s_CounterOpTextPlayer >= 0)
    {
        playerSetCurrent(s_CounterOpTextPlayer);
    }

    return prev;
}

/**
 * Co-op: an object a level script checks was used. The rest of the script
 * (messages such as "Door unlocked", objectives, fades) acts on the player
 * who used it, as for the level conditions in coopAiEnd.
 */
void coopAiActOnActivator(ChrRecord *chr)
{
    s32 player = g_CoopLastActivator;

    // Counter-op: the script stays about Bond, but its messages go to
    // whoever used the object.
    if (gameIsCounterOp())
    {
        if ((player >= 0) && (player < getPlayerCount()) && !g_playerPointers[player]->bonddead)
        {
            s_CounterOpTextPlayer = player;
        }

        return;
    }

    if (!gameIsTwoPlayerMission() || (chr == NULL) || (player < 0) || (player >= getPlayerCount()) || g_playerPointers[player]->bonddead)
    {
        return;
    }

    chr->targetplayer = player;
    chr->targetlock60 = COOP_AI_TARGET_LOCK60;
    playerSetCurrent(player);
}

/**
 * Called before each command. Starts repeating it over the players if it
 * needs that, with the current player first.
 */
void coopAiBegin(struct CoopAiState *st, AIRecord *list, s32 offset, ChrRecord *chr)
{
    s32 mode;
    s32 count;
    s32 i;

    if (st->start >= 0)
    {
        return;
    }

    mode = coopAiCommandMode((list + offset)->cmd);

    if (mode == COOP_AI_ONE)
    {
        return;
    }

    // While a script is locked onto the player who set it off (an ending
    // or a cutscene), its level conditions are about that player only.
    // Otherwise a check like Depot's "is Bond holding a gun" for the
    // cinema could pass for the other player and hand them the cutscene.
    if ((mode == COOP_AI_ANY) && (chr != NULL) && (chr->targetlock60 > 0))
    {
        return;
    }

    count = getPlayerCount();
    st->base = playerGetCurrentNum();
    st->count = 0;

    for (i = 0; i < count; i++)
    {
        s32 player = (st->base + i) % count;

        // A dead player can't be seen, heard or chased, and their body
        // doesn't meet level conditions (in a room, near a pad, ...).
        if (((mode == COOP_AI_ANY_SEEN) || (mode == COOP_AI_ANY)) && (i > 0) && g_playerPointers[player]->bonddead)
        {
            continue;
        }

        st->players[st->count++] = player;
    }

    st->start = offset;
    st->size = aiCommandSize(list, offset);
    st->mode = mode;
    st->next = 1;
}

/**
 * Called after each command. Returns TRUE if the command must run again for
 * the next player, with *offset rewound to it.
 */
s32 coopAiEnd(struct CoopAiState *st, s32 *offset, ChrRecord *chr)
{
    s32 taken;
    s32 again;

    if (st->start < 0)
    {
        return FALSE;
    }

    taken = (*offset != st->start + st->size);

    switch (st->mode)
    {
        case COOP_AI_ANY_SEEN:
        case COOP_AI_ANY:
            again = !taken;
            break;
        case COOP_AI_ALL:
            again = taken;
            break;
        default:
            again = TRUE;
            break;
    }

    if (again && (st->next < st->count))
    {
        playerSetCurrent(st->players[st->next++]);
        *offset = st->start;
        return TRUE;
    }

    if (taken && (chr != NULL) && g_CurrentPlayer->bonddead)
    {
        // Never fight a dead player.
        playerSetCurrent(st->base);
    }
    else if ((st->mode == COOP_AI_ANY_SEEN) && taken && (chr != NULL))
    {
        // Fight the player that was seen. They stay current for the rest
        // of this chr's tick.
        chr->targetplayer = playerGetCurrentNum();
    }
    else if ((st->mode == COOP_AI_ANY) && taken && (chr != NULL))
    {
        // A level condition (in a room, near a pad, ...) was met by this
        // player. The rest of the script acts on them: fades, the camera,
        // Bond's cutscene animations. Keep them for a while so later ticks
        // of the same script don't switch to the other player.
        chr->targetplayer = playerGetCurrentNum();
        chr->targetlock60 = COOP_AI_TARGET_LOCK60;
    }
    else
    {
        playerSetCurrent(st->base);
    }

    st->start = -1;
    return FALSE;
}


/**
 Counter-op: runs a vehicle's or aircraft's AI list for Bond, putting the
 current player back afterwards. Chrs do this in chrActionTick.
*/
void aiForObject(PropDefHeaderRecord *Entityp)
{
    s32 prevplayer;

    if (!gameIsCounterOp())
    {
        aiRunList(Entityp, PROP_TYPE_OBJ);
        return;
    }

    prevplayer = playerGetCurrentNum();
    playerSetCurrent(COUNTEROP_BOND);
    aiRunList(Entityp, PROP_TYPE_OBJ);
    playerSetCurrent(prevplayer);
}

/**
 * Counter-op: doors whose opening is progress for Bond, so the guard player
 * can't open them. Other doors level scripts watch (lifts driving floor
 * monitors, ending cutscenes, guard ambushes) stay open to them.
 */
static const struct
{
    s16 stage;
    u8 tag;
} s_CounterOpBondOnlyDoors[] = {
    { LEVELID_DEPOT, 0x06 }, // Trevelyan's train: opening it completes "Locate Trevelyan's train"
};

/**
 * Counter-op: TRUE if this door, or a door linked to it, is one the guard
 * player can't open (see s_CounterOpBondOnlyDoors).
 */
s32 counterOpDoorIsScripted(DoorRecord *door)
{
    s32 stage = bossGetStageNum();
    DoorRecord *cur = door;
    s32 i;

    do
    {
        for (i = 0; i < (s32)(sizeof(s_CounterOpBondOnlyDoors) / sizeof(s_CounterOpBondOnlyDoors[0])); i++)
        {
            if ((s_CounterOpBondOnlyDoors[i].stage == stage)
                && (objFindByTagId(s_CounterOpBondOnlyDoors[i].tag) == (ObjectRecord *)cur))
            {
                return TRUE;
            }
        }

        cur = cur->linkedDoor;
    } while ((cur != NULL) && (cur != door));

    return FALSE;
}
