/**
 * The inventory: a circular sorted list of weapons, dual weapon pairs and picked-up
 * props, cycling through it, keys, text overrides for picked-up objects, and the item
 * lookups the watch's inventory page uses. See docs/subsystems/weapons.md.
 */
#include <ultra64.h>
#include "game/player/bondview.h"
#include "game/chr/chr.h"
#include "game/player/player.h"
#include "game/text/text.h"
#include <bondconstants.h>
#include "game/text/language.h"
#include "game/player/bondinv.h"
#include "game/player/gun.h"
#include "game/level/lv.h"
#include <bondtypes.h>
#include <memp.h>

void invReset(void)
{
    s32 i;

    for (i = 0; i < g_CurrentPlayer->equipmaxitems; i++)
    {
        g_CurrentPlayer->p_itemcur[i].type = -1;
    }

    g_CurrentPlayer->ptr_inventory_first_in_cycle = NULL;
    g_CurrentPlayer->textoverrides                = NULL;
    g_CurrentPlayer->equipcuritem                 = ITEM_UNARMED;
}

/**
 * Sorts subject into its correct position in the inventory list.
 *
 * Subject is expected to initially be at the head of the list. It works by
 * swapping the subject with the item to its right as many times as needed.
 */
void invSort(InvItem *subject)
{
    InvItem *candidate;
    s32      subjweapon1 = -1;
    s32      subjweapon2 = -1;
    s32      candweapon1;
    s32      candweapon2;

    // Prepare subject's properties for comparisons
    if (subject->type == INV_ITEM_WEAPON)
    {
        subjweapon1 = subject->type_inv_item.type_weap.weapon;
    }
    else if (subject->type == INV_ITEM_DUAL)
    {
        subjweapon1 = subject->type_inv_item.type_dual.weapon_right;
        subjweapon2 = subject->type_inv_item.type_dual.weapon_left;
    }
    else if (subject->type == INV_ITEM_PROP)
    {
        subjweapon1 = 2000;
    }

    candidate = subject->next;

    while (g_CurrentPlayer->ptr_inventory_first_in_cycle != subject->next)
    {
        // Prepare candidate's properties for comparisons
        candweapon1 = -1;
        candweapon2 = -1;

        if (subject->next->type == INV_ITEM_WEAPON)
        {
            candweapon1 = subject->next->type_inv_item.type_weap.weapon;
        }
        else if (subject->next->type == INV_ITEM_DUAL)
        {
            candweapon1 = subject->next->type_inv_item.type_dual.weapon_right;
            candweapon2 = subject->next->type_inv_item.type_dual.weapon_left;
        }
        else if (subject->next->type == INV_ITEM_PROP)
        {
            candweapon1 = 1000;
        }

        // If the candidate should sort ahead of subject
        // then subject is in the desired position.
        if (candweapon1 >= subjweapon1 &&
            (subjweapon1 != candweapon1 || subjweapon2 <= candweapon2))
        {
            return;
        }

        // If there's only two items in the list then there's no point swapping
        // them. Just set the list head to the candidate.
        if (candidate->next == subject)
        {
            g_CurrentPlayer->ptr_inventory_first_in_cycle = candidate;
        }
        else
        {
            // Swap subject with candidate
            subject->next         = candidate->next;
            candidate->prev       = subject->prev;
            subject->prev         = candidate;
            candidate->next       = subject;
            subject->next->prev   = subject;
            candidate->prev->next = candidate;

            // Set new list head if subject was the head
            if (subject == g_CurrentPlayer->ptr_inventory_first_in_cycle)
            {
                g_CurrentPlayer->ptr_inventory_first_in_cycle = candidate;
            }
        }

        candidate = subject->next;
    }
}

void invInsertItem(InvItem *item)
{
    if (g_CurrentPlayer->ptr_inventory_first_in_cycle)
    {
        item->next = g_CurrentPlayer->ptr_inventory_first_in_cycle;
        item->prev = g_CurrentPlayer->ptr_inventory_first_in_cycle->prev;

        item->next->prev = item;
        item->prev->next = item;
    }
    else
    {
        item->next = item;
        item->prev = item;
    }

    g_CurrentPlayer->ptr_inventory_first_in_cycle = item;
    invSort(item);
    return;
}

void invRemoveItem(InvItem *item)
{
    InvItem *prev;
    InvItem *next;

    next = item->next;
    prev = item->prev;

    if (item == g_CurrentPlayer->ptr_inventory_first_in_cycle)
    {
        if (item == item->next)
        {
            g_CurrentPlayer->ptr_inventory_first_in_cycle = NULL;
        }
        else
        {
            g_CurrentPlayer->ptr_inventory_first_in_cycle = item->next;
        }
    }

    next->prev = prev;
    prev->next = next;
    item->type = -1;
    return;
}

InvItem *invGetNextAvailItem(void)
{
    int i;

    for (i = 0; i < g_CurrentPlayer->equipmaxitems; i++)
    {
        if (g_CurrentPlayer->p_itemcur[i].type == -1)
        {
            return &g_CurrentPlayer->p_itemcur[i];
        }
    }


    return NULL;
}

void invSetAllGunsFlag(s32 all_guns)
{
    g_CurrentPlayer->equipallguns = all_guns;
}

s32 invGetAllGunsFlag(void)
{
    return g_CurrentPlayer->equipallguns;
}

InvItem *invGetItem(ITEM_IDS weapon)
{
    InvItem *first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    InvItem *item  = first;

    while (item)
    {
        if (item->type == INV_ITEM_WEAPON && item->type_inv_item.type_weap.weapon == weapon)
        {
            return item;
        }

        item = item->next;

        if (item == first)
        {
            break;
        }
    }

    return NULL;
}

/**
 * Is item in inventory
 * @param item: enum Item ID eg: ITEM_KNIFE
 * @return TRUE/FALSE
 */
int invHasItem(ITEM_IDS item)
{
    return invGetItem(item) != NULL;
}

InvItem *invGetDualWeapon(ITEM_IDS right, ITEM_IDS left)
{
    InvItem *first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    InvItem *item  = first;

    while (item)
    {
        if (item->type == INV_ITEM_DUAL &&
            item->type_inv_item.type_dual.weapon_right == right &&
            item->type_inv_item.type_dual.weapon_left == left)
        {
            return item;
        }

        item = item->next;

        if (item == first)
        {
            break;
        }
    }

    return NULL;
}

/**
 * Is dual weapon in inventory
 * @param right: enum Item ID eg: ITEM_KNIFE
 * @param left: enum Item ID eg: ITEM_KNIFE
 * @return TRUE/FALSE
 */
bool invHasDualWeapon(ITEM_IDS right, ITEM_IDS left)
{
    return invGetDualWeapon(right, left) != NULL;
}

s32 invItemAvailable(ITEM_IDS weaponid)
{
    if (((g_CurrentPlayer->equipallguns) && (weaponid != ITEM_UNARMED) && (weaponid < ITEM_BOMBCASE)))
    {
        return TRUE;
    }
    return invHasItem(weaponid);
}

s32 invItemAvailableForHand(ITEM_IDS right, ITEM_IDS left)
{
#ifdef BUGFIX_R0
    if (g_CurrentPlayer->equipallguns &&
        right < ITEM_BOMBCASE &&
        right == left &&
        !isMpGame() &&
        weaponHasFlag(right, WEAPONSTATBITFLAG_CAN_DUAL_WIELD))
    {
        return TRUE;
    }
#else
    if (left == ITEM_UNARMED)
    {
        return TRUE;
    }
    else
    {
        if (g_CurrentPlayer->equipallguns &&
            right < ITEM_BOMBCASE &&
            right == left &&
            !isMpGame() &&
            weaponHasFlag(right, WEAPONSTATBITFLAG_CAN_DUAL_WIELD))
        {
            return TRUE;
        }
    }
#endif

    return invHasDualWeapon(right, left);
}

int invAddItem(ITEM_IDS item)
{
    InvItem *nextItem;

    if (invHasItem(item) == FALSE)
    {
        nextItem = invGetNextAvailItem();
        if (nextItem)
        {
            nextItem->type = INV_ITEM_WEAPON;
            nextItem->type_inv_item.type_weap.weapon = item;
            invInsertItem(nextItem);
        }

        if ((g_CurrentPlayer->equipallguns) && (item < ITEM_BOMBCASE))
        {
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

int invAddDualItem(ITEM_IDS right, ITEM_IDS left)
{
    InvItem *item;

    if (invHasDualWeapon(right, left) == FALSE)
    {
        item = invGetNextAvailItem();

        if (item)
        {
            item->type                                 = INV_ITEM_DUAL;
            item->type_inv_item.type_dual.weapon_right = right;
            item->type_inv_item.type_dual.weapon_left  = left;
            invInsertItem(item);
        }
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

WeaponObjRecord *invRemoveWeaponProp(ITEM_IDS weaponnum)
{
    if (g_CurrentPlayer->ptr_inventory_first_in_cycle)
    {
        InvItem *item = g_CurrentPlayer->ptr_inventory_first_in_cycle->next;

        while (TRUE)
        {
            InvItem *next = item->next;

            if (item->type == INV_ITEM_PROP)
            {
                PropRecord *prop = item->type_inv_item.type_prop.prop;

                if (prop->type == PROP_TYPE_WEAPON)
                {
                    ObjectRecord *obj = prop->obj;

                    if (obj->type == PROPDEF_COLLECTABLE)
                    {
                        WeaponObjRecord *weapon = (WeaponObjRecord *)prop->obj;

                        if (weapon->weaponnum == weaponnum)
                        {
                            invRemoveItem(item);
                            return weapon;
                        }
                    }
                }
            }

            if ((item == g_CurrentPlayer->ptr_inventory_first_in_cycle) || (!g_CurrentPlayer->ptr_inventory_first_in_cycle))
            {
                break;
            }

            item = next;
        }
    }

    return NULL;
}

void invRemoveItemByID(ITEM_IDS weaponnum)
{
    if (g_CurrentPlayer->ptr_inventory_first_in_cycle)
    {
        InvItem *item = g_CurrentPlayer->ptr_inventory_first_in_cycle->next;

        while (TRUE)
        {
            InvItem *next = item->next;

            if (item->type == INV_ITEM_PROP)
            {
                PropRecord *prop = item->type_inv_item.type_prop.prop;

                if (prop->type == PROP_TYPE_WEAPON)
                {
                    ObjectRecord *obj = prop->obj;

                    if (obj->type == PROPDEF_COLLECTABLE)
                    {
                        WeaponObjRecord *weapon = (WeaponObjRecord *)prop->obj;

                        if (weapon->weaponnum == weaponnum)
                        {
                            invRemoveItem(item);
                        }
                    }
                }
            }
            else if (item->type == INV_ITEM_WEAPON)
            {
                if (item->type_inv_item.type_weap.weapon == weaponnum)
                {
                    invRemoveItem(item);
                }
            }

            if ((item == g_CurrentPlayer->ptr_inventory_first_in_cycle) || (!g_CurrentPlayer->ptr_inventory_first_in_cycle))
            {
                break;
            }

            item = next;
        }
    }
}

int invAddProp(PropRecord *prop)
{
    InvItem *item;

    item = invGetNextAvailItem();

    if (item)
    {
        item->type                         = INV_ITEM_PROP;
        item->type_inv_item.type_prop.prop = prop;
        invInsertItem(item);
    }

    return TRUE;
}

int invAddWeaponByProp(PropRecord *prop)
{
    int added;
    added = FALSE;

    if (prop->type == PROP_TYPE_WEAPON)
    {
        ObjectRecord *obj = prop->obj;

        if (obj->type == PROPDEF_COLLECTABLE)
        {
            WeaponObjRecord *weapon = (WeaponObjRecord *)prop->obj;
            WeaponObjRecord *otherweapon;

            s8 weaponnum = weapon->weaponnum;
            added = invAddItem(weaponnum);

            otherweapon = weapon->dualweapon;
            if (otherweapon)
            {
                if (weapon->flags & PROPFLAG_WEAPON_LEFTHANDED)
                {
                    added = invHasDualWeapon(otherweapon->weaponnum, weaponnum) == 0;
                }
                else
                {
                    added = invHasDualWeapon(weaponnum, otherweapon->weaponnum) == 0;
                }
                weapon->dualweapon->LinkedWeaponType = weaponnum;
                weapon->dualweapon->dualweapon       = NULL;
                weapon->dualweapon                   = NULL;
            }
            else
            {
                if (weapon->LinkedWeaponType >= 0)
                {
                    if (weapon->flags & PROPFLAG_WEAPON_LEFTHANDED)
                    {
                        added = invAddDualItem(weapon->LinkedWeaponType, weaponnum);
                    }
                    else
                    {
                        added = invAddDualItem(weaponnum, weapon->LinkedWeaponType);
                    }
                }
            }
        }
    }
    return added;
}

void invCycleForward(s32 *nextright, s32 *nextleft, s32 requireammo)
{
    s32      weapon1 = *nextright;
    s32      weapon2 = *nextleft;
    InvItem *item    = g_CurrentPlayer->ptr_inventory_first_in_cycle;

    while (item)
    {
        if (item->type == INV_ITEM_WEAPON)
        {
            if (item->type_inv_item.type_weap.weapon < ITEM_BOMBCASE && item->type_inv_item.type_weap.weapon > weapon1)
            {
                if (requireammo == FALSE || weaponHasAmmo(item->type_inv_item.type_weap.weapon))
                {
                    weapon1 = item->type_inv_item.type_weap.weapon;
                    weapon2 = 0;
                    break;
                }
            }
        }
        else if (item->type == INV_ITEM_DUAL)
        {
            if (item->type_inv_item.type_dual.weapon_right > weapon1 || (weapon1 == item->type_inv_item.type_dual.weapon_right && item->type_inv_item.type_dual.weapon_left > weapon2))
            {
                if (requireammo == FALSE || weaponHasAmmo(item->type_inv_item.type_dual.weapon_right) || weaponHasAmmo(item->type_inv_item.type_dual.weapon_left))
                {
                    weapon1 = item->type_inv_item.type_dual.weapon_right;
                    weapon2 = item->type_inv_item.type_dual.weapon_left;
                    break;
                }
            }
        }

        item = item->next;

        if (item == g_CurrentPlayer->ptr_inventory_first_in_cycle)
        {
            if (requireammo)
            {
                break;
            }

            weapon1 = -1;
            weapon2 = -1;
        }
    }

    if (g_CurrentPlayer->equipallguns)
    {
        s32 candidate = *nextright;

        if (!isMpGame() && weaponHasFlag(*nextright, WEAPONSTATBITFLAG_CAN_DUAL_WIELD) && (*nextleft < *nextright) && (requireammo == FALSE || weaponHasAmmo(*nextright)) && (weapon1 != *nextright || *nextright < weapon2))
        {
            weapon1 = *nextright;
            weapon2 = *nextright;
        }
        else
        {
            if ((weapon1 != *nextright) || (weapon2 == *nextleft))
            {
                // Find next weapon
                do
                {
                    candidate = (candidate + 1) % ITEM_BOMBCASE;

                    if (candidate == ITEM_UNARMED)
                    {
                        candidate = (candidate + 1) % ITEM_BOMBCASE;
                    }

                    if ((requireammo == FALSE || weaponHasAmmo(candidate)))
                    {
                        weapon1 = candidate;
                        weapon2 = ITEM_UNARMED;
                        break;
                    }
                } while (candidate != weapon1);
            }
        }
    }

    *nextright = weapon1;
    *nextleft  = weapon2;
}

void invCycleBackward(s32 *nextright, s32 *nextleft, s32 requireammo)
{
    s32 weapon1 = *nextright;
    s32 weapon2 = *nextleft;

    if (g_CurrentPlayer->ptr_inventory_first_in_cycle != NULL)
    {
        InvItem *item = g_CurrentPlayer->ptr_inventory_first_in_cycle->prev;

        while (TRUE)
        {
            if (item->type == INV_ITEM_WEAPON)
            {
                if (item->type_inv_item.type_weap.weapon < ITEM_BOMBCASE && (item->type_inv_item.type_weap.weapon < weapon1 || (weapon1 == item->type_inv_item.type_weap.weapon && weapon2 > 0)))
                {
                    if (requireammo == FALSE || weaponHasAmmo(item->type_inv_item.type_weap.weapon))
                    {
                        weapon1 = item->type_inv_item.type_weap.weapon;
                        weapon2 = ITEM_UNARMED;
                        break;
                    }
                }
            }
            else if (item->type == INV_ITEM_DUAL)
            {
                if (item->type_inv_item.type_dual.weapon_right < weapon1 || (weapon1 == item->type_inv_item.type_dual.weapon_right && item->type_inv_item.type_dual.weapon_left < weapon2))
                {
                    if (requireammo == FALSE || weaponHasAmmo(item->type_inv_item.type_dual.weapon_right) || weaponHasAmmo(item->type_inv_item.type_dual.weapon_left))
                    {
                        weapon1 = item->type_inv_item.type_dual.weapon_right;
                        weapon2 = item->type_inv_item.type_dual.weapon_left;
                        break;
                    }
                }
            }

            if (item == g_CurrentPlayer->ptr_inventory_first_in_cycle)
            {
                if (requireammo)
                {
                    break;
                }

                weapon1 = 1000;
                weapon2 = 1000;
            }

            item = item->prev;
        }
    }

    if (g_CurrentPlayer->equipallguns)
    {
        s32 candidate = *nextright;

        if (*nextleft == ITEM_UNARMED)
        {
            candidate = (candidate + ITEM_BOMBCASE - 1) % ITEM_BOMBCASE;
            if (candidate == ITEM_UNARMED)
            {
                candidate = (candidate + ITEM_BOMBCASE - 1) % ITEM_BOMBCASE;
            }
        }

        while (TRUE)
        {
            if (candidate == weapon1)
            {
                if (!isMpGame() && weaponHasFlag(candidate, WEAPONSTATBITFLAG_CAN_DUAL_WIELD) && (requireammo == FALSE || weaponHasAmmo(candidate)) && (candidate != *nextright || candidate < *nextleft) && (weapon2 < candidate))
                {
                    weapon1 = candidate;
                    weapon2 = candidate;
                }

                break;
            }
            else if (
                (requireammo == FALSE || weaponHasAmmo(candidate)))
            {
                if (!isMpGame() && weaponHasFlag(candidate, WEAPONSTATBITFLAG_CAN_DUAL_WIELD) && (candidate != *nextright || candidate < *nextleft))
                {
                    weapon1 = candidate;
                    weapon2 = candidate;
                }
                else
                {
                    weapon1 = candidate;
                    weapon2 = ITEM_UNARMED;
                }

                break;
            }
            else
            {
                candidate = (candidate + ITEM_BOMBCASE - 1) % ITEM_BOMBCASE;
                if (candidate == ITEM_UNARMED)
                {
                    candidate = (candidate + ITEM_BOMBCASE - 1) % ITEM_BOMBCASE;
                }
            }
        }
    }

    *nextright = weapon1;
    *nextleft  = weapon2;
}

/**
 * The key flags of every key in a player's inventory.
 */
static u32 invGetHeldKeyFlags(struct player *player)
{
    u32      heldkeyflags = 0;
    InvItem *item         = player->ptr_inventory_first_in_cycle;

    while (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = item->type_inv_item.type_prop.prop;

            if (prop->type == PROP_TYPE_OBJ)
            {
                ObjectRecord *obj = prop->obj;

                if (obj->type == PROPDEF_KEY)
                {
                    KeyRecord *key = (KeyRecord *)prop->obj;

                    heldkeyflags |= key->keyflags;
                }
            }
        }

        item = item->next;

        if (item == player->ptr_inventory_first_in_cycle)
        {
            break;
        }
    }

    return heldkeyflags;
}

bool invCheckHasKeyFlags(u32 wantkeyflags)
{
    u32 heldkeyflags = invGetHeldKeyFlags(g_CurrentPlayer);
    s32 i;

    // Co-op: keys are shared, so either player opens a door the other has
    // the key for.
    if (g_CoopMode)
    {
        for (i = 0; i < getPlayerCount(); i++)
        {
            heldkeyflags |= invGetHeldKeyFlags(g_playerPointers[i]);
        }
    }

    return (wantkeyflags & heldkeyflags) == wantkeyflags;
}

bool invHasGEKey(void)
{
    InvItem      *item;
    PropRecord   *prop;
    ObjectRecord *obj;

    item = g_CurrentPlayer->ptr_inventory_first_in_cycle;

    while (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            prop = item->type_inv_item.type_prop.prop;

            if (prop->type == PROP_TYPE_WEAPON)
            {
                obj = prop->obj;

                if (obj->obj == PROJECTILES_TYPE_GE_KEY)
                {
                    return TRUE;
                }
            }
        }

        item = item->next;

        if (item == g_CurrentPlayer->ptr_inventory_first_in_cycle)
        {
            break;
        }
    }

    return FALSE;
}

/**
 * Is the player alive with flag tag token in inventory
 * @return TRUE/FALSE
 */
bool invHasFlagToken(void)
{
    if (!g_CurrentPlayer->bonddead)
    {
        return invHasItem(ITEM_TOKEN);
    }

    return FALSE;
}

/**
 * Is the Golden Gun in inventory
 * @return TRUE/FALSE
 */
bool invHasGoldenGun(void)
{
    return invHasItem(ITEM_GOLDENGUN);
}

static bool invPlayerHasProp(struct player *player, PropRecord *prop)
{
    InvItem *item = player->ptr_inventory_first_in_cycle;

    while (item)
    {
        if (item->type == INV_ITEM_PROP && item->type_inv_item.type_prop.prop == prop)
        {
            return TRUE;
        }

        item = item->next;

        if (item == player->ptr_inventory_first_in_cycle)
        {
            break;
        }
    }

    return FALSE;
}

bool invHasProp(PropRecord *prop)
{
    return invPlayerHasProp(g_CurrentPlayer, prop);
}

/**
 * Co-op: TRUE if any player carries the prop (objectives are shared).
 */
bool invAnyPlayerHasProp(PropRecord *prop)
{
    s32 i;

    if (!g_CoopMode)
    {
        return invHasProp(prop);
    }

    for (i = 0; i < getPlayerCount(); i++)
    {
        if (invPlayerHasProp(g_playerPointers[i], prop))
        {
            return TRUE;
        }
    }

    return FALSE;
}

s32 invCountItems(void)
{
    InvItem *item;
    s32      numitems = 0;

    if (g_CurrentPlayer->equipallguns)
    {
        numitems = ITEM_TANKSHELLS;
    }

    item = g_CurrentPlayer->ptr_inventory_first_in_cycle;

    while (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = item->type_inv_item.type_prop.prop;

            if (prop->type == PROP_TYPE_WEAPON)
            {
                ObjectRecord *obj = prop->obj;

                if (obj->runtime_bitflags & 0x400)
                {
                    numitems = numitems + 1;
                }
            }
            else if (prop->type == PROP_TYPE_OBJ)
            {
                if ((prop->obj->flags2 & 0x40000) == 0)
                {
                    numitems = numitems + 1;
                }
            }
        }
        else if (item->type == INV_ITEM_WEAPON)
        {
            if ((g_CurrentPlayer->equipallguns == FALSE) || (item->type_inv_item.type_weap.weapon > ITEM_TANKSHELLS))
            {
                numitems = numitems + 1;
            }
        }

        item = item->next;

        if (item == g_CurrentPlayer->ptr_inventory_first_in_cycle)
        {
            break;
        }
    }

    return numitems;
}

InvItem *invGetItemByIndex(s32 index)
{
    InvItem *item;

    if (g_CurrentPlayer->equipallguns)
    {
        if (index < ITEM_TANKSHELLS)
        {
            return NULL;
        }

        index = index - ITEM_TANKSHELLS;
    }

    item = g_CurrentPlayer->ptr_inventory_first_in_cycle;

    while (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = item->type_inv_item.type_prop.prop;

            if (prop->type == PROP_TYPE_WEAPON)
            {
                ObjectRecord *obj = prop->obj;

                if (obj->runtime_bitflags & 0x400)
                {
                    if (index == 0)
                    {
                        return item;
                    }
                    index--;
                }
            }
            else if (prop->type == PROP_TYPE_OBJ)
            {
                if ((prop->obj->flags2 & 0x40000) == 0)
                {
                    if (index == 0)
                    {
                        return item;
                    }
                    index--;
                }
            }
        }
        else if (item->type == INV_ITEM_WEAPON)
        {
            if ((g_CurrentPlayer->equipallguns == FALSE) || (item->type_inv_item.type_weap.weapon > ITEM_TANKSHELLS))
            {
                if (index == 0)
                {
                    return item;
                }
                index--;
            }
        }

        item = item->next;

        if (item == g_CurrentPlayer->ptr_inventory_first_in_cycle)
        {
            break;
        }
    }

    return NULL;
}

textoverride *invGetOverrideForObj(ObjectRecord *obj)
{
    textoverride *override = g_CurrentPlayer->textoverrides;

    while (override)
    {
        if (override->obj == obj)
        {
            return override;
        }

        override = override->next;
    }

    return NULL;
}

textoverride *invGetOverrideForWeapon(ITEM_IDS weaponnum)
{
    textoverride *override = g_CurrentPlayer->textoverrides;

    while (override)
    {
        if ((override->objoffset == 0) && (override->weapon == weaponnum))
        {
            return override;
        }

        override = override->next;
    }

    return NULL;
}

s32 invGetWeaponForIndex(s32 index)
{
    textoverride *override;
    InvItem *     inv_item;

    inv_item = invGetItemByIndex(index);

    if (inv_item)
    {
        if (inv_item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = inv_item->type_inv_item.type_prop.prop;

            override = invGetOverrideForObj(prop->obj);

            if (override)
            {
                return override->weapon;
            }
        }
        else if (inv_item->type == INV_ITEM_WEAPON)
        {
            return inv_item->type_inv_item.type_weap.weapon;
        }
    }
    else
    {
        if (g_CurrentPlayer->equipallguns)
        {
            if (index < ITEM_TANKSHELLS)
            {
                return index + 1;
            }
        }
    }

    return 0;
}

/**
 * Counter-op: the guard player's empty hand is the suicide pill
 * (counterOpSuicidePillTick), so the watch names it that instead of "-".
 */
static char *invCounterOpPillName(ITEM_IDS weaponnum)
{
    if ((weaponnum == ITEM_UNARMED) && counterOpIsGuardPlayer(playerGetCurrentNum()))
    {
        return "suicide pill\n";
    }

    return NULL;
}

u16 *invGetNameByIndex(s32 index)
{
    InvItem      *item      = invGetItemByIndex(index);
    ITEM_IDS      weaponnum = 0;
    textoverride *override;

    if (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = item->type_inv_item.type_prop.prop;
            override         = invGetOverrideForObj(prop->obj);

            if (override)
            {
                if (override->shorttext)
                {
                    return langGet(override->shorttext);
                }

                weaponnum = override->weapon;
            }
        }
        else if (item->type == INV_ITEM_WEAPON)
        {
            weaponnum = item->type_inv_item.type_weap.weapon;
            override  = invGetOverrideForWeapon(weaponnum);

            if (override && override->shorttext)
            {
                return langGet(override->shorttext);
            }
        }
    }
    else
    {
        if (g_CurrentPlayer->equipallguns)
        {
            if (index < ITEM_TANKSHELLS)
            {
                return itemGetWatchText(index + 1);
            }
        }
    }

    if (item && invCounterOpPillName(weaponnum))
    {
        return (u16 *)invCounterOpPillName(weaponnum);
    }

    return itemGetWatchText(weaponnum);
}

u16 *invGetLongNameByIndex(s32 index)
{
    InvItem      *item      = invGetItemByIndex(index);
    ITEM_IDS      weaponnum = 0;
    textoverride *override;

    if (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = item->type_inv_item.type_prop.prop;
            override         = invGetOverrideForObj(prop->obj);

            if (override)
            {
                if (override->longtext)
                {
                    return langGet(override->longtext);
                }

                weaponnum = override->weapon;
            }
        }
        else if (item->type == INV_ITEM_WEAPON)
        {
            weaponnum = item->type_inv_item.type_weap.weapon;
            override  = invGetOverrideForWeapon(weaponnum);

            if (override && override->longtext)
            {
                return langGet(override->longtext);
            }
        }
    }
    else
    {
        if (g_CurrentPlayer->equipallguns)
        {
            if (index < ITEM_TANKSHELLS)
            {
                return itemGetChoiceText(index + 1);
            }
        }
    }

    if (item && invCounterOpPillName(weaponnum))
    {
        return (u16 *)invCounterOpPillName(weaponnum);
    }

    return itemGetChoiceText(weaponnum);
}

extern f32 itemGetEquipFov(s32 item);
f32 invGetEquipFovForIndex(int index)
{
    return itemGetEquipFov(invGetWeaponForIndex(index));
}

int invGetEquipPosXForIndex(int index)
{
    return itemGetEquipPosX(invGetWeaponForIndex(index));
}

int invGetEquipPosYForIndex(int index)
{
    return itemGetEquipPosY(invGetWeaponForIndex(index));
}

int invGetEquipPosZForIndex(int index)
{
    return itemGetEquipPosZ(invGetWeaponForIndex(index));
}

u16 *invGetWatchTitle1ForIndex(s32 index)
{
    InvItem      *item      = invGetItemByIndex(index);
    ITEM_IDS      weaponnum = 0;
    textoverride *override;

    if (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = item->type_inv_item.type_prop.prop;
            override         = invGetOverrideForObj(prop->obj);

            if (override)
            {
                if (override->titletext1)
                {
                    return langGet(override->titletext1);
                }

                weaponnum = override->weapon;
            }
        }
        else if (item->type == INV_ITEM_WEAPON)
        {
            weaponnum = item->type_inv_item.type_weap.weapon;
            override  = invGetOverrideForWeapon(weaponnum);

            if (override && override->titletext1)
            {
                return langGet(override->titletext1);
            }
        }
    }
    else
    {
        if (g_CurrentPlayer->equipallguns)
        {
            if (index < ITEM_TANKSHELLS)
            {
                return itemGetWatchTitle1(index + 1);
            }
        }
    }

    return itemGetWatchTitle1(weaponnum);
}

u16 *invGetWatchTitle2ForIndex(s32 index)
{
    InvItem      *item      = invGetItemByIndex(index);
    ITEM_IDS      weaponnum = 0;
    textoverride *override;

    if (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = item->type_inv_item.type_prop.prop;
            override         = invGetOverrideForObj(prop->obj);

            if (override)
            {
                if (override->titletext2)
                {
                    return langGet(override->titletext2);
                }

                weaponnum = override->weapon;
            }
        }
        else if (item->type == INV_ITEM_WEAPON)
        {
            weaponnum = item->type_inv_item.type_weap.weapon;
            override  = invGetOverrideForWeapon(weaponnum);

            if (override && override->titletext2)
            {
                return langGet(override->titletext2);
            }
        }
    }
    else
    {
        if (g_CurrentPlayer->equipallguns)
        {
            if (index < ITEM_TANKSHELLS)
            {
                return itemGetWatchTitle2(index + 1);
            }
        }
    }

    return itemGetWatchTitle2(weaponnum);
}
extern f32 itemGetWatchFov(s32 item);
f32 invGetWatchFovForIndex(int index)
{
    return itemGetWatchFov(invGetWeaponForIndex(index));
}

int invGetWatchPosXForIndex(int index)
{
    return itemGetWatchPosX(invGetWeaponForIndex(index));
}

int invGetWatchPosYForIndex(int index)
{
    return itemGetWatchPosY(invGetWeaponForIndex(index));
}

int invGetWatchPosZForIndex(int index)
{
    return itemGetWatchPosZ(invGetWeaponForIndex(index));
}

int invGetWatchRotXForIndex(int index)
{
    return itemGetWatchRotX(invGetWeaponForIndex(index));
}

int invGetWatchRotYForIndex(int index)
{
    return itemGetWatchRotY(invGetWeaponForIndex(index));
}

/**
 * Co-op respawn: empty the current player's inventory but keep the objects
 * they picked up (keys, mission items like Silo's DAT tape), so a death
 * can't make the mission impossible. The level's item names stay as well.
 */
void invResetKeepProps(void)
{
    PropRecord   *props[32];
    s32           count     = 0;
    textoverride *overrides = g_CurrentPlayer->textoverrides;
    InvItem      *item      = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    s32           i;

    while (item && (count < (s32)(sizeof(props) / sizeof(props[0]))))
    {
        if (item->type == INV_ITEM_PROP)
        {
            props[count++] = item->type_inv_item.type_prop.prop;
        }

        item = item->next;

        if (item == g_CurrentPlayer->ptr_inventory_first_in_cycle)
        {
            break;
        }
    }

    invReset();
    g_CurrentPlayer->textoverrides = overrides;

    for (i = 0; i < count; i++)
    {
        invAddProp(props[i]);
    }
}

void invAddTextOverride(textoverride *override)
{
    override->next                 = g_CurrentPlayer->textoverrides;
    g_CurrentPlayer->textoverrides = override;

    // Co-op: the setup's renames (e.g. Dam's "covert modem") are loaded
    // with the last player current, but every player's inventory uses them.
    if (g_CoopMode)
    {
        s32 i;

        for (i = 0; i < getPlayerCount(); i++)
        {
            g_playerPointers[i]->textoverrides = override;
        }
    }
}

int invGetCurEquippedItem(void)
{
    return g_CurrentPlayer->equipcuritem;
}

void invSetCurEquippedItem(int current_item)
{
    g_CurrentPlayer->equipcuritem = current_item;
}

void invDetermineEquippedItem(void)
{
    s32 current_weapon;
    s32 i;

    current_weapon = gunGetWeapon(GUNRIGHT);

    g_CurrentPlayer->equipcuritem = ITEM_UNARMED;

    for (i = 0; i < invCountItems(); i++)
    {
        if (invGetWeaponForIndex(i) == current_weapon)
        {
            g_CurrentPlayer->equipcuritem = i;
            return;
        }
    }
}

u8 *invGetActivatedTextObject(ObjectRecord *obj)
{
    textoverride *override = invGetOverrideForObj(obj);

    if (override && override->pickuptext)
    {
        return langGet(override->pickuptext);
    }

    return NULL;
}

u8 *invGetActivatedTextWeapon(ITEM_IDS weaponnum)
{
    textoverride *override = invGetOverrideForWeapon(weaponnum);

    if (override && override->pickuptext)
    {
        return langGet(override->pickuptext);
    }

    return NULL;
}

void invIncrementHeldTime(s32 weapon1, s32 weapon2)
{
    s32 leastusedtime;
    s32 leastusedindex;
    s32 i;

    if (!weaponHasFlag(weapon1, WEAPONSTATBITFLAG_USE_HOLD_TIME))
    {
        return;
    }

    leastusedtime  = 0x7fffffff;
    leastusedindex = 0;

    if (!weaponHasFlag(weapon2, WEAPONSTATBITFLAG_USE_HOLD_TIME))
    {
        weapon2 = ITEM_UNARMED;
    }

    for (i = 0; i < 10; i++)
    {
        s32 time = g_CurrentPlayer->gunheldarr[i].totaltime;

        if (time >= 0)
        {
            if (weapon1 == g_CurrentPlayer->gunheldarr[i].weapon1 &&
                weapon2 == g_CurrentPlayer->gunheldarr[i].weapon2)
            {
                g_CurrentPlayer->gunheldarr[i].totaltime = time + g_ClockTimer;
                break;
            }

            if (time < leastusedtime)
            {
                leastusedtime  = time;
                leastusedindex = i;
            }
        }
        else
        {
            leastusedindex = i;
            i              = 10;
            break;
        }
    }

    if (i == 10)
    {
        g_CurrentPlayer->gunheldarr[leastusedindex].totaltime = g_ClockTimer;
        g_CurrentPlayer->gunheldarr[leastusedindex].weapon1   = weapon1;
        g_CurrentPlayer->gunheldarr[leastusedindex].weapon2   = weapon2;
    }
}

s32 invGetWeaponOfChoice(s32 *weapon1, s32 *weapon2)
{
    s32 mosttime = -1;
    s32 i;

    *weapon1 = ITEM_UNARMED;
    *weapon2 = ITEM_UNARMED;

    for (i = 0; i < 10; i++)
    {
        if (g_CurrentPlayer->gunheldarr[i].totaltime >= 0 && g_CurrentPlayer->gunheldarr[i].totaltime > mosttime)
        {
            mosttime = g_CurrentPlayer->gunheldarr[i].totaltime;
            *weapon1 = g_CurrentPlayer->gunheldarr[i].weapon1;
            *weapon2 = g_CurrentPlayer->gunheldarr[i].weapon2;
        }
    }
}


void invResetHeldTimes(void) {
    s32 i;
  
    g_CurrentPlayer->equipallguns = FALSE;
    
    for (i = 0; i != 10; i++) {
        g_CurrentPlayer->gunheldarr[i].totaltime = -1;
    }
}

void invAllocate(s32 additionalentries) {
  g_CurrentPlayer->equipmaxitems = additionalentries + 0x1e;
    g_CurrentPlayer->p_itemcur     = mempAlloc((g_CurrentPlayer->equipmaxitems * 0x14 + 0xfU | 0xf) ^ 0xf, MEMPOOL_STAGE);
  invReset();
}
