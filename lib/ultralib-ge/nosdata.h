/* Included first in every library C file (see lib/ultralib.mk).
 *
 * The library is built with small data (-G 8) like the game. Variables that are not in
 * .sdata/.sbss must be marked as normal data, or GCC would try to reach them through $gp:
 * the boot variables at fixed addresses (osTvType and the rest, see ge007.ld) and data
 * defined in assembly. */
#include <PR/ultratypes.h>

#define GE_NO_SDATA __attribute__((section(".data")))

extern s32 osTvType GE_NO_SDATA;
extern s32 osRomType GE_NO_SDATA;
extern void *osRomBase GE_NO_SDATA;
extern s32 osResetType GE_NO_SDATA;
extern s32 osCicId GE_NO_SDATA;
extern s32 osVersion GE_NO_SDATA;
extern u32 osMemSize GE_NO_SDATA;
extern s32 osAppNMIBuffer[] GE_NO_SDATA;

/* src/gu/libm_vals.s */
extern float __libm_qnan_f GE_NO_SDATA;
