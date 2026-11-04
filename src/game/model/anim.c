/**
 * The animation table: the guard, player and vehicle animations.
 *
 * At boot the animation headers (ModelAnimation) and all their frames (1.4 MB) are copied
 * from ROM into the permanent pool, and their offsets turned into pointers. (The original
 * game left the frames in ROM and copied each one it drew by DMA, several times a frame.)
 */
#include <ultra64.h>
#include <memp.h>
#include "game/model/anim.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "bondgame.h"
#include "rom.h"
#include <macro.h>

// Animation table ptr
struct animation_table_data * g_AnimTable;

/** Every animation's frames, copied from the animation_entries segment. */
static u8 *s_AnimFrames;

ModelAnimation *animation_table_ptrs1[] = {
    ANIM_OFFSET(PTR_ANIM_idle),
    ANIM_OFFSET(PTR_ANIM_fire_standing),
    ANIM_OFFSET(PTR_ANIM_fire_standing_fast),
    ANIM_OFFSET(PTR_ANIM_fire_hip),
    ANIM_OFFSET(PTR_ANIM_fire_shoulder_left),
    ANIM_OFFSET(PTR_ANIM_fire_turn_right1),
    ANIM_OFFSET(PTR_ANIM_fire_turn_right2),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_right_leg),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_left_leg),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_left),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_right),
    ANIM_OFFSET(PTR_ANIM_fire_roll_left),
    ANIM_OFFSET(PTR_ANIM_fire_roll_right1),
    ANIM_OFFSET(PTR_ANIM_fire_roll_left_fast),
    ANIM_OFFSET(PTR_ANIM_hit_left_shoulder),
    ANIM_OFFSET(PTR_ANIM_hit_right_shoulder),
    ANIM_OFFSET(PTR_ANIM_hit_left_arm),
    ANIM_OFFSET(PTR_ANIM_hit_right_arm),
    ANIM_OFFSET(PTR_ANIM_hit_left_hand),
    ANIM_OFFSET(PTR_ANIM_hit_right_hand),
    ANIM_OFFSET(PTR_ANIM_hit_left_leg),
    ANIM_OFFSET(PTR_ANIM_hit_right_leg),
    ANIM_OFFSET(PTR_ANIM_death_genitalia),
    ANIM_OFFSET(PTR_ANIM_hit_neck),
    ANIM_OFFSET(PTR_ANIM_death_neck),
    ANIM_OFFSET(PTR_ANIM_death_stagger_back_to_wall),
    ANIM_OFFSET(PTR_ANIM_death_forward_face_down),
    ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up),
    ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1),
    ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right),
    ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right),
    ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left),
    ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left),
    ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard),
    ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft),
    ANIM_OFFSET(PTR_ANIM_death_fetal_position_right),
    ANIM_OFFSET(PTR_ANIM_death_fetal_position_left),
    ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2),
    ANIM_OFFSET(PTR_ANIM_side_step_left),
    ANIM_OFFSET(PTR_ANIM_fire_roll_right2),
    ANIM_OFFSET(PTR_ANIM_walking),
    ANIM_OFFSET(PTR_ANIM_sprinting),
    ANIM_OFFSET(PTR_ANIM_running),
    ANIM_OFFSET(PTR_ANIM_bond_eye_walk),
    ANIM_OFFSET(PTR_ANIM_bond_eye_fire),
    ANIM_OFFSET(PTR_ANIM_bond_watch),
    ANIM_OFFSET(PTR_ANIM_surrendering_armed),
    ANIM_OFFSET(PTR_ANIM_surrendering_armed_drop_weapon),
    ANIM_OFFSET(PTR_ANIM_fire_walking),
    ANIM_OFFSET(PTR_ANIM_fire_running),
    ANIM_OFFSET(PTR_ANIM_null50),
    ANIM_OFFSET(PTR_ANIM_null51),
    ANIM_OFFSET(PTR_ANIM_fire_jump_to_side_left),
    ANIM_OFFSET(PTR_ANIM_fire_jump_to_side_right),
    ANIM_OFFSET(PTR_ANIM_hit_butt_long),
    ANIM_OFFSET(PTR_ANIM_hit_butt_short),
    ANIM_OFFSET(PTR_ANIM_death_head),
    ANIM_OFFSET(PTR_ANIM_death_left_leg),
    ANIM_OFFSET(PTR_ANIM_slide_right),
    ANIM_OFFSET(PTR_ANIM_slide_left),
    ANIM_OFFSET(PTR_ANIM_jump_backwards),
    ANIM_OFFSET(PTR_ANIM_extending_left_hand),
    ANIM_OFFSET(PTR_ANIM_fire_throw_grenade),
    ANIM_OFFSET(PTR_ANIM_spotting_bond),
    ANIM_OFFSET(PTR_ANIM_look_around),
    ANIM_OFFSET(PTR_ANIM_fire_standing_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_fire_standing_draw_one_handed_weapon_fast),
    ANIM_OFFSET(PTR_ANIM_fire_standing_draw_one_handed_weapon_slow),
    ANIM_OFFSET(PTR_ANIM_fire_hip_one_handed_weapon_fast),
    ANIM_OFFSET(PTR_ANIM_fire_hip_one_handed_weapon_slow),
    ANIM_OFFSET(PTR_ANIM_fire_hip_forward_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_fire_standing_right_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_fire_step_right_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_fire_standing_left_one_handed_weapon_slow),
    ANIM_OFFSET(PTR_ANIM_fire_standing_left_one_handed_weapon_fast),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_forward_one_handed_weapon_slow),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_forward_one_handed_weapon_fast),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_right_one_handed_weapon_slow),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_right_one_handed_weapon_fast),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_left_one_handed_weapon_slow),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_left_one_handed_weapon_fast),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_left_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_aim_walking_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_aim_walking_left_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_aim_walking_right_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_aim_running_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_aim_running_right_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_aim_running_left_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_aim_sprinting_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_running_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_sprinting_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_null91),
    ANIM_OFFSET(PTR_ANIM_null92),
    ANIM_OFFSET(PTR_ANIM_null93),
    ANIM_OFFSET(PTR_ANIM_null94),
    ANIM_OFFSET(PTR_ANIM_null95),
    ANIM_OFFSET(PTR_ANIM_null96),
    ANIM_OFFSET(PTR_ANIM_draw_one_handed_weapon_and_look_around),
    ANIM_OFFSET(PTR_ANIM_draw_one_handed_weapon_and_stand_up),
    ANIM_OFFSET(PTR_ANIM_aim_one_handed_weapon_left_right),
    ANIM_OFFSET(PTR_ANIM_cock_one_handed_weapon_and_turn_around),
    ANIM_OFFSET(PTR_ANIM_holster_one_handed_weapon_and_cross_arms),
    ANIM_OFFSET(PTR_ANIM_cock_one_handed_weapon_turn_around_and_stand_up),
    ANIM_OFFSET(PTR_ANIM_draw_one_handed_weapon_and_turn_around),
    ANIM_OFFSET(PTR_ANIM_step_forward_and_hold_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_holster_one_handed_weapon_and_adjust_suit),
    ANIM_OFFSET(PTR_ANIM_idle_unarmed),
    ANIM_OFFSET(PTR_ANIM_walking_unarmed),
    ANIM_OFFSET(PTR_ANIM_fire_walking_dual_wield),
    ANIM_OFFSET(PTR_ANIM_fire_walking_dual_wield_hands_crossed),
    ANIM_OFFSET(PTR_ANIM_fire_running_dual_wield),
    ANIM_OFFSET(PTR_ANIM_fire_running_dual_wield_hands_crossed),
    ANIM_OFFSET(PTR_ANIM_fire_sprinting_dual_wield),
    ANIM_OFFSET(PTR_ANIM_fire_sprinting_dual_wield_hands_crossed),
    ANIM_OFFSET(PTR_ANIM_walking_female),
    ANIM_OFFSET(PTR_ANIM_running_female),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_dual_wield),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_dual_wield_left),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_dual_wield_right),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_dual_wield_hands_crossed),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_dual_wield_hands_crossed_left),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_dual_wield_hands_crossed_right),
    ANIM_OFFSET(PTR_ANIM_fire_standing_dual_wield),
    ANIM_OFFSET(PTR_ANIM_fire_standing_dual_wield_left),
    ANIM_OFFSET(PTR_ANIM_fire_standing_dual_wield_right),
    ANIM_OFFSET(PTR_ANIM_fire_standing_dual_wield_hands_crossed_left),
    ANIM_OFFSET(PTR_ANIM_fire_standing_dual_wield_hands_crossed_right),
    ANIM_OFFSET(PTR_ANIM_fire_standing_aiming_down_sights),
    ANIM_OFFSET(PTR_ANIM_fire_kneel_aiming_down_sights),
    ANIM_OFFSET(PTR_ANIM_hit_taser),
    ANIM_OFFSET(PTR_ANIM_death_explosion_forward),
    ANIM_OFFSET(PTR_ANIM_death_explosion_left1),
    ANIM_OFFSET(PTR_ANIM_death_explosion_back_left),
    ANIM_OFFSET(PTR_ANIM_death_explosion_back1),
    ANIM_OFFSET(PTR_ANIM_death_explosion_right),
    ANIM_OFFSET(PTR_ANIM_death_explosion_forward_right1),
    ANIM_OFFSET(PTR_ANIM_death_explosion_back2),
    ANIM_OFFSET(PTR_ANIM_death_explosion_forward_roll),
    ANIM_OFFSET(PTR_ANIM_death_explosion_forward_face_down),
    ANIM_OFFSET(PTR_ANIM_death_explosion_left2),
    ANIM_OFFSET(PTR_ANIM_death_explosion_forward_right2),
    ANIM_OFFSET(PTR_ANIM_death_explosion_forward_right2_alt),
    ANIM_OFFSET(PTR_ANIM_death_explosion_forward_right3),
    ANIM_OFFSET(PTR_ANIM_null143),
    ANIM_OFFSET(PTR_ANIM_null144),
    ANIM_OFFSET(PTR_ANIM_null145),
    ANIM_OFFSET(PTR_ANIM_null146),
    ANIM_OFFSET(PTR_ANIM_running_hands_up),
    ANIM_OFFSET(PTR_ANIM_sprinting_hands_up),
    ANIM_OFFSET(PTR_ANIM_aim_and_blow_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_aim_one_handed_weapon_left),
    ANIM_OFFSET(PTR_ANIM_aim_one_handed_weapon_right),
    ANIM_OFFSET(PTR_ANIM_conversation),
    ANIM_OFFSET(PTR_ANIM_drop_weapon_and_show_fight_stance),
    ANIM_OFFSET(PTR_ANIM_yawning),
    ANIM_OFFSET(PTR_ANIM_swatting_flies),
    ANIM_OFFSET(PTR_ANIM_scratching_leg),
    ANIM_OFFSET(PTR_ANIM_scratching_butt),
    ANIM_OFFSET(PTR_ANIM_adjusting_crotch),
    ANIM_OFFSET(PTR_ANIM_sneeze),
    ANIM_OFFSET(PTR_ANIM_conversation_cleaned),
    ANIM_OFFSET(PTR_ANIM_conversation_listener),
    ANIM_OFFSET(PTR_ANIM_startled_and_looking_around),
    ANIM_OFFSET(PTR_ANIM_laughing_in_disbelief),
    ANIM_OFFSET(PTR_ANIM_surrendering_unarmed),
    ANIM_OFFSET(PTR_ANIM_coughing_standing),
    ANIM_OFFSET(PTR_ANIM_coughing_kneel1),
    ANIM_OFFSET(PTR_ANIM_coughing_kneel2),
    ANIM_OFFSET(PTR_ANIM_standing_up),
    ANIM_OFFSET(PTR_ANIM_null169),
    ANIM_OFFSET(PTR_ANIM_dancing),
    ANIM_OFFSET(PTR_ANIM_dancing_one_handed_weapon),
    ANIM_OFFSET(PTR_ANIM_keyboard_right_hand1),
    ANIM_OFFSET(PTR_ANIM_keyboard_right_hand2),
    ANIM_OFFSET(PTR_ANIM_keyboard_left_hand),
    ANIM_OFFSET(PTR_ANIM_keyboard_right_hand_tapping),
    ANIM_OFFSET(PTR_ANIM_bond_eye_fire_alt),
    ANIM_OFFSET(PTR_ANIM_dam_jump),
    ANIM_OFFSET(PTR_ANIM_surface_vent_jump),
    ANIM_OFFSET(PTR_ANIM_cradle_jump),
    ANIM_OFFSET(PTR_ANIM_cradle_fall),
    ANIM_OFFSET(PTR_ANIM_credits_bond_kissing),
    ANIM_OFFSET(PTR_ANIM_credits_natalya_kissing),
    0
};

ModelAnimation *animation_table_ptrs2[] = {
    ANIM_OFFSET(PTR_ANIM_helicopter_cradle),
    ANIM_OFFSET(PTR_ANIM_plane_runway),
    ANIM_OFFSET(PTR_ANIM_helicopter_takeoff),
    0
};


extern u8 _animation_entriesSegmentRomStart[];
extern u8 _animation_entriesSegmentRomEnd[];

/** The memory animLoadTable needs for the animation frames, in the permanent pool. */
u32 animGetFramesSize(void)
{
    return ALIGN16_a(_animation_entriesSegmentRomEnd - _animation_entriesSegmentRomStart);
}

/**
 * Where the animation frames are kept: straight below the z-buffer, sharing its bank. Only
 * the CPU reads them, so they cost the RDP's depth accesses little; the stage pool ends here.
 */
u8 *animGetFramesStart(void)
{
    return (u8 *) &_zbufSegmentStart - animGetFramesSize();
}

/**
 * Turns each animation offset in a NULL-terminated table into a pointer, and resolves
 * the offsets inside that animation's header. ANIM_UNUSED slots are left alone.
 */
void animResolveTable(ModelAnimation **table)
{
    ModelAnimation **entry;

    for (entry = table; *entry != NULL; entry++)
    {
        ModelAnimation *anim;

        if (*entry == ANIM_UNUSED)
        {
            continue;
        }

        anim = (ModelAnimation *)&g_AnimTable->data[(u32)*entry];
        anim->bitDescriptors = (ModelAnimBitField *)&g_AnimTable->data[(u32)anim->bitDescriptors];
        anim->bitStream = &g_AnimTable->data[(u32)anim->bitStream];
        anim->address += (u32) s_AnimFrames;
        *entry = anim;
    }
}

void animLoadTable(void)
{
    s32 animsDataSegmentSize;
    
    animsDataSegmentSize = (s32)&_animation_dataSegmentEnd - (s32)&_animation_dataSegmentStart;
    
    g_AnimTable = mempAlloc(animsDataSegmentSize, MEMPOOL_PERMANENT);

    romCopy(g_AnimTable, &_animation_dataSegmentRomStart, animsDataSegmentSize);

    s_AnimFrames = animGetFramesStart();
    romCopy(s_AnimFrames, _animation_entriesSegmentRomStart, animGetFramesSize());
    animResolveTable(animation_table_ptrs1);
    animResolveTable(animation_table_ptrs2);
}
