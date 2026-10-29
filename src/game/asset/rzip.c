/**
 * rzip: decompression of Rare's "1172" format, used for most files in the ROM, room
 * geometry, compressed textures and music.
 *
 * A compressed file is the two bytes 0x11 0x72 followed by a raw deflate stream (RFC 1951):
 * a gzip file with its header and trailer cut off (see tools/1172compress.sh).
 *
 * The boot code uses this same file to unpack the game code (it's built a second time,
 * with RZIP_BOOT, into the boot segment; see init.c and the Makefile), so it can't call
 * anything in the game code. It keeps its state in statics, so only one thread can use it
 * (the main thread loads files, textures, rooms and music).
 *
 * Deflate in brief. The stream is a series of blocks, each stored (raw bytes), or
 * compressed with either the fixed Huffman codes from the RFC or codes described at the
 * start of the block. A compressed block is a series of symbols: 0-255 is a literal byte,
 * 256 ends the block, and 257-285 start a "copy `length` bytes from `distance` bytes back"
 * (the length and distance each have a base value plus some extra bits). Bits are read
 * from the low end of each byte first; Huffman codes are stored most significant bit
 * first, so they come out bit-reversed.
 *
 * Huffman codes are decoded with a lookup table indexed by the next RZIP_FAST_BITS bits of
 * the stream, which gives the symbol and code length of any code that short. Longer codes
 * (rare) fall back to the canonical decoding: find the code length whose range of codes
 * contains the next bits.
 */
#include <ultra64.h>
#include "game/asset/rzip.h"

#ifdef RZIP_BOOT
#define rzipInflate bootInflate
#endif

/** Huffman codes up to this many bits long are decoded with one table lookup. */
#define RZIP_FAST_BITS 9
#define RZIP_FAST_MASK ((1 << RZIP_FAST_BITS) - 1)

#define RZIP_MAX_CODE_BITS 15
#define RZIP_MAX_SYMBOLS   288

typedef struct RzipHuffman
{
    /* (code length << 9) | symbol for every code up to RZIP_FAST_BITS long, else 0 */
    u16 fast[1 << RZIP_FAST_BITS];
    /* For each code length: the first code of that length, and its index in `symbols` */
    u16 firstCode[RZIP_MAX_CODE_BITS + 1];
    u16 firstIndex[RZIP_MAX_CODE_BITS + 1];
    /* For each code length: the first code that is longer, shifted up to 16 bits */
    u32 endCode[RZIP_MAX_CODE_BITS + 2];
    /* The symbols sorted by code, and each one's code length */
    u16 symbols[RZIP_MAX_SYMBOLS];
    u8 lengths[RZIP_MAX_SYMBOLS];
} RzipHuffman;

/** Where decompression is up to. */
typedef struct RzipState
{
    const u8 *in;
    u32 bits;     /* bits read from `in` but not used yet, lowest first */
    u32 bitCount;
    u8 *outStart;
    u8 *out;
    u8 *outEnd;   /* where the output must stop: the end of the buffer, or the input */
    bool inPlace; /* the input is inside the output buffer */
} RzipState;

/* Base values and extra bits for length symbols 257-285 and distance symbols 0-29 */
static const u16 m_rzipLengthBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};
static const u8 m_rzipLengthExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};
static const u16 m_rzipDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};
static const u8 m_rzipDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

/** The order a dynamic block lists the code lengths of the code length code in. */
static const u8 m_rzipLengthOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
};

// Explicitly zeroed: in the boot copy that puts them in .data, inside the boot segment
static RzipHuffman m_rzipLiterals = {0};
static RzipHuffman m_rzipDistances = {0};
static const u8 *m_rzipInputEnd = NULL;

/** Reverses the lowest 16 bits of `value`. */
static inline u32 rzipReverse16(u32 value)
{
    // Swap neighbouring bits, then pairs, nibbles and bytes
    value = ((value & 0xAAAA) >> 1) | ((value & 0x5555) << 1);
    value = ((value & 0xCCCC) >> 2) | ((value & 0x3333) << 2);
    value = ((value & 0xF0F0) >> 4) | ((value & 0x0F0F) << 4);
    value = ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);

    return value;
}

/** Tops up the bit buffer to at least 25 bits. */
static inline void rzipFillBits(RzipState *s)
{
    while (s->bitCount <= 24)
    {
        s->bits |= (u32) *s->in++ << s->bitCount;
        s->bitCount += 8;
    }
}

/** Reads `count` bits (0 to 16). */
static inline u32 rzipReadBits(RzipState *s, s32 count)
{
    u32 value;

    rzipFillBits(s);
    value = s->bits & ((1u << count) - 1);
    s->bits >>= count;
    s->bitCount -= count;

    return value;
}

/**
 * Builds the decoding tables for a Huffman code, given each symbol's code length (0 for
 * symbols that aren't used). Returns false if the lengths don't make a valid code.
 */
static bool rzipBuildHuffman(RzipHuffman *h, const u8 *lengths, s32 count)
{
    s32 lengthCounts[RZIP_MAX_CODE_BITS + 1] = {0};
    u32 nextCode[RZIP_MAX_CODE_BITS + 1];
    u32 code = 0;
    s32 index = 0;

    for (s32 i = 0; i < (1 << RZIP_FAST_BITS); i++)
    {
        h->fast[i] = 0;
    }

    for (s32 i = 0; i < count; i++)
    {
        lengthCounts[lengths[i]]++;
    }

    lengthCounts[0] = 0;

    // Canonical codes: the codes of each length follow on from the last code of the
    // length before, doubled
    for (s32 len = 1; len <= RZIP_MAX_CODE_BITS; len++)
    {
        nextCode[len] = code;
        h->firstCode[len] = code;
        h->firstIndex[len] = index;
        code += lengthCounts[len];

        if (lengthCounts[len] != 0 && code > (1u << len))
        {
            return false; // more codes of this length than there is room for
        }

        h->endCode[len] = code << (16 - len);
        code <<= 1;
        index += lengthCounts[len];
    }

    h->endCode[RZIP_MAX_CODE_BITS + 1] = 0x10000; // stops the search in rzipDecode

    for (s32 symbol = 0; symbol < count; symbol++)
    {
        s32 len = lengths[symbol];

        if (len == 0)
        {
            continue;
        }

        index = h->firstIndex[len] + (nextCode[len] - h->firstCode[len]);
        h->symbols[index] = symbol;
        h->lengths[index] = len;

        if (len <= RZIP_FAST_BITS)
        {
            // Every RZIP_FAST_BITS-bit pattern that starts with this code
            for (u32 j = rzipReverse16(nextCode[len]) >> (16 - len); j < (1 << RZIP_FAST_BITS); j += 1 << len)
            {
                h->fast[j] = (len << 9) | symbol;
            }
        }

        nextCode[len]++;
    }

    return true;
}

/** Decodes the next symbol with the Huffman code `h`. Returns -1 for an invalid code. */
static inline s32 rzipDecode(RzipState *s, const RzipHuffman *h)
{
    u32 entry;
    u32 code;
    s32 len;
    s32 index;

    rzipFillBits(s);
    entry = h->fast[s->bits & RZIP_FAST_MASK];

    if (entry != 0)
    {
        len = entry >> 9;
        s->bits >>= len;
        s->bitCount -= len;
        return entry & 0x1FF;
    }

    // A longer code: find the length whose codes include the next 16 bits
    code = rzipReverse16(s->bits & 0xFFFF);

    for (len = RZIP_FAST_BITS + 1; code >= h->endCode[len]; len++)
    {
    }

    if (len > RZIP_MAX_CODE_BITS)
    {
        return -1;
    }

    index = (code >> (16 - len)) - h->firstCode[len] + h->firstIndex[len];

    if (index >= RZIP_MAX_SYMBOLS || h->lengths[index] != len)
    {
        return -1;
    }

    s->bits >>= len;
    s->bitCount -= len;

    return h->symbols[index];
}

/** Moves the output limit up to the input, when decompressing in place. */
static inline void rzipUpdateLimit(RzipState *s, u8 *bufferEnd)
{
    if (s->inPlace)
    {
        // Bytes still in the bit buffer have been read, so the output can overwrite them
        const u8 *unread = s->in;
        s->outEnd = (unread < bufferEnd) ? (u8 *) unread : bufferEnd;
    }
}

/** Decompresses a block of Huffman codes, up to its end symbol. */
static bool rzipInflateCodes(RzipState *state, u8 *bufferEnd)
{
    // Work on a copy: the state can then live in registers, which it can't while the
    // output bytes written through s.out might alias it
    RzipState s = *state;
    bool ok = false;

    for (;;)
    {
        s32 symbol = rzipDecode(&s, &m_rzipLiterals);

        rzipUpdateLimit(&s, bufferEnd);

        if (symbol < 256)
        {
            if (symbol < 0 || s.out >= s.outEnd)
            {
                break;
            }

            *s.out++ = symbol;
        }
        else if (symbol == 256)
        {
            ok = true;
            break;
        }
        else
        {
            s32 length;
            u32 distance;
            u8 *from;

            symbol -= 257;

            if (symbol >= 29)
            {
                break;
            }

            length = m_rzipLengthBase[symbol] + rzipReadBits(&s, m_rzipLengthExtra[symbol]);
            symbol = rzipDecode(&s, &m_rzipDistances);

            if (symbol < 0 || symbol >= 30)
            {
                break;
            }

            distance = m_rzipDistBase[symbol] + rzipReadBits(&s, m_rzipDistExtra[symbol]);
            rzipUpdateLimit(&s, bufferEnd);

            if (distance > (u32) (s.out - s.outStart) || length > s.outEnd - s.out)
            {
                break;
            }

            // The copy can overlap what it writes (distance < length repeats a pattern),
            // so it goes a byte at a time
            from = s.out - distance;

            while (length-- > 0)
            {
                *s.out++ = *from++;
            }
        }
    }

    *state = s;

    return ok;
}

/** Copies a stored (uncompressed) block. */
static bool rzipInflateStored(RzipState *s, u8 *bufferEnd)
{
    u32 length;
    u32 check;

    // Stored blocks start on a byte boundary
    rzipReadBits(s, s->bitCount & 7);
    length = rzipReadBits(s, 16);
    check = rzipReadBits(s, 16);

    if (length != (~check & 0xFFFF))
    {
        return false;
    }

    while (length-- > 0)
    {
        u8 byte = rzipReadBits(s, 8);

        rzipUpdateLimit(s, bufferEnd);

        if (s->out >= s->outEnd)
        {
            return false;
        }

        *s->out++ = byte;
    }

    return true;
}

/** Sets up the fixed Huffman codes from the RFC. */
static void rzipBuildFixed(void)
{
    u8 lengths[RZIP_MAX_SYMBOLS];
    s32 i;

    for (i = 0; i < 144; i++)
    {
        lengths[i] = 8;
    }
    for (; i < 256; i++)
    {
        lengths[i] = 9;
    }
    for (; i < 280; i++)
    {
        lengths[i] = 7;
    }
    for (; i < 288; i++)
    {
        lengths[i] = 8;
    }

    rzipBuildHuffman(&m_rzipLiterals, lengths, 288);

    for (i = 0; i < 30; i++)
    {
        lengths[i] = 5;
    }

    rzipBuildHuffman(&m_rzipDistances, lengths, 30);
}

/** Reads the Huffman codes at the start of a dynamic block. */
static bool rzipBuildDynamic(RzipState *s)
{
    u8 lengths[286 + 30];
    u8 lengthLengths[19] = {0};
    s32 literalCount = rzipReadBits(s, 5) + 257;
    s32 distanceCount = rzipReadBits(s, 5) + 1;
    s32 lengthLengthCount = rzipReadBits(s, 4) + 4;
    s32 total = literalCount + distanceCount;
    s32 i;

    // The code lengths are themselves Huffman coded, with this code
    for (i = 0; i < lengthLengthCount; i++)
    {
        lengthLengths[m_rzipLengthOrder[i]] = rzipReadBits(s, 3);
    }

    if (!rzipBuildHuffman(&m_rzipLiterals, lengthLengths, 19))
    {
        return false;
    }

    i = 0;

    while (i < total)
    {
        s32 symbol = rzipDecode(s, &m_rzipLiterals);
        s32 repeat;
        u8 value;

        if (symbol < 0)
        {
            return false;
        }

        if (symbol < 16)
        {
            lengths[i++] = symbol;
            continue;
        }

        if (symbol == 16)
        {
            // Repeat the previous length 3 to 6 times
            if (i == 0)
            {
                return false;
            }

            value = lengths[i - 1];
            repeat = 3 + rzipReadBits(s, 2);
        }
        else
        {
            // 3 to 10, or 11 to 138, zero lengths
            value = 0;
            repeat = (symbol == 17) ? 3 + rzipReadBits(s, 3) : 11 + rzipReadBits(s, 7);
        }

        if (i + repeat > total)
        {
            return false;
        }

        while (repeat-- > 0)
        {
            lengths[i++] = value;
        }
    }

    return rzipBuildHuffman(&m_rzipLiterals, lengths, literalCount)
        && rzipBuildHuffman(&m_rzipDistances, lengths + literalCount, distanceCount);
}

/**
 * Decompresses 1172 data at `src` into `dst`, which has room for `dstSize` bytes, and
 * returns the decompressed size. Returns 0 if the data is corrupt or doesn't fit.
 *
 * The compressed data can sit at the end of the output buffer (decompressing in place);
 * decompression then fails if the output would catch up with the data still to be read.
 */
u32 rzipInflate(const u8 *src, u8 *dst, u32 dstSize)
{
    RzipState s;
    u8 *bufferEnd = dst + dstSize;
    bool last;

    s.in = src + 2; // skip the magic bytes
    s.bits = 0;
    s.bitCount = 0;
    s.outStart = dst;
    s.out = dst;
    s.outEnd = bufferEnd;
    s.inPlace = src >= dst && src < bufferEnd;

    do
    {
        u32 type;

        last = rzipReadBits(&s, 1);
        type = rzipReadBits(&s, 2);

        if (type == 0)
        {
            if (!rzipInflateStored(&s, bufferEnd))
            {
                return 0;
            }
        }
        else if (type == 1 || type == 2)
        {
            if (type == 1)
            {
                rzipBuildFixed();
            }
            else if (!rzipBuildDynamic(&s))
            {
                return 0;
            }

            if (!rzipInflateCodes(&s, bufferEnd))
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
    } while (!last);

    // Give back the whole bytes the bit buffer read ahead
    m_rzipInputEnd = s.in - s.bitCount / 8;

    return s.out - dst;
}

#ifndef RZIP_BOOT
/** Returns the address just past the compressed data that the last rzipInflate read. */
const u8 *rzipGetInputEnd(void)
{
    return m_rzipInputEnd;
}
#endif
