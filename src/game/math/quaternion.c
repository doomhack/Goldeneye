#include <ultra64.h>
#include "game/math/quaternion.h"
#include "game/math/trig.h"

#define HALF2RAD(x) (x * (M_PI_F / 32768.0f))

void quatFromAnglesU16(vec3u angles, quatf q)
{
    f32 cosX = cosf(HALF2RAD(angles[0]) * 0.5f);
    f32 sinX = sinf(HALF2RAD(angles[0]) * 0.5f);
    f32 cosY = cosf(HALF2RAD(angles[1]) * 0.5f);
    f32 sinY = sinf(HALF2RAD(angles[1]) * 0.5f);
    f32 cosZ = cosf(HALF2RAD(angles[2]) * 0.5f);
    f32 sinZ = sinf(HALF2RAD(angles[2]) * 0.5f);
    f32 cosXCosY = cosX * cosY;
    f32 cosXSinY = cosX * sinY;
    f32 sinXCosY = sinX * cosY;
    f32 sinXSinY = sinX * sinY;
    q[0] = ((cosXCosY * cosZ) + (sinXSinY * sinZ));
    q[1] = ((sinXCosY * cosZ) - (cosXSinY * sinZ));
    q[2] = ((cosXSinY * cosZ) + (sinXCosY * sinZ));
    q[3] = ((cosXCosY * sinZ) - (sinXSinY * cosZ));
}

void quatFromAngles(vec3f angles, quatf q)
{
    f32 cosX = cosf(angles[0] * 0.5f);
    f32 sinX = sinf(angles[0] * 0.5f);
    f32 cosY = cosf(angles[1] * 0.5f);
    f32 sinY = sinf(angles[1] * 0.5f);
    f32 cosZ = cosf(angles[2] * 0.5f);
    f32 sinZ = sinf(angles[2] * 0.5f);
    f32 cosXCosY = cosX * cosY;
    f32 cosXSinY = cosX * sinY;
    f32 sinXCosY = sinX * cosY;
    f32 sinXSinY = sinX * sinY;
    q[0] = (cosXCosY * cosZ) + (sinXSinY * sinZ);
    q[1] = (sinXCosY * cosZ) - (cosXSinY * sinZ);
    q[2] = (cosXSinY * cosZ) + (sinXCosY * sinZ);
    q[3] = (cosXCosY * sinZ) - (sinXSinY * cosZ);
}

void quatFromXRotation(f32 angle, quatf q)
{
    q[0] = cosf(angle * 0.5f);
    q[1] = sinf(angle * 0.5f);
    q[2] = 0.0f;
    q[3] = 0.0f;
}

void quatFromYRotation(f32 angle, quatf q)
{
    q[0] = cosf(angle * 0.5f);
    q[1] = 0.0f;
    q[2] = sinf(angle * 0.5f);
    q[3] = 0.0f;
}

void quatFromZRotation(f32 angle, quatf q)
{
    q[0] = cosf(angle * 0.5f);
    q[1] = 0.0f;
    q[2] = 0.0f;
    q[3] = sinf(angle * 0.5f);
}

void quatToMtx4(quatf q, mat44f matrix)
{
    f32 scale = 2.0f / ((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) +  (q[3] * q[3]));
    f32 xs = q[1] * scale;
    f32 ys = q[2] * scale;
    f32 zs = q[3] * scale;
    f32 wx = q[0] * xs;
    f32 wy = q[0] * ys;
    f32 wz = q[0] * zs;
    f32 xx = q[1] * xs;
    f32 xy = q[1] * ys;
    f32 xz = q[1] * zs;
    f32 yy = q[2] * ys;
    f32 yz = q[2] * zs;
    f32 zz = q[3] * zs;
    matrix[0][0] = (1.0f - (yy + zz));
    matrix[0][1] = (xy + wz);
    matrix[0][2] = (xz - wy);
    matrix[1][0] = (xy - wz);
    matrix[1][1] = (1.0f - (xx + zz));
    matrix[1][2] = (yz + wx);
    matrix[2][0] = (xz + wy);
    matrix[2][1] = (yz - wx);
    matrix[2][2] = (1.0f - (xx + yy));
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][3] = 0.0f;
    matrix[3][3] = 1.0f;
}

void quatFromMtx4(mat44f matrix, quatf q)
{
    f32 root;
    f32 halfInvRoot;
    f32 trace = matrix[0][0] + matrix[1][1] + matrix[2][2] + 1.0f;
    if (0.0f < trace) {
        root = sqrtf(trace);
        halfInvRoot = 0.5f / root;
        q[0] = root * 0.5f;
        q[1] = (matrix[1][2] - matrix[2][1]) * halfInvRoot;
        q[2] = (matrix[2][0] - matrix[0][2]) * halfInvRoot;
        q[3] = (matrix[0][1] - matrix[1][0]) * halfInvRoot;
    } else {
        s32 i;
        s32 j;
        s32 indices[3] = {1,2,0};
        s32 k;
        i = 0;
        if (matrix[0][0] < matrix[1][1]) {
            i = 1;
        }
        if (matrix[i][i] < matrix[2][2]) {
            i = 2;
        }
        j = indices[i];
        k = indices[j];
        root = sqrtf(matrix[i][i] - matrix[j][j] - matrix[k][k] + 1.0f);
        halfInvRoot = 0.5f / root;
        q[i + 1] = root * 0.5f;
        q[    0] = (matrix[j][k] - matrix[k][j]) * halfInvRoot;
        q[j + 1] = (matrix[i][j] + matrix[j][i]) * halfInvRoot;
        q[k + 1] = (matrix[i][k] + matrix[k][i]) * halfInvRoot;
    }
}

void quatToMtx4WithTranslation(vec3f position, quatf rotation, mat44f matrix)
{
    quatToMtx4(rotation, matrix);
    matrix[3][0] = position[0];
    matrix[3][1] = position[1];
    matrix[3][2] = position[2];
}

#define EPSILON 0.00001001f

void quatSlerp(quatf q1, quatf q2, f32 t, quatf result)
{
    f32 dot = (q1[0] * q2[0]) + (q1[1] * q2[1]) + (q1[2] * q2[2]) + (q1[3] * q2[3]);
    f32 theta;
    f32 thetaQ1;
    f32 thetaQ2;
    f32 sine;
    f32 coeffQ1;
    f32 coeffQ2;
    if (dot < (-1.0f + EPSILON)) {
        result[0] = ((1.0f - t) * q1[0]) - (q2[0] * t);
        result[1] = ((1.0f - t) * q1[1]) - (q2[1] * t);
        result[2] = ((1.0f - t) * q1[2]) - (q2[2] * t);
        result[3] = ((1.0f - t) * q1[3]) - (q2[3] * t);
    }
    else if (dot <= (1.0f - EPSILON)) {
        theta = mathAcos(dot);
        thetaQ1 = (1.0f - t) * theta;
        thetaQ2 = t * theta;
        sine = sinf(theta);
        coeffQ1 = sinf(thetaQ1) / sine;
        coeffQ2 = sinf(thetaQ2) / sine;
        result[0] = (coeffQ1 * q1[0]) + (q2[0] * coeffQ2);
        result[1] = (coeffQ1 * q1[1]) + (q2[1] * coeffQ2);
        result[2] = (coeffQ1 * q1[2]) + (q2[2] * coeffQ2);
        result[3] = (coeffQ1 * q1[3]) + (q2[3] * coeffQ2);
    } else {
        result[0] = ((1.0f - t) * q1[0]) + (q2[0] * t);
        result[1] = ((1.0f - t) * q1[1]) + (q2[1] * t);
        result[2] = ((1.0f - t) * q1[2]) + (q2[2] * t);
        result[3] = ((1.0f - t) * q1[3]) + (q2[3] * t);
    }
}

void quatSlerpFromIdentity(quatf q, f32 t, quatf result)
{
    f32 dot = q[0];
    f32 identityW = 1.0f;
    f32 theta;
    f32 thetaQ;
    f32 thetaIdentity;
    f32 sine;
    f32 coeffQ;
    f32 coeffIdentity;
    if (q[0] < 0.0f) {
        dot = -dot;
        identityW = -identityW;
    }
    if (dot < -0.99998999f) {
        result[0] = (q[0] * t) - ((1.0f - t) * identityW);
        result[1] = (q[1] * t);
        result[2] = (q[2] * t);
        result[3] = (q[3] * t);
    } else if (dot <= 0.99998999f) {
        theta = mathAcos(dot);
        thetaQ = t * theta;
        thetaIdentity = (1.0f - t) * theta;
        sine = sinf(theta);
        coeffQ = sinf(thetaQ) / sine;
        coeffIdentity = sinf(thetaIdentity) / sine;
        result[0] = (q[0] * coeffQ) + (coeffIdentity * identityW);
        result[1] = (q[1] * coeffQ);
        result[2] = (q[2] * coeffQ);
        result[3] = (q[3] * coeffQ);
    } else {
        result[0] = (q[0] * t) + ((1.0f - t) * identityW);
        result[1] = (q[1] * t);
        result[2] = (q[2] * t);
        result[3] = (q[3] * t);
    }
}

void quatMakeShortestPath(quatf q1, quatf q2)
{
    f32 dot = (q1[0] * q2[0]) + (q1[1] * q2[1]) + (q1[2] * q2[2]) + (q1[3] * q2[3]);
    if (dot < 0.0f) {
        q2[0] = -q2[0];
        q2[1] = -q2[1];
        q2[2] = -q2[2];
        q2[3] = -q2[3];
    }
}

void quatMult(quatf lhs, quatf rhs, quatf result)
{
    result[0] = (lhs[0] * rhs[0]) - (lhs[1] * rhs[1]) - (lhs[2] * rhs[2]) - (lhs[3] * rhs[3]);
    result[1] = (lhs[0] * rhs[1]) + (rhs[0] * lhs[1]) + (lhs[2] * rhs[3]) - (lhs[3] * rhs[2]);
    result[2] = (lhs[0] * rhs[2]) + (rhs[0] * lhs[2]) + (lhs[3] * rhs[1]) - (lhs[1] * rhs[3]);
    result[3] = (lhs[0] * rhs[3]) + (rhs[0] * lhs[3]) + (lhs[1] * rhs[2]) - (lhs[2] * rhs[1]);
}

void quatMultInPlace(quatf lhs, quatf rhs)
{
    quatf result;
    quatMult(lhs, rhs, result);
    rhs[0] = result[0];
    rhs[1] = result[1];
    rhs[2] = result[2];
    rhs[3] = result[3];
}

void quatLog(quatf q, quatf result)
{
    f32 angle = mathAcos(q[0]);
    f32 sine = sinf(angle);
    if (sine == 0.0f) {
        result[0] = 0.0f;
        result[1] = 0.0f;
        result[2] = 0.0f;
        result[3] = 0.0f;
    } else {
        result[0] = 0.0f;
        result[1] = (q[1] * (angle / sine));
        result[2] = (q[2] * (angle / sine));
        result[3] = (q[3] * (angle / sine));
    }
}

void quatExp(quatf q, quatf result)
{
    f32 sine;
    f32 angle = sqrtf((q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]));
    f32 scale;
    if (angle == 0.0f) {
        result[0] = 1.0f;
        result[1] = 0.0f;
        result[2] = 0.0f;
        result[3] = 0.0f;
    } else {
        sine = sinf(angle);
        scale = sine / angle;
        result[0] = cosf(angle);
        result[1] = (q[1] * scale);
        result[2] = (q[2] * scale);
        result[3] = (q[3] * scale);
    }
}

void quatSquadControlPoint(quatf prev, quatf q, quatf next, quatf result)
{
    quatf conjugate;
    quatf toPrev;
    quatf toNext;
    quatf logPrev;
    quatf logNext;
    conjugate[0] = q[0];
    conjugate[1] = -q[1];
    conjugate[2] = -q[2];
    conjugate[3] = -q[3];
    quatMult(conjugate, prev, toPrev);
    quatMult(conjugate, next, toNext);
    quatLog(toPrev, logPrev);
    quatLog(toNext, logNext);
    logPrev[0] = -(logPrev[0] + logNext[0]) * 0.25f;
    logPrev[1] = -(logPrev[1] + logNext[1]) * 0.25f;
    logPrev[2] = -(logPrev[2] + logNext[2]) * 0.25f;
    logPrev[3] = -(logPrev[3] + logNext[3]) * 0.25f;
    quatExp(logPrev, toPrev);
    quatMult(q, toPrev, result);
}

void quatSquad(quatf q1, quatf q2, quatf q3, quatf q4, f32 t, quatf result)
{
    quatf q5;
    quatf q6;
    f32 t2 = (t + t) * (1.0f - t);
    quatMakeShortestPath(q1, q4);
    quatSlerp(q1, q4, t, q5);
    quatMakeShortestPath(q2, q3);
    quatSlerp(q2, q3, t, q6);
    quatMakeShortestPath(q5, q6);
    quatSlerp(q5, q6, t2, result);
}

void quatSpline(quatf q1, quatf q2, quatf q3, quatf q4, f32 t, quatf result)
{
    quatf q5;
    quatf q6;
    quatSquadControlPoint(q1, q2, q3, q5);
    quatSquadControlPoint(q2, q3, q4, q6);
    quatSquad(q2, q5, q6, q3, t, result);
}
