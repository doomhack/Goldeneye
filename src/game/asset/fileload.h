#ifndef _FILELOAD_H_
#define _FILELOAD_H_
#include <ultra64.h>

typedef struct FileInfo
{
    u32 romSize;
    u32 loadedSize;
    u32 allocatedSize;
    u8  loadedPool;
    u8  unk_11;
    u16 reserved;
} FileInfo;

typedef struct FileEntry
{
    s32   index;
    char *filename;
    u8   *romAddr;
} FileEntry;

typedef enum FileLoadMode
{
    FILELOAD_SIGNED_ENUM = -1,
    FILELOAD_KEEP_SPARE,
    FILELOAD_TRIM,
    FILELOAD_TRIM_2
} FileLoadMode;

void  fileLoadFromRom(u8 *ptrdata, s32 bytes,  FileEntry *srcfile,  FileInfo *lookupdata);

void  fileForgetPool(u8 i);
void  fileForgetMePool(void);

void *fileLoadToPool(s32 index, FileLoadMode loadMethod, s32 size, u8 bank);
void *fileLoadToAddr(s32 index, FileLoadMode loadMethod, void *ptrdata, s32 bytes);

void *fileLoadByNameToAddr(char *filename, FileLoadMode loadMethod, u8 *ptrdata, s32 size);
void *fileLoadByNameToPool(char *filename, FileLoadMode loadMethod, s32 size, u8 bank);


void fileSetSize(s32 filenum, u8* ptr, u32 size, s32 reallocate);
s32 fileGetLoadedSizeByName(u8 *name);
s32 fileGetAllocSize(s32 index);
s32 fileGetLoadedSize(s32 index);
s32 fileGetIndex(char *resname);

void fileInit(void);
void fileLoadPartByName(u8 *name, u8 *dst, s32 offset, s32 len);

#endif
