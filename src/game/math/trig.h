#ifndef _TRIG_H_
#define _TRIG_H_

#include <ultra64.h>

/* Rare's inverse trig, see trig.c for how it differs from the C library. */
f32 mathAtan2(f32 y, f32 x);
f32 mathAcos(f32 cosine);

/* Replace the C library's sinf and cosf, see trig.c. */
f32 sinf(f32 x);
f32 cosf(f32 x);

#endif
