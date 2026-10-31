/**
 * Graphics memory for each frame.
 *
 * There are two kinds: the master display list, which every drawing function appends to,
 * and the vertex memory, for the vertices, matrices and lights a frame's display list
 * points at (dynAllocate*). Each is two buffers: the CPU builds a frame in one while the
 * RSP draws the last frame from the other, and dynSwapBuffers swaps them when a frame is
 * finished. Their sizes come from the level's memory budget (boss.c), doubled in co-op,
 * which draws the level twice.
 */

#include <ultra64.h>
#include <memp.h>
#include <macro.h>
#include "boss.h"
#include "dbgscreen.h"
#include "game/gfx/dyn.h"
#include "game/player/player.h"
#include "game/level/gamemode.h"

/* A pair of buffers, the one in use given by s_Current */
typedef struct
{
    u8 *buffers[2];
    s32 size;
} DynBuffers;

static DynBuffers s_Gfx;
static DynBuffers s_Vtx;
static s32 s_Current;
static u8 *s_VtxNext; /* the next free byte of this frame's vertex memory */

static void initBuffers(DynBuffers *pair, s32 size)
{
    pair->size = size;
    pair->buffers[0] = mempAlloc(size * 2, MEMPOOL_STAGE);
    pair->buffers[1] = pair->buffers[0] + size;
}

void dynInitMemory(void)
{
    s32 views = gameIsTwoPlayerMission() ? 2 : 1;

    initBuffers(&s_Gfx, g_LevelMemBudget->gfxKB * 1024 * views);
    initBuffers(&s_Vtx, g_LevelMemBudget->vtxKB * 1024 * views);

    s_Current = 0;
    s_VtxNext = s_Vtx.buffers[0];
}

Gfx *dynGetMasterDisplayList(void)
{
    return (Gfx *)s_Gfx.buffers[s_Current];
}

/** Takes `size` bytes of this frame's vertex memory, rounded up to 16 */
void *dynAllocate(s32 size)
{
    u8 *ptr = s_VtxNext;

    s_VtxNext += ALIGN16_a(size);

    if (s_VtxNext > s_Vtx.buffers[s_Current] + s_Vtx.size)
    {
        dbgFatal("dyn: out of vertex memory", s_Vtx.size);
    }

    return ptr;
}

void *dynAllocateVertices(s32 count)
{
    return dynAllocate(count * sizeof(Vtx));
}

Mtx *dynAllocateMatrix(void)
{
    return dynAllocate(sizeof(Mtx));
}

Light *dynAllocateLights(s32 count)
{
    return dynAllocate(count * sizeof(Light));
}

void dynSwapBuffers(void)
{
    s_Current ^= 1;
    s_VtxNext = s_Vtx.buffers[s_Current];
}
