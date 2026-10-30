#ifndef _CHRAI_H_
#define _CHRAI_H_
#include <ultra64.h>
#include "game/prop/prop.h"

#include <bondtypes.h>

#define CHRAI_TICKRATE_F 60.0f

#define SFX_RELATED_LEN 8
#define MAX_PROPS 600
#define MAX_WEAPON_SLOTS 30
#define MAX_HAT_SLOTS 10
#define MAX_AMMO_CRATES 20
#define PROJECTILES_ARR_MAX 20
#define PROXIMITYARRMAX 30 /*canonical name for max mines*/
#define EMBEDMENT_ARR_MAX 40
#define ONSCREEN_PROP_LIST_LEN 500


extern struct SetupPtrs g_SetupPtrs;


extern stagesetup g_CurrentSetup;

extern PropRecord *                       g_InteractProp;
extern sfxRecord                          sfx_related[];
extern struct WeaponObjRecord             g_WeaponSlots[];
extern struct HatRecord                   g_HatSlots[];
extern struct AmmoCrateRecord             g_AmmoCrates[];

extern stagesetup                        *g_ptrStageSetupFile;
extern char *setup_text_pointers[];

s32                aiCommandSize(AIRecord *list, s32 offset);
AIRecord          *ailistFindById(s32 ID);
void               aiForObject(PropDefHeaderRecord *Entityp);
s32                counterOpDoorIsScripted(DoorRecord *door);
void ai(PropDefHeaderRecord *Entityp, PROP_TYPE EntityType);
void aiSoundsTick(void);

#endif
