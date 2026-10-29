#ifndef _CHROBJHANDLER_H_
#define _CHROBJHANDLER_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <snd.h>   

#ifdef VERSION_EU

#define CHROBJ_TIMETOREGEN   50
#define CHROBJ_TIMETOREGEN_F 50.0f

#define PLASTIQUE_EXPLOSION_DELAY_TICKS 100

#else

#define CHROBJ_TIMETOREGEN   60
#define CHROBJ_TIMETOREGEN_F 60.0f
#define PLASTIQUE_EXPLOSION_DELAY_TICKS 120

#endif

extern f32 g_CctvAlarmTimeScale;
extern f32 g_CctvDamageScale;
extern f32 g_AutogunPendingDamageTick;
extern f32 g_AutogunDamageScalar;
extern f32 g_AutogunDamageScale;
extern f32 g_SoloAmmoMultiplier;

extern s32 g_RemoteMineOwnerTriggerFlag;
extern s32 g_NextWeaponSlot;
extern s32 g_NextHatSlot;
extern struct LinkRecord *g_LevelLoadPropSwitch;
extern LockDoorRecord *g_LevelLoadPropLockDoor;
extern SafeObjectRecord *g_LevelLoadPropSafeItem;
extern f32 g_AutogunPendingDamageTick;
extern f32 g_AutogunDamageScalar;
extern f32 g_SoloAmmoMultiplier;

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

Gfx                 *weaponRenderTracers(Gfx *gdl);
void                 objGetTileShading(PropRecord *prop, rgba_u8 *colour);
void                 objGetShadeColour(PropRecord *prop, rgba_u8 *colour);
void                 propobjSetDropped(PropRecord *prop, DROPTYPE droptype);
void                 objDropRecursively(PropRecord *prop);
void                 weaponSetGunfireVisible(PropRecord *prop, s32 firing);
WeaponObjRecord     *weaponCreateFromModel(PROP modelnum, s32 weaponid);
void                 chrobjCollisionRelated(ObjectRecord *);
void                 objChangeShading(ObjectRecord *, coord3d *, Mtxf *, StandTile *);
void                 objGetCollisionBounds(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom);
Gfx                 *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 xlupass);

f32                  chrobjFogVisRangeRelated(PropRecord *prop, f32 size);
s32                  propGetRoomsBbox2d(PropRecord *prop, bbox2d *bbox);

// note: rgba to rgb
void                 shadingLerp(rgba_s32 *dest, s32 enable, rgba_f32 *src);
void                 remoteMinesDetonate(void);

void                 objDetach(PropRecord *prop);
void                 chrRemoveHeldWeapon(ChrRecord *chr, GUNHAND hand);
void                 propweaponSetDual(WeaponObjRecord *leftweapon, WeaponObjRecord *rightweapon);
void                 objPlaceAtPad(ObjectRecord *baseobj, coord3d *pad, Mtxf *matrix, StandTile *stan, coord3d *pos2);
void                 objPlaceAtPadSideways(ObjectRecord *obj, coord3d *pos, Mtxf *padmtx, StandTile *stan2, coord3d *pos2);
bool                 chrEquipWeapon(WeaponObjRecord *wep, ChrRecord *chr);
TICKOP               propPickupByPlayer(PropRecord *prop, bool showstring);
TICKOP               propobjInteract(PropRecord *prop);
WeaponObjRecord      *weaponFindThrown(s32 weaponnum);
HatRecord           *hatCreate(bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef);
PropRecord*          objInitWithAutoModel(ObjectRecord* obj);
PropRecord*          objInitWithModelDef(ObjectRecord* object, ModelFileHeader* header);
void                 objDetach(PropRecord *prop);
s32                  chrobjTestPolygonsTouchingOrOverlap2D(coord2d *poly1, s32 edges1, coord2d *poly2, s32 edges2);
s32                  chrobjTestPointPolygonCollision(coord3d *point, f32 collision_radius, coord2d *polygon, s32 edges);
struct ModelRoData_BoundingBoxRecord* chrobjGetBboxFromObjectRecord(ObjectRecord *obj);
ModelRoData_BoundingBoxRecord *chrobjGetBboxFromObjFile(ModelFileHeader *obj);
bool                 padBoundsContainPoint(coord3d* pos, f32 radius, BoundPadRecord *boundpads);
void                 glassCreateShards(coord3d *pos, coord3d *xaxis, coord3d *yaxis, coord3d *zaxis, f32 xmin, f32 xmax, f32 ymin, f32 ymax);
void                 modelGetXYExtents(Model *model, f32 *xmax, f32 *xmin, f32 *ymax, f32 *ymin);
s32                  vehicleFindBlockingProp(struct PropRecord *argProp);
PropRecord*          weaponInitWithModelDef(struct ObjectRecord* obj, ModelFileHeader* modeldef);
void                 chrSetupHeldWeapon(WeaponObjRecord* weapon, ChrRecord* chr);
void                 playerDropInventory(void);
void                 objsFreeDeformedVertices(void);
f32                  bondviewGetPlayerPitchRadians(void);
TICKOP               objTickPlayer(struct PropRecord* prop);
TICKOP               weaponTickPlayer(struct PropRecord* prop);
bool                 posIsOnScreen(PropRecord *prop, coord3d *pos, f32 radius, bool checkFogEnd);
void                 objUpdateShading(rgba_u8 *dest, rgba_u8 *src);
void                 chrRenderHeldWeapon(PropRecord *chrprop, GUNHAND hand, JointListEntry **jointList);
HATTYPE              hatGetModel(PropRecord *prop);
s32                  objDrop(PropRecord *prop);
void                 modelResetDeformedVertices(Model* model);
PropRecord          *chrGiveWeaponEx(ChrRecord *self, s32 propid, ITEM_IDS itemid, s32 flags, WeaponObjRecord *weapon, ModelFileHeader *modeldef);

PropRecord *hatCreateForChr(ChrRecord *chr, s32 modelnum, u32 flags);
bool objGetOnscreenRenderBounds(PropRecord *prop, coord3d *screenpos, coord2d *max, coord2d *min);
void pickupAddAmmo(AMMOTYPE ammotype,int amount,int doplaysound,int dodisplaytext);
void pickupPlayWeaponSound(ITEM_IDS weapontype);
void pickupShowWeaponText(ITEM_IDS weaponid);
bool objTestForInteract(PropRecord* prop);
s32 objTick(struct PropRecord *prop);
void hatAssignToChr(HatRecord* hat, ChrRecord* chr);
void proxminesTickGuards(void);

AmmoCrateRecord     *ammocrateAllocate(void);

#include "game/prop/objdamage.h"
#include "game/prop/objhit.h"

#endif
