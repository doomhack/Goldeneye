#ifndef _CHROBJHANDLER_H_
#define _CHROBJHANDLER_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <snd.h>   

#ifdef VERSION_EU

#define CHROBJ_TIMETOREGEN   50
#define CHROBJ_TIMETOREGEN_F 50.0f
#define CHROBJ_GAS_TIMER 0x5dc /* 1500 */

#define PLASTIQUE_EXPLOSION_DELAY_TICKS 100

#else

#define CHROBJ_TIMETOREGEN   60
#define CHROBJ_TIMETOREGEN_F 60.0f
#define CHROBJ_GAS_TIMER 0x708 /* 1800 */
#define PLASTIQUE_EXPLOSION_DELAY_TICKS 120

#endif

extern f32 g_CctvAlarmTimeScale;
extern f32 g_CctvDamageScale;
extern f32 g_AutogunPendingDamageTick;
extern f32 g_AutogunDamageScalar;
extern f32 g_AutogunDamageScale;
extern f32 g_SoloAmmoMultiplier;

extern s32 alarm_timer;
extern s32 *g_AlarmSound;
extern f32 toxic_gas_sound_timer;
extern s32 activate_gas_sound_timer;
extern coord3d gasLeakSource;
extern s32 g_GasLastCoughTime;
extern f32 gasLeakTimer;
extern ALSoundState *g_GasSound;
extern s32 clock_drawn_flag;
extern s32 clock_enable;
extern f32 clock_time;
extern s32 g_RemoteMineOwnerTriggerFlag;
extern s32 g_NextWeaponSlot;
extern s32 g_NextHatSlot;
extern ObjectRecord *g_LevelLoadPropSwitch;
extern LockDoorRecord *g_LevelLoadPropLockDoor;
extern ObjectRecord *g_LevelLoadPropSafeItem;
extern struct PropRecord *g_ProjectileHitProp;
extern s32 bodypartshot;
extern f32 g_AutogunPendingDamageTick;
extern f32 g_AutogunDamageScalar;
extern f32 g_SoloAmmoMultiplier;
extern struct Model *g_CurrentProjectileModel;
extern struct ModelNode *g_ProjectileHitNode;

#if defined(VERSION_EU)
extern ExplosionDetailsRecordEuList object_explosion_details;
#else
extern ExplosionDetailsRecord object_explosion_details[];
#endif

/**
 * @param arg0: Prop for tank
 * @param arg1: maybe flags
 */
void                 objSetMoving(struct PropRecord *arg0, s32 arg1);

void                 objFreePermanently(struct ObjectRecord *obj, bool freeprop);

void                 chrobjApplySpeed(f32 *openPosition, f32 maxFrac, f32 *speedPtr, f32 accel, f32 decel, f32 maxSpeed);
void                 chrobjCallsApplySpeed(f32 *openPosition, f32 maxFrac, f32 *speedPtr, f32 accel, f32 decel, f32 maxSpeed);
Gfx                 *weaponRenderTracers(Gfx *gdl);
void                 objGetTileShading(PropRecord *prop, u8 col[4]);
void                 propobjSetDropped(PropRecord *prop, DROPTYPE droptype);
void                 objDropRecursively(PropRecord *prop);
void                 chrobjSndCreatePostEventDefault(ALSoundState *, coord3d *);
void                 alarmActivate(void);
void                 weaponSetGunfireVisible(PropRecord *prop, s32 firing);
s32                  sndVolumeForPosDefault(coord3d *pos); //getVolume?
ObjectRecord        *weaponCreateFromModel(PROP modelnum, s32 weaponid);
void                 objApplyDamage(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS, s32 playernum);
void                 chrobjMaybeDetonateObjectIfFlags(ObjectRecord *arg0, f32 arg1, coord3d *arg2, ITEM_IDS item, s32 arg4);
void                 objEnsureProjectile(PropRecord *);
void                 projectileSetSticky(PropRecord *);
void                 chrobjCollisionRelated(ObjectRecord *);
void                 objChangeShading(ObjectRecord *, coord3d *, Mtxf *, StandTile *);
s32                  projectileTestPropBoundingSphere(coord3d *arg0, coord3d *arg1, coord3d *arg2, f32 arg3);
void                 objGetCollisionBounds(PropRecord *arg0, struct rect4f **arg1, s32 *arg2, f32 *arg3, f32 *arg4);
void                 doorActivate(DoorRecord *door, DOORSTATE State);
bool                 posIsInFrontOfDoor(PropRecord *prop, DoorRecord *door);
void                 doorsChooseSwingDirection(PropRecord *arg0, DoorRecord *arg1);
Gfx                 *chrobjRenderProp(PropRecord *arg0, Gfx *arg1, s32 arg2);

f32                  chrobjFogVisRangeRelated(PropRecord *prop, f32 size);
s32                  propGetRoomsBbox2d(PropRecord *prop, bbox2d *bbox);

// note: rgba to rgb
void                 shadingLerp(rgba_s32 *arg0, s32 arg1, rgba_f32 *arg2);
void                 remoteMinesDetonate(void);

void                 objDetach(PropRecord *prop);
void                 objFreeEmbedmentOrProjectile(PropRecord *);
void                 objBounce(ObjectRecord *obj, coord3d *arg1);
bool                 alarmIsActive(void);
void                 gasStart(coord3d *source);
void                 chrRemoveHeldWeapon(ChrRecord *chr, GUNHAND hand);
void                 doorStopSounds(DoorRecord *door);
void                 doorActivatePortal(DoorRecord *door);
void                 doorUpdateBbox(DoorRecord *door);
s32                  sndVolumeForDistanceDefault(f32 vol);
void                 monitorSetImageByNum(MonitorRecord *mon, s32 monAnimID);
void                 propweaponSetDual(WeaponObjRecord *leftweapon, WeaponObjRecord *rightweapon);
f32                  countdownTimerGetValue(void);
bool                 countdownTimerIsRunning(void);
void                 countdownTimerSetRunning(bool enable);
void                 countdownTimerSetValue(f32 time);
void                 countdownTimerSetVisible(int clocklockbits, bool unset);
void                 objPlaceAtPad(ObjectRecord *baseobj, struct coord3d *pad, Mtxf *matrix, StandTile *stan, struct coord3d *pos2);
void                 objPlaceAtPadSideways(ObjectRecord *obj, struct coord3d *pos, Mtxf *arg2, StandTile *stan2, struct coord3d *pos2);
bool                 chrEquipWeapon(WeaponObjRecord *wep, ChrRecord *chr);
bool                 objIsCollectable(PropDefHeaderRecord *obj);
TICKOP               propPickupByPlayer(PropRecord *prop, bool showstring);
TICKOP               propobjInteract(PropRecord *prop);
s32                  objGetDestroyedLevel(ObjectRecord *obj);
void                 doorActivateWrapper(PropRecord *prop);
bool                 objIsHealthy(ObjectRecord *self);
bool                 objIsMortal(ObjectRecord* obj);
KeyRecord            *weaponFindThrown(s32 ID);
bool                 gasIsActive(void);
HatRecord           *hatCreate(bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef);
PropRecord*          objInitWithAutoModel(ObjectRecord* obj);
PropRecord*          objInitWithModelDef(ObjectRecord* object, ModelFileHeader* header);
Embedment           *embedmentAllocate(void);
void                 objDetach(PropRecord *prop);
void                 objUpdateThrowKnifeSound(ObjectRecord *obj);
s32                  chrobjTestPolygonsTouchingOrOverlap2D(struct rect4f *arg0, s32 arg1, struct rect4f *arg2, s32 arg3);
s32                  chrobjTestPointPolygonCollision(struct coord3d *arg0, f32 arg1, struct rect4f *arg2, s32 arg3);
void                 objDetonateRecursive(PropRecord *arg0, f32 arg1, struct coord3d *arg2, s32 arg3, s32 owner);
struct ModelRoData_BoundingBoxRecord* chrobjGetBboxFromObjectRecord(ObjectRecord *arg0);
void                 alarmStopSound(void);
void                 alarmsReset(void);
bool                 padBoundsContainPoint(coord3d* pos, f32 arg1, BoundPadRecord *boundpads);
void                 glassCreateShards(f32*, f32*, f32*, f32*, f32, f32, f32, f32, f32, f32);
void                 modelGetXYExtents(Model *model, f32 *arg1, f32 *arg2, f32 *arg3, f32 *arg4);
s32                  vehicleFindBlockingProp(struct PropRecord *arg0);
PropRecord*          weaponInitWithModelDef(struct ObjectRecord* arg0, ModelFileHeader* arg1);
void                 chrSetupHeldWeapon(WeaponObjRecord* arg0, ChrRecord* arg1);
void                 playerDropInventory(void);
void                 objsFreeDeformedVertices(void);
f32                  bondviewGetPlayerPitchRadians(void);
TICKOP               objTickPlayer(struct PropRecord* arg0);
TICKOP               weaponTickPlayer(struct PropRecord* arg0);
void                 doorShatterGlass(DoorRecord *door);
bool                 posIsOnScreen(PropRecord *prop, coord3d *pos, f32 arg2, bool arg3);
void                 objUpdateShading(rgba_u8 *dest, rgba_u8 *src);
void                 chrRenderHeldWeapon(void *renderContext, GUNHAND hand, Gfx **gdl);
HATTYPE              hatGetModel(PropRecord *prop);
s32                  objDrop(PropRecord *prop);
void                 modelResetDeformedVertices(Model* model);
PropRecord          *chrGiveWeaponEx(ChrRecord *self, s32 propid, ITEM_IDS itemid, s32 flags, WeaponObjRecord *weapon, ItemModelFileRecord *prop_header);
Gfx                 *countdownTimerRender(Gfx *DL);

#endif
