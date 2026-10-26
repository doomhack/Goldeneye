/**
 * Inverse trigonometry: atan2, asin and acos.
 *
 * These are Rare's own versions, not the C library's, and they differ from it:
 *  - mathAtan2 returns an angle in [0, 2pi), not (-pi, pi].
 *  - mathAcos and mathAsin work in 16-bit fixed point through a lookup table, so they are
 *    only accurate to about 1/65536 of pi.
 *
 * The fixed-point versions take a sine or cosine scaled so that 1.0 is 0x7FFF, and return
 * an angle scaled so that pi is 0x10000.
 *
 * sinf and cosf are in math_sincos.s.
 */
#include <ultra64.h>
#include <math.h>
#include "game/math/trig.h"

/**
 * Returns the angle of the vector (x, y) from the +x axis towards +y, like the C library's
 * atan2(y, x) but in [0, 2pi) instead of (-pi, pi]. The game usually calls it as
 * mathAtan2(dx, dz), which gives a heading measured from +z towards +x.
 */
f32 mathAtan2(f32 y, f32 x)
{
    f32 angle;

    if (y == 0.0f)
    {
        if (0.0f <= x)
        {
            angle = 0.0f;
        }
        else
        {
            angle = M_PI_F;
        }
    }
    else if (x == 0.0f)
    {
        if (0.0f < y)
        {
            angle = M_HALF_PI;
        }
        else
        {
            angle = M_THREE_HALF_PI;
        }
    }
    else
    {
        angle = sqrtf((y * y) + (x * x));

        if (x < y)
        {
            angle = mathAcos(x / angle);

            if (y < 0.0f)
            {
                angle = M_TAU_F - angle;
            }
        }
        else
        {
            angle = M_HALF_PI - mathAcos(y / angle);

            if (x < 0.0f)
            {
                angle = M_PI_F - angle;
            }

            if (angle < 0.0f)
            {
                angle += M_TAU_F;
            }
        }
    }

    return angle;
}

/*
 * acos lookup tables. The input is a cosine from 0 to 0x7FFF, the output an angle where
 * 0x8000 is pi/2. acos gets steep near 1, so the tables get finer there:
 *   coarse: inputs 0x0000-0x7800, one entry every 0x200
 *   fine:   inputs 0x7800-0x7FE0, one entry every 0x20
 *   finest: inputs 0x7FE0-0x7FFF, one entry every 0x8
 * Each table's first entry repeats the previous table's last.
 */
u16 m_acosTableCoarse[] = {
    0x8000,0x7EBA,0x7D74,0x7C2D,0x7AE7,0x79A0,0x7859,0x7711,0x75C9,0x7480,
    0x7337,0x71EC,0x70A1,0x6F55,0x6E07,0x6CB8,0x6B68,0x6A17,0x68C4,0x6770,
    0x661A,0x64C1,0x6367,0x620B,0x60AD,0x5F4C,0x5DE9,0x5C83,0x5B1A,0x59AE,
    0x583E,0x56CB,0x5555,0x53DB,0x525C,0x50D9,0x4F51,0x4DC5,0x4C32,0x4A9A,
    0x48FC,0x4757,0x45AB,0x43F7,0x423A,0x4075,0x3EA5,0x3CCB,0x3AE5,0x38F1,
    0x36EF,0x34DC,0x32B7,0x307D,0x2E2B,0x2BBD,0x292E,0x2678,0x2391,0x206C,
    0x1CF6,     0
};

u16 m_acosTableFine[] = {
    0x1CF6,0x1CBB,0x1C80,0x1C45,0x1C08,0x1BCC,0x1B8F,0x1B51,0x1B13,0x1AD4,
    0x1A95,0x1A55,0x1A14,0x19D3,0x1992,0x194F,0x190C,0x18C9,0x1884,0x183F,
    0x17F9,0x17B3,0x176B,0x1723,0x16DA,0x1690,0x1645,0x15F9,0x15AC,0x155E,
    0x150F,0x14BE,0x146D,0x141A,0x13C6,0x1370,0x1319,0x12C1,0x1267,0x120B,
    0x11AD,0x114E,0x10EC,0x1088,0x1022, 0xFB9, 0xF4D, 0xEDE, 0xE6C, 0xDF7,
    0xD7D, 0xD00, 0xC7D, 0xBF4, 0xB66, 0xAD0, 0xA31, 0x989, 0x8D3, 0x80E,
    0x734, 0x63D, 0x518, 0x39A
};

u16 m_acosTableFinest[] = {
    0x39A, 0x31E, 0x28C, 0x1CD,     0,     0
};

/**
 * acos of a cosine in [0, 0x7FFF], interpolating linearly between table entries.
 * Returns an angle where 0x8000 is pi/2 (so 0 for a cosine of 1).
 */
static s32 acosFixedFromTable(s32 cosine)
{
    s32 mask;
    u16 *table;
    s32 shift;

    s32 above;
    s32 below;
    s32 step;

    if (cosine >= 0x7FE0)
    {
        mask = 7;
        shift = 3;
        table = &m_acosTableFinest;
        cosine = cosine - 0x7FE0;
    }
    else if (cosine >= 0x7800)
    {
        mask = 0x1F;
        shift = 5;
        table = &m_acosTableFine;
        cosine = cosine - 0x7800;
    }
    else
    {
        mask = 0x1FF;
        shift = 9;
        table = &m_acosTableCoarse;
    }

    above = table[cosine >> shift];
    below = table[(cosine >> shift) + 1];
    step = ((above - below) * (cosine & mask)) >> shift;
    return above - step;
}

/**
 * acos in fixed point: the cosine is scaled so 1.0 is 0x7FFF, the result so pi is 0x10000
 * (0xFFFF for a cosine of -1).
 */
u16 mathAcosFixed(s16 cosine)
{
    s32 angle;

    if (cosine >= 0)
    {
        angle = cosine;
    }
    else
    {
        angle = -cosine;
    }

    angle = acosFixedFromTable(angle);

    if (cosine < 0)
    {
        angle = 0xFFFF - angle;
    }

    return (u16)angle;
}

/**
 * asin in fixed point: the sine is scaled so 1.0 is 0x7FFF, the result so pi/2 is 0x7FFF.
 */
s16 mathAsinFixed(s16 sine)
{
    s32 acosAngle;
    s32 angle;

    if (sine >= 0)
    {
        angle = sine;
    }
    else
    {
        angle = -sine;
    }

    acosAngle = acosFixedFromTable(angle);

    if (sine >= 0)
    {
        angle = 0x7FFF - acosAngle;
    }
    else
    {
        angle = acosAngle - 0x8000;
    }

    return (s16)angle;
}

/** acos in radians, through the fixed-point table. Inputs outside [-1, 1] are clamped. */
f32 mathAcos(f32 cosine)
{
    s16 cosineFixed;

    if (1.0f <= cosine)
    {
        cosineFixed = 0x7FFF;
    }
    else if (cosine <= -1.0f)
    {
        cosineFixed = -0x7FFF;
    }
    else
    {
        cosineFixed = (cosine * 32767.0f);
    }

    return (mathAcosFixed(cosineFixed) * M_PI_F) / 65535.0f;
}

/** asin in radians, through the fixed-point table. Inputs outside [-1, 1] are clamped. */
f32 mathAsin(f32 sine)
{
    s16 sineFixed;

    if (1.0f <= sine)
    {
        sineFixed = 0x7FFF;
    }
    else if (sine <= -1.0f)
    {
        sineFixed = -0x7FFF;
    }
    else
    {
        sineFixed = (sine * 32767.0f);
    }

    return (mathAsinFixed(sineFixed) * M_PI_F) / 65535.0f;
}
