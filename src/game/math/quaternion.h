#ifndef _QUATERNION_H_
#define _QUATERNION_H_

#include <ultra64.h>
#include "game/math/mathtypes.h"

/* Quaternions, for blending rotations in animation; see quaternion.c. */

void quatFromAngles(const coord3d *angles, quatf *q);
void quatToMtx4(const quatf *q, Mtxf *matrix);
void quatToMtx4WithTranslation(const coord3d *position, const quatf *q, Mtxf *matrix);
void quatMakeShortestPath(const quatf *from, quatf *to);
void quatSlerp(const quatf *from, const quatf *to, f32 t, quatf *result);
void quatSlerpFromIdentity(const quatf *q, f32 t, quatf *result);
void quatSpline(const quatf *q0, const quatf *q1, const quatf *q2, const quatf *q3, f32 t, quatf *result);

#endif
