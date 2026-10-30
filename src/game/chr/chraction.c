/**
 * Guard actions: what a guard is physically doing (ACT_STAND, ACT_PATROL, ACT_GOPOS,
 * ACT_ATTACK, ACT_DIE...). Each action has a function that starts it and a tick function
 * called from chrActionTick. Also taking shots, and the background AI chrs (g_BgChrs)
 * that run level scripts not tied to a guard. Moving is in chrnav.c, attacking and
 * firing in chrattack.c.
 * The chrTry* functions are the ones the AI commands call. See docs/subsystems/guards.md.
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



// forward declarations

u32 weaponIsOneHanded            (PropRecord *weaponprop);
void chrStandChooseAnimation                (ChrRecord *self, f32 duration);
f32 chrGetGuard007SpeedRating               (ChrRecord *self, f32 min, f32 max);
s32 chrGetGuard007SpeedRatingInt            (ChrRecord *self, s32 scale);
f32 chrGetGuard007ArghRating                (ChrRecord *self, f32 min, f32 max);
void chrStand            (ChrRecord *self);
void chrStandImmediateCheckWall        (ChrRecord *self, f32 mergetime);
void chrStandCheckWall    (ChrRecord *self);
void chrKneelChooseAnimation                  (ChrRecord *self);
void chrStartAnim            (ChrRecord *self, s32 animID, s32 startframe, s32 endframe, u8 bitfield, s32 interpol_time60);
void chrStartAlarmChooseAnimation             (ChrRecord *self);
void chrThrowGrenade                        (ChrRecord *self, GUNHAND hand, s32 startframe);
void chrSpotChooseAnimation            (ChrRecord *self, f32 mergetime);
void chrActorShuffleFeet                    (ChrRecord *self);
void chrStartSurprised           (ChrRecord *self);
void chrActorLookFlustered                  (ChrRecord *self);
void chrActorThrowWeaponSurrender           (ChrRecord *self);
void chrActorFadeAway                       (ChrRecord *self);
f32 chrGetClearDistInDir              (PropRecord *prop, f32 angle, f32 maxdist, s32 cdtypes, f32 unkHeight, f32 unkA);
f32 chrGetClearDistInDirDefault      (PropRecord *prop, f32 angle, f32 maxdist);
void chrReactToHit                    (ChrRecord *self, coord3d *dir, f32 angle, s32 req_animation_id, ITEM_IDS item);
s32 chrGetHitInterruptFrame       (ChrRecord *self, f32 *result);
void chrAlertOthersOfInjury              (ChrRecord *, s32);
void chrTickSurrender                       (ChrRecord *self);
void chrGetSideDir                    (ChrRecord *self, s32 side, coord3d *dir);
bool chrIsNotDeadOrShot                       (ChrRecord *chr);
void chrTickAnim                            (ChrRecord *self);
void chrTickDead                            (ChrRecord *self);
void chrTickArgh                            (ChrRecord *self);
void chrTickPreArgh                         (ChrRecord *self);
void chrTickSidestep                        (ChrRecord *self);
void chrTickJumpout                         (ChrRecord *self);
void chrTickTest                            (ChrRecord *self);
void chrTickStartAlarm                      (ChrRecord *self);
void chrTickSurprised                       (ChrRecord *self);
void chrTickThrowGrenade                    (ChrRecord *self);
void chrTickBondIntro                       (ChrRecord *self);
void chrTickBondDieRemoved                  (void);
void chrTickStand                           (ChrRecord *self);



// end forward declarations


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


/**
 * Address 0x7F023A94 (VERSION_US).
 * Address 0x7F023D94 (other)
 */
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


void chrStand(ChrRecord *self)
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


void chrSpotChooseAnimation(ChrRecord *self, f32 mergetime)
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
void chrSidestep(ChrRecord *self, GUNHAND side)
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
void chrJumpOut(ChrRecord *self, GUNHAND side)
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


f32 chrGetClearDistInDir(PropRecord *prop, f32 angle, f32 maxdist, s32 cdtypes, f32 unkHeight, f32 unkA)
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
 * @param arg0:
 * @param arg1:
 * @param arg2:
 * @param req_animation_id: Lookup by id property in g_HitReactionTable
 * @param item: argument to weaponGetImpactForce
 */
void chrReactToHit(ChrRecord *self, coord3d *dir, f32 angle, s32 req_animation_id, ITEM_IDS item)
{
    // stack offset in decimal

    s32 flag9c; // 156(sp)
    PropRecord *prop; // 152
    struct Model *model; // 148
    s32 another_flag; // 144
    f32 impact_force; // ?
    s32 animation_something_index; // 136
    s32 flag1; // 132
    ModelAnimation *anim = NULL;
    struct ChrHitReaction *something_ani = NULL; // ?
    f32 fa;
    f32 f_under; // 112(sp)
    f32 f_over; // 108(sp)
    f32 ft;
    struct StruckAnim *struck_ani; // 100
    s32 i;

    flag9c = 1;
    prop = self->prop;
    model = self->model;
    another_flag = 0;
    animation_something_index = 0;

    if ((self->prop->type != PROP_TYPE_VIEWER) || (getPlayerCount() < 2))
    {
        flag1 = (self->actiontype == ACT_ARGH) && (g_GlobalTimer == self->act_argh.lastHit60);

        for (i=0; g_HitReactionTable[i].hitpart != -1; i++)
        {
            if (req_animation_id == g_HitReactionTable[i].hitpart)
            {
                animation_something_index = i;

                break;
            }
        }

        if (self->damage >= self->maxdamage)
        {
            if (((angle < 1.5707964f) || (angle > 4.712389f)) && ((randomGetNext() % (u32)0x14) == 0))
            {
                ft = modelGetRootRotY(model) + M_PI_F;

                fa = ft + 0.17453294f;
                f_under = ft - 0.17453294f;

                if (fa >= M_TAU_F)
                {
                    fa -= M_TAU_F;
                }

                if (f_under >= M_TAU_F)
                {
                    f_under -= M_TAU_F;
                }

                f_over = chrGetClearDistInDirDefault(prop, fa, 150.0f);
                f_under = chrGetClearDistInDirDefault(prop, f_under, 150.0f);

                if ((f_over < 150.0f) && (f_under < 150.0f))
                {
                    ft = f_over - f_under;
                    if ((ft < 10.0f) && (ft > -10.0f))
                    {
                        struck_ani = &death_stagger[randomGetNext() & 1];

                        chrStopFiring(self);
                        self->actiontype = ACT_DIE;
                        self->act_die.notifychrindex = 0;
                        self->act_die.thudframe1 = struck_ani->thudframe1;
                        self->act_die.thudframe2 = struck_ani->thudframe2;
                        self->sleep = 0;
                        self->act_die.timeextra = 0.0f;

                        modelSetAnimationWithMerge(model, struck_ani->struck_anim, struck_ani->flip, 0.0f, struck_ani->speed, 16.0f, flag1 == 0);

                        if (struck_ani->endframe >= 0.0f)
                        {
                            modelSetAnimEndFrame(model, struck_ani->endframe);
                        }

                        // Note: PD sets the chrwidth to 10 when a guard dies slumped against an object or wall
                        self->chrwidth = 10.0f;

                        another_flag = 1;
                    }
                }
            }

            if (another_flag == 0)
            {
                if ((g_HitReactionTable[animation_something_index].deathAnims != NULL) && (g_HitReactionTable[animation_something_index].deathAnimCount > 0))
                {
                    struct StruckAnim *struck_anib; // sp(92)
                    s32 tr;

                    if (0)
                    {
                        // removed
                    }

                    another_flag = 1;

                    tr = (randomGetNext() % (u32)g_HitReactionTable[animation_something_index].deathAnimCount);
                    struck_anib = &g_HitReactionTable[animation_something_index].deathAnims[tr];
                    chrStopFiring(self);

                    self->actiontype = ACT_DIE;
                    self->act_die.notifychrindex = 0;
                    self->act_die.thudframe1 = struck_anib->thudframe1;
                    self->act_die.thudframe2 = struck_anib->thudframe2;
                    self->sleep = 0;
                    self->act_die.timeextra = 0.0f;

                    modelSetAnimationWithMerge(model, struck_anib->struck_anim, struck_anib->flip, 0.0f, struck_anib->speed, 16.0f, flag1 == 0);

                    if (struck_anib->struck_anim == ANIM(death_neck) && ((randomGetNext() % (u32)0x64) != 0))
                    {
                        modelSetAnimEndFrame(model, 241.0f);
                    }
                    else if (struck_anib->endframe >= 0.0f)
                    {
                        modelSetAnimEndFrame(model, struck_anib->endframe);
                    }

                    impact_force = weaponGetImpactForce(item);

                    if ((impact_force <= 0.0f) && ((self->chrflags & CHRFLAG_IMPACT_ALWAYS) != 0))
                    {
                        impact_force = 6.0f;
                    }

                    if ((struck_anib->knockback != 0) && (impact_force > 0.0f))
                    {
                        self->act_die.elapseextra = 0.0f;
                        self->act_die.timeextra = ((impact_force * 90.0f) / 6.0f);
                        self->act_die.extraspeed.f[0] = (dir->f[0] * impact_force);
                        self->act_die.extraspeed.f[1] = (dir->f[1] * impact_force);
                        self->act_die.extraspeed.f[2] = (dir->f[2] * impact_force);
                    }
                }
            }

            chrDropItems(self);
            statsAddKill();

            if (self->chrflags & CHRFLAG_COUNT_DEATH_AS_CIVILIAN)
            {
                statsAddCivilianKill();
            }
        }
        else
        {
            if ((req_animation_id == 7) && (angle > 2.3561945f) && (angle < 3.926991f) && ((u32) (randomGetNext() % (u32)5) < 2U))
            {
                u32 choice = randomGetNext() % (u32)5;
                chrStopFiring(self);
                self->actiontype = ACT_ARGH;
                self->act_argh.notifychrindex = 0;
                self->act_argh.lastHit60 = g_GlobalTimer;
                self->sleep = 0;

                if ((randomGetNext() & 1) != 0)
                {
                    anim = ANIM(hit_butt_long);
                    modelSetAnimationWithMerge(model, anim, randomGetNext() & 1, 10.f, 0.5f, 16.0f, flag1 == 0);

                    if (choice < 2U)
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, 34.0f, 8.0f));
                    }
                    else if (choice < 4U)
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, 71.0f, 8.0f));
                    }
                    else
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, (f32) (((u16*)anim)[2] - 1), 8.0f));
                    }
                }
                else
                {
                    anim = ANIM(hit_butt_short);
                    modelSetAnimationWithMerge(model, anim, randomGetNext() & 1, 0.0f, 0.5f, 16.0f, flag1 == 0);

                    if (choice < 2U)
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, 37.0f, 8.0f));
                    }
                    else if (choice < 4U)
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, 70.0f, 8.0f));
                    }
                    else
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, (f32) (((u16*)anim)[2] - 1), 8.0f));
                    }
                }

                another_flag = 1;
            }

            if (another_flag == 0)
            {
                if ((g_HitReactionTable[animation_something_index].flinchAnims != NULL) && (g_HitReactionTable[animation_something_index].flinchAnimCount > 0))
                {
                    PropRecord *leftprop = chrGetEquippedWeaponProp(self, GUNLEFT);
                    PropRecord *rightprop = chrGetEquippedWeaponProp(self, GUNRIGHT);
                    s32 tr;
                    struct StruckAnim *struck_ani;
                    s32 ff = flag1 == 0;

                    another_flag = 1;
                    something_ani = &g_HitReactionTable[animation_something_index];

                    if (((s32)&g_HitReactionTable[9] == (s32)something_ani) && (leftprop != NULL))
                    {
                        animation_something_index = 10;
                    }
                    else if (((s32)&g_HitReactionTable[12] == (s32)something_ani) && (rightprop != NULL))
                    {
                        animation_something_index = 13;
                    }

                    something_ani = &g_HitReactionTable[animation_something_index];
                    tr = (randomGetNext() % (u32) something_ani->flinchAnimCount);
                    struck_ani = &something_ani->flinchAnims[tr];

                    chrStopFiring(self);

                    self->actiontype = ACT_ARGH;
                    self->act_argh.notifychrindex = 0;
                    self->act_argh.lastHit60 = g_GlobalTimer;
                    self->sleep = 0;

                    modelSetAnimationWithMerge(model, struck_ani->struck_anim, struck_ani->flip, 0.0f, struck_ani->speed, 16.0f, ff);

                    if (struck_ani->endframe >= 0.0f)
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, struck_ani->endframe, 8.0f));
                    }
                    else
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, (f32)((s32)((u16*)struck_ani->struck_anim)[2] - (s32)1), 8.0f));
                    }
                }
            }

            flag9c = 0;
        }

        if (flag9c && another_flag)
        {
            if ((self->weapons_held[GUNRIGHT] != NULL) && ((self->weapons_held[GUNRIGHT]->obj->flags & PROPFLAG_AIUNDROPPABLE) == FALSE))
            {
                objSetDropType(self->weapons_held[GUNRIGHT], 1);
                self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
            }

            if ((self->weapons_held[GUNLEFT] != NULL) && ((self->weapons_held[GUNLEFT]->obj->flags & PROPFLAG_AIUNDROPPABLE) == FALSE))
            {
                objSetDropType(self->weapons_held[GUNLEFT], 1);
                self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
            }
        }
    }
}


/**
 * @param self:
 * @param result: out parameter, will contain result
 * @returns status indicating if result is set
*/
s32 chrGetHitInterruptFrame(ChrRecord *self, f32 *result)
{
    s32 flag;
    f32 out_val;

    flag = 0;

    if (self->actiontype == ACT_ATTACKROLL)
    {
        if (self->act_attackroll.oneHanded != 0)
        {
            if (
                (self->act_attackroll.animfloats == &g_ChrFireAnims[4])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[5])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[6])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[7]))
            {
                out_val = self->act_attackroll.animfloats->turnEndFrame - 8.0f;

                if (self->act_attackroll.animfloats->end_frame < self->act_attackroll.animfloats->turnEndFrame)
                {
                    out_val = self->act_attackroll.animfloats->end_frame;
                }

                if (modelGetAnimFrame(self->model) < out_val)
                {
                    *result = out_val;
                    flag = 1;
                }
            }
        }
        else
        {
            out_val = self->act_attackroll.animfloats->turnEndFrame - 8.0f;
            if (modelGetAnimFrame(self->model) < out_val)
            {
                *result = out_val;
                flag = 1;
            }
        }
    }
    else if (self->actiontype == ACT_PREARGH)
    {
        // typo/mistake, return without setting *result
        flag = 1;
    }

    return flag;
}


void chrPlayHurtSound(ChrRecord *self)
{
    PropRecord *prop;
    bool male;
    ALSoundState *sndstate;
 
    static s32 male_guard_yelp_counter = 0;
    static s32 female_guard_yelp_counter = 0;
 
    prop = self->prop;
    male = 0;

    if (prop->type == PROP_TYPE_VIEWER)
    {
        if (g_playerPointers[playerGetIndexForProp(prop)]->bonddead != FALSE)
        {
            return;
        }
    }

    if (self->prop->type == PROP_TYPE_VIEWER)
    {
        // Co-op player bodies are the chosen character's, as in solo.
        if ((getPlayerCount() == 1) || g_CoopMode)
        {
            if (c_item_entries[self->bodynum].isMale != FALSE)
            {
                male = TRUE;
            }
        }
        else
        {
            if (frontGetMpCharGender(playerGetIndexForProp(self->prop)) != FEMALE)
            {
                male = TRUE;
            }
        }
    }
    else
    {
        if (c_item_entries[self->bodynum].isMale != FALSE)
        {
            male = TRUE;
        }
    }

    if (male)
    {
        s16 male_yelps[] = {
            GET_HIT_MALE0_SFX,  GET_HIT_MALE1_SFX,  GET_HIT_MALE2_SFX,  GET_HIT_MALE3_SFX,  GET_HIT_MALE4_SFX,
            GET_HIT_MALE5_SFX,  GET_HIT_MALE6_SFX,  GET_HIT_MALE7_SFX,  GET_HIT_MALE8_SFX,  GET_HIT_MALE9_SFX,
            GET_HIT_MALE10_SFX, GET_HIT_MALE11_SFX, GET_HIT_MALE12_SFX, GET_HIT_MALE13_SFX, GET_HIT_MALE14_SFX,
            GET_HIT_MALE15_SFX, GET_HIT_MALE16_SFX, GET_HIT_MALE17_SFX, GET_HIT_MALE18_SFX, GET_HIT_MALE19_SFX,
            GET_HIT_MALE20_SFX, GET_HIT_MALE21_SFX, GET_HIT_MALE22_SFX, GET_HIT_MALE23_SFX, GET_HIT_MALE24_SFX
        };
        
        sndstate = sndPlaySfx(g_musicSfxBufferPtr, male_yelps[male_guard_yelp_counter], NULL);
        male_guard_yelp_counter++;

        if (male_guard_yelp_counter >= 25)
        {
            male_guard_yelp_counter = 0;
        }
    }
    else
    {
        s16 female_yelps[] = {
            GET_HIT_GIRL1_SFX,
            GET_HIT_GIRL2_SFX,
            GET_HIT_GIRL3_SFX
        };

        sndstate = sndPlaySfx(g_musicSfxBufferPtr, female_yelps[female_guard_yelp_counter], NULL);
        female_guard_yelp_counter++;

        if (female_guard_yelp_counter >= 3)
        {
            female_guard_yelp_counter = 0;
        }
    }

    sfxSetVolumeAt(sndstate, &self->prop->pos);
}


s16 metal_ricochet_SFX[3] = {HIT_BULLET_METAL_A3_SFX, HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_B_SFX};

/**
 * Counter-op: damage one player's shot does to the other, in the units of
 * playerTakeDamage (a player's full health is 1).
 *
 * The guard player is as tough as the guard they took over: Bond's shot,
 * with its body part and difficulty multipliers already in damage, is
 * measured against that guard's health. The guard player's shot on Bond
 * does what a guard's shot does (see the guard firing code below).
 */
f32 counterOpPlayerDamage(s32 shooter, f32 damage, s32 weaponid)
{
    if (counterOpIsGuardPlayer(shooter))
    {
        return 0.125f * weaponGetDamage(weaponid) * g_AiDamageModifier * file007Damage();
    }

    return damage / counterOpGetGuardHealth();
}


bool chrTakeShot(ChrRecord *self, s32 hitpart, coord3d *vector, s32 weaponid, bool isPlayer)
{
    s32 hattype;
    PropRecord *myprop = self->prop;

    // Counter-op: the guard player's shots pass through other guards.
    if (isPlayer && counterOpIsGuardPlayer(playerGetCurrentNum()) && (myprop->type != PROP_TYPE_VIEWER))
    {
        return FALSE;
    }

    // Handle hat shots.
    if (hitpart == HIT_HAT && self->handle_positiondata_hat)
    {
        hattype = hatGetModel(self->handle_positiondata_hat);

        if (hattype == HATTYPE_MOON) //moon - count as head hit
        {
            hitpart = HIT_HEAD;
        }
        else if (hattype != HATTYPE_HELMATE) //normal hats - knock off
        {
            objSetDropType(self->handle_positiondata_hat, 4); //objSetDropType
            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;           //drop hat
        }
        else //steel helmate - ricochet
        {
            s16 mrs[3];
            bcopy(metal_ricochet_SFX, mrs, sizeof(mrs));
            ALSoundState * p = sndPlaySfx(g_musicSfxBufferPtr, mrs[randomGetNext() % 3U], NULL);
            sfxSetVolumeAt(p, &self->prop->pos);
        }
    }

    // Handle incrementing player shot count
    if (isPlayer && hitpart)
    {
        switch (hitpart)
        {
            case HIT_HEAD:
            {
                statsAddHit(weaponid, SHOT_REGISTER_HEAD);
                break;
            }
            case HIT_GUN:
            {
                statsAddHit(weaponid, SHOT_REGISTER_GUN);
                break;
            }
            case HIT_HAT:
            {
                statsAddHit(weaponid, SHOT_REGISTER_HAT);
                break;
            }
            case HIT_CHEST:
            case HIT_PELVIS:
            {
                statsAddHit(weaponid, SHOT_REGISTER_BODY);
                break;
            }
            default:
            {
                statsAddHit(weaponid, SHOT_REGISTER_LIMB);
                break;
            }
        }
    }

    self->numarghs++;
    self->chrflags |= CHRFLAG_WAS_HIT;

    // If the chr is invincible, make them flinch then we're done
    if (self->chrflags & CHRFLAG_INVINCIBLE)
    {
        chrSetHiddenToRandom(self); //chrFlinchBody
        return FALSE;
    }

    // If chr is dying or already dead then we're done
    if ((self->actiontype != ACT_DIE) && (self->actiontype != ACT_DEAD))
    {
        coord3d vec;
        f32   angle;
        f32   damageToCause;
        s32   playerNum;

        damageToCause = weaponGetDamage(weaponid);

        if (isPlayer && (!isMpGame()))
        {
            damageToCause *= g_AiHealthModifier;
        }

        vec.x = myprop->pos.x - vector->x;
        vec.y = myprop->pos.y - vector->y;
        vec.z = myprop->pos.z - vector->z;
        angle = chrGetAngleToPos(self, &vec);

        if (hitpart == HIT_GENERAL)
        {
            // Halve the damage because it's doubled for torso below
            hitpart = HIT_CHEST;
            damageToCause *= 0.5f;
        }
        else if (hitpart == HIT_GENERALHALF)
        {
            // Likewise, quarter it here so it becomes half below
            hitpart = HIT_CHEST;
            damageToCause *= 0.25f;
        }

        if (weaponid == ITEM_FIST)
        {
            if ((self->actiontype != ACT_STAND) &&
                (self->actiontype != ACT_PATROL) &&
                (self->actiontype != ACT_SURRENDER) &&
                (self->actiontype != ACT_ANIM) &&
                ((self->actiontype != ACT_GOPOS) || self->act_gopos.speedMode))
            {
                // Punching and pistol whipping is less effective from the front
                if ((angle < DegToRad(60.0)) || (angle > DegToRad(300.0)))
                {
                    damageToCause *= 0.125f;
                }
                else if ((angle < DegToRad(180.0 - 60.0)) || (angle > DegToRad(180.0 + 60.0)))
                {
                    damageToCause *= 0.25f;
                }
                else
                {
                    damageToCause *= 0.5f;
                }
            }
        }

        // Apply damage multipliers based on which body parts were hit,
        // and flinch head if shot in the head - PD Only
        if (hitpart == HIT_HEAD)
        {
            damageToCause *= 4.0f;
        }
        else if (hitpart == HIT_CHEST)
        {
            damageToCause *= 2.0f;
        }
        else if (hitpart == HIT_GUN)
        {
            damageToCause = 0.0f;
        }
        else if (hitpart == HIT_HAT)
        {
            damageToCause = 0.0f;
        }

        if ((self->prop->type == PROP_TYPE_VIEWER) && g_CoopMode && (g_CounterOpMode ? !isPlayer : isPlayer))
        {
            // Co-op: no friendly fire. Counter-op: the two players hurt
            // each other, and guards' stray shots don't hurt the guard
            // player.
            if (g_CounterOpMode)
            {
                return FALSE;
            }
        }
        else if (self->prop->type == PROP_TYPE_VIEWER)
        {
            playerNum = playerGetCurrentNum();

            if (g_CounterOpMode)
            {
                damageToCause = counterOpPlayerDamage(playerNum, damageToCause, weaponid);
            }
            else
            {
                damageToCause *= 0.125f;
            }

            playerSetCurrent(playerGetIndexForProp(self->prop));

            if (g_CounterOpMode && counterOpIsGuardPlayer(playerNum))
            {
                // For the end screen: how much of Bond's health and armour
                // this shot took.
                f32 before = g_CurrentPlayer->bondhealth + g_CurrentPlayer->bondarmour;

                playerTakeDamage(damageToCause, vector->x, vector->z, playerNum, 1);
                f32 after = g_CurrentPlayer->bondhealth + g_CurrentPlayer->bondarmour;

                counterOpNoteDamageToBond((before - ((after > 0.0f) ? after : 0.0f)) * 100.0f);
            }
            else
            {
                playerTakeDamage(damageToCause, vector->x, vector->z, playerNum, 1);
            }

            playerSetCurrent(playerNum);
        }
        else
        {
            if (isPlayer)
            {
                coopChrShotByCurrentPlayer(self);
            }

            self->chrflags |= CHRFLAG_WAS_DAMAGED;
                self->damage += damageToCause;

            if (self->damage < 0.0f)
            {
                f32 endframe = -1.0f;
                if (!chrGetHitInterruptFrame(self, &endframe))
                {
                    chrSetHiddenToRandom(self);
                    return FALSE;
                }
            }
        }

        if (hitpart != HIT_HAT)
        {
            // Cancel current animation and prepare for argh
            f32 endframe2 = -1.0f;
            chrPlayHurtSound(self);

            if (chrGetHitInterruptFrame(self, &endframe2)) //chrIsAnimPreventingArgh
            {
                if (endframe2 >= 0.0f)
                {
                    modelSetAnimEndFrame(self->model, endframe2); //modelSetAnimEndFrame
                }

                self->actiontype         = ACT_PREARGH;
                self->act_preargh.pos.x  = vector->x;
                self->act_preargh.pos.y  = vector->y;
                self->act_preargh.pos.z  = vector->z;
                self->act_preargh.angle = angle;
                self->act_preargh.hitpart = hitpart;
                self->act_preargh.weaponid = weaponid;
                self->sleep              = 0;
            }
            else
            {
                chrReactToHit(self, vector, angle, hitpart, weaponid);
            }
        }
    }

    return TRUE;
}


s32 chrExplosionDamage(ChrRecord *self, coord3d *pos, f32 damage, s32 playsound)
{
    Model *self_model; // 84
    PropRecord *self_prop; // 80
    f32 subroty; // 76
    f32 atan; // 72
    f32 norm; // any
    s32 dirindex; // 64
    f32 relangle; // any
    struct explosion_death_animation *deathanim; // 56
    coord3d push; // 44
    s32 t;

    self_model = self->model;
    self_prop = self->prop;

    if ((self->actiontype == ACT_DEAD) || (self->actiontype == ACT_DIE))
    {
        return 0;
    }

    self->chrflags |= CHRFLAG_WAS_HIT;
    if (self->chrflags & CHRFLAG_INVINCIBLE)
    {
        return 0;
    }

    self->numarghs += 1;
    self->damage += damage;
    self->chrflags |= CHRFLAG_WAS_DAMAGED;

    if (self->damage > 0.0f)
    {
        self->damage = self->maxdamage;

        subroty = modelGetRootRotY(self_model);

        atan = mathAtan2(self_prop->pos.f[0] - pos->f[0], self_prop->pos.f[2] - pos->f[2]);

        push.f[0] = self_prop->pos.f[0] - pos->f[0];
        push.f[1] = self_prop->pos.f[1] - pos->f[1];
        push.f[2] = self_prop->pos.f[2] - pos->f[2];

        // avoid divide by zero
        if ((push.f[0] == 0.0f) && (push.f[1] == 0.0f) && (push.f[2] == 0.0f))
        {
            push.f[2] = 1.0f;
        }

        norm = (5.0f * damage) / sqrtf(((push.f[0] * push.f[0]) + (push.f[1] * push.f[1])) + (push.f[2] * push.f[2]));
        relangle = atan - subroty;

        push.f[0] *= norm;
        push.f[1] *= norm;
        push.f[2] *= norm;

        self->fallspeed.f[0] = push.f[0];
        self->fallspeed.f[1] = push.f[1];
        self->fallspeed.f[2] = push.f[2];

        if (atan < subroty)
        {
            relangle += M_TAU_F;
        }

        dirindex = (s32) (((relangle * 8.0f) / M_TAU_F) + 0.5f);

        if (dirindex >= EXPLOSION_ANIMATION_TABLE_LEN)
        {
            dirindex = 0;
        }

        t = (u32)randomGetNext() % (u32) explosion_animation_table[dirindex].count;
        deathanim = &g_ExplosionDeathAnims[
            explosion_animation_table[dirindex].table[t]
            ];

        chrStopFiring(self);

        self->actiontype = ACT_DIE;
        self->act_die.notifychrindex = 0;
        self->act_die.thudframe1 = deathanim->anonymous_5;
        self->sleep = 0;
        self->act_die.thudframe2 = -1.0f;
        self->act_die.timeextra = 0.0f;

        modelSetAnimation(
            self_model,
            (struct ModelAnimation *) ((s32)deathanim->anonymous_0 + (s32)&g_AnimTable->data),
            deathanim->anonymous_1,
            deathanim->anonymous_3,
            deathanim->anonymous_2,
            8.0f);

        if (deathanim->anonymous_6 >= 0.0f)
        {
            modelSetAnimEndFrame(self_model, deathanim->anonymous_6);
        }

        if (playsound != 0)
        {
            chrPlayHurtSound(self);
        }

        chrDropItems(self);
        statsAddKill();

        if (self->chrflags & CHRFLAG_COUNT_DEATH_AS_CIVILIAN)
        {
            statsAddCivilianKill();
        }

        if ((self->weapons_held[GUNRIGHT] != NULL) && ((self->weapons_held[GUNRIGHT]->obj->flags & 0x2000) == 0))
        {
            objSetDropType(self->weapons_held[GUNRIGHT], 1);
            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
        }

        if ((self->weapons_held[GUNLEFT] != NULL) && ((self->weapons_held[GUNLEFT]->obj->flags & 0x2000) == 0))
        {
            objSetDropType(self->weapons_held[GUNLEFT], 1);
            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
        }

        return 1;
    }

    return 0;
}






/**
 * get vector to run on
 * @param self:
 * @param side: If GUNLEFT set result is (dz, -dx), otherwise (-dz, dx).
 * @param arg2: Out parameter. Contains result vector.
*/
void chrNormDistanceToPlayer(ChrRecord *self, GUNHAND side, coord3d *vec)
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
s32 chrIsClearLow(ChrRecord *self, GUNHAND side, f32 distance)
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
void chrGetSideDir(ChrRecord *self, s32 side, coord3d *dir)
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
s32 chrIsClear(ChrRecord *self, GUNHAND side, f32 distance)
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


void chrTickStand(ChrRecord *self)
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


void chrTickKneel(ChrRecord *actor) {
    actor->sleep = 0;
}

// #still chraction.c


void chrTickAnim(ChrRecord *self)
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


void chrTickSurrender(ChrRecord *self)
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


void chrTickDead(ChrRecord *self)
{
    if (self->act_dead.fadeTime >= 0)
    {
        self->act_dead.fadeTime += g_ClockTimer;

        if (self->act_dead.fadeTime >= CHRLV_TICK_DEAD_CHECK)
        {
            self->hidden |= CHRHIDDEN_REMOVE;
        }
        else
        {
            self->fadealpha = (u8) ((s32) ((CHRLV_TICK_DEAD_CHECK - self->act_dead.fadeTime) * 0xFF) / CHRLV_TICK_DEAD_CHECK);
        }

        return;
    }

    self->act_dead.fadeTime = 0;
}


/**
 * @param self:
 * @param flag: shot/die flag. 0 == shot, else die.
*/
void chrAlertOthersOfInjury(ChrRecord *self, s32 flag)
{
    ChrRecord *guard;
    PropRecord *self_prop;
    f32 dx;
    f32 dz;
    f32 dy;
    s32 numguards;
    PropRecord *guard_prop;
    s32 i = 0;
    s32 alert_count = 0;

    numguards = chrsGetNumSlots();

    /*
     * Maybe there's removed code in these if,elseif blocks?
    */
    if (self->actiontype == ACT_ARGH)
    {
        i = self->act_argh.notifychrindex;
    }
    else if (self->actiontype == ACT_DIE)
    {
        i = self->act_die.notifychrindex;
    }

    for (; i < numguards && alert_count < 4; i++)
    {
        guard = &g_ChrSlots[i];

        if (guard->model != NULL)
        {
            guard_prop = guard->prop;
            self_prop = self->prop;
            dx = guard_prop->pos.f[0] - self_prop->pos.f[0];
            dy = guard_prop->pos.f[1] - self_prop->pos.f[1];
            dz = guard_prop->pos.f[2] - self_prop->pos.f[2];

            if (((dx * dx) + (dy * dy) + (dz * dz)) < 4000000.0f)
            {
                alert_count++;

                if (chrCanSeePos(guard, &self_prop->pos, self_prop->stan))
                {
                    if (flag == 0)
                    {
                        guard->chrseeshot = self->chrnum;
                    }
                    else
                    {
                        guard->chrseedie = self->chrnum;
                    }
                }
            }
        }
    }

    if (self->actiontype == ACT_ARGH)
    {
        self->act_argh.notifychrindex = i;
    }
    else if (self->actiontype == ACT_DIE)
    {
        self->act_die.notifychrindex = i;
    }
}


void chrTickDie(ChrRecord *self)
{
    Model *model = self->model;

    ALSoundState * p;

    s16 body_hit_SFX[] = {0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85};

    static s32 thud_index = 0;

    if ((self->act_die.thudframe1 >= 0.0f) && (self->act_die.thudframe1 <= modelGetAnimFrame(model)))
    {
        p = sndPlaySfx(g_musicSfxBufferPtr, body_hit_SFX[thud_index], NULL);

        sfxSetVolumeAt(p, &self->prop->pos);

        thud_index++;
        if (thud_index >= 0xB)
        {
            thud_index = 0;
        }

        self->act_die.thudframe1 = -1.0f;
    }

    if ((self->act_die.thudframe2 >= 0.0f) && (self->act_die.thudframe2 <= modelGetAnimFrame(model)))
    {
        p = sndPlaySfx(g_musicSfxBufferPtr, body_hit_SFX[thud_index], NULL);

        sfxSetVolumeAt(p, &self->prop->pos);

        thud_index++;
        if (thud_index >= 0xB)
        {
            thud_index = 0;
        }

        self->act_die.thudframe2 = -1.0f;
    }

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        if (modelGetAnim(model) == ANIM(death_left_leg))
        {
            modelSetAnimation(
                model,
                ANIM(jump_backwards),
                modelGetAnimFlip(model) == 0,
                50.0f,
                0.3f,
                (ANIM(jump_backwards)->numFrames - 1.0f) - 50.0f);

            modelSetAnimSpeed(model, 0.5f, (ANIM(jump_backwards)->numFrames - 1.0f) - 50.0f);

            return;
        }

        chrActorFadeAway(self);
    }

    chrAlertOthersOfInjury(self, 1);
}


void chrTickArgh(ChrRecord *self)
{
    Model *model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        chrSetTargetToPlayer(self);

        if (modelGetAnim(model) == ANIM(death_left_leg))
        {
            chrStandImmediateCheckWall(self, 26.0f);
        }
        else
        {
            chrStandCheckWall(self);
        }
    }

    chrAlertOthersOfInjury(self, 0);
}


void chrTickPreArgh(ChrRecord *self)
{
    Model *model;
    coord3d dir;

    model = self->model;

    if (modelGetAnimFrame(model) >= modelGetAnimEndFrame(model))
    {
        dir.f[0] = self->act_preargh.pos.f[0];
        dir.f[1] = self->act_preargh.pos.f[1];
        dir.f[2] = self->act_preargh.pos.f[2];
        chrReactToHit(self, &dir, self->act_preargh.angle, self->act_preargh.hitpart, self->act_preargh.weaponid);
    }
}


/**
 * @see chrTickJumpout
 * @see chrTickTest
 * @see chrTickStartAlarm
*/
void chrTickSidestep(ChrRecord *self)
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
void chrTickJumpout(ChrRecord *self)
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
void chrTickTest(ChrRecord *self)
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
void chrTickStartAlarm(ChrRecord *self)
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


void chrTickSurprised(ChrRecord *self)
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




void chrTickThrowGrenade(ChrRecord *self)
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


void chrTickBondIntro(ChrRecord *self)
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


void chrTickBondDieRemoved(void)
{
    // removed.
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
                    chrTickBondDieRemoved();
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
