#ifndef _MEMP_H_
#define _MEMP_H_

#include <ultra64.h>

/*
* Align to 16 bit boundary. Version "b", without preliminary addition.
*/
#define ALIGN16_b(val)        (((val) | 0xf) ^ 0xf)

typedef struct MemoryPool {
    u8 *start;
    u8 *pos;
    u8 *end;
    u8 *prevpos;
} MemoryPool;

typedef struct MempPoolSizes { //mempSizes
    u32 mfIndex;
    u32 mf;
    u32 mlIndex;
    u32 ml;
    u32 meIndex;
    u32 me;
    u32 EndIndex;
    u32 EndPool;
} MempPoolSizes;

// Pool Names
enum MEMPOOL
{
    MEMPOOL_SIGNED_ENUM = -1,
    MEMPOOL_TOTAL, // the mempool starts at _heapSegmentStart and ends at the top of the 8MB
    MEMPOOL_MF,
    MEMPOOL_2,
    MEMPOOL_ML,
    MEMPOOL_STAGE,
    MEMPOOL_ME,
    MEMPOOL_PERMANENT,
    MEMPOOL_COUNT
};

typedef enum MEMP_RESIZE_RESULT
{
    MEMP_RESIZE_RESULT_SIGNED_ENUM = -1,
    MEMP_RESIZE_OK = 1,
    MEMP_RESIZE_NOT_LAST = 2
} MEMP_RESIZE_RESULT;

void mempInit(void);
void mempInitPools(int bstart,int bsize);
void mempSetPoolSizes(s32 banks[8]);
void *mempAlloc(u32 bytes,u8 bank);
MEMP_RESIZE_RESULT mempResizeLast(void *allocation, s32 newsize, u8 poolnum);
void mempCheckPools(void);
s32 mempGetFreeBytes(u8 bank);
u32 mempAllocPacked(u32 param_1);
void mempResetPool(u8 bank);
void mempClosePool(u8 bank);

#endif
