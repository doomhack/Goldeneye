/**
 * Loading files ("assets") from the ROM.
 *
 * Everything the game loads by name (levels, setups, models, text, briefings) is a file in
 * g_FileTable, which assets/obseg/file_resource_table.inc.c fills in with each file's name
 * and ROM address. Most files are compressed in Rare's 1172 format (see rzip.c). A file's
 * compressed size is the distance to the next file in the ROM.
 *
 * A file is normally loaded into a memory pool (memp.c). How big a file is once
 * decompressed isn't known until it has been loaded once, so the first time, the loader
 * takes all the free space left in the pool, copies the compressed data to the end of that
 * space, decompresses it to the start, and then gives back what it didn't use. It
 * remembers the decompressed size, so later loads of the same file allocate just that
 * much. Emptying the stage pool forgets the sizes (assetForgetSizes).
 *
 * A file that is missing, or doesn't fit, or is corrupt, stops the game with an error.
 */
#include <ultra64.h>
#include "game/asset/asset.h"
#include "game/asset/rzip.h"
#include <assets/obseg/obseg.h>
#include "assets/obseg/file_resource_id_enums.h"
#include "dbgscreen.h"
#include "memp.h"
#include "rom.h"
#include "str.h"

#include <assets/obseg/file_resource_table.inc.c>

/** What the loader knows about each file, indexed like g_FileTable. */
typedef struct AssetInfo
{
    u32 romSize;    /* compressed size */
    u32 size;       /* decompressed size, or 0 if not known yet */
    u32 bufferSize; /* the size of the buffer it was last loaded into */
} AssetInfo;

static AssetInfo m_assetInfo[OBJ_INDEX_MAX];

/* Entry 0 is a blank, and the last one marks the end of the last file in the ROM */
#define ASSET_FIRST 1
#define ASSET_END   OBENDSEG

/**
 * Space left between the compressed data and the end of the buffer. mupen64plus drops the
 * last byte of a PI DMA that ends exactly at the top of RDRAM, which corrupts a file whose
 * compressed size is a multiple of 16 when its pool ends there.
 */
#define ASSET_DMA_GAP 0x10

/** Works out each file's compressed size from where the next file starts. */
void assetInit(void)
{
    for (s32 i = ASSET_FIRST; i < ASSET_END; i++)
    {
        m_assetInfo[i] = (AssetInfo) { .romSize = g_FileTable[i + 1].romAddr - g_FileTable[i].romAddr };
    }
}

/**
 * Forgets every file's decompressed size, so the next load of each one takes all the free
 * space again. Called when the stage pool is emptied, because a size remembered there
 * could be the size the model loader shrank a file to (see assetSetSize).
 */
void assetForgetSizes(void)
{
    for (s32 i = ASSET_FIRST; i < ASSET_END; i++)
    {
        m_assetInfo[i].size = 0;
    }
}

/** Returns the index of a file in g_FileTable. Stops the game if there's no such file. */
s32 assetFind(const char *name)
{
    for (s32 i = ASSET_FIRST; i < ASSET_END; i++)
    {
        if (strcmp(name, g_FileTable[i].filename) == 0)
        {
            return i;
        }
    }

    dbgFatal("asset: no such file", (uintptr_t) name);
}

/**
 * Loads and decompresses file `index` into the `bufferSize`-byte buffer at `dst`, and
 * records its decompressed size. The compressed data is copied to the end of the buffer
 * and decompressed to its start, so the buffer must hold both, more or less.
 */
static void assetLoadIndexInto(s32 index, void *dst, u32 bufferSize)
{
    AssetInfo *info = &m_assetInfo[index];
    u32 romSize = (info->romSize + 7) & ~7u; // DMA lengths are rounded up to 8
    u8 *compressed;

    if (bufferSize < romSize + ASSET_DMA_GAP + 8)
    {
        dbgFatal("asset: buffer too small for file", index);
    }

    compressed = (u8 *) dst + bufferSize - romSize - ASSET_DMA_GAP;
    romCopy(compressed, g_FileTable[index].romAddr, info->romSize);

    info->bufferSize = bufferSize;
    info->size = rzipInflate(compressed, dst, bufferSize);

    if (info->size == 0)
    {
        dbgFatal("asset: file doesn't fit or is corrupt", index);
    }
}

/** Loads a file into a new allocation in `pool`, leaving the allocation as big as it was. */
static void *assetLoadIndexToPool(s32 index, MemPool pool)
{
    // The first time, take all the free space
    u32 size = m_assetInfo[index].size != 0 ? m_assetInfo[index].size : mempGetFreeBytes(pool);
    void *data = mempAlloc(size, pool);

    assetLoadIndexInto(index, data, size);

    return data;
}

/** Loads and decompresses a file into a new allocation in `pool`, just big enough for it. */
void *assetLoad(const char *name, MemPool pool)
{
    s32 index = assetFind(name);
    void *data = assetLoadIndexToPool(index, pool);

    mempResizeLast(data, m_assetInfo[index].size);

    return data;
}

/**
 * Loads and decompresses a file into a new allocation in `pool`, and leaves the allocation
 * as it is: on a file's first load that's all the pool's free space. The caller unpacks the
 * file further in place and then trims it with assetSetSize (the model loader does this).
 */
void *assetLoadUntrimmed(const char *name, MemPool pool)
{
    return assetLoadIndexToPool(assetFind(name), pool);
}

/**
 * Loads and decompresses a file into the `bufferSize`-byte buffer at `dst`, and returns
 * `dst`. assetGetSize gives the decompressed size afterwards.
 */
void *assetLoadInto(const char *name, void *dst, u32 bufferSize)
{
    assetLoadIndexInto(assetFind(name), dst, bufferSize);

    return dst;
}

/**
 * Copies `len` bytes from `offset` in a file straight from the ROM, without decompressing.
 * The level geometry files (bg_*.seg) aren't compressed as a whole (each room's data is
 * compressed on its own), so the level code reads them in parts this way.
 */
void assetLoadPart(const char *name, void *dst, u32 offset, u32 len)
{
    s32 index = assetFind(name);

    // ROM sizes are rounded up to 16 bytes, so the last read can go a little past the end
    if (offset + len > m_assetInfo[index].romSize + 0xF)
    {
        dbgFatal("asset: reading past the end of file", index);
    }

    romCopy(dst, g_FileTable[index].romAddr + offset, len);
}

/** A file's decompressed size, or 0 if it hasn't been loaded since the sizes were forgotten. */
u32 assetGetSize(s32 index)
{
    return m_assetInfo[index].size;
}

u32 assetGetSizeByName(const char *name)
{
    return m_assetInfo[assetFind(name)].size;
}

/** The size of the buffer the file was last loaded into. */
u32 assetGetBufferSize(s32 index)
{
    return m_assetInfo[index].bufferSize;
}

/**
 * Records a file's size after the caller has unpacked it further in place, so later loads
 * allocate that much, and optionally shrinks its allocation (from assetLoadUntrimmed) to
 * match.
 */
void assetSetSize(s32 index, void *data, u32 size, bool shrinkAllocation)
{
    m_assetInfo[index].size = size;
    m_assetInfo[index].bufferSize = size;

    if (shrinkAllocation)
    {
        mempResizeLast(data, size);
    }
}
