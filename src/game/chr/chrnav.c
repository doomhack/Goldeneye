/**
 * How guards move: walking to a position along the waypoint graph (ACT_GOPOS), walking a
 * patrol path (ACT_PATROL) and running straight to a nearby position (ACT_RUNPOS).
 * Each tick chrNavTick heads for the next waypoint and steers round anything in the way
 * (chrNavTryObstacle). When nobody can see the route, chrGoPosTickMagic slides the guard
 * along it without animating. See docs/subsystems/guards.md.
 */
#include <ultra64.h>
#include <bondaicommands.h>
#include <bondgame.h>
#include <bondconstants.h>
#include "game/chr/chraction.h"
#include <math.h>
#include "include/math.h"
#include <random.h>
#include "game/world/bg.h"
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/model/anim.h"
#include "game/prop/setup.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/modelfile.h"
#include "game/player/player.h"
#include "game/prop/propobj.h"
#include "game/prop/door.h"
#include "game/nav/stan.h"
#include "game/nav/waypoint.h"
#include "game/math/line_tri_intersect.h"
#include "game/math/physics.h"

static waypoint *waypointFindOnTile(StandTile *tile);
static bool waypointIsOnTile(StandTile *tile);
static f32 waypointDistSqXz(coord3d *pos, waypoint *wp);
static s32 chrPathIsOffscreen(ChrRecord *self, coord3d *pos, StandTile *tile);
static s32 chrPathToPadIsOffscreen(ChrRecord *self, PadRecord *pad);
static void chrSetGoposSegDistTotal(ChrRecord *self, struct waydata *way, coord3d *pos);
static void chrGoPosGetCurWaypointInfo(ChrRecord *self, coord3d *target_point, StandTile **target_stan);
static s32 chrGoPosCalculateBaseTtl(ChrRecord *self);
static void chrGoPosClearRestartTtl(ChrRecord *self);
static void chrGoPosConsiderRestart(ChrRecord *self);
static void chrGoPosResetMagic(ChrRecord *self);
static void chrActGoposIncCurIndex(ChrRecord *self);
static s32 chrPatrolCalculateStep(ChrRecord *self, bool32 *forward, s32 numsteps);
static PadRecord *chrGetPatrolStepPad(ChrRecord *self, s32 numsteps);
static PadRecord *chrGetNextPatrolStepPad(ChrRecord *self);
static void chrSetNextActPatrolStepPadPos(ChrRecord *self);
static void chrAdvancePatrolStep(ChrRecord *self);
static bool chrIsPosClearOfObjectBounds(coord3d *pos, StandTile *stan);
static void chrGoPosTickMagic(ChrRecord *self, struct waydata *way, f32 speed, coord3d *pos, StandTile *tile);
static void chrGoPosChooseAnimationEx(ChrRecord *self, s32 speed, s32 isMale);
static void chrGoPosChooseAnimation(ChrRecord *self);
static void chrPatrolChooseAnimation(ChrRecord *self);
static s32 chrCanWalkLine(ChrRecord *self, coord3d *point, StandTile *tile, coord3d *dest, StandTile * desttile, s32 cdtypes);
static s32 chrCanWalkLineWithin(ChrRecord *self, coord3d *point, StandTile *tile, coord3d *dest, f32 halfwidth, s32 cdtypes);
static void orderPointsBySide(coord3d *a, coord3d *b, coord3d *dir);
static s32 chrNavCanSeeNextPos(ChrRecord *self, coord3d *from, StandTile *fromtile, coord3d *to, coord3d *edgeStart, coord3d *edgeEnd, f32 overshoot, f32 width, s32 cdtypes);
static s32 chrNavCheckForObstacle(ChrRecord *self, coord3d *from, StandTile *fromtile, coord3d *to, coord3d *edgeStart, coord3d *edgeEnd, f32 overshoot, f32 width, s32 cdtypes);
static s32 chrNavTryObstacle(ChrRecord *self, coord3d *corner, s32 left, coord3d *othercorner, f32 clearance, s32 checktarget, coord3d *target, struct waydata *way, f32 overshoot, s32 cdtypes, s32 set_copy);
static void chrNavTick(ChrRecord *self, coord3d *target, struct waydata *way);

/**
 * Run straight to a nearby position (ACT_RUNPOS), with no route planning.
 * PD: chrJumpOutChooseAnimation (has a few things in common)
 */
void chrRunToPos(ChrRecord *self, coord3d *pos)
{
    f32 dx;
    f32 dz;
    f32 sq;
    PropRecord *left;
    PropRecord *right;
    s32 straight;
    s32 flip;

    dx = self->prop->pos.f[0] - pos->f[0];
    dz = self->prop->pos.f[2] - pos->f[2];
    sq = sqrtf((dx * dx) + (dz * dz));

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    straight = 1;

    if (((left != NULL) && (right != NULL)) || ((left == NULL) && (right == NULL)))
    {
        straight = 0;
        flip = randomGetNext() & 1;
    }
    else
    {
        if ((weaponIsOneHanded(left)) || (weaponIsOneHanded(right)))
        {
            straight = 0;
            flip = left != 0;
        }
        else
        {
            flip = left != 0;
        }
    }

    chrStopFiring(self);

    self->actiontype = ACT_RUNPOS;
    self->act_runpos.pos.f[0] = pos->f[0];
    self->act_runpos.pos.f[1] = pos->f[1];
    self->act_runpos.pos.f[2] = pos->f[2];
    self->sleep = 0;
    self->act_runpos.turnspeed = 0;
    self->act_runpos.neardist = 30.0f;

    if (straight)
    {
#ifdef VERSION_EU
        self->act_runpos.eta60 = (s32) (((sq / (g_ChrSpeedRun * 0.5f)) * 50.0f) / 60.0f);
#else
        self->act_runpos.eta60 = (s32) (sq / (g_ChrSpeedRun * 0.5f));
#endif
        modelSetAnimation(self->model, ANIM(running), flip, 0, 0.5f, 16.0f);
    }
    else
    {
#ifdef VERSION_EU
        self->act_runpos.eta60 = (s32) (((sq / (g_ChrSpeedRunOneHanded * 0.5f)) * 50.0f) / 60.0f);
#else
        self->act_runpos.eta60 = (s32) (sq / (g_ChrSpeedRunOneHanded * 0.5f));
#endif
        modelSetAnimation(self->model, ANIM(running_one_handed_weapon), flip, 0, 0.5f, 16.0f);
    }
}


/**
 * Given a stan tile, find the first waypoint on that tile.
 */
static waypoint *waypointFindOnTile(StandTile *tile)
{
    waypoint *head;
    waypoint *wp;

    head = g_CurrentSetup.pathwaypoints;

    if (head != NULL) {
        wp = head;
        while (wp->padID >= 0) {
            PadRecord* pad = &g_CurrentSetup.pads[wp->padID];
            if (tile == pad->stan) {
                return wp;
            }
            wp++;
        }
    }

    return NULL;
}


static bool waypointIsOnTile(StandTile *tile)
{
    return waypointFindOnTile(tile) != NULL;
}


/**
 * 100% match, unsure of argument types.
 * Addresss 0x7F027C84.
*/
static f32 waypointDistSqXz(coord3d *pos, waypoint *wp)
{
    f32 dz;
    f32 dx;
    PadRecord *pad;

    pad = &g_CurrentSetup.pads[wp->padID];
    dx = pad->pos.f[0] - pos->f[0];
    dz = pad->pos.f[2] - pos->f[2];
    return (dx * dx) + (dz * dz);
}


waypoint *waypointFindNearPos(coord3d *pos, StandTile *starttile)
{
    StandTile *tile = NULL;
    f32 bestdist;
    waypoint *ret = NULL;
    waypoint *wayp = NULL;
    s32 *n = NULL;

    tile = stanSearchOutward(starttile, waypointIsOnTile);
    if (tile != NULL)
    {
        ret = waypointFindOnTile(tile);

        if (ret != NULL)
        {
            bestdist = waypointDistSqXz(pos, ret);

            for (n = ret->neighbours; *n >= 0; n++)
            {
                wayp = &g_CurrentSetup.pathwaypoints[*n];

                if (waypointDistSqXz(pos, wayp) < bestdist)
                {
                    ret = wayp;
                }
            }
        }
    }

    return ret;
}


static s32 chrPathIsOffscreen(ChrRecord *self, coord3d *pos, StandTile *tile)
{

#define MAX_PATH_ROOMS 0x14

    s32 rooms[MAX_PATH_ROOMS];
    PropRecord *prop;
    s32 tile_something;
    s32 i;

    prop = self->prop;
    tile_something = stanGetRoomsBetween(prop->stan, prop->pos.x, prop->pos.f[2], &tile, pos->f[0], pos->f[2], &rooms[0], MAX_PATH_ROOMS);

    if (tile_something > 0 && tile_something < MAX_PATH_ROOMS)
    {
        for (i=0; i<tile_something; i++)
        {
            if (bgRoomIsVisible(rooms[i]) != 0)
            {
                return 0;
            }
        }
    }
    else
    {
        return 0;
    }

    return 1;
}


static s32 chrPathToPadIsOffscreen(ChrRecord *self, PadRecord *pad)
{
    return chrPathIsOffscreen(self, &pad->pos, pad->stan);
}


/**
 * PD: chrGoPosInitMagic
*/
static void chrSetGoposSegDistTotal(ChrRecord *self, struct waydata *way, coord3d *pos)
{
    PropRecord *prop;
    f32 dx;
    f32 dz;
    f32 angle;

    prop = self->prop;
    dx = pos->f[0] - prop->pos.f[0];
    dz = pos->f[2] - prop->pos.f[2];

    angle = mathAtan2(dx, dz);

    way->mode = 6;
    way->segdistdone = 0.0f;
    way->segdisttotal = sqrtf((dx * dx) + (dz * dz));

    modelSetRootRotY(self->model, angle);
}


/**
 * @param self:
 * @param target_point: out paramter, will contain target position
 * @param target_stan: out parameter, will contain pointer to target stan
 *
 * PD: chrGoPosGetCurWaypointInfoWithFlags (somewhat similar)
*/
static void chrGoPosGetCurWaypointInfo(ChrRecord *self, coord3d *target_point, StandTile **target_stan)
{
    waypoint *waypoint;
    PadRecord *pad;

    waypoint = self->act_gopos.waypoints[self->act_gopos.curindex];

    if (waypoint != 0)
    {
        pad = &g_CurrentSetup.pads[waypoint->padID];

        target_point->f[0] = pad->pos.f[0];
        target_point->f[1] = pad->pos.f[1];
        target_point->f[2] = pad->pos.f[2];

        *target_stan = pad->stan;
    }
    else
    {
        target_point->f[0] = self->act_gopos.targetpos.f[0];
        target_point->f[1] = self->act_gopos.targetpos.f[1];
        target_point->f[2] = self->act_gopos.targetpos.f[2];

        *target_stan = self->act_gopos.target;
    }
}


/**
 * PD: func0f0370a8 (but GE has much more cases)
*/
f32 chrGetRunAnimSpeed(ChrRecord *self)
{
    f32 scale_factor = g_ChrSpeedWalk;

    if (modelGetAnim(self->model) == ANIM(sprinting))
    {
        scale_factor = g_ChrSpeedSprint;
    }
    else if (modelGetAnim(self->model) == ANIM(running))
    {
        scale_factor = g_ChrSpeedRun;
    }
    else if (modelGetAnim(self->model) == ANIM(sprinting_one_handed_weapon))
    {
        scale_factor = g_ChrSpeedSprintOneHanded;
    }
    else if (modelGetAnim(self->model) == ANIM(running_one_handed_weapon))
    {
        scale_factor = g_ChrSpeedRunOneHanded;
    }
    else if (modelGetAnim(self->model) == ANIM(walking_unarmed))
    {
        scale_factor = g_ChrSpeedWalkUnarmed;
    }
    // typo/mistake, `ANIM_DATA_sprinting_one_handed_weapon` is duplicate of above.
    // compiler swaps addition order when reading this from the stack, unlike addresses only seen once (seen once means not saved to stack).
    else if (modelGetAnim(self->model) == ANIM(sprinting_one_handed_weapon))
    {
        scale_factor = g_ChrSpeedSprintOneHanded2;
    }
    else if (modelGetAnim(self->model) == ANIM(running_female))
    {
        scale_factor = g_ChrSpeedRunFemale;
    }
    else if (modelGetAnim(self->model) == ANIM(walking_female))
    {
        scale_factor = g_ChrSpeedWalkFemale;
    }

    return self->model->scale * scale_factor * 9.999999f;
}


static s32 chrGoPosCalculateBaseTtl(ChrRecord *self)
{
    f32 dx;
    f32 dz;
    PropRecord *prop;
    coord3d waypos;
    StandTile *waytile; // 28
    f32 animspeed; // 24

    animspeed = modelGetAbsAnimSpeed(self->model);
    chrGoPosGetCurWaypointInfo(self, &waypos, &waytile);
    prop = self->prop;
    dx = waypos.f[0] - prop->pos.f[0];
    dz = waypos.f[2] - prop->pos.f[2];

    if (dx < 0.0f)
    {
        dx = -dx;
    }

    if (dz < 0.0f)
    {
        dz = -dz;
    }

    return (s32) ((dx + dz) / (chrGetRunAnimSpeed(self) * animspeed));
}


static void chrGoPosClearRestartTtl(ChrRecord *self)
{
    self->act_gopos.restartTtl = 0;
}


static void chrGoPosConsiderRestart(ChrRecord *self)
{
    s32 newttl;
    s32 ttl;
    s32 elapsed;

    if (self->act_gopos.waydata.mode != WAYMODE_MAGIC)
    {
        ttl = self->act_gopos.restartTtl;

        if (ttl == 0)
        {
#ifndef REFRESH_PAL
            newttl = (chrGoPosCalculateBaseTtl(self) * 2) + 300;
#else
            newttl = ((chrGoPosCalculateBaseTtl(self) * 100) + 15000) / 60;
#endif

            if (newttl >= 0x10000)
            {
                newttl = (u16)-1;
            }

            self->act_gopos.restartTtl = (s16)newttl;

            return;
        }

        elapsed = (u16)g_ClockTimer;

        if (elapsed >= ttl)
        {
            chrGoToPos(self, &self->act_gopos.targetpos, self->act_gopos.target, self->act_gopos.speedMode);

            return;
        }

        self->act_gopos.restartTtl = (u16) (ttl - elapsed);
    }
}


/**
 * PD: chrGoPosInitExpensive
*/
static void chrGoPosResetMagic(ChrRecord *self)
{
    coord3d waypos;
    StandTile *waytile;

    chrGoPosGetCurWaypointInfo(self, (coord3d *) &waypos, &waytile);

    self->act_gopos.waydata.mode = 0;
    self->act_gopos.waydata.iter = 0;
    self->act_gopos.waydata.gotPos = 0;

    self->act_gopos.waydata.pos.f[0] = waypos.f[0];
    self->act_gopos.waydata.pos.f[1] = waypos.f[1];
    self->act_gopos.waydata.pos.f[2] = waypos.f[2];

    chrGoPosClearRestartTtl(self);
}


/**
 * PD: chrGoPosAdvanceWaypoint
*/
static void chrActGoposIncCurIndex(ChrRecord *self)
{
    if (self->act_gopos.curindex < 3)
    {
        self->act_gopos.curindex++;
    }
    else
    {
        waypoint * p = self->act_gopos.waypoints[self->act_gopos.curindex];

        self->act_gopos.curindex = 1;

        waypointFindRoute(p, self->act_gopos.target_path, (waypoint **)&self->act_gopos.waypoints, MAX_CHRWAYPOINTS);
    }

    chrGoPosResetMagic(self);
}


/**
 * Determines which step index the chr will be at given their current index, the
 * number of steps to take and in which direction (forward or back).
 *
 * Returns the step index and populates *forward with true or false depending on
 * whether the chr will be traversing the path in the forward direction at that
 * point.
 */
static s32 chrPatrolCalculateStep(ChrRecord *self, bool32 *forward, s32 numsteps)
{
    s32 nextstep = self->act_patrol.nextstep;
    bool isforward = *forward;

    if (numsteps < 0)
    {
        isforward = !isforward;
        numsteps = -numsteps;
    }

    while (numsteps > 0)
    {
        numsteps--;

        if (isforward)
        {
            nextstep++;

            if (self->act_patrol.path->waypoints[nextstep] < 0)
            {
                nextstep -= 2;

                // Reached the end of the list
                if (self->act_patrol.path->isLoop & 1)
                {
                    nextstep = 0;
                }
                else
                {
                    isforward = FALSE;
                }
            }
        }
        else
        {
            nextstep--;

            if (nextstep < 0)
            {
                nextstep = 1;

                // Reached the start of the list
                if (self->act_patrol.path->isLoop & 1)
                {
                    nextstep = self->act_patrol.path->len - 1;
                }
                else
                {
                    isforward = TRUE;
                }
            }
        }
    }

    *forward = isforward;

    return nextstep;
}


/**
 * PD: chrPatrolCalculatePadNum (had some nice finds when searching for "patrol" in "chraction.c" in PD)
*/
static PadRecord *chrGetPatrolStepPad(ChrRecord *self, s32 numsteps)
{
    waypoint *wp;
    bool32 forward;
    s32 step;
    s32 *data;

    forward = self->act_patrol.forward;
    step = chrPatrolCalculateStep(self, &forward, numsteps);

    data = &self->act_patrol.path->waypoints[step];

    wp = &g_CurrentSetup.pathwaypoints[*data];

    return &g_CurrentSetup.pads[wp->padID];
}


static PadRecord *chrGetNextPatrolStepPad(ChrRecord *self)
{
    return chrGetPatrolStepPad(self, 0);
}


static void chrSetNextActPatrolStepPadPos(ChrRecord *self)
{
    PadRecord *pad;

    pad = chrGetNextPatrolStepPad(self);
    self->act_patrol.waydata.mode = 0;
    self->act_patrol.waydata.iter = 0;
    self->act_patrol.waydata.gotPos = 0;
    self->act_patrol.waydata.pos.f[0] = pad->pos.f[0];
    self->act_patrol.waydata.pos.f[1] = pad->pos.f[1];
    self->act_patrol.waydata.pos.f[2] = pad->pos.f[2];
}


static void chrAdvancePatrolStep(ChrRecord *self)
{
    self->act_patrol.nextstep = chrPatrolCalculateStep(self, &self->act_patrol.forward, 1);
    chrSetNextActPatrolStepPadPos(self);
}


/**
 * Returns true if pos is not inside the 2D collision footprint of
 * any normal object (PROP_TYPE_OBJ) in the stan's room.
*/
static bool chrIsPosClearOfObjectBounds(coord3d *pos, StandTile *stan)
{
    s32 roomids[8];
    s16 *propnum;
    PropRecord *props = (PropRecord *)&g_Props;
    coord2d *polygon;
    s32 numedges;

    roomids[0] = stan->room;
    roomids[1] = -1;

    roomGetProps(roomids);

    for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
    {
        PropRecord *prop = &props[*propnum];

        if (prop->type == PROP_TYPE_OBJ)
        {
            propGetCollisionPolygon2d(prop, &polygon, &numedges);

            if (numedges > 0 && polygonContainsPoint(pos, polygon, numedges))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}


/**
 * contrast with @see chrNavTick
*/
static void chrGoPosTickMagic(ChrRecord *self, struct waydata *way, f32 speed, coord3d *pos, StandTile *tile)
{
    /**
     * Three unused stack variables.
    */
    PropRecord *self_prop;
    u8 curindex;
    waypoint *pta;
    PadRecord *pad;
    coord3d waypos;
    StandTile *waytile;

    self->invalidmove = 0;
    self->lastmoveok60 = g_GlobalTimer;
    way->segdistdone += speed * modelGetAbsAnimSpeed(self->model) * g_GlobalTimerDelta;

    if (way->segdisttotal <= way->segdistdone)
    {
        chrSetMoving(self, 0);

        if (
            (stanTestVolume(&tile, pos->f[0], pos->f[2], self->chrwidth, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f) < 0)
            && chrIsPosClearOfObjectBounds(pos, tile))
        {
            self_prop = self->prop;
            self_prop->stan = tile;
            self_prop->pos.f[0] = pos->f[0];
            self_prop->pos.f[1] = pos->f[1];
            self_prop->pos.f[2] = pos->f[2];
            self->chrflags |= CHRFLAG_INIT;

            modelSetRootPosition(self->model, pos);
            chrRootMoveCallback(self->model, &self_prop->pos, &self_prop->pos, &self->ground);
            chrDetectRooms(self);

            if (self->actiontype == ACT_PATROL)
            {
                chrAdvancePatrolStep(self);
                chrSetGoposSegDistTotal(self, way, &chrGetNextPatrolStepPad(self)->pos);
            }
            else if (self->actiontype == ACT_GOPOS)
            {
                curindex = self->act_gopos.curindex;

                if (self->act_gopos.waypoints[curindex] == NULL)
                {
                    if (curindex > 0)
                    {
                        pta = self->act_gopos.waypoints[curindex - 1];
                        pad = &g_CurrentSetup.pads[pta->padID];

                        modelSetRootRotY(
                            self->model,
                            mathAtan2(self_prop->pos.f[0] - pad->pos.f[0], self_prop->pos.f[2] - pad->pos.f[2]));
                    }

                    chrStandCheckWall(self);
                }
                else
                {
                    chrActGoposIncCurIndex(self);
                    chrGoPosGetCurWaypointInfo(self, &waypos, &waytile);
                    chrSetGoposSegDistTotal(self, way, &waypos);
                }
            }
        }
        else
        {
            way->segdistdone = way->segdisttotal;

            if (self->actiontype == ACT_PATROL)
            {
                self->act_patrol.lastvisible60 = g_GlobalTimer;
                chrSetNextActPatrolStepPadPos(self);
            }
            else
            {
                self->act_gopos.magicCheck60 = g_GlobalTimer;
                chrGoPosResetMagic(self);
            }
        }

        chrSetMoving(self, 1);
    }
}


/**
 * Gets character segment percent completed.
 * If action type is ACT_PATROL, computes distance between chr and target pad.
 * If action type is ACT_GOPOS, computes distance between chr and target position.
 * Otherwise result is chr->prop position.
 *
 * @param self:
 * @param arg1: Out parameter. Contains result.
 *
 * PD: chrCalculatePosition.
*/
void chrGetMovementTarget(ChrRecord *self, coord3d *target)
{
    PadRecord *pad;
    f32 percent;
    coord3d waypos; // 44
    StandTile *stan;

    if ((self->actiontype == ACT_PATROL) && (self->act_patrol.waydata.mode == WAYMODE_MAGIC))
    {
        pad = chrGetNextPatrolStepPad(self);

        if (self->act_patrol.waydata.segdisttotal <= self->act_patrol.waydata.segdistdone)
        {
            target->f[0] = pad->pos.f[0];
            target->f[1] = pad->pos.f[1];
            target->f[2] = pad->pos.f[2];

            return;
        }

        percent = self->act_patrol.waydata.segdistdone / self->act_patrol.waydata.segdisttotal;

        target->f[0] = self->prop->pos.f[0] + ((pad->pos.f[0] - self->prop->pos.f[0]) * percent);
        target->f[1] = self->prop->pos.f[1] + ((pad->pos.f[1] - self->prop->pos.f[1]) * percent);
        target->f[2] = self->prop->pos.f[2] + ((pad->pos.f[2] - self->prop->pos.f[2]) * percent);

        return;
    }

    if ((self->actiontype == ACT_GOPOS) && (self->act_gopos.waydata.mode == WAYMODE_MAGIC))
    {
        chrGoPosGetCurWaypointInfo(self, &waypos, &stan);

        if (self->act_gopos.waydata.segdisttotal <= self->act_gopos.waydata.segdistdone)
        {
            target->f[0] = waypos.f[0];
            target->f[1] = waypos.f[1];
            target->f[2] = waypos.f[2];

            return;
        }

        percent = self->act_gopos.waydata.segdistdone / self->act_gopos.waydata.segdisttotal;

        target->f[0] = self->prop->pos.f[0] + ((waypos.f[0] - self->prop->pos.f[0]) * percent);
        target->f[1] = self->prop->pos.f[1] + ((waypos.f[1] - self->prop->pos.f[1]) * percent);
        target->f[2] = self->prop->pos.f[2] + ((waypos.f[2] - self->prop->pos.f[2]) * percent);

        return;
    }

    target->f[0] = self->prop->pos.f[0];
    target->f[1] = self->prop->pos.f[1];
    target->f[2] = self->prop->pos.f[2];
}


/**
 * @param self:
 * @param arg1: sprinting animation when 2, running animation when 1, otherwise walking animation
 * @param arg2:
*/
static void chrGoPosChooseAnimationEx(ChrRecord *self, s32 speed, s32 isMale)
{
    PropRecord *left;
    PropRecord *right;
    s32 ani_arg;
    s32 flag;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    if (((left != NULL) && (right != NULL)) || ((left == NULL) && (right == NULL)))
    {
        flag = 0;
        ani_arg = randomGetNext() & 1;
    }
    else
    {
        s32 t;
        if (weaponIsOneHanded(left) || weaponIsOneHanded(right))
        {
            t = 0;
            flag = t;
            ani_arg = (s32)left != 0;
        }
        else
        {
            t = 1;
            flag = t;
            ani_arg = (s32)left != 0;
        }
    }

    if (flag != 0)
    {
        if (speed == 2)
        {
            modelSetAnimation(self->model, ANIM(sprinting), ani_arg, 0.0f, 0.5f, 16.0f);
        }
        else if (speed == 1)
        {
            modelSetAnimation(self->model, ANIM(running), ani_arg, 0.0f, 0.5f, 16.0f);
        }
        else
        {
            modelSetAnimation(self->model, ANIM(walking), ani_arg, 0.0f, 0.5f, 16.0f);
        }

        return;
    }

    if (isMale)
    {
        if (speed == 2)
        {
            modelSetAnimation(self->model, ANIM(sprinting_one_handed_weapon), ani_arg, 0.0f, 0.5f, 16.0f);
        }
        else if (speed == 1)
        {
            modelSetAnimation(self->model, ANIM(running_one_handed_weapon), ani_arg, 0.0f, 0.5f, 16.0f);
        }
        else
        {
            modelSetAnimation(self->model, ANIM(walking_unarmed), ani_arg, 0.0f, 0.5f, 16.0f);
        }

        return;
    }

    if (speed == 2)
    {
        modelSetAnimation(self->model, ANIM(sprinting_one_handed_weapon), ani_arg, 0.0f, 0.5f, 16.0f);
    }
    else if (speed == 1)
    {
        modelSetAnimation(self->model, ANIM(running_female), ani_arg, 0.0f, 0.5f, 16.0f);
    }
    else
    {
        modelSetAnimation(self->model, ANIM(walking_female), ani_arg, 0.0f, 0.5f, 16.0f);
    }

    return;
}


static void chrGoPosChooseAnimation(ChrRecord *self)
{
    chrGoPosChooseAnimationEx(self, self->act_gopos.speedMode, c_item_entries[self->bodynum].isMale);
}


s32 chrGoToPos(ChrRecord *self, coord3d *pos, StandTile *stan, SPEED speed)
{
    PropRecord *prop;
    waypoint *prop_waypoint;
    waypoint *target_waypoint;
    waypoint *route[MAX_CHRWAYPOINTS];
    s32 i;
    coord3d waypos;
    StandTile *waytile;
    s32 samespeed;

    prop = self->prop;

    samespeed = (self->actiontype == ACT_GOPOS) && (self->act_gopos.speedMode == (u8)speed);

    prop_waypoint = waypointFindNearPos(&prop->pos, prop->stan);
    target_waypoint = waypointFindNearPos(pos, stan);

    if ((prop_waypoint != NULL)
        && (target_waypoint != NULL)
        && !(waypointFindRoute(prop_waypoint, target_waypoint, (waypoint **)&route, MAX_CHRWAYPOINTS) < 2)
    )
    {
        chrStopFiring(self);

        self->actiontype = ACT_GOPOS;

        self->act_gopos.targetpos.f[0] = pos->f[0];
        self->act_gopos.targetpos.f[1] = pos->f[1];
        self->act_gopos.targetpos.f[2] = pos->f[2];
        self->act_gopos.target = stan;
        self->act_gopos.target_path = target_waypoint;
        self->act_gopos.curindex = 0;
        self->act_gopos.speedMode = speed;
        self->act_gopos.speed = 0.0f;
        self->act_gopos.waydata.age = (s32) (randomGetNext() % 100U);
        self->act_gopos.waydata.gotPosCopy = 0;
        self->act_gopos.magicCheck60 = -1;

        for (i=0; i<MAX_CHRWAYPOINTS; i++)
        {
            self->act_gopos.waypoints[i] = route[i];
        }

        chrGoPosResetMagic(self);
        self->sleep = 0;

        if (samespeed == 0)
        {
            chrGoPosChooseAnimation(self);
        }

        chrGoPosGetCurWaypointInfo(self, &waypos, &waytile);

        if (((prop->flags & 2) == 0) && (chrPathIsOffscreen(self, &waypos, waytile) != 0))
        {
            chrSetGoposSegDistTotal(self, &self->act_gopos.waydata, &waypos);
        }

        return 1;
    }

    return 0;
}


static void chrPatrolChooseAnimation(ChrRecord *self)
{
    PropRecord *left;
    PropRecord *right;
    s32 ani_arg;
    s32 flag;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    if (((left != NULL) && (right != NULL)) || ((left == NULL) && (right == NULL)))
    {
        flag = 0;
        ani_arg = randomGetNext() & 1;
    }
    else
    {
        s32 t;
        if (weaponIsOneHanded(left) || weaponIsOneHanded(right))
        {
            t = 0;
            flag = t;
            ani_arg = (s32)left != 0;
        }
        else
        {
            t = 1;
            flag = t;
            ani_arg = (s32)left != 0;
        }
    }

    if (flag != 0)
    {
        modelSetAnimation(self->model, ANIM(walking), ani_arg, 0.0f, 0.5f, 16.0f);
    }
    else
    {
        f32 tf = (0.5f * g_ChrSpeedWalk) / g_ChrSpeedWalkUnarmed;
        modelSetAnimation(self->model, ANIM(walking_unarmed), ani_arg, 0.0f, tf, 16.0f);
    }

    return;
}


void chrStartPatrol(ChrRecord *self, PathRecord *path)
{
    PadRecord *pad;
    s32 next_step = -1;
    PropRecord *prop = self->prop;
    s32 count = 0;
    waypoint *pta;
    s32 *dataptr;
    f32 dx;
    f32 dz;

    if (path->waypoints[0] >= 0)
    {
        do
        {
            dataptr = &path->waypoints[count];
            pta = &g_CurrentSetup.pathwaypoints[*dataptr];
            pad = &g_CurrentSetup.pads[pta->padID];

            if ((pad->stan != NULL) && (pad->stan == prop->stan))
            {
                dx = pad->pos.f[0] - prop->pos.f[0];
                dz = pad->pos.f[2] - prop->pos.f[2];

                if (((dx * dx) + (dz * dz)) < 10000.0f)
                {
                    
                    next_step = count;
                    break;
                }
            }

            count++;
        }
        while (path->waypoints[count] >= 0);
    }

    if (next_step < 0)
    {
        next_step = 0;
    }

    chrStopFiring(self);

    self->actiontype = ACT_PATROL;
    self->act_patrol.path = path;
    self->act_patrol.nextstep = next_step;
    self->act_patrol.forward = 1;
    self->act_patrol.waydata.age = randomGetNext() % 0x64U;
    self->act_patrol.waydata.gotPosCopy = 0;
    self->act_patrol.lastvisible60 = -1;
    self->act_patrol.speed = 0.0f;

    chrSetNextActPatrolStepPadPos(self);

    self->sleep = 0;

    chrPatrolChooseAnimation(self);

    pad = chrGetNextPatrolStepPad(self);

    if ((self->prop->flags & PROPFLAG_ONSCREEN) == FALSE)
    {
        if (chrPathToPadIsOffscreen(self, pad) != 0)
        {
            chrSetGoposSegDistTotal(self, &self->act_patrol.waydata, &pad->pos);
        }
    }
}


s32 propCanWalkLine(PropRecord *prop, coord3d *dest, coord3d *dir)
{
    ChrRecord *chr;
    f32 startX;
    f32 startZ;
    f32 endX;
    f32 endZ;
    StandTile *stan;
    f32 chrx;
    f32 chrz;
    s32 ret;
    f32 height;
    f32 always20;
    f32 width;

    chr = prop->chr;
    chrx = dir->f[0] * chr->chrwidth * 1.2f;
    chrz = dir->f[2] * chr->chrwidth * 1.2f;
    ret = 0;

    chrGetChrWidthHeight(prop, &width, &height, &always20);
    chrSetMoving(chr, 0);

    startX = prop->pos.f[0] + chrz;
    startZ = prop->pos.f[2] - chrx;
    endX = dest->f[0] + chrz + chrx;
    endZ = (dest->f[2] - chrx) + chrz;

    stan = prop->stan;

    if (
        (stanTestLineUnobstructed(&stan, prop->pos.f[0], prop->pos.f[2], startX, startZ, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER , height, always20, 0.0f, 1.0f) != 0)
        && (stanTestLineUnobstructed(&stan, startX, startZ, endX, endZ, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER , height, always20, 0.0f, 1.0f) != 0)
        )
    {
        startX = prop->pos.f[0] - chrz;
        startZ = prop->pos.f[2] + chrx;
        endX = (dest->f[0] - chrz) + chrx;
        endZ = dest->f[2] + chrx + chrz;

        // why is this getting set again?
        stan = prop->stan;

        if (
            (stanTestLineUnobstructed(&stan, prop->pos.f[0], prop->pos.f[2], startX, startZ, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER , height, always20, 0.0f, 1.0f) != 0)
            && (stanTestLineUnobstructed(&stan, startX, startZ, endX, endZ, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER , height, always20, 0.0f, 1.0f) != 0)
            )
        {
            ret = 1;
        }
    }

    chrSetMoving(chr, 1);

    return ret;
}


/**
*/
s32 propCanWalkInDir(PropRecord *prop, coord3d *dir, f32 dist)
{
    coord3d dest;

    dest.f[0] = prop->pos.f[0] + (dir->f[0] * dist);
    dest.f[1] = prop->pos.f[1];
    dest.f[2] = prop->pos.f[2] + (dir->f[2] * dist);

    return propCanWalkLine(prop, &dest, dir);
}


bool chrHasStoppedOrPatroling(ChrRecord *self)
{
    if ((self->actiontype == ACT_STAND) && !self->act_stand.prestand && !self->act_stand.reaim)
    {
        return TRUE;
    }
    else if (self->actiontype == ACT_ANIM)
    {
        if (self->act_anim.playSfx ||
            ((modelGetAnimSpeed(self->model) >= 0.0f) && modelGetAnimFrame(self->model) >= modelGetAnimEndFrame(self->model)) ||
            ((modelGetAnimSpeed(self->model)  < 0.0f) && modelGetAnimFrame(self->model) <= 0.0f)
           )
        {
            return TRUE;
        }
    }
    else if (self->actiontype == ACT_PATROL)
    {
        return TRUE;
    }

    return FALSE;
}


bool chrGoToPad(ChrRecord *self, s32 padid, SPEED speed)
{
    PadRecord *pad;
    StandTile *stan2;
    coord3d region;
    StandTile *stan;

    if ((padid >= 0) && chrIsNotDeadOrShot(self) && (g_SeenBondRecentlyGuardCount < 10))
    {
        padid = chrResolvePadId(self, padid);
        if (isNotBoundPad(padid))
        {
            pad = &g_CurrentSetup.pads[padid];
        }
        else
        {
            pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padid)];
        }

        stan = pad->stan;
        if (stan)
        {
            //if pad is not vertical
            if (pad->up.y < 0.5f)
            {
                stan2    = stan;
                region.x = (pad->up.x * (self->chrwidth * 1.1f)) + pad->pos.x;
                region.y = (pad->up.y * (self->chrwidth * 1.1f)) + pad->pos.y;
                region.z = (pad->up.z * (self->chrwidth * 1.1f)) + pad->pos.z;

                //if able to reach region surrounding pad?
                if (stanWalkLine(&stan2, pad->pos.x, pad->pos.z, region.x, region.z) &&
                    chrGoToPos(self, &region, stan2, speed))
                {
                    return TRUE;
                }
            }
            else if (chrGoToPos(self, &pad->pos, stan, speed))
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}


#if !defined(REFRESH_PAL)
/* NTSC */
#define MAX_SPEED_A 0.2991993f
#define ACCEL_A 0.014959966f

#define MAX_SPEED_B1 0.019634955f
#define MAX_SPEED_B2 0.09817477f
#define MAX_SPEED_B3 0.19634955f
#define ACCEL_B 0.014959966f

#define MAX_SPEED_C1 0.009817477f
#define MAX_SPEED_C2 0.049087387f
#define MAX_SPEED_C3 0.12566371f
#define ACCEL_C 0.009817477f
#endif

#if defined(REFRESH_PAL)
/* PAL */
#define MAX_SPEED_A 0.359039157629013f
#define ACCEL_A 0.0179519578814507f

#define MAX_SPEED_B1 0.0235619451850653f
#define MAX_SPEED_B2 0.117809727787972f
#define MAX_SPEED_B3 0.235619455575943f
#define ACCEL_B 0.0179519578814507f

#define MAX_SPEED_C1 0.0117809725925326f
#define MAX_SPEED_C2 0.0589048638939858f
#define MAX_SPEED_C3 0.150796458125114f
#define ACCEL_C 0.0117809725925326f
#endif

s32 chrApplySpeed(ChrRecord *self, coord3d *target, s32 speed, f32 *speedPtr)
{
    f32 maxSpeed;
    Model *self_model; // 72
    PropRecord *self_prop;
    f32 accel;
    f32 maxFrac; // 60
    f32 openPosition; // 56
    f32 angle;
    f32 f0f0;
    f32 dx;
    f32 dz;
    s32 arrived;

    self_prop = self->prop;
    self_model = self->model;

    dx = target->f[0] - self_prop->pos.f[0];
    dz = target->f[2] - self_prop->pos.f[2];

    maxFrac = mathAtan2(dx, dz);
    openPosition = modelGetRootRotY(self_model);

    arrived = 0;
    angle = maxFrac - openPosition;

    if (maxFrac < openPosition)
    {
        angle = angle + M_TAU_F;
    }

    f0f0 = angle;

    if (angle > M_PI_F)
    {
        f0f0 = M_TAU_F - angle;
    }

    if (speed == 2)
    {
        maxSpeed = MAX_SPEED_A;
        accel = ACCEL_A;
    }
    else if (speed == 1)
    {
        if (f0f0 < 0.3926991f)
        {
            maxSpeed = MAX_SPEED_B1;
        }
        else if (f0f0 < 1.2566371f)
        {
            maxSpeed = MAX_SPEED_B2;
        }
        else
        {
            maxSpeed = MAX_SPEED_B3;
        }

        accel = ACCEL_B;
    }
    else
    {
        if (f0f0 < 0.3926991f)
        {
            maxSpeed = MAX_SPEED_C1;
        }
        else if (f0f0 < 1.2566371f)
        {
            maxSpeed = MAX_SPEED_C2;
        }
        else
        {
            maxSpeed = MAX_SPEED_C3;
        }

        accel = ACCEL_C;
    }

    maxSpeed *= self_model->playspeed;
    accel *= self_model->playspeed;

    physTurnTowards(
        &openPosition,
        maxFrac,
        speedPtr,
        accel,
        accel * 2.0f,
        maxSpeed, g_ClockTimer);

    if (openPosition == maxFrac)
    {
        *speedPtr = 0.0f;
        arrived = 1;
    }

    modelSetRootRotY(self_model, openPosition);

    return arrived;
}


/**
 * @param arg0: point in 3d
 * @param arg1: 3 vec
 * @param arg0: point in 3d
 * @param arg0: scalar value
*/
s32 rayPassesNearPoint2d(coord3d *origin, coord3d *dir, coord3d *point, f32 range)
{
    coord3d dd;
    f32 along;

    dd.f[0] = point->f[0] - origin->f[0];
    dd.f[2] = point->f[2] - origin->f[2];

    if ((dir->f[0] == 0.0f) && (dir->f[2] == 0.0f))
    {
        return ((dd.f[0] * dd.f[0]) + (dd.f[2] * dd.f[2])) <= (range * range);
    }

    along = (dir->f[0] * dd.f[0]) + (dir->f[2] * dd.f[2]);

    if (along > 0.0f)
    {
        f32 alongSq = along * along;
        f32 dirLenSq = (dir->f[0] * dir->f[0]) + (dir->f[2] * dir->f[2]);
        f32 f3 = (dd.f[0] * dd.f[0]) + (dd.f[2] * dd.f[2]);

        if ((dirLenSq * (f3 - (range * range))) <= alongSq)
        {
            return 1;
        }

        return 0;
    }

    return 0;
}


/**
 * @param prevpos: point in 3d
 * @param curpos: 3 vec
 * @param prevpos: point in 3d
 * @param prevpos: scalar value
 *
 *
 * PD posIsArrivingLaterallyAtPos.
*/
s32 chrIsArrivingLaterallyAtPos(coord3d *prevpos, coord3d *curpos, coord3d *targetpos, f32 range)
{
    coord3d dir;

    if ((prevpos->f[0] <= targetpos->f[0] - range) && (curpos->f[0] <= targetpos->f[0] - range))
    {
        return 0;
    }

    if ((targetpos->f[0] + range <= prevpos->f[0]) && (targetpos->f[0] + range <= curpos->f[0]))
    {
        return 0;
    }

    if ((prevpos->f[2] <= targetpos->f[2] - range) && (curpos->f[2] <= targetpos->f[2] - range))
    {
        return 0;
    }

    if ((targetpos->f[2] + range <= prevpos->f[2]) && (targetpos->f[2] + range <= curpos->f[2]))
    {
        return 0;
    }

    dir.f[0] = curpos->f[0] - prevpos->f[0];
    dir.f[1] = 0.0f;
    dir.f[2] = curpos->f[2] - prevpos->f[2];

    return rayPassesNearPoint2d(prevpos, &dir, targetpos, range);
}


void chrTickRunPos(ChrRecord *self)
{
    PropRecord *self_prop;
    Model *self_model;
    f32 frame; // 52

    self_prop = self->prop;
    self_model = self->model;
    self->lastwalk60 = g_GlobalTimer;

    if(1)
    {
        // removed
    }

    if ((self->invalidmove == 1)
        || (self->lastmoveok60 < (g_GlobalTimer - CHRLV_LASTMOVEOK60_CHECK))
        || (chrIsArrivingLaterallyAtPos(&self->prevpos, &self_prop->pos, &self->act_runpos.pos, self->act_runpos.neardist)))
    {
        f32 offset = 0;

        // Maybe had debug side effects, otherwise this doesn't do anything.
        modelGetAnim(self_model);

        frame = modelGetAnimFrame(self_model) - offset;

        if (frame < 0.0f)
        {
            frame += (f32)modelGetAnim(self_model)->numFrames;
        }

        if (((f32)modelGetAnim(self_model)->numFrames * 0.5f) < frame)
        {
            frame = (f32)modelGetAnim(self_model)->numFrames - offset;
            modelSetAnimSpeedToReachFrame(self_model, frame, 16.0f);
        }
        else
        {
            frame = ((f32)modelGetAnim(self_model)->numFrames * 0.5f) - offset;

            if (frame < 0)
            {
                frame += (f32)modelGetAnim(self_model)->numFrames;
            }

            modelSetAnimSpeedToReachFrame(self_model, frame, 16.0f);
        }

        chrStandCheckWall(self);

        return;
    }

    chrApplySpeed(self, &self->act_runpos.pos, 1, &self->act_runpos.turnspeed);

    if (self->act_runpos.eta60 > 0)
    {
        self->act_runpos.eta60 -= g_ClockTimer;
    }
    else
    {
        f32 speed;

        speed = g_ChrSpeedRun;

        if (modelGetAnim(self_model) == ANIM(running_one_handed_weapon))
        {
            speed = g_ChrSpeedRunOneHanded;
        }

        self->act_runpos.neardist += speed * g_GlobalTimerDelta * modelGetAbsAnimSpeed(self_model);
    }
}


static s32 chrCanWalkLine(ChrRecord *self, coord3d *point, StandTile *tile, coord3d *dest, StandTile * desttile, s32 cdtypes)
{
    StandTile *curtile;
    s32 ok;
    f32 height;
    f32 always20;
    f32 width;

    curtile = tile;
    ok = 0;

    chrGetChrWidthHeight(self->prop, &width, &height, &always20);

    chrSetMoving(self, 0);

    if (
        stanTestLineUnobstructed(&curtile, point->f[0], point->f[2], dest->f[0], dest->f[2], cdtypes, height, always20, 0.0f, 1.0f)
        && ((desttile == NULL) || (curtile == desttile)))
    {
        ok = 1;
    }

    chrSetMoving(self, 1);

    return ok;
}


static s32 chrCanWalkLineWithin(ChrRecord *self, coord3d *point, StandTile *tile, coord3d *dest, f32 halfwidth, s32 cdtypes)
{
    StandTile *pstan;
    coord3d dd;
    f32 offsetX;
    f32 offsetZ;
    f32 norm;
    s32 ret; // 104
    f32 height;
    f32 always20;
    f32 width;

    ret = 0;

    chrGetChrWidthHeight(self->prop, &width, &height, &always20);

    dd.f[0] = dest->f[0] - point->f[0];
    dd.f[1] = 0.0f;
    dd.f[2] = dest->f[2] - point->f[2];

    if ((dd.f[0] == 0.0f) && (dd.f[2] == 0.0f))
    {
        ret = 1;
    }
    else
    {
        norm = (dd.f[0] * dd.f[0]) + (dd.f[2] * dd.f[2]);
        norm = 1.0f / sqrtf(norm);

        dd.f[0] *= norm;
        dd.f[2] *= norm;

        offsetX = halfwidth * dd.f[0];
        offsetZ = halfwidth * dd.f[2];

        chrSetMoving(self, 0);

        pstan = tile;

        if (stanTestLineUnobstructed(&pstan, point->f[0], point->f[2], point->f[0] + offsetZ, point->f[2] - offsetX, cdtypes, height, always20, 0.0f, 1.0f)
            && stanTestLineUnobstructed(&pstan, point->f[0] + offsetZ, point->f[2] - offsetX, dest->f[0] + offsetZ, dest->f[2] - offsetX, cdtypes, height, always20, 0.0f, 1.0f))
        {
            pstan = tile;

            if (stanTestLineUnobstructed(&pstan, point->f[0], point->f[2], point->f[0] - offsetZ, point->f[2] + offsetX, cdtypes, height, always20, 0.0f, 1.0f)
                && stanTestLineUnobstructed(&pstan, point->f[0] - offsetZ, point->f[2] + offsetX, dest->f[0] - offsetZ, dest->f[2] + offsetX, cdtypes, height, always20, 0.0f, 1.0f))
            {
                ret = 1;
            }
        }

        chrSetMoving(self, 1);
    }

    return ret;
}


s32 chrCanWalkViaPoint(ChrRecord *self, coord3d *mypos, StandTile *mystan, coord3d *viapoint, coord3d *bondpos, StandTile *bondstan, s32 cdtypes)
{
    StandTile *tile;
    bool pass;
    f32 height;
    f32 always20;
    f32 width;
    StandTile *viatile;

    tile = mystan;
    pass = FALSE;

    chrGetChrWidthHeight(self->prop, &width, &height, &always20);
    chrSetMoving(self, 0);

    if (stanTestLineUnobstructed(&tile, mypos->x, mypos->z, viapoint->x, viapoint->z, cdtypes, height, always20, 0.0f, 1.0f))
    {
        viatile = tile;

        if (stanTestLineUnobstructed(&viatile, viapoint->x, viapoint->z, bondpos->x, bondpos->z, cdtypes, height, always20, 0.0f, 1.0f)
            && ((bondstan == NULL) || (viatile == bondstan)))
        {
            pass = TRUE;
        }
    }

    chrSetMoving(self, 1);

    return pass;
}


/**
 * Subtract arg0 from arg1. Take the determinate of the result and arg2.
 * If determinate is not greater than zero, then swap arg0 and arg1.
*/
static void orderPointsBySide(coord3d *a, coord3d *b, coord3d *dir)
{
    coord3d ab;
    coord3d perp;

    ab.f[0] = b->f[0] - a->f[0];
    ab.f[1] = b->f[1] - a->f[1];
    ab.f[2] = b->f[2] - a->f[2];

    perp.f[0] = -dir->f[2];
    perp.f[1] = 0.0f;
    perp.f[2] = dir->f[0];

    if (!(((perp.f[0] * ab.f[0]) + (perp.f[2] * ab.f[2])) > 0.0f))
    {
        ab.f[0] = a->f[0];
        ab.f[1] = a->f[1];
        ab.f[2] = a->f[2];

        a->f[0] = b->f[0];
        a->f[1] = b->f[1];
        a->f[2] = b->f[2];

        b->f[0] = ab.f[0];
        b->f[1] = ab.f[1];
        b->f[2] = ab.f[2];
    }
}


/**
 * Very similar to @see chrNavCheckForObstacle .
*/
static s32 chrNavCanSeeNextPos(ChrRecord *self, coord3d *from, StandTile *fromtile, coord3d *to, coord3d *edgeStart, coord3d *edgeEnd, f32 overshoot, f32 width, s32 cdtypes)
{
    StandTile *tile;
    coord3d dir;
    f32 nearX; // 156
    f32 nearZ; // 152
    f32 farX; // 148
    f32 farZ; // 144
    f32 norm;
    s32 leftBlocked; // 136
    s32 rightBlocked; // 132
    coord3d leftEdgeStart;
    coord3d leftEdgeEnd;
    coord3d rightEdgeStart;
    coord3d rightEdgeEnd;
    s32 ok;
    f32 height;
    f32 always20;
    f32 chrw;

    leftBlocked = 0;
    rightBlocked = 0;
    ok = 0;

    chrGetChrWidthHeight(self->prop, &chrw, &height, &always20);

    dir.f[0] = to->f[0] - from->f[0];
    dir.f[1] = 0.0f;
    dir.f[2] = to->f[2] - from->f[2];

    if ((dir.f[0] == 0.0f) && (dir.f[2] == 0.0f))
    {
        return 1;
    }

    norm = (dir.f[0] * dir.f[0]) + (dir.f[2] * dir.f[2]);
    norm = 1.0f / sqrtf(norm);

    dir.f[0] *= norm;
    dir.f[2] *= norm;

    nearX = 0.95f * (width * dir.f[0]);
    nearZ = 0.95f * (width * dir.f[2]);

    farX = 1.2f * (width * dir.f[0]);
    farZ = 1.2f * (width * dir.f[2]);

    chrSetMoving(self, 0);
    stanResetHits();

    tile = fromtile;

    if ((stanTestLineUnobstructed(
        &tile,
        from->f[0],
        from->f[2],
        from->f[0] + nearZ,
        from->f[2] - nearX,
        cdtypes,
        height,
        always20,
        0.0f,
        1.0f) == 0)
        || (stanTestLineUnobstructed(
            &tile,
            from->f[0] + nearZ,
            from->f[2] - nearX,
            (to->f[0] + farZ) + (dir.f[0] * overshoot),
            (to->f[2] - farX) + (dir.f[2] * overshoot),
            cdtypes,
            height,
            always20,
            0.0f,
            1.0f) == 0))
    {
        leftBlocked = 1;

        stanGetCollisionEdge(&leftEdgeStart, &leftEdgeEnd);
        orderPointsBySide(&leftEdgeStart, &leftEdgeEnd, &dir);
    }

    tile = fromtile;

    if ((stanTestLineUnobstructed(
        &tile,
        from->f[0],
        from->f[2],
        from->f[0] - nearZ,
        from->f[2] + nearX,
        cdtypes,
        height,
        always20,
        0.0f,
        1.0f) == 0)
        || (stanTestLineUnobstructed(
            &tile,
            from->f[0] - nearZ,
            from->f[2] + nearX,
            (to->f[0] - farZ) + (dir.f[0] * overshoot),
            (to->f[2] + farX) + (dir.f[2] * overshoot),
            cdtypes,
            height,
            always20,
            0.0f,
            1.0f) == 0))
    {
        rightBlocked = 1;

        stanGetCollisionEdge(&rightEdgeStart, &rightEdgeEnd);
        orderPointsBySide(&rightEdgeStart, &rightEdgeEnd, &dir);
    }

    if ((leftBlocked != 0) && (rightBlocked != 0))
    {
        orderPointsBySide(&leftEdgeStart, &rightEdgeStart, &dir);
        orderPointsBySide(&leftEdgeEnd, &rightEdgeEnd, &dir);

        edgeStart->f[0] = leftEdgeStart.f[0];
        edgeStart->f[1] = leftEdgeStart.f[1];
        edgeStart->f[2] = leftEdgeStart.f[2];

        edgeEnd->f[0] = rightEdgeEnd.f[0];
        edgeEnd->f[1] = rightEdgeEnd.f[1];
        edgeEnd->f[2] = rightEdgeEnd.f[2];
    }
    else if (leftBlocked != 0)
    {
        edgeStart->f[0] = leftEdgeStart.f[0];
        edgeStart->f[1] = leftEdgeStart.f[1];
        edgeStart->f[2] = leftEdgeStart.f[2];

        edgeEnd->f[0] = leftEdgeEnd.f[0];
        edgeEnd->f[1] = leftEdgeEnd.f[1];
        edgeEnd->f[2] = leftEdgeEnd.f[2];
    }
    else if (rightBlocked != 0)
    {
        edgeStart->f[0] = rightEdgeStart.f[0];
        edgeStart->f[1] = rightEdgeStart.f[1];
        edgeStart->f[2] = rightEdgeStart.f[2];

        edgeEnd->f[0] = rightEdgeEnd.f[0];
        edgeEnd->f[1] = rightEdgeEnd.f[1];
        edgeEnd->f[2] = rightEdgeEnd.f[2];
    }
    else
    {
        tile = fromtile;

        if (stanTestLineUnobstructed(&tile, from->f[0], from->f[2], to->f[0], to->f[2], cdtypes, height, always20, 0.0f, 1.0f)
            && stanTestVolume(&tile, to->f[0], to->f[2], width, cdtypes, height, always20) < 0)
        {
            ok = 1;
        }
        else
        {
            stanGetCollisionEdge(edgeStart, edgeEnd);
            orderPointsBySide(edgeStart, edgeEnd, &dir);
        }
    }

    chrSetMoving(self, 1);

    return ok;
}


/**
 * Very similar to @see chrNavCanSeeNextPos .
*/
static s32 chrNavCheckForObstacle(ChrRecord *self, coord3d *from, StandTile *fromtile, coord3d *to, coord3d *edgeStart, coord3d *edgeEnd, f32 overshoot, f32 width, s32 cdtypes)
{
    StandTile *tile;
    coord3d dir;
    f32 nearX; // 164
    f32 nearZ; // 160
    f32 farX; // 156
    f32 farZ; // 152
    f32 norm;
    s32 leftBlocked; // 144
    s32 rightBlocked; // 140
    coord3d leftEdgeStart; // x
    coord3d leftEdgeEnd;
    coord3d rightEdgeStart;
    coord3d rightEdgeEnd;
    s32 ok;
    f32 stanval1;
    f32 stanval2;
    f32 height;
    f32 always20;
    f32 chrw;

    leftBlocked = 0;
    rightBlocked = 0;
    ok = 0;

    chrGetChrWidthHeight(self->prop, &chrw, &height, &always20);

    dir.f[0] = to->f[0] - from->f[0];
    dir.f[1] = 0.0f;
    dir.f[2] = to->f[2] - from->f[2];

    if ((dir.f[0] == 0.0f) && (dir.f[2] == 0.0f))
    {
        return 1;
    }

    norm = (dir.f[0] * dir.f[0]) + (dir.f[2] * dir.f[2]);
    norm = 1.0f / sqrtf(norm);

    dir.f[0] *= norm;
    dir.f[2] *= norm;

    nearX = 0.95f * (width * dir.f[0]);
    nearZ = 0.95f * (width * dir.f[2]);

    farX = 1.2f * (width * dir.f[0]);
    farZ = 1.2f * (width * dir.f[2]);

    chrSetMoving(self, 0);
    stanResetHits();

    tile = fromtile;

    if ((stanTestLineUnobstructed(
        &tile,
        from->f[0],
        from->f[2],
        from->f[0] + nearZ,
        from->f[2] - nearX,
        cdtypes,
        height,
        always20,
        0.0f,
        1.0f) == 0)
        || (stanTestLineUnobstructed(
            &tile,
            from->f[0] + nearZ,
            from->f[2] - nearX,
            (to->f[0] + farZ) + (dir.f[0] * overshoot),
            (to->f[2] - farX) + (dir.f[2] * overshoot),
            cdtypes,
            height,
            always20,
            0.0f,
            1.0f) == 0))
    {
        leftBlocked = 1;

        stanGetCollisionEdge(&leftEdgeStart, &leftEdgeEnd);
        orderPointsBySide(&leftEdgeStart, &leftEdgeEnd, &dir);

        stanval1 = g_StanHit.frac;
    }

    tile = fromtile;

    if ((stanTestLineUnobstructed(
        &tile,
        from->f[0],
        from->f[2],
        from->f[0] - nearZ,
        from->f[2] + nearX,
        cdtypes,
        height,
        always20,
        0.0f,
        1.0f) == 0)
        || (stanTestLineUnobstructed(
            &tile,
            from->f[0] - nearZ,
            from->f[2] + nearX,
            (to->f[0] - farZ) + (dir.f[0] * overshoot),
            (to->f[2] + farX) + (dir.f[2] * overshoot),
            cdtypes,
            height,
            always20,
            0.0f,
            1.0f) == 0))
    {
        rightBlocked = 1;

        stanGetCollisionEdge(&rightEdgeStart, &rightEdgeEnd);
        orderPointsBySide(&rightEdgeStart, &rightEdgeEnd, &dir);

        stanval2 = g_StanHit.frac;
    }

    if ((leftBlocked != 0) && (rightBlocked != 0))
    {
        if (stanval1 < stanval2)
        {
            edgeStart->f[0] = leftEdgeStart.f[0];
            edgeStart->f[1] = leftEdgeStart.f[1];
            edgeStart->f[2] = leftEdgeStart.f[2];

            edgeEnd->f[0] = leftEdgeEnd.f[0];
            edgeEnd->f[1] = leftEdgeEnd.f[1];
            edgeEnd->f[2] = leftEdgeEnd.f[2];
        }
        else
        {
            edgeStart->f[0] = rightEdgeStart.f[0];
            edgeStart->f[1] = rightEdgeStart.f[1];
            edgeStart->f[2] = rightEdgeStart.f[2];

            edgeEnd->f[0] = rightEdgeEnd.f[0];
            edgeEnd->f[1] = rightEdgeEnd.f[1];
            edgeEnd->f[2] = rightEdgeEnd.f[2];
        }
    }
    else if (leftBlocked != 0)
    {
        edgeStart->f[0] = leftEdgeStart.f[0];
        edgeStart->f[1] = leftEdgeStart.f[1];
        edgeStart->f[2] = leftEdgeStart.f[2];

        edgeEnd->f[0] = leftEdgeEnd.f[0];
        edgeEnd->f[1] = leftEdgeEnd.f[1];
        edgeEnd->f[2] = leftEdgeEnd.f[2];
    }
    else if (rightBlocked != 0)
    {
        edgeStart->f[0] = rightEdgeStart.f[0];
        edgeStart->f[1] = rightEdgeStart.f[1];
        edgeStart->f[2] = rightEdgeStart.f[2];

        edgeEnd->f[0] = rightEdgeEnd.f[0];
        edgeEnd->f[1] = rightEdgeEnd.f[1];
        edgeEnd->f[2] = rightEdgeEnd.f[2];
    }
    else
    {
        tile = fromtile;

        if (stanTestLineUnobstructed(&tile, from->f[0], from->f[2], to->f[0], to->f[2], cdtypes, height, always20, 0.0f, 1.0f)
            && stanTestVolume(&tile, to->f[0], to->f[2], width, cdtypes, height, always20) < 0)
        {
            ok = 1;
        }
        else
        {
            stanGetCollisionEdge(edgeStart, edgeEnd);
            orderPointsBySide(edgeStart, edgeEnd, &dir);
        }
    }

    chrSetMoving(self, 1);

    return ok;
}


static s32 chrNavTryObstacle(
    ChrRecord *self,
    coord3d *corner,
    s32 left,
    coord3d *othercorner,
    f32 clearance,
    s32 checktarget,
    coord3d *target,
    struct waydata *way,
    f32 overshoot,
    s32 cdtypes,
    s32 set_copy)
{
    PropRecord *self_prop; // -- 124
    coord3d dd; // -- 112
    coord3d pos; // -- 100
    f32 norm; // -- 96
    f32 angle; // 92
    coord3d offset; // 80
    coord3d *edgeStart; // 76
    coord3d *edgeEnd; // 72

    self_prop = self->prop;

    if (left != 0)
    {
        edgeStart = corner;
        edgeEnd = othercorner;
    }
    else
    {
        edgeStart = othercorner;
        edgeEnd = corner;
    }

    dd.f[0] = corner->f[0] - self_prop->pos.f[0];
    dd.f[1] = 0.0f;
    dd.f[2] = corner->f[2] - self_prop->pos.f[2];

    norm = 1.0f / sqrtf((dd.f[0] * dd.f[0]) + (dd.f[2] * dd.f[2]));

    dd.f[0] *= clearance * norm;
    dd.f[2] *= clearance * norm;

    if (clearance * norm > 1.0f)
    {
        angle = DegToRad(45);
    }
    else
    {
        angle = mathAcos(clearance * norm);
    }

    if ((left == 0) && (angle != 0.0f))
    {
        angle = M_TAU_F - angle;
    }

    offset.f[0] = (-cosf(angle) * dd.f[0]) + (sinf(angle) * dd.f[2]);
    offset.f[1] = 0.0f;
    offset.f[2] = (-sinf(angle) * dd.f[0]) - (cosf(angle) * dd.f[2]);

    pos.f[0] = corner->f[0] + offset.f[0];
    pos.f[1] = corner->f[1];
    pos.f[2] = corner->f[2] + offset.f[2];

    if (chrNavCanSeeNextPos(self, &self_prop->pos, self_prop->stan, &pos, edgeStart, edgeEnd, overshoot, self->chrwidth, cdtypes)
        && ((checktarget == 0) || chrCanWalkViaPoint(self, &self_prop->pos, self_prop->stan, &pos, target, NULL, cdtypes)))
    {
        if (set_copy != 0)
        {
            way->gotPosCopy = 1;

            way->pos_copy.f[0] = pos.f[0];
            way->pos_copy.f[1] = pos.f[1];
            way->pos_copy.f[2] = pos.f[2];
        }
        else
        {
            way->gotPos = 1;

            way->pos.f[0] = pos.f[0];
            way->pos.f[1] = pos.f[1];
            way->pos.f[2] = pos.f[2];
        }

        return 1;
    }

    return 0;
}


/**
 * Iterates travel mode. Used by both ACT_GOPOS and ACT_PATROL.
 * 10% chance to open door.
 * Calls apply speed.
 *
 * @see chrTickPatrol
 * @see chrTickGoPos
 * contrast with @see chrGoPosTickMagic
*/
static void chrNavTick(ChrRecord *self, coord3d *target, struct waydata *way)
{
    coord3d targetpos; // 260
    coord3d othercorner; // 244
    s32 i; // 240
    f32 clearance; // 232
    struct ObjectRecord *obj;
    f32 atan_pos2_a; // 212
    f32 atan_pos3_a;
    PropRecord *self_prop;
    f32 dx;
    f32 atan_pos2_b; // 196
    f32 atan_pos3_b;
    f32 dy;
    f32 dz;
    f32 atan_pos2_c; // 180
    f32 atan_pos3_c;
    s32 max;
    f32 atan_pos;
    s32 cdtypes;
    PropRecord *door;

    self_prop = self->prop;
    cdtypes = CDTYPE_OBJS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER | CDTYPE_DOORSLOCKEDTOAI;
    if ((self->hidden & CHRHIDDEN_OFFSCREEN_PATROL) != 0)
    {
        cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
    }

    max=1;
    for (i=0; i<1; i++)
    {
        if ((way->mode == WAYMODE_0) || (way->mode == WAYMODE_2))
        {
            targetpos.f[0] = target->f[0];
            targetpos.f[1] = target->f[1];
            targetpos.f[2] = target->f[2];

            if (chrNavCanSeeNextPos(self, &self_prop->pos, self_prop->stan, &targetpos, &way->pos2, &way->pos3, -(self->chrwidth), self->chrwidth, CDTYPE_PATHBLOCKER) != 0)
            {
                way->gotPos = (u8) 1;
                way->pos.f[0] = targetpos.f[0];
                way->pos.f[1] = targetpos.f[1];
                way->pos.f[2] = targetpos.f[2];
                way->mode = WAYMODE_4;
            }
            else
            {
                if (way->mode == WAYMODE_0)
                {
                    way->mode = (s8) WAYMODE_1;
                    way->iter = 0;
                }
                else if (way->mode == WAYMODE_2)
                {
                    way->mode = WAYMODE_3;
                    way->iter = 0;
                }
            }
        }
        else if (way->mode == WAYMODE_1)
        {
            clearance = self->chrwidth * 1.2f * 1.05f;

            if (chrNavTryObstacle(self, &way->pos2, 1, &othercorner, clearance, 1, target, way, 0.0f, 0x10, 0) != 0)
            {
                way->mode = WAYMODE_4;
            }
            else if (chrNavTryObstacle(self, &way->pos3, 0, &othercorner, clearance, 1, target, way, 0.0f, 0x10, 0) != 0)
            {
                way->mode = WAYMODE_4;
            }
            else
            {
                way->iter = way->iter + 1;
                if (way->iter >= MAX_WAYMODE)
                {
                    way->mode = WAYMODE_2;
                }
            }
        }
        else if (way->mode == WAYMODE_3)
        {
            clearance = self->chrwidth * 1.2f * 1.05f;

            if (chrNavTryObstacle(self, &way->pos2, 1, &othercorner, clearance, 0, NULL, way, 0.0f, 0x10, 0) != 0)
            {
                way->mode = WAYMODE_4;
            }
            else if (chrNavTryObstacle(self, &way->pos3, 0, &othercorner, clearance, 0, NULL, way, 0.0f, 0x10, 0) != 0)
            {
                way->mode = WAYMODE_4;
            }
            else
            {
                way->iter = way->iter + 1;
                if (way->iter >= MAX_WAYMODE)
                {
                    way->gotPos = 0;
                    way->gotPosCopy = (u8) (s8) way->gotPos;
                    way->pos_copy.f[0] =
                        way->pos.f[0] = target->f[0];
                    way->pos_copy.f[1] =
                        way->pos.f[1] = target->f[1];
                    way->pos_copy.f[2] =
                        way->pos.f[2] = target->f[2];
                    way->mode = WAYMODE_0;
                }
            }
        }
        else if (way->mode == WAYMODE_4)
        {
            if (chrNavCheckForObstacle(self, &self_prop->pos, self_prop->stan, &way->pos, &way->pos2, &way->pos3, -(self->chrwidth), self->chrwidth, cdtypes) != 0)
            {
                way->gotPosCopy = max;
                way->mode = WAYMODE_0;
                way->pos_copy.f[0] = way->pos.f[0];
                way->pos_copy.f[1] = way->pos.f[1];
                way->pos_copy.f[2] = way->pos.f[2];
            }
            else
            {
                way->mode = WAYMODE_5;
                way->iter = 0;
            }
        }
        else if (way->mode == WAYMODE_5)
        {
            clearance = self->chrwidth * 1.2f * 1.05f;

            atan_pos =       mathAtan2( way->pos.f[0] - self_prop->pos.f[0],  way->pos.f[2] - self_prop->pos.f[2]);
            atan_pos2_a = atan_pos - mathAtan2(way->pos2.f[0] - self_prop->pos.f[0], way->pos2.f[2] - self_prop->pos.f[2]);
            atan_pos3_a = atan_pos - mathAtan2(way->pos3.f[0] - self_prop->pos.f[0], way->pos3.f[2] - self_prop->pos.f[2]);

            if (atan_pos2_a < 0.0f)
            {
                atan_pos2_a = atan_pos2_a + M_TAU_F;
            }

            if (atan_pos2_a >= M_PI_F)
            {
                atan_pos2_a = atan_pos2_a - M_TAU_F;
            }

            if (atan_pos2_a < 0.0f)
            {
                atan_pos2_a = -atan_pos2_a;
            }

            if (atan_pos3_a < 0.0f)
            {
                atan_pos3_a = atan_pos3_a + M_TAU_F;
            }

            if (atan_pos3_a >= M_PI_F)
            {
                atan_pos3_a = atan_pos3_a - M_TAU_F;
            }

            if (atan_pos3_a < 0.0f)
            {
                atan_pos3_a = -atan_pos3_a;
            }

            if (atan_pos2_a < atan_pos3_a)
            {
                if (chrNavTryObstacle(self, &way->pos2, 1, &othercorner, clearance, 0, NULL, way, clearance * 1.1f, cdtypes, 1) != 0)
                {
                    way->mode = WAYMODE_0;
                    break;
                }
                else
                {
                    atan_pos2_b = atan_pos - mathAtan2(way->pos2.f[0] - self_prop->pos.f[0], way->pos2.f[2] - self_prop->pos.f[2]);
                    atan_pos3_b = atan_pos - mathAtan2(othercorner.f[0] - self_prop->pos.f[0], othercorner.f[2] - self_prop->pos.f[2]);

                    if (atan_pos2_b < 0.0f)
                    {
                        atan_pos2_b = atan_pos2_b + M_TAU_F;
                    }
                    if (atan_pos2_b >= M_PI_F)
                    {
                        atan_pos2_b = atan_pos2_b - M_TAU_F;
                    }
                    if (atan_pos2_b < 0.0f)
                    {
                        atan_pos2_b = -atan_pos2_b;
                    }

                    if (atan_pos3_b < 0.0f)
                    {
                        atan_pos3_b = atan_pos3_b + M_TAU_F;
                    }
                    if (atan_pos3_b >= M_PI_F)
                    {
                        atan_pos3_b = atan_pos3_b - M_TAU_F;
                    }
                    if (atan_pos3_b < 0.0f)
                    {
                        atan_pos3_b = -atan_pos3_b;
                    }

                    if ((atan_pos3_b < atan_pos2_b) && (chrNavTryObstacle(self, &othercorner, 0, &othercorner, clearance, 0, NULL, way, clearance * 1.1f, cdtypes, 1) != 0))
                    {
                        way->mode = WAYMODE_0;
                        break;
                    }
                }
            }
            else
            {
                if (chrNavTryObstacle(self,  &way->pos3, 0, &othercorner, clearance, 0, NULL, way, clearance * 1.1f, cdtypes, 1) != 0)
                {
                    way->mode = WAYMODE_0;
                    break;
                }
                else
                {
                    atan_pos2_c = atan_pos - mathAtan2(way->pos3.f[0] - self_prop->pos.f[0], way->pos3.f[2] - self_prop->pos.f[2]);
                    atan_pos3_c = atan_pos - mathAtan2(othercorner.f[0] - self_prop->pos.f[0], othercorner.f[2] - self_prop->pos.f[2]);

                    if (atan_pos2_c < 0.0f)
                    {
                        atan_pos2_c = atan_pos2_c + M_TAU_F;
                    }
                    if (atan_pos2_c >= M_PI_F)
                    {
                        atan_pos2_c = atan_pos2_c - M_TAU_F;
                    }
                    if (atan_pos2_c < 0.0f)
                    {
                        atan_pos2_c = -atan_pos2_c;
                    }

                    if (atan_pos3_c < 0.0f)
                    {
                        atan_pos3_c = atan_pos3_c + M_TAU_F;
                    }
                    if (atan_pos3_c >= M_PI_F)
                    {
                        atan_pos3_c = atan_pos3_c - M_TAU_F;
                    }
                    if (atan_pos3_c < 0.0f)
                    {
                        atan_pos3_c = -atan_pos3_c;
                    }

                    if ((atan_pos3_c < atan_pos2_c) && (chrNavTryObstacle(self, &othercorner, 1, &othercorner, clearance, 0, NULL, way, clearance * 1.1f, cdtypes, 1) != 0))
                    {
                        way->mode = WAYMODE_0;
                        break;
                    }
                }
            }

            way->iter = way->iter + 1;
            if (way->iter >= MAX_WAYMODE)
            {
                way->gotPosCopy = 0;
                way->mode = WAYMODE_0;
            }
        }
    }

    if (way->gotPosCopy == 0)
    {
        way->pos_copy.f[0] = way->pos.f[0];
        way->pos_copy.f[1] = way->pos.f[1];
        way->pos_copy.f[2] = way->pos.f[2];
    }

    if (((s32) way->age % 10) == 0)
    {
        door = stanLineFindDoor(self_prop->stan, self_prop->pos.f[0], self_prop->pos.f[2], way->pos_copy.f[0], way->pos_copy.f[2], 0x5000);

        if (door != NULL)
        {
            obj = door->obj;
            if (!(obj->flags2 & PROPFLAG_DOOR_OPENTOFRONT))
            {
                dx = door->pos.f[0] - self_prop->pos.f[0];
                dy = door->pos.f[1] - self_prop->pos.f[1];
                dz = door->pos.f[2] - self_prop->pos.f[2];

                if (((dx * dx) + (dy * dy) + (dz  * dz )) < 40000.0f)
                {
                    chrGoPosClearRestartTtl(self);
                    doorChooseSwingDirection(self_prop, door->door);
                    doorMove(door->door, DOORSTATE_OPENING);

                    if (((self->hidden & CHRHIDDEN_OFFSCREEN_PATROL) == 0)
                        && (modelGetAnim(self->model) != ANIM(idle_unarmed))
                        && (modelGetAnim(self->model) != ANIM(idle)))
                    {
                        chrStandChooseAnimation(self, 16.0f);
                        self->lastmoveok60 = g_GlobalTimer;
                    }
                }
                else
                {
                    door = NULL;
                }
            }
            else
            {
                door = NULL;
            }
        }

        if ((door == NULL) || ((self->hidden & CHRHIDDEN_OFFSCREEN_PATROL) != 0))
        {
            if ((modelGetAnim(self->model) == ANIM(idle_unarmed))
                || (modelGetAnim(self->model) == ANIM(idle)))
            {
                if (self->actiontype == ACT_PATROL)
                {
                    chrPatrolChooseAnimation(self);
                }
                else
                {
                    chrGoPosChooseAnimation(self);
                }
            }

            if (door == NULL)
            {
                self->hidden &= ~(CHRHIDDEN_OFFSCREEN_PATROL);
            }
        }
    }

    if (self->actiontype == ACT_PATROL)
    {
        chrApplySpeed(self, &way->pos_copy, 0, &self->act_patrol.speed);
    }
    else
    {
        chrApplySpeed(self, &way->pos_copy, (s32) self->act_gopos.speedMode, &self->act_gopos.speed);

        if (self->act_gopos.speedMode == 2)
        {
            if (self->act_gopos.speed != 0.0f)
            {
                modelSetAnimSpeed(self->model, 0.25f, 0.0f);
            }
            else if (self->chrflags & CHRFLAG_INCREASE_RUNNING_SPEED)
            {
                modelSetAnimSpeed(self->model, 0.65f, 0.0f);
            }
            else
            {
                modelSetAnimSpeed(self->model, 0.5f, 0.0f);
            }
        }
        else if (self->act_gopos.speedMode == 1)
        {
            if (self->act_gopos.speed != 0.0f)
            {
                modelSetAnimSpeed(self->model, 0.4f, 0.0f);
            }
            else
            {
                modelSetAnimSpeed(self->model, 0.5f, 0.0f);
            }
        }
    }
}


void chrTickGoPos(ChrRecord *self)
{
    waypoint *wp;
    coord3d *wp_pos;
    StandTile *wp_stan;
    PropRecord *self_prop;
    s32 restarted;
    coord3d waypos;
    StandTile *waytile;
    coord3d waypos2;
    StandTile *waytile2;
    PadRecord *pad; // 80
    s32 arrived; // 76

    self_prop = self->prop;
    restarted = 0;
    self->act_gopos.waydata.age += 1;
    self->lastwalk60 = g_GlobalTimer;

    if (self->lastmoveok60 < (g_GlobalTimer - CHRLV_LASTMOVEOK60_CHECK))
    {
        chrGoToPos(self, &self->act_gopos.targetpos, self->act_gopos.target, (s32) self->act_gopos.speedMode);
    }

    chrGoPosConsiderRestart(self);

    if ((self->act_gopos.waydata.mode != WAYMODE_MAGIC) && ((self->act_gopos.magicCheck60 + CHRLV_DEFAULT_TIMER) < g_GlobalTimer))
    {
        chrGoPosGetCurWaypointInfo(self, &waypos, &waytile);

        if (chrPathIsOffscreen(self, &waypos, waytile))
        {
            restarted = 1;
            chrSetGoposSegDistTotal(self, &self->act_gopos.waydata, &waypos);
        }
    }

    if (g_SeenBondRecentlyGuardCount >= 0xA)
    {
        chrStandCheckWall(self);

        return;
    }

    if (self->act_gopos.waydata.mode == WAYMODE_MAGIC)
    {
        chrGoPosGetCurWaypointInfo(self, &waypos2, &waytile2);

        if ((restarted == 0)
            && ((self_prop->flags & PROPFLAG_ONSCREEN) || (chrPathIsOffscreen(self, &waypos2, waytile2) == 0)))
        {
            chrGoPosResetMagic(self);
            self->act_gopos.magicCheck60 = g_GlobalTimer;

            return;
        }

        chrGoPosTickMagic(self, &self->act_gopos.waydata, chrGetRunAnimSpeed(self), &waypos2, waytile2);

        return;
    }


    arrived = 0;

    wp = self->act_gopos.waypoints[self->act_gopos.curindex];

    if (wp != NULL)
    {
        if (chrIsArrivingLaterallyAtPos(&self->prevpos, &self_prop->pos, &g_CurrentSetup.pads[wp->padID].pos, 30.0f) != 0)
        {
            arrived = 1;
        }
    }
    else
    {
        if (chrIsArrivingLaterallyAtPos(&self->prevpos, &self_prop->pos, &self->act_gopos.targetpos, 30.0f) != 0)
        {
            chrStandCheckWall(self);

            return;
        }
    }

    if (arrived != 0)
    {
        chrActGoposIncCurIndex(self);
    }

    if (((s32) self->act_gopos.waydata.age % 10) == 5)
    {
        wp = self->act_gopos.waypoints[self->act_gopos.curindex];

        if (wp != NULL)
        {
            wp = self->act_gopos.waypoints[self->act_gopos.curindex + 1];

            if (wp != NULL)
            {
                wp = self->act_gopos.waypoints[self->act_gopos.curindex + 2];

                if (wp != NULL)
                {
                    pad = &g_CurrentSetup.pads[wp->padID];
                    wp_pos = &pad->pos;
                    wp_stan = pad->stan;
                }
                else
                {
                    wp_pos = &self->act_gopos.targetpos;
                    wp_stan = self->act_gopos.target;
                }

                if (chrCanWalkLine(self, &self_prop->pos, self_prop->stan, wp_pos, wp_stan, CDTYPE_PATHBLOCKER)
                    && chrCanWalkLineWithin(self, &self_prop->pos, self_prop->stan, wp_pos, self->chrwidth * 1.2f, CDTYPE_PATHBLOCKER))
                {
                    chrActGoposIncCurIndex(self);
                    chrActGoposIncCurIndex(self);
                }
            }
        }
    }

    if (((s32) self->act_gopos.waydata.age % 10) == 0)
    {
        wp = self->act_gopos.waypoints[self->act_gopos.curindex];

        if (wp != NULL)
        {
            wp = self->act_gopos.waypoints[self->act_gopos.curindex + 1];

            if (wp != NULL)
            {
                pad = &g_CurrentSetup.pads[wp->padID];
                wp_pos = &pad->pos;
                wp_stan = pad->stan;
            }
            else
            {
                wp_pos = &self->act_gopos.targetpos;
                wp_stan = self->act_gopos.target;
            }

            if (chrCanWalkLine(self, &self_prop->pos, self_prop->stan, wp_pos, wp_stan, CDTYPE_PATHBLOCKER)
                && chrCanWalkLineWithin(self, &self_prop->pos, self_prop->stan, wp_pos, self->chrwidth * 1.2f, CDTYPE_PATHBLOCKER))
            {
                chrActGoposIncCurIndex(self);
            }
        }
    }

    wp = self->act_gopos.waypoints[self->act_gopos.curindex];

    if (wp != NULL)
    {
        pad = &g_CurrentSetup.pads[wp->padID];
        wp_pos = &pad->pos;
        wp_stan = pad->stan;
    }
    else
    {
        wp_pos = &self->act_gopos.targetpos;
        wp_stan = self->act_gopos.target;
    }

    chrNavTick(self, wp_pos, &self->act_gopos.waydata);
}


void chrTickPatrol(ChrRecord *self)
{
    PropRecord *self_prop;
    s32 restarted;
    PadRecord *pad;

    self_prop = self->prop;
    pad = (PadRecord *) chrGetNextPatrolStepPad(self);
    restarted = 0;
    self->act_patrol.waydata.age += 1;
    self->lastwalk60 = g_GlobalTimer;

    if ((self->act_patrol.waydata.mode != WAYMODE_MAGIC)
        && ((self->act_patrol.lastvisible60 + CHRLV_DEFAULT_TIMER) < g_GlobalTimer)
        && chrPathToPadIsOffscreen(self, pad))
    {
        restarted = 1;
        chrSetGoposSegDistTotal(self, &self->act_patrol.waydata, &pad->pos);
    }

    if (self->act_patrol.waydata.mode == WAYMODE_MAGIC)
    {
        if ((restarted == 0)
            && ((self_prop->flags & PROPFLAG_ONSCREEN) || (chrPathToPadIsOffscreen(self, pad) == 0)))
        {
            self->act_patrol.lastvisible60 = g_GlobalTimer;
            chrSetNextActPatrolStepPadPos(self);
        }
        else
        {
            chrGoPosTickMagic(self, &self->act_patrol.waydata, g_ChrSpeedWalk, &pad->pos, pad->stan);
        }
    }
    else
    {
        if(1)
        {
            // removed
        }

        if (chrIsArrivingLaterallyAtPos(&self->prevpos, &self_prop->pos, &pad->pos, 30.0f))
        {
            chrAdvancePatrolStep(self);
            pad = (PadRecord *)chrGetNextPatrolStepPad(self);
        }

        chrNavTick(self, &pad->pos, &self->act_patrol.waydata);
    }
}

