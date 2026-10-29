#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <ultra64.h>
#include "game/math/mathtypes.h"

/* 4x4 float matrices and their conversion to fixed point; see matrix.c for the conventions. */

void mtx4LoadIdentity(Mtxf *matrix);
void mtx4Copy(const Mtxf *src, Mtxf *dst);

void mtx4Mult(const Mtxf *lhs, const Mtxf *rhs, Mtxf *result);
void mtx4MultInPlace(const Mtxf *lhs, Mtxf *rhs);
void mtx4MultAffine(const Mtxf *lhs, const Mtxf *rhs, Mtxf *result);
void mtx4MultAffineInPlace(const Mtxf *lhs, Mtxf *rhs);

void mtx4RotateVec(const Mtxf *matrix, const coord3d *vector, coord3d *result);
void mtx4RotateVecInPlace(const Mtxf *matrix, coord3d *vector);
void mtx4TransformVec(const Mtxf *matrix, const coord3d *point, coord3d *result);
void mtx4TransformVecInPlace(const Mtxf *matrix, coord3d *point);

void mtx4LoadXRotation(f32 angle, Mtxf *matrix);
void mtx4LoadYRotation(f32 angle, Mtxf *matrix);
void mtx4LoadZRotation(f32 angle, Mtxf *matrix);
void mtx4LoadYRotationAndTranslation(const coord3d *position, f32 angle, Mtxf *matrix);
void mtx4LoadRotation(const coord3d *angles, Mtxf *matrix);
void mtx4LoadRotationAndTranslation(const coord3d *position, const coord3d *angles, Mtxf *matrix);
void mtx4GetRotation(const Mtxf *matrix, coord3d *angles);
void mtx4LoadTranslation(const coord3d *position, Mtxf *matrix);
void mtx4SetTranslation(const coord3d *position, Mtxf *matrix);

void mtx4ScaleRow0(f32 scale, Mtxf *matrix);
void mtx4ScaleRow1(f32 scale, Mtxf *matrix);
void mtx4ScaleRow2(f32 scale, Mtxf *matrix);
void mtx4ScaleRow2AndW(f32 scale, Mtxf *matrix);
void mtx4ScaleRows012(f32 scale, Mtxf *matrix);
void mtx4Scale3x3(f32 scale, Mtxf *matrix);
void mtx4ScaleColumn2(f32 scale, Mtxf *matrix);
void mtx4ScaleColumns012(f32 scale, Mtxf *matrix);

void mtx4LoadLookDir(Mtxf *matrix, f32 eyeX, f32 eyeY, f32 eyeZ, f32 forwardX, f32 forwardY, f32 forwardZ, f32 upX, f32 upY, f32 upZ);
void mtx4LoadLookAt(Mtxf *matrix, f32 eyeX, f32 eyeY, f32 eyeZ, f32 targetX, f32 targetY, f32 targetZ, f32 upX, f32 upY, f32 upZ);
void mtx4LoadFacingDir(Mtxf *matrix, f32 posX, f32 posY, f32 posZ, f32 dirX, f32 dirY, f32 dirZ, f32 upX, f32 upY, f32 upZ);
void mtx4LoadFacing(Mtxf *matrix, f32 posX, f32 posY, f32 posZ, f32 targetX, f32 targetY, f32 targetZ, f32 upX, f32 upY, f32 upZ);
void mtx4LoadAlign(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z);
void mtx4LoadAlignInverse(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z);

void mtx4TransposeRotation(const Mtxf *matrix, Mtxf *result);
void mtx4InvertRigid(const Mtxf *matrix, Mtxf *result);
void mtx4InvertAffine(const Mtxf *matrix, Mtxf *result);

void mtx3Mult(const Mtx3f *lhs, const Mtx3f *rhs, Mtx3f *result);
void mtx3MultInPlace(const Mtx3f *lhs, Mtx3f *rhs);
void mtx3ToMtx4(const Mtx3f *src, Mtxf *dst);
void mtx4ToMtx3(const Mtxf *src, Mtx3f *dst);

void mtxSetWorldScale(f32 scale);
void mtxPushUnitWorldScale(void);
void mtxPopWorldScale(void);
void mtxF2L(const Mtxf *src, Mtxf *dst);
void mtxL2F(const Mtxf *src, Mtxf *dst);
u16 mtxCalcPerspNorm(f32 near, f32 far);

#endif
