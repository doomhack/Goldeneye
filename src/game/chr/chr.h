#ifndef _CHR_H_
#define _CHR_H_
#include <ultra64.h>
#include "game/prop/prop.h"
#include <bondgame.h>
#include <bondtypes.h>
#include "game/chr/chrai.h"

#define EXPLOSION_ANIMATION_TABLE_LEN 8

struct StruckAnim;

// Per-body-part hits and deaths reaction descriptor.
struct ChrHitReaction
{
    s32 hitpart; // HITTARGET key
    s32 impactPuffCount;
    s32 unused_8;
    f32 impactPuffSize;
    s32 backImpactPuffCount; // Count for the impact puff(s) that appears behind the hit point
    s32 unused_14;
    f32 backImpactPuffSize;

    struct StruckAnim *deathAnims;
    s32 deathAnimCount;

    struct StruckAnim *flinchAnims;
    s32 flinchAnimCount;
};

// Animations for when characters are wounded or killed.
struct StruckAnim
{
    ModelAnimation *struck_anim;
    s32 flip; // Mirror flag selecting left/right variant
    f32 endframe;
    f32 speed;
    s32 knockback; // Enables characters to be pushed back when the gun's impact force > 0
    f32 thudframe1;
    f32 thudframe2;
};

struct explosion_death_animation
{
    s32 anonymous_0;
    s32 anonymous_1;
    f32 anonymous_2;
    f32 anonymous_3;
    f32 anonymous_4;
    f32 anonymous_5;
    f32 anonymous_6;
};

struct explosion_anim_group_info
{
    s8 *table;
    s32 count;
};

struct weapon_firing_animation_table
{
    union
    {
        s32 offset;
        struct ModelAnimation *anim;
    } anim;
    
    f32 turnEndFrame; // the turn at the start of the animation ends here
    f32 turn_angle_per_frame;
    f32 angle_offset;

    /**
     * Offset 0x10.
    */
    f32 start_frame;
    f32 end_frame;
    f32 shoot_start_frame;
    f32 shoot_end_frame;

    /**
     * Offset 0x20.
    */
    f32 recoil_start_frame;
    f32 recoil_end_frame;
    f32 aim_start_frame;
    f32 aim_end_frame;

    /**
     * Offset 0x30.
    */
    f32 max_up;
    f32 max_down;

    /**
     * Some kind of minimum. See chrUpdateAimendsideback.
     * Offset 0x38.
    */
    f32 max_left;

    /**
     * Some kind of maximum. See chrUpdateAimendsideback.
     * Offset 0x3c.
    */
    f32 max_right;

    /**
     * Offset 0x40.
    */
    f32 free_arm_frac_up;
    f32 free_arm_frac_down;
};

struct anim_group_info
{
    struct weapon_firing_animation_table (*table)[];
    s32 len;
};

extern s32 objectiveregisters1;
extern ChrRecord* g_BgChrs;
extern s32 g_NumBgChrs;

extern f32 g_AiAccuracyModifier;
extern f32 g_AiDamageModifier;
extern f32 g_AiHealthModifier;
extern f32 g_AiReactionSpeed;
extern s32 g_SeenBondRecentlyGuardCount;

extern struct ChrHitReaction g_HitReactionTable[];

extern struct StruckAnim death_left_foot[];
extern struct StruckAnim flinch_left_foot[];
extern struct StruckAnim death_left_leg[];
extern struct StruckAnim flinch_left_leg[];
extern struct StruckAnim death_left_thigh[];
extern struct StruckAnim flinch_left_thigh[];
extern struct StruckAnim death_right_foot[];
extern struct StruckAnim flinch_right_foot[];
extern struct StruckAnim death_right_leg[];
extern struct StruckAnim flinch_right_leg[];
extern struct StruckAnim death_right_thigh[];
extern struct StruckAnim flinch_right_thigh[];
extern struct StruckAnim death_pelvis[];
extern struct StruckAnim flinch_pelvis[];
extern struct StruckAnim death_head[];
extern struct StruckAnim flinch_head[];
extern struct StruckAnim death_left_hand[];
extern struct StruckAnim flinch_left_hand[];
extern struct StruckAnim death_left_arm[];
extern struct StruckAnim flinch_left_arm[];
extern struct StruckAnim death_left_shoulder[];
extern struct StruckAnim flinch_left_shoulder[];
extern struct StruckAnim death_right_hand[];
extern struct StruckAnim flinch_right_hand[];
extern struct StruckAnim death_right_arm[];
extern struct StruckAnim flinch_right_arm[];
extern struct StruckAnim death_right_shoulder[];
extern struct StruckAnim flinch_right_shoulder[];
extern struct StruckAnim death_chest[];
extern struct StruckAnim flinch_chest[];
extern struct StruckAnim death_gun[];
extern struct StruckAnim flinch_gun[];
extern struct StruckAnim death_stagger[];


extern struct weapon_firing_animation_table rifle_firing_animation_group1[];
extern struct weapon_firing_animation_table rifle_firing_animation_group2[];
extern struct weapon_firing_animation_table rifle_firing_animation_group5[];
extern struct weapon_firing_animation_table rifle_firing_animation_group3[];
extern struct weapon_firing_animation_table rifle_firing_animation_group4[];
extern struct weapon_firing_animation_table pistol_firing_animation_group1[];
extern struct weapon_firing_animation_table pistol_firing_animation_group2[];
extern struct weapon_firing_animation_table pistol_firing_animation_group3[];
extern struct weapon_firing_animation_table pistol_firing_animation_group6[];
extern struct weapon_firing_animation_table pistol_firing_animation_group4[];
extern struct weapon_firing_animation_table pistol_firing_animation_group5[];
extern struct weapon_firing_animation_table doubles_firing_animation_group1[];
extern struct weapon_firing_animation_table doubles_firing_animation_group2[];
extern struct weapon_firing_animation_table doubles_firing_animation_group3[];
extern struct weapon_firing_animation_table crouched_rifle_firing_animation_group1[];
extern struct weapon_firing_animation_table crouched_rifle_firing_animation_groupA[];
extern struct weapon_firing_animation_table crouched_rifle_firing_animation_group2[];
extern struct weapon_firing_animation_table crouched_rifle_firing_animation_group3[];
extern struct weapon_firing_animation_table crouched_pistol_firing_animation_group1[];
extern struct weapon_firing_animation_table crouched_pistol_firing_animation_group2[];
extern struct weapon_firing_animation_table crouched_pistol_firing_animation_group3[];
extern struct weapon_firing_animation_table crouched_doubles_firing_animation_group1[];
extern struct weapon_firing_animation_table crouched_doubles_firing_animation_group2[];
extern struct weapon_firing_animation_table crouched_doubles_firing_animation_group3[];


extern struct weapon_firing_animation_table g_ChrFireAnims[];
extern struct weapon_firing_animation_table g_ChrMoveFireAnims[];

extern struct anim_group_info *ptr_rifle_firing_animation_groups[];
extern struct anim_group_info *ptr_pistol_firing_animation_groups[];
extern struct anim_group_info *ptr_doubles_firing_animation_groups[];
extern struct anim_group_info *ptr_crouched_rifle_firing_animation_groups[];
extern struct anim_group_info *ptr_crouched_pistol_firing_animation_groups[];
extern struct anim_group_info *ptr_crouched_doubles_firing_animation_groups[];

extern f32 g_ChrAnimRate;
extern s32 g_ChrDebugAnimOverride;
extern s32 g_ChrDebugAnimIndex;
extern s32 g_ChrDebugPaused;
extern s32 g_ChrDebugStep;

extern s32 g_ChrUnused;
extern s32 g_ChrShowPatrols;
extern s32 g_NextChrNum;
extern ChrRecord *g_ChrSlots;
extern s32 g_NumChrSlots;
extern rgba_u8 gBloodColour;

extern f32 g_ChrSpeedWalk;
extern f32 g_ChrSpeedRun;
extern f32 g_ChrSpeedSprint;
extern f32 g_ChrSpeedWalkUnarmed;
extern f32 g_ChrSpeedRunOneHanded;
extern f32 g_ChrSpeedSprintOneHanded;
extern f32 g_ChrSpeedWalkFemale;
extern f32 g_ChrSpeedRunFemale;
extern f32 g_ChrSpeedSprintOneHanded2;


extern u32 num_bodies;
extern u32 num_male_heads;
extern u32 num_female_heads;
extern s32 list_of_bodies[];
extern s32 random_male_heads[];
extern s32 random_female_heads[];
extern u32 current_random_body;
extern u32 current_random_male_head;
extern u32 current_random_female_head;

extern s32 female_guard_yelp_counter;
extern s16 female_guard_yelps[];
extern s32 male_guard_yelp_counter;
extern s16 male_guard_yelps[];

extern s16 metal_ricochet_SFX[3];

extern struct explosion_anim_group_info explosion_animation_table[];
extern struct explosion_death_animation g_ExplosionDeathAnims[];

void        chrFreeResources(PropRecord* prop);
void        chrDetectRooms(ChrRecord *);
void        chrSetMoving(ChrRecord *guard, bool unset);
f32         chrsGetAnimRate(void);
void        chrsSetAnimRate(f32);
PropRecord *chrInit(PropRecord *prop, Model *model, coord3d *pos, f32 angle, StandTile *tile, struct AIListRecord *ailist);
PropRecord *chrAllocate(struct Model *model, coord3d *pos, f32 angle, StandTile *tile, s32 ailist);
void        chrSetHiddenToRandom(ChrRecord *self);
void        chrCheckGuardsHeardSound(f32 noise);
ChrRecord  *chrFindByLiteralId(s32 index);
PropRecord *chrGetEquippedWeaponProp(ChrRecord *self, GUNHAND hand);
PropRecord *chrGetEquippedWeaponPropWithCheck(ChrRecord *ChrRecord, GUNHAND hand);
void        chrUpdateCollisionBounds(PropRecord *prop, struct rect4f **polygon, s32 *edges, f32 *y_out, f32 *ground);
void        chrGetChrWidthHeight(PropRecord *prop, f32 *width, f32 *height, f32 *always_20);
f32         chrGetChrGround(PropRecord *prop);
void        chrDropItems(struct ChrRecord *self);
s32         chrsGetNumSlots(void);
Gfx        *chrRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha);

void        chrAddHealth(ChrRecord *chr, f32 health);
void        chrSetMaxDamage(ChrRecord *chr, f32 maxdamage);
s32         propIsOfCdType(PropRecord* prop, s32 cdtypes);
s32         chrGetOnscreenRenderBounds(PropRecord *prop, struct coord3d *pos, struct coord2d *xbounds, struct coord2d *ybounds);
void        chrHandleBulletHit(struct ShotData *shot, struct BulletHit *bhit);

//tentative signature
s32         chrRootMoveCallback(Model *, coord3d *, coord3d *, f32 *);
void        chrCreateBloodStain(Model *model, s32 hitpart, ModelNode *root, struct coord3d *pos);
void        chrTestHit(PropRecord *prop, ShotData *shotdata);

#ifdef BUGFIX_R1
#endif

void chrsInit(void);
void bgChrsInit(void);
void chrsAllocateSlots(s32 count);
void chrsFreeAll(void);

#endif
