#ifndef GE_ULTRA64_H
#define GE_ULTRA64_H

/* Nintendo's library headers (lib/ultralib/include/ultra64.h), plus the few things the game adds
 * to them. Game code includes this rather than the library header. */
#include <PR/ultratypes.h> /* first: the game's own types (include/PR/ultratypes.h) */
#include <math.h>            /* before gbi.h: its MIN and MAX win */
#include_next <ultra64.h>

/* Some library headers include their own ultratypes.h by a relative path. If one of them came
 * first, s32 would be "long" here and the game's types would not match. */
static_assert(__builtin_types_compatible_p(s32, int), "include <ultra64.h> before other library headers");

/* The library's version subtracts from a char pointer, which GCC rejects for addresses it
 * knows (arrays in KSEG0) as out of bounds. */
#undef OS_K0_TO_PHYSICAL
#undef OS_K1_TO_PHYSICAL
#define OS_K0_TO_PHYSICAL(x) ((u32)((uintptr_t)(x) - 0x80000000))
#define OS_K1_TO_PHYSICAL(x) ((u32)((uintptr_t)(x) - 0xa0000000))

/* For extern declarations of things that are not C small data: linker script symbols, asset
 * offsets and variables defined in assembly. Without it, -G 8 makes GCC reach them through $gp,
 * which only works for variables placed in .sdata/.sbss. */
#define NO_SDATA __attribute__((section(".data")))

typedef s32 ssize_t;

/* Boot variables at fixed addresses (ge007.ld), so they are not small data either */
extern s32 osTvType NO_SDATA;
extern s32 osRomType NO_SDATA;
extern void *osRomBase NO_SDATA;
extern s32 osResetType NO_SDATA;
extern s32 osCicId NO_SDATA;
extern s32 osVersion NO_SDATA;
extern u32 osMemSize NO_SDATA;
extern s32 osAppNMIBuffer[] NO_SDATA;

#endif
