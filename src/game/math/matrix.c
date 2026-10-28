/**
 * 4x4 float matrices (Mtxf) and their conversion to the N64's fixed-point Mtx.
 *
 * Matrices are stored the way the N64 and libultra expect: m[row][column], with points
 * as row vectors on the left (p' = p * M). Rows 0-2 are the transformed x, y and z axes
 * and row 3 is the translation, so m[3][0..2] is the position.
 *
 * mtx4Mult(a, b, out) makes `out` apply b first and then a. In column-vector notation that
 * is out = a * b.
 */
#include "game/math/matrix.h"
#include "game/math/trig.h"
#include <PR/gu.h>
#include <math.h>

/* Below this, mtx4GetRotation treats the matrix as pitched straight up or down. */
#define EPSILON FLT_EPSILON * 16

/* The world scale while mtxPushUnitWorldScale has it set to 1. */
f32 m_savedWorldScale;

/*
 * Scales applied when converting to fixed point (mtxF2L) and back (mtxL2F). [0] is for
 * columns 0-2 and is 65536 times the level's world scale, see mtxSetWorldScale. [1] is
 * for column 3 and is always 65536.
 */
f32 g_MtxFixedScale[2] = {M_U16_MAX_VALUE_F, M_U16_MAX_VALUE_F};

/* PAL only: 3x3 matrix helpers. */
#ifdef VERSION_EU
void mtx3Copy(f32 src[3][3], f32 dst[3][3])
{
    s32 i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            dst[i][j] = src[i][j];
        }
    }
}
#endif


#ifdef VERSION_EU
void mtx3MultInPlace(f32 lhs[3][3], f32 rhs[3][3])
{
    f32 result[3][3];
    mtx3Mult(lhs, rhs, result);
    mtx3Copy(result, rhs);
}
#endif


#ifdef VERSION_EU
void mtx3Mult(f32 lhs[3][3], f32 rhs[3][3], f32 result[3][3])
{
    s32 i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            result[j][i] = (lhs[0][i] * rhs[j][0]) + (lhs[1][i] * rhs[j][1]) + (lhs[2][i] * rhs[j][2]);
        }
    }
}
#endif


#ifdef VERSION_EU
void mtx3ToMtx4(f32 src[][3], f32 dst[4][4])
{
    s32 i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            dst[i][j] = src[i][j];
        }
    }

    dst[0][3] = 0.0;
    dst[1][3] = 0.0;
    dst[2][3] = 0.0;
    dst[3][0] = 0.0;
    dst[3][1] = 0.0;
    dst[3][2] = 0.0;
    dst[3][3] = 1.0;
}
#endif

#ifdef VERSION_EU
void mtx4ToMtx3(f32 src[][4], f32 dst[3][3])
{
    s32 i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            dst[i][j] = src[i][j];
        }
    }
}
#endif

void mtx4LoadIdentity(Mtxf *matrix)
{
    matrix->m[0][0] = 1.0f;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = 0.0f;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = 1.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}

void mtx4Copy(Mtxf *src, Mtxf *dst)
{
    s32 i, j;
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            dst->m[i][j] = src->m[i][j];
        }
    }
}

/** rhs = lhs * rhs, see mtx4Mult. */
void mtx4MultInPlace(Mtxf *lhs, Mtxf *rhs)
{
    Mtxf result;
    mtx4Mult(lhs, rhs, &result);
    mtx4Copy(&result, rhs);
}

/** rhs = lhs * rhs for affine matrices, see mtx4MultAffine. */
void mtx4MultAffineInPlace(Mtxf *lhs, Mtxf *rhs)
{
    Mtxf result;
    mtx4MultAffine(lhs, rhs, &result);
    mtx4Copy(&result, rhs);
}

/** result = lhs * rhs: transforms by rhs, then by lhs. result must not be lhs or rhs. */
void mtx4Mult(Mtxf *lhs, Mtxf *rhs, Mtxf *result)
{
    s32 i, j;
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            result->m[j][i] = lhs->m[0][i] * rhs->m[j][0] + lhs->m[1][i] * rhs->m[j][1] + lhs->m[2][i] * rhs->m[j][2] + lhs->m[3][i] * rhs->m[j][3];
        }
    }
}

/**
 * mtx4Mult for affine matrices (no perspective): column 3 of both is assumed to be
 * (0, 0, 0, 1) and is set to that in the result.
 */
void mtx4MultAffine(Mtxf *lhs, Mtxf *rhs, Mtxf *result)
{
    s32 i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 4; j++)
        {
            result->m[j][i] = (lhs->m[0][i] * rhs->m[j][0]) + (lhs->m[1][i] * rhs->m[j][1]) + (lhs->m[2][i] * rhs->m[j][2]);
            if (j == 3)
            {
                result->m[j][i] += lhs->m[3][i];
            }
        }
    }

    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
    result->m[3][3] = 1.0f;
}

/** Applies only the rotation and scale part of a matrix to a vector. */
void mtx4RotateVec(Mtxf *matrix, struct coord3d *vector, struct coord3d *result)
{
    s32 i;
    for (i = 0; i < 3; i++)
    {
        result->f[i] = matrix->m[0][i] * vector->f[0] + matrix->m[1][i] * vector->f[1] + matrix->m[2][i] * vector->f[2];
    }
}

void mtx4RotateVecInPlace(Mtxf *matrix, struct coord3d *vector)
{
    struct coord3d result;
    mtx4RotateVec(matrix, vector, &result);
    vector->f[0] = result.f[0];
    vector->f[1] = result.f[1];
    vector->f[2] = result.f[2];
}

/** Applies a matrix, translation included, to a point. */
void mtx4TransformVec(Mtxf *matrix, struct coord3d *vector, struct coord3d *result)
{
    mtx4RotateVec(matrix, vector, result);
    result->f[0] += matrix->m[3][0];
    result->f[1] += matrix->m[3][1];
    result->f[2] += matrix->m[3][2];
}

void mtx4TransformVecInPlace(Mtxf *matrix, struct coord3d *vector)
{
    mtx4RotateVecInPlace(matrix, vector);
    vector->f[0] += matrix->m[3][0];
    vector->f[1] += matrix->m[3][1];
    vector->f[2] += matrix->m[3][2];
}

/** A rotation by `angle` radians around the y axis, then a move to `position`. */
void mtx4LoadYRotationWithTranslation(f32 *position, f32 angle, Mtxf *matrix)
{
    f32 cosine      = cosf(angle);
    f32 sine        = sinf(angle);
    matrix->m[0][0] = cosine;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = -sine;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = sine;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = cosine;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = position[0];
    matrix->m[3][1] = position[1];
    matrix->m[3][2] = position[2];
    matrix->m[3][3] = 1.0f;
}

/** Rotations by `angle` radians around one axis. */
void mtx4LoadXRotation(f32 angle, Mtxf *matrix)
{
    f32 cosine      = cosf(angle);
    f32 sine        = sinf(angle);
    matrix->m[0][0] = 1.0f;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = cosine;
    matrix->m[1][2] = sine;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = 0.0f;
    matrix->m[2][1] = -sine;
    matrix->m[2][2] = cosine;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}

void mtx4LoadYRotation(f32 angle, Mtxf *matrix)
{
    f32 cosine      = cosf(angle);
    f32 sine        = sinf(angle);
    matrix->m[0][0] = cosine;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = -sine;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = sine;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = cosine;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}

void mtx4LoadZRotation(f32 angle, Mtxf *matrix)
{
    f32 cosine      = cosf(angle);
    f32 sine        = sinf(angle);
    matrix->m[0][0] = cosine;
    matrix->m[0][1] = sine;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = -sine;
    matrix->m[1][1] = cosine;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = 0.0f;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = 1.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}

/** A rotation by the Euler angles in `angles` (radians), applied x first, then y, then z. */
void mtx4LoadRotation(struct coord3d *angles, Mtxf *matrix)
{
    f32 cosX       = cosf(angles->f[0]);
    f32 sinX       = sinf(angles->f[0]);
    f32 cosY       = cosf(angles->f[1]);
    f32 sinY       = sinf(angles->f[1]);
    f32 cosZ       = cosf(angles->f[2]);
    f32 sinZ       = sinf(angles->f[2]);
    f32 sinXSinZ = sinX * sinZ;
    f32 cosXSinZ = cosX * sinZ;
    f32 sinXCosZ = sinX * cosZ;
    f32 cosXCosZ = cosX * cosZ;
    matrix->m[0][0] = (cosY * cosZ);
    matrix->m[0][1] = (cosY * sinZ);
    matrix->m[0][2] = -sinY;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = ((sinXCosZ * sinY) - cosXSinZ);
    matrix->m[1][1] = ((sinXSinZ * sinY) + cosXCosZ);
    matrix->m[1][2] = sinX * cosY;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = ((cosXCosZ * sinY) + sinXSinZ);
    matrix->m[2][1] = ((cosXSinZ * sinY) - sinXCosZ);
    matrix->m[2][2] = cosX * cosY;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}

// https://stackoverflow.com/a/15029416
/** The inverse of mtx4LoadRotation: the Euler angles of a rotation matrix. */
void mtx4GetRotation(Mtxf *matrix, struct coord3d *angles)
{
    f32 norm;
    f32 sinXCosY = matrix->m[1][2];
    f32 cosXCosY = matrix->m[2][2];
    norm            = sqrtf(SQR(sinXCosY) + SQR(cosXCosY));
    if (EPSILON < norm)
    {
        angles->f[0] = mathAtan2(matrix->m[1][2], matrix->m[2][2]);
        angles->f[1] = mathAtan2(-matrix->m[0][2], norm);
        angles->f[2] = mathAtan2(matrix->m[0][1], matrix->m[0][0]);
    }
    else
    {
        angles->f[0] = 0.0f;
        angles->f[1] = mathAtan2(-matrix->m[0][2], norm);
        angles->f[2] = mathAtan2(-matrix->m[1][0], matrix->m[1][1]);
    }
}

void mtx4LoadRotationAndTranslation(struct coord3d *position, struct coord3d * rotation, Mtxf *matrix)
{
    mtx4LoadRotation(rotation, matrix);
    mtx4SetTranslation(position, matrix);
}

/** An identity matrix that moves to `position`. */
void mtx4LoadTranslation(struct coord3d * position, Mtxf *matrix)
{
    mtx4LoadIdentity(matrix);
    mtx4SetTranslation(position, matrix);
}

/** Replaces only the translation. */
void mtx4SetTranslation(struct coord3d *position, Mtxf *matrix)
{
    matrix->m[3][0] = position->f[0];
    matrix->m[3][1] = position->f[1];
    matrix->m[3][2] = position->f[2];
}

/*
 * Scaling parts of a matrix, treated as 16 floats. Row n is m[n], so rows 0-2 are the
 * x, y and z axes. Scaling rows 0-2 scales the object before it is placed, scaling
 * columns 0-2 scales the result, translation included.
 */
void mtx4ScaleRow0(f32 scalar, Mtxf *matrix)
{
    matrix->m[0][0] *= scalar;
    matrix->m[0][1] *= scalar;
    matrix->m[0][2] *= scalar;
}

void mtx4ScaleRow1(f32 scalar, Mtxf *matrix)
{
    matrix->m[1][0] *= scalar;
    matrix->m[1][1] *= scalar;
    matrix->m[1][2] *= scalar;
}

void mtx4ScaleRow2AndW(f32 scalar, Mtxf *matrix)
{
    matrix->m[2][0] *= scalar;
    matrix->m[2][1] *= scalar;
    matrix->m[2][2] *= scalar;
    matrix->m[2][3] *= scalar;
}

void mtx4ScaleRow2(f32 scalar, Mtxf *matrix)
{
    matrix->m[2][0] *= scalar;
    matrix->m[2][1] *= scalar;
    matrix->m[2][2] *= scalar;
}

void mtx4ScaleRows012(f32 scalar, Mtxf *matrix)
{
    s32 i;

    for (i = 0; i < 4; i++)
    {
        matrix->m[0][i] *= scalar;
        matrix->m[1][i] *= scalar;
        matrix->m[2][i] *= scalar;
    }
}

void mtx4Scale3x3(f32 scalar, f32 *matrix)
{
    matrix[0] *= scalar;
    matrix[1] *= scalar;
    matrix[2] *= scalar;
    matrix[4] *= scalar;
    matrix[5] *= scalar;
    matrix[6] *= scalar;
    matrix[8] *= scalar;
    matrix[9] *= scalar;
    matrix[10] *= scalar;
}

void mtx4ScaleColumn2(f32 scalar, f32 *matrix)
{
    matrix[2] *= scalar;
    matrix[6] *= scalar;
    matrix[10] *= scalar;
    matrix[14] *= scalar;
}

void mtx4ScaleColumns012(f32 scalar, f32 *matrix)
{
    matrix[0] *= scalar;
    matrix[4] *= scalar;
    matrix[8] *= scalar;
    matrix[12] *= scalar;
    matrix[1] *= scalar;
    matrix[5] *= scalar;
    matrix[9] *= scalar;
    matrix[13] *= scalar;
    matrix[2] *= scalar;
    matrix[6] *= scalar;
    matrix[10] *= scalar;
    matrix[14] *= scalar;
}

/*
 * The world scale. Each level has a scale (its "visibility" setting in the level table)
 * that mtxF2L applies when it makes the fixed-point matrices the RSP uses, presumably so
 * that large levels still fit the s15.16 range. The first-person gun code pushes a scale
 * of 1 while it builds its matrices.
 */
void mtxSetWorldScale(f32 scale)
{
    g_MtxFixedScale[0] = (M_U16_MAX_VALUE_F * scale);
}

void mtxPushUnitWorldScale(void)
{
    m_savedWorldScale = g_MtxFixedScale[0];
    g_MtxFixedScale[0]         = M_U16_MAX_VALUE_F;
}

void mtxPopWorldScale(void)
{
    g_MtxFixedScale[0] = m_savedWorldScale;
}

/**
 * Converts a float matrix to the N64's fixed-point Mtx (like libultra's guMtxF2L), applying
 * the world scale to columns 0-2. The Mtx keeps the integer halves of all 16 values in its
 * first 8 words and the fraction halves in the last 8. dstmtx is Mtx storage, typed Mtxf
 * like mtxL2F's source because the game often converts a Mtxf array in place.
 */
void mtxF2L(Mtxf *srcmtx, Mtxf *dstmtx)
{
    f32 *src = (f32 *)srcmtx;
    s32 *dst = (s32 *)dstmtx;
    s32 i;

    for (i = 0; i < 8; i += 1)
    {
        s32 e1, e2;

        e1 = (s32)(src[((i + 0) << 1) + 0] * g_MtxFixedScale[0]);
        e2 = (s32)(src[((i + 0) << 1) + 1] * g_MtxFixedScale[(i + 0) & 1]);
        dst[(i + 0) + 0] = (e1 & 0xffff0000) | (((u32)e2) >> 16);
        dst[(i + 0) + 8] = (e1 << 16) | (e2 & 0xffff);
    }
}

/** The inverse of mtxF2L, dividing the world scale back out. */
void mtxL2F(Mtxf *src, Mtxf *dst) {
    u32 *srcwords = (u32 *) src;
    f32 *dstfloats = (f32 *) dst;
    s32 i;

    for (i = 0; i < 8; i++) {
        u32 word1 = srcwords[i + 0];
        u32 word2 = srcwords[i + 8];

        dstfloats[(i << 1) + 0] = (s32) ((word1 & 0xffff0000) | (word2 >> 16)) / g_MtxFixedScale[0];
        dstfloats[(i << 1) + 1] = (s32) ((word1 << 16) | (word2 & 0xffff)) / g_MtxFixedScale[i & 1];
    }
}


/** Converts an N64 Mtx into 16 s15.16 values, one per word. */
void mtxUnpackFixed(Mtx *src, Mtx *dst) {
    u32 *srcwords = (u32 *) src;
    u32 *dstwords = (u32 *) dst;
    s32 i;

    for (i = 0; i < 8; i++) {
        u32 word1 = srcwords[i + 0];
        u32 word2 = srcwords[i + 8];

        dstwords[(i << 1) + 0] = (word1 & 0xffff0000) | (word2 >> 16);
        dstwords[(i << 1) + 1] = (word1 << 16) | (word2 & 0xffff);
    }
}

/**
 * A camera (view) matrix for an eye at `eye` looking along `forward`, with `up` roughly up.
 * Like libultra's guLookAtF, but taking a direction instead of a target.
 */
void mtx4LoadLookDir(Mtxf *matrix, f32 eyeX, f32 eyeY, f32 eyeZ, f32 forwardX, f32 forwardY, f32 forwardZ, f32 upX, f32 upY, f32 upZ) {
    f32 rightX;
    f32 rightY;
    f32 normRight;
    f32 normUp;
    f32 rightZ;

    // Normalize forward vector
    f32 normForward = -1.0f / sqrtf((forwardX * forwardX) + (forwardY * forwardY) + (forwardZ * forwardZ));
    forwardX *= normForward;
    forwardY *= normForward;
    forwardZ *= normForward;

    // Compute right vector (cross product of up and forward)
    rightX = (upY * forwardZ) - (upZ * forwardY);
    rightY = (upZ * forwardX) - (upX * forwardZ);
    rightZ = (upX * forwardY) - (upY * forwardX);

    // Normalize right vector
    normRight = 1.0f / sqrtf((rightX * rightX) + (rightY * rightY) + (rightZ * rightZ));
    rightX *= normRight;
    rightY *= normRight;
    rightZ *= normRight;

    // Recompute up vector (cross product of forward and right)
    upX = (forwardY * rightZ) - (forwardZ * rightY);
    upY = (forwardZ * rightX) - (forwardX * rightZ);
    upZ = (forwardX * rightY) - (forwardY * rightX);

    // Normalize up vector
    normUp = 1.0f / sqrtf((upX * upX) + (upY * upY) + (upZ * upZ));
    upX *= normUp;
    upY *= normUp;
    upZ *= normUp;

    // Set matrix columns
    matrix->m[0][0] = rightX;
    matrix->m[1][0] = rightY;
    matrix->m[2][0] = rightZ;
    matrix->m[3][0] = -((eyeX * rightX) + (eyeY * rightY) + (eyeZ * rightZ));
    matrix->m[0][1] = upX;
    matrix->m[1][1] = upY;
    matrix->m[2][1] = upZ;
    matrix->m[3][1] = -((eyeX * upX) + (eyeY * upY) + (eyeZ * upZ));
    matrix->m[0][2] = forwardX;
    matrix->m[1][2] = forwardY;
    matrix->m[2][2] = forwardZ;
    matrix->m[3][2] = -((eyeX * forwardX) + (eyeY * forwardY) + (eyeZ * forwardZ));
    matrix->m[0][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][3] = 1.0f;
}

/** A camera (view) matrix for an eye at `eye` looking at `target`. */
void mtx4LoadLookAt(Mtxf *matrix, f32 eyeX, f32 eyeY, f32 eyeZ, f32 targetX, f32 targetY, f32 targetZ, f32 upX, f32 upY, f32 upZ) {
    mtx4LoadLookDir(matrix, eyeX, eyeY, eyeZ, targetX - eyeX, targetY - eyeY, targetZ - eyeZ, upX, upY, upZ);
}

/**
 * The inverse of mtx4LoadLookDir: places an object at `pos` with its z axis pointing
 * against `dir` and its y axis roughly along `up`.
 */
void mtx4LoadFacingDir(Mtxf *matrix, f32 posX, f32 posY, f32 posZ, f32 basisX, f32 basisY, f32 basisZ, f32 upX, f32 upY, f32 upZ) {
    f32 rightX;
    f32 rightY;
    f32 normRight;
    f32 normUp;
    f32 rightZ;
    f32 normBasis = -1.0f / sqrtf((basisX * basisX) + (basisY * basisY) + (basisZ * basisZ));
    basisX *= normBasis;
    basisY *= normBasis;
    basisZ *= normBasis;
    rightX = (upY * basisZ) - (upZ * basisY);
    rightY = (upZ * basisX) - (upX * basisZ);
    rightZ = (upX * basisY) - (upY * basisX);
    normRight = 1.0f / sqrtf((rightX * rightX) + (rightY * rightY) + (rightZ * rightZ));
    rightX *= normRight;
    rightY *= normRight;
    rightZ *= normRight;
    upX = (basisY * rightZ) - (basisZ * rightY);
    upY = (basisZ * rightX) - (basisX * rightZ);
    upZ = (basisX * rightY) - (basisY * rightX);
    normUp = 1.0f / sqrtf((upX * upX) + (upY * upY) + (upZ * upZ));
    upX *= normUp;
    upY *= normUp;
    upZ *= normUp;
    matrix->m[0][0] = rightX;
    matrix->m[1][0] = upX;
    matrix->m[2][0] = basisX;
    matrix->m[3][0] = posX;
    matrix->m[0][1] = rightY;
    matrix->m[1][1] = upY;
    matrix->m[2][1] = basisY;
    matrix->m[3][1] = posY;
    matrix->m[0][2] = rightZ;
    matrix->m[1][2] = upZ;
    matrix->m[2][2] = basisZ;
    matrix->m[3][2] = posZ;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][3] = 1.0f;
}

/** mtx4LoadFacingDir with a target point instead of a direction. */
void mtx4LoadFacing(Mtxf *matrix, f32 posX, f32 posY, f32 posZ, f32 targetX, f32 targetY, f32 targetZ, f32 upX, f32 upY, f32 upZ) {
    mtx4LoadFacingDir(matrix, posX, posY, posZ, targetX - posX, targetY - posY, targetZ - posZ, upX, upY, upZ);
}


/** The perspNorm value the RSP needs with a perspective matrix, like guPerspective's. */
u32 mtxCalcPerspNorm(f32 near, f32 far)
{
    f32 sum = near + far;
    u16 result;

    if (sum <= 2)
    {
        result = 0xffff;
    }
    else
    {
        result = 0x20000 / sum;

        if (result <= 0)
        {
            result = 1;
        }
    }

    return result;
}

/** Like libultra's guAlignF, with the angle in radians. */
void mtx4LoadAlign(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z) 
{
    f32 sine;
    f32 cosine;
    f32 norm;
    f32 invnorm;
    f32 cosX;
    f32 sinX;
    f32 cosZ;
    f32 sinZ;
    guNormalize(&x, &y, &z);
    sine = sinf(angle);
    cosine = cosf(angle);
    norm = sqrtf((x * x) + (z * z));
    if (norm != 0.0f) {
        cosX = x * cosine;
        sinX = x * sine;
        cosZ = z * cosine;
        sinZ = z * sine;
        invnorm = 1.0f / norm;
        matrix->m[0][0] = ((-cosZ - (y * sinX)) * invnorm);
        matrix->m[1][0] = (sine * norm);
        matrix->m[2][0] = ((cosX - (y * sinZ)) * invnorm);
        matrix->m[3][0] = 0.0f;
        matrix->m[0][1] = ((sinZ - (y * cosX)) * invnorm);
        matrix->m[1][1] = (cosine * norm);
        matrix->m[2][1] = ((-sinX - (y * cosZ)) * invnorm);
        matrix->m[3][1] = 0.0f;
        matrix->m[0][2] = -x;
        matrix->m[1][2] = -y;
        matrix->m[2][2] = -z;
        matrix->m[3][2] = 0.0f;
        matrix->m[0][3] = 0.0f;
        matrix->m[1][3] = 0.0f;
        matrix->m[2][3] = 0.0f;
        matrix->m[3][3] = 1.0f;
        return;
    }
    mtx4LoadIdentity(matrix);
}


/** The same through libultra's guAlignF. */
void mtx4LoadAlignViaGu(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z) 
{
    angle = RadToDeg(angle);
    guAlignF(matrix->m, angle, x, y, z);
}

/** The inverse of a pure rotation matrix (its transpose), dropping any translation. */
void mtx4TransposeRotation(Mtxf *rotation, Mtxf *transpose) 
{
    transpose->m[0][0] = rotation->m[0][0];
    transpose->m[0][1] = rotation->m[1][0];
    transpose->m[0][2] = rotation->m[2][0];
    transpose->m[1][0] = rotation->m[0][1];
    transpose->m[1][1] = rotation->m[1][1];
    transpose->m[1][2] = rotation->m[2][1];
    transpose->m[2][0] = rotation->m[0][2];
    transpose->m[2][1] = rotation->m[1][2];
    transpose->m[2][2] = rotation->m[2][2];
    transpose->m[3][0] = 0.0f;
    transpose->m[3][1] = 0.0f;
    transpose->m[3][2] = 0.0f;
    transpose->m[0][3] = 0.0f;
    transpose->m[1][3] = 0.0f;
    transpose->m[2][3] = 0.0f;
    transpose->m[3][3] = 1.0f;
}

/** The inverse of a rotation with a uniform scale and a translation. */
void mtx4InvertRigid(Mtxf *matrix, Mtxf *result) {
    f32 norm = (matrix->m[0][0] * matrix->m[0][0]) +
               (matrix->m[1][0] * matrix->m[1][0]) +
               (matrix->m[2][0] * matrix->m[2][0]);
    norm = 1.0f / norm;
    result->m[0][0] = (matrix->m[0][0] * norm);
    result->m[0][1] = (matrix->m[1][0] * norm);
    result->m[0][2] = (matrix->m[2][0] * norm);
    result->m[1][0] = (matrix->m[0][1] * norm);
    result->m[1][1] = (matrix->m[1][1] * norm);
    result->m[1][2] = (matrix->m[2][1] * norm);
    result->m[2][0] = (matrix->m[0][2] * norm);
    result->m[2][1] = (matrix->m[1][2] * norm);
    result->m[2][2] = (matrix->m[2][2] * norm);
    result->m[3][0] = -((result->m[0][0] * matrix->m[3][0]) + (result->m[1][0] * matrix->m[3][1]) + (result->m[2][0] * matrix->m[3][2]));
    result->m[3][1] = -((result->m[0][1] * matrix->m[3][0]) + (result->m[1][1] * matrix->m[3][1]) + (result->m[2][1] * matrix->m[3][2]));
    result->m[3][2] = -((result->m[0][2] * matrix->m[3][0]) + (result->m[1][2] * matrix->m[3][1]) + (result->m[2][2] * matrix->m[3][2]));
    result->m[3][3] = 1.0f;
    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
}


/** The inverse of any affine matrix. */
void mtx4InvertAffine(Mtxf *matrix, Mtxf *result)
{
    f32 invDet = 0.0f;
    invDet += matrix->m[0][0] * matrix->m[1][1] * matrix->m[2][2];
    invDet += matrix->m[0][1] * matrix->m[1][2] * matrix->m[2][0];
    invDet += matrix->m[0][2] * matrix->m[1][0] * matrix->m[2][1];
    invDet -= matrix->m[0][2] * matrix->m[1][1] * matrix->m[2][0];
    invDet -= matrix->m[0][1] * matrix->m[1][0] * matrix->m[2][2];
    invDet -= matrix->m[0][0] * matrix->m[1][2] * matrix->m[2][1];
    invDet = 1.0f / invDet;

    result->m[0][0] = (matrix->m[1][1] * matrix->m[2][2] - matrix->m[1][2] * matrix->m[2][1]) * invDet;
    result->m[1][0] = (matrix->m[1][2] * matrix->m[2][0] - matrix->m[1][0] * matrix->m[2][2]) * invDet;
    result->m[2][0] = (matrix->m[1][0] * matrix->m[2][1] - matrix->m[1][1] * matrix->m[2][0]) * invDet;
    result->m[0][1] = (matrix->m[0][2] * matrix->m[2][1] - matrix->m[0][1] * matrix->m[2][2]) * invDet;
    result->m[1][1] = (matrix->m[0][0] * matrix->m[2][2] - matrix->m[0][2] * matrix->m[2][0]) * invDet;
    result->m[2][1] = (matrix->m[0][1] * matrix->m[2][0] - matrix->m[0][0] * matrix->m[2][1]) * invDet;
    result->m[0][2] = (matrix->m[0][1] * matrix->m[1][2] - matrix->m[0][2] * matrix->m[1][1]) * invDet;
    result->m[1][2] = (matrix->m[0][2] * matrix->m[1][0] - matrix->m[0][0] * matrix->m[1][2]) * invDet;
    result->m[2][2] = (matrix->m[0][0] * matrix->m[1][1] - matrix->m[0][1] * matrix->m[1][0]) * invDet;
    result->m[3][0] = -(matrix->m[3][0] * result->m[0][0] + matrix->m[3][1] * result->m[1][0] + matrix->m[3][2] * result->m[2][0]);
    result->m[3][1] = -(matrix->m[3][0] * result->m[0][1] + matrix->m[3][1] * result->m[1][1] + matrix->m[3][2] * result->m[2][1]);
    result->m[3][2] = -(matrix->m[3][0] * result->m[0][2] + matrix->m[3][1] * result->m[1][2] + matrix->m[3][2] * result->m[2][2]);
    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
    result->m[3][3] = 1.0f;
}


