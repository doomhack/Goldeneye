/**
 * Host tests for the maths library (src/game/math). See run.sh.
 *
 * The game's state depends on the exact results of these functions, so a rewrite must
 * keep them bit for bit (the demos check that on the N64). These tests check that the
 * results are also *right*: each function against the C library's double-precision maths,
 * or against what it should do (an inverse undoes, a rotation keeps lengths, and so on).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/game/math/matrix.c"
#include "../../src/game/math/quaternion.c"
#include "../../src/game/math/trig.c"
#include "../../src/game/math/round.c"
#include "../../src/game/math/interp.c"
#include "../../src/game/math/physics.c"
#include "../../src/game/math/line_tri_intersect.c"

/* The C library's double-precision functions, as the reference */
double sin(double);
double cos(double);
double atan2(double, double);
double acos(double);
double floor(double);
double ceil(double);
double fabs(double);
double sqrt(double);

static s32 s_failures;

#define CHECK(cond)                                                               \
    do {                                                                          \
        if (!(cond)) {                                                            \
            printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond);       \
            if (++s_failures > 30) exit(1);                                       \
        }                                                                         \
    } while (0)

#define CHECK_NEAR(a, b, tolerance) CHECK(fabs((double) (a) - (double) (b)) <= (tolerance))

static u32 s_random = 1;

u32 randomGetNext(void)
{
    s_random = s_random * 1664525 + 1013904223;
    return s_random;
}

/** A random float in [lo, hi). */
static f32 testRandom(f32 lo, f32 hi)
{
    return lo + (hi - lo) * (randomGetNext() >> 8) * (1.0f / 16777216.0f);
}

static void testRandomAngles(coord3d *angles)
{
    angles->x = testRandom(-M_PI_F, M_PI_F);
    angles->y = testRandom(-M_PI_F, M_PI_F);
    angles->z = testRandom(-M_PI_F, M_PI_F);
}

static bool testMtxNear(const Mtxf *a, const Mtxf *b, f32 tolerance)
{
    for (s32 i = 0; i < 4; i++)
    {
        for (s32 j = 0; j < 4; j++)
        {
            if (fabs(a->m[i][j] - b->m[i][j]) > tolerance)
            {
                return false;
            }
        }
    }

    return true;
}

static bool testVecNear(const coord3d *a, const coord3d *b, f32 tolerance)
{
    return fabs(a->x - b->x) <= tolerance && fabs(a->y - b->y) <= tolerance && fabs(a->z - b->z) <= tolerance;
}

static f32 testLength(const coord3d *v)
{
    return sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
}

/** The angle a in [0, 2pi) wrapped into (-pi, pi]. */
static double testWrap(double a)
{
    return a > M_PI ? a - 2 * M_PI : a;
}

static void testTrig(void)
{
    for (s32 i = 0; i < 200000; i++)
    {
        f32 x = testRandom(-1000.0f, 1000.0f);
        f32 small = testRandom(-1.6f, 1.6f);

        // Within a few float steps of the true values for the angles the game uses. (cosf is
        // a little worse: it adds a slightly low pi/2. Bigger angles lose more, because the
        // reduction's second part of pi is wrong; see trig.c.)
        CHECK_NEAR(sinf(small), sin(small), 1.5e-7);
        CHECK_NEAR(cosf(small), cos(small), 4e-7);
        CHECK_NEAR(sinf(x), sin(x), 1e-7 * fabs(x) + 1e-6);
        CHECK_NEAR(cosf(x), cos(x), 2e-7 * fabs(x) + 1e-6); // x + pi/2 rounds too
    }

    CHECK(sinf(0.0f) == 0.0f);
    CHECK(sinf(1e-20f) == 1e-20f);
    CHECK(sinf(1e30f) == 0.0f); // too big to reduce

    for (s32 i = 0; i < 200000; i++)
    {
        f32 y = testRandom(-100.0f, 100.0f);
        f32 x = testRandom(-100.0f, 100.0f);
        f32 c = testRandom(-1.0f, 1.0f);
        double expected = atan2(y, x);
        f32 angle = mathAtan2(y, x);

        // [0, 2pi), and as accurate as the acos table allows: about 5e-4 radians, but up to
        // 5e-3 (0.3 degrees) where acos is steep, near cosines of +-1. mathAtan2 goes there
        // for angles near 90 and 270 degrees.
        CHECK(angle >= 0.0f && angle < M_TAU_F);
        CHECK_NEAR(testWrap(angle), expected, 6e-3);
        CHECK_NEAR(mathAcos(c), acos(c), fabs(c) < 0.99f ? 6e-4 : 5e-3);
    }

    CHECK(mathAtan2(0.0f, 1.0f) == 0.0f);
    CHECK(mathAtan2(0.0f, -1.0f) == M_PI_F);
    CHECK(mathAtan2(1.0f, 0.0f) == M_HALF_PI);
    CHECK(mathAtan2(-1.0f, 0.0f) == M_THREE_HALF_PI);
    // Clamped to +-1, whose angles aren't quite 0 and pi: the table's last step stops short
    CHECK(mathAcos(2.0f) == mathAcos(1.0f));
    CHECK_NEAR(mathAcos(1.0f), 0.0, 3e-3);
    CHECK_NEAR(mathAcos(-2.0f), M_PI, 3e-3);
}

static void testRound(void)
{
    for (s32 i = 0; i < 200000; i++)
    {
        f32 x = testRandom(-100000.0f, 100000.0f) / (1 + i % 100);

        CHECK(mathFloor(x) == (f32) floor(x));
        CHECK(mathCeil(x) == (f32) ceil(x));
        CHECK(mathFloorToInt(x) == (s32) floor(x));
        CHECK(mathCeilToInt(x) == (s32) ceil(x));
    }

    CHECK(mathFloor(-2.0f) == -2.0f && mathCeil(-2.0f) == -2.0f);
    CHECK(mathFloorToInt(-0.5f) == -1 && mathCeilToInt(0.5f) == 1);
}

static void testMatrices(void)
{
    for (s32 i = 0; i < 20000; i++)
    {
        coord3d angles;
        coord3d angles2;
        coord3d pos = {{testRandom(-1000, 1000), testRandom(-1000, 1000), testRandom(-1000, 1000)}};
        coord3d v = {{testRandom(-10, 10), testRandom(-10, 10), testRandom(-10, 10)}};
        coord3d result;
        coord3d result2;
        Mtxf rx, ry, rz, rot, tmp, tmp2, place, inverse, product, identity;
        f32 scale = testRandom(0.1f, 5.0f);

        testRandomAngles(&angles);
        mtx4LoadIdentity(&identity);

        // mtx4LoadRotation is x, then y, then z
        mtx4LoadXRotation(angles.x, &rx);
        mtx4LoadYRotation(angles.y, &ry);
        mtx4LoadZRotation(angles.z, &rz);
        mtx4Mult(&ry, &rx, &tmp);
        mtx4Mult(&rz, &tmp, &tmp2);
        mtx4LoadRotation(&angles, &rot);
        CHECK(testMtxNear(&rot, &tmp2, 1e-5f));

        // The affine product agrees with the full one on affine matrices
        mtx4LoadRotationAndTranslation(&pos, &angles, &place);
        mtx4Mult(&place, &rot, &tmp);
        mtx4MultAffine(&place, &rot, &tmp2);
        CHECK(testMtxNear(&tmp, &tmp2, 1e-3f));

        // Rotating keeps lengths; transforming adds the translation
        mtx4RotateVec(&rot, &v, &result);
        CHECK_NEAR(testLength(&result), testLength(&v), 1e-4);
        mtx4TransformVec(&place, &v, &result2);
        mtx4RotateVec(&place, &v, &result);
        result.x += pos.x;
        result.y += pos.y;
        result.z += pos.z;
        CHECK(testVecNear(&result, &result2, 1e-3f));

        // The three inverses undo the matrix
        mtx4TransposeRotation(&rot, &inverse);
        mtx4Mult(&inverse, &rot, &product);
        CHECK(testMtxNear(&product, &identity, 1e-5f));

        tmp = place;
        mtx4Scale3x3(scale, &tmp);
        mtx4InvertRigid(&tmp, &inverse);
        mtx4MultAffine(&inverse, &tmp, &product);
        CHECK(testMtxNear(&product, &identity, 1e-3f));

        tmp = place;
        mtx4ScaleRow0(scale, &tmp);
        mtx4ScaleRow2(1.0f / scale, &tmp);
        mtx4InvertAffine(&tmp, &inverse);
        mtx4MultAffine(&inverse, &tmp, &product);
        CHECK(testMtxNear(&product, &identity, 1e-3f));

        // mtx4GetRotation gets the angles back (for y away from straight up or down)
        if (fabs(angles.y) < 1.5)
        {
            mtx4GetRotation(&rot, &angles2);
            CHECK_NEAR(testWrap(angles2.x), angles.x, 6e-3); // mathAtan2's accuracy
            CHECK_NEAR(testWrap(angles2.y), angles.y, 6e-3);
            CHECK_NEAR(testWrap(angles2.z), angles.z, 6e-3);
        }

        // Scaling rows scales the object, columns the result
        tmp = place;
        mtx4ScaleRows012(scale, &tmp);
        mtx4TransformVec(&tmp, &v, &result);
        coord3d scaled = {{v.x * scale, v.y * scale, v.z * scale}};
        mtx4TransformVec(&place, &scaled, &result2);
        CHECK(testVecNear(&result, &result2, 1e-3f));

        tmp = place;
        mtx4ScaleColumns012(scale, &tmp);
        mtx4TransformVec(&tmp, &v, &result);
        mtx4TransformVec(&place, &v, &result2);
        CHECK_NEAR(result.y, result2.y * scale, 2e-3 * scale);

        // The quaternion of some angles makes the same matrix
        quatf q;
        quatFromAngles(&angles, &q);
        quatToMtx4(&q, &tmp);
        CHECK(testMtxNear(&tmp, &rot, 1e-5f));

        // 3x3 matrices multiply like the 4x4 ones
        Mtx3f r3, p3;
        mtx4LoadRotation(&angles, &tmp);
        mtx4ToMtx3(&tmp, &r3);
        mtx4ToMtx3(&rot, &p3);
        mtx3MultInPlace(&r3, &p3);
        mtx3ToMtx4(&p3, &tmp2);
        mtx4Mult(&tmp, &rot, &product);
        CHECK(testMtxNear(&tmp2, &product, 1e-5f));
    }
}

static void testCameras(void)
{
    for (s32 i = 0; i < 20000; i++)
    {
        coord3d eye = {{testRandom(-1000, 1000), testRandom(-1000, 1000), testRandom(-1000, 1000)}};
        coord3d target = {{testRandom(-1000, 1000), testRandom(-1000, 1000), testRandom(-1000, 1000)}};
        coord3d result;
        Mtxf view, place, product, identity, align, alignInverse;
        f32 dist = sqrt(SQR(target.x - eye.x) + SQR(target.y - eye.y) + SQR(target.z - eye.z));

        mtx4LoadIdentity(&identity);

        // The view matrix moves the eye to 0 and the target onto the -z axis
        mtx4LoadLookAt(&view, eye.x, eye.y, eye.z, target.x, target.y, target.z, 0.0f, 1.0f, 0.0f);
        mtx4TransformVec(&view, &eye, &result);
        CHECK(testVecNear(&result, &(coord3d) {{0, 0, 0}}, 2e-3f));
        mtx4TransformVec(&view, &target, &result);
        CHECK(testVecNear(&result, &(coord3d) {{0, 0, -dist}}, dist * 1e-5f + 2e-3f));

        // ... and mtx4LoadFacing is its inverse
        mtx4LoadFacing(&place, eye.x, eye.y, eye.z, target.x, target.y, target.z, 0.0f, 1.0f, 0.0f);
        mtx4MultAffine(&view, &place, &product);
        CHECK(testMtxNear(&product, &identity, 1e-3f));

        // mtx4LoadAlign points -z along the direction, mtx4LoadAlignInverse undoes it
        f32 roll = testRandom(-M_PI_F, M_PI_F);
        coord3d dir = {{target.x - eye.x, target.y - eye.y, target.z - eye.z}};
        coord3d minusZ = {{0, 0, -1}};
        mtx4LoadAlign(&align, roll, dir.x, dir.y, dir.z);
        mtx4RotateVec(&align, &minusZ, &result);
        CHECK(testVecNear(&result, &(coord3d) {{dir.x / dist, dir.y / dist, dir.z / dist}}, 1e-5f));
        mtx4LoadAlignInverse(&alignInverse, roll, dir.x, dir.y, dir.z);
        mtx4Mult(&alignInverse, &align, &product);
        CHECK(testMtxNear(&product, &identity, 1e-5f));
    }

    // Straight up: no unique answer, so the identity
    Mtxf m;
    mtx4LoadAlign(&m, 1.0f, 0.0f, 5.0f, 0.0f);
    CHECK(m.m[0][0] == 1.0f && m.m[1][1] == 1.0f && m.m[0][1] == 0.0f);
}

static void testFixedPoint(void)
{
    for (s32 i = 0; i < 20000; i++)
    {
        Mtxf m, fixed, back;
        f32 worldScale = testRandom(0.5f, 2.0f);

        for (s32 j = 0; j < 16; j++)
        {
            m.m[j / 4][j % 4] = testRandom(-10000.0f, 10000.0f);
        }

        // There and back, to within one fixed-point step
        mtxSetWorldScale(worldScale);
        mtxF2L(&m, &fixed);
        mtxL2F(&fixed, &back);
        CHECK(testMtxNear(&m, &back, 1e-3f * 2));

        // The world scale applies to columns 0-2 only; the unit scale can be pushed and popped
        mtxPushUnitWorldScale();
        mtxF2L(&m, &fixed);
        mtxPopWorldScale();
        mtxSetWorldScale(1.0f);
        mtxL2F(&fixed, &back);
        CHECK(testMtxNear(&m, &back, 1e-4f));
    }

    // 1.5 in s15.16: integer half 0x0001, fraction half 0x8000
    Mtxf one = {{{1.5f, -1.0f, 0, 0}, {0}, {0}, {0}}};
    Mtxf fixed;
    mtxSetWorldScale(1.0f);
    mtxF2L(&one, &fixed);
    u32 *words = (u32 *) &fixed;
    CHECK(words[0] == 0x0001FFFF && words[8] == 0x80000000);

    CHECK(mtxCalcPerspNorm(0.0f, 1.0f) == 0xFFFF);
    CHECK(mtxCalcPerspNorm(10.0f, 22.0f) == 0x1000);
    CHECK(mtxCalcPerspNorm(0.0f, 1e9f) == 1);
}

static void testQuaternions(void)
{
    for (s32 i = 0; i < 20000; i++)
    {
        coord3d angles[4];
        quatf q[4];
        quatf result;
        Mtxf m1, m2;
        f32 t = testRandom(0.0f, 1.0f);

        for (s32 j = 0; j < 4; j++)
        {
            testRandomAngles(&angles[j]);
            quatFromAngles(&angles[j], &q[j]);
        }

        // Unit length, and -q is the same rotation
        CHECK_NEAR(q[0].w * q[0].w + q[0].x * q[0].x + q[0].y * q[0].y + q[0].z * q[0].z, 1.0, 1e-5);
        quatf negated = {{-q[0].w, -q[0].x, -q[0].y, -q[0].z}};
        quatToMtx4(&q[0], &m1);
        quatToMtx4(&negated, &m2);
        CHECK(testMtxNear(&m1, &m2, 1e-6f));

        // Slerp: the ends, the middle, and a steady unit length
        quatMakeShortestPath(&q[0], &q[1]);
        CHECK(quatDot(&q[0], &q[1]) >= 0.0f);
        quatSlerp(&q[0], &q[1], 0.0f, &result);
        CHECK(fabs(quatDot(&result, &q[0]) - 1.0) < 1e-4);
        quatSlerp(&q[0], &q[1], 1.0f, &result);
        CHECK(fabs(quatDot(&result, &q[1]) - 1.0) < 1e-4);
        quatSlerp(&q[0], &q[1], t, &result);
        CHECK_NEAR(quatDot(&result, &result), 1.0, 1e-3);

        // Half way from the identity is half the angle
        quatf half;
        quatSlerpFromIdentity(&q[2], 0.5f, &half);
        CHECK_NEAR(fabs(half.w), cos(acos(fabs(q[2].w)) / 2), 2e-4);

        // The spline passes through its middle two points
        quatMakeShortestPath(&q[1], &q[2]);
        quatMakeShortestPath(&q[2], &q[3]);
        quatMakeShortestPath(&q[1], &q[0]);
        quatSpline(&q[0], &q[1], &q[2], &q[3], 0.0f, &result);
        CHECK(fabs(fabs(quatDot(&result, &q[1])) - 1.0) < 1e-3);
        quatSpline(&q[0], &q[1], &q[2], &q[3], 1.0f, &result);
        CHECK(fabs(fabs(quatDot(&result, &q[2])) - 1.0) < 1e-3);
    }
}

static void testInterp(void)
{
    for (s32 i = 0; i < 20000; i++)
    {
        coord3d p[4];
        coord3d result;
        coord3d catmull;
        f32 t = testRandom(0.0f, 1.0f);

        for (s32 j = 0; j < 4; j++)
        {
            p[j] = (coord3d) {{testRandom(-100, 100), testRandom(-100, 100), testRandom(-100, 100)}};
        }

        interpLerp3(&p[0], &p[1], 0.0f, &result);
        CHECK(testVecNear(&result, &p[0], 1e-5f));
        interpLerp3(&p[0], &p[1], 1.0f, &result);
        CHECK(testVecNear(&result, &p[1], 1e-4f));

        // Both splines go from p1 to p2, and agree with tangent scale 0.5
        interpCardinal3(&p[0], &p[1], &p[2], &p[3], 0.0f, 0.5f, &result);
        CHECK(testVecNear(&result, &p[1], 1e-4f));
        interpCardinal3(&p[0], &p[1], &p[2], &p[3], 1.0f, 0.5f, &result);
        CHECK(testVecNear(&result, &p[2], 1e-4f));
        interpCardinal3(&p[0], &p[1], &p[2], &p[3], t, 0.5f, &result);
        interpCatmullRom3(&p[0], &p[1], &p[2], &p[3], t, &catmull);
        CHECK(testVecNear(&result, &catmull, 1e-3f));
    }
}

static void testPhysics(void)
{
    // A fall matches the closed form: h = v t - g t^2 / 2
    f32 height = 0.0f;
    f32 speed = 5.0f;

    for (s32 i = 0; i < 100; i++)
    {
        physFall(&height, &speed, 1.0f);
    }

    CHECK_NEAR(height, 5.0 * 100 - 0.2777778 * 100 * 100 / 2, 1e-2);
    CHECK_NEAR(speed, 5.0 - 0.2777778 * 100, 1e-4);

    for (s32 i = 0; i < 1000; i++)
    {
        coord3d velocity;
        Mtxf tumble;
        Mtxf mtx;
        Mtxf product;
        Mtxf identity;

        physRandomLaunch(&velocity, &tumble);
        CHECK(fabs(velocity.x) <= 3.34f && velocity.y >= 0.0f && velocity.y <= 6.67f && fabs(velocity.z) <= 3.34f);

        // A small rotation: close to the identity, and still a rotation after many ticks
        mtx4LoadIdentity(&identity);
        CHECK(testMtxNear(&tumble, &identity, 0.25f));
        mtx4LoadIdentity(&mtx);
        physApplyTumble(&mtx, &tumble, 30);
        mtx4TransposeRotation(&mtx, &product);
        mtx4MultAffineInPlace(&mtx, &product);
        CHECK(testMtxNear(&product, &identity, 1e-4f));
    }
}

static void testMoveTowards(void)
{
    // Doors: from closed to open, it arrives, never overshoots, and never goes over the
    // top speed
    for (s32 i = 0; i < 2000; i++)
    {
        f32 start = testRandom(-10, 10);
        f32 target = testRandom(-10, 10);
        f32 accel = testRandom(0.001f, 0.05f);
        f32 decel = testRandom(0.001f, 0.05f);
        f32 maxSpeed = testRandom(0.05f, 0.5f);
        f32 pos = start;
        f32 speed = 0.0f;
        s32 ticks = 0;

        while (pos != target && ticks < 100000)
        {
            f32 before = pos;

            physMoveTowards(&pos, target, &speed, accel, decel, maxSpeed, 1);
            ticks++;

            CHECK(fabs(speed) <= maxSpeed + 1e-6f);
            // Each tick moves towards the target (it brakes before it gets there)
            CHECK(fabs(target - pos) <= fabs(target - before));
        }

        CHECK(pos == target);

        // Several ticks at once are the same as one at a time, until it arrives (it
        // stops there; callers zero the speed then, or the next call would move it on)
        f32 pos1 = start;
        f32 speed1 = 0.0f;
        f32 pos5 = start;
        f32 speed5 = 0.0f;

        for (s32 j = 0; j < 5; j++)
        {
            physMoveTowards(&pos1, target, &speed1, accel, decel, maxSpeed, 1);
        }

        physMoveTowards(&pos5, target, &speed5, accel, decel, maxSpeed, 5);
        CHECK(pos5 == target || (pos1 == pos5 && speed1 == speed5));
    }

    // Moving away from the target brakes first, then comes back
    f32 pos = 0.0f;
    f32 speed = -0.3f;

    physMoveTowards(&pos, 10.0f, &speed, 0.01f, 0.1f, 0.5f, 1);
    CHECK_NEAR(speed, -0.2, 1e-6);
    CHECK_NEAR(pos, -0.2, 1e-6);

    // Angles turn the short way round and stay in [0, 2pi)
    for (s32 i = 0; i < 2000; i++)
    {
        f32 angle = testRandom(0, M_TAU_F - 0.001f);
        f32 target = testRandom(0, M_TAU_F - 0.001f);
        f32 turnSpeed = 0.0f;
        double shortWay = target - angle;

        if (shortWay < -M_PI_F) shortWay += M_TAU_F;
        if (shortWay >= M_PI_F) shortWay -= M_TAU_F;

        f32 before = angle;
        physTurnTowards(&angle, target, &turnSpeed, 0.01f, 0.01f, 0.1f, 1);
        CHECK(angle >= 0.0f && angle < M_TAU_F);

        double moved = angle - before;

        if (moved < -M_PI) moved += 2 * M_PI;
        if (moved > M_PI) moved -= 2 * M_PI;
        CHECK(moved * shortWay >= 0);

        for (s32 j = 0; j < 10000 && angle != target; j++)
        {
            physTurnTowards(&angle, target, &turnSpeed, 0.01f, 0.01f, 0.1f, 1);
        }

        // It ends on the target, or one turn away from it (stored in range)
        CHECK(angle == target || fabs(fabs(angle - target) - M_TAU_F) < 1e-5);
    }
}

static void testRaySphere(void)
{
    coord3d origin = {{0, 0, 0}};
    coord3d alongX = {{2, 0, 0}};

    CHECK(intersectRaySphere(&origin, &alongX, &(coord3d) {{100, 5, 0}}, 6.0f));
    CHECK(!intersectRaySphere(&origin, &alongX, &(coord3d) {{100, 7, 0}}, 6.0f));
    // Behind the start
    CHECK(!intersectRaySphere(&origin, &alongX, &(coord3d) {{-100, 0, 0}}, 6.0f));

    // Random rays against the distance from the line, in doubles
    for (s32 i = 0; i < 100000; i++)
    {
        coord3d start = {{testRandom(-1000, 1000), testRandom(-1000, 1000), testRandom(-1000, 1000)}};
        coord3d dir = {{testRandom(-1, 1), testRandom(-1, 1), testRandom(-1, 1)}};
        coord3d centre = {{testRandom(-1000, 1000), testRandom(-1000, 1000), testRandom(-1000, 1000)}};
        f32 radius = testRandom(1, 300);
        double tx = centre.x - start.x;
        double ty = centre.y - start.y;
        double tz = centre.z - start.z;
        double along = (tx * dir.x + ty * dir.y + tz * dir.z) / sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        double lineDist = sqrt(fabs(tx * tx + ty * ty + tz * tz - along * along));

        // Skip the cases too close to call in floats
        if (fabs(lineDist - radius) > 0.5 && fabs(along) > 0.5)
        {
            CHECK(intersectRaySphere(&start, &dir, &centre, radius) == (along > 0 && lineDist <= radius));
        }
    }
}

static void testRayTriangle(void)
{
    coord16 v0 = {{0, 0, 0}};
    coord16 v1 = {{100, 0, 0}};
    coord16 v2 = {{0, 100, 0}};
    coord3d offset = {{1000, 0, 0}};
    coord3d start = {{1010, 10, 50}};
    coord3d down = {{0, 0, -1}};
    coord3d hit;
    coord3d normal;

    // Straight down onto the triangle (moved by the offset)
    CHECK(intersectRayTriangle(&v0, &v1, &v2, &offset, &start, &start, &down, &hit, &normal));
    CHECK(testVecNear(&hit, &(coord3d) {{1010, 10, 0}}, 1e-4f));
    CHECK(normal.x == 0.0f && normal.y == 0.0f && normal.z != 0.0f);

    // Pointing away: the line hits, but behind the ray's start
    coord3d up = {{0, 0, 1}};
    CHECK(!intersectRayTriangle(&v0, &v1, &v2, &offset, &start, &start, &up, &hit, &normal));

    // Outside the triangle, along its plane, and a triangle with no area
    coord3d outside = {{1080, 80, 50}};
    coord3d along = {{1, 0, 0}};
    CHECK(!intersectRayTriangle(&v0, &v1, &v2, &offset, &outside, &outside, &down, &hit, &normal));
    CHECK(!intersectRayTriangle(&v0, &v1, &v2, &offset, &start, &start, &along, &hit, &normal));
    CHECK(!intersectRayTriangle(&v0, &v1, &v1, &offset, &start, &start, &down, &hit, &normal));

    // Random rays at random triangles: a reported hit lies on the triangle's plane and the ray
    for (s32 i = 0; i < 100000; i++)
    {
        coord16 a = {{(s16) testRandom(-2000, 2000), (s16) testRandom(-2000, 2000), (s16) testRandom(-2000, 2000)}};
        coord16 b = {{(s16) testRandom(-2000, 2000), (s16) testRandom(-2000, 2000), (s16) testRandom(-2000, 2000)}};
        coord16 c = {{(s16) testRandom(-2000, 2000), (s16) testRandom(-2000, 2000), (s16) testRandom(-2000, 2000)}};
        coord3d from = {{testRandom(-3000, 3000), testRandom(-3000, 3000), testRandom(-3000, 3000)}};
        coord3d centre = {{(a.x + b.x + c.x) / 3.0f + testRandom(-300, 300), (a.y + b.y + c.y) / 3.0f, (a.z + b.z + c.z) / 3.0f}};
        coord3d dir = {{centre.x - from.x, centre.y - from.y, centre.z - from.z}};
        coord3d zero = {{0, 0, 0}};

        if (intersectRayTriangle(&a, &b, &c, &zero, &from, &from, &dir, &hit, &normal))
        {
            double planeDist = normal.x * (hit.x - a.x) + normal.y * (hit.y - a.y) + normal.z * (hit.z - a.z);
            double normalLength = testLength(&normal);
            double t = (hit.x - from.x) * dir.x + (hit.y - from.y) * dir.y + (hit.z - from.z) * dir.z;

            CHECK(fabs(planeDist) / normalLength < 0.05);
            CHECK(t >= -1e-3);
        }
    }
}

int main(void)
{
    testTrig();
    testRound();
    testMatrices();
    testCameras();
    testFixedPoint();
    testQuaternions();
    testInterp();
    testPhysics();
    testMoveTowards();
    testRaySphere();
    testRayTriangle();

    if (s_failures != 0)
    {
        printf("math: %d checks failed\n", s_failures);
        return 1;
    }

    printf("math: all tests passed\n");
    return 0;
}
