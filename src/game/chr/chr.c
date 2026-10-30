/**
 * Guard slots and the per-frame guard tick. chrsAllocateSlots makes one ChrRecord per
 * guard in the setup file plus spares for guards spawned in play. chrTick runs from the
 * prop loop for each player's view; the first pass advances the guard (AI and action via
 * chrActionTick, animation, moving with collision), every pass draws it. Also here:
 * shot hit tests against the body, and hearing.
 * See docs/subsystems/guards.md.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <macro.h>
#include <random.h>
#include <snd.h>
#include <math.h>
#include "bondaicommands.h"
#include "game/world/bg.h"
#include "game/menu/cheat.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/chraction.h"
#include "game/model/chrobjdata.h"
#include "game/gfx/dyn.h"
#include "game/gfx/vtxstore.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/save/file.h"
#include "game/prop/propobj.h"
#include "game/prop/explosion.h"
#include "game/player/gun.h"
#include "game/model/anim.h"
#include "joy.h"
#include "game/level/lv.h"
#include "game/text/language.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/player/player.h"
#include "game/prop/propobj.h"
#include "game/nav/stan.h"
#include "game/model/model.h"
#include "game/world/surface.h"
#include "game/math/physics.h"
#include "game/world/bgfog.h"
#include "game/math/line_tri_intersect.h"

#ifdef VERSION_EU
#define GROUND_SMOOTH_FACTOR 0.118799984f /* 0x3DF34D68 (PAL-scaled 0.1) */
#define FALLSPEED_DECAY      0.8812f      /* 0x3F619653 (PAL-scaled 0.9) */
#else
#define GROUND_SMOOTH_FACTOR 0.100000024f /* 0x3DCCCCD0 */
#define FALLSPEED_DECAY      0.9f         /* 0x3F666666 */
#endif

// forward declarations

void chrUpdateAimProperties( ChrRecord *self);
void chrUpdateAnim( ChrRecord *chr, s32 tickamount);
f32  file007Health(void);

// end forward declarations

// data
f32 g_ChrAnimRate = 0;
s32 g_ChrDebugAnimOverride = 0;

s32 g_ChrDebugAnimIndex = 0;
s32 g_ChrDebugPaused = 0;
s32 g_ChrDebugStep = 0;

/*
 * D:8002C914
 * Per-body-part hit reaction table, indexed by HITTARGET.
 * Legend: iN/iSz = impact puff count/size, bN/bSz = back puff count/size,
 *         u8/u14 = unused, dN/fN = death/flinch anim counts (filled at init by initWeaponAnimGroups).
 */
/*                       part,                       iN,  u8,  iSz,   bN, u14,  bSz,  deathAnims,           dN, flinchAnims,          fN */
struct ChrHitReaction g_HitReactionTable[] = {     
/* HIT_NULL_PART      */ {0                        , 0,   0,   0,     0,  0,    0,    NULL,                 0,  NULL,                  0},
/* HIT_LEFT_FOOT      */ {1                        , 1,   0,   17.0,  3,  0,    34.0, death_left_foot,      0,  flinch_left_foot,      0},
/* HIT_LEFT_LEG       */ {2                        , 1,   0,   17.0,  3,  0,    39.0, death_left_leg,       0,  flinch_left_leg,       0},
/* HIT_LEFT_THIGH     */ {3                        , 1,   0,   21.0,  3,  0,    43.0, death_left_thigh,     0,  flinch_left_thigh,     0},
/* HIT_RIGHT_FOOT     */ {4                        , 1,   0,   17.0,  3,  0,    34.0, death_right_foot,     0,  flinch_right_foot,     0},
/* HIT_RIGHT_LEG      */ {5                        , 1,   0,   17.0,  3,  0,    39.0, death_right_leg,      0,  flinch_right_leg,      0},
/* HIT_RIGHT_THIGH    */ {6                        , 1,   0,   21.0,  3,  0,    43.0, death_right_thigh,    0,  flinch_right_thigh,    0},
/* HIT_PELVIS         */ {7                        , 1,   0,   21.0,  3,  0,    52.0, death_pelvis,         0,  flinch_pelvis,         0},
/* HIT_HEAD           */ {8                        , 1,   0,   21.0,  3,  0,    43.0, death_head,           0,  flinch_head,           0},
/* HIT_LEFT_HAND      */ {9                        , 1,   0,   17.0,  3,  0,    34.0, death_left_hand,      0,  flinch_left_hand,      0},
/* HIT_LEFT_ARM       */ {0xA                      , 1,   0,   17.0,  3,  0,    43.0, death_left_arm,       0,  flinch_left_arm,       0},
/* HIT_LEFT_SHOULDER  */ {0xB                      , 1,   0,   21.0,  3,  0,    52.0, death_left_shoulder,  0,  flinch_left_shoulder,  0},
/* HIT_RIGHT_HAND     */ {0xC                      , 1,   0,   17.0,  3,  0,    34.0, death_right_hand,     0,  flinch_right_hand,     0},
/* HIT_RIGHT_ARM      */ {0xD                      , 1,   0,   17.0,  3,  0,    43.0, death_right_arm,      0,  flinch_right_arm,      0},
/* HIT_RIGHT_SHOULDER */ {0xE                      , 1,   0,   21.0,  3,  0,    52.0, death_right_shoulder, 0,  flinch_right_shoulder, 0},
/* HIT_CHEST          */ {0xF                      , 1,   0,   26.0,  3,  0,    60.0, death_chest,          0,  flinch_chest,          0},
/* HIT_GUN            */ {0x64                     , 1,   0,   26.0,  0,  0,    0.0,  death_gun,            0,  flinch_gun,            0},
/* HIT_HAT            */ {0x6E                     , 1,   0,   21.0,  0,  0,    0.0,  NULL,                 0,  NULL,                  0},
/* terminator         */ {0xFFFFFFFF               , 0,   0,   0.0,   0,  0,    0.0,  NULL,                 0,  NULL,                  0},
};

s32 g_ChrUnused = 0; // Set to 0 but otherwise never used.
s32 g_ChrShowPatrols = FALSE;
s32 g_NextChrNum = 5000;
ChrRecord *g_ChrSlots = 0;
s32 g_NumChrSlots = 0;



rgba_u8 gBloodColour = { {0x5a, 0, 0, 0}};



u32 num_bodies = 0;
u32 num_male_heads = 0;
u32 num_female_heads = 0;

s32 list_of_bodies[] = {
    BODY_Jungle_Commando,BODY_St_Petersburg_Guard,BODY_Russian_Soldier,BODY_Russian_Infantry,
    BODY_Janus_Special_Forces,BODY_Brosnan_Tuxedo,BODY_Boris,BODY_Ourumov,
    BODY_Trevelyan_Janus,BODY_Valentin_,BODY_Xenia,BODY_Baron_Samedi,
    BODY_Jaws,BODY_Mayday,BODY_Oddjob,BODY_Natalya_Skirt,
    BODY_Janus_Marine,BODY_Russian_Commandant,BODY_Siberian_Guard_1_Mishkin,BODY_Naval_Officer,
    BODY_Siberian_Special_Forces,BODY_Special_Operations_Uniform,BODY_Formal_Wear,BODY_Jungle_Fatigues,
    BODY_Unused_Female,BODY_Rosika,BODY_Scientist_2_Female,BODY_Civilian_1_Female,
    BODY_Unused_Male_1,BODY_Unused_Male_2,BODY_Civilian_4,BODY_Civilian_2,
    BODY_Civilian_3,BODY_Scientist_1_Male,BODY_Brosnan_Tuxedo,BODY_Brosnan_Tuxedo,
    BODY_Brosnan_Tuxedo,BODY_Helicopter_Pilot,BODY_Siberian_Guard_2,BODY_Arctic_Commando,
    BODY_Moonraker_Elite_1_Male,BODY_Moonraker_Elite_2_Female,-1,
};

s32 random_male_heads[] = {
    HEAD_Male_Jim,HEAD_Male_Chris,HEAD_Male_Lee,HEAD_Male_Graeme,HEAD_Male_Steve_H,
    HEAD_Male_Neil,HEAD_Male_Robin,HEAD_Male_Des,HEAD_Male_Grant,HEAD_Male_Dave_Dr_Doak,
    HEAD_Male_Karl,HEAD_Male_Alan,HEAD_Male_Pete,HEAD_Male_Martin,HEAD_Male_Mark,
    HEAD_Male_Duncan,HEAD_Male_Shaun,HEAD_Male_Dwayne,HEAD_Male_B,HEAD_Male_Steve_Ellis,
    HEAD_Male_Joel,HEAD_Male_Scott,HEAD_Male_Joe_Altered,HEAD_Male_Ken,HEAD_Male_Joe,
    -1
};

s32 random_female_heads[] = {
    HEAD_Female_Sally,HEAD_Female_Marion_Rosika,HEAD_Female_Mandy,HEAD_Female_Vivien, -1
};

u32 current_random_body = 0;
u32 current_random_male_head = 0;
u32 current_random_female_head = 0;

/*
* Enemy accuracy modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiAccuracyModifier = 1.0f;

/*
* Enemy damage modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiDamageModifier = 1.0f;

f32 g_AiHealthModifier = 1.0f;

/*
* Enemy reaction speed modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiReactionSpeed = 1.0f;

/**
 * Count of number of guards that have recently seen bond:
 *     (guard->lastseetarget60 > 0) && (g_GlobalTimer - guard->lastseetarget60 < 120)
 * Updated every tick.
*/
s32 g_SeenBondRecentlyGuardCount = 0;

struct StruckAnim death_left_foot[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 0, -1.0, 0.5, 0, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 0, -1.0, 0.5, 0, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 1, -1.0, 0.5, 0, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 1, -1.0, 0.5, 0, 23.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_left_leg[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 0, -1.0, 0.5, 0, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 0, -1.0, 0.5, 0, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 1, -1.0, 0.5, 0, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 1, -1.0, 0.5, 0, 23.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_left_thigh[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 0, -1.0, 0.5, 1, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 0, -1.0, 0.5, 1, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 1, -1.0, 0.5, 1, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 1, -1.0, 0.5, 1, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_left_leg), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_right_foot[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 0, -1.0, 0.5, 0, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 0, -1.0, 0.5, 0, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 1, -1.0, 0.5, 0, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 1, -1.0, 0.5, 0, 26.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_right_leg[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 0, -1.0, 0.5, 0, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 0, -1.0, 0.5, 0, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 1, -1.0, 0.5, 0, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 1, -1.0, 0.5, 0, 26.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_right_thigh[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 0, -1.0, 0.5, 1, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 0, -1.0, 0.5, 1, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 1, -1.0, 0.5, 1, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 1, -1.0, 0.5, 1, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_left_leg), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_pelvis[] = {
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 0, -1.0, 0.5, 0, 55.0, 39.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 1, -1.0, 0.5, 0, 55.0, 39.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up), 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up), 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1), 0, -1.0, 0.5, 1, 29.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1), 1, -1.0, 0.5, 1, 29.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard), 0, -1.0, 0.5, 0, 97.0, 64.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard), 1, -1.0, 0.5, 0, 97.0, 64.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 0, -1.0, 0.5, 0, 31.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 1, -1.0, 0.5, 0, 31.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_left), 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_left), 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2), 0, -1.0, 0.5, 0, 28.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2), 1, -1.0, 0.5, 0, 28.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_genitalia), 0, -1.0, 0.5, 0, 79.0, 415.0 },
    { ANIM_OFFSET(PTR_ANIM_death_genitalia), 1, -1.0, 0.5, 0, 79.0, 415.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_head[] = {
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 0, -1.0, 0.5, 0, 55.0, 39.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 1, -1.0, 0.5, 0, 55.0, 39.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up), 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up), 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1), 0, -1.0, 0.5, 1, 29.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1), 1, -1.0, 0.5, 1, 29.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 0, -1.0, 0.5, 1, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 1, -1.0, 0.5, 1, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 0, -1.0, 0.5, 1, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 1, -1.0, 0.5, 1, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 0, -1.0, 0.5, 1, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 1, -1.0, 0.5, 1, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 0, -1.0, 0.5, 1, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 1, -1.0, 0.5, 1, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard), 0, -1.0, 0.5, 0, 97.0, 64.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard), 1, -1.0, 0.5, 0, 97.0, 64.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 0, -1.0, 0.5, 0, 94.0, 66.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 1, -1.0, 0.5, 0, 94.0, 66.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 0, -1.0, 0.5, 0, 31.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 1, -1.0, 0.5, 0, 31.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_left), 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_left), 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2), 0, -1.0, 0.5, 0, 28.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2), 1, -1.0, 0.5, 0, 28.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_neck), 0, -1.0, 0.5, 0, 87.0, 203.0 },
    { ANIM_OFFSET(PTR_ANIM_death_neck), 1, -1.0, 0.5, 0, 87.0, 203.0 },
    { ANIM_OFFSET(PTR_ANIM_death_head), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_head), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_left_hand[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 0, -1.0, 0.5, 0, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 0, -1.0, 0.5, 0, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 1, -1.0, 0.5, 0, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 1, -1.0, 0.5, 0, 23.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_left_arm[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 0, -1.0, 0.5, 0, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 0, -1.0, 0.5, 0, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 1, -1.0, 0.5, 0, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 1, -1.0, 0.5, 0, 23.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_left_shoulder[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 0, -1.0, 0.5, 1, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 0, -1.0, 0.5, 1, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 1, -1.0, 0.5, 1, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 1, -1.0, 0.5, 1, 23.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_right_hand[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 0, -1.0, 0.5, 0, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 0, -1.0, 0.5, 0, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 1, -1.0, 0.5, 0, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 1, -1.0, 0.5, 0, 26.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_right_arm[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 0, -1.0, 0.5, 0, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 0, -1.0, 0.5, 0, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 1, -1.0, 0.5, 0, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 1, -1.0, 0.5, 0, 26.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_right_shoulder[] = {
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 0, -1.0, 0.5, 1, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 0, -1.0, 0.5, 1, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 1, -1.0, 0.5, 1, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 1, -1.0, 0.5, 1, 26.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_chest[] = {
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 0, -1.0, 0.5, 0, 55.0, 39.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 1, -1.0, 0.5, 0, 55.0, 39.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up), 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up), 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1), 0, -1.0, 0.5, 1, 29.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1), 1, -1.0, 0.5, 1, 29.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 0, -1.0, 0.5, 1, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_right), 1, -1.0, 0.5, 1, 25.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 0, -1.0, 0.5, 1, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_right), 1, -1.0, 0.5, 1, 23.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 0, -1.0, 0.5, 1, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_down_left), 1, -1.0, 0.5, 1, 27.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 0, -1.0, 0.5, 1, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_spin_face_up_left), 1, -1.0, 0.5, 1, 26.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard), 0, -1.0, 0.5, 0, 97.0, 64.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard), 1, -1.0, 0.5, 0, 97.0, 64.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 0, -1.0, 0.5, 0, 94.0, 66.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 1, -1.0, 0.5, 0, 94.0, 66.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 0, -1.0, 0.5, 0, 31.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 1, -1.0, 0.5, 0, 31.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_left), 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_left), 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2), 0, -1.0, 0.5, 0, 28.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2), 1, -1.0, 0.5, 0, 28.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_gun[] = {
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 0, -1.0, 0.5, 0, 55.0, 39.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 1, -1.0, 0.5, 0, 55.0, 39.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up), 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_spin_face_up), 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1), 0, -1.0, 0.5, 1, 29.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up1), 1, -1.0, 0.5, 1, 29.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard), 0, -1.0, 0.5, 0, 97.0, 64.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_hard), 1, -1.0, 0.5, 0, 97.0, 64.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 0, -1.0, 0.5, 0, 94.0, 66.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 1, -1.0, 0.5, 0, 94.0, 66.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 0, -1.0, 0.5, 0, 31.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 1, -1.0, 0.5, 0, 31.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_left), 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_left), 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2), 0, -1.0, 0.5, 0, 28.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_backward_fall_face_up2), 1, -1.0, 0.5, 0, 28.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim death_stagger[] = {
    { ANIM_OFFSET(PTR_ANIM_death_stagger_back_to_wall), 0, -1.0, 0.5, 0, 67.0, 54.0 },
    { ANIM_OFFSET(PTR_ANIM_death_stagger_back_to_wall), 1, -1.0, 0.5, 0, 67.0, 54.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_left_foot[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_left_leg), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_right_leg), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_left_leg[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_left_leg), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_right_leg), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_left_thigh[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_left_leg), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_right_leg), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_left_leg), 1, 20.0, 0.40000001, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_right_foot[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_right_leg), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_left_leg), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_right_leg[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_right_leg), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_left_leg), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_right_thigh[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_right_leg), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_left_leg), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_left_leg), 0, 20.0, 0.40000001, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_pelvis[] = {
    { ANIM_OFFSET(PTR_ANIM_death_genitalia), 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_genitalia), 1, 30.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 1, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 0, 15.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 1, 15.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 0, 10.0, 0.25, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_fetal_position_right), 1, 10.0, 0.25, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_head[] = {
    { ANIM_OFFSET(PTR_ANIM_death_neck), 0, 15.0, 0.5, 0, 87.0, 203.0 },
    { ANIM_OFFSET(PTR_ANIM_death_neck), 1, 15.0, 0.5, 0, 87.0, 203.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 1, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 0, 15.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 1, 15.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_left_hand[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_left_hand), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_right_hand), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_left_arm[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_left_arm), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_right_arm), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_left_shoulder[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_left_shoulder), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_right_shoulder), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 0, 20.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_right_hand[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_right_hand), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_left_hand), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_right_arm[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_right_arm), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_left_arm), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_right_shoulder[] = {
    { ANIM_OFFSET(PTR_ANIM_hit_right_shoulder), 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_hit_left_shoulder), 1, -1.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 1, 20.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_chest[] = {
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 1, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 0, 15.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 1, 15.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct StruckAnim flinch_gun[] = {
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down_soft), 1, 20.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 0, 15.0, 0.5, 0, -1.0, -1.0 },
    { ANIM_OFFSET(PTR_ANIM_death_forward_face_down), 1, 15.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};


struct explosion_death_animation g_ExplosionDeathAnims[] = {
    { PTR_ANIM_death_explosion_forward, 0, 0.5, 9.0, 18.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_forward, 1, 0.5, 9.0, 18.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_face_down, 0, 0.5, 11.0, 19.0, 31.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_face_down, 1, 0.5, 11.0, 19.0, 31.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_roll, 0, 0.5, 6.0, 20.0, 27.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_roll, 1, 0.5, 6.0, 20.0, 27.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right2, 0, 0.5, 29.0, 36.0, 48.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right2, 1, 0.5, 29.0, 36.0, 48.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right2_alt, 0, 0.5, 29.0, 38.0, 49.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right2_alt, 1, 0.5, 29.0, 38.0, 49.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right3, 0, 0.5, 19.0, 30.0, 42.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right3, 1, 0.5, 19.0, 30.0, 42.0, -1.0 },
    { PTR_ANIM_death_explosion_left1, 0, 0.5, 9.0, 21.0, 29.0, 55.0 },
    { PTR_ANIM_death_explosion_left1, 1, 0.5, 9.0, 21.0, 29.0, 55.0 },
    { PTR_ANIM_death_explosion_right, 0, 0.5, 6.0, 18.0, 27.0, -1.0 },
    { PTR_ANIM_death_explosion_right, 1, 0.5, 6.0, 18.0, 27.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right1, 0, 0.5, 6.0, 19.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right1, 1, 0.5, 6.0, 19.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_back_left, 0, 0.5, 8.0, 14.0, 25.0, -1.0 },
    { PTR_ANIM_death_explosion_back_left, 1, 0.5, 8.0, 14.0, 25.0, -1.0 },
    { PTR_ANIM_death_explosion_back1, 0, 0.5, 8.0, 19.0, 25.0, -1.0 },
    { PTR_ANIM_death_explosion_back1, 1, 0.5, 8.0, 19.0, 25.0, -1.0 },
    { PTR_ANIM_death_explosion_back2, 0, 0.5, 12.0, 21.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_back2, 1, 0.5, 12.0, 21.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_left2, 0, 0.5, 22.0, 30.0, 41.0, -1.0 },
    { PTR_ANIM_death_explosion_left2, 1, 0.5, 22.0, 30.0, 41.0, -1.0 },
    {0, 0, 0.5, 0.0, 0.0, 0.0, -1.0},
};

s8 expl_forward[] = {0x0, 0x01, 0x02, 0x03, 0x4, 0x05, 0x00, 0x00};
s8 expl_f_left[] = {0x7, 0x09, 0x0B, 0x00};
s8 expl_f_right[] = {0x06, 0x08, 0x0A, 0x00};
s8 expl_left[] = {0x0C, 0x0F, 0x11, 0x00};
s8 expl_right[] = {0x0D, 0x0E, 0x10, 0x00};
s8 expl_back[] = {0x14, 0x15, 0x16, 0x17};
s8 expl_b_right[] = {0x12, 0x18, 0x00, 0x00};
s8 expl_b_left[] = {0x13, 0x19, 0x00, 0x00};

struct explosion_anim_group_info explosion_animation_table[EXPLOSION_ANIMATION_TABLE_LEN] = {
    {expl_forward, 6},
    {expl_f_left, 3},
    {expl_left, 3},
    {expl_b_right, 2},
    {expl_back, 4},
    {expl_b_left, 2},
    {expl_right, 3},
    {expl_f_right, 3}
};

struct weapon_firing_animation_table rifle_firing_animation_group1[] = {
    { {PTR_ANIM_fire_standing_fast}, 28.0, 0, 0, 0, -1.0, 23.0, 54.0, -1.0, -1.0, 18.0, 54.0, 0.87266463, -0.52359879, 1.0471976, -0.34906587, 1.6, 1.8 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group1 = { rifle_firing_animation_group1, -1 };

struct weapon_firing_animation_table rifle_firing_animation_group2[] = {
    { {PTR_ANIM_fire_standing}, 37.0, 0, 0, 0, -1.0, 30.0, 81.0, -1.0, -1.0, 25.0, 81.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 1.6, 1.75 },
    { {PTR_ANIM_fire_hip}, 27.0, 0, 0, 0, -1.0, 22.0, 61.0, -1.0, -1.0, 17.0, 61.0, 0.87266463, -0.2617994, 0.69813174, -0.69813174, 2.0, 1.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group2 = { rifle_firing_animation_group2, -1 };

struct weapon_firing_animation_table rifle_firing_animation_group5[] = {
    { {PTR_ANIM_fire_standing}, 37.0, 0, 0, 0, -1.0, 30.0, 81.0, -1.0, -1.0, 25.0, 81.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 1.6, 1.75 },
    { {PTR_ANIM_fire_hip}, 27.0, 0, 0, 0, -1.0, 22.0, 61.0, -1.0, -1.0, 17.0, 61.0, 0.87266463, -0.2617994, 0.69813174, -0.69813174, 2.0, 1.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group5 = { rifle_firing_animation_group5, -1 };

struct weapon_firing_animation_table rifle_firing_animation_group3[] = {
    { {PTR_ANIM_fire_shoulder_left}, 19.0, 0, 1.5707964, 0, -1.0, 19.0, 61.0, -1.0, -1.0, 14.0, 61.0, 0.87266463, -0.34906587, 0.43633232, -1.0471976, 2.5, 2.5 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group3 = { rifle_firing_animation_group3, -1 };

struct weapon_firing_animation_table rifle_firing_animation_group4[] = {
    { {PTR_ANIM_fire_turn_right2}, 27.0, 0, 0, 0, -1.0, 39.0, 74.0, -1.0, -1.0, 34.0, 74.0, 0.87266463, -0.69813174, 0.78539819, -0.69813174, 1.5, 1.5 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group4 = { rifle_firing_animation_group4, -1 };

struct anim_group_info *ptr_rifle_firing_animation_groups[] = {
    &ptr_rifle_firing_animation_group1,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group1
};

struct weapon_firing_animation_table pistol_firing_animation_group1[] = {
    { {PTR_ANIM_fire_standing_one_handed_weapon}, 26.0, 0, 0, 12.0, 140.0, 58.0, 92.0, 60.0, 79.0, 20.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    { {PTR_ANIM_fire_hip_one_handed_weapon_fast}, 0.0, 0, 0, 17.0, 100.0, 25.0, 87.0, 30.0, 55.0, 20.0, 93.0, 0.87266463, -0.69813174, 0.69813174, -1.0471976, 0.0, 0.0 },
    { {PTR_ANIM_fire_hip_one_handed_weapon_slow}, 0.0, 0, 0, 12.0, 64.0, 19.0, 51.0, 24.0, 46.0, 14.0, 58.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },
    { {PTR_ANIM_fire_hip_forward_one_handed_weapon}, 22.0, 0, 0, 4.0, 69.0, 22.0, 49.0, 22.0, 33.0, 8.0, 58.0, 0.87266463, -0.69813174, 0.43633232, -0.78539819, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group1 = { pistol_firing_animation_group1, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group2[] = {
    { {PTR_ANIM_fire_standing_one_handed_weapon}, 26.0, 0, 0, 12.0, 140.0, 58.0, 92.0, 60.0, 79.0, 20.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    { {PTR_ANIM_fire_hip_forward_one_handed_weapon}, 22.0, 0, 0, 4.0, 69.0, 22.0, 49.0, 22.0, 33.0, 8.0, 58.0, 0.87266463, -0.69813174, 0.43633232, -0.78539819, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group2 = { pistol_firing_animation_group2, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group3[] = {
    { {PTR_ANIM_fire_standing_one_handed_weapon}, 26.0, 0, 0, 12.0, 140.0, 58.0, 92.0, 60.0, 79.0, 20.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    { {PTR_ANIM_fire_hip_forward_one_handed_weapon}, 22.0, 0, 0, 4.0, 69.0, 22.0, 49.0, 22.0, 33.0, 8.0, 58.0, 0.87266463, -0.69813174, 0.43633232, -0.78539819, 0.0, 0.0 },
    { {PTR_ANIM_fire_standing_left_one_handed_weapon_slow}, 0.0, 0, 1.5707964, 7.0, 130.0, 45.0, 93.0, 56.0, 73.0, 26.0, 107.0, 0.87266463, -0.69813174, 0.34906587, -0.52359879, 0.0, 0.0 },
    { {PTR_ANIM_fire_standing_left_one_handed_weapon_fast}, 15.0, 0, 1.5707964, 5.0, 76.0, 20.0, 31.0, 31.0, 38.0, 15.0, 49.0, 0.87266463, -0.69813174, 0.52359879, -1.0471976, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group3 = { pistol_firing_animation_group3, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group6[] = {
    { {PTR_ANIM_fire_standing_one_handed_weapon}, 26.0, 0, 0, 12.0, 140.0, 58.0, 92.0, 60.0, 79.0, 20.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    { {PTR_ANIM_fire_hip_forward_one_handed_weapon}, 22.0, 0, 0, 4.0, 69.0, 22.0, 49.0, 22.0, 33.0, 8.0, 58.0, 0.87266463, -0.69813174, 0.43633232, -0.78539819, 0.0, 0.0 },
    { {PTR_ANIM_fire_standing_right_one_handed_weapon}, 0.0, 0, 4.712389, 7.0, 139.0, 54.0, 105.0, 61.0, 88.0, 26.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.61086529, 0.0, 0.0 },
    { {PTR_ANIM_fire_step_right_one_handed_weapon}, 19.0, 0, 4.712389, 4.0, 79.0, 21.0, 50.0, 26.0, 42.0, 10.0, 64.0, 0.87266463, -0.69813174, 0.69813174, -0.61086529, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group6 = { pistol_firing_animation_group6, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group4[] = {
    { {PTR_ANIM_fire_standing_left_one_handed_weapon_fast}, 19.0, 0, 1.5707964, 5.0, 76.0, 20.0, 31.0, 31.0, 38.0, 15.0, 49.0, 0.87266463, -0.69813174, 0.52359879, -1.0471976, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group4 = { pistol_firing_animation_group4, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group5[] = {
    { {PTR_ANIM_fire_step_right_one_handed_weapon}, 19.0, 0, 4.712389, 4.0, 79.0, 21.0, 50.0, 26.0, 42.0, 10.0, 64.0, 0.87266463, -0.69813174, 0.69813174, -0.61086529, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group5 = { pistol_firing_animation_group5, -1 };

struct anim_group_info *ptr_pistol_firing_animation_groups[] = {
    &ptr_pistol_firing_animation_group1,
    &ptr_pistol_firing_animation_group1,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group1,
    &ptr_pistol_firing_animation_group1
};

struct weapon_firing_animation_table doubles_firing_animation_group1[] = {
    { {PTR_ANIM_fire_standing_dual_wield}, 26.0, 0, 0, 7.0, 92.0, 28.0, 68.0, -1.0, -1.0, 11.0, 73.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_doubles_firing_animation_group1 = { doubles_firing_animation_group1, -1 };

struct weapon_firing_animation_table doubles_firing_animation_group2[] = {
    { {PTR_ANIM_fire_standing_dual_wield_left}, 26.0, 0, 1.5707964, 9.0, 112.0, 38.0, 87.0, -1.0, -1.0, 19.0, 98.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },
    { {PTR_ANIM_fire_standing_dual_wield_hands_crossed_left}, 25.0, 0, 1.5707964, 10.0, 112.0, 32.0, 86.0, -1.0, -1.0, 19.0, 97.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_doubles_firing_animation_group2 = { doubles_firing_animation_group2, -1 };

struct weapon_firing_animation_table doubles_firing_animation_group3[] = {
    { {PTR_ANIM_fire_standing_dual_wield_right}, 39.0, 0, 4.712389, 22.0, 127.0, 44.0, 102.0, -1.0, -1.0, 28.0, 112.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },
    { {PTR_ANIM_fire_standing_dual_wield_hands_crossed_right}, 39.0, 0, 4.712389, 23.0, 130.0, 46.0, 100.0, -1.0, -1.0, 30.0, 110.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_doubles_firing_animation_group3 = { doubles_firing_animation_group3, -1 };

struct anim_group_info *ptr_doubles_firing_animation_groups[] = {
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1
};

// Kneeling rifle fire: right leg forward, then left leg forward. firing_animation_groups in
// bondview.c uses the second entry on its own.
struct weapon_firing_animation_table crouched_rifle_firing_animation_group1[] = {
    { {PTR_ANIM_fire_kneel_right_leg}, 27.0, 0, 0, 0, -1.0, 35.0, 75.0, -1.0, -1.0, 31.0, 75.0, 0.87266463, -0.69813174, 0.90757126, -0.69813174, 1.5, 1.5 },
    { {PTR_ANIM_fire_kneel_left_leg}, 24.0, 0, 0, 0, -1.0, 46.0, 98.0, -1.0, -1.0, 41.0, 98.0, 0.87266463, -0.52359879, 1.134464, -0.69813174, 1.6, 1.6 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_rifle_firing_animation_group1 = { crouched_rifle_firing_animation_group1, -1 };

struct weapon_firing_animation_table crouched_rifle_firing_animation_group2[] = {
    { {PTR_ANIM_fire_kneel_left}, 26.0, 0, 0, 0, -1.0, 34.0, 87.0, -1.0, -1.0, 29.0, 87.0, 0.87266463, -0.52359879, 0.69813174, -0.95993108, 1.6, 2.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_rifle_firing_animation_group2 = { crouched_rifle_firing_animation_group2, -1 };

struct weapon_firing_animation_table crouched_rifle_firing_animation_group3[] = {
    { {PTR_ANIM_fire_kneel_right}, 28.0, 0, 0, 0, -1.0, 36.0, 88.0, -1.0, -1.0, 31.0, 88.0, 0.87266463, -0.69813174, 0.87266463, -0.43633232, 1.6, 1.5 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_rifle_firing_animation_group3 = { crouched_rifle_firing_animation_group3, -1 };

struct anim_group_info *ptr_crouched_rifle_firing_animation_groups[] = {
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1
};

struct weapon_firing_animation_table crouched_pistol_firing_animation_group1[] = {
    { {PTR_ANIM_fire_kneel_forward_one_handed_weapon_slow}, 25.0, 0, 0, 12.0, 132.0, 55.0, 87.0, 67.0, 87.0, 26.0, 111.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },
    { {PTR_ANIM_fire_kneel_forward_one_handed_weapon_fast}, 26.0, 0, 0, 8.0, 89.0, 31.0, 63.0, 41.0, 51.0, 21.0, 80.0, 0.87266463, -0.69813174, 0.34906587, -1.134464, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_pistol_firing_animation_group1 = { crouched_pistol_firing_animation_group1, -1 };

struct weapon_firing_animation_table crouched_pistol_firing_animation_group2[] = {
    { {PTR_ANIM_fire_kneel_left_one_handed_weapon_slow}, 47.0, 0, 1.5707964, 7.0, 128.0, 33.0, 86.0,47.0, 74.0, 23.0, 106.0, 0.87266463, -0.52359879, 0.52359879, -0.78539819, 0.0, 0.0 },
    { {PTR_ANIM_fire_kneel_left_one_handed_weapon_fast}, 18.0, 0, 1.5707964, 7.0, 78.0, 28.0, 52.0, 35.0, 45.0, 15.0, 66.0, 0.87266463, -0.087266468, 0.69813174, -0.78539819, 1.5, 1.0 },
    { {PTR_ANIM_fire_kneel_left_one_handed_weapon}, 20.0, 0, 1.5707964, 13.0, 92.0, 37.0, 67.0, 42.0, 55.0, 25.0, 84.0, 0.87266463, -0.52359879, 0.34906587, -0.69813174, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_pistol_firing_animation_group2 = { crouched_pistol_firing_animation_group2, -1 };

struct weapon_firing_animation_table crouched_pistol_firing_animation_group3[] = {
    { {PTR_ANIM_fire_kneel_right_one_handed_weapon_slow}, 28.0, 0, 4.712389, 15.0, 124.0, 38.0, 97.0, 60.0, 84.0, 20.0, 106.0, 0.87266463, -0.69813174, 0.52359879, -0.87266463, 0.0, 0.0 },
    { {PTR_ANIM_fire_kneel_right_one_handed_weapon_fast}, 23.0, 0, 4.712389, 0, 85.0, 32.0, 38.0, 38.0, 60.0, 14.0, 71.0, 0.87266463, -0.69813174, 0.61086529, -0.95993108, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_pistol_firing_animation_group3 = { crouched_pistol_firing_animation_group3, -1 };

struct anim_group_info *ptr_crouched_pistol_firing_animation_groups[] = {
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1
};

struct weapon_firing_animation_table crouched_doubles_firing_animation_group1[] = {
    { {PTR_ANIM_fire_kneel_dual_wield}, 22.0, 0, 0, 10.0, 111.0, 34.0, 87.0, -1.0, -1.0, 17.0, 104.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },
    { {PTR_ANIM_fire_kneel_dual_wield_hands_crossed}, 25.0, 0, 0, 9.0, 92.0, 33.0, 62.0, -1.0, -1.0, 18.0, 69.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_doubles_firing_animation_group1 = { crouched_doubles_firing_animation_group1, -1 };

struct weapon_firing_animation_table crouched_doubles_firing_animation_group2[] = {
    { {PTR_ANIM_fire_kneel_dual_wield_left}, 28.0, 0, 1.5707964, 15.0, 108.0, 34.0, 73.0, -1.0, -1.0, 17.0, 93.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },
    { {PTR_ANIM_fire_kneel_dual_wield_hands_crossed_left}, 19.0, 0, 1.5707964, 3.0, 95.0, 30.0, 64.0, -1.0, -1.0, 14.0, 71.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 1.5, 1.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_doubles_firing_animation_group2 = { crouched_doubles_firing_animation_group2, -1 };

struct weapon_firing_animation_table crouched_doubles_firing_animation_group3[] = {
    { {PTR_ANIM_fire_kneel_dual_wield_right}, 31.0, 0, 4.712389, 14.0, 111.0, 40.0, 83.0,-1.0, -1.0, 21.0, 94.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },
    { {PTR_ANIM_fire_kneel_dual_wield_hands_crossed_right}, 26.0, 0, 4.712389, 7.0, 89.0, 34.0, 60.0, -1.0, -1.0, 20.0, 68.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },
    {{0}, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_doubles_firing_animation_group3 = { crouched_doubles_firing_animation_group3, -1 };

struct anim_group_info* ptr_crouched_doubles_firing_animation_groups[] = {
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1
};

struct weapon_firing_animation_table g_ChrFireAnims[] = {
    // sizeof(struct weapon_firing_animation_table) = 0x48 = 72.
    // index 0
    { {PTR_ANIM_fire_roll_left}, 76.0, 0.0, 0.0, 20.0, -1.0, 98.0, 161.0, -1.0, -1.0, 93.0, 161.0, 0.87266463, -0.52359879, 0.69813174, -0.69813174, 1.7, 2.0 },

    // index 1
    { {PTR_ANIM_fire_roll_right1}, 58.0, 0.0, 0.0, 10.0, -1.0, 77.0, 104.0, -1.0, -1.0, 72.0, 104.0, 0.87266463, -0.34906587, 0.61086529, -0.69813174, 1.55, 1.5 },

    // index 2
    { {PTR_ANIM_fire_roll_left_fast}, 61.0, 0.0, 0.0, 10.0, -1.0, 83.0, 128.0, -1.0, -1.0, 78.0, 128.0, 0.87266463, -0.52359879, 0.87266463, -0.52359879, 1.2, 1.3 },

    // index 3
    { {PTR_ANIM_fire_roll_right2}, 63.0, 0.0, 0.0, 10.0, -1.0, 73.0, 114.0, -1.0, -1.0, 68.0, 114.0, 0.87266463, -0.52359879, 0.61086529, -0.61086529, 1.65, 1.5 },

    // index 4
    { {PTR_ANIM_fire_roll_left}, 76.0, 0.0, 0.0, 20.0, 76.0, 98.0, 161.0, -1.0, -1.0, 93.0, 161.0, 0.87266463, -0.52359879, 0.69813174, -0.69813174, 1.7, 2.0 },

    // index 5
    { {PTR_ANIM_fire_roll_right1}, 58.0, 0.0, 0.0, 10.0, 63.0, 77.0, 104.0, -1.0, -1.0, 72.0, 104.0, 0.87266463, -0.34906587, 0.61086529, -0.69813174, 1.55, 1.5 },

    // index 6
    { {PTR_ANIM_fire_roll_left_fast}, 61.0, 0.0, 0.0, 10.0, 56.0, 83.0, 128.0, -1.0, -1.0, 78.0, 128.0, 0.87266463, -0.52359879, 0.87266463, -0.52359879, 1.2, 1.3 },

    // index 7
    { {PTR_ANIM_fire_roll_right2}, 63.0, 0.0, 0.0, 10.0, 50.0, 73.0, 114.0, -1.0, -1.0, 68.0, 114.0, 0.87266463, -0.52359879, 0.61086529, -0.61086529, 1.65, 1.5 },

    // index 8
    { {PTR_ANIM_fire_hip_one_handed_weapon_slow}, 0.0, 0.0, 0.0, 7.0, 64.0, 19.0, 51.0, 24.0, 46.0, 14.0, 58.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },

    // index 9
    { {PTR_ANIM_fire_standing_left_one_handed_weapon_fast}, 0.0, 0.0, 1.5707964, 14.0, 76.0, 26.0, 31.0, 31.0, 38.0, 15.0, 49.0, 0.87266463, -0.69813174, 0.52359879, -1.0471976, 0.0, 0.0 },

    // index 10
    { {PTR_ANIM_fire_kneel_forward_one_handed_weapon_fast}, 26.0, 0.0, 0.0, 25.0, 89.0, 41.0, 63.0, 41.0, 51.0, 21.0, 80.0, 0.87266463, -0.69813174, 0.34906587, -1.134464, 0.0, 0.0 },

    // index 11
    { {PTR_ANIM_fire_kneel_left_one_handed_weapon_fast}, 18.0, 0.0, 1.5707964, 11.0, 78.0, 33.0, 52.0, 35.0, 45.0, 15.0, 66.0, 0.87266463, -0.087266468, 0.69813174, -0.78539819, 1.5, 1.0 },

    // index 12
    { {PTR_ANIM_fire_standing_dual_wield}, 26.0, 0.0, 0.0, 7.0, 92.0, 28.0, 68.0, -1.0, -1.0, 11.0, 73.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },

    // index 13
    { {PTR_ANIM_fire_standing_dual_wield_left}, 26.0, 0.0, 1.5707964, 9.0, 112.0, 38.0, 87.0, -1.0, -1.0, 19.0, 98.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },

    // index 14
    { {PTR_ANIM_fire_kneel_dual_wield}, 22.0, 0.0, 0.0, 10.0, 11.0, 34.0, 87.0, -1.0, -1.0, 17.0, 104.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },

    // index 15
    { {PTR_ANIM_fire_kneel_dual_wield_left}, 28.0, 0.0, 1.5707964, 15.0, 108.0, 34.0, 73.0, -1.0, -1.0, 17.0, 93.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },

    // index 16
    { {PTR_ANIM_fire_standing_dual_wield}, 26.0, 0.0, 0.0, 7.0, 92.0, 28.0, 68.0, -1.0, -1.0, 11.0, 73.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },

    // index 17
    { {PTR_ANIM_fire_standing_dual_wield_hands_crossed_left}, 25.0, 0.0, 1.5707964, 10.0, 112.0, 32.0, 86.0, -1.0, -1.0, 19.0, 97.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },

    // index 18
    { {PTR_ANIM_fire_kneel_dual_wield_hands_crossed}, 25.0, 0.0, 0.0, 9.0, 92.0, 33.0, 62.0, -1.0, -1.0, 18.0, 69.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },

    // index 19
    { {PTR_ANIM_fire_kneel_dual_wield_hands_crossed_left}, 19.0, 0.0, 1.5707964, 3.0, 95.0, 30.0, 64.0, -1.0, -1.0, 14.0, 71.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 1.5, 1.0 },

    // index 20
    {{0}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct weapon_firing_animation_table g_ChrMoveFireAnims[] = {
    // address 0x80030660. Index 0, = g_ChrMoveFireAnims + 0.
    { {PTR_ANIM_fire_walking}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 1.4, 1.3 },

    // address 0x800306A8. Index 1, = g_ChrMoveFireAnims + 72.
    { {PTR_ANIM_fire_running}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 1.1, 1.2 },

    // address 0x800306F0. Index 2, = g_ChrMoveFireAnims + 144.
    { {PTR_ANIM_aim_walking_one_handed_weapon}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030738. Index 3, = g_ChrMoveFireAnims + 216.
    { {PTR_ANIM_aim_running_one_handed_weapon}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030780. Index 4, = g_ChrMoveFireAnims + 288.
    { {PTR_ANIM_fire_walking_dual_wield}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x800307C8. Index 5, = g_ChrMoveFireAnims + 360.
    { {PTR_ANIM_fire_running_dual_wield}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030810. Index 6, = g_ChrMoveFireAnims + 432.
    { {PTR_ANIM_fire_walking_dual_wield_hands_crossed}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030858. Index 7, = g_ChrMoveFireAnims + 504.
    { {PTR_ANIM_fire_running_dual_wield_hands_crossed}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x800308A0. Index 8, = g_ChrMoveFireAnims + 576.
    { {PTR_ANIM_aim_running_left_one_handed_weapon}, 0.0, 0.0, 1.5707964, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x800308E8. Index 9, = g_ChrMoveFireAnims + 648.
    { {PTR_ANIM_aim_running_right_one_handed_weapon}, 0.0, 0.0, 4.712389, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030930. Index 10, = g_ChrMoveFireAnims + 720.
    {{0}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

s32 objectiveregisters1 = 0;

/**
 * List of characters updated every tick.
*/
ChrRecord* g_BgChrs = 0;
static ChrRecord g_BgChrTemplate = { 0 };

/**
 * Number of items in g_BgChrs.
 *
 * Adress 0x0x80030980.
*/
s32 g_NumBgChrs = 0;

/**
 * Default factor in chrGetRunAnimSpeed.
*/
f32 g_ChrSpeedWalk = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_running.
*/
f32 g_ChrSpeedRun = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_sprinting.
*/
f32 g_ChrSpeedSprint = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_walking_unarmed.
*/
f32 g_ChrSpeedWalkUnarmed = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_running_one_handed_weapon.
*/
f32 g_ChrSpeedRunOneHanded = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by first ANIM_DATA_sprinting_one_handed_weapon.
*/
f32 g_ChrSpeedSprintOneHanded = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_walking_female.
*/
f32 g_ChrSpeedWalkFemale = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by ANIM_DATA_running_female.
*/
f32 g_ChrSpeedRunFemale = 0;

/**
 * Scale factor in chrGetRunAnimSpeed, used by duplicate ANIM_DATA_sprinting_one_handed_weapon.
*/
f32 g_ChrSpeedSprintOneHanded2 = 0;


/**
 * Default firing state, left and right hand.
*/

/**
 * Default firing state when crouched, left and right hand.
*/





s32 chrsGetNumSlots(void)
{
  return g_NumChrSlots;
}


Vertex *dynAllocateVerticesDiscard(s32 count)
{
  return dynAllocate(count * sizeof(Vertex));
}


void chrSetMoving(ChrRecord *self, bool unset)
{
    if (unset)
    {
        self->hidden &= ~CHRHIDDEN_MOVING;
    }
    else
    {
        self->hidden |= CHRHIDDEN_MOVING;
    }
    return;
}


StandTile *chrMoveWithCollision(ChrRecord *guard, StandTile *stan, coord3d *src, coord3d *dst, s32 updateLastMoveOk)
{
    StandTile *ret;
    StandTile *tile;
    f32 height;
    f32 always_20;
    f32 width;
    coord3d edgeA;
    coord3d edgeB;
    coord3d delta;
    coord3d dir;
    f32 tmp;
    s32 hasprojection;
    coord3d newpos;

    ret = NULL;
    tile = stan;

    chrGetChrWidthHeight(guard->prop, &width, &height, &always_20);
    chrSetMoving(guard, 0);
    stanResetHits();

    if (stanTestLineUnobstructed(&tile, src->x, src->z, dst->x, dst->z, 0x1f, height, always_20, 0.0f, 1.0f))
    {
        if (stanTestVolume(&tile, dst->x, dst->z, width, 0x1f, height, always_20) < 0)
        {
            if (updateLastMoveOk)
            {
                guard->invalidmove = 0;
                guard->lastmoveok60 = g_GlobalTimer;
            }

            ret = tile;
            goto done;
        }
    }

    hasprojection = 0;

    if (stanGetCollisionEdge(&edgeA, &edgeB))
    {
        delta.x = dst->x - src->x;
        delta.z = dst->z - src->z;

        if ((edgeA.x != edgeB.x) || (edgeA.z != edgeB.z))
        {
            hasprojection = 1;

            dir.x = edgeB.x - edgeA.x;
            dir.z = edgeB.z - edgeA.z;

            tmp = 1.0f / sqrtf((dir.z * dir.z) + (dir.x * dir.x));

            dir.x *= tmp;
            dir.z *= tmp;

            tmp = (dir.z * delta.z) + (delta.x * dir.x);

            newpos.x = (dir.x * tmp) + src->x;
            newpos.z = (dir.z * tmp) + src->z;


            tile = stan;
        }

        if (hasprojection)
        {
            if (stanTestLineUnobstructed(&tile, src->x, src->z, newpos.x, newpos.z, 0x1f, height, always_20, 0.0f, 1.0f))
            {
                if (stanTestVolume(&tile, newpos.x, newpos.z, width, 0x1f, height, always_20) < 0)
                {
                    dst->x = newpos.x;
                    dst->z = newpos.z;
                    guard->invalidmove = 2;
                    ret = tile;
                    goto done;
                }
            }
        }

        tmp = width * width;

        dir.x = edgeA.x - dst->x;
        dir.z = edgeA.z - dst->z;

        if (((dir.z * dir.z) + (dir.x * dir.x)) <= tmp)
        {
            if ((edgeA.x == src->x) && (edgeA.z == src->z))
            {
                goto done;
            }

            dir.x = -(edgeA.z - src->z);
            dir.z = edgeA.x - src->x;

            tmp = 1.0f / sqrtf((dir.z * dir.z) + (dir.x * dir.x));

            dir.x *= tmp;
            dir.z *= tmp;

            tmp = (dir.z * delta.z) + (delta.x * dir.x);

            newpos.x = (dir.x * tmp) + src->x;
            newpos.z = (dir.z * tmp) + src->z;

            tile = stan;

            if (stanTestLineUnobstructed(&tile, src->x, src->z, newpos.x, newpos.z, 0x1f, height, always_20, 0.0f, 1.0f))
            {
                if (stanTestVolume(&tile, newpos.x, newpos.z, width, 0x1f, height, always_20) < 0)
                {
                    dst->x = newpos.x;
                    dst->z = newpos.z;
                    guard->invalidmove = 2;
                    ret = tile;
                    goto done;
                }
            }
        }
        else
        {
            dir.x = edgeB.x - dst->x;
            dir.z = edgeB.z - dst->z;

            if (((dir.z * dir.z) + (dir.x * dir.x)) <= tmp)
            {
                if ((edgeB.x == src->x) && (edgeB.z == src->z))
                {
                    goto done;
                }

                dir.x = -(edgeB.z - src->z);
                dir.z = edgeB.x - src->x;

                tmp = 1.0f / sqrtf((dir.z * dir.z) + (dir.x * dir.x));

                dir.x *= tmp;
                dir.z *= tmp;

                tmp = (dir.z * delta.z) + (delta.x * dir.x);

                newpos.x = (dir.x * tmp) + src->x;
                newpos.z = (dir.z * tmp) + src->z;

                tile = stan;

                if (stanTestLineUnobstructed(&tile, src->x, src->z, newpos.x, newpos.z, 0x1f, height, always_20, 0.0f, 1.0f))
                {
                    if (stanTestVolume(&tile, newpos.x, newpos.z, width, 0x1f, height, always_20) < 0)
                    {
                        dst->x = newpos.x;
                        dst->z = newpos.z;
                        guard->invalidmove = 2;
                        ret = tile;
                    }
                }
            }
        }
    }

done:
    chrSetMoving(guard, 1);

    if (ret == NULL)
    {
        guard->invalidmove = 1;
    }

    return ret;
}


s32 chrRootMoveCallback(Model *model, coord3d *src, coord3d *dst, f32 *ground_y)
{
    ChrRecord *chr;
    s32 moved;
    f32 ground;
    coord3d *groundpos;
    f32 tmp;
    s32 i;
    StandTile *tile;
    union ModelRwData *rwdata;

    chr = model->chr;
    moved = 0;
    ground = 0.0f;
    groundpos = src;

    if (chr->prop->stan != NULL)
    {
        if ((chr->actiontype == ACT_DIE) && (chr->act_die.timeextra > ground))
        {
#ifdef BUGFIX_R1
            tmp = ((model->playspeed * g_GlobalTimerDeltaRaw) * (chr->act_die.timeextra - chr->act_die.elapseextra)) / chr->act_die.timeextra;
#else
            tmp = ((model->playspeed * g_GlobalTimerDelta) * (chr->act_die.timeextra - chr->act_die.elapseextra)) / chr->act_die.timeextra;
#endif

            dst->x += chr->act_die.extraspeed.x * tmp;
            dst->y += chr->act_die.extraspeed.y * tmp;
            dst->z += chr->act_die.extraspeed.z * tmp;

#ifdef BUGFIX_R1
            chr->act_die.elapseextra += g_GlobalTimerDeltaRaw * model->playspeed;
#else
            chr->act_die.elapseextra += g_GlobalTimerDelta * model->playspeed;
#endif

            if (chr->act_die.timeextra < chr->act_die.elapseextra)
            {
                chr->act_die.timeextra = 0.0f;
            }
        }

        dst->x += chr->fallspeed.x * g_GlobalTimerDelta;
        dst->z += chr->fallspeed.z * g_GlobalTimerDelta;

        tile = chrMoveWithCollision(chr, chr->prop->stan, src, dst, 1);

        if (tile != NULL)
        {
            chr->prop->stan = tile;
            groundpos = dst;
            moved = 1;
        }

        if (!(chr->chrflags & CHRFLAG_LOCK_Y_POS))
        {
            ground = stanGetFloorY(chr->prop->stan, groundpos->x, groundpos->z);
            chr->ground = ground;

            if (chr->chrflags & CHRFLAG_INIT)
            {
                rwdata = modelGetNodeRwData(model, model->obj->RootNode);

                chr->chrflags &= ~1;
                chr->manground = chr->ground;
                chr->sumground = chr->ground / GROUND_SMOOTH_FACTOR;

                rwdata->Header.framePos.y = rwdata->Header.nextPos.y;
            }
            else
            {
                if ((chr->fallspeed.y != 0.0f) || (chr->manground > chr->ground))
                {
                    physFall(&chr->manground, &chr->fallspeed.y, g_GlobalTimerDelta);

                    if (chr->manground <= chr->ground)
                    {
                        chr->manground = chr->ground;
                        chr->sumground = chr->ground / GROUND_SMOOTH_FACTOR;
                        chr->fallspeed.y = 0.0f;
                    }
                }

                if (chr->manground <= chr->ground)
                {
                    i = 0;

                    if (g_ClockTimer > 0)
                    {
                        tmp = FALLSPEED_DECAY;

                        do
                        {
                            chr->sumground = (chr->sumground * tmp) + chr->ground;
                            chr->fallspeed.x *= tmp;
                            chr->fallspeed.z *= tmp;
                            i++;
                        }
                        while (i < g_ClockTimer);
                    }

                    tmp = 0.1f;
                    chr->manground = chr->sumground * GROUND_SMOOTH_FACTOR;

                    if (chr->fallspeed.x < tmp)
                    {
                        if (-0.1f < chr->fallspeed.x)
                        {
                            if (chr->fallspeed.z < tmp)
                            {
                                if (-0.1f < chr->fallspeed.z)
                                {
                                    chr->fallspeed.z = 0.0f;
                                    chr->fallspeed.x = 0.0f;
                                }
                            }
                        }
                    }
                }
            }

            dst->y += chr->manground - ground;
        }
        else
        {
            ground = chr->ground;
        }
    }

    *ground_y = ground;

    if (!moved)
    {
        dst->x = src->x;
        dst->z = src->z;
    }

    return 1;
}

#undef GROUND_SMOOTH_FACTOR
#undef FALLSPEED_DECAY


s32 chrGetNumFree(void)
{
    s32 count = 0;
    s32 i;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model == 0)
        {
            count++;
        }
    }

    return count;
}


void chrSetMaxDamage(ChrRecord *chr, f32 maxdamage)
{
    chr->maxdamage = (file007Health() * maxdamage);
}


void chrAddHealth(ChrRecord *chr, f32 health)
{
    chr->damage -= (health * file007Health());
}


PropRecord *chrInit(PropRecord *prop, Model *model, coord3d *pos, f32 angle, StandTile *tile, AIRecord *ailist)
{
    ChrRecord *chr;
    s32 i;

    chr = NULL;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model == NULL)
        {
            chr = &g_ChrSlots[i];
            break;
        }
    }

    // All chr slots in use: the original wrote through a NULL chr here.
    if (chr == NULL)
    {
        propFree(prop);
        return NULL;
    }

    prop->type = PROP_TYPE_CHR;
    prop->chr = chr;
    prop->pos.f[0] = pos->f[0];
    prop->pos.f[1] = pos->f[1];
    prop->pos.f[2] = pos->f[2];
    prop->stan = tile;

    modelSetRootMoveCallback(model, (s32) chrRootMoveCallback);

    model->chr = chr;

    modelSetRootPosition(model, pos);
    modelSetRootRotY(model, angle);

    #if defined(VERSION_EU)
    modelSetAnimPlaySpeed(model, g_ChrAnimRate * 1.2f, 0.0f);
    #else
    modelSetAnimPlaySpeed(model, g_ChrAnimRate, 0.0f);
    #endif

    chr->chrnum = (s16) g_NextChrNum;
    g_NextChrNum += 1;
    chr->headnum = 0;
    chr->bodynum = 0;
    chr->prop = prop;
    chr->model = model;
    chr->jointList = NULL;
    chr->numarghs = 0;
    chr->lastwalk60 = 0;
    chr->invalidmove = 0;
    chr->lastmoveok60 = g_GlobalTimer;
    chr->lastseetarget60 = 0;
    chr->lastknowntargetpos.f[0] = 0.0f;
    chr->lastknowntargetpos.f[1] = 0.0f;
    chr->lastknowntargetpos.f[2] = 0.0f;
    chr->targetTile = NULL;
    chr->seen_bond_time = 0;
    chr->lastheartarget60 = 0;
    chr->numclosearghs = 0;
    chr->shotbondsum = 0.0f;
    chr->damage = 0.0f;
    chr->visionrange = 250.0f;
    chr->hearingscale = 1.0f;

    chr->maxdamage = file007Health() * 4.0f;
    objGetTileShading(prop, &chr->nextcol);

    chr->shadecol.rgba[0] = chr->nextcol.rgba[0];
    chr->shadecol.rgba[1] = chr->nextcol.rgba[1];
    chr->shadecol.rgba[2] = chr->nextcol.rgba[2];
    chr->shadecol.rgba[3] = chr->nextcol.rgba[3];
    chr->fadealpha = 0xFF;
    chr->gunSounds[0].ptr_SEbuffer1 = NULL;
    chr->gunSounds[0].ptr_SEbuffer2 = NULL;
    chr->gunSounds[1].ptr_SEbuffer1 = NULL;
    chr->gunSounds[1].ptr_SEbuffer2 = NULL;
    chr->nextGunSoundTime[0] = 0;
    chr->nextGunSoundTime[1] = 0;
    chr->chrflags = CHRFLAG_INIT;
    chr->hidden = CHRHIDDEN_NONE;
    chr->sumground = 0.0f;
    chr->manground = 0.0f;
    chr->ground = 0.0f;
    chr->fallspeed.f[0] = 0.0f;
    chr->fallspeed.f[1] = 0.0f;
    chr->fallspeed.f[2] = 0.0f;
    chr->prevpos.f[0] = pos->f[0];
    chr->prevpos.f[1] = pos->f[1];
    chr->prevpos.f[2] = pos->f[2];
    chr->actiontype = 0;
    chr->sleep = 0;
    chr->ailist = ailist;
    chr->aioffset = 0;
    chr->aireturnlist = -1;
    chr->morale = 0;
    chr->alertness = 0;
    chr->flags2 = 0;
    chr->random = 0;
    chr->timer60 = 0;
    chr->padpreset1 = -1;
    chr->chrseeshot = -1;
    chr->chrseedie = -1;
    chr->chrpreset1 = -1;
    chr->beams[0].age = -1;
    chr->beams[1].age = -1;
    chr->firecount[0] = 0;
    chr->firecount[1] = 0;
    chr->grenadeprob = 0;
    chr->accuracyrating = 0;
    chr->speedrating = 0;
    chr->arghrating = 0;
    chr->flinchcnt = -1;
    chr->aimuplshoulder = 0.0f;
    chr->aimuprshoulder = 0.0f;
    chr->aimupback = 0.0f;
    chr->aimsideback = 0.0f;
    chr->aimendlshoulder = 0.0f;
    chr->aimendrshoulder = 0.0f;
    chr->aimendback = 0.0f;
    chr->aimendsideback = 0.0f;
    chr->aimendcount = 0;
    chr->weapons_held[0] = NULL;
    chr->weapons_held[1] = NULL;
    chr->handle_positiondata_hat = NULL;
    chr->targetplayer = 0;
    chr->targetlock60 = 0;
    chr->chrwidth = 20.0f;
    chr->chrheight = 185.0f;

    chrRootMoveCallback(model, &prop->pos, &prop->pos, &chr->ground);
    chrDetectRooms(chr);

    return prop;
}


PropRecord *chrAllocate(Model *model, coord3d *pos, f32 angle, StandTile *tile, AIRecord *ailist)
{
    PropRecord * ret;

    ret = propAllocate();

    if (ret != 0)
    {
        ret = chrInit(ret, model, pos, angle, tile, ailist);
    }

    return ret;
}


void chrFreeResources(PropRecord *prop)
{
    ChrRecord *chr;
    Model *model;
    PropRecord *child;
    PropRecord *prev;
    ObjectRecord *obj;

    chr = prop->chr;
    model = chr->model;

    if (chr->gunSounds[0].ptr_SEbuffer1 != NULL && sndGetPlayingState(chr->gunSounds[0].ptr_SEbuffer1) != 0) {
        sndDeactivate(chr->gunSounds[0].ptr_SEbuffer1);
    }
    if (chr->gunSounds[0].ptr_SEbuffer2 != NULL && sndGetPlayingState(chr->gunSounds[0].ptr_SEbuffer2) != 0) {
        sndDeactivate(chr->gunSounds[0].ptr_SEbuffer2);
    }
    if (chr->gunSounds[1].ptr_SEbuffer1 != NULL && sndGetPlayingState(chr->gunSounds[1].ptr_SEbuffer1) != 0) {
        sndDeactivate(chr->gunSounds[1].ptr_SEbuffer1);
    }
    if (chr->gunSounds[1].ptr_SEbuffer2 != NULL && sndGetPlayingState(chr->gunSounds[1].ptr_SEbuffer2) != 0) {
        sndDeactivate(chr->gunSounds[1].ptr_SEbuffer2);
    }

    modelResetDeformedVertices(model);
    propRemoveFromRooms(prop);

    child = prop->child;

    if (child != NULL) {
        do {
            obj = (ObjectRecord *)child->obj;
            prev = child->prev;
            objDetach(child);
            objFreePermanently(obj, TRUE);
            child = prev;
        } while (child != NULL);
    }

    modelFreeAnimSlot(model);

    chr->model = NULL;
    chr->chrnum = -1;

    if (chr->jointList != NULL) {
        jointListFree(chr->jointList);
    }
}


/**
 * Address 0x7F020540 (VERSION_US, VERSION_JP).
 * Address 0x7F0203B4 (VERSION_EU).
 */
void chrsSetAnimRate(f32 rate)
{
    s32 i;

    g_ChrAnimRate = rate;

    for (i=0; i<g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model != NULL)
        {
#if defined(REFRESH_PAL)
/* should reference D_80047E4C (1.2f) */
            modelSetAnimPlaySpeed(g_ChrSlots[i].model, g_ChrAnimRate * 1.2f, 600.0f);
#else
            modelSetAnimPlaySpeed(g_ChrSlots[i].model, g_ChrAnimRate, 600.0f);
#endif
        }
    }
}


f32 chrsGetAnimRate(void)
{
  return g_ChrAnimRate;
}


/**
 * Address 0x7F0205F0 (all versions).
 */
void chrUpdateAimProperties( ChrRecord *self)
{
    f32 mult;

    if (self->aimendcount >= 2)
    {
#if defined(BUGFIX_R1)
        mult = g_GlobalTimerDeltaRaw / (f32) self->aimendcount;
#else
        mult = g_GlobalTimerDelta / (f32) self->aimendcount;
#endif

        if (mult > 1.0f)
        {
            mult = 1.0f;
        }

        self->aimuplshoulder += ((self->aimendlshoulder - self->aimuplshoulder) * mult);
        self->aimuprshoulder += ((self->aimendrshoulder - self->aimuprshoulder) * mult);
        self->aimupback += ((self->aimendback - self->aimupback) * mult);
        self->aimsideback += ((self->aimendsideback - self->aimsideback) * mult);
        self->aimendcount -= g_ClockTimer;

        return;
    }

    self->aimuplshoulder = self->aimendlshoulder;
    self->aimuprshoulder = self->aimendrshoulder;
    self->aimupback = self->aimendback;
    self->aimsideback = self->aimendsideback;
}


void chrSetHiddenToRandom(ChrRecord *self)
{
    u32 rand;

    if ((s32) self->flinchcnt < 0)
    {
        self->flinchcnt = 1;
        self->hidden &= ~CHRHIDDEN_RAND_FLINCH_MASK;

        // roll for bits 12,13.
        // rand -> value
        // 2 => nothing
        // 0 => set bit 12
        // 1 => set bit 13
        rand = randomGetNext() % 3U;

        if (rand == 0)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_1;
        }
        else if (rand == 1)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_2;
        }

        // roll for bits 14,15.
        // rand -> value
        // 2 => nothing
        // 0 => set bit 14
        // 1 => set bit 15
        rand = randomGetNext() % 3U;

        if (rand == 0)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_4;
        }
        else if (rand == 1)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_8;
        }
    }
}


/**
 * Flinch animation envelope. Maps a character's flinchcnt timer to a normalized
 * intensity that goes 0->1 then falls 1->0, used to drive the flinch animation.
 * 
 * Flinches happen when a character has body armor or has the CHRFLAG_INVINCIBLE flag and is shot in the arm or torso.
 * 
 * Rise and fall are measured in VI retraces: 1/60 s on NTSC and 1/50 s on PAL.
 */
f32 chrGetFlinchAmount(ChrRecord *chr)
{
    f32 count;
    f32 amount;
#if defined(VERSION_US)
    f32 rise = 10.0f;
    f32 fall = 20.0f;
#else
    f32 rise = 8.0f;
    f32 fall = 16.0f;
#endif

    count = chr->flinchcnt;

    if (count < rise)
    {
        amount = sinf(((count * M_TAU_F) * 0.25f) / rise);
    }
    else
    {
        amount = 1.0f - sinf((((count - rise) * M_TAU_F) * 0.25f) / fall);
    }

    return amount;
}


#define PI_OVER_3 1.0471976f
#define FIVEPI_OVER_18 0.87266463f


void chrHandleJointPositioned(enum CHR_RENDER_PART bodypart, Mtxf *matrix)
{
    f32 scale;
    f32 xrot;
    f32 yrot;
    f32 zrot;
    f32 tmp;
    f32 amount;
    f32 savedposz;
    f32 savedposy;
    f32 savedposx;
    f32 sideback;
    Mtxf rotmtx;
    u16 hidden;
    ChrRecord *chr;

    scale = 1.0f;

#ifdef BUGFIX_R1
    if (cheatIsActive(CHEAT_DK_MODE))
    {
        if (bodypart == CHR_RENDERPART_HEAD)
        {
            scale = 4.0f;
        }
        else if ((bodypart == CHR_RENDERPART_LEFT_ARM) || (bodypart == CHR_RENDERPART_RIGHT_ARM))
        {
            if (!(g_CurModelChr->chrflags & CHRFLAG_08000000))
            {
                scale = 2.5f;
            }
        }
    }
#else
    if (cheatIsActive(CHEAT_DK_MODE))
    {
        if (bodypart == CHR_RENDERPART_HEAD)
        {
            scale = 4.0f;
        }
        else if ((bodypart == CHR_RENDERPART_LEFT_ARM) || (bodypart == CHR_RENDERPART_RIGHT_ARM))
        {
            scale = 2.5f;
        }
    }
#endif
    if ((((bodypart != CHR_RENDERPART_LEFT_ARM) && (bodypart != CHR_RENDERPART_RIGHT_ARM)) && (bodypart != CHR_RENDERPART_TORSO)) && (bodypart != CHR_RENDERPART_HEAD))
    {
        return;
    }

    zrot = (yrot = (xrot = 0.0f));

#ifdef BUGFIX_R1
    chr = g_CurModelChr;
#endif
    if (bodypart == CHR_RENDERPART_RIGHT_ARM)
    {
#ifdef BUGFIX_R1
        xrot = chr->aimuprshoulder;
#else
        xrot = g_CurModelChr->aimuprshoulder;
#endif
    }
    else if (bodypart == CHR_RENDERPART_LEFT_ARM)
    {
#ifdef BUGFIX_R1
        xrot = chr->aimuplshoulder;
#else
        xrot = g_CurModelChr->aimuplshoulder;
#endif
    }
    else if (bodypart == CHR_RENDERPART_TORSO)
    {
#ifndef BUGFIX_R1
        chr = g_CurModelChr;
#endif
        xrot = chr->aimupback;
        if (chr->hidden & CHRHIDDEN_0400)
        {
            if (PI_OVER_3 < xrot)
            {
                xrot -= PI_OVER_3;
            }
            else if (-FIVEPI_OVER_18 > xrot)
            {
                xrot += FIVEPI_OVER_18;
            }
            else
            {
                xrot = 0.0f;
            }
        }

        yrot = chr->aimsideback;

    }
    else if (bodypart == CHR_RENDERPART_HEAD)
    {
#ifndef BUGFIX_R1
        chr = g_CurModelChr;
#endif
        if (chr->hidden & CHRHIDDEN_0400)
        {
            xrot = chr->aimupback;

            if (chr->hidden & CHRHIDDEN_0400)
            {
                if (PI_OVER_3 < xrot)
                {
                    xrot = PI_OVER_3;
                }
                else if (xrot < -FIVEPI_OVER_18)
                {
                    xrot = -FIVEPI_OVER_18;
                }
            }
        }
        else if (chr->model->flip != GUNRIGHT)
        {
            xrot = chr->aimuplshoulder;
        }
        else
        {
            xrot = chr->aimuprshoulder;
        }
    }

    chr = g_CurModelChr;

    if (chr->flinchcnt >= 0)
    {

        if ((bodypart == CHR_RENDERPART_RIGHT_ARM) || (bodypart == CHR_RENDERPART_LEFT_ARM))
        {
            amount = chrGetFlinchAmount(chr) * M_TAU_F * 15.0f / 360.0f;
            chr = g_CurModelChr;
            hidden = chr->hidden;
            xrot -= amount;

            if (hidden & CHRHIDDEN_RAND_FLINCH_1)
            {
                yrot -= amount;
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_2)
            {
                yrot += amount;
            }
        }
        else if (bodypart == CHR_RENDERPART_TORSO)
        {
            tmp = chrGetFlinchAmount(chr);
            tmp *= M_TAU_F;
            amount = tmp * 15.0f;
            chr = g_CurModelChr;
            hidden = chr->hidden;
            amount /= 360.0f;
            xrot += amount;

            if (hidden & CHRHIDDEN_RAND_FLINCH_1)
            {
                yrot += amount;
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_2)
            {
                yrot -= amount;
            }

            if (hidden & CHRHIDDEN_RAND_FLINCH_4)
            {
                 zrot += (tmp * 10.0f) / 360.0f;
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_8)
            {
                zrot -= (tmp * 10.0f) / 360.0f;
            }
        }
    }

    if ((((xrot == 0.0f) && (yrot == 0.0f)) && (zrot == 0.0f)) && (scale == 1.0f))
    {
        return;
    }

    sideback = chrGetSubrotySideback(chr);

    if (xrot < 0.0f)
    {
        xrot = -xrot;
    }
    else
    {
        xrot = M_TAU_F - xrot;
    }

    if (yrot < 0.0f)
    {
        yrot += M_TAU_F;
    }

    mtx4MultAffineInPlace(camGetViewToWorldMtxf(), matrix);

    savedposx = matrix->m[3][0];
    savedposy = matrix->m[3][1];
    savedposz = matrix->m[3][2];

    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;

    if ((xrot != 0.0f) || (zrot != 0.0f))
    {
        yrot -= sideback;

        if (yrot < 0.0f)
        {
            yrot += M_TAU_F;
        }

        mtx4LoadYRotation(yrot, &rotmtx);
        mtx4MultAffineInPlace(&rotmtx, matrix);

        if (xrot != 0.0f)
        {
            mtx4LoadXRotation(xrot, &rotmtx);
            mtx4MultAffineInPlace(&rotmtx, matrix);
        }

        if (zrot != 0.0f)
        {
            mtx4LoadZRotation(zrot, &rotmtx);
            mtx4MultAffineInPlace(&rotmtx, matrix);
        }

        mtx4LoadYRotation(sideback, &rotmtx);
        mtx4MultAffineInPlace(&rotmtx, matrix);
    }
    else
    {
        mtx4LoadYRotation(yrot, &rotmtx);
        mtx4MultAffineInPlace(&rotmtx, matrix);
    }

    if (scale != 1.0f)
    {
        mtx4ScaleRows012(scale, matrix);
    }

    matrix->m[3][0] = savedposx;
    matrix->m[3][1] = savedposy;
    matrix->m[3][2] = savedposz;

    mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), matrix);
}


#undef PI_OVER_3
#undef FIVEPI_OVER_18


/**
 * For visibility, player collision, tank collision, bullet collision, and explosion damage
 * tests the game needs to know which room(s) a character is in. This allows the game to perform
 * tests on only characters in loaded rooms, not the entire stage.
 */
void chrDetectRooms(ChrRecord *self)
{
    PropRecord *myprop;
    coord3d     lowerbounds;
    coord3d     upperbounds;

    // Create a roughly character sized bounding box.
    myprop        = self->prop;
    lowerbounds.x = myprop->pos.x - 50.0f;
    lowerbounds.y = self->ground - 1.0f;
    lowerbounds.z = myprop->pos.z - 50.0f;
    upperbounds.x = myprop->pos.x + 50.0f;
    upperbounds.y = myprop->pos.y + 100.0f;
    upperbounds.z = myprop->pos.z + 50.0f;

    // Delist the character prop from its previous room(s)
    propRemoveFromRooms(myprop);

    // Detect rooms overlapped by the bounding box
    propCalcRooms(myprop, &lowerbounds, &upperbounds, 50.0f);

    // Re-register the character prop in those rooms
    propAddToRooms(myprop);
}


void chrUpdateAnim(ChrRecord *chr, s32 tickamount)
{
    Model *model;
    PropRecord* prop;

    model = chr->model;
    prop = chr->prop;

    if (!(chr->hidden & CHRHIDDEN_FREEZE))
    {
        modelGetRootPosition(model, &chr->prevpos);
        modelTickAnim(model, tickamount, 1);
        modelCalcPosition(model);
        objGetTileShading(prop, &chr->nextcol);
        modelGetRootPosition(model, &prop->pos);
        chrDetectRooms(chr);

        return;
    }

    modelCalcPosition(model);
    modelGetRootPosition(model, &prop->pos);
}


/**
 * Address:
 * US: 0x7F020EF0
 * JP: 0x7F021188
 * EU: 0x7F020E68
 *
 *   This function does the following:
 * - Drive character animations
 * - Remove characters if needed
 * - Held weapon garbage collection
 * - Character visibility tests
 * - Render characters/hats/held weapons
 * - Drop held items
 * - Fire held weapons
 */
s32 chrTick(PropRecord *prop)
{
    ModelRenderData renderdata;
    ChrRecord *chr;
    Model *model;
    s32 headSwitchVisible;
    s32 headVisible;
    s32 tickamount;
    s32 coopRepeat;

    renderdata = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};
    chr = prop->chr;
    model = chr->model;
    headVisible = 1;
    tickamount = g_ClockTimer;

    // Co-op: props are ticked once for each player's view. A chr's AI,
    // actions and animation must only advance on the first pass, or guards
    // run, aim and shoot twice as fast. The later pass doesn't update the
    // animation at all: with no time passing it would only redo the same
    // position, room and shading work.
    coopRepeat = g_CoopMode && (prop->type == PROP_TYPE_CHR)
        && (playerGetShuffledIndex(playerGetCurrentNum()) != 0);

    if (coopRepeat)
    {
        tickamount = 0;
    }

    if ((!(chr->chrflags & CHRFLAG_HIDDEN)) || (chr->chrflags & CHRFLAG_00040000))
    {
        if (g_ChrDebugAnimOverride)
        {
            if (animation_table_ptrs1[g_ChrDebugAnimIndex] != ANIM_UNUSED)
            {
                if (modelGetAnim(model) != animation_table_ptrs1[g_ChrDebugAnimIndex])
                {
                    modelSetAnimation(model, animation_table_ptrs1[g_ChrDebugAnimIndex], 0, 0.0f, 0.5f, 0.0f);
                }
            }
        }
        else if (!coopRepeat)
        {
            chrActionTick(chr);

            if (chr->model == NULL)
            {
                return TICKOP_FREE;
            }
        }

        if (g_ChrDebugPaused)
        {
            tickamount = 0;

            if (g_ChrDebugStep)
            {
                tickamount = 1;
            }
        }
    }

    if (chr->hidden & CHRHIDDEN_REMOVE)
    {
        chrFreeResources(prop);
        return TICKOP_FREE;
    }

    if (chr->weapons_held[GUNRIGHT] != NULL)
    {
        if (chr->weapons_held[GUNRIGHT]->obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
        {
            objFreePermanently(chr->weapons_held[GUNRIGHT]->obj, 1);
        }
    }

    if (chr->weapons_held[GUNLEFT] != NULL)
    {
        if (chr->weapons_held[GUNLEFT]->obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
        {
            objFreePermanently(chr->weapons_held[GUNLEFT]->obj, 1);
        }
    }

    if (chr->chrflags & CHRFLAG_HIDDEN)
    {
        headSwitchVisible = 0;
    }
    else
    {
        if (((prop->type == PROP_TYPE_VIEWER) && (g_playerPointers[playerGetIndexForProp(prop)]->cameramode == 1)) || (chr->chrflags & CHRFLAG_CULL_USING_HITBOX))
        {
            headSwitchVisible = 1;

            if (((chr->actiontype == ACT_ANIM) && (chr->act_anim.alwaysAnimate == 0)) && (chr->act_anim.noTranslate != 0))
            {
                if (!coopRepeat)
                {
                    modelTickAnim(model, tickamount, 0);
                }
            }
            else
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
            }

            goto after_position_update;
        }

        if ((chr->actiontype == ACT_PATROL) || (chr->actiontype == ACT_GOPOS))
        {
            if (((chr->actiontype == ACT_PATROL) && (chr->act_patrol.waydata.mode == WAYMODE_MAGIC)) || ((chr->actiontype == ACT_GOPOS) && (chr->act_gopos.waydata.mode == WAYMODE_MAGIC)))
            {
                headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);

                if (headSwitchVisible)
                {

                    modelGetRootPosition(model, &chr->prevpos);
                    modelCalcPosition(model);
                    objGetTileShading(prop, &chr->nextcol);
                    modelGetRootPosition(model, &prop->pos);
                    chrDetectRooms(chr);
                }
            }
            else
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
                headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);

                if (headSwitchVisible)
                {
                    if (chr->actiontype == ACT_PATROL)
                    {
                        chr->act_patrol.lastvisible60 = g_GlobalTimer;
                    }
                    else if (chr->actiontype == ACT_GOPOS)
                    {
                        chr->act_gopos.magicCheck60 = g_GlobalTimer;
                    }
                }
            }
        }
        else if ((chr->actiontype == ACT_ANIM) && (chr->act_anim.alwaysAnimate == 0))
        {
            headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);

            if (headSwitchVisible && (chr->act_anim.noTranslate == 0))
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
            }
            else
            {
                if (!coopRepeat)
                {
                    modelTickAnim(model, tickamount, 0);
                }
            }
        }
        else if (chr->actiontype == ACT_STAND)
        {
            headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);

            if (headSwitchVisible || (chr->chrflags & CHRFLAG_INIT))
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
            }
            else if (model->anim2 != NULL)
            {
                if (!coopRepeat)
                {
                    modelTickAnim(model, tickamount, 0);
                }
            }
        }
        else
        {
            if (chr->chrflags & CHRFLAG_IGNORE_ANIM_TRANSLATION)
            {
                if (!coopRepeat)
                {
                    modelTickAnim(model, tickamount, 0);
                }
            }
            else
            {
                if (!coopRepeat)
                {
                    chrUpdateAnim(chr, tickamount);
                }
            }

            headSwitchVisible = posIsOnScreen(prop, &prop->pos, modelGetRadius(model), 1);
        }
    }

after_position_update:
    if (((chr->actiontype != ACT_STAND) || (model->anim2 != NULL)) || (prop->type == PROP_TYPE_VIEWER))
    {
        chr->hidden |= CHRHIDDEN_BACKGROUND_AI;
    }

    if (!coopRepeat)
    {
        chrUpdateAimProperties(chr);
    }

    if (chr->jointList != NULL)
    {
        jointListFree(chr->jointList);
        chr->jointList = NULL;
    }

    if (headSwitchVisible)
    {

        prop->flags |= PROPFLAG_ONSCREEN;
        chr->chrflags |= CHRFLAG_HAS_BEEN_ON_SCREEN;

#ifdef BUGFIX_R1
    if (cheatIsActive(CHEAT_DK_MODE))
    {
        modelSetLodScale(0.3125f);

        if (chr->chrflags & CHRFLAG_10000000)
        {
            chr->chrflags &= ~CHRFLAG_10000000;
            modelSetScale(chr->model, chr->model->scale / 0.8f);
        }
    }
#else
        if (cheatIsActive(CHEAT_DK_MODE))
        {
            modelSetLodScale(0.3125f);
        }
#endif

        g_ModelJointPositionedFunc = chrHandleJointPositioned;
        g_CurModelChr = chr;

        renderdata.basemtx = camGetWorldToScreenMtxf();
        renderdata.mtxlist = dynAllocate(model->obj->numMatrices * (sizeof(Mtxf)));

        if (g_CurModelChr->flinchcnt >= 0)
        {
            g_CurModelChr->flinchcnt += g_ClockTimer;

#ifdef VERSION_EU
               if (g_CurModelChr->flinchcnt >= 24)
#else
               if (g_CurModelChr->flinchcnt >= 30)
#endif
            {
                g_CurModelChr->flinchcnt = -1;
            }
        }

        modelCalcMatrices(&renderdata, model);

        g_ModelJointPositionedFunc = NULL;
        modelSetLodScale(1.0f);

        objUpdateShading(&chr->shadecol, &chr->nextcol);

        prop->zDepth = modelGetRootDepth(model);

        chr->jointList = jointListAddModel(NULL, model);

        chrRenderHeldWeapon(prop, GUNRIGHT, &chr->jointList);
        chrRenderHeldWeapon(prop, GUNLEFT, &chr->jointList);

        if (chr->handle_positiondata_hat != NULL)
        {
            ObjectRecord *hatobj;
            Model *hatmodel;

            hatobj = chr->handle_positiondata_hat->obj;
            hatmodel = hatobj->model;

            chr->handle_positiondata_hat->flags |= PROPFLAG_ONSCREEN;

            renderdata.basemtx = modelFindNodeMtx(model, hatmodel->attachedto_objinst, 0);
            renderdata.mtxlist = dynAllocate(hatmodel->obj->numMatrices * (sizeof(Mtxf)));

            modelCalcMatricesNoAnim(&renderdata, hatmodel);

            if ((chr->headnum >= HEAD_START) && (chr->headnum < BODY_Female_Sally))
            {
                coord3d pos;
                f32 xscale;
                f32 yscale;
                f32 zscale;
                Mtxf mtx;
                Mtxf tmp;
                HATTYPE hat;
                struct headHat *entry;
                s32 headindex;

                pos = (coord3d){0};

                hat = hatGetModel(chr->handle_positiondata_hat);

                headindex = chr->headnum - HEAD_START;
                entry = &((struct headHat (*)[6]) g_HeadHats)[headindex][hat];


                pos.x = entry->xoffset * 21.3f;
                pos.y = entry->yoffset * 21.3f;
                pos.z = entry->zoffset * 21.3f;

                xscale = entry->xsize;
                yscale = entry->ysize;
                zscale = entry->zsize;

                mtx4LoadTranslation(&pos, &mtx);

                mtx4ScaleRow0(xscale, &mtx);
                mtx4ScaleRow1(yscale, &mtx);
                mtx4ScaleRow2(zscale, &mtx);

                mtx4MultAffine((Mtxf *)hatmodel->render_pos, &mtx, &tmp);
                mtx4Copy(&tmp, (Mtxf *)hatmodel->render_pos);

                if (hat == HATTYPE_PEAKED)
                {
                    headVisible = 0;
                }
            }

            if ((!(chr->hidden & CHRHIDDEN_DROP_HELD_ITEMS)) || (!(hatobj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)))
            {
                chr->jointList = jointListAddModel(chr->jointList, hatmodel);
            }
        }

        if (model->obj->Switches[4] != NULL)
        {
            union ModelRwData *rwdata;
            ModelFileHeader *headfile;

            rwdata = modelGetNodeRwData(model, model->obj->Switches[4]);
            headfile = rwdata->HeadPlaceholder.ModelFileHeader;

            if ((headfile != NULL) && (headfile->Switches[1] != NULL))
            {
                modelGetNodeRwData(model, headfile->Switches[1])->Switch.visible = headVisible;
            }
        }

        jointListCalcDepths(chr->jointList);
        chr->jointList = jointListSortByDepth(chr->jointList);
    }
    else
    {
        if (chr->weapons_held[GUNRIGHT] != NULL)
        {
            chr->weapons_held[GUNRIGHT]->flags &= ~PROPFLAG_ONSCREEN;
        }

        if (chr->weapons_held[GUNLEFT] != NULL)
        {
            chr->weapons_held[GUNLEFT]->flags &= ~PROPFLAG_ONSCREEN;
        }

        if (chr->handle_positiondata_hat != NULL)
        {
            chr->handle_positiondata_hat->flags &= ~PROPFLAG_ONSCREEN;
        }

        prop->flags &= ~PROPFLAG_ONSCREEN;

        chr->shadecol.r = chr->nextcol.r;
        chr->shadecol.g = chr->nextcol.g;
        chr->shadecol.b = chr->nextcol.b;
        chr->shadecol.a = chr->nextcol.a;
    }

    if (!(chr->chrflags & CHRFLAG_HIDDEN))
    {
        if (chr->hidden & CHRHIDDEN_DROP_HELD_ITEMS)
        {
            PropRecord *dropprop = prop->child;

            while (dropprop != NULL)
            {
                PropRecord *nextprop = dropprop->prev;

                objDrop(dropprop);

                dropprop = nextprop;
            }

            chr->hidden &= ~CHRHIDDEN_DROP_HELD_ITEMS;
        }

        chrTriggerFireWeapon(chr);
    }

    return TICKOP_NONE;
}


void chrDropItems(ChrRecord *self)
{
    PropRecord *childprop = self->prop->child;
    while (childprop)
    {
        if ((childprop != self->handle_positiondata_hat) &&
            (childprop != self->weapons_held[GUNLEFT]) &&
            (childprop != self->weapons_held[GUNRIGHT]))
        {
            WeaponObjRecord *wep = childprop->weapon;
            if (!(wep->flags & 0x2000))
            {
                objSetDropType(childprop, 1);
            }
        }
        childprop = childprop->prev;
    };

    self->hidden |= 1;
}


Gfx *chrRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    ChrRecord *chr;
    Model *chrmodel;
    rgba_f32 fogColour; // 192
    s32 fogType; // 188
    s32 renderFlags; // 184
    s32 chrfadealpha; // 180
    ModelRenderData mrData; // 112
    bbox2d bbox;
    rgba_s32 chrShade;
    s32 shadowAlpha; // 76
    PropRecord *prop_held_right; // 72
    PropRecord *prop_held_left; // 68
    PropRecord *prop_held_hat; // 64
    ObjectRecord *held_right_obj; // 60
    ObjectRecord *held_left_obj; // 56
    ObjectRecord *held_hat_obj; // 52

    //

    chr = prop->chr;
    chrmodel = chr->model;
    chrfadealpha = (s32) chr->fadealpha;

    if (!(chr->chrflags & CHRFLAG_04000000))
    {
        f32 f = propGetFogFade(prop, modelGetRadius(chrmodel)); //0-1
        chrfadealpha = (s32) (f * (f32) chrfadealpha);
    }

    if ((chrfadealpha < 0xFF) || (chr->chrflags & CHRFLAG_00020000))
    {
        if (withalpha == 0)
        {
            // nothing to do
            return gdl;
        }
        else
        {
            renderFlags = 3;
        }
    }
    else
    {
        if (withalpha == 0)
        {
            renderFlags = 1;
        }
        else
        {
            renderFlags = 2;
        }
    }

    fogType = fogGetPropDistColor(prop, &fogColour);
    if (fogType != 0)
    {
        if (chrfadealpha > 0)
        {
            mrData = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};


            shadowAlpha = 0x50;

            prop_held_right = chr->weapons_held[GUNRIGHT];
            prop_held_left = chr->weapons_held[GUNLEFT];
            prop_held_hat = chr->handle_positiondata_hat;
            held_right_obj = NULL;
            held_left_obj = NULL;
            held_hat_obj = NULL;

            if (prop_held_right != NULL)
            {
                held_right_obj = prop_held_right->obj;
            }

            if (prop_held_left != NULL)
            {
                held_left_obj = prop_held_left->obj;
            }

            if (prop_held_hat != NULL)
            {
                held_hat_obj = prop_held_hat->obj;
            }

            if ((propGetRoomsBbox2d(prop, &bbox) > 0) && !(chr->chrflags & CHRFLAG_CULL_USING_HITBOX))
            {
                gdl = bgScissorCurrentPlayerViewF(gdl, bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y);
            }
            else
            {
                gdl = bgScissorCurrentPlayerViewDefault(gdl);
            }

            mrData.flags = renderFlags;
            mrData.zbufferenabled = TRUE;
            mrData.gdl = gdl;

            if ((chr->chrflags & CHRFLAG_NO_SHADOW) != 0)
            {
                shadowAlpha = 0;
            }
            else if (fogType == 1)
            {
                shadowAlpha = ((1.0f - fogColour.a) * (f32)(shadowAlpha));
            }

            modelSetShadowAlpha(shadowAlpha);

            chrShade.r = chr->shadecol.r;
            chrShade.g = chr->shadecol.g;
            chrShade.b = chr->shadecol.b;
            chrShade.a = chr->shadecol.a;

            shadingLerp(&chrShade, fogType, &fogColour);

            mrData.envcolour.word = ((gBloodColour.rgba[0] << 0x18) | (gBloodColour.rgba[1] << 0x10)) | (gBloodColour.rgba[2] << 0x08);
            mrData.fogcolour.word = (chrShade.rgba[0] << 0x18) | (chrShade.rgba[1] << 0x10) | (chrShade.rgba[2] << 0x08) | (chrShade.rgba[3] << 0x00);

            if (chrfadealpha < 0xFF)
            {
                mrData.PropType = 8;
                mrData.envcolour.word |= (u8)chrfadealpha;
            }
            else
            {
                mrData.PropType = 7;
            }

            g_playerPerm->time_other_players_on_screen += 1;
            jointListDraw(&mrData, chr->jointList);

            gdl = mrData.gdl;

            if ((held_right_obj != NULL) && (( held_right_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = impactsRenderOnProp(gdl, prop_held_right, withalpha);
            }

            if ((held_left_obj != NULL) && (( held_left_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = impactsRenderOnProp(gdl, prop_held_left, withalpha);
            }

            if ((held_hat_obj != NULL) && (( held_hat_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = impactsRenderOnProp(gdl, prop_held_hat, withalpha);
            }

            if (withalpha != 0)
            {
                mtxsF2L(chr->model->render_pos, chr->model->obj->numMatrices);

                if ((held_right_obj != NULL) && ((held_right_obj->runtime_bitflags & 0x800) == 0))
                {
                    mtxsF2L(held_right_obj->model->render_pos, held_right_obj->model->obj->numMatrices);
                }

                if ((held_left_obj != NULL) && ((held_left_obj->runtime_bitflags & 0x800) == 0))
                {
                    mtxsF2L(held_left_obj->model->render_pos, held_left_obj->model->obj->numMatrices);
                }

                if (held_hat_obj != NULL)
                {
                    mtxsF2L(held_hat_obj->model->render_pos, held_hat_obj->model->obj->numMatrices);
                }
            }
        }
    }

    if (withalpha != 0)
    {
        jointListFree(chr->jointList);
        chr->jointList = NULL;
    }

    return gdl;
}


/**
 * Creates a smoke puff at the front of the character when the character is shot,
 * and also has a ~50% chance of creating a second smoke puff just behind the character.
 */
void chrCreateHitPuffs(PropRecord *prop, s32 anim_id, coord3d *vec, coord3d *pos)
{
    s32 i;
    f32 scale;
    coord3d puffPos;
    s32 index;
    struct ChrHitReaction *entry;

    index = 0;
    i = 0;

    if (g_HitReactionTable[0].hitpart != -1) 
    {
        do 
        {
            if (anim_id == g_HitReactionTable[i].hitpart) 
            {
                index = i;
                break;
            }

            i++;
        }
        while (g_HitReactionTable[i].hitpart != -1);
    }

    entry = &g_HitReactionTable[index];

    if (entry->backImpactPuffCount) {
        // True when randomGetNext() bit 2 is 0, so roughly 50% chance.
        if ((randomGetNext() & 4) == 0) 
        {
            scale = (42.0f / sqrtf(vec->z * vec->z + (vec->x * vec->x + vec->y * vec->y))) + 1.0f;

            puffPos.x = vec->x * scale;
            puffPos.y = vec->y * scale;
            puffPos.z = vec->z * scale;

            mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &puffPos);

            sparkCreate(&puffPos, entry->backImpactPuffCount, entry->backImpactPuffSize, prop->stan->room);
        }
    }

    if (entry->impactPuffCount) 
    {
        sparkCreate(pos, entry->impactPuffCount, entry->impactPuffSize, prop->stan->room);
    }
}


void chrCreateBloodStain(Model *model, s32 hitpart, ModelNode *root, coord3d *pos)
{
    s32 sp_pos[3];
    s32 bestdist;
    ModelNode *bestnode;
    s32 bestindex;
    ModelNode *node;
    s32 opcode;
    ModelRoData_DisplayList_CollisionRecord *rodata;
    ModelRoData_DisplayList_CollisionRecord *relatedrodata;
    ModelRwData_DisplayList_CollisionRecord *relatedrwdata;
    ModelNode *relatednode;
    ModelNode *rwdata;
    Vertex *vtx;
    Vertex *newvertices;
    s32 n;
    s32 paintval;
    s32 relatedindex;

    sp_pos[0] = pos->x;
    sp_pos[1] = pos->y;
    sp_pos[2] = pos->z;

    bestnode = NULL;
    bestindex = 0;
    bestdist = 0x7fffffff;
    node = root;

    while (node != NULL)
    {
        opcode = node->Opcode & 0xff;

        if (opcode == MODELNODE_OPCODE_LOD)
        {
            goto apply_distance;
        }

        if (opcode == MODELNODE_OPCODE_SWITCH)
        {
            goto apply_toggle;
        }

        if (opcode == MODELNODE_OPCODE_HEAD)
        {
            goto apply_head;
        }

        if (opcode != MODELNODE_OPCODE_DLCOLLISION)
        {
            goto after_opcode;
        }

        rodata = &node->Data->DisplayListCollisions;
        n = 0;

        if (rodata->numCollisionVertices > 0)
        {
            vtx = rodata->CollisionVertices;

            do
            {
                s32 dx;
                s32 dy;
                s32 dz;
                s32 dist;

                dx = sp_pos[0] - vtx->coord.x;
                dy = sp_pos[1] - vtx->coord.y;
                dz = sp_pos[2] - vtx->coord.z;
                dist = ((((u32) dx) * ((u32) dx)) + (((u32) dy) * ((u32) dy))) + (((u32) dz) * ((u32) dz));

                if (dist < bestdist)
                {
                    bestdist = dist;
                    bestnode = node;
                    bestindex = n;
                }

                n++;
                vtx++;
            }
            while (n < rodata->numCollisionVertices);
        }

        goto after_opcode;

apply_distance:
        modelApplyDistanceRelations(model, node);
        goto after_opcode;

apply_toggle:
        modelApplyToggleRelations(model, node);
        goto after_opcode;

apply_head:
        modelApplyHeadRelations(model, node);

after_opcode:
        if ((node->Child != NULL) && ((node == root) || ((opcode != MODELNODE_OPCODE_BBOX) && (opcode != MODELNODE_OPCODE_OP17))))
        {
            node = node->Child;
        }
        else
        {
            while (node != NULL)
            {
                if (node == root)
                {
                    node = NULL;
                    break;
                }

                if (node->Next != NULL)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }

    if (bestnode == NULL)
    {
        return;
    }

    node = (ModelNode *) bestnode->Data;
    rwdata = (ModelNode *) modelGetNodeRwData(model, bestnode);
    relatedrodata = NULL;
    relatedrwdata = NULL;
    relatedindex = 0;
    paintval = (randomGetNext() % 50) + 20;

    if (hitpart == 8)
    {
        paintval += 100;
    }

    if (hitpart == 0x0f)
    {
        paintval += 50;
    }

    relatednode = (ModelNode *) ((ModelRoData_DisplayList_CollisionRecord *) node)->CollisionVertices[bestindex].CollisionRelatedNode;

    if (((ModelRoData_DisplayList_CollisionRecord *) node)->CollisionVertices[bestindex].CollisionRelatedNode != NULL)
    {
        relatedrodata = &relatednode->Data->DisplayListCollisions;
        relatedrwdata = &modelGetNodeRwData(model, relatednode)->DisplayListCollisions;
        relatedindex = ((ModelRoData_DisplayList_CollisionRecord *) node)->CollisionVertices[bestindex].CollisionRelatedIndex;
    }

    if (((ModelRwData_DisplayList_CollisionRecord *) rwdata)->Vertices == ((ModelRoData_DisplayList_CollisionRecord *) node)->Vertices)
    {
        newvertices = vtxstoreAllocate(((ModelRoData_DisplayList_CollisionRecord *) node)->numVertices, VTXSTORE_CHR, NULL, 0);

        if (newvertices != NULL)
        {
            s32 j;

            ((ModelRwData_DisplayList_CollisionRecord *) rwdata)->Vertices = newvertices;

            for (j = 0; j < ((ModelRoData_DisplayList_CollisionRecord *) node)->numVertices; j++)
            {
                newvertices[j] = ((ModelRoData_DisplayList_CollisionRecord *) node)->Vertices[j];
            }
        }
    }

    if ((relatedrwdata != NULL) && (relatedrwdata->Vertices == relatedrodata->Vertices))
    {
        newvertices = vtxstoreAllocate(relatedrodata->numVertices, VTXSTORE_CHR, NULL, 0);

        if (newvertices != NULL)
        {
            s32 j;

            relatedrwdata->Vertices = newvertices;

            for (j = 0; j < relatedrodata->numVertices; j++)
            {
                newvertices[j] = relatedrodata->Vertices[j];
            }
        }
    }

    if (((ModelRwData_DisplayList_CollisionRecord *) rwdata)->Vertices != ((ModelRoData_DisplayList_CollisionRecord *) node)->Vertices)
    {
        s32 index;

        index = ((ModelRoData_DisplayList_CollisionRecord *) node)->CollisionVertices[bestindex].index;

        while (index >= 0)
        {
            ((ModelRwData_DisplayList_CollisionRecord *) rwdata)->Vertices[index].a = paintval;
            index = ((ModelRoData_DisplayList_CollisionRecord *) node)->PointUsage[index];
        }
    }

    if ((relatedrwdata != NULL) && (relatedrwdata->Vertices != relatedrodata->Vertices))
    {
        s32 index;

        index = relatedrodata->CollisionVertices[relatedindex].index;

        while (index >= 0)
        {
            relatedrwdata->Vertices[index].a = paintval;
            index = relatedrodata->PointUsage[index];
        }
    }
}


/**
 * Tests a shot's ray against a character.
 *
 * First tests the shot against the chr's bounding sphere expanded by the
 * largest held weapon's model size. On a bounds hit, the model hit list is
 * walked for a body part. A confirmed body hit is registered via shotAddHit; 
 * a near miss inside max range flags CHRFLAG_NEAR_MISS and bumps numclosearghs.
 *
 * PD: chr_test_hit
 */

/**
 * This assert is from the former NONMATCHING block and belongs somewhere in chrTestHit.
 * 
 * #ifdef DEBUG
 * assert(hits && hits->HasHits());
 * #endif
 */
void chrTestHit(PropRecord *prop, ShotData *shotdata)
{
    ChrRecord *chr;
    Model *model;
    f32 modelsize;
    s32 hitpart;
    Model *hitmodel;
    ModelNode *hitnode;
    s32 hitbounds;
    struct HitThing hitthing;
    s32 mtxindex;
    ModelNode *dlnode;
    Mtxf *submatrix;
    f32 heldmodelsize;
    Mtxf *nodemtx;
    f32 len;
    struct WeaponObjRecord *weapon;
    f32 dist;
    JointListEntry *entry;
    coord3d viewdir;
    f32 size;
    s32 i;
    coord3d diff;

    chr = prop->chr;

    if (chr->actiontype == ACT_DEAD)
    {
        return;
    }

    model = chr->model;
    modelsize = modelGetRadius(model);

    if ((prop->flags & PROPFLAG_ONSCREEN) == FALSE)
    {
        return;
    }

    hitpart = HIT_NULL_PART;

    if (!((prop->zDepth - modelsize) < shotdata->maxdist))
    {
        return;
    }

    hitmodel = NULL;
    hitnode = NULL;
    hitbounds = FALSE;
    dlnode = NULL;
    submatrix = modelGetRootMtx(model);
    heldmodelsize = 0.0f;

    for (i = 0; i < 2; i++)
    {
        if (chr->weapons_held[i] != NULL)
        {
            weapon = chr->weapons_held[i]->weapon;
            size = model->scale * modelGetRadius(weapon->model);

            if (heldmodelsize < size)
            {
                heldmodelsize = size;
            }
        }
    }

    modelsize += heldmodelsize;

    if (intersectRaySphere(&shotdata->viewOrigin, &shotdata->viewDir, (coord3d *) &submatrix->m[3][0], modelsize))
    {
        hitbounds = TRUE;
        hitpart = HIT_LEFT_FOOT;
    }

    if (hitpart != HIT_NULL_PART)
    {
        entry = chr->jointList;
        hitpart = jointListFindHit(&entry, &shotdata->viewOrigin, &shotdata->viewDir, &hitmodel, &hitnode);

        while ((hitpart == HIT_GUN) || (hitpart == HIT_HAT))
        {
            if (modelFindTriangleHit(hitmodel, hitnode, &shotdata->viewOrigin, &shotdata->viewDir, &hitthing, &mtxindex, &dlnode))
            {
                break;
            }

            // Leave if (1) for matching.
            if (1)
            {
                hitpart = jointListFindHitNext(&entry, &shotdata->viewOrigin, &shotdata->viewDir, &hitmodel, &hitnode);
            }
        }
    }

    if (hitpart > HIT_NULL_PART)
    {
        viewdir.x = shotdata->viewDir.x;
        viewdir.y = shotdata->viewDir.y;
        viewdir.z = shotdata->viewDir.z;

        submatrix = camGetViewToWorldMtxf();
        mtx4RotateVecInPlace(submatrix, &viewdir);
        nodemtx = modelFindNodeMtx(hitmodel, hitnode, 0);


        diff.x = nodemtx->m[3][0] - shotdata->viewOrigin.x;
        diff.y = nodemtx->m[3][1] - shotdata->viewOrigin.y;
        diff.z = nodemtx->m[3][2] - shotdata->viewOrigin.z;

        len = sqrtf((diff.z * diff.z) + ((diff.x * diff.x) + (diff.y * diff.y)));
        dist = -((shotdata->viewDir.z * len) + shotdata->viewOrigin.z);

        if (dist < shotdata->maxdist)
        {
            shotAddHit(shotdata, prop, dist, hitpart, hitnode, &hitthing, mtxindex, dlnode, hitmodel, TRUE, FALSE);
        }
    }

    if (hitbounds && (prop->zDepth <= shotdata->maxdist))
    {
        chr->chrflags |= CHRFLAG_NEAR_MISS;
        chr->numclosearghs++;
    }
}


/**
 * Resolves a known hit against a character.
 */
void chrHandleBulletHit(struct ShotData *shot, struct BulletHit *bhit)
{
    f32 scale;
    Mtxf invmtx;
    coord3d nearhitpos;
    s32 temp;
    coord3d hitpos;
    Mtxf *mtx;
    ChrRecord *chr;
    PropRecord *prop;
    const SurfaceHit *sound;
    WeaponObjRecord *weaponobj;
    s32 i;
    s32 temp2;
    coord3d jointpos;
    const SurfaceHit *sound2;

    chr = bhit->prop->chr;

    // Calculate the view space hit position for impact effects.
    hitpos.f[0] = shot->viewOrigin.x - ((bhit->dist * shot->viewDir.x) / shot->viewDir.z);
    hitpos.f[1] = shot->viewOrigin.y - ((bhit->dist * shot->viewDir.y) / shot->viewDir.z);
    hitpos.f[2] = shot->viewOrigin.z - bhit->dist;

    scale = 1.0f - (42.0f / sqrtf(SQ(hitpos.f[0]) + SQ(hitpos.f[1]) + SQ(hitpos.f[2])));

    /** Use scale to create a near hit position closer to the camera.
     *  Probably to prevent the tracer effect and smoke puffs from landing inside the character model.
    */
    nearhitpos.x = hitpos.f[0] * scale;
    nearhitpos.y = hitpos.f[1] * scale;
    nearhitpos.z = hitpos.f[2] * scale;

    mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &nearhitpos);

    // Point the tracer effect towards the impact position.
    gunSetTracerTarget(&nearhitpos);

    // Make a fleshy impact sound.
    shotPlayHitPropSound(shot->weapon, bhit->prop, -1);

    chrCreateHitPuffs(bhit->prop, bhit->hitpart, &hitpos, &nearhitpos);

    // Apply damage to the character.
    if (!chrTakeShot(chr, bhit->hitpart, &shot->dir, shot->weapon, TRUE)) {
        return;
    }

    if (bhit->hitpart == HIT_GUN) {
        for (i = 0; i != 2; i++) {
            prop = chr->weapons_held[i];

            if (prop != NULL) {
                weaponobj = (WeaponObjRecord *)prop->obj;
                /**
                 * If a character is holding an explosive such as a grenade, and the explosive is shot, detonate it.
                 */
                if (weaponobj->model == bhit->model) {
                    if (
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_GRENADE ||
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_GRENADEROUND ||
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_ROCKETROUND ||
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_TIMEDMINE ||
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_BOMBCASE  ||
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_REMOTEMINE  ||
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_PROXIMITYMINE
                    )
                    {
                        objSetDropType(prop, 1);
                        chr->hidden |= 1;

                        objApplyDamage(prop->obj, weaponGetDamage(shot->weapon), &hitpos, shot->weapon, playerGetCurrentNum());
                    // Create a bullet hole on the character's held weapon.
                    } else {
                        if (bhit->hit.texturenum < 0) {
                            sound = &g_SurfaceHits[HIT_DEFAULT];
                        } else {
                            sound = &g_SurfaceHits[g_Textures[bhit->hit.texturenum].hitTexture];
                        }

                        temp = randomGetNext() % (s16)sound->numImpacts;
                        impactCreate(&bhit->hit.hitpos, &bhit->hit.normal, sound->impacts[temp], 1, prop, bhit->room, 0);
                    }
                }
            }
        }

        return;
    }

    // Create a bullet on hole on a hat or helmet attached to a character's head.
    if (bhit->hitpart == HIT_HAT) {
        if (bhit->hit.texturenum < 0) {
            sound2 = &g_SurfaceHits[HIT_DEFAULT];
        } else {
            sound2 = &g_SurfaceHits[g_Textures[bhit->hit.texturenum].hitTexture];
        }

        temp2 = randomGetNext() % (s16)sound2->numImpacts;
        impactCreate(&bhit->hit.hitpos, &bhit->hit.normal, sound2->impacts[temp2], 1, chr->handle_positiondata_hat, bhit->room, 0);

        return;
    }

    mtx = modelFindNodeMtx(bhit->model, bhit->node, 0);

    jointpos.x = hitpos.f[0];
    jointpos.y = hitpos.f[1];
    jointpos.z = hitpos.f[2];

    jointpos.x += (jointpos.x - mtx->m[3][0]) * 0.5f;
    jointpos.y += (jointpos.y - mtx->m[3][1]) * 0.5f;
    jointpos.z += (jointpos.z - mtx->m[3][2]) * 0.5f;

    jointpos.x -= modelGetNodeRadius(bhit->model, bhit->node) * 0.5f * shot->viewDir.x;
    jointpos.y -= modelGetNodeRadius(bhit->model, bhit->node) * 0.5f * shot->viewDir.y;
    jointpos.z -= modelGetNodeRadius(bhit->model, bhit->node) * 0.5f * shot->viewDir.z;

    mtx4InvertRigid(mtx, &invmtx);
    mtx4TransformVecInPlace(&invmtx, &jointpos);

    // Create a blood stain at the impact point.
    chrCreateBloodStain(bhit->model, bhit->hitpart, bhit->node, &jointpos);
}


/**
 * Removed.
 */


/**
 * Iterates g_ChrSlots. Returns the first object that (1) model
 * is not null and (2) chrnum matches index.
 *
 * chrFindByLiteralId
 */
ChrRecord* chrFindByLiteralId(s32 index)
{
    s32 i;

    for (i=0; i<g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model != NULL && g_ChrSlots[i].chrnum == index)
        {
            return &g_ChrSlots[i];
        }
    }

    return NULL;
}


PropRecord *chrGetEquippedWeaponProp(ChrRecord *self, GUNHAND hand)
{
    return self->weapons_held[hand];
}


PropRecord *chrGetEquippedWeaponPropWithCheck(ChrRecord *self, GUNHAND hand)
{
    PropRecord *gunprop = self->weapons_held[hand];
    if (gunprop)
    {
        WeaponObjRecord *wep = gunprop->weapon;

        if (weaponHasFlag(wep->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN) == 0)
        {
            gunprop = NULL;
        }
    }
    return gunprop;
}


/**
 * Updates character collision bounds based on chracter width.
 *
 * @param arg0: prop
 * @param arg1: out parameter, will contain character collision_bounds.
 * @param arg2: out parameter, will contain 0 or 4
 * @param y_out: out parameter, will be character ground + character height
 * @param ground: out parameter, will contain character ground
 */
void chrUpdateCollisionBounds(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *y_out, f32 *ground)
{
    ChrRecord *chr;

    chr = prop->chr;

    if (
        (chr->actiontype != ACT_DIE) &&
        (chr->actiontype != ACT_DEAD) &&
        ((chr->chrflags & (CHRFLAG_00010000 | CHRFLAG_HIDDEN)) == 0) &&
        ((chr->hidden & CHRHIDDEN_MOVING) == 0)
        )
    {
        *edges = 4;
        *polygon = chr->collision_bounds;

        //collision box is a diamond around chr

        chr->collision_bounds[0].x = prop->pos.x + chr->chrwidth;
        chr->collision_bounds[0].y = prop->pos.z;

        chr->collision_bounds[1].x = prop->pos.x;
        chr->collision_bounds[1].y = prop->pos.z + chr->chrwidth;

        chr->collision_bounds[2].x = prop->pos.x - chr->chrwidth;
        chr->collision_bounds[2].y = prop->pos.z;

        chr->collision_bounds[3].x = prop->pos.x;
        chr->collision_bounds[3].y = prop->pos.z - chr->chrwidth;

        *ground = chr->ground;
        *y_out = *ground + chr->chrheight;

        return;
    }

    *edges = 0;
}


/**
 * @param arg0: prop
 * @param width: out parameter, will be set to character width
 * @param height: out parameter, will be set to character height - 20
 * @param always_20: out parameter, will be set to 20
 */
void chrGetChrWidthHeight(PropRecord *prop, f32 *width, f32 *height, f32 *always_20)
{

    ChrRecord *c = prop->chr;

    *width = c->chrwidth;
    *height = c->chrheight - 20.0f;
    *always_20 = 20.0f;
}


f32 chrGetChrGround(PropRecord *prop)
{
    ChrRecord *c = prop->chr;
    return c->ground;
}


/**
 * Calculate auto aim position coordinates.
 *
 * US address 7F023194.
*/
s32 chrGetOnscreenRenderBounds(PropRecord *prop, coord3d *pos, coord2d *xbounds, coord2d *ybounds)
{
    struct ChrRecord *chr;

    chr = prop->chr;

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        if ((chr->actiontype != ACT_DIE) && (chr->actiontype != ACT_DEAD) && !(chr->chrflags & CHRFLAG_NO_AUTOAIM))
        {
            struct Model *model;
            RenderPosView *model_render_pos_1;
            RenderPosView *model_render_pos_2;

            model = chr->model;
            model_render_pos_1 = &model->render_pos[0];
            model_render_pos_2 = &model->render_pos[1];

            pos->f[2] = model_render_pos_2->pos.m[3][2] + ((model_render_pos_1->pos.m[3][2] - model_render_pos_2->pos.m[3][2]) * 0.25f);

            if (pos->f[2] < 0.0f)
            {
                pos->f[0] = model_render_pos_2->pos.m[3][0] + ((model_render_pos_1->pos.m[3][0] - model_render_pos_2->pos.m[3][0]) * 0.25f);
                pos->f[1] = model_render_pos_2->pos.m[3][1] + ((model_render_pos_1->pos.m[3][1] - model_render_pos_2->pos.m[3][1]) * 0.25f);

                ybounds->y = 0.0f;
                ybounds->x = 0.0f;

                xbounds->x = 0.0f;
                xbounds->y = 0.0f;

                modelGetXYExtents(model, &xbounds->y, &xbounds->x, &ybounds->y, &ybounds->x);

                return 1;
            }
        }
    }

    return 0;
}


//consolidate with other align_addr_zero macro when cleaning up
#define align_addr_zero(X) ((((X) + 0xF) | 0xF) ^ 0xF)

void chrsInit(void) {
    g_ChrAnimRate = 1.0f;
    g_ChrDebugAnimOverride = 0;
    g_ChrDebugAnimIndex = 0;
    g_ChrDebugPaused = 0;
    g_ChrDebugStep = 0;
    g_ChrUnused = 0;
    g_ChrShowPatrols = FALSE;
    g_NextChrNum = 0x1388;
    g_ChrSlots = 0;
    g_NumChrSlots = 0;
    objectiveregisters1 = 0;
    g_AiAccuracyModifier = 1.0f;
    g_AiDamageModifier = 1.0f;
    g_AiHealthModifier = 1.0f;
    g_AiReactionSpeed = 1.0f;
    g_SeenBondRecentlyGuardCount = 0;
}

/**
 * Makes a background chr (g_BgChrs) for each of the setup's AI lists numbered 0x1000 and
 * up: invisible dummies that only run their AI list.
 */
void bgChrsInit(void)
{
    s32 i;
    s32 count;

    g_BgChrs = NULL;
    g_NumBgChrs = 0;

    if (!g_CurrentSetup.ailists) { return; }

    count = 0;

    // Count the number of background AI lists
    i = 0;
    while (g_CurrentSetup.ailists[i].ailist)
    {
        if (g_CurrentSetup.ailists[i].ID >= 0x1000)
        {
            g_NumBgChrs++;
        }

        i++;
    }

    if (g_NumBgChrs > 0)
    {
        ChrRecord tmp;

        // Allocate BG chrs
        g_BgChrs = mempAlloc(ALIGN16_a(g_NumBgChrs * sizeof(ChrRecord)), MEMPOOL_STAGE);

        // Initialise BG chrs
        i = 0;
        while (g_CurrentSetup.ailists[i].ailist)
        {
            if (g_CurrentSetup.ailists[i].ID >= 0x1000)
            {
                tmp = g_BgChrTemplate;

                g_BgChrs[count] = tmp;

                g_BgChrs[count].chrnum = 0xFE;
                g_BgChrs[count].ailist = g_CurrentSetup.ailists[i].ailist;
                g_BgChrs[count].aioffset = 0;
                g_BgChrs[count].aireturnlist = -1;
                g_BgChrs[count].actiontype = ACT_NULL;
                count++;
            }

            i++;
        }
    }
}

void chrsAllocateSlots(s32 count)
{
    s32 i;
    
    g_NumChrSlots = count + 10; //special guards?

    g_ChrSlots = mempAlloc(align_addr_zero(g_NumChrSlots * sizeof(ChrRecord)), MEMPOOL_STAGE);
    for(i = 0; g_NumChrSlots > i; i++)
    {
        g_ChrSlots[i].model = 0;
    }
}


void chrsFreeAll(void)
{
    int i;

    for (i = 0; i < g_NumChrSlots; i++) {
        if (g_ChrSlots[i].model != 0) {
            chrFreeResources(g_ChrSlots[i].prop);
            propDelist(g_ChrSlots[i].prop);
            propDisable(g_ChrSlots[i].prop);
            propFree(g_ChrSlots[i].prop);
        }
    }
}
