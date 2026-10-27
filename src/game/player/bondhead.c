/**
 * Head bob. Bond has a hidden walk animation whose root motion moves the camera up and
 * down and side to side as he walks; bheadUpdate advances it and bheadInit sets it up
 * at level start. Also breathing. See docs/subsystems/player.md.
 */
#include <ultra64.h>
#include <limits.h>
#include <assets/animationtable_data.h>
#include <random.h>
#include "game/player/bondview.h"
#include "game/chr/chrai.h"
#include "game/model/anim.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/player/player.h"
#include "game/player/bondhead.h"
#include "game/model/model.h"
#include "game/math/interp.h"
#include <math.h>
#include <bondtypes.h>
#include "game/chr/chr.h"
#include "game/model/chrobjdata.h"


struct init_bond_anim_unk g_BondMoveAnimationSetup[2] = {
    // address 0x80036AD0 = g_BondMoveAnimationSetup + 0
    {PTR_ANIM_bond_eye_walk, 9.5f, 27.0f, 0.0f, 0.0f, 1.5f},
    // address 0x80036AE8 = g_BondMoveAnimationSetup + 24
    {PTR_ANIM_sprinting, 7.5f, 17.0f, 0.0f, 1.5f, 100.0f}
};





// forward declarations

void bheadUpdatePos(coord3d *vel);
void bheadUpdateRot(coord3d *lookvel, coord3d *upvel);
void bheadSetdamp(f32 headdamp);

// end forward declarations





void bheadFlipAnimation()
{
    g_CurrentPlayer->animFlipFlag = !g_CurrentPlayer->animFlipFlag;
}

void bheadUpdateIdleRoll()
{
    f32 mult = 1.0f / UINT_MAX;

	g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[0] = ((f32)randomGetNext() * mult - 0.5f) * 0.02f;
	g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[2] = 1;
	g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[0] = ((f32)randomGetNext() * mult - 0.5f) * 0.02f;
	g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[1] = 1;

	if (g_CurrentPlayer->standcnt)
    {
		g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[1] = (f32)randomGetNext() * mult * 0.01f;
		g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[2] = (f32)randomGetNext() * mult * -0.01f;
	}
    else
    {
		g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[1] = (f32)randomGetNext() * mult * -0.01f;
		g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[2] = (f32)randomGetNext() * mult * 0.01f;
	}

	g_CurrentPlayer->standcnt = 1 - g_CurrentPlayer->standcnt;
}

void bheadUpdatePos(coord3d *vel)
{
#if defined(VERSION_EU)
#define CURRENTPLAYERUPDATEHEADPOS_SCALE 0.916599988937f
#else
#define CURRENTPLAYERUPDATEHEADPOS_SCALE 0.93f
#endif
    s32 i;

    if (g_CurrentPlayer->resetheadpos)
    {
        g_CurrentPlayer->headpossum.f[0] = 0.0f;
        g_CurrentPlayer->headpossum.f[1] = (vel->f[1] / (1.0f - CURRENTPLAYERUPDATEHEADPOS_SCALE));
        g_CurrentPlayer->headpossum.f[2] = 0.0f;

        g_CurrentPlayer->resetheadpos = FALSE;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->headpossum.f[0] = ((CURRENTPLAYERUPDATEHEADPOS_SCALE * g_CurrentPlayer->headpossum.f[0]) + vel->f[0]);
        g_CurrentPlayer->headpossum.f[1] = ((CURRENTPLAYERUPDATEHEADPOS_SCALE * g_CurrentPlayer->headpossum.f[1]) + vel->f[1]);
        g_CurrentPlayer->headpossum.f[2] = ((CURRENTPLAYERUPDATEHEADPOS_SCALE * g_CurrentPlayer->headpossum.f[2]) + vel->f[2]);
    }

    g_CurrentPlayer->headpos.f[0] = (g_CurrentPlayer->headpossum.f[0] * (1.0f - CURRENTPLAYERUPDATEHEADPOS_SCALE));
    g_CurrentPlayer->headpos.f[1] = (g_CurrentPlayer->headpossum.f[1] * (1.0f - CURRENTPLAYERUPDATEHEADPOS_SCALE));
    g_CurrentPlayer->headpos.f[2] = (g_CurrentPlayer->headpossum.f[2] * (1.0f - CURRENTPLAYERUPDATEHEADPOS_SCALE));
#undef CURRENTPLAYERUPDATEHEADPOS_SCALE
}

void bheadUpdateRot(coord3d *lookvel, coord3d *upvel)
{
	s32 i;

	if (g_CurrentPlayer->resetheadrot)
    {
		g_CurrentPlayer->headlooksum.f[0] = lookvel->f[0] / (1.0f - g_CurrentPlayer->headdamp);
		g_CurrentPlayer->headlooksum.f[1] = lookvel->f[1] / (1.0f - g_CurrentPlayer->headdamp);
		g_CurrentPlayer->headlooksum.f[2] = lookvel->f[2] / (1.0f - g_CurrentPlayer->headdamp);

        g_CurrentPlayer->headupsum.f[0] = upvel->f[0] / (1.0f - g_CurrentPlayer->headdamp);
		g_CurrentPlayer->headupsum.f[1] = upvel->f[1] / (1.0f - g_CurrentPlayer->headdamp);
		g_CurrentPlayer->headupsum.f[2] = upvel->f[2] / (1.0f - g_CurrentPlayer->headdamp);

		g_CurrentPlayer->resetheadrot = FALSE;
	}

	for (i = 0; i < g_ClockTimer; i++)
    {
		g_CurrentPlayer->headlooksum.f[0] = g_CurrentPlayer->headdamp * g_CurrentPlayer->headlooksum.f[0] + lookvel->f[0];
		g_CurrentPlayer->headlooksum.f[1] = g_CurrentPlayer->headdamp * g_CurrentPlayer->headlooksum.f[1] + lookvel->f[1];
		g_CurrentPlayer->headlooksum.f[2] = g_CurrentPlayer->headdamp * g_CurrentPlayer->headlooksum.f[2] + lookvel->f[2];

        g_CurrentPlayer->headupsum.f[0] = g_CurrentPlayer->headdamp * g_CurrentPlayer->headupsum.f[0] + upvel->f[0];
		g_CurrentPlayer->headupsum.f[1] = g_CurrentPlayer->headdamp * g_CurrentPlayer->headupsum.f[1] + upvel->f[1];
		g_CurrentPlayer->headupsum.f[2] = g_CurrentPlayer->headdamp * g_CurrentPlayer->headupsum.f[2] + upvel->f[2];
	}

	g_CurrentPlayer->headlook.f[0] = g_CurrentPlayer->headlooksum.f[0] * (1.0f - g_CurrentPlayer->headdamp);
	g_CurrentPlayer->headlook.f[1] = g_CurrentPlayer->headlooksum.f[1] * (1.0f - g_CurrentPlayer->headdamp);
	g_CurrentPlayer->headlook.f[2] = g_CurrentPlayer->headlooksum.f[2] * (1.0f - g_CurrentPlayer->headdamp);

    g_CurrentPlayer->headup.f[0] = g_CurrentPlayer->headupsum.f[0] * (1.0f - g_CurrentPlayer->headdamp);
	g_CurrentPlayer->headup.f[1] = g_CurrentPlayer->headupsum.f[1] * (1.0f - g_CurrentPlayer->headdamp);
	g_CurrentPlayer->headup.f[2] = g_CurrentPlayer->headupsum.f[2] * (1.0f - g_CurrentPlayer->headdamp);
}

void bheadSetdamp(f32 headdamp)
{
	if (headdamp != g_CurrentPlayer->headdamp)
    {
		f32 divisor = 1.0f - headdamp;

        g_CurrentPlayer->headlooksum.f[0] = (g_CurrentPlayer->headlooksum.f[0] * (1.0f - g_CurrentPlayer->headdamp)) / divisor;
		g_CurrentPlayer->headlooksum.f[1] = (g_CurrentPlayer->headlooksum.f[1] * (1.0f - g_CurrentPlayer->headdamp)) / divisor;
		g_CurrentPlayer->headlooksum.f[2] = (g_CurrentPlayer->headlooksum.f[2] * (1.0f - g_CurrentPlayer->headdamp)) / divisor;

        g_CurrentPlayer->headupsum.f[0] = (g_CurrentPlayer->headupsum.f[0] * (1.0f - g_CurrentPlayer->headdamp)) / divisor;
		g_CurrentPlayer->headupsum.f[1] = (g_CurrentPlayer->headupsum.f[1] * (1.0f - g_CurrentPlayer->headdamp)) / divisor;
		g_CurrentPlayer->headupsum.f[2] = (g_CurrentPlayer->headupsum.f[2] * (1.0f - g_CurrentPlayer->headdamp)) / divisor;

        g_CurrentPlayer->headdamp = headdamp;
	}
}


coord3d initialHeadPosition = { 0.0f, 0.0f, 0.0f };

coord3d headLookDirection = { 0.0f, 0.0f, 1.0f };

coord3d headUpDirection = { 0.0f, 1.0f, 0.0f };

ModelRenderData headModelRenderData = {NULL,
                              TRUE,
                              0x00000003,
                              NULL,
                              NULL,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              {0, 0, 0, 0},
                              {0, 0, 0, 0},
                              CULLMODE_BOTH};




/**
 * Updates the head movement based on the player's speed and animation state.
 */
void bheadUpdate(f32 percent_speed, f32 speedsideways)
{
    coord3d headpos;
    coord3d lookvel;
    coord3d upvel;
    f32 abs_anim_speed;
    ModelRenderData renderData;
    Mtxf sp40;
    coord3d offset;
    u32 isMergable;

    headpos = initialHeadPosition;
    lookvel = headLookDirection;
    upvel = headUpDirection;

    abs_anim_speed = modelGetAbsAnimSpeed(&g_CurrentPlayer->model);

    if (g_CurrentPlayer->headanim == 0)
    {
        if (abs_anim_speed > 0.7f)
        {
            g_CurrentPlayer->headamplitude = 1.0f;
        }
        else if (abs_anim_speed > 0.1f)
        {
            g_CurrentPlayer->headamplitude = (((abs_anim_speed - 0.1f) * 0.6f) / 0.59999996f) + 0.4f;
        }
        else
        {
            g_CurrentPlayer->headamplitude = 0.4f;
        }

        g_CurrentPlayer->sideamplitude = g_CurrentPlayer->headamplitude;
    }
    else if (g_CurrentPlayer->headanim == 1)
    {
        g_CurrentPlayer->headamplitude = 0.9f;
        g_CurrentPlayer->sideamplitude = 0.5f;
    }
    else
    {
        g_CurrentPlayer->headamplitude = 1.0f;
        g_CurrentPlayer->sideamplitude = g_CurrentPlayer->headamplitude;
    }

    renderData = headModelRenderData;
    offset = (coord3d){0};

    isMergable = modelIsAnimMergingEnabled();

    g_CurrentPlayer->resetheadtick = FALSE;

    modelSetAnimMergingEnabled(0);
    modelTickAnim(&g_CurrentPlayer->model, g_ClockTimer, 1);
    modelSetAnimMergingEnabled((s32) isMergable);

    modelCalcPosition(&g_CurrentPlayer->model);
    mtx4LoadIdentity(&sp40);

    renderData.basemtx = &sp40;
    renderData.mtxlist = &g_CurrentPlayer->bondheadmatrices[0];

    modelCalcMatrices(&renderData, &g_CurrentPlayer->model);

    g_CurrentPlayer->headbodyoffset.f[0] = g_CurrentPlayer->standbodyoffset.x;
    g_CurrentPlayer->headbodyoffset.f[1] = g_CurrentPlayer->standbodyoffset.y;
    g_CurrentPlayer->headbodyoffset.f[2] = g_CurrentPlayer->standbodyoffset.z;

    modelGetRootPosition(&g_CurrentPlayer->model, (struct float3 *) &offset);

    offset.f[0] -= g_CurrentPlayer->bondheadmatrices[0].m[3][0];
    offset.f[2] -= g_CurrentPlayer->bondheadmatrices[0].m[3][2];

    modelSetRootPosition(&g_CurrentPlayer->model, (coord3d *) &offset);

    if (abs_anim_speed > 0.0f)
    {
        g_CurrentPlayer->bondheadmatrices[0].m[3][0] += speedsideways;
        g_CurrentPlayer->bondheadmatrices[0].m[3][2] *= percent_speed;

        if (g_ClockTimer > 0)
        {
            g_CurrentPlayer->bondheadmatrices[0].m[3][0] /= g_GlobalTimerDelta;
            g_CurrentPlayer->bondheadmatrices[0].m[3][2] /= g_GlobalTimerDelta;
        }

        headpos.f[0] =   g_CurrentPlayer->bondheadmatrices[0].m[3][0] * g_CurrentPlayer->headamplitude;
        headpos.f[1] = ((g_CurrentPlayer->bondheadmatrices[0].m[3][1] - g_CurrentPlayer->standheight) * g_CurrentPlayer->headamplitude) + g_CurrentPlayer->standheight;
        headpos.f[2] =   g_CurrentPlayer->bondheadmatrices[0].m[3][2] * g_CurrentPlayer->headamplitude;

        if (g_CurrentPlayer->headanim >= 0)
        {
            lookvel.f[0] = g_CurrentPlayer->bondheadmatrices[0].m[2][0] * g_CurrentPlayer->sideamplitude;
            lookvel.f[1] = g_CurrentPlayer->bondheadmatrices[0].m[2][1] * g_CurrentPlayer->headamplitude;
            lookvel.f[2] = ((g_CurrentPlayer->bondheadmatrices[0].m[2][2] - 1.0f) * g_CurrentPlayer->headamplitude) + 1.0f;

            upvel.f[0] = g_CurrentPlayer->bondheadmatrices[0].m[1][0] * g_CurrentPlayer->headamplitude;
            upvel.f[1] = ((g_CurrentPlayer->bondheadmatrices[0].m[1][1] - 1.0f) * g_CurrentPlayer->headamplitude) + 1.0f;
            upvel.f[2] = g_CurrentPlayer->bondheadmatrices[0].m[1][2] * g_CurrentPlayer->headamplitude;

            g_CurrentPlayer->headwalkingtime60 += g_ClockTimer;

#if defined(VERSION_EU)
            if (g_CurrentPlayer->headwalkingtime60 > TICKS_PER_SECOND)
            {
                bheadSetdamp(0.916599988937f);
            }
            else
            {
                bheadSetdamp(0.987999975681f);
            }
#else
            if (g_CurrentPlayer->headwalkingtime60 > TICKS_PER_SECOND)
            {
                bheadSetdamp(0.93f);
            }
            else
            {
                bheadSetdamp(0.99f);
            }
#endif
        }
        else
        {
            lookvel.f[0] = g_CurrentPlayer->bondheadmatrices[0].m[2][0];
            lookvel.f[1] = g_CurrentPlayer->bondheadmatrices[0].m[2][1];
            lookvel.f[2] = g_CurrentPlayer->bondheadmatrices[0].m[2][2];

            upvel.f[0] = g_CurrentPlayer->bondheadmatrices[0].m[1][0];
            upvel.f[1] = g_CurrentPlayer->bondheadmatrices[0].m[1][1];
            upvel.f[2] = g_CurrentPlayer->bondheadmatrices[0].m[1][2];

            bheadSetdamp(0.85f);
        }
    }
    else
    {
        g_CurrentPlayer->headbodyoffset.f[0] = g_CurrentPlayer->standbodyoffset.x;
        g_CurrentPlayer->headbodyoffset.f[1] = g_CurrentPlayer->standbodyoffset.y;
        g_CurrentPlayer->headbodyoffset.f[2] = g_CurrentPlayer->standbodyoffset.z;

        headpos.f[0] = 0.0f;
        headpos.f[1] = g_CurrentPlayer->standheight;
        headpos.f[2] = 0.0f;

        g_CurrentPlayer->headwalkingtime60 = 0;
#if defined(VERSION_EU)
        bheadSetdamp(0.987999975681f);
#else
        bheadSetdamp(0.99f);
#endif
        g_CurrentPlayer->standfrac += (0.008333334f + (0.025000002f * playerGetBreathing())) * g_GlobalTimerDelta;

        if (g_CurrentPlayer->standfrac >= 1.0f)
        {
            bheadUpdateIdleRoll();
            g_CurrentPlayer->standfrac -= 1.0f;
        }

        // result = x vector plus ((y - x vector) * scaler)
        // lookvel = ...
        interpLerp3(
            &g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[0],
            &g_CurrentPlayer->standlook[1 - g_CurrentPlayer->standcnt].f[0],
            g_CurrentPlayer->standfrac,
            &lookvel);

        lookvel.f[0] *= (1.0f + (5.0f * playerGetBreathing()));
        lookvel.f[1] *= (1.0f + (5.0f * playerGetBreathing()));

        // result = x vector plus ((y - x vector) * scaler)
        // upvel = ...
        interpLerp3(
            &g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[0],
            &g_CurrentPlayer->standup[1 - g_CurrentPlayer->standcnt].f[0],
            g_CurrentPlayer->standfrac,
            &upvel);

        upvel.f[0] *= (1.0f + (5.0f * playerGetBreathing()));
        upvel.f[2] *= (1.0f + (5.0f * playerGetBreathing()));
    }

    bheadUpdatePos(&headpos);
    bheadUpdateRot(&lookvel, &upvel);
}



/**
 * Adjusts Bond's head animation based on movement speed.
 *
 * @param f32 speed: The movement speed to adjust the animation to.
 */
void bheadAdjustAnimation(f32 speed)
{
    s32 i;
    f32 startframe;

    speed *= g_BondMoveAnimationSetup[1].speedMultiplier;

    for (i=0; i<2; i++)
    {
        if (speed <= g_BondMoveAnimationSetup[i].maxSpeed * g_BondMoveAnimationSetup[i].speedMultiplier)
        {
            if (i != g_CurrentPlayer->headanim)
            {
                startframe = 0.0f;

                if (g_CurrentPlayer->headanim >= 0)
                {
                    startframe = (g_CurrentPlayer->field_5C0 - g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].loopframe)
                        / (g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].endframe - g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].loopframe);

                    startframe = g_BondMoveAnimationSetup[i].loopframe + ((g_BondMoveAnimationSetup[i].endframe - g_BondMoveAnimationSetup[i].loopframe) * startframe);
                }

                modelSetAnimation(
                    &g_CurrentPlayer->model,
                    // match hack: addu address backwards
                    (struct ModelAnimation *) ((s32)g_BondMoveAnimationSetup[i].anim_id + (s32)&g_AnimTable->data),
                    (s32) g_CurrentPlayer->animFlipFlag,
                    startframe,
                    0.5f,
                    12.0f);

                modelSetAnimLooping(&g_CurrentPlayer->model, g_BondMoveAnimationSetup[i].loopframe, 0.0f);
                modelSetAnimEndFrame(&g_CurrentPlayer->model, g_BondMoveAnimationSetup[i].endframe);
                modelSetAnimFlipFunction(&g_CurrentPlayer->model, bheadFlipAnimation);
                g_CurrentPlayer->headanim = i;
            }

            speed /= g_BondMoveAnimationSetup[i].speedMultiplier;

            modelSetAnimSpeed(&g_CurrentPlayer->model, speed * 0.5f, 0.0f);
            return;
        }
    }
}


/**
 * Starts a new death animation for Bond's head.
 * 
 * @param animNum The animation to play.
 * @param flip Whether to flip the animation.
 * @param startFrame The starting frame of the animation.
 * @param speed The speed of the animation.
 */
void bheadStartDeathAnimation(struct ModelAnimation *animnum, s32 flip, f32 fstarttime, f32 speed)
{
    modelSetAnimation(&g_CurrentPlayer->model, animnum, flip, fstarttime, speed * 0.5f, 12.0f);
    g_CurrentPlayer->headanim = -1;
}

/**
 * Sets the speed of the current head animation.
 * 
 * @param speed The speed to set for the head animation.
 */
void bheadSetSpeed(f32 speed)
{
    modelSetAnimSpeed(&g_CurrentPlayer->model, speed * 0.5f, 0.0f);
}


/**
 * Calculates the breathing value for Bond's head animation.
 * 
 * @return The calculated breathing value.
 */
f32 bheadGetBreathingValue(void)
{
	if (g_CurrentPlayer->headanim >= 0) {
        // playerGetBreathing() * (1/80) + (1/240)
		f32 baseBreathing = playerGetBreathing() * 0.012500001f + 0.004166667f;
		f32 animSpeed = modelGetAbsAnimSpeed(&g_CurrentPlayer->model);

		if (animSpeed > 0) {
			f32 calculatedBreathing = animSpeed / (g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].endframe - g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].loopframe);

			if (calculatedBreathing < baseBreathing) {
				calculatedBreathing = baseBreathing;
			}

			return calculatedBreathing;
		}

		return baseBreathing;
	}

	return 0;
}


//data

// forward declarations

void bheadSumRootMotion(void *anim, s32 arg1, s32 arg2, s32 *arg3);

// end forward declarations





/**
 * @param anim:
 * @param arg1:
 * @param arg2:
 * @param arg3: unknown type.
*/
void bheadSumRootMotion(void *anim, s32 arg1, s32 arg2, s32 *arg3)
{
    // todo: is this a struct? see: animReadRootMotion16
    s16 sp40[6];

    arg3[0] = 0;
    arg3[1] = 0;
    arg3[2] = 0;

    for (; arg1<arg2; arg1++)
    {
        animReadRootMotion16(0, 0, &skeleton_guard, anim, arg1, &sp40[2]);
        arg3[0] += sp40[2];
        arg3[1] += sp40[3];
        arg3[2] += sp40[4];
    }
}



#ifdef REFRESH_PAL
#define ANIMRATE 1.2f
#define DAMPVAL 0.9166f
#define HEADSUM 11.990406f
#else
#define ANIMRATE 1.0f
#define DAMPVAL 0.93f
#define HEADSUM 14.285716f
#endif


void bheadInit(void)
{
    s32 i;
    s32 spD0[3];
    ModelRenderData renderData;
    Mtxf identityMatrix;


    animInit(&g_CurrentPlayer->model, &player_gait_object_header, &g_CurrentPlayer->field_654);
    modelSetScale(&g_CurrentPlayer->model, IDO_POINT_ONE);

#if defined (BUGFIX_R1)
    modelSetAnimPlaySpeed(&g_CurrentPlayer->model, ANIMRATE, 0.0f);
#endif

    g_CurrentPlayer->headanim = 0;

    g_CurrentPlayer->headdamp = DAMPVAL;

    g_CurrentPlayer->headwalkingtime60 = 0;
    g_CurrentPlayer->headamplitude = 1.0f;
    g_CurrentPlayer->sideamplitude = 1.0f;
    g_CurrentPlayer->headpos.f[0] = 0.0f;
    g_CurrentPlayer->headpos.f[1] = 0.0f;
    g_CurrentPlayer->headpos.f[2] = 0.0f;
    g_CurrentPlayer->headlook.f[0] = 0.0f;
    g_CurrentPlayer->headlook.f[1] = 0.0f;
    g_CurrentPlayer->headlook.f[2] = 0.0f;
    g_CurrentPlayer->headup.f[0] = 0.0f;
    g_CurrentPlayer->headup.f[1] = 0.0f;
    g_CurrentPlayer->headup.f[2] = 0.0f;
    g_CurrentPlayer->headpossum.f[0] = 0.0f;
    g_CurrentPlayer->headpossum.f[1] = 0.0f;
    g_CurrentPlayer->headpossum.f[2] = 0.0f;
    g_CurrentPlayer->headlooksum.f[0] = 0.0f;
    g_CurrentPlayer->headlooksum.f[1] = 0.0f;
    g_CurrentPlayer->headlooksum.f[2] = HEADSUM;


    g_CurrentPlayer->headupsum.f[0] = 0.0f;
    g_CurrentPlayer->headupsum.f[1] = HEADSUM;
    g_CurrentPlayer->headupsum.f[2] = 0.0f;
    g_CurrentPlayer->resetheadpos = TRUE;
    g_CurrentPlayer->resetheadrot = TRUE;
    g_CurrentPlayer->resetheadtick = TRUE;
    g_CurrentPlayer->headbodyoffset.f[0] = 0.0f;
    g_CurrentPlayer->headbodyoffset.f[1] = 0.0f;
    g_CurrentPlayer->headbodyoffset.f[2] = 0.0f;
    g_CurrentPlayer->standheight = 0.0f;
    g_CurrentPlayer->standbodyoffset.x = 0.0f;
    g_CurrentPlayer->standbodyoffset.y = 0.0f;
    g_CurrentPlayer->standbodyoffset.z = 0.0f;
    g_CurrentPlayer->standfrac = 0.0f;
    g_CurrentPlayer->standlook[0].f[0] = 0.0f;
    g_CurrentPlayer->standlook[0].f[1] = 0.0f;
    g_CurrentPlayer->standlook[0].f[2] = 1.0f;
    g_CurrentPlayer->standlook[1].f[0] = 0.0f;
    g_CurrentPlayer->standlook[1].f[1] = 0.0f;
    g_CurrentPlayer->standlook[1].f[2] = 1.0f;
    g_CurrentPlayer->standup[0].f[0] = 0.0f;
    g_CurrentPlayer->standup[0].f[1] = 1.0f;
    g_CurrentPlayer->standup[0].f[2] = 0.0f;
    g_CurrentPlayer->standup[1].f[0] = 0.0f;
    g_CurrentPlayer->standup[1].f[1] = 1.0f;
    g_CurrentPlayer->standup[1].f[2] = 0.0f;
    g_CurrentPlayer->standcnt = 0;

    for (i=0; i<2; i++)
    {
        bheadSumRootMotion(
            // match hack: addu address calculated backwards
            (void*)((s32)g_BondMoveAnimationSetup[i].anim_id + (s32)&g_AnimTable->data),
            (s32)g_BondMoveAnimationSetup[i].loopframe,
            (s32)g_BondMoveAnimationSetup[i].endframe,
            &spD0);

        g_BondMoveAnimationSetup[i].speedMultiplier = (f32) (((f32) spD0[2] * IDO_POINT_ONE) / (g_BondMoveAnimationSetup[i].endframe - g_BondMoveAnimationSetup[i].loopframe));
    }

    renderData = (ModelRenderData){NULL, TRUE, 3};

    modelSetAnimation(&g_CurrentPlayer->model, (struct ModelAnimation *)&g_AnimTable->data[(s32)&ANIM_DATA_idle], 0, 0.0f, 0.5f, 0.0f);

    modelCalcPosition(&g_CurrentPlayer->model);
    mtx4LoadIdentity(&identityMatrix);

    renderData.basemtx = &identityMatrix;
    renderData.mtxlist = &g_CurrentPlayer->bondheadmatrices[0];

    modelCalcMatrices(&renderData, &g_CurrentPlayer->model);

    g_CurrentPlayer->standheight = g_CurrentPlayer->bondheadmatrices[0].m[3][1];
    g_CurrentPlayer->standbodyoffset.x = 0.0f;
    g_CurrentPlayer->standbodyoffset.y = g_CurrentPlayer->bondheadmatrices[1].m[3][1] - g_CurrentPlayer->bondheadmatrices[0].m[3][1];
    g_CurrentPlayer->standbodyoffset.z = g_CurrentPlayer->bondheadmatrices[1].m[3][2] - g_CurrentPlayer->bondheadmatrices[0].m[3][2];

    modelSetAnimation(
        &g_CurrentPlayer->model,
        // match hack: addu address calculated backwards
        (struct ModelAnimation *) ((s32)g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].anim_id + (s32)&g_AnimTable->data),
        0,
        g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].loopframe,
        0.5f,
        0.0f);

    modelSetAnimLooping(&g_CurrentPlayer->model, g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].loopframe, 0.0f);
    modelSetAnimEndFrame(&g_CurrentPlayer->model, g_BondMoveAnimationSetup[g_CurrentPlayer->headanim].endframe);
    modelSetAnimFlipFunction(&g_CurrentPlayer->model, bheadFlipAnimation);

    bheadUpdateIdleRoll();
}


void bheadInitGaitModel(void) {
  player_gait_object_header.RootNode = (int)&player_gait_hdr;
  return;
}
