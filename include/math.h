

#ifndef _MATH_EXT_H_
#define _MATH_EXT_H_

/* Some useful constants */
/*  The base of natural logarithms, e.*/
#define M_E        2.7182818284590452354
/*  The base 2 logarithm of e. (log2e)*/
#define M_LOG2E    1.4426950408889634074
/*  The base 10 logarithm of e. (log10e)*/
#define M_LOG10E   0.43429448190325182765
/*  The natural logarithm of 2. (ln 2)*/
#define M_LN2      0.69314718055994530942
/*  The natural logarithm of 10. (ln 10)*/
#define M_LN10     2.30258509299404568402
/*  The circumference of a circle with diameter 1, PI.*/
#define M_PI       3.14159265358979323846
/*  Half of PI. (PI/2)*/
#define M_PI_2     1.57079632679489661923
/*  A quarter of PI. (PI/4)*/
#define M_PI_4     0.78539816339744830962
/*  The inverse of PI. (1/PI)*/
#define M_1_PI     0.31830988618379067154
/*	Twice the inverse of PI. (2/PI)*/
#define M_2_PI     0.63661977236758134308
/*	The inverse of the square root of PI. (2/(sqrtPI))*/
#define M_2_SQRTPI 1.12837916709551257390
/*  The square root of 2. (sqrt2) */
#define M_SQRT2    1.41421356237309504880
/*	The inverse of the square root of 2. 1/(sqrt2)*/
#define M_SQRT1_2  0.70710678118654752440

// extra defines

/* The circumference of a circle with diameter 1, PI.*/
#define M_PI_F        3.1415927f
/* The circumference of a circle with diameter 1, PI.*/
#define M_MINUS_PI_F -3.1415927f

/* The circumference of a circle with radius 1, PI*2.*/
#define M_TAU         6.28318530717958647692
/* The circumference of a circle with radius 1, PI*2.*/
#define M_TAU_F       6.2831855f

#define M_PI_2F     1.5707964f
#define M_LN2F      0.69813174f


/* Half of PI. (PI/2)*/
#define M_HALF_PI       (M_PI_F / 2)
/* 3 Halves of PI. (3*(PI/2))*/
#define M_THREE_HALF_PI (3 * M_HALF_PI)


#define M_U16_MAX_VALUE_F 65536.0f
#define M_U32_MAX_VALUE_F 4294967296.0f
/*
 The difference between 1.0 and the next larger floating point number
*/
#define FLT_EPSILON       1.19209290E-07F

/*Inline Math Funcs*/


#define SECS_TO_TIMER60(SECS) (SECS * GAME_TICKRATE)
#define MINS_TO_TIMER60(MINS) (SECS_TO_TIMER60(MINS * GAME_TICKRATE))
/* Gets the byte representation of an angle (degrees) */
#define DEG2BYTE(DEG)         (char)(256.0f / 360.0f * (DEG))
/* Gets the byte representation of an angle (radians) */
#define RAD2BYTE(RAD)         (char)(256.0f / M_TAU_F * (RAD))
/* Converts an angle from Degrees to Radians */
#define DegToRad(DEG)         (float)((DEG) * M_TAU_F / 360.0f)
/* Converts an angle from Degrees to Radians with only one multiply*/
#define DegToRad1Fact(DEG)         (float)((DEG) * (float)(M_TAU / 360.0))
/* Converts and halves an angle from Degrees to Radians */
#define mDegToHalfRad(x)      ((x * M_PI_F) / 360.0f)
/* Converts an angle from Radians to Degrees */
#define RadToDeg(RAD)         (float)((RAD) * (360.0f / M_TAU_F))
/* Gets the angle in radians represented by a byte */
#define ByteToRadian(Byte)    ((Byte * M_TAU_F) * (1.0f / 256.0f))

/* Macro Versions of functions */

#define ABS(x)    ((x) < 0 ? -(x) : (x))          /* absolute value of x */
#define SGN(x)    ((x) < 0 ? -1 : (x) > 0 ? 1 : 0)/* sign of x */
#define MIN(x, y) ((x) < (y) ? (x) : (y))         /* minimum of x and y */
#define MAX(x, y) ((x) > (y) ? (x) : (y))         /* maximum of x and y */
#define SQR(x)    ((x) * (x))                     /* square of x */

/**
 * 0.1f
*/
#define IDO_POINT_ONE 0.10000001f

float  sinf(float);
double sin(double);
float  cosf(float);
double cos(double);

float  sqrtf(float);
#if defined(__GNUC__) && !defined(sqrtf)
/* One sqrt.s instruction instead of a call (-fno-builtin stops GCC doing this itself) */
#define sqrtf(x) __builtin_sqrtf(x)
#endif

#endif

