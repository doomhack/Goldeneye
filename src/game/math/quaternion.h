#ifndef _QUATERNION_H_
#define _QUATERNION_H_

#include <ultra64.h>

typedef u16 vec3u[3];
typedef f32 vec3f[3];
typedef f32 mat44f[4][4];
typedef f32 quatf[4]; // w, x, y, z

void quatFromAngles(vec3f angles, quatf q);
void quatToMtx4(quatf q, mat44f matrix);
void quatToMtx4WithTranslation(vec3f position, quatf rotation, mat44f matrix);
void quatSlerp(quatf q1, quatf q2, f32 t, quatf result);
void quatSlerpFromIdentity(quatf q, f32 t, quatf result);
void quatMakeShortestPath(quatf q1, quatf q2);
void quatMult(quatf lhs, quatf rhs, quatf result);
void quatLog(quatf q, quatf result);
void quatExp(quatf q, quatf result);
void quatSquadControlPoint(quatf prev, quatf q, quatf next, quatf result);
void quatSquad(quatf q1, quatf q2, quatf q3, quatf q4, f32 t, quatf result);
void quatSpline(quatf q1, quatf q2, quatf q3, quatf q4, f32 t, quatf result);

#endif
