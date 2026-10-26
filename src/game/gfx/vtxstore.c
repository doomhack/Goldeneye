/**
 * Per-instance copies of model vertices, for models whose vertices change at run time.
 *
 * Characters get a copy when a blood stain is painted on them (VTXSTORE_CHR). Props get one
 * when damage moves their vertices (VTXSTORE_OBJ); props with the same model and damage
 * level can share a copy, and vtxstoreTick merges such duplicates when the store runs low.
 * Each store is a block of vertices carved into batches, kept as a doubly linked list in
 * address order so neighbouring free batches can be merged. Sizes are set per level by
 * vtxstoreReset (bigger in multiplayer).
 */

#include <ultra64.h>
#include <memp.h>
#include "bondtypes.h"
#include "game/gfx/vtxstore.h"
#include "game/prop/propobj.h"
#include "game/model/model.h"

/** One block of a store: in use (refs > 0), free (0) or an unused entry (-1). */
struct VtxstoreBatch {
    Vertex* vertices;
    s32 modeldef;
    s32 destroyedLevel;
    s16 numVtx;
    s16 refs;
    s16 next;
    s16 prev;
};

// bss
//CODE.bss:8007A0D0
s32 g_VtxstoreChrNumVtx; // item count for g_VtxstoreChrVtx
//CODE.bss:8007A0D4
s32 g_VtxstoreChrNumBatches; // item count for g_VtxstoreChrBatches
//CODE.bss:8007A0D8
s32 g_VtxstoreObjNumVtx; // item count for g_VtxstoreObjVtx
//CODE.bss:8007A0DC
s32 g_VtxstoreObjNumBatches; // item count for g_VtxstoreObjBatches
//CODE.bss:8007A0E0
Vertex* g_VtxstoreChrVtx; // array ( uses g_VtxstoreChrNumVtx as alloc count, item size 0x10 )
//CODE.bss:8007A0E4
Vertex* g_VtxstoreObjVtx; // array ( uses g_VtxstoreObjNumVtx as alloc count, item size 0x10 )
//CODE.bss:8007A0E8
struct VtxstoreBatch* g_VtxstoreChrBatches; // array ( uses g_VtxstoreChrNumBatches as alloc count, item size 0x14 )
//CODE.bss:8007A0EC
struct VtxstoreBatch* g_VtxstoreObjBatches; // array ( uses g_VtxstoreObjNumBatches as alloc count, item size 0x14 )
//CODE.bss:8007A0F0
s16 g_VtxstoreChrFreeVtx;
//CODE.bss:8007A0F2
s16 g_VtxstoreObjFreeVtx;

/*
* Address: 7F09B820(
* PD name: vtxstore_reset
*/

void vtxstoreReset(void)
{
    u32 tmp;
    s32 stage;
    s32 i;

    tmp = 0x5DC;

    if (getPlayerCount() >= 2)
    {
        g_VtxstoreChrNumVtx = 0xBB8;
        g_VtxstoreChrNumBatches = 0x50;
        g_VtxstoreObjNumVtx = 0x1F4;
        g_VtxstoreObjNumBatches = 0x14;
    }
    else
    {
        stage = lvlGetCurrentStageToLoad();
        if ((stage != 0x1E) && (stage != 0x1D))
        {
            g_VtxstoreChrNumVtx = 0x1F4;
            g_VtxstoreChrNumBatches = 0x14;
            g_VtxstoreObjNumVtx = tmp;
            g_VtxstoreObjNumBatches = 0x28;
        }
        else
        {
            g_VtxstoreChrNumVtx = 0x1F4;
            g_VtxstoreChrNumBatches = 0x14;
            g_VtxstoreObjNumVtx = 0x1F4;
            g_VtxstoreObjNumBatches = 0x14;
        }
    }

    tmp = 0x14;
    g_VtxstoreChrBatches = mempAlloc(g_VtxstoreChrNumBatches * tmp, MEMPOOL_STAGE);
    g_VtxstoreChrVtx = mempAlloc(g_VtxstoreChrNumVtx * 0x10, MEMPOOL_STAGE);
    g_VtxstoreObjBatches = mempAlloc(g_VtxstoreObjNumBatches * tmp, MEMPOOL_STAGE);
    g_VtxstoreObjVtx = mempAlloc(g_VtxstoreObjNumVtx * 0x10, MEMPOOL_STAGE);

    g_VtxstoreChrFreeVtx = (s16) g_VtxstoreChrNumVtx;
    g_VtxstoreChrBatches->vertices = g_VtxstoreChrVtx;
    g_VtxstoreChrBatches->numVtx = (s16) g_VtxstoreChrNumVtx;
    g_VtxstoreChrBatches->refs = 0;
    g_VtxstoreChrBatches->next = -1;
    g_VtxstoreChrBatches->prev = -1;

    for (i = 1; i < g_VtxstoreChrNumBatches; i++)
    {
        g_VtxstoreChrBatches[i].refs = -1;
    }

    g_VtxstoreObjFreeVtx = (s16) g_VtxstoreObjNumVtx;
    g_VtxstoreObjBatches->vertices = g_VtxstoreObjVtx;
    g_VtxstoreObjBatches->numVtx = (s16) g_VtxstoreObjNumVtx;
    g_VtxstoreObjBatches->refs = 0;
    g_VtxstoreObjBatches->next = -1;
    g_VtxstoreObjBatches->prev = -1;

    for (i = 1; i < g_VtxstoreObjNumBatches; i++)
    {
        g_VtxstoreObjBatches[i].refs = -1;
    }
}


/*
* Address: 0x7F09BAC4
*
* PD name: vtxstore_fix_refs
* PD description:
*  Search all props and their model data for references to the `find` address
*  and replace it with the `replacement` address.
*/
void vtxstoreFixRefs(s32 find, s32 replacement) {
    PropRecord* var_s1;
    ChrRecord* var_v0;
    Model* temp_a0;
    s32* temp_v0_2;
    ModelNode* var_a1;
    ModelFileHeader* var_v1;
    s32 val;

    var_s1 = propsGetActiveTail();
    while (var_s1 != NULL) {
        if (var_s1->type == 1) {
            var_v0 = var_s1->chr;
            var_v1 = ((Model*)var_v0->chrflags)->obj;
            var_a1 = var_v1->RootNode;
            while (var_a1 != NULL) {
                val = var_a1->Opcode & 0xFF;
                if (val == 0x18) {
                    temp_v0_2 = modelGetNodeRwData((Model *)var_v0->chrflags, var_a1);
                    if (find == *temp_v0_2) {
                        *temp_v0_2 = replacement;
                    }
                    break;
                } else {
                    if (var_a1->Child != NULL) {
                        var_a1 = var_a1->Child;
                    } else {
                        while (var_a1 != NULL) {
                            if (var_a1->Next != NULL) {
                                var_a1 = var_a1->Next;
                                break;
                            }
                            var_a1 = var_a1->Parent;
                        }
                    }
                }
            }
        }

        var_s1 = var_s1->prev;
    }
}





/*
* Address: 7F09BBBC
* PD name: vtxstore_tick
* Description: Merge duplicate batches. May free memory.
*/
void vtxstoreTick(void)
{
    s16 temp_s2;
    s16 var_fp;
    s16 var_s2;
    s32 stop;
    s32 var_a1;
    s32 var_s6;

    var_s6 = 0;

    if (g_VtxstoreObjFreeVtx < ((s32)g_VtxstoreObjNumVtx >> 2))
    {
        for (var_fp = 0; var_fp < g_VtxstoreObjNumBatches - 1; var_fp++) {
            if (g_VtxstoreObjBatches[var_fp].refs > 0)
            {
                for (var_s2 = var_fp + 1; var_s2 < g_VtxstoreObjNumBatches; var_s2++) {
                    if ((g_VtxstoreObjBatches[var_s2].refs > 0) &&
                        (g_VtxstoreObjBatches[var_fp].modeldef == g_VtxstoreObjBatches[var_s2].modeldef) &&
                        (g_VtxstoreObjBatches[var_fp].destroyedLevel == g_VtxstoreObjBatches[var_s2].destroyedLevel))
                    {
                        vtxstoreFixRefs((s32)g_VtxstoreObjBatches[var_s2].vertices, (s32)g_VtxstoreObjBatches[var_fp].vertices);
                        var_s6 = 1;

                        g_VtxstoreObjBatches[var_fp].refs += g_VtxstoreObjBatches[var_s2].refs;
                        g_VtxstoreObjBatches[var_s2].refs = 0;
                        
                        g_VtxstoreObjFreeVtx += g_VtxstoreObjBatches[var_s2].numVtx;
                    }
                }
            }
        }
    }

    if (var_s6 != 0) {
        stop  = 0;
        var_fp = 0;

        while (stop == 0) {
            var_s2 = g_VtxstoreObjBatches[var_fp].next;

            if (var_s2 >= 0) {
                if (g_VtxstoreObjBatches[var_fp].refs == 0) {
                    if (g_VtxstoreObjBatches[var_s2].refs == 0) {
                        g_VtxstoreObjBatches[var_fp].numVtx += g_VtxstoreObjBatches[var_s2].numVtx;
                        g_VtxstoreObjBatches[var_s2].refs = -1;
                        var_s2 = g_VtxstoreObjBatches[var_s2].next;
                        g_VtxstoreObjBatches[var_fp].next = var_s2;

                        if (var_s2 >= 0) {
                            g_VtxstoreObjBatches[var_s2].prev = var_fp;
                        }
                        continue;
                    }
                }
                var_fp = var_s2;
            } else {
                stop = 1;
            }
        }
    }

    if (g_VtxstoreObjFreeVtx < ((s32)g_VtxstoreObjNumVtx >> 2)) {
        objsFreeDeformedVertices();
    }
}


/*
* Address: 7F09BE4C
* PD name: vtxstoreAllocate
* Description: Allocation for batches within the storage space
*/
s32 vtxstoreAllocate(s32 arg0, s32 type, s32 arg2, s32 arg3) 
{
    s16* var_t3;
    s16 temp_t2;
    s32 var_a1;
    s32 var_a2;
    s32 var_t4;
    s32 var_v0;
    s32 var_v1;
    s16 var_v1_2;
    struct VtxstoreBatch* var_t0;

    switch (type) {
        case VTXSTORE_CHR:
            var_t0 = g_VtxstoreChrBatches;
            var_t3 = &g_VtxstoreChrFreeVtx;
            var_a2 = (s16)g_VtxstoreChrNumBatches;
            break;
        case VTXSTORE_OBJ:
            var_t0 = g_VtxstoreObjBatches;
            var_t3 = &g_VtxstoreObjFreeVtx;
            var_a2 = (s16)g_VtxstoreObjNumBatches;
            break;
        default:
            return 0;
    }

    var_v1_2 = 0;
    var_v0 = 0;
    var_a1 = 0;
    
    do {
        if ((var_t0[var_a1].refs == 0) && (var_t0[var_a1].numVtx >= arg0)) {
            var_v1_2 = 1;
        } else {
            var_a1 = var_t0[var_a1].next;
            var_v0 += 1;
            if ((var_a1 == -1) || (var_a2 < var_v0)) {
                var_v1_2 = (s16)-1;
            }
        }
    } while (var_v1_2 == 0);
    if (var_a2 < var_v0) {
        return 0;
    }
    // FAKE
    if (var_v0) {}
    if (var_v1_2 == 1) {
        var_t4 = 0;
        temp_t2 = var_t0[var_a1].numVtx;
        var_t0[var_a1].modeldef = arg2;
        var_t0[var_a1].destroyedLevel = arg3;
        var_t0[var_a1].refs++;
        if (temp_t2 != arg0) {
            for (var_v1 = 0; var_v1 < var_a2; var_v1++) {
                if (var_t0[var_v1].refs == -1) {
                    var_t0[var_a1].numVtx = arg0;
                    var_t0[var_v1].vertices = var_t0[var_a1].vertices + arg0;
                    var_t0[var_v1].numVtx = temp_t2 - arg0;
                    var_t0[var_v1].refs = 0;
                    var_t0[var_v1].prev = var_a1;
                    var_t4 = 1;
                    var_t0[var_v1].next = var_t0[var_a1].next;
                    if (var_t0[var_a1].next >= 0) {
                        var_t0[var_t0[var_a1].next].prev = var_v1;
                    }
                    var_t0[var_a1].next = var_v1;
                    break;
                }
            }
        }
        if (var_t4 != 0) {
            *var_t3 -= arg0;
        } else {
            *var_t3 -= temp_t2;
        }
        return (s32)var_t0[var_a1].vertices;
    }
    return 0;
}


/*
* Address: 7F09C044
* PD name: vtxstore_free (likely)
* Description: Either deforming a vertex or frees a vertex (deallocation)
*/
void vtxstoreFree(Vertex* arg0) {
    s16* var_t2;
    struct VtxstoreBatch* var_a3;
    s16 var_a1;
    s32 stop;
    s32 var_v1;

    if ((arg0 >= g_VtxstoreChrVtx) &&
        (arg0 <= g_VtxstoreChrVtx + (g_VtxstoreChrNumVtx - 1))) {
        var_a3 = g_VtxstoreChrBatches;
        var_t2 = &g_VtxstoreChrFreeVtx;
    } else if ((arg0 >= g_VtxstoreObjVtx) &&
               (arg0 <= g_VtxstoreObjVtx + (g_VtxstoreObjNumVtx - 1))) {
        var_a3 = g_VtxstoreObjBatches;
        var_t2 = &g_VtxstoreObjFreeVtx;
    } else {
        return;
    }

    var_a1 = 0;
    stop = 0;

    while (stop == 0) {
        if (var_a3[var_a1].vertices == arg0) {
            stop = 1;
            // FAKE
            if (var_a3[var_a1].refs);
            var_a3[var_a1].refs--;
            if (var_a3[var_a1].refs == 0) {
                *var_t2 += var_a3[var_a1].numVtx;

                // Merge with next free block
                var_v1 = var_a3[var_a1].next;
                if (var_v1 >= 0) {
                    if (var_a3[var_v1].refs == 0) {
                        var_a3[var_a1].numVtx += var_a3[var_v1].numVtx;
                        var_a3[var_a1].next = var_a3[var_v1].next;
                        var_a3[var_v1].refs = -1;
                        var_v1 = var_a3[var_a1].next;
                        if (var_v1 >= 0) {
                            var_a3[var_v1].prev = var_a1;
                        }
                    }
                }

                // Merge with prev free block
                var_v1 = var_a3[var_a1].prev;
                if (var_v1 >= 0) {
                    if (var_a3[var_v1].refs == 0) {
                        var_a3[var_v1].numVtx += var_a3[var_a1].numVtx;
                        var_a3[var_v1].next = var_a3[var_a1].next;
                        var_a3[var_a1].refs = -1;
                        var_a1 = var_v1;
                        var_v1 = var_a3[var_a1].next;
                        if (var_v1 >= 0) {
                            var_a3[var_v1].prev = var_a1;
                        }
                    }
                }
            }
        } else {
            var_a1 = var_a3[var_a1].next;
            if (var_a1 == -1) {
                stop = 1;
            }
        }
    }
}

