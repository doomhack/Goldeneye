/**
 * Texture decompression: turns a texture as the ROM stores it into the images and palette
 * that get loaded into the RDP's texture memory (TMEM). It has no state of its own and
 * nothing N64-specific, so tools/hosttest/textest.c decodes every texture in the ROM with
 * it. texload.c finds the textures in the ROM and keeps the results.
 *
 * A texture starts with one byte:
 *
 *     ezllllll
 *
 *     e = the smaller levels of detail are stored ("explicit") rather than made here by
 *         shrinking the full-size image
 *     z = a paletted texture compressed with zlib (texDecodeZlib) rather than one of Rare's
 *         own methods (texDecodeRare)
 *     l = the number of levels of detail. With 0 there is one image, and its odd rows
 *         aren't swapped (see below)
 *
 * The output is each level of detail in turn, biggest first, then the palette (16-bit
 * colours) for a paletted texture. Each row of pixels is padded to a multiple of 8 bytes
 * (16 for 32-bit pixels; texRowBytes), and every odd row has its 32-bit words swapped in
 * pairs (64-bit ones for 32-bit pixels), which is how the RDP expects a texture that is
 * loaded into TMEM in one go with LoadBlock.
 *
 * Images are bit streams, read most significant bit first. Rare's methods store the
 * pixels uncompressed, Huffman coded, run-length coded (both one colour channel at a time),
 * as indices into a lookup table of colours, or as differences from neighbouring pixels.
 *
 * Row padding is zeroed. It is never drawn.
 */
#include <ultra64.h>
#include "game/gfx/texdecode.h"
#include "game/asset/rzip.h"

/* What each TEXFORMAT_* looks like, to the decoders */
typedef struct TexFormatInfo
{
    u8 pixelBits;     /* bits per pixel in TMEM: 32, 16, 8 or 4 */
    u8 numChannels;   /* the colour channels Huffman and RLE images store one after another */
    bool alphaBit;    /* followed by a 1-bit alpha channel */
    u16 channelRange; /* each channel's values are 0 to channelRange - 1 */
    u8 lookupBits;    /* bits per lookup table colour */
} TexFormatInfo;

static const TexFormatInfo s_Formats[TEXFORMAT_COUNT] = {
    [TEXFORMAT_RGBA32]     = { 32, 4, false, 256, 32 },
    [TEXFORMAT_RGBA16]     = { 16, 3, true,   32, 16 },
    [TEXFORMAT_RGB24]      = { 32, 3, false, 256, 24 },
    [TEXFORMAT_RGB15]      = { 16, 3, false,  32, 15 },
    [TEXFORMAT_IA16]       = { 16, 2, false, 256, 16 },
    [TEXFORMAT_IA8]        = {  8, 2, false,  16,  8 },
    [TEXFORMAT_IA4]        = {  4, 1, true,    8,  4 },
    [TEXFORMAT_I8]         = {  8, 1, false, 256,  8 },
    [TEXFORMAT_I4]         = {  4, 1, false,  16,  4 },
    [TEXFORMAT_RGBA16_CI8] = {  8, 1, false, 256, 16 },
    [TEXFORMAT_RGBA16_CI4] = {  4, 1, false,  16, 16 },
    [TEXFORMAT_IA16_CI8]   = {  8, 1, false, 256, 16 },
    [TEXFORMAT_IA16_CI4]   = {  4, 1, false,  16, 16 },
};

/* Rare's compression methods (texDecodeRare) */
enum
{
    TEXCOMP_UNCOMPRESSED0,
    TEXCOMP_UNCOMPRESSED1,
    TEXCOMP_HUFFMAN,             /* all channels coded with one Huffman table */
    TEXCOMP_HUFFMANPERCHANNEL,   /* a Huffman table per channel */
    TEXCOMP_RLE,
    TEXCOMP_LOOKUP,              /* indices into a table of colours, packed into bits */
    TEXCOMP_HUFFMANLOOKUP,       /* the indices Huffman coded */
    TEXCOMP_RLELOOKUP,           /* the indices run-length coded */
    TEXCOMP_HUFFMANBLUR,         /* Huffman coded differences from neighbouring pixels */
    TEXCOMP_RLEBLUR,             /* run-length coded differences */
};

/* Scratch space, kept out of the stack because the main thread's stack is small. Texture
   decoding only ever happens on the main thread. */
#define TEX_SCRATCH_SIZE 0x2000
#define TEX_LOOKUP_SIZE  0x1000
#define HUFF_MAX_VALUES  2048

static u8 s_Scratch[TEX_SCRATCH_SIZE] __attribute__((aligned(8)));
static u8 s_Lookup[TEX_LOOKUP_SIZE] __attribute__((aligned(8)));
static u16 s_HuffFrequencies[HUFF_MAX_VALUES];
static s16 s_HuffNodes[HUFF_MAX_VALUES][2];

/** Reads a texture's bit stream, most significant bit first */
typedef struct TexBits
{
    const u8 *pos;
    u32 buffer;
    s32 count; /* bits left in buffer, 0 to 7 between reads */
} TexBits;

static void bitsInit(TexBits *bits, const u8 *pos)
{
    bits->pos = pos;
    bits->buffer = 0;
    bits->count = 0;
}

/** Reads the next `count` bits (at most 24) */
static u32 bitsRead(TexBits *bits, s32 count)
{
    while (bits->count < count)
    {
        bits->buffer = bits->buffer << 8 | *bits->pos++;
        bits->count += 8;
    }

    bits->count -= count;

    return (bits->buffer >> bits->count) & ((1 << count) - 1);
}

/**
 * The bytes in one row of pixels: padded to a multiple of 8 bytes, or 16 for 32-bit
 * pixels (whose red/green and blue/alpha halves go to different halves of TMEM).
 */
s32 texRowBytes(s32 format, s32 width)
{
    switch (s_Formats[format].pixelBits)
    {
        case 32:
            return ((width + 3) & ~3) * 4;
        case 16:
            return ((width + 3) & ~3) * 2;
        case 8:
            return (width + 7) & ~7;
        default:
            return ((width + 15) & ~15) / 2;
    }
}

/** Zeroes an image's `bytes` (a multiple of 8, as its rows are), padding included */
static void clearImage(u8 *image, s32 bytes)
{
    for (s32 i = 0; i < bytes; i += 4)
    {
        *(u32 *)&image[i] = 0;
    }
}

static void storePixel(u8 *row, s32 x, s32 pixelBits, u32 value)
{
    switch (pixelBits)
    {
        case 32:
            ((u32 *)row)[x] = value;
            break;
        case 16:
            ((u16 *)row)[x] = value;
            break;
        default:
            row[x] = value;
            break;
    }
}

/**
 * Swaps the 32-bit words of every odd row in pairs (the 64-bit halves of each 16 bytes,
 * for 32-bit pixels), which is what the RDP expects of a texture loaded with LoadBlock.
 */
static void swapOddRows(u8 *image, s32 width, s32 height, s32 format)
{
    s32 rowBytes = texRowBytes(format, width);
    s32 numWords = rowBytes / 4;
    s32 swapWith = s_Formats[format].pixelBits == 32 ? 2 : 1;

    for (s32 y = 1; y < height; y += 2)
    {
        u32 *row = (u32 *)(image + y * rowBytes);

        for (s32 x = 0; x < numWords; x += swapWith * 2)
        {
            for (s32 i = x; i < x + swapWith; i++)
            {
                u32 tmp = row[i];
                row[i] = row[i + swapWith];
                row[i + swapWith] = tmp;
            }
        }
    }
}

/* -------------------------------------------------------------------------------------- */
/* Making smaller levels of detail                                                         */
/* -------------------------------------------------------------------------------------- */

/* The average of four 5551 colours: colour channels rounded down, alpha to nearest */
static u16 average5551(u32 c1, u32 c2, u32 c3, u32 c4)
{
    s32 r = (((c1 >> 11) & 0x1f) + ((c2 >> 11) & 0x1f) + ((c3 >> 11) & 0x1f) + ((c4 >> 11) & 0x1f)) >> 2;
    s32 g = (((c1 >> 6) & 0x1f) + ((c2 >> 6) & 0x1f) + ((c3 >> 6) & 0x1f) + ((c4 >> 6) & 0x1f)) >> 2;
    s32 b = (((c1 >> 1) & 0x1f) + ((c2 >> 1) & 0x1f) + ((c3 >> 1) & 0x1f) + ((c4 >> 1) & 0x1f)) >> 2;
    s32 a = ((c1 & 1) + (c2 & 1) + (c3 & 1) + (c4 & 1) + 2) >> 2;

    return r << 11 | g << 6 | b << 1 | (a & 1);
}

/* The average of four 88 (intensity, alpha) colours: intensity rounded down */
static u16 average88(u32 c1, u32 c2, u32 c3, u32 c4)
{
    s32 i = (((c1 >> 8) & 0xff) + ((c2 >> 8) & 0xff) + ((c3 >> 8) & 0xff) + ((c4 >> 8) & 0xff)) >> 2;
    s32 a = ((c1 & 0xff) + (c2 & 0xff) + (c3 & 0xff) + (c4 & 0xff) + 1) >> 2;

    return i << 8 | (a & 0xff);
}

/**
 * The palette entry closest to `colour` (RGBA16, or IA16 if `isIA`).
 *
 * An exact match if there is one. Otherwise it binary searches the palette by each
 * colour's length (the squared sum of its channels, with alpha counting as much as a whole
 * colour channel's range), which only works if the palette is sorted that way, and then
 * looks properly at four entries either side. So it isn't always the closest.
 */
static s32 findClosestColour(const u16 *palette, s32 numColours, u16 colour, bool isIA)
{
    // A colour's channels, and how much each counts: the one alpha bit of RGBA16 is worth
    // the most a 5-bit channel can differ by, 31 squared
    s32 target[4];
    s32 weights[4];
    s32 low;
    s32 high;
    s32 targetLength;
    s32 best;
    s32 bestDistance;

#define SPLIT(c, out)                                                                      \
    do                                                                                     \
    {                                                                                      \
        if (isIA)                                                                          \
        {                                                                                  \
            (out)[0] = ((c) >> 8) & 0xff, (out)[1] = (c) & 0xff, (out)[2] = 0, (out)[3] = 0; \
        }                                                                                  \
        else                                                                               \
        {                                                                                  \
            (out)[0] = ((c) >> 11) & 0x1f, (out)[1] = ((c) >> 6) & 0x1f;                   \
            (out)[2] = ((c) >> 1) & 0x1f, (out)[3] = (c) & 1;                              \
        }                                                                                  \
    } while (0)

    for (s32 i = 0; i < numColours; i++)
    {
        if (palette[i] == colour)
        {
            return i;
        }
    }

    weights[0] = weights[1] = weights[2] = 1;
    weights[3] = isIA ? 0 : 31 * 31;

    SPLIT(colour, target);
    targetLength = 0;

    for (s32 c = 0; c < 4; c++)
    {
        targetLength += target[c] * target[c] * weights[c];
    }

    low = 0;
    high = numColours - 1;

    while (high - low >= 2)
    {
        s32 mid = (high + low) >> 1;
        s32 parts[4];
        s32 length = 0;

        SPLIT(palette[mid], parts);

        for (s32 c = 0; c < 4; c++)
        {
            length += parts[c] * parts[c] * weights[c];
        }

        if (length < targetLength)
        {
            low = mid;
        }
        else if (length > targetLength)
        {
            high = mid;
        }
        else
        {
            low = high = mid;
        }
    }

    low = high - 4 < 0 ? 0 : high - 4;
    high = high + 4 >= numColours ? numColours - 1 : high + 4;
    best = 0;
    bestDistance = 999999;

    for (s32 i = low; i <= high; i++)
    {
        s32 parts[4];
        s32 distance = 0;

        SPLIT(palette[i], parts);

        for (s32 c = 0; c < 4; c++)
        {
            distance += (parts[c] - target[c]) * (parts[c] - target[c]) * weights[c];
        }

        if (distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }

#undef SPLIT

    return best;
}

/**
 * Makes the next level of detail of a paletted image: half the size, each pixel the
 * palette entry closest to the average of a 2x2 square. An odd last column or row is
 * averaged with itself.
 */
static void shrinkPaletted(const u8 *src, u8 *dst, s32 width, s32 height, s32 format, const u16 *palette, s32 numColours)
{
    bool isIA = format == TEXFORMAT_IA16_CI8 || format == TEXFORMAT_IA16_CI4;
    s32 srcRowBytes = texRowBytes(format, width);
    s32 dstRowBytes = texRowBytes(format, (width + 1) >> 1);

    for (s32 y = 0; y < height; y += 2)
    {
        s32 nextRow = y + 1 < height ? srcRowBytes : 0;

        if (s_Formats[format].pixelBits == 8)
        {
            for (s32 x = 0; x < srcRowBytes; x += 2)
            {
                s32 nextCol = x + 1 < width ? x + 1 : x;
                u16 c1 = palette[src[x]];
                u16 c2 = palette[src[nextCol]];
                u16 c3 = palette[src[nextRow + x]];
                u16 c4 = palette[src[nextRow + nextCol]];
                u16 average = isIA ? average88(c1, c2, c3, c4) : average5551(c1, c2, c3, c4);

                dst[x >> 1] = findClosestColour(palette, numColours, average, isIA);
            }
        }
        else
        {
            // Two source bytes (four pixels) make one byte (two pixels)
            for (s32 x = 0; x < srcRowBytes * 2; x += 2)
            {
                const u8 *top = &src[x >> 1];
                const u8 *bottom = &src[nextRow + (x >> 1)];
                s32 rightShift = x + 1 < width ? 0 : 4;
                u16 c1 = palette[(*top >> 4) & 0xf];
                u16 c2 = palette[(*top >> rightShift) & 0xf];
                u16 c3 = palette[(*bottom >> 4) & 0xf];
                u16 c4 = palette[(*bottom >> rightShift) & 0xf];
                u16 average = isIA ? average88(c1, c2, c3, c4) : average5551(c1, c2, c3, c4);
                s32 index = findClosestColour(palette, numColours, average, isIA);

                if ((x & 2) == 0)
                {
                    dst[x >> 2] = index << 4;
                }
                else
                {
                    dst[x >> 2] |= index;
                }
            }
        }

        dst += dstRowBytes;
        src += srcRowBytes * 2;
    }
}

/**
 * Makes the next level of detail of a non-paletted image: half the size, each pixel the
 * average of a 2x2 square. Colour channels are rounded down and alpha to nearest (both
 * rounded to nearest in I8). An odd last row is averaged with itself, and so is an odd
 * last column except in 4-bit formats, which average it with the row's padding.
 */
static void shrinkNonPaletted(const u8 *src, u8 *dst, s32 width, s32 height, s32 format)
{
    s32 srcRowBytes = texRowBytes(format, width);
    s32 dstRowBytes = texRowBytes(format, (width + 1) >> 1);
    s32 pixelBits = s_Formats[format].pixelBits;
    s32 srcRowPixels = srcRowBytes * 8 / pixelBits;

    for (s32 y = 0; y < height; y += 2)
    {
        s32 nextRow = y + 1 < height ? srcRowBytes : 0;

        if (pixelBits == 4)
        {
            // Two bytes in each of the two rows: the first pair of pixels makes the
            // output's high nibble, the second pair the low one
            for (s32 x = 0; x < srcRowPixels; x += 4)
            {
                u32 t1 = src[x >> 1];
                u32 b1 = src[nextRow + (x >> 1)];
                u32 t2 = src[(x >> 1) + 1];
                u32 b2 = src[nextRow + (x >> 1) + 1];

                if (format == TEXFORMAT_IA4)
                {
                    // Each nibble is 3 bits of intensity and 1 of alpha
                    s32 i1 = ((t1 >> 5) & 7) + ((t1 >> 1) & 7) + ((b1 >> 5) & 7) + ((b1 >> 1) & 7);
                    s32 i2 = ((t2 >> 5) & 7) + ((t2 >> 1) & 7) + ((b2 >> 5) & 7) + ((b2 >> 1) & 7);
                    s32 a1 = ((t1 >> 4) & 1) + (t1 & 1) + ((b1 >> 4) & 1) + (b1 & 1) + 1;
                    s32 a2 = ((t2 >> 4) & 1) + (t2 & 1) + ((b2 >> 4) & 1) + (b2 & 1) + 1;

                    dst[x >> 2] = ((i1 >> 2) & 7) << 5 | ((a1 >> 2) & 1) << 4 | ((i2 >> 2) & 7) << 1 | ((a2 >> 2) & 1);
                }
                else
                {
                    s32 i1 = ((t1 >> 4) & 0xf) + (t1 & 0xf) + ((b1 >> 4) & 0xf) + (b1 & 0xf);
                    s32 i2 = ((t2 >> 4) & 0xf) + (t2 & 0xf) + ((b2 >> 4) & 0xf) + (b2 & 0xf);

                    dst[x >> 2] = ((i1 >> 2) & 0xf) << 4 | ((i2 >> 2) & 0xf);
                }
            }
        }
        else
        {
            for (s32 x = 0; x < srcRowPixels; x += 2)
            {
                s32 nextCol = x + 1 < width ? x + 1 : x;
                u32 c1;
                u32 c2;
                u32 c3;
                u32 c4;
                u32 average;

                switch (format)
                {
                    case TEXFORMAT_RGBA32:
                    case TEXFORMAT_RGB24:
                        c1 = ((const u32 *)src)[x];
                        c2 = ((const u32 *)src)[nextCol];
                        c3 = ((const u32 *)(src + nextRow))[x];
                        c4 = ((const u32 *)(src + nextRow))[nextCol];
                        average = 0;

                        for (s32 shift = 24; shift >= 0; shift -= 8)
                        {
                            s32 sum = ((c1 >> shift) & 0xff) + ((c2 >> shift) & 0xff) + ((c3 >> shift) & 0xff) + ((c4 >> shift) & 0xff);

                            if (shift == 0)
                            {
                                sum++; // alpha rounds to nearest
                            }

                            average |= (u32)((sum >> 2) & 0xff) << shift;
                        }
                        break;
                    case TEXFORMAT_RGBA16:
                    case TEXFORMAT_RGB15:
                    case TEXFORMAT_IA16:
                        c1 = ((const u16 *)src)[x];
                        c2 = ((const u16 *)src)[nextCol];
                        c3 = ((const u16 *)(src + nextRow))[x];
                        c4 = ((const u16 *)(src + nextRow))[nextCol];
                        average = format == TEXFORMAT_IA16 ? average88(c1, c2, c3, c4) : average5551(c1, c2, c3, c4);
                        break;
                    case TEXFORMAT_IA8:
                        c1 = src[x];
                        c2 = src[nextCol];
                        c3 = src[nextRow + x];
                        c4 = src[nextRow + nextCol];
                        average = ((((c1 >> 4) + (c2 >> 4) + (c3 >> 4) + (c4 >> 4)) >> 2) & 0xf) << 4
                                | ((((c1 & 0xf) + (c2 & 0xf) + (c3 & 0xf) + (c4 & 0xf) + 1) >> 2) & 0xf);
                        break;
                    default: // I8
                        average = (src[x] + src[nextCol] + src[nextRow + x] + src[nextRow + nextCol] + 1) >> 2;
                        break;
                }

                storePixel(dst, x >> 1, pixelBits, average);
            }
        }

        dst += dstRowBytes;
        src += srcRowBytes * 2;
    }
}

/**
 * Turns the full-size image at `dst` (`*totalBytes` long) into a chain of `numLods`
 * levels of detail, each half the size of the one before, swapping odd rows as it goes.
 * A paletted texture stops early if its images would take more than half of TMEM (the
 * palette has the other half). Returns the number of levels it made, or
 * TEX_DECODE_NO_ROOM if they don't fit in `dstSize` bytes.
 */
static s32 makeLods(u8 *dst, s32 dstSize, s32 *totalBytes, s32 width, s32 height, s32 format, s32 numLods, const u16 *palette, s32 numColours)
{
    u8 *image = dst;
    u8 *next = dst + *totalBytes;

    for (s32 lod = 1; lod < numLods; lod++)
    {
        s32 bytes = texRowBytes(format, (width + 1) >> 1) * ((height + 1) >> 1);

        if (*totalBytes + bytes > dstSize)
        {
            return TEX_DECODE_NO_ROOM;
        }

        if (palette != NULL)
        {
            shrinkPaletted(image, next, width, height, format, palette, numColours);

            if (*totalBytes + bytes > 0x800)
            {
                swapOddRows(image, width, height, format);
                return lod;
            }
        }
        else
        {
            shrinkNonPaletted(image, next, width, height, format);
        }

        swapOddRows(image, width, height, format);

        *totalBytes += bytes;
        width = (width + 1) >> 1;
        height = (height + 1) >> 1;
        image = next;
        next += bytes;
    }

    swapOddRows(image, width, height, format);

    return numLods;
}

/* -------------------------------------------------------------------------------------- */
/* Zlib textures                                                                           */
/* -------------------------------------------------------------------------------------- */

/**
 * A paletted texture compressed with zlib. After the first byte:
 *
 *     ffffffff nnnnnnnn [palette]    pixel format, palette size - 1, 16-bit colours
 *
 * then each image, which is its width and height (8 bits each) followed by the palette
 * indices, compressed in 1172 format (see rzip.c). The indices are one per byte, or two in
 * 4-bit formats, and the rows aren't padded.
 */
static s32 texDecodeZlib(const u8 *src, u8 *dst, s32 dstSize, TexInfo *info)
{
    static u16 palette[256];
    TexBits bits;
    s32 numImages = info->explicitLods && info->numLods ? info->numLods : 1;
    s32 totalBytes = 0;
    s32 format;
    s32 numColours;
    s32 width = 0;
    s32 height = 0;

    bitsInit(&bits, src);

    format = bitsRead(&bits, 8);
    numColours = bitsRead(&bits, 8) + 1;

    if (format < TEXFORMAT_RGBA16_CI8 || format >= TEXFORMAT_COUNT)
    {
        return TEX_DECODE_BAD;
    }

    for (s32 i = 0; i < numColours; i++)
    {
        palette[i] = bitsRead(&bits, 16);
    }

    info->format = format;
    info->lastColour = numColours - 1;

    for (s32 i = 0; i < numImages; i++)
    {
        s32 rowBytes;
        s32 indexBytes;
        const u8 *indices = s_Scratch;

        width = bitsRead(&bits, 8);
        height = bitsRead(&bits, 8);
        info->widths[i] = width;
        info->heights[i] = height;

        if (width * height > 0x1000)
        {
            return TEX_DECODE_BAD;
        }

        rowBytes = texRowBytes(format, width);
        indexBytes = s_Formats[format].pixelBits == 8 ? width : (width + 1) / 2;

        if (totalBytes + rowBytes * height > dstSize)
        {
            return TEX_DECODE_NO_ROOM;
        }

        if (rzipInflate(bits.pos, s_Scratch, sizeof(s_Scratch)) == 0)
        {
            return TEX_DECODE_BAD;
        }

        clearImage(&dst[totalBytes], rowBytes * height);

        for (s32 y = 0; y < height; y++)
        {
            for (s32 x = 0; x < indexBytes; x++)
            {
                dst[totalBytes + y * rowBytes + x] = *indices++;
            }
        }

        bitsInit(&bits, rzipGetInputEnd());

        if (info->explicitLods && info->numLods > 0)
        {
            swapOddRows(&dst[totalBytes], width, height, format);
        }

        totalBytes += rowBytes * height;
    }

    if (!info->explicitLods && info->numLods > 0)
    {
        s32 numLods = makeLods(dst, dstSize, &totalBytes, width, height, format, info->numLods, palette, numColours);

        if (numLods < 0)
        {
            return numLods;
        }

        info->numLods = numLods;
    }

    if (totalBytes + numColours * 2 > dstSize)
    {
        return TEX_DECODE_NO_ROOM;
    }

    for (s32 i = 0; i < numColours; i++)
    {
        dst[totalBytes++] = palette[i] >> 8;
        dst[totalBytes++] = palette[i] & 0xff;
    }

    return (totalBytes + 7) & ~7;
}

/* -------------------------------------------------------------------------------------- */
/* Rare's methods                                                                          */
/* -------------------------------------------------------------------------------------- */

/* Huffman nodes: a frequency of HUFF_USED or more means the node has been joined */
#define HUFF_USED 9999

/**
 * Finds the two lowest frequencies below HUFF_USED, for the Huffman tree. A frequency
 * lower than both replaces the higher of them (min2 if they're equal). Returns false if
 * there aren't two, leaving the indices of the ones it didn't find as they were.
 */
static bool findTwoSmallest(const u16 *frequencies, s32 numValues, s32 *min1, s32 *min2)
{
    u16 minFreq1 = HUFF_USED;
    u16 minFreq2 = HUFF_USED;

    for (s32 i = 0; i < numValues; i++)
    {
        if (frequencies[i] < minFreq1)
        {
            if (minFreq2 < minFreq1)
            {
                minFreq1 = frequencies[i];
                *min1 = i;
            }
            else
            {
                minFreq2 = frequencies[i];
                *min2 = i;
            }
        }
        else if (frequencies[i] < minFreq2)
        {
            minFreq2 = frequencies[i];
            *min2 = i;
        }
    }

    return minFreq1 != HUFF_USED && minFreq2 != HUFF_USED;
}

/**
 * Decodes `count` Huffman coded values, of `numValues` possible ones, to `dst`: bytes, or
 * 16-bit values if there are more than 256 possible ones. Returns false for bad data.
 *
 * The stream starts with each value's frequency (8 bits each) rather than the codes
 * themselves, and the tree is built from them here. Building it has to match Rare's
 * encoder exactly, quirks included: the frequencies are 16 bits, and a node whose total
 * reaches HUFF_USED counts as already joined.
 */
static bool huffmanDecode(TexBits *bits, u8 *dst, s32 count, s32 numValues)
{
    // A node's branches: below 10000 is the index of another node, 10000 and up a leaf,
    // the value + 10000. Leaves start out as the node with the value's index.
#define HUFF_LEAF 10000
    u16 *frequencies = s_HuffFrequencies;
    s16 (*nodes)[2] = s_HuffNodes;
    s32 root = 0;
    s32 min1 = 0;
    s32 min2 = 0;

    if (numValues > HUFF_MAX_VALUES)
    {
        return false;
    }

    for (s32 i = 0; i < numValues; i++)
    {
        frequencies[i] = bitsRead(bits, 8);
    }

    for (s32 i = 0; i < HUFF_MAX_VALUES; i++)
    {
        nodes[i][0] = -1;
        nodes[i][1] = -1;
    }

    // Join the two least frequent nodes until fewer than two are left (but at least once)
    findTwoSmallest(frequencies, numValues, &min1, &min2);

    do
    {
        s32 sum = frequencies[min1] + frequencies[min2];

        if (sum == 0)
        {
            sum = 1;
        }

        frequencies[min1] = HUFF_USED;
        frequencies[min2] = HUFF_USED;

        // A leaf node becomes the parent of itself and the other node. Two inner nodes
        // need a node that isn't in use.
        if (nodes[min1][0] < 0 && nodes[min1][1] < 0)
        {
            root = min1;
            nodes[min1][0] = min1 + HUFF_LEAF;
            nodes[min1][1] = nodes[min2][0] < 0 && nodes[min2][1] < 0 ? min2 + HUFF_LEAF : min2;
        }
        else if (nodes[min2][0] < 0 && nodes[min2][1] < 0)
        {
            root = min2;
            nodes[min2][0] = min2 + HUFF_LEAF;
            nodes[min2][1] = nodes[min1][0] < 0 && nodes[min1][1] < 0 ? min1 + HUFF_LEAF : min1;
        }
        else
        {
            for (root = 0; root < numValues; root++)
            {
                if (nodes[root][0] < 0 && nodes[root][1] < 0 && frequencies[root] >= HUFF_USED)
                {
                    break;
                }
            }

            if (root == numValues)
            {
                return false;
            }

            nodes[root][0] = min1;
            nodes[root][1] = min2;
        }

        frequencies[root] = sum;
    } while (findTwoSmallest(frequencies, numValues, &min1, &min2));

    for (s32 i = 0; i < count; i++)
    {
        s32 node = root;

        while (node < HUFF_LEAF)
        {
            node = nodes[node][bitsRead(bits, 1)];

            if (node < 0)
            {
                return false;
            }
        }

        if (numValues <= 256)
        {
            dst[i] = node - HUFF_LEAF;
        }
        else
        {
            ((u16 *)dst)[i] = node - HUFF_LEAF;
        }
    }

#undef HUFF_LEAF
    return true;
}

/**
 * Decodes `count` run-length coded values to `dst` (which has room for `capacity`
 * bytes): bytes, or 16-bit values if they're more than 8 bits. `*valueBits` returns their
 * size. Returns false for bad data.
 *
 * The stream starts with three sizes in bits: of a backtrack distance (3 bits), of a run
 * length (3 bits) and of a value (4 bits). Then each item is a 0 bit and a value, or a 1
 * bit and a run: the distance back to copy from - 1, the run's length less the shortest
 * run worth coding, and then a value, without a marker bit. Runs can overlap what they
 * write, like LZ77.
 */
static bool rleDecode(TexBits *bits, u8 *dst, s32 count, s32 capacity, s32 *valueBits)
{
    s32 distanceBits = bitsRead(bits, 3);
    s32 lengthBits = bitsRead(bits, 3);
    s32 blockBits = bitsRead(bits, 4);
    bool wide = blockBits > 8;
    s32 maxValues = wide ? capacity / 2 : capacity;
    s32 shortestRun = 0;
    s32 done = 0;

    *valueBits = blockBits;

    // The shortest run that takes fewer bits than writing its values out
    for (s32 cost = distanceBits + lengthBits + blockBits + 1; cost > 0; cost -= blockBits + 1)
    {
        shortestRun++;
    }

    while (done < count)
    {
        if (bitsRead(bits, 1) != 0)
        {
            s32 from = done - (s32)bitsRead(bits, distanceBits) - 1;
            s32 length = bitsRead(bits, lengthBits) + shortestRun;

            if (from < 0 || done + length + 1 > maxValues)
            {
                return false;
            }

            for (s32 i = from; i < from + length; i++)
            {
                if (wide)
                {
                    ((u16 *)dst)[done++] = ((u16 *)dst)[i];
                }
                else
                {
                    dst[done++] = dst[i];
                }
            }
        }
        else if (done + 1 > maxValues)
        {
            return false;
        }

        if (wide)
        {
            ((u16 *)dst)[done++] = bitsRead(bits, blockBits);
        }
        else
        {
            dst[done++] = bitsRead(bits, blockBits);
        }
    }

    return true;
}

/**
 * Undoes a blurred (difference) image: each value was stored as the difference from a
 * prediction made from the pixels to its left and above, modulo the channel's range.
 * `height` counts all the channels' planes, one above the other.
 */
static void unblur(u8 *pixels, s32 width, s32 height, s32 method, s32 range)
{
    for (s32 y = 0; y < height; y++)
    {
        for (s32 x = 0; x < width; x++)
        {
            u8 *pixel = &pixels[y * width + x];
            s32 value = *pixel + range * 2;
            s32 left = x > 0 ? pixel[-1] : 0;
            s32 above = y > 0 ? pixel[-width] : 0;
            s32 aboveLeft = x > 0 && y > 0 ? pixel[-width - 1] : 0;

            switch (method)
            {
                case 0:
                    *pixel = (value + left) % range;
                    break;
                case 1:
                    *pixel = (value + above) % range;
                    break;
                case 2:
                    *pixel = (value + aboveLeft) % range;
                    break;
                case 3:
                    *pixel = (value + (left + above - aboveLeft)) % range;
                    break;
                case 4:
                    *pixel = (value + ((above - aboveLeft) / 2 + left)) % range;
                    break;
                case 5:
                    *pixel = (value + ((left - aboveLeft) / 2 + above)) % range;
                    break;
                case 6:
                    *pixel = (value + ((left + above) / 2)) % range;
                    break;
            }
        }
    }
}

/** Reads one uncompressed pixel (not a 4-bit format) */
static u32 readPixel(TexBits *bits, s32 format)
{
    u32 high;

    switch (format)
    {
        case TEXFORMAT_RGBA32:
            high = bitsRead(bits, 16) << 16;
            return high | bitsRead(bits, 16);
        case TEXFORMAT_RGB24:
            return bitsRead(bits, 24) << 8 | 0xff;
        case TEXFORMAT_RGB15:
            return bitsRead(bits, 15) << 1 | 1;
        case TEXFORMAT_RGBA16:
        case TEXFORMAT_IA16:
            return bitsRead(bits, 16);
        default:
            return bitsRead(bits, 8);
    }
}

/**
 * Builds a pixel from channels stored one after another: `channels[c * numPixels + pos]`.
 * A 1-bit alpha channel comes after the others.
 */
static u32 pixelFromChannels(const u8 *channels, s32 pos, s32 numPixels, s32 format)
{
    const u8 *c = &channels[pos];
    s32 n = numPixels;

    switch (format)
    {
        case TEXFORMAT_RGBA32:
            return (u32)c[0] << 24 | c[n] << 16 | c[n * 2] << 8 | c[n * 3];
        case TEXFORMAT_RGB24:
            return (u32)c[0] << 24 | c[n] << 16 | c[n * 2] << 8 | 0xff;
        case TEXFORMAT_RGBA16:
            return c[0] << 11 | c[n] << 6 | c[n * 2] << 1 | c[n * 3];
        case TEXFORMAT_RGB15:
            return c[0] << 11 | c[n] << 6 | c[n * 2] << 1 | 1;
        case TEXFORMAT_IA16:
            return c[0] << 8 | c[n];
        case TEXFORMAT_IA8:
            return c[0] << 4 | c[n];
        case TEXFORMAT_IA4:
            return c[0] << 1 | c[n];
        default: // I8, I4
            return c[0];
    }
}

/** A colour from a lookup table built by readLookup */
static u32 lookupColour(const u8 *lookup, s32 index, s32 format)
{
    if (s_Formats[format].lookupBits > 16)
    {
        return ((const u32 *)lookup)[index];
    }

    return ((const u16 *)lookup)[index];
}

/**
 * Turns a lookup colour into a pixel. RGB24 from an index bit stream (`fromStream`)
 * gets alpha 0, where from a buffer of indices it gets 255.
 */
static u32 pixelFromLookup(u32 colour, s32 format, bool fromStream)
{
    switch (format)
    {
        case TEXFORMAT_RGB24:
            return fromStream ? colour << 8 : colour << 8 | 0xff;
        case TEXFORMAT_RGB15:
            return colour << 1 | 1;
        default:
            return colour;
    }
}

/**
 * Reads a lookup table into s_Lookup: the number of colours (11 bits), then each colour
 * in the format's lookupBits. Returns the number of colours, or -1 if it doesn't fit.
 */
static s32 readLookup(TexBits *bits, s32 format)
{
    s32 colourBits = s_Formats[format].lookupBits;
    s32 numColours = bitsRead(bits, 11);

    if (numColours * (colourBits > 16 ? 4 : 2) > (s32)sizeof(s_Lookup))
    {
        return -1;
    }

    for (s32 i = 0; i < numColours; i++)
    {
        if (colourBits <= 16)
        {
            ((u16 *)s_Lookup)[i] = bitsRead(bits, colourBits);
        }
        else if (colourBits <= 24)
        {
            ((u32 *)s_Lookup)[i] = bitsRead(bits, colourBits);
        }
        else
        {
            u32 high = bitsRead(bits, 24) << 8;
            ((u32 *)s_Lookup)[i] = high | bitsRead(bits, colourBits - 24);
        }
    }

    return numColours;
}

/** The bits needed for an index into `numColours` colours */
static s32 indexBits(s32 numColours)
{
    s32 count = 0;

    for (s32 max = numColours - 1; max > 0; max >>= 1)
    {
        count++;
    }

    return count;
}

/**
 * Decodes one image of a texture in one of Rare's formats into `dst`, its rows padded.
 * Returns false for bad data.
 */
static bool decodeRareImage(TexBits *bits, u8 *dst, s32 width, s32 height, s32 format, s32 method)
{
    const TexFormatInfo *info = &s_Formats[format];
    s32 rowBytes = texRowBytes(format, width);
    s32 numPixels = width * height;
    s32 numValues = info->numChannels * numPixels;
    s32 numColours;
    s32 numIndexBits;
    s32 valueBits;
    s32 blurMethod = 0;

    switch (method)
    {
        case TEXCOMP_UNCOMPRESSED0:
        case TEXCOMP_UNCOMPRESSED1:
            for (s32 y = 0; y < height; y++)
            {
                u8 *row = &dst[y * rowBytes];

                for (s32 x = 0; x < width; x += info->pixelBits == 4 ? 2 : 1)
                {
                    if (info->pixelBits == 4)
                    {
                        row[x >> 1] = bitsRead(bits, 8);
                    }
                    else
                    {
                        storePixel(row, x, info->pixelBits, readPixel(bits, format));
                    }
                }
            }
            return true;

        case TEXCOMP_LOOKUP:
            numColours = readLookup(bits, format);

            if (numColours < 0)
            {
                return false;
            }

            numIndexBits = indexBits(numColours);

            for (s32 y = 0; y < height; y++)
            {
                u8 *row = &dst[y * rowBytes];

                for (s32 x = 0; x < width; x++)
                {
                    u32 pixel = pixelFromLookup(lookupColour(s_Lookup, bitsRead(bits, numIndexBits), format), format, true);

                    if (info->pixelBits == 4)
                    {
                        if (x & 1)
                        {
                            row[x >> 1] |= pixel & 0xff;
                        }
                        else
                        {
                            row[x >> 1] = pixel << 4;
                        }
                    }
                    else
                    {
                        storePixel(row, x, info->pixelBits, pixel);
                    }
                }
            }
            return true;

        case TEXCOMP_HUFFMANLOOKUP:
        case TEXCOMP_RLELOOKUP:
            numColours = readLookup(bits, format);

            if (numColours < 0 || numPixels * (numColours > 256 ? 2 : 1) > TEX_SCRATCH_SIZE)
            {
                return false;
            }

            if (method == TEXCOMP_HUFFMANLOOKUP)
            {
                if (!huffmanDecode(bits, s_Scratch, numPixels, numColours))
                {
                    return false;
                }
            }
            else if (!rleDecode(bits, s_Scratch, numPixels, TEX_SCRATCH_SIZE, &valueBits) || (valueBits > 8) != (numColours > 256))
            {
                return false;
            }

            for (s32 y = 0; y < height; y++)
            {
                u8 *row = &dst[y * rowBytes];

                for (s32 x = 0; x < width; x += info->pixelBits == 4 ? 2 : 1)
                {
                    s32 pos = y * width + x;
                    s32 index = numColours > 256 ? ((u16 *)s_Scratch)[pos] : s_Scratch[pos];
                    u32 pixel = pixelFromLookup(lookupColour(s_Lookup, index, format), format, false);

                    if (info->pixelBits == 4)
                    {
                        row[x >> 1] = pixel << 4;

                        if (x + 1 < width)
                        {
                            s32 next = numColours > 256 ? ((u16 *)s_Scratch)[pos + 1] : s_Scratch[pos + 1];

                            row[x >> 1] |= pixelFromLookup(lookupColour(s_Lookup, next, format), format, false);
                        }
                    }
                    else
                    {
                        storePixel(row, x, info->pixelBits, pixel);
                    }
                }
            }
            return true;

        case TEXCOMP_HUFFMANBLUR:
        case TEXCOMP_RLEBLUR:
            blurMethod = bitsRead(bits, 3);
            // fall through
        case TEXCOMP_HUFFMAN:
        case TEXCOMP_HUFFMANPERCHANNEL:
        case TEXCOMP_RLE:
            if (numValues + (info->alphaBit ? numPixels : 0) > TEX_SCRATCH_SIZE)
            {
                return false;
            }

            if (method == TEXCOMP_HUFFMANPERCHANNEL)
            {
                for (s32 c = 0; c < info->numChannels; c++)
                {
                    if (!huffmanDecode(bits, &s_Scratch[numPixels * c], numPixels, info->channelRange))
                    {
                        return false;
                    }
                }
            }
            else if (method == TEXCOMP_HUFFMAN || method == TEXCOMP_HUFFMANBLUR)
            {
                if (!huffmanDecode(bits, s_Scratch, numValues, info->channelRange))
                {
                    return false;
                }
            }
            else if (!rleDecode(bits, s_Scratch, numValues, TEX_SCRATCH_SIZE, &valueBits) || valueBits > 8)
            {
                return false;
            }

            if (method == TEXCOMP_HUFFMANBLUR || method == TEXCOMP_RLEBLUR)
            {
                unblur(s_Scratch, width, info->numChannels * height, blurMethod, info->channelRange);
            }

            if (info->alphaBit)
            {
                for (s32 i = 0; i < numPixels; i++)
                {
                    s_Scratch[numValues + i] = bitsRead(bits, 1);
                }
            }

            for (s32 y = 0; y < height; y++)
            {
                u8 *row = &dst[y * rowBytes];

                for (s32 x = 0; x < width; x += info->pixelBits == 4 ? 2 : 1)
                {
                    s32 pos = y * width + x;
                    u32 pixel = pixelFromChannels(s_Scratch, pos, numPixels, format);

                    if (info->pixelBits == 4)
                    {
                        row[x >> 1] = pixel << 4;

                        if (x + 1 < width)
                        {
                            row[x >> 1] |= pixelFromChannels(s_Scratch, pos + 1, numPixels, format);
                        }
                    }
                    else
                    {
                        storePixel(row, x, info->pixelBits, pixel);
                    }
                }
            }
            return true;
    }

    return false;
}

/**
 * A texture in Rare's own formats. Each image has a 3-byte header:
 *
 *     ffffwwww wwwwhhhh hhhhcccc    format, width, height, compression method
 *
 * then its data, then 1 to 8 bits of padding up to the next byte. The images of one
 * texture can differ in format and method.
 */
static s32 texDecodeRare(const u8 *src, u8 *dst, s32 dstSize, TexInfo *info)
{
    TexBits bits;
    s32 numImages = info->explicitLods && info->numLods ? info->numLods : 1;
    s32 totalBytes = 0;
    s32 format = 0;
    s32 width = 0;
    s32 height = 0;

    bitsInit(&bits, src);

    for (s32 i = 0; i < numImages; i++)
    {
        s32 method;
        s32 bytes;

        format = bitsRead(&bits, 4);
        width = bitsRead(&bits, 8);
        height = bitsRead(&bits, 8);
        method = bitsRead(&bits, 4);

        if (i == 0)
        {
            info->format = format;
        }

        info->widths[i] = width;
        info->heights[i] = height;

        if (format >= TEXFORMAT_RGBA16_CI8 || width * height > 0x2000)
        {
            return TEX_DECODE_BAD;
        }

        bytes = texRowBytes(format, width) * height;

        if (totalBytes + bytes > dstSize)
        {
            return TEX_DECODE_NO_ROOM;
        }

        clearImage(&dst[totalBytes], bytes);

        if (!decodeRareImage(&bits, &dst[totalBytes], width, height, format, method))
        {
            return TEX_DECODE_BAD;
        }

        if (info->explicitLods && info->numLods > 0)
        {
            swapOddRows(&dst[totalBytes], width, height, format);
        }

        totalBytes += bytes;

        if (bits.count == 0)
        {
            bits.pos++;
        }
        else
        {
            bits.count = 0;
        }
    }

    if (!info->explicitLods && info->numLods > 0)
    {
        if (makeLods(dst, dstSize, &totalBytes, width, height, format, info->numLods, NULL, 0) < 0)
        {
            return TEX_DECODE_NO_ROOM;
        }
    }

    return totalBytes;
}

/**
 * Decodes the texture at `src` into `dst`, which has room for `dstSize` bytes, and
 * describes it in `info`. Returns the bytes written (a multiple of 8), TEX_DECODE_NO_ROOM
 * or TEX_DECODE_BAD. `dst` must be 8-byte aligned.
 */
s32 texDecode(const u8 *src, u8 *dst, s32 dstSize, TexInfo *info)
{
    *info = (TexInfo){0};
    info->explicitLods = (src[0] & 0x80) != 0;
    info->numLods = src[0] & 0x3f;

    if (info->numLods > TEX_MAX_LODS)
    {
        return TEX_DECODE_BAD;
    }

    if (src[0] & 0x40)
    {
        return texDecodeZlib(&src[1], dst, dstSize, info);
    }

    return texDecodeRare(&src[1], dst, dstSize, info);
}
