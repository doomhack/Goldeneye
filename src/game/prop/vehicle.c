/**
 * Vehicles, aircraft and the tank's model.
 *
 * A vehicle (a truck) drives along a path set by its AI
 * script: it turns towards the next waypoint, moves forward along its heading and tilts
 * to fit the floor under its four wheels. It stops if anything is in the way.
 *
 * An aircraft (helicopters, the plane) is moved by an animation, or not at all, and its
 * rotors spin at a speed its AI script sets.
 *
 * The tank is driven by the player (player code); here are only its model's matrices.
 * Engine sounds play while a vehicle or aircraft is moving and near.
 */

#include <ultra64.h>
#include <math.h>
#include <boss.h>
#include <music.h>
#include <snd.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/vehicle.h"
#include "game/prop/mine.h"
#include "game/prop/objdamage.h"
#include "game/prop/propobj.h"
#include "game/prop/prop.h"
#include "game/prop/setup.h"
#include "game/audio/sfxvolume.h"
#include "game/chr/chraction.h"
#include "game/chr/chrai.h"
#include "game/chr/chraicoop.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/math/physics.h"
#include "game/math/trig.h"
#include "game/model/anim.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/player/player.h"

/* How fast vehicles turn, in radians per frame (and per frame per frame) */
#if defined(VERSION_EU)
#define VEHICLE_TURN_ACCEL     0.0001308997f
#define VEHICLE_TURN_DECEL     0.0002617994f
#define VEHICLE_TURN_MAX_SPEED 0.007853982f
#else
#define VEHICLE_TURN_ACCEL     0.000109083085f
#define VEHICLE_TURN_DECEL     0.00021816617f
#define VEHICLE_TURN_MAX_SPEED 0.006544985f    // 22.5 degrees a second
#endif

/* The vehicle model's switches: the four wheels (front left and right, back left and
   right), the wheel size and the body's collision box */
#define VEHICLE_SWITCH_WHEEL1   1
#define VEHICLE_SWITCH_WHEEL2   2
#define VEHICLE_SWITCH_WHEEL3   3
#define VEHICLE_SWITCH_WHEEL4   4
#define VEHICLE_SWITCH_WHEELBOX 6
#define VEHICLE_SWITCH_BODYBOX  10

#define VEHICLE_COLLIDE_TYPES (CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER)


/* Ease a speed towards its aim over the time left */
static void speedTowardsAim(f32 *speed, f32 aim, f32 *timeleft)
{
    if (*timeleft >= 0.0f)
    {
        if (*timeleft <= g_GlobalTimerDelta)
        {
            *speed = aim;
        }
        else
        {
            *speed += ((aim - *speed) * g_GlobalTimerDelta) / *timeleft;
        }

        *timeleft -= g_GlobalTimerDelta;
    }
}


/* Play an engine sound at the given volume, or stop it at 0 */
static void objUpdateEngineSound(struct ALSoundState_s **sound, s32 sfx, s32 volume)
{
    if (volume > 0)
    {
        if ((*sound == NULL || sndGetPlayingState(*sound) == 0) && !lvGetControlsLocked())
        {
            sndPlaySfx(g_musicSfxBufferPtr, sfx, sound);
        }

        if (*sound != NULL)
        {
            sndCreatePostEvent(*sound, 8, volume);
        }
    }
    else if (*sound != NULL && sndGetPlayingState(*sound) != 0)
    {
        sndDeactivate(*sound);
    }
}


/* Find where a wheel touches the floor, walking from the vehicle's position to it.
   Returns FALSE if the wheel is off the floor. */
static bool vehicleFindWheelFloor(PropRecord *prop, coord3d *point)
{
    StandTile *tile = prop->stan;

    if (stanWalkLine(&tile, prop->pos.x, prop->pos.z, point->x, point->z))
    {
        point->y = stanGetFloorY(tile, point->x, point->z);
        return TRUE;
    }

    return FALSE;
}


static void edgeNormalize(coord3d *edge, coord3d *from, coord3d *to)
{
    edge->x = to->x - from->x;
    edge->y = to->y - from->y;
    edge->z = to->z - from->z;
    guNormalize(&edge->x, &edge->y, &edge->z);
}


/**
 * Tilt the vehicle to fit the floor under its four wheels, and set its height. If a
 * wheel is off the floor it stays level at the floor height under its middle. Returns
 * TRUE if all four wheels were on the floor.
 */
bool vehicleAlignToFloor(VehichleRecord *vehicle)
{
    ObjectRecord *obj = (ObjectRecord *)vehicle;
    Model *model = obj->model;
    PropRecord *prop = obj->prop;
    coord3d *wheel1 = (coord3d *)model->obj->Switches[VEHICLE_SWITCH_WHEEL1]->Data;
    coord3d *wheel2 = (coord3d *)model->obj->Switches[VEHICLE_SWITCH_WHEEL2]->Data;
    coord3d *wheel3 = (coord3d *)model->obj->Switches[VEHICLE_SWITCH_WHEEL3]->Data;
    coord3d *wheel4 = (coord3d *)model->obj->Switches[VEHICLE_SWITCH_WHEEL4]->Data;
    ModelRoData_BoundingBoxRecord *bbox = (ModelRoData_BoundingBoxRecord *)model->obj->Switches[VEHICLE_SWITCH_WHEELBOX]->Data;
    coord3d point0, point1, point2, point3;
    bool onfloor = TRUE;
    Mtxf mtx;
    f32 ypos;

    mtx4LoadYRotation(vehicle->roty, &mtx);
    mtx4ScaleRows012(model->scale, &mtx);
    mtx4SetTranslation(&obj->runtime_pos, &mtx);

    mtx4TransformVec(&mtx, wheel1, &point0);
    mtx4TransformVec(&mtx, wheel2, &point1);
    mtx4TransformVec(&mtx, wheel3, &point2);
    mtx4TransformVec(&mtx, wheel4, &point3);

    // (Each is tested, even after one fails)
    if (!vehicleFindWheelFloor(prop, &point0))
    {
        onfloor = FALSE;
    }

    if (!vehicleFindWheelFloor(prop, &point1))
    {
        onfloor = FALSE;
    }

    if (!vehicleFindWheelFloor(prop, &point2))
    {
        onfloor = FALSE;
    }

    if (!vehicleFindWheelFloor(prop, &point3))
    {
        onfloor = FALSE;
    }

    if (onfloor)
    {
        coord3d edge0, edge1, edge2, edge3;
        coord3d normal;
        coord3d yawvec;
        f32 cross0y, cross1y, cross2y, cross3y;
        Mtxf *m = &obj->mtx;

        // The edges around the four wheels: 1-2, 2-4, 4-3, 3-1
        edgeNormalize(&edge0, &point0, &point1);
        edgeNormalize(&edge1, &point1, &point3);
        edgeNormalize(&edge2, &point3, &point2);
        edgeNormalize(&edge3, &point2, &point0);

        // The floor's normal is the cross product at the corner whose normal points
        // most upwards
        cross0y = (edge0.z * edge1.x) - (edge1.z * edge0.x);
        cross1y = (edge1.z * edge2.x) - (edge2.z * edge1.x);
        cross2y = (edge2.z * edge3.x) - (edge3.z * edge2.x);
        cross3y = (edge3.z * edge0.x) - (edge0.z * edge3.x);

        if (cross1y <= cross0y && cross2y <= cross0y && cross3y <= cross0y)
        {
            normal.x = (edge0.y * edge1.z) - (edge1.y * edge0.z);
            normal.y = cross0y;
            normal.z = (edge0.x * edge1.y) - (edge1.x * edge0.y);
        }
        else if (cross2y <= cross1y && cross3y <= cross1y)
        {
            normal.x = (edge1.y * edge2.z) - (edge2.y * edge1.z);
            normal.y = cross1y;
            normal.z = (edge1.x * edge2.y) - (edge2.x * edge1.y);
        }
        else if (cross3y <= cross2y)
        {
            normal.x = (edge2.y * edge3.z) - (edge3.y * edge2.z);
            normal.y = cross2y;
            normal.z = (edge2.x * edge3.y) - (edge3.x * edge2.y);
        }
        else
        {
            normal.x = (edge3.y * edge0.z) - (edge0.y * edge3.z);
            normal.y = cross3y;
            normal.z = (edge3.x * edge0.y) - (edge0.x * edge3.y);
        }

        yawvec.x = sinf(vehicle->roty);
        yawvec.y = 0.0f;
        yawvec.z = cosf(vehicle->roty);

        // Up is the normal, sideways is up x forward, and forward is sideways x up
        mtx4LoadIdentity(m);
        m->m[1][0] = normal.x;
        m->m[1][1] = normal.y;
        m->m[1][2] = normal.z;

        m->m[0][0] = (m->m[1][1] * yawvec.f[2]) - (m->m[1][2] * yawvec.f[1]);
        m->m[0][1] = (m->m[1][2] * yawvec.f[0]) - (m->m[1][0] * yawvec.f[2]);
        m->m[0][2] = (m->m[1][0] * yawvec.f[1]) - (m->m[1][1] * yawvec.f[0]);

        m->m[2][0] = (m->m[0][1] * m->m[1][2]) - (m->m[0][2] * m->m[1][1]);
        m->m[2][1] = (m->m[0][2] * m->m[1][0]) - (m->m[0][0] * m->m[1][2]);
        m->m[2][2] = (m->m[0][0] * m->m[1][1]) - (m->m[0][1] * m->m[1][0]);

        mtx4ScaleRows012(model->scale, m);
        mtx4TransformVec(m, wheel1, &point1);

        // Sit the first wheel's bottom on the floor
        ypos = (point0.y - (bboxGetYmin(bbox) * model->scale)) - point1.y;
    }
    else
    {
        mtx4LoadYRotation(vehicle->roty, &obj->mtx);
        mtx4ScaleRows012(model->scale, &obj->mtx);

        ypos = stanGetFloorY(prop->stan, prop->pos.x, prop->pos.z) - ((bboxGetYmin(bbox) + wheel1->f[1]) * model->scale);
    }

    prop->pos.y = ypos;
    obj->runtime_pos.y = ypos;

    return onfloor;
}


/* Point the vehicle along its matrix's heading (after being placed by a script) */
static void vehicleResetHeading(VehichleRecord *vehicle)
{
    vehicle->roty = mathAtan2(vehicle->mtx.m[2][0], vehicle->mtx.m[2][2]);
    vehicle->flags &= ~PROPFLAG_INMOTION;
    vehicleAlignToFloor(vehicle);
}


/* Whether the body's collision box fits on the floor where the vehicle is: walking its
   outline must not leave the floor */
static bool vehicleBodyFitsFloor(PropRecord *prop, VehichleRecord *vehicle, Model *model)
{
    ModelRoData_BoundingBoxRecord *bbox = &model->obj->Switches[VEHICLE_SWITCH_BODYBOX]->Data->BoundingBox;
    coord3d xmin, xmax, zmin, zmax;
    f32 corners[4][2];
    StandTile *tile = prop->stan;

    xmin.f[0] = bbox->Bounds.xmin * vehicle->mtx.m[0][0];
    xmin.f[2] = bbox->Bounds.xmin * vehicle->mtx.m[0][2];
    xmax.f[0] = bbox->Bounds.xmax * vehicle->mtx.m[0][0];
    xmax.f[2] = bbox->Bounds.xmax * vehicle->mtx.m[0][2];
    zmin.f[0] = bbox->Bounds.zmin * vehicle->mtx.m[2][0];
    zmin.f[2] = bbox->Bounds.zmin * vehicle->mtx.m[2][2];
    zmax.f[0] = bbox->Bounds.zmax * vehicle->mtx.m[2][0];
    zmax.f[2] = bbox->Bounds.zmax * vehicle->mtx.m[2][2];

    corners[0][0] = (prop->pos.f[0] + xmin.f[0]) + zmin.f[0];
    corners[0][1] = (prop->pos.f[2] + xmin.f[2]) + zmin.f[2];
    corners[1][0] = (prop->pos.f[0] + xmax.f[0]) + zmin.f[0];
    corners[1][1] = (prop->pos.f[2] + xmax.f[2]) + zmin.f[2];
    corners[2][0] = (prop->pos.f[0] + xmax.f[0]) + zmax.f[0];
    corners[2][1] = (prop->pos.f[2] + xmax.f[2]) + zmax.f[2];
    corners[3][0] = (prop->pos.f[0] + xmin.f[0]) + zmax.f[0];
    corners[3][1] = (prop->pos.f[2] + xmin.f[2]) + zmax.f[2];

    if (!stanWalkLine(&tile, prop->pos.f[0], prop->pos.f[2], corners[0][0], corners[0][1]))
    {
        return FALSE;
    }

    for (s32 i = 0; i < 4; i++)
    {
        s32 next = (i + 1) % 4;

        if (!stanWalkLine(&tile, corners[i][0], corners[i][1], corners[next][0], corners[next][1]))
        {
            return FALSE;
        }
    }

    return TRUE;
}


/* Stop dead, and drive on at the same speed after a second */
static void vehicleStop(VehichleRecord *vehicle, f32 prevyaw, f32 prevturnspeed)
{
    if (vehicle->speedtime60 < 0.0f)
    {
        vehicle->speedaim = (f32)vehicle->speed;
        vehicle->speedtime60 = 60.0f;
    }

    vehicle->speed = 0.0f;
    vehicle->roty = prevyaw;
    vehicle->turnrot60 = prevturnspeed;
}


/* Turn towards the target yaw and move forward, stopping if anything is in the way.
   waypointpos is the waypoint it is heading to, or NULL. */
static void vehicleDrive(PropRecord *prop, VehichleRecord *vehicle, Model *model, f32 targetyaw, coord3d *waypointpos)
{
    ObjectRecord *obj = prop->obj;
    StandTile *tile = prop->stan;
    f32 prevyaw = vehicle->roty;
    f32 prevturnspeed = vehicle->turnrot60;
    f32 angularspeed = 0.0f;
    coord3d forward;
    coord3d newpos;
    coord3d *axlepos;
    f32 sideslip;

    if (waypointpos != NULL)
    {
        // Already heading straight for the waypoint
        forward.f[0] = sinf(vehicle->roty);
        forward.f[1] = 0.0f;
        forward.f[2] = cosf(vehicle->roty);

        if (rayPassesNearPoint2d(&vehicle->runtime_pos, &forward, waypointpos, 10.0f))
        {
            targetyaw = vehicle->roty;
        }
    }

    physTurnTowards(&vehicle->roty, targetyaw, &vehicle->turnrot60, VEHICLE_TURN_ACCEL, VEHICLE_TURN_DECEL, VEHICLE_TURN_MAX_SPEED, g_ClockTimer);

    while (vehicle->roty >= M_TAU_F)
    {
        vehicle->roty -= M_TAU_F;
    }

    while (vehicle->roty < 0.0f)
    {
        vehicle->roty += M_TAU_F;
    }

    if (targetyaw == vehicle->roty
            && vehicle->turnrot60 <= VEHICLE_TURN_DECEL && vehicle->turnrot60 >= -VEHICLE_TURN_DECEL)
    {
        vehicle->turnrot60 = 0.0f;
    }

    // While turning, the vehicle slides sideways a little, by the back axle's distance
    // from its middle
    axlepos = &model->obj->Switches[VEHICLE_SWITCH_WHEEL3]->Data->Group.Origin;

    if (g_GlobalTimerDelta > 0.0f)
    {
        angularspeed = (vehicle->roty - prevyaw) / g_GlobalTimerDelta;
    }

    if (angularspeed < 0.0f)
    {
        angularspeed += M_TAU_F;
    }

    sideslip = ((axlepos->f[2] * model->scale) * sinf(angularspeed)) * g_GlobalTimerDelta;

    forward.f[0] = sinf(vehicle->roty);
    forward.f[1] = 0.0f;
    forward.f[2] = cosf(vehicle->roty);

    newpos.f[0] = (vehicle->runtime_pos.f[0] + ((vehicle->speed * g_GlobalTimerDelta) * forward.f[0])) - (forward.f[2] * sideslip);
    newpos.f[1] = vehicle->runtime_pos.f[1];
    newpos.f[2] = (vehicle->runtime_pos.f[2] + ((vehicle->speed * g_GlobalTimerDelta) * forward.f[2])) + (forward.f[0] * sideslip);

    if (stanTestLineUnobstructed(&tile, prop->pos.f[0], prop->pos.f[2], newpos.f[0], newpos.f[2], VEHICLE_COLLIDE_TYPES, 0.0f, 1.0f, 0.0f, 1.0f)
            && stanTestVolume(&tile, newpos.f[0], newpos.f[2], 10.0f, VEHICLE_COLLIDE_TYPES, 0.0f, 1.0f) < 0)
    {
        StandTile *prevtile = prop->stan;
        coord3d prevpos;
        bool canmove;

        prevpos.f[0] = prop->pos.f[0];
        prevpos.f[1] = prop->pos.f[1];
        prevpos.f[2] = prop->pos.f[2];

        prop->stan = tile;
        vehicle->runtime_pos.f[0] = (prop->pos.f[0] = newpos.f[0]);
        vehicle->runtime_pos.f[2] = (prop->pos.f[2] = newpos.f[2]);
        objUpdateCollisionBounds(obj);
        setupUpdateObjectRoomPosition(obj);

        canmove = propIsUnblocked(prop) && vehicleBodyFitsFloor(prop, vehicle, model);

        if (canmove)
        {
            vehicleAlignToFloor(vehicle);
            objGetShadeColour(prop, &vehicle->nextcol);
            proxminesDetonateNear(&vehicle->runtime_pos);

            if (waypointpos != NULL && chrIsArrivingLaterallyAtPos(&prevpos, &newpos, waypointpos, 100.0f))
            {
                // Next waypoint; at the end of the path, slow to a stop over a second
                vehicle->nextstep++;

                if (vehicle->path->waypoints[vehicle->nextstep] < 0)
                {
                    vehicle->path = NULL;
                    vehicle->speedaim = 0.0f;
                    vehicle->speedtime60 = 60.0f;
                }
            }
        }
        else
        {
            vehicleStop(vehicle, prevyaw, prevturnspeed);

            prop->stan = prevtile;
            obj->runtime_pos.f[0] = (prop->pos.f[0] = prevpos.f[0]);
            obj->runtime_pos.f[1] = (prop->pos.f[1] = prevpos.f[1]);
            obj->runtime_pos.f[2] = (prop->pos.f[2] = prevpos.f[2]);
            objUpdateCollisionBounds(obj);
            setupUpdateObjectRoomPosition(obj);
        }
    }
    else
    {
        vehicleStop(vehicle, prevyaw, prevturnspeed);
    }
}


/* Run the vehicle's AI script, play its engine sound, and drive. Runs once a frame. */
void vehicleTick(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    VehichleRecord *vehicle = (VehichleRecord *)obj;
    f32 targetyaw = vehicle->roty;
    coord3d *waypointpos = NULL;
    s32 volume = 0;

    aiForObject((PropDefHeaderRecord *)vehicle);

    speedTowardsAim(&vehicle->speed, vehicle->speedaim, &vehicle->speedtime60);

    if (!(obj->flags2 & PROPFLAG2_00080000) && objIsHealthy(obj) && (vehicle->speed > 0.0f || vehicle->speedaim > 0.0f))
    {
        volume = sfxVolumeAtRange(&vehicle->runtime_pos, 2000.0f, 3000.0f);
    }

    objUpdateEngineSound(&vehicle->Sound, TRUCK_RUN_SFX, volume);

    if (vehicle->path != NULL)
    {
        waypoint *next = &g_CurrentSetup.pathwaypoints[vehicle->path->waypoints[vehicle->nextstep]];

        waypointpos = &g_CurrentSetup.pads[next->padID].pos;
        targetyaw = mathAtan2(waypointpos->f[0] - vehicle->runtime_pos.f[0], waypointpos->f[2] - vehicle->runtime_pos.f[2]);

        // Just placed on the path: face the waypoint
        if (vehicle->flags & PROPFLAG_INMOTION)
        {
            vehicle->roty = targetyaw;
            obj->flags &= ~PROPFLAG_INMOTION;
            vehicleAlignToFloor(vehicle);
        }
    }
    else if (vehicle->flags & PROPFLAG_INMOTION)
    {
        vehicleResetHeading(vehicle);
    }

    if (vehicle->speed > 0.0f)
    {
        vehicleDrive(prop, vehicle, obj->model, targetyaw, waypointpos);
    }
    else if (vehicle->flags & PROPFLAG_INMOTION)
    {
        vehicleResetHeading(vehicle);
    }
}


static void vehicleRollWheels(VehichleRecord *vehicle, f32 spin)
{
    vehicle->wheelxrot += spin;

    while (vehicle->wheelxrot >= M_TAU_F)
    {
        vehicle->wheelxrot -= M_TAU_F;
    }

    while (vehicle->wheelxrot < 0.0f)
    {
        vehicle->wheelxrot += M_TAU_F;
    }
}


/**
 * The wheels' matrices: rolling with the speed, and the front wheels steering with the
 * turn. Only the player who ticks the vehicle rolls the wheels.
 */
void vehicleGetModelMatrices(PropRecord *prop, Model *model, Mtxf *mtxs, bool issimowner)
{
    VehichleRecord *vehicle = (VehichleRecord *)prop->obj;
    ModelNode **switches = model->obj->Switches;
    coord3d *wheel1pos = &switches[VEHICLE_SWITCH_WHEEL1]->Data->Group.Origin;
    coord3d *wheel2pos = &switches[VEHICLE_SWITCH_WHEEL2]->Data->Group.Origin;
    coord3d *wheel3pos = &switches[VEHICLE_SWITCH_WHEEL3]->Data->Group.Origin;
    coord3d *wheel4pos = &switches[VEHICLE_SWITCH_WHEEL4]->Data->Group.Origin;
    ModelRoData_BoundingBoxRecord *wheelbox = &switches[VEHICLE_SWITCH_WHEELBOX]->Data->BoundingBox;
    f32 wheeldiameter = (wheelbox->Bounds.ymax - wheelbox->Bounds.ymin) * model->scale;
    f32 wheelspin = 0.0f;
    Mtxf rollmtx;
    Mtxf steermtx;

    // (The spin is added twice on the ticking player's pass, so the wheels turn twice as
    // fast as the vehicle moves)
    if (issimowner)
    {
        wheelspin = ((vehicle->speed * g_GlobalTimerDelta) * M_TAU_F) / ((wheeldiameter * M_TAU_F) * 0.5f);
        vehicleRollWheels(vehicle, wheelspin);
    }

    vehicleRollWheels(vehicle, wheelspin);

    mtx4LoadXRotation(vehicle->wheelxrot, &rollmtx);

    if (vehicle->speed > 0.0f)
    {
        f32 turn = vehicle->turnrot60;
        f32 wheelbase = (wheel3pos->f[2] - wheel1pos->f[2]) * model->scale;
        f32 side;

        if (turn < 0.0f)
        {
            turn = -turn;
        }

        side = sinf(turn) * wheelbase;
        vehicle->wheelyrot = mathAtan2(side, (cosf(turn) * wheelbase) - (wheelbase - vehicle->speed));

        if (vehicle->wheelyrot < turn)
        {
            vehicle->wheelyrot = turn;
        }

        if (vehicle->turnrot60 > 0.0f)
        {
            vehicle->wheelyrot = M_TAU_F - vehicle->wheelyrot;
        }
    }

    mtx4LoadYRotation(vehicle->wheelyrot, &steermtx);

    // The back wheels only roll
    mtx4Copy(&rollmtx, &mtxs[3]);
    mtx4SetTranslation(wheel3pos, &mtxs[3]);
    mtx4MultAffineInPlace(&mtxs[0], &mtxs[3]);
    mtx4Copy(&rollmtx, &mtxs[4]);
    mtx4SetTranslation(wheel4pos, &mtxs[4]);
    mtx4MultAffineInPlace(&mtxs[0], &mtxs[4]);

    // The front wheels steer too
    mtx4MultAffineInPlace(&steermtx, &rollmtx);
    mtx4Copy(&rollmtx, &mtxs[1]);
    mtx4SetTranslation(wheel1pos, &mtxs[1]);
    mtx4MultAffineInPlace(&mtxs[0], &mtxs[1]);
    mtx4Copy(&rollmtx, &mtxs[2]);
    mtx4SetTranslation(wheel2pos, &mtxs[2]);
    mtx4MultAffineInPlace(&mtxs[0], &mtxs[2]);
}


/* Move an animated aircraft along its animation. Each level's animation is scaled and
   turned to fit the level. */
static void aircraftTickAnim(PropRecord *prop, AircraftRecord *aircraft)
{
    Model *model = aircraft->model;
    PadRecord *pad;

    modelSetRootPosition(model, &aircraft->runtime_pos);
#if defined(VERSION_EU)
    modelSetAnimPlaySpeed(aircraft->model, 1.2f, 0.0f);
#endif
    model = aircraft->model;

    if (model->anim == animation_table_ptrs2[1])
    {
        modelSetAnimTranslationScale(model, 10.438f);
        modelSetRootRotY(aircraft->model, M_PI_F);
    }
    else if (bossGetStageNum() == LEVELID_STATUE)
    {
        modelSetAnimTranslationScale(aircraft->model, 1.0438f);
        modelSetRootRotY(aircraft->model, 2.3561945f);
    }
    else if (bossGetStageNum() == LEVELID_FRIGATE)
    {
        modelSetAnimTranslationScale(aircraft->model, 1.0438f);
        modelSetRootRotY(aircraft->model, 3.9269907f);
    }
    else
    {
        modelSetAnimTranslationScale(aircraft->model, 1.0438f);
        modelSetRootRotY(aircraft->model, 0.0f);
    }

    modelTickAnim(aircraft->model, g_ClockTimer, 1);
    modelCalcPosition(aircraft->model);
    modelGetRootPosition(aircraft->model, &aircraft->runtime_pos);

    prop->pos.f[0] = aircraft->runtime_pos.f[0];
    prop->pos.f[2] = aircraft->runtime_pos.f[2];

    // The animation's height is above the aircraft's pad
    if (aircraft->pad < 10000)
    {
        pad = &g_CurrentSetup.pads[aircraft->pad];
    }
    else
    {
        pad = (PadRecord *)&g_CurrentSetup.boundpads[aircraft->pad - 10000];
    }

    prop->pos.f[1] = pad->pos.f[1] + aircraft->runtime_pos.f[1];
    aircraft->runtime_pos.f[1] = prop->pos.f[1];
    modelSetRootPosition(aircraft->model, &aircraft->runtime_pos);
}


/* Run the aircraft's AI script, move it along its animation, speed up its rotors and
   play its engine sound. Runs once a frame. */
void aircraftTick(PropRecord *prop)
{
    ObjectRecord *obj = prop->obj;
    AircraftRecord *aircraft = (AircraftRecord *)obj;
    s32 volume = 0;

    aiForObject((PropDefHeaderRecord *)aircraft);

    if (aircraft->model->anim != NULL)
    {
        aircraftTickAnim(prop, aircraft);
    }

    speedTowardsAim(&aircraft->speed, aircraft->speedaim, &aircraft->speedtime60);
    speedTowardsAim(&aircraft->rotaryspeed, aircraft->rotaryspeedaim, &aircraft->rotaryspeedtime);

    if (!(aircraft->flags2 & PROPFLAG2_00080000) && objIsHealthy(obj) && aircraft->rotaryspeed != 0.0f
            && !(aircraft->flags & PROPFLAG_INMOTION))
    {
        volume = sfxVolumeAt(&aircraft->runtime_pos);
    }

    objUpdateEngineSound(&aircraft->Sound, HELI_RUN_SFX, volume);
}


/**
 * The aircraft's matrices: its animation (if it has one) and the rotors. A crashed
 * aircraft (PROPFLAG_INMOTION) has its rotor turned on its side. Only the player who
 * ticks the aircraft spins the rotors.
 */
void aircraftGetModelMatrices(PropRecord *prop, Model *model, Mtxf *mtxs, bool issimowner)
{
    AircraftRecord *aircraft = (AircraftRecord *)prop->obj;
    coord3d *rotorpos = &model->obj->Switches[2]->Data->Group.Origin;
    coord3d *nodepos;
    Mtxf *nodemtx;
    Mtxf rotormtx;

    if (g_ClockTimer > 0 && issimowner)
    {
        aircraft->rotoryrot += aircraft->rotaryspeed;

        while (aircraft->rotoryrot >= M_TAU_F)
        {
            aircraft->rotoryrot -= M_TAU_F;
        }

        while (aircraft->rotoryrot < 0.0f)
        {
            aircraft->rotoryrot += M_TAU_F;
        }
    }

    if (aircraft->model->anim != NULL)
    {
        ModelRenderData renderdata = {.zbufferenabled = TRUE, .flags = 3};

        renderdata.basemtx = camGetWorldToScreenMtxf();
        renderdata.mtxlist = &mtxs[0];
        modelCalcMatrices(&renderdata, aircraft->model);
    }
    else
    {
        mtx4Copy(&mtxs[0], &mtxs[1]);
    }

    if (aircraft->flags & PROPFLAG_INMOTION)
    {
        mtx4LoadZRotation(aircraft->rotoryrot, &rotormtx);
    }
    else
    {
        mtx4LoadYRotation(aircraft->rotoryrot, &rotormtx);
    }

    mtx4Copy(&rotormtx, &mtxs[2]);
    mtx4SetTranslation(rotorpos, &mtxs[2]);
    mtx4MultAffineInPlace(&mtxs[1], &mtxs[2]);

    // The tail rotor
    if (model->obj->Switches[3] != NULL)
    {
        nodepos = (coord3d *)model->obj->Switches[3]->Data;
        nodemtx = modelFindNodeMtx(model, model->obj->Switches[3], 0);
        mtx4LoadXRotation(aircraft->rotoryrot, &rotormtx);
        mtx4Copy(&rotormtx, nodemtx);
        mtx4SetTranslation(nodepos, nodemtx);
        mtx4MultAffineInPlace(&mtxs[1], nodemtx);
    }

    if (model->obj->Switches[4] != NULL)
    {
        nodepos = (coord3d *)model->obj->Switches[4]->Data;
        nodemtx = modelFindNodeMtx(model, model->obj->Switches[4], 0);
        mtx4LoadTranslation(nodepos, nodemtx);
        mtx4MultAffineInPlace(&mtxs[1], nodemtx);
    }
}


/**
 * The tank's matrices: the turret turned, the gun pitched, and the muzzle flash. Also
 * works out the turret's outline for collisions.
 */
void tankGetModelMatrices(PropRecord *prop, Model *model, Mtxf *mtxs)
{
    TankRecord *tank = (TankRecord *)prop->obj;
    ModelNode **switches = model->obj->Switches;
    coord3d *turretpos = &switches[1]->Data->Group.Origin;
    coord3d *turrettoppos = &switches[2]->Data->Group.Origin;
    coord3d *gunpos = &switches[3]->Data->Group.Origin;
    coord3d *guntippos = &switches[4]->Data->Group.Origin;
    ModelRoData_BoundingBoxRecord *turretbox = &switches[6]->Data->BoundingBox;
    f32 pitch = -tank->turret_vertical_angle;
    f32 yaw;
    Mtxf turretworldmtx;

    if (pitch < 0.0f)
    {
        pitch += M_TAU_F;
    }

    yaw = -tank->turret_orientation_angle;

    if (yaw < 0.0f)
    {
        yaw += M_TAU_F;
    }

    mtx4LoadYRotation(yaw, &mtxs[1]);
    mtx4SetTranslation(turretpos, &mtxs[1]);
    mtx4MultAffineInPlace(&mtxs[0], &mtxs[1]);
    mtx4LoadXRotation(pitch, &mtxs[3]);
    mtx4SetTranslation(gunpos, &mtxs[3]);
    mtx4MultAffineInPlace(&mtxs[1], &mtxs[3]);
    mtx4LoadYRotation(M_PI_2F, &mtxs[4]);
    mtx4SetTranslation(guntippos, &mtxs[4]);
    mtx4MultAffineInPlace(&mtxs[3], &mtxs[4]);
    mtx4LoadTranslation(turrettoppos, &mtxs[2]);
    mtx4MultAffineInPlace(&mtxs[1], &mtxs[2]);

    mtx4MultAffine(camGetViewToWorldMtxf(), &mtxs[1], &turretworldmtx);
    tank->hullEdges = bboxGetHull2d(turretbox, &turretworldmtx, tank->hull);

    if (switches[7] != NULL)
    {
        modelGetNodeRwData(model, switches[7])->Gunfire.visible = (s16)tank->is_firing_tank;
    }

    if (switches[8] != NULL)
    {
        modelGetNodeRwData(model, switches[8])->Switch.visible = 0;
    }
}
