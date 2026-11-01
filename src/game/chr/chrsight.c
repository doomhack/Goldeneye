/**
 * What guards see and hear. A guard notices the player at random, more likely the closer
 * the player is and the more nearly in front, and only along a clear line
 * (chrCheckTargetInSight). Seeing or hearing the player records when
 * (lastseetarget60, lastheartarget60). Gunfire and other noise
 * alerts every guard within its hearing range (chrCheckGuardsHeardSound).
 *
 * "The player" is the current player. In co-op each guard picks which player that is
 * before its AI and action run (coopChrSelectTarget).
 */
#include <ultra64.h>
#include <bondaicommands.h>
#include <bondconstants.h>
#include <bondgame.h>
#include <random.h>
#include "game/world/bgfog.h"
#include "game/player/player.h"
#include "game/chr/chr.h"
#include "game/chr/chraction.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/level/gamemode.h"
#include "game/player/coop.h"

/* Sight lines are taken from this far below the top of a guard's head */
#define EYE_DROP 20.0f

/* A guard sees what's within this angle either side of straight ahead */
#define SIGHT_HALF_ANGLE 110

/* ...but only notices what's within this angle at full speed */
#define NOTICE_HALF_ANGLE 45

/* Guards see this far whatever their vision range */
#define SIGHT_MIN_RANGE 200

/* Co-op: how often (in 60ths of a second) a chr that hasn't seen anyone lately looks
 * for the nearest player to fight */
#define COOP_RETARGET_INTERVAL60 30

/* Co-op: how often a chr that is fighting one player checks whether it can see the
 * other player closer by */
#define COOP_SWITCH_INTERVAL60 60

/* Co-op: the other player must be this much closer (as a fraction of the distance to
 * the current target) for a chr to switch to them */
#define COOP_SWITCH_CLOSER 0.75f

/**
 * Whether the chr saw its target in the last 10 seconds.
 */
bool chrSawTargetRecently(ChrRecord *self)
{
    return self->lastseetarget60 > 0 && (g_GlobalTimer - self->lastseetarget60) < CHRLV_10_SEC_TIMER;
}

/**
 * Whether the chr heard its target in the last 10 seconds.
 */
bool chrHeardTargetRecently(ChrRecord *self)
{
    return self->lastheartarget60 > 0 && (g_GlobalTimer - self->lastheartarget60) < CHRLV_10_SEC_TIMER;
}

/**
 * Whether there's a clear sight line from pos (on tile) to what the chr's attacking
 * (the current player unless it's attacking something else). Always TRUE for an attack
 * aimed straight ahead.
 */
bool chrCheckCanSeeTarget(ChrRecord *self, coord3d *pos, StandTile *tile)
{
    s32 attacktype = ATTACKTYPE_BOND;
    bool canSee = FALSE;
    StandTile *stan = tile;
    StandTile *targettile;
    s32 cdtypes;

    if (self->actiontype == ACT_ATTACK)
    {
        attacktype = self->act_attack.attacktype;
    }

    if (attacktype & ATTACKTYPE_FORWARD)
    {
        return TRUE;
    }

    coord3d *targetpos = chrGetChrOrPresetLocation(self, attacktype, self->act_attack.entityid, &targettile);

    chrSetMoving(self, FALSE);

    if (attacktype & ATTACKTYPE_BOND)
    {
        playerSetMoving(g_CurrentPlayer->prop, 0);

        if (playerGetVisibleToGuards()
            && stanTestLineUnobstructed(&stan, pos->x, pos->z, targetpos->x, targetpos->z,
                   CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE,
                   pos->y, pos->y, targetpos->y, targetpos->y)
            && stan == targettile)
        {
            self->seen_bond_time = g_GlobalTimer;
            canSee = TRUE;
        }

        playerSetMoving(g_CurrentPlayer->prop, 1);
    }
    else if (attacktype & (ATTACKTYPE_CHR | ATTACKTYPE_PAD))
    {
        // A chr target mustn't block the line itself, but players do
        cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE;

        if (!(attacktype & ATTACKTYPE_CHR))
        {
            cdtypes |= CDTYPE_CHRS;
        }

        if (stanTestLineUnobstructed(&stan, pos->x, pos->z, targetpos->x, targetpos->z, cdtypes,
                pos->y, pos->y, targetpos->y, targetpos->y)
            && stan == targettile)
        {
            canSee = TRUE;
        }
    }

    chrSetMoving(self, TRUE);

    return canSee;
}

/**
 * Whether the chr has a clear sight line to the current player, who must be visible to
 * guards. Records the sighting.
 */
bool chrCanSeeBond(ChrRecord *self)
{
    bool canSee = FALSE;

    if (!playerGetVisibleToGuards())
    {
        return FALSE;
    }

    PropRecord *prop = self->prop;
    PropRecord *playerprop = currentPlayerGetProp();
    f32 eyeheight = self->chrheight - EYE_DROP;

    chrSetMoving(self, FALSE);
    playerSetMoving(g_CurrentPlayer->prop, 0);

    StandTile *stan = prop->stan;

    if (stanTestLineUnobstructed(&stan, prop->pos.x, prop->pos.z, playerprop->pos.x, playerprop->pos.z,
            CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE, eyeheight, eyeheight, 0.0f, 1.0f)
        && stan == playerprop->stan)
    {
        self->seen_bond_time = g_GlobalTimer;
        canSee = TRUE;
    }

    chrSetMoving(self, TRUE);
    playerSetMoving(g_CurrentPlayer->prop, 1);

    return canSee;
}

/**
 * Whether the chr has a clear sight line to pos, on stan. Chrs and players don't block it.
 */
bool chrHasLineOfSightToPos(ChrRecord *self, coord3d *pos, StandTile *stan)
{
    PropRecord *prop = self->prop;
    f32 eyeheight = self->chrheight - EYE_DROP;
    bool canSee = FALSE;

    chrSetMoving(self, FALSE);

    StandTile *propstan = prop->stan;

    if (stanTestLineUnobstructed(&propstan, prop->pos.x, prop->pos.z, pos->x, pos->z,
            CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE, eyeheight, eyeheight, 0.0f, 1.0f)
        && propstan == stan)
    {
        canSee = TRUE;
    }

    chrSetMoving(self, TRUE);

    return canSee;
}

/**
 * Whether the chr can see pos: it's within 100 degrees either side of where the chr
 * faces, and the sight line is clear.
 */
bool chrCanSeePos(ChrRecord *self, coord3d *pos, StandTile *tile)
{
    f32 heading = modelGetRootRotY(self->model);
    f32 angle = mathAtan2(pos->x - self->prop->pos.x, pos->z - self->prop->pos.z);
    f32 diff = angle - heading;

    if (angle < heading)
    {
        diff += M_TAU_F;
    }

    if (diff < DegToRad(100) || diff > DegToRad(260))
    {
        return chrHasLineOfSightToPos(self, pos, tile);
    }

    return FALSE;
}

/**
 * Whether there's a clear line from the current player's eyes to the chr: objects,
 * doors and scenery block it, chrs don't.
 */
bool chrHasLineOfSightToPlayer(ChrRecord *self)
{
    PropRecord *prop = self->prop;
    PropRecord *playerprop = currentPlayerGetProp();
    bool canSee = FALSE;

    playerSetMoving(g_CurrentPlayer->prop, 0);

    StandTile *stan = playerprop->stan;

    if (stanTestLineUnobstructed(&stan, playerprop->pos.x, playerprop->pos.z, prop->pos.x, prop->pos.z,
            CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, playerprop->pos.y, playerprop->pos.y, 0.0f, 1.0f)
        && stan == prop->stan)
    {
        canSee = TRUE;
    }

    playerSetMoving(g_CurrentPlayer->prop, 1);

    return canSee;
}

/**
 * The chr sees the current player now.
 */
void chrSetTargetToPlayer(ChrRecord *self)
{
    self->lastseetarget60 = g_GlobalTimer;
}

/**
 * The chr hears the current player now.
 */
void chrAlertGuardToPlayerPosition(ChrRecord *self)
{
    self->hidden |= CHRHIDDEN_ALERT_GUARD_RELATED;
    self->lastheartarget60 = g_GlobalTimer;
}

/**
 * Whether the chr sees the current player this tick. Once it has seen them in the last
 * 10 seconds it keeps seeing them while the sight line is clear. Otherwise the player
 * must be in range (vision range, at least 200), within 110 degrees either side of
 * ahead and not lost in fog, and then the chr notices them at random: one chance in
 * about distance/5 m, more when they're off to the side, fewer for a slow (low 007
 * speed rating) guard.
 */
bool chrCheckTargetInSight(ChrRecord *self)
{
    PropRecord *prop = self->prop;
    PropRecord *playerprop = currentPlayerGetProp();
    f32 heading = modelGetRootRotY(self->model);
    f32 dx = playerprop->pos.x - prop->pos.x;
    f32 dy = playerprop->pos.y - prop->pos.y;
    f32 dz = playerprop->pos.z - prop->pos.z;
    f32 angle = mathAtan2(dx, dz);
    f32 diff = angle - heading;
    bool sees = FALSE;

    if (angle < heading)
    {
        diff += M_TAU_F;
    }

    if (chrSawTargetRecently(self))
    {
        sees = TRUE;
    }
    else
    {
        f32 distsq = SQR(dx) + SQR(dy) + SQR(dz);
        bool inview = diff < DegToRad(SIGHT_HALF_ANGLE) || diff > DegToRad(360 - SIGHT_HALF_ANGLE);
        bool inrange = distsq < (self->visionrange * self->visionrange * 100.0f * 100.0f) || distsq < SQR(SIGHT_MIN_RANGE);

        if (inview && inrange && distsq < fogGetScaledFarFogIntensitySquared())
        {
            s32 chance = (s32)((sqrtf(distsq) * 30.0f) / 16000.0f);

            // Off to the side: fewer chances, the further the more
            if (diff > DegToRad(NOTICE_HALF_ANGLE) && diff < DegToRad(360.0 - NOTICE_HALF_ANGLE))
            {
                f32 offangle = diff;

                if (diff > M_PI_F)
                {
                    offangle = M_TAU_F - diff;
                }

                offangle -= DegToRad(NOTICE_HALF_ANGLE);

                chance *= (s32)((offangle * 24.0f) / M_TAU_F) + 1;
            }

            chance = chrGetGuard007SpeedRatingInt(self, chance) + 1;
            sees = randomGetNext() % (u32)chance == 0;
        }
    }

    if (sees)
    {
        sees = chrCanSeeBond(self);
    }

    if (sees)
    {
        chrSetTargetToPlayer(self);
    }

    return sees;
}

/**
 * Something the current player did made a noise (gunfire, an explosion): every guard
 * closer than its hearing scale times the noise (in metres) hears them. In counter-op
 * the guard player is one of the guards, so their noise alerts nobody.
 */
void chrCheckGuardsHeardSound(f32 noise)
{
    s32 i;

    if (gameIsCounterOp() && playerGetCurrentNum() != COUNTEROP_BOND)
    {
        return;
    }

    for (i = 0; i < g_NumChrSlots; i++)
    {
        ChrRecord *chr = &g_ChrSlots[i];

        if (chr->model != NULL && chrGetDistanceToBond(chr) < chr->hearingscale * (noise * 100.0f))
        {
            chrAlertGuardToPlayerPosition(chr);
        }
    }
}

static f32 coopChrDistSqToPlayer(ChrRecord *self, s32 player)
{
    PropRecord *prop = g_playerPointers[player]->prop;
    f32 dx = prop->pos.x - self->prop->pos.x;
    f32 dz = prop->pos.z - self->prop->pos.z;

    return (dx * dx) + (dz * dz);
}

/**
 * Co-op: a chr busy fighting one player switches to another living player it can see
 * who is clearly closer, so that a second player walking into a fight gets shot at too.
 */
static s32 coopChrTrySwitchToCloserPlayer(ChrRecord *self, s32 target)
{
    s32 count = getPlayerCount();
    f32 targetdist = coopChrDistSqToPlayer(self, target);
    s32 i;

    for (i = 0; i < count; i++)
    {
        s32 lastsee = self->lastseetarget60;

        if (i == target || g_playerPointers[i]->bonddead)
        {
            continue;
        }

        if (coopChrDistSqToPlayer(self, i) >= (targetdist * (COOP_SWITCH_CLOSER * COOP_SWITCH_CLOSER)))
        {
            continue;
        }

        playerSetCurrent(i);

        if (chrCanSeeBond(self))
        {
            // chrCanSeeBond noted the sighting; make it the new target's
            self->targetplayer = i;
            chrSetTargetToPlayer(self);
            self->lastheartarget60 = 0;
            return i;
        }

        self->lastseetarget60 = lastsee;
    }

    return target;
}

/**
 * Co-op: a chr shot by a player turns on them.
 */
void coopChrShotByCurrentPlayer(ChrRecord *self)
{
    s32 player = playerGetCurrentNum();

    if (!gameIsCoop() || self->prop == NULL || self->prop->type != PROP_TYPE_CHR
        || self->targetplayer == player || g_CurrentPlayer->bonddead)
    {
        return;
    }

    self->targetplayer = player;
    self->lastseetarget60 = 0;
    self->lastheartarget60 = g_GlobalTimer;
}

/**
 * Co-op: picks the player this chr fights and makes them the current player for its AI
 * list and action tick, so all of the "Bond" logic (sight, shooting, damage, chasing)
 * applies to that player.
 *
 * A chr keeps a living target. Without one, or when it hasn't seen anyone for a while,
 * it switches to the nearest living player every half second, forgetting when it last
 * saw or heard its old target. Unarmed chrs (allies such as Natalya, civilians) stay
 * with the player they met, so an ally follows whoever rescued her, until that player
 * dies. Seeing or hearing the other player retargets it straight away (see the
 * any-player AI commands in aiRunList()), and so does being shot by them. While it fights one
 * player it also switches to the other if it can see them clearly closer by (checked
 * every second).
 */
void coopChrSelectTarget(ChrRecord *self)
{
    s32 count = getPlayerCount();
    s32 target = self->targetplayer;
    bool unarmed = self->weapons_held[GUNRIGHT] == NULL && self->weapons_held[GUNLEFT] == NULL;
    s32 i;

    if (target < 0 || target >= count)
    {
        target = 0;
    }

    if (self->prop != NULL && self->prop->type == PROP_TYPE_VIEWER)
    {
        // A player's own body (cutscene AI lists, death and intro animations) acts for
        // that player
        target = playerGetIndexForProp(self->prop);

        if (target < 0)
        {
            target = 0;
        }
    }
    else if (gameIsCounterOp())
    {
        // Counter-op: every guard, and every level script, is about Bond
        target = COUNTEROP_BOND;
    }
    else if (self->targetlock60 > 0 && !g_playerPointers[target]->bonddead)
    {
        // A level-script check picked this player: stay with them while the script
        // carries on (e.g. Dam's bungee jump)
        self->targetlock60 -= g_ClockTimer;
    }
    else if (self->prop != NULL
        && (g_playerPointers[target]->bonddead
            || (!unarmed && !chrSawTargetRecently(self) && (((u32)g_GlobalTimer + self->chrnum) % COOP_RETARGET_INTERVAL60) < (u32)g_ClockTimer)))
    {
        f32 bestdist = 0.0f;
        s32 best = -1;

        for (i = 0; i < count; i++)
        {
            if (g_playerPointers[i]->bonddead)
            {
                continue;
            }

            f32 dist = coopChrDistSqToPlayer(self, i);

            if (best < 0 || dist < bestdist)
            {
                best = i;
                bestdist = dist;
            }
        }

        if (best >= 0)
        {
            target = best;
        }
    }
    else if (self->prop != NULL
        && self->prop->type == PROP_TYPE_CHR
        && !unarmed
        && chrSawTargetRecently(self)
        && (((u32)g_GlobalTimer + self->chrnum) % COOP_SWITCH_INTERVAL60) < (u32)g_ClockTimer)
    {
        target = coopChrTrySwitchToCloserPlayer(self, target);
    }

    if (target != self->targetplayer)
    {
        // What a chr saw or heard was the old target. Without this the new target counts
        // as seen for up to 10 seconds, so guards would know where a respawned player is
        // without looking.
        self->lastseetarget60 = 0;
        self->lastheartarget60 = 0;
    }

    self->targetplayer = target;
    playerSetCurrent(target);
}
