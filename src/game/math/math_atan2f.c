/**
* Isnt mathAtan2 a libc file? 
* there doesnt seem to be any source for it (SDK or IRIX) so maybe part of cc itself?
*/

#include <ultra64.h>
#include <math.h>
#include "game/math/math_asinfacosf.h"
#include "game/math/math_atan2f.h"

f32 mathAtan2(f32 y, f32 x)
{
    f32 angle;
    if (y == 0.0f) {
        if (0.0f <= x) {
            angle = 0.0f;
        } else {
            angle = M_PI_F;
        }
    } else if (x == 0.0f) {
        if (0.0f < y) {
            angle = M_HALF_PI;
        } else {
            angle = M_THREE_HALF_PI;
        }
    } else {
        angle = sqrtf((y * y) + (x * x));
        if (x < y) {
            angle = mathAcos(x / angle);
            if (y < 0.0f) {
                angle = M_TAU_F - angle;
            }
        } else {
            angle = M_HALF_PI - mathAcos(y / angle);
            if (x < 0.0f) {
                angle = M_PI_F - angle;
            }
            if (angle < 0.0f) {
                angle += M_TAU_F;
            }
        }
    }

    return angle;
}

