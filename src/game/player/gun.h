#ifndef _BONDWALK_H_
#define _BONDWALK_H_
#include <ultra64.h>
#include "game/model/chrobjdata.h"
#include <bondconstants.h>
#include <bondtypes.h>

/**
 * Pool of AlSoundState slots for impact and ricochet SFX.
 */
#define NUM_IMPACT_SFX_STATES 4

typedef struct WeaponStats
{
    /**
     * Distance of gun flash from the end of the barrel.
     */
    f32 MuzzleFlashExtension;

    /**
     * On screen gun position, X.
     */
    f32 PosX;

    /**
     * On screen gun position, Y.
     */
    f32 PosY;

    /**
     * On screen gun position, Z.
     */
    f32 PosZ;

    /**
     * The amount of play the guns are given when you move forward/back.
     */
    f32 PlayX;

    /**
     * The amount of play the guns are given when you move side
     * to side.
     */
    f32 PlayY;

    /**
     * The amount of play the guns are given when you move up or down.
     */
    f32 PlayZ;

    /**
     * Ammo type, and what ammo img is shown.
     */
    s32 AmmoType;

    /**
     * Number of rounds before needing to reload.
     */
    s16 MagSize;

    /**
     * Time between automatic shots. -1 (0xFF) for disabled.
     * For reference, RC-P90=0x2, and KF7=0x3.
     * (is this an enum?)
     */
    u8 AutomaticFiringRate;

    /**
     * Time between manual shots.
     * For reference, KF7=0, while rocket launcher = 0x14.
     */
    s8 SingleFiringRate;

    /**
     * How many objects the bullet goes through.
     */
    u8 ObjectsShootThrough;

    /**
     * Sound trigger rate.
     */
    u8 SoundTriggerRate;

    /**
     * Sound effect played when gun is shot. There are 261 sound effects, or 0 - 105h.
     */
    u16 Sound;

    /**
     * Comment from long ago:
     * Location of address that displays the bullet shells flying from the guns, and runs
     * the ping sound of the casings hitting the ground. This value is either 00000000 for
     * no bullet casings (laser, knife, grenade, so on) or 8003CB60, the location of pointers
     * that point to code possibly, I haven't explored the area a lot near that address
     * (8003CB60)
     */
    struct ModelFileHeader * ptr_cartridge_struct;

    /**
     * Amount of destruction or power each bullet packs.
     * For reference, KF7=1.0, while golden gun = 100.0.
     */
    f32 DestructionAmount;

    /**
     * Amount of inaccuracy the gun has.
     * For reference, KF7=10.0, sniper rifle = 0.0.
     */
    f32 Inaccuracy;

    /**
     * Amount of zoom the gun has. Just setting this value doesn't give the option of 
     * zooming in and out, that is somewhere else.
     * For reference, KF7=30.0, sniper rifle = 15.0.
     */
    f32 Zoom;

    /**
     * Speed of red cross-hair.
     */
    f32 CrosshairSpeed;

    /**
     * Weapon Aim/Lock-On Speed.
     */
    f32 AimLockSpeed;

    /**
     * Hand stabilization. A low value will keep the hands still while a high value will
     * cause bonds arms to twirl around in circles and turn every which way.
     */
    f32 Sway;

    /**
     * Recoil speed.
     */
    union {
        s32 RecoilSpeed;
        s8 b44[4];
    };

    /**
     * How far back bonds hands pull back when the gun is fired.
     */
    f32 RecoilBack;

    /**
     * Amount of recoil the gun has.
     */
    f32 RecoilUp;

    /**
     * How far back the bolt slides when the gun is fired.
     */
    f32 BoltRecoilBack;

    /**
     * The minimum amount of sound an enemy can hear from this weapon.
     */
    f32 LoudnessMin;

    /**
     * The maximum amount of sound an enemy can hear from this weapon.
     */
    f32 LoudnessMax;

    /**
     * Amount of noise increased with each shot fired.
     * ("Noise" is the value used to determine if a guard should be alerted).
     */
    f32 NoiseIncreasePerShot;

    /**
     * Noise bleeds off at a constant NoiseIncreasePerShot / this value per
     * second, i.e. one shot's worth of noise decays over this many seconds.
     * This is the guaranteed minimum decay, so accumulated noise always
     * drains to LoudnessMin in finite time. All weapons use 2.0 for this value.
     */
    f32 NoiseDecayLinearTime;

    /**
     * Proportional noise decay time constant in seconds.
     * Noise above LoudnessMin relaxes toward the floor with this time
     * constant: (noise - LoudnessMin) / this value is removed per second.
     * Dominates when the player is loud; near LoudnessMin the linear term
     * (NoiseDecayLinearTime) takes over. All weapons use 4.0 for this value.
     */
    f32 NoiseDecayScaledTime;

    /**
     * Force of impact.
     */
    f32 ForceOfImpact;

    u32 BitFlags;
} WeaponStats;

typedef struct AmmoStats {
    /**
     * The maximum amount of ammo.
     */
    u32 MaxAmmo;

    /**
     * Segmented address (segment 0x02, global image bank) of this ammo
     * type's HUD icon image. 0 for no icon. Resolved to an RDRAM pointer
     * by adding g_ImageBankOffset before use.
     */
    u32 IconImage;

    /**
     * Vertical offset in pixels when the icon is drawn on the HUD.
     */
    f32 IconYOffset;
} AmmoStats;

typedef struct GunModelFileRecord
{
  ModelFileHeader * item_header;
  char * item_file_name;
  s32 has_no_model;
  WeaponStats * item_weapon_stats;
  u16 upper_watch_text;
  u16 lower_watch_text;
  f32 watch_pos_x;
  f32 watch_pos_y;
  f32 watch_pos_z;
  f32 x_rotation;
  f32 y_rotation;
  u16 weapon_of_choice_text;
  u16 watch_equipment_text;
  f32 equip_watch_x;
  f32 equip_watch_y;
  f32 equip_watch_z;
} GunModelFileRecord;

// Per item: the first-person model, the watch texts and where the watch shows it
extern GunModelFileRecord gitem_structs[];

typedef struct CasingRecord {
    f32 floor_y_pos;
    coord3d pos;
    coord3d vel;
#if VERSION_EU
    Mtx3f rot_mtx;
    Mtx3f rot_velocity_mtx;
#else
    Mtxf rot_mtx;
    Mtxf rot_velocity_mtx;
#endif
    ModelFileHeader *header;
} CasingRecord;

typedef struct CartridgeModelFileRecord {
    ModelFileHeader* header;
    char * text;
} CartridgeModelFileRecord;

extern CasingRecord g_Casings[20];
extern s32 g_NumLiveCasings;
extern u32 size_item_buffer[];
extern WeaponStats sniperrifle_stats;
extern WeaponStats camera_stats;

typedef struct WatchContButtonPositions {
    f32 words[19];
    coord3d start;
    coord3d joystick;
    coord3d dPad;
    coord3d cUp;
    coord3d cDown;
    coord3d cLeft;
    coord3d cRight;
    coord3d a;
    coord3d b;
    coord3d l;
    coord3d r;
    coord3d z;
} WatchContButtonPositions;

f32 weaponGetImpactForce(ITEM_IDS item);

u32 weaponHasFlag(ITEM_IDS item, u32 mask);

void gunUpdateAndFireBothHands(void);

f32 gunGetReloadDipDepth(s32 item);

f32 gunGetDualOffset(GUNHAND hand);


s32 weaponHasAmmo(ITEM_IDS item);


WeaponStats *weaponGetStats(ITEM_IDS item);

ITEM_IDS gunGetWeapon(GUNHAND hand);
void gunEquip(GUNHAND hand, s32 next_weapon);
void gunTryReload(GUNHAND hand);

s8 weaponGetFireRate(ITEM_IDS item);
f32 weaponGetDamage(ITEM_IDS item);
u16 weaponGetSound(ITEM_IDS item);
u8 weaponGetSoundRate(ITEM_IDS item);

void gunLaunchProjectile(ObjectRecord *obj, coord3d *pos,  StandTile *stan, Mtxf *matrix, coord3d *velocity, Mtxf *tumble,  PropRecord *owner);
void gunGetAimRay(coord3d *zeropos, coord3d *vec);

s8 gunGetFiringStatus(GUNHAND hand);
void gunFireTankShell(s32 hand);
void         gunClearHandModel(GUNHAND hand);
void gunUnequip(enum GUNHAND hand, enum ITEM_IDS weapid);
s32 gunHandModelIsEmpty(enum GUNHAND hand);
void gunFreeRocket(enum GUNHAND hand);
void gunSetTracerTarget(coord3d* pos);

void gunCalcBlend(enum GUNHAND handnum);
void gunUpdateSway(f32 breathing, f32 maxspeed, f32 speed_verta, f32 speed_theta);
void gunSetPullback(f32 angle);

void gunGetCrosshairPos(f32* horizontal_angle, f32* vertical_angle);

ITEM_IDS gunGetShownWeapon(GUNHAND hand);
void gunStartWatchSwap(GUNHAND hand, s32 next_weapon);
void gunClearWatchMenuWeapon(GUNHAND hand);
void gunUpdateCrosshairSwivel(f32 turn_x, f32 turn_y, f32 max_aim_lock_speed);
void gunSwitchWhenEmpty(void);
f32 gunGetNoise(GUNHAND hand);
void gunZoomIn(f32 zoom);
void gunZoomOut(f32 zoom);
f32 gunGetZoomFov(void);
void gunCycleForward(void);
void gunCycleBackward(void);
void gunUpdateCrosshairManual(f32 turn_x, f32 turn_y);
void gunTickGameplay(s32 triggerOn);
u8 weaponGetPenetration(ITEM_IDS item);


void gunRenderHands(Gfx **gdl);


void gunsInitAnimGroups(void);
void gunsStopSounds(void);

char *itemGetChoiceText(ITEM_IDS item);
s32 playerHasCopiedGEKey(void);
u8 * gunGetHandModelBuffer(GUNHAND hand);
u32 gunGetHandModelBufferSize(s32 hand);
ModelFileHeader *gunGetHandModelHeader(GUNHAND hand);
void gunCreateRocket(GUNHAND hand);
void gunDebApplyKeyframe(void);
void gunEquipPreviousWeapons(void);
int gunGetFistModelItem(void);
s32 gunGetHandModelItem(GUNHAND hand);
void gunLoadHandModel(GUNHAND hand);
void gunSetFirstPersonPartsVisible(Model* model, s32 val);
void gunSpawnGLGrenade(s32 handnum);
void gunThrowGrenade(s32 hand);
void gunThrowKnife(s32 hand);
void gunThrowObject(s32 hand);
void gunTickBoltRecoil(GUNHAND hand);
void gunTickHingeAngle(enum GUNHAND hand, s32 open);
void gunUpdateAttachedRocket(s32 handIndex);

extern Lights1 g_WeaponEnvmapLight;
void gunGetShotDir(coord3d *origin, coord3d *result, enum GUNHAND hand);
void gunLaunchProjectileFromPlayer(ObjectRecord *obj, coord3d *targetpos, Mtxf *mtx, coord3d *velocity, Mtxf *tumble);
void gunAnalyzeGEKey(void);
void gunOpenWeaponCase(void);
f32 gunSetHorizontalOffset(GUNHAND hand);
f32 gunGetMaxHingeAngle(GUNHAND hand);
void gunStartBoltRecoil(GUNHAND hand);
void gunReload(enum GUNHAND hand);
extern ALSoundState *g_ImpactSfxStates[NUM_IMPACT_SFX_STATES];
extern u32 g_GunDebApplyKeyframe;
extern u32 g_gunDebKeyframeIndex;
extern CartridgeModelFileRecord ejected_cartridge[];
ModelFileHeader *get_ptr_weapon_model_header_line(ITEM_IDS weapon);
void gunSetWatchMenuWeapon(GUNHAND hand, ITEM_IDS weaponnum_watchmenu);
void gunSetHeldItemVisible(Model *model, s32 val);

#include "game/hud/ammohud.h"
#include "game/menu/watchmodel.h"
#include "game/player/ammo.h"
#include "game/player/beam.h"
#include "game/player/casing.h"
#include "game/player/playerstats.h"
#include "game/player/shot.h"

#endif
