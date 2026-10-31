/**
 * The prop manager.
 *
 * Every prop lives in one pool of 600 (g_Props). propAllocate takes one from the free
 * list, propActivate puts it on the active list, and propFree returns it. Each frame:
 *
 * - propsTickOnce runs once: background AI scripts, respawning objects, beams, explosions
 *   and smoke.
 * - For each player's view, propsTickPerPlayer ticks every active prop (chrTick, objTick,
 *   playerTick...), which also decides whether it is on screen. propsSortOnscreen sorts
 *   those far to near, and propsRenderPass draws them room by room.
 * - propsTickPickups checks what the player is touching, and playerInteract handles the
 *   use button.
 *
 * A tick returns a TICKOP saying what to do with the prop: keep it, free it, take it off
 * the active list, tick it again this frame, or give it to the player
 * (propExecuteTickOperation).
 *
 * Which props are in which rooms is in proprooms.c, collision between props in
 * propcollision.c, bounding boxes in bbox.c and auto-aim in autoaim.c.
 */

#include <ultra64.h>
#include <math.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <boss.h>
#include <music.h>
#include <snd.h>
#include "game/prop/prop.h"
#include "game/prop/alarm.h"
#include "game/prop/door.h"
#include "game/prop/explosion.h"
#include "game/prop/projectile.h"
#include "game/prop/propobj.h"
#include "game/prop/setup.h"
#include "game/audio/sfxvolume.h"
#include "game/chr/chr.h"
#include "game/chr/chraction.h"
#include "game/chr/chrai.h"
#include "game/level/lv.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/world/bg.h"

/* A respawning object waits while a player is this close */
#define RESPAWN_PLAYER_DIST 400.0f

/* How long a used-up respawning object stays away */
#ifdef VERSION_EU
#define RESPAWN_TIME 1000
#else
#define RESPAWN_TIME 1200
#endif

/* The prop pool: every prop in the game is one of these */
PropRecord g_Props[MAX_PROPS];

PropRecord *g_ActivePropsTail = NULL;
PropRecord *g_ActivePropsHead = NULL;
PropRecord *g_FreeProps = NULL;

/* The props on screen in this view, sorted far to near, ended by NULL */
PropRecord *g_OnScreenPropList[ONSCREEN_PROP_LIST_LEN];
PropRecord **g_LastOnScreenProp;
s32 g_OnScreenPropCount;

/* The prop the use button acts on, found by propFindForInteract */
PropRecord *g_InteractProp;


/**
 * Empty the prop pool and the room lists for a new level.
 */
void propsInit(void)
{
    g_AutoAimTolerance = 1.0f;
    g_ActivePropsTail = g_ActivePropsHead = NULL;
    g_OnScreenPropCount = 0;
    g_OnScreenPropList[0] = NULL;
    g_LastOnScreenProp = g_OnScreenPropList;
    g_FreeProps = g_Props;

    for (s32 i = 0; i < MAX_PROPS - 1; i++)
    {
        g_Props[i].prev = &g_Props[i + 1];
    }

    roomPropListsInit();
}


/**
 * Collect the enabled props on screen and sort them far to near (largest zDepth first).
 */
void propsSortOnscreen(void)
{
    s32 count = 0;

    for (PropRecord *prop = propsGetActiveTail(); prop != NULL; prop = prop->prev)
    {
        if ((prop->flags & (PROPFLAG_ENABLED | PROPFLAG_ONSCREEN)) == (PROPFLAG_ENABLED | PROPFLAG_ONSCREEN))
        {
            g_OnScreenPropList[count] = prop;
            count++;
        }
    }

    g_OnScreenPropCount = count;
    g_OnScreenPropList[count] = NULL;
    g_LastOnScreenProp = &g_OnScreenPropList[count];

    // Selection sort
    for (s32 i = 0; i < count; i++)
    {
        s32 best = -1;
        f32 bestz = -4.2949673e9f;

        for (s32 j = i; j < count; j++)
        {
            f32 z = g_OnScreenPropList[j]->zDepth;

            if (bestz < z)
            {
                bestz = z;
                best = j;
            }
        }

        if (best >= 0)
        {
            PropRecord *swap = g_OnScreenPropList[i];

            g_OnScreenPropList[i] = g_OnScreenPropList[best];
            g_OnScreenPropList[best] = swap;
        }
    }
}


void propEnable(PropRecord *prop)
{
    prop->flags |= PROPFLAG_ENABLED;
}


void propDisable(PropRecord *prop)
{
    prop->flags &= ~PROPFLAG_ENABLED;
}


PropRecord *propsGetActiveTail(void)
{
    return g_ActivePropsTail;
}


/* Take a prop from the free list, cleared. NULL when the pool is used up. */
PropRecord *propAllocate(void)
{
    PropRecord *prop = g_FreeProps;

    if (prop == NULL)
    {
        return NULL;
    }

    g_FreeProps = prop->prev;

    prop->prev = NULL;
    prop->next = NULL;
    prop->parent = NULL;
    prop->child = NULL;

    prop->flags = 0;
    prop->stan = NULL;
    prop->timetoregen = 0;
    prop->rooms[0] = 0xff;

    return prop;
}


/* Return a prop to the free list */
void propFree(PropRecord *prop)
{
    prop->prev = g_FreeProps;
    prop->next = NULL;
    prop->stan = NULL;
    g_FreeProps = prop;
}


/* Add a prop to the tail of the active list. The ticks walk the list from the tail, so
   it is ticked first. */
void propActivate(PropRecord *prop)
{
    if (g_ActivePropsTail != NULL)
    {
        g_ActivePropsTail->next = prop;

        prop->prev = g_ActivePropsTail;
        prop->next = NULL;

        g_ActivePropsTail = prop;
        return;
    }

    prop->prev = NULL;
    prop->next = NULL;
    g_ActivePropsTail = g_ActivePropsHead = prop;
}


/* Add a prop to the head of the active list, so a tick in progress still reaches it */
void propActivateThisFrame(PropRecord *prop)
{
    if (g_ActivePropsHead != NULL)
    {
        g_ActivePropsHead->prev = prop;
        prop->next = g_ActivePropsHead;
        prop->prev = NULL;
        g_ActivePropsHead = prop;
        return;
    }

    prop->prev = NULL;
    prop->next = NULL;
    g_ActivePropsHead = prop;
    g_ActivePropsTail = prop;
}


/* Take a prop off the active list */
void propDelist(PropRecord *prop)
{
    if (prop == g_ActivePropsTail)
    {
        g_ActivePropsTail = prop->prev;
    }

    if (prop == g_ActivePropsHead)
    {
        g_ActivePropsHead = prop->next;
    }

    if (prop->prev != NULL)
    {
        prop->prev->next = prop->next;
    }

    if (prop->next != NULL)
    {
        prop->next->prev = prop->prev;
    }

    prop->prev = NULL;
    prop->next = NULL;
}


/* Attach a prop to another (host), as its newest child */
void propReparent(PropRecord *child, PropRecord *host)
{
    child->parent = host;

    if (host->child != NULL)
    {
        host->child->next = child;
    }

    child->prev = host->child;
    child->next = NULL;
    child->stan = NULL;
    host->child = child;
}


/* Detach a prop from its parent, if it has one */
void propDetach(PropRecord *prop)
{
    PropRecord *parent = prop->parent;

    if (parent == NULL)
    {
        return;
    }

    if (prop == parent->child)
    {
        parent->child = prop->prev;
    }

    if (prop->prev != NULL)
    {
        prop->prev->next = prop->next;
    }

    if (prop->next != NULL)
    {
        prop->next->prev = prop->prev;
    }

    prop->parent = NULL;
    prop->prev = NULL;
    prop->next = NULL;
}


/* Draw a prop on the opaque (withalpha 0) or translucent (1) pass */
static Gfx *propRender(Gfx *gdl, PropRecord *prop, s32 withalpha)
{
    switch (prop->type)
    {
        case PROP_TYPE_CHR:
            return chrRenderProp(prop, gdl, withalpha);
        case PROP_TYPE_OBJ:
        case PROP_TYPE_WEAPON:
        case PROP_TYPE_DOOR:
            return objRenderProp(prop, gdl, withalpha);
        case PROP_TYPE_EXPLOSION:
            return explosionRender(prop, gdl, withalpha);
        case PROP_TYPE_SMOKE:
            return smokeRender(prop, gdl, withalpha);
        case PROP_TYPE_VIEWER:
            return playerRenderProp(prop, gdl, withalpha);
    }

    return gdl;
}


/* Whether a prop is drawn with this room: the first of its rooms that is visible */
static bool propIsDrawnInRoom(PropRecord *prop, s32 roomid)
{
    s32 rooms[8];

    propGetRoomIds(prop, rooms);

    for (s32 *room = rooms; *room >= 0; room++)
    {
        if (bgRoomIsVisible(*room))
        {
            return roomid == *room;
        }
    }

    return FALSE;
}


/**
 * Draw the props on screen that are drawn with a room, for one render pass:
 *
 * - 0: opaque props, near to far, before the room's translucent parts;
 * - 2: opaque props flagged PROPFLAG_RENDERPOSTBG, near to far, after them;
 * - 1: the translucent pass of every prop, far to near. A prop with PROPFLAG_00000020
 *   draws its opaque pass here too, just before.
 *
 * Cuba (the credits) draws its opaque props in pass 2's place.
 */
Gfx *propsRenderPass(Gfx *gdl, s32 roomid, s32 renderpass)
{
    if (bossGetStageNum() == LEVELID_CUBA)
    {
        if (renderpass == 0)
        {
            return gdl;
        }

        if (renderpass == 2)
        {
            renderpass = 0;
        }
    }

    if (renderpass == 0 || renderpass == 2)
    {
        u32 want = renderpass == 0 ? 0 : PROPFLAG_RENDERPOSTBG;

        for (PropRecord **pp = g_LastOnScreenProp; --pp >= g_OnScreenPropList;)
        {
            PropRecord *prop = *pp;

            if (prop != NULL
                && (prop->flags & (PROPFLAG_00000020 | PROPFLAG_RENDERPOSTBG)) == want
                && propIsDrawnInRoom(prop, roomid))
            {
                gdl = propRender(gdl, prop, 0);
            }
        }
    }
    else
    {
        for (PropRecord **pp = g_OnScreenPropList; pp < g_LastOnScreenProp; pp++)
        {
            PropRecord *prop = *pp;

            if (prop != NULL && propIsDrawnInRoom(prop, roomid))
            {
                if (prop->flags & PROPFLAG_00000020)
                {
                    gdl = propRender(gdl, prop, 0);
                }

                gdl = propRender(gdl, prop, 1);
            }
        }
    }

    return bgScissorCurrentPlayerViewDefault(gdl);
}


/**
 * Do what a tick asked for a prop:
 *
 * - FREE: remove it, or for a used-up object that respawns, hide it until it does;
 * - DISABLE: take it off the active list and out of the rooms;
 * - GIVETOPLAYER: as DISABLE, then attach it to the current player.
 */
void propExecuteTickOperation(PropRecord *prop, TICKOP op)
{
    if (op == TICKOP_FREE)
    {
        if (prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_OBJ)
        {
            ObjectRecord *obj = prop->obj;

            if (obj->state & PROPSTATE_RESPAWN)
            {
                prop->timetoregen = RESPAWN_TIME;
                obj->runtime_bitflags |= RUNTIMEBITFLAG_RESPAWNING;
                obj->runtime_bitflags &= ~RUNTIMEBITFLAG_REMOVE;
                obj->state &= ~PROPSTATE_DESTROYED;
                obj->damageTaken = 0.0f;
                propRemoveFromRooms(prop);
                propDisable(prop);
                return;
            }
        }

        propRemoveFromRooms(prop);
        propDelist(prop);
        propDisable(prop);
        propFree(prop);
    }
    else if (op == TICKOP_DISABLE)
    {
        propRemoveFromRooms(prop);
        propDelist(prop);
        propDisable(prop);
    }
    else if (op == TICKOP_GIVETOPLAYER)
    {
        propRemoveFromRooms(prop);
        propDelist(prop);
        propDisable(prop);
        objDetach(prop);
        objFreeEmbedmentOrProjectile(prop);
        propReparent(prop, currentPlayerGetProp());
    }
}


/**
 * After a prop's tick: act on its TICKOP and return the next prop to tick. prev is the
 * prop before it, taken before the tick, for a tick that changed the list.
 */
static PropRecord *propFinishTick(PropRecord *prop, PropRecord *prev, TICKOP op)
{
    if (op == TICKOP_CHANGEDLIST)
    {
        return prev;
    }

    PropRecord *next = prop->prev;

    if (op == TICKOP_RETICK)
    {
        // Move it to the head of the list, to be ticked again at the end
        propDelist(prop);
        propActivateThisFrame(prop);

        if (next == NULL)
        {
            next = prop;
        }
    }
    else
    {
        propExecuteTickOperation(prop, op);
    }

    return next;
}


/**
 * Find the prop the use button would act on: the nearest object, weapon or door on
 * screen that takes it. Also kept in g_InteractProp.
 */
PropRecord *propFindForInteract(void)
{
    g_InteractProp = NULL;

    for (PropRecord **pp = g_LastOnScreenProp - 1; pp >= g_OnScreenPropList; pp--)
    {
        PropRecord *prop = *pp;
        bool checkmore = TRUE;

        if (prop == NULL)
        {
            continue;
        }

        if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON)
        {
            checkmore = objTestForInteract(prop);
        }
        else if (prop->type == PROP_TYPE_DOOR)
        {
            checkmore = doorTestForInteract(prop);
        }

        if (!checkmore)
        {
            break;
        }
    }

    return g_InteractProp;
}


/**
 * The use button: use the prop in front of the player. TRUE when there was nothing to
 * use (the button then does its other jobs).
 */
bool playerInteract(void)
{
    PropRecord *prop = propFindForInteract();
    TICKOP op = TICKOP_NONE;

    if (prop == NULL)
    {
        return TRUE;
    }

    if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON)
    {
        op = objInteract(prop);
    }
    else if (prop->type == PROP_TYPE_DOOR)
    {
        op = doorInteract(prop);
    }

    propExecuteTickOperation(prop, op);

    return FALSE;
}


/* Whether no player is within RESPAWN_PLAYER_DIST of a prop */
static bool propIsFarFromPlayers(PropRecord *prop)
{
    s32 numplayers = getPlayerCount();

    for (s32 i = 0; i < numplayers; i++)
    {
        PropRecord *playerprop = g_playerPointers[i]->prop;
        coord3d diff;

        diff.x = playerprop->pos.x - prop->pos.x;
        diff.y = playerprop->pos.y - prop->pos.y;
        diff.z = playerprop->pos.z - prop->pos.z;

        if (sqrtf((diff.x * diff.x) + (diff.y * diff.y) + (diff.z * diff.z)) < RESPAWN_PLAYER_DIST)
        {
            return FALSE;
        }
    }

    return TRUE;
}


/**
 * Bring back a respawning object as it starts to fade in: a used-up one reappears where
 * it was (back inside its container, for one inside another object), and a destroyed
 * one is repaired. Body armour is refilled.
 */
static void objRespawn(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    bool playsfx = TRUE;

    if (obj->damageTaken == 0.0f && !(obj->state & PROPSTATE_DESTROYED))
    {
        if (obj->flags & PROPFLAG_INSIDEANOTHEROBJ)
        {
            propRemoveFromRooms(prop);
            propDelist(prop);

            obj->runtime_bitflags &= ~RUNTIMEBITFLAG_RESPAWNING;

            s32 cmdindex = setupGetCommandIndexByProp(prop);
            s32 pad = obj->pad;
            ObjectRecord *container = setupCommandGetObject(cmdindex + pad);

            if (container != NULL && container->prop != NULL)
            {
                modelSetScale(obj->model, obj->model->scale);
                propReparent(obj->prop, container->prop);
                playsfx = FALSE;
            }
        }
        else
        {
            propEnable(prop);
            propUpdateRooms(prop);
            obj->runtime_bitflags &= ~RUNTIMEBITFLAG_RESPAWNING;
        }
    }
    else
    {
        if (obj->state & PROPSTATE_EXT_COLISION_BLOCK)
        {
            obj->flags |= PROPFLAG_00000100;
        }
        else
        {
            obj->flags &= ~PROPFLAG_00000100;
        }

        obj->damageTaken = 0.0f;
        obj->state &= ~PROPSTATE_DESTROYED;
        modelResetDeformedVertices(obj->model);
    }

    if (obj->type == PROPDEF_ARMOUR)
    {
        ((BodyArmourRecord *)obj)->amount = ((BodyArmourRecord *)obj)->initialamount;
    }

    if (playsfx)
    {
        sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, OBJ_REGEN_SFX, NULL), &prop->pos);
    }
}


/**
 * Count down a respawning object. It comes back with CHROBJ_TIMETOREGEN ticks to go, and
 * fades in over them (objRenderProp), but waits while a player is near enough to see it
 * appear.
 */
static void objTickRespawn(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    bool fading = prop->timetoregen < CHROBJ_TIMETOREGEN;

    prop->timetoregen -= g_ClockTimer;

    if (prop->timetoregen < CHROBJ_TIMETOREGEN && !fading && !propIsFarFromPlayers(prop))
    {
        prop->timetoregen += CHROBJ_TIMETOREGEN;
    }

    if (prop->timetoregen <= 0)
    {
        prop->timetoregen = 0;

        if (obj->state & PROPSTATE_10)
        {
            obj->runtime_bitflags |= RUNTIMEBITFLAG_WRECKED;
        }
        else
        {
            obj->runtime_bitflags &= ~RUNTIMEBITFLAG_WRECKED;
        }
    }
    else if (prop->timetoregen < CHROBJ_TIMETOREGEN && !fading)
    {
        objRespawn(prop);
    }
}


/**
 * The tick run once per frame, before any player's view: guards' background AI,
 * respawning objects, the beams of guards, drone guns and players, and explosions and
 * smoke.
 */
void propsTickOnce(void)
{
    bgChrsTick();

    PropRecord *prop = propsGetActiveTail();

    while (prop != NULL)
    {
        PropRecord *prev = prop->prev;
        TICKOP op = TICKOP_NONE;

        if (prop->type == PROP_TYPE_CHR)
        {
            beamTick(&prop->chr->beams[0]);
            beamTick(&prop->chr->beams[1]);
        }
        else if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_DOOR)
        {
            if (prop->timetoregen > 0)
            {
                objTickRespawn(prop);
            }

            if (prop->obj->type == PROPDEF_AUTOGUN)
            {
                beamTick(((AutogunRecord *)prop->obj)->beam);
            }
        }
        else if (prop->type == PROP_TYPE_EXPLOSION)
        {
            op = explosionTick(prop);
        }
        else if (prop->type == PROP_TYPE_SMOKE)
        {
            op = smokeTick(prop);
        }
        else if (prop->type == PROP_TYPE_VIEWER)
        {
            beamTick(&g_playerPointers[playerGetIndexForProp(prop)]->hands[0].weapon_beam);
            beamTick(&g_playerPointers[playerGetIndexForProp(prop)]->hands[1].weapon_beam);

            // A player's body (in multiplayer) has beams too
            if (prop->chr != NULL && getPlayerCount() >= 2)
            {
                beamTick(&prop->chr->beams[0]);
                beamTick(&prop->chr->beams[1]);
            }
        }

        prop = propFinishTick(prop, prev, op);
    }
}


/**
 * The tick run for each player's view: tick every active prop, then (on the frame's
 * first view only) the alarm, gas and countdown timer, proximity mines, AI sounds and
 * the room lists.
 */
void propsTickPerPlayer(void)
{
    PropRecord *prop = propsGetActiveTail();

    while (prop != NULL)
    {
        PropRecord *prev = prop->prev;
        TICKOP op = TICKOP_NONE;

        switch (prop->type)
        {
            case PROP_TYPE_CHR:
                op = chrTick(prop);
                break;
            case PROP_TYPE_OBJ:
            case PROP_TYPE_WEAPON:
            case PROP_TYPE_DOOR:
                op = objTick(prop);
                break;
            case PROP_TYPE_EXPLOSION:
                op = explosionPropTick(prop);
                break;
            case PROP_TYPE_SMOKE:
                op = smokePropTick(prop);
                break;
            case PROP_TYPE_VIEWER:
                op = playerTick(prop);
                break;
        }

        prop = propFinishTick(prop, prev, op);
    }

    if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
    {
        alarmGasTimerTick();
        proxminesTickGuards();
        g_RemoteMineOwnerTriggerFlag = 0;
        aiSoundsTick();
        propsDefragRoomProps();
    }
}


/**
 * Let the current player pick up what they touch: objects and weapons that are not
 * respawning. Not while in the tank, nor while invincible (dead or in a cutscene).
 */
void propsTickPickups(void)
{
    if (playerIsInTank() || g_PlayerInvincible)
    {
        return;
    }

    PropRecord *prev;

    for (PropRecord *prop = propsGetActiveTail(); prop != NULL; prop = prev)
    {
        TICKOP op = TICKOP_NONE;

        if (prop->timetoregen <= 0)
        {
            if (prop->type == PROP_TYPE_OBJ)
            {
                op = objTickPlayer(prop);
            }
            else if (prop->type == PROP_TYPE_WEAPON)
            {
                op = weaponTickPlayer(prop);
            }
        }

        prev = prop->prev;
        propExecuteTickOperation(prop, op);
    }
}


/* A switch toggles the doors linked to it */
void switchActivateLinkedDoors(PropRecord *prop)
{
    if (!(prop->obj->runtime_bitflags & RUNTIMEBITFLAG_LINKEDSWITCH))
    {
        return;
    }

    for (struct LinkRecord *link = g_LevelLoadPropSwitch; link != NULL; link = link->next)
    {
        if (prop == link->first && link->second != NULL)
        {
            doorToggle(link->second);
        }
    }
}


/* Whether a padlocked door's padlocks are all shot off */
bool doorIsPadlockFree(DoorRecord *door)
{
    if (!(door->runtime_bitflags & RUNTIMEBITFLAG_PADLOCKEDDOOR))
    {
        return TRUE;
    }

    for (LockDoorRecord *link = g_LevelLoadPropLockDoor; link != NULL; link = link->next)
    {
        if (door == link->door && link->lock != NULL && link->lock->prop != NULL && objIsHealthy(link->lock))
        {
            return FALSE;
        }
    }

    return TRUE;
}


/* Whether an object in a safe can be picked up: the safe's door is more than half open */
bool objCanPickupFromSafe(ObjectRecord *obj)
{
    if (!(obj->flags2 & PROPFLAG2_LINKEDTOSAFE))
    {
        return TRUE;
    }

    for (SafeObjectRecord *link = g_LevelLoadPropSafeItem; link != NULL; link = link->next)
    {
        if (obj == link->item && link->door != NULL && link->door->prop != NULL && link->door->openPosition <= 0.5f)
        {
            return FALSE;
        }
    }

    return TRUE;
}


/* The first active object placed at a pad */
ObjectRecord *objFindAtPad(s32 padid)
{
    s16 pad = padid;

    for (PropRecord *prop = propsGetActiveTail(); prop != NULL; prop = prop->prev)
    {
        if (prop->type == PROP_TYPE_OBJ && pad == prop->obj->pad)
        {
            return prop->obj;
        }
    }

    return NULL;
}


/* The first active object in a room whose collision outline contains a point */
ObjectRecord *objFindAtPos(coord3d *pos, s32 roomid)
{
    for (PropRecord *prop = propsGetActiveTail(); prop != NULL; prop = prop->prev)
    {
        if (prop->type == PROP_TYPE_OBJ && roomid == prop->stan->room)
        {
            coord2d *polygon;
            s32 edges;

            propGetCollisionPolygon2d(prop, &polygon, &edges);

            if (polygonContainsPoint(pos, polygon, edges))
            {
                return prop->obj;
            }
        }
    }

    return NULL;
}
