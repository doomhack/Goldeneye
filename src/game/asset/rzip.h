#ifndef _RZIP_H_
#define _RZIP_H_
#include <ultra64.h>

u32 rzipInflate(const u8 *src, u8 *dst, u32 dstSize);
const u8 *rzipGetInputEnd(void);

/* The boot code's copy of rzipInflate (see init.c) */
u32 bootInflate(const u8 *src, u8 *dst, u32 dstSize);

#endif
