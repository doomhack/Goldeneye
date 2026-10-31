/**
 * Guard animation tables: the hit reaction for each body part (g_HitReactionTable), the
 * death and flinch animations it picks from, the explosion death animations, and the
 * firing animations for each weapon type, standing, kneeling and on the move.
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
#include "game/nav/stan.h"
#include "game/world/surface.h"
#include "game/math/physics.h"
#include "game/world/bgfog.h"
#include "game/math/line_tri_intersect.h"

/*
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
// bondview2.c uses the second entry on its own.
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
    { {PTR_ANIM_fire_walking}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 1.4, 1.3 },

    { {PTR_ANIM_fire_running}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 1.1, 1.2 },

    { {PTR_ANIM_aim_walking_one_handed_weapon}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    { {PTR_ANIM_aim_running_one_handed_weapon}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    { {PTR_ANIM_fire_walking_dual_wield}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    { {PTR_ANIM_fire_running_dual_wield}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    { {PTR_ANIM_fire_walking_dual_wield_hands_crossed}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    { {PTR_ANIM_fire_running_dual_wield_hands_crossed}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    { {PTR_ANIM_aim_running_left_one_handed_weapon}, 0.0, 0.0, 1.5707964, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    { {PTR_ANIM_aim_running_right_one_handed_weapon}, 0.0, 0.0, 4.712389, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    {{0}, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

