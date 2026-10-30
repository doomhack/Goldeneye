/**
 * Guard actions: what a guard is physically doing (actiontype). Each action has a
 * function that starts it and a tick function; chrActionTick runs the guard's AI list and
 * then the tick for its action. This file has standing, kneeling, scripted animations,
 * dodging (sidestep, jump out), surprise, surrender and alarm actions, the chrTry*
 * functions the AI commands call, and the background AI chrs (g_BgChrs) that run level
 * scripts not tied to a guard.
 * Moving is in chrnav.c, attacking and firing in chrattack.c, being hit and dying in
 * chrdamage.c. See docs/subsystems/guards.md.
 */
#include <ultra64.h>
#include <bondaicommands.h>
#include <bondgame.h>
#include <bondconstants.h>
#include "game/audio/sfxvolume.h"
#include "game/chr/chraction.h"
#include <limits.h>
#include <math.h>
#include "include/math.h"
#include <music.h>
#include <random.h>
#include "game/world/bg.h"
#include "game/world/bgfog.h"
#include "game/player/bondhead.h"
#include "game/player/bondview.h"
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/chr/chrai.h"
#include "game/save/file.h"
#include "game/menu/front.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/player/gun.h"
#include "game/model/anim.h"
#include "game/prop/setup.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/player/player.h"
#include "game/prop/propobj.h"
#include "game/prop/door.h"
#include "game/nav/stan.h"
#include "game/nav/waypoint.h"
#include "game/prop/alarm.h"
#include "game/math/physics.h"
#include "game/math/line_tri_intersect.h"



static void chrStand(ChrRecord *self);
static void chrSpotChooseAnimation(ChrRecord *self, f32 mergetime);
static void chrSidestep(ChrRecord *self, GUNHAND side);
static void chrJumpOut(ChrRecord *self, GUNHAND side);
static f32 chrGetClearDistInDir(PropRecord *prop, f32 angle, f32 maxdist, s32 cdtypes, f32 unkHeight, f32 unkA);
static void chrNormDistanceToPlayer(ChrRecord *self, GUNHAND side, coord3d *vec);
static s32 chrIsClearLow(ChrRecord *self, GUNHAND side, f32 distance);
static void chrGetSideDir(ChrRecord *self, s32 side, coord3d *dir);
static s32 chrIsClear(ChrRecord *self, GUNHAND side, f32 distance);
static void chrTickStand(ChrRecord *self);
static void chrTickKneel(ChrRecord *actor);
static void chrTickAnim(ChrRecord *self);
static void chrTickSurrender(ChrRecord *self);
static void chrTickSidestep(ChrRecord *self);
static void chrTickJumpout(ChrRecord *self);
static void chrTickTest(ChrRecord *self);
static void chrTickStartAlarm(ChrRecord *self);
static void chrTickSurprised(ChrRecord *self);
static void chrTickThrowGrenade(ChrRecord *self);
static void chrTickBondIntro(ChrRecord *self);


/* weaponprop is a held weapon's prop */
u32 weaponIsOneHanded(PropRecord *weaponprop)
{
    if (weaponprop != NULL)
    {
        return weaponHasFlag(weaponprop->weapon->weaponnum, WEAPONSTATBITFLAG_ONLY_1_HANDED);
    }

    return 0U;
}


void chrStandChooseAnimation(ChrRecord *self, f32 duration)
{
    PropRecord *left;
    PropRecord *right;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    if (
        ((left != NULL) && (right != NULL))
        || ((left == NULL) && (right == NULL))
        || (weaponIsOneHanded(left) != 0)
        || (weaponIsOneHanded(right) != 0))
    {
        modelSetAnimation(self->model, ANIM(idle_unarmed), randomGetNext() & 1, 0, 0.25f, duration);
        modelSetAnimLooping(self->model, 0, 16.0f);
    }
    else if ((right != NULL) || (left != NULL))
    {
        modelSetAnimation(self->model, ANIM(idle), left != NULL, 0, 0.25f, duration);
        modelSetAnimLooping(self->model, 0, 16.0f);
        modelSetAnimEndFrame(self->model, 120.0f);
    }

    return;
}


#ifdef REFRESH_PAL
#define RATE 1.2f
#else
#define RATE 1.0f
#endif

/**
 * At the end of a character's kneeling animation smoothly merge into their standing animation.
 */
void chrStandImmediate(ChrRecord *self, f32 mergetime)
{
    f32 fsleep;

    chrStopFiring(self);
    self->actiontype = ACT_STAND;

    self->act_stand.prestand = 0;
    self->act_stand.face_entitytype = 0;
    self->act_stand.face_entityid = 0;
    self->act_stand.reaim = 0;
    self->act_stand.turning = 2;
    self->act_stand.checkfacingwall = 0;
    //eu bug, doesnt use pal version of CHRLV_SEEN_RECENT_CHECK) + CHRLV_DEFAULT_TIMER;
    //so temp hardcoded to 120) + 180;
    self->act_stand.wallcount = (randomGetNext() % (u32) 120) + 180;

    fsleep = mergetime;

    if (self->model->playspeed != RATE)
    {
#if defined(BUGFIX_R1)
        fsleep *= (RATE / self->model->playspeed);
#else
        fsleep = mergetime / self->model->playspeed;
#endif
    }

    if (fsleep > 127.0f)
    {
        fsleep = 127.0f;
    }

    self->sleep = (s8) (s32) fsleep;
    chrStandChooseAnimation(self, mergetime);
}


/**
 * @param arg0: guard
 * @param min: min reaction speed range
 * @param max: max reaction speed range
 */
f32 chrGetGuard007SpeedRating(ChrRecord *self, f32 min, f32 max)
{
    f32 ret;

    ret = (f32) self->speedrating;
    ret = (file007Reaction() * (100.0f - ret)) + ret;
    return ((ret * (max - min)) / 100.0f) + min;
}


/**
 * @param self: guard
 * @param scale: scale factor
 */
s32 chrGetGuard007SpeedRatingInt(ChrRecord *self, s32 scale)
{
    s32 ret;

    ret = (s32) self->speedrating;
    ret = (s32)(file007Reaction() * (f32)(100 - ret)) + ret;
    return ((100 - ret) * scale) / 100;
}


/**
 * @param arg0: guard
 * @param min: min argh speed range
 * @param max: max argh speed range
 */
f32 chrGetGuard007ArghRating(ChrRecord *self, f32 min, f32 max)
{
    f32 ret;

    ret = (f32) self->arghrating;
    ret = (file007Reaction() * (100.0f - ret)) + ret;
    return ((ret * (max - min)) / 100.0f) + min;
}


static void chrStand(ChrRecord *self)
{
    if (self->actiontype == ACT_KNEEL)
    {
        chrStopFiring(self);

        self->actiontype = ACT_STAND;
        self->act_stand.prestand = 1;
        self->act_stand.face_entitytype = 0;
        self->act_stand.face_entityid = 0;
        self->act_stand.reaim = 0;
        self->act_stand.turning = 2;
        self->act_stand.checkfacingwall = 0;
        // bug/typo??: this is the only code like this not adjusted for VERSION_EU
        self->act_stand.wallcount = (randomGetNext() % 120) + 180;
        self->sleep = 0;

        if (modelGetAnim(self->model) == ANIM(fire_kneel_forward_one_handed_weapon_slow))
        {
            modelSetAnimation(self->model, ANIM(fire_kneel_forward_one_handed_weapon_slow), (s32) self->model->flip, 109.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
            modelSetAnimEndFrame(self->model, 140.0f);
        }
        else
        {
            modelSetAnimation(self->model, ANIM(fire_kneel_left_leg), (s32) self->model->flip, 120.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
            modelSetAnimEndFrame(self->model, 151.0f);
        }

        return;
    }

    chrStandImmediate(self, 16.0f);
}


/**
 * PD: func0f02ed28
 */
void chrStandImmediateCheckWall(ChrRecord *self, f32 mergetime)
{
    chrStandImmediate(self, mergetime);
    self->act_stand.checkfacingwall = 1;
}


/**
 * PD: chrStop
 */
void chrStandCheckWall(ChrRecord *self)
{
    chrStand(self);
    self->act_stand.checkfacingwall = 1;
}


/**
 * PD: chrKneelChooseAnimation
 */
void chrKneelChooseAnimation(ChrRecord *self)
{
    PropRecord *left;
    PropRecord *right;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);
    chrStopFiring(self);

    if ((left && right)
        || (!left && !right)
        || weaponIsOneHanded(left)
        || weaponIsOneHanded(right))
    {
        s32 r = randomGetNext() & 1;
        modelSetAnimation(self->model, ANIM(fire_kneel_forward_one_handed_weapon_slow), r, 0.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
        modelSetAnimEndFrame(self->model, 28.0f);
    }
    else if (right || left)
    {
        modelSetAnimation(self->model, ANIM(fire_kneel_left_leg), left != NULL, 0.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
        modelSetAnimEndFrame(self->model, 27.0f);
    }

    self->actiontype = ACT_KNEEL;
    self->sleep = 0;
}


void chrStartAnim(ChrRecord *self, s32 animID, s32 startframe, s32 endframe, u8 bitfield, s32 interpol_time60)
{
    f32 startframef = (f32)startframe;
    f32 speed;

    speed = 0.5f;
    if ((bitfield & ANIM_REVERSE_LOOPING_ANIMATION) != 0)
    {
        speed = -0.5f;
    }

    chrStopFiring(self);
    modelSetAnimation(self->model, animation_table_ptrs1[animID], (bitfield & ANIM_MIRROR) != 0, startframef, speed, (f32)interpol_time60);

    if (endframe >= 0)
    {
        modelSetAnimEndFrame(self->model, (f32)endframe);
    }

    if ((bitfield & ANIM_TRANSLATION_SCALE_4X) != 0)
    {
        modelSetAnimTranslationScale(self->model, self->model->anim_translation_scale * 4.0f);
    }

    self->chrflags &= ~CHRFLAG_02000000;
    self->actiontype = ACT_ANIM;

    self->act_anim.alwaysAnimate = (bitfield & ANIM_UNKNOWN) != 0;
    self->act_anim.holdLastFrame = (bitfield & ANIM_LOOP_HOLD_LAST_FRAME) != 0;
    self->act_anim.playSfx       = (bitfield & ANIM_PLAY_SFX) != 0;
    self->act_anim.idleOnEnd     = (bitfield & ANIM_IDLE_POSE_WHEN_COMPLETE) != 0;
    self->act_anim.noTranslate   = (bitfield & ANIM_NO_TRANSLATION) != 0;

    if (self->act_anim.idleOnEnd)
    {
        self->sleep = (s8) interpol_time60;
    }
    else
    {
        self->sleep = 0;
    }
}


/**
 * Extend left hand = ACT_STARTALARM.
 *
 * PD: chrStartAlarmChooseAnimation
 */
void chrStartAlarmChooseAnimation(ChrRecord *self)
{
    PropRecord *left = chrGetEquippedWeaponProp(self, GUNLEFT);
    PropRecord *right = chrGetEquippedWeaponProp(self, GUNRIGHT);
    bool flip = FALSE;

    if (left && !right)
    {
        flip = TRUE;
    }
    else if ((left && right) || (!left && !right))
    {
        flip = randomGetNext() & 1;
    }

    chrStopFiring(self);

    self->actiontype = ACT_STARTALARM;
    self->sleep = 0;

    modelSetAnimation(self->model, ANIM(extending_left_hand), flip, 40.0f, 1.0f, 16.0f);
    modelSetAnimEndFrame(self->model, 82.0f);
}


/**
 * Play the grenade throw animation. Takes a PropRecord* as an argument but
 * does not use it.
 */
void chrThrowGrenade(ChrRecord *self, GUNHAND hand, s32 startframe)
{
    chrStopFiring(self);

    self->actiontype = ACT_THROWGRENADE;
    self->sleep = 0;

    if (startframe != 0)
    {
        modelSetAnimation(self->model, ANIM(fire_throw_grenade), hand != 0, 0.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
    }
    else
    {
        modelSetAnimation(self->model, ANIM(fire_throw_grenade), hand != 0, 84.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
    }

    modelSetAnimEndFrame(self->model, 193.0f);
}


static void chrSpotChooseAnimation(ChrRecord *self, f32 mergetime)
{
    PropRecord *left;
    PropRecord *right;
    s32 flip;
    f32 objarg4;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    flip = 0;
    if ((left != NULL) && (right == NULL))
    {
        flip = 1;
    }
    else if (((left != NULL) && (right != NULL)) || ((left == NULL) && (right == NULL)))
    {
        flip = randomGetNext() & 1;
    }

    objarg4 = chrGetGuard007SpeedRating(self, 0.6f, 0.96000004f); // 0.96000004 is different from 0.96
    modelSetAnimation(self->model, ANIM(spotting_bond), flip, 10.0f, objarg4, mergetime);
    modelSetAnimEndFrame(self->model, 52.0f);
}


void chrActorShuffleFeet(ChrRecord *self)
{
    f32 angle;

    angle = chrGetAngleToBond(self);

    if ((angle < 0.17453294f) || (angle > 6.1086526f))
    {
        chrSpotChooseAnimation(self, 16.0f);
        chrStopFiring(self);
        self->actiontype = ACT_SURPRISED;
        self->sleep = 0;

        return;
    }

    if (chrHasStoppedOrPatroling(self) == 0)
    {
        chrStand(self);
    }
}


void chrStartSurprised(ChrRecord *self)
{
    chrStopFiring(self);
    self->actiontype = ACT_SURPRISED;
    self->sleep = 0;
    modelSetAnimation(self->model, ANIM(surrendering_armed), randomGetNext() & 1, 0.0f, chrGetGuard007SpeedRating(self, 0.35f, 0.56f), 16.0f);
    modelSetAnimEndFrame(self->model, 7.0f);
}


/**
 * PD: chrSurprisedChooseAnimation
 */
void chrActorLookFlustered(ChrRecord *self)
{
    u32 choice;

    choice = randomGetNext() % 3U;

    chrStopFiring(self);

    self->actiontype = ACT_SURPRISED;
    self->sleep = 0;
    modelSetAnimation(self->model, ANIM(look_around), randomGetNext() & 1, 17.0f, 0.6f, 16.0f);

    if (choice == 0)
    {
        modelSetAnimEndFrame(self->model, chrGetGuard007SpeedRating(self, 38.0f, 8.0f));
    }
    else if (choice == 1)
    {
        modelSetAnimEndFrame(self->model, chrGetGuard007SpeedRating(self, 66.0f, 8.0f));
    }
    else
    {
        modelSetAnimEndFrame(self->model, chrGetGuard007SpeedRating(self, 96.0f, 8.0f));
    }
}


/**
 * PD: chrSurrenderChooseAnimation
 */
void chrActorThrowWeaponSurrender(ChrRecord *self)
{
    PropRecord *left;
    PropRecord *right;

    if (self->actiontype != ACT_SURRENDER)
    {
        left = chrGetEquippedWeaponProp(self, GUNLEFT);
        right = chrGetEquippedWeaponProp(self, GUNRIGHT);

        chrStopFiring(self);

        self->actiontype = ACT_SURRENDER;

        if ((right != NULL) || (left != NULL))
        {
            modelSetAnimation(self->model, ANIM(surrendering_armed_drop_weapon), randomGetNext() & 1, 0.0f, 0.5f, 16.0f);
            modelSetAnimLooping(self->model, 40.0f, 16.0f);

            self->sleep = 0x10;

            if (left != 0)
            {
                objSetDropType(left, 2);
            }
            if (right != 0)
            {
                objSetDropType(right, 2);
            }

            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
        }
        else
        {
            modelSetAnimation(self->model, ANIM(surrendering_armed), randomGetNext() & 1, 0.0f, 0.5f, 16.0f);
            modelSetAnimLooping(self->model, 30.0f, 16.0f);

            self->sleep = 0x10;
        }

        chrDropItems(self);
    }
}


void chrActorFadeAway(ChrRecord *self)
{
    if (self->actiontype != ACT_DEAD)
    {
        chrStopFiring(self);
        self->actiontype = ACT_DEAD;
        self->act_dead.fadeTime = -1;
        self->sleep = 0;
    }
}


/**
 * chrStepToSide
 * PD: chrSidestepChooseAnimation (Somewhat similar)
 */
static void chrSidestep(ChrRecord *self, GUNHAND side)
{
    PropRecord *left;
    PropRecord *right;
    s32 flip;
    s32 variant;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);
    flip = 0;
    variant = 0;

    if ((left != NULL) && (right != NULL))
    {
        flip = randomGetNext() & 1;
        variant = randomGetNext() & 1;
    }
    else if (weaponIsOneHanded(left) == 0)
    {
        if ((weaponIsOneHanded(right) == 0) && ((left != NULL) || (right != NULL)))
        {
            flip = left != 0;
            variant = randomGetNext() & 1;
        }
    }

    chrStopFiring(self);

    self->actiontype = ACT_SIDESTEP;
    self->sleep = 0;

    if (variant == 0)
    {
        if (side != GUNRIGHT)
        {
            modelSetAnimation(self->model, ANIM(side_step_left), 0, 5.0f, chrGetGuard007SpeedRating(self, 0.55f, 0.88000005f), 16.0f);
            modelSetAnimEndFrame(self->model, 27.0f);
        }
        else
        {
            modelSetAnimation(self->model, ANIM(side_step_left), 1, 5.0f, chrGetGuard007SpeedRating(self, 0.55f, 0.88000005f), 16.0f);
            modelSetAnimEndFrame(self->model, 27.0f);
        }

        return;
    }

    if (((side != GUNRIGHT) && (flip == 0)) ||
        ((side == GUNRIGHT) && (flip != 0)))
    {
        modelSetAnimation(self->model, ANIM(slide_left), flip, 5.0f, chrGetGuard007SpeedRating(self, 0.7f, 1.12f), 16.0f);
        modelSetAnimEndFrame(self->model, 34.0f);

    }
    else
    {
        modelSetAnimation(self->model, ANIM(slide_right), flip, 5.0f, chrGetGuard007SpeedRating(self, 0.7f, 1.12f), 16.0f);
        modelSetAnimEndFrame(self->model, 32.0f);
    }

    return;
}


/**
 * chrHopToSide
 * PD: chrSidestepChooseAnimation (somewhat similar)
 */
static void chrJumpOut(ChrRecord *self, GUNHAND side)
{
    PropRecord *left;
    PropRecord *right;
    s32 side2;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    side2 = GUNRIGHT;

    if ((left != NULL) && (right == NULL))
    {
        side2 = GUNLEFT;
    }
    else if (
        ((left != NULL) && (right != NULL))
        || ((left == NULL) && (right == NULL))
        || (weaponIsOneHanded(left) != 0)
        || (weaponIsOneHanded(right) != 0))
    {
        side2 = randomGetNext() & 1;
    }

    chrStopFiring(self);

    self->actiontype = ACT_JUMPOUT;
    self->sleep = 0;

    if (((side != GUNRIGHT) && (side2 == GUNRIGHT)) ||
        ((side == GUNRIGHT) && (side2 != GUNRIGHT)))
    {
        if ((randomGetNext() & 1) != 0)
        {
            modelSetAnimation(self->model, ANIM(fire_jump_to_side_left), side2, 5.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
            modelSetAnimEndFrame(self->model, 49.0f);
        }
        else
        {
            modelSetAnimation(self->model, ANIM(fire_jump_to_side_right), side2, 130.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
            modelSetAnimEndFrame(self->model, 173.0f);
        }

        return;
    }

    if ((randomGetNext() & 1) != 0)
    {
        modelSetAnimation(self->model, ANIM(fire_jump_to_side_right), side2, 20.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
        modelSetAnimEndFrame(self->model, 63.0f);
    }
    else
    {
        modelSetAnimation(self->model, ANIM(fire_jump_to_side_left), side2, 91.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
        modelSetAnimEndFrame(self->model, 136.0f);
    }

    return;
}






/**
 * Line-line intersection, where arg0 and arg1 are two points on line1, and arg2 and arg3 are a point and a direction of line2.
 * 3d coord/vector are passed as arguments, but only the 2d (x,z) values are used to find the intersection.
 *
 * @param line1_p1: first point to describe line1
 * @param line1_p2: second point to describe line1
 * @param line2_p3: first point to describe line2
 * @param dir: vector giving direction of line2
 * @param result: contains result
 */
void chrLineLineIntersection(coord3d *line1_p1, coord3d *line1_p2, coord3d *line2_p3, coord3d *dir, coord3d *result)
{
    /*
     * Line1 = P1 + u * (P2 - P1)
     * Line2 = P3 + v * D
     *
     * Intersection is where Line1==Line2, or:
     *     P1 + u * (P2 - P1) = P3 + v * D
     *
     * u and v are unknown.
     *
     * Isolate u:
     *
     * u = (P3 + v*D - P1) / (P2 - P1)
     */
    f32 denom;

    // solve for v. (much algebra follows, not shown)

    denom = (dir->f[2] * (line1_p2->f[0] - line1_p1->f[0])) - (dir->f[0] * (line1_p2->f[2] - line1_p1->f[2]));

    if (denom != 0.0f)
    {
        f32 v = (
            ((line1_p1->f[2] - line2_p3->f[2]) * (line1_p2->f[0] - line1_p1->f[0]))
            + ((line2_p3->f[0] - line1_p1->f[0]) * (line1_p2->f[2] - line1_p1->f[2]))
        ) / denom;

        // v is known, denom is non-zero, plug back into equation for Line2 = P3 + v * D

        result->f[0] = line2_p3->f[0] + (dir->f[0] * v);
        result->f[1] = line2_p3->f[1] + (dir->f[1] * v);
        result->f[2] = line2_p3->f[2] + (dir->f[2] * v);
    }
    else if ((dir->f[0] == 0.0f) && (dir->f[2] == 0.0f))
    {
        // else, denominator is zero, but direction is also zero, so assume Line 2 point as result
        result->f[0] = line2_p3->f[0];
        result->f[1] = line2_p3->f[1];
        result->f[2] = line2_p3->f[2];
    }
    else
    {
        // all other cases, fallback to Line 1 first point
        result->f[0] = line1_p1->f[0];
        result->f[1] = line1_p1->f[1];
        result->f[2] = line1_p1->f[2];
    }
}


/**
 * Line-line intersection.
 * The first two points are retrieved from stanGetCollisionEdge.
 * The arguments to the method supply the other line, described by a point and direction.
 *
 * 3d coord/vector are passed as arguments, but only the 2d (x,z) values are used to find the intersection.
 *
 * @param line2_p3: first point to describe line2
 * @param dir: vector giving direction of line2
 * @param result: out parameter, contains result.
 */
void chrStanLineDirIntersection(coord3d *line2_p3, coord3d *dir, coord3d *result)
{
    coord3d edgeStart;
    coord3d edgeEnd;

    stanGetCollisionEdge(&edgeStart, &edgeEnd);
    chrLineLineIntersection(&edgeStart, &edgeEnd, line2_p3, dir, result);
}


/**
 * @param arg0:
 * @param arg1:
 * @param result: out parameter, contains result.
 */
void chrStanPointPointIntersection(coord3d *pos, coord3d *dir, coord3d *result)
{
    coord3d edgeStart;
    coord3d edgeEnd;
    f32 v;

    stanGetCollisionEdge(&edgeStart, &edgeEnd);

    // see comments in chrLineLineIntersection

    v = ((dir->f[0] * (edgeStart.f[2] - pos->f[2])) - (dir->f[2] * (edgeStart.f[0] - pos->f[0])))
        / ((dir->f[2] * (edgeEnd.f[0] - edgeStart.f[0])) - (dir->f[0] * (edgeEnd.f[2] - edgeStart.f[2])));

    result->f[0] = edgeStart.f[0] + ((edgeEnd.f[0] - edgeStart.f[0]) * v);
    result->f[1] = edgeStart.f[1] + ((edgeEnd.f[1] - edgeStart.f[1]) * v);
    result->f[2] = edgeStart.f[2] + ((edgeEnd.f[2] - edgeStart.f[2]) * v);
}


static f32 chrGetClearDistInDir(PropRecord *prop, f32 angle, f32 maxdist, s32 cdtypes, f32 unkHeight, f32 unkA)
{
    coord3d dir;
    f32 dest_x;
    f32 dest_z;
    StandTile *stan;
    ChrRecord *chr;
    f32 ret;
    coord3d hitpos;

    stan = prop->stan;
    chr = prop->chr;

    dir.f[0] = sinf(angle);
    dir.f[1] = 0.0f;
    dir.f[2] = cosf(angle);
    dest_x = prop->pos.f[0] + (dir.f[0] * maxdist);
    dest_z = prop->pos.f[2] + (dir.f[2] * maxdist);

    chrSetMoving(chr, 0);
    stanResetHits();

    if (stanTestLineUnobstructed(&stan, prop->pos.f[0], prop->pos.f[2], dest_x, dest_z, cdtypes, unkHeight, unkA, 0.0f, 1.0f) != 0)
    {
        ret = maxdist;
    }
    else
    {
        chrStanLineDirIntersection(&prop->pos, &dir, &hitpos);
        dest_x = hitpos.f[0] - prop->pos.f[0];
        dest_z = hitpos.f[2] - prop->pos.f[2];
        ret = sqrtf((dest_x * dest_x) + (dest_z * dest_z));
    }

    chrSetMoving(chr, 1);

    return ret;
}


f32 chrGetClearDistInDirDefault(PropRecord *prop, f32 angle, f32 maxdist)
{
    f32 height;
    f32 always20;
    f32 width;

    chrGetChrWidthHeight(prop, &width, &height, &always20);
    return chrGetClearDistInDir(prop, angle, maxdist, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, height, always20);
}








/**
 * get vector to run on
 * @param self:
 * @param side: If GUNLEFT set result is (dz, -dx), otherwise (-dz, dx).
 * @param arg2: Out parameter. Contains result vector.
*/
static void chrNormDistanceToPlayer(ChrRecord *self, GUNHAND side, coord3d *vec)
{
    PropRecord *prop;
    f32 norm;
    f32 dx;
    f32 dz;
    PropRecord *player_prop;

    prop = self->prop;
    player_prop = currentPlayerGetProp();
    dx = player_prop->pos.f[0] - prop->pos.f[0];
    dz = player_prop->pos.f[2] - prop->pos.f[2];

    norm = sqrtf((dx * dx) + (dz * dz));

    dx = dx / norm;
    dz = dz / norm;

    if (side != GUNRIGHT)
    {
        vec->f[1]  = 0;
        vec->f[0]  = dz;
        vec->f[2]  = -(dx);
    }
    else
    {
        vec->f[2]  = dx;
        vec->f[0]  = -(dz);
        vec->f[1]  = 0;
    }
}


/**
 * @see chrIsClear
*/
static s32 chrIsClearLow(ChrRecord *self, GUNHAND side, f32 distance)
{
    PropRecord *prop;
    coord3d dir;
    coord3d dest;

    prop = self->prop;
    chrNormDistanceToPlayer(self, side, &dir);

    dest.f[0] = prop->pos.f[0] + (dir.f[0] * distance);
    dest.f[1] = prop->pos.f[1];
    dest.f[2] = prop->pos.f[2] + (dir.f[2] * distance);

    return propCanWalkLine(prop, &dest, &dir);
}


/**
 * @param self:
 * @param arg1: flag. If set result is (cos, -sin), otherwise (-cos, sin).
 * @param arg2: out parameter, contains coordinate result.
*/
static void chrGetSideDir(ChrRecord *self, s32 side, coord3d *dir)
{
    f32 angle;

    angle = modelGetRootRotY(self->model);

    if (side != 0)
    {
        dir->f[0] = cosf(angle);
        dir->f[1] = 0.0f;
        dir->f[2] = -sinf(angle);
    }
    else
    {
        dir->f[0] = -cosf(angle);
        dir->f[1] = 0.0f;
        dir->f[2] = sinf(angle);
    }
}


/**
 * @see chrIsClearLow
*/
static s32 chrIsClear(ChrRecord *self, GUNHAND side, f32 distance)
{
    PropRecord *prop;
    coord3d dir;
    coord3d dest;

    prop = self->prop;
    chrGetSideDir(self, side, &dir);

    dest.f[0] = prop->pos.f[0] + (dir.f[0] * distance);
    dest.f[1] = prop->pos.f[1];
    dest.f[2] = prop->pos.f[2] + (dir.f[2] * distance);

    return propCanWalkLine(prop, &dest, &dir);
}


bool chrIsNotDeadOrShot(ChrRecord *self)
{
    s8 currentaction = self->actiontype;

    if ((currentaction == ACT_DIE) || (currentaction == ACT_DEAD) || (currentaction == ACT_PREARGH)
        || ((currentaction == ACT_ARGH) && !(self->chrflags & CHRFLAG_00000200)))
    {
        return FALSE;
    }

    return TRUE;
}


bool chrIsDead(ChrRecord *self)
{
    s8 currentaction = self->actiontype;

    return ((currentaction == ACT_DIE) || (currentaction == ACT_DEAD));
}


bool chrTrySidestep(ChrRecord *self)
{
    PropRecord *myprop;
    PropRecord *bondprop;
    f32 myRadDirection;
    f32 myRadDirectionToBond;
    f32 radChangeToFaceBond;
    GUNHAND HopOtherDirection; //needed for stack size
    GUNHAND HopDirection;
    f32 myNormalizedRadToBond;

    if (chrIsNotDeadOrShot(self))
    {
        myprop                = self->prop;
        bondprop              = currentPlayerGetProp();
        myRadDirection        = modelGetRootRotY(self->model);
        myRadDirectionToBond  = mathAtan2(bondprop->pos.x - myprop->pos.x, bondprop->pos.z - myprop->pos.z);
        radChangeToFaceBond   = myRadDirectionToBond - myRadDirection;
        myNormalizedRadToBond = radChangeToFaceBond;

        if (myRadDirectionToBond < myRadDirection) //avoid negative radians
        {
            myNormalizedRadToBond = radChangeToFaceBond + M_TAU_F;
        }

        if ((myNormalizedRadToBond < DegToRad(45)) || (myNormalizedRadToBond > DegToRad(360.0 - 45)) ||        /*Front*/
            ((myNormalizedRadToBond > DegToRad(180.0 - 45)) && (myNormalizedRadToBond < DegToRad(180.0 + 45))) /*Back*/
        )
        {
            HopDirection = (randomGetNext() & 1) == 0;         //Hop Left or Right
            if (chrIsClear(self, HopDirection, 100.0f)) //able to step dir?
            {
                chrSidestep(self, HopDirection);
                return TRUE;
            }

            HopOtherDirection = HopDirection == 0;

            if (chrIsClear(self, HopOtherDirection, 100.0f)) //able to step other dir?
            {
                chrSidestep(self, HopOtherDirection);
                return TRUE;
            }
        }
    }

    return FALSE; //unable to step
}


bool chrTryJumpOut(ChrRecord *self)
{
    PropRecord *myprop;
    PropRecord *bondprop;
    f32 myRadDirection;
    f32 myRadDirectionToBond;
    f32 radChangeToFaceBond;
    GUNHAND HopOtherDirection; //needed for stack size
    GUNHAND HopDirection;
    f32 myNormalizedRadToBond;

    if (chrIsNotDeadOrShot(self))
    {
        myprop                = self->prop;
        bondprop              = currentPlayerGetProp();
        myRadDirection        = modelGetRootRotY(self->model);
        myRadDirectionToBond  = mathAtan2(bondprop->pos.x - myprop->pos.x, bondprop->pos.z - myprop->pos.z);
        radChangeToFaceBond   = myRadDirectionToBond - myRadDirection;
        myNormalizedRadToBond = radChangeToFaceBond;

        if (myRadDirectionToBond < myRadDirection) //avoid negative radians
        {
            myNormalizedRadToBond = radChangeToFaceBond + M_TAU_F;
        }

        if ((myNormalizedRadToBond < DegToRad(45)) || (myNormalizedRadToBond > DegToRad(360.0 - 45)) ||        /*Front*/
            ((myNormalizedRadToBond > DegToRad(180.0 - 45)) && (myNormalizedRadToBond < DegToRad(180.0 + 45))) /*Back*/
        )
        {
            HopDirection = (randomGetNext() & 1) == 0;         //Hop Left or Right

            if (chrIsClear(self, HopDirection, 200.0f)) //able to hop dir?
            {
                chrJumpOut(self, HopDirection);
                return TRUE;
            }

            HopOtherDirection = HopDirection == 0;

            if (chrIsClear(self, HopOtherDirection, 200.0f)) //able to hop other dir?
            {
                chrJumpOut(self, HopOtherDirection);
                return TRUE;
            }
        }
    }

    return FALSE; //unable to hop
}


bool chrTryRunSideways(ChrRecord *self)
{
    PropRecord *myprop;
    f32         distToRun;
    coord3d       TargetVector;
    coord3d     TargetCoord;

    if (chrIsNotDeadOrShot(self) && ((g_GlobalTimer - self->lastwalk60) >= CHRLV_RECENT_TIME_CHECK)) //>3 seconds since last walk
    {
        myprop    = self->prop;
        distToRun = ((u32)randomGetNext() * (1.0f / UINT_MAX) * 200.0f) + 200.0f;         //random dist to run between 0 and 200
        chrNormDistanceToPlayer(self, ((u32)randomGetNext() & 1) == 0, &TargetVector);  //get vector to run on

        TargetCoord.x = (TargetVector.x * distToRun) + myprop->pos.x;
        TargetCoord.y = myprop->pos.y;
        TargetCoord.z = (TargetVector.z * distToRun) + myprop->pos.z;

        if (propCanWalkLine(myprop, &TargetCoord, &TargetVector))
        {
            chrRunToPos(self, &TargetCoord);
            return TRUE;
        }

        TargetVector.x = -TargetVector.x;
        TargetVector.z = -TargetVector.z;
        TargetCoord.x  = (TargetVector.x * distToRun) + myprop->pos.x;
        TargetCoord.y  = myprop->pos.y;
        TargetCoord.z  = (TargetVector.z * distToRun) + myprop->pos.z;


        if (propCanWalkLine(myprop, &TargetCoord, &TargetVector))
        {
            chrRunToPos(self, &TargetCoord);
            return TRUE;
        }
    }

    return FALSE;
}


bool chrTryAttackWalk(ChrRecord *self)
{
    PropRecord *myprop;
    PropRecord *bondprop;

    if (chrIsNotDeadOrShot(self))
    {
        myprop   = self->prop;
        bondprop = currentPlayerGetProp();

        if (
            (chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT) || chrGetEquippedWeaponPropWithCheck(self, GUNLEFT))
            &&
            ((g_GlobalTimer - self->lastwalk60) >= CHRLV_RECENT_TIME_CHECK)
            )
        {
            f32 dx = bondprop->pos.x - myprop->pos.x;
            f32 dy = bondprop->pos.y - myprop->pos.y;
            f32 dz = bondprop->pos.z - myprop->pos.z;

            if ( (SQR(dx) + SQR(dy) + SQR(dz)) >= (1000000.0f))
            {
                chrInitActAttackWalk(self, SPEED_WALK);
                return TRUE;
            }
        }
    }

    return FALSE;
}


bool chrTryAttackRun(ChrRecord *self)
{
    PropRecord *myprop;
    PropRecord *bondprop;

    if (chrIsNotDeadOrShot(self))
    {
        myprop   = self->prop;
        bondprop = currentPlayerGetProp();

        if (
            (chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT) || chrGetEquippedWeaponPropWithCheck(self, GUNLEFT))
            &&
            ((g_GlobalTimer - self->lastwalk60) >= CHRLV_RECENT_TIME_CHECK)
            )
        {
            f32 dx = bondprop->pos.x - myprop->pos.x;
            f32 dy = bondprop->pos.y - myprop->pos.y;
            f32 dz = bondprop->pos.z - myprop->pos.z;

            if ((SQR(dx) + SQR(dy) + SQR(dz)) >= (1000000.0f))
            {
                chrInitActAttackWalk(self, SPEED_RUN);
                return TRUE;
            }
        }
    }

    return FALSE;
}


bool chrTryAttackRoll(ChrRecord *self)
{
    PropRecord *myprop;
    PropRecord *bondprop;

    coord3d vec;

    GUNHAND HopOtherDirection;
    GUNHAND HopDirection;
    float vec2rd;

    if (chrIsNotDeadOrShot(self))
    {
        myprop   = self->prop;
        bondprop = currentPlayerGetProp();

        if (chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT) || chrGetEquippedWeaponPropWithCheck(self, GUNLEFT))
        {
            vec.x  = bondprop->pos.x - myprop->pos.x;
            vec.y  = bondprop->pos.y - myprop->pos.y;
            vec.z  = bondprop->pos.z - myprop->pos.z;
            vec2rd = SQR(vec.x) + SQR(vec.y) + SQR(vec.z);

            if (SQR(200.0f) <= vec2rd) /*Bond GT 200 from chr*/
            {
                HopDirection = (randomGetNext() & 1) == 0; //Hop Left or Right

                if (chrIsClearLow(self, HopDirection, 200))
                {
                    chrInitActAttackRoll(self, HopDirection);
                    return TRUE;
                }

                HopOtherDirection = HopDirection == 0;

                if (chrIsClearLow(self, HopOtherDirection, 200))
                {
                    chrInitActAttackRoll(self, HopOtherDirection);
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}


bool chrTryAttackStand(ChrRecord *self, s32 attack_type, s32 b)
{
    if ((chrIsNotDeadOrShot(self)) &&
        ((chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT)) || (chrGetEquippedWeaponPropWithCheck(self, GUNLEFT))))
    {
        chrAttackStand(self, attack_type, b);
        return TRUE;
    }

    return FALSE;
}


bool chrTryAttackKneel(ChrRecord *self, s32 targettype, s32 targetid)
{
    if ((chrIsNotDeadOrShot(self)) &&
        ((chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT)) || (chrGetEquippedWeaponPropWithCheck(self, GUNLEFT))))
    {
        chrAttackKneel(self, targettype, targetid);
        return TRUE;
    }

    return FALSE;
}


bool chrTryModifyAttack(ChrRecord *self, s32 newtargettype, s32 newtargetid)
{
    if (self->actiontype == ACT_ATTACK)
    {
        if (self->act_attack.attacktype & (TARGET_AIM_ONLY | TARGET_DONTTURN))
        {
            self->act_attack.attacktype = newtargettype;
            self->act_attack.entityid   = newtargetid;
            chrAttackSetEndFrame(self);
            return TRUE;
        }
    }

    return FALSE;
}


bool chrTryStop(ChrRecord *self, s32 faceentitytype, s32 faceentityid)
{
    if (chrIsNotDeadOrShot(self) != 0)
    {
        if (self->actiontype != ACT_STAND)
        {
            chrStand(self);
        }

        self->act_stand.face_entitytype = faceentitytype;
        self->act_stand.face_entityid   = faceentityid;
        self->act_stand.reaim          = 0;
        self->act_stand.checkfacingwall          = 0;

        return TRUE;
    }

    return FALSE;
}




bool chrTryStartPatrol(ChrRecord *self, PathRecord *path)
{
    if (path != NULL && chrIsNotDeadOrShot(self))
    {
        chrStartPatrol(self, path);
        return TRUE;
    }

    return FALSE;
}


static void chrTickStand(ChrRecord *self)
{
    s32 i;             // any
    PropRecord *left;  // 160
    PropRecord *right; // 156
    s32 index;         // any
    f32 cleardist[8];       // 116
    f32 subroty;       // 112
    f32 angle;       // 108
    f32 subrotyarg2;   // any
    s32 j;             // any
    s32 order[8];       // 68

    if (self->sleep > 0)
    {
        return;
    }

    if (self->act_stand.prestand != 0)
    {
        // needs to save $f0 into sp(0x3c)
        if (modelGetAnimFrame(self->model) >= modelGetAnimEndFrame(self->model))
        {
            chrStandChooseAnimation(self, 8.0f);
            self->act_stand.prestand = 0;
        }

        self->sleep = 0;

        return;
    }

    if (self->act_stand.face_entitytype > 0)
    {
        if (self->act_stand.reaim)
        {
            subrotyarg2 = modelGetAnim(self->model)->numFrames - 1;
            self->act_stand.turning = chrSetSubroty(self, self->act_stand.turning, subrotyarg2, 1.0f, 0.0f);

            if (self->act_stand.turning != 1)
            {
                chrStandChooseAnimation(self, 8.0f);
                self->act_stand.reaim = 0;

                if (self->act_stand.face_entitytype & 0x10)
                {
                    self->act_stand.face_entitytype = 0;
                }
            }
        }
        else
        {
            angle = chrGetAngleToTarget(self, self->act_stand.face_entitytype, self->act_stand.face_entityid);
            if ((angle > 0.34906587f) && (angle < 5.9341197f))
            {
                left = chrGetEquippedWeaponProp(self, 1);
                right = chrGetEquippedWeaponProp(self, 0);

                self->act_stand.reaim = 1;
                self->act_stand.turning = 1;

                if (((left != NULL) && (right != NULL))
                    || ((left == NULL) && (right == NULL))
                    || weaponIsOneHanded(left)
                    || weaponIsOneHanded(right))
                {
                    // required to fix stack above
                    // looks like it doesn't matter which `s32` is used.
                    i = (s32)((u32)randomGetNext() & 1U);
                    modelSetAnimation(
                        self->model,
                        // awkward fix: addu instruction is backwards
                        ANIM(walking_unarmed),
                        i,
                        0.0f,
                        0.5f,
                        16.0f);

                    modelSetAnimEndFrame(
                        self->model,
                        (ANIM(walking_unarmed)->numFrames - 1));
                }
                else if ((right != NULL) || (left != NULL))
                {
                    modelSetAnimation(
                        self->model,
                        // awkward fix: addu instruction is backwards
                        ANIM(walking),
                        left != NULL,
                        0.0f,
                        0.5f,
                        16.0f);

                    modelSetAnimEndFrame(
                        self->model,
                        (ANIM(walking)->numFrames - 1));
                }
            }
            else if (self->act_stand.face_entitytype & 0x10)
            {
                self->act_stand.face_entitytype = 0;
            }
        }

        self->sleep = 0;

        return;
    }

    self->sleep = ((u32)randomGetNext() % 5U) + 0xE;

    if (self->act_stand.checkfacingwall)
    {
        if (self->chrflags & CHRFLAG_00000080)
        {
            self->act_stand.checkfacingwall = 0;
            return;
        }

        self->act_stand.wallcount -= self->sleep;
        if (self->act_stand.wallcount < 0)
        {
            subroty = modelGetRootRotY(self->model);

            angle = subroty;
            for (i = 0; i < 8; i++)
            {
                angle += DegToRad(45);

                if (angle >= M_TAU_F)
                {
                    angle -= M_TAU_F;
                }

                cleardist[i] = chrGetClearDistInDir(self->prop, angle, 1000.0f, 0, 0.0f, 1.0f);
            }

            for (i = 0; i < 8; i++)
            {
                order[i] = i;
            }

            /**
             * Selection sort.
            */
            for (i=0; i<7; i++)
            {
                index = i;

                for (j = i + 1; j < 8; j++)
                {
                    if (cleardist[order[j]] < cleardist[order[index]])
                    {
                        index = j;
                    }
                }

                j = order[i];
                order[i] = order[index];
                order[index] = j;
            }

            index = -1;
            if (cleardist[0] < 490.0f)
            {
                if (cleardist[order[4]] < 200.0f)
                {
                    index = 7;
                }
                else if ((order[0] == 0) || (order[1] == 0) || (order[2] == 0))
                {
                    if (((order[3] == 4) || (order[4] == 4)) && ((randomGetNext() % 3U) == 0))
                    {
                        if (order[3] == 4)
                        {
                            index = 3;
                        }
                        else
                        {
                            index = 4;
                        }
                    }
                    else
                    {
                        index = 5 + (randomGetNext() % 3U);
                    }
                }
                else if (((order[0] == 1) || (order[0] == 7)) && (order[5] != 0) && (order[6] != 0) && (order[7] != 0))
                {
                    index = 5 + (randomGetNext() % 3U);
                }
            }

            if (index >= 0)
            {
                i = order[index];
                angle = ((f32)i * M_TAU_F * 0.125f) + subroty;

                if (angle >= M_TAU_F)
                {
                    angle -= M_TAU_F;
                }

                chrTryStop(self, 0x10, (s32) ((angle * M_U16_MAX_VALUE_F) / M_TAU_F));
            }
            else
            {
                self->act_stand.checkfacingwall = 0;
            }
        }
    }
}


static void chrTickKneel(ChrRecord *actor)
{
    actor->sleep = 0;
}

// #still chraction.c


static void chrTickAnim(ChrRecord *self)
{

    if (self->act_anim.holdLastFrame == 0)
    {
        f32 frame = modelGetAnimFrame(self->model);

        if (modelGetAnimEndFrame(self->model) <= frame)
        {
            chrStand(self);
        }
    }

    if (
        (modelGetAnim(self->model) == ANIM(sneeze))
        && (modelGetAnimFrame(self->model) >= 42.0f)
        && !(self->chrflags & CHRFLAG_02000000)
       )
    {
        if (((g_LvTickCount & 1) == 0) && (chrGetDistanceToBond(self) < 800.0f))
        {
            sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, SNEEZE_SFX, 0), &self->prop->pos);
        }

        self->chrflags |= CHRFLAG_02000000;
    }

    if (((s32) self->sleep <= 0) && (self->act_anim.idleOnEnd != 0))
    {
        self->sleep = (randomGetNext() % 5U) + 0xE;
    }
}


static void chrTickSurrender(ChrRecord *self)
{
    Model *model;

    if ((s32) self->sleep <= 0)
    {
        model = self->model;
        self->sleep = 0x10;

        if ((modelGetAnim(model) == ANIM(surrendering_armed_drop_weapon))
            && (modelGetAnimFrame(model) >= 80.0f))
        {
            coord3d dir = {0};

            f32 t = modelGetRootRotY(model);

            dir.f[0] = -sinf(t);
            dir.f[2] = -cosf(t);

            if (propCanWalkInDir(self->prop, &dir, 20.0f) == 0)
            {
                modelSetAnimation(self->model, ANIM(surrendering_armed), randomGetNext() & 1, 30.0f, 0.5f, 16.0f);
                modelSetAnimLooping(self->model, 30.0f, 16.0f);
            }
        }
    }
}




/**
 * @see chrTickJumpout
 * @see chrTickTest
 * @see chrTickStartAlarm
*/
static void chrTickSidestep(ChrRecord *self)
{
    Model *model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        chrSetTargetToPlayer(self);
        chrStandImmediateCheckWall(self, 10.0f);
    }
}


/**
 * @see chrTickSidestep
 * @see chrTickTest
 * @see chrTickStartAlarm
*/
static void chrTickJumpout(ChrRecord *self)
{
    Model *model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        chrSetTargetToPlayer(self);
        chrStandCheckWall(self);
    }
}


/**
 * @see chrTickSidestep
 * @see chrTickJumpout
 * @see chrTickStartAlarm
*/
static void chrTickTest(ChrRecord *self)
{
    Model *model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        chrStand(self);
    }
}


/**
 * @see chrTickSidestep
 * @see chrTickJumpout
 * @see chrTickTest
*/
static void chrTickStartAlarm(ChrRecord *self)
{
    Model *model = self->model;

    // bug/typo, should be 50.0f on VERSION_EU
    if (modelGetAnimFrame(model) >= 60.0f)
    {
        alarmActivate();
    }

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        chrStandCheckWall(self);
    }
}


static void chrTickSurprised(ChrRecord *self)
{
    Model *model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        if (modelGetAnim(model) == ANIM(surrendering_armed))
        {
            chrStandImmediateCheckWall(self, 26.0f);
        }
        else if (modelGetAnim(model) == ANIM(spotting_bond))
        {
            chrStandImmediateCheckWall(self, 26.0f);
        }
        else
        {
            chrStandCheckWall(self);
        }
    }
}




static void chrTickThrowGrenade(ChrRecord *self)
{
    Model *self_model;
    f32 frame;
    s32 gunhand;
    PropRecord *held_prop;

    self_model = self->model;
    frame = modelGetAnimFrame(self_model);
    gunhand = (self_model->flip != GUNRIGHT) ? GUNLEFT : GUNRIGHT;
    held_prop = chrGetEquippedWeaponProp(self, gunhand);

    if ((frame >= 20.0f) && (held_prop != NULL))
    {
        struct ObjectRecord *obj = held_prop->obj;
        obj->runtime_bitflags &= ~0x800;
    }

    if ((frame >= 61.0f) && (held_prop != NULL))
    {
        struct WeaponObjRecord *weap = held_prop->weapon;
        weap->timer = CHRLV_DEFAULT_TIMER;
    }

    if ((frame >= 119.0f) && (held_prop != NULL))
    {
        objSetDropType(self->weapons_held[gunhand], 3);
        self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
    }

    if (modelGetAnimFrame(self_model) >= modelGetAnimEndFrame(self_model))
    {
        chrStandCheckWall(self);

        return;
    }

    if ((frame >= 87.0f) && (frame <= 110.0f))
    {
        chrSetSubroty(self, 1, 110.0f, chrGetGuard007SpeedRating(self, 1.0f, 1.6f), 0.0f);
    }
}


static void chrTickBondIntro(ChrRecord *self)
{
    Model *self_model;
    f32 frame;

    self_model = self->model;
    frame = modelGetAnimFrame(self_model);

    if ((frame < 86.0f) && (modelGetAnimEndFrame(self_model) <= frame))
    {
        modelSetAnimation(
            self_model,
            ANIM(fire_standing_draw_one_handed_weapon_fast),
            0,
            86.0f,
            modelGetAnimSpeed(self_model),
            24.0f);

        modelSetAnimEndFrame(self_model, 131.0f);

        return;
    }

    if (modelGetAnimEndFrame(self_model) <= frame)
    {
        chrStand(self);
    }
}










static void chrActionTickBody(ChrRecord *self);

/**
 * Runs a chr's AI list and current action.
 *
 * This is also called from chrTick while each player's view is being drawn,
 * so in co-op the current player is put back afterwards.
 */
void chrActionTick(ChrRecord *self)
{
    s32 prevplayer;

    if (!g_CoopMode)
    {
        chrActionTickBody(self);
        return;
    }

    prevplayer = playerGetCurrentNum();
    coopChrSelectTarget(self);
    chrActionTickBody(self);
    playerSetCurrent(prevplayer);
}

static void chrActionTickBody(ChrRecord *self)
{
    if (g_ClockTimer > 0)
    {
        if (self->actiontype == ACT_INIT)
        {
            self->chrflags |= CHRFLAG_INIT;
            chrStandImmediate(self, 0.0f);
            self->sleep = 0;
        }

        if ((self->hidden & CHRHIDDEN_TIMER_ACTIVE) != 0)
        {
            self->timer60 += g_ClockTimer;
        }

        self->sleep -= g_ClockTimer;

        if (((s32)self->sleep < 0) || (self->chrflags & CHRFLAG_00040000))
        {
            self->sleep = 0;
            ai((PropDefHeaderRecord *)self, PROP_TYPE_CHR);

            switch (self->actiontype)
            {
                case ACT_STAND:
                    chrTickStand(self);
                    break;
                case ACT_KNEEL:
                    chrTickKneel(self);
                    break;
                case ACT_ANIM:
                    chrTickAnim(self);
                    break;
                case ACT_DIE:
                    chrTickDie(self);
                    break;
                case ACT_ARGH:
                    chrTickArgh(self);
                    break;
                case ACT_PREARGH:
                    chrTickPreArgh(self);
                    break;
                case ACT_SIDESTEP:
                    chrTickSidestep(self);
                    break;
                case ACT_JUMPOUT:
                    chrTickJumpout(self);
                    break;
                case ACT_DEAD:
                    chrTickDead(self);
                    break;
                case ACT_ATTACK:
                    chrTickAttack(self);
                    break;
                case ACT_ATTACKWALK:
                    chrTickAttackWalk(self);
                    break;
                case ACT_ATTACKROLL:
                    chrTickAttackRoll(self);
                    break;
                case ACT_RUNPOS:
                    chrTickRunPos(self);
                    break;
                case ACT_PATROL:
                    chrTickPatrol(self);
                    break;
                case ACT_GOPOS:
                    chrTickGoPos(self);
                    break;
                case ACT_SURRENDER:
                    chrTickSurrender(self);
                    break;
                case ACT_TEST:
                    chrTickTest(self);
                    break;
                case ACT_SURPRISED:
                    chrTickSurprised(self);
                    break;
                case ACT_STARTALARM:
                    chrTickStartAlarm(self);
                    break;
                case ACT_THROWGRENADE:
                    chrTickThrowGrenade(self);
                    break;
                case ACT_BONDINTRO:
                    chrTickBondIntro(self);
                    break;
                case ACT_BONDDIE:
                    // Bond's model in the death cutscene: nothing to tick
                    break;

                default:
                    break;
            }

            self->chrflags &= ~CHRFLAG_NEAR_MISS;
            self->hidden &= ~(CHRHIDDEN_ALERT_GUARD_RELATED | CHRHIDDEN_BACKGROUND_AI);
            self->chrseeshot = CHR_FREE;
            self->chrseedie  = CHR_FREE;
        }
    }
}


/**
 * Runs the background AI scripts (level scripts with no guard of their own), and updates
 * the count of guards that have recently seen Bond.
*/
void bgChrsTick(void)
{
    s32 i;
    s32 max;
    ChrRecord *guard;

    max = chrsGetNumSlots();

    for (i=0; i<g_NumBgChrs; i++)
    {
        chrActionTick(&g_BgChrs[i]);
    }

    g_SeenBondRecentlyGuardCount = 0;

    for (i=0; i<max; i++)
    {
        guard = &g_ChrSlots[i];

        if (guard->model != NULL)
        {
            if ((guard->lastseetarget60 > 0) && (g_GlobalTimer - guard->lastseetarget60 < CHRLV_SEEN_RECENT_CHECK))
            {
                g_SeenBondRecentlyGuardCount++;
            }
        }
    }
}
