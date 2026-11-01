/**
 * Guards attacking: starting an attack standing, kneeling, walking or rolling
 * (chrAttackStand, chrInitActAttackWalk...), turning and aiming at the target
 * (chrSetSubroty, chrUpdateAimendsideback), and firing: muzzle flash, gunfire sound,
 * whether the shot hits the player (chrUpdateShotbondsum) and the bullet itself
 * (chrFireHand). The attack ticks (chrTickAttack...) run from chrActionTick.
 * See docs/subsystems/guards.md.
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


static void chrInitActAttack(ChrRecord *self, struct anim_group_info **animgroups, s32 flip, point2d *hasGun, s32 attack_type, s32 entityid, s32 standing);
static void chrPlayGunfireSound(ChrRecord *self, s32 hand, s32 ratelimit);
static f32 chrGetAimUpAngle(ChrRecord *self);
static s32 chrUpdateAimendsideback(ChrRecord *self, struct weapon_firing_animation_table *anim, s32 leftgun, s32 rightgun, f32 inaccuracy);
static void chrResetAimend(ChrRecord *self);
static void chrSetHandFiring(ChrRecord *self, s32 hand, s32 firing);
static void chrUpdateShotbondsum(ChrRecord *self, s32 *inSight, s32 *hitBond, ITEM_IDS item);
static void chrFireHand(ChrRecord *self, s32 hand);
static s32 chrAttackRollToKneel(ChrRecord *self);
static void chrAttackRollResumeAfterShot(ChrRecord *self);
static void chrTickAttackCommon(ChrRecord *self);

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


/**
 * The angle from the way the guard faces to what it attacks or faces (an ATTACKTYPE),
 * 0 to 2 pi: 0 for straight ahead, for a compass direction the difference to it.
 */
f32 chrGetAngleToTarget(ChrRecord *self, s32 attack_type, s32 entityid)
{
    f32 ret;
    StandTile *stan;

    if (attack_type & ATTACKTYPE_FORWARD)
    {
        return 0.0f;
    }

    if (attack_type & ATTACKTYPE_DIRECTION)
    {
        ret = ((f32)entityid * M_TAU_F) / M_U16_MAX_VALUE_F;

        ret -= modelGetRootRotY(self->model);

        if (ret < 0.0f)
        {
            ret += M_TAU_F;
        }

        return ret;
    }

    return chrGetAngleToPos(self, chrGetChrOrPresetLocation(self, attack_type, entityid, &stan));
}


/**
 * Starts ACT_ATTACK: picks a firing animation from animgroups (32 of them by the angle to
 * the target, mirrored when flip is set), and how many shots to fire with which guns.
 *
 * @param animgroups: e.g. ptr_pistol_firing_animation_groups
 * @param hasGun: which hands fire
 * @param standing: 1 standing, 0 kneeling
 */
static void chrInitActAttack(ChrRecord *self, struct anim_group_info **animgroups, s32 flip, point2d *hasGun, s32 attack_type, s32 entityid, s32 standing)
{
    Model *self_model;
    s32 oneBurst;
    s32 next_anim;
    struct weapon_firing_animation_table *panim_float;
    s32 recoil;
    f32 angle;
    s32 anim_index;
    WeaponObjRecord *weapon;
    point2d singleShot;
    point2d isLauncher;
    s32 i;

    self_model = self->model;
    singleShot = (point2d){0};
    isLauncher = (point2d){0};
    self->actiontype = ACT_ATTACK;
    recoil = 1;
    oneBurst = 0;

    angle = chrGetAngleToTarget(self, attack_type, entityid);

    if (flip != 0)
    {
        anim_index = (s32)((((M_TAU_F - angle) * 32.0f) / M_TAU_F) + 0.5f);
    }
    else
    {
        anim_index = (s32)(((angle * 32.0f) / M_TAU_F) + 0.5f);
    }

    if (anim_index >= 32)
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

    for (i = 0; i < 2; i++)
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

    modelSetAnimation(self_model, panim_float->anim.anim, flip, panim_float->start_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);

    chrAttackSetEndFrame(self);
}


/**
 * Starts an attack from the given animation tables: two guns fire both at once (doubles)
 * two times in three, otherwise one gun fires (pistol), and a single two-handed gun
 * uses the rifle animations.
 */
static void chrAttackWithAnims(ChrRecord *self, s32 attack_type, s32 entityid, s32 standing,
    struct anim_group_info **pistol, struct anim_group_info **doubles, struct anim_group_info **rifle)
{
    PropRecord *left;
    PropRecord *right;
    s32 flip;
    struct anim_group_info **animgroups;
    point2d hasGun;
    PropRecord *leftgun;
    PropRecord *rightgun;

    left = chrGetEquippedWeaponProp(self, GUNLEFT);
    right = chrGetEquippedWeaponProp(self, GUNRIGHT);

    hasGun = (point2d){0};

    if ((left != NULL) && (right != NULL))
    {
        leftgun = chrGetEquippedWeaponPropWithCheck(self, GUNLEFT);
        rightgun = chrGetEquippedWeaponPropWithCheck(self, GUNRIGHT);

        if ((leftgun != NULL) && (rightgun != NULL))
        {
            flip = (u32)randomGetNext() & 1u;

            if (((u32)randomGetNext() % 3U) == 0)
            {
                animgroups = pistol;
                hasGun.p[GUNLEFT] = flip;
                hasGun.p[GUNRIGHT] = !flip;
            }
            else
            {
                animgroups = doubles;
                hasGun.p[GUNLEFT] = 1;
                hasGun.p[GUNRIGHT] = 1;
            }
        }
        else
        {
            flip = rightgun == NULL;
            animgroups = pistol;
            hasGun.p[GUNLEFT] = flip;
            hasGun.p[GUNRIGHT] = !flip;
        }
    }
    else
    {
        flip = left != NULL;
        animgroups = (weaponIsOneHanded(left) || weaponIsOneHanded(right)) ? pistol : rifle;
        hasGun.p[GUNLEFT] = flip;
        hasGun.p[GUNRIGHT] = !flip;
    }

    chrInitActAttack(self, animgroups, flip, &hasGun, attack_type, entityid, standing);
}


void chrAttackStand(ChrRecord *self, s32 attack_type, s32 entityid)
{
    chrAttackWithAnims(self, attack_type, entityid, 1,
        (struct anim_group_info **)ptr_pistol_firing_animation_groups,
        (struct anim_group_info **)ptr_doubles_firing_animation_groups,
        (struct anim_group_info **)ptr_rifle_firing_animation_groups);
}


void chrAttackKneel(ChrRecord *self, s32 attack_type, s32 entityid)
{
    chrAttackWithAnims(self, attack_type, entityid, 0,
        (struct anim_group_info **)ptr_crouched_pistol_firing_animation_groups,
        (struct anim_group_info **)ptr_crouched_doubles_firing_animation_groups,
        (struct anim_group_info **)ptr_crouched_rifle_firing_animation_groups);
}


/**
 * Starts ACT_ATTACKWALK: walking (or running) at the target while firing.
 */
void chrInitActAttackWalk(ChrRecord *chr, s32 run)
{
    struct weapon_firing_animation_table *panim_float;
    s32 i;
    WeaponObjRecord *weapon;
    s32 flip;
    point2d hasGun;
    point2d singleShot;
    point2d isLauncher;
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

        variant = 0;

        if ((left2 != NULL) && (right2 != NULL))
        {
            flip = (u32)randomGetNext() & 1U;
            variant = (u32)randomGetNext() % 3U;
        }
        else
        {
            flip = right2 == NULL;
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
        flip = left != NULL;

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
        flip = left != NULL;

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

    for (i = 0; i < 2; i++)
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
    chr->act_attackwalk.clock_timer34 = ((u32)randomGetNext() % (u32)(s32)(333.333343506f * g_AiReactionSpeed)) + CHRLV_SEEN_RECENT_CHECK;
#else
    chr->act_attackwalk.clock_timer34 = ((u32)randomGetNext() % (u32)(s32)(400.0f * g_AiReactionSpeed)) + CHRLV_SEEN_RECENT_CHECK;
#endif
    chr->act_attackwalk.facedTarget = 0;
    chr->act_attackwalk.animfloats = panim_float;
    chr->act_attackwalk.timer40 = 0;
    chr->act_attackwalk.nextGun = (u32)randomGetNext() & 1U;
    chr->act_attackwalk.hasGun[1] = hasGun.p[1];
    chr->act_attackwalk.hasGun[0] = hasGun.p[0];
    chr->act_attackwalk.singleShot[1] = singleShot.p[1];
    chr->act_attackwalk.singleShot[0] = singleShot.p[0];
    chr->act_attackwalk.isLauncher[1] = isLauncher.p[1];
    chr->act_attackwalk.isLauncher[0] = isLauncher.p[0];
    chr->sleep = 0;
    chr->act_attackwalk.speed = 0.0f;

    modelSetAnimation(chr->model, panim_float->anim.anim, flip, panim_float->start_frame, 0.5f, 16.0f);
}


/**
 * Starts ACT_ATTACKROLL: a roll to the given side, firing.
 */
void chrInitActAttackRoll(ChrRecord *chr, GUNHAND side)
{
    Model *self_model;
    struct weapon_firing_animation_table *panim_float;
    PropRecord *left;
    PropRecord *left_2;
    s32 flip;
    s32 oneHanded;
    WeaponObjRecord *weapon;
    PropRecord *right;
    point2d hasGun;
    PropRecord *right_2;
    s32 oneBurst;
    point2d singleShot;
    point2d isLauncher;
    s8 recoil;
    s32 i;

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
            flip = right_2 == NULL;
            oneHanded = 1;
            hasGun.p[1] = flip;
            hasGun.p[0] = flip == 0;
        }
    }
    else if (weaponIsOneHanded(left) || weaponIsOneHanded(right))
    {
        flip = left != NULL;
        oneHanded = 1;
        hasGun.p[1] = flip;
        hasGun.p[0] = flip == 0;
    }
    else
    {
        flip = left != NULL;
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

    // The one-handed rolls follow the two-handed ones
    if (oneHanded != 0)
    {
        panim_float += 4;
    }

    for (i = 0; i < 2; i++)
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
    chr->act_attackroll.nextGun = (u32)randomGetNext() & 1u;
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
            chr->act_attackroll.maxShots = 1;
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

    chr->act_attackroll.attacktype = ATTACKTYPE_BOND;
    chr->act_attackroll.entityid = 0;
    chr->act_attackroll.standing = 1;
    chr->act_attackroll.type_of_motion = 0;
    chr->act_attackroll.lastFire60 = 0;
    chr->act_attackroll.attack_time = 0;
    chr->sleep = 0;

    modelSetAnimation(self_model, panim_float->anim.anim, flip, panim_float->start_frame, chrGetGuard007SpeedRating(chr, 0.5f, 0.8f), 16.0f);

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
 * Plays the gun's sound for a shot from that hand, if the gun's sound rate allows
 * (once per frame per guard, and not again until soundrate frames have passed).
 */
static void chrPlayGunfireSound(ChrRecord *self, s32 hand, s32 ratelimit)
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
            if (!(self->hidden & CHRHIDDEN_FIRE_TRACER) && (self->nextGunSoundTime[hand] < g_GlobalTimer))
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

        if ((u16)soundid != 0)
        {
            slot = NULL;

            if (self->gunSounds[hand].ptr_SEbuffer1 == NULL)
            {
                slot = (ALSoundState **)&self->gunSounds[hand].ptr_SEbuffer1;
            }
            else if (self->gunSounds[hand].ptr_SEbuffer2 == NULL)
            {
                slot = (ALSoundState **)&self->gunSounds[hand].ptr_SEbuffer2;
            }

            if (slot != NULL)
            {
                sndPlaySfx(g_musicSfxBufferPtr, soundid, slot);
                sfxSetVolumeAt(*slot, &self->prop->pos);
                self->nextGunSoundTime[hand] = g_GlobalTimer + soundrate;
                self->hidden |= CHRHIDDEN_FIRE_TRACER;
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


static f32 chrGetAimUpAngle(ChrRecord *self)
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
 * Aims the guard's arms and back (aimend*) at its target, with an error from its
 * accuracy, and returns whether the shot from that gun would hit: the muzzle's line to
 * the target is clear. The aim at the player is randomly high or low by inaccuracy.
 */
static s32 chrUpdateAimendsideback(ChrRecord *self, struct weapon_firing_animation_table *anim, s32 leftgun, s32 rightgun, f32 inaccuracy)
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
                        dy -= ducking_height * (0.55f + (0.1f * RANDOMGETNEXT_F32() * inaccuracy));
                    }
                    else if ((current_player_pos->f[1] - (ducking_height * 0.5f)) < self_prop->pos.f[1])
                    {
                        dy -= ducking_height * (0.15f + (0.1f * RANDOMGETNEXT_F32() * inaccuracy));
                    }
                    else
                    {
                        dy = ((RANDOMGETNEXT_F32() * 0.1f * inaccuracy) + 1.0f) * 40.0f;
                    }
                }
                else
                {
                    dy += ducking_height * (0.025f - (0.05f * RANDOMGETNEXT_F32() * inaccuracy));
                }
            }
            else if (((dx * dx) + (dy * dy) + (dz * dz)) > 1000000.0f)
            {
                if (((u32)randomGetNext() % 3U) == 0)
                {
                    dy += ducking_height * (0.05f + (0.1f * RANDOMGETNEXT_F32() * inaccuracy));
                }
                else
                {
                    dy -= ducking_height * (0.05f + (0.55f * RANDOMGETNEXT_F32() * inaccuracy));
                }
            }
            else
            {
                if (self_prop->pos.f[1] < (current_player_pos->f[1] - ducking_height))
                {
                    dy -= ducking_height * (0.55f + (0.1f * RANDOMGETNEXT_F32() * inaccuracy));
                }
                else if ((current_player_pos->f[1] - (ducking_height * 0.5f)) < self_prop->pos.f[1])
                {
                    dy -= ducking_height * (0.15f + (0.1f * RANDOMGETNEXT_F32() * inaccuracy));
                }
                else
                {
                    dy = ((RANDOMGETNEXT_F32() * 0.1f * inaccuracy) - 0.05f) * ducking_height;
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
            coord3d side;
            Mtxf *nodemtx;
            struct ObjectRecord *obj;
            f32 t1;

            subroty = chrGetSubrotySideback(self);

            if (rightgun)
            {
                weapon_prop = chrGetEquippedWeaponProp(self, GUNRIGHT);
            }
            else
            {
                weapon_prop = chrGetEquippedWeaponProp(self, GUNLEFT);
            }

            // Within 10 m, aim from the gun's muzzle (or the hand) rather than the guard's
            // centre. Much as chrGetHandPos, but from last frame's matrices.
            if ((weapon_prop != NULL) && (weapon_prop->flags & PROPFLAG_ONSCREEN) && (dxdydz_square < 1000000.0f))
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

            // Firing at the player: sweep the aim from side to side once a second
            if ((attack_type & ATTACKTYPE_BOND) && !(attack_type & (ATTACKTYPE_AIMONLY | ATTACKTYPE_DONTTURN)))
            {
                t1 = (((f32)(((s32)((f32)g_GlobalTimer * self->model->playspeed) + self->chrnum) % 60) * M_TAU_F) / 60.0f);
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
    self->aimendcount = 10;

    return ret;
}


/**
 * Splits an up/down aim angle between the shoulders and the back: the gun arm takes it up
 * to the animation's limits (max_up, max_down) and the back the rest. The free arm
 * follows by free_arm_frac_up or _down. A gun in each hand: both arms aim.
 *
 * @param anim: the firing animation, or NULL for no limits
 * @param leftgun, rightgun: which hands hold guns (rightgun = the right arm aims)
 * @param next: the aim angle
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


static void chrResetAimend(ChrRecord *self)
{
    self->aimendcount = 10;
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


static void chrSetHandFiring(ChrRecord *self, s32 hand, s32 firing)
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
 * Whether a guard's shot hits the player. Each shot towards the player adds to the
 * guard's shotbondsum by its accuracy, distance and the 007 accuracy slider; when it
 * reaches 1 the player takes the gun's damage.
 *
 * @param inSight: out, whether the guard faces the player closely enough to hit
 * @param hitBond: out, whether the player took damage
 */
static void chrUpdateShotbondsum(ChrRecord *self, s32 *inSight, s32 *hitBond, ITEM_IDS item)
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

    insight = (relangle < limit_angle) || ((M_TAU_F - limit_angle) < relangle);

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

        if (self->accuracyrating > 0)
        {
            increment *= 1.0f + ((f32)self->accuracyrating / 10.0f);
        }
        else if (self->accuracyrating < 0)
        {
            if (self->accuracyrating < -99)
            {
                increment = 0.0f;
            }
            else
            {
                increment *= (f32)(self->accuracyrating + 100) / 100.0f;
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
 * The world position of the muzzle of the gun in that hand (or of the hand holding it).
 * FALSE if the hand is empty or the gun is offscreen.
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

    if (weapon_prop != NULL)
    {
        obj = weapon_prop->obj;
        weapon_prop_model = obj->model;

        if (weapon_prop->flags & PROPFLAG_ONSCREEN)
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


/**
 * Fires the gun in one hand, at the rate of fire: a bullet (traced along the aim to what
 * it hits, with chrUpdateShotbondsum deciding whether it hits the player), or a rocket or
 * grenade. Also its sound, tracer and muzzle flash.
 */
static void chrFireHand(ChrRecord *self, s32 hand)
{
    PropRecord *self_prop;
    s32 ratelimit;
    s32 firing;
    s32 betweenShots;
    WeaponObjRecord *weapon;
    PropRecord *player_prop;
    s32 attacktype;
    s32 shoot;
    s32 tracer;
    coord3d hitpos;
    StandTile *hittile;
    f32 subroty;
    f32 aimup;
    coord3d gunpos;
    StandTile *self_stan;
    StandTile *guntile;
    s32 inSight;
    s32 hitBond;
    s32 showHit;
    coord3d shotdir;
    s32 canSeeTarget;
    f32 dy;
    f32 dz;
    f32 dx;
    f32 shotDistSq;
    struct WeaponObjRecord *rocket;
    Mtxf rocketMtx;
    coord3d rocketSpeed;
    PropRecord *weapon_prop;
    coord3d rocketAccel;
    Mtxf rocketRot;
    Mtxf rocketYRot;
    struct WeaponObjRecord *grenade;
    Mtxf grenadeMtx;
    coord3d grenadeSpeed;
    Mtxf grenadeRot;
    Mtxf grenadeYRot;
    s32 needsSight;

    self_prop = self->prop;
    weapon_prop = chrGetEquippedWeaponProp(self, hand);

    if (weapon_prop != NULL)
    {
        firing = 0;
        betweenShots = 0;
        weapon = weapon_prop->weapon;
        player_prop = currentPlayerGetProp();
        attacktype = ATTACKTYPE_BOND;

        if (self->actiontype == ACT_ATTACK)
        {
            attacktype = self->act_attack.attacktype;
        }

        // Firing at the player needs them seen in the last 2 seconds, unless the gun
        // fires single shots.
        needsSight = attacktype & ATTACKTYPE_BOND;

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
            else if ((self->firecount[hand] % weaponGetFireRate(weapon->weaponnum)) == 0)
            {
                // Automatic: a bullet every fire-rate frames, a tracer every other bullet
                shoot = 1;

                if (((self->firecount[hand] % (weaponGetFireRate(weapon->weaponnum) * 2)) == 0)
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

                if (!chrGetHandPos(self, hand, &gunpos))
                {
                    gunpos.f[0] = self_prop->pos.f[0];
                    gunpos.f[1] = self_prop->pos.f[1] + 30.0f;
                    gunpos.f[2] = self_prop->pos.f[2];

                    if (hand == GUNLEFT)
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

                // No firing through a door the gun pokes through
                if (stanTestLineUnobstructed(&self_stan, self_prop->pos.x, self_prop->pos.f[2], gunpos.f[0], gunpos.f[2], CDTYPE_DOORS, gunpos.f[1] - self->ground, gunpos.f[1] - self->ground, 0.0f, 1.0f))
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

                    chrSetMoving(self, FALSE);
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

                    chrSetMoving(self, TRUE);

                    dx = hitpos.f[0] - gunpos.f[0];
                    dy = hitpos.f[1] - gunpos.f[1];
                    dz = hitpos.f[2] - gunpos.f[2];

                    shotDistSq = (dx * dx) + (dy * dy) + (dz * dz);

                    // Rockets and grenades are only fired at something over 4 m away
                    if (weapon->weaponnum == ITEM_ROCKETLAUNCH)
                    {
                        if (((dx * dx) + (dy * dy) + (dz * dz)) > 160000.0f)
                        {
                            rocket = weaponCreateFromModel(PROP_CHRROCKET, ITEM_ROCKETROUND);
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
                                    rocket->projectile->flags |= PROJECTILEFLAG_LAUNCHING;
                                    rocket->timer = -1;
                                    rocket->projectile->flags |= PROJECTILEFLAG_RISING;

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
                            grenade = weaponCreateFromModel(PROP_CHRGRENADEROUND, ITEM_GRENADEROUND);
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
                                gunLaunchProjectile((ObjectRecord *)grenade, &gunpos, guntile, &grenadeRot, &grenadeSpeed, &grenadeMtx, self_prop);

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
                                    (g_StanHit.prop == NULL)
                                    || ((g_StanHit.prop->type != PROP_TYPE_CHR) && (g_StanHit.prop->type != PROP_TYPE_VIEWER))
                                )
                                && (shotDistSq < 10000.0f))
                            {
                                showHit = 0;
                            }
                        }

                        if (showHit != 0)
                        {
                            if (hittile != NULL)
                            {
                                sparkCreate(&hitpos, 1, 26.0f, (s16)hittile->room);
                            }

                            if (g_StanHit.prop != NULL)
                            {
                                shotPlayHitPropSound(weapon->weaponnum, g_StanHit.prop, -1);

                                if (g_StanHit.prop->type == PROP_TYPE_CHR)
                                {
                                    if ((self->chrflags & CHRFLAG_CAN_SHOOT_CHRS) != 0)
                                    {
                                        chrTakeShot(g_StanHit.prop->chr, HIT_CHEST, &shotdir, weapon->weaponnum, FALSE);
                                    }
                                }
                                else if ((g_StanHit.prop->type == PROP_TYPE_OBJ) || (g_StanHit.prop->type == PROP_TYPE_WEAPON))
                                {
                                    objApplyShotDamage(
                                        g_StanHit.prop->obj,
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


/**
 * A roll that ends kneeling (g_ChrFireAnims[2] and [3]) goes into kneeling fire
 * (g_ChrFireAnims[1]) instead of standing up. FALSE for other rolls and attacks.
 */
static s32 chrAttackRollToKneel(ChrRecord *self)
{
    Model *model;
    struct weapon_firing_animation_table *p;
    s32 flip;

    if ((self->act_attackroll.animfloats == &g_ChrFireAnims[2]) || (self->act_attackroll.animfloats == &g_ChrFireAnims[3]))
    {
        model = self->model;
        flip = (s32)model->flip;
        self->act_attackroll.turning = 2;
        self->act_attackroll.animfloats = &g_ChrFireAnims[1];
        self->sleep = 0;

        p = &g_ChrFireAnims[1];

        modelSetAnimation(model, p->anim.anim, flip, p->shoot_end_frame, chrGetGuard007SpeedRating(self, 0.7f, 1.12f), 22.0f);

        if (g_ChrFireAnims[1].end_frame >= 0.0f)
        {
            modelSetAnimEndFrame(model, g_ChrFireAnims[1].end_frame);
        }

        return 1;
    }

    return 0;
}


/**
 * After the last shot, plays the attack's animation on from the end of the recoil (or of
 * the shooting).
 */
static void chrAttackRollResumeAfterShot(ChrRecord *self)
{
    Model *model;

    model = self->model;

    if (self->act_attackroll.animfloats->recoil_end_frame > 0.0f)
    {
        modelSetAnimation(model, modelGetAnim(model), (s32)model->flip, self->act_attackroll.animfloats->recoil_end_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 8.0f);
    }
    else
    {
        modelSetAnimation(model, modelGetAnim(model), (s32)model->flip, self->act_attackroll.animfloats->shoot_end_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 8.0f);
    }

    if (self->act_attackroll.animfloats->end_frame >= 0.0f)
    {
        modelSetAnimEndFrame(model, self->act_attackroll.animfloats->end_frame);
    }
}


/**
 * The shared tick of ACT_ATTACK and ACT_ATTACKROLL (the roll's fields are laid out as the
 * attack's): stops an attack whose target hasn't been hit for a while, replays the
 * firing part of the animation for each shot, turns to the target, aims, and sets each
 * hand firing.
 */
static void chrTickAttackCommon(ChrRecord *self)
{
    s32 i;
    Model *self_model;
    f32 df;
    f32 speed;
    f32 startframe;
    f32 frame;
    f32 endframe;
    f32 angleoffset;
    f32 turnendframe;

    self_model = self->model;
    frame = modelGetAnimFrame(self_model);

    // Half a second of firing without the shots counting as aimed at the target (see
    // chrFireHand): give up.
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
            if ((self->act_attack.animfloats->recoil_end_frame < 0.0f) || (frame < self->act_attack.animfloats->recoil_end_frame))
            {
                if (self->act_attack.doRecoil == 0)
                {
                    if (!chrAttackRollToKneel(self))
                    {
                        modelSetAnimation(self_model, modelGetAnim(self_model), (s32)self_model->flip, self->act_attack.animfloats->shoot_end_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 8.0f);

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

                self->act_attack.numShots = self->act_attack.maxShots + 1;
                frame = modelGetAnimFrame(self_model);
            }
        }
    }

    if (modelGetAnimEndFrame(self_model) <= frame)
    {
        if ((self->act_attack.doOneBurst != 0) || (self->act_attack.maxShots < self->act_attack.numShots))
        {
            if (!chrAttackRollToKneel(self))
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
            // Replay the shot (or recoil) part of the animation for the next shot, at
            // a speed that makes it last 12 to 16 frames.
            speed = 0.5f;

            if (self->act_attack.doRecoil != 0)
            {
                if (self->act_attack.animfloats->recoil_start_frame > 0.0f)
                {
                    startframe = self->act_attack.animfloats->recoil_start_frame;
                }
                else
                {
                    startframe = self->act_attack.animfloats->shoot_start_frame;
                }

                if (self->act_attack.animfloats->recoil_end_frame > 0.0f)
                {
                    endframe = self->act_attack.animfloats->recoil_end_frame;
                }
                else
                {
                    endframe = self->act_attack.animfloats->shoot_end_frame;
                }
            }
            else
            {
                startframe = self->act_attack.animfloats->shoot_start_frame;

                if (self->act_attack.animfloats->recoil_start_frame > 0.0f)
                {
                    endframe = self->act_attack.animfloats->recoil_start_frame;
                }
                else
                {
                    endframe = self->act_attack.animfloats->shoot_end_frame;
                }
            }

            df = endframe - startframe;
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

            modelSetAnimation(self_model, modelGetAnim(self_model), (s32)self_model->flip, startframe, speed, 8.0f);
            modelSetAnimEndFrame(self_model, endframe);
        }

        frame = modelGetAnimFrame(self_model);
    }

    if ((self->act_attack.attacktype & TARGET_DONTTURN) == 0)
    {
        angleoffset = self->act_attack.animfloats->angle_offset;
        turnendframe = self->act_attack.animfloats->turnEndFrame;

        if ((self->act_attack.attacktype & TARGET_AIM_ONLY) != 0)
        {
            if (modelGetAnimEndFrame(self_model) < turnendframe)
            {
                turnendframe = modelGetAnimEndFrame(self_model);
            }
        }

        if (self_model->flip != GUNRIGHT)
        {
            angleoffset = M_TAU_F - angleoffset;
        }

        self->act_attack.turning = chrSetSubroty(self, (s32)self->act_attack.turning, turnendframe, chrGetGuard007SpeedRating(self, 1.0f, 1.6f), angleoffset);
    }

    if ((self->act_attack.animfloats->aim_start_frame < frame) && (frame < self->act_attack.animfloats->aim_end_frame))
    {
        chrUpdateAimendsideback(self, self->act_attack.animfloats, (s32)self->act_attack.hasGun[1], (s32)self->act_attack.hasGun[0], 1.0f);
    }
    else
    {
        chrResetAimend(self);
    }

    for (i = 0; i < 2; i++)
    {
        if (self->act_attack.hasGun[i] != 0)
        {
            // Automatic guns fire for the whole shooting part of the animation; a roll
            // slows down while firing so the burst lasts long enough.
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
                            if ((s32)self->act_attack.numWaitTicks >= (50 - ((s32)df * 2)))
#else
                            if ((s32)self->act_attack.numWaitTicks >= (60 - ((s32)df * 2)))
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
                    modelSetAnimSpeed(self_model, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 0.0f);
                }
            }
            else if (
                (self->act_attack.fired == 0)
                && ((i == self->act_attack.nextGun) || (self->act_attack.singleShot[self->act_attack.nextGun] == 0))
                && (((self->act_attack.animfloats->recoil_start_frame >= 0.0f)
                        && (self->act_attack.animfloats->recoil_start_frame <= frame)
                        && (frame <= self->act_attack.animfloats->recoil_end_frame))
                    || ((self->act_attack.animfloats->recoil_start_frame < 0.0f)
                        && (self->act_attack.animfloats->shoot_start_frame <= frame))))
            {
                // A single shot gun fires once per replay of the shot, the hands taking turns
                self->act_attack.fired = 1;
                self->act_attack.nextGun = 1 - self->act_attack.nextGun;
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

            modelSetAnimation(self_model, modelGetAnim(self_model), (s32)self_model->flip, endframe, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);

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
                // PAL doesn't set TARGET_AIM_ONLY
                self->act_attack.attacktype |= TARGET_AIM_ONLY;
#endif
                self->act_attack.attacktype &= ~TARGET_DONTTURN;

                if (self->act_attack.standing != 0)
                {
                    chrAttackStand(self, (s32)self->act_attack.attacktype, self->act_attack.entityid);

                    return;
                }

                chrAttackKneel(self, (s32)self->act_attack.attacktype, self->act_attack.entityid);

                return;
            }

            return;
        }
    }

    if ((self->act_attack.attacktype & TARGET_AIM_ONLY) != 0)
    {
        if ((self->act_attack.attacktype & TARGET_DONTTURN) != 0)
        {
            if (chrUpdateAimendsideback(self, self->act_attack.animfloats, (s32)self->act_attack.hasGun[1], (s32)self->act_attack.hasGun[0], 0.2f) == 0)
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
                modelSetAnimation(self_model, modelGetAnim(self_model), (s32)self_model->flip, self->act_attack.animfloats->recoil_end_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);

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


/**
 * Which of g_ChrFireAnims (0 to 19) a firing animation is, or -1.
 */
static s32 chrGetRollAnimIndex(struct weapon_firing_animation_table *anim)
{
    s32 i;

    for (i = 0; i < 20; i++)
    {
        if (anim == &g_ChrFireAnims[i])
        {
            return i;
        }
    }

    return -1;
}


/**
 * A one-handed roll (g_ChrFireAnims[4] to [7]) goes on into firing from a crouch
 * (g_ChrFireAnims[8] to [19], with both guns from [12]), blending over the frames
 * it takes to turn back to face the target.
 */
void chrTickAttackRoll(ChrRecord *self)
{
    // Blend frames for g_ChrFireAnims[8] to [19]
    static const f32 turnframes_for_anim[12] = {24.0f, 24.0f, 32.0f, 44.0f, 24.0f, 34.0f, 32.0f, 44.0f, 24.0f, 34.0f, 32.0f, 44.0f};
    Model *model;
    f32 frame;
    struct weapon_firing_animation_table *nextanim;
    s32 flip;
    f32 turnframes;
    ModelRwData_HeaderRecord *rootrw;
    s32 index;
    s32 nextindex;

    if (self->act_attackroll.oneHanded != 0)
    {
        model = self->model;
        frame = modelGetAnimFrame(model);
        index = chrGetRollAnimIndex(self->act_attackroll.animfloats);

        if ((index >= 4) && (index <= 7))
        {
            if (self->act_attackroll.animfloats->end_frame <= frame)
            {
                flip = (s32)model->flip;
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

                nextindex = chrGetRollAnimIndex(nextanim);

                if ((nextindex >= 8) && (nextindex <= 19))
                {
                    turnframes = turnframes_for_anim[nextindex - 8];
                }

                self->act_attackroll.turning = 2;
                self->act_attackroll.animfloats = nextanim;
                self->sleep = 0;

                modelSetAnimation(model, nextanim->anim.anim, flip, nextanim->start_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), turnframes);

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
                    rootrw->turnPerFrame = -self->act_attackroll.animfloats->angle_offset / turnframes;

                    if (flip != GUNRIGHT)
                    {
                        rootrw->turnPerFrame = -rootrw->turnPerFrame;
                    }
                }
            }
        }
        else if ((index >= 8) && (index <= 19) && (self->act_attackroll.doRecoil == 0))
        {
            if ((self->act_attackroll.animfloats->recoil_end_frame > 0.0f) && (frame <= self->act_attackroll.animfloats->recoil_end_frame))
            {
                if (modelGetAnimEndFrame(model) <= frame)
                {
                    modelSetAnimation(model, modelGetAnim(model), (s32)model->flip, self->act_attackroll.animfloats->recoil_end_frame, chrGetGuard007SpeedRating(self, 0.5f, 0.8f), 16.0f);

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
        chrUpdateAimendsideback(self, self->act_attackwalk.animfloats, (s32)self->act_attackwalk.hasGun[1], (s32)self->act_attackwalk.hasGun[0], 1.0f);
    }
    else
    {
        chrResetAimend(self);
    }

    if (self->act_attackwalk.facedTarget && !(self->act_attackwalk.clock_timer30 < CHRLV_ATTACKWALK_CLOCK_TIMER30_MAX))
    {
        for (i = 0; i < 2; i++)
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

