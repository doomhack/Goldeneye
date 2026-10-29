/**
 * Host tests for the 1172 decompressor (src/game/asset/rzip.c). See run.sh.
 *
 * zlib makes raw deflate streams (stored, fixed and dynamic blocks) from test data, which
 * get the 0x11 0x72 magic in front, like tools/1172compress.sh does. Every stream must
 * decompress to the original, in a separate buffer and in place; a buffer that's too
 * small and corrupt data must give 0 without touching memory outside the buffers (the
 * address sanitizer checks that).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "../../src/game/asset/rzip.c"

static s32 s_failures;

#define CHECK(cond) \
    do { if (!(cond)) { printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); s_failures++; } } while (0)

/* Zeroes after the compressed data: the decompressor reads a few bytes ahead, and corrupt
   data can send it further. All-zero input ends in a bad stored block. */
#define INPUT_PADDING 4096

typedef struct Compressed
{
    u8 *data; /* magic + deflate stream + INPUT_PADDING zeroes */
    u32 size; /* magic + deflate stream */
} Compressed;

static u32 s_random = 1;

static u32 testRandom(void)
{
    s_random = s_random * 1103515245 + 12345;
    return s_random >> 8;
}

/** Compresses `len` bytes with zlib, as a 1172 stream. `level` 0 makes stored blocks. */
static Compressed testCompress(const u8 *src, u32 len, int level, int strategy)
{
    z_stream z = {0};
    uLong bound;
    Compressed c;

    deflateInit2(&z, level, Z_DEFLATED, -15, 9, strategy);
    bound = deflateBound(&z, len) + 16;
    c.data = calloc(1, 2 + bound + INPUT_PADDING);
    c.data[0] = 0x11;
    c.data[1] = 0x72;

    z.next_in = (u8 *) src;
    z.avail_in = len;
    z.next_out = c.data + 2;
    z.avail_out = bound;

    // Change level halfway, for a stream of several blocks
    if (len > 2)
    {
        z.avail_in = len / 2;
        deflate(&z, Z_NO_FLUSH);
        deflateParams(&z, level == 0 ? 0 : 10 - level, strategy);
        z.avail_in = len - len / 2;
    }

    if (deflate(&z, Z_FINISH) != Z_STREAM_END)
    {
        printf("zlib failed\n");
        exit(1);
    }

    c.size = 2 + z.total_out;
    deflateEnd(&z);

    return c;
}

/* Test data generators */

static void fillText(u8 *buf, u32 len)
{
    static const char *words[] = {"bond", "james", "facility", "dam", "the", "a", "guard", "of", "Natalya", "\n"};

    for (u32 i = 0; i < len;)
    {
        const char *word = words[testRandom() % 10];

        for (u32 j = 0; word[j] != 0 && i < len; j++)
        {
            buf[i++] = word[j];
        }

        if (i < len)
        {
            buf[i++] = ' ';
        }
    }
}

static void fillRandom(u8 *buf, u32 len)
{
    for (u32 i = 0; i < len; i++)
    {
        buf[i] = testRandom();
    }
}

/* Skewed byte frequencies (geometric), for Huffman codes longer than the fast table. */
static void fillSkewed(u8 *buf, u32 len)
{
    for (u32 i = 0; i < len; i++)
    {
        u32 r = testRandom();
        u8 value = 0;

        while ((r & 1) && value < 255)
        {
            value++;
            r = (r >> 1) | (testRandom() << 23);
        }

        buf[i] = value * 7;
    }
}

static void fillRuns(u8 *buf, u32 len)
{
    for (u32 i = 0; i < len;)
    {
        u32 run = 1 + testRandom() % 300;
        u8 value = testRandom();

        while (run-- > 0 && i < len)
        {
            buf[i++] = value;
        }
    }
}

/* Like model or level data: words with a few common values. */
static void fillWords(u8 *buf, u32 len)
{
    static const u32 values[] = {0, 0x3F800000, 0x01000000, 0x80000000, 0xB8000000, 0x0000FFFF};

    for (u32 i = 0; i < len; i++)
    {
        buf[i] = (i % 4 == 0 && testRandom() % 4 == 0) ? testRandom() : (values[(i / 4) % 6] >> (8 * (3 - i % 4))) & 0xFF;
    }
}

typedef void (*FillFunc)(u8 *buf, u32 len);

static const FillFunc s_fills[] = {fillText, fillRandom, fillSkewed, fillRuns, fillWords};
static const u32 s_lengths[] = {0, 1, 2, 100, 4095, 65536, 300000};
static const int s_levels[] = {0, 1, 6, 9};
static const int s_strategies[] = {Z_DEFAULT_STRATEGY, Z_FIXED, Z_HUFFMAN_ONLY, Z_RLE};

/** Decompresses into a separate buffer exactly the right size, and one byte too small. */
static void testSeparate(const u8 *orig, u32 len, Compressed c)
{
    u8 *out = malloc(len + 1);
    u32 size = rzipInflate(c.data, out, len);

    CHECK(size == len);
    CHECK(memcmp(out, orig, len) == 0);
    CHECK(rzipGetInputEnd() == c.data + c.size);
    free(out);

    if (len > 0)
    {
        out = malloc(len - 1);
        CHECK(rzipInflate(c.data, out, len - 1) == 0);
        free(out);
    }
}

/**
 * Decompresses in place, the way the asset loader does: the compressed data at the end of
 * the buffer. With room to spare it must work; with too little it must fail, not corrupt.
 */
static void testInPlace(const u8 *orig, u32 len, Compressed c)
{
    u32 bufferSize = len + c.size + 64;
    u8 *buffer = malloc(bufferSize);
    u8 *src = buffer + bufferSize - c.size - 16;

    memcpy(src, c.data, c.size);
    memset(src + c.size, 0, 16);
    CHECK(rzipInflate(src, buffer, bufferSize) == len);
    CHECK(memcmp(buffer, orig, len) == 0);
    free(buffer);

    // A buffer only as big as the output: the output may well run into the input
    bufferSize = len > c.size ? len : c.size;
    buffer = malloc(bufferSize + 16);
    src = buffer + bufferSize - c.size;
    memcpy(src, c.data, c.size);
    memset(src + c.size, 0, 16);

    u32 size = rzipInflate(src, buffer, bufferSize);

    CHECK(size == 0 || (size == len && memcmp(buffer, orig, len) == 0));
    free(buffer);
}

/** Flips bits in the stream: the result must fail or fit the buffer, never overrun. */
static void testCorrupt(u32 len, Compressed c)
{
    u8 *bad = malloc(c.size + INPUT_PADDING);
    u32 outSize = len + 1000;
    u8 *out = malloc(outSize);

    for (s32 i = 0; i < 20; i++)
    {
        memcpy(bad, c.data, c.size + INPUT_PADDING);

        for (s32 flips = 1 + testRandom() % 4; flips > 0; flips--)
        {
            u32 bit = 16 + testRandom() % ((c.size - 2) * 8);
            bad[bit / 8] ^= 1 << (bit % 8);
        }

        CHECK(rzipInflate(bad, out, outSize) <= outSize);
    }

    free(out);
    free(bad);
}

static void testHandMade(void)
{
    u8 out[16];

    // A stored block, the last one, of "hi"
    static const u8 stored[] = {0x11, 0x72, 0x01, 0x02, 0x00, 0xFD, 0xFF, 'h', 'i', 0, 0, 0, 0};
    CHECK(rzipInflate(stored, out, sizeof(out)) == 2 && out[0] == 'h' && out[1] == 'i');

    // Block type 3 doesn't exist
    static const u8 badType[] = {0x11, 0x72, 0x07, 0, 0, 0, 0};
    CHECK(rzipInflate(badType, out, sizeof(out)) == 0);

    // Stored block with a wrong length check
    static const u8 badCheck[] = {0x11, 0x72, 0x01, 0x02, 0x00, 0xFD, 0xFE, 'h', 'i', 0, 0, 0, 0};
    CHECK(rzipInflate(badCheck, out, sizeof(out)) == 0);
}

int main(void)
{
    u32 maxLen = 300000;
    u8 *orig = malloc(maxLen);
    s32 streams = 0;

    testHandMade();

    for (u32 f = 0; f < sizeof(s_fills) / sizeof(s_fills[0]); f++)
    {
        for (u32 l = 0; l < sizeof(s_lengths) / sizeof(s_lengths[0]); l++)
        {
            for (u32 lv = 0; lv < sizeof(s_levels) / sizeof(s_levels[0]); lv++)
            {
                for (u32 st = 0; st < sizeof(s_strategies) / sizeof(s_strategies[0]); st++)
                {
                    u32 len = s_lengths[l];
                    Compressed c;

                    s_fills[f](orig, len);
                    c = testCompress(orig, len, s_levels[lv], s_strategies[st]);

                    testSeparate(orig, len, c);
                    testInPlace(orig, len, c);

                    if (len >= 100 && len <= 65536)
                    {
                        testCorrupt(len, c);
                    }

                    free(c.data);
                    streams++;
                }
            }
        }
    }

    free(orig);

    if (s_failures != 0)
    {
        printf("rzip: %d checks failed\n", s_failures);
        return 1;
    }

    printf("rzip: all tests passed (%d streams)\n", streams);
    return 0;
}
