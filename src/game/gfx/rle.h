#ifndef _RLE_H_
#define _RLE_H_

#include <ultra64.h>

void rleExpand8Bit(u8 *src, u8 *dst);
void rleExpandRgbToRgba16(u8 *src, u16 *dst);
void rleExpandRgbToRgba32(u8 *src, u8 *dst);
#endif
