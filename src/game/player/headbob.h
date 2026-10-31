#ifndef _HEADBOB_H_
#define _HEADBOB_H_
#include <ultra64.h>

#include <bondtypes.h>

struct init_bond_anim_unk {
    u32 anim_id;
    f32 loopframe;
    f32 endframe;
    f32 speedMultiplier;

    f32 unk10;
    f32 maxSpeed; // the animation is used up to this speed
};

extern struct init_bond_anim_unk g_BondMoveAnimationSetup[];

void headBobFlipAnimation(void);
void headBobUpdateIdleRoll(void);
void headBobUpdate(f32 percent_speed, f32 speedsideways);
void headBobAdjustAnimation(f32 speed);
void headBobStartDeathAnimation(struct ModelAnimation *animnum, s32 flip, f32 fstarttime, f32 speed);
void headBobSetSpeed(f32 speed);

void headBobInit(void);
void headBobInitGaitModel(void);

f32 headBobGetBreathingValue(void);

#endif
