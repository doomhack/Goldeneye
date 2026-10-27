/**
 * The hands and their guns: which weapon each hand holds, loading its first-person
 * model, switching and cycling weapons, reloading, zoom, weapon stat lookups
 * (weaponGet*), sway, and launching thrown and fired projectiles. The gunDeb* functions
 * are a debug keyframe editor. See docs/subsystems/weapons.md.
 */
#include <ultra64.h>
#include "include/limits.h"
#include <bondconstants.h>
#include <bondtypes.h>
#include <bondgame.h>
#include <music.h>
#include <snd.h>
#include "game/player/bondview.h"
#include "game/player/bondinv.h"
#include "game/player/gun.h"
#include "game/model/chrobjdata.h"
#include "game/prop/propobj.h"
#include "game/mission/objective.h"
#include "game/math/quaternion.h"
#include "game/gfx/imagebank.h"
#include "game/gfx/texrect.h"
#include "game/gfx/texselect.h"
#include "game/player/player.h"
#include "game/level/lv.h"
#include "random.h"
#include "game/math/trig.h"
#include "game/prop/setup.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/gfx/image.h"
#include "game/gfx/tex.h"
#include "game/debug/debugmenu_handler.h"
#include "fr.h"
#include "assets/obseg/text/LgunE.h"
#include "game/text/textrelated.h"
#include "game/chr/chrai.h"
#include "game/model/model.h"
#include "game/menu/watchmenu.h"
#include "game/menu/mpmenu.h"
#include "joy.h"
#include "game/math/matrix.h"
#include "game/player/bondinv.h"
#include "game/nav/stan.h"
#include "gbi_extension.h"
#include "game/math/physics.h"
#include "game/math/interp.h"


// bss
ALSoundState *g_CasingSfxState;
ALSoundState* g_UnusedSfxState; // Unused, type assumed from surrounding variables.
ALSoundState* g_ImpactSfxStates[NUM_IMPACT_SFX_STATES];

CasingRecord g_Casings[20];
s32 dword_CODE_bss_80076A48; // Unused

#ifdef REFRESH_PAL
    /* PAL */
    #define THROWN_ITEM_REFRESH_RATE                   50
    #define THROWN_ITEM_TIMER_SOLO                     250
    #define THROWN_ITEM_TIMER_MULTI                    150
    #define THROWN_ITEM_TIMER_DEFAULT                  200
    #define GLGRENADE_TIMER                            1000
    #define DUAL_WIELD_TRIGGER_SWAP_TICKS              24
    #define DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS       36
    #define WATCH_SOUND_DURATION_TICKS                 250
    #define GUN_SPRING_DAMP                            0.9402999877929688f
    #define GUN_SPRING_SCALE                           0.05970001220703125f
#else
    /* NTSC */
    #define THROWN_ITEM_REFRESH_RATE                   60
    #define THROWN_ITEM_TIMER_SOLO                     300
    #define THROWN_ITEM_TIMER_MULTI                    180
    #define THROWN_ITEM_TIMER_DEFAULT                  240
    #define GLGRENADE_TIMER                            1200
    #define DUAL_WIELD_TRIGGER_SWAP_TICKS              20
    #define DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS       30
    #define WATCH_SOUND_DURATION_TICKS                 300
    #define GUN_SPRING_DAMP                            0.95f
    #define GUN_SPRING_SCALE                           0.050000012f
#endif

extern f32 g_GLGrenadeLaunchUnk8C;
extern f32 g_GLGrenadeLaunchUnk94;
extern f32 g_TankShellSpeed;

// data
////D:80032440
//rgba_u8 D_80032440[] = {
//	{0x96, 0x96, 0x96, 0},
//	{0x96, 0x96, 0x96, 0}
//};
//
////D:80032448
//rgba_u8 D_80032448[] = {
//	{0xFF, 0xFF, 0xFF, 0},
//	{0xFF, 0xFF, 0xFF, 0},
//	{0xB2, 0x4D, 0x2E, 0}
//};
/**
 * Controls the lighting on environment mapped weapons such as the Cougar Magnum and Golden Gun.
 */
Lights1 g_WeaponEnvmapLight = gdSPDefLights1(
    0x96, 0x96, 0x96,   // ambient RGB
    0xff, 0xff, 0xff,   // diffuse RGB
    0xb2, 0x4d, 0x2e);  // direction
//D:80032454
//u32 D_80032454 = 0;

//D:80032458
u32 D_80032458 = 0;

//D:8003245C
u32 size_item_buffer[] = {0x14820, 0x14820};

//D:80032464
u32 D_80032464[] ={0x7530, 0x7530};



//D:8003246C
CartridgeModelFileRecord ejected_cartridge[] = {
	{&cartridge_header, "GcartridgeZ"},
	{&cartrifle_header, "GcartrifleZ"},
	{&cartblue_header, "GcartblueZ"},
	{&cartshell_header, "GcartshellZ"},
	{0, ""}
};

#include <assets/obseg/gun/gunWeaponStats.inc.c>

//D:80033924
#include <assets/obseg/gun/gunModelFileRecord.inc.c>
#include <PR/libaudio.h>

//D:80034C9C
u32 g_GunDebApplyKeyframe = 0;
//D:80034CA0
u32 g_gunDebKeyframeIndex = 0;

//D:80034CA4
u32 D_80034CA4[] = {
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,    0x3F000000,
	0x41000000,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	0x3F000000,    0x41000000,           0x0,    0x40C00000,
	0xBFC00000,           0x0,    0x40B487B1,    0x3E70C0AD,
	0x3E0AE536,    0x3F000000,    0x41000000,           0x0,
	0x41480000,    0xC0600000,           0x0,    0x40C159EC,
	0x3D374BC7,    0x3F0E4378,    0x3F000000,    0x41000000,
	       0x0,    0xC1200000,    0xC1300000,           0x0,
	0x3F9ED962,    0x3EA24C40,    0x3F8B0DF1,    0x3F000000,
	0x41000000,           0x0,    0xC1600000,    0xC1700000,
	       0x0,    0x3FEA4780,    0x40C498E3,    0x3FA316D3,
	0x3F000000,    0x41200000,           0x0,    0xBF800000,
	0xC1100000,           0x0,    0x3EC4BBA1,    0x3EB87C42,
	0x3DD75968,    0x3F000000,    0x41200000,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,    0x3F000000,    0x41A00000,
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,    0x3F000000,
	0x41A00000,           0x1,           0x0,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	       0,           0
};

u32 D_80034E0C[] = {
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,    0x3F000000,
	0x41000000,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	0x3F000000,    0x41000000,           0x0,    0xC1080000,
	0xC0C00000,           0x0,    0x40AF7506,    0x40BAB4B9,
	0x40C2A5C2,    0x3F000000,    0x41000000,           0x0,
	0xC0400000,    0xC0600000,           0x0,    0x3ECE08F2,
	0x40B75721,    0x40B62409,    0x3F000000,    0x41000000,
	       0x0,    0xBF000000,    0xC1080000,           0x0,
	0x3F9DFD7A,    0x40B768CD,    0x40B37BDF,    0x3F000000,
	0x41000000,           0x0,    0x40E00000,    0xC1E40000,
	0xBFC00000,    0x3FA74949,    0x40B63EBC,    0x40B6443D,
	0x3F000000,    0x41200000,           0x0,    0xBFC00000,
	0xC1100000,           0x0,    0x3D8ADEEC,    0x40C84E72,
	0x3E506749,    0x3F000000,    0x41200000,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,    0x3F000000,    0x41A00000,
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,    0x3F000000,
	0x41A00000,           0x1,           0x0,           0x0,
	       0x0,           0x0,           0x0,           0x0,
           0x0,           0x0
};

/**
 * Throwing Knife animation for when Z is pressed/held down.
 */
struct Weapon1PTransformKeyframe throwKnifeDrawBackKeyframes[6] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 4.5f}, { 5.576369f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 20.5f}, { 5.26209f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 3.0f, 5.5f}, { 0.031375f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Throwing Knife animation for when Z is released.
 */
struct Weapon1PTransformKeyframe throwKnifeReleaseKeyframes[6] = {
    { 0, { 0.0f, 0.0f, 4.5f}, { 5.576369f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 20.5f}, { 5.26209f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 3.0f, 5.5f}, { 0.031375f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, -20.0f, 18.0f}, { 0.785458f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 0, { 0.0f, -20.0f, 18.0f}, { 0.785458f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Keyframes for the Grenade which is strange since you cannot see it on screen. Perhaps the developers once intended to have a proper first person Grenade throwing animation?
 */
struct Weapon1PTransformKeyframe grenadeThrowKeyframes[6] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 10.0f, 12.5f, 17.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 10.0f, 34.5f, 25.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 10.0f, 34.5f, 25.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Keyframes for the Timed Mine, but changing the durations has no effect.
 */
struct Weapon1PTransformKeyframe timedMineThrowKeyframes[6] = {
    { 0, { 10.0f, 34.5f, 25.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 10.0f, 34.5f, 25.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 10.0f, 12.5f, 17.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Keyframes for the Proximity Mine. Changing the durations does effect the time it takes to throw the mine, although nothing is seen on screen.
 */
struct Weapon1PTransformKeyframe proxMineThrowKeyframes[6] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 0.0f, 0.0f, 4.5f}, { 5.576369f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 0.0f, 0.0f, 20.5f}, { 5.26209f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 3.0f, 5.5f}, { 0.031375f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Keyframes for the Remote Mine. Changing the durations does not effect the time it takes to throw them,
 * but it does change the time it takes before you can throw another.
 */
struct Weapon1PTransformKeyframe remoteMineThrowKeyframes[7] = {
    { 0, { 0.0f, 0.0f, 4.5f}, { 5.576369f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 20.5f}, { 5.26209f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 3.0f, 5.5f}, { 0.031375f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, -20.0f, 18.0f}, { 0.785458f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Slapper attack when the hand starts at the right of the screen then chops downward and to the left.
 */
Weapon1PTransformKeyframe fistMeleeKeyframes1[10] = {
    { 0, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.5f, 10.0f },
    { 0, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.5f, 10.0f },
    { 0, {   6.0f, 23.0f, 0.0f }, {  5.91572f, 0.085832f, 0.219482f }, 0.5f, 10.0f },
    { 0, {  18.0f, 35.0f, 9.5f }, { 4.998193f, 0.084203f, 0.268954f }, 0.5f, 10.0f },
    { 0, { -20.0f, 25.5f, 4.0f }, { 0.126148f, 0.304284f, 0.548047f }, 0.5f, 10.0 },
    { 0, { -28.0f, -4.0f, 2.0f }, { 0.506821f,  0.51473f, 0.484098f }, 0.5f,  1.0f },
    { 0, { -28.0f, -4.0f, 2.0f }, { 0.506821f,  0.51473f, 0.484098f }, 0.5f,  1.0f },
    { 0, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.5f, 20.0f },
    { 0, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.5f, 20.0f },
    { 1, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.0f,  0.0f }
};

/**
 * Slapper attack when the hand moves to the left of the screen then chops downward and to the right.
 */
Weapon1PTransformKeyframe fistMeleeKeyframes2[10] = {
       { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
       { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
       { 0, { -6.0f, 23.0f, 0.0f}, { 5.08683f, 6.131295f, 5.534376f}, 0.5f, 10.0f},
       { 0, { -18.0f, 35.0f, 9.5f}, { 4.880698f, 0.070396f, 5.53615f}, 0.5f, 10.0f},
       { 0, { 8.0f, 25.5f, 4.0f}, { 0.107213f, 6.062361f, 5.404225f}, 0.5f, 10.0f},
       { 0, { 28.0f, -4.0f, 2.0f}, { 0.107213f, 6.062361f, 5.404225f}, 0.5f, 1.0f},
       { 0, { 28.0f, -4.0f, 2.0f}, { 0.107213f, 6.062361f, 5.404225f}, 0.5f, 1.0f},
       { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
       { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
       { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Sniper swing right to left.
 */
Weapon1PTransformKeyframe sniperMeleeKeyframes1[11] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 9.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, {9.5f, -0.5f, 3.5f}, { 0.291053f, 5.584375f, 6.212358f}, 0.5f, 8.0f},
    { 0, {18.0f, 7.5f, 3.5f}, { 0.439372f, 5.945201f, 5.993666f}, 0.5f, 8.0f},
    { 0, {-9.0f, 8.5f, 5.5f}, { 0.704803f, 0.194459f, 6.168447f}, 0.5f, 7.0f},
    { 0, {-29.0f, -5.5f, 5.5f}, { 2.281831f, 1.106353f, 1.489998f}, 0.5f, 7.0f},
    { 0, {-57.5f, -27.5f, 5.5f}, { 2.281831f, 1.106353f, 1.489998f}, 0.5f, 7.0f},
    { 0, {-19.5f, -20.0f, 5.5f}, { 1.22519f, 0.726087f, 1.210713f}, 0.5f, 15.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Sniper swing left to right.
 */
Weapon1PTransformKeyframe sniperMeleeKeyframes2[11] = {
    { 0, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.5f,  9.0f },
    { 0, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.5f,  8.0f },
    { 0, { -15.5f,  0.5f, 15.0f }, {  0.9344959855079651f,  0.6256099939346313f,  0.2237969934940338f }, 0.5f,  8.0f },
    { 0, { -23.0f,  2.0f, 12.0f }, {  1.8016400337219238f,  0.9494050145149231f,  0.6307389736175537f }, 0.5f,  8.0f },
    { 0, { -18.0f, -0.5f,  4.0f }, {  0.8478249907493591f,  0.9247649908065796f, 0.07744300365447998f }, 0.5f,  7.0f },
    { 0, {  10.5f,  5.0f,  2.5f }, { 0.22940599918365479f, 0.24570399522781372f, 0.09906300157308578f }, 0.5f,  7.0f },
    { 0, {  18.0f,  5.0f,  2.5f }, { 0.03281300142407417f,    6.20933723449707f,  0.1350640058517456f }, 0.5f,  7.0f },
    { 0, {   9.5f,  3.5f, -1.5f }, {   6.273238182067871f,   6.005795001983643f, 0.08971499651670456f }, 0.5f,  7.0f },
    { 0, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.5f, 20.0f },
    { 0, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.5f, 20.0f },
    { 1, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.0f,  0.0f },
};

/**
 * Animation when the Taser is lowering to fire position.
 */
Weapon1PTransformKeyframe taserFireKeyFrames[6] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.5f, -6.0f, -8.0f}, { 0.439468f, 0.278829f, 0.195178f}, 0.5f, 8.0f},
    { 0, { -2.0f, -8.0f, -10.0f}, { 1.101655f, 0.460753f, 0.570961f}, 0.5f, 8.0f},
    { 0, { -2.0f, -8.0f, -10.0f}, { 1.101655f, 0.460753f, 0.570961f}, 0.5f, 8.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Animation once the Taser has fired and goes back to idle position.
 */
Weapon1PTransformKeyframe taserRaiseKeyframes[6] = {
    {0, { -2.0f, -8.0f, -10.0f}, {1.101655f, 0.460753f, 0.570961f}, 0.5f, 8.0f},
    {0, { -2.0f, -8.0f, -10.0f}, {1.101655f, 0.460753f, 0.570961f}, 0.5f, 8.0f},
    {0, { 0.5f, -6.0f, -8.0f}, {0.439468f, 0.278829f, 0.195178f}, 0.5f, 8.0f},
    {0, { 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    {0, { 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    {1, { 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

coord3d D_80035C40 = {0.0f, 0.0f, 0.0f};
coord3d D_80035C4C = {0.0f, 0.0f, 0.0f};
coord3d D_80035C58 = {0.0f, 0.0f, -1.0f};
coord3d D_80035C64 = {0.0f, 1.0f, 0.0f};
coord3d D_80035C70 = {6.2536321f, 6.2592888f, 0.204238f};
coord3d D_80035C7C = {0.25044999f, 0.90482301f, 0.28716999f};
coord3d D_80035C88 = {1.715736f, 0.37460899f, 0.92193699f};

//D:80035C94
f32 D_80035C94 = 0;


//D:80035C98
Vtx D_80035C98 = {0, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff };
//D:80035CA8
coord3d D_80035CA8 = { 0.0f, 0.0f, 0.0f };
//D:80035CB4
coord3d D_80035CB4 = { 0.0f, 0.0f, 0.0f };
// These were a zero scalar followed by an array, read as one ModelRenderData
// (or coord3d) through a cast. That only works while the compiler keeps them
// adjacent, which -G 8 and zero-init-in-bss do not, so define real structs.
//D:80035CC0
ModelRenderData D_80035CC0 = {NULL, TRUE, 0x00000003};
//D:80035D00
ModelRenderData D_80035D00 = {NULL, TRUE, 0x00000003};
//D:80035D40
ModelRenderData D_80035D40 = {NULL, TRUE, 0x00000003};
//D:80035D44
u32 watchControllerButtonBases[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//D:0x80035E04
struct RicochetSoundsSmall ricochet_sounds_small = {
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_5_A_SFX,      RICO_5_B_SFX,      RICO_5_C_SFX,      RICO_5_D_SFX,
    RICO_6_HBBA_A_SFX, RICO_6_HBBA_B_SFX, RICO_6_HBBA_C_SFX, RICO_6_HBBA_D_SFX
};

//D:80035E2C
struct PunchSounds punch_sounds = {
    PUNCH1_SFX,
    PUNCH2_SFX,
    PUNCH3_SFX
};

//D:80035E34
struct BulletFleshSounds bullet_flesh_sounds = {
    HIT_BULLET_FLESH_SFX,
    HIT_BULLET_FLESH_SFX
};

struct LaserRichochetSounds laser_ricochet_sounds = {
    RICO_LASER2_SFX,
    RICO_LASER3_SFX
};

struct RicochetSoundsLarge ricochet_sounds_large = {
	RICO_12_GBU_A_SFX, RICO_12_GBU_B_SFX, RICO_12_GBU_C_SFX, RICO_12_GBU_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_5_A_SFX,      RICO_5_B_SFX,      RICO_5_C_SFX,      RICO_5_D_SFX,
    RICO_6_HBBA_A_SFX, RICO_6_HBBA_B_SFX, RICO_6_HBBA_C_SFX, RICO_6_HBBA_D_SFX
};

//D:80035E84
struct EarWhistleSounds ear_whistle_sounds = {
    RICO_EAR_WHISTLE1_SFX,
    RICO_EAR_WHISTLE2_SFX,
    RICO_EAR_WHISTLE3_SFX,
    RICO_EAR_WHISTLE4_SFX,
    RICO_EAR_WHISTLE5_SFX
};

//D:80035E90
struct sfx2 watchlaser_fire_sounds = { RICO_LASER2_SFX, RICO_LASER3_SFX };
//D:80035E94
struct sfx3 knife_throw_sounds = { KNIFE_THROW1_SFX, KNIFE_THROW2_SFX, KNIFE_THROW3_SFX };
//D:80035E9C
struct gun_trigger_state g_ZeroTriggerState = { 0, 0 };
//D:80035EA0
//u32 D_80035EA0 = 0;
//D:80035EA4
coord3d D_80035EA4 = {0.0f, 0.0f, 0.0f};
//D:80035EB0
ModelRenderData g_DefaultCasingModelRenderData = {NULL, TRUE, 0x00000003};

//D:80035EF0
#define AMMO_RELATED_MAX 30
AmmoStats ammo_related[AMMO_RELATED_MAX] = {
    { 0x0    , 0x00000000,   0.0f, },
    { 0x320  , 0x02000C84,   0.0f, },
    { 0xC8   , 0x00000000,   0.0f, },
    { 0x190  , 0x02000C90,  -2.0f, },
    { 0x64   , 0x02000C9C,   0.0f, },
    { 0xC    , 0x02000CD8,   0.0f, },
    { 0x3    , 0x02000CC0,  -2.0f, },
    { 0xA    , 0x02000CFC,   1.0f, },
    { 0xA    , 0x02000D14,   1.0f, },
    { 0xA    , 0x02000D08,   1.0f, },
    { 0xA    , 0x02000CA8,   0.0f, },
    { 0xC    , 0x02000CB4,   0.0f, },
    { 0xC8   , 0x02000CE4,   0.0f, },
    { 0x64   , 0x02000CF0,   0.0f, },
    { 0x32   , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x2    , 0x00000000,   0.0f, },
    { 0x8    , 0x00000000,   0.0f, },
    { 0x6    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x1    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x3E8  , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x32   , 0x02000D20,  -1.0f, },
    { 0x1    , 0x00000000,   0.0f, },
};

//was previously attached to ammo_related[] (array at D:80035EF0)
//D:80036058
u16 D_80036058[] = { 0, 0, 0, 0, };

extern struct ModelSkeleton skeleton_gun_kf7;

typedef struct ModelHeader {
            s16                unk00;
            s16                Type;
            struct ChrRecord  *chr;
            ModelFileHeader   *obj;
            RenderPosView     *render_pos;
            union ModelRwData **datas;
            f32               scale;
            struct Model     *attachedto;
            ModelNode        *attachedto_objinst;
} ModelHeader;


// forward declarations

void gunGetShotDir(coord3d* arg0, coord3d* arg1, enum GUNHAND arg2);
void gunLaunchProjectileFromPlayer(ObjectRecord *obj, coord3d *targetpos, Mtxf *arg2, coord3d *velocity, Mtxf *arg4);
s32 gunSample1PTransform(Weapon1PTransformKeyframe *keyframes, f32 time, Mtxf *matrix, GUNHAND hand);
void gunAnalyzeGEKey(void);
void gunOpenWeaponCase(void);
struct ModelFileHeader * get_ptr_weapon_model_header_line(ITEM_IDS weapon);
s32 gunGetHeldAmmo(enum GUNHAND hand);
s32 weaponGetAmmoType(ITEM_IDS weapon);
f32 gunSetHorizontalOffset(GUNHAND hand);
void gunOpenWeaponCase(void);
void gunSetWatchMenuWeapon(GUNHAND hand, ITEM_IDS weaponnum_watchmenu);
void gunStartBoltRecoil(GUNHAND hand);
void gunReload(enum GUNHAND hand);
void gunCreateBeamForHand(enum GUNHAND hand);
CasingRecord* casingCreate(ModelFileHeader* header, Mtxf* mtx);
void gunEjectCasings(GUNHAND handnum, f32 floor_y_pos);
Vtx *dynAllocateVertices(s32 count);
Mtx *dynAllocateMatrix(void);
void camViewDeltaToScreen(coord3d *in, f32 divisor, coord3d *out);

// end forward declarations

// current debug keyframes
#define DEB_KEYFRAMES sniperMeleeKeyframes2


void gunDebSetApplyKeyframe(u32 uParm1)
{
    g_GunDebApplyKeyframe = uParm1;
}


u32 gunDebGetApplyKeyframe(void)
{
    return g_GunDebApplyKeyframe;
}


// Unreferenced debPrintKeyframe
// Address: ~ 7F05C538
void nullsub_73(void)
{
#ifdef DEBUG
    osSyncPrintf("\t{");
    osSyncPrintf("0");
    osSyncPrintf(",{%ff,%ff,%ff}", DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos.x, DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos.y, DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos.z);
    osSyncPrintf(",{%ff,%ff,%ff}", DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot.x, DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot.y, DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot.z);
    osSyncPrintf(",0.5f,20.0f");
    osSyncPrintf("},\n");
#endif
    return;
}


// Unreferenced - force keyframe to position
// Address: 7F05C540
void gunDebMoveKeyframe(coord3d* pos)
{
    Weapon1PTransformKeyframe* temp_v0;

    temp_v0 = &DEB_KEYFRAMES[g_gunDebKeyframeIndex];
    temp_v0->pos.x += pos->x;
    temp_v0->pos.y += pos->y;
    temp_v0->pos.z += pos->z;
}


// Unreferenced
void gunDebRotateKeyframe(Mtxf* mtxf)
{
    Mtxf sp18;
    mtx4LoadRotation(&DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot, &sp18);
    mtx4MultInPlace(mtxf, &sp18);
    mtx4GetRotation(&sp18, &DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot);
}


void gunDebApplyKeyframe(void)
{
    if (!g_GunDebApplyKeyframe) { return; }

    g_CurrentPlayer->hands[0].field_92C = 1;
    mtx4LoadRotation(&DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot, (Mtxf *)&g_CurrentPlayer->hands[0].field_8EC);
    mtx4SetTranslation(&DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos, (Mtxf *)&g_CurrentPlayer->hands[0].field_8EC);
    g_GunDebApplyKeyframe = 0;
}


// Unreferenced increment keyframe index, loop back to start if final keyframe is reached
// Address: 7F05C6B8
void gunDebAdvanceKeyframe(void)
{
    g_gunDebKeyframeIndex++;
    if (DEB_KEYFRAMES[g_gunDebKeyframeIndex].isFinalKey & 1)
    {
        g_gunDebKeyframeIndex = 0;
    }
}


/**
 * Address: 7F05C6FC
 * 
 * Sample a first person weapon transform keyframe animation at `time` and
 * write the interpolated transform into `matrix`
 * 
 * @returns 1 when an in-progress frame was interpolated, 0 when `time` reached
 * the final keyframe and the static end pose was written.
 */
s32 gunSample1PTransform(Weapon1PTransformKeyframe *keyframes, f32 time, Mtxf *matrix, GUNHAND hand)
{
    Weapon1PTransformKeyframe *current;
    f32 frac;
    f32 tangent;
    coord3d posResult;
    quatf qResult;
    quatf q0;
    quatf q1;
    quatf q2;
    quatf q3;
    s32 i;

    i = 1;
    frac = keyframes[1].duration;
    current = keyframes + i;

    while (time >= current->duration)
    {
        time -= current->duration;
        i++;
        current++;

        if (current[2].isFinalKey & 1)
        {
            break;
        }
    }

     // Comma operator is intentional: nudges IDO to emit addu s0,a0,t8.
    current = (0, keyframes) + i;

    i = 2;

    if (current[i].isFinalKey & 1)
    {
        mtx4LoadRotation(&current->rot, matrix);
        mtx4SetTranslation(&current->pos, matrix);
        return 0;
    }

    frac = time / current->duration;
    tangent = current->interpParam;

    quatFromAngles(&current[-1].rot, &q0);
    quatFromAngles(&current->rot, &q1);
    quatFromAngles(&current[1].rot, &q2);
    quatFromAngles(&current[2].rot, &q3);

    quatMakeShortestPath(&q1, &q2);
    quatMakeShortestPath(&q2, &q3);
    quatMakeShortestPath(&q1, &q0);

    quatSpline(&q0, &q1, &q2, &q3, frac, &qResult);

    interpCardinal3(&current[-1].pos, &current->pos, &current[1].pos, &current[2].pos, frac, tangent, &posResult);

    if (hand == GUNLEFT)
    {
        posResult.x = -posResult.x;
        qResult[0] = -qResult[0];
        qResult[1] = -qResult[1];
    }

    quatToMtx4(&qResult, matrix);
    mtx4SetTranslation(&posResult, matrix);

    return 1;
}


WeaponStats *weaponGetStats(ITEM_IDS item)
{
    if (gitem_structs[item].has_no_model == 0)
    { /* weapon has model, return stats struct */
        return gitem_structs[item].item_weapon_stats;
    }
    return &default_weaponstats; /* no model, return defaults */
}




void gunDebGetHandPos(coord3d *pos)
{
    ITEM_IDS item;
    WeaponStats *stats;

    item = gunGetWeapon(0);
    stats = weaponGetStats(item);

    pos->x = stats->PosX;
    pos->y = stats->PosY;
    pos->z = stats->PosZ;
}


void gunDebSetHandPos(coord3d *pos) {

    WeaponStats *stats;
    ITEM_IDS item;

    item = gunGetWeapon(0);
    stats = weaponGetStats(item);

    stats->PosX = pos->x;
    stats->PosY = pos->y;
    stats->PosZ = pos->z;
}


void gunCalcBlend(enum GUNHAND handnum)
{
    s32 sp60[2];
    s32 sp58[2];
    f32 mult = weaponGetStats(gunGetWeapon(handnum))->Sway;

    sp60[handnum] = (g_CurrentPlayer->hands[handnum].curblendpos + 2) % 4;
    sp58[handnum] = (g_CurrentPlayer->hands[handnum].curblendpos + 1) % 4;
    g_CurrentPlayer->hands[handnum].curblendpos = sp58[handnum];

    g_CurrentPlayer->hands[handnum].blendlook[sp60[handnum]].x = (RANDOMFRAC() - 0.5f) * 0.08f * mult;
    g_CurrentPlayer->hands[handnum].blendlook[sp60[handnum]].y = (RANDOMFRAC() - 0.5f) * 0.1f * mult;
    g_CurrentPlayer->hands[handnum].blendlook[sp60[handnum]].z = -1;

    g_CurrentPlayer->hands[handnum].blendup[sp60[handnum]].x = (RANDOMFRAC() - 0.5f) * 0.1f * mult;
    g_CurrentPlayer->hands[handnum].blendup[sp60[handnum]].y = 1;
    g_CurrentPlayer->hands[handnum].blendup[sp60[handnum]].z = (RANDOMFRAC() - 0.5f) * 0.1f * mult;

    g_CurrentPlayer->hands[handnum].blendpos[sp60[handnum]].x = (RANDOMFRAC() * 0.75f) + 1.5f;
    g_CurrentPlayer->hands[handnum].blendpos[sp60[handnum]].y = (2 + RANDOMFRAC()) * g_CurrentPlayer->hands[handnum].blendscale1;
    g_CurrentPlayer->hands[handnum].blendpos[sp60[handnum]].z = (RANDOMFRAC() - 0.5f) * 2.5f;

    if (g_CurrentPlayer->hands[handnum].sideflag < 0)
    {
        g_CurrentPlayer->hands[handnum].blendpos[sp60[handnum]].x *= -1;

        if (g_CurrentPlayer->hands[handnum].sideflag == -2)
        {
            g_CurrentPlayer->hands[handnum].sideflag = 1;
        }
        else
        {
            g_CurrentPlayer->hands[handnum].sideflag = -2;
        }
    }
    else
    {
        if (g_CurrentPlayer->hands[handnum].sideflag == 2)
        {
            g_CurrentPlayer->hands[handnum].sideflag = -1;
        }
        else
        {
            g_CurrentPlayer->hands[handnum].sideflag = 2;
        }
    }

    g_CurrentPlayer->hands[handnum].blendscale1 = -g_CurrentPlayer->hands[handnum].blendscale1;
}


s32 gunHandModelIsEmpty(enum GUNHAND arg0)
{
    return g_CurrentPlayer->hand_invisible[arg0] > 0
        || (g_CurrentPlayer->hand_item[arg0] == 0 && g_CurrentPlayer->field_2A44[arg0] < 0);
}


s32 gunGetHandModelItem(GUNHAND hand)
{
    return g_CurrentPlayer->hand_item[hand];
}


ModelFileHeader *gunGetHandModelHeader(GUNHAND hand)
{
    return &g_CurrentPlayer->copy_of_body_obj_header[hand];
}


u8 * gunGetHandModelBuffer(GUNHAND hand)
{
    return g_CurrentPlayer->ptr_hand_weapon_buffer[hand];
}


u32 gunGetHandModelBufferSize(s32 hand)
{
    return size_item_buffer[hand];
}


void gunClearHandModel(GUNHAND hand)
{
  g_CurrentPlayer->hand_invisible[hand] = 0;
  g_CurrentPlayer->hand_item[hand] = ITEM_UNARMED;
  g_CurrentPlayer->field_2A44[hand] = -1;
  g_CurrentPlayer->lock_hand_model[hand] = 1;
  return;
}


void gunSetHandModel(GUNHAND hand, ITEM_IDS item)
{
    if (g_CurrentPlayer->lock_hand_model[hand]) { return; }

    if (g_CurrentPlayer->hand_invisible[hand] >= 0)
    {
        if (item != g_CurrentPlayer->hand_item[hand])
        {
            g_CurrentPlayer->hand_invisible[hand] = -1;
            g_CurrentPlayer->field_2A44[hand] = item;
        }
        return;
    }

    if (item != g_CurrentPlayer->hand_item[hand])
    {
        g_CurrentPlayer->field_2A44[hand] = item;
        return;
    }

    g_CurrentPlayer->hand_invisible[hand] = 1;
}


char *gunGetModelFileName(ITEM_IDS item)
{
    if (item == ITEM_FIST)
    {
        item = g_CurrentPlayer->cur_item_weapon_getname;
    }
    return gitem_structs[item].item_file_name;
}


 ModelFileHeader *get_ptr_weapon_model_header_line(ITEM_IDS weapon)
{
    if (weapon == ITEM_FIST)
    {
        weapon = g_CurrentPlayer->cur_item_weapon_getname;
    }
    return gitem_structs[weapon].item_header;
}


int gunGetFistModelItem(void)
{
    return g_CurrentPlayer->cur_item_weapon_getname;
}


void gunLoadHandModel(GUNHAND hand)
{
    u32              size_buffer_weapon;
    s8              *ptr_item_text;
    ModelFileHeader *ptr_weapon_model;
    u8              *buffer_weapon;
    enum ITEM_IDS    item;

    if ((g_CurrentPlayer->hand_invisible[hand] < 0) && (g_CurrentPlayer->lock_hand_model[hand] == 0))
    {
        if ((g_CurrentPlayer->hand_invisible[hand] < -2) || (g_CurrentPlayer->hand_item[hand] == ITEM_UNARMED))
        {
            item             = g_CurrentPlayer->field_2A44[hand];
            ptr_item_text    = (s8 *)gunGetModelFileName(item);
            ptr_weapon_model = get_ptr_weapon_model_header_line(item);

            if ((ptr_item_text != NULL) && (ptr_weapon_model != NULL))
            {
                buffer_weapon      = gunGetHandModelBuffer(hand);
                size_buffer_weapon = gunGetHandModelBufferSize(hand);

                g_CurrentPlayer->copy_of_body_obj_header[hand] = *ptr_weapon_model;

                if (item == ITEM_SUIT_LF_HAND)
                {
                    texInitPool(&g_CurrentPlayer->item_related[hand], buffer_weapon + 0xBD70, size_buffer_weapon + 0xFFFF4290);
                    modelFileLoad(&g_CurrentPlayer->copy_of_body_obj_header[hand], (u8 *)ptr_item_text, buffer_weapon, 0xBD70, &g_CurrentPlayer->item_related[hand]);
                }
                else if ((item == ITEM_TRIGGER) || (item == ITEM_WATCHLASER))
                {
                    texInitPool(&g_CurrentPlayer->item_related[hand], buffer_weapon + 0xAFD0, size_buffer_weapon + 0xFFFF5030);
                    modelFileLoad(&g_CurrentPlayer->copy_of_body_obj_header[hand], (u8 *)ptr_item_text, buffer_weapon, 0xAFD0, &g_CurrentPlayer->item_related[hand]);
                }
                else
                {
                    texInitPool(&g_CurrentPlayer->item_related[hand], &buffer_weapon[D_80032464[hand]], size_buffer_weapon - D_80032464[hand]);
                    modelFileLoad(&g_CurrentPlayer->copy_of_body_obj_header[hand], (u8 *)ptr_item_text, buffer_weapon, D_80032464[hand], &g_CurrentPlayer->item_related[hand]);
                }
            }

            g_CurrentPlayer->hand_invisible[hand] = 1;
            g_CurrentPlayer->hand_item[hand]      = item;
            g_CurrentPlayer->field_2A44[hand]     = -1;
        }
        else
        {
            g_CurrentPlayer->hand_invisible[hand]--;
        }
    }
}


// Called by unused functions.
ITEM_IDS invStepItem(ITEM_IDS item, s32 arg1)
{
    while (arg1 > 0)
    {
        do
        {
            item = (item + 1) % ITEM_BOMBCASE;
        } while (invItemAvailable(item) == 0);
        arg1--;
    }

    while (arg1 < 0)
    {
        do
        {
            item--;
            if (item < 0)
            {
                item = 0x20 - (-(item + 1) % ITEM_BOMBCASE);
            }
        } while (invItemAvailable(item) == 0);
        arg1++;
    }

    return item;
}


ITEM_IDS gunGetNextWeapon(GUNHAND hand, s32 direction)
{
	if (g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_LOWER) 
    {
		if (
			(direction < 0 && (g_CurrentPlayer->hands[hand].field_8B8 > 0)) ||
			(direction > 0 && (g_CurrentPlayer->hands[hand].field_8B8 < 0)) ) 
        {
			return gunGetWeapon(hand);
		}
		else 
        {
			return g_CurrentPlayer->hands[hand].weapon_next_weapon;
		}

    }
    
    if (g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP) 
    {
        return g_CurrentPlayer->hands[hand].weapon_next_weapon;
    }

    return gunGetWeapon(hand);
}


void gunRequestHandWeaponChange(enum GUNHAND hand, s32 nextWeapon, s32 cycleDirection)
{
    if ((g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_LOWER) || (g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP))
    {
        g_CurrentPlayer->hands[hand].field_8B0 = g_CurrentPlayer->hands[hand].field_890;

#ifdef VERSION_EU
        if (getPlayerCount() == 1) {
            g_CurrentPlayer->hands[hand].field_8B0 += 0xE;
        } else {
            g_CurrentPlayer->hands[hand].field_8B0 += 0xA;
        }
#else
        if (getPlayerCount() == 1) {
            g_CurrentPlayer->hands[hand].field_8B0 += 0x11;
        } else {
            g_CurrentPlayer->hands[hand].field_8B0 += 0xD;
        }
#endif
    }

    if (gunGetNextWeapon(hand, 0) != nextWeapon)
    {
        if ((g_CurrentPlayer->hands[hand].weapon_action_state != GUN_ANIM_STATE_SWITCH_LOWER) && (g_CurrentPlayer->hands[hand].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP))
        {
            g_CurrentPlayer->hands[hand].weapon_current_animation = 5;
        }

        g_CurrentPlayer->hands[hand].weapon_next_weapon = nextWeapon;
        g_CurrentPlayer->hands[hand].weapon_animation_trigger = 1;
        g_CurrentPlayer->hands[hand].field_8B8 = cycleDirection;
    }
}


// Unused
void gunSwitchToNext(GUNHAND hand)
{
    gunRequestHandWeaponChange(hand, invStepItem(gunGetNextWeapon(hand, 0), 1), 0);
}


// Unused
void gunSwitchToPrev(GUNHAND hand)
{
    gunRequestHandWeaponChange(hand, invStepItem(gunGetNextWeapon(hand, 0), -1), 0);
}


void gunEquipPreviousWeapons(void)
{
    gunEquip(GUNRIGHT, g_CurrentPlayer->hands[GUNRIGHT].previous_weapon);
    gunEquip(GUNLEFT, g_CurrentPlayer->hands[GUNLEFT].previous_weapon);
}


void gunCycleForward(void)
{
    ITEM_IDS nextright;
    ITEM_IDS nextleft;

    nextright = gunGetNextWeapon(GUNRIGHT, 1);
    nextleft = gunGetNextWeapon(GUNLEFT, 1);

    if ((nextright >= ITEM_BOMBCASE) || (nextleft >= ITEM_BOMBCASE))
    {
        nextright = g_CurrentPlayer->hands[GUNRIGHT].previous_weapon;
        nextleft = g_CurrentPlayer->hands[GUNLEFT].previous_weapon;
    }
    else
    {
        invCycleForward(&nextright, &nextleft, FALSE);
    }

    gunRequestHandWeaponChange(GUNRIGHT, nextright, 1);
    gunRequestHandWeaponChange(GUNLEFT, nextleft, 1);
}


void gunCycleBackward(void)
{
    ITEM_IDS nextright;
    ITEM_IDS nextleft;

    nextright = gunGetNextWeapon(GUNRIGHT, -1);
    nextleft = gunGetNextWeapon(GUNLEFT, -1);

    if ((nextright >= ITEM_BOMBCASE) || (nextleft >= ITEM_BOMBCASE))
    {
        nextright = g_CurrentPlayer->hands[GUNRIGHT].previous_weapon;
        nextleft = g_CurrentPlayer->hands[GUNLEFT].previous_weapon;
    }
    else
    {
        invCycleBackward(&nextright, &nextleft, FALSE);
    }

    gunRequestHandWeaponChange(GUNRIGHT, nextright, -1);
    gunRequestHandWeaponChange(GUNLEFT, nextleft, -1);
}

void gunSwitchWhenEmpty(void)
{
	ITEM_IDS nextright;
	ITEM_IDS nextleft;
	ITEM_IDS duperight;
	ITEM_IDS dupeleft;

    nextright = gunGetNextWeapon(GUNRIGHT, 1);
    duperight = nextright;

    nextleft = gunGetNextWeapon(GUNLEFT, 1);
    dupeleft = nextleft;

    if ((duperight >= ITEM_BOMBCASE) || (dupeleft >= ITEM_BOMBCASE))
    {
        duperight = g_CurrentPlayer->hands[GUNRIGHT].previous_weapon;
        dupeleft = g_CurrentPlayer->hands[GUNLEFT].previous_weapon;
    }
    else if ((duperight == ITEM_REMOTEMINE) && ((invItemAvailable(ITEM_TRIGGER))))
    {
        duperight = ITEM_TRIGGER;
        dupeleft = ITEM_UNARMED;
    }
    else
    {
        invCycleForward(&duperight, &dupeleft, TRUE);

        if ((duperight < nextright) || ((duperight == nextright) && (nextleft >= dupeleft)))
        {
			duperight = nextright;
			dupeleft = nextleft;
			invCycleBackward(&duperight, &dupeleft, TRUE);
        }
    }

    gunRequestHandWeaponChange(GUNRIGHT, duperight, 1);
    gunRequestHandWeaponChange(GUNLEFT, dupeleft, 1);
}

s32 gunEquip(GUNHAND hand, s32 next_weapon) {
    g_CurrentPlayer->hands[hand].weapon_current_animation = 5;
    g_CurrentPlayer->hands[hand].weapon_next_weapon = next_weapon;
    g_CurrentPlayer->hands[hand].weapon_animation_trigger = 0;
}

void gunTryReload(GUNHAND hand) {
    s32 ammo_type = weaponGetAmmoType(gunGetWeapon(hand));
    if (ammo_type != 0) {
        if (g_CurrentPlayer->hands[hand].weapon_current_animation == 0) {
            g_CurrentPlayer->hands[hand].weapon_current_animation = 9;
        }
    }
}

ITEM_IDS gunGetWeapon(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weaponnum;
}

void gunStartWatchSwap(GUNHAND hand, s32 next_weapon) {
	g_CurrentPlayer->hands[hand].weapon_current_animation = 0xE;
	g_CurrentPlayer->hands[hand].weapon_next_weapon = next_weapon;
}

ITEM_IDS gunGetShownWeapon(GUNHAND hand) {
	if (g_CurrentPlayer->hands[hand].weaponnum_watchmenu >= 0) {
		return g_CurrentPlayer->hands[hand].weaponnum_watchmenu;
	} else {
		return g_CurrentPlayer->hands[hand].weaponnum;
	}
}

void gunSetWatchMenuWeapon(GUNHAND hand, ITEM_IDS weaponnum_watchmenu) {
    gunSetHandModel(hand, weaponnum_watchmenu);
	g_CurrentPlayer->hands[hand].weaponnum_watchmenu = weaponnum_watchmenu;
}

void gunClearWatchMenuWeapon(GUNHAND hand) {
    if (g_CurrentPlayer->hands[hand].weaponnum_watchmenu >= 0) {
        gunSetHandModel(hand, g_CurrentPlayer->hands[hand].weaponnum);
		g_CurrentPlayer->hands[hand].weaponnum_watchmenu = -1;
    }
}


void gunUnequip(enum GUNHAND hand, enum ITEM_IDS weapid)
{
    enum ITEM_IDS weapon_num;
    s32 ammo_type;

    weapon_num = g_CurrentPlayer->hands[hand].weaponnum;
    ammo_type = weaponGetAmmoType(weapon_num);

    if (g_CurrentPlayer->hands[hand].weaponnum_watchmenu < 0)
    {
        gunSetHandModel(hand, weapid);
    }

    if (g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine > 0)
    {
        g_CurrentPlayer->ammoheldarr[ammo_type] += g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine;
    }

    if (weapon_num < ITEM_BOMBCASE)
    {
        g_CurrentPlayer->hands[hand].previous_weapon = weapon_num;
    }

    if (getPlayerCount() >= 2)
    {
        playerBodyRemoveGun(hand);
    }

    gunFreeRocket(hand);
    g_CurrentPlayer->hands[hand].weaponnum = weapid;
    g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine = 0;
    g_CurrentPlayer->hands[hand].field_A4C = 0;
    g_CurrentPlayer->hands[hand].field_A50 = 0;
    invDetermineEquippedItem();
}


s8 gunGetFiringStatus(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weapon_firing_status;
}

f32 gunGetDualOffset(GUNHAND hand) {
	return g_CurrentPlayer->hands[hand].field_A34;
}

/**
 * Positions the gun to the right or left side of the screen depending on which hand is holding it.
 */
f32 gunSetHorizontalOffset(GUNHAND hand)
{
	f32 offset;

	if (hand == GUNRIGHT)
	{
		offset = weaponGetStats(gunGetShownWeapon(GUNRIGHT))->PosX;
	}
	else
	{
		offset = -weaponGetStats(gunGetShownWeapon(GUNLEFT))->PosX;
	}

	return offset;
}

f32 gunGetZoomFov(void) {
    if (gunGetShownWeapon(GUNRIGHT) == ITEM_SNIPERRIFLE) {
        return g_CurrentPlayer->sniper_zoom;
    }
    if (gunGetShownWeapon(GUNRIGHT) == ITEM_CAMERA) {
        return g_CurrentPlayer->camera_zoom;
    }
    return weaponGetStats(gunGetShownWeapon(GUNRIGHT))->Zoom;
}

void gunZoomOut(f32 zoom)
{
	if (gunGetShownWeapon(GUNRIGHT) == ITEM_SNIPERRIFLE) {
		g_CurrentPlayer->sniper_zoom *= (1.0f + (zoom * 0.1f));
		if (g_CurrentPlayer->sniper_zoom > 60.0f) {
			g_CurrentPlayer->sniper_zoom = 60.0f;
		}
	}
	else
	{
		if (gunGetShownWeapon(GUNRIGHT) == ITEM_CAMERA) {
			g_CurrentPlayer->camera_zoom *= (1.0f + (zoom * 0.1f));
			if (g_CurrentPlayer->camera_zoom > 60.0f) {
				g_CurrentPlayer->camera_zoom = 60.0f;
			}
		}
	}
}

void gunZoomIn(f32 zoom)
{
	if (gunGetShownWeapon(GUNRIGHT) == ITEM_SNIPERRIFLE) {
		g_CurrentPlayer->sniper_zoom /= (1.0f + (zoom * 0.1f));
		if (g_CurrentPlayer->sniper_zoom < 7.0f) {
			g_CurrentPlayer->sniper_zoom = 7.0f;
		}
	}
	else
	{
		if (gunGetShownWeapon(GUNRIGHT) == ITEM_CAMERA) {
			g_CurrentPlayer->camera_zoom /= (1.0f + (zoom * 0.1f));
			if (g_CurrentPlayer->camera_zoom < 7.0f) {
				g_CurrentPlayer->camera_zoom = 7.0f;
			}
		}
	}
}

f32 weaponGetDamage(ITEM_IDS item)
{
  return weaponGetStats(item)->DestructionAmount;
}


f32 weaponGetImpactForce(ITEM_IDS item)
{
	return weaponGetStats(item)->ForceOfImpact;
}

/**
 * Address 0x7F05DFCC
 */
s8 weaponGetFireRate(ITEM_IDS item) {
    return weaponGetStats(item)->AutomaticFiringRate;
}


u8 weaponGetSoundRate(ITEM_IDS item) {
    return weaponGetStats(item)->SoundTriggerRate;
}


u16 weaponGetSound(ITEM_IDS item)
{
  return weaponGetStats(item)->Sound;
}


u8 weaponGetPenetration(ITEM_IDS item)
{
  return weaponGetStats(item)->ObjectsShootThrough;
}


s32 weaponHasAmmo(ITEM_IDS item)
{
    if (weaponHasFlag(item, WEAPONSTATBITFLAG_HAS_AMMO) != 0)
    {
        if ((weaponGetAmmoType(item) == 0) || (weaponGetHeldAmmo(item) > 0))
        {
            return 1;
        }
    }
    return 0;
}


u32 weaponHasFlag(ITEM_IDS item, u32 mask)
{
  return ((weaponGetStats(item)->BitFlags & mask) != 0);
}


void gunUpdateSway(f32 breathing, f32 arg1, f32 arg2, f32 arg3)
{
    f32 dampt[2];
    s32 i;
    u32 unused[2];
    f32 sp50 = arg2;
    f32 sp4c;
    u32 stack2;
    f32 minbreathing;

    if (sp50 < 0.0f) { sp50 = -sp50; }

    if (arg1 > 0.8f)
    {
        g_CurrentPlayer->gunposamplitude = 1.0f;
    }
    else
    {
        if (arg1 > 0.1f)
        {
            f32 tmp = (1.0f - cosf((arg1 - 0.1f) * M_TAU_F / 2.8f));
            g_CurrentPlayer->gunposamplitude = 0.8f * tmp + 0.2f;
        }
        else
        {
            g_CurrentPlayer->gunposamplitude = 0.1f;
        }
    }

    if (g_CurrentPlayer->gunposamplitude < (playerGetBreathing() * 0.3f))
    {
        g_CurrentPlayer->gunposamplitude = playerGetBreathing() * 0.3f;
    }

    if (g_CurrentPlayer->gunposamplitude < 0.5f * sp50)
    {
        g_CurrentPlayer->gunposamplitude = 0.5f * sp50;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->field_1080 = (g_CurrentPlayer->field_1080 * (PAL ? 0.9403f : 0.95f)) + g_CurrentPlayer->gunposamplitude;
    }

    g_CurrentPlayer->gunposamplitude = g_CurrentPlayer->field_1080 * (PAL ? 0.059700012f : 0.050000012f);

    minbreathing = 0.016666668f * sp50;
    if (breathing < minbreathing)
    {
        breathing = minbreathing;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->field_107C = (g_CurrentPlayer->field_107C * (PAL ? 0.9403f : 0.95f)) + breathing;
    }

    breathing = g_CurrentPlayer->field_107C * (PAL ? 0.059700012f : 0.050000012f);

    sp4c = breathing * g_GlobalTimerDelta;
    dampt[0] = g_CurrentPlayer->hands[0].dampt + sp4c;

    while (dampt[0] >= 1.0f)
    {
        gunCalcBlend(GUNRIGHT);
        dampt[0] -= 1.0f;
        g_CurrentPlayer->syncoffset++;
    }

    g_CurrentPlayer->synccount += g_GlobalTimerDelta;

    if (g_CurrentPlayer->synccount > 60.0f)
    {
        g_CurrentPlayer->synccount = 0.0f;
        g_CurrentPlayer->syncchange = (RANDOMFRAC() - 0.5f) * 0.2f / 60.0f;
    }

    if (g_CurrentPlayer->syncchange + sp4c > 0.0f)
    {
        g_CurrentPlayer->gunsync += g_CurrentPlayer->syncchange;
    }

    if (g_CurrentPlayer->gunsync > 0.5f)
    {
        g_CurrentPlayer->gunsync = 0.5f;
    }
    else if (g_CurrentPlayer->gunsync < -0.5f)
    {
        g_CurrentPlayer->gunsync = -0.5f;
    }
    else if (g_CurrentPlayer->gunsync < 0.1f && g_CurrentPlayer->gunsync > -0.1f)
    {
        if (g_CurrentPlayer->gunsync > 0.0f)
        {
            g_CurrentPlayer->gunsync = -0.1f;
        }
        else
        {
            g_CurrentPlayer->gunsync = 0.1f;
        }
    }

    dampt[1] = dampt[0] + g_CurrentPlayer->syncoffset + g_CurrentPlayer->gunsync;

    while (dampt[1] >= 1.0f)
    {
        gunCalcBlend(GUNLEFT);
        dampt[1] -= 1.0f;
        g_CurrentPlayer->syncoffset--;
    }

    for (i = 0; i < 2; i++)
    {
        g_CurrentPlayer->hands[i].dampt = dampt[i];
        g_CurrentPlayer->hands[i].weapon_theta_displacement = -1.75f * arg3;
        g_CurrentPlayer->hands[i].weapon_verta_displacement = -2.0f * arg2;
    }
}


void gunSetPullback(f32 param_1)
{
    g_CurrentPlayer->hands[GUNRIGHT].gunofs2_z = (1.0f - cosf(param_1)) * 5.0f;
    g_CurrentPlayer->hands[GUNLEFT].gunofs2_z = (1.0f - cosf(param_1)) * 5.0f;
}


f32 gunGetMaxHingeAngle(GUNHAND hand)
{
  if ((gunGetWeapon(hand) == ITEM_TRIGGER) || (gunGetWeapon(hand) == ITEM_WATCHLASER))
  {
    return 0.08726647f;
  }
  else
  {
    return 0.17453294f;
  }
}


void gunTickHingeAngle(enum GUNHAND hand, s32 arg1)
{
    if (arg1 != 0)
    {
        if (g_CurrentPlayer->hands[hand].field_A84 < gunGetMaxHingeAngle(hand))
        {
            g_CurrentPlayer->hands[hand].field_A84 += (0.029088823f * g_GlobalTimerDelta);
        }
        if (g_CurrentPlayer->hands[hand].field_A84 > gunGetMaxHingeAngle(hand)) {
            g_CurrentPlayer->hands[hand].field_A84 = gunGetMaxHingeAngle(hand);
        }
    }
    else
    {
        if (g_CurrentPlayer->hands[hand].field_A84 > 0.0f)
        {
            g_CurrentPlayer->hands[hand].field_A84 -= (0.017453294f * g_GlobalTimerDelta);
        }
        if (g_CurrentPlayer->hands[hand].field_A84 < 0.0f)
        {
            g_CurrentPlayer->hands[hand].field_A84 = 0.0f;
        }
    }
}


void gunStartBoltRecoil(GUNHAND hand) {
	g_CurrentPlayer->hands[hand].field_A8C = 1;
}


void gunTickBoltRecoil(GUNHAND hand)
{
    f32 recoil_back;

    recoil_back = weaponGetStats(gunGetShownWeapon(hand))->BoltRecoilBack;

    if (g_CurrentPlayer->hands[hand].field_A8C != 0)
    {
        if (g_CurrentPlayer->hands[hand].field_A88 < recoil_back)
        {
            g_CurrentPlayer->hands[hand].field_A88 = (g_CurrentPlayer->hands[hand].field_A88 + (recoil_back * 0.25f * g_GlobalTimerDelta));

        }
        if (recoil_back <= g_CurrentPlayer->hands[hand].field_A88) {
            g_CurrentPlayer->hands[hand].field_A88 = recoil_back;
            g_CurrentPlayer->hands[hand].field_A8C = 0;
        }
    }
    else if (g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine > 0)
    {
        if (g_CurrentPlayer->hands[hand].field_A88 > 0.0f)
        {
            g_CurrentPlayer->hands[hand].field_A88 = (g_CurrentPlayer->hands[hand].field_A88 - (recoil_back * 0.16666667f * g_GlobalTimerDelta));

        }
        if (g_CurrentPlayer->hands[hand].field_A88 < 0.0f)
        {
            g_CurrentPlayer->hands[hand].field_A88 = 0.0f;
        }
    }
}


void sub_GAME_7F05E978(Model* model, s32 val)
{
    if (model->obj->Switches[8] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[8])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[9] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[9])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[10] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[10])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[11] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[11])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[12] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[12])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[13] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[13])->DisplayList.unk00 = val;
    }

    if (model->obj->numSwitches >= 0x24)
    {
        if (model->obj->Switches[35] != NULL)
        {
            modelGetNodeRwData(model, model->obj->Switches[35])->DisplayList.unk00 = val;
        }
    }
}


void sub_GAME_7F05EA94(Model* model, s32 val)
{
    ModelNode* switch_14;
    ModelNode* switch_15;

    if (model->obj->numSwitches >= 0x10)
    {
        switch_14 = model->obj->Switches[14];
        if (switch_14 != NULL)
        {
            // Guessing DisplayList here
            modelGetNodeRwData(model, switch_14)->DisplayList.unk00 = val;
        }

        switch_15 = model->obj->Switches[15];
        if (switch_15 != NULL)
        {
            // Guessing DisplayList here
            modelGetNodeRwData(model, switch_15)->DisplayList.unk00 = val;
        }
    }
}


/**
 * Address 0x7F05EB0C.
*/
void gunLaunchProjectile(ObjectRecord *obj, coord3d *pos, StandTile *stan, Mtxf *matrix, coord3d *velocity, Mtxf *arg5, PropRecord *owner)
{
    PropRecord *temp_s1;
    Projectile *temp_v0;

    temp_s1 = obj->prop;

    if (temp_s1 != NULL)
    {
        propActivate(temp_s1);
        propEnable(temp_s1);
        mtx4ScaleRows012(obj->model->scale, matrix);
        objChangeShading(obj, pos, matrix, stan);

        setupUpdateObjectRoomPosition(obj);

        chrobjCollisionRelated(obj);
        objEnsureProjectile(temp_s1);

        if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        {
            temp_v0 = obj->projectile;
            temp_v0->flags |= 0x41;
            obj->projectile->ownerprop = owner;
            projectileSetSticky(temp_s1);
            mtx4Copy(arg5, &obj->projectile->mtx);
            obj->projectile->speed.f[0] = velocity->f[0];
            obj->projectile->speed.f[1] = velocity->f[1];
            obj->projectile->speed.f[2] = velocity->f[2];
            obj->projectile->obj = obj;
            obj->projectile->unkE8 = g_LvTickCount;
        }
    }
}


/**
 * Address: 7F05EC1C
 *
 * Determines where the projectile may safely enter the world. Ideally that is the targetpos position, but if targetpos
 * is obstructed the player's position used as a fallback. This prevents the player from launching
 * projectiles through nearby surfaces.
 */
void gunLaunchProjectileFromPlayer(ObjectRecord *obj, coord3d *targetpos, Mtxf *arg2, coord3d *velocity, Mtxf *arg4)
{
    PropRecord *playerprop;
    coord3d pos;
    StandTile *tile;
    u32 pad_c[2];
    f32 yhi;
    f32 ylo;
    s32 usedfallback;
    f32 stanheight;
    u8 rooms[2];
    s32 pad_rooms;
    u8 pad_a[0x4c];
    s32 sp54;
    s32 sp50;
    s32 pad_sp;

    // fake
    if (obj->prop);
    if (obj->prop == NULL) {
        return;
    }

    playerprop = currentPlayerGetProp();
    stanheight = playerGetStanHeight(g_CurrentPlayer);

    usedfallback = 0;

    if (targetpos->y < playerprop->pos.y) {
        yhi = playerprop->pos.y - stanheight;
        ylo = targetpos->y - stanheight;
    }
    else
    {
        yhi = targetpos->y - stanheight;
        ylo = playerprop->pos.y - stanheight;
    }

    tile = playerprop->stan;
    playerSetMoving(playerprop, 0);

    // If there is no obstruction, spawn the projectile at the target position.
    if (stanTestLineUnobstructed(&tile, playerprop->pos.x, playerprop->pos.z, targetpos->x, targetpos->z, 0x1f, yhi, ylo, 0.0f, 1.0f))
    {
        pos.x = targetpos->x;
        pos.y = targetpos->y;
        pos.z = targetpos->z;
    }
    // Otherwise spawn it from the player's position.
    else
    {
        tile = playerprop->stan;
        pos.x = playerprop->pos.x;
        pos.y = playerprop->pos.y;
        pos.z = playerprop->pos.z;
        usedfallback = 1;
    }

    playerSetMoving(playerprop, 1);

    gunLaunchProjectile(obj, &pos, tile, arg2, velocity, arg4, playerprop);

    if (obj->runtime_bitflags & 0x80) {
        if (usedfallback) {
            obj->projectile->flags |= PROJECTILEFLAG_00000100;
            ((coord3d *)&obj->projectile->unkd4)->x = targetpos->x;
            ((coord3d *)&obj->projectile->unkd4)->y = targetpos->y;
            ((coord3d *)&obj->projectile->unkd4)->z = targetpos->z;
        }

        rooms[0] = playerGetRoom();
        rooms[1] = 0xff;

        bgFindRoomsAlongSegment(playerGetPos3(), &pos, rooms, obj->projectile->unkCC, &sp54, &sp50, 0x14);
    }
}


/**
 * Address 0x7F05EE24 (NTSC)
 * Address 0x7F05F2DC (PAL)
*/
void gunThrowGrenade(s32 hand)
{
    s32 padding;
    Mtxf spFC;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d spE0;
    Mtxf spA0_a;
    struct WeaponObjRecord *wor;
    s32 new_prop_type;
    s32 sp94; // sp148
    struct coord3d base_speed_vec; // sp136
    struct PropRecord* player_prop; // sp132
    struct coord3d *bondprevpos;  // sp128
    Mtxf sp40_f;
    ALSoundState *sfx_state;
    s32 current_weapon;
    s32 unused;

    wor = NULL;
    base_velocity = 16.666666f;

    player_prop = currentPlayerGetProp();
    bondprevpos = playerGetPrevPos();
    current_weapon = gunGetWeapon(hand);

    physRandomLaunch(&throw_speed_vec, &spFC);
    gunGetShotDir(&sp94, &base_speed_vec, hand);
    mtx4RotateVecInPlace(camGetViewToWorldMtxf(), (f32*)&base_speed_vec);

    throw_speed_vec.f[0] = (base_speed_vec.f[0] * base_velocity);
    throw_speed_vec.f[1] = (base_speed_vec.f[1] * base_velocity) + 5.0f;
    throw_speed_vec.f[2] = (base_speed_vec.f[2] * base_velocity);

    if (g_ClockTimer > 0)
    {
        throw_speed_vec.f[0] = ((player_prop->pos.f[0] - bondprevpos->f[0]) / g_GlobalTimerDelta) + throw_speed_vec.f[0];
        throw_speed_vec.f[1] = ((player_prop->pos.f[1] - bondprevpos->f[1]) / g_GlobalTimerDelta) + throw_speed_vec.f[1];
        throw_speed_vec.f[2] = ((player_prop->pos.f[2] - bondprevpos->f[2]) / g_GlobalTimerDelta) + throw_speed_vec.f[2];
    }

    spE0.f[0] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][0];
    spE0.f[1] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][1];
    spE0.f[2] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][2];

    mtx4LoadIdentity(&spA0_a);
    mtx4Copy(&g_CurrentPlayer->hands[hand].throw_item_pos_related, &sp40_f);
    sp40_f.m[3][0] = 0.0f;
    sp40_f.m[3][1] = 0.0f;
    sp40_f.m[3][2] = 0.0f;
    mtx4MultInPlace(&sp40_f, &spA0_a);

    wor = weaponCreateFromModel(PROP_CHRGRENADE, current_weapon);

    if (wor != NULL)
    {
        wor->timer = THROWN_ITEM_TIMER_DEFAULT - g_CurrentPlayer->last_z_trigger_timer;

        if (wor->timer < 0)
        {
            wor->timer = 0;
        }

        wor->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
        wor->runtime_bitflags |= playerGetCurrentNum() << RUNTIMEBITSHIFT_OWNER;

        gunLaunchProjectileFromPlayer(wor, &spE0, &spA0_a, &throw_speed_vec, &spFC);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->unk8C = 0.3f;
            wor->projectile->unk94 = 0.13333333f;
            wor->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            sfx_state = sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, GRENADE_THROW_SFX, NULL);

            if (sfx_state != NULL)
            {
                chrobjSndCreatePostEventDefault(sfx_state, (struct coord3d *) &wor->runtime_pos);
            }
        }
    }
}


/**
 * Address 0x7F05F09C (NTSC)
 * Address 0x7F05F554 (PAL)
*/
void gunThrowKnife(s32 hand)
{
    struct WeaponObjRecord *wor;
    Mtxf spFC;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d spE0;
    Mtxf spA0_a;
    s32 padding;
    s32 new_prop_type;
    s32 sp94;
    struct coord3d base_speed_vec;
    Mtxf sp40_f;
    struct PropRecord* player_prop;
    struct coord3d *bondprevpos;

    wor = NULL;
    base_velocity = 25.0f;

    player_prop = currentPlayerGetProp();
    bondprevpos = playerGetPrevPos();

    physRandomLaunch(&throw_speed_vec, &spFC);
    gunGetShotDir(&sp94, &base_speed_vec, hand);
    mtx4RotateVecInPlace(camGetViewToWorldMtxf(), (f32*)&base_speed_vec);

    throw_speed_vec.f[0] = (base_speed_vec.f[0] * base_velocity);
    throw_speed_vec.f[1] = (base_speed_vec.f[1] * base_velocity) + 5.0f;
    throw_speed_vec.f[2] = (base_speed_vec.f[2] * base_velocity);

    if (g_ClockTimer > 0)
    {
        throw_speed_vec.f[0] = ((player_prop->pos.f[0] - bondprevpos->f[0]) / g_GlobalTimerDelta) + throw_speed_vec.f[0];
        throw_speed_vec.f[1] = ((player_prop->pos.f[1] - bondprevpos->f[1]) / g_GlobalTimerDelta) + throw_speed_vec.f[1];
        throw_speed_vec.f[2] = ((player_prop->pos.f[2] - bondprevpos->f[2]) / g_GlobalTimerDelta) + throw_speed_vec.f[2];
    }

    spE0.f[0] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][0];
    spE0.f[1] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][1];
    spE0.f[2] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][2];

    mtx4LoadZRotation(4.712389f, &spA0_a);
    mtx4LoadXRotation(M_PI_F, &sp40_f);
    mtx4MultInPlace(&sp40_f, &spA0_a);
    mtx4Copy(&g_CurrentPlayer->hands[hand].throw_item_pos_related, &sp40_f);

    sp40_f.m[3][0] = 0.0f;
    sp40_f.m[3][1] = 0.0f;
    sp40_f.m[3][2] = 0.0f;
    mtx4MultInPlace(&sp40_f, &spA0_a);

    guRotateF(&spFC, 360.0f / ((randomGetNext() * (0.5f / (f32)INT_MAX)) + 12.1f), spA0_a.m[1][0], spA0_a.m[1][1], spA0_a.m[1][2]);

    wor = weaponCreateFromModel(PROP_CHRKNIFE, ITEM_THROWKNIFE);

    if (wor != NULL)
    {
        wor->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
        wor->runtime_bitflags |= playerGetCurrentNum() << RUNTIMEBITSHIFT_OWNER;

        gunLaunchProjectileFromPlayer(wor, &spE0, &spA0_a, &throw_speed_vec, &spFC);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->unk8C = 0.1f;
            wor->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            wor->runtime_bitflags |= RUNTIMEBITFLAG_THROWING_KNIFE_RELATED;
        }

        objUpdateThrowKnifeSound(wor);
    }
}





/**
 * Address 0x7F05F358 (NTSC)
 * Address 0x7F05F810 (PAL)
*/
void gunThrowObject(s32 hand)
{
/*
    else {
        assertPrint_8291E690(".\\ported\\gun.cpp",0x8df,"throwmineremote - Not a mine!");
    }
*/

    s32 padding;
    Mtxf unk_mtxf;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d spE0;
    Mtxf spA0_a;
    struct WeaponObjRecord *wor;
    s32 new_prop_type;
    s32 sp94; // sp148
    struct coord3d base_speed_vec; // sp136
    struct PropRecord* player_prop; // sp132
    struct coord3d *bondprevpos;  // sp128
    Mtxf sp40_f;
    ALSoundState *sfx_state;
    s32 current_weapon;
    s32 unused;

    wor = NULL;
    base_velocity = 16.666666f;

    player_prop = currentPlayerGetProp();
    bondprevpos = playerGetPrevPos();
    current_weapon = gunGetWeapon(hand);

    if (current_weapon == ITEM_GOLDENEYEKEY)
    {
        base_velocity = 6.6666665f;
    }

    physRandomLaunch(&throw_speed_vec, &unk_mtxf);
    gunGetShotDir(&sp94, &base_speed_vec, hand);
    mtx4RotateVecInPlace(camGetViewToWorldMtxf(), (f32*)&base_speed_vec);

    throw_speed_vec.f[0] = (base_speed_vec.f[0] * base_velocity);
    throw_speed_vec.f[1] = (base_speed_vec.f[1] * base_velocity) + 5.0f;
    throw_speed_vec.f[2] = (base_speed_vec.f[2] * base_velocity);

    if (g_ClockTimer > 0)
    {
        throw_speed_vec.f[0] = ((player_prop->pos.f[0] - bondprevpos->f[0]) / g_GlobalTimerDelta) + throw_speed_vec.f[0];
        throw_speed_vec.f[1] = ((player_prop->pos.f[1] - bondprevpos->f[1]) / g_GlobalTimerDelta) + throw_speed_vec.f[1];
        throw_speed_vec.f[2] = ((player_prop->pos.f[2] - bondprevpos->f[2]) / g_GlobalTimerDelta) + throw_speed_vec.f[2];
    }

    spE0.f[0] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][0];
    spE0.f[1] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][1];
    spE0.f[2] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][2];

    mtx4LoadIdentity(&spA0_a);
    mtx4Copy(&g_CurrentPlayer->hands[hand].throw_item_pos_related, &sp40_f);
    sp40_f.m[3][0] = 0.0f;
    sp40_f.m[3][1] = 0.0f;
    sp40_f.m[3][2] = 0.0f;
    mtx4MultInPlace(&sp40_f, &spA0_a);

    if (current_weapon == ITEM_GOLDENEYEKEY)
    {
        wor = invRemoveWeaponProp(current_weapon);
        invRemoveItemByID(current_weapon);

        if (wor != NULL)
        {
            objDetach(wor->prop);
        }

        gunEquipPreviousWeapons();
    }

    if (wor == NULL)
    {
        new_prop_type = PROP_CHRREMOTEMINE;

        switch (current_weapon)
        {
        case ITEM_PROXIMITYMINE:
            new_prop_type = PROP_CHRPROXIMITYMINE;
            break;
        case ITEM_TIMEDMINE:
            new_prop_type = PROP_CHRTIMEDMINE;
            break;
        case ITEM_BOMBCASE:
            new_prop_type = PROP_CHRBOMBCASE;
            break;
        case ITEM_BUG:
            new_prop_type = PROP_CHRBUG;
            break;
        case ITEM_MICROCAMERA:
            new_prop_type = PROP_CHRMICROCAMERA;
            break;
        case ITEM_GOLDENEYEKEY:
            new_prop_type = PROP_CHRGOLDENEYEKEY;
            break;
        case ITEM_PLASTIQUE:
            new_prop_type = PROP_CHRPLASTIQUE;
            break;
#ifdef DEBUG
        default:
            assertmsg2(current_weapon = PROP_CHRREMOTEMINE, "throwmineremote - Not a mine!");
#endif

        }

        wor = weaponCreateFromModel(new_prop_type, current_weapon);
    }

    if (wor != NULL)
    {
        switch (current_weapon)
        {
            case ITEM_REMOTEMINE:
            if (!isMpGame())
            {
                wor->timer = THROWN_ITEM_TIMER_SOLO;
            }
            else
            {
                wor->timer = THROWN_ITEM_TIMER_MULTI;
            }
            break;

            case ITEM_PROXIMITYMINE:
            if (!isMpGame())
            {
                wor->timer = THROWN_ITEM_TIMER_SOLO;
            }
            else
            {
                wor->timer = THROWN_ITEM_TIMER_MULTI;
            }
            break;

            case ITEM_TIMEDMINE:
            if (!isMpGame())
            {
                wor->timer = THROWN_ITEM_TIMER_SOLO;
            }
            else
            {
                wor->timer = THROWN_ITEM_TIMER_MULTI;
            }
            break;

            case ITEM_BOMBCASE:
            if (!isMpGame())
            {
                wor->timer = THROWN_ITEM_TIMER_SOLO;
            }
            else
            {
                wor->timer = THROWN_ITEM_TIMER_MULTI;
            }
            break;

            case ITEM_PLASTIQUE:
            case ITEM_BUG:
            case ITEM_MICROCAMERA:
            case ITEM_GOLDENEYEKEY:
                wor->timer = 1;
            break;

            default:
                wor->timer = THROWN_ITEM_TIMER_DEFAULT;
            break;
        }

        wor->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
        wor->runtime_bitflags |= playerGetCurrentNum() << RUNTIMEBITSHIFT_OWNER;

        gunLaunchProjectileFromPlayer(wor, &spE0, &spA0_a, &throw_speed_vec, &unk_mtxf);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->unk8C = 0.1f;
            wor->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            sfx_state = sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, GRENADE_THROW_SFX, NULL);

            if (sfx_state != NULL)
            {
                chrobjSndCreatePostEventDefault(sfx_state, (struct coord3d *) &wor->runtime_pos);
            }
        }
    }
}


/**
 * Address: 7F05F73C
 *
 * Spawns Grenade Launcher rounds and makes them inherit the player's momentum.
 */
void gunSpawnGLGrenade(s32 handnum)
{
    WeaponObjRecord *grenadeobj;
    struct hand *hand;
    Mtxf identitymtx;
    coord3d launchvel;
    s32 pad;
    Mtxf launchmtx;
    coord3d aimpos;
    coord3d aimdir;
    PropRecord *playerprop;
    coord3d *prevplayerpos;

    hand = &g_CurrentPlayer->hands[handnum];

    playerprop = currentPlayerGetProp();
    prevplayerpos = playerGetPrevPos();

    mtx4LoadIdentity(&identitymtx);
    gunGetShotDir(&aimpos, &aimdir, handnum);
    mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &aimdir);

    launchvel.x = aimdir.x * 33.333332f;
    launchvel.y = aimdir.y * 33.333332f;
    launchvel.z = aimdir.z * 33.333332f;

    if (g_ClockTimer > 0)
    {
        launchvel.x += (playerprop->pos.x - prevplayerpos->x) / g_GlobalTimerDelta;
        launchvel.y += (playerprop->pos.y - prevplayerpos->y) / g_GlobalTimerDelta;
        launchvel.z += (playerprop->pos.z - prevplayerpos->z) / g_GlobalTimerDelta;
    }

    mtx4Copy(&g_CurrentPlayer->hands[handnum].throw_item_pos_related, &launchmtx);

    launchmtx.m[3][0] = 0.0f;
    launchmtx.m[3][1] = 0.0f;
    launchmtx.m[3][2] = 0.0f;

    grenadeobj = weaponCreateFromModel(PROP_CHRGRENADEROUND, ITEM_GRENADEROUND);

    if (grenadeobj != NULL)
    {
        grenadeobj->timer = GLGRENADE_TIMER;
        grenadeobj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
        grenadeobj->runtime_bitflags |= playerGetCurrentNum() << RUNTIMEBITSHIFT_OWNER;

        gunLaunchProjectileFromPlayer(grenadeobj, &hand->field_B58, &launchmtx, &launchvel, (s32 *)&identitymtx);

        if (grenadeobj->runtime_bitflags & RUNTIMEBITFLAG_00000080)
        {
            grenadeobj->projectile->unk8C = g_GLGrenadeLaunchUnk8C;
            grenadeobj->projectile->unk94 = g_GLGrenadeLaunchUnk94;
            grenadeobj->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;
        }
    }
}


/**
 * Address: 0x7F05F928
 * This function is responsible for attaching a rocket to the end of the Rocket Launcher and updating its matrices.
 */
void gunUpdateAttachedRocket(s32 handIndex)
{
    struct hand *entry;
    AttachedObj *attachedRocket;
    Model *rocketModel;
    Mtxf worldMtx;
    PropRecord *prop;
    AttachmentChild *attachmentChild;

    entry = &g_CurrentPlayer->hands[handIndex];

    attachedRocket = entry->rocket;

    if (attachedRocket == NULL)
    {
        return;
    }

    attachmentChild = attachedRocket->child;

    if (attachmentChild == NULL)
    {
        return;
    }

    prop = currentPlayerGetProp();
    rocketModel = attachedRocket->model;

    mtx4Copy(&entry->throw_item_pos_related, &worldMtx);

    worldMtx.m[3][0] = 0.0f;
    worldMtx.m[3][1] = 0.0f;
    worldMtx.m[3][2] = 0.0f;

    mtx4ScaleRows012(attachedRocket->model->scale, (f32 *)&worldMtx);

    objChangeShading(attachedRocket, &entry->field_B58, &worldMtx, prop->stan);
    chrobjCollisionRelated(attachedRocket);

    rocketModel->render_pos = dynAllocate((s32)rocketModel->obj->numMatrices << 6);

    mtx4Copy(&attachedRocket->transform, &worldMtx);
    mtx4SetTranslation((Mtxf *)&attachedRocket->position, &worldMtx);

    mtx4MultAffine(camGetWorldToScreenMtxf(), &worldMtx, rocketModel->render_pos);
    modelUpdateRelationsQuick(rocketModel, rocketModel->obj->RootNode);

    attachmentChild->flags1 |= 2;
    attachmentChild->unk18 = -rocketModel->render_pos->pos.m[3][2];
}


/*
* Address: 0x7f05fa7c
*/
void gunCreateRocket(GUNHAND hand)
{
    struct hand * hand_ptr;
    struct WeaponObjRecord * rocket;

    hand_ptr = &g_CurrentPlayer->hands[hand];

    if ((hand_ptr->rocket == NULL) && (hand_ptr->weapon_ammo_in_magazine > 0))
    {
        rocket = (struct WeaponObjRecord *)weaponCreateFromModel(PROP_CHRROCKET, ITEM_ROCKETROUND);

        if (rocket != NULL)
        {
            hand_ptr->rocket = (ObjectRecord *)rocket;
            hand_ptr->firedrocket = 0;
            rocket->timer = 1;
        }
    }
}


/*
* Address: 0x7F05FB00
* This function frees some sort of ObjectRecord from the given hand
*/
#if defined(VERSION_EU)
void gunFreeRocket(enum GUNHAND hand)
{
    struct hand* hand_ptr;
    ObjectRecord* hand_obj_record;

    hand_ptr = &g_CurrentPlayer->hands[hand];
    hand_obj_record = hand_ptr->rocket;

    if (hand_obj_record != NULL)
    {
        objFreePermanently(hand_obj_record, 1);
        hand_ptr->rocket = NULL;
    }
}


extern f32 D_80053DDC;

/*
* Address: 0x7F05FB64
*/
void gunFireTankShell(s32 handnum)
{
    WeaponObjRecord *obj;
    struct hand *hand;
    Mtxf identitymtx;
    coord3d velocity;
    ObjectRecord *tankobj;
    coord3d unscaledvelocity;
    Mtxf shellmtx;
    coord3d screenpos;
    coord3d aimdir;
    PropRecord *playerprop;
    coord3d *prevplayerpos;
    ITEM_IDS weaponid;
    coord3d spawnpos;
    PropRecord *tankprop;

    hand = &g_CurrentPlayer->hands[handnum];

    playerprop = currentPlayerGetProp();
    prevplayerpos = playerGetPrevPos();
    weaponid = gunGetWeapon(handnum);

    mtx4LoadIdentity(&identitymtx);

    if (weaponid == ITEM_TANKSHELLS) 
    {
        tankprop = playerGetTank();

        if (1);

        if ((tankprop != NULL) && (tankprop->flags & TANK_RUN_STATE_RUNNING)) 
        {
            tankGetTurretDir(&aimdir);
        } 
        else 
        {
            gunGetAimRay(&screenpos, &aimdir);
            mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &aimdir);
        }

        velocity.x = aimdir.x * g_TankShellSpeed;
        velocity.y = aimdir.y * g_TankShellSpeed;
        velocity.z = aimdir.z * g_TankShellSpeed;

        if (g_ClockTimer > 0) {
            velocity.x += (playerprop->pos.x - prevplayerpos->x) / g_GlobalTimerDelta;
            velocity.y += (playerprop->pos.y - prevplayerpos->y) / g_GlobalTimerDelta;
            velocity.z += (playerprop->pos.z - prevplayerpos->z) / g_GlobalTimerDelta;
        }

        if ((tankprop != NULL) && (tankprop->flags & TANK_RUN_STATE_RUNNING)) 
        {
            tankobj = tankprop->obj;
            spawnpos.x = tankobj->model->render_pos[4].pos.m[3][0];
            spawnpos.y = tankobj->model->render_pos[4].pos.m[3][1];
            spawnpos.z = tankobj->model->render_pos[4].pos.m[3][2];

            mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &spawnpos);
        } 
        else 
        {
            spawnpos.x = playerprop->pos.x;
            spawnpos.y = playerprop->pos.y;
            spawnpos.z = playerprop->pos.z;
        }

        if ((g_CurrentPlayer && g_CurrentPlayer));

        explosionStartShake();
    } 
    else 
    {
        gunGetShotDir(&screenpos, &aimdir, handnum);
        mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &aimdir);

        spawnpos.x = hand->field_B58.x;
        spawnpos.y = hand->field_B58.y;
        spawnpos.z = hand->field_B58.z;

        if (1);

        unscaledvelocity.x = aimdir.x * D_80053DDC;
        unscaledvelocity.y = aimdir.y * D_80053DDC;
        unscaledvelocity.z = aimdir.z * D_80053DDC;

        velocity.x = unscaledvelocity.x * g_GlobalTimerDelta;
        velocity.y = unscaledvelocity.y * g_GlobalTimerDelta;
        velocity.z = unscaledvelocity.z * g_GlobalTimerDelta;

        if (g_ClockTimer > 0) 
        {
            velocity.x += (playerprop->pos.x - prevplayerpos->x) / g_GlobalTimerDelta;
            velocity.y += (playerprop->pos.y - prevplayerpos->y) / g_GlobalTimerDelta;
            velocity.z += (playerprop->pos.z - prevplayerpos->z) / g_GlobalTimerDelta;
        }
    }

    mtx4Copy(&g_CurrentPlayer->hands[handnum].throw_item_pos_related, &shellmtx);

    shellmtx.m[3][0] = 0.0f;
    shellmtx.m[3][1] = 0.0f;
    shellmtx.m[3][2] = 0.0f;

    if (hand->rocket != NULL) 
    {
        obj = (WeaponObjRecord *) hand->rocket;
        hand->firedrocket = 1;
    } 
    else 
    {
        obj = (WeaponObjRecord *) weaponCreateFromModel(PROP_CHRROCKET, ITEM_ROCKETROUND);
    }

    if (obj == NULL) 
    {
        return;
    }

    obj->timer = -1;
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
    obj->runtime_bitflags |= playerGetCurrentNum() << RUNTIMEBITSHIFT_OWNER;

    gunLaunchProjectileFromPlayer(obj, &spawnpos, &shellmtx, &velocity, (s32 *) &identitymtx);

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_00000080)
    {
        obj->projectile->flags |= PROJECTILEFLAG_LAUNCHING;

        if (weaponid != ITEM_TANKSHELLS)
        {
            obj->projectile->flags |= PROJECTILEFLAG_00000020;
            obj->projectile->unkB0 = obj->runtime_pos.y;
            obj->projectile->unkB4 = obj->projectile->speed.y;
            obj->projectile->unk10.x = unscaledvelocity.x;
            obj->projectile->unk10.y = unscaledvelocity.y;
            obj->projectile->unk10.z = unscaledvelocity.z;
            obj->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            if (obj->projectile->sounds[0] == NULL)
            {
                sndPlaySfx(g_musicSfxBufferPtr, 1, &obj->projectile->sounds[0]);
            } 
            else if (obj->projectile->sounds[1] == NULL)
            {
                sndPlaySfx(g_musicSfxBufferPtr, 1, &obj->projectile->sounds[1]);
            }
        }
    }
}
#endif

const char g_GunHudIntegerFormat[] = "%d\n";
const char aSD[] = "%s: %d\n";
const char g_GunDeathCountFormat[] = "%s %d %s\n";
const char aSD_0[] = "%s: %d\n";


//uncomment when actor is worked on
//#include "chr.h"
extern s32 dynAllocateVerticesDiscard NO_SDATA;

void gunsInitAnimGroups(void) {
    modelSetVtxAllocator(&dynAllocateVerticesDiscard);
    initWeaponAnimGroups();
}


// Address 0x7F007980 NTSC.
// Address 0x7F007900 PAL.
void gunsStopSounds(void)
{
    int i;
    int j;

    for (i = 0; i < getPlayerCount(); i++)
    {
        for (j = 0; j < 2; j++)
        {
            struct hand *hand = &g_playerPointers[i]->hands[j];
            
            if (hand->audioHandle != NULL && sndGetPlayingState(hand->audioHandle))
            {
                sndDeactivate(hand->audioHandle);
            }
        }
    }
}
