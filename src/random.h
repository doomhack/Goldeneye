#ifndef _RANDOM_H_
#define _RANDOM_H_
#include <ultra64.h>

extern u64 g_randomSeed;
extern u64 g_chrObjRandomSeed;

void randomSetSeed(u32 seed);
u32 randomGetNext(void);
u32 randomGetNextFrom(u64 *seed);
void chrObjRandomSetSeed(u32 seed);
u32 chrObjRandomGetNext(void);

// 4294967295 == UINT_MAX
#define RANDOMGETNEXT_F32() ((f32) (u32)randomGetNext() * (1.0f / 4294967295))

#endif
