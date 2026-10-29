/**
 * Host tests for the floor-plane geometry (src/game/nav/geometry.c). See run.sh.
 *
 * Collision depends on the exact results of these functions, so a rewrite must keep them
 * bit for bit (the demos check that on the N64). These tests check that the results are
 * also right: against double-precision maths, or a brute-force answer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/game/nav/geometry.c"

double sqrt(double);
double fabs(double);

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

static u32 testRandomU32(void)
{
    s_random = s_random * 1664525 + 1013904223;
    return s_random;
}

/** A random float in [lo, hi). */
static f32 testRandom(f32 lo, f32 hi)
{
    return lo + (hi - lo) * (testRandomU32() >> 8) * (1.0f / 16777216.0f);
}

/** The distance from p to the segment a-b, in doubles */
static double segmentDist(double ax, double az, double bx, double bz, double px, double pz)
{
    double dx = bx - ax;
    double dz = bz - az;
    double lengthSq = dx * dx + dz * dz;
    double t = lengthSq > 0 ? ((px - ax) * dx + (pz - az) * dz) / lengthSq : 0;

    t = t < 0 ? 0 : t > 1 ? 1 : t;
    dx = ax + t * dx - px;
    dz = az + t * dz - pz;

    return sqrt(dx * dx + dz * dz);
}

static void testDistances(void)
{
    s32 i;

    CHECK(geoDist2d(1, 2, 4, 6) == 5.0f);
    CHECK(geoDist2d(3, 3, 3, 3) == 0.0f);

    /* Left of a->b is positive: with x right and z down, a line going right has its left above */
    CHECK(geoSignedLineDist(0, 0, 10, 0, 5, -3) == 3.0f);
    CHECK(geoSignedLineDist(0, 0, 10, 0, 5, 3) == -3.0f);
    CHECK(geoSignedLineDist(0, 0, 10, 0, 20, -3) == 3.0f); /* the whole line, not the segment */
    CHECK(geoSignedLineDist(2, 2, 2, 2, 5, 6) == 5.0f);  /* a point: the distance from it */

    for (i = 0; i < 100000; i++)
    {
        f32 ax = testRandom(-1000, 1000), az = testRandom(-1000, 1000);
        f32 bx = testRandom(-1000, 1000), bz = testRandom(-1000, 1000);
        f32 px = testRandom(-1000, 1000), pz = testRandom(-1000, 1000);
        double dx = (double) bx - ax, dz = (double) bz - az;
        double expected = (dz * (px - ax) - dx * (pz - az)) / sqrt(dx * dx + dz * dz);

        CHECK_NEAR(geoDist2d(ax, az, px, pz), sqrt(((double) px - ax) * (px - ax) + ((double) pz - az) * (pz - az)), 1e-3);
        CHECK_NEAR(geoSignedLineDist(ax, az, bx, bz, px, pz), expected, 1e-2);
    }
}

static void testProjection(void)
{
    s32 i;

    CHECK(geoProjectsOntoSegment(0, 0, 10, 0, 5, 7));
    CHECK(geoProjectsOntoSegment(0, 0, 10, 0, 5, -7));
    CHECK(!geoProjectsOntoSegment(0, 0, 10, 0, -1, 7));
    CHECK(!geoProjectsOntoSegment(0, 0, 10, 0, 11, 7));
    CHECK(!geoProjectsOntoSegment(0, 0, 10, 0, 0, 7));  /* the ends don't count */
    CHECK(!geoProjectsOntoSegment(0, 0, 10, 0, 10, 7));
    CHECK(!geoProjectsOntoSegment(3, 3, 3, 3, 3, 3));

    for (i = 0; i < 100000; i++)
    {
        f32 ax = testRandom(-100, 100), az = testRandom(-100, 100);
        f32 bx = testRandom(-100, 100), bz = testRandom(-100, 100);
        f32 px = testRandom(-100, 100), pz = testRandom(-100, 100);
        double dx = (double) bx - ax, dz = (double) bz - az;
        double t = ((px - ax) * dx + (pz - az) * dz) / (dx * dx + dz * dz);

        if (t > 1e-4 && t < 1 - 1e-4)
        {
            CHECK(geoProjectsOntoSegment(ax, az, bx, bz, px, pz));
        }
        else if (t < -1e-4 || t > 1 + 1e-4)
        {
            CHECK(!geoProjectsOntoSegment(ax, az, bx, bz, px, pz));
        }
    }
}

static void testSegments(void)
{
    coord2d start = {.f = {0, 0}};
    coord2d end = {.f = {10, 0}};
    coord2d edgeA = {.f = {4, -5}};
    coord2d edgeB = {.f = {4, 5}};
    coord2d farA = {.f = {14, -5}};
    coord2d farB = {.f = {14, 5}};
    s32 i;

    /* An X crosses either way; a T (one end on the other segment) only when touching counts */
    CHECK(geoSegmentsCross(0, 0, 10, 10, 0, 10, 10, 0, FALSE));
    CHECK(geoSegmentsCross(0, 0, 10, 10, 0, 10, 10, 0, TRUE));
    CHECK(!geoSegmentsCross(0, 0, 10, 0, 5, 0, 5, 10, FALSE));
    CHECK(geoSegmentsCross(0, 0, 10, 0, 5, 0, 5, 10, TRUE));
    CHECK(!geoSegmentsCross(0, 0, 10, 0, 5, 1, 5, 10, TRUE));
    CHECK(!geoSegmentsCross(0, 0, 10, 0, 11, -5, 11, 5, TRUE));
    CHECK(geoSegmentsIntersect(0, 0, 10, 0, 5, 0, 5, 10));

    CHECK(geoSegmentHitFrac(&start, &end, &edgeA, &edgeB) == 0.4f);
    CHECK(geoSegmentHitFrac(&start, &end, &farA, &farB) == 1.0f);   /* beyond the end */
    CHECK(geoSegmentHitFrac(&end, &start, &farA, &farB) == 1.0f);   /* behind the start */
    CHECK(geoSegmentHitFrac(&start, &end, &start, &end) == 1.0f);   /* parallel */

    for (i = 0; i < 100000; i++)
    {
        f32 ax = testRandom(-100, 100), az = testRandom(-100, 100);
        f32 bx = testRandom(-100, 100), bz = testRandom(-100, 100);
        f32 cx = testRandom(-100, 100), cz = testRandom(-100, 100);
        f32 dx = testRandom(-100, 100), dz = testRandom(-100, 100);
        /* Where the lines meet, along a-b (s) and along c-d (t) */
        double ex = (double) bx - ax, ez = (double) bz - az;
        double fx = (double) dx - cx, fz = (double) dz - cz;
        double denom = ex * fz - ez * fx;
        double s = ((cx - ax) * fz - (cz - az) * fx) / denom;
        double t = ((cx - ax) * ez - (cz - az) * ex) / denom;
        bool crosses = s > 0 && s < 1 && t > 0 && t < 1;
        coord2d a = {.f = {ax, az}}, b = {.f = {bx, bz}}, c = {.f = {cx, cz}}, d = {.f = {dx, dz}};

        if (fabs(denom) < 1e-3 || fabs(s) < 1e-4 || fabs(s - 1) < 1e-4 || fabs(t) < 1e-4 || fabs(t - 1) < 1e-4)
        {
            continue;
        }

        CHECK(geoSegmentsCross(ax, az, bx, bz, cx, cz, dx, dz, FALSE) == crosses);
        CHECK(geoSegmentsCross(ax, az, bx, bz, cx, cz, dx, dz, TRUE) == crosses);

        if (s > 0 && s < 1)
        {
            CHECK_NEAR(geoSegmentHitFrac(&a, &b, &c, &d), s, 1e-3);
        }
        else
        {
            CHECK(geoSegmentHitFrac(&a, &b, &c, &d) == 1.0f);
        }
    }
}

/** The fraction of move a circle can go before touching the segment, by stepping along it */
static double sweepByStepping(f32 radius, const coord2d *centre, const coord2d *move, const coord2d *a, const coord2d *b)
{
    s32 steps = 20000;
    s32 i;

    for (i = 0; i <= steps; i++)
    {
        double t = (double) i / steps;
        double x = centre->f[0] + t * move->f[0];
        double z = centre->f[1] + t * move->f[1];

        if (segmentDist(a->f[0], a->f[1], b->f[0], b->f[1], x, z) <= radius)
        {
            return t;
        }
    }

    return 1.0;
}

static void testCircleSweep(void)
{
    coord2d centre = {.f = {0, 0}};
    coord2d move = {.f = {10, 0}};
    coord2d wallA = {.f = {8, -5}};
    coord2d wallB = {.f = {8, 5}};
    coord2d zero = {.f = {0, 0}};
    s32 tested = 0;
    s32 i;

    CHECK(geoCircleSweepFrac(1.0f, &centre, &move, &wallA, &wallB) == 0.7f);
    CHECK(geoCircleSweepFrac(1.0f, &centre, &zero, &wallA, &wallB) == 1.0f);

    for (i = 0; i < 200000 && tested < 3000; i++)
    {
        f32 radius = testRandom(0.5f, 5);
        coord2d c = {.f = {testRandom(-20, 20), testRandom(-20, 20)}};
        coord2d m = {.f = {testRandom(-30, 30), testRandom(-30, 30)}};
        coord2d a = {.f = {testRandom(-20, 20), testRandom(-20, 20)}};
        coord2d b = {.f = {testRandom(-20, 20), testRandom(-20, 20)}};
        double moveLen = sqrt((double) m.f[0] * m.f[0] + (double) m.f[1] * m.f[1]);
        double expected;

        /* Starting clear of the segment: it stops where the circle first touches */
        if (segmentDist(a.f[0], a.f[1], b.f[0], b.f[1], c.f[0], c.f[1]) <= radius + 0.01 || moveLen < 1)
        {
            continue;
        }

        expected = sweepByStepping(radius, &c, &m, &a, &b);

        /*
         * Only hits on the middle of the segment. Moving away from it, it gives 0, and for
         * a hit on one of its ends it sometimes picks the wrong end (about one in fifteen
         * of these): Rare's, kept as it is.
         */
        if (expected < 1.0)
        {
            double tx = c.f[0] + expected * m.f[0];
            double tz = c.f[1] + expected * m.f[1];
            double dx = (double) b.f[0] - a.f[0];
            double dz = (double) b.f[1] - a.f[1];
            double along = ((tx - a.f[0]) * dx + (tz - a.f[1]) * dz) / (dx * dx + dz * dz);

            if (along > 0.01 && along < 0.99)
            {
                tested++;
                CHECK_NEAR(geoCircleSweepFrac(radius, &c, &m, &a, &b), expected, 1e-3 + 2.0 / 20000);
            }
        }
    }

    CHECK(tested == 3000);
}

int main(void)
{
    testDistances();
    testProjection();
    testSegments();
    testCircleSweep();

    if (s_failures != 0)
    {
        printf("geotest: %d failures\n", s_failures);
        return 1;
    }

    printf("geotest: ok\n");
    return 0;
}
