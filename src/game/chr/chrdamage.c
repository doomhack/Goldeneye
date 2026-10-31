/**
 * Guards being hurt: a bullet hit (chrTakeShot) or an explosion (chrExplosionDamage)
 * takes armour then health, plays a hurt sound and picks a reaction (chrReactToHit):
 * a flinch, a stagger (ACT_PREARGH, ACT_ARGH) or a death (ACT_DIE, then ACT_DEAD and
 * the fade out). Nearby guards hear about it (chrAlertOthersOfInjury).
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
#include "game/level/gamemode.h"


static void chrReactToHit(ChrRecord *self, coord3d *dir, f32 angle, s32 req_animation_id, ITEM_IDS item);
static s32 chrGetHitInterruptFrame(ChrRecord *self, f32 *result);
static void chrPlayHurtSound(ChrRecord *self);
static void chrAlertOthersOfInjury(ChrRecord *self, s32 flag);

/**
 * @param arg0:
 * @param arg1:
 * @param arg2:
 * @param req_animation_id: Lookup by id property in g_HitReactionTable
 * @param item: argument to weaponGetImpactForce
 */
static void chrReactToHit(ChrRecord *self, coord3d *dir, f32 angle, s32 req_animation_id, ITEM_IDS item)
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
static s32 chrGetHitInterruptFrame(ChrRecord *self, f32 *result)
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


static void chrPlayHurtSound(ChrRecord *self)
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
        if ((getPlayerCount() == 1) || gameIsTwoPlayerMission())
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


static s16 metal_ricochet_SFX[3] = {HIT_BULLET_METAL_A3_SFX, HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_B_SFX};

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

        if (isPlayer && (!gameIsMulti()))
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

        if ((self->prop->type == PROP_TYPE_VIEWER) && gameIsTwoPlayerMission() && (gameIsCounterOp() ? !isPlayer : isPlayer))
        {
            // Co-op: no friendly fire. Counter-op: the two players hurt
            // each other, and guards' stray shots don't hurt the guard
            // player.
            if (gameIsCounterOp())
            {
                return FALSE;
            }
        }
        else if (self->prop->type == PROP_TYPE_VIEWER)
        {
            playerNum = playerGetCurrentNum();

            if (gameIsCounterOp())
            {
                damageToCause = counterOpPlayerDamage(playerNum, damageToCause, weaponid);
            }
            else
            {
                damageToCause *= 0.125f;
            }

            playerSetCurrent(playerGetIndexForProp(self->prop));

            if (gameIsCounterOp() && counterOpIsGuardPlayer(playerNum))
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
static void chrAlertOthersOfInjury(ChrRecord *self, s32 flag)
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

