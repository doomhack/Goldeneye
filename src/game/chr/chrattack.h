#ifndef _CHRATTACK_H_
#define _CHRATTACK_H_
#include <ultra64.h>
#include <bondtypes.h>

/* Starting an attack */
void chrAttackStand(ChrRecord *self, s32 attack_type, s32 entityid);
void chrAttackKneel(ChrRecord *self, s32 attack_type, s32 entityid);
void chrInitActAttackWalk(ChrRecord *chr, s32 run);
void chrInitActAttackRoll(ChrRecord *chr, GUNHAND side);
void chrAttackSetEndFrame(ChrRecord *self);
f32  chrGetAngleToTarget(ChrRecord *self, s32 attack_type, s32 entityid);

/* Action ticks, from chrActionTick */
void chrTickAttack(ChrRecord *self);
void chrTickAttackRoll(ChrRecord *self);
void chrTickAttackWalk(ChrRecord *self);

/* Aiming */
s32  chrSetSubroty(ChrRecord *self, s32 state, f32 endframe, f32 speed, f32 angleoffset);
f32  chrGetSubrotySideback(ChrRecord *self);
void chrUpdateAimendbackShoulders(ChrRecord *self, struct weapon_firing_animation_table *anim, s32 leftgun, s32 rightgun, f32 next);
f32  chrGetAimLimitAngle(f32 sqdist);

/* Firing */
void chrSetFiring(ChrRecord *self, s32 hand, s32 firing);
void chrStopFiring(ChrRecord *self);
void chrTriggerFireWeapon(ChrRecord *self);
s32  chrGetHandPos(ChrRecord *self, GUNHAND hand, coord3d *pos);

#endif
