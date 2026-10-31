#ifndef _PLAYERBODY_H_
#define _PLAYERBODY_H_
#include <ultra64.h>
#include <bondtypes.h>

extern s32 g_bondviewBondDeathAnimations[];
extern s32 g_bondviewBondDeathAnimationsCount;
void playerLoadBody(void);
Gfx *playerRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha);
void playerRemoveBody(void);
void playerSelectCuff(Model *model, ModelFileHeader *header, s32 switchindex);
s32 playerTick(PropRecord *prop);
extern struct firing_anim_struct firing_animation_groups[][6];

void playerBodyRemoveGun(enum GUNHAND hand);
void playerBodyGiveGun(enum GUNHAND hand);
PROP weaponGetChrModel(ITEM_IDS item);

#endif
