/**
 * Guard slots and the per-frame guard tick. chrsAllocateSlots makes one ChrRecord per
 * guard in the setup file plus spares for guards spawned in play. chrTick runs from the
 * prop loop for each player's view; the first pass advances the guard (AI and action via
 * chrActionTick, animation, moving with collision), every pass draws it. Also here:
 * collision bounds, the background AI chrs, and the auto-aim screen bounds.
 * Drawing is in chrrender.c, bullet hits in chrhit.c, animation tables in chranims.c.
 * See docs/subsystems/guards.md.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <macro.h>
#include <random.h>
#include <snd.h>
#include <math.h>
#include "bondaicommands.h"
#include "game/world/bg.h"
#include "game/menu/cheat.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/chraction.h"
#include "game/model/chrobjdata.h"
#include "game/gfx/dyn.h"
#include "game/gfx/vtxstore.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/save/file.h"
#include "game/prop/propobj.h"
#include "game/prop/explosion.h"
#include "game/player/gun.h"
#include "game/model/anim.h"
#include "joy.h"
#include "game/level/lv.h"
#include "game/text/language.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/player/player.h"
#include "game/nav/stan.h"
#include "game/world/surface.h"
#include "game/math/physics.h"
#include "game/world/bgfog.h"
#include "game/math/line_tri_intersect.h"
#include "game/level/gamemode.h"

#ifdef VERSION_EU
#define GROUND_SMOOTH_FACTOR 0.118799984f /* 0x3DF34D68 (PAL-scaled 0.1) */
#define FALLSPEED_DECAY      0.8812f      /* 0x3F619653 (PAL-scaled 0.9) */
#else
#define GROUND_SMOOTH_FACTOR 0.100000024f /* 0x3DCCCCD0 */
#define FALLSPEED_DECAY      0.9f         /* 0x3F666666 */
#endif

static void chrUpdateAimProperties(ChrRecord *self);
static void chrUpdateAnim(ChrRecord *chr, s32 tickamount);

f32 g_ChrAnimRate = 0;
s32 g_ChrDebugAnimOverride = 0;

s32 g_ChrDebugAnimIndex = 0;
s32 g_ChrDebugPaused = 0;
s32 g_ChrDebugStep = 0;

s32 g_ChrShowPatrols = FALSE;
s32 g_NextChrNum = 5000;
ChrRecord *g_ChrSlots = NULL;

/* For each chr slot, the two fields the per-frame scans of every slot read (chrnum, only
   while the slot holds a chr, and lastseetarget60), so chrFindByLiteralId and bgChrsTick
   don't pull in a 464-byte ChrRecord per slot. chrSlotSync keeps it up to date. */
ChrSlotHot *g_ChrSlotHot = NULL;
s32 g_NumChrSlots = 0;

rgba_u8 gBloodColour = {{0x5a, 0, 0, 0}};

u32 num_bodies = 0;
u32 num_male_heads = 0;
u32 num_female_heads = 0;

s32 list_of_bodies[] = {
    BODY_Jungle_Commando,BODY_St_Petersburg_Guard,BODY_Russian_Soldier,BODY_Russian_Infantry,
    BODY_Janus_Special_Forces,BODY_Brosnan_Tuxedo,BODY_Boris,BODY_Ourumov,
    BODY_Trevelyan_Janus,BODY_Valentin_,BODY_Xenia,BODY_Baron_Samedi,
    BODY_Jaws,BODY_Mayday,BODY_Oddjob,BODY_Natalya_Skirt,
    BODY_Janus_Marine,BODY_Russian_Commandant,BODY_Siberian_Guard_1_Mishkin,BODY_Naval_Officer,
    BODY_Siberian_Special_Forces,BODY_Special_Operations_Uniform,BODY_Formal_Wear,BODY_Jungle_Fatigues,
    BODY_Unused_Female,BODY_Rosika,BODY_Scientist_2_Female,BODY_Civilian_1_Female,
    BODY_Unused_Male_1,BODY_Unused_Male_2,BODY_Civilian_4,BODY_Civilian_2,
    BODY_Civilian_3,BODY_Scientist_1_Male,BODY_Brosnan_Tuxedo,BODY_Brosnan_Tuxedo,
    BODY_Brosnan_Tuxedo,BODY_Helicopter_Pilot,BODY_Siberian_Guard_2,BODY_Arctic_Commando,
    BODY_Moonraker_Elite_1_Male,BODY_Moonraker_Elite_2_Female,-1,
};

s32 random_male_heads[] = {
    HEAD_Male_Jim,HEAD_Male_Chris,HEAD_Male_Lee,HEAD_Male_Graeme,HEAD_Male_Steve_H,
    HEAD_Male_Neil,HEAD_Male_Robin,HEAD_Male_Des,HEAD_Male_Grant,HEAD_Male_Dave_Dr_Doak,
    HEAD_Male_Karl,HEAD_Male_Alan,HEAD_Male_Pete,HEAD_Male_Martin,HEAD_Male_Mark,
    HEAD_Male_Duncan,HEAD_Male_Shaun,HEAD_Male_Dwayne,HEAD_Male_B,HEAD_Male_Steve_Ellis,
    HEAD_Male_Joel,HEAD_Male_Scott,HEAD_Male_Joe_Altered,HEAD_Male_Ken,HEAD_Male_Joe,
    -1
};

s32 random_female_heads[] = {
    HEAD_Female_Sally,HEAD_Female_Marion_Rosika,HEAD_Female_Mandy,HEAD_Female_Vivien, -1
};

u32 current_random_body = 0;
u32 current_random_male_head = 0;
u32 current_random_female_head = 0;

/*
* Enemy accuracy modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiAccuracyModifier = 1.0f;

/*
* Enemy damage modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiDamageModifier = 1.0f;

f32 g_AiHealthModifier = 1.0f;

/*
* Enemy reaction speed modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiReactionSpeed = 1.0f;

/**
 * Count of number of guards that have recently seen bond:
 *     (guard->lastseetarget60 > 0) && (g_GlobalTimer - guard->lastseetarget60 < 120)
 * Updated every tick.
*/
s32 g_SeenBondRecentlyGuardCount = 0;


s32 objectiveregisters1 = 0;

/**
 * List of characters updated every tick.
*/
ChrRecord *g_BgChrs = NULL;
static ChrRecord g_BgChrTemplate = { 0 };

/**
 * Number of items in g_BgChrs.
*/
s32 g_NumBgChrs = 0;

/**
 * Default factor in chrGetRunAnimSpeed.
*/
f32 g_ChrSpeedWalk = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_running.
*/
f32 g_ChrSpeedRun = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_sprinting.
*/
f32 g_ChrSpeedSprint = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_walking_unarmed.
*/
f32 g_ChrSpeedWalkUnarmed = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_running_one_handed_weapon.
*/
f32 g_ChrSpeedRunOneHanded = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by first ANIM_DATA_sprinting_one_handed_weapon.
*/
f32 g_ChrSpeedSprintOneHanded = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_walking_female.
*/
f32 g_ChrSpeedWalkFemale = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_running_female.
*/
f32 g_ChrSpeedRunFemale = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by duplicate ANIM_DATA_sprinting_one_handed_weapon.
*/
f32 g_ChrSpeedSprintOneHanded2 = 0;


s32 chrsGetNumSlots(void)
{
    return g_NumChrSlots;
}


Vertex *dynAllocateVerticesDiscard(s32 count)
{
    return dynAllocate(count * sizeof(Vertex));
}


/**
 * CHRHIDDEN_MOVING is set while chrMoveWithCollision moves the guard, so that its own
 * collision bounds don't block it.
 */
void chrSetMoving(ChrRecord *self, bool unset)
{
    if (unset)
    {
        self->hidden &= ~CHRHIDDEN_MOVING;
    }
    else
    {
        self->hidden |= CHRHIDDEN_MOVING;
    }
}


// What a guard's movement collides with
#define CHR_MOVE_CDTYPES (CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER)

/**
 * The tile at (x, z) if a guard of this width and height can move there in a straight
 * line from src, otherwise NULL.
 */
static StandTile *chrTryMoveTo(StandTile *stan, coord3d *src, f32 x, f32 z, f32 width, f32 top, f32 bottom)
{
    StandTile *tile = stan;

    if (stanTestLineUnobstructed(&tile, src->x, src->z, x, z, CHR_MOVE_CDTYPES, top, bottom, 0.0f, 1.0f)
        && (stanTestVolume(&tile, x, z, width, CHR_MOVE_CDTYPES, top, bottom) < 0))
    {
        return tile;
    }

    return NULL;
}


/**
 * Tries the part of the move (delta, from src) along the direction (dirx, dirz). On
 * success moves dst there and returns the new tile.
 */
static StandTile *chrTrySlide(ChrRecord *guard, StandTile *stan, coord3d *src, coord3d *dst, coord3d *delta, f32 dirx, f32 dirz, f32 width, f32 top, f32 bottom)
{
    StandTile *tile;
    f32 scale;
    f32 along;
    f32 x;
    f32 z;

    scale = 1.0f / sqrtf((dirz * dirz) + (dirx * dirx));
    dirx *= scale;
    dirz *= scale;

    along = (dirz * delta->z) + (delta->x * dirx);
    x = (dirx * along) + src->x;
    z = (dirz * along) + src->z;

    tile = chrTryMoveTo(stan, src, x, z, width, top, bottom);

    if (tile != NULL)
    {
        dst->x = x;
        dst->z = z;
        guard->invalidmove = 2;
    }

    return tile;
}


/**
 * Moves a guard from src towards dst. If the way is blocked, slides along the edge it hit,
 * or around the edge's end if dst is within the guard's width of a corner, and moves dst
 * to where it ends up. Returns the new tile, or NULL (and invalidmove = 1) if the guard
 * can't move. invalidmove is 0 for a clean move (with updateLastMoveOk) and 2 for a slide.
 */
StandTile *chrMoveWithCollision(ChrRecord *guard, StandTile *stan, coord3d *src, coord3d *dst, s32 updateLastMoveOk)
{
    StandTile *ret;
    f32 top;
    f32 bottom;
    f32 width;
    coord3d edgeA;
    coord3d edgeB;
    coord3d delta;
    f32 dx;
    f32 dz;
    f32 widthsq;

    chrGetChrWidthHeight(guard->prop, &width, &top, &bottom);
    chrSetMoving(guard, FALSE);
    stanResetHits();

    ret = chrTryMoveTo(stan, src, dst->x, dst->z, width, top, bottom);

    if (ret != NULL)
    {
        if (updateLastMoveOk)
        {
            guard->invalidmove = 0;
            guard->lastmoveok60 = g_GlobalTimer;
        }
    }
    else if (stanGetCollisionEdge(&edgeA, &edgeB))
    {
        delta.x = dst->x - src->x;
        delta.z = dst->z - src->z;

        // Slide along the edge
        if ((edgeA.x != edgeB.x) || (edgeA.z != edgeB.z))
        {
            ret = chrTrySlide(guard, stan, src, dst, &delta, edgeB.x - edgeA.x, edgeB.z - edgeA.z, width, top, bottom);
        }

        if (ret == NULL)
        {
            // Slide around a corner the guard is pushing into: at right angles to the
            // line from src to the corner.
            widthsq = width * width;

            dx = edgeA.x - dst->x;
            dz = edgeA.z - dst->z;

            if (((dz * dz) + (dx * dx)) <= widthsq)
            {
                if ((edgeA.x != src->x) || (edgeA.z != src->z))
                {
                    ret = chrTrySlide(guard, stan, src, dst, &delta, -(edgeA.z - src->z), edgeA.x - src->x, width, top, bottom);
                }
            }
            else
            {
                dx = edgeB.x - dst->x;
                dz = edgeB.z - dst->z;

                if ((((dz * dz) + (dx * dx)) <= widthsq) && ((edgeB.x != src->x) || (edgeB.z != src->z)))
                {
                    ret = chrTrySlide(guard, stan, src, dst, &delta, -(edgeB.z - src->z), edgeB.x - src->x, width, top, bottom);
                }
            }
        }
    }

    chrSetMoving(guard, TRUE);

    if (ret == NULL)
    {
        guard->invalidmove = 1;
    }

    return ret;
}


s32 chrRootMoveCallback(Model *model, coord3d *src, coord3d *dst, f32 *ground_y)
{
    ChrRecord *chr;
    s32 moved;
    f32 ground;
    coord3d *groundpos;
    f32 tmp;
    s32 i;
    StandTile *tile;
    union ModelRwData *rwdata;

    chr = model->chr;
    moved = 0;
    ground = 0.0f;
    groundpos = src;

    if (chr->prop->stan != NULL)
    {
        // Knockback of a death from a hit (chrReactToHit), slowing to a stop
        if ((chr->actiontype == ACT_DIE) && (chr->act_die.timeextra > 0.0f))
        {
#ifdef BUGFIX_R1
            tmp = ((model->playspeed * g_GlobalTimerDeltaRaw) * (chr->act_die.timeextra - chr->act_die.elapseextra)) / chr->act_die.timeextra;
#else
            tmp = ((model->playspeed * g_GlobalTimerDelta) * (chr->act_die.timeextra - chr->act_die.elapseextra)) / chr->act_die.timeextra;
#endif

            dst->x += chr->act_die.extraspeed.x * tmp;
            dst->y += chr->act_die.extraspeed.y * tmp;
            dst->z += chr->act_die.extraspeed.z * tmp;

#ifdef BUGFIX_R1
            chr->act_die.elapseextra += g_GlobalTimerDeltaRaw * model->playspeed;
#else
            chr->act_die.elapseextra += g_GlobalTimerDelta * model->playspeed;
#endif

            if (chr->act_die.timeextra < chr->act_die.elapseextra)
            {
                chr->act_die.timeextra = 0.0f;
            }
        }

        dst->x += chr->fallspeed.x * g_GlobalTimerDelta;
        dst->z += chr->fallspeed.z * g_GlobalTimerDelta;

        tile = chrMoveWithCollision(chr, chr->prop->stan, src, dst, 1);

        if (tile != NULL)
        {
            chr->prop->stan = tile;
            groundpos = dst;
            moved = 1;
        }

        if (!(chr->chrflags & CHRFLAG_LOCK_Y_POS))
        {
            ground = stanGetFloorY(chr->prop->stan, groundpos->x, groundpos->z);
            chr->ground = ground;

            if (chr->chrflags & CHRFLAG_INIT)
            {
                rwdata = modelGetNodeRwData(model, model->obj->RootNode);

                chr->chrflags &= ~CHRFLAG_INIT;
                chr->manground = chr->ground;
                chr->sumground = chr->ground / GROUND_SMOOTH_FACTOR;

                rwdata->Header.framePos.y = rwdata->Header.nextPos.y;
            }
            else
            {
                if ((chr->fallspeed.y != 0.0f) || (chr->manground > chr->ground))
                {
                    physFall(&chr->manground, &chr->fallspeed.y, g_GlobalTimerDelta);

                    if (chr->manground <= chr->ground)
                    {
                        chr->manground = chr->ground;
                        chr->sumground = chr->ground / GROUND_SMOOTH_FACTOR;
                        chr->fallspeed.y = 0.0f;
                    }
                }

                // On the ground: follow it smoothly and let the push from a fall or
                // explosion die away.
                if (chr->manground <= chr->ground)
                {
                    for (i = 0; i < g_ClockTimer; i++)
                    {
                        chr->sumground = (chr->sumground * FALLSPEED_DECAY) + chr->ground;
                        chr->fallspeed.x *= FALLSPEED_DECAY;
                        chr->fallspeed.z *= FALLSPEED_DECAY;
                    }

                    chr->manground = chr->sumground * GROUND_SMOOTH_FACTOR;

                    if ((chr->fallspeed.x < 0.1f) && (chr->fallspeed.x > -0.1f)
                        && (chr->fallspeed.z < 0.1f) && (chr->fallspeed.z > -0.1f))
                    {
                        chr->fallspeed.z = 0.0f;
                        chr->fallspeed.x = 0.0f;
                    }
                }
            }

            dst->y += chr->manground - ground;
        }
        else
        {
            ground = chr->ground;
        }
    }

    *ground_y = ground;

    if (!moved)
    {
        dst->x = src->x;
        dst->z = src->z;
    }

    return 1;
}

#undef GROUND_SMOOTH_FACTOR
#undef FALLSPEED_DECAY


s32 chrGetNumFree(void)
{
    s32 count = 0;
    s32 i;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model == NULL)
        {
            count++;
        }
    }

    return count;
}


void chrSetMaxDamage(ChrRecord *chr, f32 maxdamage)
{
    chr->maxdamage = file007Health() * maxdamage;
}


void chrAddHealth(ChrRecord *chr, f32 health)
{
    chr->damage -= health * file007Health();
}


PropRecord *chrInit(PropRecord *prop, Model *model, coord3d *pos, f32 angle, StandTile *tile, AIRecord *ailist)
{
    ChrRecord *chr;
    s32 i;

    chr = NULL;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model == NULL)
        {
            chr = &g_ChrSlots[i];
            break;
        }
    }

    // All chr slots in use: the original wrote through a NULL chr here.
    if (chr == NULL)
    {
        propFree(prop);
        return NULL;
    }

    prop->type = PROP_TYPE_CHR;
    prop->chr = chr;
    prop->pos.f[0] = pos->f[0];
    prop->pos.f[1] = pos->f[1];
    prop->pos.f[2] = pos->f[2];
    prop->stan = tile;

    modelSetRootMoveCallback(model, (s32) chrRootMoveCallback);

    model->chr = chr;

    modelSetRootPosition(model, pos);
    modelSetRootRotY(model, angle);

#if defined(VERSION_EU)
    modelSetAnimPlaySpeed(model, g_ChrAnimRate * 1.2f, 0.0f);
#else
    modelSetAnimPlaySpeed(model, g_ChrAnimRate, 0.0f);
#endif

    chr->chrnum = (s16)g_NextChrNum;
    g_NextChrNum++;
    chr->headnum = 0;
    chr->bodynum = 0;
    chr->prop = prop;
    chr->model = model;
    chr->jointList = NULL;
    chr->numarghs = 0;
    chr->lastwalk60 = 0;
    chr->invalidmove = 0;
    chr->lastmoveok60 = g_GlobalTimer;
    chr->lastseetarget60 = 0;
    chrSlotSync(chr);
    chr->seen_bond_time = 0;
    chr->lastheartarget60 = 0;
    chr->numclosearghs = 0;
    chr->shotbondsum = 0.0f;
    chr->damage = 0.0f;
    chr->visionrange = 250.0f;
    chr->hearingscale = 1.0f;

    chr->maxdamage = file007Health() * 4.0f;
    objGetTileShading(prop, &chr->nextcol);

    chr->shadecol.rgba[0] = chr->nextcol.rgba[0];
    chr->shadecol.rgba[1] = chr->nextcol.rgba[1];
    chr->shadecol.rgba[2] = chr->nextcol.rgba[2];
    chr->shadecol.rgba[3] = chr->nextcol.rgba[3];
    chr->fadealpha = 0xFF;
    chr->gunSounds[0].ptr_SEbuffer1 = NULL;
    chr->gunSounds[0].ptr_SEbuffer2 = NULL;
    chr->gunSounds[1].ptr_SEbuffer1 = NULL;
    chr->gunSounds[1].ptr_SEbuffer2 = NULL;
    chr->nextGunSoundTime[0] = 0;
    chr->nextGunSoundTime[1] = 0;
    chr->chrflags = CHRFLAG_INIT;
    chr->hidden = CHRHIDDEN_NONE;
    chr->sumground = 0.0f;
    chr->manground = 0.0f;
    chr->ground = 0.0f;
    chr->fallspeed.f[0] = 0.0f;
    chr->fallspeed.f[1] = 0.0f;
    chr->fallspeed.f[2] = 0.0f;
    chr->prevpos.f[0] = pos->f[0];
    chr->prevpos.f[1] = pos->f[1];
    chr->prevpos.f[2] = pos->f[2];
    chr->actiontype = 0;
    chr->sleep = 0;
    chr->ailist = ailist;
    chr->aioffset = 0;
    chr->aireturnlist = -1;
    chr->morale = 0;
    chr->alertness = 0;
    chr->flags2 = 0;
    chr->random = 0;
    chr->timer60 = 0;
    chr->padpreset1 = -1;
    chr->chrseeshot = -1;
    chr->chrseedie = -1;
    chr->chrpreset1 = -1;
    chr->beams[0].age = -1;
    chr->beams[1].age = -1;
    chr->firecount[0] = 0;
    chr->firecount[1] = 0;
    chr->grenadeprob = 0;
    chr->accuracyrating = 0;
    chr->speedrating = 0;
    chr->arghrating = 0;
    chr->flinchcnt = -1;
    chr->aimuplshoulder = 0.0f;
    chr->aimuprshoulder = 0.0f;
    chr->aimupback = 0.0f;
    chr->aimsideback = 0.0f;
    chr->aimendlshoulder = 0.0f;
    chr->aimendrshoulder = 0.0f;
    chr->aimendback = 0.0f;
    chr->aimendsideback = 0.0f;
    chr->aimendcount = 0;
    chr->weapons_held[0] = NULL;
    chr->weapons_held[1] = NULL;
    chr->handle_positiondata_hat = NULL;
    chr->targetplayer = 0;
    chr->targetlock60 = 0;
    chr->chrwidth = 20.0f;
    chr->chrheight = 185.0f;

    chrRootMoveCallback(model, &prop->pos, &prop->pos, &chr->ground);
    chrDetectRooms(chr);

    return prop;
}


PropRecord *chrAllocate(Model *model, coord3d *pos, f32 angle, StandTile *tile, AIRecord *ailist)
{
    PropRecord *ret;

    ret = propAllocate();

    if (ret != NULL)
    {
        ret = chrInit(ret, model, pos, angle, tile, ailist);
    }

    return ret;
}


void chrFreeResources(PropRecord *prop)
{
    ChrRecord *chr;
    Model *model;
    PropRecord *child;
    PropRecord *prev;
    ObjectRecord *obj;

    chr = prop->chr;
    model = chr->model;

    for (s32 i = 0; i < 2; i++)
    {
        if ((chr->gunSounds[i].ptr_SEbuffer1 != NULL) && sndGetPlayingState(chr->gunSounds[i].ptr_SEbuffer1))
        {
            sndDeactivate(chr->gunSounds[i].ptr_SEbuffer1);
        }

        if ((chr->gunSounds[i].ptr_SEbuffer2 != NULL) && sndGetPlayingState(chr->gunSounds[i].ptr_SEbuffer2))
        {
            sndDeactivate(chr->gunSounds[i].ptr_SEbuffer2);
        }
    }

    modelResetDeformedVertices(model);
    propRemoveFromRooms(prop);

    for (child = propChild(prop); child != NULL; child = prev)
    {
        obj = child->obj;
        prev = child->prev;
        objDetach(child);
        objFreePermanently(obj, TRUE);
    }

    modelFreeAnimSlot(model);

    chr->model = NULL;
    chr->chrnum = -1;
    chrSlotSync(chr);

    for (s32 i = 0; i < 2; i++)
    {
        if (chr->beams[i].age >= 0)
        {
            chr->beams[i].age = -1;
            g_NumLiveBeams--;
        }
    }

    if (chr->jointList != NULL)
    {
        jointListFree(chr->jointList);
    }
}


void chrsSetAnimRate(f32 rate)
{
    s32 i;

    g_ChrAnimRate = rate;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model != NULL)
        {
#if defined(REFRESH_PAL)
            modelSetAnimPlaySpeed(g_ChrSlots[i].model, g_ChrAnimRate * 1.2f, 600.0f);
#else
            modelSetAnimPlaySpeed(g_ChrSlots[i].model, g_ChrAnimRate, 600.0f);
#endif
        }
    }
}


f32 chrsGetAnimRate(void)
{
    return g_ChrAnimRate;
}


/**
 * Turns the aim (shoulders and back) a step towards its target, so it gets there over
 * aimendcount frames.
 */
static void chrUpdateAimProperties(ChrRecord *self)
{
    f32 mult;

    if (self->aimendcount >= 2)
    {
#if defined(BUGFIX_R1)
        mult = g_GlobalTimerDeltaRaw / (f32)self->aimendcount;
#else
        mult = g_GlobalTimerDelta / (f32)self->aimendcount;
#endif

        if (mult > 1.0f)
        {
            mult = 1.0f;
        }

        self->aimuplshoulder += ((self->aimendlshoulder - self->aimuplshoulder) * mult);
        self->aimuprshoulder += ((self->aimendrshoulder - self->aimuprshoulder) * mult);
        self->aimupback += ((self->aimendback - self->aimupback) * mult);
        self->aimsideback += ((self->aimendsideback - self->aimsideback) * mult);
        self->aimendcount -= g_ClockTimer;

        return;
    }

    self->aimuplshoulder = self->aimendlshoulder;
    self->aimuprshoulder = self->aimendrshoulder;
    self->aimupback = self->aimendback;
    self->aimsideback = self->aimendsideback;
}


void chrSetHiddenToRandom(ChrRecord *self)
{
    u32 rand;

    if (self->flinchcnt < 0)
    {
        self->flinchcnt = 1;
        self->hidden &= ~CHRHIDDEN_RAND_FLINCH_MASK;

        // roll for bits 12,13.
        // rand -> value
        // 2 => nothing
        // 0 => set bit 12
        // 1 => set bit 13
        rand = randomGetNext() % 3U;

        if (rand == 0)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_1;
        }
        else if (rand == 1)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_2;
        }

        // roll for bits 14,15.
        // rand -> value
        // 2 => nothing
        // 0 => set bit 14
        // 1 => set bit 15
        rand = randomGetNext() % 3U;

        if (rand == 0)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_4;
        }
        else if (rand == 1)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_8;
        }
    }
}


/**
 * For visibility, player collision, tank collision, bullet collision, and explosion damage
 * tests the game needs to know which room(s) a character is in. This allows the game to perform
 * tests on only characters in loaded rooms, not the entire stage.
 */
void chrDetectRooms(ChrRecord *self)
{
    PropRecord *myprop;
    coord3d     lowerbounds;
    coord3d     upperbounds;

    // Create a roughly character sized bounding box.
    myprop        = self->prop;
    lowerbounds.x = myprop->pos.x - 50.0f;
    lowerbounds.y = self->ground - 1.0f;
    lowerbounds.z = myprop->pos.z - 50.0f;
    upperbounds.x = myprop->pos.x + 50.0f;
    upperbounds.y = myprop->pos.y + 100.0f;
    upperbounds.z = myprop->pos.z + 50.0f;

    // Delist the character prop from its previous room(s)
    propRemoveFromRooms(myprop);

    // Detect rooms overlapped by the bounding box
    propCalcRooms(myprop, &lowerbounds, &upperbounds, 50.0f);

    // Re-register the character prop in those rooms
    propAddToRooms(myprop);
}


static void chrUpdateAnim(ChrRecord *chr, s32 tickamount)
{
    Model *model;
    PropRecord* prop;

    model = chr->model;
    prop = chr->prop;

    if (!(chr->hidden & CHRHIDDEN_FREEZE))
    {
        modelGetRootPosition(model, &chr->prevpos);
        modelTickAnim(model, tickamount, 1);
        modelCalcPosition(model);
        objGetTileShading(prop, &chr->nextcol);
        modelGetRootPosition(model, &prop->pos);
        chrDetectRooms(chr);

        return;
    }

    modelCalcPosition(model);
    modelGetRootPosition(model, &prop->pos);
}


/**
 * This function does the following:
 * - Drive character animations
 * - Remove characters if needed
 * - Held weapon garbage collection
 * - Character visibility tests
 * - Render characters/hats/held weapons
 * - Drop held items
 * - Fire held weapons
 */
s32 chrTick(PropRecord *prop)
{
    ModelRenderData renderdata;
    ChrRecord *chr;
    Model *model;
    s32 headSwitchVisible;
    s32 headVisible;
    s32 tickamount;
    s32 coopRepeat;

    renderdata = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};
    chr = prop->chr;
    model = chr->model;
    headVisible = 1;
    tickamount = g_ClockTimer;

    // Co-op: props are ticked once for each player's view. A chr's AI,
    // actions and animation must only advance on the first pass, or guards
    // run, aim and shoot twice as fast. The later pass doesn't update the
    // animation at all: with no time passing it would only redo the same
    // position, room and shading work.
    coopRepeat = gameIsTwoPlayerMission() && (prop->type == PROP_TYPE_CHR)
        && (playerGetShuffledIndex(playerGetCurrentNum()) != 0);

    if (coopRepeat)
    {
        tickamount = 0;
    }

    if ((!(chr->chrflags & CHRFLAG_HIDDEN)) || (chr->chrflags & CHRFLAG_00040000))
    {
        if (g_ChrDebugAnimOverride)
        {
            if (animation_table_ptrs1[g_ChrDebugAnimIndex] != ANIM_UNUSED)
            {
                if (modelGetAnim(model) != animation_table_ptrs1[g_ChrDebugAnimIndex])
                {
                    modelSetAnimation(model, animation_table_ptrs1[g_ChrDebugAnimIndex], 0, 0.0f, 0.5f, 0.0f);
                }
            }
        }
        else if (!coopRepeat)
        {
            chrActionTick(chr);

            if (chr->model == NULL)
            {
                return TICKOP_FREE;
            }
        }

        if (g_ChrDebugPaused)
        {
            tickamount = 0;

            if (g_ChrDebugStep)
            {
                tickamount = 1;
            }
        }
    }

    if (chr->hidden & CHRHIDDEN_REMOVE)
    {
        chrFreeResources(prop);
        return TICKOP_FREE;
    }

    if (chr->weapons_held[GUNRIGHT] != NULL)
    {
        if (chr->weapons_held[GUNRIGHT]->obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
        {
            objFreePermanently(chr->weapons_held[GUNRIGHT]->obj, 1);
        }
    }

    if (chr->weapons_held[GUNLEFT] != NULL)
    {
        if (chr->weapons_held[GUNLEFT]->obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
        {
            objFreePermanently(chr->weapons_held[GUNLEFT]->obj, 1);
        }
    }

    if (chr->chrflags & CHRFLAG_HIDDEN)
    {
        headSwitchVisible = 0;
    }
    else
    {
        if (((prop->type == PROP_TYPE_VIEWER) && (g_playerPointers[playerGetIndexForProp(prop)]->cameramode == 1)) || (chr->chrflags & CHRFLAG_CULL_USING_HITBOX))
        {
            headSwitchVisible = 1;

            if (((chr->actiontype == ACT_ANIM) && (chr->act_anim.alwaysAnimate == 0)) && (chr->act_anim.noTranslate != 0))
            {
                if (!coopRepeat)
                {
                    modelTickAnim(model, tickamount, 0);
                }
            }
            else
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
            }
        }
        else if ((chr->actiontype == ACT_PATROL) || (chr->actiontype == ACT_GOPOS))
        {
            if (((chr->actiontype == ACT_PATROL) && (chr->act_patrol.waydata.mode == WAYMODE_MAGIC)) || ((chr->actiontype == ACT_GOPOS) && (chr->act_gopos.waydata.mode == WAYMODE_MAGIC)))
            {
                headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);

                if (headSwitchVisible)
                {
                    modelGetRootPosition(model, &chr->prevpos);
                    modelCalcPosition(model);
                    objGetTileShading(prop, &chr->nextcol);
                    modelGetRootPosition(model, &prop->pos);
                    chrDetectRooms(chr);
                }
            }
            else
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
                headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);

                if (headSwitchVisible)
                {
                    if (chr->actiontype == ACT_PATROL)
                    {
                        chr->act_patrol.lastvisible60 = g_GlobalTimer;
                    }
                    else if (chr->actiontype == ACT_GOPOS)
                    {
                        chr->act_gopos.magicCheck60 = g_GlobalTimer;
                    }
                }
            }
        }
        else if ((chr->actiontype == ACT_ANIM) && (chr->act_anim.alwaysAnimate == 0))
        {
            headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);

            if (headSwitchVisible && (chr->act_anim.noTranslate == 0))
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
            }
            else
            {
                if (!coopRepeat)
                {
                    modelTickAnim(model, tickamount, 0);
                }
            }
        }
        else if (chr->actiontype == ACT_STAND)
        {
            headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);

            if (headSwitchVisible || (chr->chrflags & CHRFLAG_INIT))
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
            }
            else if (model->anim2 != NULL)
            {
                if (!coopRepeat)
                {
                    modelTickAnim(model, tickamount, 0);
                }
            }
        }
        else
        {
            if (chr->chrflags & CHRFLAG_IGNORE_ANIM_TRANSLATION)
            {
                if (!coopRepeat)
                {
                    modelTickAnim(model, tickamount, 0);
                }
            }
            else
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
            }

            headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);
        }
    }

    if (((chr->actiontype != ACT_STAND) || (model->anim2 != NULL)) || (prop->type == PROP_TYPE_VIEWER))
    {
        chr->hidden |= CHRHIDDEN_BACKGROUND_AI;
    }

    if (!coopRepeat)
    {
        chrUpdateAimProperties(chr);
    }

    if (chr->jointList != NULL)
    {
        jointListFree(chr->jointList);
        chr->jointList = NULL;
    }

    if (headSwitchVisible)
    {
        prop->flags |= PROPFLAG_ONSCREEN;
        chr->chrflags |= CHRFLAG_HAS_BEEN_ON_SCREEN;

#ifdef BUGFIX_R1
        if (cheatIsActive(CHEAT_DK_MODE))
        {
            modelSetLodScale(0.3125f);

            if (chr->chrflags & CHRFLAG_10000000)
            {
                chr->chrflags &= ~CHRFLAG_10000000;
                modelSetScale(chr->model, chr->model->scale / 0.8f);
            }
        }
#else
        if (cheatIsActive(CHEAT_DK_MODE))
        {
            modelSetLodScale(0.3125f);
        }
#endif

        g_ModelJointPositionedFunc = chrHandleJointPositioned;
        g_CurModelChr = chr;

        renderdata.basemtx = camGetWorldToScreenMtxf();
        renderdata.mtxlist = dynAllocate(model->obj->numMatrices * (sizeof(Mtxf)));

        if (g_CurModelChr->flinchcnt >= 0)
        {
            g_CurModelChr->flinchcnt += g_ClockTimer;

#ifdef VERSION_EU
            if (g_CurModelChr->flinchcnt >= 24)
#else
            if (g_CurModelChr->flinchcnt >= 30)
#endif
            {
                g_CurModelChr->flinchcnt = -1;
            }
        }

        modelCalcMatrices(&renderdata, model);

        g_ModelJointPositionedFunc = NULL;
        modelSetLodScale(1.0f);

        objUpdateShading(&chr->shadecol, &chr->nextcol);

        prop->zDepth = modelGetRootDepth(model);

        chr->jointList = jointListAddModel(NULL, model);

        chrRenderHeldWeapon(prop, GUNRIGHT, &chr->jointList);
        chrRenderHeldWeapon(prop, GUNLEFT, &chr->jointList);

        if (chr->handle_positiondata_hat != NULL)
        {
            ObjectRecord *hatobj;
            Model *hatmodel;

            hatobj = chr->handle_positiondata_hat->obj;
            hatmodel = hatobj->model;

            chr->handle_positiondata_hat->flags |= PROPFLAG_ONSCREEN;

            renderdata.basemtx = modelFindNodeMtx(model, hatmodel->attachedto_objinst, 0);
            renderdata.mtxlist = dynAllocate(hatmodel->obj->numMatrices * (sizeof(Mtxf)));

            modelCalcMatricesNoAnim(&renderdata, hatmodel);

            if ((chr->headnum >= HEAD_START) && (chr->headnum < BODY_Female_Sally))
            {
                coord3d pos;
                f32 xscale;
                f32 yscale;
                f32 zscale;
                Mtxf mtx;
                Mtxf tmp;
                HATTYPE hat;
                struct headHat *entry;
                s32 headindex;

                pos = (coord3d){0};

                hat = hatGetModel(chr->handle_positiondata_hat);

                headindex = chr->headnum - HEAD_START;
                // g_HeadHats has a row of 6 hat types per head
                entry = &g_HeadHats[(headindex * 6) + hat];

                pos.x = entry->xoffset * 21.3f;
                pos.y = entry->yoffset * 21.3f;
                pos.z = entry->zoffset * 21.3f;

                xscale = entry->xsize;
                yscale = entry->ysize;
                zscale = entry->zsize;

                mtx4LoadTranslation(&pos, &mtx);

                mtx4ScaleRow0(xscale, &mtx);
                mtx4ScaleRow1(yscale, &mtx);
                mtx4ScaleRow2(zscale, &mtx);

                mtx4MultAffine((Mtxf *)hatmodel->render_pos, &mtx, &tmp);
                mtx4Copy(&tmp, (Mtxf *)hatmodel->render_pos);

                if (hat == HATTYPE_PEAKED)
                {
                    headVisible = 0;
                }
            }

            if ((!(chr->hidden & CHRHIDDEN_DROP_HELD_ITEMS)) || (!(hatobj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)))
            {
                chr->jointList = jointListAddModel(chr->jointList, hatmodel);
            }
        }

        if (model->obj->Switches[4] != NULL)
        {
            union ModelRwData *rwdata;
            ModelFileHeader *headfile;

            rwdata = modelGetNodeRwData(model, model->obj->Switches[4]);
            headfile = rwdata->HeadPlaceholder.ModelFileHeader;

            if ((headfile != NULL) && (headfile->Switches[1] != NULL))
            {
                modelGetNodeRwData(model, headfile->Switches[1])->Switch.visible = headVisible;
            }
        }

        jointListCalcDepths(chr->jointList);
        chr->jointList = jointListSortByDepth(chr->jointList);
    }
    else
    {
        if (chr->weapons_held[GUNRIGHT] != NULL)
        {
            chr->weapons_held[GUNRIGHT]->flags &= ~PROPFLAG_ONSCREEN;
        }

        if (chr->weapons_held[GUNLEFT] != NULL)
        {
            chr->weapons_held[GUNLEFT]->flags &= ~PROPFLAG_ONSCREEN;
        }

        if (chr->handle_positiondata_hat != NULL)
        {
            chr->handle_positiondata_hat->flags &= ~PROPFLAG_ONSCREEN;
        }

        prop->flags &= ~PROPFLAG_ONSCREEN;

        chr->shadecol.r = chr->nextcol.r;
        chr->shadecol.g = chr->nextcol.g;
        chr->shadecol.b = chr->nextcol.b;
        chr->shadecol.a = chr->nextcol.a;
    }

    if (!(chr->chrflags & CHRFLAG_HIDDEN))
    {
        if (chr->hidden & CHRHIDDEN_DROP_HELD_ITEMS)
        {
            PropRecord *dropprop = propChild(prop);

            while (dropprop != NULL)
            {
                PropRecord *nextprop = dropprop->prev;

                objDrop(dropprop);

                dropprop = nextprop;
            }

            chr->hidden &= ~CHRHIDDEN_DROP_HELD_ITEMS;
        }

        chrTriggerFireWeapon(chr);
    }

    return TICKOP_NONE;
}


/**
 * Drops everything the guard carries other than its hat and the guns in its hands
 * (unless marked undroppable).
 */
void chrDropItems(ChrRecord *self)
{
    PropRecord *childprop;

    for (childprop = propChild(self->prop); childprop != NULL; childprop = childprop->prev)
    {
        if ((childprop != self->handle_positiondata_hat) &&
            (childprop != self->weapons_held[GUNLEFT]) &&
            (childprop != self->weapons_held[GUNRIGHT]))
        {
            if (!(childprop->weapon->flags & PROPFLAG_AIUNDROPPABLE))
            {
                objSetDropType(childprop, 1);
            }
        }
    }

    self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
}


/**
 * The guard in a slot with this chrnum, or NULL.
 */
ChrRecord *chrFindByLiteralId(s32 index)
{
    s32 i;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlotHot[i].chrnum == index)
        {
            return &g_ChrSlots[i];
        }
    }

    return NULL;
}


PropRecord *chrGetEquippedWeaponProp(ChrRecord *self, GUNHAND hand)
{
    return self->weapons_held[hand];
}


/**
 * The prop in that hand if it's held as a gun (not e.g. a key or a hat), otherwise NULL.
 */
PropRecord *chrGetEquippedWeaponPropWithCheck(ChrRecord *self, GUNHAND hand)
{
    PropRecord *gunprop = self->weapons_held[hand];

    if (gunprop != NULL)
    {
        if (!weaponHasFlag(gunprop->weapon->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN))
        {
            gunprop = NULL;
        }
    }

    return gunprop;
}


/**
 * A guard's collision shape for the player and other props: a diamond reaching chrwidth
 * from its position, from its ground up to ground + chrheight. Dying, dead, hidden and
 * moving guards have none (*edges = 0).
 *
 * @param polygon: out, the 4 corners
 * @param edges: out, 4, or 0 for no collision
 * @param y_out: out, top
 * @param ground: out, bottom
 */
void chrUpdateCollisionBounds(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *y_out, f32 *ground)
{
    ChrRecord *chr;

    chr = prop->chr;

    if (
        (chr->actiontype != ACT_DIE) &&
        (chr->actiontype != ACT_DEAD) &&
        ((chr->chrflags & (CHRFLAG_00010000 | CHRFLAG_HIDDEN)) == 0) &&
        ((chr->hidden & CHRHIDDEN_MOVING) == 0)
        )
    {
        *edges = 4;
        *polygon = chr->collision_bounds;

        chr->collision_bounds[0].x = prop->pos.x + chr->chrwidth;
        chr->collision_bounds[0].y = prop->pos.z;

        chr->collision_bounds[1].x = prop->pos.x;
        chr->collision_bounds[1].y = prop->pos.z + chr->chrwidth;

        chr->collision_bounds[2].x = prop->pos.x - chr->chrwidth;
        chr->collision_bounds[2].y = prop->pos.z;

        chr->collision_bounds[3].x = prop->pos.x;
        chr->collision_bounds[3].y = prop->pos.z - chr->chrwidth;

        *ground = chr->ground;
        *y_out = *ground + chr->chrheight;

        return;
    }

    *edges = 0;
}


/**
 * The size a guard collides with as it moves: its width, and the height band above the
 * ground it can't pass through (from 20 up to chrheight - 20, so it steps over low
 * obstacles).
 */
void chrGetChrWidthHeight(PropRecord *prop, f32 *width, f32 *top, f32 *bottom)
{
    ChrRecord *c = prop->chr;

    *width = c->chrwidth;
    *top = c->chrheight - 20.0f;
    *bottom = 20.0f;
}


f32 chrGetChrGround(PropRecord *prop)
{
    ChrRecord *c = prop->chr;
    return c->ground;
}


/**
 * For auto-aim: the point a quarter of the way from the second model matrix (render_pos[1])
 * to the root, and the guard's screen extents. FALSE if it's offscreen, behind the camera,
 * dying, dead or not auto-aimable.
 */
s32 chrGetOnscreenRenderBounds(PropRecord *prop, coord3d *pos, coord2d *xbounds, coord2d *ybounds)
{
    struct ChrRecord *chr;

    chr = prop->chr;

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        if ((chr->actiontype != ACT_DIE) && (chr->actiontype != ACT_DEAD) && !(chr->chrflags & CHRFLAG_NO_AUTOAIM))
        {
            struct Model *model;
            RenderPosView *model_render_pos_1;
            RenderPosView *model_render_pos_2;

            model = chr->model;
            model_render_pos_1 = &model->render_pos[0];
            model_render_pos_2 = &model->render_pos[1];

            pos->f[2] = model_render_pos_2->pos.m[3][2] + ((model_render_pos_1->pos.m[3][2] - model_render_pos_2->pos.m[3][2]) * 0.25f);

            if (pos->f[2] < 0.0f)
            {
                pos->f[0] = model_render_pos_2->pos.m[3][0] + ((model_render_pos_1->pos.m[3][0] - model_render_pos_2->pos.m[3][0]) * 0.25f);
                pos->f[1] = model_render_pos_2->pos.m[3][1] + ((model_render_pos_1->pos.m[3][1] - model_render_pos_2->pos.m[3][1]) * 0.25f);

                ybounds->y = 0.0f;
                ybounds->x = 0.0f;

                xbounds->x = 0.0f;
                xbounds->y = 0.0f;

                modelGetXYExtents(model, &xbounds->y, &xbounds->x, &ybounds->y, &ybounds->x);

                return 1;
            }
        }
    }

    return 0;
}


void chrsInit(void)
{
    g_ChrAnimRate = 1.0f;
    g_ChrDebugAnimOverride = 0;
    g_ChrDebugAnimIndex = 0;
    g_ChrDebugPaused = 0;
    g_ChrDebugStep = 0;
    g_ChrShowPatrols = FALSE;
    g_NextChrNum = 5000;
    g_ChrSlots = NULL;
    g_NumChrSlots = 0;
    objectiveregisters1 = 0;
    g_AiAccuracyModifier = 1.0f;
    g_AiDamageModifier = 1.0f;
    g_AiHealthModifier = 1.0f;
    g_AiReactionSpeed = 1.0f;
    g_SeenBondRecentlyGuardCount = 0;
}

/**
 * Makes a background chr (g_BgChrs) for each of the setup's AI lists numbered 0x1000 and
 * up: invisible dummies that only run their AI list.
 */
void bgChrsInit(void)
{
    s32 i;
    s32 count;

    g_BgChrs = NULL;
    g_NumBgChrs = 0;

    if (!g_CurrentSetup.ailists)
    {
        return;
    }

    count = 0;

    for (i = 0; g_CurrentSetup.ailists[i].ailist; i++)
    {
        if (g_CurrentSetup.ailists[i].ID >= 0x1000)
        {
            g_NumBgChrs++;
        }
    }

    if (g_NumBgChrs > 0)
    {
        g_BgChrs = mempAlloc(ALIGN16_a(g_NumBgChrs * sizeof(ChrRecord)), MEMPOOL_STAGE);

        for (i = 0; g_CurrentSetup.ailists[i].ailist; i++)
        {
            if (g_CurrentSetup.ailists[i].ID >= 0x1000)
            {
                g_BgChrs[count] = g_BgChrTemplate;

                g_BgChrs[count].chrnum = 0xFE;
                g_BgChrs[count].ailist = g_CurrentSetup.ailists[i].ailist;
                g_BgChrs[count].aioffset = 0;
                g_BgChrs[count].aireturnlist = -1;
                g_BgChrs[count].actiontype = ACT_NULL;
                count++;
            }
        }
    }
}

void chrsAllocateSlots(s32 count)
{
    s32 i;

    // 10 spare slots for guards spawned in play
    g_NumChrSlots = count + 10;

    g_ChrSlots = mempAlloc(ALIGN16_a(g_NumChrSlots * sizeof(ChrRecord)), MEMPOOL_STAGE);
    g_ChrSlotHot = mempAlloc(ALIGN16_a(g_NumChrSlots * sizeof(ChrSlotHot)), MEMPOOL_STAGE);

    for (i = 0; i < g_NumChrSlots; i++)
    {
        g_ChrSlots[i].model = NULL;
        g_ChrSlotHot[i].chrnum = CHRSLOT_EMPTY;
        g_ChrSlotHot[i].lastseetarget60 = 0;
    }
}


/* Copy a chr's chrnum and lastseetarget60 to g_ChrSlotHot, after either changes (or its model
   is set or cleared). Does nothing for the background chrs. */
void chrSlotSync(ChrRecord *chr)
{
    u32 i = chr - g_ChrSlots;

    if (g_ChrSlots != NULL && i < (u32)g_NumChrSlots)
    {
        g_ChrSlotHot[i].chrnum = chr->model != NULL ? chr->chrnum : CHRSLOT_EMPTY;
        g_ChrSlotHot[i].lastseetarget60 = chr->lastseetarget60;
    }
}


void chrsFreeAll(void)
{
    s32 i;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model != NULL)
        {
            chrFreeResources(g_ChrSlots[i].prop);
            propDelist(g_ChrSlots[i].prop);
            propDisable(g_ChrSlots[i].prop);
            propFree(g_ChrSlots[i].prop);
        }
    }
}
