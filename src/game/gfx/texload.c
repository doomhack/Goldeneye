/**
 * Textures: finding them in the ROM, and the pools their decoded pixels live in.
 *
 * Display lists in ROM name textures by number (a G_SETTIMG whose address is 0xabcdNNNN).
 * texLoad finds texture NNNN in g_Textures, copies it from ROM, decodes it into a texture
 * pool (texdecode.c) and replaces the number with the texture's address. A pool fills from
 * both ends: pixel data from the left, a struct tex describing each texture from the
 * right.
 *
 * Most textures go in the shared pool (g_TexSharedPool, sized by the level's memory
 * budget); models and the guns can pass their own. See docs/subsystems/rendering.md.
 */

#include <ultra64.h>
#include "bondconstants.h"
#include "game/gfx/texload.h"
#include "rom.h"
#include "boss.h"
#include "dbgscreen.h"
#include <memp.h>
#include "game/player/player.h"

struct texpool g_TexSharedPool;
struct texcacheitem g_TexCacheItems[150];
s32 g_TexCacheCount;

/* How each TEXFORMAT_* is loaded into TMEM */
static const struct
{
    u8 gbiFormat; /* G_IM_FMT_* */
    u8 depth;     /* G_IM_SIZ_* */
    u8 lutMode;   /* G_TT_* >> G_MDSFT_TEXTLUT */
} s_TexFormatGbi[TEXFORMAT_COUNT] = {
    [TEXFORMAT_RGBA32]     = { G_IM_FMT_RGBA, G_IM_SIZ_32b, G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_RGBA16]     = { G_IM_FMT_RGBA, G_IM_SIZ_16b, G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_RGB24]      = { G_IM_FMT_RGBA, G_IM_SIZ_32b, G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_RGB15]      = { G_IM_FMT_RGBA, G_IM_SIZ_16b, G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_IA16]       = { G_IM_FMT_IA,   G_IM_SIZ_16b, G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_IA8]        = { G_IM_FMT_IA,   G_IM_SIZ_8b,  G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_IA4]        = { G_IM_FMT_IA,   G_IM_SIZ_4b,  G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_I8]         = { G_IM_FMT_I,    G_IM_SIZ_8b,  G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_I4]         = { G_IM_FMT_I,    G_IM_SIZ_4b,  G_TT_NONE >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_RGBA16_CI8] = { G_IM_FMT_CI,   G_IM_SIZ_8b,  G_TT_RGBA16 >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_RGBA16_CI4] = { G_IM_FMT_CI,   G_IM_SIZ_4b,  G_TT_RGBA16 >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_IA16_CI8]   = { G_IM_FMT_CI,   G_IM_SIZ_8b,  G_TT_IA16 >> G_MDSFT_TEXTLUT },
    [TEXFORMAT_IA16_CI4]   = { G_IM_FMT_CI,   G_IM_SIZ_4b,  G_TT_IA16 >> G_MDSFT_TEXTLUT },
};

#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) { HS, HT, SZ, F3, F4, F5, F6 },

/* Each texture's sizes, turned into offsets at boot by texResolveTableOffsets, with an
   end marker */
struct image_entry g_Textures[] = {
#include <assets/images.def>
    { HIT_DEFAULT, HIT_DEFAULT, 0xFFFF, 0, 0, 0, 0 }
};

#undef IMAGE

extern u8 _imagesSegmentRomStart NO_SDATA;

/**
 * Called once at boot: g_Textures holds each texture's compressed size, turn those into
 * offsets from the start of the texture data.
 */
void texResolveTableOffsets(void)
{
    s32 offset = 0;
    struct image_entry *entry;

    for (entry = g_Textures; entry->dataoffset != 0xffff; entry++)
    {
        s32 size = entry->dataoffset;

        entry->dataoffset = offset;
        offset += size;
    }

    entry->dataoffset = offset;
}

void texInitPool(struct texpool *pool, u8 *start, s32 len)
{
    pool->start = start;
    pool->end = (struct tex *)(start + len);
    pool->leftpos = start;
    pool->rightpos = (struct tex *)(start + len);
}

#define COOP_EXTRA_TEXTURE_BYTES (512 * 1024)

/** Makes the shared pool for a level, sized by its memory budget */
void texInitSharedPool(void)
{
    s32 size = g_LevelMemBudget->textureKB * 1024;

    g_TexCacheCount = 0;

    if (g_CoopMode)
    {
        // The per-level sizes are tuned for solo, where Bond's body textures
        // go in the weapon buffer. Co-op loads both bodies MP style into this
        // pool, and textures that no longer fit draw as garbage.
        size += COOP_EXTRA_TEXTURE_BYTES;
    }

    texInitPool(&g_TexSharedPool, mempAlloc(size, MEMPOOL_STAGE), size);
}

/** Finds a texture in a pool (NULL for the shared one). Returns NULL if it isn't there. */
struct tex *texFindInPool(s32 texturenum, struct texpool *pool)
{
    if (pool == NULL)
    {
        pool = &g_TexSharedPool;
    }

    for (struct tex *tex = pool->rightpos; tex < pool->end; tex++)
    {
        if (tex->texturenum == texturenum)
        {
            return tex;
        }
    }

    return NULL;
}

/** Remembers the sizes of a texture's stored levels of detail, for texGetWidthAtLod */
static void texCacheLodSizes(s32 texturenum, const TexInfo *info)
{
    struct texcacheitem *item = &g_TexCacheItems[g_TexCacheCount];

    for (s32 i = 0; i < g_TexCacheCount; i++)
    {
        if (g_TexCacheItems[i].texturenum == texturenum)
        {
            return;
        }
    }

    for (s32 lod = 1; lod < info->numLods; lod++)
    {
        item->widths[lod - 1] = info->widths[lod];
        item->heights[lod - 1] = info->heights[lod];
    }

    item->texturenum = texturenum;

    // When the list is full it starts again from the beginning, and forgets the rest
    g_TexCacheCount++;

    if (g_TexCacheCount >= ARRAYCOUNT(g_TexCacheItems))
    {
        g_TexCacheCount = 0;
    }
}

/**
 * Copies texture `texturenum` from ROM and decodes it into `pool`. Returns NULL if there
 * isn't room.
 */
static struct tex *texLoadIntoPool(s32 texturenum, struct texpool *pool)
{
    // Big enough for the largest texture and romCopyUnaligned's 17 bytes. Static, because
    // the main thread's stack is small.
    static u8 compressed[0x1000] __attribute__((aligned(16)));
    s32 offset = g_Textures[texturenum].dataoffset;
    s32 size = g_Textures[texturenum + 1].dataoffset - offset;
    const u8 *src;
    u8 *header;
    struct tex *tex;
    TexInfo info;
    s32 bytes;
    s32 room;

    if (size + 17 > (s32)sizeof(compressed))
    {
        dbgFatal("tex: too big", texturenum);
    }

    src = romCopyUnaligned(compressed, &_imagesSegmentRomStart + offset, size);

    // Rare's rule of thumb for whether a texture fits: the most that one texture of each
    // kind could need
    room = (u8 *)pool->rightpos - pool->leftpos;

    if (room < ((src[0] & 0x40) ? 0xA28 : 0x10CC))
    {
        return NULL;
    }

    // The pixels start with 8 bytes holding the texture number (texSelect reads it), and
    // the struct tex goes at the right end
    header = pool->leftpos;
    tex = pool->rightpos - 1;
    room -= 8 + sizeof(struct tex);

    bytes = texDecode(src, header + 8, room, &info);

    if (bytes == TEX_DECODE_NO_ROOM)
    {
        return NULL;
    }

    if (bytes < 0)
    {
        dbgFatal("tex: bad texture", texturenum);
    }

    *(s16 *)header = texturenum;
    pool->leftpos = header + 8 + bytes;
    pool->rightpos = tex;

    *tex = (struct tex){
        .texturenum = texturenum,
        .data = header + 8,
        .width = info.widths[0],
        .height = info.heights[0],
        .lastColour = info.lastColour,
        .maxlod = info.numLods,
        .gbiformat = s_TexFormatGbi[info.format].gbiFormat,
        .depth = s_TexFormatGbi[info.format].depth,
        .lutmodeindex = s_TexFormatGbi[info.format].lutMode,
        .hasExplicitLods = info.explicitLods,
    };

    if (info.explicitLods)
    {
        texCacheLodSizes(texturenum, &info);
    }

    return tex;
}

/**
 * Loads a texture into `pool` (NULL for the shared one) unless it's there already.
 *
 * `updateword` holds a texture number in its low 16 bits (as display lists in the ROM
 * name textures 0xabcdNNNN), and is replaced with the physical address of the texture's
 * pixels. If there isn't room for it, that's the start of the pool instead, which draws
 * as garbage rather than crashing.
 */
void texLoad(u32 *updateword, struct texpool *pool)
{
    s32 texturenum = *updateword & 0xffff;
    struct tex *tex;

    if (pool == NULL)
    {
        pool = &g_TexSharedPool;
    }

    tex = texFindInPool(texturenum, pool);

    if (tex == NULL)
    {
        tex = texLoadIntoPool(texturenum, pool);

        if (tex == NULL)
        {
            *updateword = osVirtualToPhysical(pool->start);
            return;
        }
    }

    *updateword = osVirtualToPhysical(tex->data);
}

void texLoadFromTextureNum(s32 texturenum, struct texpool *pool)
{
    u32 word = texturenum;

    texLoad(&word, pool);
}

/** Loads the textures a model file lists */
void texLoadFromModelFileHeader(ModelFileHeader *header, struct texpool *pool)
{
    ModelFileTextures *textures = header->Textures;

    for (s32 i = 0; i < header->numtextures; i++)
    {
        if ((s32)textures[i].TextureID < MAX_TEXTURES)
        {
            texLoad(&textures[i].TextureID, pool);
        }
    }
}

/** Loads the textures a display list names with G_SETTIMG 0xabcdNNNN */
void texLoadFromDisplayList(Gfx *gdl, struct texpool *pool)
{
    for (u8 *cmd = (u8 *)gdl; cmd[0] != (u8)G_ENDDL; cmd += 8)
    {
        if (cmd[0] == G_SETTIMG && cmd[4] == 0xab && cmd[5] == 0xcd)
        {
            texLoad((u32 *)(cmd + 4), pool);
        }
    }
}
