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
#include "fr.h"
#include "assets/obseg/text/LgunE.h"
#include "game/text/text.h"
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
ALSoundState* g_ImpactSfxStates[NUM_IMPACT_SFX_STATES];

CasingRecord g_Casings[20];

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
/**
 * Controls the lighting on environment mapped weapons such as the Cougar Magnum and Golden Gun.
 */
Lights1 g_WeaponEnvmapLight = gdSPDefLights1(
    0x96, 0x96, 0x96,   // ambient RGB
    0xff, 0xff, 0xff,   // diffuse RGB
    0xb2, 0x4d, 0x2e);  // direction
u32 size_item_buffer[] = {0x14820, 0x14820};

u32 g_GunModelBufferOffset[] ={0x7530, 0x7530};


CartridgeModelFileRecord ejected_cartridge[] = {
	{&cartridge_header, "GcartridgeZ"},
	{&cartrifle_header, "GcartrifleZ"},
	{&cartblue_header, "GcartblueZ"},
	{&cartshell_header, "GcartshellZ"},
	{0, ""}
};

#include <assets/obseg/gun/gunWeaponStats.inc.c>

#include <assets/obseg/gun/gunModelFileRecord.inc.c>
#include <PR/libaudio.h>
#include "game/gfx/dyn.h"
#include "game/model/initactorpropstuff.h"
#include "game/prop/explosion.h"
#include "game/world/bg.h"

u32 g_GunDebApplyKeyframe = 0;
u32 g_gunDebKeyframeIndex = 0;

/** Knife slash: the first of the two alternating swings. */
Weapon1PTransformKeyframe knifeSlashKeyframes1[10] = {
    { 0, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 8.0f },
    { 0, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 8.0f },
    { 0, { 6.0f, -1.5f, 0.0f }, { 5.641564f, 0.23511f, 0.13564f }, 0.5f, 8.0f },
    { 0, { 12.5f, -3.5f, 0.0f }, { 6.042227f, 0.04475f, 0.555717f }, 0.5f, 8.0f },
    { 0, { -10.0f, -11.0f, 0.0f }, { 1.241009f, 0.316988f, 1.086363f }, 0.5f, 8.0f },
    { 0, { -14.0f, -15.0f, 0.0f }, { 1.830307f, 6.143663f, 1.274134f }, 0.5f, 10.0f },
    { 0, { -1.0f, -9.0f, 0.0f }, { 0.384244f, 0.360323f, 0.105151f }, 0.5f, 10.0f },
    { 0, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 20.0f },
    { 0, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 20.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f }
};

/** Knife slash: the second swing. */
Weapon1PTransformKeyframe knifeSlashKeyframes2[10] = {
    { 0, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 8.0f },
    { 0, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 8.0f },
    { 0, { -8.5f, -6.0f, 0.0f }, { 5.483035f, 5.834561f, 6.082734f }, 0.5f, 8.0f },
    { 0, { -3.0f, -3.5f, 0.0f }, { 0.402412f, 5.729386f, 5.691899f }, 0.5f, 8.0f },
    { 0, { -0.5f, -8.5f, 0.0f }, { 1.234298f, 5.731543f, 5.608871f }, 0.5f, 8.0f },
    { 0, { 7.0f, -28.5f, -1.5f }, { 1.306924f, 5.695158f, 5.69583f }, 0.5f, 10.0f },
    { 0, { -1.5f, -9.0f, 0.0f }, { 0.067808f, 6.259576f, 0.203519f }, 0.5f, 10.0f },
    { 0, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 20.0f },
    { 0, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 20.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f }
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



// These were a zero scalar followed by an array, read as one ModelRenderData
// (or coord3d) through a cast. That only works while the compiler keeps them
// adjacent, which -G 8 and zero-init-in-bss do not, so define real structs.
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

struct PunchSounds punch_sounds = {
    PUNCH1_SFX,
    PUNCH2_SFX,
    PUNCH3_SFX
};

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

struct sfx2 watchlaser_fire_sounds = { RICO_LASER2_SFX, RICO_LASER3_SFX };
struct sfx3 knife_throw_sounds = { KNIFE_THROW1_SFX, KNIFE_THROW2_SFX, KNIFE_THROW3_SFX };
struct gun_trigger_state g_ZeroTriggerState = { 0, 0 };
ModelRenderData g_DefaultCasingModelRenderData = {NULL, TRUE, 0x00000003};

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

/* Unused. Kept after ammo_related[] in case anything reads past the end of it. */
u16 g_AmmoRelatedPad[] = { 0, 0, 0, 0, };

extern struct ModelSkeleton skeleton_gun_kf7;


// forward declarations

void gunGetShotDir(coord3d* origin, coord3d* result, enum GUNHAND hand);
void gunLaunchProjectileFromPlayer(ObjectRecord *obj, coord3d *targetpos, Mtxf *mtx, coord3d *velocity, Mtxf *tumble);
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


void gunDebApplyKeyframe(void)
{
    if (!g_GunDebApplyKeyframe) { return; }

    g_CurrentPlayer->hands[0].useAnimMtx = 1;
    mtx4LoadRotation(&DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot, (Mtxf *)&g_CurrentPlayer->hands[0].animMtx);
    mtx4SetTranslation(&DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos, (Mtxf *)&g_CurrentPlayer->hands[0].animMtx);
    g_GunDebApplyKeyframe = 0;
}


/**
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


void gunCalcBlend(enum GUNHAND handnum)
{
    s32 nextpos[2];
    s32 curpos[2];
    f32 mult = weaponGetStats(gunGetWeapon(handnum))->Sway;

    nextpos[handnum] = (g_CurrentPlayer->hands[handnum].curblendpos + 2) % 4;
    curpos[handnum] = (g_CurrentPlayer->hands[handnum].curblendpos + 1) % 4;
    g_CurrentPlayer->hands[handnum].curblendpos = curpos[handnum];

    g_CurrentPlayer->hands[handnum].blendlook[nextpos[handnum]].x = (RANDOMFRAC() - 0.5f) * 0.08f * mult;
    g_CurrentPlayer->hands[handnum].blendlook[nextpos[handnum]].y = (RANDOMFRAC() - 0.5f) * 0.1f * mult;
    g_CurrentPlayer->hands[handnum].blendlook[nextpos[handnum]].z = -1;

    g_CurrentPlayer->hands[handnum].blendup[nextpos[handnum]].x = (RANDOMFRAC() - 0.5f) * 0.1f * mult;
    g_CurrentPlayer->hands[handnum].blendup[nextpos[handnum]].y = 1;
    g_CurrentPlayer->hands[handnum].blendup[nextpos[handnum]].z = (RANDOMFRAC() - 0.5f) * 0.1f * mult;

    g_CurrentPlayer->hands[handnum].blendpos[nextpos[handnum]].x = (RANDOMFRAC() * 0.75f) + 1.5f;
    g_CurrentPlayer->hands[handnum].blendpos[nextpos[handnum]].y = (2 + RANDOMFRAC()) * g_CurrentPlayer->hands[handnum].blendscale1;
    g_CurrentPlayer->hands[handnum].blendpos[nextpos[handnum]].z = (RANDOMFRAC() - 0.5f) * 2.5f;

    if (g_CurrentPlayer->hands[handnum].sideflag < 0)
    {
        g_CurrentPlayer->hands[handnum].blendpos[nextpos[handnum]].x *= -1;

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


s32 gunHandModelIsEmpty(enum GUNHAND hand)
{
    return g_CurrentPlayer->handModelState[hand] > 0
        || (g_CurrentPlayer->handModelItem[hand] == 0 && g_CurrentPlayer->handModelNextItem[hand] < 0);
}


s32 gunGetHandModelItem(GUNHAND hand)
{
    return g_CurrentPlayer->handModelItem[hand];
}


ModelFileHeader *gunGetHandModelHeader(GUNHAND hand)
{
    return &g_CurrentPlayer->handModelHeader[hand];
}


u8 * gunGetHandModelBuffer(GUNHAND hand)
{
    return g_CurrentPlayer->handModelBuffer[hand];
}


u32 gunGetHandModelBufferSize(s32 hand)
{
    return size_item_buffer[hand];
}


void gunClearHandModel(GUNHAND hand)
{
  g_CurrentPlayer->handModelState[hand] = 0;
  g_CurrentPlayer->handModelItem[hand] = ITEM_UNARMED;
  g_CurrentPlayer->handModelNextItem[hand] = -1;
  g_CurrentPlayer->handModelLocked[hand] = 1;
  return;
}


void gunSetHandModel(GUNHAND hand, ITEM_IDS item)
{
    if (g_CurrentPlayer->handModelLocked[hand]) { return; }

    if (g_CurrentPlayer->handModelState[hand] >= 0)
    {
        if (item != g_CurrentPlayer->handModelItem[hand])
        {
            g_CurrentPlayer->handModelState[hand] = -1;
            g_CurrentPlayer->handModelNextItem[hand] = item;
        }
        return;
    }

    if (item != g_CurrentPlayer->handModelItem[hand])
    {
        g_CurrentPlayer->handModelNextItem[hand] = item;
        return;
    }

    g_CurrentPlayer->handModelState[hand] = 1;
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

    if ((g_CurrentPlayer->handModelState[hand] < 0) && (g_CurrentPlayer->handModelLocked[hand] == 0))
    {
        if ((g_CurrentPlayer->handModelState[hand] < -2) || (g_CurrentPlayer->handModelItem[hand] == ITEM_UNARMED))
        {
            item             = g_CurrentPlayer->handModelNextItem[hand];
            ptr_item_text    = (s8 *)gunGetModelFileName(item);
            ptr_weapon_model = get_ptr_weapon_model_header_line(item);

            if ((ptr_item_text != NULL) && (ptr_weapon_model != NULL))
            {
                buffer_weapon      = gunGetHandModelBuffer(hand);
                size_buffer_weapon = gunGetHandModelBufferSize(hand);

                g_CurrentPlayer->handModelHeader[hand] = *ptr_weapon_model;

                if (item == ITEM_SUIT_LF_HAND)
                {
                    texInitPool(&g_CurrentPlayer->handModelTexPool[hand], buffer_weapon + 0xBD70, size_buffer_weapon + 0xFFFF4290);
                    modelFileLoad(&g_CurrentPlayer->handModelHeader[hand], (u8 *)ptr_item_text, buffer_weapon, 0xBD70, &g_CurrentPlayer->handModelTexPool[hand]);
                }
                else if ((item == ITEM_TRIGGER) || (item == ITEM_WATCHLASER))
                {
                    texInitPool(&g_CurrentPlayer->handModelTexPool[hand], buffer_weapon + 0xAFD0, size_buffer_weapon + 0xFFFF5030);
                    modelFileLoad(&g_CurrentPlayer->handModelHeader[hand], (u8 *)ptr_item_text, buffer_weapon, 0xAFD0, &g_CurrentPlayer->handModelTexPool[hand]);
                }
                else
                {
                    texInitPool(&g_CurrentPlayer->handModelTexPool[hand], &buffer_weapon[g_GunModelBufferOffset[hand]], size_buffer_weapon - g_GunModelBufferOffset[hand]);
                    modelFileLoad(&g_CurrentPlayer->handModelHeader[hand], (u8 *)ptr_item_text, buffer_weapon, g_GunModelBufferOffset[hand], &g_CurrentPlayer->handModelTexPool[hand]);
                }
            }

            g_CurrentPlayer->handModelState[hand] = 1;
            g_CurrentPlayer->handModelItem[hand]      = item;
            g_CurrentPlayer->handModelNextItem[hand]     = -1;
        }
        else
        {
            g_CurrentPlayer->handModelState[hand]--;
        }
    }
}


ITEM_IDS gunGetNextWeapon(GUNHAND hand, s32 direction)
{
	if (g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_LOWER) 
    {
		if (
			(direction < 0 && (g_CurrentPlayer->hands[hand].cycleDirection > 0)) ||
			(direction > 0 && (g_CurrentPlayer->hands[hand].cycleDirection < 0)) ) 
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
        g_CurrentPlayer->hands[hand].switchTime = g_CurrentPlayer->hands[hand].stateTime;

#ifdef VERSION_EU
        if (getPlayerCount() == 1) {
            g_CurrentPlayer->hands[hand].switchTime += 0xE;
        } else {
            g_CurrentPlayer->hands[hand].switchTime += 0xA;
        }
#else
        if (getPlayerCount() == 1) {
            g_CurrentPlayer->hands[hand].switchTime += 0x11;
        } else {
            g_CurrentPlayer->hands[hand].switchTime += 0xD;
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
        g_CurrentPlayer->hands[hand].cycleDirection = cycleDirection;
    }
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
    g_CurrentPlayer->hands[hand].nextSoundTime = 0;
    g_CurrentPlayer->hands[hand].lastSoundTime = 0;
    invDetermineEquippedItem();
}


s8 gunGetFiringStatus(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weapon_firing_status;
}

f32 gunGetDualOffset(GUNHAND hand) {
	return g_CurrentPlayer->hands[hand].dualOffset;
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


void gunUpdateSway(f32 breathing, f32 maxspeed, f32 vertaSpeed, f32 thetaSpeed)
{
    f32 dampt[2];
    s32 i;
    f32 absVerta = vertaSpeed;
    f32 breathStep;
    f32 minbreathing;

    if (absVerta < 0.0f) { absVerta = -absVerta; }

    if (maxspeed > 0.8f)
    {
        g_CurrentPlayer->gunposamplitude = 1.0f;
    }
    else
    {
        if (maxspeed > 0.1f)
        {
            f32 tmp = (1.0f - cosf((maxspeed - 0.1f) * M_TAU_F / 2.8f));
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

    if (g_CurrentPlayer->gunposamplitude < 0.5f * absVerta)
    {
        g_CurrentPlayer->gunposamplitude = 0.5f * absVerta;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->gunAmpSum = (g_CurrentPlayer->gunAmpSum * (PAL ? 0.9403f : 0.95f)) + g_CurrentPlayer->gunposamplitude;
    }

    g_CurrentPlayer->gunposamplitude = g_CurrentPlayer->gunAmpSum * (PAL ? 0.059700012f : 0.050000012f);

    minbreathing = 0.016666668f * absVerta;
    if (breathing < minbreathing)
    {
        breathing = minbreathing;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->cycleSum = (g_CurrentPlayer->cycleSum * (PAL ? 0.9403f : 0.95f)) + breathing;
    }

    breathing = g_CurrentPlayer->cycleSum * (PAL ? 0.059700012f : 0.050000012f);

    breathStep = breathing * g_GlobalTimerDelta;
    dampt[0] = g_CurrentPlayer->hands[0].dampt + breathStep;

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

    if (g_CurrentPlayer->syncchange + breathStep > 0.0f)
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
        g_CurrentPlayer->hands[i].weapon_theta_displacement = -1.75f * thetaSpeed;
        g_CurrentPlayer->hands[i].weapon_verta_displacement = -2.0f * vertaSpeed;
    }
}


void gunSetPullback(f32 angle)
{
    g_CurrentPlayer->hands[GUNRIGHT].gunofs2_z = (1.0f - cosf(angle)) * 5.0f;
    g_CurrentPlayer->hands[GUNLEFT].gunofs2_z = (1.0f - cosf(angle)) * 5.0f;
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


void gunTickHingeAngle(enum GUNHAND hand, s32 open)
{
    if (open != 0)
    {
        if (g_CurrentPlayer->hands[hand].hingeAngle < gunGetMaxHingeAngle(hand))
        {
            g_CurrentPlayer->hands[hand].hingeAngle += (0.029088823f * g_GlobalTimerDelta);
        }
        if (g_CurrentPlayer->hands[hand].hingeAngle > gunGetMaxHingeAngle(hand)) {
            g_CurrentPlayer->hands[hand].hingeAngle = gunGetMaxHingeAngle(hand);
        }
    }
    else
    {
        if (g_CurrentPlayer->hands[hand].hingeAngle > 0.0f)
        {
            g_CurrentPlayer->hands[hand].hingeAngle -= (0.017453294f * g_GlobalTimerDelta);
        }
        if (g_CurrentPlayer->hands[hand].hingeAngle < 0.0f)
        {
            g_CurrentPlayer->hands[hand].hingeAngle = 0.0f;
        }
    }
}


void gunStartBoltRecoil(GUNHAND hand) {
	g_CurrentPlayer->hands[hand].boltRecoiling = 1;
}


void gunTickBoltRecoil(GUNHAND hand)
{
    f32 recoil_back;

    recoil_back = weaponGetStats(gunGetShownWeapon(hand))->BoltRecoilBack;

    if (g_CurrentPlayer->hands[hand].boltRecoiling != 0)
    {
        if (g_CurrentPlayer->hands[hand].boltRecoil < recoil_back)
        {
            g_CurrentPlayer->hands[hand].boltRecoil = (g_CurrentPlayer->hands[hand].boltRecoil + (recoil_back * 0.25f * g_GlobalTimerDelta));

        }
        if (recoil_back <= g_CurrentPlayer->hands[hand].boltRecoil) {
            g_CurrentPlayer->hands[hand].boltRecoil = recoil_back;
            g_CurrentPlayer->hands[hand].boltRecoiling = 0;
        }
    }
    else if (g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine > 0)
    {
        if (g_CurrentPlayer->hands[hand].boltRecoil > 0.0f)
        {
            g_CurrentPlayer->hands[hand].boltRecoil = (g_CurrentPlayer->hands[hand].boltRecoil - (recoil_back * 0.16666667f * g_GlobalTimerDelta));

        }
        if (g_CurrentPlayer->hands[hand].boltRecoil < 0.0f)
        {
            g_CurrentPlayer->hands[hand].boltRecoil = 0.0f;
        }
    }
}


/** Shows or hides the parts of a gun model only drawn in first person (switches 8-13 and 35). */
void gunSetFirstPersonPartsVisible(Model* model, s32 val)
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


/** Shows or hides the item held in a thrown weapon's hand, such as the grenade (switches 14 and 15). */
void gunSetHeldItemVisible(Model* model, s32 val)
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


void gunLaunchProjectile(ObjectRecord *obj, coord3d *pos, StandTile *stan, Mtxf *matrix, coord3d *velocity, Mtxf *tumble, PropRecord *owner)
{
    PropRecord *prop;
    Projectile *projectile;

    prop = obj->prop;

    if (prop != NULL)
    {
        propActivate(prop);
        propEnable(prop);
        mtx4ScaleRows012(obj->model->scale, matrix);
        objChangeShading(obj, pos, matrix, stan);

        setupUpdateObjectRoomPosition(obj);

        chrobjCollisionRelated(obj);
        objEnsureProjectile(prop);

        if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        {
            projectile = obj->projectile;
            projectile->flags |= 0x41;
            obj->projectile->ownerprop = owner;
            projectileSetSticky(prop);
            mtx4Copy(tumble, &obj->projectile->mtx);
            obj->projectile->speed.f[0] = velocity->f[0];
            obj->projectile->speed.f[1] = velocity->f[1];
            obj->projectile->speed.f[2] = velocity->f[2];
            obj->projectile->obj = obj;
            obj->projectile->launchTick = g_LvTickCount;
        }
    }
}


/**
 * Determines where the projectile may safely enter the world. Ideally that is the targetpos position, but if targetpos
 * is obstructed the player's position used as a fallback. This prevents the player from launching
 * projectiles through nearby surfaces.
 */
void gunLaunchProjectileFromPlayer(ObjectRecord *obj, coord3d *targetpos, Mtxf *mtx, coord3d *velocity, Mtxf *tumble)
{
    PropRecord *playerprop;
    coord3d pos;
    StandTile *tile;
    f32 yhi;
    f32 ylo;
    s32 usedfallback;
    f32 stanheight;
    u8 rooms[2];
    s32 traversed[20];
    s32 numtraversed;

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

    gunLaunchProjectile(obj, &pos, tile, mtx, velocity, tumble, playerprop);

    if (obj->runtime_bitflags & 0x80) {
        if (usedfallback) {
            obj->projectile->flags |= PROJECTILEFLAG_00000100;
            obj->projectile->targetPos.x = targetpos->x;
            obj->projectile->targetPos.y = targetpos->y;
            obj->projectile->targetPos.z = targetpos->z;
        }

        rooms[0] = playerGetRoom();
        rooms[1] = 0xff;

        bgFindRoomsAlongSegment(playerGetPos3(), &pos, rooms, obj->projectile->rooms, traversed, &numtraversed, ARRAYCOUNT(traversed));
    }
}


/**
 * Address 0x7F05EE24 (NTSC)
 * Address 0x7F05F2DC (PAL)
*/
void gunThrowGrenade(s32 hand)
{
    Mtxf tumble;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d pos;
    Mtxf mtx;
    struct WeaponObjRecord *wor;
    struct coord3d shotOrigin;
    struct coord3d base_speed_vec;
    struct PropRecord* player_prop;
    struct coord3d *bondprevpos;
    Mtxf handmtx;
    ALSoundState *sfx_state;
    s32 current_weapon;

    wor = NULL;
    base_velocity = 16.666666f;

    player_prop = currentPlayerGetProp();
    bondprevpos = playerGetPrevPos();
    current_weapon = gunGetWeapon(hand);

    physRandomLaunch(&throw_speed_vec, &tumble);
    gunGetShotDir(&shotOrigin, &base_speed_vec, hand);
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

    pos.f[0] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][0];
    pos.f[1] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][1];
    pos.f[2] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][2];

    mtx4LoadIdentity(&mtx);
    mtx4Copy(&g_CurrentPlayer->hands[hand].gunWorldMtx, &handmtx);
    handmtx.m[3][0] = 0.0f;
    handmtx.m[3][1] = 0.0f;
    handmtx.m[3][2] = 0.0f;
    mtx4MultInPlace(&handmtx, &mtx);

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

        gunLaunchProjectileFromPlayer(wor, &pos, &mtx, &throw_speed_vec, &tumble);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->bounciness = 0.3f;
            wor->projectile->groundFriction = 0.13333333f;
            wor->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            sfx_state = sndPlaySfx(g_musicSfxBufferPtr, GRENADE_THROW_SFX, NULL);

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
    Mtxf tumble;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d pos;
    Mtxf mtx;
    struct coord3d shotOrigin;
    struct coord3d base_speed_vec;
    Mtxf handmtx;
    struct PropRecord* player_prop;
    struct coord3d *bondprevpos;

    wor = NULL;
    base_velocity = 25.0f;

    player_prop = currentPlayerGetProp();
    bondprevpos = playerGetPrevPos();

    physRandomLaunch(&throw_speed_vec, &tumble);
    gunGetShotDir(&shotOrigin, &base_speed_vec, hand);
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

    pos.f[0] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][0];
    pos.f[1] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][1];
    pos.f[2] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][2];

    mtx4LoadZRotation(4.712389f, &mtx);
    mtx4LoadXRotation(M_PI_F, &handmtx);
    mtx4MultInPlace(&handmtx, &mtx);
    mtx4Copy(&g_CurrentPlayer->hands[hand].gunWorldMtx, &handmtx);

    handmtx.m[3][0] = 0.0f;
    handmtx.m[3][1] = 0.0f;
    handmtx.m[3][2] = 0.0f;
    mtx4MultInPlace(&handmtx, &mtx);

    guRotateF(&tumble, 360.0f / ((randomGetNext() * (0.5f / (f32)INT_MAX)) + 12.1f), mtx.m[1][0], mtx.m[1][1], mtx.m[1][2]);

    wor = weaponCreateFromModel(PROP_CHRKNIFE, ITEM_THROWKNIFE);

    if (wor != NULL)
    {
        wor->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
        wor->runtime_bitflags |= playerGetCurrentNum() << RUNTIMEBITSHIFT_OWNER;

        gunLaunchProjectileFromPlayer(wor, &pos, &mtx, &throw_speed_vec, &tumble);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->bounciness = 0.1f;
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

    Mtxf tumble;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d pos;
    Mtxf mtx;
    struct WeaponObjRecord *wor;
    s32 new_prop_type;
    struct coord3d shotOrigin;
    struct coord3d base_speed_vec;
    struct PropRecord* player_prop;
    struct coord3d *bondprevpos;
    Mtxf handmtx;
    ALSoundState *sfx_state;
    s32 current_weapon;

    wor = NULL;
    base_velocity = 16.666666f;

    player_prop = currentPlayerGetProp();
    bondprevpos = playerGetPrevPos();
    current_weapon = gunGetWeapon(hand);

    if (current_weapon == ITEM_GOLDENEYEKEY)
    {
        base_velocity = 6.6666665f;
    }

    physRandomLaunch(&throw_speed_vec, &tumble);
    gunGetShotDir(&shotOrigin, &base_speed_vec, hand);
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

    pos.f[0] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][0];
    pos.f[1] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][1];
    pos.f[2] = g_CurrentPlayer->hands[hand].gunWorldMtx.m[3][2];

    mtx4LoadIdentity(&mtx);
    mtx4Copy(&g_CurrentPlayer->hands[hand].gunWorldMtx, &handmtx);
    handmtx.m[3][0] = 0.0f;
    handmtx.m[3][1] = 0.0f;
    handmtx.m[3][2] = 0.0f;
    mtx4MultInPlace(&handmtx, &mtx);

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

        gunLaunchProjectileFromPlayer(wor, &pos, &mtx, &throw_speed_vec, &tumble);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->bounciness = 0.1f;
            wor->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            sfx_state = sndPlaySfx(g_musicSfxBufferPtr, GRENADE_THROW_SFX, NULL);

            if (sfx_state != NULL)
            {
                chrobjSndCreatePostEventDefault(sfx_state, (struct coord3d *) &wor->runtime_pos);
            }
        }
    }
}


/**
 * Spawns Grenade Launcher rounds and makes them inherit the player's momentum.
 */
void gunSpawnGLGrenade(s32 handnum)
{
    WeaponObjRecord *grenadeobj;
    struct hand *hand;
    Mtxf identitymtx;
    coord3d launchvel;
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

    mtx4Copy(&g_CurrentPlayer->hands[handnum].gunWorldMtx, &launchmtx);

    launchmtx.m[3][0] = 0.0f;
    launchmtx.m[3][1] = 0.0f;
    launchmtx.m[3][2] = 0.0f;

    grenadeobj = weaponCreateFromModel(PROP_CHRGRENADEROUND, ITEM_GRENADEROUND);

    if (grenadeobj != NULL)
    {
        grenadeobj->timer = GLGRENADE_TIMER;
        grenadeobj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
        grenadeobj->runtime_bitflags |= playerGetCurrentNum() << RUNTIMEBITSHIFT_OWNER;

        gunLaunchProjectileFromPlayer(grenadeobj, &hand->muzzlePos, &launchmtx, &launchvel, (s32 *)&identitymtx);

        if (grenadeobj->runtime_bitflags & RUNTIMEBITFLAG_00000080)
        {
            grenadeobj->projectile->bounciness = g_GLGrenadeLaunchUnk8C;
            grenadeobj->projectile->groundFriction = g_GLGrenadeLaunchUnk94;
            grenadeobj->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;
        }
    }
}


/**
 * This function is responsible for attaching a rocket to the end of the Rocket Launcher and updating its matrices.
 */
void gunUpdateAttachedRocket(s32 handIndex)
{
    struct hand *entry;
    ObjectRecord *attachedRocket;
    Model *rocketModel;
    Mtxf worldMtx;
    PropRecord *prop;
    PropRecord *rocketProp;

    entry = &g_CurrentPlayer->hands[handIndex];

    attachedRocket = entry->rocket;

    if (attachedRocket == NULL)
    {
        return;
    }

    rocketProp = attachedRocket->prop;

    if (rocketProp == NULL)
    {
        return;
    }

    prop = currentPlayerGetProp();
    rocketModel = attachedRocket->model;

    mtx4Copy(&entry->gunWorldMtx, &worldMtx);

    worldMtx.m[3][0] = 0.0f;
    worldMtx.m[3][1] = 0.0f;
    worldMtx.m[3][2] = 0.0f;

    mtx4ScaleRows012(attachedRocket->model->scale, &worldMtx);

    objChangeShading(attachedRocket, &entry->muzzlePos, &worldMtx, prop->stan);
    chrobjCollisionRelated(attachedRocket);

    rocketModel->render_pos = dynAllocate((s32)rocketModel->obj->numMatrices << 6);

    mtx4Copy(&attachedRocket->mtx, &worldMtx);
    mtx4SetTranslation((Mtxf *)&attachedRocket->runtime_pos, &worldMtx);

    mtx4MultAffine(camGetWorldToScreenMtxf(), &worldMtx, rocketModel->render_pos);
    modelUpdateRelationsQuick(rocketModel, rocketModel->obj->RootNode);

    rocketProp->flags |= 2;
    rocketProp->zDepth = -rocketModel->render_pos->pos.m[3][2];
}


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


extern f32 g_ThrowVelocityScale;

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


        explosionStartShake();
    } 
    else 
    {
        gunGetShotDir(&screenpos, &aimdir, handnum);
        mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &aimdir);

        spawnpos.x = hand->muzzlePos.x;
        spawnpos.y = hand->muzzlePos.y;
        spawnpos.z = hand->muzzlePos.z;


        unscaledvelocity.x = aimdir.x * g_ThrowVelocityScale;
        unscaledvelocity.y = aimdir.y * g_ThrowVelocityScale;
        unscaledvelocity.z = aimdir.z * g_ThrowVelocityScale;

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

    mtx4Copy(&g_CurrentPlayer->hands[handnum].gunWorldMtx, &shellmtx);

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
            obj->projectile->riseY = obj->runtime_pos.y;
            obj->projectile->riseSpeed = obj->projectile->speed.y;
            obj->projectile->accel.x = unscaledvelocity.x;
            obj->projectile->accel.y = unscaledvelocity.y;
            obj->projectile->accel.z = unscaledvelocity.z;
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
