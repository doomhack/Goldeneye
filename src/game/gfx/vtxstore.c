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
#include "game/level/lv.h"
#include "game/player/player.h"
#include "game/prop/prop.h"

/** One block of a store: in use (refs > 0), free (0) or an unused entry (-1). */
struct VtxstoreBatch {
    Vertex* vertices;
    ModelFileHeader* modeldef;
    s32 destroyedLevel;
    s16 numVtx;
    s16 refs;
    s16 next;
    s16 prev;
};

// bss
s32 g_VtxstoreChrNumVtx; // item count for g_VtxstoreChrVtx
s32 g_VtxstoreChrNumBatches; // item count for g_VtxstoreChrBatches
s32 g_VtxstoreObjNumVtx; // item count for g_VtxstoreObjVtx
s32 g_VtxstoreObjNumBatches; // item count for g_VtxstoreObjBatches
Vertex* g_VtxstoreChrVtx; // array ( uses g_VtxstoreChrNumVtx as alloc count, item size 0x10 )
Vertex* g_VtxstoreObjVtx; // array ( uses g_VtxstoreObjNumVtx as alloc count, item size 0x10 )
struct VtxstoreBatch* g_VtxstoreChrBatches; // array ( uses g_VtxstoreChrNumBatches as alloc count, item size 0x14 )
struct VtxstoreBatch* g_VtxstoreObjBatches; // array ( uses g_VtxstoreObjNumBatches as alloc count, item size 0x14 )
s16 g_VtxstoreChrFreeVtx;
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
        stage = lvGetCurrentStage();
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
* PD name: vtxstore_fix_refs
* PD description:
*  Search all props and their model data for references to the `find` address
*  and replace it with the `replacement` address.
*/
void vtxstoreFixRefs(Vertex *find, Vertex *replacement) {
    PropRecord* prop;
    Model* model;
    union ModelRwData* rwdata;
    ModelNode* node;

    prop = propsGetActiveTail();
    while (prop != NULL) {
        if (prop->type == PROP_TYPE_OBJ) {
            model = prop->obj->model;
            node = model->obj->RootNode;
            while (node != NULL) {
                if ((node->Opcode & 0xFF) == MODELNODE_OPCODE_DLCOLLISION) {
                    rwdata = modelGetNodeRwData(model, node);
                    if (rwdata->DisplayListCollisions.Vertices == find) {
                        rwdata->DisplayListCollisions.Vertices = replacement;
                    }
                    break;
                } else {
                    if (node->Child != NULL) {
                        node = node->Child;
                    } else {
                        while (node != NULL) {
                            if (node->Next != NULL) {
                                node = node->Next;
                                break;
                            }
                            node = node->Parent;
                        }
                    }
                }
            }
        }

        prop = prop->prev;
    }
}





/*
* PD name: vtxstore_tick
* Description: Merge duplicate batches. May free memory.
*/
void vtxstoreTick(void)
{
    s16 i;
    s16 j;
    s32 stop;
    s32 merged;

    merged = 0;

    if (g_VtxstoreObjFreeVtx < ((s32)g_VtxstoreObjNumVtx >> 2))
    {
        for (i = 0; i < g_VtxstoreObjNumBatches - 1; i++) {
            if (g_VtxstoreObjBatches[i].refs > 0)
            {
                for (j = i + 1; j < g_VtxstoreObjNumBatches; j++) {
                    if ((g_VtxstoreObjBatches[j].refs > 0) &&
                        (g_VtxstoreObjBatches[i].modeldef == g_VtxstoreObjBatches[j].modeldef) &&
                        (g_VtxstoreObjBatches[i].destroyedLevel == g_VtxstoreObjBatches[j].destroyedLevel))
                    {
                        vtxstoreFixRefs(g_VtxstoreObjBatches[j].vertices, g_VtxstoreObjBatches[i].vertices);
                        merged = 1;

                        g_VtxstoreObjBatches[i].refs += g_VtxstoreObjBatches[j].refs;
                        g_VtxstoreObjBatches[j].refs = 0;
                        
                        g_VtxstoreObjFreeVtx += g_VtxstoreObjBatches[j].numVtx;
                    }
                }
            }
        }
    }

    if (merged != 0) {
        stop  = 0;
        i = 0;

        while (stop == 0) {
            j = g_VtxstoreObjBatches[i].next;

            if (j >= 0) {
                if (g_VtxstoreObjBatches[i].refs == 0) {
                    if (g_VtxstoreObjBatches[j].refs == 0) {
                        g_VtxstoreObjBatches[i].numVtx += g_VtxstoreObjBatches[j].numVtx;
                        g_VtxstoreObjBatches[j].refs = -1;
                        j = g_VtxstoreObjBatches[j].next;
                        g_VtxstoreObjBatches[i].next = j;

                        if (j >= 0) {
                            g_VtxstoreObjBatches[j].prev = i;
                        }
                        continue;
                    }
                }
                i = j;
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
* PD name: vtxstoreAllocate
* Description: Allocation for batches within the storage space
*/
Vertex *vtxstoreAllocate(s32 numVtx, s32 type, ModelFileHeader *modeldef, s32 destroyedLevel)
{
    s16* freeVtx;
    s16 batchVtx;
    s32 index;
    s32 numBatches;
    s32 split;
    s32 steps;
    s32 spare;
    s16 result;
    struct VtxstoreBatch* batches;

    switch (type) {
        case VTXSTORE_CHR:
            batches = g_VtxstoreChrBatches;
            freeVtx = &g_VtxstoreChrFreeVtx;
            numBatches = (s16)g_VtxstoreChrNumBatches;
            break;
        case VTXSTORE_OBJ:
            batches = g_VtxstoreObjBatches;
            freeVtx = &g_VtxstoreObjFreeVtx;
            numBatches = (s16)g_VtxstoreObjNumBatches;
            break;
        default:
            return NULL;
    }

    result = 0;
    steps = 0;
    index = 0;
    
    do {
        if ((batches[index].refs == 0) && (batches[index].numVtx >= numVtx)) {
            result = 1;
        } else {
            index = batches[index].next;
            steps += 1;
            if ((index == -1) || (numBatches < steps)) {
                result = (s16)-1;
            }
        }
    } while (result == 0);
    if (numBatches < steps) {
        return NULL;
    }
    if (result == 1) {
        split = 0;
        batchVtx = batches[index].numVtx;
        batches[index].modeldef = modeldef;
        batches[index].destroyedLevel = destroyedLevel;
        batches[index].refs++;
        if (batchVtx != numVtx) {
            for (spare = 0; spare < numBatches; spare++) {
                if (batches[spare].refs == -1) {
                    batches[index].numVtx = numVtx;
                    batches[spare].vertices = batches[index].vertices + numVtx;
                    batches[spare].numVtx = batchVtx - numVtx;
                    batches[spare].refs = 0;
                    batches[spare].prev = index;
                    split = 1;
                    batches[spare].next = batches[index].next;
                    if (batches[index].next >= 0) {
                        batches[batches[index].next].prev = spare;
                    }
                    batches[index].next = spare;
                    break;
                }
            }
        }
        if (split != 0) {
            *freeVtx -= numVtx;
        } else {
            *freeVtx -= batchVtx;
        }
        return batches[index].vertices;
    }
    return NULL;
}


/*
* PD name: vtxstore_free (likely)
* Description: Either deforming a vertex or frees a vertex (deallocation)
*/
void vtxstoreFree(Vertex* vertices) {
    s16* freeVtx;
    struct VtxstoreBatch* batches;
    s16 index;
    s32 stop;
    s32 other;

    if ((vertices >= g_VtxstoreChrVtx) &&
        (vertices <= g_VtxstoreChrVtx + (g_VtxstoreChrNumVtx - 1))) {
        batches = g_VtxstoreChrBatches;
        freeVtx = &g_VtxstoreChrFreeVtx;
    } else if ((vertices >= g_VtxstoreObjVtx) &&
               (vertices <= g_VtxstoreObjVtx + (g_VtxstoreObjNumVtx - 1))) {
        batches = g_VtxstoreObjBatches;
        freeVtx = &g_VtxstoreObjFreeVtx;
    } else {
        return;
    }

    index = 0;
    stop = 0;

    while (stop == 0) {
        if (batches[index].vertices == vertices) {
            stop = 1;
            batches[index].refs--;
            if (batches[index].refs == 0) {
                *freeVtx += batches[index].numVtx;

                // Merge with next free block
                other = batches[index].next;
                if (other >= 0) {
                    if (batches[other].refs == 0) {
                        batches[index].numVtx += batches[other].numVtx;
                        batches[index].next = batches[other].next;
                        batches[other].refs = -1;
                        other = batches[index].next;
                        if (other >= 0) {
                            batches[other].prev = index;
                        }
                    }
                }

                // Merge with prev free block
                other = batches[index].prev;
                if (other >= 0) {
                    if (batches[other].refs == 0) {
                        batches[other].numVtx += batches[index].numVtx;
                        batches[other].next = batches[index].next;
                        batches[index].refs = -1;
                        index = other;
                        other = batches[index].next;
                        if (other >= 0) {
                            batches[other].prev = index;
                        }
                    }
                }
            }
        } else {
            index = batches[index].next;
            if (index == -1) {
                stop = 1;
            }
        }
    }
}

