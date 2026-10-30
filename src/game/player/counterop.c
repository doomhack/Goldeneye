/**
 * Counter-op: player 2 plays as the level's guards against Bond. On death (and at the
 * start) they take over a guard out of Bond's sight (counterOpTakeOverGuard), with that
 * guard's body, guns and toughness, or appear where a dead guard first stood. Also the
 * suicide pill and the end screen's stats. See docs/subsystems/player.md.
 */
#include "game/player/counterop.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/player/bondinv.h"
#include "game/player/bondview.h"
#include "game/player/bondview_internal.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "random.h"
#include <ultra64.h>
#include <bondtypes.h>

/**
 * Counter-op: a guard closer to Bond than this can't be taken over, so the
 * guard player has time to get into position. About 20 metres.
 */
#define COUNTEROP_MIN_GUARD_DIST 2000.0f

/**
 * Counter-op: the guard player takes over a guard closer to Bond than
 * this when there is one, so they start near the action. About 60 metres.
 */
#define COUNTEROP_MAX_GUARD_DIST 6000.0f

/**
 * Counter-op: the guard the guard player took over last. Their body, guns
 * and toughness are that guard's.
 */
static s32 s_CounterOpPossessed = FALSE;
static s32 s_CounterOpHead = -1;
static s32 s_CounterOpBody = -1;
static f32 s_CounterOpGuardHealth = 4.0f;
static ITEM_IDS s_CounterOpWeapons[2];

/**
 * Counter-op: a place the guard player can appear, as the guard who stands
 * (or stood) there.
 */
struct CounterOpSpawn
{
    coord3d pos;
    StandTile *stan;
    f32 look_angle;
    f32 height;
    f32 health;
    s16 head;
    s16 body;
    s16 chrnum;
    ITEM_IDS weapons[2];
    ChrRecord *chr;
};

/**
 * Counter-op: where each guard the guard player could take over stood when
 * the mission started, so the guard player can still appear once those
 * guards are dead. -1 until recorded.
 */
#define COUNTEROP_MAX_STARTS 80
static struct CounterOpSpawn s_CounterOpStarts[COUNTEROP_MAX_STARTS];
static s32 s_CounterOpStartCount = -1;

/**
 * Counter-op: for the end screen, the bodies the guard player has used and
 * the damage they did to Bond, in percent of his health and armour.
 */
static s32 s_CounterOpBodiesUsed = 0;
static f32 s_CounterOpDamageToBond = 0.0f;

s32 counterOpGetBodiesUsed(void)
{
    return s_CounterOpBodiesUsed;
}

f32 counterOpGetDamageToBond(void)
{
    return s_CounterOpDamageToBond;
}

void counterOpNoteDamageToBond(f32 percent)
{
    s_CounterOpDamageToBond += percent;
}

void counterOpResetForStage(void)
{
    s32 i;

    // g_GlobalTimer restarts each stage, so death times from the last
    // stage would look newer than any death in this one.
    for (i = 0; i < 4; i++)
    {
        g_CoopDeathTime[i] = 0;
    }

    s_CounterOpPossessed = FALSE;
    s_CounterOpHead = -1;
    s_CounterOpBody = -1;
    s_CounterOpGuardHealth = 4.0f;
    s_CounterOpStartCount = -1;
    s_CounterOpBodiesUsed = 0;
    s_CounterOpDamageToBond = 0.0f;
}

/**
 * Counter-op: the guard player's full health in a guard's damage units.
 */
f32 counterOpGetGuardHealth(void)
{
    return (s_CounterOpGuardHealth > 0.0f) ? s_CounterOpGuardHealth : 4.0f;
}

/**
 * Counter-op: the head and body the guard player wears, or FALSE before
 * they have taken over a guard.
 */
s32 counterOpGetGuardModels(s32 *head, s32 *body)
{
    if (!g_CounterOpMode || (s_CounterOpBody < 0))
    {
        return FALSE;
    }

    *head = s_CounterOpHead;
    *body = s_CounterOpBody;
    return TRUE;
}

/**
 * Counter-op: TRUE if Bond has a clear line of sight to a point height
 * above the floor at pos.
 */
static s32 counterOpBondCanSeePos(coord3d *pos, StandTile *stan, f32 height, PropRecord *bondprop)
{
    return stanTestLineUnobstructed(&stan, pos->x, pos->z, bondprop->pos.x, bondprop->pos.z,
            CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE, height, height, 0.0f, 1.0f)
        && (stan == bondprop->stan);
}

/**
 * Counter-op: TRUE if a spot is far enough from Bond and out of his sight
 * for the guard player to appear there. *distp is the squared distance.
 */
static s32 counterOpSpotIsHidden(coord3d *pos, StandTile *stan, f32 height, PropRecord *bondprop, f32 *distp)
{
    f32 dx = pos->x - bondprop->pos.x;
    f32 dz = pos->z - bondprop->pos.z;

    *distp = (dx * dx) + (dz * dz);

    if (*distp < (COUNTEROP_MIN_GUARD_DIST * COUNTEROP_MIN_GUARD_DIST))
    {
        return FALSE;
    }

    return !counterOpBondCanSeePos(pos, stan, height, bondprop);
}

/**
 * Counter-op: the named characters (allies, bosses, scientists and
 * civilians) by body or head. Level scripts need them, so the guard player
 * never takes one over.
 */
static const s16 s_CounterOpExcludedModels[] = {
    BODY_Boris, BODY_Ourumov, BODY_Trevelyan_Janus, BODY_Trevelyan_006,
    BODY_Valentin_, BODY_Xenia, BODY_Baron_Samedi, BODY_Jaws, BODY_Mayday,
    BODY_Oddjob, BODY_Natalya_Skirt, BODY_Natalya_Jungle_Fatigues,
    BODY_Siberian_Guard_1_Mishkin, BODY_Russian_Commandant,
    BODY_Naval_Officer, BODY_Rosika, BODY_Scientist_2_Female,
    BODY_Civilian_1_Female, BODY_Civilian_4, BODY_Civilian_2, BODY_Civilian_3,
    BODY_Scientist_1_Male, BODY_Helicopter_Pilot,
    BODY_Brosnan_Tuxedo, BODY_Male_Pierce_Bond_1, BODY_Male_Pierce_Bond_2,
    BODY_Male_Pierce_Bond_3, BODY_Male_Pierce_Bond_Parka, BODY_Male_Pierce_Bond_Tuxedo,
    BODY_Male_Dave_Dr_Doak, BODY_Male_Mishkin, BODY_Female_Marion_Rosika,
};

static s32 counterOpChrIsExcluded(ChrRecord *chr)
{
    PropRecord *child;
    s32 i;

    if (chr->chrflags & CHRFLAG_COUNT_DEATH_AS_CIVILIAN)
    {
        return TRUE;
    }

    // A guard carrying anything besides their guns and hat (a key card,
    // a mission item) keeps it for Bond to take from their body.
    for (child = chr->prop->child; child != NULL; child = child->prev)
    {
        if ((child != chr->handle_positiondata_hat) && (child != chr->weapons_held[GUNLEFT])
            && (child != chr->weapons_held[GUNRIGHT]) && !(child->weapon->flags & 0x2000))
        {
            return TRUE;
        }
    }

    for (i = 0; i < (s32)(sizeof(s_CounterOpExcludedModels) / sizeof(s_CounterOpExcludedModels[0])); i++)
    {
        if ((chr->bodynum == s_CounterOpExcludedModels[i]) || (chr->headnum == s_CounterOpExcludedModels[i]))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Counter-op: TRUE if chr is a living, armed guard the guard player may
 * become, wherever they are.
 */
static s32 counterOpChrIsGuard(ChrRecord *chr)
{
    return (chr->model != NULL) && (chr->prop != NULL) && (chr->prop->type == PROP_TYPE_CHR)
        && (chr->prop->flags & PROPFLAG_ENABLED)
        && (chr->actiontype != ACT_DIE) && (chr->actiontype != ACT_DEAD)
        && (chr->damage < chr->maxdamage)
        && !(chr->chrflags & (CHRFLAG_INVINCIBLE | CHRFLAG_HIDDEN))
        && !(chr->hidden & CHRHIDDEN_REMOVE)
        && ((chr->weapons_held[GUNRIGHT] != NULL) || (chr->weapons_held[GUNLEFT] != NULL))
        && !counterOpChrIsExcluded(chr);
}

/**
 * Counter-op: TRUE if the guard player may take over chr: a guard (see
 * counterOpChrIsGuard), out of Bond's sight and not too close to him.
 */
static s32 counterOpGuardCanBeTaken(ChrRecord *chr, PropRecord *bondprop, f32 *distp)
{
    if (!counterOpChrIsGuard(chr))
    {
        return FALSE;
    }

    return counterOpSpotIsHidden(&chr->prop->pos, chr->prop->stan, chr->chrheight - 20.0f, bondprop, distp);
}

/**
 * Counter-op: fills a spawn from a guard as they are now.
 */
static void counterOpSpawnFromChr(struct CounterOpSpawn *spawn, ChrRecord *chr)
{
    s32 hand;

    spawn->pos = chr->prop->pos;
    spawn->stan = chr->prop->stan;
    // A model's yaw turns the other way to a player's view.
    spawn->look_angle = M_TAU_F - modelGetRootRotY(chr->model);
    spawn->height = chr->chrheight - 20.0f;
    spawn->health = chr->maxdamage;
    spawn->head = chr->headnum;
    spawn->body = chr->bodynum;
    spawn->chrnum = chr->chrnum;
    spawn->chr = chr;

    for (hand = GUNRIGHT; hand <= GUNLEFT; hand++)
    {
        spawn->weapons[hand] = (chr->weapons_held[hand] != NULL) ? chr->weapons_held[hand]->weapon->weaponnum : ITEM_UNARMED;
    }
}

/**
 * Counter-op: records where every guard stands as the mission starts.
 */
static void counterOpRecordStarts(void)
{
    s32 i;

    s_CounterOpStartCount = 0;

    for (i = 0; (i < g_NumChrSlots) && (s_CounterOpStartCount < COUNTEROP_MAX_STARTS); i++)
    {
        ChrRecord *chr = &g_ChrSlots[i];

        if (counterOpChrIsGuard(chr))
        {
            counterOpSpawnFromChr(&s_CounterOpStarts[s_CounterOpStartCount++], chr);
        }
    }
}

/**
 * Counter-op: when no guard can be taken over, picks the start of a guard
 * who has died (or been taken over) that is out of Bond's sight, nearest
 * first as for guards. FALSE if there is none either.
 */
static s32 counterOpPickDeadGuardStart(struct CounterOpSpawn *spawn)
{
    PropRecord *bondprop = g_playerPointers[COUNTEROP_BOND]->prop;
    struct CounterOpSpawn *pick = NULL;
    struct CounterOpSpawn *nearest = NULL;
    f32 nearestdist = 0.0f;
    s32 count = 0;
    s32 i;

    for (i = 0; i < s_CounterOpStartCount; i++)
    {
        struct CounterOpSpawn *start = &s_CounterOpStarts[i];
        f32 dist;

        // Still alive: they are a guard to take over, not a start to use.
        if ((start->chr->chrnum == start->chrnum) && counterOpChrIsGuard(start->chr))
        {
            continue;
        }

        if (!counterOpSpotIsHidden(&start->pos, start->stan, start->height, bondprop, &dist))
        {
            continue;
        }

        if ((nearest == NULL) || (dist < nearestdist))
        {
            nearest = start;
            nearestdist = dist;
        }

        if (dist <= (COUNTEROP_MAX_GUARD_DIST * COUNTEROP_MAX_GUARD_DIST))
        {
            count++;

            if ((randomGetNext() % count) == 0)
            {
                pick = start;
            }
        }
    }

    if (pick == NULL)
    {
        pick = nearest;
    }

    if (pick == NULL)
    {
        return FALSE;
    }

    *spawn = *pick;
    spawn->chr = NULL;
    return TRUE;
}

/**
 * Counter-op: picks the guard for the guard player to take over, or NULL
 * if none qualifies right now.
 */
static ChrRecord *counterOpPickGuard(void)
{
    PropRecord *bondprop = g_playerPointers[COUNTEROP_BOND]->prop;
    ChrRecord *pick = NULL;
    ChrRecord *nearest = NULL;
    f32 nearestdist = 0.0f;
    s32 count = 0;
    s32 i;

    // Any guard that qualifies within range of Bond, at random, so Bond
    // can't tell which guard the guard player will be. With none in range,
    // the nearest that qualifies.
    for (i = 0; i < g_NumChrSlots; i++)
    {
        ChrRecord *chr = &g_ChrSlots[i];
        f32 dist;

        if (!counterOpGuardCanBeTaken(chr, bondprop, &dist))
        {
            continue;
        }

        if ((nearest == NULL) || (dist < nearestdist))
        {
            nearest = chr;
            nearestdist = dist;
        }

        if (dist <= (COUNTEROP_MAX_GUARD_DIST * COUNTEROP_MAX_GUARD_DIST))
        {
            count++;

            if ((randomGetNext() % count) == 0)
            {
                pick = chr;
            }
        }
    }

    return (pick != NULL) ? pick : nearest;
}

/**
 * Counter-op: the guard player's inventory is the guard's gun, or pair of
 * guns, with full ammo.
 */
static void counterOpGiveGuardWeapons(void)
{
    textoverride *overrides = g_CurrentPlayer->textoverrides;
    ITEM_IDS right = s_CounterOpWeapons[GUNRIGHT];
    ITEM_IDS left = s_CounterOpWeapons[GUNLEFT];
    s32 i;

    invReset();
    g_CurrentPlayer->textoverrides = overrides;

    for (i = 0; i < 30; i++)
    {
        g_CurrentPlayer->ammoheldarr[i] = 0;
    }

    if (right == ITEM_UNARMED)
    {
        right = left;
        left = ITEM_UNARMED;
    }

    if (left == right)
    {
        invAddDualItem(right, left);
    }
    else
    {
        invAddItem(right);
        left = ITEM_UNARMED;
    }

    ammoGive(weaponGetAmmoType(right), ammoGetMax(weaponGetAmmoType(right)));

    // The empty hand is the suicide pill (counterOpSuicidePillTick).
    invAddItem(ITEM_UNARMED);

    gunEquip(GUNLEFT, left);
    gunEquip(GUNRIGHT, right);
}

/**
 * Counter-op: the guard player's empty hand is a suicide pill, a way out of
 * a bad spawn. Pressing fire with it selected is a death that gives Bond no
 * kill, followed by the usual Press Start prompt. Returns the trigger state
 * left for the gun code.
 */
static s32 s_CounterOpPillTrigger = FALSE;
static s32 s_CounterOpPillSelected = FALSE;

s32 counterOpSuicidePillTick(s32 triggerOn)
{
    s32 pressed;
    s32 selected;

    if (!counterOpIsGuardPlayer(playerGetCurrentNum()))
    {
        return triggerOn;
    }

    if (g_CurrentPlayer->bonddead)
    {
        // The button pressed to respawn mustn't count as a fresh press.
        s_CounterOpPillTrigger = TRUE;
        s_CounterOpPillSelected = FALSE;
        return triggerOn;
    }

    // Only a fresh press, so the fire button still held from the gun
    // doesn't take the pill.
    pressed = triggerOn && !s_CounterOpPillTrigger;
    s_CounterOpPillTrigger = triggerOn;

    // Not while the hand is empty only because a gun is being drawn, as
    // after spawning.
    selected = (gunGetWeapon(GUNRIGHT) == ITEM_UNARMED)
               && (gunGetWeapon(GUNLEFT) == ITEM_UNARMED)
               && (g_CurrentPlayer->hands[GUNRIGHT].weapon_next_weapon == ITEM_UNARMED);

    if (selected && !s_CounterOpPillSelected)
    {
        HUDMESSAGEBOTTOM("Suicide pill: fire to use\n");

        // Hold it for 3 seconds rather than the usual 1.6 (PAL) or 2
        // (NTSC), so it's still up once the gun is put away.
        g_CurrentPlayer->bondmesscnt = 180;
    }

    s_CounterOpPillSelected = selected;

    if (!selected)
    {
        return triggerOn;
    }

    if (pressed)
    {
        playerDie();
    }

    return FALSE;
}

/**
 * Counter-op: the current (guard) player takes over a guard. The guard
 * leaves the level and the player appears where they stood, wearing their
 * head and body, carrying their guns, with their toughness. With no guard
 * to take over, the player appears as a dead guard where that guard stood
 * at the start, so they are never out. Returns FALSE if neither can be
 * done right now.
 */
s32 counterOpTakeOverGuard(void)
{
    ChrRecord *chr;
    struct CounterOpSpawn spawn;
    s32 hand;

    if (s_CounterOpStartCount < 0)
    {
        counterOpRecordStarts();
    }

    chr = counterOpPickGuard();

    if (chr != NULL)
    {
        counterOpSpawnFromChr(&spawn, chr);

        // The guard leaves (with their guns) on their next tick.
        chr->hidden |= CHRHIDDEN_REMOVE;
    }
    else if (!counterOpPickDeadGuardStart(&spawn))
    {
        return FALSE;
    }

    s_CounterOpHead = spawn.head;
    s_CounterOpBody = spawn.body;
    s_CounterOpGuardHealth = spawn.health;

    for (hand = GUNRIGHT; hand <= GUNLEFT; hand++)
    {
        s_CounterOpWeapons[hand] = spawn.weapons[hand];
    }

    if (g_CurrentPlayer->bonddead)
    {
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

        g_CurrentPlayer->deltaPos.x = 0.0f;
        g_CurrentPlayer->deltaPos.y = -0.0001f;
        g_CurrentPlayer->deltaPos.z = 0.0f;
        currentPlayerStartChrFade(120.0f, 1.0f);
    }

    coopPlaceCurrentPlayer(&spawn.pos, spawn.stan, spawn.look_angle);
    counterOpGiveGuardWeapons();

    g_CurrentPlayer->bondhealth = 1.0f;
    g_CurrentPlayer->bondarmour = 0.0f;

    // Swap the body for the guard's.
    if (g_CurrentPlayer->prop->chr != NULL)
    {
        chrFreeResources(g_CurrentPlayer->prop);
        g_CurrentPlayer->prop->chr = NULL;
        g_CurrentPlayer->bodyModel = NULL;
    }

    playerLoadBody();
    playerUpdateRooms(g_CurrentPlayer);

    s_CounterOpPossessed = TRUE;
    s_CounterOpBodiesUsed++;
    return TRUE;
}

/**
 * Counter-op: called for each player each frame. The guard player takes
 * over their first guard once the intro is over.
 */
void counterOpTick(void)
{
    if (counterOpIsGuardPlayer(playerGetCurrentNum()) && !s_CounterOpPossessed
        && !g_CurrentPlayer->bonddead && (g_CameraMode == CAMERAMODE_FP))
    {
        counterOpTakeOverGuard();
    }
}
