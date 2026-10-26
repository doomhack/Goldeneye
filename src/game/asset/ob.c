#include <ultra64.h>
#include "macro.h"
#include "game/asset/ob.h"
#include <deb.h>
#include <memp.h>
#include <assets/obseg/obseg.h>
#include "game/asset/decompress.h"
#include "game/debug/indy_comms.h"
#include "assets/obseg/file_resource_id_enums.h"

//bss
//800888b0

 FileInfo m_fileInfo[OBJ_INDEX_MAX];


// data
//D:80046050
s32 m_fileDebug = 0;


#include <assets/obseg/file_resource_table.inc.c>
/* struct FileEntry g_FileTable[] =
   {
       blah;
    };
 */


//D:800482D4
s32 m_fileCount = OBJ_INDEX_END;




void fileLoadFromRom(u8 *ptrdata, s32 bytes,  FileEntry *srcfile,  FileInfo *lookupdata)
{
    u8 *source;
    u8  buffer[0x2100];
    s32 unused;


    if (bytes == 0)
    {
        romCopy(ptrdata, srcfile->romAddr, lookupdata->romSize);
        return;
    }
    // Keep 16 bytes clear after the compressed data. mupen64plus drops the last
    // byte of a PI DMA that ends exactly at the top of RDRAM, which corrupts a
    // file whose compressed size is a multiple of 16 when its bank ends there.
    source = (ptrdata + bytes) - ((lookupdata->romSize + 7) & -8) - 0x10;
    if ((u32) (source - ptrdata) < 8U)
    {
        lookupdata->loadedSize = 0;
    }
    else
    {
#if DEBUG
        char sp54[128];
        u32  stack[2];
#endif

        romCopy(source, srcfile->romAddr, lookupdata->romSize);
        lookupdata->loadedSize = rzipInflate(source, ptrdata, buffer);;
#if DEBUG
        if (result == 0)
        {
            sprintf(sp54, "DMA-Crash %s %d Ram: %02x%02x%02x%02x%02x%02x%02x%02x", "ob.c", 204, scratch[0], scratch[1], scratch[2], scratch[3], scratch[4], scratch[5], scratch[6], scratch[7]);
            crashSetMessage(sp54);
            CRASH();
        }
#endif

    }
}

void fileLoadFromHost(u8 *ptrdata, s32 bytes,  FileEntry *srcfile,  FileInfo *lookupdata)
{
    u8 *pPayload;
    u8 buffer[8448];
    s32 size;
    static const u8 rz_header_1[] = {0x11, 0x72, 0x00, 0x00};
    static const u8 rz_header_2[] = {0x11, 0x72, 0x00, 0x00};

    if (!bytes)
    {
        indycommHostLoadFile(srcfile->filename, ptrdata);
        return;
    }
    indycommHostCheckFileExists(srcfile->filename, &lookupdata->hostSize);
    pPayload = (ptrdata + bytes) - ((lookupdata->hostSize + 7) & -8);
    if ((pPayload - ptrdata) < 8U)
    {
        lookupdata->loadedSize = 0;
    }
    else
    {
        indycommHostLoadFile(srcfile->filename, pPayload);
        if ((pPayload[0] == rz_header_1[0]) && (pPayload[1] == rz_header_2[1]))
        {
            size = rzipInflate(pPayload, ptrdata, &buffer);
        }
        else
        {
            bcopy(pPayload, ptrdata, lookupdata->hostSize);
            size = lookupdata->hostSize;
        }
        lookupdata->loadedSize = size;
    }
}





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


#if !defined(LEFTOVERDEBUG)
/* VERSION_EU */
/* same as below version, shuffled in EU */
void fileLoadPartByName(u8 *bgname, u8 *target, s32 offset, s32 len)
{
  s32 index;
  FileEntry *entry;

  index = fileGetIndex(bgname);
  entry = &g_FileTable[index];

  if (m_fileInfo[index].romSize != 0)
  {
    //if the size of offset data would exceed file size, loop forever
    if ((m_fileInfo[index].romSize + 0xF) < (offset + len))
    {
      while (1){};
    }
    romCopy(target, &entry->romAddr[offset], len, entry);
  }

}
#endif

#if defined(LEFTOVERDEBUG)
/* no VERSION_EU */
void *fileLoadToPool2(s32 index, FileLoadMode param_2, s32 size, u8 bank)
{
    return fileLoadToPool(index, param_2, size, bank);
}
#endif

#if defined(LEFTOVERDEBUG)
/* no VERSION_EU */
void *fileLoadToAddr2(int index, FileLoadMode param_2, u8 *ptrdata, int size)
{
    return fileLoadToAddr(index, param_2, ptrdata, size);
}
#endif

void *fileLoadByNameToPool(char *filename, FileLoadMode loadMethod, s32 size, u8 bank)
{
    return fileLoadToPool(fileGetIndex(filename), loadMethod, size, bank);
}

void * fileLoadByNameToAddr(char *filename, FileLoadMode loadMethod, u8 *ptrdata, s32 size)
{
    return fileLoadToAddr(fileGetIndex(filename), loadMethod, ptrdata, size);
}

#if defined(LEFTOVERDEBUG)
/* no VERSION_EU */
/**
 * 0F18AC 7F0BCD7C
 * loads data stored at an offset of a bg file
 */
void fileLoadPartByName(u8 *bgname, u8 *target, s32 offset, s32 len)
{
  s32 index;
  FileEntry *entry;

  index = fileGetIndex(bgname);
  entry = &g_FileTable[index];

  if (m_fileInfo[index].romSize != 0)
  {
    //if the size of offset data would exceed file size, loop forever
    if ((m_fileInfo[index].romSize + 0xF) < (offset + len))
    {
      while (1){};
    }
    romCopy(target, &entry->romAddr[offset], len, entry);
  }

}
#endif





void *fileLoadToPool(s32 index, FileLoadMode loadMethod, s32 size, u8 bank) //#MATCH https://decomp.me/scratch/uqiBe
{
    FileInfo *info = &m_fileInfo[index];
    s32                         bytes;
    void                       *ptrdata = NULL;

    if (loadMethod == FILELOAD_KEEP_SPARE || loadMethod == FILELOAD_TRIM || loadMethod == FILELOAD_TRIM_2)
    {
        // bytes = info->loadedSize;
        if (info->loadedSize == 0)
        { // verify pool remaining is 0
            info->loadedSize = mempGetFreeBytes(bank);
            // info->loadedSize = bytes;
        }
        // bytes = info->loadedSize;
        ptrdata             = mempAlloc(info->loadedSize, bank); // get pointer to allocated space in bank
        info->allocatedSize = info->loadedSize;

        if (g_FileTable[index].romAddr == 0) //IF NULL, check indy
        {
            fileLoadFromHost(ptrdata, info->loadedSize, &g_FileTable[index], info);
        }
        else
        {
            fileLoadFromRom(ptrdata, info->loadedSize, &g_FileTable[index], info);
        }
        if (loadMethod != FILELOAD_KEEP_SPARE)
        {
            // mempRealloc
            mempResizeLast(ptrdata, info->loadedSize, bank);
        }
    }
    else // skipped in PD
    {
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
        ptrdata             = mempAlloc(info->loadedSize, bank);
        info->allocatedSize = info->loadedSize;

        if (g_FileTable[index].romAddr == 0)
        {
            fileLoadFromHost(ptrdata, 0, &g_FileTable[index], info);
        }
        else
        {
            fileLoadFromRom(ptrdata, 0, &g_FileTable[index], info);
        }
        if (size == 0)
        {
            info->loadedPool = bank;
        }
    }
    return ptrdata;
}





void *fileLoadToAddr(s32 index, FileLoadMode loadMethod, void *ptrdata, s32 bytes) //#match https://decomp.me/scratch/YExRh
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
    if (loadMethod == FILELOAD_KEEP_SPARE || loadMethod == FILELOAD_TRIM || loadMethod == FILELOAD_TRIM_2)
    {
        if (!g_FileTable[index].romAddr)
        {
            info->allocatedSize = bytes;
            fileLoadFromHost(ptrdata, bytes, &g_FileTable[index], &m_fileInfo[index]);
        }
        else
        {
            info->allocatedSize = bytes;
            //fix a1/a0 inversion by manual pointer "info"
            fileLoadFromRom(ptrdata, bytes, &g_FileTable[index], &m_fileInfo[index]);
        }
    }
    else
    {
        if (!g_FileTable[index].romAddr)
        {
            fileLoadFromHost(ptrdata, 0, &g_FileTable[index], &m_fileInfo[index]);
        }
        else
        {
            fileLoadFromRom(ptrdata, 0, &g_FileTable[index], &m_fileInfo[index]);
        }
    }

    return ptrdata;
}






s32 fileGetLoadedSize(s32 index)
{
    return m_fileInfo[index].loadedSize;
}


s32 fileGetAllocSize(s32 index)
{
    return m_fileInfo[index].allocatedSize;
}


void fileSetSize(s32 filenum, u8* ptr, u32 size, s32 reallocate)
{
    m_fileInfo[filenum].loadedSize = size;
    m_fileInfo[filenum].allocatedSize = size;
    if (reallocate != 0)
    {
        mempResizeLast(ptr, m_fileInfo[filenum].loadedSize, MEMPOOL_STAGE);
    }
}


s32 fileGetLoadedSizeByName(u8 *name)
{
    int index;

    index = fileGetIndex(name);
    return m_fileInfo[index].loadedSize;
}


void fileForgetPool(u8 bank)
{
    int i;
    for (i = 1; i < m_fileCount; i++) {
        if (m_fileInfo[i].loadedPool <= bank) {
            m_fileInfo[i].loadedPool = '\0';
        }
        if (bank == 4) {
            m_fileInfo[i].loadedSize = 0;
        }
    }
}

void fileForgetMePool(void) {
  fileForgetPool(MEMPOOL_ME);
}




s32 fileGetIndex(char *resname)
{
    s32 i;
    s32 stack;
    s32 size;
    struct FileInfo *lookup;

    for (i = 1; i < m_fileCount; i++)
    {
        if (g_FileTable[i].filename != NULL)
        {
            if (strcmp(resname, g_FileTable[i].filename) == 0)
            {
                return i;
            }
        }
    }
    
    i = m_fileCount;
    
    //too many files exist
    if (i >= OBJ_INDEX_MAX)
    {
        return 0;
    }
    
    m_fileCount++;

    if (indycommHostCheckFileExists(resname, &size) == 0)
    {
        return 0;
    }

    g_FileTable[i].index = i; // offset 0
    g_FileTable[i].filename = resname;  // offset 4

    lookup = &m_fileInfo[i];
    
    if(1);

    lookup->unk_11 = 0;
    g_FileTable[i].romAddr = 0;  // offset 8
    lookup->romSize = 0;
    lookup->loadedSize = 0;
    lookup->hostSize = ALIGN16_a(size);
    lookup->allocatedSize = 0;
    lookup->loadedPool = 0;

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




// removed
void sub_GAME_7F0BD410(void)
{
    s32 i;

    for (i = 1; i < m_fileCount; i++)
    {
        // unknown which property is referenced, just need to get the compiler to
        // correctly reference the parent object.
        if (m_fileInfo[i].loadedSize)
        {
            // removed
        }    
    }
}



