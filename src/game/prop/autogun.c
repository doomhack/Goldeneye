/**
 * Autoguns (drone guns): ceiling and wall guns that track a player and fire.
 *
 * Each frame the gun looks for its target: in range (aimdist), within 70 degrees of where
 * it points (or anywhere once it is alerted or damaged), within its turn limits either
 * side of its home yaw, and in sight. It turns towards the target, sways a little while
 * firing, and fires while it is aimed close enough, and for 2 seconds after losing sight.
 * Its barrels spin up while firing and run down after. A gun that wanders instead
 * (AUTOGUN_FLAG2_WANDER) drifts slowly between random home angles.
 *
 * Damage builds up while the gun is aimed right at the player, and a hit lands each time
 * it reaches 1, scaled down beyond 2 m. Every fourth shot draws a tracer.
 *
 * A destroyed gun (PROPFLAG_AUTOGUN_DISABLED) stops. In co-op the gun keeps one target,
 * the nearest player it may watch.
 */

#include <ultra64.h>
#include <math.h>
#include <random.h>
#include <music.h>
#include <snd.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/autogun.h"
#include "game/prop/cctv.h"
#include "game/prop/propobj.h"
#include "game/audio/sfxvolume.h"
#include "game/chr/chraction.h"
#include "game/hud/sparks.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/math/physics.h"
#include "game/math/trig.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/player/gun.h"
#include "game/player/player.h"

/* A gun with this in flags2 wanders between random home angles instead of aiming; with
   PROPFLAG2_40000000 too, it stays still */
#define AUTOGUN_FLAG2_WANDER PROPFLAG2_DOOR_ALTCOORDSYSTEM

/* Turning speeds, in radians per frame (and per frame per frame) */
#if defined(VERSION_EU)
#define AUTOGUN_WANDER_YAW_MAX_SPEED   0.0008377581f
#define AUTOGUN_WANDER_YAW_ACCEL       0.0000139626345f
#define AUTOGUN_WANDER_PITCH_ACCEL     0.0000069813173f
#define AUTOGUN_WANDER_PITCH_MAX_SPEED 0.00041887906f
#define AUTOGUN_AIM_ACCEL              0.0010471976f
#define AUTOGUN_TRACKING_FRAMES        100
#else
#define AUTOGUN_WANDER_YAW_MAX_SPEED   0.00069813174f   // 2.4 degrees a second
#define AUTOGUN_WANDER_YAW_ACCEL       0.000011635529f
#define AUTOGUN_WANDER_PITCH_ACCEL     0.0000058177643f
#define AUTOGUN_WANDER_PITCH_MAX_SPEED 0.00034906587f   // 1.2 degrees a second
#define AUTOGUN_AIM_ACCEL              0.0008726647f
#define AUTOGUN_TRACKING_FRAMES        120              // 2 seconds
#endif

/* The barrels spin up by about 34 degrees a second each frame, to at most 5.7 turns a
   second */
#define AUTOGUN_SPIN_ACCEL     0.009973311f
#define AUTOGUN_SPIN_MAX_SPEED 0.5983986f

/* The model switches: the gun's yaw and pitch joints, the barrels, and the muzzle
   flashes (one for each barrel; the second is optional) */
#define AUTOGUN_SWITCH_YAW      1
#define AUTOGUN_SWITCH_PITCH    2
#define AUTOGUN_SWITCH_BARREL1  3
#define AUTOGUN_SWITCH_SIGHT    4
#define AUTOGUN_SWITCH_FLASH1   5
#define AUTOGUN_SWITCH_BARREL2  6
#define AUTOGUN_SWITCH_FLASH2   7

/* Set by difficulty: how fast damage builds up, how much each hit does, and how much
   damage the gun itself takes */
f32 g_AutogunPendingDamageTick = 1.0;
f32 g_AutogunDamageScalar = 1.0;
f32 g_AutogunDamageScale = 1.0;


/**
 * Co-op: the player the gun aims at and shoots this frame, or -1 to use the current
 * player (outside co-op, or with nobody to watch). Objects only tick on one player's pass
 * a frame, and which player that is changes from frame to frame, so the gun takes the
 * nearest player it may watch instead and keeps one target.
 */
static s32 autogunGetTargetPlayer(PropRecord *prop)
{
    s32 best = -1;
    f32 bestdist = 0.0f;

    if (!g_CoopMode)
    {
        return -1;
    }

    for (s32 i = 0; i < getPlayerCount(); i++)
    {
        PropRecord *playerprop = g_playerPointers[i]->prop;
        f32 x, y, z, dist;

        if (playerprop == NULL || !objMayWatchPlayer(i))
        {
            continue;
        }

        x = playerprop->pos.f[0] - prop->pos.f[0];
        y = playerprop->pos.f[1] - prop->pos.f[1];
        z = playerprop->pos.f[2] - prop->pos.f[2];
        dist = (x * x) + (y * y) + (z * z);

        if (best < 0 || dist < bestdist)
        {
            best = i;
            bestdist = dist;
        }
    }

    return best;
}


static PropRecord *autogunGetTargetProp(s32 target)
{
    return (target >= 0) ? g_playerPointers[target]->prop : currentPlayerGetProp();
}


/* Bring an angle into -pi to pi, for a difference that is in 0 to 2 pi or a little
   outside */
static f32 angleWrapSigned(f32 angle)
{
    if (angle < 0.0f)
    {
        angle += M_TAU_F;
    }

    if (angle > M_PI_F)
    {
        angle -= M_TAU_F;
    }

    return angle;
}


/* Bring an angle into -pi to pi, for an angle that is at most a turn out */
static f32 angleWrapFromHome(f32 angle)
{
    if (angle < -M_PI_F)
    {
        angle += M_TAU_F;
    }
    else if (angle >= M_PI_F)
    {
        angle -= M_TAU_F;
    }

    return angle;
}


static f32 angleWrapPositive(f32 angle)
{
    if (angle < 0.0f)
    {
        angle += M_TAU_F;
    }

    if (angle >= M_TAU_F)
    {
        angle -= M_TAU_F;
    }

    return angle;
}


/* Drift slowly to a random home angle, then pick another */
static void autogunWander(AutogunRecord *autogun)
{
    if (autogun->flags2 & PROPFLAG2_40000000)
    {
        autogun->homePitch = autogun->pitch;
        autogun->homeYaw = autogun->yaw;
    }
    else if (autogun->yaw == autogun->homeYaw && autogun->pitch == autogun->homePitch)
    {
        // Pitch 1 to 40 degrees
        autogun->homePitch = (((RANDOMFRAC() * 39.0f) + 1.0f) * M_TAU_F) / 360.0f;
        autogun->homeYaw = RANDOMFRAC() * M_TAU_F;
    }

    physTurnTowards(&autogun->yaw, autogun->homeYaw, &autogun->yawSpeed, AUTOGUN_WANDER_YAW_ACCEL,
                    AUTOGUN_WANDER_YAW_ACCEL, AUTOGUN_WANDER_YAW_MAX_SPEED, g_ClockTimer);
    physTurnTowards(&autogun->pitch, autogun->homePitch, &autogun->pitchSpeed, AUTOGUN_WANDER_PITCH_ACCEL,
                    AUTOGUN_WANDER_PITCH_ACCEL, AUTOGUN_WANDER_PITCH_MAX_SPEED, g_ClockTimer);
}


static bool autogunRecentlySawTarget(AutogunRecord *autogun)
{
    return autogun->lastSeenTime >= 0 && (g_GlobalTimer - AUTOGUN_TRACKING_FRAMES) < autogun->lastSeenTime;
}


/* Spin the barrels up while firing, and let them run down after */
static void autogunSpinBarrels(AutogunRecord *autogun, bool firing)
{
    if (firing)
    {
        autogun->barrelSpinSpeed += AUTOGUN_SPIN_ACCEL * g_GlobalTimerDelta;

        if (autogun->barrelSpinSpeed > AUTOGUN_SPIN_MAX_SPEED)
        {
            autogun->barrelSpinSpeed = AUTOGUN_SPIN_MAX_SPEED;
        }
    }
    else if (autogun->barrelSpinSpeed > 0.0f)
    {
        // About 45% of the spin is lost each second
        for (s32 i = 0; i < g_ClockTimer; i++)
        {
            autogun->barrelSpinSpeed *= 0.99f;
        }

        if (autogun->barrelSpinSpeed <= 0.0001f)
        {
            autogun->barrelSpinSpeed = 0.0f;
        }
    }

    if (autogun->barrelSpinSpeed > 0.0f)
    {
        autogun->barrelSpinAngle += autogun->barrelSpinSpeed * g_GlobalTimerDelta;

        while (autogun->barrelSpinAngle >= M_TAU_F)
        {
            autogun->barrelSpinAngle -= M_TAU_F;
        }
    }
}


/**
 * Look for the target and turn towards it, deciding whether to fire (is_active). Runs
 * once a frame.
 */
void autogunTick(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    AutogunRecord *autogun = (AutogunRecord *)obj;
    PropRecord *targetprop = autogunGetTargetProp(autogunGetTargetPlayer(prop));
    bool seen = false;
    bool insight = false;
    bool firing = false;
    f32 xdiff, zdiff, ydiff;
    f32 hdistsq, hdist, dist;
    f32 aimlimit, aimyaw, aimpitch;
    f32 fromhome, yawtogo, pitchtogo;

    if (obj->flags2 & AUTOGUN_FLAG2_WANDER)
    {
        autogunWander(autogun);
        return;
    }

    // Aim 20 units below the player's eyes
    xdiff = targetprop->pos.f[0] - obj->runtime_pos.f[0];
    ydiff = (targetprop->pos.f[1] - obj->runtime_pos.f[1]) - 20.0f;
    zdiff = targetprop->pos.f[2] - obj->runtime_pos.f[2];
    hdistsq = (xdiff * xdiff) + (zdiff * zdiff);
    hdist = sqrtf(hdistsq);
    dist = hdist;

    // Guns flagged PROPFLAG_DOOR_TWOWAY measure their range in 3D
    if (obj->flags & PROPFLAG_DOOR_TWOWAY)
    {
        hdistsq += ydiff * ydiff;
        dist = sqrtf(hdistsq);
    }

    aimlimit = chrGetAimLimitAngle(hdistsq);
    aimyaw = autogun->homeYaw;
    aimpitch = autogun->homePitch;

    if (dist <= autogun->aimdist)
    {
        f32 targetyaw = mathAtan2(xdiff, zdiff);
        f32 targetpitch = mathAtan2(ydiff, hdist);

        // Once damaged or alerted the gun sees all round
        if ((obj->flags & PROPFLAG_NO_AMMO) || (obj->flags & PROPFLAG_INMOTION))
        {
            seen = true;
        }
        else
        {
            f32 yawtotarget = angleWrapSigned(targetyaw - autogun->yaw);

            if (yawtotarget < DegToRad(70) && yawtotarget > DegToRad(-70))
            {
                seen = true;
            }
        }

        if (seen)
        {
            StandTile *tile = prop->stan;

            fromhome = angleWrapFromHome(targetyaw - autogun->homeYaw);

            playerSetMoving(targetprop, 0);

            if (fromhome <= autogun->maxYaw && autogun->minYaw <= fromhome
                    && stanTestLineUnobstructed(&tile, prop->pos.f[0], prop->pos.f[2], targetprop->pos.f[0], targetprop->pos.f[2],
                                                CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER,
                                                prop->pos.f[1], prop->pos.f[1], targetprop->pos.f[1], targetprop->pos.f[1])
                    && tile == targetprop->stan)
            {
                obj->flags |= PROPFLAG_INMOTION;
                insight = true;
                aimyaw = targetyaw;
                aimpitch = targetpitch;
            }
            else if (autogunRecentlySawTarget(autogun))
            {
                // Hold still for a while where it lost the target
                aimyaw = autogun->yaw;
                aimpitch = autogun->pitch;
            }
            else
            {
                seen = false;
            }

            playerSetMoving(targetprop, 1);
        }
    }

    if (seen)
    {
        aimlimit = chrGetAimLimitAngle(hdistsq);
    }

    // Sway from side to side once every 2 seconds while firing
    if (autogun->is_active)
    {
        aimyaw += (aimlimit * 0.8f) * sinf((((f32)(((s32)g_GlobalTimer) % AUTOGUN_TRACKING_FRAMES)) * M_TAU_F) / (f32)AUTOGUN_TRACKING_FRAMES);
        aimyaw = angleWrapPositive(aimyaw);
    }

    // Keep within the turn limits
    fromhome = angleWrapFromHome(aimyaw - autogun->homeYaw);

    if (autogun->maxYaw < fromhome)
    {
        aimyaw = autogun->homeYaw + autogun->maxYaw;
    }
    else if (fromhome < autogun->minYaw)
    {
        aimyaw = autogun->homeYaw + autogun->minYaw;
    }

    aimyaw = angleWrapPositive(aimyaw);

    physTurnTowards(&autogun->yaw, aimyaw, &autogun->yawSpeed, AUTOGUN_AIM_ACCEL, AUTOGUN_AIM_ACCEL, autogun->speed, g_ClockTimer);
    physTurnTowards(&autogun->pitch, aimpitch, &autogun->pitchSpeed, AUTOGUN_AIM_ACCEL, AUTOGUN_AIM_ACCEL, autogun->speed, g_ClockTimer);

    yawtogo = angleWrapSigned(aimyaw - autogun->yaw);
    pitchtogo = angleWrapSigned(aimpitch - autogun->pitch);

    // Fire while aimed close to the target (it only counts as on target within the aim
    // limit), and for a while after losing sight of it
    autogun->is_active = 0;

    if (seen)
    {
        f32 widelimit = 2.0f * aimlimit;

        if (yawtogo < aimlimit && -aimlimit < yawtogo && pitchtogo < aimlimit && -aimlimit < pitchtogo)
        {
            autogun->is_active = 1;
            firing = true;

            if (insight)
            {
                autogun->lastSeenTime = (s32)g_GlobalTimer;
                autogun->onTargetTime = (s32)g_GlobalTimer;
            }
        }
        else if (yawtogo < widelimit && -widelimit < yawtogo && pitchtogo < widelimit && -widelimit < pitchtogo)
        {
            autogun->is_active = 1;
            firing = true;

            if (insight)
            {
                autogun->lastSeenTime = (s32)g_GlobalTimer;
            }
        }
        else if (autogunRecentlySawTarget(autogun))
        {
            autogun->is_active = 1;
            firing = true;
        }
    }

    autogunSpinBarrels(autogun, firing);
}


/* Where a shot leaves the gun: the muzzle flash node if the gun is drawn and the node can
   be reached, else the gun's position */
static void autogunGetShotOrigin(PropRecord *prop, Model *model, s32 flashnode, StandTile **tile, coord3d *from)
{
    if ((prop->flags & PROPFLAG_ONSCREEN) && model->obj->Switches[flashnode] != NULL)
    {
        Mtxf *nodemtx = modelFindNodeMtx(model, model->obj->Switches[flashnode], 0);
        coord3d *nodepos = &model->obj->Switches[flashnode]->Data->Group.Origin;
        Mtxf worldmtx;

        from->f[0] = nodepos->f[0];
        from->f[1] = nodepos->f[1];
        from->f[2] = nodepos->f[2];
        mtx4MultAffine(camGetViewToWorldMtxf(), nodemtx, &worldmtx);
        mtx4TransformVecInPlace(&worldmtx, from);

        if (stanWalkLine(tile, prop->pos.f[0], prop->pos.f[2], from->f[0], from->f[2]))
        {
            return;
        }
    }

    from->f[0] = prop->pos.f[0];
    from->f[1] = prop->pos.f[1];
    from->f[2] = prop->pos.f[2];
}


/* Start a tracer from the gun to where the shot ended */
static void autogunCreateBeam(BeamRecord *beam, coord3d *from, coord3d *to)
{
    f32 dist;
    f32 scale;

    beam->from.f[0] = from->f[0];
    beam->from.f[1] = from->f[1];
    beam->from.f[2] = from->f[2];
    beam->dir.f[0] = to->f[0] - beam->from.f[0];
    beam->dir.f[1] = to->f[1] - beam->from.f[1];
    beam->dir.f[2] = to->f[2] - beam->from.f[2];

    dist = sqrtf(((beam->dir.f[0] * beam->dir.f[0]) + (beam->dir.f[1] * beam->dir.f[1])) + (beam->dir.f[2] * beam->dir.f[2]));
    scale = 1.0f / dist;
    beam->dir.f[0] = (f32)(beam->dir.f[0] * scale);
    beam->dir.f[1] = (f32)(beam->dir.f[1] * scale);
    beam->dir.f[2] = (f32)(beam->dir.f[2] * scale);

    if (dist > 10000.0f)
    {
        dist = 10000.0f;
    }

    beam->age = 0;
    beam->weaponnum = ITEM_FNP90;
    beam->maxdist = dist;

    if (dist < 500.0f)
    {
        dist = 500.0f;
    }

    // (The laser branch is never used, as the beam is always the RC-P90's)
    if (beam->weaponnum == ITEM_LASER)
    {
        beam->speed = 0.25f * dist;
        beam->length = 0.6f * dist;

        if (beam->length > 3000.0f)
        {
            beam->length = 3000.0f;
        }

        beam->dist = ((-0.1f) - (RANDOMFRAC() * 0.3f)) * dist;
    }
    else
    {
        beam->speed = 0.2f * dist;
        beam->length = 0.2f * dist;

        if (beam->length > 3000.0f)
        {
            beam->length = 3000.0f;
        }

        beam->dist = ((2.0f * RANDOMFRAC()) - 1.0f) * beam->speed;
    }
}


/* Fire one shot: it hits the target if the gun was aimed right at them this frame and
   nothing was in the way, else it hits the wall */
static void autogunFireShot(PropRecord *prop, Model *model, AutogunRecord *autogun)
{
    bool hitwall = true;
    StandTile *hittile = NULL;
    StandTile *tile = prop->stan;
    bool showbeam = (autogun->numShots & 3) == 0;
    // Co-op: the shots are for the gun's target, wherever this pass's player is
    s32 target = autogunGetTargetPlayer(prop);
    PropRecord *targetprop = autogunGetTargetProp(target);
    s32 flashnode = AUTOGUN_SWITCH_FLASH1;
    s32 prevplayer;
    coord3d from, dir, to;

    if (model->obj->Switches[AUTOGUN_SWITCH_FLASH2] != 0 && !(autogun->numShots & 7))
    {
        flashnode = AUTOGUN_SWITCH_FLASH2;
    }

    autogunGetShotOrigin(prop, model, flashnode, &tile, &from);

    dir.f[0] = cosf(autogun->pitch) * sinf(autogun->yaw);
    dir.f[1] = sinf(autogun->pitch);
    dir.f[2] = cosf(autogun->pitch) * cosf(autogun->yaw);
    to.f[0] = from.f[0] + (dir.f[0] * 65536.0f);
    to.f[1] = from.f[1] + (dir.f[1] * 65536.0f);
    to.f[2] = from.f[2] + (dir.f[2] * 65536.0f);

    stanResetHits();

    if (!stanTestLineUnobstructed(&tile, from.f[0], from.f[2], to.f[0], to.f[2], CDTYPE_DOORS, 100.0f, 100.0f, 0.0f, 1.0f))
    {
        // Stop just short of the wall
        chrStanLineDirIntersection(&from, &dir, &to);
        hittile = tile;
        to.f[0] -= 26.0f * dir.f[0];
        to.f[1] -= 26.0f * dir.f[1];
        to.f[2] -= 26.0f * dir.f[2];
    }

    prevplayer = playerGetCurrentNum();

    if (target >= 0)
    {
        playerSetCurrent(target);
    }

    if (g_GlobalTimer == (s32)autogun->onTargetTime)
    {
        f32 xdiff = targetprop->pos.f[0] - from.f[0];
        f32 ydiff = targetprop->pos.f[1] - from.f[1];
        f32 zdiff = targetprop->pos.f[2] - from.f[2];
        f32 targetdistsq = ((xdiff * xdiff) + (ydiff * ydiff)) + (zdiff * zdiff);

        xdiff = to.f[0] - from.f[0];
        ydiff = to.f[1] - from.f[1];
        zdiff = to.f[2] - from.f[2];

        // The target is nearer than the wall
        if (targetdistsq <= (((xdiff * xdiff) + (ydiff * ydiff)) + (zdiff * zdiff)) && !playerIsShowingDamage())
        {
            f32 targetdist = sqrtf(targetdistsq);
            f32 damage = (0.16f * g_GlobalTimerDelta) * g_AutogunPendingDamageTick;

            if (targetdist > 200.0f)
            {
                damage *= 200.0f / targetdist;
            }

            autogun->damageAccum += damage;

            if (autogun->damageAccum >= 1.0f)
            {
                playerTakeDamageFromAngle((weaponGetDamage(ITEM_FNP90) * 0.125f) * g_AutogunDamageScalar, autogun->yaw, -1, 1);
                autogun->damageAccum = 0.0f;

                if (playerIsShowingDamage())
                {
                    hitwall = false;
                }
            }
        }
    }

    if (target >= 0)
    {
        playerSetCurrent(prevplayer);
    }

    if (hitwall)
    {
        if (hittile != NULL)
        {
            sparkCreate(&to, 1, 26.0f, (s16)hittile->room);
        }

        shotPlayHitSound(ITEM_FNP90, &to, -1);
    }
    else
    {
        to.f[0] = targetprop->pos.f[0];
        to.f[1] = targetprop->pos.f[1];
        to.f[2] = targetprop->pos.f[2];
        shotPlayHitPropSound(ITEM_FNP90, targetprop, -1);
    }

    if (showbeam)
    {
        autogunCreateBeam(autogun->beam, &from, &to);
    }
}


/* Play the firing sound, alternating between two sound slots, at most every 2 frames */
static void autogunPlayFireSound(PropRecord *prop, AutogunRecord *autogun)
{
    if (autogun->nextSoundTime >= g_GlobalTimer)
    {
        return;
    }

    if (autogun->fireSound1 != NULL && sndGetPlayingState(autogun->fireSound1))
    {
        sndDeactivate(autogun->fireSound1);
    }

    if (autogun->fireSound2 != NULL && sndGetPlayingState(autogun->fireSound2))
    {
        sndDeactivate(autogun->fireSound2);
    }

    if (autogun->fireSound1 == NULL)
    {
        sndPlaySfx(g_musicSfxBufferPtr, GUN_B9_CANNON_SHORT_SFX, &autogun->fireSound1);
        sfxSetVolumeAt(autogun->fireSound1, &prop->pos);
    }
    else if (autogun->fireSound2 == NULL)
    {
        sndPlaySfx(g_musicSfxBufferPtr, GUN_B9_CANNON_SHORT_SFX, &autogun->fireSound2);
        sfxSetVolumeAt(autogun->fireSound2, &prop->pos);
    }

    autogun->nextSoundTime = (s32)(g_GlobalTimer + 2);
}


/**
 * Fire if the gun is active: one shot a frame, alternating barrels, and show the muzzle
 * flashes. Runs once a frame, after the model's matrices are worked out.
 */
void autogunTickFire(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    AutogunRecord *autogun = (AutogunRecord *)obj;
    Model *model = obj->model;
    bool flash1 = false;
    bool flash2 = false;

    if (autogun->is_active && !(obj->flags & PROPFLAG_AUTOGUN_DISABLED))
    {
        autogun->numShots = autogun->numShots + 1;
        flash1 = (autogun->numShots & 1) == 0;

        if (model->obj->Switches[AUTOGUN_SWITCH_FLASH1] != 0)
        {
            flash2 = (autogun->numShots & 1) == 1;
        }

        autogunPlayFireSound(prop, autogun);

        if (flash1 || flash2)
        {
            autogunFireShot(prop, model, autogun);
        }
    }

    if (model->obj->Switches[AUTOGUN_SWITCH_FLASH1] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[AUTOGUN_SWITCH_FLASH1])->Gunfire.visible = (s16)flash1;
    }

    if (model->obj->Switches[AUTOGUN_SWITCH_FLASH2] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[AUTOGUN_SWITCH_FLASH2])->Gunfire.visible = (s16)flash2;
    }
}


/**
 * The gun's matrices: mtxs[1] turned by its yaw at the yaw joint, mtxs[2] by its pitch,
 * and the barrels spinning.
 */
void autogunGetModelMatrices(PropRecord *prop, Model *model, Mtxf *objmtx, Mtxf *mtxs)
{
    AutogunRecord *autogun = (AutogunRecord *)prop->obj;
    f32 gunyaw = autogun->yaw + M_PI_2F;
    f32 gunpitch = -autogun->pitch;
    coord3d *nodepos;
    coord3d gunpos;
    Mtxf *nodemtx;

    if (gunyaw >= M_TAU_F)
    {
        gunyaw -= M_TAU_F;
    }

    nodepos = &model->obj->Switches[AUTOGUN_SWITCH_YAW]->Data->Group.Origin;
    gunpos.f[0] = nodepos->f[0];
    gunpos.f[1] = nodepos->f[1];
    gunpos.f[2] = nodepos->f[2];

    mtx4TransformVecInPlace(objmtx, &gunpos);
    mtx4LoadYRotation(gunyaw, &mtxs[1]);
    mtx4SetTranslation(&gunpos, &mtxs[1]);
    mtx4ScaleRows012(autogun->model->scale, &mtxs[1]);
    mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), &mtxs[1]);

    nodepos = (coord3d *)model->obj->Switches[AUTOGUN_SWITCH_PITCH]->Data;
    mtx4LoadZRotation(gunpitch, &mtxs[2]);
    mtx4SetTranslation(nodepos, &mtxs[2]);
    mtx4MultAffineInPlace(&mtxs[1], &mtxs[2]);

    if (model->obj->Switches[AUTOGUN_SWITCH_BARREL1] != NULL)
    {
        nodemtx = modelFindNodeMtx(model, model->obj->Switches[AUTOGUN_SWITCH_BARREL1], 0);
        nodepos = (coord3d *)model->obj->Switches[AUTOGUN_SWITCH_BARREL1]->Data;
        mtx4LoadXRotation(autogun->barrelSpinAngle, nodemtx);
        mtx4SetTranslation(nodepos, nodemtx);
        mtx4MultAffineInPlace(&mtxs[2], nodemtx);
    }

    if (model->obj->Switches[AUTOGUN_SWITCH_SIGHT] != NULL)
    {
        nodemtx = modelFindNodeMtx(model, model->obj->Switches[AUTOGUN_SWITCH_SIGHT], 0);
        nodepos = (coord3d *)model->obj->Switches[AUTOGUN_SWITCH_SIGHT]->Data;
        mtx4LoadTranslation(nodepos, nodemtx);
        mtx4MultAffineInPlace(&mtxs[2], nodemtx);
    }

    if (model->obj->Switches[AUTOGUN_SWITCH_BARREL2] != NULL)
    {
        nodemtx = modelFindNodeMtx(model, model->obj->Switches[AUTOGUN_SWITCH_BARREL2], 0);
        nodepos = (coord3d *)model->obj->Switches[AUTOGUN_SWITCH_BARREL2]->Data;
        mtx4LoadXRotation(autogun->barrelSpinAngle, nodemtx);
        mtx4SetTranslation(nodepos, nodemtx);
        mtx4MultAffineInPlace(&mtxs[2], nodemtx);
    }
}
