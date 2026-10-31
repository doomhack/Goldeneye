#ifndef _PLAYERDAMAGE_H_
#define _PLAYERDAMAGE_H_
#include <ultra64.h>
#include <bondtypes.h>

// Health, armour, damage and death, and explosions round the player

void playerTickDamageAndHealth(void);
void playerTickExplode(void);
void playerDie(void);
void playerTakeDamage(f32 damage_amount, f32 vectorx, f32 vectorz, s32 playerid, s32 affects_armor);
void playerTakeDamageFromAngle(f32 damage_amount, f32 angle, s32 playerid, s32 affects_armor);
int playerIsShowingDamage(void);
int playerIsShowingHealth(void);
void playerSetInvincible(u32 invincible);
u8 playerGetInvincible(void);
f32 currentPlayerGetHealth(void);
f32 currentPlayerGetArmor(void);
void currentPlayerAddArmor(f32 armour);
void playerSurroundWithExplosions(int delay);

extern s32 g_SurroundBondWithExplosionsFlag;

#endif
