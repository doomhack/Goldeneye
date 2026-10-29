/**
 * Guard actions: what a guard is physically doing (ACT_STAND, ACT_PATROL, ACT_GOPOS,
 * ACT_ATTACK, ACT_DIE...). Each action has a function that starts it and a tick function
 * called from chrActionTick. Also sight checks, walking round obstacles, firing, taking
 * shots, spawning guards at pads, and the background AI chrs (g_BgChrs) that run level
 * scripts not tied to a guard.
 * The chrTry* functions are the ones the AI commands call. See docs/subsystems/guards.md.
 */
#include <ultra64.h>
#include <bondaicommands.h>
#include <bondgame.h>
#include <bondconstants.h>
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
#include "game/prop/glass.h"
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
#include "game/nav/stan.h"



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
void chrAttackSetEndFrame                 (ChrRecord *self);
f32 chrGetAngleToTarget                 (ChrRecord *self, s32 attack_type, s32 entityid);
f32 chrGetAngleToPos            (ChrRecord *self, coord3d *pos);
f32 chrGetClearDistInDir              (PropRecord *prop, f32 angle, f32 maxdist, s32 cdtypes, f32 unkHeight, f32 unkA);
f32 chrGetClearDistInDirDefault      (PropRecord *prop, f32 angle, f32 maxdist);
void chrReactToHit                    (ChrRecord *self, coord3d *dir, f32 angle, s32 req_animation_id, ITEM_IDS item);
s32 chrGetHitInterruptFrame       (ChrRecord *self, f32 *result);
s32 chrPathIsOffscreen                      (ChrRecord *self, coord3d *pos, StandTile *tile);
f32 chrGetRunAnimSpeed           (ChrRecord *self);
void chrGoPosGetCurWaypointInfo                     (ChrRecord *self, coord3d *target_point, StandTile **target_stan);
s32 chrGoPosCalculateBaseTtl                (ChrRecord *self);
waypoint *waypointFindOnTile           (StandTile* tile);
s32 waypointIsOnTile     (StandTile* tile);
f32 waypointDistSqXz                     (coord3d *pos, waypoint *wp);
waypoint *waypointFindNearPos                (coord3d *pos, StandTile *starttile);
s32 chrPathToPadIsOffscreen                   (ChrRecord *self, PadRecord *pad);
void chrAttackStand                        (ChrRecord *self, s32 attack_type, s32 entityid);
coord3d *chrGetChrOrPresetLocation          (ChrRecord *self, s32 flags, s32 lookup_id, StandTile **stan);
void chrStopFiring                            (ChrRecord *self);
void chrGoPosClearRestartTtl                        (ChrRecord *self);
s32 chrGoToPos                     (ChrRecord *self, coord3d *pos, StandTile *stan, SPEED speed);
void chrGoPosConsiderRestart                   (ChrRecord *self);
void chrGoPosResetMagic         (ChrRecord *self);
void chrActGoposIncCurIndex                 (ChrRecord *self);
void chrGoPosChooseAnimation   (ChrRecord *self);
void chrGoPosChooseAnimationEx                       (ChrRecord *self, s32 speed, s32 isMale);
void chrSetGoposSegDistTotal                (ChrRecord *self, struct waydata *way, coord3d *pos);
void chrAlertOthersOfInjury              (ChrRecord *, s32);
s32 propCanWalkInDir                         (PropRecord *prop, coord3d *dir, f32 dist);
void chrTickSurrender                       (ChrRecord *self);
void chrPatrolChooseAnimation             (ChrRecord *self);
void chrRecordSawTarget                     (ChrRecord *guardData);
s32 chrCheckCanSeeTarget                (ChrRecord *self, coord3d *pos, StandTile *tile);
s32 chrCanSeePos                        (ChrRecord *self, coord3d *pos, StandTile *tile);
s32 chrHasLineOfSightToPlayer            (ChrRecord *self);
s32 propCanWalkLine       (PropRecord *prop, coord3d *dest, coord3d *dir);
void chrSetTargetToPlayer                   (ChrRecord *self);
void coopChrShotByCurrentPlayer               (ChrRecord *self);
s32 chrSawTargetRecently                      (ChrRecord *);
bool chrCheckTargetInSight                    (ChrRecord *self);
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
void chrPlayGunfireSound                        (ChrRecord *self, s32 hand, s32 ratelimit);
s32 chrSetSubroty                           (ChrRecord *self, s32 state, f32 endframe, f32 speed, f32 angleoffset);
s32 chrUpdateAimendsideback                 (ChrRecord *self, struct weapon_firing_animation_table *anim, s32 leftgun, s32 rightgun, f32 inaccuracy);
void chrResetAimend                         (ChrRecord *self);
void chrSetHandFiring                 (ChrRecord *self, s32 hand, s32 firing);
void chrUpdateShotbondsum                   (ChrRecord *self, s32 *inSight, s32 *hitBond, ITEM_IDS item);
f32 chrGetAimUpAngle                         (ChrRecord *self);
void chrFireHand                   (ChrRecord *self, s32 hand);
s32 chrAttackRollToKneel   (ChrRecord *self);
void chrAttackRollResumeAfterShot  (ChrRecord *self);
void chrAttackKneel                        (ChrRecord *self, s32 attack_type, s32 entityid);
void chrTickAttack                          (ChrRecord *self);
void chrTickAttackCommon                    (ChrRecord *);
void chrInitActAttackWalk                   (ChrRecord *chr, s32);
void chrRunToPos                        (ChrRecord *self, coord3d *pos);
void chrTickThrowGrenade                    (ChrRecord *self);
void chrTickBondIntro                       (ChrRecord *self);
void chrTickBondDieRemoved                  (void);
s32 chrApplySpeed                           (ChrRecord *self, coord3d *target, s32 speed, f32 *speedPtr);
void chrTickAttackWalk                      (ChrRecord *self);
void chrTickRunPos                          (ChrRecord *self);
s32 chrCanWalkLine                         (ChrRecord *self, coord3d *point, StandTile *tile, coord3d *dest, StandTile * desttile, s32 cdtypes);
s32 chrCanWalkLineWithin                         (ChrRecord *self, coord3d *point, StandTile *tile, coord3d *dest, f32 halfwidth, s32 cdtypes);
s32 chrCanWalkViaPoint                         (ChrRecord *self, coord3d *mypos, StandTile *mystan, coord3d *viapoint, coord3d *bondpos, StandTile *bondstan, s32 cdtypes);
void orderPointsBySide           (coord3d *a, coord3d *b, coord3d *dir);
s32 chrNavCanSeeNextPos                         (ChrRecord *self, coord3d *from, StandTile *fromtile, coord3d *to, coord3d *edgeStart, coord3d *edgeEnd, f32 overshoot, f32 width, s32 cdtypes);
s32 chrNavCheckForObstacle                         (ChrRecord *self, coord3d *from, StandTile *fromtile, coord3d *to, coord3d *edgeStart, coord3d *edgeEnd, f32 overshoot, f32 width, s32 cdtypes);
void chrGoPosTickMagic                     (ChrRecord *self, struct waydata *way, f32 speed, coord3d *pos, StandTile *tile);
void chrNavTick                          (ChrRecord *, coord3d *, struct waydata *);
void chrTickGoPos                           (ChrRecord *self);
void chrSetNextActPatrolStepPadPos          (ChrRecord *self);
void chrAdvancePatrolStep                   (ChrRecord *self);
void chrTickPatrol                          (ChrRecord *self);
f32 chrGetAngleToPos            (ChrRecord *self, coord3d *pos);
s32 chrResolveId                              (ChrRecord *self, s32 id);
s32 waypointIsWithin90DegreesOfPosAngle                         (waypoint *wp, coord3d *pos, f32 angle);
s32 waypointFindInQuadrant              (coord3d *bondpos, StandTile *stan, f32 rot, u8 quadrant);
s32 chrIsPosOffScreen                         (coord3d *pos, StandTile *tile);
PropRecord *chrSpawnAtCoord(s32 bodynum, s32 headnum, coord3d *pos, StandTile *stan, f32 angle, AIRecord *ailist, s32 spawnflags);
void chrInitActAttack                       (ChrRecord *self, struct anim_group_info ** animgroups, s32 flip, point2d *hasGun, s32 attack_type, s32 entityid, s32 standing);
s32 chrPatrolCalculateStep                  (ChrRecord *self, bool32 *forward, s32 numsteps);
bool chrIsPosClearOfObjectBounds            (coord3d *pos, StandTile *stan);
s32 chrNavTryObstacle                         (ChrRecord *self, coord3d *corner, s32 left, coord3d *othercorner, f32 clearance, s32 checktarget, coord3d *target, struct waydata *way, f32 overshoot, s32 cdtypes, s32 set_copy);
void chrTickStand                           (ChrRecord *self);
PadRecord * chrGetPatrolStepPad             (ChrRecord *self, s32 numsteps);



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
                propobjSetDropped(left, 2);
            }
            if (right != 0)
            {
                propobjSetDropped(right, 2);
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
 *  // run to coord
 * Address 0x7F024CF8 (not EU).
 * Address 0x7F024CE0 (VERSION_EU).
 * PD: chrJumpOutChooseAnimation (has a few things in common)
 */
void chrRunToPos(ChrRecord *self, coord3d *pos)
{
    f32 dx;
    f32 dz;
    f32 sq;
    PropRecord *left;
    PropRecord *right;
    s32 straight;
    s32 flip;

    dx = self->prop->pos.f[0] - pos->f[0];
    dz = self->prop->pos.f[2] - pos->f[2];
    sq = sqrtf((dx * dx) + (dz * dz));

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    straight = 1;

    if (((left != NULL) && (right != NULL)) || ((left == NULL) && (right == NULL)))
    {
        straight = 0;
        flip = randomGetNext() & 1;
    }
    else
    {
        if ((weaponIsOneHanded(left)) || (weaponIsOneHanded(right)))
        {
            straight = 0;
            flip = left != 0;
        }
        else
        {
            flip = left != 0;
        }
    }

    chrStopFiring(self);

    self->actiontype = ACT_RUNPOS;
    self->act_runpos.pos.f[0] = pos->f[0];
    self->act_runpos.pos.f[1] = pos->f[1];
    self->act_runpos.pos.f[2] = pos->f[2];
    self->sleep = 0;
    self->act_runpos.turnspeed = 0;
    self->act_runpos.neardist = 30.0f;

    if (straight)
    {
#ifdef VERSION_EU
        self->act_runpos.eta60 = (s32) (((sq / (g_ChrSpeedRun * 0.5f)) * 50.0f) / 60.0f);
#else
        self->act_runpos.eta60 = (s32) (sq / (g_ChrSpeedRun * 0.5f));
#endif
        modelSetAnimation(self->model, ANIM(running), flip, 0, 0.5f, 16.0f);
    }
    else
    {
#ifdef VERSION_EU
        self->act_runpos.eta60 = (s32) (((sq / (g_ChrSpeedRunOneHanded * 0.5f)) * 50.0f) / 60.0f);
#else
        self->act_runpos.eta60 = (s32) (sq / (g_ChrSpeedRunOneHanded * 0.5f));
#endif
        modelSetAnimation(self->model, ANIM(running_one_handed_weapon), flip, 0, 0.5f, 16.0f);
    }
}


/**
 * Called from chrTryModifyAttack, where action type is ACT_ATTACK.
 */
void chrAttackSetEndFrame(ChrRecord *self)
{
    Model* model = self->model;

    struct weapon_firing_animation_table *f = self->act_attack.animfloats;

    if ((self->act_attack.attacktype & TARGET_AIM_ONLY) != 0)
    {
        if ((f->recoil_start_frame >= 0.0f) && (f->recoil_start_frame < f->shoot_start_frame))
        {
            modelSetAnimEndFrame(model, f->recoil_start_frame);
        }
        else
        {
            modelSetAnimEndFrame(model, f->shoot_start_frame);
        }
    }
    else if (self->act_attack.doRecoil != 0)
    {
        if (f->recoil_start_frame >= 0.0f)
        {
            modelSetAnimEndFrame(model, f->recoil_start_frame);
        }
        else
        {
            modelSetAnimEndFrame(model, f->shoot_start_frame);
        }
    }
    else if (f->recoil_start_frame >= 0.0f)
    {
        modelSetAnimEndFrame(model, f->recoil_start_frame);
    }
    else if (f->end_frame >= 0.0f)
    {
        modelSetAnimEndFrame(model, f->end_frame);
    }
    else
    {
        modelSetAnimEndFrame(model, -1.0f);
    }
}


f32 chrGetAngleToTarget(ChrRecord *self, s32 attack_type, s32 entityid)
{
    f32 ret;
    StandTile *out_unused;

    if ((attack_type & 2) != 0)
    {
        return 0.0f;
    }

    if ((attack_type & 0x10) != 0)
    {
        ret = ((f32) entityid * M_TAU_F) / M_U16_MAX_VALUE_F;

        ret -= modelGetRootRotY(self->model);

        if (ret < 0.0f)
        {
            ret += M_TAU_F;
        }

        return ret;
    }

    return chrGetAngleToPos(self, chrGetChrOrPresetLocation(self, attack_type, entityid, &out_unused));
}


 /**
  * @param self:
  * @param arg1: address of array of firing animations (example: ptr_pistol_firing_animation_groups)
  * @param arg2: flag of some sort related to calculating distance
  * @param arg3: flags
  * @param attack_type:
  * @param arg5: chrGetAngleToTarget arg2
  * @param arg6: set self->act_attack.unk54 to this
  */
void chrInitActAttack(ChrRecord *self, struct anim_group_info **animgroups, s32 flip, point2d *hasGun, s32 attack_type, s32 entityid, s32 standing)
{
    /**
     * Two unused stack variables, I tried to use them with the animation table
     * but couldn't get a match.
    */

    //
    Model *self_model; // 140
    s32 oneBurst;
    s32 next_anim;
    struct weapon_firing_animation_table *panim_float; // 128
    s32 recoil;
    f32 dist;
    s32 anim_index;
    WeaponObjRecord *weapon;
    point2d singleShot; // 96
    point2d isLauncher; // 88
    s32 i;

    self_model = self->model;
    singleShot = (point2d){0};
    isLauncher = (point2d){0};
    self->actiontype = ACT_ATTACK;
    recoil = 1;
    oneBurst = 0;

    dist = chrGetAngleToTarget(self, attack_type, entityid);

    if (flip != 0)
    {
        anim_index = (s32) ((((M_TAU_F - dist) * 32.0f) / M_TAU_F) + 0.5f);
    }
    else
    {
        anim_index = (s32) (((dist * 32.0f) / M_TAU_F) + 0.5f);
    }

    if (anim_index >= 0x20)
    {
        anim_index = 0;
    }

    next_anim = (u32)randomGetNext() % (u32)animgroups[anim_index]->len;

    panim_float = &animgroups[anim_index]->table[next_anim];

    if ((self->chrflags & CHRSTART_FORCENOBLOOD)
        && (panim_float->anim.anim == ANIM(fire_hip)))
    {
        panim_float = &animgroups[anim_index]->table[(next_anim + 1) % animgroups[anim_index]->len];
    }

    for (i=0; i<2; i++)
    {
        if (hasGun->p[i] != 0)
        {
            weapon = chrGetEquippedWeaponProp(self, i)->weapon;

            if (weaponGetFireRate(weapon->weaponnum) < 0)
            {
                singleShot.p[i] = 1;
                if (weapon->weaponnum == ITEM_LASER)
                {
                    recoil = 0;
                }
            }
            else
            {
                recoil = 0;
                oneBurst = 1;
            }

            if ((weapon->weaponnum == ITEM_ROCKETLAUNCH) || (weapon->weaponnum == ITEM_GRENADELAUNCH))
            {
                isLauncher.p[i] = 1;
            }
        }
    }

    self->act_attack.turning = 1;
    self->act_attack.animfloats = panim_float;
    self->act_attack.fired = 0;
    self->act_attack.nextGun = (u32)randomGetNext() & 1U;
    self->act_attack.hasGun[1] = hasGun->p[1];
    self->act_attack.hasGun[0] = hasGun->p[0];
    self->act_attack.singleShot[1] = singleShot.p[1];
    self->act_attack.singleShot[0] = singleShot.p[0];
    self->act_attack.isLauncher[1] = isLauncher.p[1];
    self->act_attack.isLauncher[0] = isLauncher.p[0];
    self->act_attack.doRecoil = recoil;
    self->act_attack.doOneBurst = oneBurst;
    self->act_attack.numWaitTicks = 0;
    self->act_attack.numShots = 0;

    if ((isLauncher.p[1] != 0) || (isLauncher.p[0] != 0))
    {
        if ((isLauncher.p[1] != 0) && (isLauncher.p[0] != 0))
        {
            self->act_attack.maxShots = 2;
        }
        else
        {
            self->act_attack.maxShots = 1;
        }
    }
    else
    {
        if ((attack_type & 0x80) != 0)
        {
            self->act_attack.maxShots = 1;
        }
        else
        {
            self->act_attack.maxShots = (randomGetNext() & 3) + 2;
        }

        if ((hasGun->p[0] != 0) && (hasGun->p[1] != 0))
        {
            self->act_attack.maxShots += (randomGetNext() & 3) + 2;
        }
    }

    self->act_attack.attacktype = attack_type;
    self->act_attack.entityid = entityid;
    self->act_attack.standing = standing;
    self->act_attack.type_of_motion = 0;
    self->act_attack.lastFire60 = 0;
    self->act_attack.attack_time = 0;
    self->sleep = 0;

    modelSetAnimation(
        self_model,
        (struct ModelAnimation *) panim_float->anim.anim,
        flip,
        panim_float->start_frame,
        chrGetGuard007SpeedRating(self, 0.5f, 0.8f),
        16.0f);

    chrAttackSetEndFrame(self);
}


void chrAttackStand(ChrRecord *self, s32 attack_type, s32 entityid)
{
    PropRecord *left;
    PropRecord *right;
    s32 flip;
    struct anim_group_info **animation_pointer;
    point2d sp;
    PropRecord * left2;
    PropRecord * right2;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    sp = (point2d){0};

    if ((left != NULL) && (right != NULL))
    {
        left2 = chrGetEquippedWeaponPropWithCheck(self, GUNLEFT);
        right2 = chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT);

        if ((left2 != NULL) && (right2 != NULL))
        {
            flip = (u32)randomGetNext() & (u32)1;

            if (((u32)randomGetNext() % 3U) == 0)
            {
                animation_pointer = (struct anim_group_info **)ptr_pistol_firing_animation_groups;
                sp.p[GUNLEFT] = flip;
                sp.p[GUNRIGHT] = !flip;
            }
            else
            {
                animation_pointer = (struct anim_group_info **)ptr_doubles_firing_animation_groups;
                sp.p[GUNLEFT] = 1;
                sp.p[GUNRIGHT] = 1;
            }
        }
        else
        {
            flip = right2 == 0;
            animation_pointer = (struct anim_group_info **)ptr_pistol_firing_animation_groups;
            sp.p[GUNLEFT] = flip;
            sp.p[GUNRIGHT] = !flip;
        }
    }
    else
    {
        if ((weaponIsOneHanded(left) != 0) || (weaponIsOneHanded(right) != 0))
        {
            flip = left != 0;
            animation_pointer = (struct anim_group_info **)ptr_pistol_firing_animation_groups;
            sp.p[GUNLEFT] = flip;
            sp.p[GUNRIGHT] = !flip;
        }
        else
        {
            flip = left != 0;
            animation_pointer = (struct anim_group_info **)ptr_rifle_firing_animation_groups;
            sp.p[GUNLEFT] = flip;
            sp.p[GUNRIGHT] = !flip;
        }
    }

    chrInitActAttack(self, animation_pointer, flip, &sp, attack_type, entityid, 1);
}


void chrAttackKneel(ChrRecord *self, s32 attack_type, s32 entityid)
{
    PropRecord *left;
    PropRecord *right;
    s32 flip;
    struct anim_group_info **animation_pointer;
    point2d sp;
    PropRecord * left2;
    PropRecord * right2;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    sp = (point2d){0};

    if ((left != NULL) && (right != NULL))
    {
        left2 = chrGetEquippedWeaponPropWithCheck(self, GUNLEFT);
        right2 = chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT);

        if ((left2 != NULL) && (right2 != NULL))
        {
            flip = (u32)randomGetNext() & (u32)1;

            if (((u32)randomGetNext() % 3U) == 0)
            {
                animation_pointer = (struct anim_group_info **)ptr_crouched_pistol_firing_animation_groups;
                sp.p[GUNLEFT] = flip;
                sp.p[GUNRIGHT] = !flip;
            }
            else
            {
                animation_pointer = (struct anim_group_info **)ptr_crouched_doubles_firing_animation_groups;
                sp.p[GUNLEFT] = 1;
                sp.p[GUNRIGHT] = 1;
            }
        }
        else
        {
            flip = right2 == 0;
            animation_pointer = (struct anim_group_info **)ptr_crouched_pistol_firing_animation_groups;
            sp.p[GUNLEFT] = flip;
            sp.p[GUNRIGHT] = !flip;
        }
    }
    else
    {
        if ((weaponIsOneHanded(left) != 0) || (weaponIsOneHanded(right) != 0))
        {
            flip = left != 0;
            animation_pointer = (struct anim_group_info **)ptr_crouched_pistol_firing_animation_groups;
            sp.p[GUNLEFT] = flip;
            sp.p[GUNRIGHT] = !flip;
        }
        else
        {
            flip = left != 0;
            animation_pointer = (struct anim_group_info **)ptr_crouched_rifle_firing_animation_groups;
            sp.p[GUNLEFT] = flip;
            sp.p[GUNRIGHT] = !flip;
        }
    }

    chrInitActAttack(self, animation_pointer, flip, &sp, attack_type, entityid, 0);
}


/**
 * // run forward shooting
*/
void chrInitActAttackWalk(ChrRecord *chr, s32 run)
{
    struct weapon_firing_animation_table *panim_float; // 132
    s32 i; //
    WeaponObjRecord *weapon;
    s32 flip; // 120
    point2d hasGun; // 112
    point2d singleShot; // 104
    point2d isLauncher; // 96
    PropRecord *left;
    PropRecord *left2;
    PropRecord *right;
    PropRecord *right2;
    s32 variant;

    left = chrGetEquippedWeaponProp(chr, GUNLEFT);
    right = chrGetEquippedWeaponProp(chr, GUNRIGHT);

    hasGun = (point2d){0};
    singleShot = (point2d){0};
    isLauncher = (point2d){0};

    if ((left != NULL) && (right != NULL))
    {
        left2 = chrGetEquippedWeaponPropWithCheck(chr, GUNLEFT);
        right2 = chrGetEquippedWeaponPropWithCheck(chr, GUNRIGHT);

        variant = 0U;

        if ((left2 != NULL) && (right2 != NULL))
        {
            flip = (u32)randomGetNext() & 1U;
            variant = (u32)randomGetNext() % 3U;
        }
        else
        {
            flip = right2 == 0;
        }

        if (variant == 0)
        {
            if (run != 0)
            {
                panim_float = &g_ChrMoveFireAnims[3];
            }
            else
            {
                panim_float = &g_ChrMoveFireAnims[2];
            }

            if (flip)
            {
                hasGun.p[1] = 1;
            }
            else
            {
                hasGun.p[0] = 1;
            }
        }
        else if (variant == 1)
        {
            if (run != 0)
            {
                panim_float = &g_ChrMoveFireAnims[5];
            }
            else
            {
                panim_float = &g_ChrMoveFireAnims[4];
            }

            hasGun.p[1] = 1;
            hasGun.p[0] = 1;
        }
        else
        {
            if (run != 0)
            {
                panim_float = &g_ChrMoveFireAnims[7];
            }
            else
            {
                panim_float = &g_ChrMoveFireAnims[6];
            }

            hasGun.p[1] = 1;
            hasGun.p[0] = 1;
        }
    }
    else if (weaponIsOneHanded(left) || weaponIsOneHanded(right))
    {
        flip = (s32)left != 0;

        if (run)
        {
            panim_float = &g_ChrMoveFireAnims[3];
        }
        else
        {
            panim_float = &g_ChrMoveFireAnims[2];
        }

        if (flip)
        {
            hasGun.p[1] = 1;
        }
        else
        {
            hasGun.p[0] = 1;
        }
    }
    else
    {
        flip = (s32)left != 0;

        if (run)
        {
            panim_float = &g_ChrMoveFireAnims[1];
        }
        else
        {
            panim_float = &g_ChrMoveFireAnims[0];
        }

        if (flip)
        {
            hasGun.p[1] = 1;
        }
        else
        {
            hasGun.p[0] = 1;
        }
    }

    for (i=0; i<2; i++)
    {
        if (hasGun.p[i])
        {
            weapon = chrGetEquippedWeaponProp(chr, i)->weapon;

            if (weaponGetFireRate(weapon->weaponnum) < 0)
            {
                singleShot.p[i] = 1;
            }

            if ((weapon->weaponnum == ITEM_ROCKETLAUNCH) || (weapon->weaponnum == ITEM_GRENADELAUNCH))
            {
                isLauncher.p[i] = 1;
            }
        }
    }

    chr->actiontype = ACT_ATTACKWALK;
    chr->act_attackwalk.clock_timer30 = 0;
    #if defined(REFRESH_PAL)
    chr->act_attackwalk.clock_timer34 = ((u32) randomGetNext() % (u32) (s32) (333.333343506f * g_AiReactionSpeed)) + CHRLV_SEEN_RECENT_CHECK;
    #else
    chr->act_attackwalk.clock_timer34 = ((u32) randomGetNext() % (u32) (s32) (400.0f * g_AiReactionSpeed)) + CHRLV_SEEN_RECENT_CHECK;
    #endif
    chr->act_attackwalk.facedTarget = 0;
    chr->act_attackwalk.animfloats = panim_float;
    chr->act_attackwalk.timer40 = 0;
    chr->act_attackwalk.nextGun = (u32)randomGetNext() & 1U;
    chr->act_attackwalk.hasGun[1] = (s8) hasGun.p[1];
    chr->act_attackwalk.hasGun[0] = (s8) hasGun.p[0];
    chr->act_attackwalk.singleShot[1] = (s8) singleShot.p[1];
    chr->act_attackwalk.singleShot[0] = (s8) singleShot.p[0];
    chr->act_attackwalk.isLauncher[1] = (s8) isLauncher.p[1];
    chr->act_attackwalk.isLauncher[0] = (s8) isLauncher.p[0];
    chr->sleep = 0;
    chr->act_attackwalk.speed = 0.0f;

    modelSetAnimation(chr->model, (struct ModelAnimation *) panim_float->anim.anim, flip, panim_float->start_frame, 0.5f, 16.0f);
}


/**
 * chrRollToSide
*/
void chrInitActAttackRoll(ChrRecord *chr, GUNHAND side)
{
    Model *self_model; // 140
    struct weapon_firing_animation_table *panim_float; // 136
    PropRecord *left; // any
    PropRecord *left_2; // any
    s32 flip; // 124
    s32 oneHanded; // 120
    WeaponObjRecord *weapon;
    PropRecord *right; // any
    point2d hasGun; // 100
    PropRecord *right_2; // any
    s32 oneBurst; // 92
    point2d singleShot; // 84
    point2d isLauncher; // 76
    s8 recoil; // 72
    s32 i; // 68

    self_model = chr->model;
    left = chrGetEquippedWeaponProp(chr, GUNLEFT);
    right = chrGetEquippedWeaponProp(chr, GUNRIGHT);
    oneHanded = 0;
    hasGun = (point2d){0};
    oneBurst = 0;
    singleShot = (point2d){0};
    isLauncher = (point2d){0};
    recoil = 1;

    if ((left != NULL) && (right != NULL))
    {
        left_2 = chrGetEquippedWeaponPropWithCheck(chr, GUNLEFT);
        right_2 = chrGetEquippedWeaponPropWithCheck(chr, GUNRIGHT);

        if ((left_2 != NULL) && (right_2 != NULL))
        {
            flip = (u32)randomGetNext() & 1U;
            oneHanded = 1;

            if (((u32)randomGetNext() % 3U) == 0)
            {
                hasGun.p[1] = flip;
                hasGun.p[0] = flip == 0;
            }
            else
            {
                hasGun.p[1] = 1;
                hasGun.p[0] = 1;
            }
        }
        else
        {
            flip = (s32)right_2 == 0;
            oneHanded = 1;
            hasGun.p[1] = flip;
            hasGun.p[0] = flip == 0;
        }
    }
    else if (weaponIsOneHanded(left) || weaponIsOneHanded(right))
    {
        flip = (s32)left != 0;
        oneHanded = 1;
        hasGun.p[1] = flip;
        hasGun.p[0] = flip == 0;
    }
    else
    {
        flip = (s32)left != 0;
        hasGun.p[1] = flip;
        hasGun.p[0] = flip == 0;
    }

    if (((side != GUNRIGHT) && (flip == 0)) ||
        ((side == GUNRIGHT) && (flip != 0)))
    {
        if ((u32)randomGetNext() & 1U)
        {
            panim_float = &g_ChrFireAnims[0];
        }
        else
        {
            panim_float = &g_ChrFireAnims[2];
        }
    }
    else if ((u32)randomGetNext() & 1U)
    {
        panim_float = &g_ChrFireAnims[1];
    }
    else
    {
        panim_float = &g_ChrFireAnims[3];
    }

    if (oneHanded != 0)
    {
        panim_float += 4;
    }

    for (i=0; i<2; i++)
    {
        if (hasGun.p[i] != 0)
        {
            weapon = chrGetEquippedWeaponProp(chr, i)->weapon;

            if (weaponGetFireRate(weapon->weaponnum) < 0)
            {
                singleShot.p[i] = 1;
                if (weapon->weaponnum == ITEM_LASER)
                {
                    recoil = 0;
                }
            }
            else
            {
                recoil = 0;
                oneBurst = 1;
            }

            if ((weapon->weaponnum == ITEM_ROCKETLAUNCH) || (weapon->weaponnum == ITEM_GRENADELAUNCH))
            {
                isLauncher.p[i] = 1;
            }
        }
    }

    chr->actiontype = ACT_ATTACKROLL;
    chr->act_attackroll.animfloats = panim_float;
    chr->act_attackroll.fired = 0;
    chr->act_attackroll.nextGun = (u32)randomGetNext() & (u32)1;
    chr->act_attackroll.hasGun[1] = hasGun.p[1];
    chr->act_attackroll.hasGun[0] = hasGun.p[0];
    chr->act_attackroll.singleShot[1] = singleShot.p[1];
    chr->act_attackroll.singleShot[0] = singleShot.p[0];
    chr->act_attackroll.isLauncher[1] = isLauncher.p[1];
    chr->act_attackroll.isLauncher[0] = isLauncher.p[0];
    chr->act_attackroll.doRecoil = recoil;
    chr->act_attackroll.doOneBurst = oneBurst;
    chr->act_attackroll.oneHanded = oneHanded;
    chr->act_attackroll.numWaitTicks = 0;
    chr->act_attackroll.numShots = 0;
    chr->act_attackroll.turning = 1;

    if ((isLauncher.p[1] != 0) || (isLauncher.p[0] != 0))
    {
        if ((isLauncher.p[1] != 0) && (isLauncher.p[0] != 0))
        {
            chr->act_attackroll.maxShots = 2;
        }
        else
        {
            chr->act_attackroll.maxShots = (s8) 1U;
        }
    }
    else
    {
        chr->act_attackroll.maxShots = (s32)((u32)randomGetNext() & 3U) + 2;

        if ((hasGun.p[0] != 0) && (hasGun.p[1] != 0))
        {
            chr->act_attackroll.maxShots += (s32)((u32)randomGetNext() & 3U) + 2;
        }
    }

    chr->act_attackroll.unk4c[0] = 1;
    chr->act_attackroll.unk4c[1] = 0;
    chr->act_attackroll.unk54[0] = 1;
    chr->act_attackroll.unk54[1] = 0;
    chr->act_attackroll.lastFire60 = 0;
    chr->act_attackroll.attack_time = 0;
    chr->sleep = 0;

    modelSetAnimation(
        self_model,
        (struct ModelAnimation *) panim_float->anim.anim,
        flip,
        panim_float->start_frame,
        chrGetGuard007SpeedRating(chr, 0.5f, 0.8f),
        16.0f);

    if (oneHanded == 0)
    {
        if (recoil != 0)
        {
            if (panim_float->recoil_end_frame >= 0.0f)
            {
                modelSetAnimEndFrame(self_model, panim_float->recoil_end_frame);
            }
            else
            {
                modelSetAnimEndFrame(self_model, panim_float->shoot_end_frame);
            }
        }
        else if (panim_float->recoil_start_frame >= 0.0f)
        {
            modelSetAnimEndFrame(self_model, panim_float->recoil_start_frame);
        }
        else if (panim_float->end_frame >= 0.0f)
        {
            modelSetAnimEndFrame(self_model, panim_float->end_frame);
        }
    }
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
                propobjSetDropped(self->weapons_held[GUNRIGHT], 1);
                self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
            }

            if ((self->weapons_held[GUNLEFT] != NULL) && ((self->weapons_held[GUNLEFT]->obj->flags & PROPFLAG_AIUNDROPPABLE) == FALSE))
            {
                propobjSetDropped(self->weapons_held[GUNLEFT], 1);
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

    chrobjSndCreatePostEventDefault(sndstate, &self->prop->pos);
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
            propobjSetDropped(self->handle_positiondata_hat, 4); //propobjSetDropped
            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;           //drop hat
        }
        else //steel helmate - ricochet
        {
            s16 mrs[3];
            bcopy(metal_ricochet_SFX, mrs, sizeof(mrs));
            ALSoundState * p = sndPlaySfx(g_musicSfxBufferPtr, mrs[randomGetNext() % 3U], NULL);
            chrobjSndCreatePostEventDefault(p, &self->prop->pos);
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
            propobjSetDropped(self->weapons_held[GUNRIGHT], 1);
            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
        }

        if ((self->weapons_held[GUNLEFT] != NULL) && ((self->weapons_held[GUNLEFT]->obj->flags & 0x2000) == 0))
        {
            propobjSetDropped(self->weapons_held[GUNLEFT], 1);
            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
        }

        return 1;
    }

    return 0;
}


/**
 * Given a stan tile, find the first waypoint on that tile.
 */
waypoint *waypointFindOnTile(StandTile *tile)
{
    waypoint *head;
    waypoint *wp;

    head = g_CurrentSetup.pathwaypoints;

    if (head != NULL) {
        wp = head;
        while (wp->padID >= 0) {
            PadRecord* pad = &g_CurrentSetup.pads[wp->padID];
            if (tile == pad->stan) {
                return wp;
            }
            wp++;
        }
    }

    return NULL;
}


s32 waypointIsOnTile(StandTile* tile)
{
    return waypointFindOnTile(tile) != NULL;
}


/**
 * 100% match, unsure of argument types.
 * Addresss 0x7F027C84.
*/
f32 waypointDistSqXz(coord3d *pos, waypoint *wp)
{
    f32 dz;
    f32 dx;
    PadRecord *pad;

    pad = &g_CurrentSetup.pads[wp->padID];
    dx = pad->pos.f[0] - pos->f[0];
    dz = pad->pos.f[2] - pos->f[2];
    return (dx * dx) + (dz * dz);
}


waypoint *waypointFindNearPos(coord3d *pos, StandTile *starttile)
{
    StandTile *tile = NULL;
    f32 bestdist;
    waypoint *ret = NULL;
    waypoint *wayp = NULL;
    s32 *n = NULL;

    tile = stanFillSearch(starttile, waypointIsOnTile);
    if (tile != NULL)
    {
        ret = waypointFindOnTile(tile);

        if (ret != NULL)
        {
            bestdist = waypointDistSqXz(pos, ret);

            for (n = ret->neighbours; *n >= 0; n++)
            {
                wayp = &g_CurrentSetup.pathwaypoints[*n];

                if (waypointDistSqXz(pos, wayp) < bestdist)
                {
                    ret = wayp;
                }
            }
        }
    }

    return ret;
}


s32 chrPathIsOffscreen(ChrRecord *self, coord3d *pos, StandTile *tile)
{

#define MAX_PATH_ROOMS 0x14

    s32 rooms[MAX_PATH_ROOMS];
    PropRecord *prop;
    s32 tile_something;
    s32 i;

    prop = self->prop;
    tile_something = stanGetRoomsBetween(prop->stan, prop->pos.x, prop->pos.f[2], &tile, pos->f[0], pos->f[2], &rooms[0], MAX_PATH_ROOMS);

    if (tile_something > 0 && tile_something < MAX_PATH_ROOMS)
    {
        for (i=0; i<tile_something; i++)
        {
            if (bgRoomIsVisible(rooms[i]) != 0)
            {
                return 0;
            }
        }
    }
    else
    {
        return 0;
    }

    return 1;
}


s32 chrPathToPadIsOffscreen(ChrRecord *self, PadRecord *pad)
{
    return chrPathIsOffscreen(self, &pad->pos, pad->stan);
}


/**
 * PD: chrGoPosInitMagic
*/
void chrSetGoposSegDistTotal(ChrRecord *self, struct waydata *way, coord3d *pos)
{
    PropRecord *prop;
    f32 dx;
    f32 dz;
    f32 angle;

    prop = self->prop;
    dx = pos->f[0] - prop->pos.f[0];
    dz = pos->f[2] - prop->pos.f[2];

    angle = mathAtan2(dx, dz);

    way->mode = 6;
    way->segdistdone = 0.0f;
    way->segdisttotal = sqrtf((dx * dx) + (dz * dz));

    modelSetRootRotY(self->model, angle);
}


/**
 * @param self:
 * @param target_point: out paramter, will contain target position
 * @param target_stan: out parameter, will contain pointer to target stan
 *
 * PD: chrGoPosGetCurWaypointInfoWithFlags (somewhat similar)
*/
void chrGoPosGetCurWaypointInfo(ChrRecord *self, coord3d *target_point, StandTile **target_stan)
{
    waypoint *waypoint;
    PadRecord *pad;

    waypoint = self->act_gopos.waypoints[self->act_gopos.curindex];

    if (waypoint != 0)
    {
        pad = &g_CurrentSetup.pads[waypoint->padID];

        target_point->f[0] = pad->pos.f[0];
        target_point->f[1] = pad->pos.f[1];
        target_point->f[2] = pad->pos.f[2];

        *target_stan = pad->stan;
    }
    else
    {
        target_point->f[0] = self->act_gopos.targetpos.f[0];
        target_point->f[1] = self->act_gopos.targetpos.f[1];
        target_point->f[2] = self->act_gopos.targetpos.f[2];

        *target_stan = self->act_gopos.target;
    }
}


/**
 * PD: func0f0370a8 (but GE has much more cases)
*/
f32 chrGetRunAnimSpeed(ChrRecord *self)
{
    f32 scale_factor = g_ChrSpeedWalk;

    if (modelGetAnim(self->model) == ANIM(sprinting))
    {
        scale_factor = g_ChrSpeedSprint;
    }
    else if (modelGetAnim(self->model) == ANIM(running))
    {
        scale_factor = g_ChrSpeedRun;
    }
    else if (modelGetAnim(self->model) == ANIM(sprinting_one_handed_weapon))
    {
        scale_factor = g_ChrSpeedSprintOneHanded;
    }
    else if (modelGetAnim(self->model) == ANIM(running_one_handed_weapon))
    {
        scale_factor = g_ChrSpeedRunOneHanded;
    }
    else if (modelGetAnim(self->model) == ANIM(walking_unarmed))
    {
        scale_factor = g_ChrSpeedWalkUnarmed;
    }
    // typo/mistake, `ANIM_DATA_sprinting_one_handed_weapon` is duplicate of above.
    // compiler swaps addition order when reading this from the stack, unlike addresses only seen once (seen once means not saved to stack).
    else if (modelGetAnim(self->model) == ANIM(sprinting_one_handed_weapon))
    {
        scale_factor = g_ChrSpeedSprintOneHanded2;
    }
    else if (modelGetAnim(self->model) == ANIM(running_female))
    {
        scale_factor = g_ChrSpeedRunFemale;
    }
    else if (modelGetAnim(self->model) == ANIM(walking_female))
    {
        scale_factor = g_ChrSpeedWalkFemale;
    }

    return self->model->scale * scale_factor * 9.999999f;
}


s32 chrGoPosCalculateBaseTtl(ChrRecord *self)
{
    f32 dx;
    f32 dz;
    PropRecord *prop;
    coord3d waypos;
    StandTile *waytile; // 28
    f32 animspeed; // 24

    animspeed = modelGetAbsAnimSpeed(self->model);
    chrGoPosGetCurWaypointInfo(self, &waypos, &waytile);
    prop = self->prop;
    dx = waypos.f[0] - prop->pos.f[0];
    dz = waypos.f[2] - prop->pos.f[2];

    if (dx < 0.0f)
    {
        dx = -dx;
    }

    if (dz < 0.0f)
    {
        dz = -dz;
    }

    return (s32) ((dx + dz) / (chrGetRunAnimSpeed(self) * animspeed));
}


void chrGoPosClearRestartTtl(ChrRecord *self)
{
    self->act_gopos.restartTtl = 0;
}


/**
 * Address 0x7F0281FC (US,JP)
 * Address 0x7F028214 (VERSION_EU)
*/
void chrGoPosConsiderRestart(ChrRecord *self)
{
    s32 newttl;
    s32 ttl;
    s32 elapsed;

    if (self->act_gopos.waydata.mode != WAYMODE_MAGIC)
    {
        ttl = self->act_gopos.restartTtl;

        if (ttl == 0)
        {
#ifndef REFRESH_PAL
            newttl = (chrGoPosCalculateBaseTtl(self) * 2) + 300;
#else
            newttl = ((chrGoPosCalculateBaseTtl(self) * 100) + 15000) / 60;
#endif

            if (newttl >= 0x10000)
            {
                newttl = (u16)-1;
            }

            self->act_gopos.restartTtl = (s16)newttl;

            return;
        }

        elapsed = (u16)g_ClockTimer;

        if (elapsed >= ttl)
        {
            chrGoToPos(self, &self->act_gopos.targetpos, self->act_gopos.target, self->act_gopos.speedMode);

            return;
        }

        self->act_gopos.restartTtl = (u16) (ttl - elapsed);
    }
}


/**
 * PD: chrGoPosInitExpensive
*/
void chrGoPosResetMagic(ChrRecord *self)
{
    coord3d waypos;
    StandTile *waytile;

    chrGoPosGetCurWaypointInfo(self, (coord3d *) &waypos, &waytile);

    self->act_gopos.waydata.mode = 0;
    self->act_gopos.waydata.iter = 0;
    self->act_gopos.waydata.gotPos = 0;

    self->act_gopos.waydata.pos.f[0] = waypos.f[0];
    self->act_gopos.waydata.pos.f[1] = waypos.f[1];
    self->act_gopos.waydata.pos.f[2] = waypos.f[2];

    chrGoPosClearRestartTtl(self);
}


/**
 * PD: chrGoPosAdvanceWaypoint
*/
void chrActGoposIncCurIndex(ChrRecord *self)
{
    if (self->act_gopos.curindex < 3)
    {
        self->act_gopos.curindex++;
    }
    else
    {
        waypoint * p = self->act_gopos.waypoints[self->act_gopos.curindex];

        self->act_gopos.curindex = 1;

        waypointFindRoute(p, self->act_gopos.target_path, (waypoint **)&self->act_gopos.waypoints, MAX_CHRWAYPOINTS);
    }

    chrGoPosResetMagic(self);
}


/**
 * Determines which step index the chr will be at given their current index, the
 * number of steps to take and in which direction (forward or back).
 *
 * Returns the step index and populates *forward with true or false depending on
 * whether the chr will be traversing the path in the forward direction at that
 * point.
 */
s32 chrPatrolCalculateStep(ChrRecord *self, bool32 *forward, s32 numsteps)
{
    s32 nextstep = self->act_patrol.nextstep;
    bool isforward = *forward;

    if (numsteps < 0)
    {
        isforward = !isforward;
        numsteps = -numsteps;
    }

    while (numsteps > 0)
    {
        numsteps--;

        if (isforward)
        {
            nextstep++;

            if (self->act_patrol.path->waypoints[nextstep] < 0)
            {
                nextstep -= 2;

                // Reached the end of the list
                if (self->act_patrol.path->isLoop & 1)
                {
                    nextstep = 0;
                }
                else
                {
                    isforward = FALSE;
                }
            }
        }
        else
        {
            nextstep--;

            if (nextstep < 0)
            {
                nextstep = 1;

                // Reached the start of the list
                if (self->act_patrol.path->isLoop & 1)
                {
                    nextstep = self->act_patrol.path->len - 1;
                }
                else
                {
                    isforward = TRUE;
                }
            }
        }
    }

    *forward = isforward;

    return nextstep;
}


/**
 * PD: chrPatrolCalculatePadNum (had some nice finds when searching for "patrol" in "chraction.c" in PD)
*/
PadRecord *chrGetPatrolStepPad(ChrRecord *self, s32 numsteps)
{
    waypoint *wp;
    bool32 forward;
    s32 step;
    s32 *data;

    forward = self->act_patrol.forward;
    step = chrPatrolCalculateStep(self, &forward, numsteps);

    data = &self->act_patrol.path->waypoints[step];

    wp = &g_CurrentSetup.pathwaypoints[*data];

    return &g_CurrentSetup.pads[wp->padID];
}


PadRecord * chrGetNextPatrolStepPad(ChrRecord *self)
{
    return chrGetPatrolStepPad(self, 0);
}


void chrSetNextActPatrolStepPadPos(ChrRecord *self)
{
    PadRecord *pad;

    pad = chrGetNextPatrolStepPad(self);
    self->act_patrol.waydata.mode = 0;
    self->act_patrol.waydata.iter = 0;
    self->act_patrol.waydata.gotPos = 0;
    self->act_patrol.waydata.pos.f[0] = pad->pos.f[0];
    self->act_patrol.waydata.pos.f[1] = pad->pos.f[1];
    self->act_patrol.waydata.pos.f[2] = pad->pos.f[2];
}


void chrAdvancePatrolStep(ChrRecord *self)
{
    self->act_patrol.nextstep = chrPatrolCalculateStep(self, &self->act_patrol.forward, 1);
    chrSetNextActPatrolStepPadPos(self);
}


/**
 * Returns true if pos is not inside the 2D collision footprint of
 * any normal object (PROP_TYPE_OBJ) in the stan's room.
*/
bool chrIsPosClearOfObjectBounds(coord3d *pos, StandTile *stan)
{
    s32 roomids[8];
    s16 *propnum;
    PropRecord *props = (PropRecord *)&g_Props;
    coord2d *polygon;
    s32 numedges;

    roomids[0] = stan->room;
    roomids[1] = -1;

    roomGetProps(roomids);

    for (propnum = g_RoomPropsFound; *propnum >= 0; propnum++)
    {
        PropRecord *prop = &props[*propnum];

        if (prop->type == PROP_TYPE_OBJ)
        {
            propGetCollisionPolygon2d(prop, &polygon, &numedges);

            if (numedges > 0 && polygonContainsPoint(pos, polygon, numedges))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}


/**
 * contrast with @see chrNavTick
*/
void chrGoPosTickMagic(ChrRecord *self, struct waydata *way, f32 speed, coord3d *pos, StandTile *tile)
{
    /**
     * Three unused stack variables.
    */
    PropRecord *self_prop;
    u8 curindex;
    waypoint *pta;
    PadRecord *pad;
    coord3d waypos;
    StandTile *waytile;

    self->invalidmove = 0;
    self->lastmoveok60 = g_GlobalTimer;
    way->segdistdone += speed * modelGetAbsAnimSpeed(self->model) * g_GlobalTimerDelta;

    if (way->segdisttotal <= way->segdistdone)
    {
        chrSetMoving(self, 0);

        if (
            (stanTestVolume(&tile, pos->f[0], pos->f[2], self->chrwidth, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f) < 0)
            && chrIsPosClearOfObjectBounds(pos, tile))
        {
            self_prop = self->prop;
            self_prop->stan = tile;
            self_prop->pos.f[0] = pos->f[0];
            self_prop->pos.f[1] = pos->f[1];
            self_prop->pos.f[2] = pos->f[2];
            self->chrflags |= CHRFLAG_INIT;

            modelSetRootPosition(self->model, pos);
            chrRootMoveCallback(self->model, &self_prop->pos, &self_prop->pos, &self->ground);
            chrDetectRooms(self);

            if (self->actiontype == ACT_PATROL)
            {
                chrAdvancePatrolStep(self);
                chrSetGoposSegDistTotal(self, way, &chrGetNextPatrolStepPad(self)->pos);
            }
            else if (self->actiontype == ACT_GOPOS)
            {
                curindex = self->act_gopos.curindex;

                if (self->act_gopos.waypoints[curindex] == NULL)
                {
                    if (curindex > 0)
                    {
                        pta = self->act_gopos.waypoints[curindex - 1];
                        pad = &g_CurrentSetup.pads[pta->padID];

                        modelSetRootRotY(
                            self->model,
                            mathAtan2(self_prop->pos.f[0] - pad->pos.f[0], self_prop->pos.f[2] - pad->pos.f[2]));
                    }

                    chrStandCheckWall(self);
                }
                else
                {
                    chrActGoposIncCurIndex(self);
                    chrGoPosGetCurWaypointInfo(self, &waypos, &waytile);
                    chrSetGoposSegDistTotal(self, way, &waypos);
                }
            }
        }
        else
        {
            way->segdistdone = way->segdisttotal;

            if (self->actiontype == ACT_PATROL)
            {
                self->act_patrol.lastvisible60 = g_GlobalTimer;
                chrSetNextActPatrolStepPadPos(self);
            }
            else
            {
                self->act_gopos.magicCheck60 = g_GlobalTimer;
                chrGoPosResetMagic(self);
            }
        }

        chrSetMoving(self, 1);
    }
}


/**
 * Gets character segment percent completed.
 * If action type is ACT_PATROL, computes distance between chr and target pad.
 * If action type is ACT_GOPOS, computes distance between chr and target position.
 * Otherwise result is chr->prop position.
 *
 * @param self:
 * @param arg1: Out parameter. Contains result.
 *
 * PD: chrCalculatePosition.
*/
void chrGetMovementTarget(ChrRecord *self, coord3d *target)
{
    PadRecord *pad;
    f32 percent;
    coord3d waypos; // 44
    StandTile *stan;

    if ((self->actiontype == ACT_PATROL) && (self->act_patrol.waydata.mode == WAYMODE_MAGIC))
    {
        pad = chrGetNextPatrolStepPad(self);

        if (self->act_patrol.waydata.segdisttotal <= self->act_patrol.waydata.segdistdone)
        {
            target->f[0] = pad->pos.f[0];
            target->f[1] = pad->pos.f[1];
            target->f[2] = pad->pos.f[2];

            return;
        }

        percent = self->act_patrol.waydata.segdistdone / self->act_patrol.waydata.segdisttotal;

        target->f[0] = self->prop->pos.f[0] + ((pad->pos.f[0] - self->prop->pos.f[0]) * percent);
        target->f[1] = self->prop->pos.f[1] + ((pad->pos.f[1] - self->prop->pos.f[1]) * percent);
        target->f[2] = self->prop->pos.f[2] + ((pad->pos.f[2] - self->prop->pos.f[2]) * percent);

        return;
    }

    if ((self->actiontype == ACT_GOPOS) && (self->act_gopos.waydata.mode == WAYMODE_MAGIC))
    {
        chrGoPosGetCurWaypointInfo(self, &waypos, &stan);

        if (self->act_gopos.waydata.segdisttotal <= self->act_gopos.waydata.segdistdone)
        {
            target->f[0] = waypos.f[0];
            target->f[1] = waypos.f[1];
            target->f[2] = waypos.f[2];

            return;
        }

        percent = self->act_gopos.waydata.segdistdone / self->act_gopos.waydata.segdisttotal;

        target->f[0] = self->prop->pos.f[0] + ((waypos.f[0] - self->prop->pos.f[0]) * percent);
        target->f[1] = self->prop->pos.f[1] + ((waypos.f[1] - self->prop->pos.f[1]) * percent);
        target->f[2] = self->prop->pos.f[2] + ((waypos.f[2] - self->prop->pos.f[2]) * percent);

        return;
    }

    target->f[0] = self->prop->pos.f[0];
    target->f[1] = self->prop->pos.f[1];
    target->f[2] = self->prop->pos.f[2];
}


/**
 * @param self:
 * @param arg1: sprinting animation when 2, running animation when 1, otherwise walking animation
 * @param arg2:
*/
void chrGoPosChooseAnimationEx(ChrRecord *self, s32 speed, s32 isMale)
{
    PropRecord *left;
    PropRecord *right;
    s32 ani_arg;
    s32 flag;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    if (((left != NULL) && (right != NULL)) || ((left == NULL) && (right == NULL)))
    {
        flag = 0;
        ani_arg = randomGetNext() & 1;
    }
    else
    {
        s32 t;
        if (weaponIsOneHanded(left) || weaponIsOneHanded(right))
        {
            t = 0;
            flag = t;
            ani_arg = (s32)left != 0;
        }
        else
        {
            t = 1;
            flag = t;
            ani_arg = (s32)left != 0;
        }
    }

    if (flag != 0)
    {
        if (speed == 2)
        {
            modelSetAnimation(self->model, ANIM(sprinting), ani_arg, 0.0f, 0.5f, 16.0f);
        }
        else if (speed == 1)
        {
            modelSetAnimation(self->model, ANIM(running), ani_arg, 0.0f, 0.5f, 16.0f);
        }
        else
        {
            modelSetAnimation(self->model, ANIM(walking), ani_arg, 0.0f, 0.5f, 16.0f);
        }

        return;
    }

    if (isMale)
    {
        if (speed == 2)
        {
            modelSetAnimation(self->model, ANIM(sprinting_one_handed_weapon), ani_arg, 0.0f, 0.5f, 16.0f);
        }
        else if (speed == 1)
        {
            modelSetAnimation(self->model, ANIM(running_one_handed_weapon), ani_arg, 0.0f, 0.5f, 16.0f);
        }
        else
        {
            modelSetAnimation(self->model, ANIM(walking_unarmed), ani_arg, 0.0f, 0.5f, 16.0f);
        }

        return;
    }

    if (speed == 2)
    {
        modelSetAnimation(self->model, ANIM(sprinting_one_handed_weapon), ani_arg, 0.0f, 0.5f, 16.0f);
    }
    else if (speed == 1)
    {
        modelSetAnimation(self->model, ANIM(running_female), ani_arg, 0.0f, 0.5f, 16.0f);
    }
    else
    {
        modelSetAnimation(self->model, ANIM(walking_female), ani_arg, 0.0f, 0.5f, 16.0f);
    }

    return;
}


void chrGoPosChooseAnimation( ChrRecord *self)
{
    chrGoPosChooseAnimationEx(self, self->act_gopos.speedMode, c_item_entries[self->bodynum].isMale);
}


s32 chrGoToPos(ChrRecord *self, coord3d *pos, StandTile *stan, SPEED speed)
{
    PropRecord *prop;
    waypoint *prop_waypoint;
    waypoint *target_waypoint;
    waypoint *route[MAX_CHRWAYPOINTS];
    s32 i;
    coord3d waypos;
    StandTile *waytile;
    s32 samespeed;

    prop = self->prop;

    samespeed = (self->actiontype == ACT_GOPOS) && (self->act_gopos.speedMode == (u8)speed);

    prop_waypoint = waypointFindNearPos(&prop->pos, prop->stan);
    target_waypoint = waypointFindNearPos(pos, stan);

    if ((prop_waypoint != NULL)
        && (target_waypoint != NULL)
        && !(waypointFindRoute(prop_waypoint, target_waypoint, (waypoint **)&route, MAX_CHRWAYPOINTS) < 2)
    )
    {
        chrStopFiring(self);

        self->actiontype = ACT_GOPOS;

        self->act_gopos.targetpos.f[0] = pos->f[0];
        self->act_gopos.targetpos.f[1] = pos->f[1];
        self->act_gopos.targetpos.f[2] = pos->f[2];
        self->act_gopos.target = stan;
        self->act_gopos.target_path = target_waypoint;
        self->act_gopos.curindex = 0;
        self->act_gopos.speedMode = speed;
        self->act_gopos.speed = 0.0f;
        self->act_gopos.waydata.age = (s32) (randomGetNext() % 100U);
        self->act_gopos.waydata.gotPosCopy = 0;
        self->act_gopos.magicCheck60 = -1;

        for (i=0; i<MAX_CHRWAYPOINTS; i++)
        {
            self->act_gopos.waypoints[i] = route[i];
        }

        chrGoPosResetMagic(self);
        self->sleep = 0;

        if (samespeed == 0)
        {
            chrGoPosChooseAnimation(self);
        }

        chrGoPosGetCurWaypointInfo(self, &waypos, &waytile);

        if (((prop->flags & 2) == 0) && (chrPathIsOffscreen(self, &waypos, waytile) != 0))
        {
            chrSetGoposSegDistTotal(self, &self->act_gopos.waydata, &waypos);
        }

        return 1;
    }

    return 0;
}


void chrPatrolChooseAnimation(ChrRecord *self)
{
    PropRecord *left;
    PropRecord *right;
    s32 ani_arg;
    s32 flag;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    if (((left != NULL) && (right != NULL)) || ((left == NULL) && (right == NULL)))
    {
        flag = 0;
        ani_arg = randomGetNext() & 1;
    }
    else
    {
        s32 t;
        if (weaponIsOneHanded(left) || weaponIsOneHanded(right))
        {
            t = 0;
            flag = t;
            ani_arg = (s32)left != 0;
        }
        else
        {
            t = 1;
            flag = t;
            ani_arg = (s32)left != 0;
        }
    }

    if (flag != 0)
    {
        modelSetAnimation(self->model, ANIM(walking), ani_arg, 0.0f, 0.5f, 16.0f);
    }
    else
    {
        f32 tf = (0.5f * g_ChrSpeedWalk) / g_ChrSpeedWalkUnarmed;
        modelSetAnimation(self->model, ANIM(walking_unarmed), ani_arg, 0.0f, tf, 16.0f);
    }

    return;
}


void chrStartPatrol(ChrRecord *self, PathRecord *path)
{
    PadRecord *pad;
    s32 next_step = -1;
    PropRecord *prop = self->prop;
    s32 count = 0;
    waypoint *pta;
    s32 *dataptr;
    f32 dx;
    f32 dz;

    if (path->waypoints[0] >= 0)
    {
        do
        {
            dataptr = &path->waypoints[count];
            pta = &g_CurrentSetup.pathwaypoints[*dataptr];
            pad = &g_CurrentSetup.pads[pta->padID];

            if ((pad->stan != NULL) && (pad->stan == prop->stan))
            {
                dx = pad->pos.f[0] - prop->pos.f[0];
                dz = pad->pos.f[2] - prop->pos.f[2];

                if (((dx * dx) + (dz * dz)) < 10000.0f)
                {
                    
                    next_step = count;
                    break;
                }
            }

            count++;
        }
        while (path->waypoints[count] >= 0);
    }

    if (next_step < 0)
    {
        next_step = 0;
    }

    chrStopFiring(self);

    self->actiontype = ACT_PATROL;
    self->act_patrol.path = path;
    self->act_patrol.nextstep = next_step;
    self->act_patrol.forward = 1;
    self->act_patrol.waydata.age = randomGetNext() % 0x64U;
    self->act_patrol.waydata.gotPosCopy = 0;
    self->act_patrol.lastvisible60 = -1;
    self->act_patrol.speed = 0.0f;

    chrSetNextActPatrolStepPadPos(self);

    self->sleep = 0;

    chrPatrolChooseAnimation(self);

    pad = chrGetNextPatrolStepPad(self);

    if ((self->prop->flags & PROPFLAG_ONSCREEN) == FALSE)
    {
        if (chrPathToPadIsOffscreen(self, pad) != 0)
        {
            chrSetGoposSegDistTotal(self, &self->act_patrol.waydata, &pad->pos);
        }
    }
}


void chrRecordSawTarget(ChrRecord *self)
{
  self->seen_bond_time = g_GlobalTimer;
  return;
}


s32 chrCheckCanSeeTarget(ChrRecord *self, coord3d *pos, StandTile *tile)
{
    s32 ret;
    s32 flags;
    StandTile *stan;
    StandTile *targettile;
    coord3d *targetpos;

    ret = 0;
    flags = TARGET_BOND;

    if (self->actiontype == ACT_ATTACK)
    {
        flags = self->act_attack.attacktype;
    }

    if ((flags & TARGET_FRONT_OF_CHR) != 0)
    {
        ret = 1;
    }
    else
    {
        stan = tile;
        targetpos = chrGetChrOrPresetLocation(self, flags, self->act_attack.entityid, &targettile);
        chrSetMoving(self, 0);

        if ((flags & 1) != 0)
        {
            playerSetMoving(g_CurrentPlayer->prop, 0);

            if (playerGetVisibleToGuards() != 0)
            {
                if ((stanTestLineUnobstructed(&stan, pos->x, pos->f[2], targetpos->x, targetpos->f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE, pos->f[1], pos->f[1], targetpos->f[1], targetpos->f[1]) != 0) && (stan == targettile))
                {
                    chrRecordSawTarget(self);
                    ret = 1;
                }
            }

            playerSetMoving(g_CurrentPlayer->prop, 1);
        }
        else if ((flags & 4) != 0)
        {
            if ((stanTestLineUnobstructed(&stan, pos->x, pos->f[2], targetpos->x, targetpos->f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE, pos->f[1], pos->f[1], targetpos->f[1], targetpos->f[1]) != 0) && (stan == targettile))
            {
                ret = 1;
            }
        }
        else if ((flags & 8) != 0)
        {
            if ((stanTestLineUnobstructed(&stan, pos->x, pos->f[2], targetpos->x, targetpos->f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE, pos->f[1], pos->f[1], targetpos->f[1], targetpos->f[1]) != 0) && (stan == targettile))
            {
                ret = 1;
            }
        }

        chrSetMoving(self, 1);
    }

    return ret;
}


bool chrCanSeeBond(ChrRecord *self)
{
    bool pass = FALSE;
    PropRecord *myprop;
    PropRecord *bondprop;
    StandTile *mystan;
    f32 myheight;

    if (playerGetVisibleToGuards())
    {
        myprop   = self->prop;
        bondprop = currentPlayerGetProp();
        myheight = self->chrheight - 20.0f;

        chrSetMoving(self, FALSE);
        playerSetMoving(g_CurrentPlayer->prop, 0);

        mystan = myprop->stan;

        if (stanTestLineUnobstructed(&mystan, myprop->pos.x, myprop->pos.z, bondprop->pos.x, bondprop->pos.z, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE, myheight, myheight, 0.0f, 1.0f) && (mystan == bondprop->stan))
        {
            chrRecordSawTarget(self);
            pass = TRUE;
        }

        chrSetMoving(self, TRUE);
        playerSetMoving(g_CurrentPlayer->prop, 1);
    }

    return pass;
}


bool chrHasLineOfSightToPos(ChrRecord *self, coord3d *pos, StandTile *stan)
{
    PropRecord *myprop   = self->prop;
    StandTile  *propstan;
    f32         myheight = self->chrheight - 20.0f;
    bool        pass     = FALSE;

    chrSetMoving(self, 0);

    propstan = myprop->stan;

    if (stanTestLineUnobstructed(&propstan, myprop->pos.x, myprop->pos.z, pos->x, pos->z, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE, myheight, myheight, 0.0f, 1.0f) && (propstan == stan))
    {
        pass = TRUE;
    }

    chrSetMoving(self, 1);

    return pass;
}


s32 chrCanSeePos(ChrRecord *self, coord3d *pos, StandTile *tile)
{
    f32 atan;
    f32 roty;
    f32 df;

    roty = modelGetRootRotY(self->model);
    atan = mathAtan2(pos->f[0] - self->prop->pos.f[0], pos->f[2] - self->prop->pos.f[2]);
    df = atan - roty;

    if (atan < roty)
    {
        df += M_TAU_F;
    }
    // if NOT in rear left quadrant?
    if ((df < DegToRad(100)) || (df > DegToRad(260)))
    {
        return chrHasLineOfSightToPos(self, pos, tile);
    }

    return 0;
}


s32 chrHasLineOfSightToPlayer(ChrRecord *self)
{
    PropRecord *prop;
    PropRecord *bond_prop;
    StandTile *bond_stan;
    s32 ret;

    prop = self->prop;
    bond_prop = currentPlayerGetProp();
    ret = 0;

    playerSetMoving(g_CurrentPlayer->prop, 0);

    bond_stan = bond_prop->stan;

    if ((stanTestLineUnobstructed(
            &bond_stan,
            bond_prop->pos.f[0],
            bond_prop->pos.f[2],
            prop->pos.f[0],
            prop->pos.f[2],
            0x13,
            bond_prop->pos.f[1],
            bond_prop->pos.f[1],
            0.0f,
            1.0f) != 0)
        && (bond_stan == prop->stan))
    {
        ret = 1;
    }

    playerSetMoving(g_CurrentPlayer->prop, 1);

    return ret;
}


s32 propCanWalkLine(PropRecord *prop, coord3d *dest, coord3d *dir)
{
    ChrRecord *chr;
    f32 startX;
    f32 startZ;
    f32 endX;
    f32 endZ;
    StandTile *stan;
    f32 chrx;
    f32 chrz;
    s32 ret;
    f32 height;
    f32 always20;
    f32 width;

    chr = prop->chr;
    chrx = dir->f[0] * chr->chrwidth * 1.2f;
    chrz = dir->f[2] * chr->chrwidth * 1.2f;
    ret = 0;

    chrGetChrWidthHeight(prop, &width, &height, &always20);
    chrSetMoving(chr, 0);

    startX = prop->pos.f[0] + chrz;
    startZ = prop->pos.f[2] - chrx;
    endX = dest->f[0] + chrz + chrx;
    endZ = (dest->f[2] - chrx) + chrz;

    stan = prop->stan;

    if (
        (stanTestLineUnobstructed(&stan, prop->pos.f[0], prop->pos.f[2], startX, startZ, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER , height, always20, 0.0f, 1.0f) != 0)
        && (stanTestLineUnobstructed(&stan, startX, startZ, endX, endZ, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER , height, always20, 0.0f, 1.0f) != 0)
        )
    {
        startX = prop->pos.f[0] - chrz;
        startZ = prop->pos.f[2] + chrx;
        endX = (dest->f[0] - chrz) + chrx;
        endZ = dest->f[2] + chrx + chrz;

        // why is this getting set again?
        stan = prop->stan;

        if (
            (stanTestLineUnobstructed(&stan, prop->pos.f[0], prop->pos.f[2], startX, startZ, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER , height, always20, 0.0f, 1.0f) != 0)
            && (stanTestLineUnobstructed(&stan, startX, startZ, endX, endZ, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER , height, always20, 0.0f, 1.0f) != 0)
            )
        {
            ret = 1;
        }
    }

    chrSetMoving(chr, 1);

    return ret;
}


/**
*/
s32 propCanWalkInDir(PropRecord *prop, coord3d *dir, f32 dist)
{
    coord3d dest;

    dest.f[0] = prop->pos.f[0] + (dir->f[0] * dist);
    dest.f[1] = prop->pos.f[1];
    dest.f[2] = prop->pos.f[2] + (dir->f[2] * dist);

    return propCanWalkLine(prop, &dest, dir);
}


/**
 * Same as chrAlertGuardToPlayerPosition, except without setting `hidden` flag 0x2.
*/
void chrSetTargetToPlayer(ChrRecord *self)
{
    PropRecord *playerprop;

    playerprop = currentPlayerGetProp();
    self->lastseetarget60 = g_GlobalTimer;
    self->lastknowntargetpos.f[0] = playerprop->pos.f[0];
    self->lastknowntargetpos.f[1] = playerprop->pos.f[1];
    self->lastknowntargetpos.f[2] = playerprop->pos.f[2];
    self->targetTile = playerprop->stan;
}


/**
 * See also chrSetTargetToPlayer.
 */
void chrAlertGuardToPlayerPosition(ChrRecord *self)
{
    PropRecord *playerprop;

    playerprop = currentPlayerGetProp();
    self->hidden |= CHRHIDDEN_ALERT_GUARD_RELATED;
    self->lastheartarget60 = g_GlobalTimer;
    self->lastknowntargetpos.f[0] = playerprop->pos.x;
    self->lastknowntargetpos.f[1] = playerprop->pos.y;
    self->lastknowntargetpos.f[2] = playerprop->pos.z;
    self->targetTile = playerprop->stan;
}


bool chrHasStoppedOrPatroling(ChrRecord *self) //chrHasStoppedOrPatroling
{
    if ((self->actiontype == ACT_STAND) && !self->act_stand.prestand && !self->act_stand.reaim)
    {
        return TRUE;
    }
    else if (self->actiontype == ACT_ANIM)
    {
        if (self->act_anim.playSfx ||
            ((modelGetAnimSpeed(self->model) >= 0.0f) && modelGetAnimFrame(self->model) >= modelGetAnimEndFrame(self->model)) ||
            ((modelGetAnimSpeed(self->model)  < 0.0f) && modelGetAnimFrame(self->model) <= 0.0f)
           )
        {
            return TRUE;
        }
    }
    else if (self->actiontype == ACT_PATROL)
    {
        return TRUE;
    }

    return FALSE;
}


bool chrCheckTargetInSight(ChrRecord *self)
{
    PropRecord *myprop;
    PropRecord *bondprop;
    f32         rrr;
    f32         vec2rd;
    f32         myRadDirection;
    coord3d     vec;
    f32         atn;
    f32         radChangeToFaceBond;
    bool        pass;
    s32         distance;

    myprop               = self->prop;
    bondprop             = currentPlayerGetProp();
    myRadDirection       = modelGetRootRotY(self->model);
    //Note: x and z get swapped
    vec.z                = bondprop->pos.x - myprop->pos.x;
    vec.y                = bondprop->pos.y - myprop->pos.y;
    vec.x                = bondprop->pos.z - myprop->pos.z;

    atn = mathAtan2(vec.z, vec.x);

    pass                = FALSE;
    rrr                 = atn - myRadDirection;
    radChangeToFaceBond = rrr;

    if (atn < myRadDirection)
    {
        radChangeToFaceBond = rrr + M_TAU_F;
    }

    if (chrSawTargetRecently(self))
    {
        pass = TRUE;
    }
    else
    {
        vec2rd = SQR(vec.z) + SQR(vec.y) + SQR(vec.x);

        if (
            /*within 220 degrees of forward and within range*/
            (
                (vec2rd < (self->visionrange * self->visionrange * 100.0f * 100.0f)) &&
                ((radChangeToFaceBond < DegToRad(110)) || (radChangeToFaceBond > DegToRad(360 - 110)))
            )
            ||
            /*or within clamped minimum of 200*/
            (
                (vec2rd < SQR(200)) &&
                ((radChangeToFaceBond < DegToRad(110)) || (radChangeToFaceBond > DegToRad(360 - 110)))
            )
        )
        {
            if (vec2rd < fogGetScaledFarFogIntensitySquared())
            {
                distance = (s32)((sqrtf(vec2rd) * 30.0f) / 16000.0f);

                //Not facing bond
                if ((radChangeToFaceBond > DegToRad(45)) && (radChangeToFaceBond < DegToRad(360.0 - 45)))
                {
                    f32 f0 = radChangeToFaceBond;
                    if (radChangeToFaceBond > M_PI_F)
                    {
                        //confine/wrap to half
                        f0 = M_TAU_F - radChangeToFaceBond;
                    }

                    f0 -= DegToRad(45);

                    distance *= (s32)((f0 * 24.0f) / M_TAU_F) + 1;
                }

                distance = chrGetGuard007SpeedRatingInt(self, distance) + 1;
                pass = ((u32)randomGetNext() % (u32)distance) == 0;
            }
        }
    }

    if (pass)
    {
        pass = chrCanSeeBond(self);
    }

    if (pass)
    {
        chrSetTargetToPlayer(self);
    }

    return pass;
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


bool chrGoToPad(ChrRecord *self, s32 padid, SPEED speed)
{
    PadRecord *pad;
    StandTile *stan2;
    coord3d region;
    StandTile *stan;

    if ((padid >= 0) && chrIsNotDeadOrShot(self) && (g_SeenBondRecentlyGuardCount < 10))
    {
        padid = chrResolvePadId(self, padid);
        if (isNotBoundPad(padid))
        {
            pad = &g_CurrentSetup.pads[padid];
        }
        else
        {
            pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padid)];
        }

        stan = pad->stan;
        if (stan)
        {
            //if pad is not vertical
            if (pad->up.y < 0.5f)
            {
                stan2    = stan;
                region.x = (pad->up.x * (self->chrwidth * 1.1f)) + pad->pos.x;
                region.y = (pad->up.y * (self->chrwidth * 1.1f)) + pad->pos.y;
                region.z = (pad->up.z * (self->chrwidth * 1.1f)) + pad->pos.z;

                //if able to reach region surrounding pad?
                if (stanWalkLineNoCallback(&stan2, pad->pos.x, pad->pos.z, region.x, region.z) &&
                    chrGoToPos(self, &region, stan2, speed))
                {
                    return TRUE;
                }
            }
            else if (chrGoToPos(self, &pad->pos, stan, speed))
            {
                return TRUE;
            }
        }
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
            chrobjSndCreatePostEventDefault(sndPlaySfx(g_musicSfxBufferPtr, SNEEZE_SFX, 0), &self->prop->pos);
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

        chrobjSndCreatePostEventDefault(p, &self->prop->pos);

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

        chrobjSndCreatePostEventDefault(p, &self->prop->pos);

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


void chrPlayGunfireSound(ChrRecord *self, s32 hand, s32 ratelimit)
{
    PropRecord *prop;
    WeaponObjRecord *weapon;
    s32 play;
    u8 soundrate;
    s16 soundid;
    ALSoundState **slot;

    prop = chrGetEquippedWeaponProp(self, hand);
    weapon = prop->weapon;
    play = 0;

    soundrate = weaponGetSoundRate(weapon->weaponnum);
    soundid = weaponGetSound(weapon->weaponnum);

    if (ratelimit != 0)
    {
        if (soundrate > 0)
        {
            if (((self->hidden & 0x80) == 0) && (self->nextGunSoundTime[hand] < g_GlobalTimer))
            {
                play = 1;
            }
        }
        else
        {
            play = 1;
        }
    }

    if (play != 0)
    {
        if (self->gunSounds[hand].ptr_SEbuffer1 != NULL)
        {
            if (sndGetPlayingState(self->gunSounds[hand].ptr_SEbuffer1) != 0)
            {
                sndDeactivate(self->gunSounds[hand].ptr_SEbuffer1);
            }
        }

        if (self->gunSounds[hand].ptr_SEbuffer2 != NULL)
        {
            if (sndGetPlayingState(self->gunSounds[hand].ptr_SEbuffer2) != 0)
            {
                sndDeactivate(self->gunSounds[hand].ptr_SEbuffer2);
            }
        }

        if (((u16) soundid) != 0)
        {
            slot = NULL;

            if (self->gunSounds[hand].ptr_SEbuffer1 == NULL)
            {
                slot = (ALSoundState **) (&self->gunSounds[hand].ptr_SEbuffer1);
            }
            else if (self->gunSounds[hand].ptr_SEbuffer2 == NULL)
            {
                slot = (ALSoundState **) (&self->gunSounds[hand].ptr_SEbuffer2);
            }

            if (slot != NULL)
            {
                sndPlaySfx(g_musicSfxBufferPtr, soundid, slot);
                chrobjSndCreatePostEventDefault(*slot, &self->prop->pos);
                self->nextGunSoundTime[hand] = g_GlobalTimer + soundrate;
                self->hidden |= 0x80;
            }
        }
    }
}


f32 chrGetSubrotySideback(ChrRecord *self)
{
    Model *model;
    f32 offset;
    f32 ret;

    model = self->model;
    ret = modelGetRootRotY(model) + self->aimsideback;
    offset = 0.0f;

    if (ret >= M_TAU_F)
    {
        ret = ret - M_TAU_F;
    }
    else if (ret < 0.0f)
    {
        ret = ret + M_TAU_F;
    }

    if ((self->actiontype == ACT_ATTACK) || (self->actiontype == ACT_ATTACKROLL))
    {
        offset = self->act_attack.animfloats->angle_offset;
    }
    else if (self->actiontype == ACT_BONDMULTI)
    {
        if (self->act_bondmulti.animfloats != NULL)
        {
            offset = self->act_bondmulti.animfloats->angle_offset;
        }
    }

    if (offset != 0.0f)
    {
        if (self->model->flip != GUNRIGHT)
        {
            offset = M_TAU_F - offset;
        }

        ret = ret + offset;

        if (ret >= M_TAU_F)
        {
            ret = ret - M_TAU_F;
        }
    }

    return ret;
}


f32 chrGetAimUpAngle(ChrRecord *self)
{
    f32 angle;

    angle = self->aimuprshoulder + self->aimupback;
    if (angle < 0.0f)
    {
        angle = angle + M_TAU_F;
    }

    return angle;
}


s32 chrSetSubroty(ChrRecord *self, s32 state, f32 endframe, f32 speed, f32 angleoffset)
{
    Model *model;
    f32 frame;
    f32 dist;
    f32 roty;
    f32 step;

    if (state != 2)
    {
        model = self->model;
        frame = modelGetAnimFrame(model);
        roty = modelGetRootRotY(model);

#if defined(BUGFIX_R1)
        step = 0.06283186f * speed * g_GlobalTimerDeltaRaw * model->playspeed;
#else
        step = 0.06283186f * speed * g_GlobalTimerDelta * model->playspeed;
#endif

        if (self->actiontype == ACT_ATTACK)
        {
            dist = chrGetAngleToTarget(self, self->act_attack.attacktype, self->act_attack.entityid);
        }
        else if (self->actiontype == ACT_STAND)
        {
            dist = chrGetAngleToTarget(self, self->act_stand.face_entitytype, self->act_stand.face_entityid);
        }
        else
        {
            PropRecord* p;
            p = currentPlayerGetProp();
            dist = chrGetAngleToPos(self, &p->pos);
        }

        dist = dist - angleoffset;

        if (dist < 0.0f)
        {
            dist = dist + M_TAU_F;
        }

        if ((dist < step) || ((M_TAU_F - step) < dist))
        {
            roty += dist;
            if (roty >= M_TAU_F)
            {
                roty -= M_TAU_F;
            }

            modelSetRootRotY(model, roty);
            state = 3;
        }
        else if (dist < M_PI_F)
        {
            roty += step;
            if (roty >= M_TAU_F)
            {
                roty -= M_TAU_F;
            }

            modelSetRootRotY(model, roty);
        }
        else
        {
            roty -= step;

            if (roty < 0.0f)
            {
                roty += M_TAU_F;
            }

            modelSetRootRotY(model, roty);
        }

        if (endframe <= frame)
        {
            state = 2;
        }
    }

    return state;
}


/**
 * @param self:
 * @param arg1:
 * @param arg2:
 * @param arg3:
 * @param arg4:
*/
s32 chrUpdateAimendsideback(ChrRecord *self, struct weapon_firing_animation_table *anim, s32 leftgun, s32 rightgun, f32 inaccuracy)
{
    f32 aimup;
    f32 calc_aimendsideback;
    u32 attack_type;
    s32 entity_id;
    s32 ret;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dxdydz_square;
    PropRecord *self_prop;
    s32 seen_bond_flag;
    coord3d *current_player_pos;
    f32 ducking_height;
    StandTile *pstan;
    coord3d rootpos;
    PropRecord *player_prop;
    f32 subroty;

    /////////////////////

    ret = 1;
    aimup = 0.0f;
    attack_type = TARGET_BOND;
    entity_id = 0;
    calc_aimendsideback = 0.0f;

    if (self->actiontype == ACT_ATTACK)
    {
        attack_type = self->act_attack.attacktype;
        entity_id = self->act_attack.entityid;
    }
    else if (self->actiontype == ACT_STAND)
    {
        attack_type = self->act_stand.face_entitytype;
        entity_id = self->act_stand.face_entityid;
    }

    if ((attack_type & TARGET_FRONT_OF_CHR) == 0)
    {
        player_prop = currentPlayerGetProp();
        self_prop = self->prop;
        current_player_pos = &player_prop->pos;

        dx = player_prop->pos.f[0] - self_prop->pos.f[0];
        dy = player_prop->pos.f[1] - self_prop->pos.f[1];
        dz = player_prop->pos.f[2] - self_prop->pos.f[2];

        dxdydz_square = (dx * dx) + (dy * dy) + (dz * dz);

        if (attack_type & TARGET_BOND)
        {
            if ((attack_type & TARGET_DONTTURN) != 0)
            {
                seen_bond_flag = 1;
            }
            else
            {
                seen_bond_flag = chrCanSeeBond(self);
            }
        }
        else
        {
            seen_bond_flag = 1;
        }

        if (attack_type & TARGET_BOND)
        {
            ducking_height = playerGetEyeHeight(g_CurrentPlayer);
            if ((self->chrflags & CHRSTART_FORCENOBLOOD) != 0)
            {
                if (((dx * dx) + (dy * dy) + (dz * dz)) < 160000.0f)
                {
                    if (self_prop->pos.f[1] < (current_player_pos->f[1] - (2.0f * ducking_height)))
                    {
                        dy -= ducking_height * (0.55f + (0.1f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX)) * inaccuracy));
                    }
                    else if ((current_player_pos->f[1] - (ducking_height * 0.5f)) < self_prop->pos.f[1])
                    {
                        dy -= ducking_height * (0.15f + (0.1f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX)) * inaccuracy));
                    }
                    else
                    {
                        dy = (((f32) (u32)randomGetNext() * (1.0f / UINT_MAX) * 0.1f * inaccuracy) + 1.0f) * 40.0f;
                    }
                }
                else
                {
                    dy += ducking_height * (0.025f - (0.05f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX)) * inaccuracy));
                }
            }
            else if (((dx * dx) + (dy * dy) + (dz * dz)) > 1000000.0f)
            {
                if (((u32)randomGetNext() % 3U) == 0)
                {
                    dy += ducking_height * (0.05f + (0.1f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX)) * inaccuracy));
                }
                else
                {
                    dy -= ducking_height * (0.05f + (0.55f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX)) * inaccuracy));
                }
            }
            else
            {
                if (self_prop->pos.f[1] < (current_player_pos->f[1] - ducking_height))
                {
                    dy -= ducking_height * (0.55f + (0.1f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX)) * inaccuracy));
                }
                else if ((current_player_pos->f[1] - (ducking_height * 0.5f)) < self_prop->pos.f[1])
                {
                    dy -= ducking_height * (0.15f + (0.1f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX)) * inaccuracy));
                }
                else
                {
                    dy = (((f32) (u32)randomGetNext() * (1.0f / UINT_MAX) * 0.1f * inaccuracy) - 0.05f) * ducking_height;
                }
            }
        }
        else
        {
            modelGetRootPosition(self->model, &rootpos);
            current_player_pos = chrGetChrOrPresetLocation(self, attack_type, entity_id, &pstan);
            dx = current_player_pos->f[0] - rootpos.f[0];
            dy = current_player_pos->f[1] - rootpos.f[1];
            dz = current_player_pos->f[2] - rootpos.f[2];
        }

        if ((attack_type & 0x100) == 0)
        {
            f32 sr = sqrtf((dx * dx) + (dz * dz));
            aimup = mathAtan2(dy, sr);

            if (aimup >= M_PI_F)
            {
                aimup = aimup - M_TAU_F;
            }
        }

        if (seen_bond_flag)
        {
            Model *weapon_prop_model;
            coord3d gunpos;
            PropRecord *weapon_prop;
            ModelRwData_HeaderRecord *rootrw;
            Mtxf gunmtx;
            f32 *muzzle;
            coord3d muzzlepos;
            s32 intersect_flag;
            Mtxf handmtx;
            coord3d closest;
            coord3d facing;
            coord3d side; // handmtx
            Mtxf *nodemtx;
            struct ObjectRecord *obj;
            f32 t1;

            ////////////////////////////////////////////

            subroty = chrGetSubrotySideback(self);

            if (rightgun)
            {
                weapon_prop = chrGetEquippedWeaponProp(self, GUNRIGHT);
            }
            else
            {
                weapon_prop = chrGetEquippedWeaponProp(self, GUNLEFT);
            }

            // This if block is a slight modification of @see chrGetHandPos.
            if ((weapon_prop != NULL) && (weapon_prop->flags & 2) && (dxdydz_square < 1000000.0f))
            {
                obj = weapon_prop->obj;
                weapon_prop_model = obj->model;
                intersect_flag = 0;

                if (weapon_prop_model->obj->Switches[0])
                {
                    nodemtx = modelFindNodeMtx(weapon_prop_model, weapon_prop_model->obj->Switches[0], 0);
                    muzzle = weapon_prop_model->obj->Switches[0]->Data->Gunfire.Offset.f;
                    mtxL2F(nodemtx, &gunmtx);

                    mtx4MultAffineInPlace(camGetPrevViewToWorldMtxf(), &gunmtx);

                    muzzlepos.f[0] = muzzle[0];
                    muzzlepos.f[1] = muzzle[1];
                    muzzlepos.f[2] = muzzle[2];

                    mtx4TransformVecInPlace(&gunmtx, &muzzlepos);

                    gunpos.f[0] = muzzlepos.f[0];
                    gunpos.f[1] = muzzlepos.f[1];
                    gunpos.f[2] = muzzlepos.f[2];

                    intersect_flag = 1;
                }
                else if (weapon_prop_model->obj->Switches[1])
                {
                    nodemtx = modelFindNodeMtx(weapon_prop_model, weapon_prop_model->obj->Switches[1], 0);
                    mtxL2F(nodemtx, &handmtx);
                    mtx4MultAffineInPlace(camGetPrevViewToWorldMtxf(), &handmtx);
                    gunpos.f[0] = handmtx.m[3][0];
                    gunpos.f[1] = handmtx.m[3][1];
                    gunpos.f[2] = handmtx.m[3][2];

                    intersect_flag = 1;
                }

                if (intersect_flag != 0)
                {
                    facing.f[0] = sinf(subroty);
                    facing.f[1] = 0.0f;
                    facing.f[2] = cosf(subroty);
                    side.f[0] = self_prop->pos.f[0] - dz;
                    side.f[1] = self_prop->pos.f[1];
                    side.f[2] = self_prop->pos.f[2] + dx;
                    chrLineLineIntersection(&self_prop->pos, &side, &gunpos, &facing, &closest);
                    dx = current_player_pos->f[0] - closest.f[0];
                    dz = current_player_pos->f[2] - closest.f[2];
                }
            }

            t1 = mathAtan2(dx, dz);

            calc_aimendsideback = t1 - subroty;
            if (t1 < subroty)
            {
                calc_aimendsideback = t1 - subroty + M_TAU_F;
            }

            rootrw = &modelGetNodeRwData(self->model, self->model->obj->RootNode)->Header;

            if (rootrw->turnFrames > 0.0f)
            {
                calc_aimendsideback = calc_aimendsideback - (rootrw->turnFrames * rootrw->turnPerFrame);

                if (calc_aimendsideback < 0.0f)
                {
                    calc_aimendsideback = calc_aimendsideback + M_TAU_F;
                }

                if (calc_aimendsideback >= M_TAU_F)
                {
                    calc_aimendsideback = calc_aimendsideback - M_TAU_F;
                }
            }

            if ((attack_type & 1) && ((attack_type & 0x60) == 0))
            {
                t1 = (((f32) ((s32) ((s32) ((f32) g_GlobalTimer * self->model->playspeed) + self->chrnum) % 60) * M_TAU_F) / 60.0f);
                t1 = sinf(t1) * (chrGetAimLimitAngle(dxdydz_square) * 0.5f);
                calc_aimendsideback += t1;

                if (calc_aimendsideback < 0.0f)
                {
                    calc_aimendsideback = calc_aimendsideback + M_TAU_F;
                }

                if (calc_aimendsideback >= M_TAU_F)
                {
                    calc_aimendsideback = calc_aimendsideback - M_TAU_F;
                }
            }

            if (calc_aimendsideback >= M_PI_F)
            {
                calc_aimendsideback = calc_aimendsideback - M_TAU_F;
            }

            calc_aimendsideback += self->aimsideback;

            if (self->model->flip != GUNRIGHT)
            {
                if (calc_aimendsideback < -anim->max_left)
                {
                    calc_aimendsideback = -anim->max_left;
                    ret = 0;
                }
                else if (-anim->max_right < calc_aimendsideback)
                {
                    calc_aimendsideback = -anim->max_right;
                    ret = 0;
                }
            }
            else
            {
                if (anim->max_left < calc_aimendsideback)
                {
                    calc_aimendsideback = anim->max_left;
                    ret = 0;
                }
                else if (calc_aimendsideback < anim->max_right)
                {
                    calc_aimendsideback = anim->max_right;
                    ret = 0;
                }
            }
        }
    }

    chrUpdateAimendbackShoulders(self, anim, leftgun, rightgun, aimup);
    self->aimendsideback = calc_aimendsideback;
    self->aimendcount = 0xA;

    return ret;
}


/**
 * Calculates and sets chr aimendrshoulder, aimendlshoulder, and aimendback.
 * rshoulder defaults to 0.0f, lshoulder defaults to @param next.
 *
 * @param self:
 * @param arg1: todo/fixme/hack: unsure of arg1 type.
 * @param same: When set, both shoulders will receive lshoulder value. Only
 *     applies with @param swap is set.
 * @param swap: When set, aimendrshoulder will get the calculated lshoulder value,
 *     and aimendlshoulder will get the rshoulder value. If both @param swap and
 *     @param same is set they will both be set to lshoulder value.
 * @param next: Starting aimendlshoulder value.
*/
void chrUpdateAimendbackShoulders(ChrRecord *self, struct weapon_firing_animation_table *anim, s32 leftgun, s32 rightgun, f32 next)
{
    f32 next_lshoulder;
    f32 next_rshoulder;
    f32 next_aimendback;

    next_rshoulder = 0.0f;
    next_aimendback = 0.0f;
    next_lshoulder = next;

    if (anim != NULL)
    {
        if (anim->max_up < next)
        {
            next_aimendback = next - anim->max_up;
            next_lshoulder = anim->max_up;
        }

        else if (next < anim->max_down)
        {
            next_aimendback = next - anim->max_down;
            next_lshoulder = anim->max_down;
        }

        if (next_lshoulder > 0.0f)
        {
            next_rshoulder = anim->free_arm_frac_up * next_lshoulder;
        }
        else
        {
            next_rshoulder = anim->free_arm_frac_down * next_lshoulder;
        }
    }

    if (rightgun != 0)
    {
        self->aimendrshoulder = next_lshoulder;

        if (leftgun != 0)
        {
            self->aimendlshoulder = next_lshoulder;
        }
        else
        {
            self->aimendlshoulder = next_rshoulder;
        }
    }
    else
    {
        self->aimendrshoulder = next_rshoulder;
        self->aimendlshoulder = next_lshoulder;
    }

    self->aimendback = next_aimendback;
}


void chrResetAimend(ChrRecord *self)
{
    self->aimendcount = 0xA;
    self->aimendrshoulder = 0.0f;
    self->aimendlshoulder = 0.0f;
    self->aimendback = 0.0f;
    self->aimendsideback = 0.0f;
}


/**
 * PD: chrSetFiring
*/
void chrSetFiring(ChrRecord *self, s32 hand, s32 firing)
{
    PropRecord *prop;

    prop = chrGetEquippedWeaponProp(self, hand);

    if (prop != NULL)
    {
        weaponSetGunfireVisible(prop, firing);
    }
}


/**
 * PD: chrStopFiring
*/
void chrStopFiring(ChrRecord *self)
{
    chrSetFiring(self, GUNRIGHT, FALSE);
    chrSetFiring(self, GUNLEFT, FALSE);
    chrResetAimend(self);
}


void chrSetHandFiring(ChrRecord *self, s32 hand, s32 firing)
{
    if (firing != 0)
    {
        if (hand == GUNLEFT)
        {
            self->hidden |= CHRHIDDEN_FIRE_WEAPON_LEFT;
        }
        else
        {
            self->hidden |= CHRHIDDEN_FIRE_WEAPON_RIGHT;
        }
    }
    else if (hand == GUNLEFT)
    {
        self->hidden &= ~CHRHIDDEN_FIRE_WEAPON_LEFT;
    }
    else
    {
        self->hidden &= ~CHRHIDDEN_FIRE_WEAPON_RIGHT;
    }

    if (firing == 0)
    {
        chrSetFiring(self, hand, FALSE);
    }
}


f32 chrGetAimLimitAngle(f32 sqdist)
{
    if (sqdist > (1600.0f * 1600.0f))
    {
        return (M_PI_F / 167.5f);
    }

    if (sqdist > (800.0f * 800.0f))
    {
        return (M_PI_F / 83.5f);
    }

    if (sqdist > (400.0f * 400.0f))
    {
        return (M_PI_F / 42.0f);
    }

    if (sqdist > (200.0f * 200.0f))
    {
        return (M_PI_F / 21.0f);
    }

    return (M_PI_F / 12.5f);
}


/**
 * @param arg0:
 * @param arg1: out parameter. bool. Whether or not self has correct line of site to hit player.
 * @param arg2: out parameter. bool. True if damage done, false otherwise.
 * @param item: weapon doing damage
*/
void chrUpdateShotbondsum(ChrRecord *self, s32 *inSight, s32 *hitBond, ITEM_IDS item)
{
    f32 limit_angle;
    f32 dxdydz_square;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 atan;
    f32 subroty;
    f32 relangle;
    PropRecord *player_prop;
    f32 dist;
    PropRecord *self_prop;
    f32 t2;
    f32 increment;
    s32 insight;

    player_prop = currentPlayerGetProp();
    self_prop = self->prop;

    dx = player_prop->pos.f[0] - self_prop->pos.f[0];
    dy = player_prop->pos.f[1] - self_prop->pos.f[1];
    dz = player_prop->pos.f[2] - self_prop->pos.f[2];

    atan = mathAtan2(dx, dz);
    subroty = chrGetSubrotySideback(self);
    relangle = atan - subroty;
    dxdydz_square = (dx * dx) + (dy * dy) + (dz * dz);

    limit_angle = chrGetAimLimitAngle(dxdydz_square);

    if (relangle < 0.0f)
    {
        relangle += M_TAU_F;
    }

    insight = (relangle < limit_angle);

    if ((relangle < limit_angle) == 0)
    {
        insight = ((M_TAU_F - limit_angle) < relangle);
    }

    *inSight = insight;
    *hitBond = 0;

    if ((playerIsShowingDamage() == 0) && (insight != 0))
    {
        dist = sqrtf(dxdydz_square);

        increment = 0.16f * g_GlobalTimerDelta;

        if (dist > 300.0f)
        {
            increment *= (300.0f / dist);
        }

        if ((s32) self->accuracyrating > 0)
        {
            increment *= (1.0f + ((f32) self->accuracyrating / 10.0f));
        }
        else if ((s32) self->accuracyrating < 0)
        {
            if ((s32) self->accuracyrating < -0x63)
            {
                increment = 0.0f;
            }
            else
            {
                increment *= ((f32) (self->accuracyrating + 0x64) / 100.0f);
            }
        }

        if (file007Accuracy() <= 1.0f)
        {
            increment *= file007Accuracy();
        }
        else
        {
            increment *= (9.0f / (10.001f - file007Accuracy()));
        }

        increment *= g_AiAccuracyModifier;

        if (weaponGetFireRate(item) <= 0)
        {
            increment *= 2.0f;
        }

        if ((item == ITEM_SHOTGUN) || (item == ITEM_AUTOSHOT))
        {
            increment *= 2.0f;
        }

        self->shotbondsum += increment;

        if (self->shotbondsum >= 1.0f)
        {
            t2 = (0.125f * weaponGetDamage(item) * g_AiDamageModifier) * file007Damage();

            if ((item == ITEM_SHOTGUN) || (item == ITEM_AUTOSHOT))
            {
                t2 *= 3.0f;
            }

            playerTakeDamageFromAngle(t2, subroty, -1, 1);

            self->shotbondsum = 0.0f;

            if (playerIsShowingDamage() != 0)
            {
                *hitBond = 1;
            }
        }
    }
}


/**
 * Slight modification of a part of @see chrUpdateAimendsideback.
*/
s32 chrGetHandPos(ChrRecord *self, GUNHAND hand, coord3d *pos)
{
    struct ObjectRecord *obj;
    PropRecord *weapon_prop;
    Model *weapon_prop_model;
    s32 ret;
    Mtxf *nodemtx;
    Mtxf worldmtx;
    f32 *muzzle;
    Mtxf *nodemtx2;
    Mtxf worldmtx2;

    weapon_prop = chrGetEquippedWeaponProp(self, hand);
    ret = 0;

    if ((weapon_prop != NULL) )
    {
        obj = weapon_prop->obj;
        weapon_prop_model = obj->model;

        if ((weapon_prop->flags & 2))
        {
            if (weapon_prop_model->obj->Switches[0])
            {
                nodemtx = modelFindNodeMtx(weapon_prop_model, weapon_prop_model->obj->Switches[0], 0);
                muzzle = weapon_prop_model->obj->Switches[0]->Data->Gunfire.Offset.f;

                pos->f[0] = muzzle[0];
                pos->f[1] = muzzle[1];
                pos->f[2] = muzzle[2];

                mtx4MultAffine(camGetViewToWorldMtxf(), nodemtx, &worldmtx);
                mtx4TransformVecInPlace(&worldmtx, pos);

                ret = 1;
            }
            else if (weapon_prop_model->obj->Switches[1])
            {
                nodemtx2 = modelFindNodeMtx(weapon_prop_model, weapon_prop_model->obj->Switches[1], 0);
                mtx4MultAffine(camGetViewToWorldMtxf(), nodemtx2, &worldmtx2);

                pos->f[0] = worldmtx2.m[3][0];
                pos->f[1] = worldmtx2.m[3][1];
                pos->f[2] = worldmtx2.m[3][2];

                ret = 1;
            }
        }
    }

    return ret;
}


void chrFireHand(ChrRecord *self, s32 hand)
{
    PropRecord *self_prop; // 644
    s32 ratelimit; // ?
    s32 firing; // stack 636
    s32 betweenShots;
    WeaponObjRecord *weapon;
    PropRecord *player_prop; // 624
    s32 attacktype; // ?
    s32 shoot; // 616
    s32 tracer; // 612
    coord3d hitpos; // 600
    StandTile *hittile; // 596
    f32 subroty; // 592
    f32 aimup; // 588
    coord3d gunpos; // 576
    StandTile *self_stan; // 572
    StandTile *guntile; // 568
    s32 inSight; // 564
    s32 hitBond; // 560
    s32 showHit; // 556
    coord3d shotdir;
    s32 canSeeTarget;
    f32 dy;
    f32 dz;
    f32 dx;
    f32 shotDistSq; // 524
    struct WeaponObjRecord *rocket;
    Mtxf rocketMtx;
    coord3d rocketSpeed;  // 444
    PropRecord *weapon_prop;
    coord3d rocketAccel; // 428
    Mtxf rocketRot;
    Mtxf rocketYRot;
    struct WeaponObjRecord *grenade; // 296
    Mtxf grenadeMtx;
    coord3d grenadeSpeed; // 220
    Mtxf grenadeRot;
    Mtxf grenadeYRot; // 92
    s32 needsSight;

    self_prop = self->prop;
    weapon_prop = chrGetEquippedWeaponProp(self, hand);

    if (weapon_prop != NULL)
    {
        firing = 0;
        betweenShots = 0;
        weapon = weapon_prop->weapon;
        player_prop = currentPlayerGetProp();
        attacktype = 1;

        if (self->actiontype == ACT_ATTACK)
        {
            attacktype = self->act_attack.attacktype;
        }

        needsSight = attacktype & 1;

        if (
            (needsSight == 0)
            || (self->seen_bond_time >= (g_GlobalTimer - CHRLV_SEEN_RECENT_CHECK))
            || (weaponGetFireRate(weapon->weaponnum) < 0))
        {
            shoot = 0;
            tracer = 0;

            self->firecount[hand]++;

            if (weaponGetFireRate(weapon->weaponnum) < 0)
            {
                shoot = 1;
                tracer = 1;
            }
            else if (((s32) self->firecount[hand] % weaponGetFireRate(weapon->weaponnum)) == 0)
            {
                shoot = 1;

                if ((((s32) self->firecount[hand] % (s32) (weaponGetFireRate(weapon->weaponnum) * 2)) == 0)
                    || (weapon->weaponnum == ITEM_LASER))
                {
                    tracer = 1;
                }
            }
            else
            {
                betweenShots = 1;
            }

            if (shoot != 0)
            {
                hittile = NULL;
                subroty = chrGetSubrotySideback(self);
                aimup = chrGetAimUpAngle(self);
                self_stan = self_prop->stan;
                firing = 1;

                if (chrGetHandPos(self, hand, (coord3d *) &gunpos) == 0)
                {
                    gunpos.f[0] = self_prop->pos.f[0];
                    gunpos.f[1] = self_prop->pos.f[1] + 30.0f;
                    gunpos.f[2] = self_prop->pos.f[2];

                    if (hand == 1)
                    {
                        gunpos.f[0] += cosf(subroty) * 10.0f;
                        gunpos.f[2] += -sinf(subroty) * 10.0f;
                    }
                    else
                    {
                        gunpos.f[0] += -cosf(subroty) * 10.0f;
                        gunpos.f[2] += sinf(subroty) * 10.0f;
                    }
                }

                if (stanTestLineUnobstructed(&self_stan, self_prop->pos.x, self_prop->pos.f[2], gunpos.f[0], gunpos.f[2], CDTYPE_DOORS, gunpos.f[1] - self->ground, gunpos.f[1] - self->ground, 0.0f, 1.0f) != 0)
                {
                    guntile = self_stan;
                }
                else
                {
                    self->firecount[hand]--;
                    firing = 0;
                }

                if (firing != 0)
                {
                    inSight = 0;
                    hitBond = 0;
                    showHit = 1;

                    canSeeTarget = chrCheckCanSeeTarget(self, &gunpos, guntile);

                    shotdir.f[0] = cosf(aimup) * sinf(subroty);
                    shotdir.f[1] = sinf(aimup);
                    shotdir.f[2] = cosf(aimup) * cosf(subroty);

                    hitpos.f[0] = gunpos.f[0] + (shotdir.f[0] * M_U16_MAX_VALUE_F);
                    hitpos.f[1] = gunpos.f[1] + (shotdir.f[1] * M_U16_MAX_VALUE_F);
                    hitpos.f[2] = gunpos.f[2] + (shotdir.f[2] * M_U16_MAX_VALUE_F);

                    chrSetMoving(self, 0);
                    stanResetHits();
                    self_stan = guntile;

                    if (stanTestLineUnobstructed(&self_stan, gunpos.f[0], gunpos.f[2], hitpos.f[0], hitpos.f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, gunpos.f[1], gunpos.f[1], hitpos.f[1], hitpos.f[1]) == 0)
                    {
                        chrStanLineDirIntersection(&gunpos, &shotdir, &hitpos);
                        hittile = self_stan;
                        hitpos.f[0] -= 26.0f * shotdir.f[0];
                        hitpos.f[1] -= 26.0f * shotdir.f[1];
                        hitpos.f[2] -= 26.0f * shotdir.f[2];
                    }

                    chrSetMoving(self, 1);

                    dx = hitpos.f[0] - gunpos.f[0];
                    dy = hitpos.f[1] - gunpos.f[1];
                    dz = hitpos.f[2] - gunpos.f[2];

                    shotDistSq = (dx * dx) + (dy * dy) + (dz * dz);

                    if (weapon->weaponnum == ITEM_ROCKETLAUNCH)
                    {
                        if (((dx * dx) + (dy * dy) + (dz * dz)) > 160000.0f)
                        {
                            rocket = weaponCreateFromModel(PROP_CHRROCKET, 0x56);
                            if (rocket != NULL)
                            {
                                mtx4LoadIdentity(&rocketMtx);
                                mtx4LoadXRotation(aimup, &rocketRot);
                                mtx4LoadYRotation(subroty, &rocketYRot);
                                mtx4MultAffineInPlace(&rocketYRot, &rocketRot);

                                rocketAccel.f[0] = shotdir.f[0] * 1.111111f;
                                rocketAccel.f[1] = shotdir.f[1] * 1.111111f;
                                rocketAccel.f[2] = shotdir.f[2] * 1.111111f;

                                rocketSpeed.f[0] = rocketAccel.f[0] * g_GlobalTimerDelta;
                                rocketSpeed.f[1] = rocketAccel.f[1] * g_GlobalTimerDelta;
                                rocketSpeed.f[2] = rocketAccel.f[2] * g_GlobalTimerDelta;

                                gunLaunchProjectile((ObjectRecord *)rocket, &gunpos, guntile, &rocketRot, &rocketSpeed, &rocketMtx, self_prop);

                                if (rocket->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
                                {
                                    rocket->projectile->flags |= 0x80;
                                    rocket->timer = -1;
                                    rocket->projectile->flags |= 0x20;

                                    rocket->projectile->riseY = rocket->runtime_pos.y;
                                    rocket->projectile->riseSpeed = rocket->projectile->speed.f[1];

                                    rocket->projectile->accel.x = rocketAccel.f[0];
                                    rocket->projectile->accel.y = rocketAccel.f[1];
                                    rocket->projectile->accel.z = rocketAccel.f[2];

                                    if (rocket->projectile->sounds[0] == NULL)
                                    {
                                        sndPlaySfx(g_musicSfxBufferPtr, ROCKET_LAUNCH_SFX, &rocket->projectile->sounds[0]);
                                    }
                                    else if (rocket->projectile->sounds[1] == NULL)
                                    {
                                        sndPlaySfx(g_musicSfxBufferPtr, ROCKET_LAUNCH_SFX, &rocket->projectile->sounds[1]);
                                    }
                                }
                            }
                        }
                        else
                        {
                            firing = 0;
                        }
                    }
                    else if (weapon->weaponnum == ITEM_GRENADELAUNCH)
                    {
                        if (((dx * dx) + (dy * dy) + (dz * dz)) > 160000.0f)
                        {
                            grenade = weaponCreateFromModel(PROP_CHRGRENADEROUND, 0x57);
                            if (grenade != NULL)
                            {
                                mtx4LoadIdentity(&grenadeMtx);
                                grenadeSpeed.f[0] = shotdir.f[0] * 33.333332f;
                                grenadeSpeed.f[1] = shotdir.f[1] * 33.333332f;
                                grenadeSpeed.f[2] = shotdir.f[2] * 33.333332f;
                                mtx4LoadXRotation(aimup, &grenadeRot);
                                mtx4LoadYRotation(subroty, &grenadeYRot);
                                mtx4MultAffineInPlace(&grenadeYRot, &grenadeRot);
                                grenade->timer = CHRLV_DEFAULT_TIMER;
                                gunLaunchProjectile((ObjectRecord *) grenade, &gunpos, guntile, &grenadeRot, &grenadeSpeed, &grenadeMtx, self_prop);

                                if (grenade->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
                                {
                                    grenade->projectile->bounciness = 0.3f;
                                    grenade->projectile->groundFriction = 0.13333333f;
#ifdef REFRESH_PAL
                                    grenade->projectile->refreshrate = 50;
#else
                                    grenade->projectile->refreshrate = 60;
#endif
                                }
                            }
                        }
                        else
                        {
                            firing = 0;
                        }
                    }
                    else
                    {
                        if ((needsSight != 0) && (canSeeTarget != 0))
                        {
                            dx = (player_prop->pos.f[0] - gunpos.f[0]) - (shotdir.f[0] * 15.0f);
                            dy = (player_prop->pos.f[1] - gunpos.f[1]) - (shotdir.f[1] * 15.0f);
                            dz = (player_prop->pos.f[2] - gunpos.f[2]) - (shotdir.f[2] * 15.0f);

                            if (((dx * dx) + (dy * dy) + (dz * dz)) <= shotDistSq)
                            {
                                chrUpdateShotbondsum(self, &inSight, &hitBond, weapon->weaponnum);
                                showHit = hitBond == 0;

                                if ((inSight != 0) && ((self->actiontype == ACT_ATTACK) || (self->actiontype == ACT_ATTACKROLL)))
                                {
                                    self->act_attack.attack_time = g_GlobalTimer;
                                }
                            }
                        }
                        else
                        {
                            if ((self->actiontype == ACT_ATTACK) || (self->actiontype == ACT_ATTACKROLL))
                            {
                                self->act_attack.attack_time = g_GlobalTimer;
                            }
                        }

                        if (hitBond != 0)
                        {
                            hitpos.f[0] = player_prop->pos.f[0];
                            hitpos.f[1] = player_prop->pos.f[1];
                            hitpos.f[2] = player_prop->pos.f[2];
                            hittile = player_prop->stan;
                            shotPlayHitPropSound(weapon->weaponnum, player_prop, -1);
                        }
                        else
                        {
                            if ((
                                    (g_StanHitProp == NULL)
                                    || ((g_StanHitProp->type != PROP_TYPE_CHR) && (g_StanHitProp->type != PROP_TYPE_VIEWER))
                                )
                                && (shotDistSq < 10000.0f))
                            {
                                showHit = 0;
                            }
                        }

                        if (showHit != 0)
                        {
                            if (hittile != 0)
                            {
                                sparkCreate(&hitpos, 1, 26.0f, (s16) hittile->room);
                            }

                            if (g_StanHitProp != NULL)
                            {
                                shotPlayHitPropSound(weapon->weaponnum, g_StanHitProp, -1);

                                if (g_StanHitProp->type == PROP_TYPE_CHR)
                                {
                                    if ((self->chrflags & CHRFLAG_CAN_SHOOT_CHRS) != 0)
                                    {
                                        chrTakeShot(g_StanHitProp->chr, 0xF, &shotdir, weapon->weaponnum, 0);
                                    }
                                }
                                else if ((g_StanHitProp->type == PROP_TYPE_OBJ) || (g_StanHitProp->type == PROP_TYPE_WEAPON))
                                {
                                    chrobjMaybeDetonateObjectIfFlags(
                                        g_StanHitProp->obj,
                                        weaponGetDamage(weapon->weaponnum),
                                        &hitpos,
                                        weapon->weaponnum,
                                        playerGetCurrentNum());
                                }
                            }
                            else
                            {
                                shotPlayHitSound(weapon->weaponnum, &hitpos, -1);
                            }
                        }

                        if (tracer != 0)
                        {
                            switch (weapon->weaponnum)
                            {
                                case ITEM_WPPK:
                                case ITEM_WPPKSIL:
                                case ITEM_TT33:
                                case ITEM_SKORPION:
                                case ITEM_AK47:
                                case ITEM_UZI:
                                case ITEM_MP5K:
                                case ITEM_MP5KSIL:
                                case ITEM_SPECTRE:
                                case ITEM_M16:
                                case ITEM_FNP90:
                                case ITEM_RUGER:
                                case ITEM_GOLDENGUN:
                                case ITEM_SILVERWPPK:
                                case ITEM_GOLDWPPK:
                                case ITEM_LASER:
                                    tracer = 1;
                                    break;

                                default:
                                    tracer = 0;
                                    break;
                            }
                        }

                        if (tracer != 0)
                        {
                            beamCreate(&self->beams[hand], weapon->weaponnum, &gunpos, &hitpos);
                        }
                    }
                }
            }

            ratelimit = (firing != 0) || (betweenShots != 0);

            chrPlayGunfireSound(self, hand, ratelimit);
        }

        chrSetFiring(self, hand, firing);
    }
}


void chrTriggerFireWeapon(ChrRecord *self)
{
    self->hidden &= ~CHRHIDDEN_FIRE_TRACER;

    if (self->hidden & CHRHIDDEN_FIRE_WEAPON_RIGHT)
    {
        chrFireHand(self, GUNRIGHT);

        self->hidden &= ~CHRHIDDEN_FIRE_WEAPON_RIGHT;
    }

     if (self->hidden & CHRHIDDEN_FIRE_WEAPON_LEFT)
    {
        chrFireHand(self, GUNLEFT);

        self->hidden &= ~CHRHIDDEN_FIRE_WEAPON_LEFT;
    }
}


s32 chrAttackRollToKneel(ChrRecord *self)
{
    Model *model;
    struct weapon_firing_animation_table *p;
    s32 flip;

    if ((self->act_attackroll.animfloats == &g_ChrFireAnims[2]) || (self->act_attackroll.animfloats == &g_ChrFireAnims[3]))
    {
        model = self->model;
        flip = (s32) model->flip;
        self->act_attackroll.turning = 2;
        self->act_attackroll.animfloats = &g_ChrFireAnims[1];
        self->sleep = 0;

        p = &g_ChrFireAnims[1];

        modelSetAnimation(
            model,
            (void *) p->anim.anim,
            flip,
            p->shoot_end_frame,
            chrGetGuard007SpeedRating(self, 0.7f, 1.12f),
            22.0f);

        if (g_ChrFireAnims[1].end_frame >= 0.0f)
        {
            modelSetAnimEndFrame(model, g_ChrFireAnims[1].end_frame);
        }

        return 1;
    }

    return 0;
}


void chrAttackRollResumeAfterShot(ChrRecord *self)
{
    Model *model;

    model = self->model;

    if (self->act_attackroll.animfloats->recoil_end_frame > 0.0f)
    {
        modelSetAnimation(
            model,
            modelGetAnim(model),
            (s32) model->flip,
            self->act_attackroll.animfloats->recoil_end_frame,
            chrGetGuard007SpeedRating(self, 0.5f, 0.8f),
            8.0f);
    }
    else
    {
        modelSetAnimation(
            model,
            modelGetAnim(model),
            (s32) model->flip,
            self->act_attackroll.animfloats->shoot_end_frame,
            chrGetGuard007SpeedRating(self, 0.5f, 0.8f),
            8.0f);
    }

    if (self->act_attackroll.animfloats->end_frame >= 0.0f)
    {
        modelSetAnimEndFrame(model, self->act_attackroll.animfloats->end_frame);
    }
}


/**
 * Address 0x7F02E4F4 (VERSION_EU).
*/
void chrTickAttackCommon(ChrRecord *self)
{
    s32 i;
    Model *self_model;
    f32 df;
    f32 speed;
    f32 fp1; // 92
    f32 frame;
    f32 fp2;
    f32 fn40; // 80
    f32 fanon1; // 76

    self_model = self->model;
    frame = modelGetAnimFrame(self_model);

    if (
#ifdef REFRESH_PAL
        (self->act_attack.attack_time < (self->act_attack.lastFire60 - 25))
#else
        (self->act_attack.attack_time < (self->act_attack.lastFire60 - 30))
#endif
        && (self_model->anim2 == NULL))
    {
        if (((self->act_attack.animfloats->shoot_start_frame + 10.0f) < frame)
            && (frame < self->act_attack.animfloats->shoot_end_frame))
        {
            if (((self->act_attack.animfloats->recoil_end_frame < 0.0f)) || (frame < self->act_attack.animfloats->recoil_end_frame))
            {
                if (self->act_attack.doRecoil == 0)
                {
                    if (chrAttackRollToKneel(self) == 0)
                    {
                        modelSetAnimation(
                            self_model,
                            modelGetAnim(self_model),
                            (s32) self_model->flip,
                            self->act_attack.animfloats->shoot_end_frame,
                            chrGetGuard007SpeedRating(self, 0.5f, 0.8f),
                            8.0f);

                        if (self->act_attack.animfloats->end_frame >= 0.0f)
                        {
                            modelSetAnimEndFrame(self_model, self->act_attack.animfloats->end_frame);
                        }
                    }
                }
                else
                {
                    chrAttackRollResumeAfterShot(self);
                }

                self->act_attack.numShots = (s8) (self->act_attack.maxShots + 1);
                frame = modelGetAnimFrame(self_model);
            }
        }
    }

    if (modelGetAnimEndFrame(self_model) <= frame)
    {
        if ((self->act_attack.doOneBurst != 0) || (self->act_attack.maxShots < self->act_attack.numShots))
        {
            if (chrAttackRollToKneel(self) == 0)
            {
                if ((self->act_attack.attacktype & TARGET_BOND) != 0)
                {
                    chrSetTargetToPlayer(self);
                }

                chrStandCheckWall(self);

                return;
            }
        }
        else if (self->act_attack.numShots == self->act_attack.maxShots)
        {
            self->act_attack.numShots++;
            chrAttackRollResumeAfterShot(self);
        }
        else if (self->act_attack.fired != 0)
        {
            speed = 0.5f;

            if (self->act_attack.doRecoil != 0)
            {
                if (self->act_attack.animfloats->recoil_start_frame > 0.0f)
                {
                    fp1 = self->act_attack.animfloats->recoil_start_frame;
                }
                else
                {
                    fp1 = self->act_attack.animfloats->shoot_start_frame;
                }

                if (self->act_attack.animfloats->recoil_end_frame > 0.0f)
                {
                    fp2 = self->act_attack.animfloats->recoil_end_frame;
                }
                else
                {
                    fp2 = self->act_attack.animfloats->shoot_end_frame;
                }
            }
            else
            {
                fp1 = self->act_attack.animfloats->shoot_start_frame;

                if (self->act_attack.animfloats->recoil_start_frame > 0.0f)
                {
                    fp2 = self->act_attack.animfloats->recoil_start_frame;
                }
                else
                {
                    fp2 = self->act_attack.animfloats->shoot_end_frame;
                }
            }

            df = fp2 - fp1;
            if (df < 12.0f)
            {
                speed = (df * 0.5f) / 12.0f;
            }
            else if (df > 16.0f)
            {
                speed = df * 0.5f * 0.0625f;
            }

            if ((self->act_attack.singleShot[0] != 0) && (self->act_attack.singleShot[1] != 0))
            {
                speed = 2.0f * speed;
            }

            self->act_attack.fired = 0;

            modelSetAnimation(self_model, modelGetAnim(self_model), (s32) self_model->flip, fp1, speed, 8.0f);
            modelSetAnimEndFrame(self_model, fp2);
        }

        frame = modelGetAnimFrame(self_model);
    }

    if ((self->act_attack.attacktype & TARGET_DONTTURN) == 0)
    {
        fn40 = self->act_attack.animfloats->angle_offset;
        fanon1 = self->act_attack.animfloats->turnEndFrame;

        if ((self->act_attack.attacktype & TARGET_AIM_ONLY) != 0)
        {
            if (modelGetAnimEndFrame(self_model) < fanon1)
            {
                fanon1 = modelGetAnimEndFrame(self_model);
            }
        }

        if (self_model->flip != GUNRIGHT)
        {
            fn40 = M_TAU_F - fn40;
        }

        self->act_attack.turning = chrSetSubroty(
            self,
            (s32) self->act_attack.turning,
            fanon1,
            chrGetGuard007SpeedRating(self, 1.0f, 1.6f),
            fn40);
    }

    if ((self->act_attack.animfloats->aim_start_frame < frame) && (frame < self->act_attack.animfloats->aim_end_frame))
    {
        chrUpdateAimendsideback(self, self->act_attack.animfloats, (s32) self->act_attack.hasGun[1], (s32) self->act_attack.hasGun[0], 1.0f);
    }
    else
    {
        chrResetAimend(self);
    }

    for (i=0; i<2; i++)
    {
        if (self->act_attack.hasGun[i] != 0)
        {
            if (self->act_attack.singleShot[i] == 0)
            {
                if ((self->act_attack.animfloats->shoot_start_frame <= frame) && (frame < self->act_attack.animfloats->shoot_end_frame))
                {
                    chrSetHandFiring(self, i, 1);
                    self->act_attack.lastFire60 = g_GlobalTimer;

                    if (self->actiontype == ACT_ATTACKROLL)
                    {
#ifdef REFRESH_PAL
                        df = ((self->act_attack.animfloats->shoot_end_frame - self->act_attack.animfloats->shoot_start_frame) * 50.0f) / 60.0f;
#else
                        df = self->act_attack.animfloats->shoot_end_frame - self->act_attack.animfloats->shoot_start_frame;
#endif

                        if (df < 30.0f)
                        {
#ifdef REFRESH_PAL
                            if ((s32) self->act_attack.numWaitTicks >= (50 - ((s32) df * 2)))
#else
                            if ((s32) self->act_attack.numWaitTicks >= (60 - ((s32) df * 2)))
#endif
                            {
                                modelSetAnimSpeed(self_model, 0.5f, 0.0f);
                            }
                            else
                            {
                                modelSetAnimSpeed(self_model, 0.1f, 0.0f);
                                self->act_attack.numWaitTicks += g_ClockTimer;
                            }
                        }
                        else
                        {
                            modelSetAnimSpeed(self_model, 0.5f, 0.0f);
                        }
                    }
                    else
                    {
                        modelSetAnimSpeed(self_model, 0.5f, 0.0f);
                    }
                }
                else
                {
                    chrSetHandFiring(self, i, 0);

                    if (self->actiontype == ACT_ATTACKROLL)
                    {
                        modelSetAnimSpeed(self_model, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 0.0f);
                    }
                    else
                    {
                        modelSetAnimSpeed(self_model, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 0.0f);
                    }
                }
            }
            else if (
                (self->act_attack.fired == 0)
                && ((i == self->act_attack.nextGun) || (self->act_attack.singleShot[self->act_attack.nextGun] == 0))
                && (
                    (
                        ( self->act_attack.animfloats->recoil_start_frame >= 0.0f)
                        && (self->act_attack.animfloats->recoil_start_frame <= frame)
                        && (frame <= self->act_attack.animfloats->recoil_end_frame))
                    ||
                    (
                        (self->act_attack.animfloats->recoil_start_frame < 0.0f)
                        && (self->act_attack.animfloats->shoot_start_frame <= frame)
                    )))
            {
                self->act_attack.fired = 1;
                self->act_attack.nextGun = (s8) (1 - self->act_attack.nextGun);
                self->act_attack.numShots++;
                self->act_attack.lastFire60 = g_GlobalTimer;

                chrSetHandFiring(self, i, 1);
            }
            else
            {
                chrSetHandFiring(self, i, 0);
            }
        }
        else
        {
            chrSetHandFiring(self, i, 0);
        }
    }
}


/**
 * Address 0x7F02EBFC (VERSION_US).
 * Adresss 0x7F02EF04 (other).
*/
void chrTickAttack(ChrRecord *self)
{
    Model *self_model;
    f32 frame;
    f32 endframe;

    self_model = self->model;
    frame = modelGetAnimFrame(self_model);

    if (self->act_attack.type_of_motion)
    {
        if (self->act_attack.type_of_motion == 1)
        {
            if (self->act_attack.animfloats->recoil_end_frame >= 0.0f)
            {
                endframe = self->act_attack.animfloats->recoil_end_frame;
            }
            else
            {
                endframe = self->act_attack.animfloats->shoot_end_frame;
            }

            modelSetAnimation(
                self_model,
                modelGetAnim(self_model),
                (s32) self_model->flip,
                endframe,
                chrGetGuard007SpeedRating(self, 0.5f, 0.8f),
                16.0f);

            if (self->act_attack.animfloats->end_frame >= 0.0f)
            {
                modelSetAnimEndFrame(self_model, self->act_attack.animfloats->end_frame);
            }

            self->act_attack.type_of_motion = 2;
            chrResetAimend(self);

            return;
        }

        if (self->act_attack.type_of_motion == 2)
        {
            if (modelGetAnimEndFrame(self_model) <= frame)
            {
#if defined(VERSION_US)
                self->act_attack.attacktype |= TARGET_AIM_ONLY;
#else
                // don't set TARGET_AIM_ONLY
#endif
                self->act_attack.attacktype &= ~TARGET_DONTTURN;

                if (self->act_attack.standing != 0)
                {
                    chrAttackStand(self, (s32) self->act_attack.attacktype, self->act_attack.entityid);

                    return;
                }

                chrAttackKneel(self, (s32) self->act_attack.attacktype, self->act_attack.entityid);

                return;
            }

            return;
        }
    }

    if ((self->act_attack.attacktype & TARGET_AIM_ONLY) != 0)
    {
        if ((self->act_attack.attacktype & TARGET_DONTTURN) != 0)
        {
            if (chrUpdateAimendsideback(self, self->act_attack.animfloats, (s32) self->act_attack.hasGun[1], (s32) self->act_attack.hasGun[0], 0.2f) == 0)
            {
                self->act_attack.type_of_motion = 1;
            }

            return;
        }

        if (modelGetAnimEndFrame(self_model) <= frame)
        {
            self->act_attack.attacktype |= TARGET_DONTTURN;
            self->act_attack.turning = 2;

            return;
        }
    }

    if (self->act_attack.doRecoil == 0)
    {
        if ((self->act_attack.animfloats->recoil_end_frame > 0.0f) && (frame <= self->act_attack.animfloats->recoil_end_frame))
        {
            if (modelGetAnimEndFrame(self_model) <= frame)
            {
                modelSetAnimation(
                    self_model,
                    modelGetAnim(self_model),
                    (s32) self_model->flip,
                    self->act_attack.animfloats->recoil_end_frame,
                    chrGetGuard007SpeedRating(self, 0.5f, 0.8f),
                    16.0f);

                if (self->act_attack.doOneBurst != 0)
                {
                    if (self->act_attack.animfloats->end_frame >= 0.0f)
                    {
                        modelSetAnimEndFrame(self_model, self->act_attack.animfloats->end_frame);
                    }
                }
                else
                {
                    modelSetAnimEndFrame(self_model, self->act_attack.animfloats->shoot_end_frame);
                }
            }
        }
    }

    chrTickAttackCommon(self);
}


void chrTickAttackRoll(ChrRecord *self)
{
    Model *model; // 68
    f32 frame;
    struct weapon_firing_animation_table *nextanim; // 60
    s32 flip; // 56
    f32 turnframes; // 52
    ModelRwData_HeaderRecord *rootrw;

    if (self->act_attackroll.oneHanded != 0)
    {
        model = self->model;
        frame = modelGetAnimFrame(model);

        if (
            (self->act_attackroll.animfloats == &g_ChrFireAnims[4])
            || (self->act_attackroll.animfloats == &g_ChrFireAnims[5])
            || (self->act_attackroll.animfloats == &g_ChrFireAnims[6])
            || (self->act_attackroll.animfloats == &g_ChrFireAnims[7])
        )
        {
            if (self->act_attackroll.animfloats->end_frame <= frame)
            {
                flip = (s32) model->flip;
                nextanim = &self->act_attackroll.animfloats[4];

                turnframes = 16.0f;

                if ((self->act_attackroll.hasGun[1] != 0) && (self->act_attackroll.hasGun[0] != 0))
                {
                    if ((randomGetNext() & 1) == 0)
                    {
                        nextanim = &nextanim[4];
                    }
                    else
                    {
                        nextanim = &nextanim[8];
                    }
                }

                if (nextanim == &g_ChrFireAnims[8])
                {
                    turnframes = 24.0f;
                }
                else if (nextanim == &g_ChrFireAnims[9])
                {
                    turnframes = 24.0f;
                }
                else if (nextanim == &g_ChrFireAnims[10])
                {
                    turnframes = 32.0f;
                }
                else if (nextanim == &g_ChrFireAnims[11])
                {
                    turnframes = 44.0f;
                }
                else if (nextanim == &g_ChrFireAnims[12])
                {
                    turnframes = 24.0f;
                }
                else if (nextanim == &g_ChrFireAnims[13])
                {
                    turnframes = 34.0f;
                }
                else if (nextanim == &g_ChrFireAnims[14])
                {
                    turnframes = 32.0f;
                }
                else if (nextanim == &g_ChrFireAnims[15])
                {
                    turnframes = 44.0f;
                }
                else if (nextanim == &g_ChrFireAnims[16])
                {
                    turnframes = 24.0f;
                }
                else if (nextanim == &g_ChrFireAnims[17])
                {
                    turnframes = 34.0f;
                }
                else if (nextanim == &g_ChrFireAnims[18])
                {
                    turnframes = 32.0f;
                }
                else if (nextanim == &g_ChrFireAnims[19])
                {
                    turnframes = 44.0f;
                }

                self->act_attackroll.turning = 2;
                self->act_attackroll.animfloats = nextanim;
                self->sleep = 0;

                modelSetAnimation(model, (void *) nextanim->anim.anim, flip, nextanim->start_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), turnframes);

                if (self->act_attackroll.doRecoil != 0)
                {
                    if (nextanim->recoil_end_frame >= 0.0f)
                    {
                        modelSetAnimEndFrame(model, nextanim->recoil_end_frame);
                    }
                    else
                    {
                        modelSetAnimEndFrame(model, nextanim->shoot_end_frame);
                    }
                }
                else if (nextanim->recoil_start_frame >= 0.0f)
                {
                    modelSetAnimEndFrame(model, nextanim->recoil_start_frame);
                }
                else if (nextanim->end_frame >= 0.0f)
                {
                    modelSetAnimEndFrame(model, nextanim->end_frame);
                }

                if (self->act_attackroll.animfloats->angle_offset != 0.0f)
                {
                    rootrw = &modelGetNodeRwData(model, model->obj->RootNode)->Header;
                    rootrw->turnFrames = turnframes;
                    rootrw->turnPerFrame = (-self->act_attackroll.animfloats->angle_offset / turnframes);

                    if (flip != GUNRIGHT)
                    {
                        rootrw->turnPerFrame = -rootrw->turnPerFrame;
                    }
                }
            }
        }
        else if (
            (
                (self->act_attackroll.animfloats == &g_ChrFireAnims[8])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[9])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[10])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[11])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[12])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[13])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[14])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[15])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[16])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[17])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[18])
                || (self->act_attackroll.animfloats == &g_ChrFireAnims[19])
            )
            && (self->act_attackroll.doRecoil == 0))
        {
            if ((self->act_attackroll.animfloats->recoil_end_frame > 0.0f) && (frame <= self->act_attackroll.animfloats->recoil_end_frame))
            {
                if (modelGetAnimEndFrame(model) <= frame)
                {
                    modelSetAnimation(model, modelGetAnim(model), (s32) model->flip, self->act_attackroll.animfloats->recoil_end_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);

                    if (self->act_attackroll.doOneBurst != 0)
                    {
                        if (self->act_attackroll.animfloats->end_frame >= 0.0f)
                        {
                            modelSetAnimEndFrame(model, self->act_attackroll.animfloats->end_frame);
                        }
                    }
                    else
                    {
                        modelSetAnimEndFrame(model, self->act_attackroll.animfloats->shoot_end_frame);
                    }
                }
            }
        }
    }
    chrTickAttackCommon(self);
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
        propobjSetDropped(self->weapons_held[gunhand], 3);
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


#if !defined(REFRESH_PAL)
/* NTSC */
#define MAX_SPEED_A 0.2991993f
#define ACCEL_A 0.014959966f

#define MAX_SPEED_B1 0.019634955f
#define MAX_SPEED_B2 0.09817477f
#define MAX_SPEED_B3 0.19634955f
#define ACCEL_B 0.014959966f

#define MAX_SPEED_C1 0.009817477f
#define MAX_SPEED_C2 0.049087387f
#define MAX_SPEED_C3 0.12566371f
#define ACCEL_C 0.009817477f
#endif

#if defined(REFRESH_PAL)
/* PAL */
#define MAX_SPEED_A 0.359039157629013f
#define ACCEL_A 0.0179519578814507f

#define MAX_SPEED_B1 0.0235619451850653f
#define MAX_SPEED_B2 0.117809727787972f
#define MAX_SPEED_B3 0.235619455575943f
#define ACCEL_B 0.0179519578814507f

#define MAX_SPEED_C1 0.0117809725925326f
#define MAX_SPEED_C2 0.0589048638939858f
#define MAX_SPEED_C3 0.150796458125114f
#define ACCEL_C 0.0117809725925326f
#endif

s32 chrApplySpeed(ChrRecord *self, coord3d *target, s32 speed, f32 *speedPtr)
{
    f32 maxSpeed;
    Model *self_model; // 72
    PropRecord *self_prop;
    f32 accel;
    f32 maxFrac; // 60
    f32 openPosition; // 56
    f32 angle;
    f32 f0f0;
    f32 dx;
    f32 dz;
    s32 arrived;

    self_prop = self->prop;
    self_model = self->model;

    dx = target->f[0] - self_prop->pos.f[0];
    dz = target->f[2] - self_prop->pos.f[2];

    maxFrac = mathAtan2(dx, dz);
    openPosition = modelGetRootRotY(self_model);

    arrived = 0;
    angle = maxFrac - openPosition;

    if (maxFrac < openPosition)
    {
        angle = angle + M_TAU_F;
    }

    f0f0 = angle;

    if (angle > M_PI_F)
    {
        f0f0 = M_TAU_F - angle;
    }

    if (speed == 2)
    {
        maxSpeed = MAX_SPEED_A;
        accel = ACCEL_A;
    }
    else if (speed == 1)
    {
        if (f0f0 < 0.3926991f)
        {
            maxSpeed = MAX_SPEED_B1;
        }
        else if (f0f0 < 1.2566371f)
        {
            maxSpeed = MAX_SPEED_B2;
        }
        else
        {
            maxSpeed = MAX_SPEED_B3;
        }

        accel = ACCEL_B;
    }
    else
    {
        if (f0f0 < 0.3926991f)
        {
            maxSpeed = MAX_SPEED_C1;
        }
        else if (f0f0 < 1.2566371f)
        {
            maxSpeed = MAX_SPEED_C2;
        }
        else
        {
            maxSpeed = MAX_SPEED_C3;
        }

        accel = ACCEL_C;
    }

    maxSpeed *= self_model->playspeed;
    accel *= self_model->playspeed;

    // void chrobjCallsApplySpeed(f32 *openPosition, f32 maxFrac, f32 *speedPtr, f32 accel, f32 decel, f32 maxSpeed)
    chrobjCallsApplySpeed(
        &openPosition,
        maxFrac,
        speedPtr,
        accel,
        accel * 2.0f,
        maxSpeed);

    if (openPosition == maxFrac)
    {
        *speedPtr = 0.0f;
        arrived = 1;
    }

    modelSetRootRotY(self_model, openPosition);

    return arrived;
}


void chrTickAttackWalk(ChrRecord *self)
{
    Model *self_model;
    PropRecord *player_prop;
    f32 dx;
    f32 dz;
    PropRecord *self_prop;
    s32 i;

    self_model = self->model;
    self_prop = self->prop;
    player_prop = currentPlayerGetProp();
    self->act_attackwalk.clock_timer30 += g_ClockTimer;
    self->lastwalk60 = g_GlobalTimer;

    if (
        (self->invalidmove == 1)
        || (self->lastmoveok60 < (g_GlobalTimer - CHRLV_LASTMOVEOK60_CHECK))
        || (self->act_attackwalk.clock_timer34 < self->act_attackwalk.clock_timer30))
    {
        if (modelGetAnimFrame(self_model) > ((f32)modelGetAnim(self_model)->numFrames * 0.5f))
        {
            modelSetAnimSpeedToReachFrame(self_model, 0.0f, 16.0f);
        }
        else
        {
            modelSetAnimSpeedToReachFrame(self_model, (f32)modelGetAnim(self_model)->numFrames * 0.5f, 16.0f);
        }

        chrSetTargetToPlayer(self);
        chrStandCheckWall(self);

        return;
    }

    dx = player_prop->pos.f[0] - self_prop->pos.f[0];
    dz = player_prop->pos.f[2] - self_prop->pos.f[2];
    if ((dx < 300.0f) && (dx > -300.0f) && (dz < 300.0f) && (dz > -300.0f))
    {
        chrSetTargetToPlayer(self);
        chrStandCheckWall(self);

        return;
    }

    if (chrApplySpeed(self, &player_prop->pos, 0, &self->act_attackwalk.speed) != 0)
    {
        self->act_attackwalk.facedTarget = 1;
    }

    if (self->act_attackwalk.clock_timer30 >= CHRLV_ATTACKWALK_CLOCK_TIMER30_MIN)
    {
        chrUpdateAimendsideback(self, self->act_attackwalk.animfloats, (s32) self->act_attackwalk.hasGun[1], (s32) self->act_attackwalk.hasGun[0], 1.0f);
    }
    else
    {
        chrResetAimend(self);
    }

    if (self->act_attackwalk.facedTarget && !(self->act_attackwalk.clock_timer30 < CHRLV_ATTACKWALK_CLOCK_TIMER30_MAX))
    {
        for (i=0; i<2; i++)
        {
            if (self->act_attackwalk.hasGun[i])
            {
                if (self->act_attackwalk.singleShot[i] == 0)
                {
                    chrSetHandFiring(self, i, 1);
                }
                else
                {
                    if ((self->act_attackwalk.timer40 < self->act_attackwalk.clock_timer30)
                        && ((i == self->act_attackwalk.nextGun) || (self->act_attackwalk.singleShot[self->act_attackwalk.nextGun] == 0)))
                    {
                        self->act_attackwalk.timer40 = self->act_attackwalk.clock_timer30;

                        if (self->act_attackwalk.singleShot[1 - i])
                        {
                            if (self->act_attackwalk.isLauncher[i])
                            {
                                self->act_attackwalk.timer40 += CHRLV_ATTACKWALK_TIMER40_A;
                            }
                            else
                            {
                                self->act_attackwalk.timer40 += CHRLV_ATTACKWALK_TIMER40_B;
                            }
                        }
                        else if (self->act_attackwalk.isLauncher[i])
                        {
                            self->act_attackwalk.timer40 += CHRLV_ATTACKWALK_TIMER40_C;
                        }
                        else
                        {
                            self->act_attackwalk.timer40 += CHRLV_ATTACKWALK_TIMER40_D;
                        }

                        self->act_attackwalk.nextGun = 1 - self->act_attackwalk.nextGun;

                        chrSetHandFiring(self, i, 1);
                    }
                    else
                    {
                        chrSetHandFiring(self, i, 0);
                    }
                }
            }
            else
            {
                chrSetHandFiring(self, i, 0);
            }
        }

        return;
    }

    chrSetHandFiring(self, GUNLEFT, 0);
    chrSetHandFiring(self, GUNRIGHT, 0);
}


/**
 * @param arg0: point in 3d
 * @param arg1: 3 vec
 * @param arg0: point in 3d
 * @param arg0: scalar value
*/
s32 rayPassesNearPoint2d(coord3d *origin, coord3d *dir, coord3d *point, f32 range)
{
    coord3d dd;
    f32 along;

    dd.f[0] = point->f[0] - origin->f[0];
    dd.f[2] = point->f[2] - origin->f[2];

    if ((dir->f[0] == 0.0f) && (dir->f[2] == 0.0f))
    {
        return ((dd.f[0] * dd.f[0]) + (dd.f[2] * dd.f[2])) <= (range * range);
    }

    along = (dir->f[0] * dd.f[0]) + (dir->f[2] * dd.f[2]);

    if (along > 0.0f)
    {
        f32 alongSq = along * along;
        f32 dirLenSq = (dir->f[0] * dir->f[0]) + (dir->f[2] * dir->f[2]);
        f32 f3 = (dd.f[0] * dd.f[0]) + (dd.f[2] * dd.f[2]);

        if ((dirLenSq * (f3 - (range * range))) <= alongSq)
        {
            return 1;
        }

        return 0;
    }

    return 0;
}


/**
 * @param prevpos: point in 3d
 * @param curpos: 3 vec
 * @param prevpos: point in 3d
 * @param prevpos: scalar value
 *
 *
 * PD posIsArrivingLaterallyAtPos.
*/
s32 chrIsArrivingLaterallyAtPos(coord3d *prevpos, coord3d *curpos, coord3d *targetpos, f32 range)
{
    coord3d dir;

    if ((prevpos->f[0] <= targetpos->f[0] - range) && (curpos->f[0] <= targetpos->f[0] - range))
    {
        return 0;
    }

    if ((targetpos->f[0] + range <= prevpos->f[0]) && (targetpos->f[0] + range <= curpos->f[0]))
    {
        return 0;
    }

    if ((prevpos->f[2] <= targetpos->f[2] - range) && (curpos->f[2] <= targetpos->f[2] - range))
    {
        return 0;
    }

    if ((targetpos->f[2] + range <= prevpos->f[2]) && (targetpos->f[2] + range <= curpos->f[2]))
    {
        return 0;
    }

    dir.f[0] = curpos->f[0] - prevpos->f[0];
    dir.f[1] = 0.0f;
    dir.f[2] = curpos->f[2] - prevpos->f[2];

    return rayPassesNearPoint2d(prevpos, &dir, targetpos, range);
}


void chrTickRunPos(ChrRecord *self)
{
    PropRecord *self_prop;
    Model *self_model;
    f32 frame; // 52

    self_prop = self->prop;
    self_model = self->model;
    self->lastwalk60 = g_GlobalTimer;

    if(1)
    {
        // removed
    }

    if ((self->invalidmove == 1)
        || (self->lastmoveok60 < (g_GlobalTimer - CHRLV_LASTMOVEOK60_CHECK))
        || (chrIsArrivingLaterallyAtPos(&self->prevpos, &self_prop->pos, &self->act_runpos.pos, self->act_runpos.neardist)))
    {
        f32 offset = 0;

        // Maybe had debug side effects, otherwise this doesn't do anything.
        modelGetAnim(self_model);

        frame = modelGetAnimFrame(self_model) - offset;

        if (frame < 0.0f)
        {
            frame += (f32)modelGetAnim(self_model)->numFrames;
        }

        if (((f32)modelGetAnim(self_model)->numFrames * 0.5f) < frame)
        {
            frame = (f32)modelGetAnim(self_model)->numFrames - offset;
            modelSetAnimSpeedToReachFrame(self_model, frame, 16.0f);
        }
        else
        {
            frame = ((f32)modelGetAnim(self_model)->numFrames * 0.5f) - offset;

            if (frame < 0)
            {
                frame += (f32)modelGetAnim(self_model)->numFrames;
            }

            modelSetAnimSpeedToReachFrame(self_model, frame, 16.0f);
        }

        chrStandCheckWall(self);

        return;
    }

    chrApplySpeed(self, &self->act_runpos.pos, 1, &self->act_runpos.turnspeed);

    if (self->act_runpos.eta60 > 0)
    {
        self->act_runpos.eta60 -= g_ClockTimer;
    }
    else
    {
        f32 speed;

        speed = g_ChrSpeedRun;

        if (modelGetAnim(self_model) == ANIM(running_one_handed_weapon))
        {
            speed = g_ChrSpeedRunOneHanded;
        }

        self->act_runpos.neardist += speed * g_GlobalTimerDelta * modelGetAbsAnimSpeed(self_model);
    }
}


s32 chrCanWalkLine(ChrRecord *self, coord3d *point, StandTile *tile, coord3d *dest, StandTile * desttile, s32 cdtypes)
{
    StandTile *curtile;
    s32 ok;
    f32 height;
    f32 always20;
    f32 width;

    curtile = tile;
    ok = 0;

    chrGetChrWidthHeight(self->prop, &width, &height, &always20);

    chrSetMoving(self, 0);

    if (
        stanTestLineUnobstructed(&curtile, point->f[0], point->f[2], dest->f[0], dest->f[2], cdtypes, height, always20, 0.0f, 1.0f)
        && ((desttile == NULL) || (curtile == desttile)))
    {
        ok = 1;
    }

    chrSetMoving(self, 1);

    return ok;
}


s32 chrCanWalkLineWithin(ChrRecord *self, coord3d *point, StandTile *tile, coord3d *dest, f32 halfwidth, s32 cdtypes)
{
    StandTile *pstan;
    coord3d dd;
    f32 offsetX;
    f32 offsetZ;
    f32 norm;
    s32 ret; // 104
    f32 height;
    f32 always20;
    f32 width;

    ret = 0;

    chrGetChrWidthHeight(self->prop, &width, &height, &always20);

    dd.f[0] = dest->f[0] - point->f[0];
    dd.f[1] = 0.0f;
    dd.f[2] = dest->f[2] - point->f[2];

    if ((dd.f[0] == 0.0f) && (dd.f[2] == 0.0f))
    {
        ret = 1;
    }
    else
    {
        norm = (dd.f[0] * dd.f[0]) + (dd.f[2] * dd.f[2]);
        norm = 1.0f / sqrtf(norm);

        dd.f[0] *= norm;
        dd.f[2] *= norm;

        offsetX = halfwidth * dd.f[0];
        offsetZ = halfwidth * dd.f[2];

        chrSetMoving(self, 0);

        pstan = tile;

        if (stanTestLineUnobstructed(&pstan, point->f[0], point->f[2], point->f[0] + offsetZ, point->f[2] - offsetX, cdtypes, height, always20, 0.0f, 1.0f)
            && stanTestLineUnobstructed(&pstan, point->f[0] + offsetZ, point->f[2] - offsetX, dest->f[0] + offsetZ, dest->f[2] - offsetX, cdtypes, height, always20, 0.0f, 1.0f))
        {
            pstan = tile;

            if (stanTestLineUnobstructed(&pstan, point->f[0], point->f[2], point->f[0] - offsetZ, point->f[2] + offsetX, cdtypes, height, always20, 0.0f, 1.0f)
                && stanTestLineUnobstructed(&pstan, point->f[0] - offsetZ, point->f[2] + offsetX, dest->f[0] - offsetZ, dest->f[2] + offsetX, cdtypes, height, always20, 0.0f, 1.0f))
            {
                ret = 1;
            }
        }

        chrSetMoving(self, 1);
    }

    return ret;
}


s32 chrCanWalkViaPoint(ChrRecord *self, coord3d *mypos, StandTile *mystan, coord3d *viapoint, coord3d *bondpos, StandTile *bondstan, s32 cdtypes)
{
    StandTile *tile;
    bool pass;
    f32 height;
    f32 always20;
    f32 width;
    StandTile *viatile;

    tile = mystan;
    pass = FALSE;

    chrGetChrWidthHeight(self->prop, &width, &height, &always20);
    chrSetMoving(self, 0);

    if (stanTestLineUnobstructed(&tile, mypos->x, mypos->z, viapoint->x, viapoint->z, cdtypes, height, always20, 0.0f, 1.0f))
    {
        viatile = tile;

        if (stanTestLineUnobstructed(&viatile, viapoint->x, viapoint->z, bondpos->x, bondpos->z, cdtypes, height, always20, 0.0f, 1.0f)
            && ((bondstan == NULL) || (viatile == bondstan)))
        {
            pass = TRUE;
        }
    }

    chrSetMoving(self, 1);

    return pass;
}


/**
 * Subtract arg0 from arg1. Take the determinate of the result and arg2.
 * If determinate is not greater than zero, then swap arg0 and arg1.
*/
void orderPointsBySide(coord3d *a, coord3d *b, coord3d *dir)
{
    coord3d ab;
    coord3d perp;

    ab.f[0] = b->f[0] - a->f[0];
    ab.f[1] = b->f[1] - a->f[1];
    ab.f[2] = b->f[2] - a->f[2];

    perp.f[0] = -dir->f[2];
    perp.f[1] = 0.0f;
    perp.f[2] = dir->f[0];

    if (!(((perp.f[0] * ab.f[0]) + (perp.f[2] * ab.f[2])) > 0.0f))
    {
        ab.f[0] = a->f[0];
        ab.f[1] = a->f[1];
        ab.f[2] = a->f[2];

        a->f[0] = b->f[0];
        a->f[1] = b->f[1];
        a->f[2] = b->f[2];

        b->f[0] = ab.f[0];
        b->f[1] = ab.f[1];
        b->f[2] = ab.f[2];
    }
}


/**
 * Very similar to @see chrNavCheckForObstacle .
*/
s32 chrNavCanSeeNextPos(ChrRecord *self, coord3d *from, StandTile *fromtile, coord3d *to, coord3d *edgeStart, coord3d *edgeEnd, f32 overshoot, f32 width, s32 cdtypes)
{
    StandTile *tile;
    coord3d dir;
    f32 nearX; // 156
    f32 nearZ; // 152
    f32 farX; // 148
    f32 farZ; // 144
    f32 norm;
    s32 leftBlocked; // 136
    s32 rightBlocked; // 132
    coord3d leftEdgeStart;
    coord3d leftEdgeEnd;
    coord3d rightEdgeStart;
    coord3d rightEdgeEnd;
    s32 ok;
    f32 height;
    f32 always20;
    f32 chrw;

    leftBlocked = 0;
    rightBlocked = 0;
    ok = 0;

    chrGetChrWidthHeight(self->prop, &chrw, &height, &always20);

    dir.f[0] = to->f[0] - from->f[0];
    dir.f[1] = 0.0f;
    dir.f[2] = to->f[2] - from->f[2];

    if ((dir.f[0] == 0.0f) && (dir.f[2] == 0.0f))
    {
        return 1;
    }

    norm = (dir.f[0] * dir.f[0]) + (dir.f[2] * dir.f[2]);
    norm = 1.0f / sqrtf(norm);

    dir.f[0] *= norm;
    dir.f[2] *= norm;

    nearX = 0.95f * (width * dir.f[0]);
    nearZ = 0.95f * (width * dir.f[2]);

    farX = 1.2f * (width * dir.f[0]);
    farZ = 1.2f * (width * dir.f[2]);

    chrSetMoving(self, 0);
    stanResetHits();

    tile = fromtile;

    if ((stanTestLineUnobstructed(
        &tile,
        from->f[0],
        from->f[2],
        from->f[0] + nearZ,
        from->f[2] - nearX,
        cdtypes,
        height,
        always20,
        0.0f,
        1.0f) == 0)
        || (stanTestLineUnobstructed(
            &tile,
            from->f[0] + nearZ,
            from->f[2] - nearX,
            (to->f[0] + farZ) + (dir.f[0] * overshoot),
            (to->f[2] - farX) + (dir.f[2] * overshoot),
            cdtypes,
            height,
            always20,
            0.0f,
            1.0f) == 0))
    {
        leftBlocked = 1;

        stanGetCollisionEdge(&leftEdgeStart, &leftEdgeEnd);
        orderPointsBySide(&leftEdgeStart, &leftEdgeEnd, &dir);
    }

    tile = fromtile;

    if ((stanTestLineUnobstructed(
        &tile,
        from->f[0],
        from->f[2],
        from->f[0] - nearZ,
        from->f[2] + nearX,
        cdtypes,
        height,
        always20,
        0.0f,
        1.0f) == 0)
        || (stanTestLineUnobstructed(
            &tile,
            from->f[0] - nearZ,
            from->f[2] + nearX,
            (to->f[0] - farZ) + (dir.f[0] * overshoot),
            (to->f[2] + farX) + (dir.f[2] * overshoot),
            cdtypes,
            height,
            always20,
            0.0f,
            1.0f) == 0))
    {
        rightBlocked = 1;

        stanGetCollisionEdge(&rightEdgeStart, &rightEdgeEnd);
        orderPointsBySide(&rightEdgeStart, &rightEdgeEnd, &dir);
    }

    if ((leftBlocked != 0) && (rightBlocked != 0))
    {
        orderPointsBySide(&leftEdgeStart, &rightEdgeStart, &dir);
        orderPointsBySide(&leftEdgeEnd, &rightEdgeEnd, &dir);

        edgeStart->f[0] = leftEdgeStart.f[0];
        edgeStart->f[1] = leftEdgeStart.f[1];
        edgeStart->f[2] = leftEdgeStart.f[2];

        edgeEnd->f[0] = rightEdgeEnd.f[0];
        edgeEnd->f[1] = rightEdgeEnd.f[1];
        edgeEnd->f[2] = rightEdgeEnd.f[2];
    }
    else if (leftBlocked != 0)
    {
        edgeStart->f[0] = leftEdgeStart.f[0];
        edgeStart->f[1] = leftEdgeStart.f[1];
        edgeStart->f[2] = leftEdgeStart.f[2];

        edgeEnd->f[0] = leftEdgeEnd.f[0];
        edgeEnd->f[1] = leftEdgeEnd.f[1];
        edgeEnd->f[2] = leftEdgeEnd.f[2];
    }
    else if (rightBlocked != 0)
    {
        edgeStart->f[0] = rightEdgeStart.f[0];
        edgeStart->f[1] = rightEdgeStart.f[1];
        edgeStart->f[2] = rightEdgeStart.f[2];

        edgeEnd->f[0] = rightEdgeEnd.f[0];
        edgeEnd->f[1] = rightEdgeEnd.f[1];
        edgeEnd->f[2] = rightEdgeEnd.f[2];
    }
    else
    {
        tile = fromtile;

        if (stanTestLineUnobstructed(&tile, from->f[0], from->f[2], to->f[0], to->f[2], cdtypes, height, always20, 0.0f, 1.0f)
            && stanTestVolume(&tile, to->f[0], to->f[2], width, cdtypes, height, always20) < 0)
        {
            ok = 1;
        }
        else
        {
            stanGetCollisionEdge(edgeStart, edgeEnd);
            orderPointsBySide(edgeStart, edgeEnd, &dir);
        }
    }

    chrSetMoving(self, 1);

    return ok;
}


/**
 * Very similar to @see chrNavCanSeeNextPos .
*/
s32 chrNavCheckForObstacle(ChrRecord *self, coord3d *from, StandTile *fromtile, coord3d *to, coord3d *edgeStart, coord3d *edgeEnd, f32 overshoot, f32 width, s32 cdtypes)
{
    StandTile *tile;
    coord3d dir;
    f32 nearX; // 164
    f32 nearZ; // 160
    f32 farX; // 156
    f32 farZ; // 152
    f32 norm;
    s32 leftBlocked; // 144
    s32 rightBlocked; // 140
    coord3d leftEdgeStart; // x
    coord3d leftEdgeEnd;
    coord3d rightEdgeStart;
    coord3d rightEdgeEnd;
    s32 ok;
    f32 stanval1;
    f32 stanval2;
    f32 height;
    f32 always20;
    f32 chrw;

    leftBlocked = 0;
    rightBlocked = 0;
    ok = 0;

    chrGetChrWidthHeight(self->prop, &chrw, &height, &always20);

    dir.f[0] = to->f[0] - from->f[0];
    dir.f[1] = 0.0f;
    dir.f[2] = to->f[2] - from->f[2];

    if ((dir.f[0] == 0.0f) && (dir.f[2] == 0.0f))
    {
        return 1;
    }

    norm = (dir.f[0] * dir.f[0]) + (dir.f[2] * dir.f[2]);
    norm = 1.0f / sqrtf(norm);

    dir.f[0] *= norm;
    dir.f[2] *= norm;

    nearX = 0.95f * (width * dir.f[0]);
    nearZ = 0.95f * (width * dir.f[2]);

    farX = 1.2f * (width * dir.f[0]);
    farZ = 1.2f * (width * dir.f[2]);

    chrSetMoving(self, 0);
    stanResetHits();

    tile = fromtile;

    if ((stanTestLineUnobstructed(
        &tile,
        from->f[0],
        from->f[2],
        from->f[0] + nearZ,
        from->f[2] - nearX,
        cdtypes,
        height,
        always20,
        0.0f,
        1.0f) == 0)
        || (stanTestLineUnobstructed(
            &tile,
            from->f[0] + nearZ,
            from->f[2] - nearX,
            (to->f[0] + farZ) + (dir.f[0] * overshoot),
            (to->f[2] - farX) + (dir.f[2] * overshoot),
            cdtypes,
            height,
            always20,
            0.0f,
            1.0f) == 0))
    {
        leftBlocked = 1;

        stanGetCollisionEdge(&leftEdgeStart, &leftEdgeEnd);
        orderPointsBySide(&leftEdgeStart, &leftEdgeEnd, &dir);

        stanval1 = g_StanHitFrac;
    }

    tile = fromtile;

    if ((stanTestLineUnobstructed(
        &tile,
        from->f[0],
        from->f[2],
        from->f[0] - nearZ,
        from->f[2] + nearX,
        cdtypes,
        height,
        always20,
        0.0f,
        1.0f) == 0)
        || (stanTestLineUnobstructed(
            &tile,
            from->f[0] - nearZ,
            from->f[2] + nearX,
            (to->f[0] - farZ) + (dir.f[0] * overshoot),
            (to->f[2] + farX) + (dir.f[2] * overshoot),
            cdtypes,
            height,
            always20,
            0.0f,
            1.0f) == 0))
    {
        rightBlocked = 1;

        stanGetCollisionEdge(&rightEdgeStart, &rightEdgeEnd);
        orderPointsBySide(&rightEdgeStart, &rightEdgeEnd, &dir);

        stanval2 = g_StanHitFrac;
    }

    if ((leftBlocked != 0) && (rightBlocked != 0))
    {
        if (stanval1 < stanval2)
        {
            edgeStart->f[0] = leftEdgeStart.f[0];
            edgeStart->f[1] = leftEdgeStart.f[1];
            edgeStart->f[2] = leftEdgeStart.f[2];

            edgeEnd->f[0] = leftEdgeEnd.f[0];
            edgeEnd->f[1] = leftEdgeEnd.f[1];
            edgeEnd->f[2] = leftEdgeEnd.f[2];
        }
        else
        {
            edgeStart->f[0] = rightEdgeStart.f[0];
            edgeStart->f[1] = rightEdgeStart.f[1];
            edgeStart->f[2] = rightEdgeStart.f[2];

            edgeEnd->f[0] = rightEdgeEnd.f[0];
            edgeEnd->f[1] = rightEdgeEnd.f[1];
            edgeEnd->f[2] = rightEdgeEnd.f[2];
        }
    }
    else if (leftBlocked != 0)
    {
        edgeStart->f[0] = leftEdgeStart.f[0];
        edgeStart->f[1] = leftEdgeStart.f[1];
        edgeStart->f[2] = leftEdgeStart.f[2];

        edgeEnd->f[0] = leftEdgeEnd.f[0];
        edgeEnd->f[1] = leftEdgeEnd.f[1];
        edgeEnd->f[2] = leftEdgeEnd.f[2];
    }
    else if (rightBlocked != 0)
    {
        edgeStart->f[0] = rightEdgeStart.f[0];
        edgeStart->f[1] = rightEdgeStart.f[1];
        edgeStart->f[2] = rightEdgeStart.f[2];

        edgeEnd->f[0] = rightEdgeEnd.f[0];
        edgeEnd->f[1] = rightEdgeEnd.f[1];
        edgeEnd->f[2] = rightEdgeEnd.f[2];
    }
    else
    {
        tile = fromtile;

        if (stanTestLineUnobstructed(&tile, from->f[0], from->f[2], to->f[0], to->f[2], cdtypes, height, always20, 0.0f, 1.0f)
            && stanTestVolume(&tile, to->f[0], to->f[2], width, cdtypes, height, always20) < 0)
        {
            ok = 1;
        }
        else
        {
            stanGetCollisionEdge(edgeStart, edgeEnd);
            orderPointsBySide(edgeStart, edgeEnd, &dir);
        }
    }

    chrSetMoving(self, 1);

    return ok;
}


s32 chrNavTryObstacle(
    ChrRecord *self,
    coord3d *corner,
    s32 left,
    coord3d *othercorner,
    f32 clearance,
    s32 checktarget,
    coord3d *target,
    struct waydata *way,
    f32 overshoot,
    s32 cdtypes,
    s32 set_copy)
{
    PropRecord *self_prop; // -- 124
    coord3d dd; // -- 112
    coord3d pos; // -- 100
    f32 norm; // -- 96
    f32 angle; // 92
    coord3d offset; // 80
    coord3d *edgeStart; // 76
    coord3d *edgeEnd; // 72

    self_prop = self->prop;

    if (left != 0)
    {
        edgeStart = corner;
        edgeEnd = othercorner;
    }
    else
    {
        edgeStart = othercorner;
        edgeEnd = corner;
    }

    dd.f[0] = corner->f[0] - self_prop->pos.f[0];
    dd.f[1] = 0.0f;
    dd.f[2] = corner->f[2] - self_prop->pos.f[2];

    norm = 1.0f / sqrtf((dd.f[0] * dd.f[0]) + (dd.f[2] * dd.f[2]));

    dd.f[0] *= clearance * norm;
    dd.f[2] *= clearance * norm;

    if (clearance * norm > 1.0f)
    {
        angle = DegToRad(45);
    }
    else
    {
        angle = mathAcos(clearance * norm);
    }

    if ((left == 0) && (angle != 0.0f))
    {
        angle = M_TAU_F - angle;
    }

    offset.f[0] = (-cosf(angle) * dd.f[0]) + (sinf(angle) * dd.f[2]);
    offset.f[1] = 0.0f;
    offset.f[2] = (-sinf(angle) * dd.f[0]) - (cosf(angle) * dd.f[2]);

    pos.f[0] = corner->f[0] + offset.f[0];
    pos.f[1] = corner->f[1];
    pos.f[2] = corner->f[2] + offset.f[2];

    if (chrNavCanSeeNextPos(self, &self_prop->pos, self_prop->stan, &pos, edgeStart, edgeEnd, overshoot, self->chrwidth, cdtypes)
        && ((checktarget == 0) || chrCanWalkViaPoint(self, &self_prop->pos, self_prop->stan, &pos, target, NULL, cdtypes)))
    {
        if (set_copy != 0)
        {
            way->gotPosCopy = 1;

            way->pos_copy.f[0] = pos.f[0];
            way->pos_copy.f[1] = pos.f[1];
            way->pos_copy.f[2] = pos.f[2];
        }
        else
        {
            way->gotPos = 1;

            way->pos.f[0] = pos.f[0];
            way->pos.f[1] = pos.f[1];
            way->pos.f[2] = pos.f[2];
        }

        return 1;
    }

    return 0;
}


/**
 * Iterates travel mode. Used by both ACT_GOPOS and ACT_PATROL.
 * 10% chance to open door.
 * Calls apply speed.
 *
 * @see chrTickPatrol
 * @see chrTickGoPos
 * contrast with @see chrGoPosTickMagic
*/
void chrNavTick(ChrRecord *self, coord3d *target, struct waydata *way)
{
    coord3d targetpos; // 260
    coord3d othercorner; // 244
    s32 i; // 240
    f32 clearance; // 232
    struct ObjectRecord *obj;
    f32 atan_pos2_a; // 212
    f32 atan_pos3_a;
    PropRecord *self_prop;
    f32 dx;
    f32 atan_pos2_b; // 196
    f32 atan_pos3_b;
    f32 dy;
    f32 dz;
    f32 atan_pos2_c; // 180
    f32 atan_pos3_c;
    s32 max;
    f32 atan_pos;
    s32 cdtypes;
    PropRecord *door;

    self_prop = self->prop;
    cdtypes = CDTYPE_OBJS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER | CDTYPE_DOORSLOCKEDTOAI;
    if ((self->hidden & CHRHIDDEN_OFFSCREEN_PATROL) != 0)
    {
        cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
    }

    max=1;
    for (i=0; i<1; i++)
    {
        if ((way->mode == WAYMODE_0) || (way->mode == WAYMODE_2))
        {
            targetpos.f[0] = target->f[0];
            targetpos.f[1] = target->f[1];
            targetpos.f[2] = target->f[2];

            if (chrNavCanSeeNextPos(self, &self_prop->pos, self_prop->stan, &targetpos, &way->pos2, &way->pos3, -(self->chrwidth), self->chrwidth, CDTYPE_PATHBLOCKER) != 0)
            {
                way->gotPos = (u8) 1;
                way->pos.f[0] = targetpos.f[0];
                way->pos.f[1] = targetpos.f[1];
                way->pos.f[2] = targetpos.f[2];
                way->mode = WAYMODE_4;
            }
            else
            {
                if (way->mode == WAYMODE_0)
                {
                    way->mode = (s8) WAYMODE_1;
                    way->iter = 0;
                }
                else if (way->mode == WAYMODE_2)
                {
                    way->mode = WAYMODE_3;
                    way->iter = 0;
                }
            }
        }
        else if (way->mode == WAYMODE_1)
        {
            clearance = self->chrwidth * 1.2f * 1.05f;

            if (chrNavTryObstacle(self, &way->pos2, 1, &othercorner, clearance, 1, target, way, 0.0f, 0x10, 0) != 0)
            {
                way->mode = WAYMODE_4;
            }
            else if (chrNavTryObstacle(self, &way->pos3, 0, &othercorner, clearance, 1, target, way, 0.0f, 0x10, 0) != 0)
            {
                way->mode = WAYMODE_4;
            }
            else
            {
                way->iter = way->iter + 1;
                if (way->iter >= MAX_WAYMODE)
                {
                    way->mode = WAYMODE_2;
                }
            }
        }
        else if (way->mode == WAYMODE_3)
        {
            clearance = self->chrwidth * 1.2f * 1.05f;

            if (chrNavTryObstacle(self, &way->pos2, 1, &othercorner, clearance, 0, NULL, way, 0.0f, 0x10, 0) != 0)
            {
                way->mode = WAYMODE_4;
            }
            else if (chrNavTryObstacle(self, &way->pos3, 0, &othercorner, clearance, 0, NULL, way, 0.0f, 0x10, 0) != 0)
            {
                way->mode = WAYMODE_4;
            }
            else
            {
                way->iter = way->iter + 1;
                if (way->iter >= MAX_WAYMODE)
                {
                    way->gotPos = 0;
                    way->gotPosCopy = (u8) (s8) way->gotPos;
                    way->pos_copy.f[0] =
                        way->pos.f[0] = target->f[0];
                    way->pos_copy.f[1] =
                        way->pos.f[1] = target->f[1];
                    way->pos_copy.f[2] =
                        way->pos.f[2] = target->f[2];
                    way->mode = WAYMODE_0;
                }
            }
        }
        else if (way->mode == WAYMODE_4)
        {
            if (chrNavCheckForObstacle(self, &self_prop->pos, self_prop->stan, &way->pos, &way->pos2, &way->pos3, -(self->chrwidth), self->chrwidth, cdtypes) != 0)
            {
                way->gotPosCopy = max;
                way->mode = WAYMODE_0;
                way->pos_copy.f[0] = way->pos.f[0];
                way->pos_copy.f[1] = way->pos.f[1];
                way->pos_copy.f[2] = way->pos.f[2];
            }
            else
            {
                way->mode = WAYMODE_5;
                way->iter = 0;
            }
        }
        else if (way->mode == WAYMODE_5)
        {
            clearance = self->chrwidth * 1.2f * 1.05f;

            atan_pos =       mathAtan2( way->pos.f[0] - self_prop->pos.f[0],  way->pos.f[2] - self_prop->pos.f[2]);
            atan_pos2_a = atan_pos - mathAtan2(way->pos2.f[0] - self_prop->pos.f[0], way->pos2.f[2] - self_prop->pos.f[2]);
            atan_pos3_a = atan_pos - mathAtan2(way->pos3.f[0] - self_prop->pos.f[0], way->pos3.f[2] - self_prop->pos.f[2]);

            if (atan_pos2_a < 0.0f)
            {
                atan_pos2_a = atan_pos2_a + M_TAU_F;
            }

            if (atan_pos2_a >= M_PI_F)
            {
                atan_pos2_a = atan_pos2_a - M_TAU_F;
            }

            if (atan_pos2_a < 0.0f)
            {
                atan_pos2_a = -atan_pos2_a;
            }

            if (atan_pos3_a < 0.0f)
            {
                atan_pos3_a = atan_pos3_a + M_TAU_F;
            }

            if (atan_pos3_a >= M_PI_F)
            {
                atan_pos3_a = atan_pos3_a - M_TAU_F;
            }

            if (atan_pos3_a < 0.0f)
            {
                atan_pos3_a = -atan_pos3_a;
            }

            if (atan_pos2_a < atan_pos3_a)
            {
                if (chrNavTryObstacle(self, &way->pos2, 1, &othercorner, clearance, 0, NULL, way, clearance * 1.1f, cdtypes, 1) != 0)
                {
                    way->mode = WAYMODE_0;
                    break;
                }
                else
                {
                    atan_pos2_b = atan_pos - mathAtan2(way->pos2.f[0] - self_prop->pos.f[0], way->pos2.f[2] - self_prop->pos.f[2]);
                    atan_pos3_b = atan_pos - mathAtan2(othercorner.f[0] - self_prop->pos.f[0], othercorner.f[2] - self_prop->pos.f[2]);

                    if (atan_pos2_b < 0.0f)
                    {
                        atan_pos2_b = atan_pos2_b + M_TAU_F;
                    }
                    if (atan_pos2_b >= M_PI_F)
                    {
                        atan_pos2_b = atan_pos2_b - M_TAU_F;
                    }
                    if (atan_pos2_b < 0.0f)
                    {
                        atan_pos2_b = -atan_pos2_b;
                    }

                    if (atan_pos3_b < 0.0f)
                    {
                        atan_pos3_b = atan_pos3_b + M_TAU_F;
                    }
                    if (atan_pos3_b >= M_PI_F)
                    {
                        atan_pos3_b = atan_pos3_b - M_TAU_F;
                    }
                    if (atan_pos3_b < 0.0f)
                    {
                        atan_pos3_b = -atan_pos3_b;
                    }

                    if ((atan_pos3_b < atan_pos2_b) && (chrNavTryObstacle(self, &othercorner, 0, &othercorner, clearance, 0, NULL, way, clearance * 1.1f, cdtypes, 1) != 0))
                    {
                        way->mode = WAYMODE_0;
                        break;
                    }
                }
            }
            else
            {
                if (chrNavTryObstacle(self,  &way->pos3, 0, &othercorner, clearance, 0, NULL, way, clearance * 1.1f, cdtypes, 1) != 0)
                {
                    way->mode = WAYMODE_0;
                    break;
                }
                else
                {
                    atan_pos2_c = atan_pos - mathAtan2(way->pos3.f[0] - self_prop->pos.f[0], way->pos3.f[2] - self_prop->pos.f[2]);
                    atan_pos3_c = atan_pos - mathAtan2(othercorner.f[0] - self_prop->pos.f[0], othercorner.f[2] - self_prop->pos.f[2]);

                    if (atan_pos2_c < 0.0f)
                    {
                        atan_pos2_c = atan_pos2_c + M_TAU_F;
                    }
                    if (atan_pos2_c >= M_PI_F)
                    {
                        atan_pos2_c = atan_pos2_c - M_TAU_F;
                    }
                    if (atan_pos2_c < 0.0f)
                    {
                        atan_pos2_c = -atan_pos2_c;
                    }

                    if (atan_pos3_c < 0.0f)
                    {
                        atan_pos3_c = atan_pos3_c + M_TAU_F;
                    }
                    if (atan_pos3_c >= M_PI_F)
                    {
                        atan_pos3_c = atan_pos3_c - M_TAU_F;
                    }
                    if (atan_pos3_c < 0.0f)
                    {
                        atan_pos3_c = -atan_pos3_c;
                    }

                    if ((atan_pos3_c < atan_pos2_c) && (chrNavTryObstacle(self, &othercorner, 1, &othercorner, clearance, 0, NULL, way, clearance * 1.1f, cdtypes, 1) != 0))
                    {
                        way->mode = WAYMODE_0;
                        break;
                    }
                }
            }

            way->iter = way->iter + 1;
            if (way->iter >= MAX_WAYMODE)
            {
                way->gotPosCopy = 0;
                way->mode = WAYMODE_0;
            }
        }
    }

    if (way->gotPosCopy == 0)
    {
        way->pos_copy.f[0] = way->pos.f[0];
        way->pos_copy.f[1] = way->pos.f[1];
        way->pos_copy.f[2] = way->pos.f[2];
    }

    if (((s32) way->age % 10) == 0)
    {
        door = stanLineFindDoor(self_prop->stan, self_prop->pos.f[0], self_prop->pos.f[2], way->pos_copy.f[0], way->pos_copy.f[2], 0x5000);

        if (door != NULL)
        {
            obj = door->obj;
            if (!(obj->flags2 & PROPFLAG_DOOR_OPENTOFRONT))
            {
                dx = door->pos.f[0] - self_prop->pos.f[0];
                dy = door->pos.f[1] - self_prop->pos.f[1];
                dz = door->pos.f[2] - self_prop->pos.f[2];

                if (((dx * dx) + (dy * dy) + (dz  * dz )) < 40000.0f)
                {
                    chrGoPosClearRestartTtl(self);
                    doorsChooseSwingDirection(self_prop, door->door);
                    doorActivate(door->door, 1);

                    if (((self->hidden & CHRHIDDEN_OFFSCREEN_PATROL) == 0)
                        && (modelGetAnim(self->model) != ANIM(idle_unarmed))
                        && (modelGetAnim(self->model) != ANIM(idle)))
                    {
                        chrStandChooseAnimation(self, 16.0f);
                        self->lastmoveok60 = g_GlobalTimer;
                    }
                }
                else
                {
                    door = NULL;
                }
            }
            else
            {
                door = NULL;
            }
        }

        if ((door == NULL) || ((self->hidden & CHRHIDDEN_OFFSCREEN_PATROL) != 0))
        {
            if ((modelGetAnim(self->model) == ANIM(idle_unarmed))
                || (modelGetAnim(self->model) == ANIM(idle)))
            {
                if (self->actiontype == ACT_PATROL)
                {
                    chrPatrolChooseAnimation(self);
                }
                else
                {
                    chrGoPosChooseAnimation(self);
                }
            }

            if (door == NULL)
            {
                self->hidden &= ~(CHRHIDDEN_OFFSCREEN_PATROL);
            }
        }
    }

    if (self->actiontype == ACT_PATROL)
    {
        chrApplySpeed(self, &way->pos_copy, 0, &self->act_patrol.speed);
    }
    else
    {
        chrApplySpeed(self, &way->pos_copy, (s32) self->act_gopos.speedMode, &self->act_gopos.speed);

        if (self->act_gopos.speedMode == 2)
        {
            if (self->act_gopos.speed != 0.0f)
            {
                modelSetAnimSpeed(self->model, 0.25f, 0.0f);
            }
            else if (self->chrflags & CHRFLAG_INCREASE_RUNNING_SPEED)
            {
                modelSetAnimSpeed(self->model, 0.65f, 0.0f);
            }
            else
            {
                modelSetAnimSpeed(self->model, 0.5f, 0.0f);
            }
        }
        else if (self->act_gopos.speedMode == 1)
        {
            if (self->act_gopos.speed != 0.0f)
            {
                modelSetAnimSpeed(self->model, 0.4f, 0.0f);
            }
            else
            {
                modelSetAnimSpeed(self->model, 0.5f, 0.0f);
            }
        }
    }
}


void chrTickGoPos(ChrRecord *self)
{
    waypoint *wp;
    coord3d *wp_pos;
    StandTile *wp_stan;
    PropRecord *self_prop;
    s32 restarted;
    coord3d waypos;
    StandTile *waytile;
    coord3d waypos2;
    StandTile *waytile2;
    PadRecord *pad; // 80
    s32 arrived; // 76

    self_prop = self->prop;
    restarted = 0;
    self->act_gopos.waydata.age += 1;
    self->lastwalk60 = g_GlobalTimer;

    if (self->lastmoveok60 < (g_GlobalTimer - CHRLV_LASTMOVEOK60_CHECK))
    {
        chrGoToPos(self, &self->act_gopos.targetpos, self->act_gopos.target, (s32) self->act_gopos.speedMode);
    }

    chrGoPosConsiderRestart(self);

    if ((self->act_gopos.waydata.mode != WAYMODE_MAGIC) && ((self->act_gopos.magicCheck60 + CHRLV_DEFAULT_TIMER) < g_GlobalTimer))
    {
        chrGoPosGetCurWaypointInfo(self, &waypos, &waytile);

        if (chrPathIsOffscreen(self, &waypos, waytile))
        {
            restarted = 1;
            chrSetGoposSegDistTotal(self, &self->act_gopos.waydata, &waypos);
        }
    }

    if (g_SeenBondRecentlyGuardCount >= 0xA)
    {
        chrStandCheckWall(self);

        return;
    }

    if (self->act_gopos.waydata.mode == WAYMODE_MAGIC)
    {
        chrGoPosGetCurWaypointInfo(self, &waypos2, &waytile2);

        if ((restarted == 0)
            && ((self_prop->flags & PROPFLAG_ONSCREEN) || (chrPathIsOffscreen(self, &waypos2, waytile2) == 0)))
        {
            chrGoPosResetMagic(self);
            self->act_gopos.magicCheck60 = g_GlobalTimer;

            return;
        }

        chrGoPosTickMagic(self, &self->act_gopos.waydata, chrGetRunAnimSpeed(self), &waypos2, waytile2);

        return;
    }


    arrived = 0;

    wp = self->act_gopos.waypoints[self->act_gopos.curindex];

    if (wp != NULL)
    {
        if (chrIsArrivingLaterallyAtPos(&self->prevpos, &self_prop->pos, &g_CurrentSetup.pads[wp->padID].pos, 30.0f) != 0)
        {
            arrived = 1;
        }
    }
    else
    {
        if (chrIsArrivingLaterallyAtPos(&self->prevpos, &self_prop->pos, &self->act_gopos.targetpos, 30.0f) != 0)
        {
            chrStandCheckWall(self);

            return;
        }
    }

    if (arrived != 0)
    {
        chrActGoposIncCurIndex(self);
    }

    if (((s32) self->act_gopos.waydata.age % 10) == 5)
    {
        wp = self->act_gopos.waypoints[self->act_gopos.curindex];

        if (wp != NULL)
        {
            wp = self->act_gopos.waypoints[self->act_gopos.curindex + 1];

            if (wp != NULL)
            {
                wp = self->act_gopos.waypoints[self->act_gopos.curindex + 2];

                if (wp != NULL)
                {
                    pad = &g_CurrentSetup.pads[wp->padID];
                    wp_pos = &pad->pos;
                    wp_stan = pad->stan;
                }
                else
                {
                    wp_pos = &self->act_gopos.targetpos;
                    wp_stan = self->act_gopos.target;
                }

                if (chrCanWalkLine(self, &self_prop->pos, self_prop->stan, wp_pos, wp_stan, CDTYPE_PATHBLOCKER)
                    && chrCanWalkLineWithin(self, &self_prop->pos, self_prop->stan, wp_pos, self->chrwidth * 1.2f, CDTYPE_PATHBLOCKER))
                {
                    chrActGoposIncCurIndex(self);
                    chrActGoposIncCurIndex(self);
                }
            }
        }
    }

    if (((s32) self->act_gopos.waydata.age % 10) == 0)
    {
        wp = self->act_gopos.waypoints[self->act_gopos.curindex];

        if (wp != NULL)
        {
            wp = self->act_gopos.waypoints[self->act_gopos.curindex + 1];

            if (wp != NULL)
            {
                pad = &g_CurrentSetup.pads[wp->padID];
                wp_pos = &pad->pos;
                wp_stan = pad->stan;
            }
            else
            {
                wp_pos = &self->act_gopos.targetpos;
                wp_stan = self->act_gopos.target;
            }

            if (chrCanWalkLine(self, &self_prop->pos, self_prop->stan, wp_pos, wp_stan, CDTYPE_PATHBLOCKER)
                && chrCanWalkLineWithin(self, &self_prop->pos, self_prop->stan, wp_pos, self->chrwidth * 1.2f, CDTYPE_PATHBLOCKER))
            {
                chrActGoposIncCurIndex(self);
            }
        }
    }

    wp = self->act_gopos.waypoints[self->act_gopos.curindex];

    if (wp != NULL)
    {
        pad = &g_CurrentSetup.pads[wp->padID];
        wp_pos = &pad->pos;
        wp_stan = pad->stan;
    }
    else
    {
        wp_pos = &self->act_gopos.targetpos;
        wp_stan = self->act_gopos.target;
    }

    chrNavTick(self, wp_pos, &self->act_gopos.waydata);
}


void chrTickPatrol(ChrRecord *self)
{
    PropRecord *self_prop;
    s32 restarted;
    PadRecord *pad;

    self_prop = self->prop;
    pad = (PadRecord *) chrGetNextPatrolStepPad(self);
    restarted = 0;
    self->act_patrol.waydata.age += 1;
    self->lastwalk60 = g_GlobalTimer;

    if ((self->act_patrol.waydata.mode != WAYMODE_MAGIC)
        && ((self->act_patrol.lastvisible60 + CHRLV_DEFAULT_TIMER) < g_GlobalTimer)
        && chrPathToPadIsOffscreen(self, pad))
    {
        restarted = 1;
        chrSetGoposSegDistTotal(self, &self->act_patrol.waydata, &pad->pos);
    }

    if (self->act_patrol.waydata.mode == WAYMODE_MAGIC)
    {
        if ((restarted == 0)
            && ((self_prop->flags & PROPFLAG_ONSCREEN) || (chrPathToPadIsOffscreen(self, pad) == 0)))
        {
            self->act_patrol.lastvisible60 = g_GlobalTimer;
            chrSetNextActPatrolStepPadPos(self);
        }
        else
        {
            chrGoPosTickMagic(self, &self->act_patrol.waydata, g_ChrSpeedWalk, &pad->pos, pad->stan);
        }
    }
    else
    {
        if(1)
        {
            // removed
        }

        if (chrIsArrivingLaterallyAtPos(&self->prevpos, &self_prop->pos, &pad->pos, 30.0f))
        {
            chrAdvancePatrolStep(self);
            pad = (PadRecord *)chrGetNextPatrolStepPad(self);
        }

        chrNavTick(self, &pad->pos, &self->act_patrol.waydata);
    }
}


/**
 * Co-op: how often (in 60ths of a second) a chr that hasn't seen anyone
 * lately looks for the nearest player to fight.
 */
#define COOP_RETARGET_INTERVAL60 30

/**
 * Co-op: how often a chr that is fighting one player checks whether it can
 * see the other player closer by.
 */
#define COOP_SWITCH_INTERVAL60 60

/**
 * Co-op: the other player must be this much closer (as a fraction of the
 * distance to the current target) for a chr to switch to them.
 */
#define COOP_SWITCH_CLOSER 0.75f

static f32 coopChrDistSqToPlayer(ChrRecord *self, s32 player)
{
    PropRecord *prop = g_playerPointers[player]->prop;
    f32 dx = prop->pos.x - self->prop->pos.x;
    f32 dz = prop->pos.z - self->prop->pos.z;

    return (dx * dx) + (dz * dz);
}

/**
 * Co-op: a chr busy fighting one player switches to another living player it
 * can see who is clearly closer, so that a second player walking into a
 * fight gets shot at too.
 */
static s32 coopChrTrySwitchToCloserPlayer(ChrRecord *self, s32 target)
{
    s32 count = getPlayerCount();
    f32 targetdist = coopChrDistSqToPlayer(self, target);
    s32 i;

    for (i = 0; i < count; i++)
    {
        s32 lastsee = self->lastseetarget60;

        if ((i == target) || g_playerPointers[i]->bonddead)
        {
            continue;
        }

        if (coopChrDistSqToPlayer(self, i) >= (targetdist * (COOP_SWITCH_CLOSER * COOP_SWITCH_CLOSER)))
        {
            continue;
        }

        playerSetCurrent(i);

        if (chrCanSeeBond(self))
        {
            // chrCanSeeBond noted the sighting; make it the new target's.
            self->targetplayer = i;
            chrSetTargetToPlayer(self);
            self->lastheartarget60 = 0;
            return i;
        }

        self->lastseetarget60 = lastsee;
    }

    return target;
}

/**
 * Co-op: a chr shot by a player turns on them.
 */
void coopChrShotByCurrentPlayer(ChrRecord *self)
{
    s32 player = playerGetCurrentNum();

    if (!g_CoopMode || g_CounterOpMode || (self->prop == NULL) || (self->prop->type != PROP_TYPE_CHR) || (self->targetplayer == player)
        || g_CurrentPlayer->bonddead)
    {
        return;
    }

    self->targetplayer = player;
    self->lastseetarget60 = 0;
    self->lastheartarget60 = g_GlobalTimer;
}

/**
 * Co-op: pick the player this chr fights and make them the current player
 * for its AI list and action tick, so all of the "Bond" logic (sight,
 * shooting, damage, chasing) applies to that player.
 *
 * A chr keeps a living target. Without one, or when it hasn't seen anyone
 * for a while, it switches to the nearest living player every half second,
 * forgetting when it last saw or heard its old target. Unarmed chrs (allies
 * such as Natalya, civilians) stay with the player they met, so an ally
 * follows whoever rescued her, until that player dies.
 * Seeing or hearing the other player retargets it straight away (see the
 * any-player AI commands in ai()), and so does being shot by them. While it
 * fights one player it also switches to the other if it can see them
 * clearly closer by (checked every second).
 */
static void coopChrSelectTarget(ChrRecord *self)
{
    s32 count = getPlayerCount();
    s32 target = self->targetplayer;
    s32 unarmed = (self->weapons_held[GUNRIGHT] == NULL) && (self->weapons_held[GUNLEFT] == NULL);
    s32 i;

    if ((target < 0) || (target >= count))
    {
        target = 0;
    }

    // A player's own body (cutscene AI lists, death and intro animations)
    // acts for that player.
    if ((self->prop != NULL) && (self->prop->type == PROP_TYPE_VIEWER))
    {
        target = playerGetIndexForProp(self->prop);

        if (target < 0)
        {
            target = 0;
        }
    }
    else if (g_CounterOpMode)
    {
        // Counter-op: every guard, and every level script, is about Bond.
        target = COUNTEROP_BOND;
    }
    else if ((self->targetlock60 > 0) && !g_playerPointers[target]->bonddead)
    {
        // A level-script check picked this player; stay with them while
        // the script carries on (e.g. Dam's bungee jump).
        self->targetlock60 -= g_ClockTimer;
    }
    else if ((self->prop != NULL)
        && (g_playerPointers[target]->bonddead
            || (!unarmed && !chrSawTargetRecently(self) && ((((u32)g_GlobalTimer + self->chrnum) % COOP_RETARGET_INTERVAL60) < (u32)g_ClockTimer))))
    {
        f32 bestdist = 0.0f;
        s32 best = -1;

        for (i = 0; i < count; i++)
        {
            PropRecord *prop = g_playerPointers[i]->prop;
            f32 dx;
            f32 dz;
            f32 dist;

            if (g_playerPointers[i]->bonddead)
            {
                continue;
            }

            dx = prop->pos.x - self->prop->pos.x;
            dz = prop->pos.z - self->prop->pos.z;
            dist = (dx * dx) + (dz * dz);

            if ((best < 0) || (dist < bestdist))
            {
                best = i;
                bestdist = dist;
            }
        }

        if (best >= 0)
        {
            target = best;
        }
    }
    else if ((self->prop != NULL)
        && (self->prop->type == PROP_TYPE_CHR)
        && !unarmed
        && chrSawTargetRecently(self)
        && ((((u32)g_GlobalTimer + self->chrnum) % COOP_SWITCH_INTERVAL60) < (u32)g_ClockTimer))
    {
        target = coopChrTrySwitchToCloserPlayer(self, target);
    }

    if (target != self->targetplayer)
    {
        // What a chr saw or heard was the old target. Without this the new
        // target counts as seen for up to 10 seconds, so guards would know
        // where a respawned player is without looking.
        self->lastseetarget60 = 0;
        self->lastheartarget60 = 0;
    }

    self->targetplayer = target;
    playerSetCurrent(target);
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


s32 chrSawTargetRecently(ChrRecord *self)
{
    if ((self->lastseetarget60 > 0) && ((g_GlobalTimer - self->lastseetarget60) < CHRLV_10_SEC_TIMER))
    {
        return TRUE;
    }
    return FALSE;
}


s32 chrHeardTargetRecently(ChrRecord *self)
{
    if ((self->lastheartarget60 > 0) && ((g_GlobalTimer - self->lastheartarget60) < CHRLV_10_SEC_TIMER))
    {
        return TRUE;
    }
    return FALSE;
}


/**
 * get angle to pos in Radians
*/
f32 chrGetAngleToPos(ChrRecord *self, coord3d *pos)
{
    f32         radToPos;
    f32         radMyHeading;
    PropRecord *myprop;
    f32         anglebetween;

    radMyHeading = modelGetRootRotY(self->model);
    myprop       = self->prop;
    anglebetween = mathAtan2(pos->x - myprop->pos.x, pos->z - myprop->pos.z);
    radToPos     = anglebetween - radMyHeading;

    if (anglebetween < radMyHeading)
    {
        radToPos = radToPos + M_TAU_F;
    }

    return radToPos;
}


f32 chrGetAngleToBond(ChrRecord *self)
{
    return chrGetAngleToPos(self, &currentPlayerGetProp()->pos);
}


/**
 * @param self:
 * @param flags: Lookup mode. 4 == lookup guard. 8 == lookup pad preset. Else, use current player.
 * @param lookup_id: Lookup id for guard or preset.
 * @param stan: Out parameter. Will contain found stan.
 * @returns: Position of found item (depends on lookup mode).
*/
coord3d *chrGetChrOrPresetLocation(ChrRecord *self, s32 flags, s32 lookup_id, StandTile **stan)
{
    ChrRecord *guard;
    PropRecord *player_prop;
    s32 padid;
    PadRecord *preset_pad;

    if ((flags & 4) != 0)
    {
        guard = chrFindById(self, lookup_id);

        if ((guard == 0) || (guard->prop == 0))
        {
            guard = self;
        }

        *stan = (StandTile *) self->prop->stan;

        return &guard->prop->pos;
    }

    if ((flags & 8) != 0)
    {
        padid = chrResolvePadId(self, lookup_id);

        if (isNotBoundPad(padid))
        {
            preset_pad = &g_CurrentSetup.pads[padid];
        }
        else
        {
            preset_pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padid)];
        }

        *stan = (StandTile *) preset_pad->stan;

        return &preset_pad->pos;
    }

    player_prop = currentPlayerGetProp();
    *stan = (StandTile *) player_prop->stan;

    return &player_prop->pos;
}


f32 chrGetAngleFromBond(ChrRecord *self)
{
    f32 radBondHeading   = bondviewGetPlayerYawRadians();
    PropRecord *myprop   = self->prop;
    PropRecord *bondprop = currentPlayerGetProp();
    f32 anglebetween     = mathAtan2(myprop->pos.x - bondprop->pos.x, myprop->pos.z - bondprop->pos.z);
    f32 radFromBond      = anglebetween - radBondHeading;

    if (anglebetween < radBondHeading)
    {
        radFromBond = radFromBond + M_TAU_F;
    }

    return radFromBond;
}


f32 chrGetDistanceToBond(ChrRecord *guardData)
{
    PropRecord *guardPosData;
    PropRecord *playerPosData;
    float xDiff;
    float yDiff;
    float zDiff;

    guardPosData = guardData->prop;
    playerPosData = currentPlayerGetProp();
    xDiff = playerPosData->pos.x - guardPosData->pos.x;
    yDiff = playerPosData->pos.y - guardPosData->pos.y;
    zDiff = playerPosData->pos.z - guardPosData->pos.z;

    return sqrtf(SQR(xDiff) + SQR(yDiff) + SQR(zDiff));
}


f32 chrGetDistanceToPad(ChrRecord *self, s32 padID)
{
    PropRecord *myprop;
    PadRecord *pad;

    myprop = self->prop;
    padID  = chrResolvePadId(self, padID);

    if (isNotBoundPad(padID))
    {
        pad = (PadRecord *)&g_CurrentSetup.pads[padID];
    }
    else
    {
        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padID)];
    }

    return sqrtf(
        SQR(pad->pos.x - myprop->pos.x) +
        SQR(pad->pos.y - myprop->pos.y) +
        SQR(pad->pos.z - myprop->pos.z));
}


bool chrPadRoomIsVisible(ChrRecord *self, s32 padnum)
{
    PadRecord *pad;
    StandTile *padstan;

    padnum = chrResolvePadId(self, padnum);

    if (isNotBoundPad(padnum))
    {
        pad = (PadRecord *)&g_CurrentSetup.pads[padnum];
    }
    else
    {
        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
    }

    padstan = pad->stan;

    if (padstan)
    {
        return bgRoomIsVisible(stanGetTileRoom(padstan));
    }

    return FALSE;
}


s32 chrResolvePadId(ChrRecord *guardData,s32 padNo)
{
    // Guard's target pad.
    if (padNo == PAD_PRESET1)
    {
        padNo = (s32)guardData->padpreset1;
    }

    return padNo;
}


s32 chrResolveId(ChrRecord *self, s32 id)
{
    if (id == (u8)CHR_SEE_SHOT)
    {
        id = self->chrseeshot;
    }
    else if (id == (u8)CHR_SEE_DIE)
    {
        id = self->chrseedie;
    }
    else if (id == (u8)CHR_PRESET)
    {
        id = self->chrpreset1;
    }
    else if (id == (u8)CHR_SELF)
    {
        id = self->chrnum;
    }
    else if (id == (u8)CHR_CLONE)
    {
        id = self->chrnum + 0x2710;
    }
    else if (id == (u8)CHR_BOND_CINEMA)
    {
        if (g_CurrentPlayer->prop->chr)
        {
            id = g_CurrentPlayer->prop->chr->chrnum;
        }
    }

    return id;
}


/**
 * chrFindById
*/
ChrRecord *chrFindById(ChrRecord *self, s32 guard_id)
{
    s32 i;
    ChrRecord* guard;

    guard_id = chrResolveId(self, guard_id);
    guard = chrFindByLiteralId(guard_id);

    if (guard == NULL)
    {
        for (i=0; i<g_NumBgChrs; i++)
        {
            if (guard_id == g_BgChrs[i].chrnum)
            {
                guard = &g_BgChrs[i];
                break;
            }
        }
    }

    return guard;
}


f32 chrGetDistanceToChr(ChrRecord *self, s32 chrID)
{
    PropRecord *myprop;
    ChrRecord  *chr;
    f32         distance;

    myprop   = self->prop;
    chr      = chrFindById(self, chrID);
    distance = 0.0f;

    if (chr && chr->model && chr->prop)
    {
        distance = sqrtf(
            SQR(chr->prop->pos.x - myprop->pos.x) +
            SQR(chr->prop->pos.y - myprop->pos.y) +
            SQR(chr->prop->pos.z - myprop->pos.z));
    }

    return distance;
}


f32 chrGetDistanceFromBondToPad(ChrRecord *self, s32 padid)
{
    PropRecord *bondprop;
    PadRecord *pad;

    bondprop = currentPlayerGetProp();
    padid    = chrResolvePadId(self, padid);

    if (isNotBoundPad(padid))
    {
        pad = (PadRecord *)&g_CurrentSetup.pads[padid];
    }
    else
    {
        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padid)];
    }

    return sqrtf(
        SQR(pad->pos.x - bondprop->pos.x) +
        SQR(pad->pos.y - bondprop->pos.y) +
        SQR(pad->pos.z - bondprop->pos.z));
}


/**
 * The property is named "flags2".
*/
void chrSetFlags2(ChrRecord *self, u8 flags2)
{
    self->flags2 |= flags2;
}


/**
 * The property is named "flags2".
*/
void chrUnsetFlags2(ChrRecord *self, u8 flags2)
{
    self->flags2 &= ~flags2;
}


/**
 * The property is named "flags2".
*/
s32 chrHasFlags2(ChrRecord *self, u8 flags2)
{
    return (self->flags2 & flags2) != 0;
}


/**
 * The property is named "flags2".
*/
void chrSetFlags2ById(ChrRecord *self, s32 chrNum, u8 flags2)
{
    ChrRecord *chr;

    chr = chrFindById(self, chrNum);

    if (chr != NULL)
    {
        chrSetFlags2(chr, flags2);
    }
}


/**
 * The property is named "flags2".
*/
void chrUnsetFlags2ById(ChrRecord *self, s32 chrNum, u8 flags2)
{
    ChrRecord *chr;

    chr = chrFindById(self, chrNum);

    if (chr != NULL)
    {
        chrUnsetFlags2(chr, flags2);
    }
}


/**
 * The property is named "flags2".
*/
bool chrHasFlags2ById(ChrRecord *self, s32 chrNum, u8 flags2)
{
    ChrRecord *chr;

    chr = chrFindById(self, chrNum);

    if (chr != NULL)
    {
        return chrHasFlags2(chr, flags2);
    }

    return FALSE;
}


void chrSetStageFlags(s32 flags)
{
    objectiveregisters1 |= flags;
}


void chrUnsetStageFlags(u32 flags)
{
    objectiveregisters1 = ~flags & objectiveregisters1; //shorthand does not match
}


bool chrHasStageFlag(s32 flags)
{
    return (objectiveregisters1 & flags) != 0;
}


bool chrIsHearingBond(ChrRecord *self)
{
    return (self->hidden & CHRHIDDEN_ALERT_GUARD_RELATED) != 0;
}


bool chrTrySurrender(ChrRecord *self)
{
    if (chrIsNotDeadOrShot(self))
    {
        chrActorThrowWeaponSurrender(self);

        return TRUE;
    }

    return FALSE;
}


bool chrFadeOut(ChrRecord *self)
{
    chrActorFadeAway(self);

    return TRUE;
}


void chrRestartTimer(ChrRecord *self)
{
    self->timer60 = 0;
    self->hidden |= CHRHIDDEN_TIMER_ACTIVE;
}


f32 chrGetTimer(ChrRecord *self)
{
    return self->timer60 / CHRLV_FRAMERATE_F;
}


bool chrIsTargetedByPlayer(ChrRecord *self)
{
    Model  *mymodel;
    coord3d zeropos;
    coord3d pos;
    coord3d vec;
    f32     scale;

    if (chrHasLineOfSightToPlayer(self))
    {
        mymodel = self->model;
        scale   = modelGetRadius(mymodel) * 0.8f;
        gunGetAimRay(&zeropos, &pos);
        modelGetRootPosition(mymodel, &vec);
        mtx4TransformVecInPlace(camGetWorldToScreenMtxf(), &vec);

        if (projectileTestPropBoundingSphere(&zeropos, &pos, &vec, scale))
        {
            return TRUE;
        }
    }

    return FALSE;
}


bool chrIfNearMiss(ChrRecord *self)
{
    return (self->chrflags & CHRFLAG_NEAR_MISS) != 0;
}


bool chrGoToBond(ChrRecord *self, SPEED speed)
{
    PropRecord *bondprop;

    if (chrIsNotDeadOrShot(self) && (g_SeenBondRecentlyGuardCount < 10))
    {
        bondprop = currentPlayerGetProp();

        if (chrGoToPos(self, &bondprop->pos, bondprop->stan, speed))
        {
            return TRUE;
        }
    }

    return FALSE;
}


/**
 * Address 0x7F03350C0.
*/
bool chrGoToChr(ChrRecord *self, s32 chrid, SPEED speed)
{
    ChrRecord *chr;
    PropRecord *chrprop;

    if (chrIsNotDeadOrShot(self) && (g_SeenBondRecentlyGuardCount < 10))
    {
        chr = chrFindById(self, chrid);
        if (chr && chr->model && chr->prop)
        {
            chrprop = chr->prop;

            if (chrGoToPos(self, &chrprop->pos, chrprop->stan, speed))
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}


/**
 * Return number of hits.
 *
 * PD: chrGetNumArghs
 */
s8 chrGetNumArghs(ChrRecord *self)
{
    return self->numarghs;
}


/**
 * Return number of near misses
 *
 * PD: chrGetNumCloseArghs
 */
s8 chrGetNumCloseArghs(ChrRecord *self)
{
    return self->numclosearghs;
}


/**
 * Return false if chrseeshot is negative.
 */
bool chrSawInjury(ChrRecord *self)
{
    return ((self->chrseeshot < 0) ^ 1);
}


/**
 * Return false if chrseedie is negative.
 */
bool chrSawDeath(ChrRecord *self)
{
    return ((self->chrseedie < 0) ^ 1);
}


bool chraiStopAnimation(ChrRecord *self)
{
    if (chrIsNotDeadOrShot(self))
    {
        chrStandCheckWall(self);

        return TRUE;
    }

    return FALSE;
}


bool chrTrySurprisedOneHand(ChrRecord *self)
{
    if (chrIsNotDeadOrShot(self))
    {
        chrActorShuffleFeet(self);

        return TRUE;
    }

    return FALSE;
}


bool chrTrySurprisedSurrender(ChrRecord *self)
{
    if (chrIsNotDeadOrShot(self))
    {
        chrStartSurprised(self);

        return TRUE;

    }
    return FALSE;
}


bool chrTrySurprisedLookAround(ChrRecord *self)
{
    if (chrIsNotDeadOrShot(self))
    {
        chrActorLookFlustered(self);

        return TRUE;
    }

    return FALSE;
}


bool chrTryKneel(ChrRecord *self)
{
    if (chrIsNotDeadOrShot(self))
    {
        chrKneelChooseAnimation(self);

        return TRUE;
    }

    return FALSE;
}


s32 chrTryStartAnim(ChrRecord *self, s32 animID, s32 startframe, s32 endframe, u8 bitfield, s32 interpol_time60)
{
    if (chrIsNotDeadOrShot(self))
    {
        chrStartAnim(self, animID, startframe, endframe, bitfield, interpol_time60);

        return TRUE;
    }

    return FALSE;
}


/**
 * PD: chrCanHearAlarm
*/
bool chrCanHearAlarm(void)
{
    /*
     possibly this was to be more advanced than simply
     a stub to alarmIsActive.
     It could have for example done a room check
     since this has a "self" reference.
     */
    return alarmIsActive();
}


s32 waypointIsWithin90DegreesOfPosAngle(waypoint *wp, coord3d *pos, f32 angle)
{
    f32 padangle;
    PadRecord *pad;
    f32 dx;
    f32 dz;
    f32 ff;

    pad = &g_CurrentSetup.pads[wp->padID];
    dx = pad->pos.f[0] - pos->f[0];
    dz = pad->pos.f[2] - pos->f[2];

    padangle = mathAtan2(dx, dz);
    ff = angle - padangle;

    if (angle < padangle)
    {
        ff += M_TAU_F;
    }

    if ((ff < DegToRad(90)) || (ff > DegToRad(270)))
    {
        return 1;
    }

    return 0;
}


/**
 * Attempt to find a waypoint near pos which is in a particular quadrant to pos,
 * then return its padnum.
 *
 * For example, pos is typically the player's position, angle is the direction
 * the player is facing, and quadrant is which quadrant (front/back/left/right)
 * that is desired relative to the player's position and angle.
 *
 * The function starts by finding the closest waypoint to the pos. If it's not
 * in the quadrant then its neighouring waypoints are checked too. If none of
 * those are in the quadrant then no further checks are made and the function
 * returns -1.
 *
 * PD: chrFindWaypointWithinPosQuadrant
*/
s32 waypointFindInQuadrant(coord3d *bondpos, StandTile *stan, f32 rot, u8 quadrant)
{
    waypoint *waypoint;
    s32 path_id;
    s32 neighbor_index;

    waypoint = waypointFindNearPos(bondpos, stan);

    if (waypoint)
    {
        switch (quadrant)
        {
            case QUADRANT_BACK:
                rot = rot + DegToRad(180);
                break;

            case QUADRANT_SIDE1:
                rot = rot + DegToRad(90);
                break;

            case QUADRANT_SIDE2:
                rot = rot + DegToRad(270);
                break;

            case QUADRANT_FRONT:
                break;
        }

        if (rot >= M_TAU_F)
        {
            rot = rot - M_TAU_F;
        }

        if (waypointIsWithin90DegreesOfPosAngle(waypoint, bondpos, rot))
        {
            return waypoint->padID;
        }

        for (
            neighbor_index = 0, path_id = waypoint->neighbours[neighbor_index];
            path_id>=0;
            neighbor_index++, path_id = waypoint->neighbours[neighbor_index]
            )
        {
            if (waypointIsWithin90DegreesOfPosAngle(&g_CurrentSetup.pathwaypoints[path_id], bondpos, rot) != 0)
            {
                return g_CurrentSetup.pathwaypoints[path_id].padID;
            }
        }
    }

    return -1;
}


bool chrSetPadPresetToWaypointInTargetQuadrant(ChrRecord *self, u8 quadrant)
{
    PropRecord *myprop;
    PropRecord *bondprop;

    waypoint *myclosestwaypoint;
    waypoint *bondsclosestwaypoint;

    waypoint *route[PATH_FINDING_WP_LIMIT];

    if ((quadrant == QUADRANT_2NDWPTOTARGET) || (quadrant == QUADRANT_20))
    {
        myprop               = self->prop;
        bondprop             = currentPlayerGetProp();
        myclosestwaypoint    = waypointFindNearPos(&myprop->pos, myprop->stan);
        bondsclosestwaypoint = waypointFindNearPos(&bondprop->pos, bondprop->stan);

        if (myclosestwaypoint != NULL && bondsclosestwaypoint != NULL)
        {
            if (quadrant == QUADRANT_2NDWPTOTARGET)
            {
                if (waypointFindRoute(myclosestwaypoint, bondsclosestwaypoint, (waypoint **)&route, PATH_FINDING_WP_LIMIT) >= PATH_FINDING_WP_LIMIT)
                {
                    self->padpreset1 = route[1]->padID;

                    return TRUE;
                }
            }
            else
            {
                myclosestwaypoint = waypointFindNextStepToward(myclosestwaypoint, bondsclosestwaypoint);
                if (myclosestwaypoint != NULL)
                {
                    self->padpreset1 = myclosestwaypoint->padID;

                    return TRUE;
                }
            }
        }
    }
    else
    {
        s32 closestpadid = waypointFindInQuadrant(&self->prop->pos, self->prop->stan, modelGetRootRotY(self->model), quadrant);

        if (closestpadid >= 0)
        {
            self->padpreset1 = closestpadid;

            return TRUE;
        }
    }

    return FALSE;
}


bool chrSetPadPresetToWaypointInQuadrant(ChrRecord *self, u8 padnum)
{
    f32 yaw;
    s32 bondnearestpad;
    PropRecord *bondprop;

    if ((padnum == 16) || (padnum == 32))
    {
        return chrSetPadPresetToWaypointInTargetQuadrant(self, padnum);
    }

    yaw           = bondviewGetPlayerYawRadians();
    bondprop       = currentPlayerGetProp();
    bondnearestpad = waypointFindInQuadrant(&bondprop->pos, bondprop->stan, yaw, padnum);

    if (bondnearestpad >= 0)
    {
        self->padpreset1 = bondnearestpad;

        return TRUE;
    }

    return FALSE;
}


/**
 * PD: chrSetChrPresetToChrNearPos
*/
bool chrSetChrPresetToNearbyChr(ChrRecord *self, f32 distance)
{
    PropRecord *myprop;
    ChrRecord *chr;
    s32 numguards;
    coord3d distneg;
    coord3d distplus;
    s32 myroom;
    s32 i;

    numguards = chrsGetNumSlots();
    myprop    = self->prop;
    myroom    = myprop->stan->room;

    distneg.x  = myprop->pos.x - distance;
    distplus.x = myprop->pos.x + distance;
    distneg.y  = myprop->pos.y - distance;
    distplus.y = myprop->pos.y + distance;
    distneg.z  = myprop->pos.z - distance;
    distplus.z = myprop->pos.z + distance;

    for (i = 0; i < numguards; i++)
    {
        chr = &g_ChrSlots[i];

        /* In co-op the player bodies are chrs too: an ally like Natalya must not pick them as a target. */
        if ((chr != self) && chr->model && !chrIsDead(chr) && chr->prop && (chr->prop->type != PROP_TYPE_VIEWER))
        {
            coord3d *pos = &chr->prop->pos;

            if (
                (pos->x >= distneg.x)  &&
                (pos->x <= distplus.x) &&
                (pos->y >= distneg.y)  &&
                (pos->y <= distplus.y) &&
                (pos->z >= distneg.z)  &&
                (pos->z <= distplus.z) &&
                ((chr->prop->stan->room == myroom) || bgRoomsSharePortal(myroom, chr->prop->stan->room)))
            {
                self->chrpreset1 = chr->chrnum;

                return TRUE;
            }
        }
    }
    return FALSE;
}


void chrSetChrPreset(ChrRecord *self, s32 id)
{
    self->chrpreset1 = chrResolveId(self, id);
}


void chrSetChrPresetOf(ChrRecord *self, s32 id, s32 id2)
{
    ChrRecord *chr;

    chr = chrFindById(self, id);

    if (chr)
    {
        chr->chrpreset1 = chrResolveId(self, id2);
    }
}


void chrSetPadPreset( ChrRecord *self, s32 padid)
{
    self->padpreset1 = chrResolvePadId(self, padid);
}


void chrSetPadPresetByChrnum(ChrRecord *self, s32 chrid, s32 padid)
{
    ChrRecord *chr = chrFindById(self, chrid);

    if (chr)
    {
        chr->padpreset1 = chrResolvePadId(self, padid);
    }
}


s32 chrIsTargetNearlyInSight(ChrRecord *self)
{
    PropRecord *player_prop;
    PropRecord *self_prop;
    StandTile *stan;
    coord3d edgeStart;
    coord3d edgeEnd;

    player_prop = currentPlayerGetProp();
    self_prop   = self->prop;
    stan        = self_prop->stan;

    stanResetHits();

    if (stanWalkLineNoCallback(&stan, self_prop->pos.x, self_prop->pos.z, player_prop->pos.x, player_prop->pos.z))
    {
        return FALSE;
    }
    else
    {
        stanGetCollisionEdge(&edgeStart, &edgeEnd); //extreme edges of stan tile

        if (
            chrCanWalkViaPoint(self, &self_prop->pos, self_prop->stan, &edgeStart, &player_prop->pos, player_prop->stan, 0)
            || chrCanWalkViaPoint(self, &self_prop->pos, self_prop->stan, &edgeEnd, &player_prop->pos, player_prop->stan, 0))
        {
            return TRUE;
        }
    }

    return FALSE;
}


/**
 * PD: chrIsPosOffScreen
*/
s32 chrIsPosOffScreen(coord3d *pos, StandTile *tile)
{
    bool offscreen;
    bbox2d box;

    offscreen = TRUE;

    if (bgRoomIsVisible(stanGetTileRoom(tile)) && fogPositionIsVisibleThroughFog(pos, 0.0f))
    {
        if (bgGet2dBboxByRoomId(stanGetTileRoom(tile), &box))
        {
            offscreen = camIsPosInScreenBox(pos, 200.0f, &box) == 0;
        }
        else
        {
            offscreen = camIsPosInScreen(pos, 200.0f) == 0;
        }
    }

    return offscreen;
}


/**
 * PD: chrAdjustPosForSpawn
*/
bool chrAdjustPosForSpawn(coord3d *pos, StandTile **tile, f32 facing, bool allowonscreen)
{
    coord3d testpos;
    StandTile *s;
    s32 i;
    StandTile **spp;

    s = *tile;
    spp = &s;

    if ((stanTestVolume(spp, pos->f[0], pos->z, 20.0f, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f) < 0) &&
        (allowonscreen || chrIsPosOffScreen(pos, *tile)))
    {
        return TRUE;
    }

    for (i = 0; i < 8; i++)
    {
        testpos.f[0] = pos->f[0] + (sinf(facing) * 60.0f);
        testpos.f[1] = pos->f[1];
        testpos.f[2] = pos->f[2] + (cosf(facing) * 60.0f);

        s = *tile;

        if (stanTestLineUnobstructed(spp, pos->f[0], pos->f[2], testpos.f[0], testpos.f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f, 0.0f, 1.0f)
            && (stanTestVolume(spp, testpos.f[0], testpos.f[2], 20.0f, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f) < 0)
            && (allowonscreen || chrIsPosOffScreen(&testpos, s)))
        {
            *tile = s;

            pos->f[0] = testpos.f[0]; //send back upstream
            pos->f[2] = testpos.f[2];

            return TRUE;
        }

        facing += 0.7853982f;

        if (facing >= M_TAU_F) //clamp to 1 revolution
        {
            facing -= M_TAU_F;
        }
    }

    return FALSE;
}


/**
 * PD: chrSpawnAtCoord
*/
PropRecord *chrSpawnAtCoord(s32 bodynum, s32 headnum, coord3d *pos, StandTile *stan, f32 angle, AIRecord *ailist, s32 spawnflags)
{
    PropRecord *chrprop;
    coord3d newpos; //struct copy here would have been more efficient
    ChrRecord *chr;
    StandTile *stancopy;
    Model *chrHeader;

    if (chrGetNumFree() >= 3)
    {
        if (headnum < 0)
        {
            headnum = bodyChooseHead(bodynum);
        }

        newpos.x = pos->x;
        newpos.y = pos->y;
        newpos.z = pos->z;
        stancopy = stan;

        if (chrAdjustPosForSpawn(&newpos, &stancopy, angle, ((spawnflags & 0x10) != 0)))
        {
            chrHeader = bodyCreateModelWithFlags(bodynum, headnum, spawnflags);

            if (chrHeader != NULL)
            {
                chrprop = chrAllocate(chrHeader, &newpos, angle, stancopy, ailist);

                if (chrprop != NULL)
                {
                    propActivateThisFrame(chrprop);
                    propEnable(chrprop);
                    chr          = chrprop->chr;
                    chr->headnum = headnum;
                    chr->bodynum = bodynum;

                    return chrprop;
                }
            }
        }
    }

    return NULL;
}


PropRecord *chrSpawnAtPad(ChrRecord *self, s32 bodynum, s32 headnum, s32 padid, AIRecord *ailist, s32 flags)
{
    PadRecord *pad;
    padid = chrResolvePadId(self, padid);

    if (isNotBoundPad(padid))
    {
        pad = (PadRecord *)&g_CurrentSetup.pads[padid];
    }
    else
    {
        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padid)];
    }
    //<- not here...
    return chrSpawnAtCoord(bodynum, headnum, &pad->pos, pad->stan, mathAtan2(pad->look.f[0], pad->look.f[2]), ailist, flags);
}


PropRecord *chrSpawnAtChr(ChrRecord *self, s32 bodynum, s32 headnum, s32 chrnum, AIRecord *ailist, s32 flags)
{
    ChrRecord *chr;
    chr = chrFindById(self, chrnum);

    if (!(chr->chrflags & CHRFLAG_HAS_BEEN_ON_SCREEN))
    {
        f32 chrRadHeading   = modelGetRootRotY(chr->model);
        PropRecord *chrprop = chr->prop;

        return chrSpawnAtCoord(bodynum, headnum, &chrprop->pos, chrprop->stan, chrRadHeading, ailist, flags);
    }

    return NULL;
}


bool chrIfInPadRoom(ChrRecord *self, s32 chrnum, s32 padnum)
{
    PadRecord *pad;
    ChrRecord *chr;

    chr    = chrFindById(self, chrnum);
    padnum = chrResolvePadId(self, padnum);

    if (isNotBoundPad(padnum))
    {
        pad = (PadRecord *)&g_CurrentSetup.pads[padnum];
    }
    else
    {
        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
    }

    if (pad->stan && chr)
    {
        if (chr->prop && (pad->stan->room == chr->prop->stan->room))
        {
            return TRUE;
        }
    }

    return FALSE;
}


bool chrPlayerIsInPadRoom(ChrRecord *self, s32 padnum)
{
    PropRecord *bondprop;
    PadRecord  *pad;

    bondprop = currentPlayerGetProp();
    padnum   = chrResolvePadId(self, padnum);

    if (isNotBoundPad(padnum))
    {
        pad = (PadRecord *)&g_CurrentSetup.pads[padnum];
    }
    else
    {
        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
    }

    if (pad->stan && (pad->stan->room == bondprop->stan->room))
    {
        return TRUE;
    }

    return FALSE;
}


bool chrRemovedAiCommand27(void)
{
    return FALSE;
}


bool chrRemovedAiCommand2B(void)
{
    return FALSE;
}


bool chrTryStartAlarm(ChrRecord *self, s32 PadId)
{
    ObjectRecord *objinst;

    PadId = chrResolvePadId(self, PadId);

    if (chrIsNotDeadOrShot(self))
    {
        objinst = objFindAtPad(PadId);

        if (objinst && objIsHealthy(objinst))
        {
            chrStartAlarmChooseAnimation(self);

            return TRUE;
        }
    }

    return FALSE;
}


bool chrTryThrowGrenade(ChrRecord *self)
{
    PropRecord *Left;
    PropRecord *Right;

    PropRecord      *NewGrenadeProp;
    WeaponObjRecord *NewGrenadeObj;
    WeaponObjRecord *LeftWep;
    WeaponObjRecord *RightWep;

    s32 flags;
    //GUNHAND hand;
    //"grenade prob: no chr number %d for obj number %d!\n"
    if (((u32)randomGetNext() % (u32)0xFF) >= self->grenadeprob)
    {
        return FALSE;
    }

    if (chrGetDistanceToBond(self) < 10.0f)
    {
        return FALSE;
    }

    if (chrIsNotDeadOrShot(self))
    {
        Left  = chrGetEquippedWeaponProp(self, GUNLEFT);
        Right = chrGetEquippedWeaponProp(self, GUNRIGHT);

        if (Right && (RightWep = Right->weapon, RightWep->weaponnum == ITEM_GRENADE))
        {
            chrThrowGrenade(self, GUNRIGHT, 0);

            return TRUE;
        }

        if (Left && (LeftWep = Left->weapon, LeftWep->weaponnum == ITEM_GRENADE))
        {
            chrThrowGrenade(self, GUNLEFT, 0);

            return TRUE;
        }

        if (!Left || !Right)
        {
            flags = 0;

            if (Right)
            {
                flags = 0x10000000;
            }

            NewGrenadeProp = chrGiveWeapon(self, 0xC4, ITEM_GRENADE, flags);

            if (NewGrenadeProp)
            {
                NewGrenadeObj = NewGrenadeProp->weapon;
                NewGrenadeObj->runtime_bitflags |= 0x800; //manual bitflags are more effecient

                chrThrowGrenade(self, !Right ? GUNRIGHT : GUNLEFT, 1);

                return TRUE;
            }
        }
    }

    return FALSE;
}


/**
 * chrDropItem
*/
bool chrDropItem(ChrRecord *self, s32 modelnum, u8 weaponid)
{
    WeaponObjRecord *NewModel = weaponCreateFromModel(modelnum, weaponid);

    if (NewModel && NewModel->prop)
    {
        modelSetScale(NewModel->model, NewModel->model->scale);
        propReparent(NewModel->prop, self->prop);
        NewModel->timer = CHRLV_DEFAULT_TIMER;
        propobjSetDropped(NewModel->prop, 1);
        self->hidden = self->hidden | CHRHIDDEN_DROP_HELD_ITEMS;

        return TRUE;
    }

    return FALSE;
}
