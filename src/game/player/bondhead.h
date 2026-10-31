#ifndef _BONDHEAD_H_
#define _BONDHEAD_H_
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

void bheadFlipAnimation(void);
void bheadUpdateIdleRoll(void);
void bheadUpdate(f32 percent_speed, f32 speedsideways);
void bheadAdjustAnimation(f32 speed);
void bheadStartDeathAnimation(struct ModelAnimation *animnum, s32 flip, f32 fstarttime, f32 speed);
void bheadSetSpeed(f32 speed);

void bheadInit(void);
void bheadInitGaitModel(void);

f32 bheadGetBreathingValue(void);

#endif
