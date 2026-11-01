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
#include "game/player/headbob.h"
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
#include "game/level/gamemode.h"


static void chrStand(ChrRecord *self);
static void chrSpotChooseAnimation(ChrRecord *self, f32 mergetime);
static void chrSidestep(ChrRecord *self, GUNHAND side);
static void chrJumpOut(ChrRecord *self, GUNHAND side);
static f32 chrGetClearDistInDir(PropRecord *prop, f32 angle, f32 maxdist, s32 cdtypes, f32 top, f32 bottom);
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
    // 2 to 5 seconds at 60 Hz. PAL doesn't scale it (CHRLV_SEEN_RECENT_CHECK + CHRLV_DEFAULT_TIMER would).
    self->act_stand.wallcount = (randomGetNext() % 120u) + 180;

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

    self->sleep = (s8)(s32)fsleep;
    chrStandChooseAnimation(self, mergetime);
}


/**
 * A value from min (slowest guard) to max (fastest), by the guard's speed rating raised
 * by the 007 reaction slider.
 */
f32 chrGetGuard007SpeedRating(ChrRecord *self, f32 min, f32 max)
{
    f32 ret;

    ret = (f32)self->speedrating;
    ret = (file007Reaction() * (100.0f - ret)) + ret;
    return ((ret * (max - min)) / 100.0f) + min;
}


/**
 * scale for the slowest guard down to 0 for the fastest (speed rating and 007 reaction
 * slider as chrGetGuard007SpeedRating).
 */
s32 chrGetGuard007SpeedRatingInt(ChrRecord *self, s32 scale)
{
    s32 ret;

    ret = (s32)self->speedrating;
    ret = (s32)(file007Reaction() * (f32)(100 - ret)) + ret;
    return ((100 - ret) * scale) / 100;
}


/**
 * As chrGetGuard007SpeedRating, by the guard's argh (hit reaction) rating.
 */
f32 chrGetGuard007ArghRating(ChrRecord *self, f32 min, f32 max)
{
    f32 ret;

    ret = (f32)self->arghrating;
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
            modelSetAnimation(self->model, ANIM(fire_kneel_forward_one_handed_weapon_slow), (s32)self->model->flip, 109.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
            modelSetAnimEndFrame(self->model, 140.0f);
        }
        else
        {
            modelSetAnimation(self->model, ANIM(fire_kneel_left_leg), (s32)self->model->flip, 120.0f, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);
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
 * Starts the grenade throw with the given hand: from the start (reaching for the grenade)
 * if startframe is non-zero, otherwise from frame 84.
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
    f32 speed;

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

    speed = chrGetGuard007SpeedRating(self, 0.6f, 0.96000004f); // 0.96000004 is different from 0.96
    modelSetAnimation(self->model, ANIM(spotting_bond), flip, 10.0f, speed, mergetime);
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

            self->sleep = 16;

            if (left != NULL)
            {
                objSetDropType(left, 2);
            }

            if (right != NULL)
            {
                objSetDropType(right, 2);
            }

            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
        }
        else
        {
            modelSetAnimation(self->model, ANIM(surrendering_armed), randomGetNext() & 1, 0.0f, 0.5f, 16.0f);
            modelSetAnimLooping(self->model, 30.0f, 16.0f);

            self->sleep = 16;
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
 * A step to the given side (GUNRIGHT = right), or with some weapons a slide.
 * PD: chrSidestepChooseAnimation (somewhat similar)
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
            flip = left != NULL;
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
}


/**
 * A jump to the given side, firing.
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
}


/**
 * Line-line intersection, where line1 goes through two points and line2 is a point and a direction.
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
 * Where the line through pos in direction dir crosses the last collision edge
 * (stanGetCollisionEdge), as a point on the edge.
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


/**
 * How far the prop can see or move in a direction (an angle), up to maxdist.
 */
static f32 chrGetClearDistInDir(PropRecord *prop, f32 angle, f32 maxdist, s32 cdtypes, f32 top, f32 bottom)
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

    chrSetMoving(chr, FALSE);
    stanResetHits();

    if (stanTestLineUnobstructed(&stan, prop->pos.f[0], prop->pos.f[2], dest_x, dest_z, cdtypes, top, bottom, 0.0f, 1.0f))
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

    chrSetMoving(chr, TRUE);

    return ret;
}


/**
 * How far a guard can move in a direction, up to maxdist.
 */
f32 chrGetClearDistInDirDefault(PropRecord *prop, f32 angle, f32 maxdist)
{
    f32 top;
    f32 bottom;
    f32 width;

    chrGetChrWidthHeight(prop, &width, &top, &bottom);
    return chrGetClearDistInDir(prop, angle, maxdist, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, top, bottom);
}


/**
 * The unit vector at right angles to the line from the guard to the current player:
 * (dz, -dx) for side GUNLEFT, otherwise (-dz, dx).
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
        vec->f[1] = 0;
        vec->f[0] = dz;
        vec->f[2] = -dx;
    }
    else
    {
        vec->f[2] = dx;
        vec->f[0] = -dz;
        vec->f[1] = 0;
    }
}


/**
 * Whether the guard can walk distance units sideways from the player (as chrIsClear,
 * but relative to the player rather than the guard's facing).
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
 * The guard's sideways unit vector: (cos, 0, -sin) of its facing if side is non-zero,
 * otherwise the opposite.
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
 * Whether the guard can walk distance units to one side of its facing.
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


/**
 * Whether the current player is within 45 degrees of straight ahead of the guard or
 * straight behind it, the directions a sidestep or jump to the side gets out of.
 */
static bool chrIsBondInFrontOrBehind(ChrRecord *self)
{
    PropRecord *myprop = self->prop;
    PropRecord *bondprop = currentPlayerGetProp();
    f32 facing = modelGetRootRotY(self->model);
    f32 toBond = mathAtan2(bondprop->pos.x - myprop->pos.x, bondprop->pos.z - myprop->pos.z);
    f32 angle = toBond - facing;

    if (toBond < facing)
    {
        angle += M_TAU_F;
    }

    return (angle < DegToRad(45)) || (angle > DegToRad(360.0 - 45))
        || ((angle > DegToRad(180.0 - 45)) && (angle < DegToRad(180.0 + 45)));
}


bool chrTrySidestep(ChrRecord *self)
{
    GUNHAND side;

    if (chrIsNotDeadOrShot(self) && chrIsBondInFrontOrBehind(self))
    {
        // A random side, or the other one if that's blocked
        side = (randomGetNext() & 1) == 0;

        if (chrIsClear(self, side, 100.0f))
        {
            chrSidestep(self, side);
            return TRUE;
        }

        side = side == 0;

        if (chrIsClear(self, side, 100.0f))
        {
            chrSidestep(self, side);
            return TRUE;
        }
    }

    return FALSE;
}


bool chrTryJumpOut(ChrRecord *self)
{
    GUNHAND side;

    if (chrIsNotDeadOrShot(self) && chrIsBondInFrontOrBehind(self))
    {
        side = (randomGetNext() & 1) == 0;

        if (chrIsClear(self, side, 200.0f))
        {
            chrJumpOut(self, side);
            return TRUE;
        }

        side = side == 0;

        if (chrIsClear(self, side, 200.0f))
        {
            chrJumpOut(self, side);
            return TRUE;
        }
    }

    return FALSE;
}


/**
 * Runs 200 to 400 units to one side (at right angles to the player), or the other side
 * if that's blocked. Not within 3 seconds of the last walk.
 */
bool chrTryRunSideways(ChrRecord *self)
{
    PropRecord *myprop;
    f32 dist;
    coord3d dir;
    coord3d target;

    if (chrIsNotDeadOrShot(self) && ((g_GlobalTimer - self->lastwalk60) >= CHRLV_RECENT_TIME_CHECK))
    {
        myprop = self->prop;
        dist = (RANDOMGETNEXT_F32() * 200.0f) + 200.0f;
        chrNormDistanceToPlayer(self, ((u32)randomGetNext() & 1) == 0, &dir);

        target.x = (dir.x * dist) + myprop->pos.x;
        target.y = myprop->pos.y;
        target.z = (dir.z * dist) + myprop->pos.z;

        if (propCanWalkLine(myprop, &target, &dir))
        {
            chrRunToPos(self, &target);
            return TRUE;
        }

        dir.x = -dir.x;
        dir.z = -dir.z;
        target.x = (dir.x * dist) + myprop->pos.x;
        target.y = myprop->pos.y;
        target.z = (dir.z * dist) + myprop->pos.z;

        if (propCanWalkLine(myprop, &target, &dir))
        {
            chrRunToPos(self, &target);
            return TRUE;
        }
    }

    return FALSE;
}


/**
 * Walks or runs while firing, if armed, the player is at least 10 m away and the last
 * walk was over 3 seconds ago.
 */
static bool chrTryAttackMoving(ChrRecord *self, SPEED speed)
{
    PropRecord *myprop;
    PropRecord *bondprop;

    if (chrIsNotDeadOrShot(self))
    {
        myprop = self->prop;
        bondprop = currentPlayerGetProp();

        if ((chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT) || chrGetEquippedWeaponPropWithCheck(self, GUNLEFT))
            && ((g_GlobalTimer - self->lastwalk60) >= CHRLV_RECENT_TIME_CHECK))
        {
            f32 dx = bondprop->pos.x - myprop->pos.x;
            f32 dy = bondprop->pos.y - myprop->pos.y;
            f32 dz = bondprop->pos.z - myprop->pos.z;

            if ((SQR(dx) + SQR(dy) + SQR(dz)) >= 1000000.0f)
            {
                chrInitActAttackWalk(self, speed);
                return TRUE;
            }
        }
    }

    return FALSE;
}


bool chrTryAttackWalk(ChrRecord *self)
{
    return chrTryAttackMoving(self, SPEED_WALK);
}


bool chrTryAttackRun(ChrRecord *self)
{
    return chrTryAttackMoving(self, SPEED_RUN);
}


bool chrTryAttackRoll(ChrRecord *self)
{
    PropRecord *myprop;
    PropRecord *bondprop;
    coord3d vec;
    GUNHAND side;
    f32 distsq;

    if (chrIsNotDeadOrShot(self))
    {
        myprop   = self->prop;
        bondprop = currentPlayerGetProp();

        if (chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT) || chrGetEquippedWeaponPropWithCheck(self, GUNLEFT))
        {
            vec.x = bondprop->pos.x - myprop->pos.x;
            vec.y = bondprop->pos.y - myprop->pos.y;
            vec.z = bondprop->pos.z - myprop->pos.z;
            distsq = SQR(vec.x) + SQR(vec.y) + SQR(vec.z);

            // The player is at least 2 m away: roll to a random side, or the other one.
            if (SQR(200.0f) <= distsq)
            {
                side = (randomGetNext() & 1) == 0;

                if (chrIsClearLow(self, side, 200))
                {
                    chrInitActAttackRoll(self, side);
                    return TRUE;
                }

                side = side == 0;

                if (chrIsClearLow(self, side, 200))
                {
                    chrInitActAttackRoll(self, side);
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
            self->act_attack.entityid = newtargetid;
            chrAttackSetEndFrame(self);
            return TRUE;
        }
    }

    return FALSE;
}


bool chrTryStop(ChrRecord *self, s32 faceentitytype, s32 faceentityid)
{
    if (chrIsNotDeadOrShot(self))
    {
        if (self->actiontype != ACT_STAND)
        {
            chrStand(self);
        }

        self->act_stand.face_entitytype = faceentitytype;
        self->act_stand.face_entityid = faceentityid;
        self->act_stand.reaim = 0;
        self->act_stand.checkfacingwall = 0;

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


/**
 * Standing: finishes getting up from a kneel, turns to face a target (walking on the
 * spot), and after a while standing facing a wall turns away from it.
 */
static void chrTickStand(ChrRecord *self)
{
    s32 i;
    PropRecord *left;
    PropRecord *right;
    s32 index;
    f32 cleardist[8];
    f32 subroty;
    f32 angle;
    f32 endframe;
    s32 j;
    s32 order[8];

    if (self->sleep > 0)
    {
        return;
    }

    if (self->act_stand.prestand != 0)
    {
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
            endframe = modelGetAnim(self->model)->numFrames - 1;
            self->act_stand.turning = chrSetSubroty(self, self->act_stand.turning, endframe, 1.0f, 0.0f);

            if (self->act_stand.turning != 1)
            {
                chrStandChooseAnimation(self, 8.0f);
                self->act_stand.reaim = 0;

                if (self->act_stand.face_entitytype & TARGET_COMPASS)
                {
                    self->act_stand.face_entitytype = 0;
                }
            }
        }
        else
        {
            angle = chrGetAngleToTarget(self, self->act_stand.face_entitytype, self->act_stand.face_entityid);
            // More than 20 degrees off: turn
            if ((angle > 0.34906587f) && (angle < 5.9341197f))
            {
                left = chrGetEquippedWeaponProp(self, GUNLEFT);
                right = chrGetEquippedWeaponProp(self, GUNRIGHT);

                self->act_stand.reaim = 1;
                self->act_stand.turning = 1;

                if (((left != NULL) && (right != NULL))
                    || ((left == NULL) && (right == NULL))
                    || weaponIsOneHanded(left)
                    || weaponIsOneHanded(right))
                {
                    modelSetAnimation(self->model, ANIM(walking_unarmed), randomGetNext() & 1, 0.0f, 0.5f, 16.0f);
                    modelSetAnimEndFrame(self->model, ANIM(walking_unarmed)->numFrames - 1);
                }
                else if ((right != NULL) || (left != NULL))
                {
                    modelSetAnimation(self->model, ANIM(walking), left != NULL, 0.0f, 0.5f, 16.0f);
                    modelSetAnimEndFrame(self->model, ANIM(walking)->numFrames - 1);
                }
            }
            else if (self->act_stand.face_entitytype & TARGET_COMPASS)
            {
                self->act_stand.face_entitytype = 0;
            }
        }

        self->sleep = 0;

        return;
    }

    self->sleep = ((u32)randomGetNext() % 5U) + 14;

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
            // How far it's clear in 8 directions, cleardist[i] at (i + 1) * 45 degrees
            // round from ahead (cleardist[7] is straight ahead), then sorted nearest
            // first into order[].
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

            // Selection sort
            for (i = 0; i < 7; i++)
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

                chrTryStop(self, TARGET_COMPASS, (s32)((angle * M_U16_MAX_VALUE_F) / M_TAU_F));
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
            sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, SNEEZE_SFX, NULL), &self->prop->pos);
        }

        self->chrflags |= CHRFLAG_02000000;
    }

    if ((self->sleep <= 0) && (self->act_anim.idleOnEnd != 0))
    {
        self->sleep = (randomGetNext() % 5U) + 14;
    }
}


static void chrTickSurrender(ChrRecord *self)
{
    Model *model;

    if (self->sleep <= 0)
    {
        model = self->model;
        self->sleep = 16;

        if ((modelGetAnim(model) == ANIM(surrendering_armed_drop_weapon))
            && (modelGetAnimFrame(model) >= 80.0f))
        {
            coord3d dir = {0};
            f32 t = modelGetRootRotY(model);

            dir.f[0] = -sinf(t);
            dir.f[2] = -cosf(t);

            // Backed up against something: stop stepping back
            if (!propCanWalkInDir(self->prop, &dir, 20.0f))
            {
                modelSetAnimation(self->model, ANIM(surrendering_armed), randomGetNext() & 1, 30.0f, 0.5f, 16.0f);
                modelSetAnimLooping(self->model, 30.0f, 16.0f);
            }
        }
    }
}


static void chrTickSidestep(ChrRecord *self)
{
    Model *model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        chrSetTargetToPlayer(self);
        chrStandImmediateCheckWall(self, 10.0f);
    }
}


static void chrTickJumpout(ChrRecord *self)
{
    Model *model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        chrSetTargetToPlayer(self);
        chrStandCheckWall(self);
    }
}


static void chrTickTest(ChrRecord *self)
{
    Model *model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        chrStand(self);
    }
}


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

    // The grenade is hidden (chrTryThrowGrenade) until the guard reaches for it.
    if ((frame >= 20.0f) && (held_prop != NULL))
    {
        held_prop->obj->runtime_bitflags &= ~RUNTIMEBITFLAG_RESPAWNING;
    }

    // Pulls the pin
    if ((frame >= 61.0f) && (held_prop != NULL))
    {
        held_prop->weapon->timer = CHRLV_DEFAULT_TIMER;
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

    if (!gameIsTwoPlayerMission())
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

        if ((self->sleep < 0) || (self->chrflags & CHRFLAG_00040000))
        {
            self->sleep = 0;
            aiRunList((PropDefHeaderRecord *)self, PROP_TYPE_CHR);

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

    max = chrsGetNumSlots();

    for (i = 0; i < g_NumBgChrs; i++)
    {
        chrActionTick(&g_BgChrs[i]);
    }

    g_SeenBondRecentlyGuardCount = 0;

    for (i = 0; i < max; i++)
    {
        ChrSlotHot *hot = &g_ChrSlotHot[i];

        if (hot->chrnum != CHRSLOT_EMPTY)
        {
            if ((hot->lastseetarget60 > 0) && (g_GlobalTimer - hot->lastseetarget60 < CHRLV_SEEN_RECENT_CHECK))
            {
                g_SeenBondRecentlyGuardCount++;
            }
        }
    }
}
