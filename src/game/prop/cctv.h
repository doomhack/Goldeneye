#ifndef _CCTV_H_
#define _CCTV_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

extern f32 g_CctvAlarmTimeScale;
extern f32 g_CctvDamageScale;

bool objMayWatchPlayer(s32 player);

void cctvTick(const PropRecord *prop);
void cctvGetModelMatrices(const PropRecord *prop, const Model *model, const Mtxf *objmtx, Mtxf *mtxs);

#endif
