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

/** A run of a store's vertices: in use (refs > 0), free (0) or an unused entry (-1) */
struct VtxstoreBatch
{
    Vertex *vertices;
    ModelFileHeader *modeldef; /* VTXSTORE_OBJ: the model and damage level, for sharing */
    s32 destroyedLevel;
    s16 numVtx;
    s16 refs;
    s16 next; /* the batches in address order, -1 at the ends */
    s16 prev;
};

typedef struct
{
    struct VtxstoreBatch *batches; /* batches[0] always starts the list */
    Vertex *vertices;
    s32 numBatches;
    s32 numVtx;
    s32 freeVtx;
} VtxStore;

static VtxStore s_ChrStore;
static VtxStore s_ObjStore;

static void initStore(VtxStore *store, s32 numVtx, s32 numBatches)
{
    store->numVtx = numVtx;
    store->numBatches = numBatches;
    store->batches = mempAlloc(numBatches * sizeof(struct VtxstoreBatch), MEMPOOL_STAGE);
    store->vertices = mempAlloc(numVtx * sizeof(Vertex), MEMPOOL_STAGE);
    store->freeVtx = numVtx;

    // One free batch of everything, and the rest unused
    store->batches[0].vertices = store->vertices;
    store->batches[0].numVtx = numVtx;
    store->batches[0].refs = 0;
    store->batches[0].next = -1;
    store->batches[0].prev = -1;

    for (s32 i = 1; i < numBatches; i++)
    {
        store->batches[i].refs = -1;
    }
}

/** Makes the stores for a level */
void vtxstoreReset(void)
{
    s32 stage = lvGetCurrentStage();

    if (getPlayerCount() >= 2)
    {
        initStore(&s_ChrStore, 3000, 80);
        initStore(&s_ObjStore, 500, 20);
    }
    else if (stage == LEVELID_STREETS || stage == LEVELID_DEPOT)
    {
        initStore(&s_ChrStore, 500, 20);
        initStore(&s_ObjStore, 500, 20);
    }
    else
    {
        initStore(&s_ChrStore, 500, 20);
        initStore(&s_ObjStore, 1500, 40);
    }
}

/** Points every prop that uses the vertices at `find` at `replacement` instead */
static void vtxstoreFixRefs(Vertex *find, Vertex *replacement)
{
    for (PropRecord *prop = propsGetActiveTail(); prop != NULL; prop = prop->prev)
    {
        Model *model;
        ModelNode *node;

        if (prop->type != PROP_TYPE_OBJ)
        {
            continue;
        }

        model = prop->obj->model;
        node = model->obj->RootNode;

        // Find the model's first collision display list
        while (node != NULL)
        {
            if ((node->Opcode & 0xff) == MODELNODE_OPCODE_DLCOLLISION)
            {
                union ModelRwData *rwdata = modelGetNodeRwData(model, node);

                if (rwdata->DisplayListCollisions.Vertices == find)
                {
                    rwdata->DisplayListCollisions.Vertices = replacement;
                }
                break;
            }

            if (node->Child != NULL)
            {
                node = node->Child;
                continue;
            }

            while (node != NULL)
            {
                if (node->Next != NULL)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}

/** Merges free batches that are next to each other */
static void mergeFreeBatches(VtxStore *store)
{
    struct VtxstoreBatch *batches = store->batches;
    s32 i = 0;

    while (batches[i].next >= 0)
    {
        s32 next = batches[i].next;

        if (batches[i].refs == 0 && batches[next].refs == 0)
        {
            batches[i].numVtx += batches[next].numVtx;
            batches[next].refs = -1;
            batches[i].next = batches[next].next;

            if (batches[i].next >= 0)
            {
                batches[batches[i].next].prev = i;
            }
        }
        else
        {
            i = next;
        }
    }
}

/**
 * Called each frame. When the prop store runs low (under a quarter free), props with the
 * same model and damage share one copy of their vertices, and if it's still low, damaged
 * props that are out of sight give theirs up (objsFreeDeformedVertices).
 */
void vtxstoreTick(void)
{
    VtxStore *store = &s_ObjStore;
    struct VtxstoreBatch *batches = store->batches;
    bool merged = false;

    if (store->freeVtx < store->numVtx >> 2)
    {
        for (s32 i = 0; i < store->numBatches - 1; i++)
        {
            if (batches[i].refs <= 0)
            {
                continue;
            }

            for (s32 j = i + 1; j < store->numBatches; j++)
            {
                if (batches[j].refs > 0 && batches[i].modeldef == batches[j].modeldef
                    && batches[i].destroyedLevel == batches[j].destroyedLevel)
                {
                    vtxstoreFixRefs(batches[j].vertices, batches[i].vertices);
                    merged = true;

                    batches[i].refs += batches[j].refs;
                    batches[j].refs = 0;
                    store->freeVtx += batches[j].numVtx;
                }
            }
        }
    }

    if (merged)
    {
        mergeFreeBatches(store);
    }

    if (store->freeVtx < store->numVtx >> 2)
    {
        objsFreeDeformedVertices();
    }
}

/**
 * Takes `numVtx` vertices from a store (VTXSTORE_*). For VTXSTORE_OBJ, `modeldef` and
 * `destroyedLevel` say what they're for, so vtxstoreTick can share them. Returns NULL if
 * there's no free run big enough.
 */
Vertex *vtxstoreAllocate(s32 numVtx, s32 type, ModelFileHeader *modeldef, s32 destroyedLevel)
{
    VtxStore *store;
    struct VtxstoreBatch *batches;
    struct VtxstoreBatch *batch;
    s32 index = 0;
    s32 steps = 0;
    s32 batchVtx;

    switch (type)
    {
        case VTXSTORE_CHR:
            store = &s_ChrStore;
            break;
        case VTXSTORE_OBJ:
            store = &s_ObjStore;
            break;
        default:
            return NULL;
    }

    batches = store->batches;

    // The first free batch that's big enough (giving up if the list loops)
    while (batches[index].refs != 0 || batches[index].numVtx < numVtx)
    {
        index = batches[index].next;
        steps++;

        if (index == -1 || steps > store->numBatches)
        {
            return NULL;
        }
    }

    batch = &batches[index];
    batchVtx = batch->numVtx;
    batch->modeldef = modeldef;
    batch->destroyedLevel = destroyedLevel;
    batch->refs++;

    if (batchVtx != numVtx)
    {
        // Split the rest off into an unused entry, if there is one. If there isn't, this
        // takes the whole batch.
        for (s32 spare = 0; spare < store->numBatches; spare++)
        {
            if (batches[spare].refs == -1)
            {
                batch->numVtx = numVtx;
                batches[spare].vertices = batch->vertices + numVtx;
                batches[spare].numVtx = batchVtx - numVtx;
                batches[spare].refs = 0;
                batches[spare].prev = index;
                batches[spare].next = batch->next;

                if (batch->next >= 0)
                {
                    batches[batch->next].prev = spare;
                }

                batch->next = spare;
                break;
            }
        }
    }

    store->freeVtx -= batch->numVtx;

    return batch->vertices;
}

/** Gives back vertices from vtxstoreAllocate. They're freed when the last user gives them back. */
void vtxstoreFree(Vertex *vertices)
{
    VtxStore *store;
    struct VtxstoreBatch *batches;
    s32 index = 0;

    if (vertices >= s_ChrStore.vertices && vertices < s_ChrStore.vertices + s_ChrStore.numVtx)
    {
        store = &s_ChrStore;
    }
    else if (vertices >= s_ObjStore.vertices && vertices < s_ObjStore.vertices + s_ObjStore.numVtx)
    {
        store = &s_ObjStore;
    }
    else
    {
        return;
    }

    batches = store->batches;

    while (batches[index].vertices != vertices)
    {
        index = batches[index].next;

        if (index == -1)
        {
            return;
        }
    }

    batches[index].refs--;

    if (batches[index].refs != 0)
    {
        return;
    }

    store->freeVtx += batches[index].numVtx;

    // Merge with the next batch if it's free, then with the previous one
    s32 other = batches[index].next;

    if (other >= 0 && batches[other].refs == 0)
    {
        batches[index].numVtx += batches[other].numVtx;
        batches[index].next = batches[other].next;
        batches[other].refs = -1;

        if (batches[index].next >= 0)
        {
            batches[batches[index].next].prev = index;
        }
    }

    other = batches[index].prev;

    if (other >= 0 && batches[other].refs == 0)
    {
        batches[other].numVtx += batches[index].numVtx;
        batches[other].next = batches[index].next;
        batches[index].refs = -1;

        if (batches[other].next >= 0)
        {
            batches[batches[other].next].prev = other;
        }
    }
}
