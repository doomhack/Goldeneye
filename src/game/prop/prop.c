/**
 * The prop manager. Every prop lives in one pool of 600 (g_Props). propAllocate takes
 * one from the free list, propActivate puts it on the active list, and propFree returns
 * it. Each frame:
 *
 * - propsTickOnce runs once: background AI scripts, weapon respawns, tracers, explosions
 *   and smoke.
 * - For each player's view, propsTickPerPlayer ticks every active prop (chrTick, objTick,
 *   playerTick...), which also decide whether it is on screen; propsSortOnscreen sorts those far
 *   to near, and propsRenderPass draws them room by room.
 * - propsTickPickups checks what the player is touching, and playerInteract handles the
 *   use button.
 *
 * Each room keeps a list of the props in it, in blocks of 15 (g_RoomPropBlocks), so that
 * collision and line-of-sight tests only look at props in nearby rooms (roomGetProps).
 * Also here: collision outlines of props (propGetCollisionPolygon), and auto-aim.
 */
#include <ultra64.h>
#include <assert.h>
#include <memp.h>
#include <bondgame.h>
#include <bondtypes.h>
#include <bondaicommands.h>
#include <boss.h>
#include <limits.h>
#include <music.h>
#include <random.h>
#include <snd.h>
#include "game/world/bg.h"
#include "game/world/bgfog.h"
#include "game/player/bondview.h"
#include "game/menu/cheat.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/chraidata.h"
#include "game/chr/chraction.h"
#include "game/prop/explosion.h"
#include "game/save/file.h"
#include "game/prop/glass.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/player/gun.h"
#include "game/model/anim.h"
#include "game/text/language.h"
#include "game/prop/setup.h"
#include "game/level/lv.h"
#include "math.h"
#include "game/math/trig.h"
#include "game/math/round.h"
#include "game/model/model.h"
#include "game/audio/levelmusic.h"
#include "game/player/player.h"
#include "game/prop/propobj.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/mission/objective.h"
#include "game/nav/stan.h"
#include "game/chr/chr.h"


// bss


s16 * g_RoomPropsFound;

u32 g_NumRoomPropsFound;

/**
 * This 600 length prop pool provides storage for every PropRecord in the game.
 * Props are never allocated anywhere else. propsInit threads
 * all 600 slots onto the free list, and propAllocate/propFree pop and push to that list.
 * 
 * This is g_Vars.props in PD.
*/
PropRecord g_Props[MAX_PROPS];

s16 *g_RoomFirstPropBlock;

struct roomproplistblock *g_RoomPropBlocks;

/**
 * Array of pointers, containing onscreen props.
*/
PropRecord *g_OnScreenPropList[ONSCREEN_PROP_LIST_LEN];

/**
 * Pointer to last onscreen prop.
*/
PropRecord **g_LastOnScreenProp;

/**
 * Count of onscreen props.
 * canonically propznum
*/
s32 g_OnScreenPropCount;

PropRecord *g_InteractProp;
WeaponObjRecord* proxy_mine_table[30];

f32 gasTimeToFullOpacity;
u32 gasDoesDamageFlag;

WeaponObjRecord g_WeaponSlots[MAX_WEAPON_SLOTS];

HatRecord g_HatSlots[MAX_HAT_SLOTS];

AmmoCrateRecord g_AmmoCrates[MAX_AMMO_CRATES];

Projectile g_Projectiles[PROJECTILES_ARR_MAX];

Embedment g_Embedments[EMBEDMENT_ARR_MAX];

struct Model *g_CurrentProjectileModel;
struct ModelNode * g_ProjectileHitNode;
coord3d g_ProjectileHitPos;
coord3d g_ProjectileHitNormal;

MonitorRecord g_MonitorAnimController;

MonitorRecord g_UnknownAnimController;

/**
 * Unused / unreferenced (from padding / align?)
*/
MonitorRecord g_TaserAnimController;


stagesetup g_CurrentSetup; //Public Working Setup

stagesetup                        *g_ptrStageSetupFile;

PropRecord *g_ActivePropsTail = 0;
PropRecord *g_ActivePropsHead = 0;
PropRecord *g_FreeProps = 0;

f32 difficulty = 1.0;
coord2d g_DefaultAutoAimCoord = { 0 };

// forward declarations

Gfx *propRender(Gfx *gdl, PropRecord *prop, s32 withalpha);
void shotFireHand(s32 hand);
void shotFireBullet(s32);
void shotPunch(s32 hand, s32 item_id);
void modelGetAxisExtents(Model* model, f32* max, f32* min, s32 axis);

// end forward declarations


/**
 * Counts onscreen props.
*/
void propsSortOnscreen(void)
{
    s32 i;
    s32 j;
    s32 count;
    PropRecord *prop;
    s32 best;
    f32 bestz;

    i = 0;
    count = 0;
    prop = propsGetActiveTail();

    for (; prop != NULL; prop = prop->prev)
    {
        if ((prop->flags & (PROPFLAG_ENABLED | PROPFLAG_ONSCREEN)) == (PROPFLAG_ENABLED | PROPFLAG_ONSCREEN))
        {
            g_OnScreenPropList[count] = prop;
            count++;
        }
    }

    g_OnScreenPropCount = count;
    g_OnScreenPropList[count] = NULL;

    if(1)
    {
        // removed
    }

    g_LastOnScreenProp = &g_OnScreenPropList[count];

    for (i=0; i<count; i++)
    {
        best = -1;
        bestz = -4.2949673e9f;

        for (j = i; j < count; j++)
        {
            f32 f = g_OnScreenPropList[j]->zDepth;

            if (bestz < f)
            {
                bestz = f;
                best = j;
            }
        }

        if (best >= 0)
        {
            prop = g_OnScreenPropList[i];
            g_OnScreenPropList[i] = g_OnScreenPropList[best];
            g_OnScreenPropList[best] = prop;
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


PropRecord* propAllocate(void)
{
    PropRecord* prop;

    if (g_FreeProps)
    {
        prop = g_FreeProps;
        g_FreeProps = prop->prev;

        prop->prev = NULL;
        prop->next = NULL;
        prop->parent = NULL;
        prop->child = NULL;

        prop->flags = 0;
        prop->stan = NULL;
        prop->timetoregen = 0;
        prop->rooms[0] = 0xFF;
        return prop;
    }

    return NULL;
}


void propFree(PropRecord *prop)
{
    prop->prev = g_FreeProps;
    prop->next = 0x0;
    prop->stan = 0x0;
    g_FreeProps = prop;
}


void propActivate(PropRecord* prop)
{
    PropRecord* cur;

    cur = g_ActivePropsTail;
    if (cur != NULL)
    {
        cur->next = prop;

        prop->prev = g_ActivePropsTail;
        prop->next = NULL;

        g_ActivePropsTail = prop;
        return;
    }

    prop->prev = NULL;
    prop->next = NULL;
    g_ActivePropsTail = g_ActivePropsHead = prop;
}


void propActivateThisFrame(PropRecord* prop) {
    PropRecord* first;

    first = g_ActivePropsHead;
    if (first != NULL) {
        first->prev = prop;
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


void propDelist(PropRecord *prop)
{
    PropRecord *prev;
    PropRecord *next;

    if (prop == g_ActivePropsTail)
    {
        g_ActivePropsTail = prop->prev;
    }
    if (prop == g_ActivePropsHead)
    {
        g_ActivePropsHead = prop->next;
    }
    prev = prop->prev;
    if (prev != 0)
    {
        prev->next = prop->next;
    }
    next = prop->next;
    if (next != 0)
    {
        next->prev = prop->prev;
    }
    prop->prev = NULL;
    prop->next = NULL;
}


void propReparent(PropRecord *newChild, PropRecord *host)
{
    newChild->parent = host;

    // Link the newChild into its siblings
    if (host->child)
    {
        host->child->next = newChild;
    }
    newChild->prev = host->child;
    newChild->next = NULL;
    newChild->stan = NULL;
    host->child    = newChild;

}


void propDetach(PropRecord* prop) {
    PropRecord* parent;
    PropRecord* prev;
    PropRecord* next;

    parent = prop->parent;
    if (parent) {
        if (prop == parent->child) {
            parent->child = prop->prev;
        }
        prev = prop->prev;
        if (prev) {
            prev->next = prop->next;
        }
        next = prop->next;
        if (next) {
            next->prev = prop->prev;
        }
        prop->parent = NULL;
        prop->prev = NULL;
        prop->next = NULL;
    }
}


Gfx *propRender(Gfx * gdl, PropRecord *prop, s32 withalpha)
{
    u8 type;

    type = prop->type;

    if (type == PROP_TYPE_CHR)
    {
        gdl = chrRenderProp(prop, gdl, withalpha);
    }
    else if ((type == PROP_TYPE_OBJ) || (type == PROP_TYPE_WEAPON) || (type == PROP_TYPE_DOOR))
    {
        gdl = chrobjRenderProp(prop, gdl, withalpha);
    }
    else if (type == PROP_TYPE_EXPLOSION)
    {
        gdl = explosionRenderPropExplosion(prop, gdl, withalpha);
    }
    else if (type == PROP_TYPE_SMOKE)
    {
        gdl = explosionRenderPropSmoke(prop, gdl, withalpha);
    }
    else if (type == PROP_TYPE_VIEWER)
    {
        gdl = playerRenderProp(prop, gdl, withalpha);
    }

    return gdl;
}


Gfx *propsRenderPass(Gfx *gdl, s32 roomid, s32 renderpass)
{
    s32 flag;
    PropRecord **pp;
    PropRecord *prop;
    s32* rp;
    // propGetRoomIds() writes up to PROPRECORD_STAN_ROOM_LEN ids plus a -1 terminator; other callers use 8
    s32 rooms[8];

    if (bossGetStageNum() == LEVELID_CUBA)
    {
        if (renderpass == 0)
        {
            return gdl;
        }
        else if (renderpass == 2)
        {
            renderpass = 0;
        }
    }

    if ((renderpass == 0) || (renderpass == 2))
    {
        for (pp = g_LastOnScreenProp; --pp >= g_OnScreenPropList; )
        {
            prop = *pp;

            if (prop != NULL)
            {
                flag = 0;

                if ((renderpass == 0) && ((prop->flags & (PROPFLAG_00000020 | PROPFLAG_RENDERPOSTBG)) == 0))
                {
                    flag = 1;
                }
                else if ((renderpass == 2) && ((prop->flags & (PROPFLAG_00000020 | PROPFLAG_RENDERPOSTBG)) == PROPFLAG_RENDERPOSTBG))
                {
                    flag = 1;
                }

                if (flag != 0)
                {
                    flag = 0;
                    propGetRoomIds(prop, rooms);

                    for (rp = rooms; *rp >= 0; rp++)
                    {
                        if (bgRoomIsVisible(*rp))
                        {
                            if (roomid == *rp)
                            {
                                flag = 1;
                            }

                            break;
                        }
                    }

                    if (flag)
                    {
                        gdl = propRender(gdl, prop, 0);
                    }
                }
            }
        }
    }
    else
    {
        for (pp = g_OnScreenPropList; pp < g_LastOnScreenProp; pp++)
        {
            prop = *pp;

            if (prop != NULL)
            {
                flag = 0;
                propGetRoomIds(prop, rooms);

                for (rp = rooms; *rp >= 0; rp++)
                {
                    if (bgRoomIsVisible(*rp))
                    {
                        if (roomid == *rp)
                        {
                            flag = 1;
                        }

                        break;
                    }
                }

                if (flag)
                {
                    if (prop->flags & PROPFLAG_00000020)
                    {
                        gdl = propRender(gdl, prop, 0);
                    }

                    gdl = propRender(gdl, prop, 1);
                }
            }
        }
    }

    return bgScissorCurrentPlayerViewDefault(gdl);
}


void propExecuteTickOperation(PropRecord *prop, TICKOP op)
{
    ObjectRecord *propobj;

    if (op == TICKOP_FREE)
    {
        if ((prop->type == PROP_TYPE_WEAPON) || (prop->type == PROP_TYPE_OBJ))
        {
            propobj = prop->obj;
            if (prop->obj->state & PROPSTATE_RESPAWN) //matches only if called directly (not propobj)
            {
                #ifndef VERSION_EU
                prop->timetoregen = 0x4B0;
                #else
                prop->timetoregen = 0x3E8;
                #endif
                propobj->runtime_bitflags |= RUNTIMEBITFLAG_00000800;
                propobj->runtime_bitflags &= ~RUNTIMEBITFLAG_REMOVE;
                propobj->state &= ~0x80;
                propobj->maxdamage = 0.0f;
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


PropRecord *propFindForInteract(void)
{
    PropRecord **ptr;
    bool checkmore = TRUE;

    g_InteractProp = NULL;

    // Iterate onscreen list near to far
    for (ptr = g_LastOnScreenProp - 1; ptr >= g_OnScreenPropList; ptr--)
    {
        PropRecord *prop = *ptr;

        if (prop)
        {
            if (prop->type == PROP_TYPE_CHR)
            {
                // empty
            }
            else if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON)
            {
                checkmore = objTestForInteract(prop);
            }
            else if (prop->type == PROP_TYPE_DOOR)
            {
                checkmore = doorTestForInteract(prop);
            }
            else if (prop->type == PROP_TYPE_EXPLOSION)
            {
                // empty
            }
            else if (prop->type == PROP_TYPE_SMOKE)
            {
                // empty
            }

            if (!checkmore)
            {
                break;
            }
        }
    }

    return g_InteractProp;
}


bool playerInteract(void)
{
    PropRecord *prop;
    TICKOP tickop;

    prop = propFindForInteract();
    tickop = TICKOP_NONE;

    if (prop)
    {
        switch (prop->type)
        {
            case PROP_TYPE_OBJ:
            case PROP_TYPE_WEAPON:
                tickop = propobjInteract(prop);
                break;
            case PROP_TYPE_DOOR:
                tickop = propdoorInteract(prop);
                break;
            case PROP_TYPE_CHR:
            case PROP_TYPE_PLAYER:
            case PROP_TYPE_EXPLOSION:
            case PROP_TYPE_SMOKE:
                break;
        }

        propExecuteTickOperation(prop, tickop);

        return FALSE;
    }

    return TRUE;
}


/**
* Returns true when the given prop isn't within 400 units of any player prop.
*/
s32 propIsFarFromPlayers(PropRecord* prop)
{
    PropRecord* player_prop;
    coord3d pos_diff;
    s32 rc;
    s32 i;
    s32 player_count;

    player_count = getPlayerCount();
    rc = 1;

    for (i = 0; i < player_count; i++)
    {
        player_prop = g_playerPointers[i]->prop;
        pos_diff.x = player_prop->pos.x - prop->pos.x;
        pos_diff.y = player_prop->pos.y - prop->pos.y;
        pos_diff.z = player_prop->pos.z - prop->pos.z;
        if (sqrtf((pos_diff.x * pos_diff.x) + (pos_diff.y * pos_diff.y) + (pos_diff.z * pos_diff.z)) < 400.0f)
        {
            rc = 0;
            break;
        }
    }

    return rc;
}


/**
 * Tick run once per frame, before any player view is drawn.
 *
 * 1) Advance all AI act states.
 * 2) Update NPC bullet tracers.
 * 3) Handle MP pickup respawns.
 * 4) Update autogun bullet tracers.
 * 5) Tick explosions and smoke.
 * 6) Update MP character bullet tracers.
 * 7) Handle prop delisting or activation.
 */
void propsTickOnce(void)
{
    PropRecord *prop;
    ObjectRecord *obj;
    PropRecord *prev;
    PropRecord *next;
    ChrRecord *chr;
    bool skip_regen_sfx;
    TICKOP tickop;
    bool is_under_60;
    struct ObjectRecord *autogun;
    s32 cmdindex;
    s32 pad;
    ObjectRecord *setupobj;

    // Advance AI states e.g. attacking, walking, dying, etc...
    bgChrsTick();

    prop = propsGetActiveTail();

    while (prop != NULL)
    {
        prev = prop->prev;
        tickop = TICKOP_NONE;

        if (prop->type == PROP_TYPE_CHR)
        {
            chr = prop->chr;

            // Update NPC bullet tracers.
            beamTick(&chr->beams[0]);
            beamTick(&chr->beams[1]);
        }
        else if (((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_WEAPON)) || (prop->type == PROP_TYPE_DOOR))
        {
            obj = prop->obj;
            skip_regen_sfx = FALSE;

            if (prop->timetoregen > 0)
            {
                is_under_60 = TRUE;

                if (prop->timetoregen >= CHROBJ_TIMETOREGEN)
                {
                    is_under_60 = FALSE;
                }

                prop->timetoregen -= g_ClockTimer;

                if (prop->timetoregen < CHROBJ_TIMETOREGEN)
                {
                    if (!is_under_60)
                    {
                        if (!propIsFarFromPlayers(prop))
                        {
                            prop->timetoregen += CHROBJ_TIMETOREGEN;
                        }
                    }
                }

                if (prop->timetoregen <= 0)
                {
                    prop->timetoregen = 0;
                    if (obj->state & PROPSTATE_10)
                    {
                        obj->runtime_bitflags |= RUNTIMEBITFLAG_00001000;
                    }
                    else
                    {
                        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_00001000;
                    }
                }
                else if ((prop->timetoregen < CHROBJ_TIMETOREGEN) && (!is_under_60))
                {
                    if ((obj->maxdamage == 0.0f) && (!(obj->state & PROPSTATE_DESTROYED)))
                    {
                        if (obj->flags & PROPFLAG_INSIDEANOTHEROBJ)
                        {
                            propRemoveFromRooms(prop);
                            propDelist(prop);

                            obj->runtime_bitflags &= ~RUNTIMEBITFLAG_00000800;
                            cmdindex = setupGetCommandIndexByProp(prop);
                            pad = obj->pad;
                            setupobj = setupCommandGetObject(cmdindex + pad);

                            if ((setupobj != NULL) && (setupobj->prop != NULL))
                            {
                                modelSetScale(obj->model, obj->model->scale);
                                propReparent(obj->prop, setupobj->prop);
                                skip_regen_sfx = TRUE;
                            }
                        }
                        else
                        {
                            propEnable(prop);
                            propUpdateRooms(prop);
                            obj->runtime_bitflags &= ~RUNTIMEBITFLAG_00000800;
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

                        obj->maxdamage = 0.0f;
                        obj->state &= ~PROPSTATE_DESTROYED;
                        modelResetDeformedVertices(obj->model);
                    }

                    if (obj->type == PROPDEF_ARMOUR)
                    {
                        ((BodyArmourRecord *) obj)->amount = ((BodyArmourRecord *) obj)->initialamount;
                    }

                    if (!skip_regen_sfx)
                    {
                        chrobjSndCreatePostEventDefault(sndPlaySfx(g_musicSfxBufferPtr, OBJ_REGEN_SFX, NULL), &prop->pos);
                    }

                }
            }

            // Update autogun bullet tracers.
            if (obj->type == PROPDEF_AUTOGUN)
            {
                autogun = prop->obj;
                beamTick(((AutogunRecord *) autogun)->beam);
            }
        }
        else if (prop->type == PROP_TYPE_EXPLOSION)
        {
            tickop = explosionTick(prop);
        }
        else if (prop->type == PROP_TYPE_SMOKE)
        {
            tickop = explosionSmokeTick(prop);
        }
        else
        {
            if (prop->type == PROP_TYPE_VIEWER)
            {
                s32 playernum;
                playernum = playerGetIndexForProp(prop);
                beamTick(&g_playerPointers[playernum]->hands[0].weapon_beam);
                playernum = playerGetIndexForProp(prop);
                beamTick(&g_playerPointers[playernum]->hands[1].weapon_beam);

                // Update MP character bullet tracers.
                if (prop->chr != NULL)
                {
                    if (getPlayerCount() >= 2)
                    {
                        chr = prop->chr;
                        beamTick(&chr->beams[0]);
                        beamTick(&chr->beams[1]);
                    }
                }
            }
        }

        // The tick restructured the prop list which made prop->prev stale. Resume from the prev captured before the tick.
        if (tickop == TICKOP_CHANGEDLIST)
        {
          next = prev;
        }
        else
        {
            next = prop->prev;
            if (tickop == TICKOP_RETICK)
            {
                propDelist(prop);
                propActivateThisFrame(prop);

                if (next == NULL)
                {
                    next = prop;
                }
            }
            else
            {
                propExecuteTickOperation(prop, tickop);
            }
        }

        prop = next;
    }
}


/*
* PD: propsTickPerPlayer (src/game/proptick.c)
*/
void propsTickPerPlayer(void)
{
    TICKOP tickop;
    PropRecord *prop;
    PropRecord *prev;
    PropRecord *propprev;

    prop = propsGetActiveTail();

    while (prop != NULL)
    {
        tickop = TICKOP_NONE;
        prev = prop->prev;

        if (prop->type == PROP_TYPE_CHR)
        {
            tickop = chrTick(prop);
        }
        else if ((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_WEAPON) || (prop->type == PROP_TYPE_DOOR))
        {
            tickop = objTick(prop);
        }
        else if (prop->type == PROP_TYPE_EXPLOSION)
        {
            tickop = explosionPropTick(prop);
        }
        else if (prop->type == PROP_TYPE_SMOKE)
        {
            tickop = smokePropTick(prop);
        }
        else if (prop->type == PROP_TYPE_VIEWER)
        {
            tickop = playerTick(prop);
        }

		if (tickop == TICKOP_CHANGEDLIST)
        {
			propprev = prev;
		}
        else
        {
			propprev = prop->prev;

			if (tickop == TICKOP_RETICK)
            {
				propDelist(prop);
				propActivateThisFrame(prop);

				if (propprev == NULL)
                {
					propprev = prop;
				}
			}
            else
            {
				propExecuteTickOperation(prop, tickop);
			}
		}

		prop = propprev;
    }

    if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
    {
        alarmGasTimerTick();
        aiSoundsTick();
        propsDefragRoomProps();
    }
}


/**
 * Copies stan roomids from prop to array. The list is terminated
 * with an entry of -1.
 *
 * @param self: prop
 * @param roomids: out parameter. Must contain enough space to store room ids.
*/
void propGetRoomIds(PropRecord *self, s32 *roomids)
{
    StandTile *stan;
    s32 i;

    stan = self->stan;

    if (stan == NULL)
    {
        roomids[0] = -1;
    }
    else if ((self->type == PROP_TYPE_VIEWER) && (self->obj == NULL))
    {
        roomids[0] = stan->room;
        roomids[1] = -1;
    }
    else
    {
        for (i=0; self->rooms[i] != 0xff; i++)
        {
            roomids[i] = self->rooms[i];
        }

        roomids[i] = -1;
    }
}


/**
 * @param arg0:
 * @param arg1: out parameter. Bounding coords (x,z) by (x,z).
 * @param arg2: out parameter.
 * @param arg3: out parameter. Maybe ymin. (ground)
 * @param arg4: out parameter. Maybe ymax. (ground + chr/object height)
*/
void propGetCollisionPolygon(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom)
{
    *polygon = NULL;
    *edges = 0;

    if (prop->type == PROP_TYPE_CHR)
    {
        chrUpdateCollisionBounds(prop, polygon, edges, top, bottom);
    }
    else if (prop->type == PROP_TYPE_VIEWER)
    {
        playerGetPropBounds(prop, polygon, edges, top, bottom);
    }
    else if (prop->type == PROP_TYPE_WEAPON)
    {
        // nothing to do
    }
    else if ((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_DOOR))
    {
        objGetCollisionBounds(prop, polygon, edges, top, bottom);
    }
    else if (prop->type == PROP_TYPE_PLAYER)
    {
        // nothing to do
    }
    else if (prop->type == PROP_TYPE_NUL)
    {
        // nothing to do
    }

    return;
}


/**
 * Same as @see propGetCollisionPolygon, but throws away arg3 and arg4.
 *
 * @param arg0:
 * @param arg1: out parameter. Bounding coords (x,z) by (x,z).
 * @param arg2: out parameter.
*/
void propGetCollisionPolygon2d(PropRecord *prop, coord2d **polygon, s32 *edges)
{
    f32 top;
    f32 bottom;

    propGetCollisionPolygon(prop, polygon, edges, &top, &bottom);
}


/**
 * @param point: 3d point to test if inside polygon. Only uses (x,z).
 * @param polygon: Convex polygon. Iterates edges and checks that
 * point is oriented correctly inside all of them.
 * @param edges: Number of edges to iterate in polygon.
*/
s32 polygonContainsPoint(coord3d *point, coord2d *polygon, s32 edges)
{
    /**
     * Stack overflow:
     *
     * In any case, for any convex polygon (including rectangle) the test is
     * very simple: check each edge of the polygon, assuming each edge is
     * oriented in counterclockwise direction, and test whether the point lies
     * to the left of the edge (in the left-hand half-plane). If all edges pass
     * the test - the point is inside. If at least one fails - the point is outside.
     *
     * In order to test whether the point (xp, yp) lies on the left-hand
     * side of the edge (x1, y1) - (x2, y2), you just need to calculate
     *
     * D = (x2 - x1) * (yp - y1) - (xp - x1) * (y2 - y1)
     *
     * https://stackoverflow.com/a/2752753/1462295
    */

    /**
     * Assuming the above is correct, I think that means rectangles (polygons)
     * are clockwise oriented.
    */

    f32 diff;
    s32 i;
    s32 ret = -1;

    if (edges <= 0)
    {
        return 0;
    }

    for (i=0; i<edges; i++)
    {
        // curse you compiler loop unroller
        diff = (    (polygon[(i+1) % edges].f[1] - polygon[i].f[1]) * (point->f[0] - polygon[i].f[0]))
                 - ((polygon[(i+1) % edges].f[0] - polygon[i].f[0]) * (point->f[2] - polygon[i].f[1]));

        if (diff != 0.0f)
        {
            if (i == 0 || ret < 0)
            {
                ret = (diff > 0.0f);

                continue;
            }

            if ((ret != 0) && (diff < 0.0f))
            {
                return 0;
            }

            if ((ret == 0) && (diff > 0.0f))
            {
                return 0;
            }
        }
    }

    return 1;
}


/**
 * @param arg0: prop
 * @param collision_radius: out parameter, will be set to character width or player collision radius.
 * @param height: out parameter, will be set to height
 * @param always_20: out parameter, will be set to either 20 or 30.
*/
void propGetCollisionRadius(PropRecord *prop, f32 *collision_radius, f32 *height, f32 *ymin)
{
    if (prop->type == PROP_TYPE_CHR)
    {
        chrGetChrWidthHeight(prop, collision_radius, height, ymin);
        return;
    }

    if (prop->type == PROP_TYPE_VIEWER)
    {
        playerGetCollisionSize(prop, collision_radius, height, ymin);
        return;
    }

    *collision_radius = 0.0f;
}


f32 propGetGroundY(PropRecord *prop)
{
    if (prop->type == PROP_TYPE_CHR)
    {
        return chrGetChrGround(prop);
    }

    if (prop->type == PROP_TYPE_VIEWER)
    {
        return playerGetStanHeight(g_playerPointers[playerGetIndexForProp(prop)]);
    }

    return 0.0f;
}


void propSetMoving(PropRecord *prop, bool unset) //#MATCH
{
    if (prop->type == PROP_TYPE_CHR)
    {
        chrSetMoving(prop->chr, unset);
    }
    else if (prop->type == PROP_TYPE_VIEWER)
    {
        playerSetMoving(prop, unset);
    }
    else if ((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_DOOR) || (prop->type == PROP_TYPE_WEAPON))
    {
        objSetMoving(prop, unset);
    }
}


/**
 * NTSC address: 0x7F03D0D4.
*/
void propsTickPickups(void)
{
    PropRecord *prop;
    PropRecord *propprev;
    TICKOP op = TICKOP_NONE;

    if (!playerIsInTank() && !g_PlayerInvincible)
    {
        //for each prop in setup
        for (prop = propsGetActiveTail(); prop != NULL; prop = propprev)
        {
            op = TICKOP_NONE;

            if (prop->timetoregen <= 0)
            {
                switch (prop->type)
                {
                    case PROP_TYPE_DOOR:
                    case PROP_TYPE_CHR:
                    case PROP_TYPE_PLAYER:
                    case PROP_TYPE_VIEWER:
                    case PROP_TYPE_EXPLOSION:
                    case PROP_TYPE_SMOKE:
                         break;

                    case PROP_TYPE_OBJ:
                        op = objTickPlayer(prop);
                        break;

                    case PROP_TYPE_WEAPON:
                        op = weaponTickPlayer(prop);
                        break;
                }
            }
            propprev = prop->prev; //not sure why rare put this here and not in the for statement

            propExecuteTickOperation(prop, op);
        }
    }
}


/**
 * Calculates an auto-aim score for a given prop based roughly on how close it is to the center of the screen.
 * Higher scores are better, with 1.0 being the best possible score.
 */
f32 autoaimScoreProp(PropRecord *targetprop, coord3d *aimpos, f32 *world_xbounds, f32 *world_ybounds, coord2d *out_screen)
{
    coord2d aim_screen;
    coord3d testpos;
    coord2d screen_left_edge;
    coord2d screen_right_edge;
    coord2d screen_top_edge;
    coord2d screen_bottom_edge;
    f32 crosshair_x;
    f32 crosshair_y;
    f32 autoaim_top;
    f32 autoaim_bottom;
    f32 autoaim_left;
    f32 autoaim_right;
    f32 score;
    bool passes_horizontal_check;
    f32 horizontal_tolerance = 0.0f;
    PropRecord *playerprop;
    StandTile* line_stan;
    f32 player_los_height;

    /**
     * Define a central auto-aim acceptance region.
     * The sweet spot is 65% vertically in favor of the top of the screen and 50% horizontally.
     */
    autoaim_top = camGetScreenTop() + camGetScreenHeight() * 0.175f;
    autoaim_bottom = camGetScreenTop() + camGetScreenHeight() * 0.825f;
    autoaim_left = camGetScreenLeft() + camGetScreenWidth() * 0.25f;
    autoaim_right = camGetScreenLeft() + camGetScreenWidth() * 0.75f;

    score = -2.0f;

    camViewToScreen(aimpos, &aim_screen);
    testpos.x = world_xbounds[0];
    testpos.y = aimpos->y;
    testpos.z = aimpos->z;
    camViewToScreen(&testpos, &screen_left_edge);
    testpos.x = world_xbounds[1];
    testpos.y = aimpos->y;
    testpos.z = aimpos->z;
    camViewToScreen(&testpos, &screen_right_edge);
    testpos.x = aimpos->x;
    testpos.y = world_ybounds[1];
    testpos.z = aimpos->z;
    camViewToScreen(&testpos, &screen_top_edge);
    testpos.x = aimpos->x;
    testpos.y = world_ybounds[0];
    testpos.z = aimpos->z;
    camViewToScreen(&testpos, &screen_bottom_edge);

    if (screen_bottom_edge.y >= autoaim_top && autoaim_bottom >= screen_top_edge.y)
    {
        passes_horizontal_check = FALSE;
        gunGetCrosshairPos(&crosshair_x, &crosshair_y);
        screen_left_edge.x = mathFloor(screen_left_edge.x);
        screen_right_edge.x = mathCeil(screen_right_edge.x);

        if (currentPlayerGetXAutoAimEnabledRedirect())
        {
            if (screen_left_edge.x <= autoaim_right && autoaim_left <= screen_right_edge.x)
            {
                horizontal_tolerance = (screen_right_edge.x - screen_left_edge.x) * 1.5f;

                if (!isMpGame())
                {
                    horizontal_tolerance = horizontal_tolerance * difficulty;
                }

                passes_horizontal_check = camGetScreenLeft() + 0.5f * camGetScreenWidth() >= (screen_left_edge.x + screen_right_edge.x) * 0.5f - horizontal_tolerance
                    && camGetScreenLeft() + 0.5f * camGetScreenWidth() <= (screen_left_edge.x + screen_right_edge.x) * 0.5f + horizontal_tolerance
                    && autoaim_left <= aim_screen.x
                    && autoaim_right >= aim_screen.x;
            }
        }
        else
        {
            passes_horizontal_check = screen_left_edge.x <= crosshair_x && crosshair_x <= screen_right_edge.x;
        }

        if (passes_horizontal_check)
        {
            playerprop = currentPlayerGetProp();
            line_stan = playerprop->stan;
            player_los_height = playerGetEyeHeight(g_CurrentPlayer);
            playerSetMoving(playerprop, FALSE);

            // Can auto-aim see the target?
            if ((stanTestLineUnobstructed(&line_stan, playerprop->pos.f[0], playerprop->pos.f[2], targetprop->pos.f[0], targetprop->pos.f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, player_los_height, player_los_height, 0.0f, 1.0f) != 0))
            {
                if (line_stan == targetprop->stan)
                {
                    f32 clamped_screen_y = aim_screen.y;

                    if (clamped_screen_y < autoaim_top)
                    {
                        clamped_screen_y = autoaim_top;
                    }
                    else if (clamped_screen_y > autoaim_bottom)
                    {
                        clamped_screen_y = autoaim_bottom;
                    }

                    out_screen->y = clamped_screen_y;

                    if (currentPlayerGetXAutoAimEnabledRedirect())
                    {
                        f32 clamped_screen_x = aim_screen.x;

                        if (clamped_screen_x < autoaim_left)
                        {
                            clamped_screen_x = autoaim_left;
                        }
                        else if (clamped_screen_x > autoaim_right)
                        {
                            clamped_screen_x = autoaim_right;
                        }

                        out_screen->x = clamped_screen_x;
                    }

                    /** If the screen's center x-coord overlaps the target's horizontal span, give it the best possible score of 1.0.
                     *  If this happens, this function's caller, autoaimUpdateTarget, treats this as the winning prop and stops searching.
                     */
                    if (camGetScreenLeft() + 0.5f * camGetScreenWidth() >= screen_left_edge.x && camGetScreenLeft() + 0.5f * camGetScreenWidth() <= screen_right_edge.x)
                    {
                        score = 1.0f;
                    }
                    // If the target is towards the left side of the screen, penalize it based on how far towards the left.
                    else if (camGetScreenLeft() + 0.5f * camGetScreenWidth() >= screen_left_edge.x)
                    {
                        score = 1.0f - ((camGetScreenLeft() + 0.5f * camGetScreenWidth()) - screen_right_edge.x) / horizontal_tolerance;
                    }
                    // If the target is towards the right side of the screen, penalize it based on how far towards the right.
                    else
                    {
                        score = 1.0f - (screen_left_edge.x - (camGetScreenLeft() + 0.5f * camGetScreenWidth())) / horizontal_tolerance;
                    }
                }
            }

            playerSetMoving(playerprop, TRUE);
        }
    }

    return score;
}


/**
 * Iterates on screen props to find autoaim target.
 *
 * US address 7F03D78C.
*/
void autoaimUpdateTarget(void)
{
    f32 best_score;
    coord2d best_screen_aim; // Winning target's screen space aim point.
    f32 candidate_score;
    struct PropRecord **onscreen_prop_iter;
    coord3d target_aimpos;
    coord2d target_world_xbounds;
    coord2d target_world_ybounds;
    struct PropRecord *candidate_prop;
    coord2d candidate_screen_aim;
    struct PropRecord *best_prop;
    struct ChrRecord *candidate_chr;

    best_prop = NULL;
    best_screen_aim = g_DefaultAutoAimCoord;

    if (currentPlayerGetYAutoAimEnabledRedirect() != FALSE)
    {
        best_score = -1.0f;

        // Search all on screen props and record the best target.
        for (onscreen_prop_iter = g_LastOnScreenProp - 1; onscreen_prop_iter >= &g_OnScreenPropList[0]; onscreen_prop_iter--)
        {
            candidate_prop = *onscreen_prop_iter;

            if (candidate_prop == NULL)
            {
                continue;
            }

            if ((candidate_prop->type != PROP_TYPE_CHR)
                    && ((candidate_prop->type != PROP_TYPE_VIEWER)
                        || (candidate_prop->obj == NULL)
                        || (playerGetIndexForProp(candidate_prop) == playerGetCurrentNum())))
            {
                continue;
            }

            // In co-op the other player is a friend, so auto-aim must not lock onto them.
            // In counter-op they are the enemy, and the guard player only
            // fights Bond.
            if (g_CoopMode && !g_CounterOpMode && (candidate_prop->type == PROP_TYPE_VIEWER))
            {
                continue;
            }

            if (counterOpIsGuardPlayer(playerGetCurrentNum()) && (candidate_prop->type != PROP_TYPE_VIEWER))
            {
                continue;
            }

            candidate_chr = candidate_prop->chr;

            // Characters not holding a weapon are exempt from being a target.
            if (((chrGetEquippedWeaponProp(candidate_chr, GUNRIGHT) == 0) && (chrGetEquippedWeaponProp(candidate_chr, GUNLEFT) == 0)))
            {
                continue;
            }

            if ((chrGetOnscreenRenderBounds(candidate_prop, &target_aimpos, &target_world_xbounds, &target_world_ybounds) == 0))
            {
                continue;
            }

            // Score the candidate based on how close it is to the center of the screen.
            candidate_score = autoaimScoreProp(candidate_prop, &target_aimpos, &target_world_xbounds.x, &target_world_ybounds.x, &candidate_screen_aim);

            if (best_score < candidate_score)
            {
                best_score = candidate_score;

                best_prop = candidate_prop;
                best_screen_aim.x = candidate_screen_aim.x;
                best_screen_aim.y = candidate_screen_aim.y;

                // If we find a score of 1.0, we can't do any better, so break out of this search.
                if (1.0f <= candidate_score)
                {
                    break;
                }
            }
        }
    }

    if (best_prop != NULL)
    {

        playerUpdateAutoAimY(best_prop, ((best_screen_aim.y - camGetScreenTop()) / (camGetScreenHeight() * 0.5f)) - 1.0f);

        if (currentPlayerGetXAutoAimEnabledRedirect() != FALSE)
        {
            playerUpdateAutoAimX(best_prop, ((best_screen_aim.x - camGetScreenLeft()) / (camGetScreenWidth() * 0.5f)) - 1.0f);
        }
    }
    else
    {
        playerUpdateAutoAimY(NULL, 0.0f);
        playerUpdateAutoAimX(NULL, 0.0f);
    }
}


s32 propDoorGetCdTypes(PropRecord* prop)
{
    s32 cdtypes;

    if (prop->door->openPosition <= 0.0f)
    {
        cdtypes = CDTYPE_CLOSEDDOORS;
    }
    else
    {
        cdtypes = (prop->door->maxFrac <= prop->door->openPosition)
            ? CDTYPE_OPENDOORS
            : CDTYPE_AJARDOORS;
    }

    if (((s32)prop->door->flags2 * 4) < 0)
    {
        cdtypes |= CDTYPE_DOORSLOCKEDTOAI;
    }

    return cdtypes;
}


/*
* PD: prop_is_of_cd_type
*/
s32 propIsOfCdType(PropRecord* prop, s32 cdtypes) {
    s32 ret;
    ObjectRecord *obj;
    ret = 1;

    if (prop->type == PROP_TYPE_DOOR) {
        if ((cdtypes & CDTYPE_AIOPAQUE)) {
            obj = prop->obj;

            if (obj->flags & PROPFLAG_04000000) {
                ret = 0;
            }
        }

        if (!(cdtypes & CDTYPE_DOORS)) {
            if (!(propDoorGetCdTypes(prop) & cdtypes)) {
                ret = 0;
            }
        }
    } else if (prop->type == PROP_TYPE_VIEWER) {
        if (!(cdtypes & CDTYPE_PLAYERS)) {
            ret = 0;
        }
    } else if (prop->type == PROP_TYPE_CHR) {
        if (!(cdtypes & CDTYPE_CHRS)) {
            ret = 0;
        }
    } else {
        obj = prop->obj;

        if ((cdtypes & CDTYPE_AIOPAQUE) && (obj->flags & PROPFLAG_04000000)) {
            ret = 0;
        }

        if ((cdtypes & CDTYPE_OBJSIMMUNETOEXPLOSIONS) && !(obj->flags & PROPFLAG_INVINCIBLE)) {
            ret = 0;
        }

        if (obj->flags & PROPFLAG_00000800) {
            if (!(cdtypes & CDTYPE_PATHBLOCKER)) {
                ret = 0;
            }
        } else if (!(cdtypes & CDTYPE_OBJS)) {
            ret = 0;
        }
    }

    return ret;
}


/* I think the arguments are lists of roomids but I'm not certain. This function checks if any item in the two lists match */
s32 roomListsOverlap(s32* roomids1, s32* roomids2)
{
    s32* itr1;
    s32* itr2;
    s32 itr1_val;
    s32 itr2_val;

    itr1 = roomids1;
    itr1_val = *itr1;
    while (itr1_val >= 0)
    {
        itr2 = roomids2;
        itr2_val = *itr2;
        while(itr2_val >= 0)
        {
            if (itr1_val == itr2_val) { return 1; }
            itr2++;
            itr2_val = *itr2;
        }
        itr1++;
        itr1_val = *itr1;
    }

    return 0;
}


#define MAXBLOCKS 256
#define ROOMLISTMAX 256
/*
* PD: prop_try_add_to_chunk
* block: canonical name
*/
s32 roomBlockAddProp(s16 propnum, s32 block)
{
    s32 i;
    // Note: The size of the propnums array is 16, but we're only iterating over the first 15 elements.
    //       Is this because the last element is always -1? Seems like a waste.
    for (i = 0; i < 15; i++)
    {
        if (g_RoomPropBlocks[block].propnums[i] < 0)
        {
            g_RoomPropBlocks[block].propnums[i] = propnum;
            return 1;
        }
    }

    return 0;
}


/*
* canonical name newblockforroom
* Description: Find an emtpy chunk that can be assigned to a room
* PD: room_allocate_prop_list_chunk
* room: canonical name
* prevblock: canonical name
*/
s32 roomBlockAllocate(s32 room, s32 prevblock)
{
    s32 i;
    for (i = 0; i < MAXBLOCKS; i++)
    {
        if (g_RoomPropBlocks[i].propnums[0] == -2)
        {
            // This chunk is allowed to be erased
            s32 j;
            for (j = 0; j < 16; j++)
            {
                g_RoomPropBlocks[i].propnums[j] = -1;
            }

            if (prevblock >= 0)
            {
                g_RoomPropBlocks[prevblock].propnums[0xF] = i;
            }
            else
            {
                g_RoomFirstPropBlock[room] = i;
            }

            return i;
        }
    }
    return -1;
}


/*
* PD: prop_register_room
* PD adds an upper bound check to make sure room is not above the max number of rooms
*/
void propAddToRoom(PropRecord *prop, s16 room)
{
   	s32 prevchunk = -1;
    if (room < 0)
    {
        return;
    }
    else
    {
        // Find which chunk to start at
        s32 block = g_RoomFirstPropBlock[room];
        s16 propnum = (prop - g_Props);

        while (block >= 0)
        {
            if (roomBlockAddProp(propnum, block))
            {
                return;
            }

            prevchunk = block;
            block     = g_RoomPropBlocks[block].propnums[0xF];
        }

        // Allocate a new chunk
        block = roomBlockAllocate(room, prevchunk);

        if (block >= 0)
        {
            roomBlockAddProp(propnum, block);
        }
    }
}


/*
* PD: prop_deregister_room
* PD adds an upper bound check to make sure room is not above the max number of rooms
*/
void propRemoveFromRoom(PropRecord* prop, s16 room) {
    bool removed = 0;
    s32 prev = -1;

    if (room >= 0)
    {
        s16 block = g_RoomFirstPropBlock[room];
        s16 propIndex = (prop - g_Props);

        while (block >= 0)
        {
            bool populated = 0;
            s32 i;

            // Check each prop entry in the chunk
            for (i = 0; i < 15; i++)
            {
                if (propIndex == g_RoomPropBlocks[block].propnums[i])
                {
                    g_RoomPropBlocks[block].propnums[i] = -1; // Mark entry as empty
                    removed = 1;
                }
                else if (!populated && g_RoomPropBlocks[block].propnums[i] >= 0)
                {
                    populated = 1;
                }
            }

            if (!populated) // not matching
            {
                // This chunk is empty, so it can be marked as available
                g_RoomPropBlocks[block].propnums[0] = -2;

                if (prev >= 0)
                {
                    g_RoomPropBlocks[prev].propnums[0xF] = g_RoomPropBlocks[block].propnums[0xF];
                }
                else
                {
                    g_RoomFirstPropBlock[room] = g_RoomPropBlocks[block].propnums[0xF];
                }
            }
            else
            {
                prev = block; // not matching
            }

            if (removed)
            {
                return;
            }

            block = g_RoomPropBlocks[block].propnums[0xF];

        }
    }
}


void propUpdateRooms(PropRecord* p)
{
    if (p->type == PROP_TYPE_CHR)
    {
        chrDetectRooms(p->chr);
    } else if ((p->type == PROP_TYPE_OBJ) || (p->type == PROP_TYPE_WEAPON))
    {
        setupUpdateObjectRoomPosition((ObjectRecord* ) p->obj);
    }
}


// Duplicate of the below function with a small extension.
void propRemoveFromRooms(PropRecord *prop)
{
    u8  room;
    u8 *roomIter;

    roomIter = prop->rooms;
    room     = roomIter[0];

    while (room != (u8)-1)
    {
        propRemoveFromRoom(prop, room);
        roomIter += 1;
        room = *roomIter;
    }
    if (!(prop->flags & PROPFLAG_00000010))
    {
        prop->rooms[0] = -1; //hide room
    }
}


void propAddToRooms(PropRecord *prop)
{
    u8  room;
    u8 *roomIter;

    roomIter = prop->rooms;
    room     = roomIter[0];

    while (room != (u8)-1)
    {
        propAddToRoom(prop, room);
        roomIter += 1;
        room = *roomIter;
    }
}


/*
* Recalculate the prop's room list with rooms it is found to be overlapping.
*/
void propCalcRooms(PropRecord *prop, coord3d *bbmin, coord3d *bbmax, f32 radius)
{
    ObjectRecord *obj;
    s32 rooms[7]; // Room payload only, no terminator.
    StandTile *tile;
    s32 count;
    s32 i;
    u8 *src;

    count = 0;
    obj = NULL;

    if (prop->flags & PROPFLAG_00000008) {
        // Seed from the prop's existing room list.
        if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_DOOR) {
            obj = prop->obj;
        }

        if (obj != NULL
            && obj->runtime_bitflags & 0x80
            && obj->projectile->flags & PROJECTILEFLAG_00000008) {
            src = obj->projectile->rooms;
        } else {
            src = prop->rooms;
        }

        for (i = 0; src[i] != 0xff; i++) {
            rooms[i] = src[i];
        }

        count = i;
    } else {
        // Seed from the stan tile locus around the prop's X/Z position.
        tile = prop->stan;
        count = 0;
        stanCircleRooms(&tile, prop->pos.x, prop->pos.z, radius, rooms, &count, 7);
    }

    // Update the room list with neighboring rooms reachable through portals and overlapped by the bounding box.
    bgFindRoomsInBbox(bbmin, bbmax, rooms, &count, 7);

    for (i = 0; i < count; i++) {
        prop->rooms[i] = rooms[i];
    }

    // Commit the rebuilt room list to the prop, terminated by -1.
    prop->rooms[i] = -1;
}


/**
 * Given a list of rooms (terminated by -1), populate the propnums
 * list based on which props are in any of those rooms.
 * PD: roomGetProps
 */
void roomGetProps(s32 *rooms)
{
    s16 *writeptr = g_RoomPropsFound;
    s32 room;
    s32 i;

    room = *rooms;

    // Iterate rooms
    while (room >= 0)
    {
        // Find the chunk to start at
        s32 chunkindex = g_RoomFirstPropBlock[room];

        // Iterate the chunks
        while (chunkindex >= 0)
        {
            // Iterate the propnums within each chunk
            for (i = 0; i < 15; i++)
            {
                s32 propnum = g_RoomPropBlocks[chunkindex].propnums[i];

                if (propnum >= 0)
                {
                    // Check if it's in the list already
                    s16 *ptr = g_RoomPropsFound;

                    while (ptr < writeptr)
                    {
                        if (*ptr == propnum) { break; }
                        ptr++;
                    }

                    if (ptr == writeptr)
                    {
                        // Prop is not in the list, so insert it
                        *writeptr = propnum;
                        writeptr++;
                    }
                }
            }

            chunkindex = g_RoomPropBlocks[chunkindex].propnums[15];
        }

        rooms++;
        room = *rooms;
    }

    *writeptr = -1;
    writeptr++;
    g_NumRoomPropsFound = writeptr - g_RoomPropsFound;
}


void propsDefragRoomProps(void)
{
	s32 i;
	s32 j;
	s32 k;

	// Iterate rooms
	for (i = 0; i < g_MaxNumRooms; i++)
    {
		s32 previndex = g_RoomFirstPropBlock[i];

		if (previndex >= 0)
        {
			s32 nextindex = g_RoomPropBlocks[previndex].propnums[0xF];

			// Iterate this room's chunks but skip the first
			while (nextindex >= 0)
            {
				// Iterate propnums within this chunk
				for (j = 0; j < 15; j++)
                {
					// If this propnum is unallocated
					if (g_RoomPropBlocks[previndex].propnums[j] < 0)
                    {
						// Iterate forward through the chunk list and find a
						// propnum to move back to the prev chunk
						for (k = 0; k < 15; k++)
                        {
							if (g_RoomPropBlocks[nextindex].propnums[k] >= 0)
                            {
								g_RoomPropBlocks[previndex].propnums[j] = g_RoomPropBlocks[nextindex].propnums[k];
								g_RoomPropBlocks[nextindex].propnums[k] = -1;
								break;
							}
						}

						// Check if there are more propnums in the future chunk
						for (; k < 15; k++)
                        {
							if (g_RoomPropBlocks[nextindex].propnums[k] >= 0)
                            {
								break;
							}
						}

						if (k == 15)
                        {
							// There's no more propnums, so this chunk can be removed
							g_RoomPropBlocks[nextindex].propnums[0] = -2;
							g_RoomPropBlocks[previndex].propnums[15] = g_RoomPropBlocks[nextindex].propnums[15];

							nextindex = g_RoomPropBlocks[previndex].propnums[15];

							if (nextindex < 0)
                            {
								break;
							}
						}
					}
				}

				if (nextindex >= 0)
                {
					previndex = nextindex;
					nextindex = g_RoomPropBlocks[nextindex].propnums[15];
				}
			}
		}
	}
}


/**
*/
void switchActivateLinkedDoors(PropRecord *prop)
{
    struct LinkRecord *link;
    struct ObjectRecord *obj;

    obj = prop->obj;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_00000001)
    {
        for (link = g_LevelLoadPropSwitch; link != NULL; link = link->next)
        {
            if (prop == link->first)
            {
                if (link->second != NULL)
                {
                    doorActivateWrapper(link->second);
                }
            }
        }
    }
}


bool doorIsPadlockFree(DoorRecord* door)
{
    if (door->runtime_bitflags & RUNTIMEBITFLAG_PADLOCKEDDOOR)
    {
        LockDoorRecord *padlockeddoor = g_LevelLoadPropLockDoor;

        while (padlockeddoor)
        {
            if (door == padlockeddoor->door
                    && padlockeddoor->lock
                    && padlockeddoor->lock->prop
                    && objIsHealthy(padlockeddoor->lock)) {
                return FALSE;
            }

            padlockeddoor = padlockeddoor->next;
        }
    }

    return TRUE;
}


bool objCanPickupFromSafe(ObjectRecord *obj)
{
    if (obj->flags2 & PROPFLAG2_LINKEDTOSAFE)
    {
        SafeObjectRecord *link = g_LevelLoadPropSafeItem;

        while (link)
        {

            if (obj == link->item && link->door && link->door->prop)
            {
                if (link->door->openPosition <= 0.5f)
                {
                    return FALSE;
                }
            }

            link = link->next;
        }
    }

    return TRUE;
}


f32 bboxGetYmin(ModelRoData_BoundingBoxRecord *modelBoundingBox)
{
    return modelBoundingBox->Bounds.ymin;
}

f32 bboxGetYmax(ModelRoData_BoundingBoxRecord *modelBoundingBox)
{
    return modelBoundingBox->Bounds.ymax;
}
f32 bboxGetZmin(ModelRoData_BoundingBoxRecord *modelBoundingBox)
{
    return modelBoundingBox->Bounds.zmin;
}


f32 bboxRotatedMinX(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    f32 result;

    result = 0.0f;

    if (mtx->m[0][0] >= 0.0f)
    {
        result += (bbox->Bounds.xmin * mtx->m[0][0]);
    }
    else
    {
        result += (bbox->Bounds.xmax * mtx->m[0][0]);
    }

    if (mtx->m[1][0] >= 0.0f)
    {
        result += (bbox->Bounds.ymin * mtx->m[1][0]);
    }
    else
    {
        result += (bbox->Bounds.ymax * mtx->m[1][0]);
    }

    if (mtx->m[2][0] >= 0.0f)
    {
        result += (bbox->Bounds.zmin * mtx->m[2][0]);
    }
    else
    {
        result += (bbox->Bounds.zmax * mtx->m[2][0]);
    }

    return result;
}


f32 bboxRotatedMaxX(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    f32 result;

    result = 0.0f;

    if (mtx->m[0][0] <= 0.0f)
    {
        result += (bbox->Bounds.xmin * mtx->m[0][0]);
    }
    else
    {
        result += (bbox->Bounds.xmax * mtx->m[0][0]);
    }

    if (mtx->m[1][0] <= 0.0f)
    {
        result += (bbox->Bounds.ymin * mtx->m[1][0]);
    }
    else
    {
        result += (bbox->Bounds.ymax * mtx->m[1][0]);
    }

    if (mtx->m[2][0] <= 0.0f)
    {
        result += (bbox->Bounds.zmin * mtx->m[2][0]);
    }
    else
    {
        result += (bbox->Bounds.zmax * mtx->m[2][0]);
    }

    return result;
}


f32 bboxRotatedMinY(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    f32 result;

    result = 0.0f;

    if (mtx->m[0][1] >= 0.0f)
    {
        result += (bbox->Bounds.xmin * mtx->m[0][1]);
    }
    else
    {
        result += (bbox->Bounds.xmax * mtx->m[0][1]);
    }

    if (mtx->m[1][1] >= 0.0f)
    {
        result += (bbox->Bounds.ymin * mtx->m[1][1]);
    }
    else
    {
        result += (bbox->Bounds.ymax * mtx->m[1][1]);
    }

    if (mtx->m[2][1] >= 0.0f)
    {
        result += (bbox->Bounds.zmin * mtx->m[2][1]);
    }
    else
    {
        result += (bbox->Bounds.zmax * mtx->m[2][1]);
    }

    return result;
}


f32 bboxRotatedMaxY(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    f32 result;

    result = 0.0f;

    if (mtx->m[0][1] <= 0.0f)
    {
        result += (bbox->Bounds.xmin * mtx->m[0][1]);
    }
    else
    {
        result += (bbox->Bounds.xmax * mtx->m[0][1]);
    }

    if (mtx->m[1][1] <= 0.0f)
    {
        result += (bbox->Bounds.ymin * mtx->m[1][1]);
    }
    else
    {
        result += (bbox->Bounds.ymax * mtx->m[1][1]);
    }

    if (mtx->m[2][1] <= 0.0f)
    {
        result += (bbox->Bounds.zmin * mtx->m[2][1]);
    }
    else
    {
        result += (bbox->Bounds.zmax * mtx->m[2][1]);
    }

    return result;
}


f32 bboxRotatedMinZ(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    f32 result;

    result = 0.0f;

    if (mtx->m[0][2] >= 0.0f)
    {
        result += (bbox->Bounds.xmin * mtx->m[0][2]);
    }
    else
    {
        result += (bbox->Bounds.xmax * mtx->m[0][2]);
    }

    if (mtx->m[1][2] >= 0.0f)
    {
        result += (bbox->Bounds.ymin * mtx->m[1][2]);
    }
    else
    {
        result += (bbox->Bounds.ymax * mtx->m[1][2]);
    }

    if (mtx->m[2][2] >= 0.0f)
    {
        result += (bbox->Bounds.zmin * mtx->m[2][2]);
    }
    else
    {
        result += (bbox->Bounds.zmax * mtx->m[2][2]);
    }

    return result;
}


f32 bboxRotatedMaxZ(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    f32 result;

    result = 0.0f;

    if (mtx->m[0][2] <= 0.0f)
    {
        result += (bbox->Bounds.xmin * mtx->m[0][2]);
    }
    else
    {
        result += (bbox->Bounds.xmax * mtx->m[0][2]);
    }

    if (mtx->m[1][2] <= 0.0f)
    {
        result += (bbox->Bounds.ymin * mtx->m[1][2]);
    }
    else
    {
        result += (bbox->Bounds.ymax * mtx->m[1][2]);
    }

    if (mtx->m[2][2] <= 0.0f)
    {
        result += (bbox->Bounds.zmin * mtx->m[2][2]);
    }
    else
    {
        result += (bbox->Bounds.zmax * mtx->m[2][2]);
    }

    return result;
}


s32 boxGetHull2d(f32 x1, f32 x2, f32 y1, f32 y2, f32 z1, f32 z2, Mtxf *m, coord2d *poly)
{
    f64 pts[8][2];
    s32 i;
    s32 lim;
    s32 minxi = 0;
    s32 maxxi = 0;
    s32 minzi;
    s32 maxzi = 0;
    s32 rem[4];
    s32 cnt;
    f64 x1d = x1;
    f64 x2d = x2;
    f64 y1d = y1;
    f64 y2d = y2;
    f64 z1d = z1;
    f64 z2d = z2;
    f64 m00 = m->m[0][0];
    f64 m02 = m->m[0][2];
    f64 m10 = m->m[1][0];
    f64 m12 = m->m[1][2];
    f64 m20 = m->m[2][0];
    f64 m22 = m->m[2][2];
    minzi = 0;
    pts[0][0] = ((m00 * x1d) + (m10 * y1d)) + (m20 * z1d);
    pts[0][1] = ((m02 * x1d) + (m12 * y1d)) + (m22 * z1d);
    pts[1][0] = ((m00 * x1d) + (m10 * y1d)) + (m20 * z2d);
    pts[1][1] = ((m02 * x1d) + (m12 * y1d)) + (m22 * z2d);
    pts[2][0] = ((m00 * x1d) + (m10 * y2d)) + (m20 * z1d);
    pts[2][1] = ((m02 * x1d) + (m12 * y2d)) + (m22 * z1d);
    pts[3][0] = ((m00 * x1d) + (m10 * y2d)) + (m20 * z2d);
    pts[3][1] = ((m02 * x1d) + (m12 * y2d)) + (m22 * z2d);
    pts[4][0] = ((m00 * x2d) + (m10 * y1d)) + (m20 * z1d);
    pts[4][1] = ((m02 * x2d) + (m12 * y1d)) + (m22 * z1d);
    pts[5][0] = ((m00 * x2d) + (m10 * y1d)) + (m20 * z2d);
    pts[5][1] = ((m02 * x2d) + (m12 * y1d)) + (m22 * z2d);
    pts[6][0] = ((m00 * x2d) + (m10 * y2d)) + (m20 * z1d);
    pts[6][1] = ((m02 * x2d) + (m12 * y2d)) + (m22 * z1d);
    pts[7][0] = ((m00 * x2d) + (m10 * y2d)) + (m20 * z2d);
    pts[7][1] = ((m02 * x2d) + (m12 * y2d)) + (m22 * z2d);

    for (i = 1; i < 8; i++)
    {
        if ((pts[i][0] < pts[minxi][0]) || ((pts[i][0] == pts[minxi][0]) && (pts[i][1] < pts[minxi][1])))
        {
            minxi = i;
        }
    }

    for (i = 1; i < 8; i++)
    {
        if ((pts[maxzi][1] < pts[i][1]) || ((pts[i][1] == pts[maxzi][1]) && (pts[i][0] < pts[maxzi][0])))
        {
            maxzi = i;
        }
    }

    for (i = 1; i < 8; i++)
    {
        if ((pts[maxxi][0] < pts[i][0]) || ((pts[i][0] == pts[maxxi][0]) && (pts[maxxi][1] < pts[i][1])))
        {
            maxxi = i;
        }
    }

    for (i = 1; i < 8; i++)
    {
        if ((pts[i][1] < pts[minzi][1]) || ((pts[i][1] == pts[minzi][1]) && (pts[minzi][0] < pts[i][0])))
        {
            minzi = i;
        }
    }

    lim = 8;
    cnt = 0;
    i = 0;

filterloop:
    if ((((i != minxi) && (i != maxxi)) && (i != maxzi)) && (i != minzi))
    {
        rem[cnt] = i;
        cnt++;
    }

    i++;

    if (i < lim)
    {
        goto filterloop;
    }

    cnt = 0;
    poly[cnt].x = pts[minxi][0];
    poly[cnt].y = pts[minxi][1];
    cnt++;

    for (i = 0; i < 4; i++)
    {
        s32 index = rem[i];

        if (((pts[index][0] - pts[minzi][0]) * (pts[minxi][1] - pts[minzi][1])) < ((pts[minxi][0] - pts[minzi][0]) * (pts[index][1] - pts[minzi][1])))
        {
            poly[cnt].x = pts[index][0];
            poly[cnt].y = pts[index][1];
            cnt++;
            break;
        }
    }

    poly[cnt].x = pts[minzi][0];
    poly[cnt].y = pts[minzi][1];
    cnt++;

    for (i = 0; i < 4; i++)
    {
        s32 index = rem[i];

        if (((pts[index][0] - pts[maxxi][0]) * (pts[minzi][1] - pts[maxxi][1])) < ((pts[minzi][0] - pts[maxxi][0]) * (pts[index][1] - pts[maxxi][1])))
        {
            poly[cnt].x = pts[index][0];
            poly[cnt].y = pts[index][1];
            cnt++;
            break;
        }
    }

    poly[cnt].x = pts[maxxi][0];
    poly[cnt].y = pts[maxxi][1];
    cnt++;

    for (i = 0; i < 4; i++)
    {
        s32 index = rem[i];

        if (((pts[index][0] - pts[maxzi][0]) * (pts[maxxi][1] - pts[maxzi][1])) < ((pts[maxxi][0] - pts[maxzi][0]) * (pts[index][1] - pts[maxzi][1])))
        {
            poly[cnt].x = pts[index][0];
            poly[cnt].y = pts[index][1];
            cnt++;
            break;
        }
    }

    poly[cnt].x = pts[maxzi][0];
    poly[cnt].y = pts[maxzi][1];
    cnt++;

    for (i = 0; i < 4; i++)
    {
        s32 index = rem[i];

        if (((pts[index][0] - pts[minxi][0]) * (pts[maxzi][1] - pts[minxi][1])) < ((pts[maxzi][0] - pts[minxi][0]) * (pts[index][1] - pts[minxi][1])))
        {
            poly[cnt].x = pts[index][0];
            poly[cnt].y = pts[index][1];
            cnt++;
            break;
        }
    }

    for (i = 0; i < cnt; i++)
    {
        poly[i].x += m->m[3][0];
        poly[i].y += m->m[3][2];
    }

    return cnt;
}


s32 bboxGetHull2d(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx, coord2d *hull)
{
    return boxGetHull2d(bbox->Bounds.xmin, bbox->Bounds.xmax, bbox->Bounds.ymin, bbox->Bounds.ymax, bbox->Bounds.zmin, bbox->Bounds.zmax, mtx, hull);
}


/**
 * Tests whether a world-space point is inside a bound pad's local bbox plus
 * a padding on all axes defined by the radius parameter.
 */
bool padBoundsContainPoint(coord3d *pos, f32 radius, BoundPadRecord *pad)
{
    f32 dx;
    f32 dy;
    f32 dz;
    f32 side[3];
    f32 d;

    dx = pos->x - pad->pos.x;
    dy = pos->y - pad->pos.y;
    dz = pos->z - pad->pos.z;

    side[0] = (pad->up.y * pad->look.z) - (pad->look.y * pad->up.z);
    side[1] = (pad->up.z * pad->look.x) - (pad->look.z * pad->up.x);
    side[2] = (pad->up.x * pad->look.y) - (pad->look.x * pad->up.y);

    d = (pad->look.z * dz) + ((dx * pad->look.x) + (dy * pad->look.y));
    if ((pad->bbox.zmax + radius < d) || (d < pad->bbox.zmin - radius))
    {
        return FALSE;
    }

    d = (pad->up.z * dz) + ((dx * pad->up.x) + (dy * pad->up.y));
    if ((pad->bbox.ymax + radius < d) || (d < pad->bbox.ymin - radius))
    {
        return FALSE;
    }

    d = (dx * side[0]) + (dy * side[1]) + (side[2] * dz);
    if ((pad->bbox.xmax + radius < d) || (d < pad->bbox.xmin - radius))
    {
        return FALSE;
    }

    return TRUE;
}


void modelGetAxisExtents(Model* model, f32* max, f32* min, s32 axis)
{
    ModelNode *node = model->obj->RootNode;
    bool first = TRUE;

    while (node)
    {
        u32 type = node->Opcode & 0xFF;

        if (type == MODELNODE_OPCODE_BBOX)
        {
            struct ModelRoData_BoundingBoxRecord *bbox = &node->Data->BoundingBox;
            Mtxf *mtx = modelFindNodeMtx(model, node, 0);
            f32 dist1;
            f32 dist2;

            if (axis == 0)
            {
                dist1 = bboxRotatedMaxX(bbox, mtx) + mtx->m[3][0];
                dist2 = bboxRotatedMinX(bbox, mtx) + mtx->m[3][0];
            }
            else if (axis == 1)
            {
                dist1 = bboxRotatedMaxY(bbox, mtx) + mtx->m[3][1];
                dist2 = bboxRotatedMinY(bbox, mtx) + mtx->m[3][1];
            }
            else
            {
                dist1 = bboxRotatedMaxZ(bbox, mtx) + mtx->m[3][2];
                dist2 = bboxRotatedMinZ(bbox, mtx) + mtx->m[3][2];
            }

            if (first || dist1 > *max)
            {
                *max = dist1;
            }

            if (first || dist2 < *min)
            {
                *min = dist2;
            }

            first = FALSE;
        }
        else
        {
            // empty
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelGetXYExtents(Model *model, f32 *xmax, f32 *xmin, f32 *ymax, f32 *ymin)
{
    modelGetAxisExtents(model, xmax, xmin, 0);
    modelGetAxisExtents(model, ymax, ymin, 1);
}


/**
 * Project rectangle corners to screen
*/
void projectRectCornersTo2D(coord3d *center, coord2d *xrange, coord2d *yrange, bbox2d *screen)
{
    coord3d corner;
    coord2d tout;

    corner.f[0] = xrange->f[0];
    corner.f[1] = center->f[1];
    corner.f[2] = center->f[2];
    camViewToScreen(&corner, &tout);
    screen->min.x = tout.f[0];

    corner.f[0] = xrange->f[1];
    corner.f[1] = center->f[1];
    corner.f[2] = center->f[2];
    camViewToScreen(&corner, &tout);
    screen->max.x = tout.f[0];

    corner.f[0] = center->f[0];
    corner.f[1] = yrange->f[1];
    corner.f[2] = center->f[2];
    camViewToScreen(&corner, &tout);
    screen->min.y = tout.f[1];

    corner.f[0] = center->f[0];
    corner.f[1] = yrange->f[0];
    corner.f[2] = center->f[2];
    camViewToScreen(&corner, &tout);
    screen->max.y = tout.f[1];
}


ObjectRecord *objFindAtPad(s32 PadId) {
    PropRecord *prop;
    s16 tempPadId = PadId;

    prop = propsGetActiveTail();
    while (prop != NULL)
    {
        if (prop->type == PROP_TYPE_OBJ)
        {
            if (tempPadId == prop->obj->pad)
            {
                return prop->obj;
            }
        }

        prop = prop->prev;
    }

    return NULL;
}


ObjectRecord * objFindAtPos(coord3d *pos, s32 RoomID)
{
    coord2d * polygon;
    s32 edges;
    PropRecord * prop;

    prop = propsGetActiveTail();
    while (prop != NULL)
    {
        if ((prop->type == PROP_TYPE_OBJ) && (RoomID == prop->stan->room))
        {
            propGetCollisionPolygon2d(prop, &polygon, &edges);
            if (polygonContainsPoint(pos, polygon, edges) != 0)
            {
                return (ObjectRecord *) prop->chr;
            }
        }
        prop = prop->prev;
    }

    return NULL;
}


void roomPropListsInit(void);

void propsInit(void)
{
    s32 i;

    difficulty = 1.0f;
    g_ActivePropsTail = g_ActivePropsHead = NULL;
    g_OnScreenPropCount = 0;
    g_OnScreenPropList[0] = NULL;
    g_LastOnScreenProp = g_OnScreenPropList;
    g_FreeProps = g_Props;

    for (i = 0; i < (MAX_PROPS - 1); i++)
    {
        g_Props[i].prev = &g_Props[i + 1];
    }

    roomPropListsInit();
}

void roomPropListsInit(void)
{
    s32 j;
    s32 i;

    g_RoomPropsFound = (s16 *)mempAlloc(ROOMPROPSFOUND_LEN * sizeof(s16), MEMPOOL_STAGE);
    g_RoomFirstPropBlock       = (s16 *)mempAlloc((((g_MaxNumRooms * 4) + 0xF) | 0xF) ^ 0xF, MEMPOOL_STAGE);
    g_RoomPropBlocks             = mempAlloc(ROOMPROPBLOCKS_LEN * sizeof(struct roomproplistblock), MEMPOOL_STAGE);

    g_RoomPropsFound[0] = -1;

    for (i=0; i<g_MaxNumRooms; i++)
    {
        g_RoomFirstPropBlock[i] = -1;
    }

    for (i=0; i<ROOMPROPBLOCKS_LEN; i++)
    {
        g_RoomPropBlocks[i].propnums[0] = -2;

        for (j=1; j<ROOMPROPBLOCK_LEN; j++)
        {
            g_RoomPropBlocks[i].propnums[j] = -1;
        }
    }
}
