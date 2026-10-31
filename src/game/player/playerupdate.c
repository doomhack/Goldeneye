/**
 * The player each frame: playerUpdate picks first-person movement (playerMove) or a
 * frozen camera (playerMoveFrozen); also the head bob animation (playerTickMoveAnim),
 * height and falling (playerUpdateVertical), getting in and driving the tank, and
 * the mission timer. Bond's body is in playerbody.c, input in playerinput.c, health
 * in playerdamage.c, camera modes in cammode.c. See docs/subsystems/player.md.
 */
#include <boss.h>
#include <fr.h>
#include <music.h>
#include "game/audio/sfxvolume.h"
#include "game/chr/chraction.h"
#include "game/level/frametiming.h"
#include "game/level/lv.h"
#include "game/math/physics.h"
#include "game/menu/cheat.h"
#include "game/menu/watchmenu.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/player/headbob.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/player/playertune.h"
#include "game/prop/propobj.h"
#include "game/prop/setup.h"
#include "random.h"
#include "game/level/gamemode.h"
#include "game/mission/mission.h"



/**
 * When set, will increment each tick until reaching a threshold value (4).
 * Then current items will be unequipped from left and run hands.
*/

s32 resolution = 0;


f32 g_PlayerWorldScale = 1.0;

static f32 g_PlayerWorldScaleInv = 1.0;


// Defaults for playerMoveFrozen's position and move offset
static coord3d g_DefaultMoveBondOffset = { 0 };

// struct player `pos`
static coord3d g_DefaultFrozenPlayerPos = { 0 };

// struct player `pos2`
static coord3d g_DefaultFrozenPlayerPos2 = { {0, 0, 1.0f} };

// struct player `offset`
static coord3d g_DefaultFrozenPlayerOffset = { {0, 1.0f, 0} };

// struct player `offset`
static coord3d g_DefaultFrozenMoveOffset = { 0 };


coord3d g_ForceBondMoveOffset;
s32 g_CoopForceMovePlayer = 0;

s32 mission_timer;

#if defined(VERSION_EU)
f32 watch_time_0;
#else
s32 watch_time_0;
#endif




/**
 * Will apply a move animation update. The pass through call to headBobUpdate is
 * what allows Bond to move. This will also trigger the death animation once
 * Bond dies. This chooses a random death animation from g_bondviewBondDeathAnimations.
*/
void playerTickMoveAnim(f32 speed, f32 speedforwards, f32 speedsideways)
{
    f32 percent_speed;
    Mtxf view;
    Mtxf rot;

    percent_speed = 0.0f;

    if (g_CurrentPlayer->bonddead == 0)
    {
        headBobAdjustAnimation(speed);

        if (speed != 0.0f)
        {
            percent_speed = speedforwards / speed;
        }
        else if (speedforwards == 0.0f)
        {
            //
        }
    }
    else
    {
        if (g_CurrentPlayer->startnewbonddie)
        {
            // HACK: g_AnimTable dereference addition is backwards.
            // this should be:
            // g_AnimTable->data[g_bondviewBondDeathAnimations[((u32) randomGetNext() % (u32) g_bondviewBondDeathAnimationsCount)]]
            headBobStartDeathAnimation((struct ModelAnimation *) ((s32)g_bondviewBondDeathAnimations[((u32) randomGetNext() % (u32) g_bondviewBondDeathAnimationsCount)] + (s32)&g_AnimTable->data[0]), randomGetNext() & 1, 0.0f, 1.0f);
            g_CurrentPlayer->startnewbonddie = FALSE;
        }

        headBobSetSpeed(0.5f);
        speedsideways = 0.0f;
    }

    headBobUpdate(percent_speed, speedsideways);

    mtx4LoadXRotation((360.0f - g_CurrentPlayer->vv_verta360) * DegToRad1Fact(1), &view);
    mtx4LoadFacing(&rot, 0.0f, 0.0f, 0.0f, -g_CurrentPlayer->headlook.f[0], -g_CurrentPlayer->headlook.f[1], -g_CurrentPlayer->headlook.f[2], g_CurrentPlayer->headup.f[0], g_CurrentPlayer->headup.f[1], g_CurrentPlayer->headup.f[2]);
    mtx4MultInPlace(&rot, &view);
    mtx4LoadYRotation((360.0f - g_CurrentPlayer->vv_theta) * DegToRad1Fact(1), &rot);
    mtx4MultInPlace(&rot, &view);

    g_CurrentPlayer->bond.applied_view.f[0] = view.m[2][0];
    g_CurrentPlayer->bond.applied_view.f[1] = view.m[2][1];
    g_CurrentPlayer->bond.applied_view.f[2] = view.m[2][2];

    g_CurrentPlayer->bond.applied_view2.f[0] = view.m[1][0];
    g_CurrentPlayer->bond.applied_view2.f[1] = view.m[1][1];
    g_CurrentPlayer->bond.applied_view2.f[2] = view.m[1][2];

}


f32 playerGetFloorY(StandTile *tile, f32 x, f32 z)
{
    f32 ret;

    if (g_CurrentPlayer->tankMounted != NULL)
    {
        ObjectRecord * obj = ((PropRecord *)g_CurrentPlayer->tankMounted)->obj;
        PropRecord *p = obj->prop;

        ret = stanGetFloorY(p->stan, p->pos.x, p->pos.z);

        ret += g_CurrentPlayer->tankYOffset;
    }
    else
    {
        ret = stanGetFloorY(tile, x, z);
    }

    return ret;
}


/**
 * US Address 0x7F080DF8.
 * EU Address 0x7F080E9C.
 */
void playerUpdateVertical(s32 use_stanHeight, f32 stanHeight_offset)
{
    s32 i;
    f32 floorY;
    StandTile *stan;
    f32 collision_radius;
    f32 height;
    f32 always_30;
    f32 tankfloorY;
    f32 newmanground;
    f32 newfallspeed;
    f32 ftemp2;
    f32 gravity;


    if (g_CurrentPlayer->tankDriving == 1)
    {
        g_CurrentPlayer->stanHeight = playerGetFloorY(
            g_CurrentPlayer->bond.current_tile_ptr,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2]);

        g_CurrentPlayer->sumGround = g_CurrentPlayer->manGround / (1.0f - TANK_Y_SMOOTHING);

        for (i=0; i<g_ClockTimer; i++)
        {
            g_CurrentPlayer->sumGround = (g_CurrentPlayer->sumGround * TANK_Y_SMOOTHING) + g_CurrentPlayer->stanHeight;
        }

        g_CurrentPlayer->manGround = g_CurrentPlayer->sumGround * (1.0f - TANK_Y_SMOOTHING);
    }
    else
    {
        if (use_stanHeight != 0)
        {
            g_CurrentPlayer->stanHeight = g_CurrentPlayer->stanHeight + stanHeight_offset;

            tankfloorY = playerGetFloorY(
                g_CurrentPlayer->bond.current_tile_ptr,
                g_CurrentPlayer->bond.collision_position.f[0],
                g_CurrentPlayer->bond.collision_position.f[2]);

            if (g_CurrentPlayer->stanHeight < tankfloorY)
            {
                g_CurrentPlayer->stanHeight = tankfloorY;
            }
        }
        else
        {
            stan = g_CurrentPlayer->bond.current_tile_ptr;

            playerGetCollisionSize(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

            floorY = playerGetFloorY(
                g_CurrentPlayer->bond.current_tile_ptr,
                g_CurrentPlayer->bond.collision_position.f[0],
                g_CurrentPlayer->bond.collision_position.f[2]);

            // Another error checking block, it seems this condition is almost never triggered in the game.
            if (stanTestCircleMaxHeight(
                &stan,
                g_CurrentPlayer->bond.collision_position.f[0],
                g_CurrentPlayer->bond.collision_position.f[2],
                collision_radius,
                playerGetEyeHeight(g_CurrentPlayer) + floorY) >= 0)
            {
                if (floorY < g_CurrentPlayer->stanHeight)
                {
                    floorY = g_CurrentPlayer->stanHeight;
                }
            }

            g_CurrentPlayer->stanHeight = floorY;
        }

        if ((g_CurrentPlayer->deltaPos.y >= 0.0f) || (g_CurrentPlayer->manGround < g_CurrentPlayer->stanHeight))
        {
            g_CurrentPlayer->sumGround = g_CurrentPlayer->manGround / (1.0f - TANK_Y_SMOOTHING);

            for (i=0; i<g_ClockTimer; i++)
            {
                g_CurrentPlayer->sumGround = (g_CurrentPlayer->sumGround * TANK_Y_SMOOTHING) + g_CurrentPlayer->stanHeight;
            }

            if (g_CurrentPlayer->manGround < g_CurrentPlayer->stanHeight)
            {
                g_CurrentPlayer->manGround = g_CurrentPlayer->sumGround * (1.0f - TANK_Y_SMOOTHING);
            }
        }

        if (g_CurrentPlayer->stanHeight < g_CurrentPlayer->manGround)
        {
            newfallspeed = g_CurrentPlayer->deltaPos.y;
            newmanground = g_CurrentPlayer->manGround;

            if ((g_CheatTurboMode) && (g_ForceBondMoveOffset.f[0] == 0.0f) && (g_ForceBondMoveOffset.f[2] == 0.0f))
            {
                gravity = 1.388889f;
            }
            else
            {
                gravity = 0.27777779f;
            }

            ftemp2 = newfallspeed - (g_GlobalTimerDelta * gravity);
            newmanground += (g_GlobalTimerDelta * (newfallspeed + ftemp2) * 0.5f);
            newfallspeed = ftemp2;

            if (newmanground < g_CurrentPlayer->stanHeight)
            {
                newmanground = g_CurrentPlayer->stanHeight;
                newfallspeed = -sqrtf((g_CurrentPlayer->deltaPos.y * g_CurrentPlayer->deltaPos.y) + (((2.0f * (g_CurrentPlayer->manGround - g_CurrentPlayer->stanHeight) * 0.27777779f) / 60.0f) * 60.0f));
            }

            g_CurrentPlayer->manGround = newmanground;
            g_CurrentPlayer->deltaPos.y = newfallspeed;
        }

        if ((g_CurrentPlayer->deltaPos.y < 0.0f) && (g_CurrentPlayer->manGround <= g_CurrentPlayer->stanHeight))
        {
            if (g_CurrentPlayer->deltaPos.y < -13.333333f)
            {
                g_CurrentPlayer->crouchTime240 = LANDING_DIP_FRAMES;
                g_CurrentPlayer->vertical_bounce_adjust = -90.0f;
            }
            else if (g_CurrentPlayer->deltaPos.y < -5.0f)
            {
                g_CurrentPlayer->crouchTime240 = LANDING_DIP_FRAMES;
                g_CurrentPlayer->vertical_bounce_adjust = ((-5.0f - g_CurrentPlayer->deltaPos.y) * -90.0f) / 8.333333f;
            }

            g_CurrentPlayer->deltaPos.y = 0.0f;
        }
    }

    for (i=0; i<g_ClockTimer; i++)
    {
        if (g_CurrentPlayer->crouchTime240 > 0)
        {
            g_CurrentPlayer->sumCrouch = (g_CurrentPlayer->sumCrouch * CROUCH_DAMP) + g_CurrentPlayer->vertical_bounce_adjust;
            g_CurrentPlayer->crouchTime240 += -1;
        }
        else
        {
            if (g_CurrentPlayer->vertical_bounce_adjust < 0.0f)
            {
                g_CurrentPlayer->vertical_bounce_adjust -= CROUCH_FALL_STEP;

                if (0.0f <= g_CurrentPlayer->vertical_bounce_adjust)
                {
                    g_CurrentPlayer->vertical_bounce_adjust = 0.0f;
                }
            }

            g_CurrentPlayer->sumCrouch = (g_CurrentPlayer->sumCrouch * CROUCH_DAMP) + g_CurrentPlayer->vertical_bounce_adjust;
        }
    }

    g_CurrentPlayer->crouchHeight = g_CurrentPlayer->sumCrouch * CROUCH_DAMP_INV;
}


/**
 * Address 0x7F081478 (NTSC).
 * Address 0x7F08151C (PAL).
*/
void playerUpdatePosFromCollision(void)
{
    f32 height;
    s32 i;
    StandTile *tile;

    g_CurrentPlayer->eyeheight = (g_CurrentPlayer->headpos.f[1] * g_playerPerm->player_perspective_height) + 7.0f;

    height = g_CurrentPlayer->eyeheight +
        ((g_CurrentPlayer->crouchHeight + g_CurrentPlayer->ducking_height_offset) * g_playerPerm->player_perspective_height);

    if (height < 30.0f)
    {
        height = 30.0f;
    }

    g_CurrentPlayer->bond.collision_position.f[1] = g_CurrentPlayer->manGround + height;

    if (((g_CameraMode != CAMERAMODE_DEATH_CAM_SP) && (g_CameraMode != CAMERAMODE_DEATH_CAM_MP) && (g_CameraMode != CAMERAMODE_POSEND))
        || (g_CurrentPlayer->bodyModel == 0))
    {
        g_CurrentPlayer->bond.pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
        g_CurrentPlayer->bond.pos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
        g_CurrentPlayer->bond.pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2];
    }

    if (g_CurrentPlayer->bonddead != FALSE)
    {
        if (g_CurrentPlayer->lookDownDrop > 0.0f)
        {
            g_CurrentPlayer->lookDownDrop -= 0.25f;

            if (g_CurrentPlayer->lookDownDrop < 0.0f)
            {
                g_CurrentPlayer->lookDownDrop = 0.0f;
            }
        }
    }

    if (g_CurrentPlayer->vv_verta < 0.0f)
    {
        g_CurrentPlayer->bond.pos.f[1] += -(1.0f - g_CurrentPlayer->vv_cosverta) * g_CurrentPlayer->lookDownDrop;
    }

    tile = g_CurrentPlayer->bond.current_tile_ptr;

    stanWalkLine(
        &tile,
        g_CurrentPlayer->bond.collision_position.f[0],
        g_CurrentPlayer->bond.collision_position.f[2],
        g_CurrentPlayer->bond.pos.f[0],
        g_CurrentPlayer->bond.pos.f[2]);


    g_CurrentPlayer->bond.current_tile_ptr_for_portals = tile;

    g_CurrentPlayer->bond.pos3.f[0] = g_CurrentPlayer->bond.pos.f[0];
    g_CurrentPlayer->bond.pos3.f[2] = g_CurrentPlayer->bond.pos.f[2];

    g_CurrentPlayer->bond.pos3.f[1] = playerGetFloorY(
        g_CurrentPlayer->bond.current_tile_ptr_for_portals,
        g_CurrentPlayer->bond.pos.f[0],
        g_CurrentPlayer->bond.pos.f[2]);

    g_CurrentPlayer->prop->stan = g_CurrentPlayer->bond.current_tile_ptr;

    g_CurrentPlayer->prop->pos.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
    g_CurrentPlayer->prop->pos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
    g_CurrentPlayer->prop->pos.f[2] = g_CurrentPlayer->bond.collision_position.f[2];

#if defined(VERSION_EU)
#define EYE_DAMP 0.881200015545f
#else
#define EYE_DAMP 0.9f
#endif
    for (i=0; i<g_ClockTimer; i++)
    {
        g_CurrentPlayer->dampEyeSum.f[0] = (EYE_DAMP * g_CurrentPlayer->dampEyeSum.f[0]) + g_CurrentPlayer->bond.pos.f[0];
        g_CurrentPlayer->dampEyeSum.f[1] = (EYE_DAMP * g_CurrentPlayer->dampEyeSum.f[1]) + g_CurrentPlayer->bond.pos.f[1];
        g_CurrentPlayer->dampEyeSum.f[2] = (EYE_DAMP * g_CurrentPlayer->dampEyeSum.f[2]) + g_CurrentPlayer->bond.pos.f[2];
    }

#if defined(VERSION_EU)
#define EYE_DAMP_INV 0.118799984455f
#else
#define EYE_DAMP_INV 0.100000024f
#endif
    g_CurrentPlayer->dampEye.x = g_CurrentPlayer->dampEyeSum.f[0] * EYE_DAMP_INV;
    g_CurrentPlayer->dampEye.y = g_CurrentPlayer->dampEyeSum.f[1] * EYE_DAMP_INV;
    g_CurrentPlayer->dampEye.z = g_CurrentPlayer->dampEyeSum.f[2] * EYE_DAMP_INV;
}


/**
 * Fixes vv_verta within -90 and +90.
 * Updates vv_costheta, vv_sintheta, vv_verta360, vv_cosverta, vv_sinverta, bond.theta_transform.
 *
 * Perfect Dark function bmoveUpdateVerta.
*/
void playerUpdateVertaTheta(void)
{
    while (g_CurrentPlayer->vv_verta < -180.0f)
    {
        g_CurrentPlayer->vv_verta += 360.0f;
    }

    while (g_CurrentPlayer->vv_verta >= 180.0f)
    {
        g_CurrentPlayer->vv_verta -= 360.0f;
    }

    if (g_CurrentPlayer->vv_verta > 90.0f)
    {
        g_CurrentPlayer->vv_verta = 90.0f;
    }
    else if (g_CurrentPlayer->vv_verta < -90.0f)
    {
        g_CurrentPlayer->vv_verta = -90.0f;
    }

    g_CurrentPlayer->vv_costheta = cosf(g_CurrentPlayer->vv_theta * DegToRad1Fact(1));
    g_CurrentPlayer->vv_sintheta = sinf(g_CurrentPlayer->vv_theta * DegToRad1Fact(1));

    g_CurrentPlayer->vv_verta360 = g_CurrentPlayer->vv_verta;
    if (g_CurrentPlayer->vv_verta360 < 0.0f)
    {
        g_CurrentPlayer->vv_verta360 += 360.0f;
    }

    g_CurrentPlayer->vv_cosverta = cosf(g_CurrentPlayer->vv_verta360 * DegToRad1Fact(1));

    g_CurrentPlayer->bond.theta_transform.f[0] = -g_CurrentPlayer->vv_sintheta;
    g_CurrentPlayer->bond.theta_transform.f[1] = 0;
    g_CurrentPlayer->bond.theta_transform.f[2] = g_CurrentPlayer->vv_costheta;
}


/**
 * NTSC Address 0x7F084648.
 * NTSC-J address 0x7F084CF8.
 *
 * Met by Saint Jon The Archangel in the writhing pits of hell, the beast was
 * pinned, prone on the floor. He had taken many forms throughout the ages.
 * From the creator of man to the far reaches of the universe, his perpetual
 * reincarnations reigned throughout space and time... until now. His form was
 * weak, the opposition strong. The time for atonement had finally come.
 *
 * Battered, torn, exposed, the beast slacked his gaping maw.
 *
 * "I'm sorry, Jon" he whispered.
 *
 * The archangel raised his flaming blade.
 *
 * "You are forgiven, Garfield"
 *
 * And the beast exhaled his last.
 *
 * 10 months of off and on work to match.
 * Thanks Trevor.
 * - Bethany Burns
 */
void playerMove(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons)
{
    coord3d move_offset;
    f32 ftemp;
    f32 theta;
    f32 animSideways;
    s32 i;
    f32 maxspeed;
    s32 use_stanHeight;
    f32 climbOffset;

    move_offset = g_DefaultMoveBondOffset;

    use_stanHeight = 0;
    maxspeed = 0.0f;
    climbOffset = 0.0f;


    if (g_CurrentPlayer->forceDisarmTicks > 0)
    {
        g_CurrentPlayer->forceDisarmTicks++;
        if (g_CurrentPlayer->forceDisarmTicks >= 4)
        {
            g_CurrentPlayer->forceDisarmTicks = 0;
            g_CurrentPlayer->handModelLocked[GUNLEFT] = 0;
            g_CurrentPlayer->handModelLocked[GUNRIGHT] = 0;
            gunUnequip(GUNLEFT, gunGetWeapon(GUNLEFT));
            gunUnequip(GUNRIGHT, gunGetWeapon(GUNRIGHT));
        }
    }

    playerSetCameraModeRaw(0);
    playerTickDamageAndHealth();
    playerTickExplode();
    playerProcessInput(stick_x, stick_y, buttons, oldbuttons);

    if (lvGetControlsLocked())
    {
        pauseStopSounds();
    }

    if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
    {
        watchTick();
    }

    /*
        Apply bondshotspeed vector to speedforwards scalar and speedsideways scalar.
        Crouching applies a 50% base speed reduction before applying boost.
        Bond can't be boosted while in the tank.
    */
    if (g_CurrentPlayer->tankDriving == 0)
    {
        // This `if` block is Perfect Dark bwalkApplyCrouchSpeed.
        if (currentPlayerGetCrouchPos() == CROUCH_SQUAT)
        {
            g_CurrentPlayer->speedforwards *= 0.5f;
            g_CurrentPlayer->speedsideways *= 0.5f;
        }

        if ((g_CurrentPlayer->bondshotspeed.f[0] != 0.0f) || (g_CurrentPlayer->bondshotspeed.f[2] != 0.0f))
        {
            // boost forwards
            f32 shotboost_forward;
            // boost sideways
            f32 shotboost_sideways;
            f32 shotboost_norm;

            // Assigning these two variables is done in Perfect Dark bmove0f0cba88.
            shotboost_forward =
                (-g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->vv_sintheta)
                + (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->vv_costheta);
            shotboost_sideways =
                (-g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->vv_costheta)
                - (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->vv_sintheta);

            shotboost_norm = sqrtf(
                (g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->bondshotspeed.f[0]) +
                (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->bondshotspeed.f[2]));

            g_CurrentPlayer->speedforwards += shotboost_forward;
            g_CurrentPlayer->speedsideways += shotboost_sideways;

            // 3: x,y,z components of bondshotspeed
            for (i=0; i<3; i++)
            {
                if (g_CurrentPlayer->bondshotspeed.f[i] != 0.0f)
                {
                    if (g_CurrentPlayer->bondshotspeed.f[i] > 0.0f)
                    {
                        g_CurrentPlayer->bondshotspeed.f[i] -=
                            ((0.06666667f * g_GlobalTimerDelta * g_CurrentPlayer->bondshotspeed.f[i]) / shotboost_norm);

                        if (g_CurrentPlayer->bondshotspeed.f[i] < 0.0f)
                        {
                            g_CurrentPlayer->bondshotspeed.f[i] = 0.0f;
                        }
                    }
                    else if (g_CurrentPlayer->bondshotspeed.f[i] < 0.0f)
                    {
                        g_CurrentPlayer->bondshotspeed.f[i] -=
                            ((0.06666667f * g_GlobalTimerDelta * g_CurrentPlayer->bondshotspeed.f[i]) / shotboost_norm);

                        if (g_CurrentPlayer->bondshotspeed.f[i] > 0.0f)
                        {
                            g_CurrentPlayer->bondshotspeed.f[i] = 0.0f;
                        }
                    }
                }
            }
        }
    }

    /**
     * This section updates the tank turret horizontal position (turning left and right),
     * as well as turning the tank left and right.
    */
    if (g_CurrentPlayer->tankDriving == 1)
    {
        coord3d check_collision_p1;
        coord3d check_collision_p2;
        s32 step;
        f32 curTankAngleRad;
        f32 tankChangeInAngle;
        f32 savedTurretAngle;
        coord3d tank_collision_pt1;
        coord3d tank_collision_pt2;
        f32 tank_collision_dx;
        f32 tank_collision_dz;

        tankChangeInAngle = DegToRad1Fact(g_CurrentPlayer->speedtheta * g_GlobalTimerDelta) * 3.5f;
        curTankAngleRad = g_Tank.yaw + tankChangeInAngle;

        if (curTankAngleRad >= M_TAU_F)
        {
            curTankAngleRad -= M_TAU_F;
        }

        if (curTankAngleRad < 0.0f)
        {
            curTankAngleRad += M_TAU_F;
        }

        if (tankTestCollision(
            &g_CurrentPlayer->bond.collision_position,
            g_CurrentPlayer->bond.current_tile_ptr,
            curTankAngleRad,
            &check_collision_p1,
            &check_collision_p2))
        {
            g_Tank.yaw = curTankAngleRad;
        }
        else
        {
            f32 edgeDist2b;
            f32 edgeDist2a;
            f32 pushX;
            f32 pushZ;
            f32 pushDist;
            f32 edgeDist1;
            f32 tank_collision_norm;
            f32 playerDist;
            f32 normX;
            f32 normZ;
            f32 normDist;

            pushDist = -1;
            normDist = -1;

            stanGetCollisionEdge(&tank_collision_pt1, &tank_collision_pt2);

            tank_collision_dx = (tank_collision_pt2.f[0] - tank_collision_pt1.f[0]);
            tank_collision_dz = (tank_collision_pt2.f[2] - tank_collision_pt1.f[2]);

            tank_collision_norm = 1.0f / sqrtf((tank_collision_dx * tank_collision_dx) + (tank_collision_dz * tank_collision_dz));

            tank_collision_dx *= tank_collision_norm;
            tank_collision_dz *= tank_collision_norm;
            pushX = tank_collision_dz;
            pushZ = -tank_collision_dx;

            playerDist =
                ((g_CurrentPlayer->bond.collision_position.f[0] - check_collision_p2.f[0]) * pushX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - check_collision_p2.f[2]) * pushZ);

            if (playerDist < 0.0f)
            {
                playerDist = -playerDist;
                pushX = -pushX;
                pushZ = -pushZ;
            }

            edgeDist1 =
                ((g_CurrentPlayer->bond.collision_position.f[0] - tank_collision_pt1.f[0]) * pushX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - tank_collision_pt1.f[2]) * pushZ);

            if (edgeDist1 < playerDist)
            {
                pushDist = playerDist - edgeDist1;
            }

            tank_collision_dx = check_collision_p2.f[0] - check_collision_p1.f[0];
            tank_collision_dz = check_collision_p2.f[2] - check_collision_p1.f[2];

            tank_collision_norm = 1.0f / sqrtf((tank_collision_dx * tank_collision_dx) + (tank_collision_dz * tank_collision_dz));

            tank_collision_dx *= tank_collision_norm;
            tank_collision_dz *= tank_collision_norm;
            normX = tank_collision_dz;
            normZ = -tank_collision_dx;

            playerDist =
                ((g_CurrentPlayer->bond.collision_position.f[0] - check_collision_p2.f[0]) * normX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - check_collision_p2.f[2]) * normZ);

            if (playerDist < 0.0f)
            {
                playerDist = -playerDist;
                normX = -normX;
                normZ = -normZ;
            }

            edgeDist2a =
                ((g_CurrentPlayer->bond.collision_position.f[0] - tank_collision_pt1.f[0]) * normX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - tank_collision_pt1.f[2]) * normZ);
            edgeDist2b =
                ((g_CurrentPlayer->bond.collision_position.f[0] - tank_collision_pt2.f[0]) * normX) +
                ((g_CurrentPlayer->bond.collision_position.f[2] - tank_collision_pt2.f[2]) * normZ);

            if (edgeDist2b < edgeDist2a)
            {
                edgeDist2a = edgeDist2b;
            }

            if (edgeDist2a < playerDist)
            {
                normDist = playerDist - edgeDist2a;
            }

            if ((normDist >= 0.0f) && ((normDist < pushDist) || (pushDist < 0.0f)))
            {
                pushX = normX;
                pushZ = normZ;
                pushDist = normDist;
            }

            if (pushDist >= 0.0f)
            {
                move_offset.f[0] = pushDist * pushX * 1.01f;
                move_offset.f[2] = pushDist * pushZ * 1.01f;

                playerMoveWithCollision(&move_offset, 1);

                move_offset.f[0] = 0.0f;
                move_offset.f[2] = 0.0f;

                if (tankTestCollision(
                    &g_CurrentPlayer->bond.collision_position,
                    g_CurrentPlayer->bond.current_tile_ptr,
                    curTankAngleRad,
                    &check_collision_p1,
                    &check_collision_p2))
                {
                    g_Tank.yaw = curTankAngleRad;
                }
                else
                {
                    tankChangeInAngle = 0.0f;
                }
            }
            else
            {
                tankChangeInAngle = 0.0f;
            }
        }

        savedTurretAngle = g_Tank.turretYaw;
        g_Tank.turretAngle += g_Tank.turretTurn;
        if (g_Tank.turretAngle >= M_TAU_F)
        {
            g_Tank.turretAngle -= M_TAU_F;
        }

        if (g_Tank.turretAngle < 0.0f)
        {
            g_Tank.turretAngle += M_TAU_F;
        }

        ftemp = (DegToRad1Fact(g_CurrentPlayer->speedtheta * 3.5f) * 4.0f) + g_Tank.turretAngle;

        if (ftemp < 0.0f)
        {
            ftemp += M_TAU_F;
        }

        if (ftemp >= M_TAU_F)
        {
            ftemp -= M_TAU_F;
        }

        if ((ftemp - g_Tank.turretYaw) >= M_PI_F)
        {
            ftemp -= M_TAU_F;
        }
        else if ((ftemp - g_Tank.turretYaw) < -M_PI_F)
        {
            ftemp += M_TAU_F;
        }

        for (step=0; step<g_ClockTimer; step++)
        {
            g_Tank.turretYawSum = ((TANKUPDATEROTATION_SCALE) * g_Tank.turretYawSum) + ftemp;
        }

        g_Tank.turretYaw = g_Tank.turretYawSum * (1.0f - TANKUPDATEROTATION_SCALE);

        if (g_Tank.turretYaw >= M_TAU_F)
        {
            g_Tank.turretYaw -= M_TAU_F;
            g_Tank.turretYawSum = g_Tank.turretYaw / (1.0f - TANKUPDATEROTATION_SCALE);
        }

        if (g_Tank.turretYaw < 0.0f)
        {
            g_Tank.turretYaw += M_TAU_F;
            g_Tank.turretYawSum = g_Tank.turretYaw / (1.0f - TANKUPDATEROTATION_SCALE);
        }

        if (tankTestCollisionAt(
            &g_CurrentPlayer->bond.collision_position,
            g_CurrentPlayer->bond.current_tile_ptr,
            g_Tank.yaw) == 0)
        {
            g_Tank.turretYaw = savedTurretAngle;
            g_Tank.turretYawSum = g_Tank.turretYaw / (1.0f - TANKUPDATEROTATION_SCALE);
            g_Tank.turretAngle = savedTurretAngle;
        }

        if (g_CurrentPlayer->tankMounted != NULL)
        {
            // sp 0x300
            struct TankRecord *tankRec;
            coord3d tank_move_offset;
            Mtxf rot;

            tankRec = (struct TankRecord *)g_CurrentPlayer->tankMounted->obj;

            tank_move_offset.f[1] = 0.0f;
            tank_move_offset.f[0] = g_Tank.turretOffset.f[0];
            tank_move_offset.f[2] = g_Tank.turretOffset.f[2];

            mtx4LoadYRotation(tankChangeInAngle, &rot);
            mtx4RotateVecInPlace(&rot, &tank_move_offset);
            tankUpdateTurretOffset();


            tank_move_offset.f[1] = 0.0f;
            tank_move_offset.f[0] = g_Tank.turretOffset.f[0] - tank_move_offset.f[0];
            tank_move_offset.f[2] = g_Tank.turretOffset.f[2] - tank_move_offset.f[2];

            mtx4LoadYRotation(M_TAU_F - g_Tank.yaw, &rot);
            mtx4ScaleRows012(tankRec->model->scale, &rot);
            mtx4RotateVecInPlace(&rot, &tank_move_offset);
            playerMoveWithCollision(&tank_move_offset, 1);
        }

        if (g_ClockTimer > 0) {
            for (i=0; i<g_ClockTimer; i++)
            {
                g_Tank.turretTurnSpeed = (TANKUPDATEROTATION_SCALE * g_Tank.turretTurnSpeed) + (g_Tank.turretTurn / g_GlobalTimerDelta);
            }
        }

        ftemp = g_Tank.turretTurnSpeed * (1.0f - TANKUPDATEROTATION_SCALE);

        g_CurrentPlayer->vv_theta = (
            g_Tank.yaw +
            g_Tank.turretYaw +
            ((DegToRad1Fact(g_CurrentPlayer->speedtheta * 3.5f)) * (4.0f)) +
            (ftemp * 4.0f)
            ) * 360.0f / M_TAU_F;

        while (g_CurrentPlayer->vv_theta < 0.0f)
        {
            g_CurrentPlayer->vv_theta += 360.0f;
        }
        while (g_CurrentPlayer->vv_theta >= 360.0f)
        {
            g_CurrentPlayer->vv_theta -= 360.0f;
        }
    }
    else
    {
        theta = g_CurrentPlayer->vv_theta + (g_CurrentPlayer->speedtheta * g_GlobalTimerDelta * 3.5f);

        while (theta < 0.0f)
        {
            theta += 360.0f;
        }
        while (theta >= 360.0f)
        {
            theta -= 360.0f;
        }

        g_CurrentPlayer->vv_theta = theta;
    }

    playerUpdateVertaTheta();

    // Handle crouching, and animation between standing and crouching.
    // Add basic block to declare local variables at the correct stack position.
    {
        f32 targetDuck;

        targetDuck = 0.0f;
        if (currentPlayerGetCrouchPos() == CROUCH_SQUAT)
        {
            targetDuck = FULL_CROUCH_OFFSET;
        }
        else if (currentPlayerGetCrouchPos() == CROUCH_HALF)
        {
            targetDuck = -60.0f;
        }
        else
        {
            // removed?
            currentPlayerGetCrouchPos();
        }

        if (targetDuck != g_CurrentPlayer->ducking_height_offset)
        {
            physMoveTowards(
                &g_CurrentPlayer->ducking_height_offset,
                targetDuck,
                &g_CurrentPlayer->crouchSpeed,
                CHR_OBJ_ACCEL_SPEED_FACTOR,
                CHR_OBJ_ACCEL_SPEED_FACTOR,
                CHR_OBJ_MAXSPEED, g_ClockTimer);
        }

        if (targetDuck == g_CurrentPlayer->ducking_height_offset)
        {
            g_CurrentPlayer->crouchSpeed = 0.0f;
        }
    }

    /**
     * Update forwards/backwards movement.
    */
    if (g_CurrentPlayer->tankDriving == 1)
    {
        /**
         * This section handles the forward/backwards movement of the tank.
        */

        Mtxf seatRot;
        coord3d seatPos;
        f32 turretYawDeg;
        f32 pitch;
        s32 step;
        f32 yawDiff;
        f32 tank_engine_utilization_percent;
        struct TankRecord *tank_obj;

        /**
         * Check to see if Bond is just now entering the tank.
         * If so, initialize the tank prop.
         * This also handles spinning Bond around (if required) to face the same direction as the turret.
        */
        if (g_Tank.enterAudioState == TANK_RUN_STATE_NOT_RUNNING)
        {
            if (g_CurrentPlayer->tankMounted != NULL)
            {
                tank_obj = (struct TankRecord *)g_CurrentPlayer->tankMounted->obj;
                mtx4LoadYRotation(M_TAU_F - g_Tank.yaw, &seatRot);
                mtx4ScaleRows012(tank_obj->model->scale, &seatRot);

                seatPos.f[0] = g_Tank.turretOffset.f[0];
                seatPos.f[1] = g_Tank.turretOffset.f[1];
                seatPos.f[2] = g_Tank.turretOffset.f[2];
                mtx4RotateVecInPlace(&seatRot, &seatPos);

                seatPos.f[0] += tank_obj->runtime_pos.f[0];
                seatPos.f[1] += tank_obj->runtime_pos.f[1];
                seatPos.f[2] += tank_obj->runtime_pos.f[2];

                turretYawDeg = ((g_Tank.yaw + g_Tank.turretYaw) * 360.0f) / M_TAU_F;
                pitch = g_CurrentPlayer->vv_verta;
                if (pitch < -20.0f)
                {
                    pitch = -20.0f;
                }

                g_Tank.enterProgress += g_GlobalTimerDelta / 45.0f;
                if (g_Tank.enterProgress >= 1.0f)
                {
                    g_Tank.enterProgress = 1.0f;
                }

                g_Tank.enterRemain = (cosf(g_Tank.enterProgress * M_TAU_F * 0.5f) + 1.0f) * 0.5f;

                g_CurrentPlayer->vv_verta =
                    (g_Tank.enterRemain * g_Tank.enterPitchDeg)
                    + ((1.0f - g_Tank.enterRemain) * pitch);

                yawDiff = turretYawDeg - g_Tank.enterYawDeg;
                if (yawDiff > 180.0f)
                {
                    turretYawDeg -= 360.0f;
                }
                if (yawDiff < -180.0f)
                {
                    turretYawDeg += 360.0f;
                }

                g_CurrentPlayer->vv_theta =
                    (g_Tank.enterRemain * g_Tank.enterYawDeg)
                    + ((1.0f - g_Tank.enterRemain) * turretYawDeg);

                if (g_CurrentPlayer->vv_theta >= 360.0f)
                {
                    g_CurrentPlayer->vv_theta -= 360.0f;
                }

                if (g_CurrentPlayer->vv_theta < 0.0f)
                {
                    g_CurrentPlayer->vv_theta += 360.0f;
                }

                move_offset.f[0] = (
                        (g_Tank.enterRemain * g_Tank.enterPos.f[0]) +
                        ((1.0f - g_Tank.enterRemain) * seatPos.f[0])
                    ) -
                    g_CurrentPlayer->bond.collision_position.f[0];

                move_offset.f[1] = 0.0f;

                move_offset.f[2] = (
                    (g_Tank.enterRemain * g_Tank.enterPos.f[2]) +
                    ((1.0f - g_Tank.enterRemain) * seatPos.f[2])
                    ) -
                    g_CurrentPlayer->bond.collision_position.f[2];
            }

            if (!(g_Tank.enterProgress >= 1.0f))
            {
            }
            else
            {
                g_Tank.enterAudioState = TANK_RUN_STATE_STARTING;
            }
        }
        /**
         * Else, Bond has already entered the tank.
        */
        else
        {
            /**
             * There's an initial "starting" step.
            */
            if (g_Tank.enterAudioState == TANK_RUN_STATE_STARTING)
            {
                g_Tank.enterAudioState = TANK_RUN_STATE_RUNNING;
                if ((g_Tank.sfx[0] == NULL) && (lvGetControlsLocked() == 0))
                {
                    sndPlaySfx(g_musicSfxBufferPtr, TRUCK_START_SFX, &g_Tank.sfx[0]);
                }

                sndCreatePostEvent(g_Tank.sfx[0], 8, 0x61A8);
                g_Tank.engineVolume = 0x61A8;
            }
            /**
             * Else Bond has fully entered the tank, and the engine is running.
             * Update turret vertical angle.
             * Update engine sound effect volume based on current tank speed.
            */
            else
            {
                f32 tank_scaled_speedforwards;
                f32 tank_scaled_speedtheta;
                f32 tank_vertical_angle;

                tank_scaled_speedforwards = g_CurrentPlayer->speedforwards / TANK_MAX_SPEED;
                tank_scaled_speedtheta = g_CurrentPlayer->speedtheta / 0.3f;

                if (tank_scaled_speedforwards < 0.0f)
                {
                    tank_scaled_speedforwards = -tank_scaled_speedforwards;
                }
                if (tank_scaled_speedtheta < 0.0f)
                {
                    tank_scaled_speedtheta = -tank_scaled_speedtheta;
                }

                tank_engine_utilization_percent = tank_scaled_speedforwards;
                if (tank_scaled_speedforwards < tank_scaled_speedtheta)
                {
                    tank_engine_utilization_percent = tank_scaled_speedtheta;
                }

                if (tank_engine_utilization_percent > 0.0f)
                {
                    if (tank_engine_utilization_percent > 1.0f)
                    {
                        tank_engine_utilization_percent = 1.0f;
                    }

                    if (g_Tank.sfx[1] == NULL)
                    {
                        if (lvGetControlsLocked() == 0)
                        {
                            sndPlaySfx(g_musicSfxBufferPtr, TANK_SFX, &g_Tank.sfx[1]);
                        }
                    }

                    if (g_Tank.sfx[1] != NULL)
                    {
                        s32 sfxPitch;

                        sfxPitch = 0x7FFF;
                        if (tank_engine_utilization_percent < 0.15f)
                        {
                            sfxPitch = (s32) ((tank_engine_utilization_percent * 20000.0f) / 0.15f);
                        }
                        else if (tank_engine_utilization_percent < 0.9f)
                        {
                            sfxPitch = (s32) ((((tank_engine_utilization_percent - 0.15f) * 12767.0f) / 0.75f) + 20000.0f);
                        }

                        sndCreatePostEvent(g_Tank.sfx[1], 8, sfxPitch);
                    }
                }
                else
                {
                    if (g_Tank.sfx[1] != NULL)
                    {
                        if (sndGetPlayingState(g_Tank.sfx[1]) != 0)
                        {
                            sndDeactivate(g_Tank.sfx[1]);
                        }
                    }
                }

                if (g_Tank.sfx[0] == NULL)
                {
                    if (lvGetControlsLocked() == 0)
                    {
                        sndPlaySfx(g_musicSfxBufferPtr, TRUCK_RUN_SFX, &g_Tank.sfx[0]);
                    }
                }

                if (g_Tank.sfx[0] != NULL)
                {
                    g_Tank.engineVolume = 0x7FFF;
                    if (tank_engine_utilization_percent < 0.9f)
                    {
                        g_Tank.engineVolume = (s32) (((tank_engine_utilization_percent * 7767.0f) / 0.9f) + 25000.0f);
                    }

                    sndCreatePostEvent(g_Tank.sfx[0], 8, g_Tank.engineVolume);
                }

                if (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
                {
                    tank_vertical_angle = g_CurrentPlayer->bodyAimPitch;
                    tank_vertical_angle += 0.17453294f; /* should be DegToRad1Fact(10), but that yields 0.17453293f */
                }
                else
                {
                    tank_vertical_angle = g_Tank.turretPitch;
                }

                if (tank_vertical_angle > DegToRad1Fact(25))
                {
                    tank_vertical_angle = DegToRad1Fact(25);
                }

                /* -0.087266468f should be DegToRad1Fact(-5), but that yields -0.0872664600611 */
                if (tank_vertical_angle < -0.087266468f)
                {
                    tank_vertical_angle = -0.087266468f;
                }

                for (step=0; step<g_ClockTimer; step++)
                {
                    g_Tank.turretPitchSum = (TANKTURRETVERTICALANGLERELATED_SCALE * g_Tank.turretPitchSum) + tank_vertical_angle;
                }

                g_Tank.turretPitch = g_Tank.turretPitchSum * (1.0f - TANKTURRETVERTICALANGLERELATED_SCALE);
            }

        }

        g_CurrentPlayer->bondbreathing -= (0.750f * g_GlobalTimerDelta) / 2700.0f;

        if (g_CurrentPlayer->bondbreathing < 0.0f)
        {
            g_CurrentPlayer->bondbreathing = 0.0f;
        }

        playerTickMoveAnim(0.0f, 0.0f, 0.0f);

        move_offset.f[0] += g_CurrentPlayer->speedforwards * sinf(M_TAU_F - g_Tank.yaw) * g_GlobalTimerDelta;
        move_offset.f[2] += g_CurrentPlayer->speedforwards * cosf(M_TAU_F - g_Tank.yaw) * g_GlobalTimerDelta;

        playerMoveWithCollision(&move_offset, 1);

        if ((g_Tank.enterAudioState == TANK_RUN_STATE_RUNNING) && (g_ClockTimer > 0))
        {
            f32 calc_x;
            f32 calc_z;
            f32 calc_speedforwards;

#if defined(VERSION_EU)
            // Divide by zero check.
            if (g_GlobalTimerDelta == 0)
            {
            }
#endif

            calc_x = (g_CurrentPlayer->bond.collision_position.f[0] - g_CurrentPlayer->bondprevpos.f[0]) / g_GlobalTimerDelta;
            calc_z = (g_CurrentPlayer->bond.collision_position.f[2] - g_CurrentPlayer->bondprevpos.f[2]) / g_GlobalTimerDelta;
            calc_speedforwards = sqrtf((calc_x * calc_x) + (calc_z * calc_z));

            if (g_CurrentPlayer->speedforwards < 0.0f)
            {
                calc_speedforwards = -calc_speedforwards;
            }

            g_CurrentPlayer->speedforwards = calc_speedforwards;
        }
    }
    else // not in tank: g_CurrentPlayer->tankDriving != 1
    {
        f32 swayDx;
        f32 swayDz;
        f32 dist;
        f32 movedX;
        f32 movedZ;
        f32 start_collision_pos_x;
        f32 start_collision_pos_z;
        struct StandTile *tile;
        f32 speedforwards;
        f32 swayTargetZ;
        f32 speedsideways;
        f32 speedtheta;
        f32 startDist;
        f32 endDist;
        f32 swayTargetX;
        f32 nd; // canonical name
        f32 animScale;
        f32 moveRatio;
        StanSpecialTiles curLocus;
        LadderQuad bondCollision;
        f32 shorten; // canonical name
        f32 headpos_x;
        f32 headpos_z;
        struct StandTile *ladderTile1;
        struct StandTile *ladderTile2;
        f32 edgeNormX;
        f32 edgeNormZ;

        if ((g_Tank.sfx[0] != NULL) && (sndGetPlayingState(g_Tank.sfx[0]) != 0))
        {
            #if defined(VERSION_US)
            g_Tank.engineVolume -= (g_ClockTimer * 1000);
            #endif

            #if defined(VERSION_EU)
            g_Tank.engineVolume -= (s32)(1000.0f * g_GlobalTimerDelta);
            #endif

            if (g_Tank.engineVolume > 0)
            {
                sndCreatePostEvent(g_Tank.sfx[0], 8, g_Tank.engineVolume);
            }
            else
            {
                sndDeactivate(g_Tank.sfx[0]);
            }
        }

        if ((g_Tank.sfx[1] != NULL) && (sndGetPlayingState(g_Tank.sfx[1]) != 0))
        {
            sndDeactivate(g_Tank.sfx[1]);
        }


        animScale = (g_BondMoveAnimationSetup[1].speedMultiplier * 0.5f  * g_GlobalTimerDelta);
        animSideways  = g_CurrentPlayer->speedsideways * animScale;

        /*
            The following is similar to a block of Perfect Dark bwalk0f0c69b8.
        */

        swayTargetX = -g_CurrentPlayer->swaytarget * g_CurrentPlayer->bond.theta_transform.f[2];
        swayTargetZ = g_CurrentPlayer->swaytarget * g_CurrentPlayer->bond.theta_transform.f[0];

        swayDx = (swayTargetX) - g_CurrentPlayer->swayoffset0;
        swayDz = (swayTargetZ) - g_CurrentPlayer->swayoffset2;

        dist = (swayDx * swayDx) + (swayDz * swayDz);

        if (dist >= 100.0f)
        {
            swayDx *= 0.6f * 1.0f;
            swayDz *= 0.6f * 1.0f;
        }

        speedsideways = g_CurrentPlayer->speedsideways * MAX_SPEED_FACTOR;
        speedforwards = g_CurrentPlayer->speedforwards;
        speedtheta = g_CurrentPlayer->speedtheta * MAX_SPEED_FACTOR;

        if (speedsideways < 0.0f)
        {
            speedsideways = -speedsideways;
        }
        if (speedforwards < 0.0f)
        {
            speedforwards = -speedforwards;
        }
        if (speedtheta < 0.0f)
        {
            speedtheta = -speedtheta;
        }

        maxspeed = speedforwards;

#if defined(VERSION_EU)
        if (maxspeed < speedsideways && 1)
#else
        if (maxspeed < speedsideways)
#endif
        {
            maxspeed = speedsideways;
        }
        if (maxspeed < speedtheta)
        {
            maxspeed = speedtheta;
        }

        if (dist >= 0.1f && maxspeed < MAX_SPEED_FACTOR)
        {
            maxspeed = MAX_SPEED_FACTOR;
        }

        if (maxspeed >= 0.750f)
        {
            g_CurrentPlayer->bondbreathing += (maxspeed - 0.750f) * g_GlobalTimerDelta / 900.0f ;
        }
        else
        {
            g_CurrentPlayer->bondbreathing -= (0.750f - maxspeed) * g_GlobalTimerDelta / 2700.0f;
        }

        if (g_CurrentPlayer->bondbreathing < 0.0f)
        {
            g_CurrentPlayer->bondbreathing = 0.0f;
        }
        else if (g_CurrentPlayer->bondbreathing > 1.0f)
        {
            g_CurrentPlayer->bondbreathing = 1.0f;
        }

        // perfect dark call: bmove0f0cc654
        playerTickMoveAnim(maxspeed, g_CurrentPlayer->speedforwards, animSideways);

        headpos_x = g_CurrentPlayer->headpos.f[0];
        headpos_z = g_CurrentPlayer->headpos.f[2];

        move_offset.f[0] +=
            (
                (headpos_z * g_CurrentPlayer->bond.theta_transform.f[0]) -
                (headpos_x * g_CurrentPlayer->bond.theta_transform.f[2])
            ) * g_GlobalTimerDelta;

        move_offset.f[2] +=
            (
                (headpos_z * g_CurrentPlayer->bond.theta_transform.f[2]) +
                (headpos_x * g_CurrentPlayer->bond.theta_transform.f[0])
            ) * g_GlobalTimerDelta;


        move_offset.f[0] += swayDx;
        move_offset.f[2] += swayDz;

        start_collision_pos_x = g_CurrentPlayer->bond.collision_position.f[0];
        start_collision_pos_z = g_CurrentPlayer->bond.collision_position.f[2];
        tile = g_CurrentPlayer->bond.current_tile_ptr;

        if (g_CheatTurboMode)
        {
            move_offset.f[0] +=
                (
                    (g_CurrentPlayer->bond.theta_transform.f[0] * g_CurrentPlayer->speedforwards) -
                    (g_CurrentPlayer->bond.theta_transform.f[2] * g_CurrentPlayer->speedsideways)
                ) * g_GlobalTimerDelta * 10.0f;

            move_offset.f[2] +=
                (
                    (g_CurrentPlayer->bond.theta_transform.f[2] * g_CurrentPlayer->speedforwards) +
                    (g_CurrentPlayer->bond.theta_transform.f[0] * g_CurrentPlayer->speedsideways)
                ) * g_GlobalTimerDelta * 10.0f;
        }

        playerMoveWithCollision(&move_offset, (g_CurrentPlayer->swaytarget == 0.0f));

        stanCircleGetSpecial(
            &tile,
            start_collision_pos_x,
            start_collision_pos_z,
            g_CurrentPlayer->bond.collision_radius * 1.16f,
            &curLocus);

        /* almost never true */
        if (curLocus.ladder)
        {
            use_stanHeight = 1;
        }

        stanCircleGetSpecial(
            &g_CurrentPlayer->bond.current_tile_ptr,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2],
            g_CurrentPlayer->bond.collision_radius * 1.01f,
            &curLocus);

        /* almost never true */
        if (curLocus.ladder)
        {
            use_stanHeight = 1;
        }

        stanCircleGetSpecial(
            &g_CurrentPlayer->bond.current_tile_ptr,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2],
            g_CurrentPlayer->bond.collision_radius,
            &curLocus);

        /* almost always true */
        if (!curLocus.ladder)
        {
            stanCircleGetSpecial(
                &tile,
                start_collision_pos_x,
                start_collision_pos_z,
                g_CurrentPlayer->bond.collision_radius * 0.990099f,
                &curLocus);
        }

        /* almost never true */
        if (curLocus.ladder)
        {
            use_stanHeight = 1;
            stanGetLadderQuad(&ladderTile1, &ladderTile2, &bondCollision);

            if (g_CurrentPlayer->stanHeight <= bondCollision.high0.f[1])
            {
                f32 moveLen;
                f32 climbDir2Z;
                f32 climbDir2X;
                f32 climbDirZ;
                f32 climbDirX;

                edgeNormZ = bondCollision.low0.f[0] - bondCollision.low1.f[0];
                edgeNormX = bondCollision.low1.f[2] - bondCollision.low0.f[2];

                nd = sqrtf((edgeNormX * edgeNormX) + (edgeNormZ * edgeNormZ));

                edgeNormZ = edgeNormZ / nd;
                edgeNormX = edgeNormX / nd;

                startDist =
                    ((start_collision_pos_x - bondCollision.low0.f[0]) * edgeNormX) +
                    ((start_collision_pos_z - bondCollision.low0.f[2]) * edgeNormZ);

                endDist =
                    (((start_collision_pos_x + move_offset.f[0]) - bondCollision.low0.f[0]) * edgeNormX) +
                    (((start_collision_pos_z + move_offset.f[2]) - bondCollision.low0.f[2]) * edgeNormZ);

                if ((startDist * endDist) <= 0.0f)
                {
                    shorten = 0.0f;
                }
                else
                {
                    if (endDist < 0.0f)
                    {
                        startDist = -startDist;
                        endDist = -endDist;
                    }

                    if (startDist <= endDist)
                    {
                        shorten = 0.0f;
                    }
                    else if (g_CurrentPlayer->bond.collision_radius < endDist)
                    {
                        shorten = 0.0f;
                    }
                    else if (startDist < g_CurrentPlayer->bond.collision_radius)
                    {
                        shorten = 0.0f;
                    }
                    else
                    {
                        shorten = (startDist - g_CurrentPlayer->bond.collision_radius) / (startDist - endDist);
                    }
                }

                moveLen = sqrtf((move_offset.f[0] * move_offset.f[0]) + (move_offset.f[2] * move_offset.f[2]));
                if (moveLen > 0.0f)
                {
                    f32 remaining;

                    remaining = (1.0f - shorten) * moveLen;
                    climbOffset = remaining * 0.25f;

                    if (bondCollision.high0.f[1] <= (g_CurrentPlayer->stanHeight + climbOffset))
                    {
                        climbOffset = (bondCollision.high0.f[1] - g_CurrentPlayer->stanHeight);
                        remaining -= (climbOffset / 0.25f);
                        shorten += (remaining / moveLen);
                    }
                }

                climbDirX = bondCollision.high0.f[0] - bondCollision.low0.f[0];
                climbDirZ = bondCollision.high0.f[2] - bondCollision.low0.f[2];
                climbDir2X = bondCollision.high1.f[0] - bondCollision.low0.f[0];
                climbDir2Z = bondCollision.high1.f[2] - bondCollision.low0.f[2];

                if (((climbDir2X * climbDir2X) + (climbDir2Z * climbDir2Z)) < ((climbDirX * climbDirX) + (climbDirZ * climbDirZ)))
                {
                    climbDirX = climbDir2X;
                    climbDirZ = climbDir2Z;
                }

                move_offset.f[0] = (move_offset.f[0] * shorten) + (climbDirX * (climbOffset / (bondCollision.high0.f[1] - bondCollision.low0.f[1])));
                move_offset.f[2] = (move_offset.f[2] * shorten) + (climbDirZ * (climbOffset / (bondCollision.high0.f[1] - bondCollision.low0.f[1])));

                g_CurrentPlayer->bond.collision_position.f[0] = start_collision_pos_x;
                g_CurrentPlayer->bond.collision_position.f[2] = start_collision_pos_z;
                g_CurrentPlayer->bond.current_tile_ptr = tile;

                playerMoveWithCollision(&move_offset, (g_CurrentPlayer->swaytarget == 0.0f));
            }
        }

        movedX = g_CurrentPlayer->bond.collision_position.f[0] - start_collision_pos_x;
        movedZ = g_CurrentPlayer->bond.collision_position.f[2] - start_collision_pos_z;
        moveRatio = (move_offset.f[0] * move_offset.f[0]) + (move_offset.f[2] * move_offset.f[2]);
        if (moveRatio != 0.0f)
        {
            moveRatio = ((movedX * movedX) + (movedZ * movedZ)) / moveRatio;
        }
        moveRatio = sqrtf(moveRatio);
        g_CurrentPlayer->swayoffset0 += moveRatio * swayDx;
        g_CurrentPlayer->swayoffset2 += moveRatio * swayDz;
    }

    // add basic block
    {
        f32 breathing;
        f32 turnSpeed;
        f32 weapon_speed_verta;

        turnSpeed = g_CurrentPlayer->speedtheta;
        weapon_speed_verta =
            (g_CurrentPlayer->speedverta / 0.7f) +
            (g_CurrentPlayer->crouchSpeed / CHR_OBJ_MAXSPEED);

        /*
        Following matches the end of Perfect Dark bwalk0f0c69b8
        */
        breathing = headBobGetBreathingValue();

        if (weapon_speed_verta > 1.0f)
        {
            weapon_speed_verta = 1.0f;
        }
        else if (weapon_speed_verta < -1.0f)
        {
            weapon_speed_verta = -1.0f;
        }

        if (g_CurrentPlayer->headanim == 1)
        {
            breathing *= 1.2f;
        }

        // Perfect Dark call bgun0f09d8dc
        gunUpdateSway(breathing, maxspeed, weapon_speed_verta, turnSpeed);

        // Perfect Dark call bgunSetAdjustPos
        gunSetPullback(DegToRad1Fact(g_CurrentPlayer->vv_verta360));
    }

    // end perfect dark `void bwalk0f0c69b8(void)`

    /**
     * The following section updates the TankRecord fields, and handles prop collision detection
     * with the tank. If colliding with character, play the "arrrhghhg" sound effect, or if
     * colliding with prop then set tank movement penalty and create an explosion.
    */
    if ((g_CurrentPlayer->tankMounted != NULL) && (g_CurrentPlayer->tankDriving == 1) && (g_Tank.enterAudioState == TANK_RUN_STATE_RUNNING))
    {
        struct PropRecord *prop;
        struct TankRecord *tankRec;
        struct ObjectRecord *tankObj;
        f32 smoothY;
        ModelRoData_BoundingBoxRecord *bbox;
        Mtxf tankMtx;
        coord3d tankPos;
        s32 step;
        coord2d tankBounds[4];
        // roomids
        s32 rooms[8];
        s16 *lookup_index;

        tankRec = ((struct TankRecord *)g_CurrentPlayer->tankMounted->obj);
        tankObj = (struct  ObjectRecord*)g_CurrentPlayer->tankMounted->obj;
        bbox = &tankObj->model->obj->Switches[5]->Data->BoundingBox;

        tankRec->is_firing_tank = (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
            && gunGetFiringStatus(GUNRIGHT);

        tankRec->turret_vertical_angle = g_Tank.turretPitch;
        tankRec->turret_orientation_angle = g_Tank.turretYaw;
        tankRec->tank_orientation_angle = g_Tank.yaw;

        mtx4LoadYRotation(M_TAU_F - g_Tank.yaw, &tankMtx);
        mtx4ScaleRows012(tankObj->model->scale, &tankMtx);

        tankPos.f[0] = -g_Tank.turretOffset.f[0];
        tankPos.f[1] = -g_Tank.turretOffset.f[1];
        tankPos.f[2] = -g_Tank.turretOffset.f[2];

        mtx4RotateVecInPlace(&tankMtx, &tankPos);

        tankPos.f[0] += g_CurrentPlayer->bond.collision_position.f[0];
        tankPos.f[2] += g_CurrentPlayer->bond.collision_position.f[2];

        tankObj->prop->stan = g_CurrentPlayer->bond.current_tile_ptr;

        stanWalkLine(
            &tankObj->prop->stan,
            g_CurrentPlayer->bond.collision_position.f[0],
            g_CurrentPlayer->bond.collision_position.f[2],
            tankPos.f[0],
            tankPos.f[2]);

        tankRec->stan_y = stanGetFloorY(tankObj->prop->stan, tankPos.f[0], tankPos.f[2]);

        for (step=0; step<g_ClockTimer; step++)
        {
            tankRec->smoothYSum = (tankRec->smoothYSum * TANK_Y_SMOOTHING) + tankRec->stan_y;
        }

        smoothY = (tankRec->smoothYSum * (1.0f - TANK_Y_SMOOTHING));
        tankPos.f[1] = smoothY
            - (bboxGetYmin(bbox) * tankObj->model->scale)
            + 4.0f;

        mtx4Copy(&tankMtx,  &tankObj->mtx);

        tankObj->runtime_pos.f[0] = tankObj->prop->pos.f[0] = tankPos.f[0];
        tankObj->runtime_pos.f[1] = tankObj->prop->pos.f[1] = tankPos.f[1];
        tankObj->runtime_pos.f[2] = tankObj->prop->pos.f[2] = tankPos.f[2];

        setupUpdateObjectRoomPosition(tankObj);
        objUpdateCollisionBounds(tankObj);
        tankGetCollisionBounds(tankBounds, &g_CurrentPlayer->bond.collision_position, g_Tank.yaw);
        propGetRoomIds(tankObj->prop, rooms);

        // update g_NumRoomPropsFound
        roomGetProps(rooms);

        for (lookup_index=g_RoomPropsFound; *lookup_index>=0; lookup_index++)
        {
            prop = &g_Props[*lookup_index];
            if (prop != tankObj->prop)
            {
                if (prop->type == PROP_TYPE_CHR)
                {
                    f32 ymax;
                    f32 ymin;
                    f32 radius;
                    s32 missed;
                    coord3d explosionPos;
                    struct ChrRecord *hitChr;

                    missed = 1;
                    hitChr = prop->chr;
                    propGetCollisionRadius(prop, &radius, &ymax, &ymin);

                    if (polygonContainsPoint(&prop->pos, tankBounds, 4))
                    {
                        missed = 0;

                        if (hitChr->actiontype == ACT_DIE)
                        {
                            if ((hitChr->chrflags & CHRFLAG_01000000) == 0 && lvGetControlsLocked() == 0)
                            {
                                hitChr->chrflags |= CHRFLAG_01000000;
                                if ((g_LvTickCount % 3) < 2)
                                {
                                    sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, CRUSHED_YELL_SFX, NULL), &prop->pos);
                                }
                                if ((g_LvTickCount % 3) > 0)
                                {
                                    sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, TANK_CRUSH_MAN_SFX, NULL), &prop->pos);
                                }
                            }
                        }
                    }

                    if (missed && (polygonTouchesCircle(&prop->pos, radius, tankBounds, 4)))
                    {
                        missed = 0;
                    }

                    if (missed == 0)
                    {
                        explosionPos.f[0] = tankObj->runtime_pos.f[0];
                        explosionPos.f[1] = prop->pos.f[1];
                        explosionPos.f[2] = tankObj->runtime_pos.f[2];

                        chrExplosionDamage(prop->chr, &explosionPos, 3.0f, 1);
                    }
                }
                else if (prop->type == PROP_TYPE_OBJ)
                {
                    coord2d *polygon;
                    s32 edges;

                    propGetCollisionPolygon2d(prop, &polygon, &edges);
                    if ((edges > 0) && polygonsOverlap(polygon, edges, tankBounds, 4))
                    {
                        // Explode destroyable props when the tank touches them
                        objDetonateRecursive(prop, 10000.0f, &prop->obj->runtime_pos, 0x20, playerGetCurrentNum());
                        g_Tank.damagePenaltyTicks = TANK_DAMAGE_PENTALTY_TICKS;
                    }
                }
            }
        }
    }

    playerUpdateVertical(use_stanHeight, climbOffset);
    playerUpdatePosFromCollision();
    playerUpdateCollisionBounds();
}


/**
 * US address 7F086990.
 * EU address 7F086AB0.
*/
void playerMoveFrozen(u16 buttons, u16 oldbuttons)
{
    coord3d property_pos;
    coord3d property_pos2;
    coord3d property_offset;
    coord3d offset;
    struct StandTile *room_pointer_tile;
    coord3d stan_walk_start;

    property_pos = g_DefaultFrozenPlayerPos;
    property_pos2 = g_DefaultFrozenPlayerPos2;
    property_offset = g_DefaultFrozenPlayerOffset;
    offset = g_DefaultFrozenMoveOffset;

    playerTickDamageAndHealth();
    playerTickExplode();
    playerProcessInput(0, 0, 0, 0);
    playerUpdateVertaTheta();
    playerTickMoveAnim(0, 0, 0);

    if (((g_ForceBondMoveOffset.f[0] != 0.0f) || (g_ForceBondMoveOffset.f[2] != 0.0f))
        && (!gameIsTwoPlayerMission() || (playerGetCurrentNum() == g_CoopForceMovePlayer)))
    {
        offset.f[0] += g_ForceBondMoveOffset.f[0] * g_GlobalTimerDelta;
        offset.f[2] += g_ForceBondMoveOffset.f[2] * g_GlobalTimerDelta;
    }

    offset.f[0] += ((g_CurrentPlayer->headpos.f[2] * g_CurrentPlayer->bond.theta_transform.f[0]) - (g_CurrentPlayer->headpos.f[0] * g_CurrentPlayer->bond.theta_transform.f[2])) * g_GlobalTimerDelta;
    offset.f[2] += ((g_CurrentPlayer->headpos.f[2] * g_CurrentPlayer->bond.theta_transform.f[2]) + (g_CurrentPlayer->headpos.f[0] * g_CurrentPlayer->bond.theta_transform.f[0])) * g_GlobalTimerDelta;

    playerMoveWithCollision(&offset, 1);
    playerUpdateVertical(0, 0.0f);
    playerUpdatePosFromCollision();

    if ((g_CameraMode == CAMERAMODE_FP_NOINPUT) || (g_CameraMode == CAMERAMODE_FP) || (g_CameraMode == CAMERAMODE_FADE_TO_TITLE))
    {
        playerSetCameraModeRaw(0);
        return;
    }

    playerTickFrozenCamera(buttons, oldbuttons, &property_pos, &property_pos2, &room_pointer_tile, &stan_walk_start);
    playerSetCameraModeRaw(1);
    playerSetPosition(&property_pos, &property_pos2, &property_offset, room_pointer_tile, &stan_walk_start);
}


/**
 * Sets/updates viewport for player.
 * Refreshes autoaim setting.
 * Arguments are passed into playerMove or playerMoveFrozen.
 * Checks if necessary to call bossReturnTitleStage.
 * Set player->buttons_pressed to arg2.
 *
 * Address 0x7F086F9C (VERSION_US).
 * Address 0x7F0870BC (VERSION_EU).
 * Address 0x7F087668 (VERSION_JP).
 */
void playerUpdate(s8 stick_x, s8 stick_y, u16 buttons)
{
#ifdef VERSION_EU
    f32 faspect;
#endif

    playerSetFovY(FOV_Y_F);

    // This call doesn't do anything, the call viSetFovY(g_CurrentPlayer->fovy); in lvRender
    // will actually change the field of view.
    // The call above should set g_CurrentPlayer->fovy, but it doesn't seem to affect
    // the fov....
    viSetFovY(FOV_Y_F);

    if (cameraFrameCounter1 != 0)
    {
        if ((cameraFrameCounter1 >= 4) && (resolution != 0) && (viGetFrameBuf2() == (u8*)(cfb_16[1])))
        {
            cameraBufferToggle = 1;
            cameraFrameCounter1 = 0;
        }
        else
        {
            cameraFrameCounter1 += 1;
        }
    }
    else
    {
        if (cameraFrameCounter2 != 0)
        {
            if ((cameraFrameCounter2 >= 4) && (viGetFrameBuf2() == (u8*)(cfb_16[0])))
            {
                cameraBufferToggle = 0;
                cameraFrameCounter2 = 0;
            }
            else
            {
                cameraFrameCounter2 += 1;
            }
        }
    }

    if ((cameraBufferToggle != 0) && (viGetFrameBuf2() == (u8*)(cfb_16[1])))
    {
        viSetFrameBuf2((u8 *) resolution);
    }

#ifdef VERSION_EU
    if (optionsGetScreenRatio() == SCREEN_RATIO_16_9)
    {
        faspect = ((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight()) * 0.75f * WIDESCREEN_ASPECT;
    }
    else
    {
        faspect = (f32) playerGetViewportWidth() / (f32) playerGetViewportHeight();
    }

    if (cameraBufferToggle == 0)
    {
        faspect *= EU_CAMERA_8003642C_ASPECT;
    }

    playerSetAspect(faspect);
    viSetAspect(faspect);

#else

    if (optionsGetScreenRatio() == SCREEN_RATIO_16_9)
    {
        playerSetAspect(((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight()) * 0.75f * WIDESCREEN_ASPECT);
        viSetAspect(((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight()) * 0.75f * WIDESCREEN_ASPECT);
    }
    else
    {
        playerSetAspect((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight());
        viSetAspect((f32) playerGetViewportWidth() / (f32) playerGetViewportHeight());
    }
#endif

    playerSetScreenSize( playerGetViewportWidth(), playerGetViewportHeight());
    playerSetViewportPos( playerGetViewportLeft(), playerGetViewportTop());
    viSetXY(viGetFrameWidth(), viGetFrameHeight());
    viSetBuf(viGetFrameWidth(), viGetFrameHeight());
    // Hi-res doubles the physical width only; game logic stays 320 wide.
    viSetScaleX((optionsGetHiRes() && (cameraBufferToggle == 0)) ? VI_MAX_SCALEX : 1);
    viSetViewSize(playerGetViewportWidth(), playerGetViewportHeight());
    viSetViewPosition(playerGetViewportLeft(), playerGetViewportTop());
    currentPlayerTickColourFade();
    currentPlayerTickChrFade();
    currentPlayerSetYAutoAimEnabled(optionsGetAutoAim());
    currentPlayerSetXAutoAimEnabled(optionsGetAutoAim());
    currentPlayerSetLookAheadSetting(optionsGetLookAhead());
    gunSetGunAmmoVisible(GUNAMMOREASON_OPTION, optionsGetAmmoOnScreen());

    gunSetSightVisible(
        GUNSIGHTREASON_1,
        (!gameIsMulti() && optionsGetSightOnScreen())
            || (gameIsMulti() && g_playerPerm->sight)
    );

#if defined(VERSION_EU)
#endif

    if ((g_CameraMode == CAMERAMODE_NONE) || ((g_CameraMode == CAMERAMODE_FP) && (g_Mission.playerHasControl != 0)) || (g_CameraMode == CAMERAMODE_FADE_TO_TITLE))
    {
        if (playerGetCurrentNum() == 0)
        {
            mission_timer += g_ClockTimer;
        }

        playerMove(stick_x, stick_y, buttons, (u16) g_CurrentPlayer->buttons_pressed);
    }
    else
    {
        playerMoveFrozen(buttons, (u16) g_CurrentPlayer->buttons_pressed);
    }

#if defined(BUGFIX_R1)
    watch_time_0 += g_FrameDelta60;
#else
    // VERSION_US
    watch_time_0 += g_FrameDelta;
#endif

    if (g_Mission.exitOnPress != 0)
    {
        if ((lvGetControlsLocked() == 0) && ((buttons & ~(g_CurrentPlayer->buttons_pressed) & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON | CONT_R | CONT_L))))
        {
            g_Mission.exitOnPress = 2;

            if (currentPlayerIsFadeComplete())
            {
                if (g_CurrentPlayer->colourscreenfrac == 0.0f)
                {
                    currentPlayerSetFadeColour(0, 0, 0, 0.0f);
                    currentPlayerSetFadeFrac(60.0f, 1.0f);
                }
            }
            else
            {
                if (g_CurrentPlayer->colourfadefracnew == 0.0f)
                {
                    currentPlayerSetFadeFrac(g_CurrentPlayer->colourfadetime60, 1.0f);
                }
            }
        }

        if ((g_Mission.exitOnPress == 2) && currentPlayerIsFadeComplete() && (g_CurrentPlayer->colourscreenfrac == 1.0f))
        {
            bossReturnTitleStage();
        }
    }

    if (g_CameraAfterCinema)
    {
        playerAdvanceCameraMode();
    }

    if (g_CurrentPlayer->bonddead)
    {
        if (g_CurrentPlayer->redbloodfinished == FALSE)
        {
            gunEquip(GUNLEFT, 0);
            gunEquip(GUNRIGHT, 0);

            if (0)
            {
                // removed?
            };
        }

        if (g_CurrentPlayer->redbloodfinished && g_CurrentPlayer->deathanimfinished && (camera_mode >= CAMERAMODE_SWIRL))
        {
            bossRunTitleStage();
        }
    }

    if ((g_CameraMode == CAMERAMODE_FADE_TO_TITLE) && currentPlayerIsFadeComplete())
    {
        bossRunTitleStage();
    }

    g_CurrentPlayer->buttons_pressed = buttons;
}


void playerSetWorldScale(f32 scale)
{
    f32 scalar;
    struct PlayerBond* col;

    scalar = g_PlayerWorldScale / scale;

    col = &g_CurrentPlayer->bond;
    col->collision_position.x *= scalar;
    col->collision_position.z *= scalar;

    g_PlayerWorldScale = scale;
    g_PlayerWorldScaleInv = 1.0f / scale;
}


s32 lvGetMissionTime60(void) {
#ifdef VERSION_EU
    return (mission_timer * 60) / 50;
#else
    return mission_timer;
#endif
}


