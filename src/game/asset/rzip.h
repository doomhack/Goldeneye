#ifndef _RZIP_H_
#define _RZIP_H_
#include <ultra64.h>
#include <inflate/inflate.h>

u32 rzipInflate(u8 *src, u8 *dst, struct huft *huftBuf);
u8 *rzipGetInputEnd(void);

#endif
