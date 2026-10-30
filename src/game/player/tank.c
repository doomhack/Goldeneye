/**
 * The tank Bond drives: its collision box against the floor tiles and props
 * (tankTestCollision, tankTryMoveTo), the turret's position and aim, and in co-op
 * which player owns it. Driving it is in playerMove (bondview2.c). See
 * docs/subsystems/player.md.
 */
#include "game/menu/watchmenu.h"
#include "game/nav/stan.h"
#include "game/player/bondview.h"
#include "game/player/bondview_internal.h"
#include "game/player/player.h"
#include "game/world/bgfog.h"
#include "game/player/tank.h"

/**
 * Sets g_TankModelPositionOffset: where the turret sits on the tank's body for
 * the turret's current angle.
 */
void tankUpdateTurretOffset(void)
{
    struct ObjectRecord *obj;
    coord3d *turretPos;
    coord3d *bodyPos;
    Mtxf rot;
    ModelNode **switches;

    if (g_PlayerTankProp != NULL)
    {
        obj = g_PlayerTankProp->obj;

        /// TODO: Fix Model struct Data type.
        switches = obj->model->obj->Switches;
        turretPos = (coord3d *)switches[2]->Data;
        bodyPos = (coord3d *)switches[1]->Data;
        mtx4LoadYRotation(M_TAU_F - g_TankTurretOrientationAngleRad, &rot);
        g_TankModelPositionOffset.f[0] = turretPos->f[0];
        g_TankModelPositionOffset.f[1] = turretPos->f[1];
        g_TankModelPositionOffset.f[2] = turretPos->f[2];
        mtx4RotateVecInPlace(&rot, &g_TankModelPositionOffset);
        g_TankModelPositionOffset.f[0] += bodyPos->f[0];
        g_TankModelPositionOffset.f[1] += bodyPos->f[1];
        g_TankModelPositionOffset.f[2] += bodyPos->f[2];
    }
}


/**
 * The tank's collision box (its bounding box turned by the tank's angle) as four
 * corners in x/z, centred on collision_position.
 */
void tankGetCollisionBounds(coord2d *tank_collision_bounds, coord3d *collision_position, f32 tank_orientation_angle)
{
    ObjectRecord *obj;
    f32 sinAngle;
    f32 xmin;
    f32 xmax;
    f32 zmin;
    f32 zmax;
    f32 cosAngle;
    struct ModelRoData_BoundingBoxRecord *bbox;


    obj = g_PlayerTankProp->obj;

    bbox = objFindBbox(obj);

    xmin = bbox->Bounds.xmin - g_TankModelPositionOffset.f[0];
    xmax = bbox->Bounds.xmax - g_TankModelPositionOffset.f[0];

    zmin = bbox->Bounds.zmin - g_TankModelPositionOffset.f[2];
    zmax = bbox->Bounds.zmax - g_TankModelPositionOffset.f[2];

    cosAngle = obj->model->scale * cosf(tank_orientation_angle);
    sinAngle = obj->model->scale * sinf(tank_orientation_angle);

    tank_collision_bounds[0].f[0] = collision_position->f[0] + (-zmin * sinAngle) + (xmin * cosAngle);
    tank_collision_bounds[0].f[1] = collision_position->f[2] + (zmin * cosAngle) + (xmin * sinAngle);

    tank_collision_bounds[1].f[0] = collision_position->f[0] + (-zmin * sinAngle) + (xmax * cosAngle);
    tank_collision_bounds[1].f[1] = collision_position->f[2] + (zmin * cosAngle) + (xmax * sinAngle);

    tank_collision_bounds[2].f[0] = collision_position->f[0] + (-zmax * sinAngle) + (xmax * cosAngle);
    tank_collision_bounds[2].f[1] = collision_position->f[2] + (zmax * cosAngle) + (xmax * sinAngle);

    tank_collision_bounds[3].f[0] = collision_position->f[0] + (-zmax * sinAngle) + (xmin * cosAngle);
    tank_collision_bounds[3].f[1] = collision_position->f[2] + (zmax * cosAngle) + (xmin * sinAngle);
}


/**
 * Tests a line along the floor tiles. If it is blocked, writes the line to
 * coord_p/coord_dest (when given) so the caller knows which edge was hit.
 */
static s32 tankTestLine(StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z, s32 cdtypes, coord3d *coord_p, coord3d *coord_dest)
{
    s32 ok;

    ok = stanTestLineUnobstructed(pTile, p_x, p_z, dest_x, dest_z, cdtypes, 0.0f, 1.0f, 0.0f, 1.0f);
    if ((ok == 0) && (coord_p != NULL))
    {
        coord_p->f[0] = p_x;
        coord_p->f[1] = 0.0f;
        coord_p->f[2] = p_z;
        coord_dest->f[0] = dest_x;
        coord_dest->f[1] = 0.0f;
        coord_dest->f[2] = dest_z;
    }

    return ok;
}


/**
 * TRUE if the tank fits at collision_position with this angle: the lines from the
 * centre to the first corner, round the four corners and out to the end of the
 * gun barrel must all be clear. If one is blocked it is written to hitFrom/hitTo.
 */
s32 tankTestCollision(coord3d *collision_position, StandTile *starttile, f32 tank_orientation_angle, coord3d *hitFrom, coord3d *hitTo)
{
    StandTile *tile;
    coord2d tank_collision_bounds[4];
    s32 ok;
    f32 angle;

    Model *model;

    ModelNode **switches;
    coord3d *gunEnd;
    coord3d *turretPos;
    coord3d barrelEnd;
    Mtxf rot;
    coord3d *gunStart;

    tile = starttile;
    ok = 0;

    tankGetCollisionBounds(tank_collision_bounds, collision_position, tank_orientation_angle);

    if (g_PlayerTankProp != NULL)
    {
        propSetMoving(g_PlayerTankProp, 0);
    }

    if ((tankTestLine(&tile, collision_position->f[0], collision_position->f[2], tank_collision_bounds[0].f[0], tank_collision_bounds[0].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, hitFrom, hitTo) != 0)
        && (tankTestLine(&tile, tank_collision_bounds[0].f[0], tank_collision_bounds[0].f[1], tank_collision_bounds[1].f[0], tank_collision_bounds[1].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, hitFrom, hitTo) != 0)
        && (tankTestLine(&tile, tank_collision_bounds[1].f[0], tank_collision_bounds[1].f[1], tank_collision_bounds[2].f[0], tank_collision_bounds[2].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, hitFrom, hitTo) != 0)
        && (tankTestLine(&tile, tank_collision_bounds[2].f[0], tank_collision_bounds[2].f[1], tank_collision_bounds[3].f[0], tank_collision_bounds[3].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, hitFrom, hitTo) != 0)
        && (tankTestLine(&tile, tank_collision_bounds[3].f[0], tank_collision_bounds[3].f[1], tank_collision_bounds[0].f[0], tank_collision_bounds[0].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, hitFrom, hitTo) != 0))
    {
        ok = 1;

        if (g_PlayerTankProp != NULL)
        {
            ObjectRecord *obj = g_PlayerTankProp->obj;
            model = obj->model;
            switches = model->obj->Switches;

            gunStart = &switches[3]->Data->Group.Origin;
            gunEnd = &switches[4]->Data->Group.Origin;
            turretPos = &switches[2]->Data->Group.Origin;

            barrelEnd.f[0] = gunEnd->f[0] + gunStart->f[0] - turretPos->f[0];
            barrelEnd.f[1] = 0.0f;
            barrelEnd.f[2] = gunEnd->f[2] + gunStart->f[2] - turretPos->f[2];

            angle = tank_orientation_angle + g_TankTurretOrientationAngleRad;

            if (angle >= M_TAU_F)
            {
                angle -= M_TAU_F;
            }

            if (angle < 0.0f)
            {
                angle += M_TAU_F;
            }

            mtx4LoadYRotation(M_TAU_F - angle, &rot);
            mtx4RotateVecInPlace(&rot, &barrelEnd);

            barrelEnd.f[0] *= model->scale;
            barrelEnd.f[2] *= model->scale;

            barrelEnd.f[0] += collision_position->f[0];
            barrelEnd.f[2] += collision_position->f[2];

            tile = starttile;

            if (tankTestLine(&tile, collision_position->f[0], collision_position->f[2], barrelEnd.f[0], barrelEnd.f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, hitFrom, hitTo) == 0)
            {
                ok = 0;
            }

        }
    }

    if (g_PlayerTankProp != NULL)
    {
        propSetMoving(g_PlayerTankProp, 1);
    }

    return ok;
}


// tankTestCollision without the blocked line
s32 tankTestCollisionAt(coord3d *collision_position, StandTile *tile, f32 tank_orientation_angle)
{
    return tankTestCollision(collision_position, tile, tank_orientation_angle, NULL, NULL);
}


/**
 * TRUE (and the tile at pos in *outTile) if the tank can move from the current
 * player's position to pos with this angle.
 */
s32 tankTryMoveTo(coord3d *pos, f32 angle, StandTile **outTile)
{
    StandTile *tile;

    tile = g_CurrentPlayer->bond.current_tile_ptr;

    if ((
        stanTestLineUnobstructed(
            &tile,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2],
            pos->f[0],
            pos->f[2],
            0,
            0.0f,
            1.0f,
            0.0f,
            1.0f) != 0)
        && (tankTestCollisionAt(pos, tile, angle) != 0))
    {
        *outTile = tile;
        return 1;
    }

    return 0;
}


/**
 * Co-op: the player using the tank (climbing on or driving it), or -1.
 * Streets and Runway have one tank and its state is all globals, so only
 * one player can use it; the other walks.
 */
s32 g_CoopTankPlayer = -1;

static struct
{
    s32 hidden;
    s32 inTank;
    s32 canEnter;
    f32 yOffset;
    struct PropRecord *worldTank;
    struct PropRecord *playerTank;
} s_CoopTankStash;

/**
 * Co-op: TRUE while the tank is hidden from the player not using it (it is then
 * solid for them, see playerTestMove).
 */
s32 coopTankIsHidden(void)
{
    return s_CoopTankStash.hidden;
}

/**
 * Co-op: TRUE if the other player is using the tank.
 */
s32 coopTankUsedByPartner(void)
{
    return g_CoopMode && (g_CoopTankPlayer >= 0) && (g_CoopTankPlayer != playerGetCurrentNum());
}

/**
 * Co-op: while the current player is moved or drawn, hide the tank state
 * if the other player is using the tank, so this player isn't treated as
 * being in it. Undone by coopTankEndPlayer.
 */
void coopTankBeginPlayer(void)
{
    s_CoopTankStash.hidden = coopTankUsedByPartner();

    if (s_CoopTankStash.hidden)
    {
        s_CoopTankStash.inTank = g_PlayerIsInTank;
        s_CoopTankStash.canEnter = g_BondCanEnterTank;
        s_CoopTankStash.yOffset = g_PlayerTankYOffset;
        s_CoopTankStash.worldTank = g_WorldTankProp;
        s_CoopTankStash.playerTank = g_PlayerTankProp;

        g_PlayerIsInTank = FALSE;
        g_BondCanEnterTank = FALSE;
        g_PlayerTankYOffset = 0.0f;
        g_WorldTankProp = NULL;
        g_PlayerTankProp = NULL;
    }
}

/**
 * Co-op: undo coopTankBeginPlayer. After a player's move, also note whether
 * they have started or stopped using the tank.
 */
void coopTankEndPlayer(s32 moved)
{
    if (s_CoopTankStash.hidden)
    {
        g_PlayerIsInTank = s_CoopTankStash.inTank;
        g_BondCanEnterTank = s_CoopTankStash.canEnter;
        g_PlayerTankYOffset = s_CoopTankStash.yOffset;
        g_WorldTankProp = s_CoopTankStash.worldTank;
        g_PlayerTankProp = s_CoopTankStash.playerTank;
        s_CoopTankStash.hidden = FALSE;
    }
    else if (g_CoopMode && moved)
    {
        g_CoopTankPlayer = ((g_PlayerTankProp != NULL) || g_PlayerIsInTank) ? playerGetCurrentNum() : -1;
    }
}

// TRUE if the current player is in (driving) the tank
bool playerIsInTank(void)
{
    if (coopTankUsedByPartner())
    {
        return FALSE;
    }

    // The owner, asked while the tank is hidden for the other player's view.
    if (s_CoopTankStash.hidden)
    {
        return s_CoopTankStash.inTank;
    }

    return g_PlayerIsInTank;
}


// The tank the current player is driving, or NULL
PropRecord *playerGetTank(void)
{
    if (g_PlayerIsInTank == 1)
    {
        return g_PlayerTankProp;
    }

    return NULL;
}


/**
 * The direction the tank's gun points: the tank's angle plus the turret's,
 * and the turret's vertical angle.
 */
void tankGetTurretDir(coord3d *dir)
{
    f32 f;

    f = g_TankOrientationAngle + g_TankTurretOrientationAngleRad;

    if (f >= M_TAU_F)
    {
        f = f - M_TAU_F;
    }

    if (f < 0.0f)
    {
        f = f + M_TAU_F;
    }

    dir->f[0] = -sinf(f) * cosf(g_TankTurretVerticalAngle);
    dir->f[1] = sinf(g_TankTurretVerticalAngle);
    dir->f[2] = cosf(f) * cosf(g_TankTurretVerticalAngle);
}
