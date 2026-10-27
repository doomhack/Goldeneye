/**
 * Loading files from the ROM.
 *
 * Everything the game loads by name (levels, setups, models, text, briefings) is a "file"
 * in g_FileTable, which assets/obseg/file_resource_table.inc.c fills in with each file's
 * name and ROM address. Most files are compressed in Rare's 1172 format (see rzip.c).
 *
 * A file is normally loaded into a memory pool (memp.c). The loader doesn't know how big a
 * file is once decompressed until it has loaded it once, so the first time it takes all
 * the free space left in the pool, copies the compressed data to the end of that space,
 * decompresses it to the start, and then gives back what it didn't use. It remembers the
 * decompressed size so later loads of the same file can allocate exactly that much.
 *
 * On Rare's development kit, files could also come from the host PC ("Indy") instead of
 * the ROM. A file with no ROM address is loaded that way; on a console that never happens.
 */
#include <ultra64.h>
#include "macro.h"
#include "game/asset/fileload.h"
#include <deb.h>
#include <memp.h>
#include <assets/obseg/obseg.h>
#include "game/asset/rzip.h"
#include "game/debug/indy_comms.h"
#include "assets/obseg/file_resource_id_enums.h"

/** What the loader knows about each file, indexed like g_FileTable. */
FileInfo m_fileInfo[OBJ_INDEX_MAX];

s32 m_fileDebug = 0;

#include <assets/obseg/file_resource_table.inc.c>

/** Number of entries in g_FileTable that are in use. */
s32 m_fileCount = OBJ_INDEX_END;

/**
 * Loads a file from the ROM into `dst`.
 *
 * If `bufferSize` is 0 the file is copied as it is, without decompressing it. Otherwise the
 * compressed data is copied to the end of the buffer and decompressed to its start, and
 * info->loadedSize is set to the decompressed size, or to 0 if the buffer was too small.
 */
void fileLoadFromRom(u8 *dst, s32 bufferSize, FileEntry *entry, FileInfo *info)
{
    u8 *compressed;
    u8  huftBuf[0x2100];
    s32 unused;

    if (bufferSize == 0)
    {
        romCopy(dst, entry->romAddr, info->romSize);
        return;
    }

    // Keep 16 bytes clear after the compressed data. mupen64plus drops the last
    // byte of a PI DMA that ends exactly at the top of RDRAM, which corrupts a
    // file whose compressed size is a multiple of 16 when its pool ends there.
    compressed = (dst + bufferSize) - ((info->romSize + 7) & -8) - 0x10;

    if ((u32) (compressed - dst) < 8U)
    {
        info->loadedSize = 0;
    }
    else
    {
        romCopy(compressed, entry->romAddr, info->romSize);
        info->loadedSize = rzipInflate(compressed, dst, huftBuf);
    }
}

/**
 * Like fileLoadFromRom, but loads the file from the development kit's host PC. Files from
 * the host may or may not be compressed.
 */
void fileLoadFromHost(u8 *dst, s32 bufferSize, FileEntry *entry, FileInfo *info)
{
    u8 *payload;
    u8 huftBuf[8448];
    s32 size;
    static const u8 rzipMagic1[] = {0x11, 0x72, 0x00, 0x00};
    static const u8 rzipMagic2[] = {0x11, 0x72, 0x00, 0x00};

    if (!bufferSize)
    {
        indycommHostLoadFile(entry->filename, dst);
        return;
    }

    indycommHostCheckFileExists(entry->filename, &info->hostSize);
    payload = (dst + bufferSize) - ((info->hostSize + 7) & -8);

    if ((payload - dst) < 8U)
    {
        info->loadedSize = 0;
    }
    else
    {
        indycommHostLoadFile(entry->filename, payload);

        if ((payload[0] == rzipMagic1[0]) && (payload[1] == rzipMagic2[1]))
        {
            size = rzipInflate(payload, dst, &huftBuf);
        }
        else
        {
            bcopy(payload, dst, info->hostSize);
            size = info->hostSize;
        }

        info->loadedSize = size;
    }
}

/** Works out each file's compressed size from where the next file starts in the ROM. */
void fileInit(void)
{
    s32 size;
    s32 i;

    debTryAdd(&m_fileDebug, "ob_c_debug");

    for (i = 1; i < m_fileCount - 1; i++)
    {
        size = (g_FileTable[i + 1].romAddr - g_FileTable[i].romAddr);

        m_fileInfo[i].romSize = size;
        m_fileInfo[i].loadedSize = 0;
        m_fileInfo[i].hostSize = 0;
        m_fileInfo[i].allocatedSize = 0;

        if (size);
    }
}

/**
 * Copies `len` bytes from `offset` in a file straight from the ROM, without decompressing.
 * The level geometry files (bg_*.seg) aren't compressed as a whole (each room's data is
 * compressed on its own), so the level code reads them in parts this way. Hangs if the
 * range goes past the end of the file.
 */
void fileLoadPartByName(u8 *name, u8 *dst, s32 offset, s32 len)
{
    s32 index;
    FileEntry *entry;

    index = fileGetIndex(name);
    entry = &g_FileTable[index];

    if (m_fileInfo[index].romSize != 0)
    {
        if ((m_fileInfo[index].romSize + 0xF) < (offset + len))
        {
            while (1) {};
        }

        romCopy(dst, &entry->romAddr[offset], len, entry);
    }
}

#if defined(VERSION_US)
void *fileLoadToPool2(s32 index, FileLoadMode mode, s32 size, u8 pool)
{
    return fileLoadToPool(index, mode, size, pool);
}

void *fileLoadToAddr2(int index, FileLoadMode mode, u8 *dst, int size)
{
    return fileLoadToAddr(index, mode, dst, size);
}
#endif

void *fileLoadByNameToPool(char *name, FileLoadMode mode, s32 size, u8 pool)
{
    return fileLoadToPool(fileGetIndex(name), mode, size, pool);
}

void *fileLoadByNameToAddr(char *name, FileLoadMode mode, u8 *dst, s32 size)
{
    return fileLoadToAddr(fileGetIndex(name), mode, dst, size);
}

/**
 * Loads and decompresses a file into a new allocation in a memory pool, and returns it.
 *
 * The first time a file is loaded, the allocation takes all the pool's free space. With
 * FILELOAD_TRIM the allocation is then shrunk to the file's decompressed size. With
 * FILELOAD_KEEP_SPARE it is left as it is, and the caller trims it itself with
 * fileSetSize (the model loader does this, after unpacking the model in place).
 *
 * `size` is not used.
 */
void *fileLoadToPool(s32 index, FileLoadMode mode, s32 size, u8 pool)
{
    FileInfo *info = &m_fileInfo[index];
    s32       unused;
    void     *data = NULL;

    if (mode == FILELOAD_KEEP_SPARE || mode == FILELOAD_TRIM || mode == FILELOAD_TRIM_2)
    {
        if (info->loadedSize == 0)
        {
            info->loadedSize = mempGetFreeBytes(pool);
        }

        data = mempAlloc(info->loadedSize, pool);
        info->allocatedSize = info->loadedSize;

        if (g_FileTable[index].romAddr == 0)
        {
            fileLoadFromHost(data, info->loadedSize, &g_FileTable[index], info);
        }
        else
        {
            fileLoadFromRom(data, info->loadedSize, &g_FileTable[index], info);
        }

        if (mode != FILELOAD_KEEP_SPARE)
        {
            mempResizeLast(data, info->loadedSize, pool);
        }
    }
    else
    {
        // Unused: loads the file without decompressing it.
        if (info->loadedSize == 0)
        {
            if (info->romSize != 0)
            {
                info->loadedSize = info->romSize;
            }
            else
            {
                info->loadedSize = info->hostSize;
            }
        }

        data = mempAlloc(info->loadedSize, pool);
        info->allocatedSize = info->loadedSize;

        if (g_FileTable[index].romAddr == 0)
        {
            fileLoadFromHost(data, 0, &g_FileTable[index], info);
        }
        else
        {
            fileLoadFromRom(data, 0, &g_FileTable[index], info);
        }

        if (size == 0)
        {
            info->loadedPool = pool;
        }
    }

    return data;
}

/**
 * Loads and decompresses a file into a buffer the caller provides, of `bufferSize` bytes,
 * and returns the buffer. fileGetLoadedSize gives the decompressed size afterwards (0 if
 * the buffer was too small).
 */
void *fileLoadToAddr(s32 index, FileLoadMode mode, void *dst, s32 bufferSize)
{
    FileInfo *info = &m_fileInfo[index];

    if (!info->loadedSize)
    {
        if (info->romSize)
        {
            info->loadedSize = info->romSize;
        }
        else
        {
            info->loadedSize = info->hostSize;
        }
    }

    if (mode == FILELOAD_KEEP_SPARE || mode == FILELOAD_TRIM || mode == FILELOAD_TRIM_2)
    {
        if (!g_FileTable[index].romAddr)
        {
            info->allocatedSize = bufferSize;
            fileLoadFromHost(dst, bufferSize, &g_FileTable[index], &m_fileInfo[index]);
        }
        else
        {
            info->allocatedSize = bufferSize;
            fileLoadFromRom(dst, bufferSize, &g_FileTable[index], &m_fileInfo[index]);
        }
    }
    else
    {
        if (!g_FileTable[index].romAddr)
        {
            fileLoadFromHost(dst, 0, &g_FileTable[index], &m_fileInfo[index]);
        }
        else
        {
            fileLoadFromRom(dst, 0, &g_FileTable[index], &m_fileInfo[index]);
        }
    }

    return dst;
}

/** The file's decompressed size, or 0 if it hasn't been loaded since the last reset. */
s32 fileGetLoadedSize(s32 index)
{
    return m_fileInfo[index].loadedSize;
}

/** The size of the buffer the file was last loaded into. */
s32 fileGetAllocSize(s32 index)
{
    return m_fileInfo[index].allocatedSize;
}

/**
 * Records a file's real size after the caller has processed it, and optionally shrinks
 * its allocation in the stage pool to match (used with FILELOAD_KEEP_SPARE).
 */
void fileSetSize(s32 index, u8 *data, u32 size, s32 shrinkAllocation)
{
    m_fileInfo[index].loadedSize = size;
    m_fileInfo[index].allocatedSize = size;

    if (shrinkAllocation != 0)
    {
        mempResizeLast(data, m_fileInfo[index].loadedSize, MEMPOOL_STAGE);
    }
}

s32 fileGetLoadedSizeByName(u8 *name)
{
    int index;

    index = fileGetIndex(name);
    return m_fileInfo[index].loadedSize;
}

/**
 * Called when a pool is reset. Clears the record of which files were loaded into this pool
 * or any lower-numbered one, and when the stage pool is reset, forgets every file's
 * decompressed size too.
 */
void fileForgetPool(u8 pool)
{
    int i;

    for (i = 1; i < m_fileCount; i++)
    {
        if (m_fileInfo[i].loadedPool <= pool)
        {
            m_fileInfo[i].loadedPool = 0;
        }

        if (pool == MEMPOOL_STAGE)
        {
            m_fileInfo[i].loadedSize = 0;
        }
    }
}

void fileForgetMePool(void)
{
    fileForgetPool(MEMPOOL_ME);
}

/**
 * Returns the index of a file in g_FileTable, or 0 if there is no such file.
 *
 * If the name isn't in the table, it asks the development kit's host PC for it and adds
 * it to the table if it exists there. On a console that always fails, but m_fileCount is
 * still increased, so each failed lookup uses up a table slot.
 */
s32 fileGetIndex(char *name)
{
    s32 i;
    s32 unused;
    s32 size;
    FileInfo *info;

    for (i = 1; i < m_fileCount; i++)
    {
        if (g_FileTable[i].filename != NULL)
        {
            if (strcmp(name, g_FileTable[i].filename) == 0)
            {
                return i;
            }
        }
    }

    i = m_fileCount;

    if (i >= OBJ_INDEX_MAX)
    {
        return 0;
    }

    m_fileCount++;

    if (indycommHostCheckFileExists(name, &size) == 0)
    {
        return 0;
    }

    g_FileTable[i].index = i;
    g_FileTable[i].filename = name;

    info = &m_fileInfo[i];

    if (1);

    info->unk_11 = 0;
    g_FileTable[i].romAddr = 0;
    info->romSize = 0;
    info->loadedSize = 0;
    info->hostSize = ALIGN16_a(size);
    info->allocatedSize = 0;
    info->loadedPool = 0;

    return i;
}

void removed_handle_filetable_entry(u32 index)
{
    return;
}

void removed_loop_handle_filetable_entries(void)
{
    int i;

    for (i = 1; (i < m_fileCount); i++)
    {
        removed_handle_filetable_entry(i);
    }
}

void removed_loop_filetableentries(void)
{
    int i;

    for (i = 1; (i < m_fileCount); i++)
    {
        ;
    }
}

void sub_GAME_7F0BD410(void)
{
    s32 i;

    for (i = 1; i < m_fileCount; i++)
    {
        if (m_fileInfo[i].loadedSize)
        {
        }
    }
}
