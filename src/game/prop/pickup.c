/**
 * Picking things up and using them.
 *
 * Each frame propsTickPickups calls objTickPlayer for the props near the current player.
 * It checks whether the player wants the prop (a weapon they already have with full ammo
 * is left lying) and is close enough, then propPickupByPlayer collects it: it adds the
 * weapon, ammo, armour or item, plays the pickup sound and shows the "Picked up..." text.
 * Props flagged PROPFLAG_PICKUPONINTERACT (and switches, consoles and alarms) are not
 * walked over but used with the action button: objTestForInteract finds the one in front
 * of the player, and objInteract uses it.
 */

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <snd.h>
#include <music.h>
#include "str.h"
#include "game/prop/pickup.h"
#include "game/prop/propobj.h"
#include "game/prop/prop.h"
#include "game/prop/alarm.h"
#include "game/prop/objdamage.h"
#include "game/player/inventory.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/menu/front.h"
#include "game/nav/stan.h"
#include "game/text/language.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/audio/sfxvolume.h"
#include "assets/obseg/text/LpropobjE.h"

/* How close the player must be to walk over a prop and pick it up, and, with the watch
   magnet on, to pull it in. The magnet works once it has been on for a second. */
#define PICKUP_RANGE_SQ          (100.0f * 100.0f)
#define PICKUP_HEIGHT            200.0f
#define MAGNET_RANGE_SQ          (350.0f * 350.0f)
#define MAGNET_HEIGHT            500.0f
#define MAGNET_WARMUP_TICKS      60

/* Without the magnet, a player looking further down than this doesn't pick things up */
#define PICKUP_MAX_LOOK_DOWN     -0.7853982f

static char *pickupText(s32 slot)
{
    return langGet(getStringID(LPROPOBJ, slot));
}

/* Text ids are never 0 ("Picked up "), so 0 marks "no text" in the tables below */

/* The article before an ammo name: "a" for one, "some" for more. Ammo that is only ever
   counted loosely ("some ammo") uses "some" for one as well. */
static const u8 s_AmmoArticles[AMMOTYPE_MAX] = {
    [AMMO_9MM]          = PROPOBJ_STR_01_SOME,
    [AMMO_9MM_2]        = PROPOBJ_STR_01_SOME,
    [AMMO_RIFLE]        = PROPOBJ_STR_01_SOME,
    [AMMO_PLASTIQUE]    = PROPOBJ_STR_01_SOME,
    [AMMO_SHOTGUN]      = PROPOBJ_STR_02_A,
    [AMMO_GRENADE]      = PROPOBJ_STR_02_A,
    [AMMO_ROCKETS]      = PROPOBJ_STR_02_A,
    [AMMO_REMOTEMINE]   = PROPOBJ_STR_02_A,
    [AMMO_PROXMINE]     = PROPOBJ_STR_02_A,
    [AMMO_TIMEDMINE]    = PROPOBJ_STR_02_A,
    [AMMO_KNIFE]        = PROPOBJ_STR_02_A,
    [AMMO_GRENADEROUND] = PROPOBJ_STR_02_A,
    [AMMO_MAGNUM]       = PROPOBJ_STR_02_A,
    [AMMO_GGUN]         = PROPOBJ_STR_02_A,
    [AMMO_DARTS]        = PROPOBJ_STR_02_A,
    [AMMO_FLARE]        = PROPOBJ_STR_02_A,
    [AMMO_PITON]        = PROPOBJ_STR_02_A,
    [AMMO_DYNAMITE]     = PROPOBJ_STR_02_A,
    [AMMO_BUG]          = PROPOBJ_STR_02_A,
    [AMMO_MICRO_CAMERA] = PROPOBJ_STR_02_A,
    [AMMO_EXPLOSIVEPEN] = PROPOBJ_STR_03_AN,
    [AMMO_BOMBCASE]     = PROPOBJ_STR_03_AN,
    [AMMO_GEKEY]        = PROPOBJ_STR_04_THE,
    [AMMO_TOKEN]        = PROPOBJ_STR_04_THE,
};

/* Ammo names that take an "s" for more than one. Bullets, knives and dynamite are
   worded separately (pickupAppendAmmoName). */
static const u8 s_AmmoNames[AMMOTYPE_MAX] = {
    [AMMO_SHOTGUN]      = PROPOBJ_STR_06_SHOTGUNCARTRIDGE,
    [AMMO_MAGNUM]       = PROPOBJ_STR_07_MAGNUMBULLET,
    [AMMO_GGUN]         = PROPOBJ_STR_08_GOLDENBULLET,
    [AMMO_GRENADE]      = PROPOBJ_STR_09_HANDGRENADE,
    [AMMO_GRENADEROUND] = PROPOBJ_STR_0A_GRENADEROUND,
    [AMMO_ROCKETS]      = PROPOBJ_STR_0B_ROCKET,
    [AMMO_REMOTEMINE]   = PROPOBJ_STR_0C_REMOTEMINE,
    [AMMO_PROXMINE]     = PROPOBJ_STR_0D_PROXIMITYMINE,
    [AMMO_TIMEDMINE]    = PROPOBJ_STR_0E_TIMEDMINE,
    [AMMO_DARTS]        = PROPOBJ_STR_13_DART,
    [AMMO_EXPLOSIVEPEN] = PROPOBJ_STR_14_EXPLOSIVEPEN,
    [AMMO_BOMBCASE]     = PROPOBJ_STR_15_EXPLOSIVECASE,
    [AMMO_FLARE]        = PROPOBJ_STR_16_FLARE,
    [AMMO_PITON]        = PROPOBJ_STR_17_PITON,
    [AMMO_BUG]          = PROPOBJ_STR_1B_BUG,
    [AMMO_MICRO_CAMERA] = PROPOBJ_STR_1C_MICROCAMERA,
    [AMMO_GEKEY]        = PROPOBJ_STR_1D_GOLDENEYEKEY,
    [AMMO_TOKEN]        = PROPOBJ_STR_1E_TOKEN,
    [AMMO_PLASTIQUE]    = PROPOBJ_STR_1F_PLASTIQUE,
};

/* "a PP7.", "a sniper rifle." and so on. Anything not listed is "a new weapon." */
static const u8 s_WeaponNames[ITEM_IDS_MAX] = {
    [ITEM_KNIFE]         = PROPOBJ_STR_20_AHUNTINGKNIFE,
    [ITEM_WPPK]          = PROPOBJ_STR_21_APPK,
    [ITEM_WPPKSIL]       = PROPOBJ_STR_22_ASILENCEDPPK,
    [ITEM_TT33]          = PROPOBJ_STR_23_ATT33,
    [ITEM_SKORPION]      = PROPOBJ_STR_24_ASPKORPION,
    [ITEM_AK47]          = PROPOBJ_STR_25_ANAK47,
    [ITEM_UZI]           = PROPOBJ_STR_26_ANUZI,
    [ITEM_MP5K]          = PROPOBJ_STR_27_ANMP5K,
    [ITEM_MP5KSIL]       = PROPOBJ_STR_28_ASILENCEDMP5,
    [ITEM_SPECTRE]       = PROPOBJ_STR_29_ASPECTRE,
    [ITEM_M16]           = PROPOBJ_STR_2A_ANM16,
    [ITEM_FNP90]         = PROPOBJ_STR_2B_ANFNP90,
    [ITEM_SHOTGUN]       = PROPOBJ_STR_2C_ASHOTGUN,
    [ITEM_AUTOSHOT]      = PROPOBJ_STR_2D_ANAUTOSHOTGUN,
    [ITEM_SNIPERRIFLE]   = PROPOBJ_STR_2E_ASNIPERRIFLE,
    [ITEM_GRENADELAUNCH] = PROPOBJ_STR_2F_AGRENADELAUNCHER,
    [ITEM_ROCKETLAUNCH]  = PROPOBJ_STR_30_AROCKETLAUNCHER,
    [ITEM_RUGER]         = PROPOBJ_STR_31_ARUGERMAGNUM,
    [ITEM_GOLDENGUN]     = PROPOBJ_STR_32_THEGOLDENGUN,
    [ITEM_LASER]         = PROPOBJ_STR_33_AMOOKRAKERLASER,
    [ITEM_FLAREPISTOL]   = PROPOBJ_STR_34_AFLAREPISTOL,
    [ITEM_PITONGUN]      = PROPOBJ_STR_35_APITONGUN,
    [ITEM_SILVERWPPK]    = PROPOBJ_STR_36_ASILVERPPK,
    [ITEM_GOLDWPPK]      = PROPOBJ_STR_37_AGOLDPPK,
    [ITEM_KEYCARD]       = PROPOBJ_STR_38_AKEYCARD,
    [ITEM_KEYYALE]       = PROPOBJ_STR_39_AYALEKEY,
    [ITEM_KEYBOLT]       = PROPOBJ_STR_3A_ABOLTKEY,
};

/* The inventory item that picking up some of this ammo gives (ITEM_UNARMED for none):
   thrown weapons and gadgets are items as well as ammo */
static const u8 s_AmmoItems[AMMOTYPE_MAX] = {
    [AMMO_GRENADE]      = ITEM_GRENADE,
    [AMMO_REMOTEMINE]   = ITEM_REMOTEMINE,
    [AMMO_PROXMINE]     = ITEM_PROXIMITYMINE,
    [AMMO_TIMEDMINE]    = ITEM_TIMEDMINE,
    [AMMO_KNIFE]        = ITEM_THROWKNIFE,
    [AMMO_BOMBCASE]     = ITEM_BOMBCASE,
    [AMMO_BUG]          = ITEM_BUG,
    [AMMO_MICRO_CAMERA] = ITEM_MICROCAMERA,
    [AMMO_GEKEY]        = ITEM_GOLDENEYEKEY,
    [AMMO_TOKEN]        = ITEM_TOKEN,
    [AMMO_PLASTIQUE]    = ITEM_PLASTIQUE,
};


/**
 * Find the prop the current player can use with the action button: one in front of them
 * (within 22.5 degrees), within 2 m across and up or down. Aircraft that open to the
 * front can be used from 4 m and 120 degrees. Sets g_InteractProp.
 */
bool objTestForInteract(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    PropRecord *player;
    StandTile *stan;
    f32 xdiff, ydiff, zdiff;
    f32 maxdistsq, maxydiff, maxangle;
    f32 angle, playerangle, anglediff;

    // Counter-op: of the objects, the guard player can only use alarms.
    // Consoles, switches and mission objects are Bond's.
    if (counterOpIsGuardPlayer(playerGetCurrentNum()) && obj->type != PROPDEF_ALARM)
    {
        return true;
    }

    // Usable: alarms, switches linked to doors, props collected by using them, and tagged
    // props. (The original tested PROP_TYPE_PLAYER here, which has the same value as
    // PROPDEF_ALARM.)
    if (!(obj->type == PROPDEF_ALARM
            || (obj->flags & PROPFLAG_PICKUPONINTERACT)
            || (obj->runtime_bitflags & (RUNTIMEBITFLAG_LINKEDSWITCH | RUNTIMEBITFLAG_00000002 | RUNTIMEBITFLAG_TAGGED))))
    {
        return true;
    }

    if (!(prop->flags & PROPFLAG_ONSCREEN) || !objIsHealthy(obj) || (obj->flags & PROPFLAG_CANNOT_ACTIVATE))
    {
        return true;
    }

    player = currentPlayerGetProp();

    xdiff = obj->runtime_pos.x - player->pos.x;
    ydiff = obj->runtime_pos.y - player->pos.y;
    zdiff = obj->runtime_pos.z - player->pos.z;

    stan = player->stan;

    if (obj->type == PROPDEF_AIRCRAFT && (obj->flags & PROPFLAG_DOOR_OPENTOFRONT))
    {
        maxydiff = 400.0f;
        maxdistsq = 400.0f * 400.0f;
        maxangle = 2.0943952f;
    }
    else
    {
        maxydiff = 200.0f;
        maxdistsq = 200.0f * 200.0f;
        maxangle = 0.3926991f;
    }

    if (!((xdiff * xdiff) + (zdiff * zdiff) < maxdistsq && ydiff < maxydiff && -maxydiff < ydiff))
    {
        return true;
    }

    angle = mathAtan2(xdiff, zdiff);
    playerangle = bondviewGetPlayerYawRadians();
    anglediff = angle - playerangle;

    if (angle < playerangle)
    {
        anglediff += M_TAU_F;
    }

    if (anglediff > M_PI_F)
    {
        anglediff = M_TAU_F - anglediff;
    }

    if (anglediff <= maxangle)
    {
        if (!(obj->flags2 & PROPFLAG2_INTERACTCHECKLOS)
                || stanWalkLine(&stan, player->pos.x, player->pos.z, prop->pos.x, prop->pos.z))
        {
            g_InteractProp = prop;
        }
    }

    return true;
}


/**
 * The current player uses a prop with the action button: toggles an alarm, collects a
 * prop that is picked up this way, and opens the doors a switch is linked to.
 * Returns what to do with the prop (TICKOP_NONE if it stays where it is).
 */
TICKOP objInteract(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    TICKOP op = TICKOP_NONE;

    if (obj->type == PROPDEF_ALARM)
    {
        sndPlaySfx(g_musicSfxBufferPtr, ALARM_SWITCH_SFX, 0);

        if (alarmIsActive())
        {
            alarmDeactivate();
        }
        else
        {
            alarmActivate();
        }
    }

    if (obj->flags & PROPFLAG_PICKUPONINTERACT)
    {
        op = propPickupByPlayer(prop, true);
    }

    obj->runtime_bitflags |= RUNTIMEBITFLAG_ACTIVATED;
    coopNoteActivator();
    switchActivateLinkedDoors(prop);

    return op;
}


/* "some ", "a " or "the " before an ammo name */
static void pickupAppendAmmoArticle(char *buffer, AMMOTYPE ammotype, s32 amount)
{
    s32 article = s_AmmoArticles[ammotype];

    if (article == 0)
    {
        return;
    }

    strcat(buffer, pickupText(amount == 1 ? article : PROPOBJ_STR_01_SOME));
}


static void pickupAppendAmmoName(char *buffer, AMMOTYPE ammotype, s32 amount)
{
    if (ammotype == AMMO_9MM || ammotype == AMMO_9MM_2 || ammotype == AMMO_RIFLE)
    {
        strcat(buffer, pickupText(PROPOBJ_STR_05_AMMO));
    }
    else if (ammotype == AMMO_KNIFE)
    {
        strcat(buffer, pickupText(PROPOBJ_STR_0F_THROWING));
        strcat(buffer, pickupText(amount == 1 ? PROPOBJ_STR_10_KNIFE : PROPOBJ_STR_11_KNIVES));
    }
    else if (ammotype == AMMO_DYNAMITE)
    {
        strcat(buffer, pickupText(amount == 1 ? PROPOBJ_STR_19_STICK : PROPOBJ_STR_1A_STICKS));
        strcat(buffer, pickupText(PROPOBJ_STR_18_OFDYNAMITE));
    }
    else
    {
        if (s_AmmoNames[ammotype] != 0)
        {
            strcat(buffer, pickupText(s_AmmoNames[ammotype]));
        }

        if (amount > 1)
        {
            strcat(buffer, pickupText(PROPOBJ_STR_12_S));
        }
    }
}


/* "Picked up some shotgun cartridges.", or just "shotgun cartridges." in a half-width
   view, where there isn't room */
static void pickupBuildAmmoText(char *buffer, AMMOTYPE ammotype, s32 amount)
{
    buffer[0] = '\0';

    if (!viewIsHalfWidth())
    {
        strcat(buffer, pickupText(PROPOBJ_STR_00_PICKEDUP));
        pickupAppendAmmoArticle(buffer, ammotype, amount);
    }

    pickupAppendAmmoName(buffer, ammotype, amount);
    strcat(buffer, ".\n");
}


static void pickupShowAmmoText(AMMOTYPE ammotype, s32 amount)
{
    char buffer[100] = "";

    pickupBuildAmmoText(buffer, ammotype, amount);
    HUDMESSAGEBOTTOM(buffer);
}


static void pickupPlayAmmoSound(AMMOTYPE ammotype)
{
    switch (ammotype)
    {
        case AMMO_9MM:
        case AMMO_9MM_2:
        case AMMO_RIFLE:
        case AMMO_SHOTGUN:
        case AMMO_GRENADE:
        case AMMO_ROCKETS:
        case AMMO_GRENADEROUND:
        case AMMO_MAGNUM:
        case AMMO_GGUN:
        case AMMO_DARTS:
        case AMMO_EXPLOSIVEPEN:
        case AMMO_FLARE:
        case AMMO_PITON:
        case AMMO_DYNAMITE:
        case AMMO_GEKEY:
        case AMMO_TOKEN:
            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_AMMO_SFX, 0);
            break;

        case AMMO_REMOTEMINE:
        case AMMO_PROXMINE:
        case AMMO_TIMEDMINE:
        case AMMO_BOMBCASE:
        case AMMO_BUG:
        case AMMO_MICRO_CAMERA:
        case AMMO_PLASTIQUE:
            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_MINE_SFX, 0);
            break;

        case AMMO_KNIFE:
            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_KNIFE_SFX, 0);
            break;

        default:
            break;
    }
}


void pickupPlayWeaponSound(ITEM_IDS weapon)
{
    switch (weapon)
    {
        case ITEM_KNIFE:
        case ITEM_THROWKNIFE:
            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_KNIFE_SFX, 0);
            break;

        case ITEM_REMOTEMINE:
        case ITEM_PROXIMITYMINE:
        case ITEM_TIMEDMINE:
        case ITEM_BOMBCASE:
        case ITEM_BUG:
        case ITEM_MICROCAMERA:
        case ITEM_PLASTIQUE:
            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_MINE_SFX, 0);
            break;

        case ITEM_GRENADE:
        case ITEM_GRENADEROUND:
        case ITEM_ROCKETROUND:
            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_AMMO_SFX, 0);
            break;

        case ITEM_LASER:
            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_LASER_SFX, 0);
            break;

        default:
            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_GUN_SFX, 0);
            break;
    }
}


/**
 * Give the current player some ammo, with its text and sound if asked for. Ammo that is
 * also an item (grenades, mines, gadgets) gives the item too, even when the player
 * already has as much ammo as they can carry.
 */
void pickupAddAmmo(AMMOTYPE ammotype, s32 amount, bool playsound, bool showtext)
{
    if (amount <= 0)
    {
        return;
    }

    if (ammoGetHeld(ammotype) < ammoGetMax(ammotype))
    {
        ammoGive(ammotype, ammoGetHeld(ammotype) + amount);
    }

    if (showtext)
    {
        pickupShowAmmoText(ammotype, amount);
    }

    if (playsound)
    {
        pickupPlayAmmoSound(ammotype);
    }

    if (s_AmmoItems[ammotype] != ITEM_UNARMED)
    {
        invAddItem(s_AmmoItems[ammotype]);

        if (ammotype == AMMO_REMOTEMINE)
        {
            invAddItem(ITEM_TRIGGER);
        }
    }
}


/* How much of an ammo type a magazine or a gun gives, before the solo multiplier */
static s32 ammoPerPickup(AMMOTYPE ammotype)
{
    switch (ammotype)
    {
        case AMMO_9MM:
        case AMMO_9MM_2:
        case AMMO_RIFLE:
            return 10;
        case AMMO_SHOTGUN:
        case AMMO_MAGNUM:
            return 5;
        case AMMO_GGUN:
            return 3;
        case AMMO_DARTS:
            return 4;
        default:
            return 1;
    }
}


/* In solo, a pickup of more than one round gives more on the easier difficulties */
static s32 ammoApplySoloMultiplier(s32 amount)
{
    if (amount > 1 && !isMpGame())
    {
        amount *= g_SoloAmmoMultiplier;
    }

    return amount;
}


static s32 pickupAmmoFromMagazine(AmmoCrateRecord *crate)
{
    return ammoApplySoloMultiplier(ammoPerPickup(crate->ammoType));
}


/* The ammo that comes with a picked-up gun. A grenade launcher comes loaded with 3. */
static s32 pickupAmmoFromWeapon(WeaponObjRecord *weapon)
{
    AMMOTYPE ammotype = weaponGetAmmoType(weapon->weaponnum);

    if (weapon->flags & PROPFLAG_NO_AMMO)
    {
        return 0;
    }

    return ammoApplySoloMultiplier(ammotype == AMMO_GRENADEROUND ? 3 : ammoPerPickup(ammotype));
}


/* "Picked up a PP7." Weapons that are really ammo (grenades, mines, gadgets) are worded
   as ammo: "Picked up a hand grenade." */
static void pickupBuildWeaponText(char *buffer, ITEM_IDS weapon)
{
    switch (weapon)
    {
        case ITEM_THROWKNIFE:
        case ITEM_GRENADE:
        case ITEM_TIMEDMINE:
        case ITEM_PROXIMITYMINE:
        case ITEM_REMOTEMINE:
        case ITEM_BOMBCASE:
        case ITEM_PLASTIQUE:
        case ITEM_BUG:
        case ITEM_MICROCAMERA:
        case ITEM_GOLDENEYEKEY:
        case ITEM_ROCKETROUND:
        case ITEM_GRENADEROUND:
        case ITEM_TOKEN:
            pickupBuildAmmoText(buffer, weaponGetAmmoType(weapon), 1);
            return;

        default:
            break;
    }

    buffer[0] = '\0';

    if (!viewIsHalfWidth())
    {
        strcat(buffer, pickupText(PROPOBJ_STR_00_PICKEDUP));
    }

    if ((u32)weapon < ITEM_IDS_MAX && s_WeaponNames[weapon] != 0)
    {
        strcat(buffer, pickupText(s_WeaponNames[weapon]));
    }
    else
    {
        strcat(buffer, pickupText(PROPOBJ_STR_3B_ANEWWEAPON));
    }
}


void pickupShowWeaponText(ITEM_IDS weapon)
{
    char buffer[100];

    pickupBuildWeaponText(buffer, weapon);
    HUDMESSAGEBOTTOM(buffer);
}


/* The text set up for this object by the level (eg. "Picked up the Safe Key"), or the
   given default */
static char *objPickupText(ObjectRecord *obj, s32 defaulttext)
{
    char *text = invGetActivatedTextObject(obj);

    if (text == NULL)
    {
        text = pickupText(defaulttext);
    }

    return text;
}


/* The ammo type in each slot of a multi-ammo crate. Slot 1 would be the second 9mm type,
   which is given as plain 9mm. */
static AMMOTYPE ammoCrateSlotType(s32 slot)
{
    AMMOTYPE ammotype = slot + 1;

    return ammotype == AMMO_9MM_2 ? AMMO_9MM : ammotype;
}


static TICKOP pickupWeapon(PropRecord *prop, bool showtext)
{
    ObjectRecord *obj = prop->obj;
    WeaponObjRecord *weapon = (WeaponObjRecord *)obj;
    bool collected = false;
    AMMOTYPE ammotype;
    TICKOP op;

    pickupPlayWeaponSound(weapon->weaponnum);

    if (weapon->weaponnum == ITEM_REMOTEMINE)
    {
        invAddItem(ITEM_TRIGGER);
    }
    else if (weapon->weaponnum == ITEM_TOKEN)
    {
        gunEquip(GUNRIGHT, ITEM_TOKEN);
    }

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_RENAMED)
    {
        // A mission item that keeps its prop (the player carries it)
        if (weapon->weaponnum < ITEM_BOMBCASE)
        {
            invAddWeaponByProp(prop);
        }

        if (showtext)
        {
            char *text = invGetActivatedTextObject(obj);

            if (text)
            {
                HUDMESSAGEBOTTOM(text);
            }
            else
            {
                pickupShowWeaponText(weapon->weaponnum);
            }

            collected = true;
        }

        op = TICKOP_GIVETOPLAYER;
    }
    else
    {
        if (invAddWeaponByProp(prop))
        {
            collected = true;
        }

        if (showtext)
        {
            char *text = invGetActivatedTextWeapon(weapon->weaponnum);

            if (text)
            {
                collected = true;
                HUDMESSAGEBOTTOM(text);
            }
            else if (collected)
            {
                pickupShowWeaponText(weapon->weaponnum);
            }
        }

        op = TICKOP_FREE;
    }

    // The gun's ammo. If the gun itself was new, its text is shown instead of the ammo's.
    ammotype = weaponGetAmmoType(weapon->weaponnum);

    if (ammotype != AMMO_NONE)
    {
        s32 amount = pickupAmmoFromWeapon(weapon);

        if (amount > 0 && ammoGetHeld(ammotype) < ammoGetMax(ammotype))
        {
            ammoGive(ammotype, ammoGetHeld(ammotype) + amount);

            if (!collected && showtext)
            {
                pickupShowAmmoText(ammotype, amount);
            }
        }
    }

    return op;
}


/**
 * The current player collects a prop. Returns TICKOP_FREE if it was used up (its object
 * is freed, or respawns later), TICKOP_GIVETOPLAYER if the player now carries the prop
 * (keys, mission items), or TICKOP_NONE if it can't be picked up now.
 */
TICKOP propPickupByPlayer(PropRecord *prop, bool showtext)
{
    ObjectRecord *obj = prop->obj;
    TICKOP op;

    if (g_CurrentPlayer->bonddead || g_ClockTimer == 0)
    {
        return TICKOP_NONE;
    }

    // Counter-op: the guard player only has their guard's gun, so they
    // can't take Bond's weapons or ammo.
    if (counterOpIsGuardPlayer(playerGetCurrentNum()))
    {
        return TICKOP_NONE;
    }

    switch (obj->type)
    {
        case PROPDEF_KEY:
            sndPlaySfx(g_musicSfxBufferPtr, KEYCARD_SFX, 0);

            if (showtext)
            {
                HUDMESSAGEBOTTOM(objPickupText(obj, PROPOBJ_STR_3C_PICKEDUPAKEY));
            }

            op = TICKOP_GIVETOPLAYER;
            break;

        case PROPDEF_MAGAZINE:
        {
            AmmoCrateRecord *crate = (AmmoCrateRecord *)obj;

            pickupAddAmmo(crate->ammoType, pickupAmmoFromMagazine(crate), true, showtext);
            op = TICKOP_FREE;
            break;
        }

        case PROPDEF_AMMO:
        {
            MultiAmmoCrateRecord *crate = (MultiAmmoCrateRecord *)obj;

            for (s32 slot = 0; slot < AMMOTYPE_GLOBAL_MAX; slot++)
            {
                s32 amount = crate->slots[slot].quantity;

                if (!isMpGame())
                {
                    amount *= g_SoloAmmoMultiplier;
                }

                pickupAddAmmo(ammoCrateSlotType(slot), amount, false, showtext);
            }

            sndPlaySfx(g_musicSfxBufferPtr, PICKUP_AMMO_SFX, 0);
            op = TICKOP_FREE;
            break;
        }

        case PROPDEF_COLLECTABLE:
            op = pickupWeapon(prop, showtext);
            break;

        case PROPDEF_ARMOUR:
            currentPlayerAddArmor(((BodyArmourRecord *)obj)->amount);
            sndPlaySfx(g_musicSfxBufferPtr, ARMOUR_COLLECT_SFX, 0);

            if (showtext)
            {
                HUDMESSAGEBOTTOM(objPickupText(obj, viewIsHalfWidth() ? PROPOBJ_STR_3E_BODYARMOUR
                                                                      : PROPOBJ_STR_3D_PICKEDUPSOMEBODEYARMOUR));
            }

            op = TICKOP_FREE;
            break;

        default:
            sndPlaySfx(g_musicSfxBufferPtr, KEYCARD_SFX, 0);

            if (showtext)
            {
                HUDMESSAGEBOTTOM(objPickupText(obj, PROPOBJ_STR_3F_PICKEDUPSOMETHING));
            }

            op = TICKOP_GIVETOPLAYER;
            break;
    }

    // Tagged props are kept, as the level's scripts refer to them
    if (op == TICKOP_FREE && !(obj->runtime_bitflags & RUNTIMEBITFLAG_TAGGED))
    {
        objFree(obj, false, obj->state & PROPSTATE_RESPAWN);
        return TICKOP_FREE;
    }

    if (op != TICKOP_NONE)
    {
        invAddProp(prop);
        return TICKOP_GIVETOPLAYER;
    }

    return TICKOP_NONE;
}


/* Picking up this ammo would give the player an item they haven't got: picking it up is
   worth it even when they can't carry any more of the ammo */
static bool ammoGivesMissingItem(AMMOTYPE ammotype)
{
    return (ammotype == AMMO_GRENADE && !invHasItem(ITEM_GRENADE))
        || (ammotype == AMMO_REMOTEMINE && !invHasItem(ITEM_REMOTEMINE))
        || (ammotype == AMMO_PROXMINE && !invHasItem(ITEM_PROXIMITYMINE))
        || (ammotype == AMMO_TIMEDMINE && !invHasItem(ITEM_TIMEDMINE))
        || (ammotype == AMMO_KNIFE && !invHasItem(ITEM_THROWKNIFE));
}


/* The player has this weapon and can't carry any more of its ammo. A gun that comes as a
   pair is still worth picking up if the player hasn't got the pair. */
static bool playerHasAllOfWeapon(WeaponObjRecord *weapon)
{
    ITEM_IDS other, right, left;

    if (!invHasItem(weapon->weaponnum)
            || weaponGetAmmoType(weapon->weaponnum) == AMMO_NONE
            || weaponGetHeldAmmo(weapon->weaponnum) < weaponGetMaxAmmo(weapon->weaponnum))
    {
        return false;
    }

    if (weapon->dualweapon == NULL && weapon->LinkedWeaponType < 0)
    {
        return true;
    }

    other = weapon->dualweapon != NULL ? weapon->dualweapon->weaponnum : weapon->LinkedWeaponType;
    right = left = other;

    if (weapon->flags & PROPFLAG_WEAPON_LEFTHANDED)
    {
        left = weapon->weaponnum;
    }
    else
    {
        right = weapon->weaponnum;
    }

    return invHasDualWeapon(right, left);
}


static bool weaponIsPlacedExplosive(ITEM_IDS weapon)
{
    return weapon == ITEM_REMOTEMINE || weapon == ITEM_PROXIMITYMINE || weapon == ITEM_TIMEDMINE
        || weapon == ITEM_BOMBCASE || weapon == ITEM_BUG || weapon == ITEM_MICROCAMERA
        || weapon == ITEM_PLASTIQUE;
}


/* Whether the current player would gain anything from picking the prop up */
static bool playerWantsPickup(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;

    switch (obj->type)
    {
        case PROPDEF_COLLECTABLE:
        {
            WeaponObjRecord *weapon = (WeaponObjRecord *)obj;
            bool armed = weapon->timer >= 0 || (obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE);

            // Thrown grenades and placed mines that are live, and rockets in flight
            if ((weapon->weaponnum == ITEM_GRENADE || weapon->weaponnum == ITEM_GRENADEROUND) && armed)
            {
                return false;
            }

            if (weaponIsPlacedExplosive(weapon->weaponnum) && armed)
            {
                return false;
            }

            if (weapon->weaponnum == ITEM_ROCKETROUND && (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
            {
                return false;
            }

            return !playerHasAllOfWeapon(weapon);
        }

        case PROPDEF_MAGAZINE:
        {
            AMMOTYPE ammotype = ((AmmoCrateRecord *)obj)->ammoType;

            return ammoGetHeld(ammotype) < ammoGetMax(ammotype) || ammoGivesMissingItem(ammotype);
        }

        case PROPDEF_AMMO:
        {
            MultiAmmoCrateRecord *crate = (MultiAmmoCrateRecord *)obj;

            if (objGetDestroyedLevel(obj) != 0)
            {
                return false;
            }

            for (s32 slot = 0; slot < AMMOTYPE_GLOBAL_MAX; slot++)
            {
                AMMOTYPE ammotype = ammoCrateSlotType(slot);

                if (crate->slots[slot].quantity > 0
                        && (ammoGetHeld(ammotype) < ammoGetMax(ammotype) || ammoGivesMissingItem(ammotype)))
                {
                    return true;
                }
            }

            return false;
        }

        case PROPDEF_ARMOUR:
            if (((BodyArmourRecord *)obj)->amount <= currentPlayerGetArmor())
            {
                return false;
            }

            // In these multiplayer scenarios, the player with the flag or the Golden Gun
            // can't take armour
            if (isMpGame())
            {
                MPSCENARIOS scenario = frontGetMpScenario();

                if (scenario == SCENARIO_TLD && invHasFlagToken())
                {
                    return false;
                }

                if (scenario == SCENARIO_MWTGG && invHasGoldenGun())
                {
                    return false;
                }
            }

            return true;

        default:
            return true;
    }
}


/**
 * Pick up the prop if the current player is touching it (or pulling it in with the
 * watch magnet) and would gain from it. Returns what to do with the prop.
 */
TICKOP objTickPlayer(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    PropRecord *playerprop;
    f32 xdiff, ydiff, zdiff;
    bool inrange;

    if (objIsCollectable(obj) && obj->type != PROPDEF_HAT)
    {
        if (obj->flags & PROPFLAG_UNCOLLECTABLE)
        {
            return TICKOP_NONE;
        }
    }
    else if (!(obj->flags & PROPFLAG_PICKUPONTOUCH))
    {
        return TICKOP_NONE;
    }

    if (obj->flags & PROPFLAG_PICKUPONINTERACT)
    {
        return TICKOP_NONE;
    }

    // Still flying, and hasn't bounced yet
    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
            && (s32)obj->projectile->refreshrate > 0 && obj->projectile->numBounces == 0)
    {
        return TICKOP_NONE;
    }

    if (!objCanPickupFromSafe(obj) || !playerWantsPickup(prop))
    {
        return TICKOP_NONE;
    }

    if (bondviewGetPlayerPitchRadians() < PICKUP_MAX_LOOK_DOWN && g_CurrentPlayer->magnetattracttime < 0)
    {
        return TICKOP_NONE;
    }

    playerprop = currentPlayerGetProp();

    xdiff = obj->runtime_pos.x - playerprop->pos.x;
    ydiff = obj->runtime_pos.y - playerprop->pos.y;
    zdiff = obj->runtime_pos.z - playerprop->pos.z;

    if (g_CurrentPlayer->magnetattracttime >= MAGNET_WARMUP_TICKS)
    {
        inrange = (xdiff * xdiff) + (zdiff * zdiff) <= MAGNET_RANGE_SQ
            && ydiff >= -MAGNET_HEIGHT && ydiff <= MAGNET_HEIGHT;
    }
    else
    {
        inrange = (xdiff * xdiff) + (zdiff * zdiff) <= PICKUP_RANGE_SQ
            && ydiff >= -PICKUP_HEIGHT && ydiff <= PICKUP_HEIGHT;
    }

    // The prop must be on the player's tile and reachable without passing through walls
    if (inrange && !(obj->flags2 & PROPFLAG2_PICKUPTHROUGHWALLS))
    {
        StandTile *stan = playerprop->stan;

        if (!stanTestLineUnobstructed(&stan, playerprop->pos.x, playerprop->pos.z, prop->pos.x, prop->pos.z,
                                      2, 30.0f, 30.0f, 0.0f, 1.0f)
                || stan != prop->stan)
        {
            inrange = false;
        }
    }

    if (inrange)
    {
        return propPickupByPlayer(prop, true);
    }

    return TICKOP_NONE;
}


TICKOP weaponTickPlayer(PropRecord *prop)
{
    return objTickPlayer(prop);
}
