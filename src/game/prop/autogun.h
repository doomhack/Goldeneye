#ifndef _AUTOGUN_H_
#define _AUTOGUN_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

extern f32 g_AutogunPendingDamageTick;
extern f32 g_AutogunDamageScalar;
extern f32 g_AutogunDamageScale;

void autogunTick(const PropRecord *prop);
void autogunTickFire(const PropRecord *prop);
void autogunGetModelMatrices(const PropRecord *prop, const Model *model, const Mtxf *objmtx, Mtxf *mtxs);

#endif
