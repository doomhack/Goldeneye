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
 * @param prop: Prop for tank
 * @param moving: whether the tank is moving
 */
void                 objSetMoving(struct PropRecord *prop, s32 moving);

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
void                 chrobjMaybeDetonateObjectIfFlags(ObjectRecord *obj, f32 damage, coord3d *pos, ITEM_IDS item, s32 owner);
void                 objEnsureProjectile(PropRecord *);
void                 projectileSetSticky(PropRecord *);
void                 chrobjCollisionRelated(ObjectRecord *);
void                 objChangeShading(ObjectRecord *, coord3d *, Mtxf *, StandTile *);
s32                  projectileTestPropBoundingSphere(coord3d *zeropos, coord3d *pos, coord3d *vec, f32 scale);
void                 objGetCollisionBounds(PropRecord *prop, struct rect4f **polygon, s32 *edges, f32 *top, f32 *bottom);
void                 doorActivate(DoorRecord *door, DOORSTATE State);
bool                 posIsInFrontOfDoor(PropRecord *prop, DoorRecord *door);
void                 doorsChooseSwingDirection(PropRecord *chrprop, DoorRecord *door);
Gfx                 *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 xlupass);

f32                  chrobjFogVisRangeRelated(PropRecord *prop, f32 size);
s32                  propGetRoomsBbox2d(PropRecord *prop, bbox2d *bbox);

// note: rgba to rgb
void                 shadingLerp(rgba_s32 *dest, s32 enable, rgba_f32 *src);
void                 remoteMinesDetonate(void);

void                 objDetach(PropRecord *prop);
void                 objFreeEmbedmentOrProjectile(PropRecord *);
void                 objBounce(ObjectRecord *obj, coord3d *velocity);
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
void                 objPlaceAtPadSideways(ObjectRecord *obj, struct coord3d *pos, Mtxf *padmtx, StandTile *stan2, struct coord3d *pos2);
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
s32                  chrobjTestPolygonsTouchingOrOverlap2D(struct rect4f *poly1, s32 edges1, struct rect4f *poly2, s32 edges2);
s32                  chrobjTestPointPolygonCollision(struct coord3d *point, f32 collision_radius, struct rect4f *polygon, s32 edges);
void                 objDetonateRecursive(PropRecord *prop, f32 damage, struct coord3d *pos, s32 weaponnum, s32 owner);
struct ModelRoData_BoundingBoxRecord* chrobjGetBboxFromObjectRecord(ObjectRecord *obj);
void                 alarmStopSound(void);
void                 alarmsReset(void);
bool                 padBoundsContainPoint(coord3d* pos, f32 radius, BoundPadRecord *boundpads);
void                 glassCreateShards(f32*, f32*, f32*, f32*, f32, f32, f32, f32, f32, f32);
void                 modelGetXYExtents(Model *model, f32 *xmax, f32 *xmin, f32 *ymax, f32 *ymin);
s32                  vehicleFindBlockingProp(struct PropRecord *argProp);
PropRecord*          weaponInitWithModelDef(struct ObjectRecord* obj, ModelFileHeader* modeldef);
void                 chrSetupHeldWeapon(WeaponObjRecord* weapon, ChrRecord* chr);
void                 playerDropInventory(void);
void                 objsFreeDeformedVertices(void);
f32                  bondviewGetPlayerPitchRadians(void);
TICKOP               objTickPlayer(struct PropRecord* prop);
TICKOP               weaponTickPlayer(struct PropRecord* prop);
void                 doorShatterGlass(DoorRecord *door);
bool                 posIsOnScreen(PropRecord *prop, coord3d *pos, f32 radius, bool checkFogEnd);
void                 objUpdateShading(rgba_u8 *dest, rgba_u8 *src);
void                 chrRenderHeldWeapon(void *renderContext, GUNHAND hand, Gfx **gdl);
HATTYPE              hatGetModel(PropRecord *prop);
s32                  objDrop(PropRecord *prop);
void                 modelResetDeformedVertices(Model* model);
PropRecord          *chrGiveWeaponEx(ChrRecord *self, s32 propid, ITEM_IDS itemid, s32 flags, WeaponObjRecord *weapon, ItemModelFileRecord *prop_header);
Gfx                 *countdownTimerRender(Gfx *DL);

#endif
