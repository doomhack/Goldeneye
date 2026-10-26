/**
 * Rounding floats down and up, to a float or to an integer.
 *
 * They convert through s32, so they only work for values that fit in one.
 */
#include <ultra64.h>
#include "game/math/round.h"

/** Rounds towards negative infinity. */
f32 mathFloor(f32 value)
{
    f32 truncated;

    if (value >= 0.0f)
    {
        return (f32) (s32) value;
    }

    // Truncating a negative number rounds it up, so step down unless it was already whole.
    truncated = (f32) (s32) value;

    if (value == truncated)
    {
        return truncated;
    }

    return truncated - 1.0f;
}

/** Rounds towards negative infinity, returning an integer. */
s32 mathFloorToInt(f32 value)
{
    s32 truncated;

    if (value >= 0.0f)
    {
        return (s32) value;
    }

    truncated = (s32) value;

    if (value == (f32) truncated)
    {
        return truncated;
    }

    return truncated - 1;
}

/** Rounds towards positive infinity. */
f32 mathCeil(f32 value)
{
    f32 truncated;

    if (value <= 0.0f)
    {
        return (f32) (s32) value;
    }

    // Truncating a positive number rounds it down, so step up unless it was already whole.
    truncated = (f32) (s32) value;

    if (value == truncated)
    {
        return truncated;
    }

    return truncated + 1.0f;
}

/** Rounds towards positive infinity, returning an integer. */
s32 mathCeilToInt(f32 value)
{
    s32 truncated;

    if (value <= 0.0f)
    {
        return (s32) value;
    }

    truncated = (s32) value;

    if (value == (f32) truncated)
    {
        return truncated;
    }

    return truncated + 1;
}
