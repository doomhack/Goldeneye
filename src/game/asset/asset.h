#ifndef _ASSET_H_
#define _ASSET_H_
#include <ultra64.h>
#include "memp.h"

/** A file in the ROM: an entry of g_FileTable (assets/obseg/file_resource_table.inc.c). */
typedef struct AssetEntry
{
    s32   index;
    char *filename;
    u8   *romAddr;
} AssetEntry;

void  assetInit(void);
void  assetForgetSizes(void);
s32   assetFind(const char *name);

void *assetLoad(const char *name, MemPool pool);
void *assetLoadUntrimmed(const char *name, MemPool pool);
void *assetLoadInto(const char *name, void *dst, u32 bufferSize);
void  assetLoadPart(const char *name, void *dst, u32 offset, u32 len);

u32   assetGetSize(s32 index);
u32   assetGetSizeByName(const char *name);
u32   assetGetBufferSize(s32 index);
void  assetSetSize(s32 index, void *data, u32 size, bool shrinkAllocation);

#endif
