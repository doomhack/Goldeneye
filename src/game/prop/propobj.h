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

extern f32 g_SoloAmmoMultiplier;

extern struct LinkRecord *g_LevelLoadPropSwitch;
extern LockDoorRecord *g_LevelLoadPropLockDoor;
extern SafeObjectRecord *g_LevelLoadPropSafeItem;
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

void                 objFree(ObjectRecord *obj, s32 freeprop, s32 canregen);
PropRecord          *objInit(ObjectRecord *obj, ModelFileHeader *modeldef, PropRecord *prop, Model *model);
void                 objFreePermanently(struct ObjectRecord *obj, bool freeprop);

Gfx                 *weaponRenderTracers(Gfx *gdl);
void                 objGetTileShading(PropRecord *prop, rgba_u8 *colour);
void                 objGetShadeColour(PropRecord *prop, rgba_u8 *colour);
void                 chrobjCollisionRelated(ObjectRecord *);
void                 objChangeShading(ObjectRecord *, coord3d *, Mtxf *, StandTile *);
void                 objGetCollisionBounds(PropRecord *prop, coord2d **polygon, s32 *edges, f32 *top, f32 *bottom);
Gfx                 *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 xlupass);

f32                  chrobjFogVisRangeRelated(PropRecord *prop, f32 size);
s32                  propGetRoomsBbox2d(PropRecord *prop, bbox2d *bbox);

// note: rgba to rgb
void                 shadingLerp(rgba_s32 *dest, s32 enable, rgba_f32 *src);

void                 objPlaceAtPad(ObjectRecord *baseobj, coord3d *pad, Mtxf *matrix, StandTile *stan, coord3d *pos2);
void                 objPlaceAtPadSideways(ObjectRecord *obj, coord3d *pos, Mtxf *padmtx, StandTile *stan2, coord3d *pos2);
PropRecord*          objInitWithAutoModel(ObjectRecord* obj);
PropRecord*          objInitWithModelDef(ObjectRecord* object, ModelFileHeader* header);
s32                  chrobjTestPolygonsTouchingOrOverlap2D(coord2d *poly1, s32 edges1, coord2d *poly2, s32 edges2);
s32                  chrobjTestPointPolygonCollision(coord3d *point, f32 collision_radius, coord2d *polygon, s32 edges);
struct ModelRoData_BoundingBoxRecord* chrobjGetBboxFromObjectRecord(ObjectRecord *obj);
ModelRoData_BoundingBoxRecord *chrobjGetBboxFromObjFile(ModelFileHeader *obj);
bool                 padBoundsContainPoint(coord3d* pos, f32 radius, BoundPadRecord *boundpads);
void                 glassCreateShards(coord3d *pos, coord3d *xaxis, coord3d *yaxis, coord3d *zaxis, f32 xmin, f32 xmax, f32 ymin, f32 ymax);
void                 modelGetXYExtents(Model *model, f32 *xmax, f32 *xmin, f32 *ymax, f32 *ymin);
s32                  vehicleFindBlockingProp(struct PropRecord *argProp);
void                 objsFreeDeformedVertices(void);
f32                  bondviewGetPlayerPitchRadians(void);
bool                 posIsOnScreen(PropRecord *prop, coord3d *pos, f32 radius, bool checkFogEnd);
void                 objUpdateShading(rgba_u8 *dest, rgba_u8 *src);
void                 modelResetDeformedVertices(Model* model);

bool objGetOnscreenRenderBounds(PropRecord *prop, coord3d *screenpos, coord2d *max, coord2d *min);


#include "game/prop/objdamage.h"
#include "game/prop/objhit.h"
#include "game/prop/pickup.h"
#include "game/prop/objslots.h"
#include "game/prop/carried.h"
#include "game/prop/mine.h"
#include "game/prop/objtick.h"
#include "game/prop/cctv.h"
#include "game/prop/autogun.h"
#include "game/prop/vehicle.h"

#endif
