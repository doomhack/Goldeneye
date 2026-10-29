/**
 * Security cameras.
 *
 * A camera sweeps its head between two yaws, speeding up and slowing down smoothly. If it
 * sees a player for long enough (5 seconds on most difficulties), it sets off the alarm.
 * It sees players within 45 degrees of where the head points, within its view distance,
 * and with nothing in the way. In co-op it watches every living player.
 *
 * A destroyed camera (PROPFLAG_CCTV_DISABLED) stops.
 */

#include <ultra64.h>
#include <math.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/cctv.h"
#include "game/prop/alarm.h"
#include "game/prop/propobj.h"
#include "game/math/matrix.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/nav/stan.h"
#include "game/player/player.h"
#include "game/player/bondview.h"

/* How long a camera must see a player to set off the alarm */
#ifdef VERSION_EU
#define CCTV_ALARM_FRAMES 250.0f
#else
#define CCTV_ALARM_FRAMES 300.0f
#endif

/* How fast the head speeds up and slows down: about 135 degrees per second per second */
#define CCTV_YAW_ACCEL 0.00065449846f

/* Set by difficulty: Agent cameras take twice as long to set off the alarm, and take
   twice the damage */
f32 g_CctvAlarmTimeScale = 1.0;
f32 g_CctvDamageScale = 1.0;

/**
 * Whether a camera or drone gun may watch a player. Counter-op: only Bond, as the guard
 * player is on the cameras' side. Otherwise any living player.
 */
bool objMayWatchPlayer(s32 player)
{
    if (g_CounterOpMode)
    {
        return player == COUNTEROP_BOND;
    }

    return !g_playerPointers[player]->bonddead;
}


/* The camera's yaw (relative to its base yaw) brought into 0 to 2 pi */
static f32 cctvGetHeadYaw(CCTVRecord *cctv)
{
    f32 yaw = cctv->yaw;

    if (yaw < 0.0f)
    {
        yaw += M_TAU_F;
    }
    else if (yaw >= M_TAU_F)
    {
        yaw -= M_TAU_F;
    }

    return yaw;
}


/* Whether the camera can see a player this frame */
static bool cctvCanSeePlayer(PropRecord *prop, CCTVRecord *cctv, PropRecord *playerprop)
{
    ObjectRecord *obj = prop->obj;
    f32 xdiff = playerprop->pos.f[0] - obj->runtime_pos.f[0];
    f32 viewdist = cctv->viewDist;
    f32 ydiff = playerprop->pos.f[1] - obj->runtime_pos.f[1];
    f32 zdiff = playerprop->pos.f[2] - obj->runtime_pos.f[2];
    bool seen = true;

    if (viewdist > 0.0f && (viewdist * viewdist) < (((xdiff * xdiff) + (ydiff * ydiff)) + (zdiff * zdiff)))
    {
        seen = false;
    }

    if (obj->flags & PROPFLAG_INMOTION)
    {
        seen = false;
    }

    if (seen)
    {
        f32 angletoplayer = mathAtan2(xdiff, zdiff);
        f32 headyaw = cctvGetHeadYaw(cctv) + cctv->baseYaw;
        f32 delta;

        if (headyaw >= M_TAU_F)
        {
            headyaw -= M_TAU_F;
        }

        // The angle from the head to the player, from -pi to pi
        delta = angletoplayer - headyaw;

        if (angletoplayer < headyaw)
        {
            delta += M_TAU_F;
        }

        delta -= M_PI_F;

        if (delta < 0.0f)
        {
            delta += M_TAU_F;
        }

        if (delta > M_PI_F)
        {
            delta -= M_TAU_F;
        }

        if (delta > DegToRad(45) || delta < DegToRad(-45))
        {
            seen = false;
        }
    }

    if (seen)
    {
        StandTile *tile = prop->stan;

        seen = false;
        playerSetMoving(playerprop, 0);

        if (stanTestLineUnobstructed(&tile, prop->pos.f[0], prop->pos.f[2], playerprop->pos.f[0], playerprop->pos.f[2],
                                     CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER,
                                     100.0f, 100.0f, 0.0f, 1.0f))
        {
            seen = true;
        }

        playerSetMoving(playerprop, 1);
    }

    return seen;
}


/* Turn the head towards the sweep end, speeding up and then slowing down to stop on it.
   At the end it turns back. */
static void cctvSweep(CCTVRecord *cctv)
{
    f32 target = cctv->towardsYaw1 ? cctv->sweepYaw1 : cctv->sweepYaw2;
    f32 newspeed;

    if (cctv->yaw < target)
    {
        if ((target - (((cctv->yawSpeed * cctv->yawSpeed) * 0.5f) / CCTV_YAW_ACCEL)) <= cctv->yaw)
        {
            // Close enough to start slowing down
            cctv->yawSpeed = (f32)(cctv->yawSpeed - (CCTV_YAW_ACCEL * g_GlobalTimerDelta));

            if (cctv->yawSpeed < CCTV_YAW_ACCEL)
            {
                cctv->yawSpeed = CCTV_YAW_ACCEL;
            }
        }
        else if (cctv->yawSpeed < cctv->maxYawSpeed)
        {
            newspeed = cctv->yawSpeed + (CCTV_YAW_ACCEL * g_GlobalTimerDelta);

            if (cctv->maxYawSpeed < newspeed)
            {
                newspeed = cctv->maxYawSpeed;
            }

            if (cctv->yaw < (target - (((newspeed * newspeed) * 0.5f) / CCTV_YAW_ACCEL)))
            {
                cctv->yawSpeed = newspeed;
            }
        }

        cctv->yaw += cctv->yawSpeed * g_GlobalTimerDelta;

        if (target <= cctv->yaw)
        {
            cctv->yaw = target;
            cctv->yawSpeed = 0.0f;
            cctv->towardsYaw1 = 0;
        }
    }
    else
    {
        if (cctv->yaw <= (target + (((cctv->yawSpeed * cctv->yawSpeed) * 0.5f) / CCTV_YAW_ACCEL)))
        {
            cctv->yawSpeed = (f32)(cctv->yawSpeed - (CCTV_YAW_ACCEL * g_GlobalTimerDelta));

            if (cctv->yawSpeed < CCTV_YAW_ACCEL)
            {
                cctv->yawSpeed = CCTV_YAW_ACCEL;
            }
        }
        else if (cctv->yawSpeed < cctv->maxYawSpeed)
        {
            newspeed = cctv->yawSpeed + (CCTV_YAW_ACCEL * g_GlobalTimerDelta);

            if (cctv->maxYawSpeed < newspeed)
            {
                newspeed = cctv->maxYawSpeed;
            }

            if (cctv->yaw > (target + (((newspeed * newspeed) * 0.5f) / CCTV_YAW_ACCEL)))
            {
                cctv->yawSpeed = newspeed;
            }
        }

        cctv->yaw -= cctv->yawSpeed * g_GlobalTimerDelta;

        if (cctv->yaw <= target)
        {
            cctv->yaw = target;
            cctv->yawSpeed = 0.0f;
            cctv->towardsYaw1 = 1;
        }
    }
}


/**
 * Watch for players, setting off the alarm after seeing one for long enough, and sweep
 * the head. Runs once a frame.
 */
void cctvTick(PropRecord *prop)
{
    CCTVRecord *cctv = (CCTVRecord *)prop->obj;
    // Co-op: the camera watches every player it may, and an alarm needs it to see any of
    // them for long enough
    s32 player = g_CoopMode ? 0 : playerGetCurrentNum();
    s32 end = g_CoopMode ? getPlayerCount() : (player + 1);
    bool seen = false;

    for (; player < end; player++)
    {
        PropRecord *playerprop;

        if (g_CoopMode && (!objMayWatchPlayer(player) || g_playerPointers[player]->prop == NULL))
        {
            continue;
        }

        playerprop = g_CoopMode ? g_playerPointers[player]->prop : currentPlayerGetProp();

        if (cctvCanSeePlayer(prop, cctv, playerprop))
        {
            seen = true;
        }
    }

    if (seen)
    {
        cctv->timer += g_ClockTimer;

        if (cctv->timer >= (s32)(CCTV_ALARM_FRAMES * g_CctvAlarmTimeScale))
        {
            alarmActivate();
            cctv->timer = 0;
        }
    }

    cctvSweep(cctv);
}


/* The head's matrix (mtxs[1]): turned by the sweep, at the head node */
void cctvGetModelMatrices(PropRecord *prop, Model *model, Mtxf *objmtx, Mtxf *mtxs)
{
    CCTVRecord *cctv = (CCTVRecord *)prop->obj;
    coord3d *nodepos = (coord3d *)model->obj->Switches[0]->Data;
    coord3d headpos;
    Mtxf yawmtx;

    mtx4LoadYRotation(cctvGetHeadYaw(cctv), &yawmtx);
    mtx4Mult(&yawmtx, &cctv->headMtx, &mtxs[1]);

    headpos.f[0] = nodepos->f[0];
    headpos.f[1] = nodepos->f[1];
    headpos.f[2] = nodepos->f[2];
    mtx4TransformVecInPlace(objmtx, &headpos);

    mtx4SetTranslation(&headpos, &mtxs[1]);
    mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), &mtxs[1]);
}
