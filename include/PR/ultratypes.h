#ifndef _ULTRATYPES_H_
#define _ULTRATYPES_H_

/* The game's basic types. This replaces the library's own PR/ultratypes.h for game code (the
 * include path finds this one first): the library makes s32 and u32 "long", but the game uses
 * them interchangeably with int and with its enums, so here they stay int. Both are 32 bits,
 * so the library, built with its own header, calls and returns the same values. */

typedef unsigned char               u8;  /* unsigned  8-bit */
typedef unsigned short              u16; /* unsigned 16-bit */
typedef unsigned int                u32; /* unsigned 32-bit */
typedef unsigned long long          u64; /* unsigned 64-bit */

typedef signed char                 s8;  /* signed  8-bit */
typedef short                       s16; /* signed 16-bit */
typedef int                         s32; /* signed 32-bit */
typedef long long                   s64; /* signed 64-bit */

typedef volatile unsigned char      vu8;
typedef volatile unsigned short     vu16;
typedef volatile unsigned int       vu32;
typedef volatile unsigned long long vu64;

typedef volatile signed char        vs8;
typedef volatile short              vs16;
typedef volatile int                vs32;
typedef volatile long long          vs64;

typedef float                       f32; /* single prec floating point */
typedef double                      f64; /* double prec floating point */

#ifndef NULL
#define NULL ((void *)0)
#endif

#include <stddef.h>
#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif /* _ULTRATYPES_H_ */
