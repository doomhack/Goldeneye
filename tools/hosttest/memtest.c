/**
 * Host tests for the memory allocators (src/memp.c and src/mema.c). See run.sh.
 *
 * The modules are included whole so the tests can check their private state. dbgFatal is
 * replaced by a stub that jumps back to the test that expected it.
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/memp.c"
#include "../../src/mema.c"

static s32 s_failures;
static jmp_buf s_fatalJump;
static bool s_fatalExpected;
static const char *s_fatalMessage;

[[noreturn]] void dbgFatal(const char *message, u32 value)
{
    s_fatalMessage = message;

    if (!s_fatalExpected)
    {
        printf("unexpected fatal error: %s %x\n", message, value);
        exit(1);
    }

    longjmp(s_fatalJump, 1);
}

#define CHECK(cond) \
    do { if (!(cond)) { printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); s_failures++; } } while (0)

/* Runs `stmt` and checks that it stops the game with a fatal error. */
#define CHECK_FATAL(stmt)                          \
    do {                                           \
        s_fatalExpected = true;                    \
        s_fatalMessage = NULL;                     \
        if (setjmp(s_fatalJump) == 0) { stmt; }    \
        s_fatalExpected = false;                   \
        CHECK(s_fatalMessage != NULL);             \
    } while (0)

static alignas(16) u8 s_ram[1 << 20];

static void testMemp(void)
{
    u8 *a;
    u8 *b;

    mempInit(s_ram, s_ram + sizeof(s_ram));
    CHECK(mempGetFreeBytes(MEMPOOL_STAGE) == 0); // closed
    CHECK_FATAL(mempAlloc(16, MEMPOOL_STAGE));

    mempResetPool(MEMPOOL_STAGE);
    mempResetPool(MEMPOOL_PERMANENT);
    CHECK(mempGetFreeBytes(MEMPOOL_STAGE) == sizeof(s_ram) - MEMP_PERMANENT_SIZE);
    CHECK(mempGetFreeBytes(MEMPOOL_PERMANENT) == MEMP_PERMANENT_SIZE);

    a = mempAlloc(100, MEMPOOL_STAGE);
    b = mempAlloc(200, MEMPOOL_STAGE);
    CHECK(a == s_ram);
    CHECK(b == a + 100);
    CHECK(mempAlloc(16, MEMPOOL_PERMANENT) == s_ram + sizeof(s_ram) - MEMP_PERMANENT_SIZE);

    // Only the last allocation can be resized
    CHECK(!mempResizeLast(a, 50));
    CHECK(mempResizeLast(b, 50));
    CHECK(mempAlloc(1, MEMPOOL_STAGE) == b + 50);
    CHECK(mempResizeLast(b + 50, 1000));
    CHECK(mempGetFreeBytes(MEMPOOL_STAGE) == sizeof(s_ram) - MEMP_PERMANENT_SIZE - 1150);
    CHECK_FATAL(mempResizeLast(b + 50, sizeof(s_ram)));

    // Taking all the free space, as fileLoadToPool does, then trimming it
    a = mempAlloc(mempGetFreeBytes(MEMPOOL_STAGE), MEMPOOL_STAGE);
    CHECK(mempGetFreeBytes(MEMPOOL_STAGE) == 0);
    CHECK_FATAL(mempAlloc(1, MEMPOOL_STAGE));
    CHECK(mempResizeLast(a, 64));
    CHECK(mempGetFreeBytes(MEMPOOL_STAGE) > 0);

    mempResetPool(MEMPOOL_STAGE);
    CHECK(mempAlloc(8, MEMPOOL_STAGE) == s_ram);
    mempClosePool(MEMPOOL_STAGE);
    CHECK_FATAL(mempAlloc(8, MEMPOOL_STAGE));
}

/* Checks mema's free list: sorted, inside the heap, aligned, and no two ranges touching. */
static void checkMemaList(u8 *heap, u32 heapSize)
{
    for (s32 i = 0; i < m_memaFreeCount; i++)
    {
        CHECK(m_memaFree[i].size != 0);
        CHECK(((uintptr_t) m_memaFree[i].start & 15) == 0);
        CHECK((m_memaFree[i].size & 15) == 0);
        CHECK(m_memaFree[i].start >= heap);
        CHECK(m_memaFree[i].start + m_memaFree[i].size <= heap + heapSize);

        if (i > 0)
        {
            CHECK(m_memaFree[i - 1].start + m_memaFree[i - 1].size < m_memaFree[i].start);
        }
    }
}

typedef struct Allocation {
    u8 *addr;
    u32 size;
} Allocation;

/** Random allocations, frees and resizes, checked against a byte map of the heap. */
static void testMemaRandom(u32 seed)
{
    enum { HEAP = 256 * 1024, MAX = 2000 };
    static Allocation live[MAX];
    static u8 owner[HEAP]; // 1 where the heap is allocated
    s32 count = 0;
    u8 *heap = s_ram;

    srand(seed);
    memset(owner, 0, sizeof(owner));
    memaReset(heap, HEAP);

    for (s32 step = 0; step < 200000; step++)
    {
        s32 op = rand() % 10;

        if (op < 5 && count < MAX)
        {
            u32 size = 1 + rand() % ((rand() % 8) ? 512 : 16384);
            u8 *p = memaAlloc(size);

            if (p == NULL)
            {
                CHECK(memaGetLongestFree() < MEMA_ALIGN(size));
                continue;
            }

            CHECK(((uintptr_t) p & 15) == 0);
            CHECK(p >= heap && p + MEMA_ALIGN(size) <= heap + HEAP);

            for (u32 i = 0; i < MEMA_ALIGN(size); i++)
            {
                CHECK(owner[p - heap + i] == 0);
                owner[p - heap + i] = 1;
            }

            live[count++] = (Allocation) { p, size };
        }
        else if (op < 9 && count > 0)
        {
            s32 k = rand() % count;
            memset(&owner[live[k].addr - heap], 0, MEMA_ALIGN(live[k].size));
            memaFree(live[k].addr, live[k].size);
            live[k] = live[--count];
        }
        else if (count > 0)
        {
            // Resize, as bg.c does after loading a room
            s32 k = rand() % count;
            u32 newSize = 1 + rand() % (MEMA_ALIGN(live[k].size) * 2);

            if (memaRealloc(live[k].addr, live[k].size, newSize))
            {
                memset(&owner[live[k].addr - heap], 0, MEMA_ALIGN(live[k].size));

                for (u32 i = 0; i < MEMA_ALIGN(newSize); i++)
                {
                    CHECK(owner[live[k].addr - heap + i] == 0);
                    owner[live[k].addr - heap + i] = 1;
                }

                live[k].size = newSize;
            }
            else
            {
                CHECK(newSize > live[k].size);
            }
        }

        if (step % 1000 == 0)
        {
            u32 freeBytes = 0;

            checkMemaList(heap, HEAP);

            for (s32 i = 0; i < m_memaFreeCount; i++)
            {
                freeBytes += m_memaFree[i].size;

                for (u32 j = 0; j < m_memaFree[i].size; j++)
                {
                    CHECK(owner[m_memaFree[i].start - heap + j] == 0);
                }
            }

            // Every byte is either allocated or free (no range has been dropped)
            u32 usedBytes = 0;
            for (s32 i = 0; i < HEAP; i++)
            {
                usedBytes += owner[i];
            }
            CHECK(usedBytes + freeBytes == HEAP);
        }
    }

    // Free everything: the heap is one range again
    while (count > 0)
    {
        count--;
        memaFree(live[count].addr, live[count].size);
    }

    CHECK(m_memaFreeCount == 1);
    CHECK(memaGetLongestFree() == HEAP);
}

static void testMemaEdges(void)
{
    u8 *a;
    u8 *b;
    u8 *c;

    // An unaligned heap is trimmed to 16 bytes
    memaReset(s_ram + 3, 1000);
    CHECK(m_memaFreeCount == 1);
    CHECK(m_memaFree[0].start == s_ram + 16);
    CHECK(m_memaFree[0].size == 976);

    memaReset(s_ram, 4096);
    a = memaAlloc(100);
    b = memaAlloc(100);
    c = memaAlloc(100);
    CHECK(a == s_ram && b == s_ram + 112 && c == s_ram + 224);
    CHECK(memaAlloc(0) == NULL);
    CHECK(memaAlloc(5000) == NULL);

    // Freeing the middle one, then both sides, merges into one range
    memaFree(b, 100);
    CHECK(m_memaFreeCount == 2);
    memaFree(a, 100);
    CHECK(m_memaFreeCount == 2);
    memaFree(c, 100);
    CHECK(m_memaFreeCount == 1);
    CHECK(memaGetLongestFree() == 4096);

    // Double frees and overlapping frees stop the game
    a = memaAlloc(64);
    memaFree(a, 64);
    CHECK_FATAL(memaFree(a, 64));
    a = memaAlloc(64);
    b = memaAlloc(64);
    memaFree(b, 64);
    CHECK_FATAL(memaFree(a, 128));

    // Best fit: a 32-byte hole is used before the big free range
    memaReset(s_ram, 4096);
    a = memaAlloc(32);
    b = memaAlloc(64);
    c = memaAlloc(32);
    memaFree(a, 32);
    CHECK(memaAlloc(20) == a);

    // Growing in place takes the free space right after the allocation
    memaReset(s_ram, 4096);
    a = memaAlloc(64);
    CHECK(memaRealloc(a, 64, 1024));
    CHECK(memaAlloc(16) == a + 1024);
    CHECK(!memaRealloc(a, 1024, 2048));
    CHECK(memaRealloc(a, 1024, 32));
    CHECK(memaAlloc(992) == a + 32);

    // A full free list drops the smallest range instead of overflowing
    memaReset(s_ram, 64 * 1024);
    static u8 *blocks[64 * 1024 / 16 + 1];
    s32 n = 0;
    while ((blocks[n] = memaAlloc(16)) != NULL)
    {
        n++;
    }
    for (s32 i = 0; i < n; i += 2)
    {
        memaFree(blocks[i], 16);
    }
    CHECK(m_memaFreeCount == MEMA_MAX_FREE);
    checkMemaList(s_ram, 64 * 1024);
}

int main(void)
{
    testMemp();
    testMemaEdges();

    for (u32 seed = 1; seed <= 5; seed++)
    {
        testMemaRandom(seed);
    }

    printf(s_failures ? "memtest: %d checks failed\n" : "memtest: all passed\n", s_failures);
    return s_failures != 0;
}
