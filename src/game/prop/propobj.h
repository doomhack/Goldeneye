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


void                 objFree(ObjectRecord *obj, s32 freeprop, s32 canregen);
PropRecord          *objInit(ObjectRecord *obj, ModelFileHeader *modeldef, PropRecord *prop, Model *model);
void                 objFreePermanently(struct ObjectRecord *obj, bool freeprop);

void                 objChangeShading(ObjectRecord *, coord3d *, Mtxf *, StandTile *);

void                 objPlaceAtPad(ObjectRecord *baseobj, coord3d *pad, Mtxf *matrix, StandTile *stan, coord3d *pos2);
void                 objPlaceAtPadSideways(ObjectRecord *obj, coord3d *pos, Mtxf *padmtx, StandTile *stan2, coord3d *pos2);
PropRecord*          objInitWithAutoModel(ObjectRecord* obj);
PropRecord*          objInitWithModelDef(ObjectRecord* object, ModelFileHeader* header);
void                 glassCreateShards(coord3d *pos, coord3d *xaxis, coord3d *yaxis, coord3d *zaxis, f32 xmin, f32 xmax, f32 ymin, f32 ymax);
void                 objsFreeDeformedVertices(void);
f32                  bondviewGetPlayerPitchRadians(void);
void                 modelResetDeformedVertices(Model* model);

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
#include "game/prop/objrender.h"

#endif
