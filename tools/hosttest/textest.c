/**
 * Host tests for the texture decoder (src/game/gfx/texdecode.c). See run.sh.
 *
 * Decodes every texture in the ROM and checks each result against texhashes.txt, a hash
 * per texture of what the decoder made when the hashes were last written. The hashes were
 * first written by the decoder that phase 5d checked against Rare's code texture by
 * texture, so a change to them is a change to what the game draws. Also checks that a
 * texture that doesn't fit its buffer says so without writing past it (the address
 * sanitizer checks that).
 *
 * The textures come from assets/images/combined/combined.bin, which building the ROM
 * makes; without it the test is skipped. Run with TEXTEST_WRITE=1 to rewrite the hashes
 * after a change that is meant to change textures, and say why in the commit.
 *
 * The decoder writes pixels in the machine's byte order, so the hashes are for a
 * little-endian PC; on the N64 the bytes of each 16- and 32-bit pixel are the other way
 * round.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/game/gfx/texdecode.c"
#include "../../src/game/asset/rzip.c"

#define TEXTURES_PATH "../../assets/images/combined/combined.bin"
#define SIZES_PATH "../../assets/images.def"
#define HASHES_PATH "texhashes.txt"
#define MAX_TEXTURES 4096
#define DST_SIZE 0x10000
#define SRC_PADDING 64 /* zeroes after each texture, which the bit reader may read ahead into */

static s32 s_failures;

#define CHECK(cond, texnum)                                                                \
    do                                                                                     \
    {                                                                                      \
        if (!(cond))                                                                       \
        {                                                                                  \
            printf("%s:%d: texture %d: check failed: %s\n", __FILE__, __LINE__, (texnum), #cond); \
            s_failures++;                                                                  \
        }                                                                                  \
    } while (0)

static u8 *readFile(const char *path, long *size)
{
    FILE *file = fopen(path, "rb");
    u8 *data;

    if (file == NULL)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    data = malloc(*size + 1);

    if (fread(data, 1, *size, file) != (size_t)*size)
    {
        exit(1);
    }

    data[*size] = 0;
    fclose(file);

    return data;
}

/** Reads each texture's size from images.def: IMAGE(NAME, SIZE, ...) */
static s32 readSizes(u32 *sizes)
{
    long length;
    char *text = (char *)readFile(SIZES_PATH, &length);
    s32 count = 0;

    if (text == NULL)
    {
        return 0;
    }

    for (char *pos = strstr(text, "IMAGE("); pos != NULL; pos = strstr(pos + 1, "IMAGE("))
    {
        char *comma = strchr(pos, ',');

        if (comma != NULL && count < MAX_TEXTURES)
        {
            sizes[count++] = strtoul(comma + 1, NULL, 16);
        }
    }

    free(text);

    return count;
}

/* FNV-1a */
static u32 hash(u32 h, const void *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        h = (h ^ ((const u8 *)data)[i]) * 16777619;
    }

    return h;
}

int main(void)
{
    static u32 sizes[MAX_TEXTURES];
    static u32 hashes[MAX_TEXTURES];
    long texturesSize;
    u8 *textures = readFile(TEXTURES_PATH, &texturesSize);
    s32 count = readSizes(sizes);
    bool write = getenv("TEXTEST_WRITE") != NULL;
    u32 offset = 0;
    FILE *file;

    if (textures == NULL || count == 0)
    {
        printf("textest: skipped, no %s (build the ROM first)\n", TEXTURES_PATH);
        return 0;
    }

    for (s32 i = 0; i < count; i++)
    {
        u8 *src = calloc(sizes[i] + SRC_PADDING, 1);
        u8 *dst = malloc(DST_SIZE);
        TexInfo info;
        s32 result;

        CHECK(offset + sizes[i] <= (u32)texturesSize, i);
        memcpy(src, textures + offset, sizes[i]);
        memset(dst, 0xaa, DST_SIZE); // so that anything left unwritten changes the hash
        offset += sizes[i];

        result = texDecode(src, dst, DST_SIZE, &info);
        CHECK(result > 0 && result % 8 == 0, i);
        CHECK(info.format < TEXFORMAT_COUNT && info.numLods <= TEX_MAX_LODS, i);
        CHECK(info.widths[0] > 0 && info.heights[0] > 0, i);

        if (result > 0)
        {
            // One that doesn't fit must say so, and stay inside its buffer
            u8 *small = calloc(result - 8, 1);
            TexInfo smallInfo;

            CHECK(texDecode(src, small, result - 8, &smallInfo) == TEX_DECODE_NO_ROOM, i);
            free(small);

            hashes[i] = hash(hash(2166136261u, dst, result), &info, sizeof(info));
        }

        free(src);
        free(dst);
    }

    if (write)
    {
        file = fopen(HASHES_PATH, "w");

        for (s32 i = 0; i < count; i++)
        {
            fprintf(file, "%08x\n", hashes[i]);
        }

        fclose(file);
        printf("textest: wrote %d hashes\n", count);
    }
    else
    {
        s32 changed = 0;

        file = fopen(HASHES_PATH, "r");
        CHECK(file != NULL, -1);

        for (s32 i = 0; file != NULL && i < count; i++)
        {
            u32 expected;

            if (fscanf(file, "%x", &expected) != 1 || expected != hashes[i])
            {
                if (changed++ < 20)
                {
                    printf("textest: texture %d decodes differently\n", i);
                }
            }
        }

        if (file != NULL)
        {
            fclose(file);
        }

        CHECK(changed == 0, -1);
    }

    if (s_failures != 0)
    {
        printf("textest: %d failures\n", s_failures);
        return 1;
    }

    printf("textest: %d textures OK\n", count);
    return 0;
}
