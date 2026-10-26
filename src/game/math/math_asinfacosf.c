#include <ultra64.h>
#include "game/math/math_asinacos.h"
#include "game/math/math_asinfacosf.h"

f32 mathAcos(f32 cosinef)
{
    s16 cosines;

    if (1.0f <= cosinef) {
        cosines = 0x7FFF;
    } else if (cosinef <= -1.0f) {
        cosines = -0x7FFF;
    } else {
        cosines = (cosinef * 32767.0f);
    }

    return (mathAcosFixed(cosines) * M_PI_F) / 65535.0f;
}

f32 mathAsin(f32 sinef)
{
    s16 sines;

    if (1.0f <= sinef) {
        sines = 0x7FFF;
    } else if (sinef <= -1.0f) {
        sines = -0x7FFF;
    } else {
        sines = (sinef * 32767.0f);
    }

    return (mathAsinFixed(sines) * M_PI_F) / 65535.0f;
}
