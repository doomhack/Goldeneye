/**
 * Quaternions (quatf: w, x, y, z), used to blend rotations smoothly: between animation
 * frames, from a thrown object's spin to lying flat, and along the first-person gun's
 * keyframed moves.
 *
 * A unit quaternion (w, x, y, z) is a rotation by angle a around the unit axis v, with
 * w = cos(a/2) and (x, y, z) = v sin(a/2). q and -q are the same rotation, so before
 * blending two, quatMakeShortestPath flips one if they are more than half a turn apart.
 *
 * As with the matrices, the order of the floating-point operations is kept as it was: the
 * game's state depends on the exact results.
 */
#include <ultra64.h>
#include <math.h>
#include "game/math/quaternion.h"
#include "game/math/trig.h"

/** How close to (anti)parallel two quaternions must be for slerp to fall back to a lerp. */
#define QUAT_SLERP_EPSILON 0.00001001f

/** The rotation mtx4LoadRotation makes from the same Euler angles (x, then y, then z). */
void quatFromAngles(const coord3d *angles, quatf *q)
{
    f32 cosX = cosf(angles->x * 0.5f);
    f32 sinX = sinf(angles->x * 0.5f);
    f32 cosY = cosf(angles->y * 0.5f);
    f32 sinY = sinf(angles->y * 0.5f);
    f32 cosZ = cosf(angles->z * 0.5f);
    f32 sinZ = sinf(angles->z * 0.5f);
    f32 cosXCosY = cosX * cosY;
    f32 cosXSinY = cosX * sinY;
    f32 sinXCosY = sinX * cosY;
    f32 sinXSinY = sinX * sinY;

    q->w = cosXCosY * cosZ + sinXSinY * sinZ;
    q->x = sinXCosY * cosZ - cosXSinY * sinZ;
    q->y = cosXSinY * cosZ + sinXCosY * sinZ;
    q->z = cosXCosY * sinZ - sinXSinY * cosZ;
}

/** The rotation matrix of a quaternion, which needn't have length 1. */
void quatToMtx4(const quatf *q, Mtxf *matrix)
{
    f32 scale = 2.0f / (q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z);
    f32 xs = q->x * scale;
    f32 ys = q->y * scale;
    f32 zs = q->z * scale;
    f32 wx = q->w * xs;
    f32 wy = q->w * ys;
    f32 wz = q->w * zs;
    f32 xx = q->x * xs;
    f32 xy = q->x * ys;
    f32 xz = q->x * zs;
    f32 yy = q->y * ys;
    f32 yz = q->y * zs;
    f32 zz = q->z * zs;

    *matrix = (Mtxf) {{
        {1.0f - (yy + zz), xy + wz,          xz - wy,          0.0f},
        {xy - wz,          1.0f - (xx + zz), yz + wx,          0.0f},
        {xz + wy,          yz - wx,          1.0f - (xx + yy), 0.0f},
        {0.0f,             0.0f,             0.0f,             1.0f},
    }};
}

void quatToMtx4WithTranslation(const coord3d *position, const quatf *q, Mtxf *matrix)
{
    quatToMtx4(q, matrix);
    matrix->m[3][0] = position->x;
    matrix->m[3][1] = position->y;
    matrix->m[3][2] = position->z;
}

static f32 quatDot(const quatf *a, const quatf *b)
{
    return a->w * b->w + a->x * b->x + a->y * b->y + a->z * b->z;
}

/** Negates `to` if that makes it closer to `from`, so blending takes the short way round. */
void quatMakeShortestPath(const quatf *from, quatf *to)
{
    if (quatDot(from, to) < 0.0f)
    {
        for (s32 i = 0; i < 4; i++)
        {
            to->f[i] = -to->f[i];
        }
    }
}

/**
 * Spherical linear interpolation: the rotation `t` of the way from `from` to `to`, at a
 * steady speed. Nearly equal (or opposite) quaternions are blended linearly instead, where
 * the slerp formula would divide by nearly 0.
 */
void quatSlerp(const quatf *from, const quatf *to, f32 t, quatf *result)
{
    f32 dot = quatDot(from, to);

    if (dot < -1.0f + QUAT_SLERP_EPSILON)
    {
        for (s32 i = 0; i < 4; i++)
        {
            result->f[i] = (1.0f - t) * from->f[i] - to->f[i] * t;
        }
    }
    else if (dot <= 1.0f - QUAT_SLERP_EPSILON)
    {
        f32 theta = mathAcos(dot);
        f32 sinTheta = sinf(theta);
        f32 weightFrom = sinf((1.0f - t) * theta) / sinTheta;
        f32 weightTo = sinf(t * theta) / sinTheta;

        for (s32 i = 0; i < 4; i++)
        {
            result->f[i] = weightFrom * from->f[i] + to->f[i] * weightTo;
        }
    }
    else
    {
        for (s32 i = 0; i < 4; i++)
        {
            result->f[i] = (1.0f - t) * from->f[i] + to->f[i] * t;
        }
    }
}

/** quatSlerp from no rotation (1, 0, 0, 0) to `q`, by the short way. */
void quatSlerpFromIdentity(const quatf *q, f32 t, quatf *result)
{
    f32 dot = q->w;
    f32 identityW = 1.0f;

    if (q->w < 0.0f)
    {
        dot = -dot;
        identityW = -identityW;
    }

    // (dot is never negative here, so only the last two cases happen)
    if (dot < -1.0f + QUAT_SLERP_EPSILON)
    {
        result->w = q->w * t - (1.0f - t) * identityW;
        result->x = q->x * t;
        result->y = q->y * t;
        result->z = q->z * t;
    }
    else if (dot <= 1.0f - QUAT_SLERP_EPSILON)
    {
        f32 theta = mathAcos(dot);
        f32 sinTheta = sinf(theta);
        f32 weightQ = sinf(t * theta) / sinTheta;
        f32 weightIdentity = sinf((1.0f - t) * theta) / sinTheta;

        result->w = q->w * weightQ + weightIdentity * identityW;
        result->x = q->x * weightQ;
        result->y = q->y * weightQ;
        result->z = q->z * weightQ;
    }
    else
    {
        result->w = q->w * t + (1.0f - t) * identityW;
        result->x = q->x * t;
        result->y = q->y * t;
        result->z = q->z * t;
    }
}

/** result = lhs * rhs: the rotation rhs, then lhs. */
static void quatMult(const quatf *lhs, const quatf *rhs, quatf *result)
{
    result->w = lhs->w * rhs->w - lhs->x * rhs->x - lhs->y * rhs->y - lhs->z * rhs->z;
    result->x = lhs->w * rhs->x + rhs->w * lhs->x + lhs->y * rhs->z - lhs->z * rhs->y;
    result->y = lhs->w * rhs->y + rhs->w * lhs->y + lhs->z * rhs->x - lhs->x * rhs->z;
    result->z = lhs->w * rhs->z + rhs->w * lhs->z + lhs->x * rhs->y - lhs->y * rhs->x;
}

/** The logarithm of a unit quaternion: (0, v a/2) for a rotation by a around v. */
static void quatLog(const quatf *q, quatf *result)
{
    f32 halfAngle = mathAcos(q->w);
    f32 sine = sinf(halfAngle);

    result->w = 0.0f;

    if (sine == 0.0f)
    {
        result->x = 0.0f;
        result->y = 0.0f;
        result->z = 0.0f;
    }
    else
    {
        result->x = q->x * (halfAngle / sine);
        result->y = q->y * (halfAngle / sine);
        result->z = q->z * (halfAngle / sine);
    }
}

/** The inverse of quatLog. */
static void quatExp(const quatf *q, quatf *result)
{
    f32 halfAngle = sqrtf(q->x * q->x + q->y * q->y + q->z * q->z);

    if (halfAngle == 0.0f)
    {
        *result = (quatf) {{1.0f, 0.0f, 0.0f, 0.0f}};
    }
    else
    {
        f32 scale = sinf(halfAngle) / halfAngle;

        result->w = cosf(halfAngle);
        result->x = q->x * scale;
        result->y = q->y * scale;
        result->z = q->z * scale;
    }
}

/**
 * The inner control point for the spline segment through `q`, given its neighbours:
 * q exp(-(log(q^-1 prev) + log(q^-1 next)) / 4).
 */
static void quatSplineControlPoint(const quatf *prev, const quatf *q, const quatf *next, quatf *result)
{
    quatf inverse = {{q->w, -q->x, -q->y, -q->z}};
    quatf toPrev;
    quatf toNext;
    quatf logPrev;
    quatf logNext;
    quatf tangent;
    quatf offset;

    quatMult(&inverse, prev, &toPrev);
    quatMult(&inverse, next, &toNext);
    quatLog(&toPrev, &logPrev);
    quatLog(&toNext, &logNext);

    for (s32 i = 0; i < 4; i++)
    {
        tangent.f[i] = -(logPrev.f[i] + logNext.f[i]) * 0.25f;
    }

    quatExp(&tangent, &offset);
    quatMult(q, &offset, result);
}

/**
 * Spherical quadrangle interpolation ("squad") from q1 to q2 with control points a and b:
 * slerp(slerp(q1, q2, t), slerp(a, b, t), 2t(1 - t)).
 */
static void quatSquad(const quatf *q1, const quatf *a, const quatf *b, const quatf *q2, f32 t, quatf *result)
{
    quatf ends;
    quatf controls;
    quatf q2Near = *q2;
    quatf bNear = *b;
    f32 blend = (t + t) * (1.0f - t);

    quatMakeShortestPath(q1, &q2Near);
    quatSlerp(q1, &q2Near, t, &ends);
    quatMakeShortestPath(a, &bNear);
    quatSlerp(a, &bNear, t, &controls);
    quatMakeShortestPath(&ends, &controls);
    quatSlerp(&ends, &controls, blend, result);
}

/**
 * A smooth curve through a sequence of rotations: the point `t` of the way along the
 * segment from q1 to q2, with q0 and q3 (the rotations before and after) shaping it.
 */
void quatSpline(const quatf *q0, const quatf *q1, const quatf *q2, const quatf *q3, f32 t, quatf *result)
{
    quatf a;
    quatf b;

    quatSplineControlPoint(q0, q1, q2, &a);
    quatSplineControlPoint(q1, q2, q3, &b);
    quatSquad(q1, &a, &b, q2, t, result);
}
