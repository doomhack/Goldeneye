/**
 * The chr side of the AI script commands: looking up chrs and pads by the ids scripts
 * use (with the special ids CHR_SELF, CHR_PRESET, PAD_PRESET1...), the distances, angles
 * and room checks the scripts test, the chr and level flags they set, presets, spawning
 * new guards, and the actions they ask a guard to start.
 *
 * Most of what's here is called only from the interpreter (chrai.c). The "Try" functions
 * fail when the guard is dead or reacting to a shot.
 */
#include <ultra64.h>
#include <bondaicommands.h>
#include <bondconstants.h>
#include <bondgame.h>
#include <random.h>
#include "game/world/bg.h"
#include "game/world/bgfog.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/chr/chraction.h"
#include "game/math/trig.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/math/line_tri_intersect.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/nav/waypoint.h"
#include "game/prop/alarm.h"
#include "game/prop/carried.h"
#include "game/prop/propobj.h"
#include "game/prop/setup.h"

/* A spawned guard is tried at its position, then this far away in 8 directions */
#define SPAWN_STEP_DIST 60.0f
#define SPAWN_CLEAR_RADIUS 20.0f

/* A position is off screen if it's this far outside the view */
#define OFFSCREEN_MARGIN 200.0f

/* Guards don't throw grenades closer than this to Bond */
#define GRENADE_MIN_DIST 10.0f

/* What a spawned guard mustn't overlap */
#define SPAWN_CDTYPES (CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER)

/**
 * Turns a special chr id (CHR_SELF, CHR_PRESET, CHR_SEE_SHOT...) into a chr number.
 * Other ids are returned as they are.
 */
s32 chrResolveId(ChrRecord *self, s32 id)
{
    // The ids come from a script byte, so the special ones arrive as 0xF8 to 0xFD
    if (id == (u8)CHR_SEE_SHOT)
    {
        id = self->chrseeshot;
    }
    else if (id == (u8)CHR_SEE_DIE)
    {
        id = self->chrseedie;
    }
    else if (id == (u8)CHR_PRESET)
    {
        id = self->chrpreset1;
    }
    else if (id == (u8)CHR_SELF)
    {
        id = self->chrnum;
    }
    else if (id == (u8)CHR_CLONE)
    {
        id = self->chrnum + CHR_CLONED_NUM_START;
    }
    else if (id == (u8)CHR_BOND_CINEMA)
    {
        // Bond's third person model, in the intro and ending cutscenes
        if (g_CurrentPlayer->prop->chr)
        {
            id = g_CurrentPlayer->prop->chr->chrnum;
        }
    }

    return id;
}

/**
 * Turns PAD_PRESET1 into the chr's preset pad. Other pad numbers are returned as they are.
 */
s32 chrResolvePadId(ChrRecord *self, s32 padnum)
{
    if (padnum == PAD_PRESET1)
    {
        padnum = self->padpreset1;
    }

    return padnum;
}

/* A pad by the number a script gave */
static PadRecord *chrGetPad(ChrRecord *self, s32 padnum)
{
    return setupGetPad(chrResolvePadId(self, padnum));
}

/**
 * Finds a chr by id (special ids allowed), including the background chrs that run level
 * scripts. NULL if there's none.
 */
ChrRecord *chrFindById(ChrRecord *self, s32 id)
{
    s32 i;

    id = chrResolveId(self, id);

    ChrRecord *chr = chrFindByLiteralId(id);

    if (chr == NULL)
    {
        for (i = 0; i < g_NumBgChrs; i++)
        {
            if (id == g_BgChrs[i].chrnum)
            {
                return &g_BgChrs[i];
            }
        }
    }

    return chr;
}

/**
 * The position of what an attack aims at: with ATTACKTYPE_CHR the chr id (self if it's
 * gone), with ATTACKTYPE_PAD the pad, otherwise the current player. *stan is set to the
 * target's tile, except that for a chr it's self's tile, as in the original.
 */
coord3d *chrGetChrOrPresetLocation(ChrRecord *self, s32 attacktype, s32 id, StandTile **stan)
{
    if (attacktype & ATTACKTYPE_CHR)
    {
        ChrRecord *chr = chrFindById(self, id);

        if (chr == NULL || chr->prop == NULL)
        {
            chr = self;
        }

        *stan = self->prop->stan;

        return &chr->prop->pos;
    }

    if (attacktype & ATTACKTYPE_PAD)
    {
        PadRecord *pad = chrGetPad(self, id);

        *stan = pad->stan;

        return &pad->pos;
    }

    PropRecord *playerprop = currentPlayerGetProp();

    *stan = playerprop->stan;

    return &playerprop->pos;
}

/**
 * The angle from the way the chr faces to pos, anticlockwise, 0 to 2 pi.
 */
f32 chrGetAngleToPos(ChrRecord *self, coord3d *pos)
{
    f32 heading = modelGetRootRotY(self->model);
    PropRecord *prop = self->prop;
    f32 angle = mathAtan2(pos->x - prop->pos.x, pos->z - prop->pos.z);
    f32 result = angle - heading;

    if (angle < heading)
    {
        result += M_TAU_F;
    }

    return result;
}

f32 chrGetAngleToBond(ChrRecord *self)
{
    return chrGetAngleToPos(self, &currentPlayerGetProp()->pos);
}

/**
 * The angle from the way the current player faces to the chr, 0 to 2 pi.
 */
f32 chrGetAngleFromBond(ChrRecord *self)
{
    f32 heading = bondviewGetPlayerYawRadians();
    PropRecord *prop = self->prop;
    PropRecord *playerprop = currentPlayerGetProp();
    f32 angle = mathAtan2(prop->pos.x - playerprop->pos.x, prop->pos.z - playerprop->pos.z);
    f32 result = angle - heading;

    if (angle < heading)
    {
        result += M_TAU_F;
    }

    return result;
}

static f32 distance(coord3d *a, coord3d *b)
{
    f32 dx = a->x - b->x;
    f32 dy = a->y - b->y;
    f32 dz = a->z - b->z;

    return sqrtf(SQR(dx) + SQR(dy) + SQR(dz));
}

f32 chrGetDistanceToBond(ChrRecord *self)
{
    return distance(&currentPlayerGetProp()->pos, &self->prop->pos);
}

f32 chrGetDistanceToPad(ChrRecord *self, s32 padnum)
{
    PropRecord *prop = self->prop;

    return distance(&chrGetPad(self, padnum)->pos, &prop->pos);
}

/**
 * The distance to another chr, or 0 if it's gone.
 */
f32 chrGetDistanceToChr(ChrRecord *self, s32 chrnum)
{
    PropRecord *prop = self->prop;
    ChrRecord *chr = chrFindById(self, chrnum);

    if (chr && chr->model && chr->prop)
    {
        return distance(&chr->prop->pos, &prop->pos);
    }

    return 0.0f;
}

f32 chrGetDistanceFromBondToPad(ChrRecord *self, s32 padnum)
{
    PropRecord *playerprop = currentPlayerGetProp();

    return distance(&chrGetPad(self, padnum)->pos, &playerprop->pos);
}

/**
 * Whether the current player can see the chr and is aiming at it.
 */
bool chrIsTargetedByPlayer(ChrRecord *self)
{
    coord3d raystart;
    coord3d raydir;
    coord3d centre;

    if (!chrHasLineOfSightToPlayer(self))
    {
        return FALSE;
    }

    Model *model = self->model;
    f32 radius = modelGetRadius(model) * 0.8f;

    gunGetAimRay(&raystart, &raydir);
    modelGetRootPosition(model, &centre);
    mtx4TransformVecInPlace(camGetWorldToScreenMtxf(), &centre);

    return intersectRaySphere(&raystart, &raydir, &centre, radius) != 0;
}

/**
 * Whether the current player is just round a corner from the chr: the straight line
 * between them is blocked, but it could walk to them past one end of the wall in the way.
 */
bool chrIsTargetNearlyInSight(ChrRecord *self)
{
    PropRecord *playerprop = currentPlayerGetProp();
    PropRecord *prop = self->prop;
    StandTile *stan = prop->stan;
    coord3d edgestart;
    coord3d edgeend;

    stanResetHits();

    if (stanWalkLine(&stan, prop->pos.x, prop->pos.z, playerprop->pos.x, playerprop->pos.z))
    {
        return FALSE;
    }

    stanGetCollisionEdge(&edgestart, &edgeend);

    return chrCanWalkViaPoint(self, &prop->pos, prop->stan, &edgestart, &playerprop->pos, playerprop->stan, 0)
        || chrCanWalkViaPoint(self, &prop->pos, prop->stan, &edgeend, &playerprop->pos, playerprop->stan, 0);
}

/**
 * Whether the room a pad is in is being drawn.
 */
bool chrPadRoomIsVisible(ChrRecord *self, s32 padnum)
{
    StandTile *stan = chrGetPad(self, padnum)->stan;

    if (stan)
    {
        return bgRoomIsVisible(stan->room);
    }

    return FALSE;
}

/**
 * Whether a chr is in the same room as a pad.
 */
bool chrIfInPadRoom(ChrRecord *self, s32 chrnum, s32 padnum)
{
    ChrRecord *chr = chrFindById(self, chrnum);
    PadRecord *pad = chrGetPad(self, padnum);

    return pad->stan && chr && chr->prop && pad->stan->room == chr->prop->stan->room;
}

/**
 * Whether the current player is in the same room as a pad.
 */
bool chrPlayerIsInPadRoom(ChrRecord *self, s32 padnum)
{
    PropRecord *playerprop = currentPlayerGetProp();
    PadRecord *pad = chrGetPad(self, padnum);

    return pad->stan && pad->stan->room == playerprop->stan->room;
}

/* flags2 is a byte of flags for the level's scripts to use as they like */

void chrSetFlags2ById(ChrRecord *self, s32 chrnum, u8 flags)
{
    ChrRecord *chr = chrFindById(self, chrnum);

    if (chr != NULL)
    {
        chr->flags2 |= flags;
    }
}

void chrUnsetFlags2ById(ChrRecord *self, s32 chrnum, u8 flags)
{
    ChrRecord *chr = chrFindById(self, chrnum);

    if (chr != NULL)
    {
        chr->flags2 &= ~flags;
    }
}

bool chrHasFlags2ById(ChrRecord *self, s32 chrnum, u8 flags)
{
    ChrRecord *chr = chrFindById(self, chrnum);

    if (chr != NULL)
    {
        return (chr->flags2 & flags) != 0;
    }

    return FALSE;
}

/* The level's flags, which objectives can test too */

void chrSetStageFlags(u32 flags)
{
    objectiveregisters1 |= flags;
}

void chrUnsetStageFlags(u32 flags)
{
    objectiveregisters1 &= ~flags;
}

bool chrHasStageFlag(u32 flags)
{
    return (objectiveregisters1 & flags) != 0;
}

void chrRestartTimer(ChrRecord *self)
{
    self->timer60 = 0;
    self->hidden |= CHRHIDDEN_TIMER_ACTIVE;
}

/**
 * The chr's timer, in seconds.
 */
f32 chrGetTimer(ChrRecord *self)
{
    return self->timer60 / CHRLV_FRAMERATE_F;
}

void chrSetChrPreset(ChrRecord *self, s32 chrnum)
{
    self->chrpreset1 = chrResolveId(self, chrnum);
}

/**
 * Sets another chr's preset (ids resolved as self sees them).
 */
void chrSetChrPresetOf(ChrRecord *self, s32 chrnum, s32 presetchrnum)
{
    ChrRecord *chr = chrFindById(self, chrnum);

    if (chr)
    {
        chr->chrpreset1 = chrResolveId(self, presetchrnum);
    }
}

/**
 * Sets the chr's preset to a living chr within distance (in a box), in the same room or
 * one next to it. Players' bodies don't count.
 */
bool chrSetChrPresetToNearbyChr(ChrRecord *self, f32 distance)
{
    s32 numchrs = chrsGetNumSlots();
    PropRecord *prop = self->prop;
    s32 room = prop->stan->room;
    coord3d min;
    coord3d max;
    s32 i;

    min.x = prop->pos.x - distance;
    max.x = prop->pos.x + distance;
    min.y = prop->pos.y - distance;
    max.y = prop->pos.y + distance;
    min.z = prop->pos.z - distance;
    max.z = prop->pos.z + distance;

    for (i = 0; i < numchrs; i++)
    {
        ChrRecord *chr = &g_ChrSlots[i];

        // In co-op the players' bodies are chrs too: an ally like Natalya mustn't pick them
        if (chr == self || chr->model == NULL || chrIsDead(chr) || chr->prop == NULL || chr->prop->type == PROP_TYPE_VIEWER)
        {
            continue;
        }

        coord3d *pos = &chr->prop->pos;

        if (pos->x >= min.x && pos->x <= max.x
            && pos->y >= min.y && pos->y <= max.y
            && pos->z >= min.z && pos->z <= max.z
            && (chr->prop->stan->room == room || bgRoomsSharePortal(room, chr->prop->stan->room)))
        {
            self->chrpreset1 = chr->chrnum;

            return TRUE;
        }
    }

    return FALSE;
}

void chrSetPadPreset(ChrRecord *self, s32 padnum)
{
    self->padpreset1 = chrResolvePadId(self, padnum);
}

/**
 * Sets another chr's preset pad (PAD_PRESET1 meaning self's).
 */
void chrSetPadPresetByChrnum(ChrRecord *self, s32 chrnum, s32 padnum)
{
    ChrRecord *chr = chrFindById(self, chrnum);

    if (chr)
    {
        chr->padpreset1 = chrResolvePadId(self, padnum);
    }
}

/* Whether a waypoint's pad is within 90 degrees either side of angle, seen from pos */
static bool waypointIsWithin90DegreesOfPosAngle(waypoint *wp, coord3d *pos, f32 angle)
{
    PadRecord *pad = &g_CurrentSetup.pads[wp->padID];
    f32 padangle = mathAtan2(pad->pos.x - pos->x, pad->pos.z - pos->z);
    f32 diff = angle - padangle;

    if (angle < padangle)
    {
        diff += M_TAU_F;
    }

    return diff < DegToRad(90) || diff > DegToRad(270);
}

/**
 * Finds a waypoint in a quadrant (QUADRANT_FRONT, BACK, SIDE1, SIDE2) around pos, facing
 * angle: the nearest waypoint to pos if it's in the quadrant, or else one of its
 * neighbours. Returns its pad, or -1 if none of them is.
 */
static s32 waypointFindInQuadrant(coord3d *pos, StandTile *stan, f32 angle, u8 quadrant)
{
    waypoint *wp = waypointFindNearPos(pos, stan);
    s32 i;

    if (wp == NULL)
    {
        return -1;
    }

    switch (quadrant)
    {
        case QUADRANT_BACK:
            angle += DegToRad(180);
            break;
        case QUADRANT_SIDE1:
            angle += DegToRad(90);
            break;
        case QUADRANT_SIDE2:
            angle += DegToRad(270);
            break;
        case QUADRANT_FRONT:
            break;
    }

    if (angle >= M_TAU_F)
    {
        angle -= M_TAU_F;
    }

    if (waypointIsWithin90DegreesOfPosAngle(wp, pos, angle))
    {
        return wp->padID;
    }

    for (i = 0; wp->neighbours[i] >= 0; i++)
    {
        waypoint *neighbour = &g_CurrentSetup.pathwaypoints[wp->neighbours[i]];

        if (waypointIsWithin90DegreesOfPosAngle(neighbour, pos, angle))
        {
            return neighbour->padID;
        }
    }

    return -1;
}

/**
 * Sets the chr's preset pad to a waypoint: in the given quadrant around the chr (facing
 * the way it faces), or with QUADRANT_2NDWPTOTARGET the second waypoint on the route to
 * the current player, or with QUADRANT_20 the next step towards them. FALSE if there's
 * none.
 */
bool chrSetPadPresetToWaypointInTargetQuadrant(ChrRecord *self, u8 quadrant)
{
    if (quadrant == QUADRANT_2NDWPTOTARGET || quadrant == QUADRANT_20)
    {
        PropRecord *prop = self->prop;
        PropRecord *playerprop = currentPlayerGetProp();
        waypoint *from = waypointFindNearPos(&prop->pos, prop->stan);
        waypoint *to = waypointFindNearPos(&playerprop->pos, playerprop->stan);
        waypoint *route[PATH_FINDING_WP_LIMIT];

        if (from == NULL || to == NULL)
        {
            return FALSE;
        }

        if (quadrant == QUADRANT_2NDWPTOTARGET)
        {
            if (waypointFindRoute(from, to, route, PATH_FINDING_WP_LIMIT) >= PATH_FINDING_WP_LIMIT)
            {
                self->padpreset1 = route[1]->padID;

                return TRUE;
            }

            return FALSE;
        }

        waypoint *next = waypointFindNextStepToward(from, to);

        if (next != NULL)
        {
            self->padpreset1 = next->padID;

            return TRUE;
        }

        return FALSE;
    }

    s32 padnum = waypointFindInQuadrant(&self->prop->pos, self->prop->stan, modelGetRootRotY(self->model), quadrant);

    if (padnum >= 0)
    {
        self->padpreset1 = padnum;

        return TRUE;
    }

    return FALSE;
}

/**
 * Sets the chr's preset pad to a waypoint in the given quadrant around the current player
 * (facing the way they face). QUADRANT_2NDWPTOTARGET and QUADRANT_20 work as in
 * chrSetPadPresetToWaypointInTargetQuadrant. FALSE if there's none.
 */
bool chrSetPadPresetToWaypointInQuadrant(ChrRecord *self, u8 quadrant)
{
    if (quadrant == QUADRANT_2NDWPTOTARGET || quadrant == QUADRANT_20)
    {
        return chrSetPadPresetToWaypointInTargetQuadrant(self, quadrant);
    }

    f32 yaw = bondviewGetPlayerYawRadians();
    PropRecord *playerprop = currentPlayerGetProp();
    s32 padnum = waypointFindInQuadrant(&playerprop->pos, playerprop->stan, yaw, quadrant);

    if (padnum >= 0)
    {
        self->padpreset1 = padnum;

        return TRUE;
    }

    return FALSE;
}

/* Whether pos is out of every player's view (allowing a margin) or hidden by fog */
static bool chrIsPosOffScreen(coord3d *pos, StandTile *stan)
{
    bbox2d box;

    if (!bgRoomIsVisible(stan->room) || !fogPositionIsVisibleThroughFog(pos, 0.0f))
    {
        return TRUE;
    }

    if (bgGet2dBboxByRoomId(stan->room, &box))
    {
        return !camIsPosInScreenBox(pos, OFFSCREEN_MARGIN, &box);
    }

    return !camIsPosInScreen(pos, OFFSCREEN_MARGIN);
}

/**
 * Finds room for a new guard at pos: there if nothing's in the way (and, unless
 * allowonscreen, it can't be seen), or else up to 8 steps round, starting in the
 * direction facing. Updates pos and stan and returns TRUE if it finds a place.
 */
bool chrAdjustPosForSpawn(coord3d *pos, StandTile **stan, f32 facing, bool allowonscreen)
{
    StandTile *tile = *stan;
    coord3d testpos;
    s32 i;

    if (stanTestVolume(&tile, pos->x, pos->z, SPAWN_CLEAR_RADIUS, SPAWN_CDTYPES, 0.0f, 1.0f) < 0
        && (allowonscreen || chrIsPosOffScreen(pos, *stan)))
    {
        return TRUE;
    }

    for (i = 0; i < 8; i++)
    {
        testpos.x = pos->x + (sinf(facing) * SPAWN_STEP_DIST);
        testpos.y = pos->y;
        testpos.z = pos->z + (cosf(facing) * SPAWN_STEP_DIST);

        tile = *stan;

        if (stanTestLineUnobstructed(&tile, pos->x, pos->z, testpos.x, testpos.z, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f, 0.0f, 1.0f)
            && stanTestVolume(&tile, testpos.x, testpos.z, SPAWN_CLEAR_RADIUS, SPAWN_CDTYPES, 0.0f, 1.0f) < 0
            && (allowonscreen || chrIsPosOffScreen(&testpos, tile)))
        {
            *stan = tile;
            pos->x = testpos.x;
            pos->z = testpos.z;

            return TRUE;
        }

        facing += M_TAU_F / 8;

        if (facing >= M_TAU_F)
        {
            facing -= M_TAU_F;
        }
    }

    return FALSE;
}

/**
 * Spawns a guard with the given body and head (random if headnum < 0) at or near pos,
 * running ailist. Fails if fewer than 3 chr slots are free or there's no room.
 * SPAWN_IGNORE_PAD_SIGHT_CHECK lets it appear in view.
 */
PropRecord *chrSpawnAtCoord(s32 bodynum, s32 headnum, coord3d *pos, StandTile *stan, f32 angle, AIRecord *ailist, s32 spawnflags)
{
    if (chrGetNumFree() < 3)
    {
        return NULL;
    }

    if (headnum < 0)
    {
        headnum = bodyChooseHead(bodynum);
    }

    coord3d newpos = *pos;
    StandTile *newstan = stan;

    if (!chrAdjustPosForSpawn(&newpos, &newstan, angle, (spawnflags & SPAWN_IGNORE_PAD_SIGHT_CHECK) != 0))
    {
        return NULL;
    }

    Model *model = bodyCreateModelWithFlags(bodynum, headnum, spawnflags);

    if (model == NULL)
    {
        return NULL;
    }

    PropRecord *prop = chrAllocate(model, &newpos, angle, newstan, ailist);

    if (prop == NULL)
    {
        return NULL;
    }

    propActivateThisFrame(prop);
    propEnable(prop);
    prop->chr->headnum = headnum;
    prop->chr->bodynum = bodynum;

    return prop;
}

/**
 * Spawns a guard at a pad, facing the way the pad looks.
 */
PropRecord *chrSpawnAtPad(ChrRecord *self, s32 bodynum, s32 headnum, s32 padnum, AIRecord *ailist, s32 spawnflags)
{
    PadRecord *pad = chrGetPad(self, padnum);

    return chrSpawnAtCoord(bodynum, headnum, &pad->pos, pad->stan, mathAtan2(pad->look.x, pad->look.z), ailist, spawnflags);
}

/**
 * Spawns a guard where another chr is, facing the same way, as long as that chr has never
 * been on screen.
 */
PropRecord *chrSpawnAtChr(ChrRecord *self, s32 bodynum, s32 headnum, s32 chrnum, AIRecord *ailist, s32 spawnflags)
{
    ChrRecord *chr = chrFindById(self, chrnum);

    if (chr->chrflags & CHRFLAG_HAS_BEEN_ON_SCREEN)
    {
        return NULL;
    }

    f32 angle = modelGetRootRotY(chr->model);
    PropRecord *prop = chr->prop;

    return chrSpawnAtCoord(bodynum, headnum, &prop->pos, prop->stan, angle, ailist, spawnflags);
}

/**
 * Stops whatever the chr's doing and stands.
 */
bool chrTryStopAnim(ChrRecord *self)
{
    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    chrStandCheckWall(self);

    return TRUE;
}

bool chrTryKneel(ChrRecord *self)
{
    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    chrKneelChooseAnimation(self);

    return TRUE;
}

bool chrTryStartAnim(ChrRecord *self, s32 animnum, s32 startframe, s32 endframe, u8 flags, s32 mergetime)
{
    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    chrStartAnim(self, animnum, startframe, endframe, flags, mergetime);

    return TRUE;
}

bool chrTrySurprisedOneHand(ChrRecord *self)
{
    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    chrActorShuffleFeet(self);

    return TRUE;
}

bool chrTrySurprisedSurrender(ChrRecord *self)
{
    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    chrStartSurprised(self);

    return TRUE;
}

bool chrTrySurprisedLookAround(ChrRecord *self)
{
    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    chrActorLookFlustered(self);

    return TRUE;
}

/**
 * Throws down its gun and puts its hands up.
 */
bool chrTrySurrender(ChrRecord *self)
{
    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    chrActorThrowWeaponSurrender(self);

    return TRUE;
}

/**
 * Goes to the current player, unless 10 or more guards have seen Bond recently.
 */
bool chrGoToBond(ChrRecord *self, SPEED speed)
{
    if (!chrIsNotDeadOrShot(self) || g_SeenBondRecentlyGuardCount >= 10)
    {
        return FALSE;
    }

    PropRecord *playerprop = currentPlayerGetProp();

    return chrGoToPos(self, &playerprop->pos, playerprop->stan, speed) != 0;
}

/**
 * Goes to another chr, unless 10 or more guards have seen Bond recently.
 */
bool chrGoToChr(ChrRecord *self, s32 chrnum, SPEED speed)
{
    if (!chrIsNotDeadOrShot(self) || g_SeenBondRecentlyGuardCount >= 10)
    {
        return FALSE;
    }

    ChrRecord *chr = chrFindById(self, chrnum);

    if (chr == NULL || chr->model == NULL || chr->prop == NULL)
    {
        return FALSE;
    }

    return chrGoToPos(self, &chr->prop->pos, chr->prop->stan, speed) != 0;
}

/**
 * Runs to set off the alarm at a pad, if there's an alarm there that isn't broken.
 */
bool chrTryStartAlarm(ChrRecord *self, s32 padnum)
{
    padnum = chrResolvePadId(self, padnum);

    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    ObjectRecord *alarm = objFindAtPad(padnum);

    if (alarm == NULL || !objIsHealthy(alarm))
    {
        return FALSE;
    }

    chrStartAlarmChooseAnimation(self);

    return TRUE;
}

/**
 * Maybe throws a grenade (chance grenadeprob in 255), if Bond isn't too close. Uses a
 * grenade the chr holds, or gives it one if it has a hand free.
 */
bool chrTryThrowGrenade(ChrRecord *self)
{
    if (randomGetNext() % 255U >= self->grenadeprob)
    {
        return FALSE;
    }

    if (chrGetDistanceToBond(self) < GRENADE_MIN_DIST)
    {
        return FALSE;
    }

    if (!chrIsNotDeadOrShot(self))
    {
        return FALSE;
    }

    PropRecord *left = chrGetEquippedWeaponProp(self, GUNLEFT);
    PropRecord *right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    if (right && right->weapon->weaponnum == ITEM_GRENADE)
    {
        chrThrowGrenade(self, GUNRIGHT, 0);

        return TRUE;
    }

    if (left && left->weapon->weaponnum == ITEM_GRENADE)
    {
        chrThrowGrenade(self, GUNLEFT, 0);

        return TRUE;
    }

    if (left && right)
    {
        return FALSE;
    }

    // A grenade for the free hand
    PropRecord *grenade = chrGiveWeapon(self, PROP_CHRGRENADE, ITEM_GRENADE, right ? PROPFLAG_WEAPON_LEFTHANDED : 0);

    if (grenade == NULL)
    {
        return FALSE;
    }

    grenade->weapon->runtime_bitflags |= RUNTIMEBITFLAG_RESPAWNING;
    chrThrowGrenade(self, right ? GUNLEFT : GUNRIGHT, 1);

    return TRUE;
}

/**
 * Makes an item, attaches it to the chr and marks it to be dropped.
 */
bool chrDropItem(ChrRecord *self, s32 modelnum, u8 weaponnum)
{
    WeaponObjRecord *weapon = weaponCreateFromModel(modelnum, weaponnum);

    if (weapon == NULL || weapon->prop == NULL)
    {
        return FALSE;
    }

    modelSetScale(weapon->model, weapon->model->scale);
    propReparent(weapon->prop, self->prop);
    weapon->timer = CHRLV_DEFAULT_TIMER;
    objSetDropType(weapon->prop, 1);
    self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;

    return TRUE;
}
