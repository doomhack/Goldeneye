#include <ultra64.h>
#include "bondtypes.h"
#include "game/model/initunk_005450.h"

#define MODELHITENTRIES_LEN 600

extern struct ModelHitEntry *g_ModelHitFreeList; // canonically freedist

extern ModelHitEntry g_ModelHitEntries[MODELHITENTRIES_LEN]; // dword_CODE_bss_80076A50

/**
 * Address: 7F005450
 * 
 * Called by stage load.
 * 
 * Initializes a fixed pool of ModelHitEntry records used while building per-model hit/collision traversal lists.
 * The pool is threaded as a doubly linked free list and consumed by objecthandler.c.
 */
void initModelHitEntryFreeList(void)
{
    s32 i;
    ModelHitEntry *entries = g_ModelHitEntries;

    g_ModelHitFreeList = entries;

    entries[0].next = &entries[1];

    for (i = 1; i < MODELHITENTRIES_LEN - 1; i++)
    {
        entries[i].next = &entries[i + 1];
        entries[i].prev = &entries[i - 1];
    }

    g_ModelHitEntries[MODELHITENTRIES_LEN - 1].prev = &g_ModelHitEntries[MODELHITENTRIES_LEN - 2];
}
