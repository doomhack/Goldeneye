/**
 * The multiplayer weapon sets ("Pistols", "Rockets"...): the weapons, models and ammo each
 * set puts in a level, and which set is picked. See docs/subsystems/menu.md.
 */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/menu/mp_weapon.h"
#include "assets/obseg/text/LmpweaponsE.h"
#include "game/player/player.h"
#include "game/player/gun.h"
#include "game/player/ammo.h"
#include "game/player/playerbody.h"
// data
struct s_mp_weapon_set mp_weapon_set_slaps[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_UNARMED, PROP_CHRTT33, 1.0, AMMO_9MM, 0, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UNARMED, PROP_CHRTT33, 1.0, AMMO_9MM, 0, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UNARMED, PROP_CHRTT33, 1.0, AMMO_9MM, 0, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UNARMED, PROP_CHRTT33, 1.0, AMMO_9MM, 0, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UNARMED, PROP_CHRTT33, 1.0, AMMO_9MM, 0, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UNARMED, PROP_CHRTT33, 1.0, AMMO_9MM, 0, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UNARMED, PROP_CHRTT33, 1.0, AMMO_9MM, 0, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UNARMED, PROP_CHRTT33, 1.0, AMMO_9MM, 0, 1)
};

struct s_mp_weapon_set mp_weapon_set_pistols[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_WPPKSIL, PROP_CHRWPPKSIL, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_WPPKSIL, PROP_CHRWPPKSIL, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_WPPKSIL, PROP_CHRWPPKSIL, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_RUGER, PROP_CHRRUGER, 1.0, AMMO_MAGNUM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_RUGER, PROP_CHRRUGER, 1.0, AMMO_MAGNUM, 0x32, 1)
};

struct s_mp_weapon_set mp_weapon_set_knife[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_THROWKNIFE, PROP_CHRTHROWKNIFE, 1.0, AMMO_KNIFE, 0xA, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_THROWKNIFE, PROP_CHRTHROWKNIFE, 1.0, AMMO_KNIFE, 0xA, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_THROWKNIFE, PROP_CHRTHROWKNIFE, 1.0, AMMO_KNIFE, 0xA, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_THROWKNIFE, PROP_CHRTHROWKNIFE, 1.0, AMMO_KNIFE, 0xA, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_THROWKNIFE, PROP_CHRTHROWKNIFE, 1.0, AMMO_KNIFE, 0xA, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_THROWKNIFE, PROP_CHRTHROWKNIFE, 1.0, AMMO_KNIFE, 0xA, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_THROWKNIFE, PROP_CHRTHROWKNIFE, 1.0, AMMO_KNIFE, 0xA, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_THROWKNIFE, PROP_CHRTHROWKNIFE, 1.0, AMMO_KNIFE, 0xA, 0)
};

struct s_mp_weapon_set mp_weapon_set_auto[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_WPPKSIL, PROP_CHRWPPKSIL, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_WPPKSIL, PROP_CHRWPPKSIL, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_MP5K, PROP_CHRMP5K, 1.0, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_MP5K, PROP_CHRMP5K, 1.0, AMMO_9MM, 0x64, 1)
};

struct s_mp_weapon_set mp_weapon_set_power[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_RUGER, PROP_CHRRUGER, 1.0, AMMO_MAGNUM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_RUGER, PROP_CHRRUGER, 1.0, AMMO_MAGNUM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_FNP90, PROP_CHRFNP90, 1.0, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_FNP90, PROP_CHRFNP90, 1.0, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AUTOSHOT, PROP_CHRAUTOSHOT, 1.0, AMMO_SHOTGUN, 0x1E, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AUTOSHOT, PROP_CHRAUTOSHOT, 1.0, AMMO_SHOTGUN, 0x1E, 1)
};

struct s_mp_weapon_set mp_weapon_set_sniper[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_RUGER, PROP_CHRRUGER, 1.0, AMMO_MAGNUM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_RUGER, PROP_CHRRUGER, 1.0, AMMO_MAGNUM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.0, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.0, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SNIPERRIFLE, PROP_CHRSNIPERRIFLE, 1.0, AMMO_RIFLE, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SNIPERRIFLE, PROP_CHRSNIPERRIFLE, 1.0, AMMO_RIFLE, 0x32, 1)
};

struct s_mp_weapon_set mp_weapon_set_grenade[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_GRENADE, PROP_CHRGRENADE, 1.5, AMMO_GRENADE, 5, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_GRENADE, PROP_CHRGRENADE, 1.5, AMMO_GRENADE, 5, 0)
};

struct s_mp_weapon_set mp_weapon_set_remote_m[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_WPPK, PROP_CHRWPPK, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_WPPK, PROP_CHRWPPK, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UZI, PROP_CHRUZI, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UZI, PROP_CHRUZI, 1.5, AMMO_9MM, 0x64, 1),
#ifdef BUGFIX_R0
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
#else
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRM16, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRM16, 1.5, AMMO_RIFLE, 0x64, 1),
#endif
    INLINE_S_MP_WEAPON_SET(ITEM_REMOTEMINE, PROP_CHRREMOTEMINE, 1.5, AMMO_REMOTEMINE, 5, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_REMOTEMINE, PROP_CHRREMOTEMINE, 1.5, AMMO_REMOTEMINE, 5, 0)
};


struct s_mp_weapon_set mp_weapon_set_glaunch[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_GRENADELAUNCH, PROP_CHRGRENADELAUNCH, 1.0, AMMO_GRENADEROUND, 6, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_GRENADELAUNCH, PROP_CHRGRENADELAUNCH, 1.0, AMMO_GRENADEROUND, 6, 1)
};

struct s_mp_weapon_set mp_weapon_set_timed_m[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_WPPK, PROP_CHRWPPK, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_WPPK, PROP_CHRWPPK, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UZI, PROP_CHRUZI, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UZI, PROP_CHRUZI, 1.5, AMMO_9MM, 0x64, 1),
#ifdef BUGFIX_R0
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
#else
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRM16, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRM16, 1.5, AMMO_RIFLE, 0x64, 1),
#endif
    INLINE_S_MP_WEAPON_SET(ITEM_TIMEDMINE, PROP_CHRTIMEDMINE, 1.5, AMMO_TIMEDMINE, 5, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_TIMEDMINE, PROP_CHRTIMEDMINE, 1.5, AMMO_TIMEDMINE, 5, 0)
};

struct s_mp_weapon_set mp_weapon_set_prox_m[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_WPPK, PROP_CHRWPPK, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_WPPK, PROP_CHRWPPK, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UZI, PROP_CHRUZI, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UZI, PROP_CHRUZI, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRM16, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_M16, PROP_CHRM16, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_PROXIMITYMINE, PROP_CHRPROXIMITYMINE, 1.5, AMMO_PROXMINE, 5, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_PROXIMITYMINE, PROP_CHRPROXIMITYMINE, 1.5, AMMO_PROXMINE, 5, 0)
};

struct s_mp_weapon_set mp_weapon_set_rockets[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_ROCKETLAUNCH, PROP_CHRROCKETLAUNCH, 1.5, AMMO_ROCKETS, 6, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_ROCKETLAUNCH, PROP_CHRROCKETLAUNCH, 1.5, AMMO_ROCKETS, 6, 1)
};

struct s_mp_weapon_set mp_weapon_set_lasers[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_LASER, PROP_CHRLASER, 1.5, AMMO_NONE, 0, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_LASER, PROP_CHRLASER, 1.5, AMMO_NONE, 0, 1)
};

struct s_mp_weapon_set mp_weapon_set_golden[] = 
{
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_TT33, PROP_CHRTT33, 3.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_SKORPION, PROP_CHRSKORPION, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_AK47, PROP_CHRKALASH, 1.5, AMMO_RIFLE, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_WPPKSIL, PROP_CHRWPPKSIL, 1.0, AMMO_9MM, 0x32, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_GOLDENGUN, PROP_CHRGOLDEN, 1.5, AMMO_GGUN, 0xA, 1)
};

// ZMGs, proximity mines (ammo crates only, as in the Proximity Mines set), grenade
// launchers and golden guns, two slots each.
struct s_mp_weapon_set mp_weapon_set_fire_gold[] =
{
    INLINE_S_MP_WEAPON_SET(ITEM_UZI, PROP_CHRUZI, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_UZI, PROP_CHRUZI, 1.5, AMMO_9MM, 0x64, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_PROXIMITYMINE, PROP_CHRPROXIMITYMINE, 1.5, AMMO_PROXMINE, 5, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_PROXIMITYMINE, PROP_CHRPROXIMITYMINE, 1.5, AMMO_PROXMINE, 5, 0),
    INLINE_S_MP_WEAPON_SET(ITEM_GRENADELAUNCH, PROP_CHRGRENADELAUNCH, 1.0, AMMO_GRENADEROUND, 6, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_GRENADELAUNCH, PROP_CHRGRENADELAUNCH, 1.0, AMMO_GRENADEROUND, 6, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_GOLDENGUN, PROP_CHRGOLDEN, 1.5, AMMO_GGUN, 0xA, 1),
    INLINE_S_MP_WEAPON_SET(ITEM_GOLDENGUN, PROP_CHRGOLDEN, 1.5, AMMO_GGUN, 0xA, 1)
};

static bool s_CustomBuilt;
static void mpCustomWeaponBuild(void);

// The custom set, built from s_CustomWeapons by mpCustomWeaponSetSlot. It starts
// as the Power Weapons set until a saved one is loaded.
struct s_mp_weapon_set mp_weapon_set_custom[MP_WEAPON_SET_SLOTS];

struct s_mp_weapon_set_text g_MpWeaponSets[] = 
{
    {getStringID(LMPWEAPONS, MPWEAPON_STR_00_SLAPPERSONLY), mp_weapon_set_slaps},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_01_PISTOLS), mp_weapon_set_pistols},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_0D_THROWINGKNIVES), mp_weapon_set_knife},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_02_AUTOMATICS), mp_weapon_set_auto},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_03_POWERWEAPONS), mp_weapon_set_power},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_04_SNIPERRIFLES), mp_weapon_set_sniper},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_05_GRENADES), mp_weapon_set_grenade},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_06_REMOTEMINES), mp_weapon_set_remote_m},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_07_GRENADELAUNCHERS), mp_weapon_set_glaunch},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_08_TIMEDMINES), mp_weapon_set_timed_m},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_09_PROXIMITYMINES), mp_weapon_set_prox_m},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_0A_ROCKETS), mp_weapon_set_rockets},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_0B_LASERS), mp_weapon_set_lasers},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_0C_GOLDENGUN), mp_weapon_set_golden},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_0E_FIREANDGOLD), mp_weapon_set_fire_gold},
    {getStringID(LMPWEAPONS, MPWEAPON_STR_0F_CUSTOM), mp_weapon_set_custom}
};

_Static_assert(ARRAYCOUNT(g_MpWeaponSets) == MP_WEAPON_SET_COUNT, "MP_WEAPON_SET_COUNT");

s32 g_MpWeaponSet = 0xB;


// steps to the next weapon set, wrapping round to the first
void mpWeaponSetNext(void)
{
    g_MpWeaponSet = (g_MpWeaponSet + 1) % MP_WEAPON_SET_COUNT;
}

//text ID of the selected g_MpWeaponSet
u16 mpWeaponSetGetTextId(void)
{
    return g_MpWeaponSets[g_MpWeaponSet].textID;
}

//return pointer to selected g_MpWeaponSet data
struct s_mp_weapon_set* mpWeaponSetGetItems(void)
{
    if (!s_CustomBuilt)
    {
        mpCustomWeaponBuild();
    }

    return g_MpWeaponSets[g_MpWeaponSet].weapon_set;
}

// text ID of any weapon set
u16 mpWeaponSetGetTextIdFor(s32 setnum)
{
    return g_MpWeaponSets[setnum].textID;
}

//set mp weapon set
void mpWeaponSetSelect(s32 setNUM)
{
    g_MpWeaponSet = setNUM;
}

//return mp weapon set
s32 mpWeaponSetGetIndex(void)
{
    return g_MpWeaponSet;
}


/**
 * The weapons a custom set slot can hold, with the pickup's model scale, the ammo in the
 * crate placed after it and whether the weapon itself is placed (mines, grenades and
 * throwing knives only put down their ammo crate, as in the stock sets). Values follow the
 * stock sets. The ammo type and model come from the weapon.
 */
struct custom_weapon
{
    u8 item;
    u8 size16;     // pickup model scale * 16
    u8 ammoamount;
    u8 allowpickup;
};

static const struct custom_weapon s_CustomWeapons[] =
{
    {ITEM_UNARMED,       16,   0, 1},
    {ITEM_KNIFE,         16,   0, 1},
    {ITEM_THROWKNIFE,    16,  10, 0},
    {ITEM_WPPK,          16,  50, 1},
    {ITEM_WPPKSIL,       16,  50, 1},
    {ITEM_TT33,          16,  50, 1},
    {ITEM_SKORPION,      24, 100, 1},
    {ITEM_AK47,          24, 100, 1},
    {ITEM_UZI,           24, 100, 1},
    {ITEM_MP5K,          16, 100, 1},
    {ITEM_MP5KSIL,       16, 100, 1},
    {ITEM_SPECTRE,       16, 100, 1},
    {ITEM_M16,           24, 100, 1},
    {ITEM_FNP90,         16, 100, 1},
    {ITEM_SHOTGUN,       16,  30, 1},
    {ITEM_AUTOSHOT,      16,  30, 1},
    {ITEM_SNIPERRIFLE,   16,  50, 1},
    {ITEM_RUGER,         16,  50, 1},
    {ITEM_GOLDENGUN,     24,  10, 1},
    {ITEM_SILVERWPPK,    16,  50, 1},
    {ITEM_GOLDWPPK,      16,  50, 1},
    {ITEM_LASER,         24,   0, 1},
    {ITEM_GRENADELAUNCH, 16,   6, 1},
    {ITEM_ROCKETLAUNCH,  24,   6, 1},
    {ITEM_GRENADE,       24,   5, 0},
    {ITEM_TIMEDMINE,     24,   5, 0},
    {ITEM_PROXIMITYMINE, 24,   5, 0},
    {ITEM_REMOTEMINE,    24,   5, 0},
};

#define CUSTOM_WEAPON_COUNT ARRAYCOUNT(s_CustomWeapons)

// Power Weapons: DD44s, Cougars, RC-P90s, automatic shotguns
static u8 s_CustomChoices[MP_WEAPON_SET_SLOTS] = {5, 5, 17, 17, 13, 13, 15, 15};

static void mpCustomWeaponSetSlot(s32 slot, s32 choice)
{
    const struct custom_weapon *weapon = &s_CustomWeapons[choice];
    struct s_mp_weapon_set *entry = &mp_weapon_set_custom[slot];
    s32 prop = weaponGetChrModel(weapon->item);
    s32 ammotype = weaponGetAmmoType(weapon->item);

    s_CustomChoices[slot] = choice;

    // the crate needs an ammo type even when it is empty
    if (ammotype <= AMMO_NONE || ammotype > AMMOTYPE_GLOBAL_MAX)
    {
        ammotype = AMMO_9MM;
    }

    entry->itemID = weapon->item;
    entry->propID = (prop >= 0) ? prop : PROP_CHRTT33;
    entry->ammotype = ammotype;
    entry->ammoamount = weapon->ammoamount;
    entry->allowpickup = weapon->allowpickup;
    entry->size = weapon->size16 / 16.0f;
}

static void mpCustomWeaponBuild(void)
{
    s32 slot;

    for (slot = 0; slot < MP_WEAPON_SET_SLOTS; slot++)
    {
        mpCustomWeaponSetSlot(slot, s_CustomChoices[slot]);
    }

    s_CustomBuilt = TRUE;
}

/**
 * The custom set's weapon choice (an index into the custom weapon list) for a slot.
 */
s32 mpCustomWeaponGetChoice(s32 slot)
{
    return s_CustomChoices[slot];
}

/**
 * Steps a custom set slot to the next (step 1) or previous (step -1) weapon.
 */
void mpCustomWeaponStep(s32 slot, s32 step)
{
    if (!s_CustomBuilt)
    {
        mpCustomWeaponBuild();
    }

    mpCustomWeaponSetSlot(slot, (s_CustomChoices[slot] + step + CUSTOM_WEAPON_COUNT) % CUSTOM_WEAPON_COUNT);
}

/**
 * The name of the weapon in a custom set slot, as the end-of-game stats show it.
 */
const char *mpCustomWeaponGetName(s32 slot)
{
    ITEM_IDS item = s_CustomWeapons[s_CustomChoices[slot]].item;

    if (item == ITEM_UNARMED)
    {
        return "Slappers\n";
    }

    return itemGetChoiceText(item);
}

/**
 * Sets the custom set from saved choices. Choices out of range keep their default.
 */
void mpCustomWeaponLoad(const u8 *choices)
{
    s32 slot;

    for (slot = 0; slot < MP_WEAPON_SET_SLOTS; slot++)
    {
        if (choices[slot] < CUSTOM_WEAPON_COUNT)
        {
            s_CustomChoices[slot] = choices[slot];
        }
    }

    mpCustomWeaponBuild();
}

void mpCustomWeaponSave(u8 *choices)
{
    s32 slot;

    for (slot = 0; slot < MP_WEAPON_SET_SLOTS; slot++)
    {
        choices[slot] = s_CustomChoices[slot];
    }
}


/**
 * Multiplayer option: picking up a second copy of a weapon you carry gives you the pair.
 */
static bool s_MpDualWield = FALSE;

bool mpDualWieldIsOn(void)
{
    return s_MpDualWield;
}

void mpDualWieldSet(bool on)
{
    s_MpDualWield = on;
}
