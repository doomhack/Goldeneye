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


static void chrReactToHit(ChrRecord *self, coord3d *dir, f32 angle, s32 hitpart, ITEM_IDS item);
static s32 chrGetHitInterruptFrame(ChrRecord *self, f32 *result);
static void chrPlayHurtSound(ChrRecord *self);
static void chrAlertOthersOfInjury(ChrRecord *self, s32 flag);

/**
 * Picks and starts a guard's reaction to a hit that already did its damage: a death
 * (sometimes slumping against a wall behind them) once health is gone, otherwise a
 * flinch, or the long "hit in the butt" stagger for a shot from behind into the
 * pelvis. A guard that falls or staggers drops what it holds.
 *
 * @param dir: direction the hit came from, scaled into the knockback of a death
 * @param angle: angle of the hit relative to the guard's facing (0 = from the front)
 * @param hitpart: body part, looked up in g_HitReactionTable
 * @param item: the weapon, for its impact force (weaponGetImpactForce)
 */
static void chrReactToHit(ChrRecord *self, coord3d *dir, f32 angle, s32 hitpart, ITEM_IDS item)
{
    bool dying;
    PropRecord *prop;
    struct Model *model;
    bool reacted;
    f32 impact_force;
    s32 reaction_index;
    bool hit_again_this_frame;
    ModelAnimation *anim = NULL;
    struct ChrHitReaction *reaction = NULL;
    f32 angle_left;
    f32 angle_right;
    f32 dist_left;
    f32 dist_right;
    f32 ft;
    struct StruckAnim *struck_ani;
    s32 i;

    dying = TRUE;
    prop = self->prop;
    model = self->model;
    reacted = FALSE;
    reaction_index = 0;

    if ((self->prop->type != PROP_TYPE_VIEWER) || (getPlayerCount() < 2))
    {
        // A second hit in the same frame starts its animation without blending.
        hit_again_this_frame = (self->actiontype == ACT_ARGH) && (g_GlobalTimer == self->act_argh.lastHit60);

        for (i = 0; g_HitReactionTable[i].hitpart != -1; i++)
        {
            if (hitpart == g_HitReactionTable[i].hitpart)
            {
                reaction_index = i;

                break;
            }
        }

        if (self->damage >= self->maxdamage)
        {
            // Shot from the front with a flat wall right behind: 1 in 20 slump against it.
            if (((angle < M_PI_2F) || (angle > 4.712389f)) && ((randomGetNext() % 20u) == 0))
            {
                ft = modelGetRootRotY(model) + M_PI_F;

                // 10 degrees either side of straight behind
                angle_left = ft + 0.17453294f;
                angle_right = ft - 0.17453294f;

                if (angle_left >= M_TAU_F)
                {
                    angle_left -= M_TAU_F;
                }

                if (angle_right >= M_TAU_F)
                {
                    angle_right -= M_TAU_F;
                }

                dist_left = chrGetClearDistInDirDefault(prop, angle_left, 150.0f);
                dist_right = chrGetClearDistInDirDefault(prop, angle_right, 150.0f);

                if ((dist_left < 150.0f) && (dist_right < 150.0f))
                {
                    ft = dist_left - dist_right;
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

                        modelSetAnimationWithMerge(model, struck_ani->struck_anim, struck_ani->flip, 0.0f, struck_ani->speed, 16.0f, !hit_again_this_frame);

                        if (struck_ani->endframe >= 0.0f)
                        {
                            modelSetAnimEndFrame(model, struck_ani->endframe);
                        }

                        // Note: PD sets the chrwidth to 10 when a guard dies slumped against an object or wall
                        self->chrwidth = 10.0f;

                        reacted = TRUE;
                    }
                }
            }

            if (!reacted)
            {
                reaction = &g_HitReactionTable[reaction_index];

                if ((reaction->deathAnims != NULL) && (reaction->deathAnimCount > 0))
                {
                    struct StruckAnim *struck_anib;
                    s32 tr;

                    reacted = TRUE;

                    tr = (randomGetNext() % (u32)reaction->deathAnimCount);
                    struck_anib = &reaction->deathAnims[tr];
                    chrStopFiring(self);

                    self->actiontype = ACT_DIE;
                    self->act_die.notifychrindex = 0;
                    self->act_die.thudframe1 = struck_anib->thudframe1;
                    self->act_die.thudframe2 = struck_anib->thudframe2;
                    self->sleep = 0;
                    self->act_die.timeextra = 0.0f;

                    modelSetAnimationWithMerge(model, struck_anib->struck_anim, struck_anib->flip, 0.0f, struck_anib->speed, 16.0f, !hit_again_this_frame);

                    // 99 times in 100 the neck death stops before its end
                    if (struck_anib->struck_anim == ANIM(death_neck) && ((randomGetNext() % 100u) != 0))
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
            // Shot in the pelvis from behind: 2 in 5 stagger for a while.
            if ((hitpart == HIT_PELVIS) && (angle > 2.3561945f) && (angle < 3.926991f) && ((randomGetNext() % 5u) < 2u))
            {
                u32 choice = randomGetNext() % 5u;
                chrStopFiring(self);
                self->actiontype = ACT_ARGH;
                self->act_argh.notifychrindex = 0;
                self->act_argh.lastHit60 = g_GlobalTimer;
                self->sleep = 0;

                if ((randomGetNext() & 1) != 0)
                {
                    anim = ANIM(hit_butt_long);
                    modelSetAnimationWithMerge(model, anim, randomGetNext() & 1, 10.0f, 0.5f, 16.0f, !hit_again_this_frame);

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
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, (f32)(anim->numFrames - 1), 8.0f));
                    }
                }
                else
                {
                    anim = ANIM(hit_butt_short);
                    modelSetAnimationWithMerge(model, anim, randomGetNext() & 1, 0.0f, 0.5f, 16.0f, !hit_again_this_frame);

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
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, (f32)(anim->numFrames - 1), 8.0f));
                    }
                }

                reacted = TRUE;
            }

            if (!reacted)
            {
                if ((g_HitReactionTable[reaction_index].flinchAnims != NULL) && (g_HitReactionTable[reaction_index].flinchAnimCount > 0))
                {
                    PropRecord *leftprop = chrGetEquippedWeaponProp(self, GUNLEFT);
                    PropRecord *rightprop = chrGetEquippedWeaponProp(self, GUNRIGHT);
                    s32 tr;

                    reacted = TRUE;

                    // A hand holding a gun flinches like its arm. Table rows 0-15 are in HIT_ order.
                    if ((reaction_index == HIT_LEFT_HAND) && (leftprop != NULL))
                    {
                        reaction_index = HIT_LEFT_ARM;
                    }
                    else if ((reaction_index == HIT_RIGHT_HAND) && (rightprop != NULL))
                    {
                        reaction_index = HIT_RIGHT_ARM;
                    }

                    reaction = &g_HitReactionTable[reaction_index];
                    tr = (randomGetNext() % (u32)reaction->flinchAnimCount);
                    struck_ani = &reaction->flinchAnims[tr];

                    chrStopFiring(self);

                    self->actiontype = ACT_ARGH;
                    self->act_argh.notifychrindex = 0;
                    self->act_argh.lastHit60 = g_GlobalTimer;
                    self->sleep = 0;

                    modelSetAnimationWithMerge(model, struck_ani->struck_anim, struck_ani->flip, 0.0f, struck_ani->speed, 16.0f, !hit_again_this_frame);

                    if (struck_ani->endframe >= 0.0f)
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, struck_ani->endframe, 8.0f));
                    }
                    else
                    {
                        modelSetAnimEndFrame(model, chrGetGuard007ArghRating(self, (f32)(struck_ani->struck_anim->numFrames - 1), 8.0f));
                    }
                }
            }

            dying = FALSE;
        }

        if (dying && reacted)
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
 * Whether the current animation must play on a bit before a hit can interrupt it
 * (the turn at the start of a roll), and if so the frame to stop it at in *result.
 * ACT_PREARGH returns TRUE without setting *result.
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
            objSetDropType(self->handle_positiondata_hat, 4);
            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
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
        chrSetHiddenToRandom(self);
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

            if (chrGetHitInterruptFrame(self, &endframe2))
            {
                if (endframe2 >= 0.0f)
                {
                    modelSetAnimEndFrame(self->model, endframe2);
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
    Model *self_model;
    PropRecord *self_prop;
    f32 subroty;
    f32 atan;
    f32 norm;
    s32 dirindex;
    f32 relangle;
    struct ExplosionDeathAnim *deathanim;
    coord3d push;
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
        self->act_die.thudframe1 = deathanim->thudframe;
        self->sleep = 0;
        self->act_die.thudframe2 = -1.0f;
        self->act_die.timeextra = 0.0f;

        modelSetAnimation(self_model, ANIM_AT(deathanim->animOffset), deathanim->flip, deathanim->startframe, deathanim->speed, 8.0f);

        if (deathanim->endframe >= 0.0f)
        {
            modelSetAnimEndFrame(self_model, deathanim->endframe);
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

        if ((self->weapons_held[GUNRIGHT] != NULL) && ((self->weapons_held[GUNRIGHT]->obj->flags & PROPFLAG_AIUNDROPPABLE) == 0))
        {
            objSetDropType(self->weapons_held[GUNRIGHT], 1);
            self->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
        }

        if ((self->weapons_held[GUNLEFT] != NULL) && ((self->weapons_held[GUNLEFT]->obj->flags & PROPFLAG_AIUNDROPPABLE) == 0))
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
 * Tells up to 4 guards within 20 m that can see this guard that it was shot or died.
 * The scan resumes each frame from the last guard it reached.
 *
 * @param flag: 0 when shot, otherwise died
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
        if (thud_index >= ARRAYCOUNT(body_hit_SFX))
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
        if (thud_index >= ARRAYCOUNT(body_hit_SFX))
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

