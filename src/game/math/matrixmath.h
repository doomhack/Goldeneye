#ifndef MATRIXMATH_H
#define MATRIXMATH_H

#include <ultra64.h>
#include <bondtypes.h>

void mtx4RotateVecInPlace(Mtxf *matrix, struct coord3d *vector);
void mtx4LoadLookAt(Mtxf *arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, f32 arg8, f32 arg9);

void mtx4Mult(Mtxf *lhs, Mtxf *rhs, Mtxf *result);
void mtx4SetTranslation(struct coord3d *position, Mtxf *matrix);
void mtx4LoadLookDir(Mtxf *arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, f32 arg8, f32 arg9);
u32 mtxCalcPerspNorm(f32 arg0, f32 arg1);
void mtx4Adjugate(Mtxf *arg0, Mtxf *arg1);

f32  mtx4Determinant(Mtxf *matrix);
f32  mtx3Determinant(f32 a, f32 d, f32 g, f32 b, f32 e, f32 h, f32 c, f32 f, f32 i);
f32  mtx2Determinant(f32 a, f32 c, f32 b, f32 d);

void mtxSetWorldScale(f32 arg0);
void mtx4Copy(Mtxf *src, Mtxf *dst);
void mtx4MultAffine(Mtxf *lhs, Mtxf *rhs, Mtxf *result);

void mtxF2L(f32 mf[4][4], s32 ms[4][4]);
void mtx4MultAffineInPlace(Mtxf *lhs, Mtxf *rhs);
void mtx4TransformVecInPlace(Mtxf *matrix, struct coord3d *vector);
void mtx4LoadIdentity(Mtxf *matrix);
void mtx4LoadXRotation(f32 angle, Mtxf *matrix);
void mtx4LoadYRotation(f32 angle, Mtxf *matrix);
void mtx4ScaleRows012(f32 scalar, f32 *matrix);
void mtx4LoadFacing(Mtxf *arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, f32 arg8, f32 arg9);
void mtx4LoadZRotation(f32 angle, Mtxf *matrix);
void mtx4MultInPlace(Mtxf *lhs, Mtxf *rhs);
void mtx4LoadTranslation(struct coord3d *position, Mtxf *matrix);
void mtx4LoadYRotationWithTranslation(f32 *position, f32 angle, Mtxf *matrix);
void mtx4Scale3x3(f32 scalar, f32 *matrix);
void mtx4LoadRotation(struct coord3d *angles, Mtxf *matrix);
void mtx4GetRotation(Mtxf *matrix, struct coord3d *angles);
void mtx4LoadFacingDir(Mtxf *arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, f32 arg8, f32 arg9);
void mtx4InvertRigid(Mtxf *arg0, Mtxf *arg1);
void mtx4ScaleRow0(f32 scalar, f32 *matrix);
void mtx4ScaleRow1(f32 scalar, f32 *matrix);
void mtx4ScaleRow2AndW(f32 scalar, f32 *matrix);
void mtx4ScaleRow2(f32 scalar, f32 *matrix);
void mtx4LoadRotationAndTranslation(struct coord3d *position, struct coord3d * rotation, Mtxf *matrix);
void mtx4ScaleColumns012(f32 scalar, f32 *matrix);
void mtx4LoadAlignViaGu(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z);
void mtx4LoadAlign(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z);
void mtx4TransposeRotation(Mtxf *rotation, Mtxf *transpose);
void mtx4TransformVec(Mtxf *matrix, struct coord3d *vector, struct coord3d *result);
void mtx4ScaleColumn2(f32 scalar, f32 *matrix);

// tenative guess
void mtxL2F(Mtxf *arg0, Mtxf *arg1);
void mtx4ToMtx3(f32 src[][4], f32 dst[3][3]);
void mtx3MultInPlace(f32 src[3][3], f32 dst[3][3]);
void mtx3Mult(f32 lhs[3][3], f32 rhs[3][3], f32 result[3][3]);

/* matrixmath_misc.h */

void interpLerp3(vec3d *x, vec3d *y, f32 scaler, vec3d *result);
void interpCardinal3(coord3d *prev, coord3d *start, coord3d *end, coord3d *next, f32 fraction, f32 tangentScale, coord3d *result);
void interpCatmullRom3(coord3d *p0, coord3d *p1, coord3d *p2, coord3d *p3, f32 fraction, coord3d *result);
#endif
