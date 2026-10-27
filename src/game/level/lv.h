#ifndef _LVL_H_
#define _LVL_H_
#include <ultra64.h>
#include <bondconstants.h>

#define DEFAULT_AGENT_AI_ACCURACY_MODIFIER 0.6f
#define DEFAULT_AGENT_AI_DAMAGE_MODIFIER 0.5f
#define DEFAULT_AGENT_SOLO_AMMO_MULTIPLIER 2.0f
#define DEFAULT_AGENT_AI_REACTION_SPEED 0.2f

#define DEFAULT_SECRET_AGENT_AI_ACCURACY_MODIFIER 0.75f
#define DEFAULT_SECRET_AGENT_AI_DAMAGE_MODIFIER 0.75f
#define DEFAULT_SECRET_AGENT_SOLO_AMMO_MULTIPLIER 1.5f
#define DEFAULT_SECRET_AGENT_AI_REACTION_SPEED 0.5f

#define DEFAULT_00_AGENT_AI_ACCURACY_MODIFIER 1.0f
#define DEFAULT_00_AGENT_AI_DAMAGE_MODIFIER 1.0f
#define DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER 1.0f
#define DEFAULT_00_AGENT_AI_REACTION_SPEED 1.0f

#define DEFAULT_007_AI_ACCURACY_MODIFIER DEFAULT_00_AGENT_AI_ACCURACY_MODIFIER
#define DEFAULT_007_AI_DAMAGE_MODIFIER DEFAULT_00_AGENT_AI_DAMAGE_MODIFIER
#define DEFAULT_007_SOLO_AMMO_MULTIPLIER DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER
#define DEFAULT_007_AI_REACTION_SPEED DEFAULT_00_AGENT_AI_REACTION_SPEED


extern s32 g_ClockTimer;
extern s32 g_GlobalTimer;
extern f32 g_GlobalTimerDelta;
extern s32 g_LvTickCount;
extern u32 *g_FontDl;
extern char ramrom_data_target[0x380];
extern s32 record_slot_num;
extern u8 * address_demo_loaded;

#if defined(BUGFIX_R1)
extern f32 g_GlobalTimerDeltaRaw;
#endif

void lvInit(void);
Gfx * lvRender(Gfx *);
void lvSetDifficulty(DIFFICULTY arg0);
void lvTick(void);
void lvPlayerTick(void);
void lvStageUnload(void);
DIFFICULTY lvGetDifficulty(void);
void lvStageLoad(s32 stage);

void lvSetMpTimeLimit(s32 arg0);
void lvSetMpPointLimit(s32 arg0);
void lvSetControlsLocked(s32 arg0);
s32 lvGetControlsLocked(void);
void lvApplyDifficulty(void);
s32 lvGetCurrentStage(void);
s32        lvGetDrawWorld(void);
f32        lvGetMissionTimeSec(void);
f32        lvGetSystemPowerTimeSec(void);

void cleanup_REMOVED_(void);

#endif
