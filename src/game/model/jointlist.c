/**
 * Joint lists: the joints of every model in a scene, in one list sorted by depth.
 *
 * Each character keeps one (chr->field_20) for its body, head and hat. Characters are
 * drawn joint by joint from the back to the front (jointListDraw), and shots are tested
 * against the joints from the front to the back, so the first hit found is the nearest
 * (jointListFindHit).
 *
 * The entries come from a fixed pool of 600 on a free list. Callers build a list with
 * jointListAddModel, sort it with jointListCalcDepths and jointListSortByDepth, use it,
 * and give it back with jointListFree.
 */
#include <ultra64.h>
#include "game/model/model.h"
#include "game/model/jointlist.h"

#define JOINTLIST_LEN 600

JointListEntry g_JointListEntries[JOINTLIST_LEN];
JointListEntry *g_JointListFree = NULL;

/** Called on level load: threads all the entries into the free list. */
void jointListInit(void)
{
    s32 i;
    JointListEntry *entries = g_JointListEntries;

    g_JointListFree = entries;

    entries[0].next = &entries[1];

    for (i = 1; i < JOINTLIST_LEN - 1; i++)
    {
        entries[i].next = &entries[i + 1];
        entries[i].prev = &entries[i - 1];
    }

    g_JointListEntries[JOINTLIST_LEN - 1].prev = &g_JointListEntries[JOINTLIST_LEN - 2];
}

JointListEntry* jointListAddModel(JointListEntry* head, Model* context) {
    JointListEntry* freeListCursor;
    ModelNode* sceneCursor;
    ModelNode* childPtr;
    s32 nodeType;

    sceneCursor = context->obj->RootNode;
    freeListCursor = g_JointListFree;

    while ((sceneCursor != NULL) && (freeListCursor != NULL)) {
        nodeType = sceneCursor->Opcode & 0xFF;

        switch (nodeType) {
        case 1:
        case 2:
        case 3:
        case 0xb:
        case 0xc:
        case 0xd:
        case 0xe:
        case 0xf:
        case 0x10:
        case 0x15:
            freeListCursor->model = context;
            freeListCursor->rootnode = sceneCursor;
            freeListCursor = freeListCursor->next;
            break;

        default:
            break;
        }

        childPtr = sceneCursor->Child;
        if (childPtr != NULL) {
            sceneCursor = childPtr;
            continue;
        }

        while (sceneCursor != NULL) {
            childPtr = sceneCursor->Next;
            if (childPtr != NULL) {
                sceneCursor = childPtr;
                break;
            }
            sceneCursor = sceneCursor->Parent;
        }
    }

    if (freeListCursor != g_JointListFree) {
        if (head != NULL) {
            JointListEntry *tail = head;

            while (tail->next != NULL) {
                tail = tail->next;
            }

            tail->next = g_JointListFree;
            g_JointListFree->prev = tail;
        } else {
            head = g_JointListFree;
        }

        if (freeListCursor != NULL) {
            JointListEntry* prevNode = freeListCursor->prev;
            if (prevNode != NULL) {
                prevNode->next = NULL;
                freeListCursor->prev = NULL;
            }
        }

        g_JointListFree = freeListCursor;
    }

    return head;
}


void jointListFree(JointListEntry *entry)
{
    JointListEntry *oldhead;
    JointListEntry *tail;
    
    if (entry != NULL) {
        oldhead = g_JointListFree;
        if (oldhead != NULL) {
            tail = entry;
            while (tail->next != NULL) {
                tail = tail->next;
            }
            tail->next = oldhead;
            g_JointListFree->prev = tail;
        }
        g_JointListFree = entry;
    }
}


#define OP16_NODEINDEX_0C(data) (((ModelNode_Op16Record *)(data))->nodeindex0c)
#define OP16_NODEINDEX_0E(data) (((ModelNode_Op16Record *)(data))->nodeindex0e)
#define OP16_NODEINDEX_10(data) (((ModelNode_Op16Record *)(data))->nodeindex10)

#define OP16_POS_X_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[0])
#define OP16_POS_Y_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[1])
#define OP16_POS_Z_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[2])

void jointListCalcDepths(JointListEntry *entry)
{
    JointListEntry *first;
    JointListEntry *special;

    first = entry;
    special = NULL;

    while (entry != NULL) {
        ModelNode *node = entry->rootnode;
        u16 opcode = node->Opcode;

        switch (opcode & 0xFF)
        {
            case MODELNODE_OPCODE_HEADER:
            {
                union ModelRoData *data;
                ModelNode *othernode;
                Mtxf *mtx;

                data = node->Data;
                othernode = data->Header.FirstGroupNode;

                mtx = modelFindNodeMtx(entry->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(entry->model, othernode, 0);
                    entry->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    entry->sortvalue = -mtx->m[3][2];
                }
                break;
            }

            case MODELNODE_OPCODE_GROUP:
            {
                union ModelRoData *data;
                ModelNode *othernode;
                Mtxf *mtx;

                data = node->Data;
                othernode = data->Group.ChildGroupNode;

                mtx = modelFindNodeMtx(entry->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(entry->model, othernode, 0);
                    entry->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    entry->sortvalue = -mtx->m[3][2];
                }
                break;
            }

            case MODELNODE_OPCODE_OP03:
            {
                union ModelRoData *data;
                ModelNode *othernode;
                Mtxf *mtx;

                data = node->Data;
                othernode = data->Group.ChildGroupNode;

                mtx = modelFindNodeMtx(entry->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(entry->model, othernode, 0);
                    entry->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    entry->sortvalue = -mtx->m[3][2];
                }
                break;
            }

            case MODELNODE_OPCODE_GROUPSIMPLE:
            {
                Mtxf *mtx;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                entry->sortvalue = -mtx->m[3][2];
                break;
            }

            case MODELNODE_OPCODE_OP14:
            {
                Mtxf *mtx;
                f32 *data;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                data = (f32 *)node->Data;

                entry->sortvalue = -(data[0] * mtx->m[0][2]
                                + data[1] * mtx->m[1][2]
                                + data[2] * mtx->m[2][2]
                                + mtx->m[3][2]);
                break;
            }

            case MODELNODE_OPCODE_INTERLINK:
            {
                Mtxf *mtx;
                ModelRoData_InterlinkageRecord *interlinkage;
                f32 sortvalue1;
                f32 sortvalue2;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                interlinkage = &node->Data->Interlinkage;

                sortvalue1 = -(interlinkage->pos.x * mtx->m[0][2]
                            + interlinkage->pos.y * mtx->m[1][2]
                            + interlinkage->pos.z * mtx->m[2][2]
                            + mtx->m[3][2]);

                sortvalue2 = -(interlinkage->pos2.x * mtx->m[0][2]
                            + interlinkage->pos2.y * mtx->m[1][2]
                            + interlinkage->pos2.z * mtx->m[2][2]
                            + mtx->m[3][2]);

                if (sortvalue1 < sortvalue2)
                {
                    entry->sortvalue = sortvalue1;
                }
                else
                {
                    entry->sortvalue = sortvalue2;
                }
                break;
            }

            case MODELNODE_OPCODE_OP16:
            {
                Mtxf *mtx;
                f32 *data;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                data = (f32 *)node->Data;

                entry->sortvalue = -(data[0] * mtx->m[0][2]
                                + data[1] * mtx->m[1][2]
                                + data[2] * mtx->m[2][2]
                                + mtx->m[3][2]);

                special = entry;
                break;
            }

            case MODELNODE_OPCODE_OP11:
            {
                Mtxf *mtx;
                ModelRoData_Op11Record *op11;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                op11 = &node->Data->Op11;

                entry->sortvalue = -(op11->pos.x * mtx->m[0][2]
                                + op11->pos.y * mtx->m[1][2]
                                + op11->pos.z * mtx->m[2][2]
                                + mtx->m[3][2]);
                break;
            }

            case MODELNODE_OPCODE_GUNFIRE:
            {
                Mtxf *mtx;
                f32 *data;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                data = (f32 *)node->Data;

                entry->sortvalue = -(data[0] * mtx->m[0][2]
                                + data[1] * mtx->m[1][2]
                                + data[2] * mtx->m[2][2]
                                + mtx->m[3][2]);
                break;
            }

            case MODELNODE_OPCODE_SHADOW:
            {
                Mtxf *mtx;
                ModelRoData_ShadowRecord *shadow;
                union ModelRwData *rwdata;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                shadow = &node->Data->Shadow;

                rwdata = modelGetNodeRwData(entry->model, shadow->HeaderNode);

                entry->sortvalue = -(shadow->pos.x * mtx->m[0][2]
                                + (rwdata->Header.ground - rwdata->Header.pos.y) * mtx->m[1][2]
                                + shadow->pos.y * mtx->m[2][2]
                                + mtx->m[3][2]);
                break;
            }
        }

        entry = entry->next;
    }

    if (special != NULL)
    {
        Model *model = special->model;

        if (model->attachedto != NULL)
        {
            ModelNode **switches;
            ModelNode *node0e;
            ModelNode *specialnode;
            void *op16data;
            ModelNode *node0c;
            ModelNode *node10;
            JointListEntry *entry0e;
            JointListEntry *entry0c;
            JointListEntry *entry10;
            Mtxf *mtx;
            f32 axis2dot;
            f32 axis1dot;
            f32 sort0e;
            coord3d axis2;
            coord3d axis1;
            f32 sort0c;
            f32 sort10;
            coord3d transformed;
            JointListEntry *scan;
            f32 tempf;

            tempf = special->sortvalue;
            specialnode = special->rootnode;
            op16data = specialnode->Data;

            switches = model->attachedto->obj->Switches;

            node0e = switches[OP16_NODEINDEX_0E(op16data)];
            node0c = switches[OP16_NODEINDEX_0C(op16data)];
            node10 = switches[OP16_NODEINDEX_10(op16data)];

            entry10 = NULL;
            entry0e = NULL;
            entry0c = NULL;


            mtx = modelFindNodeMtx(model, specialnode, 0);
            scan = first;

            while (scan != NULL)
            {
                if (node0e == scan->rootnode)
                {
                    entry0e = scan;
                }

                if (node0c == scan->rootnode)
                {
                    entry0c = scan;
                }

                if (node10 == scan->rootnode)
                {
                    entry10 = scan;
                }

                scan = scan->next;
            }

            axis2.f[0] = mtx->m[2][0];
            axis2.f[1] = mtx->m[2][1];
            axis2.f[2] = mtx->m[2][2];

            axis1.f[0] = mtx->m[1][0];
            axis1.f[1] = mtx->m[1][1];
            axis1.f[2] = mtx->m[1][2];

            transformed.f[0] = OP16_POS_X_VOL(op16data) * mtx->m[0][0]
                          + OP16_POS_Y_VOL(op16data) * mtx->m[1][0]
                          + OP16_POS_Z_VOL(op16data) * mtx->m[2][0]
                          + mtx->m[3][0];
            
            transformed.f[1] = OP16_POS_X_VOL(op16data) * mtx->m[0][1]
                          + OP16_POS_Y_VOL(op16data) * mtx->m[1][1]
                          + OP16_POS_Z_VOL(op16data) * mtx->m[2][1]
                          + mtx->m[3][1];
            
            transformed.f[2] = OP16_POS_X_VOL(op16data) * mtx->m[0][2]
                          + OP16_POS_Y_VOL(op16data) * mtx->m[1][2]
                          + OP16_POS_Z_VOL(op16data) * mtx->m[2][2]
                          + mtx->m[3][2];

            axis2dot = axis2.f[0] * transformed.f[0]
                     + axis2.f[1] * transformed.f[1]
                     + axis2.f[2] * transformed.f[2];
            axis1dot = axis1.f[0] * transformed.f[0]
                     + axis1.f[1] * transformed.f[1]
                     + axis1.f[2] * transformed.f[2];
            
            
            sort0e = entry0e->sortvalue;
            sort0c = entry0c->sortvalue;
            sort10 = entry10->sortvalue;
            if (axis2dot < 0.0f)
            {
                if (sort10 < sort0e)
                {
                    if (sort10 < tempf)
                    {
                        special->sortvalue = sort10 - 0.000030517578125f; // 2/65536
                    }
                }
                else
                {
                    if (sort0e < tempf)
                    {
                        special->sortvalue = sort0e - 0.000030517578125f; // 2/65536
                    }
                }
            }
            else if (0.0f <= axis2dot)
            {
                if (tempf < sort0e)
                {
                    entry0e->sortvalue = tempf - 0.00006103515625f; // 4/65536
                }

                if (tempf < sort10)
                {
                    if (sort10 < sort0e)
                    {
                        entry10->sortvalue = tempf - 0.000091552734375f; // 6/65536
                    }
                    else
                    {
                        entry10->sortvalue = tempf - 0.000030517578125f; // 2/65536
                    }
                }
            }

            if (axis1dot < 0.0f)
            {
                if (sort0c < special->sortvalue)
                {
                    if (entry0e->sortvalue < special->sortvalue && sort0c < entry0e->sortvalue)
                    {
                        entry0e->sortvalue = sort0c - 0.00006103515625f; // 4/65536
                    }

                    if (entry10->sortvalue < special->sortvalue && sort0c < entry10->sortvalue)
                    {
                        if (sort10 < sort0e)
                        {
                            entry10->sortvalue = sort0c - 0.000091552734375f; // 6/65536
                        }
                        else
                        {
                            entry10->sortvalue = sort0c - 0.000030517578125f; // 2/65536
                        }
                    }

                    special->sortvalue = sort0c - 0.0000152587890625f; // 1/65536
                }
            }
            else if (0.0f <= axis1dot)
            {
                if (special->sortvalue < sort0c)
                {
                    entry0c->sortvalue = special->sortvalue - 0.0000152587890625f; // 1/65536

                    if (sort0c < sort0e)
                    {
                        if (entry0e->sortvalue < entry0c->sortvalue)
                        {
                            entry0c->sortvalue = entry0e->sortvalue - 0.0000152587890625f; // 1/65536
                        }
                    }

                    if (sort0c < sort10)
                    {
                        if (entry10->sortvalue < entry0c->sortvalue)
                        {
                            entry0c->sortvalue = entry10->sortvalue - 0.0000152587890625f; // 1/65536
                        }
                    }
                }
            }
        }
    }
}

#undef OP16_NODEINDEX_0C
#undef OP16_NODEINDEX_0E
#undef OP16_NODEINDEX_10
#undef OP16_POS_X_VOL
#undef OP16_POS_Y_VOL
#undef OP16_POS_Z_VOL


JointListEntry *jointListSortByDepth(JointListEntry *modelhit)
{
    JointListEntry stacknodes[2];
    JointListEntry *last;
    JointListEntry *current;
    JointListEntry *next;
    JointListEntry *scan;
    JointListEntry *best;
    f32 bestvalue;

    if (modelhit != NULL)
    {
        last = modelhit;

        if (last->next != NULL)
        {
            do
            {
                last = last->next;

            }
            while (last->next != NULL);
        }

        stacknodes[1].next = modelhit;
        modelhit->prev = &stacknodes[1];

        stacknodes[0].prev = last;
        last->next = &stacknodes[0];

        current = &stacknodes[1];

        do
        {
            next = current->next;
            best = NULL;
            bestvalue = -M_U32_MAX_VALUE_F;

            if (next != &stacknodes[0])
            {
                scan = next;

                do
                {
                    if (bestvalue < scan->sortvalue)
                    {
                        bestvalue = scan->sortvalue;
                        best = scan;
                    }

                    scan = scan->next;
                }
                while (scan != &stacknodes[0]);
            }

            if (best != NULL)
            {
                best->next->prev = best->prev;
                best->prev->next = best->next;

                best->prev = current;
                best->next = current->next;

                current->next->prev = best;
                current->next = best;

                next = best;
            }

            current = next;
        }
        while (next != &stacknodes[0]);

        modelhit = stacknodes[1].next;
        stacknodes[1].next->prev = NULL;
        stacknodes[0].prev->next = NULL;
    }

    return modelhit;
}


#if defined(VERSION_US)
void jointListDraw(ModelRenderData *data, JointListEntry *entry)
{
    ModelNode *root;
    ModelNode *node;
    s32 descend;
    Gfx *gdl;
    s32 opcode;

    while (entry != NULL)
    {
        root = entry->rootnode;
        node = root;

        if (data->skipDraw != 0)
        {
            if (entry->sortvalue < modelGetNodeRadius(entry->model, root))
            {
                node = NULL;
            }
        }
        else
        {
            gdl = data->gdl++;
            gSPSegment(gdl, SPSEGMENT_MODEL_MTX, osVirtualToPhysical(entry->model->render_pos));
        }

        if (node != NULL)
        {
            do
            {
                descend = 1;
                opcode = node->Opcode & 0xff;

                switch (opcode)
                {
                case MODELNODE_OPCODE_HEADER:
                case MODELNODE_OPCODE_GROUP:
                case MODELNODE_OPCODE_OP03:
                case MODELNODE_OPCODE_OP11:
                case MODELNODE_OPCODE_GUNFIRE:
                case MODELNODE_OPCODE_SHADOW:
                case MODELNODE_OPCODE_OP14:
                case MODELNODE_OPCODE_INTERLINK:
                case MODELNODE_OPCODE_OP16:
                case MODELNODE_OPCODE_GROUPSIMPLE:
                    if (node == root)
                    {
                        modelRenderNode(data, entry->model, node);
                    }
                    else
                    {
                        descend = 0;
                    }
                    break;

                case MODELNODE_OPCODE_DL:
                case MODELNODE_OPCODE_OP05:
                case MODELNODE_OPCODE_OP06:
                case MODELNODE_OPCODE_TUBE:
                case MODELNODE_OPCODE_LOD:
                case MODELNODE_OPCODE_BSP:
                case MODELNODE_OPCODE_BBOX:
                case MODELNODE_OPCODE_OP17:
                case MODELNODE_OPCODE_SWITCH:
                case MODELNODE_OPCODE_OP19:
                case MODELNODE_OPCODE_OP20:
                case MODELNODE_OPCODE_DLPRIMARY:
                case MODELNODE_OPCODE_HEAD:
                case MODELNODE_OPCODE_DLCOLLISION:
                default:
                    modelRenderNode(data, entry->model, node);
                    break;
                }

                if (descend && node->Child != NULL)
                {
                    node = node->Child;
                }
                else if (node != NULL)
                {
                    do
                    {
                        if (node == root)
                        {
                            node = NULL;
                            break;
                        }

                        if (node->Next != NULL)
                        {
                            node = node->Next;
                            break;
                        }

                        node = node->Parent;
                    }
                    while (node != NULL);
                }
            }
            while (node != NULL);
        }

        entry = entry->next;
    }
}
#endif


#if defined(VERSION_EU)
void jointListDraw(ModelRenderData *data, JointListEntry *entry)
{
    ModelNode *root;
    ModelNode *node;
    s32 descend;
    Gfx *gdl;
    s32 opcode;

    while (entry != NULL)
    {
        root = entry->rootnode;
        node = root;

        if (data->skipDraw != 0)
        {
            if (entry->sortvalue < modelGetNodeRadius(entry->model, root))
            {
                node = NULL;
            }
        }
        else
        {
            gdl = data->gdl++;
            gSPSegment(gdl, SPSEGMENT_MODEL_MTX, osVirtualToPhysical(entry->model->render_pos));
        }

        if (node != NULL)
        {
            do
            {
                descend = 1;
                opcode = node->Opcode & 0xff;

                switch (opcode)
                {
                case MODELNODE_OPCODE_HEADER:
                case MODELNODE_OPCODE_GROUP:
                case MODELNODE_OPCODE_OP03:
                case MODELNODE_OPCODE_OP11:
                case MODELNODE_OPCODE_GUNFIRE:
                case MODELNODE_OPCODE_SHADOW:
                case MODELNODE_OPCODE_OP14:
                case MODELNODE_OPCODE_INTERLINK:
                case MODELNODE_OPCODE_OP16:
                case MODELNODE_OPCODE_GROUPSIMPLE:
                    if (node == root)
                    {
                        modelRenderNode(data, entry->model, node);
                    }
                    else
                    {
                        descend = 0;
                    }
                    break;

                case MODELNODE_OPCODE_DL:
                case MODELNODE_OPCODE_OP05:
                case MODELNODE_OPCODE_OP06:
                case MODELNODE_OPCODE_TUBE:
                case MODELNODE_OPCODE_LOD:
                case MODELNODE_OPCODE_BSP:
                case MODELNODE_OPCODE_BBOX:
                case MODELNODE_OPCODE_OP17:
                case MODELNODE_OPCODE_SWITCH:
                case MODELNODE_OPCODE_OP19:
                case MODELNODE_OPCODE_OP20:
                case MODELNODE_OPCODE_DLPRIMARY:
                case MODELNODE_OPCODE_HEAD:
                case MODELNODE_OPCODE_DLCOLLISION:
                default:
                    modelRenderNode(data, entry->model, node);
                    break;
                }

                if (descend && node->Child != NULL)
                {
                    node = node->Child;
                }
                else if (node != NULL)
                {
                    do
                    {
                        if (node == root)
                        {
                            node = NULL;
                            break;
                        }

                        if (node->Next != NULL)
                        {
                            node = node->Next;
                            break;
                        }

                        node = node->Parent;
                    }
                    while (node != NULL);
                }
            }
            while (node != NULL);
        }

        entry = entry->next;
    }
}
#endif


s32 jointListFindHitNext(JointListEntry **entryptr, coord3d *raypos, coord3d *raydir, Model **outModel, ModelNode **inoutNode)
{
    JointListEntry *entry = *entryptr;
    ModelNode *node;
    s32 descend;
    ModelNode *root;
    ModelNode *next;

    while (entry != NULL) {
        ModelNode *resume = *inoutNode;

        root = entry->rootnode;
        descend = TRUE;

        if (resume != NULL) {
            node = resume;
            *inoutNode = NULL;
        } else {
            node = root;
        }

        while (node != NULL) {
            if (descend && node->Child != NULL) {
                node = node->Child;
            } else {
                if (node != NULL) {
walk_node:
                    if (node == root) {
                        node = NULL;
                    } else {
                        next = node->Next;

                        if (next != NULL) {
                            node = next;
                        } else {
                            node = node->Parent;

                            if (node != NULL) {
                                goto walk_node;
                            }
                        }
                    }
                }

                if (node == NULL) {
                    break;
                }
            }

            descend = TRUE;
            
            {
                u16 opcode = node->Opcode;
            
                descend = TRUE;
            
                switch (opcode & 0xff) {
                case MODELNODE_OPCODE_HEADER:
                case MODELNODE_OPCODE_GROUP:
                case MODELNODE_OPCODE_OP03:
                case MODELNODE_OPCODE_GROUPSIMPLE:
                    descend = FALSE;
                    break;
            
                case MODELNODE_OPCODE_OP11:
                case MODELNODE_OPCODE_GUNFIRE:
                case MODELNODE_OPCODE_SHADOW:
                case MODELNODE_OPCODE_OP14:
                case MODELNODE_OPCODE_INTERLINK:
                case MODELNODE_OPCODE_OP16:
                    descend = FALSE;
                    break;
            
                case MODELNODE_OPCODE_BBOX:
                    if (modelTestRayIntersectsNodeBBox(entry->model, node, raypos, raydir)) {
                        *outModel = entry->model;
                        *inoutNode = node;
                        *entryptr = entry;
            
                        return *(s32 *)node->Data;
                    }
            
                    descend = FALSE;
                    break;
            
                case MODELNODE_OPCODE_OP17:
                    if (modelTestRayIntersectsNodeOp17(entry->model, node, raypos, raydir)) {
                        *outModel = entry->model;
                        *inoutNode = node;
                        *entryptr = entry;
            
                        return *(s32 *)node->Data;
                    }
            
                    descend = FALSE;
                    break;
            
                case MODELNODE_OPCODE_LOD:
                    modelApplyDistanceRelations(entry->model, node);
                    break;
            
                case MODELNODE_OPCODE_SWITCH:
                    modelApplyToggleRelations(entry->model, node);
                    break;
            
                case MODELNODE_OPCODE_HEAD:
                    modelApplyHeadRelations(entry->model, node);
                    break;
            
                case MODELNODE_OPCODE_DLCOLLISION:
                default:
                    break;
                }
            }
        }

        entry = entry->prev;
    }

    *entryptr = NULL;
    return 0;
}


/**
 * Starts a ray hit search: moves *entryptr to the last joint list entry, clears the
 * outputs, then lets jointListFindHitNext walk the list backwards.
 */
s32 jointListFindHit(JointListEntry **entryptr, coord3d *modelRayStart, coord3d *modelRayDir, Model **outModel, ModelNode **outNode)
{
    JointListEntry *entry = *entryptr;

    while (entry->next != NULL) 
    {
        entry = entry->next;
    }

    *entryptr = entry;
    *outModel = NULL;
    *outNode = NULL;

    return jointListFindHitNext( entryptr, modelRayStart, modelRayDir, outModel, outNode);
}

