/**
 * Runtime helpers that GCC expects from libgcc, built only when COMPILER=gcc.
 * The 64-bit helpers forward to the equivalents libultra already provides for IDO.
 */
#include <ultra64.h>

typedef long long s64_t;
typedef unsigned long long u64_t;

extern s64_t __ll_div(s64_t a0, s64_t a1);
extern s64_t __ll_rem(u64_t a0, s64_t a1);
extern u64_t __ull_div(u64_t a0, u64_t a1);
extern u64_t __ull_rem(u64_t a0, u64_t a1);
extern s64_t __d_to_ll(double d);
extern s64_t __f_to_ll(float f);
extern u64_t __d_to_ull(double d);
extern u64_t __f_to_ull(float f);
extern double __ll_to_d(s64_t s);
extern float __ll_to_f(s64_t s);
extern double __ull_to_d(u64_t u);
extern float __ull_to_f(u64_t u);

s64_t __divdi3(s64_t a, s64_t b) { return __ll_div(a, b); }
s64_t __moddi3(s64_t a, s64_t b) { return __ll_rem(a, b); }
u64_t __udivdi3(u64_t a, u64_t b) { return __ull_div(a, b); }
u64_t __umoddi3(u64_t a, u64_t b) { return __ull_rem(a, b); }

s64_t __fixdfdi(double d) { return __d_to_ll(d); }
s64_t __fixsfdi(float f) { return __f_to_ll(f); }
u64_t __fixunsdfdi(double d) { return __d_to_ull(d); }
u64_t __fixunssfdi(float f) { return __f_to_ull(f); }
double __floatdidf(s64_t s) { return __ll_to_d(s); }
float __floatdisf(s64_t s) { return __ll_to_f(s); }
double __floatundidf(u64_t u) { return __ull_to_d(u); }
float __floatundisf(u64_t u) { return __ull_to_f(u); }


/**
 * libultra's memset symbol is an alias of bzero(ptr, len), not the standard memset.
 * The Makefile renames memset to gcc_memset in GCC objects so they land here.
 * With LTO the game objects can't be renamed (they hold GCC's intermediate code),
 * so the memset calls GCC makes at link time come here through the alias below.
 * This file is built with -fno-tree-loop-distribute-patterns so the loop is not
 * turned back into a memset call.
 */
void *gcc_memset(void *dst, int c, unsigned int n)
{
    u8 *p = dst;

    while (n--)
    {
        *p++ = c;
    }

    return dst;
}

void *memset(void *dst, int c, unsigned int n) __attribute__((alias("gcc_memset")));
