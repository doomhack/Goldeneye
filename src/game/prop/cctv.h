#ifndef _CCTV_H_
#define _CCTV_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

extern f32 g_CctvAlarmTimeScale;
extern f32 g_CctvDamageScale;

bool objMayWatchPlayer(s32 player);

void cctvTick(PropRecord *prop);
void cctvGetModelMatrices(PropRecord *prop, Model *model, Mtxf *objmtx, Mtxf *mtxs);

#endif
