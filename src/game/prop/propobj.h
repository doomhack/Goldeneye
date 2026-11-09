#ifndef _CHROBJHANDLER_H_
#define _CHROBJHANDLER_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <snd.h>   

#define CHROBJ_TIMETOREGEN TICKS_PER_SECOND
#define CHROBJ_TIMETOREGEN_F ((f32)TICKS_PER_SECOND)
#define PLASTIQUE_EXPLOSION_DELAY_TICKS TICKS(120)

extern f32 g_SoloAmmoMultiplier;

extern struct LinkRecord *g_LevelLoadPropSwitch;
extern LockDoorRecord *g_LevelLoadPropLockDoor;
extern SafeObjectRecord *g_LevelLoadPropSafeItem;

extern ExplosionDetailsRecord object_explosion_details[];


PropRecord *objInit(ObjectRecord *obj, ModelFileHeader *header, PropRecord *prop, Model *model);
PropRecord *objInitWithModelDef(ObjectRecord *obj, ModelFileHeader *header);
PropRecord *objInitWithAutoModel(ObjectRecord *obj);
void        objSetPosition(ObjectRecord *obj, const coord3d *pos, const Mtxf *matrix, StandTile *stan);
void        objPlaceAtPad(ObjectRecord *obj, const coord3d *pos, const Mtxf *matrix, StandTile *stan, const coord3d *centre);
void        objPlaceAtPadSideways(ObjectRecord *obj, const coord3d *pos, const Mtxf *padmtx, StandTile *stan, const coord3d *centre);
void        objFree(ObjectRecord *obj, s32 freeprop, s32 canregen);
void        objFreePermanently(ObjectRecord *obj, bool freeprop);
void        modelResetDeformedVertices(const Model *model);
void        objsFreeDeformedVertices(void);

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
