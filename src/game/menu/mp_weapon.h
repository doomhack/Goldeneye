#ifndef _MP_WEAPON_H_
#define _MP_WEAPON_H_
#include <ultra64.h>

struct s_mp_weapon_set
{
    /**
     * enum ITEM_IDS
    */
    s32 itemID;
    /**
     * enum PROP
    */
    s32 propID;
    f32 size;
    s32 ammotype;
    s32 ammoamount;
    s32 allowpickup;
};

#define INLINE_S_MP_WEAPON_SET(item_id, prop_id, size, ammo_type, ammo_amount, allow_pickup) { (s32)(item_id), (s32)(prop_id), (f32)(size), (s32)(ammo_type), (s32)(ammo_amount), (s32)(allow_pickup) }

struct s_mp_weapon_set_text
{
  u16 textID;
  struct s_mp_weapon_set * weapon_set;
};

extern struct s_mp_weapon_set mp_weapon_set_slaps[];
extern struct s_mp_weapon_set mp_weapon_set_pistols[];
extern struct s_mp_weapon_set mp_weapon_set_knife[];
extern struct s_mp_weapon_set mp_weapon_set_auto[];
extern struct s_mp_weapon_set mp_weapon_set_power[];
extern struct s_mp_weapon_set mp_weapon_set_sniper[];
extern struct s_mp_weapon_set mp_weapon_set_grenade[];
extern struct s_mp_weapon_set mp_weapon_set_remote_m[];
extern struct s_mp_weapon_set mp_weapon_set_glaunch[];
extern struct s_mp_weapon_set mp_weapon_set_timed_m[];
extern struct s_mp_weapon_set mp_weapon_set_prox_m[];
extern struct s_mp_weapon_set mp_weapon_set_rockets[];
extern struct s_mp_weapon_set mp_weapon_set_lasers[];
extern struct s_mp_weapon_set mp_weapon_set_golden[];
extern struct s_mp_weapon_set mp_weapon_set_fire_gold[];

/* Weapon set indices the scenarios force */
#define MP_WEAPON_SET_PISTOLS    1
#define MP_WEAPON_SET_GOLDEN_GUN 13
#define MP_WEAPON_SET_CUSTOM     15
#define MP_WEAPON_SET_COUNT      16

#define MP_WEAPON_SET_SLOTS      8

void mpWeaponSetNext(void);
u16 mpWeaponSetGetTextId(void);
struct s_mp_weapon_set* mpWeaponSetGetItems(void);
void mpWeaponSetSelect(s32 setNUM);
s32 mpWeaponSetGetIndex(void);
u16 mpWeaponSetGetTextIdFor(s32 setnum);

s32 mpCustomWeaponGetChoice(s32 slot);
void mpCustomWeaponStep(s32 slot, s32 step);
const char *mpCustomWeaponGetName(s32 slot);
void mpCustomWeaponLoad(const u8 *choices);
void mpCustomWeaponSave(u8 *choices);

bool mpDualWieldIsOn(void);
void mpDualWieldSet(bool on);

#endif
