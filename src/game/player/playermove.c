/**
 * Moving the player with collision: the player's collision state
 * (playerResetCollision), testing a move against the floor tiles and props
 * (playerTestMove), and sliding along walls and round corners when the straight
 * move is blocked (playerMoveWithCollision). Also keeping the player prop in the
 * right rooms. See docs/subsystems/player.md.
 */
#include "game/level/lv.h"
#include "game/menu/watchmenu.h"
#include "game/mission/objective.h"
#include "game/nav/geometry.h"
#include "game/nav/stan.h"
#include "game/player/bondview.h"
#include "game/player/bondview_internal.h"
#include "game/player/player.h"
#include "game/world/bgfog.h"
#include "random.h"
#include "game/player/playermove.h"

/**
 * Puts the player's body (col) at pos on tile stan, looking along +x.
 */
void playerResetCollision(struct PlayerBond *col, coord3d *pos, StandTile *stan)
{
    col->collision_position = *pos;
    col->pos = *pos;
    col->pos3 = *pos;
    col->current_tile_ptr = stan;
    col->current_tile_ptr_for_portals = stan;

    col->applied_view.x = 1.0f;
    col->applied_view.y = 0.0f;
    col->applied_view.z = 0.0f;
    col->applied_view2.x = 0.0f;
    col->applied_view2.y = 1.0f;
    col->applied_view2.z = pos->x * 0.0f; // -0.0f when x is negative, as in the original
    col->theta_transform.x = 0.0f;
    col->theta_transform.y = 0.0f;
    col->theta_transform.z = 1.0f;

    col->collision_radius = 30;
}


#define COOP_OVERLAP_CLEAR_DIST 70.0f

/**
 * Co-op: while the two players overlap after spawning (see
 * g_CoopPlayersOverlap), returns the partner's prop so movement can ignore
 * it. Clears the overlap once they are far enough apart.
 */
static PropRecord *coopGetOverlappingPartner(void)
{
    PropRecord *other;
    f32 dx;
    f32 dz;

    if (!g_CoopPlayersOverlap || (getPlayerCount() < 2))
    {
        return NULL;
    }

    other = g_playerPointers[playerGetCurrentNum() == 0 ? 1 : 0]->prop;
    dx = other->pos.f[0] - g_CurrentPlayer->prop->pos.f[0];
    dz = other->pos.f[2] - g_CurrentPlayer->prop->pos.f[2];

    if ((dx * dx) + (dz * dz) > COOP_OVERLAP_CLEAR_DIST * COOP_OVERLAP_CLEAR_DIST)
    {
        g_CoopPlayersOverlap = FALSE;
        return NULL;
    }

    return other;
}


/**
 * TRUE (and the tile at pos in *stan) if the current player can move from their
 * collision position to pos: the line along the floor tiles is clear, there is room
 * for their collision circle, and the ceiling is high enough (unless on a ladder).
 * In the tank, tests the tank's box instead (tankTryMoveTo).
 */
s32 playerTestMove(coord3d *pos, StandTile **stan)
{
    s32 ok;
    StandTile *tile;
    PropRecord *overlap_partner;
    s32 cdtypes;
    f32 height;
    f32 always_30;
    f32 collision_radius;
    s32 special;
    struct TankRecord *tank;
    StanSpecialTiles locus;

    ok = 0;

    if ((g_PlayerIsInTank == 1) && (g_EnterTankAudioState != TANK_RUN_STATE_NOT_RUNNING))
    {
        ok = tankTryMoveTo(pos, g_TankOrientationAngle, stan);
    }
    else
    {
        tile = g_CurrentPlayer->bond.current_tile_ptr;

        if (g_PlayerCollisionOn)
        {
            cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
        }
        else
        {
            cdtypes = 0;
        }

        playerGetCollisionSize(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

        if (g_WorldTankProp != NULL)
        {
            propSetMoving(g_WorldTankProp, 0);
        }

        overlap_partner = coopGetOverlappingPartner();

        if (overlap_partner != NULL)
        {
            propSetMoving(overlap_partner, 0);
        }

        propSetMoving(g_CurrentPlayer->prop, 0);
        special = stanCircleGetSpecial(&tile, pos->f[0], pos->f[2], collision_radius, &locus);

        if (locus.crouch)
        {
            g_CurrentPlayer->autocrouchpos = CROUCH_SQUAT;
        }

        // The move is allowed if the line there and the circle round it are clear, the
        // player can stand there (crouched, or not on a crouch-only tile), and the
        // ceiling is high enough unless on a ladder
        if ((stanTestLineUnobstructed(
                &tile,
                g_CurrentPlayer->bond.collision_position.f[0],
                g_CurrentPlayer->bond.collision_position.f[2],
                pos->f[0],
                pos->f[2],
                cdtypes,
                height,
                always_30,
                0.0f,
                1.0f) != 0)
            && (stanTestVolume(&tile, pos->f[0], pos->f[2], collision_radius, cdtypes, height, always_30) < 0)
            && ((g_CurrentPlayer->ducking_height_offset == FULL_CROUCH_OFFSET) || (special < 0))
            && (locus.ladder || (stanTestCircleMaxHeight(&tile, pos->f[0], pos->f[2], collision_radius, g_CurrentPlayer->bond.collision_position.f[1] + 175.0f) < 0)))
        {
            *stan = tile;
            ok = 1;
        }
        else
        {
            // Blocked by the tank: remember it as g_WorldTankProp, which later moves
            // ignore so Bond can climb on. Not in co-op while the other player uses
            // the tank (coopTankIsHidden), so it stays solid for this one.
            if (g_PlayerTankProp == NULL
                && !coopTankIsHidden()
                && (g_StanHit.prop != NULL)
                && (g_StanHit.prop->type == PROP_TYPE_OBJ))
            {
                tank = (struct TankRecord *)g_StanHit.prop->obj;
                if (tank->type == PROPDEF_TANK)
                {
                    g_WorldTankProp = g_StanHit.prop;
                }
            }
        }

        propSetMoving(g_CurrentPlayer->prop, 1);

        if (overlap_partner != NULL)
        {
            propSetMoving(overlap_partner, 1);
        }

        if (g_WorldTankProp != NULL)
        {
            propSetMoving(g_WorldTankProp, 1);
        }
    }

    return ok;
}


/**
 * Calculates collision with current player.
 *
 * @param next_pos: 3d coordinate to attempt to move to.
 * @param collision_pt0: Out parameter. Will set {x,0,z} position of first point (from line edge) if Bond is in collision, otherwise {0}.
 * @param collision_pt1: Out parameter. Will set {x,0,z} position of second point (from line edge) if Bond is in collision, otherwise ... Bond's look angle?
 *
 * @return 1 if able to update stan and collision position, zero otherwise.
 */
static s32 playerTryMoveSimple(coord3d *next_pos, coord3d *collision_pt0, coord3d *collision_pt1)
{
    struct StandTile *stan;

    // resets stan global collision variables
    stanResetHits();

    if (playerTestMove(next_pos, &stan) != 0)
    {
        g_CurrentPlayer->bond.current_tile_ptr = stan;
        g_CurrentPlayer->bond.collision_position.f[0] = next_pos->f[0];
        g_CurrentPlayer->bond.collision_position.f[2] = next_pos->f[2];

        return 1;
    }

    stanGetCollisionEdge(collision_pt0, collision_pt1);

    return 0;
}


/**
 * This is a fallback method used when playerTryMoveSimple fails.
 * Instead of moving to the full coordinate specified by `next_pos`, it will
 * scale the position using `geoCircleSweepFrac` and try to move to that position.
 *
 * @param next_pos: 3d coordinate to attempt to move to.
 * @param collision1_pt0: Prior collision point 0.
 * @param collision1_pt1: Prior collision point 1.
 * @param collision2_pt0: Out parameter. Will set {x,0,z} position of first point (from line edge) if Bond is in collision, otherwise {0}.
 * @param collision2_pt1: Out parameter. Will set {x,0,z} position of second point (from line edge) if Bond is in collision, otherwise ... Bond's look angle?
 *
 * @return 1 if able to update stan and collision position, zero if still unable to move by failing on the same collision edge, -1 otherwise (still unable to move).
 *
 * Perfect Dark, see bondwalk.c bwalk0f0c47d0, bondbike.c bbike0f0d36d4.
*/
static s32 playerTryMoveFraction(
    coord3d *next_pos,
    coord3d *collision1_pt0,
    coord3d *collision1_pt1,
    coord3d *collision2_pt0,
    coord3d *collision2_pt1)
{
    StandTile *stan;
    f32 height;
    f32 always_30;
    coord3d try_next_pos;
    coord3d delta_pos;
    coord2d centre;
    coord2d edgeStart;
    coord2d edgeEnd;
    coord2d delta;
    f32 frac;
    f32 collision_radius;

    playerGetCollisionSize(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

    delta_pos.f[0] = next_pos->f[0] - g_CurrentPlayer->bond.collision_position.f[0];
    delta_pos.f[2] = next_pos->f[2] - g_CurrentPlayer->bond.collision_position.f[2];

    centre.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
    centre.f[1] = g_CurrentPlayer->bond.collision_position.f[2];

    edgeStart.f[0] = collision1_pt0->f[0];
    edgeStart.f[1] = collision1_pt0->f[2];

    edgeEnd.f[0] = collision1_pt1->f[0];
    edgeEnd.f[1] = collision1_pt1->f[2];

    delta.f[0] = delta_pos.f[0];
    delta.f[1] = delta_pos.f[2];

    frac = geoCircleSweepFrac(collision_radius, &centre, &delta, &edgeStart, &edgeEnd);

    try_next_pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0] + (delta_pos.f[0] * frac * 0.25f);
    try_next_pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2] + (delta_pos.f[2] * frac * 0.25f);

    if (playerTestMove(&try_next_pos, &stan) != 0)
    {
        g_CurrentPlayer->bond.current_tile_ptr = stan;
        g_CurrentPlayer->bond.collision_position.f[0] = try_next_pos.f[0];
        g_CurrentPlayer->bond.collision_position.f[2] = try_next_pos.f[2];

        return 1;
    }

    stanGetCollisionEdge(collision2_pt0, collision2_pt1);

    if (collision2_pt0->f[0] != collision1_pt0->f[0]
        || collision2_pt0->f[1] != collision1_pt0->f[1]
        || collision2_pt0->f[2] != collision1_pt0->f[2]
        || collision2_pt1->f[0] != collision1_pt1->f[0]
        || collision2_pt1->f[1] != collision1_pt1->f[1]
        || collision2_pt1->f[2] != collision1_pt1->f[2])
    {
        return 0;
    }

    return -1;
}


/**
 * This is a fallback method used when playerTryMoveSimple fails.
 * Instead of moving to the full coordinate specified by `next_pos`, it will
 * attempt to move along the collision edge.
 *
 * @param prior_next_pos: Prior 3d coordinate that Bond failed to move to.
 * @param collision1_pt0: Prior collision point 0.
 * @param collision1_pt1: Prior collision point 1.
 *
 * @return -1 if either x or z coordinates are the same for the collision points, 1 if able to update stan and collision position, zero otherwise.
 *
 */
static s32 playerTryMoveAlongEdge(coord3d *prior_next_pos, coord3d *collision_pt0, coord3d *collision_pt1)
{
    coord3d delta_pos;
    f32 tempf;
    coord3d norm_collision_edge;
    coord3d try_next_pos;
    StandTile *stan;

    delta_pos.f[0] = prior_next_pos->f[0] - g_CurrentPlayer->bond.collision_position.f[0];
    delta_pos.f[2] = prior_next_pos->f[2] - g_CurrentPlayer->bond.collision_position.f[2];

    if (collision_pt0->f[0] != collision_pt1->f[0] || collision_pt0->f[2] != collision_pt1->f[2])
    {
        norm_collision_edge.f[0] = collision_pt1->f[0] - collision_pt0->f[0];
        norm_collision_edge.f[2] = collision_pt1->f[2] - collision_pt0->f[2];

        tempf = (norm_collision_edge.f[0] * norm_collision_edge.f[0]) + (norm_collision_edge.f[2] * norm_collision_edge.f[2]);
        tempf =  1.0f / sqrtf(tempf);
        norm_collision_edge.f[0] *= tempf;
        norm_collision_edge.f[2] *= tempf;

        /**
         * Normalizing gives you the direction vector of the wall, so the dot product in the assignment to
         * tempf gives you the distance moved along the direction of the wall.
         * Then try_next_pos is simply the point of the collision with the wall, plus the
         * length moved along the wall times the direction vector of the wall.
         **/
        tempf = (delta_pos.f[0] * norm_collision_edge.f[0]) + (delta_pos.f[2] * norm_collision_edge.f[2]);
        try_next_pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0] + (tempf * norm_collision_edge.f[0]);
        try_next_pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2] + (tempf * norm_collision_edge.f[2]);

        if (playerTestMove(&try_next_pos, &stan))
        {
            g_CurrentPlayer->bond.current_tile_ptr = stan;
            g_CurrentPlayer->bond.collision_position.f[0] = try_next_pos.f[0];
            g_CurrentPlayer->bond.collision_position.f[2] = try_next_pos.f[2];

            return 1;
        }

        return 0;
    }

    return -1;
}


/**
 * This is a fallback method used when playerTryMoveSimple fails.
 * If Bond previously failed to move because of a collision, this will check
 * if Bond is near the end point of the collision edge. If within the collision
 * radius of Bond to the edge endpoint, then allow movement.
 *
 * @param prior_next_pos: Prior 3d coordinate that Bond failed to move to.
 * @param collision1_pt0: Prior collision point 0.
 * @param collision1_pt1: Prior collision point 1.
 *
 * @return 1 if able to update stan and collision position, zero otherwise.
 *
 *
 * Perfect Dark bwalk0f0c4a5c.
*/
static s32 playerTryMoveRoundCorner(coord3d *prior_next_pos, coord3d *collision_pt0, coord3d *collision_pt1)
{
    coord3d delta_pos;
    coord3d offset;
    coord3d try_next_pos;
    f32 height;
    f32 always_30;
    f32 tempf;
    StandTile *stan;
    f32 collision_radius;

    playerGetCollisionSize(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

    delta_pos.f[0] = prior_next_pos->f[0] - g_CurrentPlayer->bond.collision_position.f[0];
    delta_pos.f[2] = prior_next_pos->f[2] - g_CurrentPlayer->bond.collision_position.f[2];

    offset.f[0] = collision_pt0->f[0] - prior_next_pos->f[0];
    offset.f[2] = collision_pt0->f[2] - prior_next_pos->f[2];

    if (((offset.f[0] * offset.f[0]) + (offset.f[2] * offset.f[2])) <= (collision_radius * collision_radius))
    {
        if (collision_pt0->f[0] != g_CurrentPlayer->bond.collision_position.f[0] || collision_pt0->f[2] != g_CurrentPlayer->bond.collision_position.f[2])
        {
            offset.f[0] = -(collision_pt0->f[2] - g_CurrentPlayer->bond.collision_position.f[2]);
            offset.f[2] = collision_pt0->f[0] - g_CurrentPlayer->bond.collision_position.f[0];

            tempf = (offset.f[0] * offset.f[0]) + (offset.f[2] * offset.f[2]);
            tempf =  1.0f / sqrtf(tempf);
            offset.f[0] *= tempf;
            offset.f[2] *= tempf;

            tempf = (delta_pos.f[0] * offset.f[0]) + (delta_pos.f[2] * offset.f[2]);
            offset.f[0] *= tempf;
            offset.f[2] *= tempf;
            try_next_pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0] + (offset.f[0]);
            try_next_pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2] + (offset.f[2]);

            if (playerTestMove(&try_next_pos, &stan))
            {
                g_CurrentPlayer->bond.current_tile_ptr = stan;
                g_CurrentPlayer->bond.collision_position.f[0] = try_next_pos.f[0];
                g_CurrentPlayer->bond.collision_position.f[2] = try_next_pos.f[2];

                return 1;
            }
        }
    }
    else
    {
        offset.f[0] = collision_pt1->f[0] - prior_next_pos->f[0];
        offset.f[2] = collision_pt1->f[2] - prior_next_pos->f[2];

        if (((offset.f[0] * offset.f[0]) + (offset.f[2] * offset.f[2])) <= (collision_radius * collision_radius))
        {
            if (collision_pt1->f[0] != g_CurrentPlayer->bond.collision_position.f[0] || collision_pt1->f[2] != g_CurrentPlayer->bond.collision_position.f[2])
            {
                offset.f[0] = -(collision_pt1->f[2] - g_CurrentPlayer->bond.collision_position.f[2]);
                offset.f[2] = collision_pt1->f[0] - g_CurrentPlayer->bond.collision_position.f[0];

                tempf = (offset.f[0] * offset.f[0]) + (offset.f[2] * offset.f[2]);
                tempf =  1.0f / sqrtf(tempf);
                offset.f[0] *= tempf;
                offset.f[2] *= tempf;

                tempf = (delta_pos.f[0] * offset.f[0]) + (delta_pos.f[2] * offset.f[2]);
                offset.f[0] *= tempf;
                offset.f[2] *= tempf;
                try_next_pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0] + (offset.f[0]);
                try_next_pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2] + (offset.f[2]);

                if (playerTestMove(&try_next_pos, &stan))
                {
                    g_CurrentPlayer->bond.current_tile_ptr = stan;
                    g_CurrentPlayer->bond.collision_position.f[0] = try_next_pos.f[0];
                    g_CurrentPlayer->bond.collision_position.f[2] = try_next_pos.f[2];

                    return 1;
                }
            }
        }
    }

    return 0;
}


/**
 * Sets Bond bondprevpos, attempts to move by `offset`.
 *
 * @param offset: Attempt to move Bond by {x,0,z} amount.
 * @param allow_scoot: If movement causes collision, when set will allow Bond to scoot along the collision edge and to bump around corner edges. Otherwise, any collision will stop movement.
 */
void playerMoveWithCollision(coord3d *offset, s32 allow_scoot)
{
    coord3d next_pos;
    coord3d collision1_pt0;
    coord3d collision1_pt1;
    coord2d *polygon;
    s32 edges;
    struct TankRecord *tank_objrecord;
    struct ObjectRecord *obj;
    f32 *farr5;
    f32 *farr6;
    f32 tankheight;
    coord3d collision2_pt0;
    coord3d collision2_pt1;
    StandTile *stan;
    coord3d collision3_pt0;
    coord3d collision3_pt1;
    s32 tile_count;
    s32 i;
    s32 pick;
    s32 numcandidates;
    s32 result;


    g_CurrentPlayer->bondprevpos.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
    g_CurrentPlayer->bondprevpos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
    g_CurrentPlayer->bondprevpos.f[2] = g_CurrentPlayer->bond.collision_position.f[2];

    next_pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0] + offset->f[0];
    next_pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2] + offset->f[2];

    g_BondCanEnterTank = 0;

    g_CurrentPlayer->autocrouchpos = CROUCH_STAND;

    if (g_WorldTankProp != NULL)
    {
        propGetCollisionPolygon2d(g_WorldTankProp, &polygon, &edges);

        if ((g_PlayerIsInTank == 1)
            || (polygonContainsPoint(&g_CurrentPlayer->bond.collision_position, polygon, edges) != 0)
            || ((polygonTouchesCircle(&g_CurrentPlayer->bond.collision_position, g_CurrentPlayer->bond.collision_radius, polygon, edges) != 0)))
        {

            obj = g_WorldTankProp->obj;
            tank_objrecord = (struct TankRecord *)g_WorldTankProp->obj;

            /// TODO: replace with ModelNode structs
            farr5 = (f32*)obj->model->obj->Switches[5]->Data;
            farr6 = (f32*)obj->model->obj->Switches[6]->Data;
            g_PlayerTankProp = g_WorldTankProp;

            tankheight = (farr5[4] - farr5[3]) * obj->model->scale;

            if (g_PlayerIsInTank == 1
                || (polygonContainsPoint(&g_CurrentPlayer->bond.collision_position, tank_objrecord->hull, tank_objrecord->hullEdges) != 0))
            {
                tankheight += (farr6[4] - farr6[3]) * obj->model->scale;
                g_BondCanEnterTank = 1;
            }

            if ((g_PlayerIsInTank == 0) && (g_PlayerTankYOffset < tankheight))
            {
                g_PlayerTankYOffset += (20.0f * g_GlobalTimerDelta);
                if ((tankheight < g_PlayerTankYOffset))
                {
                    //empty;
                }
                else
                {
                    return;
                }

                g_PlayerTankYOffset = tankheight;
            }
            else
            {
                g_PlayerTankYOffset = tankheight;
            }

            if (g_PlayerIsInTank == 1)
            {
                if (g_EnterTankAudioState == TANK_RUN_STATE_NOT_RUNNING)
                {
                    g_PlayerTankYOffset += -37.0f * (1.0f - g_TankEnteringSitHeightRemain);
                }
                else
                {
                    g_PlayerTankYOffset += -37.0f;
                }
            }
        }
        else
        {
            if (g_PlayerTankProp != NULL)
            {
                g_WorldTankProp = NULL;
                g_PlayerTankProp = NULL;
                g_PlayerTankYOffset = 0.0f;
            }
        }
    }

    // This `if` block looks like Perfect Dark bbike0f0d3c60
    if (playerTryMoveSimple(&next_pos, &collision1_pt0, &collision1_pt1) == 0)
    {
        // return values are:
        //   1 if able to update stan and collision position
        //   zero if still unable to move by failing on the same collision edge
        //   -1 otherwise (still unable to move).
        result = playerTryMoveFraction(&next_pos, &collision1_pt0, &collision1_pt1, &collision2_pt0, &collision2_pt1);

        if ((result > 0) || (result < 0))
        {
            if ((allow_scoot != 0)
                && (playerTryMoveAlongEdge(&next_pos, &collision1_pt0, &collision1_pt1) <= 0)
                && (playerTryMoveRoundCorner(&next_pos, &collision1_pt0, &collision1_pt1) == 0))
            {
                // empty
            }
        }
        else if (result == 0)
        {
            playerTryMoveFraction(&next_pos, &collision2_pt0, &collision2_pt1, &collision3_pt0, &collision3_pt1);

            if ((allow_scoot != 0)
                && (playerTryMoveAlongEdge(&next_pos, &collision2_pt0, &collision2_pt1) <= 0)
                && (playerTryMoveAlongEdge(&next_pos, &collision1_pt0, &collision1_pt1) <= 0)
                && (playerTryMoveRoundCorner(&next_pos, &collision2_pt0, &collision2_pt1) == 0))
            {
                playerTryMoveRoundCorner(&next_pos, &collision1_pt0, &collision1_pt1);
            }
        }
    }

    /**
     * This block seems to be some error checking code, this will only occur when Bond
     * goes out of bounds.
    */
    if (stanIsNearlyInsideTri(
            g_CurrentPlayer->bond.current_tile_ptr,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2]) == 0)
    {

        stan = g_CurrentPlayer->bond.current_tile_ptr;

        for (tile_count=0; tile_count<5; tile_count++)
        {
            /* Step to a random neighbouring tile */
            for (i = 0, numcandidates = 0; i < stan->numPoints; i++)
            {
                if (stanLinkedTile(stan, i) != NULL)
                {
                    numcandidates++;
                }
            }

            pick = randomGetNext() % (u32) numcandidates;

            for (i = 0, numcandidates = 0; i < stan->numPoints; i++)
            {
                if (stanLinkedTile(stan, i) != NULL)
                {
                    if (numcandidates == pick)
                    {
                        stan = stanLinkedTile(stan, i);
                        break;
                    }

                    numcandidates++;
                }
            }

            if (stanIsNearlyInsideTri(
                stan,
                g_CurrentPlayer->bond.collision_position.f[0],
                g_CurrentPlayer->bond.collision_position.f[2]))
            {
                g_CurrentPlayer->bond.current_tile_ptr = stan;
                break;
            }
        }
    }

    playerUpdateRooms(g_CurrentPlayer);

    // Counter-op: only Bond's objectives are tracked, so rooms the guard
    // player enters (Depot's train) don't count.
    if ((g_CurrentPlayer->bond.current_tile_ptr != NULL) && !counterOpIsGuardPlayer(playerGetCurrentNum()))
    {
        objectivesCheckRoomEntered(g_CurrentPlayer->bond.current_tile_ptr->room);
    }
}


void playerRemoveFromRooms(struct player *player) 
{
    propRemoveFromRoom(player->prop, player->registeredroom);
    player->registeredroom = -1;
}


void playerUpdateRooms(struct player *player)
{
    playerRemoveFromRooms(player);

    if (player->prop->chr)
    {
        chrDetectRooms(player->prop->chr);
        return;
    }

    if (player->bond.current_tile_ptr)
    {
        player->registeredroom = (s16) player->bond.current_tile_ptr->room;

        propAddToRoom(player->prop, player->registeredroom);
    }
}
