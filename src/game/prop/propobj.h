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

#if defined(VERSION_EU)
extern ExplosionDetailsRecordEuList object_explosion_details;
#else
extern ExplosionDetailsRecord object_explosion_details[];
#endif


PropRecord *objInit(ObjectRecord *obj, ModelFileHeader *header, PropRecord *prop, Model *model);
PropRecord *objInitWithModelDef(ObjectRecord *obj, ModelFileHeader *header);
PropRecord *objInitWithAutoModel(ObjectRecord *obj);
void        objSetPosition(ObjectRecord *obj, coord3d *pos, Mtxf *matrix, StandTile *stan);
void        objPlaceAtPad(ObjectRecord *obj, coord3d *pos, Mtxf *matrix, StandTile *stan, coord3d *centre);
void        objPlaceAtPadSideways(ObjectRecord *obj, coord3d *pos, Mtxf *padmtx, StandTile *stan, coord3d *centre);
void        objFree(ObjectRecord *obj, s32 freeprop, s32 canregen);
void        objFreePermanently(ObjectRecord *obj, bool freeprop);
void        modelResetDeformedVertices(Model *model);
void        objsFreeDeformedVertices(void);

void        glassCreateShards(coord3d *pos, coord3d *xaxis, coord3d *yaxis, coord3d *zaxis, f32 xmin, f32 xmax, f32 ymin, f32 ymax);
f32         bondviewGetPlayerPitchRadians(void);

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
