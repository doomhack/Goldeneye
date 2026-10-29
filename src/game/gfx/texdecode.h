#ifndef _TEXDECODE_H_
#define _TEXDECODE_H_

#include <ultra64.h>

/* Texture pixel formats, as stored in the ROM (see texdecode.c) */
#define TEXFORMAT_RGBA32     0x00 // 32-bit RGBA (8/8/8/8)
#define TEXFORMAT_RGBA16     0x01 // 16-bit RGBA (5/5/5/1)
#define TEXFORMAT_RGB24      0x02 // 24-bit RGB (8/8/8), loaded as RGBA32
#define TEXFORMAT_RGB15      0x03 // 15-bit RGB (5/5/5), loaded as RGBA16
#define TEXFORMAT_IA16       0x04 // 16-bit greyscale + alpha (8/8)
#define TEXFORMAT_IA8        0x05 // 8-bit greyscale + alpha (4/4)
#define TEXFORMAT_IA4        0x06 // 4-bit greyscale + alpha (3/1)
#define TEXFORMAT_I8         0x07 // 8-bit greyscale
#define TEXFORMAT_I4         0x08 // 4-bit greyscale
#define TEXFORMAT_RGBA16_CI8 0x09 // 8-bit indices into an RGBA16 palette
#define TEXFORMAT_RGBA16_CI4 0x0a // 4-bit indices into an RGBA16 palette
#define TEXFORMAT_IA16_CI8   0x0b // 8-bit indices into an IA16 palette
#define TEXFORMAT_IA16_CI4   0x0c // 4-bit indices into an IA16 palette
#define TEXFORMAT_COUNT      0x0d

/* The most levels of detail a texture can have (struct tex has 3 bits for the count) */
#define TEX_MAX_LODS 7

/** What texDecode found out about a texture */
typedef struct TexInfo
{
    u8 format;                  /* TEXFORMAT_* of the full-size image */
    u8 numLods;                 /* levels of detail the RDP can use (struct tex's maxlod) */
    bool explicitLods;          /* the smaller levels are stored rather than made by shrinking */
    u8 lastColour;              /* palette size - 1, for paletted (zlib) textures */
    u8 widths[TEX_MAX_LODS];    /* each stored image's size; [0] is the full-size one */
    u8 heights[TEX_MAX_LODS];
} TexInfo;

/* texDecode's results for a texture that doesn't fit the output buffer, and for one it
   can't make sense of */
#define TEX_DECODE_NO_ROOM (-1)
#define TEX_DECODE_BAD     (-2)

s32 texDecode(const u8 *src, u8 *dst, s32 dstSize, TexInfo *info);
s32 texRowBytes(s32 format, s32 width);

#endif
