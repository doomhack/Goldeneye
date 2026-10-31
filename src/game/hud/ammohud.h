#ifndef _AMMOHUD_H_
#define _AMMOHUD_H_
#include <ultra64.h>
#include <bondtypes.h>

// The ammo counter and the gun sight on the HUD, and the ammo shown on the watch

void gunSetGunAmmoVisible(s32 reason, bool enable);
Gfx *hudDrawImage(Gfx *gdl, struct sImageTableEntry *tconfig, f32 x, f32 y, f32 baseY, s32 flipX, f32 offsetY, s32 translucent, s32 red, s32 green, s32 blue, s32 alpha);
Gfx *hudDrawImageDefault(Gfx *gdl, sImageTableEntry *tconfig, f32 x, f32 y, f32 baseY, s32 flipX, f32 offsetY, s32 translucent);
Gfx *gunDrawHudString(Gfx *gdl, char *text, s32 x, s32 halign, s32 y, s32 valign, bool outline);
Gfx *gunDrawHudInteger(Gfx *gdl, s32 value, s32 x, s32 halign, s32 y, s32 valign, bool outline);
Gfx *hudRenderAmmo(Gfx *gdl);
Gfx *gunDrawWatchAmmoDisplay(Gfx *gdl);
void gunSetSightVisible(s32 reason, bool visible);
void gunDrawSight(Gfx * *gdl);

#endif
