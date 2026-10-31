#ifndef _PLAYERINPUT_H_
#define _PLAYERINPUT_H_
#include <ultra64.h>
#include <bondtypes.h>

// Input: the controller each frame (playerProcessInput), auto-aim, and look and walk speeds

void currentPlayerSetLookAheadSetting(bool enabled);
void currentPlayerSetYAutoAimEnabled(bool enabled);
bool currentPlayerGetYAutoAimEnabled(void);
bool currentPlayerGetYAutoAimEnabledRedirect(void);
bool currentPlayerGetIsAiming(void);
void playerUpdateAutoAimY(struct PropRecord *autoaim_target, f32 auto_aim_y);
void currentPlayerSetXAutoAimEnabled(bool enabled);
bool currentPlayerGetXAutoAimEnabled(void);
bool currentPlayerGetXAutoAimEnabledRedirect(void);
void playerUpdateAutoAimX(struct PropRecord *autoaim_target, f32 auto_aim_x);
void playerUpdateSpeedSideways(s32 dir);
void playerUpdateSpeedForwards(s32 dir);
f32 playerScaleLookSpeedVerta(f32 value);
void playerUpdateSpeedVerta(f32 value);
f32 playerScaleLookSpeedTheta(f32 speed);
void playerUpdateSpeedTheta(f32 value);
void playerProcessInput(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons);

#endif
