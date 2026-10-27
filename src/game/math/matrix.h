#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <ultra64.h>
#include <bondtypes.h>

/* 4x4 float matrices and conversion to fixed point; see matrix.c for the conventions. */

#ifdef VERSION_EU
void mtx3Copy(f32 src[3][3], f32 dst[3][3]);
void mtx3MultInPlace(f32 lhs[3][3], f32 rhs[3][3]);
void mtx3Mult(f32 lhs[3][3], f32 rhs[3][3], f32 result[3][3]);
void mtx3ToMtx4(f32 src[][3], f32 dst[4][4]);
void mtx4ToMtx3(f32 src[][4], f32 dst[3][3]);
#endif

void mtx4LoadIdentity(Mtxf *matrix);
void mtx4Copy(Mtxf *src, Mtxf *dst);
void mtx4MultInPlace(Mtxf *lhs, Mtxf *rhs);
void mtx4MultAffineInPlace(Mtxf *lhs, Mtxf *rhs);
void mtx4Mult(Mtxf *lhs, Mtxf *rhs, Mtxf *result);
void mtx4MultAffine(Mtxf *lhs, Mtxf *rhs, Mtxf *result);
void mtx4RotateVec(Mtxf *matrix, struct coord3d *vector, struct coord3d *result);
void mtx4RotateVecInPlace(Mtxf *matrix, struct coord3d *vector);
void mtx4TransformVec(Mtxf *matrix, struct coord3d *vector, struct coord3d *result);
void mtx4TransformVecInPlace(Mtxf *matrix, struct coord3d *vector);
void mtx4LoadYRotationWithTranslation(f32 *position, f32 angle, Mtxf *matrix);
void mtx4LoadXRotation(f32 angle, Mtxf *matrix);
void mtx4LoadYRotation(f32 angle, Mtxf *matrix);
void mtx4LoadZRotation(f32 angle, Mtxf *matrix);
void mtx4LoadRotation(struct coord3d *angles, Mtxf *matrix);
void mtx4GetRotation(Mtxf *matrix, struct coord3d *angles);
void mtx4LoadRotationAndTranslation(struct coord3d *position, struct coord3d * rotation, Mtxf *matrix);
void mtx4LoadTranslation(struct coord3d * position, Mtxf *matrix);
void mtx4SetTranslation(struct coord3d *position, Mtxf *matrix);
void mtx4ScaleRow0(f32 scalar, f32 *matrix);
void mtx4ScaleRow1(f32 scalar, f32 *matrix);
void mtx4ScaleRow2AndW(f32 scalar, f32 *matrix);
void mtx4ScaleRow2(f32 scalar, f32 *matrix);
void mtx4ScaleRows012(f32 scalar, f32 *matrix);
void mtx4Scale3x3(f32 scalar, f32 *matrix);
void mtx4ScaleColumn2(f32 scalar, f32 *matrix);
void mtx4ScaleColumns012(f32 scalar, f32 *matrix);
void mtxSetWorldScale(f32 scale);
void mtxPushUnitWorldScale(void);
void mtxPopWorldScale(void);
void mtxF2L(f32 mf[4][4], s32 ms[4][4]);
void mtxL2F(Mtxf *src, Mtxf *dst);
void mtxUnpackFixed(Mtx *src, Mtx *dst);
void mtx4LoadLookDir(Mtxf *matrix, f32 eyeX, f32 eyeY, f32 eyeZ, f32 forwardX, f32 forwardY, f32 forwardZ, f32 upX, f32 upY, f32 upZ);
void mtx4LoadLookAt(Mtxf *matrix, f32 eyeX, f32 eyeY, f32 eyeZ, f32 targetX, f32 targetY, f32 targetZ, f32 upX, f32 upY, f32 upZ);
void mtx4LoadFacingDir(Mtxf *matrix, f32 posX, f32 posY, f32 posZ, f32 basisX, f32 basisY, f32 basisZ, f32 upX, f32 upY, f32 upZ);
void mtx4LoadFacing(Mtxf *matrix, f32 posX, f32 posY, f32 posZ, f32 targetX, f32 targetY, f32 targetZ, f32 upX, f32 upY, f32 upZ);
u32 mtxCalcPerspNorm(f32 near, f32 far);
void mtx4LoadAlign(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z);
void mtx4LoadAlignViaGu(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z);
void mtx4TransposeRotation(Mtxf *rotation, Mtxf *transpose);
void mtx4InvertRigid(Mtxf *matrix, Mtxf *result);
void mtx4InvertAffine(Mtxf *matrix, Mtxf *result);

#endif
