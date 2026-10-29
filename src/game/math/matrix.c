/**
 * 4x4 float matrices (Mtxf), 3x3 ones (Mtx3f), and conversion to the N64's fixed-point Mtx.
 *
 * Matrices are stored the way the N64 and libultra expect: m[row][column], with points as
 * row vectors on the left (p' = p * M). Rows 0-2 are the transformed x, y and z axes and
 * row 3 is the translation, so m[3][0..2] is the position.
 *
 * mtx4Mult(a, b, out) makes `out` apply b first and then a. In column-vector notation that
 * is out = a * b. The InPlace versions write the result over their second argument.
 *
 * Functions that build a whole matrix are mtx4Load...; ones that change part of an
 * existing matrix are mtx4Set... or mtx4Scale.... Angles are in radians.
 *
 * The order of the floating-point operations matters: the game's state (and so Rare's
 * attract-mode demos) depends on the exact results, so rewrites here keep each expression's
 * order. tools/hosttest/mathtest.c checks the results.
 */
#include <ultra64.h>
#include <math.h>
#include "game/math/matrix.h"
#include "game/math/trig.h"

/** mtxF2L's fixed-point scale: s15.16, so 1.0 is 0x10000. */
#define MTX_FIXED_ONE 65536.0f

/** Below this, mtx4GetRotation treats the matrix as pitched straight up or down. */
#define MTX_GIMBAL_EPSILON (FLT_EPSILON * 16)

/*
 * The scales mtxF2L multiplies by: [0] for columns 0-2, MTX_FIXED_ONE times the level's world
 * scale (see mtxSetWorldScale), and [1] for column 3, always MTX_FIXED_ONE.
 */
static f32 m_mtxFixedScale[2] = {MTX_FIXED_ONE, MTX_FIXED_ONE};

/** The world scale saved by mtxPushUnitWorldScale. */
static f32 m_mtxSavedWorldScale;

void mtx4LoadIdentity(Mtxf *matrix)
{
    *matrix = (Mtxf) {{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
}

void mtx4Copy(const Mtxf *src, Mtxf *dst)
{
    *dst = *src;
}

/** result = lhs * rhs: transforms by rhs, then by lhs. result must not be lhs or rhs. */
void mtx4Mult(const Mtxf *lhs, const Mtxf *rhs, Mtxf *result)
{
    for (s32 col = 0; col < 4; col++)
    {
        for (s32 row = 0; row < 4; row++)
        {
            result->m[row][col] = lhs->m[0][col] * rhs->m[row][0] + lhs->m[1][col] * rhs->m[row][1]
                                + lhs->m[2][col] * rhs->m[row][2] + lhs->m[3][col] * rhs->m[row][3];
        }
    }
}

/** rhs = lhs * rhs. */
void mtx4MultInPlace(const Mtxf *lhs, Mtxf *rhs)
{
    Mtxf result;

    mtx4Mult(lhs, rhs, &result);
    *rhs = result;
}

/**
 * mtx4Mult for affine matrices (no perspective): column 3 of both is taken to be
 * (0, 0, 0, 1), and is set to that in the result. result must not be lhs or rhs.
 */
void mtx4MultAffine(const Mtxf *lhs, const Mtxf *rhs, Mtxf *result)
{
    for (s32 col = 0; col < 3; col++)
    {
        for (s32 row = 0; row < 4; row++)
        {
            result->m[row][col] = lhs->m[0][col] * rhs->m[row][0] + lhs->m[1][col] * rhs->m[row][1]
                                + lhs->m[2][col] * rhs->m[row][2];

            if (row == 3)
            {
                result->m[row][col] += lhs->m[3][col];
            }
        }
    }

    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
    result->m[3][3] = 1.0f;
}

/** rhs = lhs * rhs, for affine matrices. */
void mtx4MultAffineInPlace(const Mtxf *lhs, Mtxf *rhs)
{
    Mtxf result;

    mtx4MultAffine(lhs, rhs, &result);
    *rhs = result;
}

/** Applies only the rotation and scale part of a matrix to a vector. */
void mtx4RotateVec(const Mtxf *matrix, const coord3d *vector, coord3d *result)
{
    for (s32 i = 0; i < 3; i++)
    {
        result->f[i] = matrix->m[0][i] * vector->f[0] + matrix->m[1][i] * vector->f[1] + matrix->m[2][i] * vector->f[2];
    }
}

void mtx4RotateVecInPlace(const Mtxf *matrix, coord3d *vector)
{
    coord3d result;

    mtx4RotateVec(matrix, vector, &result);
    *vector = result;
}

/** Applies a matrix, translation included, to a point. */
void mtx4TransformVec(const Mtxf *matrix, const coord3d *point, coord3d *result)
{
    mtx4RotateVec(matrix, point, result);
    result->x += matrix->m[3][0];
    result->y += matrix->m[3][1];
    result->z += matrix->m[3][2];
}

void mtx4TransformVecInPlace(const Mtxf *matrix, coord3d *point)
{
    mtx4RotateVecInPlace(matrix, point);
    point->x += matrix->m[3][0];
    point->y += matrix->m[3][1];
    point->z += matrix->m[3][2];
}

/* Rotations by `angle` around one axis, anticlockwise looking down the axis at the origin */

void mtx4LoadXRotation(f32 angle, Mtxf *matrix)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);

    *matrix = (Mtxf) {{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, c,    s,    0.0f},
        {0.0f, -s,   c,    0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
}

void mtx4LoadYRotation(f32 angle, Mtxf *matrix)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);

    *matrix = (Mtxf) {{
        {c,    0.0f, -s,   0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {s,    0.0f, c,    0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
}

void mtx4LoadZRotation(f32 angle, Mtxf *matrix)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);

    *matrix = (Mtxf) {{
        {c,    s,    0.0f, 0.0f},
        {-s,   c,    0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
}

/** A rotation around the y axis, then a move to `position`. */
void mtx4LoadYRotationAndTranslation(const coord3d *position, f32 angle, Mtxf *matrix)
{
    mtx4LoadYRotation(angle, matrix);
    mtx4SetTranslation(position, matrix);
}

/** A rotation by the Euler angles in `angles`, applied around x first, then y, then z. */
void mtx4LoadRotation(const coord3d *angles, Mtxf *matrix)
{
    f32 cosX = cosf(angles->x);
    f32 sinX = sinf(angles->x);
    f32 cosY = cosf(angles->y);
    f32 sinY = sinf(angles->y);
    f32 cosZ = cosf(angles->z);
    f32 sinZ = sinf(angles->z);
    f32 sinXSinZ = sinX * sinZ;
    f32 cosXSinZ = cosX * sinZ;
    f32 sinXCosZ = sinX * cosZ;
    f32 cosXCosZ = cosX * cosZ;

    *matrix = (Mtxf) {{
        {cosY * cosZ,                  cosY * sinZ,                  -sinY,       0.0f},
        {sinXCosZ * sinY - cosXSinZ,   sinXSinZ * sinY + cosXCosZ,   sinX * cosY, 0.0f},
        {cosXCosZ * sinY + sinXSinZ,   cosXSinZ * sinY - sinXCosZ,   cosX * cosY, 0.0f},
        {0.0f,                         0.0f,                         0.0f,        1.0f},
    }};
}

void mtx4LoadRotationAndTranslation(const coord3d *position, const coord3d *angles, Mtxf *matrix)
{
    mtx4LoadRotation(angles, matrix);
    mtx4SetTranslation(position, matrix);
}

/**
 * The inverse of mtx4LoadRotation: the Euler angles of a rotation matrix, each in [0, 2pi)
 * (see mathAtan2). Pitched straight up or down, the x and z rotations are the same thing,
 * so x comes back as 0.
 */
void mtx4GetRotation(const Mtxf *matrix, coord3d *angles)
{
    f32 sinXCosY = matrix->m[1][2];
    f32 cosXCosY = matrix->m[2][2];
    f32 cosY = sqrtf(sinXCosY * sinXCosY + cosXCosY * cosXCosY);

    if (cosY > MTX_GIMBAL_EPSILON)
    {
        angles->x = mathAtan2(matrix->m[1][2], matrix->m[2][2]);
        angles->y = mathAtan2(-matrix->m[0][2], cosY);
        angles->z = mathAtan2(matrix->m[0][1], matrix->m[0][0]);
    }
    else
    {
        angles->x = 0.0f;
        angles->y = mathAtan2(-matrix->m[0][2], cosY);
        angles->z = mathAtan2(-matrix->m[1][0], matrix->m[1][1]);
    }
}

/** An identity matrix that moves to `position`. */
void mtx4LoadTranslation(const coord3d *position, Mtxf *matrix)
{
    mtx4LoadIdentity(matrix);
    mtx4SetTranslation(position, matrix);
}

/** Replaces only the translation. */
void mtx4SetTranslation(const coord3d *position, Mtxf *matrix)
{
    matrix->m[3][0] = position->x;
    matrix->m[3][1] = position->y;
    matrix->m[3][2] = position->z;
}

/*
 * Scaling parts of a matrix. Row n is m[n], so rows 0-2 are the x, y and z axes: scaling
 * them scales the object before it is placed. Scaling columns 0-2 scales the result,
 * translation included.
 */

void mtx4ScaleRow0(f32 scale, Mtxf *matrix)
{
    for (s32 col = 0; col < 3; col++)
    {
        matrix->m[0][col] *= scale;
    }
}

void mtx4ScaleRow1(f32 scale, Mtxf *matrix)
{
    for (s32 col = 0; col < 3; col++)
    {
        matrix->m[1][col] *= scale;
    }
}

void mtx4ScaleRow2(f32 scale, Mtxf *matrix)
{
    for (s32 col = 0; col < 3; col++)
    {
        matrix->m[2][col] *= scale;
    }
}

/** Scales all of row 2, column 3 included. */
void mtx4ScaleRow2AndW(f32 scale, Mtxf *matrix)
{
    for (s32 col = 0; col < 4; col++)
    {
        matrix->m[2][col] *= scale;
    }
}

/** Scales all of rows 0-2, column 3 included. */
void mtx4ScaleRows012(f32 scale, Mtxf *matrix)
{
    for (s32 row = 0; row < 3; row++)
    {
        for (s32 col = 0; col < 4; col++)
        {
            matrix->m[row][col] *= scale;
        }
    }
}

/** Scales the top left 3x3: the rotation and scale part. */
void mtx4Scale3x3(f32 scale, Mtxf *matrix)
{
    for (s32 row = 0; row < 3; row++)
    {
        for (s32 col = 0; col < 3; col++)
        {
            matrix->m[row][col] *= scale;
        }
    }
}

void mtx4ScaleColumn2(f32 scale, Mtxf *matrix)
{
    for (s32 row = 0; row < 4; row++)
    {
        matrix->m[row][2] *= scale;
    }
}

void mtx4ScaleColumns012(f32 scale, Mtxf *matrix)
{
    for (s32 row = 0; row < 4; row++)
    {
        for (s32 col = 0; col < 3; col++)
        {
            matrix->m[row][col] *= scale;
        }
    }
}

/** Scales (x, y, z) to length 1, the way libultra's guNormalize does. */
static void mtxNormalize(f32 *x, f32 *y, f32 *z)
{
    f32 scale = 1.0f / sqrtf(*x * *x + *y * *y + *z * *z);

    *x *= scale;
    *y *= scale;
    *z *= scale;
}

/**
 * The axes of a camera or object facing along `dir`: `forward` is `dir` reversed (the
 * camera looks down its -z axis), and `right` and `up` are at right angles to it, with `up`
 * as close to `upHint` as it can be. All three have length 1.
 */
static void mtxMakeBasis(f32 dirX, f32 dirY, f32 dirZ, f32 upX, f32 upY, f32 upZ,
                         coord3d *right, coord3d *up, coord3d *forward)
{
    f32 scale = -1.0f / sqrtf(dirX * dirX + dirY * dirY + dirZ * dirZ);

    forward->x = dirX * scale;
    forward->y = dirY * scale;
    forward->z = dirZ * scale;

    right->x = upY * forward->z - upZ * forward->y;
    right->y = upZ * forward->x - upX * forward->z;
    right->z = upX * forward->y - upY * forward->x;
    scale = 1.0f / sqrtf(right->x * right->x + right->y * right->y + right->z * right->z);
    right->x *= scale;
    right->y *= scale;
    right->z *= scale;

    up->x = forward->y * right->z - forward->z * right->y;
    up->y = forward->z * right->x - forward->x * right->z;
    up->z = forward->x * right->y - forward->y * right->x;
    scale = 1.0f / sqrtf(up->x * up->x + up->y * up->y + up->z * up->z);
    up->x *= scale;
    up->y *= scale;
    up->z *= scale;
}

/**
 * A camera (view) matrix for an eye at `eye` looking along `forward`, with `up` roughly up.
 * Like libultra's guLookAtF, but taking a direction instead of a target.
 */
void mtx4LoadLookDir(Mtxf *matrix, f32 eyeX, f32 eyeY, f32 eyeZ, f32 forwardX, f32 forwardY, f32 forwardZ, f32 upX, f32 upY, f32 upZ)
{
    coord3d right;
    coord3d up;
    coord3d back;

    mtxMakeBasis(forwardX, forwardY, forwardZ, upX, upY, upZ, &right, &up, &back);

    // The inverse of placing the camera: its axes as columns, and the eye moved to 0
    *matrix = (Mtxf) {{
        {right.x, up.x, back.x, 0.0f},
        {right.y, up.y, back.y, 0.0f},
        {right.z, up.z, back.z, 0.0f},
        {
            -(eyeX * right.x + eyeY * right.y + eyeZ * right.z),
            -(eyeX * up.x + eyeY * up.y + eyeZ * up.z),
            -(eyeX * back.x + eyeY * back.y + eyeZ * back.z),
            1.0f,
        },
    }};
}

/** A camera (view) matrix for an eye at `eye` looking at `target`. */
void mtx4LoadLookAt(Mtxf *matrix, f32 eyeX, f32 eyeY, f32 eyeZ, f32 targetX, f32 targetY, f32 targetZ, f32 upX, f32 upY, f32 upZ)
{
    mtx4LoadLookDir(matrix, eyeX, eyeY, eyeZ, targetX - eyeX, targetY - eyeY, targetZ - eyeZ, upX, upY, upZ);
}

/**
 * The inverse of mtx4LoadLookDir: places an object at `pos` with its z axis pointing
 * against `dir` and its y axis roughly along `up`.
 */
void mtx4LoadFacingDir(Mtxf *matrix, f32 posX, f32 posY, f32 posZ, f32 dirX, f32 dirY, f32 dirZ, f32 upX, f32 upY, f32 upZ)
{
    coord3d right;
    coord3d up;
    coord3d back;

    mtxMakeBasis(dirX, dirY, dirZ, upX, upY, upZ, &right, &up, &back);

    *matrix = (Mtxf) {{
        {right.x, right.y, right.z, 0.0f},
        {up.x,    up.y,    up.z,    0.0f},
        {back.x,  back.y,  back.z,  0.0f},
        {posX,    posY,    posZ,    1.0f},
    }};
}

/** mtx4LoadFacingDir with a target point instead of a direction. */
void mtx4LoadFacing(Mtxf *matrix, f32 posX, f32 posY, f32 posZ, f32 targetX, f32 targetY, f32 targetZ, f32 upX, f32 upY, f32 upZ)
{
    mtx4LoadFacingDir(matrix, posX, posY, posZ, targetX - posX, targetY - posY, targetZ - posZ, upX, upY, upZ);
}

/**
 * A rotation that turns the -z axis to point along (x, y, z), then rolls by `angle` around
 * it. libultra's guAlignF, with the angle in radians instead of degrees.
 */
void mtx4LoadAlign(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z)
{
    f32 s;
    f32 c;
    f32 h;
    f32 hinv;

    mtxNormalize(&x, &y, &z);
    s = sinf(angle);
    c = cosf(angle);
    h = sqrtf(x * x + z * z);

    if (h == 0.0f)
    {
        // Pointing straight up or down. (libultra doesn't handle this either.)
        mtx4LoadIdentity(matrix);
        return;
    }

    hinv = 1.0f / h;

    *matrix = (Mtxf) {{
        {(-z * c - s * y * x) * hinv, s * h, (c * x - s * y * z) * hinv,  0.0f},
        {(z * s - c * y * x) * hinv,  c * h, (-s * x - c * y * z) * hinv, 0.0f},
        {-x,                          -y,    -z,                          0.0f},
        {0.0f,                        0.0f,  0.0f,                        1.0f},
    }};
}

/** The inverse (transpose) of mtx4LoadAlign. */
void mtx4LoadAlignInverse(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z)
{
    f32 s;
    f32 c;
    f32 h;
    f32 hinv;

    mtxNormalize(&x, &y, &z);
    s = sinf(angle);
    c = cosf(angle);
    h = sqrtf(x * x + z * z);

    if (h == 0.0f)
    {
        mtx4LoadIdentity(matrix);
        return;
    }

    hinv = 1.0f / h;

    *matrix = (Mtxf) {{
        {(-(z * c) - y * (x * s)) * hinv, (z * s - y * (x * c)) * hinv,  -x,   0.0f},
        {s * h,                           c * h,                         -y,   0.0f},
        {(x * c - y * (z * s)) * hinv,    (-(x * s) - y * (z * c)) * hinv, -z, 0.0f},
        {0.0f,                            0.0f,                          0.0f, 1.0f},
    }};
}

/** The inverse of a pure rotation matrix (its transpose), dropping any translation. */
void mtx4TransposeRotation(const Mtxf *matrix, Mtxf *result)
{
    for (s32 row = 0; row < 3; row++)
    {
        for (s32 col = 0; col < 3; col++)
        {
            result->m[row][col] = matrix->m[col][row];
        }

        result->m[row][3] = 0.0f;
        result->m[3][row] = 0.0f;
    }

    result->m[3][3] = 1.0f;
}

/** The inverse of a rotation with a uniform scale, and a translation. */
void mtx4InvertRigid(const Mtxf *matrix, Mtxf *result)
{
    // The rotation's transpose, divided by the scale squared
    f32 invScaleSq = 1.0f / (matrix->m[0][0] * matrix->m[0][0] + matrix->m[1][0] * matrix->m[1][0] + matrix->m[2][0] * matrix->m[2][0]);

    for (s32 row = 0; row < 3; row++)
    {
        for (s32 col = 0; col < 3; col++)
        {
            result->m[row][col] = matrix->m[col][row] * invScaleSq;
        }
    }

    // The translation, moved back through the inverse rotation
    for (s32 col = 0; col < 3; col++)
    {
        result->m[3][col] = -(result->m[0][col] * matrix->m[3][0] + result->m[1][col] * matrix->m[3][1] + result->m[2][col] * matrix->m[3][2]);
    }

    result->m[3][3] = 1.0f;
    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
}

/** The inverse of any affine matrix (one with no perspective). */
void mtx4InvertAffine(const Mtxf *matrix, Mtxf *result)
{
    const f32 (*m)[4] = matrix->m;
    f32 det = 0.0f;
    f32 invDet;

    det += m[0][0] * m[1][1] * m[2][2];
    det += m[0][1] * m[1][2] * m[2][0];
    det += m[0][2] * m[1][0] * m[2][1];
    det -= m[0][2] * m[1][1] * m[2][0];
    det -= m[0][1] * m[1][0] * m[2][2];
    det -= m[0][0] * m[1][2] * m[2][1];
    invDet = 1.0f / det;

    // The 3x3 part: the adjugate over the determinant
    result->m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
    result->m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
    result->m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
    result->m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
    result->m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
    result->m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
    result->m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
    result->m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;
    result->m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

    for (s32 col = 0; col < 3; col++)
    {
        result->m[3][col] = -(m[3][0] * result->m[0][col] + m[3][1] * result->m[1][col] + m[3][2] * result->m[2][col]);
    }

    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
    result->m[3][3] = 1.0f;
}

/*
 * 3x3 matrices, for things that only rotate. (PAL builds keep spent cartridge cases' spin
 * this way.)
 */

/** result = lhs * rhs, like mtx4Mult. result must not be lhs or rhs. */
void mtx3Mult(const Mtx3f *lhs, const Mtx3f *rhs, Mtx3f *result)
{
    for (s32 col = 0; col < 3; col++)
    {
        for (s32 row = 0; row < 3; row++)
        {
            result->m[row][col] = lhs->m[0][col] * rhs->m[row][0] + lhs->m[1][col] * rhs->m[row][1] + lhs->m[2][col] * rhs->m[row][2];
        }
    }
}

/** rhs = lhs * rhs. */
void mtx3MultInPlace(const Mtx3f *lhs, Mtx3f *rhs)
{
    Mtx3f result;

    mtx3Mult(lhs, rhs, &result);
    *rhs = result;
}

void mtx3ToMtx4(const Mtx3f *src, Mtxf *dst)
{
    for (s32 row = 0; row < 3; row++)
    {
        for (s32 col = 0; col < 3; col++)
        {
            dst->m[row][col] = src->m[row][col];
        }

        dst->m[row][3] = 0.0f;
        dst->m[3][row] = 0.0f;
    }

    dst->m[3][3] = 1.0f;
}

void mtx4ToMtx3(const Mtxf *src, Mtx3f *dst)
{
    for (s32 row = 0; row < 3; row++)
    {
        for (s32 col = 0; col < 3; col++)
        {
            dst->m[row][col] = src->m[row][col];
        }
    }
}

/*
 * The world scale. Each level has a scale (its "visibility" setting in the level table)
 * that mtxF2L applies when it makes the fixed-point matrices the RSP uses, presumably so
 * that large levels still fit the s15.16 range. The first-person gun code pushes a scale
 * of 1 while it builds its matrices.
 */
void mtxSetWorldScale(f32 scale)
{
    m_mtxFixedScale[0] = MTX_FIXED_ONE * scale;
}

void mtxPushUnitWorldScale(void)
{
    m_mtxSavedWorldScale = m_mtxFixedScale[0];
    m_mtxFixedScale[0] = MTX_FIXED_ONE;
}

void mtxPopWorldScale(void)
{
    m_mtxFixedScale[0] = m_mtxSavedWorldScale;
}

/**
 * Converts a float matrix to the N64's fixed-point Mtx (like libultra's guMtxF2L), applying
 * the world scale to columns 0-2. A Mtx holds the integer halves of all 16 values in its
 * first 8 words, then the fraction halves. `dst` is Mtx storage, typed Mtxf because the game
 * keeps Mtx arrays typed as Mtxf. It must not be `src`.
 */
void mtxF2L(const Mtxf *src, Mtxf *dst)
{
    const f32 *in = &src->m[0][0];
    u32 *out = (u32 *) dst;

    // Two values per pass: an even column (0 or 2) and the odd one after it (1 or 3)
    for (s32 i = 0; i < 8; i++)
    {
        s32 even = in[i * 2] * m_mtxFixedScale[0];
        s32 odd = in[i * 2 + 1] * m_mtxFixedScale[i & 1];

        out[i] = (even & 0xFFFF0000) | ((u32) odd >> 16);
        out[i + 8] = ((u32) even << 16) | (odd & 0xFFFF);
    }
}

/** The inverse of mtxF2L, dividing the world scale back out. `dst` must not be `src`. */
void mtxL2F(const Mtxf *src, Mtxf *dst)
{
    const u32 *in = (const u32 *) src;
    f32 *out = &dst->m[0][0];

    for (s32 i = 0; i < 8; i++)
    {
        u32 integers = in[i];
        u32 fractions = in[i + 8];

        out[i * 2] = (s32) ((integers & 0xFFFF0000) | (fractions >> 16)) / m_mtxFixedScale[0];
        out[i * 2 + 1] = (s32) ((integers << 16) | (fractions & 0xFFFF)) / m_mtxFixedScale[i & 1];
    }
}

/** The perspNorm value the RSP needs with a perspective matrix, like guPerspective's. */
u16 mtxCalcPerspNorm(f32 near, f32 far)
{
    f32 sum = near + far;
    u16 norm;

    if (sum <= 2.0f)
    {
        return 0xFFFF;
    }

    norm = 0x20000 / sum;

    return norm != 0 ? norm : 1;
}
