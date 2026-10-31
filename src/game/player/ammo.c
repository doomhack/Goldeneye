/**
 * Ammo: the per-type carry limits and HUD icons (g_AmmoStats), giving
 * ammo, and reading what is held or loaded for each hand or weapon.
 */
#include "assets/obseg/text/LgunE.h"
#include "game/menu/watchmenu.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/player/ammo.h"

/**
 * Per ammo type: the most a player can carry, the HUD icon and the icon's
 * vertical offset.
 */
AmmoStats g_AmmoStats[] = {
    { 0x0    , 0x00000000,   0.0f, },
    { 0x320  , 0x02000C84,   0.0f, },
    { 0xC8   , 0x00000000,   0.0f, },
    { 0x190  , 0x02000C90,  -2.0f, },
    { 0x64   , 0x02000C9C,   0.0f, },
    { 0xC    , 0x02000CD8,   0.0f, },
    { 0x3    , 0x02000CC0,  -2.0f, },
    { 0xA    , 0x02000CFC,   1.0f, },
    { 0xA    , 0x02000D14,   1.0f, },
    { 0xA    , 0x02000D08,   1.0f, },
    { 0xA    , 0x02000CA8,   0.0f, },
    { 0xC    , 0x02000CB4,   0.0f, },
    { 0xC8   , 0x02000CE4,   0.0f, },
    { 0x64   , 0x02000CF0,   0.0f, },
    { 0x32   , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x2    , 0x00000000,   0.0f, },
    { 0x8    , 0x00000000,   0.0f, },
    { 0x6    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x1    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x3E8  , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x32   , 0x02000D20,  -1.0f, },
    { 0x1    , 0x00000000,   0.0f, },
};

/* Unused. Kept after g_AmmoStats[] in case anything reads past the end of it. */
u16 g_AmmoStatsPad[] = { 0, 0, 0, 0, };


/**
 * Sets how much of an ammo type the current player holds, capped at the
 * carry limit. For guns that only hold a clip, it fills the clip instead.
 */
void ammoGive(s32 ammo_type, s32 ammo_amount) {
    enum ITEM_IDS weapon_id;
    s32 max_ammo;

    weapon_id = gunGetWeapon(GUNRIGHT);
    if ((weaponGetAmmoType(weapon_id) == ammo_type) && (weaponHasFlag(weapon_id, WEAPONSTATBITFLAG_AMMO_CLIP_LIMIT) != 0))
    {
        g_CurrentPlayer->hands[0].weapon_ammo_in_magazine += ammo_amount;
        if (weaponGetStats(weapon_id)->MagSize < g_CurrentPlayer->hands[0].weapon_ammo_in_magazine)
        {
            g_CurrentPlayer->hands[0].weapon_ammo_in_magazine = (s32) weaponGetStats(weapon_id)->MagSize;
        }
        g_CurrentPlayer->ammoheldarr[ammo_type] = 0;
        return;
    }

    max_ammo = g_AmmoStats[ammo_type].MaxAmmo;
    if (max_ammo < ammo_amount)
    {
        g_CurrentPlayer->ammoheldarr[ammo_type] = max_ammo;
        return;
    }

    g_CurrentPlayer->ammoheldarr[ammo_type] = ammo_amount;
}


/**
 * Ammo held in reserve (not loaded).
 */
s32 ammoGetHeld(AMMOTYPE ammotype) {
    return g_CurrentPlayer->ammoheldarr[ammotype];
}


/**
 * Ammo held plus what is loaded in either hand.
 */
s32 ammoGetTotal(AMMOTYPE ammotype) {

    s32 total_ammo = ammoGetHeld(ammotype);

    if (weaponGetAmmoType(gunGetWeapon(GUNRIGHT)) == ammotype) {
        total_ammo += gunGetMagazineAmmo(GUNRIGHT);
    }

    if (weaponGetAmmoType(gunGetWeapon(GUNLEFT)) == ammotype) {
        total_ammo += gunGetMagazineAmmo(GUNLEFT);
    }

    return total_ammo;
}


/**
 * The most of an ammo type a player can carry.
 */
s32 ammoGetMax(s32 ammotype)
{
    return g_AmmoStats[ammotype].MaxAmmo;
}


/**
 * Gives the current player full ammo of every type.
 */
void ammoGiveMaxAll(void)
{
    s32 ammo_type;

    for (ammo_type = 0; ammo_type < (s32)ARRAYCOUNT(g_AmmoStats); ammo_type++)
    {
        ammoGive(ammo_type, g_AmmoStats[ammo_type].MaxAmmo);
    }
}


/**
 * Ammo loaded in a hand's gun.
 */
s32 gunGetMagazineAmmo(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine;
}


/**
 * Reserve ammo for a hand's gun. With shotguns in both hands, the shells
 * shown loading in the other hand are taken off.
 */
s32 gunGetHeldAmmo(enum GUNHAND hand)
{
    s32 weapon_id;
    s32 ammo_count;

    weapon_id = gunGetWeapon(hand);
    ammo_count = weaponGetHeldAmmo(weapon_id);

    if ((weapon_id == ITEM_SHOTGUN) || (weapon_id == ITEM_AUTOSHOT))
    {
        s32 other_weapon_id;
        other_weapon_id = gunGetWeapon(1 - hand);

        if ((other_weapon_id == ITEM_SHOTGUN) || (other_weapon_id == ITEM_AUTOSHOT))
        {
            return ammo_count - g_CurrentPlayer->hands[1 - hand].numvisibleshells;
        }
    }

    return ammo_count;
}


/**
 * The ammo type a weapon uses.
 */
s32 weaponGetAmmoType(ITEM_IDS weapon) {
    return weaponGetStats(weapon)->AmmoType;
}


/**
 * Reserve ammo for a weapon's ammo type.
 */
s32 weaponGetHeldAmmo(ITEM_IDS weapon) {
  WeaponStats *weaponstats = weaponGetStats(weapon);
  return g_CurrentPlayer->ammoheldarr[weaponstats->AmmoType];
}


/**
 * ammoGive for a weapon's ammo type.
 */
void weaponGiveAmmo(ITEM_IDS weapon, s32 ammo) {
    ammoGive(weaponGetStats(weapon)->AmmoType, ammo);
}


/**
 * Carry limit for a weapon's ammo type.
 */
s32 weaponGetMaxAmmo(enum ITEM_IDS weapon)
{
    return g_AmmoStats[weaponGetStats(weapon)->AmmoType].MaxAmmo;
}
