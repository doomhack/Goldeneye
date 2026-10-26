#ifndef _TRIG_H_
#define _TRIG_H_

#include <ultra64.h>

/* Rare's inverse trig, see trig.c for how it differs from the C library. */
f32 mathAtan2(f32 y, f32 x);
f32 mathAcos(f32 cosine);
f32 mathAsin(f32 sine);
u16 mathAcosFixed(s16 cosine);
s16 mathAsinFixed(s16 sine);

/* In math_sincos.s. */
extern float cosf(float);
extern float sinf(float);

#endif
