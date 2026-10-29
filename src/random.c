#include <ultra64.h>
#include "random.h"

/**
 * The game's pseudo-random numbers: a 64-bit shift-and-xor generator.
 *
 * There are two independent streams. g_randomSeed is the general one, used for everything
 * from guard reactions to bullet spread. g_chrObjRandomSeed is only used to dent objects
 * when they are shot (objDeform in objdamage.c), which seeds it from each vertex so the same
 * object always dents the same way. Demo playback (ramromreplay.c) saves and restores both.
 */

u64 g_randomSeed = 0xAB8D9F7781280783;
u64 g_chrObjRandomSeed = 0xAB8D9F7781280783;

/**
 * Steps *seed and returns the low 32 bits of the new value.
 *
 * The new value is bit 0 of the old one in bit 32, old bits 1-32 in bits 0-31, xored with
 * old bits 0-19 moved up to bits 12-31; then bits 20-31 are xored into bits 0-11.
 */
u32 randomGetNextFrom(u64 *seed)
{
    u64 x = *seed;
    u64 next = ((x << 63) >> 31) | ((x << 31) >> 32);

    next ^= (x << 44) >> 32;
    next ^= (next >> 20) & 0xFFF;
    *seed = next;

    return (u32) next;
}

u32 randomGetNext(void)
{
    return randomGetNextFrom(&g_randomSeed);
}

/**
 * The seed is the value plus one, so that a seed of 0 does not lock the generator at 0.
 * It is sign-extended to 64 bits, as the original assembly did with the 32-bit argument.
 */
void randomSetSeed(u32 seed)
{
    g_randomSeed = (u64) (s64) (s32) seed + 1;
}

u32 chrObjRandomGetNext(void)
{
    return randomGetNextFrom(&g_chrObjRandomSeed);
}

void chrObjRandomSetSeed(u32 seed)
{
    g_chrObjRandomSeed = (u64) (s64) (s32) seed + 1;
}
