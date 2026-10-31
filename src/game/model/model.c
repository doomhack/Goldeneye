/**
 * Models: instances of model files, their matrices, animation and drawing.
 *
 * A model file (ModelFileHeader) is a tree of nodes. Some nodes position things (the root
 * header, joints), some choose what is shown (level of detail, switches, the head slot),
 * and some draw (display lists, gunfire, shadows). A Model is one instance of a file: its
 * scale, its animation state, and per-node runtime data (rwdata).
 *
 * Each frame the owner of a model calls:
 *   modelCalcPosition  - moves the root by the animation's root motion
 *   modelCalcMatrices  - works out every joint's matrix from the animation
 *   modelRender        - walks the tree and draws it
 *
 * See docs/subsystems/models.md.
 */
#include <ultra64.h>
#include <memp.h>
#include "game/model/model.h"
#include "rmon.h"
#include "game/player/player.h"
#include "game/chr/chr.h"
#include "game/model/chrobjdata.h"
#include "gbi_extension.h"
#include "game/model/model.h"
#include "game/math/trig.h"
#include "game/math/round.h"
#include "game/model/model.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/math/quaternion.h"
#include "random.h"
#include "game/gfx/texcmds.h"
#include "rom.h"


// forward declarations
void modelSetAnimFrame2WithChrStuff(struct Model *model, f32 framea, f32 frameb, f32 frame2a, f32 frame2b);


Model *g_AnimModelSlots;
ModelBase *g_ModelSlots;


s32 g_MaxAnimModelSlots = 0;
s32 g_MaxModelSlots = 0;
s32 g_ModelIsLvResetting = 0;
s32 g_ModelLodDisabled = 0;
f32 g_ModelLodScale = 1.0;
struct Vertex* (*g_ModelVtxAllocator)(s32 numvertices) = NULL;
void (*g_ModelJointPositionedFunc)(s32 mtxindex, Mtxf *mtx) = NULL;
struct TubeAngleRow g_ModelTubeAngleTable[32] = {
    { 0, 0, 0x10 },
    { 1, 0x1000, 0xD },
    { 1, 0x1000, 0xD },
    { 9, 0x800, 0xC },
    { 9, 0x800, 0xC },
    { 0x19, 0x400, 0xB },
    { 0x19, 0x400, 0xB },
    { 0x39, 0x400, 0xB },
    { 0x39, 0x400, 0xB },
    { 0x59, 0x400, 0xB },
    { 0x59, 0x400, 0xB },
    { 0x79, 0x400, 0xB },
    { 0x79, 0x400, 0xB },
    { 0x99, 0x400, 0xB },
    { 0x99, 0x400, 0xB, },
    { 0xB9, 0x400, 0xB, },
    { 0xB9, 0x400, 0xB, },
    { 0xD9, 0x400, 0xB, },
    { 0xD9, 0x400, 0xB, },
    { 0xF9, 0x400, 0xB, },
    { 0xF9, 0x400, 0xB, },
    { 0x119, 0x400, 0xB, },
    { 0x119, 0x400, 0xB, },
    { 0x139, 0x400, 0xB, },
    { 0x139, 0x400, 0xB, },
    { 0x159, 0x400, 0xB, },
    { 0x159, 0x400, 0xB, },
    { 0x179, 0x800, 0xC, },
    { 0x179, 0x800, 0xC, },
    { 0x189, 0x1000, 0xD },
    { 0x189, 0x1000, 0xD },
    { 0x191, 0, 0x10 },
};

u32 g_ModelAnimMergingEnabled = 1;

/* Template for the vertices of muzzle flashes: opaque white */
Vertex g_ModelGunfireVtxTemplate = {
    { {0, 0, 0} },
    0,
    { {0, 0} },
    {{0xFF,
    0xFF,
    0xFF,
    0xFF}}
};

u32 g_ModelShadowAlpha = 0x50;
/* Template for the vertices of shadows: alpha is replaced from g_ModelShadowAlpha */
Vtx g_ModelShadowVtxTemplate = {
    {
        { 0, 0, 0 },
        0,
        { 0, 0 },
        { 0xFF, 0xFF, 0xFF, 0x50 }
    }
};
struct bondstruct_unk_animation_related* g_AnimFrameBuffer =  0;


#define MODEL_SPARE_SLOTS          30
#define ANIM_MODEL_SPARE_SLOTS     10

#define MODEL_SPARE_RWDATALEN      0x14
#define ANIM_MODEL_SPARE_RWDATALEN 0x8c


void modelmgrResetSlotCounts(void)
{
    g_MaxAnimModelSlots = 0;
    g_MaxModelSlots = 0;
}


void modelmgrSetLevelResetting(bool resetting) 
{
    g_ModelIsLvResetting = resetting;
}

//this may be a file split

/**
*/
void modelmgrAllocateModelSlots(s32 numobjs)
{
    s32 i;

    g_MaxModelSlots = numobjs + MODEL_SPARE_SLOTS;
    
    g_ModelSlots = mempAlloc(g_MaxModelSlots * sizeof(ModelBase), MEMPOOL_STAGE);

    for (i = 0; i < g_MaxModelSlots; i++)
    {
        g_ModelSlots[i].obj = 0;

        if (i < numobjs)
        {
            g_ModelSlots[i].datas = NULL;
        }
        else
        {
            g_ModelSlots[i].datas = mempAlloc(MODEL_SPARE_RWDATALEN * sizeof(u32), MEMPOOL_STAGE);
            g_ModelSlots[i].rwdatalen = MODEL_SPARE_RWDATALEN;
        }
    }
}


/**
*/
void modelmgrAllocateAnimModelSlots(s32 numanimated)
{
    s32 i;

    g_MaxAnimModelSlots = numanimated + ANIM_MODEL_SPARE_SLOTS;

    g_AnimModelSlots = mempAlloc(g_MaxAnimModelSlots * sizeof(Model), MEMPOOL_STAGE);

    for (i = 0; i < g_MaxAnimModelSlots; i++)
    {
        g_AnimModelSlots[i].obj = 0;

        if (i < numanimated)
        {
            g_AnimModelSlots[i].datas = NULL;
        }
        else
        {
            g_AnimModelSlots[i].datas = mempAlloc(ANIM_MODEL_SPARE_RWDATALEN * sizeof(u32), MEMPOOL_STAGE);
            g_AnimModelSlots[i].rwdatalen = ANIM_MODEL_SPARE_RWDATALEN;
        }
    }
}


//newfile per EU
bool modelmgrCanSlotFitRwdata(Model *modelslot, ModelFileHeader *modeldef)
{
    return modeldef->numRecords <= 0
        || (modelslot->datas != NULL && modelslot->rwdatalen >= modeldef->numRecords);
}


/**
 * Takes a free slot for a model without animation (just a ModelBase).
 * Models that need animations use modelmgrInstantiateModelWithAnim.
 */
Model *modelmgrInstantiateModel(ModelFileHeader *header)
{
    Model *model;
    u32 *rwdata;
    s16 rwdatalen;

    model = NULL;
    rwdata = NULL;
    rwdatalen = -1;

    if (g_ModelIsLvResetting) 
    {
        s32 i;

        for (i = 0; i < (g_MaxModelSlots - 30); i++) 
        {
            if (g_ModelSlots[i].obj == 0) 
            {
                model = (Model *)&g_ModelSlots[i];
                break;
            }
        }

        if (model == NULL) 
        {
            model = mempAlloc(sizeof(ModelBase), MEMPOOL_STAGE);
        }

        if (header->numRecords > 0) 
        {
            rwdata = mempAlloc((((header->numRecords * 4) + 0xf) | 0xf) ^ 0xf, MEMPOOL_STAGE);
            rwdatalen = header->numRecords;
        }
    } 
    else 
    {
        s32 i;

        for (i = 0; i < g_MaxModelSlots; i++) 
        {
            if (g_ModelSlots[i].obj == 0 && modelmgrCanSlotFitRwdata((Model *)&g_ModelSlots[i], header)) 
            {
                rwdata = g_ModelSlots[i].datas;
                rwdatalen = g_ModelSlots[i].rwdatalen;
                model = (Model *)&g_ModelSlots[i];
                break;
            }
        }
    }

    if (model != NULL) 
    {
        modelInit(model, header, rwdata);
        model->rwdatalen = rwdatalen;
    }

    return model;
}


void modelFreeSlot(Model* model)
{
    model->obj = NULL;
}


/**
 * Takes a free slot for a model with animation.
 */
Model *modelmgrInstantiateModelWithAnim(ModelFileHeader *modelFileHeader)
{
    Model *newModel;
    void *rwdatas;
    s16 rwdatalen;
    s32 i;
    s16 requiredRwdatalen;
    s32 i2;

    newModel = NULL;
    rwdatas = NULL;
    rwdatalen = -1;

    if (g_ModelIsLvResetting) 
    {
        for (i = 0; i < (g_MaxAnimModelSlots - 10); i++) 
        {
            if (g_AnimModelSlots[i].obj == 0)
            {
                newModel = &g_AnimModelSlots[i];
                break;
            }
        }

        if (newModel == NULL) 
        {
            newModel = mempAlloc(sizeof(Model), MEMPOOL_STAGE);
        }

        requiredRwdatalen = modelFileHeader->numRecords;


        if (requiredRwdatalen > 0) 
        {
            i = requiredRwdatalen;
            rwdatas = mempAlloc((((i * 4) + 0xf) | 0xf) ^ 0xf, MEMPOOL_STAGE);
            rwdatalen = modelFileHeader->numRecords;
        }
    } 
    else 
    {
        requiredRwdatalen = modelFileHeader->numRecords;

        for (i2 = 0; i2 < g_MaxAnimModelSlots; i2++) 
        {
            if ((g_AnimModelSlots[i2].obj == 0) && ((requiredRwdatalen <= 0) || ((g_AnimModelSlots[i2].datas != NULL) &&(g_AnimModelSlots[i2].rwdatalen >= requiredRwdatalen)))) 
            {
                newModel = &g_AnimModelSlots[i2];
                rwdatas = g_AnimModelSlots[i2].datas;
                rwdatalen = g_AnimModelSlots[i2].rwdatalen;
                break;
            }
        }
    }

    if (newModel != NULL) 
    {
        animInit(newModel, modelFileHeader, rwdatas);
        newModel->rwdatalen = rwdatalen;
    }

    return newModel;
}


void modelAttachHead(Model *model, ModelNode *node,  ModelFileHeader *head)
{
    modelAttachPart(model,model->obj,node,head);

    modelInitRwData(model,head->RootNode);
}


void modelFreeAnimSlot(Model *objinstance)
{
    objinstance->obj = NULL;
    return;
}


void modelSetLodDisabled(s32 disabled) {
  g_ModelLodDisabled = disabled;
}


// PD: modelSetLodScale
void modelSetLodScale(f32 scale) {
  g_ModelLodScale = scale;
}


void modelSetVtxAllocator(Vertex *(*allocator)(s32 numvertices)) {
  g_ModelVtxAllocator = allocator;
}


void modelCalcDirToCamera(Model* model, coord3d* coord)
{
    Mtxf* mtx;
    f32 dist;
    f32 neg_x;
    f32 neg_y;
    f32 neg_z;
    f32 inv_dist;

    mtx = modelGetRootMtx(model);
    neg_x = -mtx->m[3][0];
    neg_y = -mtx->m[3][1];
    neg_z = -mtx->m[3][2];

    dist = sqrtf((neg_x * neg_x) + (neg_y * neg_y) + (neg_z * neg_z));
    if (dist > 0.0f)
    {
        inv_dist = 1.0f / (model->scale * dist);
        coord->f[0] = neg_x * inv_dist;
        coord->f[1] = neg_y * inv_dist;
        coord->f[2] = neg_z * inv_dist;
        return;
    }

    coord->f[0] = 0.0f;
    coord->f[1] = 0.0f;
    coord->f[2] = 1.0f / model->scale;
}


void modelGetDirToCamera(Model* model, coord3d* coord)
{
  modelCalcDirToCamera(model, coord);
}


// PD: model0001a524
s32 modelFindNodeMtxIndex(ModelNode *node, s32 which)
{
    union ModelRoData *rodata1;
    union ModelRoData *rodata2;
    union ModelRoData *rodata3;
    union ModelRoData *rodata4;

    while (node)
    {
        switch (node->Opcode & 0xff)
        {
            case MODELNODE_OPCODE_HEADER:
                rodata1 = node->Data;
                return rodata1->Header.MatrixIndex;

            case MODELNODE_OPCODE_GROUP:
                rodata2 = node->Data;
                return rodata2->Group.MatrixIDs[which == 0x200 ? 2 : (which == 0x100 ? 1 : 0)];

            case MODELNODE_OPCODE_OP03:
                rodata3 = node->Data;
                return rodata3->Group.MatrixIDs[which == 0x200 ? 2 : (which == 0x100 ? 1 : 0)];

            case MODELNODE_OPCODE_GROUPSIMPLE:
                rodata4 = node->Data;
                return rodata4->GroupSimple.Group1;
                break;
        }

        node = node->Parent;
    }

    return -1;
}


// PD: model0001a5cc
Mtxf *modelFindNodeMtx(struct Model *model, struct ModelNode *node, s32 which) {
    s32 index = modelFindNodeMtxIndex(node, which);

    if (index >= 0) {
        return &model->render_pos[index].pos;
    }

    return NULL;
}


//rejoined per EU
// PD: model0001a60c
Mtxf *modelGetRootMtx(Model *objinst)
{
    return modelFindNodeMtx(objinst, objinst->obj->RootNode, 0);
}


f32 modelGetRootDepth(Model *objinst)
{
    Mtxf *mtx = modelGetRootMtx(objinst);
    if (mtx != 0)
    {
        return -mtx->m[3][2];
    }
    return 0.0f;
}


union ModelRwData* modelGetNodeRwData(Model *Objinst, ModelNode *root)
{
    s32 index  = 0;
    u32 *data = Objinst->datas;

    switch (root->Opcode & 0xff)
    {
        case MODELNODE_OPCODE_HEADER:
        {
            index = root->Data->Header.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_DLCOLLISION:
        {
            index = root->Data->DisplayListCollisions.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_TUBE:
        {
            index = root->Data->Tube.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_LOD:
        {
            index = root->Data->LOD.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_SWITCH:
        {
            index = root->Data->Switch.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_BSP:
        {
            index = root->Data->BSP.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_OP11:
        {
            index = root->Data->Op11.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_GUNFIRE:
        {
            index = root->Data->Gunfire.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_HEAD:
        {
            index = root->Data->HeadPlaceholder.RwDataIndex;
            break;
        }
    }

    while (root->Parent)
    {
        root = root->Parent;
        if ((root->Opcode & 0xFF) == MODELNODE_OPCODE_HEAD)
        {
            data = modelGetNodeRwData(Objinst, root)->HeadPlaceholder.RwDatas;
            break;
        }
    }

    return (union ModelRwData *)&data[index];
}


void modelGetNodePosition(Model *objinst, ModelNode *part, coord3d *offset) //#MATCH - however OPCODE 3 needs defining
{
    switch (part->Opcode & 0xFF)
    {
        case MODELNODE_OPCODE_HEADER:
        {
            ModelRwData_HeaderRecord *root = &modelGetNodeRwData(objinst, part)->Header;
            offset->x                   = root->pos.x;
            offset->y                   = root->pos.y;
            offset->z                   = root->pos.z;
            break;
        }
        case MODELNODE_OPCODE_GROUP:
        {
            ModelRoData_GroupRecord *prt = &part->Data->Group;
            offset->x                  = prt->Origin.x;
            offset->y                  = prt->Origin.y;
            offset->z                  = prt->Origin.z;
            break;
        }
        case MODELNODE_OPCODE_OP03:
        {
            ModelRoData_GroupSimpleRecord *prt = &part->Data->GroupSimple; //UNUSED at this time
            offset->x                        = prt->Origin.x;
            offset->y                        = prt->Origin.y;
            offset->z                        = prt->Origin.z;
            break;
        }
        case MODELNODE_OPCODE_GROUPSIMPLE:
        {
            ModelRoData_GroupSimpleRecord *prt = &part->Data->GroupSimple;
            offset->x                        = prt->Origin.x;
            offset->y                        = prt->Origin.y;
            offset->z                        = prt->Origin.z;
            break;
        }
        default:
        {
            offset->x = 0.0f;
            offset->y = 0.0f;
            offset->z = 0.0f;
            break;
        }
    }
}


void modelSetNodePosition(Model *model, ModelNode *node, coord3d *pos)
{
    switch (node->Opcode & 0xff)
    {
        case MODELNODE_OPCODE_HEADER:
            {
                ModelRwData_HeaderRecord *rwdata = &modelGetNodeRwData(model, node)->Header;
                coord3d diff[1];

                diff[0].x = pos->x - rwdata->pos.x;
                diff[0].z = pos->z - rwdata->pos.z;

                rwdata->pos.x = pos->x;
                rwdata->pos.y = pos->y;
                rwdata->pos.z = pos->z;

                rwdata->nextPos.x += diff[0].x; rwdata->nextPos.z += diff[0].z;
                rwdata->framePos.x += diff[0].x; rwdata->framePos.z += diff[0].z;
                rwdata->anim2NextPos.x += diff[0].x; rwdata->anim2NextPos.z += diff[0].z;
                rwdata->anim2FramePos.x += diff[0].x; rwdata->anim2FramePos.z += diff[0].z;
            }
            break;
        case MODELNODE_OPCODE_GROUP:
            {
                ModelRoData_GroupRecord *rodata = &node->Data->Group;
                rodata->Origin.x = pos->x;
                rodata->Origin.y = pos->y;
                rodata->Origin.z = pos->z;
            }
            break;
        case MODELNODE_OPCODE_OP03:
            {
                ModelRoData_GroupRecord *rodata = &node->Data->Group;
                rodata->Origin.x = pos->x;
                rodata->Origin.y = pos->y;
                rodata->Origin.z = pos->z;
            }
            break;
        case MODELNODE_OPCODE_GROUPSIMPLE:
            {
                ModelRoData_GroupSimpleRecord *rodata = &node->Data->GroupSimple;
                rodata->Origin.x = pos->x;
                rodata->Origin.y = pos->y;
                rodata->Origin.z = pos->z;
            }
            break;
    }
}


void modelGetRootPosition(Model *objinst, coord3d *offset) //#MATCH
{
    modelGetNodePosition(objinst, objinst->obj->RootNode, offset);
}


void modelSetRootPosition(Model *objinst, coord3d *offset) //#MATCH
{
    modelSetNodePosition(objinst, objinst->obj->RootNode, offset);
}


f32 modelGetRootRotY(Model *objinst)
{
    ModelNode *root;


    root = objinst->obj->RootNode;
    if ((root->Opcode & 0xFF) == MODELNODE_OPCODE_HEADER)
    {
        return modelGetNodeRwData(objinst, root)->Header.yrot;
    }

    return 0.0f;
}


void modelSetRootRotY(Model *model, f32 angle)
{
    ModelNode* node;
    node = model->obj->RootNode;
    if ((node->Opcode & 0xff) == MODELNODE_OPCODE_HEADER)
    {
        ModelRwData_HeaderRecord *rwdata = &modelGetNodeRwData(model, node)->Header;
        f32 diff = angle - rwdata->yrot;

        if (diff < 0) { diff += M_TAU_F; }

        rwdata->frameYrot += diff;

        if (rwdata->frameYrot >= M_TAU_F) { rwdata->frameYrot -= M_TAU_F; }

        rwdata->nextYrot += diff;

        if (rwdata->nextYrot >= M_TAU_F) { rwdata->nextYrot -= M_TAU_F; }

        rwdata->yrot = angle;
    }
}


void modelSetScale(Model *objinst, f32 scale)
{
    objinst->scale = scale;
}


/**
 * Scales only the translation component of the root node of an animation.
 * For example, the animation for the plane flight in Runway's outro doesn't
 * actually move the plane very far. This function is used to scale up the translation
 * ~10x to allow it to fly near the camera.
 */
void modelSetAnimTranslationScale(Model* model, f32 scale)
{
    model->anim_translation_scale = scale;
}


f32 modelGetNodeRadius(Model *model, ModelNode *node)
{


    if (node)
    {
        do
        {
            switch (node->Opcode & 0xFF)
            {
                case MODELNODE_OPCODE_HEADER:
                {
                    ModelRoData_HeaderRecord *rodata = &node->Data->Header;
                    return rodata->GroupsAsF32 * model->scale;
                }
                case MODELNODE_OPCODE_GROUP:
                {
                    ModelRoData_GroupRecord *rodata = &node->Data->Group;
                    return rodata->BoundingVolumeRadius * model->scale;
                }
                case MODELNODE_OPCODE_OP03:
                {
                    ModelRoData_GroupRecord *rodata = &node->Data->Group;
                    return rodata->BoundingVolumeRadius * model->scale;
                }
                case MODELNODE_OPCODE_GROUPSIMPLE:
                {
                    ModelRoData_GroupSimpleRecord *rodata = &node->Data->GroupSimple;
                    return rodata->BoundingVolumeRadius * model->scale;
                }
                case MODELNODE_OPCODE_OP11:
                {
                    ModelRoData_Op11Record *rodata = &node->Data->Op11;
                    return rodata->BoundingVolumeRadius * model->scale;
                }
                case MODELNODE_OPCODE_GUNFIRE:
                {
                    ModelRoData_GunfireRecord *rodata = &node->Data->Gunfire;
                    return rodata->Scale * model->scale;
                }
                case MODELNODE_OPCODE_SHADOW:
                {
                    ModelRoData_ShadowRecord *rodata = &node->Data->Shadow;
                    return rodata->Scale * model->scale;
                }
                case MODELNODE_OPCODE_OP14:
                {
                    ModelRoData_Op14Record *rodata = &node->Data->Op14;
                    return rodata->Scale * model->scale;
                }
                case MODELNODE_OPCODE_INTERLINK:
                {
                    ModelRoData_InterlinkageRecord *rodata = &node->Data->Interlinkage;
                    return rodata->Scale * model->scale;
                }
                case MODELNODE_OPCODE_OP16:
                {
                    ModelNode_Op16Record *rodata = &node->Data->Op16;
                    return rodata->Scale * model->scale;
                }
                default:
                    node = node->Parent;
            }
        } while (node);
    }

    return 0.0f;
}


/**
 * PD: model0001af80
*/
f32 modelGetRadius(Model *model)
{

    return model->obj->BoundingVolumeRadius * model->scale;
}


// PD: model0001af98
void modelLerp3(coord3d *v, coord3d *w, float frac)
{
    v->x += (w->x - v->x) * frac;
    v->y += (w->y - v->y) * frac;
    v->z += (w->z - v->z) * frac;
  return;
}


// PD: model0001afe8
f32 modelLerpAngle(f32 from, f32 to, f32 mult)
{
    f32 delta = to - from;

    if (to < from)
    {
        delta += M_TAU_F;
    }

    if (delta < M_PI_F)
    {
        from += delta * mult;

        if (from >= M_TAU_F)
        {
            from -= M_TAU_F;
        }
    }
    else
    {
        from -= (M_TAU_F - delta) * mult;

        if (from < 0)
        {
            from += M_TAU_F;
        }
    }

    return from;
}


// PD: model0001b07c
void modelLerpAngles3(coord3d *dst, coord3d *target, f32 mult)
{
    dst->x = modelLerpAngle(dst->x, target->x, mult);
    dst->y = modelLerpAngle(dst->y, target->y, mult);
    dst->z = modelLerpAngle(dst->z, target->z, mult);
}


u16 animReadChannel(ModelAnimation *anim, s32 fieldIndex, s32 extraBitOffset)
{
    u32 result;
    u32 oldResult;
    struct ModelAnimBitField *desc;
    u8 *byteptr;
    u32 totalBitOffset;
    u32 byteIndex;
    u32 mask;
    u8 bitsRemaining;
    u8 bitsThisRead;

    result = 0;
    desc = anim->bitDescriptors + fieldIndex;
    bitsRemaining = desc->bitCount;

    if (bitsRemaining > 0)
    {
        totalBitOffset = extraBitOffset + desc->bitOffset;
        byteIndex = totalBitOffset >> 3;
        totalBitOffset &= 7;
        byteptr = anim->bitStream + byteIndex;
        bitsThisRead = 8 - totalBitOffset;

        if (bitsRemaining >= bitsThisRead)
        {
            do
            {
                mask = (1 << bitsThisRead) - 1;
                bitsRemaining -= bitsThisRead;
                result |= ((*byteptr) & mask) << bitsRemaining;
                result &= 0xffff;
                byteptr++;
                bitsThisRead = 8;
            }
            while (bitsRemaining >= 8);
        }

        if (bitsRemaining > 0)
        {
            result |= ((*byteptr) >> (bitsThisRead - bitsRemaining)) & ((1 << bitsRemaining) - 1);
            result &= 0xffff;
        }

        bitsRemaining = desc->bitCount;

        if (bitsRemaining < 16)
        {
            oldResult = result;
            mask = 1 << (bitsRemaining - 1);

            if (result & mask)
            {
                result = (oldResult | (((1 << (16 - bitsRemaining)) - 1) << bitsRemaining)) & 0xffff;
            }
        }
    }

    result = desc->valueOffset + result;
    return result;
}


u16 animReadRootMotion16(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, s32 frame, coord16 *out)
{
    u32 scaled;
    s32 base;
    u32 angle_raw;
    u16 angle_ret;
    
    scaled = ((u32) anim->rootBitsPerFrame) * ((u32) frame);
    
    if (flip)
    {
        base = skeleton->Joints[jointnum].mtxB;
    }
    else
    {
        base = skeleton->Joints[jointnum].mtxA;
    }
    
    out->x = animReadChannel(anim, base, scaled);
    out->y = animReadChannel(anim, base + 1, scaled);
    out->z = animReadChannel(anim, base + 2, scaled);
    angle_raw = animReadChannel(anim, base + 3, scaled);
    angle_ret = angle_raw;
    
    if (flip)
    {
        out->x = -out->x;
        
        if (angle_raw != 0)
        {
            angle_ret = 0x10000 - (angle_raw & 0xFFFFFFFFu);
        }
    }
    
    return angle_ret;
}


f32 animReadRootMotion(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, s32 frame, coord3d *pos)
{
    coord16 tmp;
    u16 angle;

    angle = animReadRootMotion16(jointnum, flip, skeleton, anim, frame, &tmp);

    pos->x = (f32)tmp.x;
    pos->y = (f32)tmp.y;
    pos->z = (f32)tmp.z;

    return ((f32)angle * M_TAU_F) / M_U16_MAX_VALUE_F;
}


void modelCalcRootPosition(Model *model, ModelNode *modelNode)
{
    union ModelRwData *rw;
    coord3d pos;
    coord3d newpos;
    f32 y;

    rw = modelGetNodeRwData(model, modelNode);

    if (rw->Header.frozen != 0)
    {
        return;
    }

    pos.x = rw->Header.framePos.x;
    pos.y = rw->Header.framePos.y;
    pos.z = rw->Header.framePos.z;
    rw->Header.yrot = rw->Header.frameYrot;

    if (model->frameFrac != 0.0f)
    {
        if (rw->Header.nextValid != 0)
        {
            modelLerp3(&pos, &rw->Header.nextPos, model->frameFrac);

            rw->Header.yrot = modelLerpAngle(rw->Header.frameYrot, rw->Header.nextYrot, model->frameFrac);
        }
    }

    if ((model->anim2 != 0) || (model->mergeWeight != 0.0f))
    {
        if (rw->Header.anim2NextValid != 0)
        {
            y = rw->Header.anim2FramePos.y;

            if (model->frame2Frac != 0.0f)
            {
                y += (rw->Header.anim2NextPos.y - y) * model->frame2Frac;
            }

            pos.y += (y - pos.y) * model->mergeWeight;
        }
    }

    newpos.x = pos.x;
    newpos.y = pos.y;
    newpos.z = pos.z;

    if (model->rootMoveCallback && !((s32 (*)(Model *, coord3d *, coord3d *, f32 *)) model->rootMoveCallback)(model, &rw->Header.pos, &newpos, &rw->Header.ground))
    {
        return;
    }

    pos.x = newpos.x - pos.x;
    pos.z = newpos.z - pos.z;

    rw->Header.pos.x = newpos.x;
    rw->Header.pos.y = ((f32) newpos.y) + rw->Header.ground;
    rw->Header.pos.z = newpos.z;

    rw->Header.framePos.x += pos.x;
    rw->Header.framePos.z += pos.z;

    if (rw->Header.nextValid != 0)
    {
        rw->Header.nextPos.x += pos.x;
        rw->Header.nextPos.z += pos.z;
    }

    if (rw->Header.anim2NextValid != 0)
    {
        rw->Header.anim2FramePos.x += pos.x;
        rw->Header.anim2FramePos.z += pos.z;
        rw->Header.anim2NextPos.x += pos.x;
        rw->Header.anim2NextPos.z += pos.z;
    }
}


void modelCalcPosition(Model *model)
{
    struct ModelNode *root;


    root = model->obj->RootNode;
    if ((root != NULL) && ((root->Opcode & 0xFF) == 1))
    {
        modelCalcRootPosition(model, root);
    }
}

void modelCalcRootMtx(ModelRenderData* renderdata, Model* model, ModelNode* node)
{
    union ModelRoData* rodata;
    union ModelRwData* rwdata;
    f32 scale;
    coord3d* pos;
    f32 yrot;
    Mtxf* parent;
    s32 mtxindex;
    RenderPosView* renderpos;
    Mtxf tmp;

    rodata = node->Data;
    rwdata = modelGetNodeRwData(model, node);

    scale = model->scale;
    pos = &rwdata->Header.pos;
    yrot = rwdata->Header.yrot;
    mtxindex = rodata->Header.MatrixIndex;
    renderpos = &model->render_pos[mtxindex];

    if (node->Parent != NULL)
    {
        parent = modelFindNodeMtx(model, node->Parent, 0);
    }
    else
    {
        parent = renderdata->basemtx;
    }

    if (rwdata->Header.yrotBlend != 0.0f)
    {
        yrot = modelLerpAngle(yrot, rwdata->Header.yrotBlendTarget, rwdata->Header.yrotBlend);
    }

    if (parent != NULL)
    {
        mtx4LoadYRotationAndTranslation(pos, yrot, &tmp);

        if (scale != 1.0f)
        {
            mtx4Scale3x3(scale, &tmp);
        }

        mtx4MultAffine(parent, &tmp, &renderpos->pos);
        return;
    }

    mtx4LoadYRotationAndTranslation(pos, yrot, &renderpos->pos);

    if (scale != 1.0f)
    {
        mtx4Scale3x3(scale, &renderpos->pos);
    }
}


void modelCalcJointMtxFromAngles(ModelRenderData *renderdata, Model *model, ModelNode *node, coord3d *rot)
{
    u32 flags = node->Opcode;
    ModelRoData_GroupRecord *group = &node->Data->Group;
    s32 matrix0 = group->MatrixID0;
    s32 matrix1 = group->MatrixID1;
    s32 matrix2 = group->MatrixID2;
    RenderPosView *render_pos = model->render_pos;
    s32 has_matrix2 = flags & MODELGROUP_MTX_HAS_MATRIX2;
    Mtxf *parent;
    Mtxf *matrix0_mtx;
    Mtxf tmp;
    quatf q;
    quatf q2;
    Mtxf *dst;
    f32 angle;

    if (node->Parent != NULL)
    {
        parent = modelFindNodeMtx(model, node->Parent, 0);
    }
    else
    {
        parent = renderdata->basemtx;
    }

    if (parent != NULL)
    {
        mtx4LoadRotationAndTranslation(&group->Origin, rot, &tmp);

        matrix0_mtx = &render_pos[matrix0].pos;
        mtx4MultAffine(parent, &tmp, matrix0_mtx);

        if (g_ModelJointPositionedFunc != NULL)
        {
            g_ModelJointPositionedFunc(matrix0, matrix0_mtx);
        }
    }
    else
    {
        mtx4LoadRotationAndTranslation(&group->Origin, rot, &render_pos[matrix0].pos);
    }
    
    if (flags & MODELGROUP_MTX_HAS_MATRIX1)
    {
        quatFromAngles(rot, &q);
        quatSlerpFromIdentity(&q, 0.5f, &q2);
        
        if (parent != NULL)
        {
            quatToMtx4WithTranslation(&group->Origin, &q2, &tmp);
            mtx4MultAffine(parent, &tmp, &render_pos[matrix1].pos);
        }
        else
        {
            quatToMtx4WithTranslation(&group->Origin, &q2, &render_pos[matrix1].pos);
        }
    }

    if (has_matrix2)
    {
        if (parent != NULL)
        {
            dst = &tmp;
        }
        else
        {
            dst = &render_pos[matrix2].pos;
        }
        
        angle = rot->y;
        
        if (angle < M_PI_F)
        {
            angle = angle * 0.5f;
        }
        else
        {
            angle = M_TAU_F - ((M_TAU_F - angle) * 0.5f);
        }
        
        mtx4LoadYRotation(angle, dst);
        
        if (angle >= M_PI_F)
        {
            angle = M_TAU_F - angle;
        }
        if (angle < 0.890118f)
        {
            angle = modelGetBendStretchScale(angle);
        }
        else
        {
            angle = 1.5f;
        }
        
        mtx4ScaleRow2(angle, dst);
        mtx4SetTranslation(&group->Origin, dst);
        
        if (parent != NULL)
        {
            mtx4MultAffine(parent, dst, &render_pos[matrix2].pos);
        }
    }
}


void modelCalcJointMtxFromQuat(ModelRenderData *renderdata, Model *model, ModelNode *node, const quatf *rot)
{
    s32 flags = node->Opcode;
    ModelRoData_GroupRecord *group = (ModelRoData_GroupRecord *)node->Data;
    s32 matrix0 = group->MatrixID0;
    s32 matrix1 = group->MatrixID1;
    s32 matrix2 = group->MatrixID2;
    RenderPosView *render_pos = model->render_pos;
    Mtxf *parent;
    Mtxf tmp;
    quatf halfrot;
    Mtxf *dst;
    f32 angle;

    if (node->Parent != NULL) {
        parent = modelFindNodeMtx(model, node->Parent, 0);
    } else {
        parent = renderdata->basemtx;
    }

    if (parent != NULL) {
        Mtxf *matrix0_mtx = &render_pos[matrix0].pos;

        quatToMtx4WithTranslation(&group->Origin, rot, &tmp);
        mtx4MultAffine(parent, &tmp, matrix0_mtx);

        if (g_ModelJointPositionedFunc != NULL) {
            g_ModelJointPositionedFunc(matrix0, matrix0_mtx);
        }
    } else {
        quatToMtx4WithTranslation(&group->Origin, rot, &render_pos[matrix0].pos);
    }

    if (flags & MODELGROUP_MTX_HAS_MATRIX1) {
        quatSlerpFromIdentity(rot, 0.5f, &halfrot);

        if (parent != NULL) {
            quatToMtx4WithTranslation(&group->Origin, &halfrot, &tmp);
            mtx4MultAffine(parent, &tmp, &render_pos[matrix1].pos);
        } else {
            quatToMtx4WithTranslation(&group->Origin, &halfrot, &render_pos[matrix1].pos);
        }
    }

    if (flags & MODELGROUP_MTX_HAS_MATRIX2) {
        if (parent != NULL) {
            dst = &tmp;
        } else {
            dst = &render_pos[matrix2].pos;
        }

        angle = 2.0f * mathAcos(rot->w);

        if (angle < M_PI_F) {
            angle = angle * 0.5f;
        } else {
            angle = M_TAU_F - ((M_TAU_F - angle) * 0.5f);
        }

        mtx4LoadYRotation(angle, dst);

        if (angle >= M_PI_F) {
            angle = M_TAU_F - angle;
        }

        if (angle < 0.890118f) {
            angle = modelGetBendStretchScale(angle);
        } else {
            angle = 1.5f;
        }

        mtx4ScaleRow2(angle, dst);
        mtx4SetTranslation(&group->Origin, dst);

        if (parent != NULL) {
            mtx4MultAffine(parent, dst, &render_pos[matrix2].pos);
        }
    }
}


u32 animReadAngleBits(u8 *bitstream, u8 width, u32 bitOffset)
{
    u32 value = 0;
    u32 mask;
    u8 numbitsthisbyte;
    u8 remainingbits;

    remainingbits = width;
    value *= bitOffset / 8;


    remainingbits = width;
    bitstream += bitOffset / 8;
    bitOffset %= 8;
    numbitsthisbyte = 8 - bitOffset;

    while (remainingbits >= numbitsthisbyte)
    {
        remainingbits -= numbitsthisbyte;
        mask = (1 << numbitsthisbyte) - 1;
        value |= ((u16)((*bitstream) & mask)) << remainingbits;
        value &= 0xffff;
        bitstream++;
        numbitsthisbyte = 8;
    }

    if (remainingbits > 0)
    {
        mask = (1 << remainingbits) - 1;
        value |= ((*bitstream) >> (numbitsthisbyte - remainingbits)) & mask;
        value &= 0xffff;
    }

    value <<= 16 - width;

    return value & 0xffff;
}


void animReadJointRotation(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, u8 *bitstream, coord3d *rot)
{
    u32 bitoffset;
    u8 width;
    u16 rotation[3];

    width = anim->angleBits;

    // Mirrored joint rotation?
    if (flip)
    {
        bitoffset = skeleton->Joints[jointnum].mtxB * width;
    }
    else
    {
        bitoffset = skeleton->Joints[jointnum].mtxA * width;
    }

    width = anim->angleBits;

    rotation[0] = animReadAngleBits(bitstream, width, bitoffset);
    bitoffset += (unsigned long) width;

    rotation[1] = animReadAngleBits(bitstream, width, bitoffset);
    bitoffset += width;

    rotation[2] = animReadAngleBits(bitstream, width, bitoffset);

    rot->x = (rotation[0] * M_TAU_F) / M_U16_MAX_VALUE_F;

    if (flip)
    {
        if (rotation[1] != 0)
        {
            rot->y = ((0x10000 - rotation[1]) * M_TAU_F) / M_U16_MAX_VALUE_F;
        }
        else
        {
            rot->y = 0.0f;
        }

        if (rotation[2] != 0)
        {
            rot->z = ((0x10000 - rotation[2]) * M_TAU_F) / M_U16_MAX_VALUE_F;
        }
        else
        {
            rot->z = 0.0f;
        }
    }
    else
    {
        rot->y = (rotation[1] * M_TAU_F) / M_U16_MAX_VALUE_F;
        rot->z = (rotation[2] * M_TAU_F) / M_U16_MAX_VALUE_F;
    }
}


void modelCalcJointMtx(ModelRenderData *renderdata, Model *model, ModelNode *node)
{
    union
    {
        s32 v;
        long long int force_structure_alignment;
    } jointnum;

    ModelSkeleton *skeleton;
    coord3d rot1;
    coord3d rot2;
    coord3d rot3;
    quatf q1;
    quatf q2;
    quatf result;
    coord3d rot4;
    ModelRoData_GroupRecord *group;

    group = &node->Data->Group;
    jointnum.v = group->JointID;
    skeleton = model->obj->Skeleton;

    rot1 = (coord3d){0};
    
    animReadJointRotation(jointnum.v, model->flip, skeleton, model->anim, model->frameaData, &rot1);

    if (model->frameFrac != 0.0f)
    {
        rot2 = (coord3d){0};
        animReadJointRotation(jointnum.v, model->flip, skeleton, model->anim, model->framebData, &rot2);
        modelLerpAngles3(&rot1, &rot2, model->frameFrac);
    }

    if (model->mergeWeight != 0.0f)
    {
        rot3 = (coord3d){0};
        animReadJointRotation(jointnum.v, model->flip2, skeleton, model->anim2, model->frame2aData, &rot3);

        if (model->frame2Frac != 0.0f)
        {
            rot4 = (coord3d){0};
            animReadJointRotation(jointnum.v, model->flip2, skeleton, model->anim2, model->frame2bData, &rot4);
            modelLerpAngles3(&rot3, &rot4, model->frame2Frac);
        }

        quatFromAngles(&rot1, &q1);
        quatFromAngles(&rot3, &q2);
        quatMakeShortestPath(&q1, &q2);
        quatSlerp(&q1, &q2, model->mergeWeight, &result);
        modelCalcJointMtxFromQuat(renderdata, model, node, &result);
    }
    else
    {
        modelCalcJointMtxFromAngles(renderdata, model, node, &rot1);
    }
}


void modelCalcYJointMtxFromAngle(ModelRenderData *renderData, Model *model, ModelNode *node, f32 angle)
{
    s32 opcode;
    union ModelRoData *data;
    Mtxf *mtx;
    RenderPosView *render_pos;
    Mtxf localMtx;
    s32 m0;
    s32 m1;
    s32 m2;
    RenderPosView *renderPosBase;
    s32 matrix2Flag;
    Mtxf *localMtxPtr;
    Mtxf *matrixPtr;
    f32 scalar;

    opcode = node->Opcode;
    data = node->Data;
    m0 = data->Group.MatrixID0;
    m1 = data->Group.MatrixID1;
    m2 = data->Group.MatrixID2;
    renderPosBase = model->render_pos;
    render_pos = renderPosBase;

    if (node->Parent != NULL)
    {
        mtx = modelFindNodeMtx(model, node->Parent, 0);
    }
    else
    {
        mtx = renderData->basemtx;
    }

    localMtxPtr = &localMtx;

    if (mtx != NULL)
    {
        mtx4LoadYRotationAndTranslation(&data->Group.Origin, angle, localMtxPtr);
        mtx4MultAffine(mtx, &localMtx, &render_pos[m0].pos);
    }
    else
    {
        mtx4LoadYRotationAndTranslation(&data->Group.Origin, angle, &render_pos[m0].pos);
    }

    matrix2Flag = 0x200;
    m0 = opcode & matrix2Flag;

    if ((opcode & 0x100) || m0)
    {

        if (angle < M_PI_F)
        {
            angle = angle * 0.5f;
        }
        else
        {
            angle = M_TAU_F - ((M_TAU_F - angle) * 0.5f);
        }
    }

    if (opcode & 0x100)
    {
        if (mtx != NULL)
        {
            mtx4LoadYRotationAndTranslation(&data->Group.Origin, angle, &localMtx);
            mtx4MultAffine(mtx, localMtxPtr, &render_pos[m1].pos);
        }
        else
        {
            mtx4LoadYRotationAndTranslation(&data->Group.Origin, angle, &render_pos[m1].pos);
        }
    }

    if (m0)
    {
        if (mtx != NULL)
        {
            matrixPtr = &localMtx;
        }
        else
        {
            matrixPtr = &render_pos[m2].pos;
        }

        mtx4LoadYRotation(angle, matrixPtr);

        if (M_PI_F <= angle)
        {
            angle = M_TAU_F - angle;
        }

        if (angle < 0.890118f)
        {
            scalar = modelGetBendStretchScale(angle);
        }
        else
        {
            scalar = 1.5f;
        }

        mtx4ScaleRow2(scalar, matrixPtr);
        mtx4SetTranslation(&data->Group.Origin, matrixPtr);

        if (mtx != NULL)
        {
            mtx4MultAffine(mtx, matrixPtr, &render_pos[m2].pos);
        }
    }
}


// Decodes a packed joint angle from the animation bitstream using either mtxA or mtxB.
f32 animReadJointYRotation(s32 jointIndex, s32 useMtxB, ModelSkeleton *skeleton, ModelAnimation *anim, u8 *bitstream)
{    
    u32 bitOffset;
    u32 raw;
    u8 width;
    f32 angle;

    angle = 0.0f;
    width = anim->angleBits;

    if (useMtxB != 0) {
        bitOffset = skeleton->Joints[jointIndex].mtxB * width;
    } else {
        bitOffset = skeleton->Joints[jointIndex].mtxA * width;
    }

    raw = animReadAngleBits(bitstream, width, bitOffset);

    if (useMtxB != 0) {
        if (raw != 0) {
            angle = ((f32)(s32)(0x10000 - raw) * M_TAU_F) / M_U16_MAX_VALUE_F;
        }
    } else {
        angle = ((f32)raw * M_TAU_F) / M_U16_MAX_VALUE_F;
    }

    return angle;
}


void modelCalcYJointMtx(ModelRenderData *renderData, Model *model, ModelNode *node)
{    
    ModelSkeleton *skeleton;
    ModelRoData_GroupRecord *rodata;
    s32 jointIndex;
    f32 angle;
    f32 tmp2;
    f32 tmp;

    rodata = &node->Data->Group;
    jointIndex = rodata->JointID;
    skeleton = model->obj->Skeleton;

    angle = animReadJointYRotation(jointIndex, model->flip, skeleton, model->anim, model->frameaData);

    if (model->frameFrac != 0.0f) {
        tmp = animReadJointYRotation(jointIndex, model->flip, skeleton, model->anim, model->framebData);
        angle = modelLerpAngle(angle, tmp, model->frameFrac);
    }

    if (model->mergeWeight != 0.0f) {
        tmp = animReadJointYRotation(jointIndex, model->flip2, skeleton, model->anim2, model->frame2aData);

        if (model->frame2Frac != 0.0f) {
            tmp2 = animReadJointYRotation(jointIndex, model->flip2, skeleton, model->anim2, model->frame2bData);
            tmp = modelLerpAngle(tmp, tmp2, model->frame2Frac);
        }

        angle = modelLerpAngle(angle, tmp, model->mergeWeight);
    }

    modelCalcYJointMtxFromAngle(renderData, model, node, angle);
}


void modelCalcOffsetMtx(ModelRenderData* renderdata, Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;
    Mtxf *parent;
    Mtxf tmp;
    s32 mtxindex = rodata->GroupSimple.Group1;
    RenderPosView *matrices = model->render_pos;

    if (node->Parent)
    {
        parent = modelFindNodeMtx(model, node->Parent, 0);
    }
    else
    {
        parent = renderdata->basemtx;
    }

    if (parent)
    {
        mtx4LoadTranslation(&rodata->GroupSimple.Origin, &tmp);
        mtx4MultAffine(parent, &tmp, &matrices[mtxindex].pos);
    }
    else
    {
        mtx4LoadTranslation(&rodata->GroupSimple.Origin, &matrices[mtxindex].pos);
    }
}

void modelUpdateDistanceRelations(Model* model, ModelNode* node)
{
    union ModelRoData *rodata = node->Data;
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    Mtxf *mtx = modelFindNodeMtx(model, node, 0);
    f32 distance;

    if (g_ModelLodDisabled)
    {
        distance = 0;
    }
    else
    {
        distance = -mtx->m[3][2] * camGetLodScaleZ();

        if (g_ModelLodScale != 1)
        {
            distance *= g_ModelLodScale;
        }
    }

    if (distance > rodata->LOD.MinDistance * model->scale || rodata->LOD.MinDistance == 0)
    {
        if (distance <= rodata->LOD.MaxDistance * model->scale)
        {
            rwdata->LOD.visible = TRUE;
            node->Child = rodata->LOD.Affects;
            return;
        }
    }

    rwdata->LOD.visible = FALSE;
    node->Child = NULL;
}

void modelApplyDistanceRelations(Model* model, ModelNode* node)
{
    ModelRoData_LODRecord *rodata = &node->Data->LOD;
    ModelRwData_LODRecord *rwdata = &modelGetNodeRwData(model, node)->LOD;

    if (rwdata->visible)
    {
        node->Child = rodata->Affects;
    }
    else
    {
        node->Child = NULL;
    }
}


void modelApplyToggleRelations(Model* model, ModelNode* node)
{
    ModelRoData_SwitchRecord *rodata = &node->Data->Switch;
    ModelRwData_SwitchRecord *rwdata = &modelGetNodeRwData(model, node)->Switch;

    if (rwdata->visible)
    {
        node->Child = rodata->Controls;
    }
    else
    {
        node->Child = NULL;
    }
}


void modelApplyHeadRelations(Model* model, ModelNode* bodynode)
{
    struct ModelRwData_HeadPlaceholderRecord *rwdata = &modelGetNodeRwData(model, bodynode)->HeadPlaceholder;

    if (rwdata->ModelFileHeader)
    {
        ModelNode *headnode = rwdata->ModelFileHeader->RootNode;

        bodynode->Child = headnode;

        while (headnode)
        {
            headnode->Parent = bodynode;
            headnode = headnode->Next;
        }
    }
}


void modelApplyReorderRelationsByArg(ModelNode *basenode, bool visible)
{
    union ModelRoData *rodata = basenode->Data;
    ModelNode *node1;
    ModelNode *node2;
    ModelNode *loopnode;

    if (visible)
    {
        node1 = rodata->BSP.leftChild;
        node2 = rodata->BSP.rightChild;
    }
    else
    {
        node1 = rodata->BSP.rightChild;
        node2 = rodata->BSP.leftChild;
    }

    if (node1)
    {
        // I think what's happening here is there's two groups of siblings,
        // where node1 and node2 are the head nodes. Either group can be first,
        // and this is ensuring the node1 group is first.
        // Note that node2 might be NULL.

        basenode->Child = node1;
        node1->Prev = NULL;

        // Skip through node1's siblings until node2 is found or the end is
        // reached
        loopnode = node1;

        while (loopnode->Next && loopnode->Next != node2)
        {
            loopnode = loopnode->Next;
        }

        loopnode->Next = node2;

        if (node2)
        {
            // Append node2 and its siblings to node1's siblings
            node2->Prev = loopnode;
            loopnode = node2;

            while (loopnode->Next && loopnode->Next != node1)
            {
                loopnode = loopnode->Next;
            }

            loopnode->Next = NULL;
        }
    }
    else
    {
        basenode->Child = node2;

        if (node2)
        {
            node2->Prev = NULL;
        }
    }
}


void modelApplyReorderRelations(Model* model, ModelNode* node)
{
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    modelApplyReorderRelationsByArg(node, rwdata->BSP.visible);
}


void modelUpdateReorderRelations(Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    Mtxf *mtx = modelFindNodeMtx(model, node, 0);
    coord3d normal;
    coord3d point;
    f32 dot;

    if (rodata->BSP.reserved == 0)
    {
        normal.x = rodata->BSP.Vector.f[0];
        normal.y = rodata->BSP.Vector.f[1];
        normal.z = rodata->BSP.Vector.f[2];
        mtx4RotateVecInPlace(mtx, &normal);
    }
    else if (rodata->BSP.reserved == 2)
    {
        normal.x = mtx->m[1][0] * rodata->BSP.Vector.f[1];
        normal.y = mtx->m[1][1] * rodata->BSP.Vector.f[1];
        normal.z = mtx->m[1][2] * rodata->BSP.Vector.f[1];
    }
    else if (rodata->BSP.reserved == 3)
    {
        normal.x = mtx->m[2][0] * rodata->BSP.Vector.f[2];
        normal.y = mtx->m[2][1] * rodata->BSP.Vector.f[2];
        normal.z = mtx->m[2][2] * rodata->BSP.Vector.f[2];
    }
    else if (rodata->BSP.reserved == 1)
    {
        normal.x = mtx->m[0][0] * rodata->BSP.Vector.f[0];
        normal.y = mtx->m[0][1] * rodata->BSP.Vector.f[0];
        normal.z = mtx->m[0][2] * rodata->BSP.Vector.f[0];
    }

    point.x = rodata->BSP.Point.f[0];
    point.y = rodata->BSP.Point.f[1];
    point.z = rodata->BSP.Point.f[2];

    mtx4TransformVecInPlace(mtx, &point);

    dot = normal.f[0] * point.f[0] + normal.f[1] * point.f[1] + normal.f[2] * point.f[2];

    if (dot < 0)
    {
        rwdata->BSP.visible = TRUE;
    }
    else
    {
        rwdata->BSP.visible = FALSE;
    }

    modelApplyReorderRelations(model, node);
}


void modelUpdateTubeIndex(Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    Mtxf *mtx = modelFindNodeMtx(model, node, 0);
    f32 ratio;
    f32 coord_multiplied;
    coord3d coord;
    s32 index1;
    f32 theta;
    s32 index2;
    s32 index3;

    modelGetDirToCamera(model, &coord);

    theta = mathAcos(((coord.x * mtx->m[1][0]) + (coord.y * mtx->m[1][1])) + (coord.z * mtx->m[1][2]));
    ratio = mathAcos((((coord.x * mtx->m[2][0]) + (coord.y * mtx->m[2][1])) + (coord.z * mtx->m[2][2])) / sinf(theta));
    coord_multiplied = ((coord.x * mtx->m[0][0]) + (coord.y * mtx->m[0][1])) + (coord.z * mtx->m[0][2]);

    if ((coord_multiplied < 0.0f) && (ratio > 0.0f))
    {
        ratio = M_TAU_F - ratio;
    }

    index1 = (theta * 64.0f) / M_TAU_F;

    index2 = (s32) ((ratio * M_U16_MAX_VALUE_F) / M_TAU_F);
    index2 += g_ModelTubeAngleTable[index1].round;
    index2 = index2 >> g_ModelTubeAngleTable[index1].shift;

    index3 = index2 + g_ModelTubeAngleTable[index1].first;

    rwdata->Tube.index = rodata->Tube.Data[index3];
}


void modelUpdateRelationsQuick(Model *model, ModelNode *parent)
{
    ModelNode *node = parent->Child;

    while (node)
    {
        s32 type = node->Opcode & 0xff;
        bool dochildren = TRUE;

        switch (type)
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
                dochildren = FALSE;
                break;
            case MODELNODE_OPCODE_LOD:
                modelUpdateDistanceRelations(model, node);
                break;
            case MODELNODE_OPCODE_BSP:
                modelUpdateReorderRelations(model, node);
                break;
            case MODELNODE_OPCODE_TUBE:
                modelUpdateTubeIndex(model, node);
                break;
            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;
            case MODELNODE_OPCODE_DLCOLLISION:
                break;
        }

        if (dochildren && node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node == parent->Parent)
                {
                    node = NULL;
                    break;
                }

                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelUpdateNodeRelations(Model *model)
{
    ModelNode *node = model->obj->RootNode;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_LOD:
                modelUpdateDistanceRelations(model, node);
                break;

            case MODELNODE_OPCODE_BSP:
                modelUpdateReorderRelations(model, node);
                break;

            case MODELNODE_OPCODE_TUBE:
                modelUpdateTubeIndex(model, node);
                break;

            case MODELNODE_OPCODE_SWITCH:
                modelApplyToggleRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEADER:
            case MODELNODE_OPCODE_DLCOLLISION:
            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelUpdateMatrices(ModelRenderData *renderdata, Model *model)
{
    ModelNode *node = model->obj->RootNode;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                modelCalcRootMtx(renderdata, model, node);
                break;

            case MODELNODE_OPCODE_GROUP:
                modelCalcJointMtx(renderdata, model, node);
                break;

            case MODELNODE_OPCODE_OP03:
                modelCalcYJointMtx(renderdata, model, node);
                break;

            case MODELNODE_OPCODE_GROUPSIMPLE:
                modelCalcOffsetMtx(renderdata, model, node);
                break;

            case MODELNODE_OPCODE_LOD:
                modelUpdateDistanceRelations(model, node);
                break;

            case MODELNODE_OPCODE_BSP:
                modelUpdateReorderRelations(model, node);
                break;

            case MODELNODE_OPCODE_TUBE:
                modelUpdateTubeIndex(model, node);
                break;

            case MODELNODE_OPCODE_SWITCH:
                modelApplyToggleRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;

            case MODELNODE_OPCODE_DLCOLLISION:
            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelCalcMatricesNoAnim(ModelRenderData* renderdata, Model* model)
{
    model->render_pos = (RenderPosView* ) renderdata->mtxlist;
    renderdata->mtxlist += model->obj->numMatrices;
    modelUpdateMatrices((ModelRenderData* ) renderdata, model);
}


/**
 * Address 0x7F06F2F8 (VERSION_US, VERSION_JP)
 * Address 0x7F06F670 (VERSION_EU)
*/
void modelCalcMatrices(ModelRenderData *renderdata, struct Model *model)
{

    if (model->anim != NULL)
    {

        model->frameaData = animLoadFrame(model->anim, model->framea);

        if (model->frameFrac != 0.0f)
        {
            model->framebData = animLoadFrame(model->anim, model->frameb);
        }

        if (model->anim2 != NULL)
        {
            model->frame2aData = animLoadFrame(model->anim2, model->frame2a);

            if (model->frame2Frac != 0.0f)
            {
                model->frame2bData = animLoadFrame(model->anim2, model->frame2b);
            }
        }

        animResetFrameBuffer();
    }

    modelCalcMatricesNoAnim(renderdata, model);
}

struct ModelAnimation * modelGetAnim(struct Model* model) {
    return model->anim;
}

s8 modelGetAnimFlip(Model *model) 
{
    return model->flip;
}


f32 modelGetAnimFrame(Model *model)
{
    return model->frame;
}


f32 modelGetAnimEndFrame(Model *model)
{
    f32 end;
    ModelAnimation *modelAnimation;

    end = model->endframe;

    if (end >= 0.0f)
    {
        return end;
    }

    modelAnimation = model->anim;

    if (modelAnimation != NULL)
    {
        return modelAnimation->numFrames - 1;
    }

    return 0.0f;
}


f32 modelGetAnimSpeed(Model *model)
{
    return model->speed;
}


/**
 * PD: modelGetAbsAnimSpeed
*/
f32 modelGetAbsAnimSpeed(Model *model)
{
    f32 speed;

    speed = model->speed;

    if (speed < 0.0f)
    {
        speed = -speed;
    }

    return speed;
}


s32 modelConstrainOrWrapAnimFrame(s32 frame, ModelAnimation *anim, f32 endframe)
{
    if (frame < 0) {
        if (anim->flags & 1) {
            frame = anim->numFrames - ((-frame) % anim->numFrames);
        } else {
            frame = 0;
        }
    }
    else if ((0.0f <= endframe) && ((s32)endframe < frame)) {
        frame = mathCeilToInt(endframe);
    }
    else if (frame >= anim->numFrames) {
        if (anim->flags & 1) {
            frame %= anim->numFrames;
        } else {
            frame = anim->numFrames - 1;
        }
    }
    else {
    }

    return frame;
}


void modelCopyAnimForMerge(Model *model, f32 timemerge)
{
    ModelAnimation *anim;
    ModelNode *root;
    ModelRwData_HeaderRecord *rwdata;
    s32 opcode; 

    if (0.0f < timemerge) {
        anim = model->anim;

        if (anim != NULL) {
            root = model->obj->RootNode;
            opcode = root->Opcode & 0xff;

            model->anim2 = anim;
            model->frame2 = model->frame;
            model->frame2Frac = model->frameFrac;
            model->flip2 = model->flip;
            model->frame2a = model->framea;
            model->frame2b = model->frameb;
            model->speed2 = model->speed;
            model->newspeed2 = model->newspeed;
            model->oldspeed2 = model->oldspeed;
            model->timespeed2 = model->timespeed;
            model->elapsespeed2 = model->elapsespeed;
            model->endframe2 = model->endframe;

            if (opcode == MODELNODE_OPCODE_HEADER) {
                rwdata = &modelGetNodeRwData(model, root)->Header;
                rwdata->anim2NextValid = 1;
                rwdata->anim2FramePos.x = rwdata->framePos.x;
                rwdata->anim2FramePos.y = rwdata->framePos.y;
                rwdata->anim2FramePos.z = rwdata->framePos.z;
                rwdata->anim2NextPos.x = rwdata->nextPos.x;
                rwdata->anim2NextPos.y = rwdata->nextPos.y;
                rwdata->anim2NextPos.z = rwdata->nextPos.z;
            }

            return;
        }
    }

    model->anim2 = NULL;
}


void modelSetAnimation2(Model *model, ModelAnimation *anim, s32 flip, f32 frame, f32 speed, f32 merge)
{
    s32 hadNoAnim = !model->anim;
    s32 type;

    if (model->anim2 != NULL) {
        model->mergeTime = merge;
        model->mergeElapsed = 0.0f;
        model->mergeWeight = 1.0f;
    } else {
        model->mergeTime = 0.0f;
        model->mergeWeight = 0.0f;
    }

    model->anim = anim;
    model->flip = flip;
    model->endframe = -1.0f;
    model->speed = speed;
    model->timespeed = 0.0f;

    modelSetAnimFrame(model, frame);

    model->animlooping = 0;

    type = model->obj->RootNode->Opcode & 0xff;

    if (type == MODELNODE_OPCODE_HEADER) {
        ModelRoData_HeaderRecord *rodata = &model->obj->RootNode->Data->Header;
        ModelRwData_HeaderRecord *rwdata = &modelGetNodeRwData(model, model->obj->RootNode)->Header;
        f32 prevYrot;
        f32 sinAngle;
        f32 scale;
        f32 cosAngle;
        s32 animPart;
        coord3d pos;
        f32 angleDelta;
        coord3d tmppos[2];
        ModelSkeleton *skeleton;

        animPart = rodata->AnimPart;
        skeleton = model->obj->Skeleton;
        scale = model->scale * model->anim_translation_scale;
        pos = (coord3d){0};

        angleDelta = animReadRootMotion(animPart, model->flip, skeleton, model->anim, model->frameb, &pos
        );

        if (scale != 1.0f) {
            pos.f[0] *= scale;
            pos.f[1] *= scale;
            pos.f[2] *= scale;
        }

        cosAngle = cosf(rwdata->yrot);
        sinAngle = sinf(rwdata->yrot);
        scale = model->frameFrac;

        if (model->frameFrac == 0.0f) {
            rwdata->framePos.x = rwdata->pos.f[0];
            rwdata->framePos.y = rwdata->pos.f[1] - rwdata->ground;
            rwdata->framePos.z = rwdata->pos.f[2];

            rwdata->frameYrot = rwdata->yrot;

            tmppos[1].x = rwdata->framePos.f[0] + (pos.f[0] * cosAngle) + (pos.f[2] * sinAngle);
            tmppos[1].y = pos.f[1];
            tmppos[1].z = rwdata->framePos.f[2] - (pos.f[0] * sinAngle) + (pos.f[2] * cosAngle);

            rwdata->nextPos.x = tmppos[1].f[0];
            rwdata->nextPos.y = tmppos[1].f[1];
            rwdata->nextPos.z = tmppos[1].f[2];

            if (rwdata->yrotBlend == 0.0f) {
                rwdata->nextYrot = rwdata->frameYrot + angleDelta;

                if (rwdata->nextYrot >= M_TAU_F) {
                    rwdata->nextYrot -= M_TAU_F;
                }
            }

            rwdata->nextValid = 1;
        } else {
            f32 x;
            f32 y;
            f32 z;

            x = pos.f[0] * cosAngle + pos.f[2] * sinAngle;
            y = pos.f[1];
            z = -pos.f[0] * sinAngle + pos.f[2] * cosAngle;

            tmppos[0].x = rwdata->pos.f[0] + x * (1.0f - model->frameFrac);
            tmppos[0].y = y;
            tmppos[0].z = rwdata->pos.f[2] + z * (1.0f - model->frameFrac);

            rwdata->nextPos.x = tmppos[0].x;
            rwdata->nextPos.y = tmppos[0].y;
            rwdata->nextPos.z = tmppos[0].z;

            rwdata->framePos.f[0] = rwdata->nextPos.f[0] - x;
            rwdata->framePos.f[1] = (rwdata->pos.f[1] - rwdata->ground) - (y - (rwdata->pos.f[1] - rwdata->ground)) * model->frameFrac / (1.0f - model->frameFrac);
            rwdata->framePos.f[2] = rwdata->nextPos.f[2] - z;

            prevYrot = rwdata->yrot - angleDelta;

            if (prevYrot < 0.0f) {
                prevYrot += M_TAU_F;
            }

            rwdata->frameYrot = modelLerpAngle(rwdata->yrot, prevYrot, model->frameFrac);

            if (rwdata->yrotBlend == 0.0f) {
                rwdata->nextYrot = rwdata->frameYrot + angleDelta;

                if (rwdata->nextYrot >= M_TAU_F) {
                    rwdata->nextYrot -= M_TAU_F;
                }
            }

            rwdata->nextValid = 1;
        }

        if (hadNoAnim) {
            rwdata->framePos.y = rwdata->nextPos.y;
        }
    }
}


void modelSetAnimationWithMerge(Model *model, ModelAnimation *modelAnimation, s32 flip, f32 startframe, f32 speed, f32 timemerge, s32 domerge) {
    if (domerge != 0) {
        modelCopyAnimForMerge(model, timemerge);
    }
    modelSetAnimation2(model, modelAnimation, flip, startframe, speed, timemerge);
}


void modelSetAnimation(Model *model, ModelAnimation *modelAnimation, s32 flip, f32 startframe, f32 speed, f32 merge) {
    modelCopyAnimForMerge(model, merge);
    modelSetAnimation2(model, modelAnimation, flip, startframe, speed, merge);
}


/*
 * Match-only overlay types for modelCopyAnimState.
 */
typedef struct ModelCopyHead {
    u32 words[8];      // 0x00-0x1f
} ModelCopyHead;


typedef struct ModelCopyBc {
    u32 words[0x2f];   // 0x00-0xbb
} ModelCopyBc;


void modelSetAnimLooping(Model *model, f32 loopframe, f32 loopmerge) {
    model->animlooping = 1;
    model->animloopframe = loopframe;
    model->animloopmerge = loopmerge;
}


void modelSetAnimEndFrame(Model *model, f32 endframe) {
    ModelAnimation *modelAnimation = model->anim;

    if ((modelAnimation != NULL) && (endframe < (modelAnimation->numFrames - 1))) {
        model->endframe = endframe;
    } else {
        model->endframe = -1.0f;
    }
}

void modelSetAnimFlipFunction(Model *model, void (*callback)(void)) {
    model->animflipfunc = callback;
}


void modelSetAnimSpeed(Model *model, f32 anim_speed, f32 startframe) {

    if (startframe > 0.0f) {
        model->timespeed = startframe;
        model->newspeed = anim_speed;
        model->elapsespeed = 0.0f;
        model->oldspeed = model->speed;
        return;
    }

    model->speed = anim_speed;
    model->timespeed = 0.0f;
}

/**
 * Picks a speed that reaches targetframe in duration ticks (which must be non-zero),
 * wrapping around the end of the animation if needed.
 */
void modelSetAnimSpeedToReachFrame(Model *model, f32 targetframe, f32 duration)
{
    f32 frame;
    f32 dist;
    f32 t;

    frame = model->frame;

    if (frame <= targetframe)
    {
        dist = targetframe - frame;
    }
    else
    {
        dist = ( (f32)model->anim->numFrames - frame) + targetframe;
    }

    t = model->speed + ((2.0f * dist) / duration);
    modelSetAnimSpeed(model, t, duration);
}

void modelSetAnimPlaySpeed(Model *model, f32 g_ChrAnimRate, f32 startframe) {
    if (startframe > 0.0f) {
        model->timeplayspeed = startframe;
        model->newplayspeed = g_ChrAnimRate;
        model->elapseplayspeed = 0.0f;
        model->oldplayspeed = model->playspeed;
        return;
    }
    model->playspeed = g_ChrAnimRate;
    model->timeplayspeed = 0.0f;
}


void modelSetRootMoveCallback(Model *model, s32 callback) {
    model->rootMoveCallback = callback;
}


void modelSetAnimFrame(Model* model, f32 frame)
{
    s32 framea;
    s32 frameb;
    bool forwards;

    framea = mathFloorToInt(frame);

    forwards = (model->speed >= 0);
    frameb = (forwards ? framea + 1 : framea - 1);

    model->framea = modelConstrainOrWrapAnimFrame(framea, model->anim, model->endframe);
    model->frameb = modelConstrainOrWrapAnimFrame(frameb, model->anim, model->endframe);

    if (model->framea == model->frameb)
    {
        model->frameFrac = 0.0f;
        model->frame = model->framea;
    }
    else if (forwards)
    {
        f32 tmp = frame - framea;
        model->frameFrac = tmp;
        model->frame = model->framea + tmp;
    }
    else
    {
        f32 tmp = 1.0f - (frame - (f32) frameb);
        model->frameFrac = tmp;
        model->frame = model->frameb + (1.0f - tmp);
    }
}


void modelSetAnimFrame2(Model* model, f32 frame1, f32 frame2)
{
    s32 framea;
    s32 frameb;
    bool forwards;

    modelSetAnimFrame(model, frame1);

    if (model->anim2 != NULL)
    {
        framea = mathFloorToInt(frame2);

        forwards = (model->speed2 >= 0.0f);
        frameb = forwards ? (framea + 1) : (framea - 1);

        model->frame2a = modelConstrainOrWrapAnimFrame(framea, model->anim2, model->endframe2);
        model->frame2b = modelConstrainOrWrapAnimFrame(frameb, model->anim2, model->endframe2);

        if (model->frame2a == model->frame2b)
        {
            model->frame2Frac = 0.0f;
            model->frame2 = model->frame2a;
        }
        else if (forwards != 0)
        {
            f32 tmp = frame2 - framea;
            model->frame2Frac = tmp;
            model->frame2 = model->frame2a + tmp;
        }
        else
        {
            f32 tmp = 1.0f - (frame2 - (f32) frameb);
            model->frame2Frac = tmp;
            model->frame2 = model->frame2b + (1.0f - tmp);
        }
    }
}


void modelSetAnimMergingEnabled(s32 enabled)
{
    g_ModelAnimMergingEnabled = enabled;
}


u32 modelIsAnimMergingEnabled(void)
{
    return g_ModelAnimMergingEnabled;
}


void modelSetAnimFrame2WithChrStuff(Model *model, f32 framea, f32 frameb, f32 frame2a, f32 frame2b)
{
    Model *modelptr;
    ModelRwData_HeaderRecord *header;
    ModelNode *root;
    ModelNode *node;
    s32 jointnum;
    ModelSkeleton *skeleton;
    f32 scale;
    s32 curframe;
    f32 speed;
    coord3d pos;
    f32 frameangle;
    f32 cosangle;
    s32 vb;
    s32 endframe;
    coord3d framepos;
    s32 forward;
    coord3d nextpos;
    f32 nextangle;
    s32 nextvalid;
    f32 angledelta;
    f32 sinangle;
    f32 speed2;
    f32 v[3];
    s32 framenum;
    s32 va;
    f32 inv;
    s32 end2;

    modelptr = model;
    root = modelptr->obj->RootNode;
    va = root->Opcode;

    if ((va & 0xff) == 1)
    {
        node = (ModelNode *) root->Data;
        header = &modelGetNodeRwData(modelptr, root)->Header;

        if (header->frozen == 0)
        {
            jointnum = node->Opcode;
            skeleton = modelptr->obj->Skeleton;
            scale = modelptr->scale * modelptr->anim_translation_scale;
            pos = (coord3d){0};

            framepos.f[0] = header->framePos.x;
            framepos.f[1] = header->framePos.y;
            framepos.f[2] = header->framePos.z;
            frameangle = header->frameYrot;
            nextpos.f[0] = header->nextPos.x;
            nextpos.f[1] = header->nextPos.y;
            nextpos.f[2] = header->nextPos.z;
            nextangle = header->nextYrot;
            nextvalid = header->nextValid;

            speed = modelptr->speed;

            if (speed < 0.0f)
            {
                speed = -speed;
            }

            speed2 = modelptr->speed2;

            if (speed2 < 0.0f)
            {
                speed2 = -speed2;
            }

            forward = 0;

            if (framea <= frameb)
            {
                forward = 1;
            }

            if (forward)
            {
                curframe = mathFloorToInt(framea) + 1;
                endframe = mathFloorToInt(frameb);
            }
            else
            {
                curframe = mathCeilToInt(framea) - 1;
                endframe = mathCeilToInt(frameb);
            }

            while (1)
            {
                if (forward)
                {
                    if (endframe < curframe)
                    {
                        break;
                    }
                }
                else
                {
                    if (curframe < endframe)
                    {
                        break;
                    }
                }

                framenum = modelConstrainOrWrapAnimFrame(curframe, modelptr->anim, modelptr->endframe);
                modelptr->framea = framenum;

                if (nextvalid)
                {
                    framepos.f[0] = nextpos.f[0];
                    framepos.f[1] = nextpos.f[1];
                    framepos.f[2] = nextpos.f[2];

                    if (header->yrotBlend == 0.0f)
                    {
                        frameangle = nextangle;
                    }
                }
                else
                {
                    angledelta = animReadRootMotion(jointnum, modelptr->flip, skeleton, modelptr->anim, framenum, &pos);

                    if (scale != 1.0f)
                    {
                        pos.x *= scale;
                        pos.y *= scale;
                        pos.z *= scale;
                    }

                    if (!forward)
                    {
                        pos.x = -pos.x;
                        pos.z = -pos.z;

                        if (0.0f < angledelta)
                        {
                            angledelta = M_TAU_F - angledelta;
                        }
                    }

                    cosangle = cosf(header->yrot);
                    sinangle = sinf(header->yrot);

                    framepos.f[0] += (pos.x * cosangle) + (pos.z * sinangle);
                    framepos.f[1] = pos.y;
                    framepos.f[2] += (-pos.x * sinangle) + (pos.z * cosangle);

                    if (header->yrotBlend == 0.0f)
                    {
                        frameangle += angledelta;

                        if (M_TAU_F <= frameangle)
                        {
                            frameangle -= M_TAU_F;
                        }
                    }
                }

                if (forward)
                {
                    curframe += 1;
                }
                else
                {
                    curframe -= 1;
                }

                framenum = modelConstrainOrWrapAnimFrame(curframe, modelptr->anim, modelptr->endframe);
                modelptr->frameb = framenum;

                if (modelptr->frameb != modelptr->framea)
                {
                    angledelta = animReadRootMotion(jointnum, modelptr->flip, skeleton, modelptr->anim, framenum, &pos);
                    nextvalid = 1;

                    if (scale != 1.0f)
                    {
                        pos.x *= scale;
                        pos.y *= scale;
                        pos.z *= scale;
                    }

                    if (!forward)
                    {
                        pos.x = -pos.x;
                        pos.z = -pos.z;

                        if (0.0f < angledelta)
                        {
                            angledelta = M_TAU_F - angledelta;
                        }
                    }

                    cosangle = cosf(header->frameYrot);
                    sinangle = sinf(header->frameYrot);

                    if (g_ModelAnimMergingEnabled && modelptr->anim2 != NULL)
                    {
                        nextpos.f[0] = (pos.z * sinangle) + (pos.x * cosangle);
                        nextpos.f[2] = (pos.z * cosangle) + (-pos.x * sinangle);

                        if (0.0f < speed)
                        {
                            f32 t = modelptr->mergeWeight - (modelptr->playspeed / (speed * modelptr->mergeTime));

                            if (t < 0.0f)
                            {
                                t = 0.0f;
                            }

                            t = (modelptr->mergeWeight + t) * 0.5f;

                            v[0] = ((header->anim2NextPos.x - header->anim2FramePos.x) * speed2) / speed;
                            v[2] = ((header->anim2NextPos.z - header->anim2FramePos.z) * speed2) / speed;

                            nextpos.f[0] += (v[0] - nextpos.f[0]) * t;
                            nextpos.f[2] += (v[2] - nextpos.f[2]) * t;
                        }
                        else
                        {
                            nextpos.f[0] += (header->anim2NextPos.x - header->anim2FramePos.x) * modelptr->mergeWeight;
                            nextpos.f[2] += (header->anim2NextPos.z - header->anim2FramePos.z) * modelptr->mergeWeight;
                        }

                        nextpos.f[0] += framepos.f[0];
                        nextpos.f[2] += framepos.f[2];
                        nextpos.f[1] = pos.y;
                    }
                    else
                    {
                        nextpos.f[0] = (framepos.f[0] + (pos.f[0] * cosangle)) + (pos.f[2] * sinangle);
                        nextpos.f[1] = pos.f[1];
                        nextpos.f[2] = (framepos.f[2] - (pos.f[0] * sinangle)) + (pos.f[2] * cosangle);
                    }

                    if (0.0f < header->turnFrames)
                    {
                        if (0.0f < speed)
                        {
                            inv = 1.0f / speed;

                            if (header->turnFrames < inv)
                            {
                                inv = header->turnFrames;
                                header->turnFrames = 0.0f;
                            }
                            else
                            {
                                header->turnFrames -= inv;
                            }

                            angledelta += header->turnPerFrame * inv;

                            if (angledelta < 0.0f)
                            {
                                angledelta += M_TAU_F;
                            }
                            else if (M_TAU_F <= angledelta)
                            {
                                angledelta -= M_TAU_F;
                            }
                        }
                    }

                    if (header->yrotBlend == 0.0f)
                    {
                        nextangle = frameangle + angledelta;

                        if (M_TAU_F <= nextangle)
                        {
                            nextangle -= M_TAU_F;
                        }
                    }
                }
            }

            header->framePos.x = framepos.f[0];
            header->framePos.y = framepos.f[1];
            header->framePos.z = framepos.f[2];
            header->frameYrot = frameangle;
            header->nextPos.x = nextpos.f[0];
            header->nextPos.y = nextpos.f[1];
            header->nextPos.z = nextpos.f[2];
            header->nextYrot = nextangle;

            va = modelptr->framea;
            vb = modelptr->frameb;

            if (vb == va)
            {
                modelptr->frameFrac = 0.0f;
                modelptr->frame = va;
            }
            else if (forward)
            {
                modelptr->frameFrac = frameb - (f32)endframe;
                modelptr->frame = (f32)va + modelptr->frameFrac;
            }
            else
            {
                modelptr->frameFrac = (f32)endframe - frameb;
                modelptr->frame = (f32)vb + (1.0f - modelptr->frameFrac);
            }

            if (modelptr->anim2 != NULL)
            {
                curframe = mathFloorToInt(frame2a);
                end2 = mathFloorToInt(frame2b);

                if ((forward && (curframe < end2)) || (!forward && (end2 < curframe)))
                {
                    if (header->anim2NextValid)
                    {
                        header->anim2FramePos.y = header->anim2NextPos.y;
                    }
                    else
                    {
                        header->anim2FramePos.y = header->framePos.y;
                    }

                    modelptr->frame2a = modelConstrainOrWrapAnimFrame(end2, modelptr->anim2, modelptr->endframe2);
                    framenum = modelConstrainOrWrapAnimFrame(end2 + 1, modelptr->anim2, modelptr->endframe2);
                    modelptr->frame2b = framenum;

                    animReadRootMotion(jointnum, modelptr->flip2, skeleton, modelptr->anim2, framenum, &pos);

                    if (scale != 1.0f)
                    {
                        pos.y *= scale;
                    }

                    header->anim2NextPos.y = pos.y;
                    header->anim2NextValid = 1;
                }

                if (forward)
                {
                    modelptr->frame2Frac = frame2b - (f32)end2;
                    modelptr->frame2 = (f32)modelptr->frame2a + modelptr->frame2Frac;
                }
                else
                {
                    modelptr->frame2Frac = 1.0f - (frame2b - (f32)end2);
                    modelptr->frame2 = (f32)modelptr->frame2b + (1.0f - modelptr->frame2Frac);
                }
            }
            else
            {
                header->anim2NextValid = 0;
            }
        }
        else
        {
            modelSetAnimFrame2(modelptr, frameb, frame2b);
        }
    }
    else
    {
        modelSetAnimFrame2(modelptr, frameb, frame2b);
    }
}


void modelTickAnim(struct Model *model, s32 numticks, s32 update_chrstuff)
{
    f32 frame;
    f32 frame2;
    f32 animlast;

    frame = model->frame;
    frame2 = model->frame2;

    if (numticks > 0) 
    {
        while (numticks > 0) 
        {
            f32 playspeed;
            f32 speed;
            f32 limit;
            f32 endframe;
            f32 saved_newspeed;
            f32 saved_oldspeed;
            f32 saved_timespeed;
            f32 saved_elapsespeed;
            f32 loopframe;
            
            if (model->timeplayspeed > 0.0f) 
            {
                model->elapseplayspeed += 1.0f;

                if (model->elapseplayspeed < model->timeplayspeed) 
                {
                    model->playspeed = model->oldplayspeed + ((model->newplayspeed - model->oldplayspeed) * model->elapseplayspeed) / model->timeplayspeed;
                } 
                else 
                {
                    model->timeplayspeed = 0.0f;
                    model->playspeed = model->newplayspeed;
                }
            }

            playspeed = model->playspeed;

            if (model->mergeTime > 0.0f)
            {
                model->mergeElapsed += playspeed;

                if (model->mergeElapsed == 0.0f)
                {
                    model->mergeWeight = 1.0f;
                    playspeed = model->playspeed;
                } 
                else if (model->mergeElapsed < model->mergeTime) 
                {
                    model->mergeWeight = (model->mergeTime - model->mergeElapsed) / model->mergeTime;
                    playspeed = model->playspeed;
                } 
                else 
                {
                    model->mergeTime = 0.0f;
                    model->mergeWeight = 0.0f;
                    model->anim2 = NULL;
                    playspeed = model->playspeed;
                }
            }

            if (model->timespeed > 0.0f) 
            {
                model->elapsespeed += playspeed;

                if (model->elapsespeed < model->timespeed) 
                {
                    model->speed = model->oldspeed
                        + ((model->newspeed - model->oldspeed) * model->elapsespeed)
                        / model->timespeed;
                    playspeed = model->playspeed;
                } 
                else 
                {
                    model->timespeed = 0.0f;
                    playspeed = model->playspeed;
                    model->speed = model->newspeed;
                }
            }

            speed = model->speed;
            frame += playspeed * speed;

            if (model->anim2 != NULL) 
            {
                if (model->timespeed2 > 0.0f) 
                {
                    model->elapsespeed2 += playspeed;

                    if (model->elapsespeed2 < model->timespeed2) 
                    {
                        model->speed2 = model->oldspeed2 + ((model->newspeed2 - model->oldspeed2) * model->elapsespeed2) / model->timespeed2;
                        playspeed = model->playspeed;
                    } 
                    else 
                    {
                        model->timespeed2 = 0.0f;
                        playspeed = model->playspeed;
                        model->speed2 = model->newspeed2;
                    }
                }


                frame2 += playspeed * model->speed2;
            }

            if (model->animlooping) 
            {
                animlast = model->anim->numFrames - 1;
                endframe = model->endframe;


                if (speed >= 0.0f) 
                {
                    limit = animlast;
                    loopframe = model->animloopframe;

                    if (endframe >= 0.0f && endframe < animlast) 
                    {
                        limit = endframe;
                    }
                } 
                else 
                {
                    limit = model->animloopframe;
                    loopframe = animlast;

                    if (endframe >= 0.0f && endframe < animlast) 
                    {
                        loopframe = endframe;
                    }
                }

                if ((speed >= 0.0f && frame >= limit) || (speed < 0.0f && frame <= limit)) 
                {
                    saved_newspeed = model->newspeed;
                    saved_oldspeed = model->oldspeed;
                    saved_timespeed = model->timespeed;
                    saved_elapsespeed = model->elapsespeed;

                    if (update_chrstuff) 
                    {
                        modelSetAnimFrame2WithChrStuff(model, model->frame, limit, 0.0f, 0.0f);
                    } 
                    else 
                    {
                        modelSetAnimFrame2(model, limit, 0.0f);
                    }

                    modelSetAnimation(model, model->anim, model->flip, loopframe, model->speed, model->animloopmerge);

                    model->animlooping = 1;
                    model->endframe = endframe;
                    model->newspeed = saved_newspeed;
                    model->oldspeed = saved_oldspeed;
                    model->timespeed = saved_timespeed;
                    model->elapsespeed = saved_elapsespeed;

                    frame2 = frame;
                    frame = loopframe + frame - limit;

                    if (model->animflipfunc != NULL) 
                    {
                        model->animflipfunc();
                    }
                }
            }

            numticks--;
        }

        if (update_chrstuff) 
        {
            if (model->anim2 != NULL) 
            {
                modelSetAnimFrame2WithChrStuff(model, model->frame, frame, model->frame2, frame2);
            } 
            else 
            {
                modelSetAnimFrame2WithChrStuff(model, model->frame, frame, 0.0f, 0.0f);
            }
        } 
        else 
        {
            if (model->anim2 != NULL) 
            {
                modelSetAnimFrame2(model, frame, frame2);
            }
            else 
            {
                modelSetAnimFrame2(model, frame, 0.0f);
            }
        }
    }
}


/**
 * @brief Model Type 1: 1Cycle No Secondary
 * @param[in,out] renderdata append cycle, CC and RM to display List
 */
void modelApplyRenderModeType1(ModelRenderData *renderdata)
{
    gDPPipeSync(renderdata->gdl++);
    gDPSetCycleType(renderdata->gdl++, G_CYC_1CYCLE);

    if (renderdata->zbufferenabled)
    {
        gDPSetRenderMode(renderdata->gdl++, G_RM_AA_ZB_OPA_SURF2, G_RM_AA_ZB_OPA_SURF);
    }
    else
    {
        gDPSetRenderMode(renderdata->gdl++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
    }

    gDPSetCombineMode(renderdata->gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
}

/**
 * @brief Model Type 3: GunLighting - Reduced Secondary Commands (guns)
    This Type Uses Vertex Alpha for Secondary Surfaces and uses the FOG Alpha value for applying Fog/"Lighting".
 * @param renderdata
 * @param isPrimary
 */
void modelApplyRenderModeType3(ModelRenderData *renderdata, bool isPrimary)
{
    if (renderdata->PropType == PROP_TYPE_VIEWER+1)
    {
        if (isPrimary)
        {
            u8 r, g, b, a;
            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

            r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);

            r = _SHIFTR(renderdata->envcolour.word, 24, 8);
            g = _SHIFTR(renderdata->envcolour.word, 16, 8);
            b = _SHIFTR(renderdata->envcolour.word, 8, 8);
            a = 0xFF;
            gDPSetEnvColor(renderdata->gdl++, r, g, b, a);

            gDPSetCombineLERP(renderdata->gdl++, TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT, TEXEL0, ENVIRONMENT, SHADE, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_EXPLOSION+1)
    {
        if (isPrimary)
        {
            u8 r, g, b, a;
            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

            r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);

            r = _SHIFTR(renderdata->envcolour.word, 24, 8);
            g = _SHIFTR(renderdata->envcolour.word, 16, 8);
            b = _SHIFTR(renderdata->envcolour.word, 8, 8);
            a = _SHIFTR(renderdata->envcolour.word, 0, 8);
            gDPSetEnvColor(renderdata->gdl++, r, g, b, a);

            gDPSetCombineLERP(renderdata->gdl++, TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT, TEXEL0, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_SMOKE+1)
    {
        if ((renderdata->envcolour.word & 0xFF) == 0)
        {
            if (isPrimary)
            {
                u8 r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
                u8 g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
                u8 b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
                u8 a = _SHIFTR(renderdata->fogcolour.word, 0, 8);

                gDPPipeSync(renderdata->gdl++);
                gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
                gDPSetFogColor(renderdata->gdl++, r, g, b, a);
                gDPSetEnvColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, 0xFF);
                gDPSetPrimColor(renderdata->gdl++, 0, 0, 0, 0, 0, (renderdata->envcolour.word >> 8) & 0xFF);

                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, PRIMITIVE);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
                }
            }
            else
            {
                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
                }
            }
        }
        else
        {
            if (isPrimary)
            {
                u8 r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
                u8 g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
                u8 b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
                u8 a = _SHIFTR(renderdata->fogcolour.word, 0, 8);

                gDPPipeSync(renderdata->gdl++);
                gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
                gDPSetFogColor(renderdata->gdl++, r, g, b, a);
                gDPSetEnvColor(renderdata->gdl++, 0, 0, 0, renderdata->envcolour.word & 0xFF);

                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, SHADE, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_TEX_EDGE2);
                }
            }
            else
            {
                gDPSetPrimColor(renderdata->gdl++, 0, 0, 0, 0, 0, (renderdata->envcolour.word >> 8) & 0xFF);
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, SHADE, ENVIRONMENT, TEXEL0, 0, COMBINED, 0, SHADE, 0, 1, 0, PRIMITIVE, COMBINED);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_TEX_EDGE2);
                }
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_CHR+1)
    {
        if (isPrimary)
        {
            u8 r = _SHIFTR(renderdata->envcolour.word, 24, 8);
            u8 g = _SHIFTR(renderdata->envcolour.word, 16, 8);
            u8 b = _SHIFTR(renderdata->envcolour.word, 8, 8);
            u8 a = _SHIFTR(renderdata->envcolour.word, 0, 8);

            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);

            gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_WEAPON+1)
    {
        u8 r, g, b, a;
        if (isPrimary)
        {
            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

            r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);

            a = renderdata->envcolour.word & 0xFF;

            if (a < 255)
            {
                gDPSetEnvColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, a);

                if (renderdata->envcolour.word & 0xFF00)
                {
                    gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, SHADE, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
                }
                else
                {
                    gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
                }
            }
            else
            {
                gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
            }

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
        else
        {
            a = renderdata->envcolour.word & 0xFF;

            if (a < 255)
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL0, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
            }
            else
            {
                gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
            }
        }
    }
    else
    {
        if (isPrimary)
        {
            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
            gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_XLU_SURF2);
            }
        }
    }
}

/**
 * @brief Model Type 4: Normal Fog/Lighting object
    This Type Uses Vertex Alpha for Secondary Surfaces and uses the FOG Alpha value for applying Fog/"Lighting".
 * @param renderdata
 * @param isPrimary Type of DisplayList
 */
void modelApplyRenderModeType4(ModelRenderData *renderdata, bool isPrimary)
{
    if (renderdata->PropType == PROP_TYPE_VIEWER+1)
    {
        u8 r, g, b, a;
        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

        r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
        g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
        b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
        a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
        gDPSetFogColor(renderdata->gdl++, r, g, b, a);

        r = _SHIFTR(renderdata->envcolour.word, 24, 8);
        g = _SHIFTR(renderdata->envcolour.word, 16, 8);
        b = _SHIFTR(renderdata->envcolour.word, 8, 8);
        a = 0xFF;
        gDPSetEnvColor(renderdata->gdl++, r, g, b, a);

        gDPSetCombineLERP(renderdata->gdl++, TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT, TEXEL0, ENVIRONMENT, SHADE, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

        if (isPrimary)
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_EXPLOSION+1)
    {
        u8 r, g, b, a;
        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

        r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
        g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
        b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
        a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
        gDPSetFogColor(renderdata->gdl++, r, g, b, a);

        r = _SHIFTR(renderdata->envcolour.word, 24, 8);
        g = _SHIFTR(renderdata->envcolour.word, 16, 8);
        b = _SHIFTR(renderdata->envcolour.word, 8, 8);
        a = _SHIFTR(renderdata->envcolour.word, 0, 8);
        gDPSetEnvColor(renderdata->gdl++, r, g, b, a);

        gDPSetCombineLERP(renderdata->gdl++, TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT, TEXEL0, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

        if (renderdata->zbufferenabled)
        {
            gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
        }
        else
        {
            gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
        }
    }
    else if (renderdata->PropType == PROP_TYPE_SMOKE+1)
    {
        if ((renderdata->envcolour.word & 0xFF) == 0)
        {
            u8 r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            u8 g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            u8 b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            u8 a = _SHIFTR(renderdata->fogcolour.word, 0, 8);

            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);
            gDPSetEnvColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, 0xFF);
            gDPSetPrimColor(renderdata->gdl++, 0, 0, 0, 0, 0, ((renderdata->envcolour.word >> 8 ) & 0xFF));

            if (isPrimary)
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, PRIMITIVE);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
                }
            }
            else
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, PRIMITIVE);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
                }
            }
        }
        else
        {
            u8 r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            u8 g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            u8 b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            u8 a = _SHIFTR(renderdata->fogcolour.word, 0, 8);

            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);
            gDPSetEnvColor(renderdata->gdl++, 0, 0, 0, renderdata->envcolour.word & 0xFF);

            if (isPrimary)
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, SHADE, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_TEX_EDGE2);
                }
            }
            else
            {
                gDPSetPrimColor(renderdata->gdl++, 0, 0, 0, 0, 0, (renderdata->envcolour.word >> 8) & 0xFF);
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, SHADE, ENVIRONMENT, TEXEL0, 0, COMBINED, 0, SHADE, 0, 1, 0, PRIMITIVE, COMBINED);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_TEX_EDGE2);
                }
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_CHR+1)
    {
        u8 r = _SHIFTR(renderdata->envcolour.word, 24, 8);
        u8 g = _SHIFTR(renderdata->envcolour.word, 16, 8);
        u8 b = _SHIFTR(renderdata->envcolour.word, 8, 8);
        u8 a = _SHIFTR(renderdata->envcolour.word, 0, 8);

        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
        gDPSetFogColor(renderdata->gdl++, r, g, b, a);

        gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);

        if (isPrimary)
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_WEAPON+1)
    {
        u8 r, g, b, a;

        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

        r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
        g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
        b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
        a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
        gDPSetFogColor(renderdata->gdl++, r, g, b, a);

        a = renderdata->envcolour.word & 0xFF;

        if (a < 255)
        {
            gDPSetEnvColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, a);

            if (isPrimary)
            {
                if (renderdata->envcolour.word & 0xFF00) //apply inverse vertex alpha if any
                {
                    gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, SHADE, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
                }
                else
                {
                    gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
                }
            }
            else
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL0, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
            }
        }
        else
        {
            gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
        }

        if (renderdata->zbufferenabled)
        {
            gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
        }
        else
        {
            gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
        }
    }
    else
    {
        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
        gDPSetFogColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, 0x00);
        gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);

        if (isPrimary)
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
}

/**
 * @brief Model Type 2: 2Cycle No Secondary
 * @param[in,out] renderdata append cycle, CC and RM to display List
 */
void modelApplyRenderModeType2(ModelRenderData *renderdata)
{
    gDPPipeSync(renderdata->gdl++);
    gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

    if (renderdata->zbufferenabled)
    {
        gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
    }
    else
    {
        gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_OPA_SURF2);
    }

    gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
}


void modelApplyCullMode(ModelRenderData *renderdata)
{
    if (renderdata->cullmode == CULLMODE_NONE)
    {
        gSPClearGeometryMode(renderdata->gdl++, G_CULL_BOTH);
    }
    else if (renderdata->cullmode == CULLMODE_FRONT)
    {
        gSPSetGeometryMode(renderdata->gdl++, G_CULL_FRONT);
    }
    else if (renderdata->cullmode == CULLMODE_BACK)
    {
        gSPSetGeometryMode(renderdata->gdl++, G_CULL_BACK);
    }
}


void modelRenderNodeGundl(ModelRenderData* renderdata, ModelNode* node)
{
    ModelRoData_DisplayListRecord* rodata = &node->Data->DisplayList;

    if (renderdata->skipDraw == 0)
    {
        if ((renderdata->flags & 1) && rodata->Primary)
        {
            gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

            if (renderdata->cullmode)
            {
                modelApplyCullMode(renderdata);
            }

            if (rodata->ModelType == 1)
            {
                modelApplyRenderModeType1(renderdata);
            }
            else if (rodata->ModelType == 3)
            {
                modelApplyRenderModeType3(renderdata, 1);
            }
            else if (rodata->ModelType == 4)
            {
                modelApplyRenderModeType4(renderdata, 1);
            }
            else if (rodata->ModelType == 2)
            {
                modelApplyRenderModeType2(renderdata);
            }

            gSPDisplayList(renderdata->gdl++, rodata->Primary);

            if ((rodata->ModelType == 3) && rodata->Secondary)
            {
                modelApplyRenderModeType3(renderdata, 0);
                gSPDisplayList(renderdata->gdl++, rodata->Secondary);
            }
        }

        if ((renderdata->flags & 2) && rodata->Primary && (rodata->ModelType == 4) && rodata->Secondary)
        {
            gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

            if (renderdata->cullmode)
            {
                modelApplyCullMode(renderdata);
            }

            modelApplyRenderModeType4(renderdata, 0);
            gSPDisplayList(renderdata->gdl++, rodata->Secondary);
        }
    }
}

/*

A1 is primary = 1, secondary = 0
Inside the T8 or whatever temporary register indicates gun or not gun (0 = gun, or UseZ = 1), for different render mode

Bool UseZ //guns = false
Bool


Model Type 0: NoSetup.
    type 0 Has No DL Setup and will use whaterver is currently set.

Model Type 1: 1Cycle No Sec
    E700000000000000 pipesync()
    BA00140200000000 CycleType(1c)
    if UseZ
      B900031D00552078 SetRendermode(AA_ZB_OPA_1) //cin ain cmem amem
    else
      B900031D00552048 SetRendermode(AA_OPA_1) //cin ain cmem amem
    end if
    FC121824FF33FFFF SetCombine(MODULATERGBA)
    No Secondary

Model Type 2: 2Cycle No Sec
    E700000000000000 pipesync()
    BA00140200100000 CycleType(2c)
    if UseZ
      B900031D0C192078 SetRendermode(AA_ZB_OPA_2) // cin 0 cin 1 //colour only
    else
      B900031D0C192048 SetRendermode(AA_OPA_2) // cin 0 cin 1 //colour only
    end if
    FC26A0041F1093FF SetCombine(TRILERP, MODULATERGBA)
    No Secondary
Model Type 3: GunLighting - Reduced Secondary Commands (guns)
    This Type Uses Vertex Alpha for Secondary Surfaces and uses the FOG Alpha value for applying Fog/"Lighting".
    Primary
    E700000000000000 pipesync()
    BA00140200100000 CycleType(2c)
    F800000000000026 SetFogColor(0,0,0,38)
    if dltype = full
      if guard
        FB0000005A0000FF SetEnvColor(90,0,0,255)
        FC1598045FFEDBF8 SetCombine(((Texel0-Env)*ShadeA+Env)
                         ((Texel0-Env)*Shade+Env),
                         MODULATERGB_DECALA)
      else if prop
        FB000000FFFFFFFF SetEnvColor(255,255,255,255)
        FA00000000000000 SetPrimColor(0,0,0,0)
        FC26A0041F1093FB SetCombine(TRILERP, MODULATERGB_ADDPRIM_A)
      endif
    else
      FC26A0041F1093FF SetCombine(TRILERP, MODULATERGBA)
    endif
    if UseZ
      B900031DC4112078 SetRendermode(AA_ZB_OPA_StanFOG_2)
    else
      B900031DC4112048 SetRendermode(AA_OPA_StanFOG_2) //acvg
    endif

    Secondary
    if UseZ
      B900031DC41049D8 SetRendermode(AA_Zcmp_XLU_StanFOG_2)
    else
      B900031DC41041C8 SetRendermode(AA_OPA_StanFOG_2)//FcBl ClrOnCvg
    endif

Model Type 4: Normal Fog/Lighting object
    This Type Uses Vertex Alpha for Secondary Surfaces and uses the FOG Alpha value for applying Fog/"Lighting".
    Primary
    E700000000000000 pipesync()
    BA00140200100000 CycleType(2c)
    F800000000000026 SetFogColor(0,0,0,38)
    if dltype = full
      if guard
        FB0000005A0000FF SetEnvColor(90,0,0,255)
        FC1598045FFEDBF8 SetCombine(((Texel0-Env)*ShadeA+Env)
                         ((Texel0-Env)*Shade+Env),
                         MODULATERGB_DECALA)
      else if prop
        FB000000FFFFFFFF SetEnvColor(255,255,255,255)
        FA00000000000000 SetPrimColor(0,0,0,0)
        FC26A0041F1093FB SetCombine(TRILERP, MODULATERGB_ADDPRIM_A)
      endif
    else
      FC26A0041F1093FF SetCombine(TRILERP, MODULATERGBA)
    endif
    if UseZ
      B900031DC4112078 SetRendermode(AA_ZB_OPA_StanFOG_2)
    else
      B900031DC4112048 SetRendermode(AA_OPA_StanFOG_2) //acvg
    endif

    Secondary
    E700000000000000 pipesync()
    BA00140200100000 CycleType(2c)
    F800000000000026 SetFogColor(0,0,0,38)
    if dltype = full
      if guard
        FB0000005A0000FF SetEnvColor(90,0,0,255)
        FC1598045FFEDBF8 SetCombine(((Texel0-Env)*ShadeA+Env)
                         ((Texel0-Env)*Shade+Env),
                         MODULATERGB_DECALA)
      else if prop
        FB000000FFFFFFFF SetEnvColor(255,255,255,255)
        FA00000000000000 SetPrimColor(0,0,0,0)
        FC26A0041F1093FB SetCombine(TRILERP, MODULATERGB_ADDPRIM_A)
      endif
    else
      FA00000000000000 SetPrimColor(0,0,0,0)
      FC26A0041F1093FB SetCombine(TRILERP, MODULATERGB_ADDPRIM_A)
    endif
    if UseZ
      B900031DC41049D8 SetRendermode(AA_Zcmp_XLU_StanFOG_2)
    else
      B900031DC41041C8 SetRendermode(AA_OPA_StanFOG_2)//FcBl ClrOnCvg
    endif
*/

/**
* 7F072A0C
* DisplayList Setups Depend on Object Type, Prop Guard or Gun.
These are applied to each part of an object at runtime and can be overridden. loading the next part will use these values once more.
GeometryMode is not in setup and is persistent accross parts.
*/
void modelRenderNodeDl(ModelRenderData *renderdata, Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;

    if (!renderdata->skipDraw)
    {
        if (renderdata->flags & 1)
        {
            union ModelRwData *rwdata = modelGetNodeRwData(model, node);

            if (rwdata->DisplayListCollisions.gdl)
            {
                gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->DisplayListCollisions.BaseAddr));

                if (renderdata->cullmode)
                {
                    modelApplyCullMode(renderdata);
                }

                if (rodata->DisplayListCollisions.ModelType == 1)
                {
                    modelApplyRenderModeType1(renderdata);
                }
                else if (rodata->DisplayListCollisions.ModelType == 3)
                {
                    modelApplyRenderModeType3(renderdata, TRUE);
                }
                else if (rodata->DisplayListCollisions.ModelType == 4)
                {
                    modelApplyRenderModeType4(renderdata, TRUE);
                }
                else if (rodata->DisplayListCollisions.ModelType == 2)
                {
                    modelApplyRenderModeType2(renderdata);
                }

                gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_VTX, osVirtualToPhysical(rwdata->DisplayListCollisions.Vertices));

                gSPDisplayList(renderdata->gdl++, rwdata->DisplayListCollisions.gdl);

                if (rodata->DisplayListCollisions.ModelType == 3 && rodata->DisplayListCollisions.Secondary)
                {
                    modelApplyRenderModeType3(renderdata, FALSE);
                    gSPDisplayList(renderdata->gdl++, rodata->DisplayListCollisions.Secondary);
                }
            }
        }

        if (renderdata->flags & 2)
        {
            union ModelRwData *rwdata = modelGetNodeRwData(model, node);

            if (rwdata->DisplayListCollisions.gdl && rodata->DisplayListCollisions.ModelType == 4 && rodata->DisplayListCollisions.Secondary)
            {
                gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->DisplayListCollisions.BaseAddr));

                if (renderdata->cullmode)
                {
                    modelApplyCullMode(renderdata);
                }

                gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_VTX, osVirtualToPhysical(rwdata->DisplayListCollisions.Vertices));

                modelApplyRenderModeType4(renderdata, FALSE);

                gSPDisplayList(renderdata->gdl++, rodata->DisplayListCollisions.Secondary);
            }
        }
    }
}




/**
 * Star gunfire is a muzzle flash in a first person perspective, where the
 * muzzle flash has 3 or 4 "arms" that flare out from the main body.
 *
 * This function reads vertices from the model definition, tweaks them randomly,
 * writes them to a newly allocated vertices table and queues the node's
 * displaylist to the renderdata's DL.
 */
void modelRenderNodeRotTex(ModelRenderData *renderdata, ModelNode *node)
{
    if (renderdata->skipDraw == 0 && (renderdata->flags & 2))
    {

        ModelRoData_DisplayListPrimaryRecord *rodata = &node->Data->DisplayListPrimary;
        s32 i;

        if (rodata->Primary)
        {
            Vertex *src;
            Vertex *dst;

            src = (Vertex *) rodata->Vertices;

            dst = g_ModelVtxAllocator(rodata->numVertices * 4);

            gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_VTX, osVirtualToPhysical(dst));
            gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

            gDPSetFogColor(renderdata->gdl++, 0x00, 0x00, 0x00, 0x00);
            gSPDisplayList(renderdata->gdl++, rodata->Primary);

            for (i = 0; i < rodata->numVertices; i++)
            {
                u16 rand1 = (randomGetNext() << 10) & 0xffff;
                s32 s4 = ((coss(rand1) << 5) * 181) >> 18;
                s32 s3 = ((sins(rand1) << 5) * 181) >> 18;
                s32 s1 = (u32)randomGetNext() >> 31;
                s32 mult = 0x10000 - (randomGetNext() & 0x3fff);
                s32 corner1 = 0x200 + s3;
                s32 corner2 = 0x200 - s3;
                s32 corner3 = 0x200 - s4;
                s32 corner4 = 0x200 + s4;

                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = src[3];

                dst[0].s = corner3;
                dst[0].t = corner2;
                dst[0].coord.x = (src[(s1 + 0) % 4].coord.x * mult) >> 16;
                dst[0].coord.y = (src[(s1 + 0) % 4].coord.y * mult) >> 16;
                dst[0].coord.z = (src[(s1 + 0) % 4].coord.z * mult) >> 16;

                dst[1].s = corner1;
                dst[1].t = corner3;
                dst[1].coord.x = (src[(s1 + 1) % 4].coord.x * mult) >> 16;
                dst[1].coord.y = (src[(s1 + 1) % 4].coord.y * mult) >> 16;
                dst[1].coord.z = (src[(s1 + 1) % 4].coord.z * mult) >> 16;

                dst[2].s = corner4;
                dst[2].t = corner1;
                dst[2].coord.x = (src[(s1 + 2) % 4].coord.x * mult) >> 16;
                dst[2].coord.y = (src[(s1 + 2) % 4].coord.y * mult) >> 16;
                dst[2].coord.z = (src[(s1 + 2) % 4].coord.z * mult) >> 16;

                dst[3].s = corner2;
                dst[3].t = corner4;
                dst[3].coord.x = (src[(s1 + 3) % 4].coord.x * mult) >> 16;
                dst[3].coord.y = (src[(s1 + 3) % 4].coord.y * mult) >> 16;
                dst[3].coord.z = (src[(s1 + 3) % 4].coord.z * mult) >> 16;

                src += 4;
                dst += 4;
            }
        }
    }
}


void modelSelectTexture(ModelRenderData *renderdata, struct sImageTableEntry *tconfig, s32 blendMode)
{
    texSelect(&renderdata->gdl, tconfig, blendMode, renderdata->zbufferenabled, 2);
}




void modelRenderNodeTube(ModelRenderData* renderdata, Model* model, ModelNode* node)
{
    s32 rw_index_sel;
    s32 rw_index_sel2;
    s32 c_entry_count;
    struct ModelRoData_TubeRecord *rodata2;
    s32 c_entry2_count;
    s32 c_entry_index;
    s32 renderpos_index;
    ModelNode *node_from_07;
    struct ModelRoData_Child *c_entry2;
    Vertex *vtx2;
    u8 *entry2_04;
    bool swap_order;
    struct ModelRoData_TubeRecord *rodata;
    struct ModelRoData_Child *c_entry;
    struct ModelRwData_TubeRecord *rwdata;
    Vertex *vtx_10;
    Vertex *vtx_10_2;
    RenderPosView *render_pos2;
    RenderPosView *render_pos;
    s32 rw_index2;
    s32 rw_index;
    Vertex *vtx1;
    struct ModelRwData_TubeRecord *rwdata2;
    u8 *entry_04;
    s32 renderpos_index2;

    rodata = &node->Data->Tube;
    rwdata = &modelGetNodeRwData(model, node)->Tube;

    if (rodata->prevTube != NULL)
    {
        node_from_07 = rodata->prevTube;
    }
    else
    {
        node_from_07 = rodata->nextTube;
    }

    rodata2 = &node_from_07->Data->Tube;
    rwdata2 = &modelGetNodeRwData(model, node_from_07)->Tube;
    swap_order = 1;

    if (renderdata->flags & 1)
    {
        renderpos_index2 = modelFindNodeMtxIndex(node, 0);
        render_pos2 = &model->render_pos[renderpos_index2];
        rw_index = rwdata->index;
        rw_index2 = rwdata2->index;
        c_entry = &rodata->Children[rw_index];

        if (rodata->prevTube != NULL)
        {
            rw_index_sel = rw_index2;
            rw_index_sel2 = rw_index;
            renderpos_index = modelFindNodeMtxIndex(node, 0x200);
        }
        else
        {
            rw_index_sel = rw_index;
            rw_index_sel2 = rw_index2;
            renderpos_index = modelFindNodeMtxIndex(rodata->nextTube, 0x200);
        }

        render_pos = &model->render_pos[renderpos_index];

        c_entry_index = ((rw_index_sel2 - rw_index_sel) + rodata->NumChildren) % rodata->NumChildren;

        if ((c_entry_index >= 2) && (c_entry_index < 7))
        {
            if (c_entry_index < 4)
            {
                c_entry_index = ((c_entry_index / 2) + rw_index_sel + rodata->NumChildren) % rodata->NumChildren;
            }
            else
            {
                c_entry_index = ((rw_index_sel - ((8 - c_entry_index) / 2)) + rodata->NumChildren) % rodata->NumChildren;
                swap_order = 0;
            }
        }
        else if ((c_entry_index >= 0xA) && (c_entry_index < 0xF))
        {
            if (c_entry_index >= 0xD)
            {
                c_entry_index = ((rw_index_sel - ((0x10 - c_entry_index) / 2)) + rodata->NumChildren) % rodata->NumChildren;
            }
            else
            {
                c_entry_index = (((c_entry_index - 8) / 2) + rw_index_sel + rodata->NumChildren) % rodata->NumChildren;
                swap_order = 0;
            }
        }
        else
        {
            if ((c_entry_index >= 7) && (c_entry_index < 0xA))
            {
                swap_order = 0;
            }
            c_entry_index = rw_index_sel;
        }

        entry_04 = c_entry->entries;

        gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

        for (c_entry_count = c_entry->NumEntries; c_entry_count > 0; c_entry_count--)
        {
            switch (*entry_04)
            {
                case MODELNODE_CHILD_VTX:
                    {
                        struct ModelRoData_Child_Vtx* child_vtx = ((struct ModelRoData_Child_Vtx*)entry_04);
                        vtx1 = g_ModelVtxAllocator(2);
                        vtx2 = &vtx1[1];

                        *vtx1 = rodata->Vertices[child_vtx->VtxIndex];
                        *vtx2 = rodata->Vertices[child_vtx->VtxIndex+1];

                        if (rodata->nextTube != NULL)
                        {
                            c_entry2 = &rodata->Children[c_entry_index];
                            entry2_04 = c_entry2->entries;

                            for (c_entry2_count = c_entry2->NumEntries; c_entry2_count > 0; c_entry2_count--)
                            {
                                struct ModelRoData_Child_Vtx* entry2_04_child = ((struct ModelRoData_Child_Vtx*)entry2_04);
                                if (entry2_04_child->Type == (u8) 1) {
                                    vtx_10   = &rodata->Vertices[entry2_04_child->VtxIndex];
                                    vtx_10_2 = vtx_10+1;

                                    vtx1->coord.AsArray[0] = vtx_10->coord.AsArray[0];
                                    vtx1->coord.AsArray[1] = vtx_10->coord.AsArray[1];
                                    vtx1->coord.AsArray[2] = vtx_10->coord.AsArray[2];

                                    vtx2->coord.AsArray[0] = vtx_10_2->coord.AsArray[0];
                                    vtx2->coord.AsArray[1] = vtx_10_2->coord.AsArray[1];
                                    vtx2->coord.AsArray[2] = vtx_10_2->coord.AsArray[2];
                                    break;
                                }

                                switch (*entry2_04)
                                {
                                    case MODELNODE_CHILD_VTX:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Vtx);
                                        break;
                                    case MODELNODE_CHILD_IMAGE:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Image);
                                        break;
                                    case MODELNODE_CHILD_TRI:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Tri);
                                        break;
                                }
                            }

                        }
                        else
                        {
                            c_entry2 = &rodata2->Children[c_entry_index];
                            entry2_04 = c_entry2->entries;

                            for (c_entry2_count = c_entry2->NumEntries; c_entry2_count > 0; c_entry2_count--)
                            {
                                struct ModelRoData_Child_Vtx* entry2_04_child = ((struct ModelRoData_Child_Vtx*)entry2_04);
                                if (entry2_04_child->Type == (u8) 1)
                                {
                                    vtx_10   = &rodata2->Vertices[entry2_04_child->VtxIndex];
                                    vtx_10_2 = vtx_10 + 1;
                                    if (swap_order != 0)
                                    {
                                        vtx1->coord.AsArray[0] = vtx_10_2->coord.AsArray[0];
                                        vtx1->coord.AsArray[1] = vtx_10_2->coord.AsArray[1];
                                        vtx1->coord.AsArray[2] = vtx_10_2->coord.AsArray[2];

                                        vtx2->coord.AsArray[0] = vtx_10->coord.AsArray[0];
                                        vtx2->coord.AsArray[1] = vtx_10->coord.AsArray[1];
                                        vtx2->coord.AsArray[2] = vtx_10->coord.AsArray[2];
                                    }
                                    else
                                    {
                                        vtx1->coord.AsArray[0] = vtx_10->coord.AsArray[0];
                                        vtx1->coord.AsArray[1] = vtx_10->coord.AsArray[1];
                                        vtx1->coord.AsArray[2] = vtx_10->coord.AsArray[2];

                                        vtx2->coord.AsArray[0] = vtx_10_2->coord.AsArray[0];
                                        vtx2->coord.AsArray[1] = vtx_10_2->coord.AsArray[1];
                                        vtx2->coord.AsArray[2] = vtx_10_2->coord.AsArray[2];
                                    }
                                    break;
                                }

                                switch (*entry2_04)
                                {
                                    case MODELNODE_CHILD_VTX:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Vtx);
                                        break;

                                    case MODELNODE_CHILD_IMAGE:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Image);
                                        break;

                                    case MODELNODE_CHILD_TRI:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Tri);
                                        break;
                                }
                            }
                        }

                        gSPMatrix(renderdata->gdl++, osVirtualToPhysical(render_pos), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                        gSPVertex(renderdata->gdl++, osVirtualToPhysical(vtx1), 2, 0);
                        gSPMatrix(renderdata->gdl++, osVirtualToPhysical(render_pos2), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                        gSPVertex(renderdata->gdl++, osVirtualToPhysical(&rodata->Vertices[child_vtx->VtxIndex] + 2), (u32)(child_vtx->numVtx - 2), 2);

                        entry_04 += sizeof(struct ModelRoData_Child_Vtx);

                        break;
                    }

                case MODELNODE_CHILD_IMAGE:
                    {
                        struct ModelRoData_Child_Image* child_image = (struct ModelRoData_Child_Image*)entry_04;
                        if (child_image->ImageIndex != 0xFF)
                        {
                            modelSelectTexture(renderdata, &rodata->Images[child_image->ImageIndex], 1);
                            entry_04 += sizeof(struct ModelRoData_Child_Image);
                        }
                        else
                        {
                            modelSelectTexture(renderdata, NULL, 1);
                            entry_04 += sizeof(struct ModelRoData_Child_Image);
                        }
                        break;
                    }

                case MODELNODE_CHILD_TRI:
                    {
                        struct ModelRoData_Child_Tri* child_tri = (struct ModelRoData_Child_Tri*)entry_04;
                        gSP1Triangle(renderdata->gdl++, child_tri->VtxIndex1, child_tri->VtxIndex2, child_tri->VtxIndex3, 0);
                        entry_04 += sizeof(struct ModelRoData_Child_Tri);
                        break;
                    }
            }
        }
    }
}






// PD: modelRenderNodeChrGunfire
void modelRenderNodeGunfire(ModelRenderData *renderdata, Model *model, ModelNode *node)
{
    f32 negUpX;
    ModelRoData_GunfireRecord *rodata = &node->Data->Gunfire;
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    sImageTableEntry *tconfig;
    f32 yaw;
    f32 pitch;
    coord3d dir;
    f32 cosYaw;
    f32 sinYaw;
    f32 cosPitch;
    f32 sinPitch;
    f32 rightX;
    f32 rightZ;
    f32 upY;
    f32 upX;
    f32 upZ;
    f32 negRightX;
    f32 negRightZ;
    f32 scale;
    Mtxf *mtx;
    f32 tmp;
    coord3d size;
    coord3d base;
    Vertex vtxtemplate = g_ModelGunfireVtxTemplate;
    Vertex *vertices;
    f32 distance;

    if ((renderdata->flags & 2) && rwdata->Gunfire.visible)
    {
        s32 index = modelFindNodeMtxIndex(node, 0);
        mtx = &model->render_pos[index].pos;

        dir.x = -(rodata->Offset.f[0] * mtx->m[0][0] + rodata->Offset.f[1] * mtx->m[1][0] + rodata->Offset.f[2] * mtx->m[2][0] + mtx->m[3][0]);
        dir.y = -(rodata->Offset.f[0] * mtx->m[0][1] + rodata->Offset.f[1] * mtx->m[1][1] + rodata->Offset.f[2] * mtx->m[2][1] + mtx->m[3][1]);
        dir.z = -(rodata->Offset.f[0] * mtx->m[0][2] + rodata->Offset.f[1] * mtx->m[1][2] + rodata->Offset.f[2] * mtx->m[2][2] + mtx->m[3][2]);

        distance = sqrtf(dir.f[0] * dir.f[0] + dir.f[1] * dir.f[1] + dir.f[2] * dir.f[2]);

        if (distance > 0)
        {
            f32 tmp = 1 / (model->scale * distance);
            dir.f[0] *= tmp;
            dir.f[1] *= tmp;
            dir.f[2] *= tmp;
        }
        else
        {
            dir.f[0] = 0;
            dir.f[1] = 0;
            dir.f[2] = 1 / model->scale;
        }

        pitch = mathAcos(dir.f[0] * mtx->m[1][0] + dir.f[1] * mtx->m[1][1] + dir.f[2] * mtx->m[1][2]);
        yaw = mathAcos(-(dir.f[0] * mtx->m[2][0] + dir.f[1] * mtx->m[2][1] + dir.f[2] * mtx->m[2][2]) / sinf(pitch));

        tmp = -(dir.f[0] * mtx->m[0][0] + dir.f[1] * mtx->m[0][1] + dir.f[2] * mtx->m[0][2]);

        if (tmp < 0)
        {
            yaw = M_TAU_F - yaw;
        }

        cosYaw = cosf(yaw);
        sinYaw = sinf(yaw);
        cosPitch = cosf(pitch);
        sinPitch = sinf(pitch);

        scale = 0.75f + (randomGetNext() % 128) * (1.0f / 256.0f); // 0.75 to 1.25

        size.f[0] = rodata->Size.f[0] * scale;
        size.f[1] = rodata->Size.f[1] * scale;
        size.f[2] = rodata->Size.f[2] * scale;

        rightX = size.f[0] * cosYaw * 0.5f;
        rightZ = size.f[2] * sinYaw * 0.5f;
        upY = size.f[1] * sinPitch * 0.5f;

        upX = size.f[0] * cosPitch * sinYaw * 0.5f;
        upZ = size.f[2] * cosPitch * cosYaw * 0.5f;

        negRightX = -rightX;
        negRightZ = -rightZ;
        negUpX = -upX;

        base.f[0] = rodata->Offset.f[0] - size.f[0] * 0.5f;
        base.f[1] = rodata->Offset.f[1];
        base.f[2] = rodata->Offset.f[2];


        vertices = g_ModelVtxAllocator(4);

        vertices[0] = vtxtemplate;
        vertices[1] = vtxtemplate;
        vertices[2] = vtxtemplate;
        vertices[3] = vtxtemplate;

        vertices[0].coord.x = base.f[0] + negRightX + negUpX;
        vertices[0].coord.y = base.f[1] - upY;
        vertices[0].coord.z = base.f[2] - negRightZ + -upZ;
        vertices[1].coord.x = base.f[0] + negRightX - negUpX;
        vertices[1].coord.y = base.f[1] + upY;
        vertices[1].coord.z = base.f[2] - negRightZ - -upZ;
        vertices[2].coord.x = base.f[0] - negRightX - negUpX;
        vertices[2].coord.y = base.f[1] + upY;
        vertices[2].coord.z = base.f[2] + negRightZ - -upZ;
        vertices[3].coord.x = base.f[0] - negRightX + negUpX;
        vertices[3].coord.y = base.f[1] - upY;
        vertices[3].coord.z = base.f[2] + negRightZ + -upZ;

        gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

        if (rodata->Image)
        {
            s32 centre;
            u16 texAngle;
            s32 texCos;
            s32 texSin;

            tconfig = rodata->Image;

            texAngle = (randomGetNext() * 1024) & 0xffff;
            texCos = (coss(texAngle) * tconfig->width * 0xb5) >> 18;
            texSin = (sins(texAngle) * tconfig->width * 0xb5) >> 18;

            centre = tconfig->width << 4;

            vertices[0].s = centre - texCos;
            vertices[0].t = centre - texSin;
            vertices[1].s = centre + texSin;
            vertices[1].t = centre - texCos;
            vertices[2].s = centre + texCos;
            vertices[2].t = centre + texSin;
            vertices[3].s = centre - texSin;
            vertices[3].t = centre + texCos;

            modelSelectTexture(renderdata, tconfig, 4);
        }
        else
        {
            modelSelectTexture(renderdata, NULL, 1);
        }

        gSPSetGeometryMode(renderdata->gdl++, G_CULL_BACK);
        gSPMatrix(renderdata->gdl++, osVirtualToPhysical(mtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(renderdata->gdl++, osVirtualToPhysical(vertices), 4, 0);
        gSP2Triangles(renderdata->gdl++, 0, 1, 2, 0, 2, 3, 0, 0);
    }
}


void modelSetShadowAlpha(s32 alpha)
{
    g_ModelShadowAlpha = alpha;
}


void modelRenderNodeShadow(ModelRenderData *renderdata, Model *model, ModelNode *node)
{
    sImageTableEntry *image;
    RenderPosView *mtx;
    ModelRoData_ShadowRecord *shadow;
    Vtx vtxtemplate;
    Vtx *vtx;
    s16 y;
    s32 temp;
    f32 sizex;
    f32 sizey;
    f32 height;
    ModelRwData_HeaderRecord *rwdata;

    if ((s32) g_ModelShadowAlpha <= 0)
    {
        return;
    }

    shadow = &node->Data->Shadow;
    vtxtemplate = g_ModelShadowVtxTemplate;
    rwdata = &modelGetNodeRwData(model, shadow->HeaderNode)->Header;
    height = rwdata->pos.y - rwdata->ground;
    sizex = shadow->size.x;
    sizey = shadow->size.y;

    if (!(renderdata->flags & 2))
    {
        return;
    }

    if ((renderdata->PropType == PROP_TYPE_CHR) || (renderdata->PropType == PROP_TYPE_SMOKE))
    {
        temp = (vtxtemplate.v.cn[3] = ((renderdata->envcolour.word & 0xff) * g_ModelShadowAlpha) / 255);
    }
    else
    {
        vtxtemplate.v.cn[3] = g_ModelShadowAlpha;
    }

    temp = modelFindNodeMtxIndex(node, 0);
    mtx = &model->render_pos[temp];

    if (renderdata->zbufferenabled)
    {
        y = (2.0f - height) / model->scale;
    }
    else
    {
        y = (-height) / model->scale;
    }

    if (height < 50.0f)
    {
        sizex *= 1.25f;
        sizey *= 1.25f;
    }
    else if (300.0f < height)
    {
        sizex = 0.0f;
        sizey = sizex;
    }
    else
    {
        sizex *= (300.0f - height) / 200.0f;
        sizey *= (300.0f - height) / 200.0f;
    }


    vtx = (Vtx *) g_ModelVtxAllocator(4);

    vtx[0] = vtxtemplate;
    vtx[1] = vtxtemplate;
    vtx[2] = vtxtemplate;
    vtx[3] = vtxtemplate;

    vtx[0].v.ob[0] = shadow->pos.x - sizex;
    vtx[0].v.ob[1] = y;
    vtx[0].v.ob[2] = shadow->pos.y - sizey;
    vtx[1].v.ob[0] = shadow->pos.x - sizex;
    vtx[1].v.ob[1] = y;
    vtx[1].v.ob[2] = shadow->pos.y + sizey;
    vtx[2].v.ob[0] = shadow->pos.x + sizex;
    vtx[2].v.ob[1] = y;
    vtx[2].v.ob[2] = shadow->pos.y + sizey;
    vtx[3].v.ob[0] = shadow->pos.x + sizex;
    vtx[3].v.ob[1] = y;
    vtx[3].v.ob[2] = shadow->pos.y - sizey;

    gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(shadow->BaseAddr));

    if (shadow->image != NULL)
    {
        image = shadow->image;

        vtx[0].v.tc[0] = 0;
        vtx[0].v.tc[1] = 0;
        vtx[1].v.tc[0] = (image->width << 5) - 1;
        vtx[1].v.tc[1] = 0;
        vtx[2].v.tc[0] = (image->width << 5) - 1;
        vtx[2].v.tc[1] = (image->height << 5) - 1;
        vtx[3].v.tc[0] = 0;
        vtx[3].v.tc[1] = (image->height << 5) - 1;

        modelSelectTexture(renderdata, image, 4);
    }
    else
    {
        modelSelectTexture(renderdata, NULL, 1);
    }

    gSPSetGeometryMode(renderdata->gdl++, G_CULL_BACK);

    {
        Gfx *gdl;
        gSPMatrix(renderdata->gdl++, osVirtualToPhysical(mtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(renderdata->gdl++, osVirtualToPhysical(vtx), 4, 0);
        // gSP2Triangles(gdl, 0, 1, 2, 0, 2, 3, 0, 0) via GE's G_TRI4. The macro
        // itself can't be used here: its inner Gfx temp would be the block's gfx
        // pointer local and shift the spill homes off the original layout.
        gdl = renderdata->gdl++;
        gdl->words.w0 = _SHIFTL(G_TRI4, 24, 8) | _SHIFTL(2, 0, 4); gdl->words.w1 = _SHIFTL(3, 12, 4) | _SHIFTL(2, 8, 4) | _SHIFTL(1, 4, 4);
    }
}




void modelRenderNode(ModelRenderData* data, Model* model, ModelNode* node) {
    u32 id = node->Opcode & 0xFF;
    switch (id) {
    case MODELNODE_OPCODE_LOD:
        modelApplyDistanceRelations(model, node);
        return;
    case MODELNODE_OPCODE_SWITCH:
        modelApplyToggleRelations(model, node);
        return;
    case MODELNODE_OPCODE_HEAD:
        modelApplyHeadRelations(model, node);
        return;
    case MODELNODE_OPCODE_BSP:
        modelApplyReorderRelations(model, node);
        return;
    case MODELNODE_OPCODE_OP11:
        return;
    case MODELNODE_OPCODE_GUNFIRE:
        modelRenderNodeGunfire(data, model, node);
        return;
    case MODELNODE_OPCODE_SHADOW:
        modelRenderNodeShadow(data, model, node);
        return;
    case MODELNODE_OPCODE_BBOX:
        return;
    case MODELNODE_OPCODE_OP17:
        return;
    case MODELNODE_OPCODE_DL:
        modelRenderNodeGundl(data, node);
        return;
    case MODELNODE_OPCODE_DLCOLLISION:
        modelRenderNodeDl(data, model, node);
        return;
    case MODELNODE_OPCODE_OP20:
        return;
    case MODELNODE_OPCODE_DLPRIMARY:
        modelRenderNodeRotTex(data, node);
        return;
    case MODELNODE_OPCODE_OP05:
        return;
    case MODELNODE_OPCODE_TUBE:
        modelRenderNodeTube(data, model, node);
        return;
    case MODELNODE_OPCODE_OP06:
        return;
    case MODELNODE_OPCODE_HEADER:
    case MODELNODE_OPCODE_GROUP:
    case MODELNODE_OPCODE_OP03:
    case MODELNODE_OPCODE_OP14:
    case MODELNODE_OPCODE_INTERLINK:
    case MODELNODE_OPCODE_OP16:
    default:
        return;
    }
}


void modelRender(ModelRenderData *mrData, Model *mdl)
{
    ModelNode *root = mdl->obj->RootNode;
    gSPSegment(mrData->gdl++, 3, osVirtualToPhysical(mdl->render_pos));

    while (root != NULL)
    {
        modelRenderNode(mrData, mdl, root);

        if (root->Child)
        {
            root = root->Child;
        }
        else
        {
            while (root)
            {
                if (root->Next)
                {
                    root = root->Next;
                    break;
                }
                root = root->Parent;
            }
        }
    }
}


/**
 * Ray vs transformed bounding box test. The bbox is transformed by mtx while the ray
 * is defined by pos and dir.
 * 
 * @returns TRUE if the ray intersects the bbox, otherwise FALSE.
 */
bool modelTestRayIntersectsTransformedBBox(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx, coord3d *pos, coord3d *dir)
{
    f32 xthingx;
    f32 xthingy;
    f32 xthingz;
    f32 xpmin;
    f32 xpmax;
    f32 xsum1;
    f32 xsum2;
    f32 xsum3;
    f32 negL2;
    f32 ythingx;
    f32 ythingy;
    f32 ythingz;
    f32 ypmin;
    f32 ypmax;
    f32 ysum1;
    f32 ysum2;
    f32 ysum3;
    f32 mult1;
    f32 mult2;
    f32 bestsum2;
    f32 bestsum1;
    f32 anotherbestsum3;
    f32 anotherbestsum1;
    f32 mult3;
    f32 mult4;
    f32 zthingx;
    f32 zthingy;
    f32 zthingz;
    f32 zpmin;
    f32 zpmax;
    f32 zsum1;
    f32 zsum2;
    f32 zsum3;

    xthingx = mtx->m[0][0] * mtx->m[0][0];
    xthingy = mtx->m[0][1] * mtx->m[0][1];
    xthingz = mtx->m[0][2] * mtx->m[0][2];

    negL2 = -((xthingx + xthingy) + xthingz);

    xpmin = negL2;
    xpmin *= bbox->Bounds.xmin;

    xpmax = negL2;
    xpmax *= bbox->Bounds.xmax;

    xsum1 = ((mtx->m[0][0] * dir->f[0]) + (mtx->m[0][1] * dir->f[1])) + (mtx->m[0][2] * dir->f[2]);
    xsum2 = ((mtx->m[0][0] * (pos->f[0] - mtx->m[3][0])) + (mtx->m[0][1] * (pos->f[1] - mtx->m[3][1]))) + (mtx->m[0][2] * (pos->f[2] - mtx->m[3][2]));

    xsum3 = -(xsum2 + xpmax);
    xsum2 = -(xsum2 + xpmin);

    if (xsum1 < 0.0f)
    {
        xsum1 = -xsum1;
        xsum2 = -xsum2;
        xsum3 = -xsum3;
    }

    if ((xsum2 < 0.0f) && (xsum3 < 0.0f))
    {
        return FALSE;
    }

    if (xsum3 < xsum2)
    {
        f32 tmp = xsum2;

        xsum2 = xsum3;
        xsum3 = tmp;
    }

    ythingx = mtx->m[1][0] * mtx->m[1][0];
    ythingy = mtx->m[1][1] * mtx->m[1][1];
    ythingz = mtx->m[1][2] * mtx->m[1][2];

    negL2 = -((ythingx + ythingy) + ythingz);

    ypmin = negL2;
    ypmin *= bbox->Bounds.ymin;

    ypmax = negL2;
    ypmax *= bbox->Bounds.ymax;

    ysum1 = ((mtx->m[1][0] * dir->f[0]) + (mtx->m[1][1] * dir->f[1])) + (mtx->m[1][2] * dir->f[2]);
    ysum2 = ((mtx->m[1][0] * (pos->f[0] - mtx->m[3][0])) + (mtx->m[1][1] * (pos->f[1] - mtx->m[3][1]))) + (mtx->m[1][2] * (pos->f[2] - mtx->m[3][2]));

    ysum3 = -(ysum2 + ypmax);
    ysum2 = -(ysum2 + ypmin);

    if (ysum1 < 0.0f)
    {
        ysum1 = -ysum1;
        ysum2 = -ysum2;
        ysum3 = -ysum3;
    }

    if ((ysum2 < 0.0f) && (ysum3 < 0.0f))
    {
        return FALSE;
    }

    if (ysum3 < ysum2)
    {
        f32 tmp = ysum2;

        ysum2 = ysum3;
        ysum3 = tmp;
    }

    mult1 = ysum2 * xsum1;
    mult2 = xsum2 * ysum1;
    mult3 = xsum3 * ysum1;
    mult4 = ysum3 * xsum1;

    if (mult1 < mult2)
    {
        if (mult4 < mult2)
        {
            return FALSE;
        }

        bestsum2 = xsum2;
        bestsum1 = xsum1;
    }
    else
    {
        if (mult3 < mult1)
        {
            return FALSE;
        }

        bestsum2 = ysum2;
        bestsum1 = ysum1;
    }

    if (mult3 < mult4)
    {
        anotherbestsum3 = xsum3;
        anotherbestsum1 = xsum1;
    }
    else
    {
        anotherbestsum3 = ysum3;
        anotherbestsum1 = ysum1;
    }

    zthingx = mtx->m[2][0] * mtx->m[2][0];
    zthingy = mtx->m[2][1] * mtx->m[2][1];
    zthingz = mtx->m[2][2] * mtx->m[2][2];

    negL2 = -((zthingx + zthingy) + zthingz);

    zpmin = negL2;
    zpmin *= bbox->Bounds.zmin;

    zpmax = negL2;
    zpmax *= bbox->Bounds.zmax;

    zsum1 = ((mtx->m[2][0] * dir->f[0]) + (mtx->m[2][1] * dir->f[1])) + (mtx->m[2][2] * dir->f[2]);
    zsum2 = ((mtx->m[2][0] * (pos->f[0] - mtx->m[3][0])) + (mtx->m[2][1] * (pos->f[1] - mtx->m[3][1]))) + (mtx->m[2][2] * (pos->f[2] - mtx->m[3][2]));

    zsum3 = -(zsum2 + zpmax);
    zsum2 = -(zsum2 + zpmin);

    if (zsum1 < 0.0f)
    {
        zsum1 = -zsum1;
        zsum2 = -zsum2;
        zsum3 = -zsum3;
    }

    if ((zsum2 < 0.0f) && (zsum3 < 0.0f))
    {
        return FALSE;
    }

    if (zsum3 < zsum2)
    {
        f32 tmp = zsum2;

        zsum2 = zsum3;
        zsum3 = tmp;
    }

    if ((bestsum2 * zsum1) < (zsum2 * bestsum1))
    {
        if ((anotherbestsum3 * zsum1) < (zsum2 * anotherbestsum1))
        {
            return FALSE;
        }
    }
    else if ((zsum3 * bestsum1) < (bestsum2 * zsum1))
    {
        return FALSE;
    }

    return TRUE;
}


bool modelTestRayIntersectsNodeBBox(Model *model, ModelNode *node, coord3d *pos, coord3d *dir)
{
    ModelRoData_BoundingBoxRecord *bbox = &node->Data->BoundingBox;

    return modelTestRayIntersectsTransformedBBox(bbox, modelFindNodeMtx(model, node, 0), pos, dir);
}


s32 modelTestRayIntersectsNodeOp17(Model *model, ModelNode *node, coord3d *raypos, coord3d *raydir)
{
    ModelRoData_Op17Record *hitData;
    Mtxf *nodeMtx;
    ModelOp17MainStack rayData[1];
    f32 centerProjection;
    Mtxf *otherNodeMtx;
    ModelOp17AxisStack axisData[1];
    f32 scaledProjection;
    f32 projectionScalar;
    f32 secondAxisScale;
    f32 centerDistanceSq;
    u32 nodeFlags;
    f32 directionDotProduct;
    
    hitData = (ModelRoData_Op17Record *) node->Data;
    nodeMtx = modelFindNodeMtx(model, node, 0);
    rayData->data.rel = (coord3d){{1.0f, 0.0f, 0.0f}};
    rayData->data.radiusSq = hitData->radiusSq;
    rayData->data.dir.f[0] = raydir->x;
    rayData->data.dir.f[1] = raydir->y;
    rayData->data.dir.f[2] = raydir->z;
    nodeFlags = node->Opcode;
    
    if (nodeFlags & 0x100)
    {
        rayData->data.pos.f[0] = hitData->pos.f[0];
        rayData->data.pos.f[1] = hitData->pos.f[1];
        rayData->data.pos.f[2] = hitData->pos.f[2];
        rayData->data.rel.f[0] = (((rayData->data.pos.f[0] * nodeMtx->m[0][0]) + (rayData->data.pos.f[1] * nodeMtx->m[1][0])) + (rayData->data.pos.f[2] * nodeMtx->m[2][0])) + (nodeMtx->m[3][0] - raypos->x);
        rayData->data.rel.f[1] = (((rayData->data.pos.f[0] * nodeMtx->m[0][1]) + (rayData->data.pos.f[1] * nodeMtx->m[1][1])) + (rayData->data.pos.f[2] * nodeMtx->m[2][1])) + (nodeMtx->m[3][1] - raypos->y);
        rayData->data.rel.f[2] = (((rayData->data.pos.f[0] * nodeMtx->m[0][2]) + (rayData->data.pos.f[1] * nodeMtx->m[1][2])) + (rayData->data.pos.f[2] * nodeMtx->m[2][2])) + (nodeMtx->m[3][2] - raypos->z);
    }
    else if (nodeFlags & 0x200)
    {
        if (hitData->othernode != NULL)
        {
            otherNodeMtx = modelFindNodeMtx(model, hitData->othernode, 0);
            rayData->data.rel.f[0] = ((nodeMtx->m[3][0] + otherNodeMtx->m[3][0]) * 0.5f) - (*raypos).f[0];
            rayData->data.rel.f[1] = ((nodeMtx->m[3][1] + otherNodeMtx->m[3][1]) * 0.5f) - (*raypos).f[1];
            rayData->data.rel.f[2] = ((nodeMtx->m[3][2] + otherNodeMtx->m[3][2]) * 0.5f) - (*raypos).f[2];
        }
        else
        {
            rayData->data.rel.f[0] = nodeMtx->m[3][0] - raypos->x;
            rayData->data.rel.f[1] = nodeMtx->m[3][1] - raypos->y;
            rayData->data.rel.f[2] = nodeMtx->m[3][2] - raypos->z;
        }
    }
    else
    {
        rayData->data.rel.f[0] = nodeMtx->m[3][0] - raypos->x;
        rayData->data.rel.f[1] = nodeMtx->m[3][1] - raypos->y;
        rayData->data.rel.f[2] = nodeMtx->m[3][2] - raypos->z;
    }

    nodeFlags = node->Opcode;

    if (nodeFlags & 0x400)
    {
        projectionScalar = hitData->scale1;
        secondAxisScale = hitData->scale2;
        scaledProjection = (((rayData->data.dir.f[0] * nodeMtx->m[0][0]) + (rayData->data.dir.f[1] * nodeMtx->m[0][1])) + (rayData->data.dir.f[2] * nodeMtx->m[0][2])) * projectionScalar;
        directionDotProduct = scaledProjection;
        rayData->data.dir.f[0] = rayData->data.dir.f[0] + (nodeMtx->m[0][0] * directionDotProduct);
        rayData->data.dir.f[1] = rayData->data.dir.f[1] + (nodeMtx->m[0][1] * directionDotProduct);
        rayData->data.dir.f[2] = rayData->data.dir.f[2] + (nodeMtx->m[0][2] * directionDotProduct);
        centerProjection = (((rayData->data.rel.f[0] * nodeMtx->m[0][0]) + (rayData->data.rel.f[1] * nodeMtx->m[0][1])) + (rayData->data.rel.f[2] * nodeMtx->m[0][2])) * projectionScalar;
        rayData->data.rel.f[0] = rayData->data.rel.f[0] + (nodeMtx->m[0][0] * centerProjection);
        rayData->data.rel.f[1] = rayData->data.rel.f[1] + (nodeMtx->m[0][1] * centerProjection);
        rayData->data.rel.f[2] = rayData->data.rel.f[2] + (nodeMtx->m[0][2] * centerProjection);
        scaledProjection = (((rayData->data.dir.f[0] * nodeMtx->m[1][0]) + (rayData->data.dir.f[1] * nodeMtx->m[1][1])) + (rayData->data.dir.f[2] * nodeMtx->m[1][2])) * secondAxisScale;
        directionDotProduct = scaledProjection;
        rayData->data.dir.f[0] = rayData->data.dir.f[0] + (nodeMtx->m[1][0] * directionDotProduct);
        rayData->data.dir.f[1] = rayData->data.dir.f[1] + (nodeMtx->m[1][1] * directionDotProduct);
        rayData->data.dir.f[2] = rayData->data.dir.f[2] + (nodeMtx->m[1][2] * directionDotProduct);
        projectionScalar = (((rayData->data.rel.f[0] * nodeMtx->m[1][0]) + (rayData->data.rel.f[1] * nodeMtx->m[1][1])) + (rayData->data.rel.f[2] * nodeMtx->m[1][2])) * secondAxisScale;
        rayData->data.rel.f[0] = rayData->data.rel.f[0] + (nodeMtx->m[1][0] * projectionScalar);
        rayData->data.rel.f[1] = rayData->data.rel.f[1] + (nodeMtx->m[1][1] * projectionScalar);
        rayData->data.rel.f[2] = rayData->data.rel.f[2] + (nodeMtx->m[1][2] * projectionScalar);
    }
    else if (((nodeFlags & 0x800) || (nodeFlags & 0x1000)) || (nodeFlags & 0x2000))
    {
        axisData->axisAdjustment.scale = hitData->scale1;
        if ((nodeFlags & 0x800))
        {
            axisData->axisAdjustment.axis.f[0] = nodeMtx->m[0][0];
            axisData->axisAdjustment.axis.f[1] = nodeMtx->m[0][1];
            axisData->axisAdjustment.axis.f[2] = nodeMtx->m[0][2];
        }
        else if (nodeFlags & 0x1000)
        {
            axisData->axisAdjustment.axis.f[0] = nodeMtx->m[1][0];
            axisData->axisAdjustment.axis.f[1] = nodeMtx->m[1][1];
            axisData->axisAdjustment.axis.f[2] = nodeMtx->m[1][2];
        }
        else if (nodeFlags & 0x2000)
        {
            axisData->axisAdjustment.axis.f[0] = nodeMtx->m[2][0];
            axisData->axisAdjustment.axis.f[1] = nodeMtx->m[2][1];
            axisData->axisAdjustment.axis.f[2] = nodeMtx->m[2][2];
        }
        
        directionDotProduct = axisData->axisAdjustment.scale * (
        (rayData->data.dir.f[0] * axisData->axisAdjustment.axis.f[0]) +
        (rayData->data.dir.f[1] * axisData->axisAdjustment.axis.f[1]) +
        (rayData->data.dir.f[2] * axisData->axisAdjustment.axis.f[2])
        );
        
        rayData->data.dir.f[0] = rayData->data.dir.f[0] + (axisData->axisAdjustment.axis.f[0] * directionDotProduct);
        rayData->data.dir.f[1] = rayData->data.dir.f[1] + (axisData->axisAdjustment.axis.f[1] * directionDotProduct);
        rayData->data.dir.f[2] = rayData->data.dir.f[2] + (axisData->axisAdjustment.axis.f[2] * directionDotProduct);
        
        scaledProjection = axisData->axisAdjustment.scale * (
        (rayData->data.rel.f[0] * axisData->axisAdjustment.axis.f[0]) +
        (rayData->data.rel.f[1] * axisData->axisAdjustment.axis.f[1]) +
        (rayData->data.rel.f[2] * axisData->axisAdjustment.axis.f[2])
        );
        
        rayData->data.rel.f[0] = rayData->data.rel.f[0] + (axisData->axisAdjustment.axis.f[0] * scaledProjection);
        rayData->data.rel.f[1] = rayData->data.rel.f[1] + (axisData->axisAdjustment.axis.f[1] * scaledProjection);
        rayData->data.rel.f[2] = rayData->data.rel.f[2] + (axisData->axisAdjustment.axis.f[2] * scaledProjection);
    }
    
    projectionScalar = 
    (rayData->data.dir.f[0] * rayData->data.rel.f[0]) +
    (rayData->data.dir.f[1] * rayData->data.rel.f[1]) +
    (rayData->data.dir.f[2] * rayData->data.rel.f[2]);
    
    if (0.0f < projectionScalar)
    {
        directionDotProduct = (rayData->data.dir.f[0] * rayData->data.dir.f[0]) + (rayData->data.dir.f[1] * rayData->data.dir.f[1]) + (rayData->data.dir.f[2] * rayData->data.dir.f[2]);
        centerDistanceSq = (rayData->data.rel.f[0] * rayData->data.rel.f[0]) + (rayData->data.rel.f[1] * rayData->data.rel.f[1]) + (rayData->data.rel.f[2] * rayData->data.rel.f[2]);

        if (((centerDistanceSq - rayData->data.radiusSq) * directionDotProduct) <= (projectionScalar * projectionScalar))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return 0;
}


u32 modelFindRayHitNext(Model *model, coord3d *raypos, coord3d *raydir, ModelNode **nodeptr)
{
    ModelNode *node;
    s32 descend;
    u32 opcode;

    descend = TRUE;

    if (*nodeptr != NULL) {
        node = *nodeptr;
        *nodeptr = NULL;
    } else {
        node = model->obj->RootNode;
    }

    if (node != NULL) {
        do {
            if (descend != 0 && node->Child != NULL) {
                node = node->Child;
            } else {
                if (node != NULL) {
                    do {
                        if (node->Next != NULL) {
                            node = node->Next;
                            break;
                        }

                        node = node->Parent;
                    } while (node != NULL);
                }

                if (node == NULL) {
                    break;
                }
            }

            descend = TRUE;
            opcode = node->Opcode & 0xff;

            /*
            * This switch is written as opcode - 1 to match the compiler's jump-table generation.
            * The real opcodes run from HEADER=1 through DLCOLLISION=24, so the compiler
            * normalizes them to a zero-based table index by subtracting 1.
            */
            switch (opcode - 1) {
            case MODELNODE_OPCODE_BBOX - 1:
                if (modelTestRayIntersectsNodeBBox(model, node, raypos, raydir) != 0) {
                    *nodeptr = node;
                    return *(u32 *)node->Data;
                }
                descend = FALSE;
                break;

            case MODELNODE_OPCODE_OP17 - 1:
                if (modelTestRayIntersectsNodeOp17(model, node, raypos, raydir) != 0) {
                    *nodeptr = node;
                    return *(u32 *)node->Data;
                }
                descend = FALSE;
                break;

            case MODELNODE_OPCODE_LOD - 1:
                modelApplyDistanceRelations(model, node);
                break;

            case MODELNODE_OPCODE_SWITCH - 1:
                modelApplyToggleRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEAD - 1:
                modelApplyHeadRelations(model, node);
                break;
            case MODELNODE_OPCODE_HEADER - 1:
            case MODELNODE_OPCODE_DLCOLLISION - 1:
            default:
                break;
            }
        } while (node != NULL);
    }

    return 0;
}


/**
 * Copy animation from ROM to RAM
*/
u8 *animLoadFrame(ModelAnimation *anim, s32 frame)
{
    u8 *ret = NULL;
    s32 frameSize = anim->frameBits >> 3;

    if (anim->address & 0x80000000) // The animation is already in RAM
    {
        ret = (u8 *)(anim->address + frame * frameSize);
    }
    else if (g_AnimFrameBuffer != NULL) // never NULL once animInitFrameBuffer has run
    {
        // Copy the frame from ROM to the next 16-byte aligned spot in the frame buffer.
        u8 *dest = (u8 *)(((u32)g_AnimFrameBuffer->animBufferPtr2 + 15) & ~15);
        u32 source = anim->address + frame * frameSize;
        u32 size;

        ret = dest;

        // ROM DMAs must start on an even address
        if (source & 1)
        {
            source--;
            frameSize++;
            ret++;
        }

        size = (frameSize + 15) & ~15;
        romCopy(dest, (void *)source, size);

        g_AnimFrameBuffer->uselessPointer++;

        // The next frame goes after this one
        g_AnimFrameBuffer->animBufferPtr2 = (char *)dest + size;
    }

    return ret;
}


void animResetFrameBuffer(void)
{
    if (g_AnimFrameBuffer != NULL) // should never be NULL after animInitFrameBuffer is called
    {
        // Reset the pointer to point to the start of the array
        g_AnimFrameBuffer->animBufferPtr2 = g_AnimFrameBuffer->animBufferPtr1;
        g_AnimFrameBuffer->uselessPointer = NULL;
    }
}


#define PROMOTE(var) \
    if (var) \
        var = (typeof(var))((u32)var + diff)

void modelPromoteNodeOffsetsToPointers(ModelNode *node, u32 vma, u32 fileramaddr)
{
    s32 diff = fileramaddr - vma;
    s32 i;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        PROMOTE(node->Data);
        PROMOTE(node->Parent);
        PROMOTE(node->Next);
        PROMOTE(node->Prev);
        PROMOTE(node->Child);

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                {
                    ModelRoData_HeaderRecord* rodata = &node->Data->Header;
                    PROMOTE(rodata->FirstGroup);
                    break;
                }

            case MODELNODE_OPCODE_GROUP:
                {
                    ModelRoData_GroupRecord* rodata = &node->Data->Group;
                    PROMOTE(rodata->ChildGroup);
                    break;
                }

            case MODELNODE_OPCODE_OP03:
                {
                    ModelRoData_GroupRecord* rodata = &node->Data->Group;
                    PROMOTE(rodata->ChildGroup);
                    break;
                }

            case MODELNODE_OPCODE_DL:
                {
                    ModelRoData_DisplayListRecord* rodata = &node->Data->DisplayList;
                    PROMOTE(rodata->Vertices);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_DLCOLLISION:
                {
                    ModelRoData_DisplayList_CollisionRecord* rodata = &node->Data->DisplayListCollisions;
                    PROMOTE(rodata->Vertices);
                    PROMOTE(rodata->CollisionVertices);
                    PROMOTE(rodata->PointUsage);
                    for (i = 0; i < rodata->numCollisionVertices; i++)
                    {
                        PROMOTE(rodata->CollisionVertices[i].LinkedTo);
                    }
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_OP20:
                {
                    ModelRoData_HeaderRecord* rodata = &node->Data->Header;
                    PROMOTE(rodata->FirstGroup);
                    break;
                }

            case MODELNODE_OPCODE_OP05:
                {
                    ModelRoData_Op05Record* rodata = &node->Data->Op05;

                    // shared with op07
                    PROMOTE(rodata->Children);
                    PROMOTE(rodata->Vertices);
                    PROMOTE(rodata->Images);
                    for (i = 0; i < rodata->NumChildren; i++)
                    {
                        PROMOTE(rodata->Children[i].entries);
                    }

                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_TUBE:
                {
                    ModelRoData_TubeRecord* rodata = &node->Data->Tube;
                    PROMOTE(rodata->prevTube);
                    PROMOTE(rodata->nextTube);

                    // shared with op05
                    PROMOTE(rodata->Children);
                    PROMOTE(rodata->Vertices);
                    PROMOTE(rodata->Images);
                    for (i = 0; i < rodata->NumChildren; i++)
                    {
                        PROMOTE(rodata->Children[i].entries);
                    }

                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_OP06:
                {
                    ModelRoData_Op06Record* rodata = &node->Data->Op06;
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_LOD:
                {
                    ModelRoData_LODRecord* rodata = &node->Data->LOD;
                    PROMOTE(rodata->Affects);
                    node->Child = rodata->Affects;
                    break;
                }

            case MODELNODE_OPCODE_SWITCH:
                {
                    ModelRoData_SwitchRecord* rodata = &node->Data->Switch;
                    PROMOTE(rodata->Controls);
                    break;
                }

            case MODELNODE_OPCODE_BSP:
                {
                    ModelRoData_BSPRecord* rodata = &node->Data->BSP;
                    PROMOTE(rodata->leftChild);
                    PROMOTE(rodata->rightChild);
                    break;
                }

            case MODELNODE_OPCODE_OP17:
                {
                    ModelRoData_GroupRecord* rodata = &node->Data->Group;
                    PROMOTE(rodata->ChildGroup);
                    break;
                }

            case MODELNODE_OPCODE_OP11:
                {
                    ModelRoData_Op11Record* rodata = &node->Data->Op11;
                    PROMOTE(rodata->raw[15]);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_GUNFIRE:
                {
                    ModelRoData_GunfireRecord* rodata = &node->Data->Gunfire;
                    PROMOTE(rodata->Image);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_SHADOW:
                {
                    ModelRoData_ShadowRecord* rodata = &node->Data->Shadow;
                    PROMOTE(rodata->image);
                    PROMOTE(rodata->Header);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_DLPRIMARY:
                {
                    ModelRoData_DisplayListPrimaryRecord* rodata = &node->Data->DisplayListPrimary;
                    PROMOTE(rodata->Vertices);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}

void modelFilePromotePointers(ModelFileHeader *header, s32 vma, void *addr) {
    s32 diff = (u32)addr - vma;
    s32 i;

    for(i = 0;i < header->numSwitches;i++)
    {
        PROMOTE(header->Switches[i]);
    }
    modelPromoteNodeOffsetsToPointers(header->RootNode, vma, (u32)addr);
}


s32 modelCalculateRwDataIndexes(ModelNode *basenode)
{
    u16 len = 0;
    ModelNode *node = basenode;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                if (1)
                {
                    ModelRoData_HeaderRecord *rodata = &node->Data->Header;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_HeaderRecord) / 4;
                    break;
                }
            case MODELNODE_OPCODE_TUBE:
                if (1)
                {
                    ModelRoData_TubeRecord *rodata = &node->Data->Tube;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_TubeRecord) / 4;
                    break;
                }
            case MODELNODE_OPCODE_LOD:
                if (1)
                {
                    ModelRoData_LODRecord *rodata = &node->Data->LOD;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_LODRecord) / 4;
                    node->Child = rodata->Affects;
                    break;
                }
            case MODELNODE_OPCODE_SWITCH:
                if (1)
                {
                    ModelRoData_SwitchRecord *rodata = &node->Data->Switch;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_SwitchRecord) / 4;
                    node->Child = rodata->Controls;
                    break;
                }
            case MODELNODE_OPCODE_HEAD:
                if (1)
                {
                    ModelRoData_HeadPlaceholderRecord *rodata = &node->Data->HeadPlaceholder;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_HeadPlaceholderRecord) / 4;
                    node->Child = NULL;
                    break;
                }
            case MODELNODE_OPCODE_BSP:
                if (1)
                {
                    ModelRoData_BSPRecord *rodata = &node->Data->BSP;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_BSPRecord) / 4;
                    modelApplyReorderRelationsByArg(node, FALSE);
                    break;
                }
            case MODELNODE_OPCODE_OP11:
                if (1)
                {
                    ModelRoData_Op11Record *rodata = &node->Data->Op11;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_Op11Record) / 4;
                    break;
                }
            case MODELNODE_OPCODE_GUNFIRE:
                if (1)
                {
                    ModelRoData_GunfireRecord *rodata = &node->Data->Gunfire;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_GunfireRecord) / 4;
                    break;
                }
            case MODELNODE_OPCODE_DLCOLLISION:
                if (1)
                {
                    ModelRoData_DisplayList_CollisionRecord *rodata = &node->Data->DisplayListCollisions;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_DisplayList_CollisionRecord) / 4;
                    break;
                }
            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node == basenode->Parent)
                {
                    node = NULL;
                    break;
                }

                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }

    return len;
}


void modelCalculateRwDataLen(struct ModelFileHeader *objheader)
{
    objheader->numRecords = modelCalculateRwDataIndexes(objheader->RootNode);
}


void modelInitRwData(Model *model, ModelNode *startnode)
{
    ModelNode *node = startnode;

    while (node)
    {
        u32 type = node->Opcode & 0xFF;

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                if (1)
                {
                    ModelRwData_HeaderRecord* rwdata = &modelGetNodeRwData(model, node)->Header;

                    rwdata->frozen = 0;
                    rwdata->ground = 0;
                    rwdata->pos.x = 0;
                    rwdata->pos.y = 0;
                    rwdata->pos.z = 0;
                    rwdata->yrot = 0;
                    rwdata->yrotBlend = 0;
                    rwdata->yrotBlendTarget = 0;

                    rwdata->nextValid = 0;
                    rwdata->framePos.x = 0;
                    rwdata->framePos.y = 0;
                    rwdata->framePos.z = 0;
                    rwdata->frameYrot = 0;
                    rwdata->nextPos.x = 0;
                    rwdata->nextPos.y = 0;
                    rwdata->nextPos.z = 0;
                    rwdata->nextYrot = 0;

                    rwdata->anim2NextValid = 0;
                    rwdata->anim2FramePos.x = 0;
                    rwdata->anim2FramePos.y = 0;
                    rwdata->anim2FramePos.z = 0;
                    rwdata->anim2NextPos.x = 0;
                    rwdata->anim2NextPos.y = 0;
                    rwdata->anim2NextPos.z = 0;
                    rwdata->turnFrames = 0;
                    break;
                }

            case MODELNODE_OPCODE_TUBE:
                if (1)
                {
                    ModelRwData_TubeRecord* rwdata = &modelGetNodeRwData(model, node)->Tube;
                    rwdata->index = 0;
                    break;
                }


            case MODELNODE_OPCODE_LOD:
                if (1)
                {
                    ModelRoData_LODRecord* rodata = &node->Data->LOD;
                    ModelRwData_LODRecord* rwdata = &modelGetNodeRwData(model, node)->LOD;
                    rwdata->visible = FALSE;
                    node->Child = rodata->Affects;
                    break;
                }

            case MODELNODE_OPCODE_SWITCH:
                if (1)
                {
                    ModelRoData_SwitchRecord* rodata = &node->Data->Switch;
                    ModelRwData_SwitchRecord* rwdata = &modelGetNodeRwData(model, node)->Switch;
                    rwdata->visible = TRUE;
                    node->Child = rodata->Controls;
                    break;
                }

            case MODELNODE_OPCODE_HEAD:
                if (1)
                {
                    ModelRwData_HeadPlaceholderRecord* rwdata = &modelGetNodeRwData(model, node)->HeadPlaceholder;
                    rwdata->ModelFileHeader = NULL;
                    rwdata->RwDatas = NULL;
                    break;
                }

            case MODELNODE_OPCODE_BSP:
                if (1)
                {
                    ModelRwData_BSPRecord* rwdata = &modelGetNodeRwData(model, node)->BSP;
                    rwdata->visible = FALSE;
                    modelApplyReorderRelations(model, node);
                    break;
                }

            case MODELNODE_OPCODE_OP11:
                if (1)
                {
                    ModelRwData_Op11Record* rwdata = &modelGetNodeRwData(model, node)->Op11;
                    rwdata->unk00 = FALSE;
                    break;
                }

            case MODELNODE_OPCODE_GUNFIRE:
                if (1)
                {
                    ModelRwData_GunfireRecord* rwdata = &modelGetNodeRwData(model, node)->Gunfire;
                    rwdata->visible = FALSE;
                    break;
                }

            case MODELNODE_OPCODE_DLCOLLISION:
                if (1)
                {
                    ModelRoData_DisplayList_CollisionRecord* rodata = &node->Data->DisplayListCollisions;
                    ModelRwData_DisplayList_CollisionRecord* rwdata = &modelGetNodeRwData(model, node)->DisplayListCollisions;
                    rwdata->Vertices = rodata->Vertices;
                    rwdata->gdl = rodata->Primary;
                    break;
                }

            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node == startnode->Parent)
                {
                    node = NULL;
                    break;
                }

                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelInit(struct Model *objinst, struct ModelFileHeader *header, u32 *data)
{
  objinst->obj = header;
  objinst->datas = data;
  objinst->rwdatalen = -1;
  objinst->attachedto = NULL;
  objinst->attachedto_objinst = NULL;
  objinst->scale = 1.0;
  modelInitRwData(objinst, header->RootNode);
}


// PD: animInit
void animInit(struct Model *objinst, struct ModelFileHeader *header, u32 *data)
{
    modelInit(objinst, header, data);
    objinst->anim = NULL;
    objinst->anim2 = NULL;
    objinst->animlooping = 0;
    objinst->animflipfunc = NULL;
    objinst->rootMoveCallback = 0;
    objinst->frameFrac = 0.0f;
    objinst->timespeed = 0.0f;
    objinst->frame2Frac = 0.0f;
    objinst->timespeed2 = 0.0f;
    objinst->mergeWeight = 0.0f;
    objinst->mergeTime = 0.0f;
    objinst->timeplayspeed = 0.0f;
    objinst->speed = 1.0f;
    objinst->speed2 = 1.0f;
    objinst->playspeed = 1.0f;
    objinst->anim_translation_scale = 1.0f;
    objinst->endframe = -1.0f;
    objinst->endframe2 = -1.0f;
}


// PD: model00023108
void modelAttachPart(Model *pmodel, ModelFileHeader *pmodeldef, ModelNode *pnode, ModelFileHeader *cmodeldef)
{
    ModelRwData_HeadPlaceholderRecord *rwdata = &modelGetNodeRwData(pmodel, pnode)->HeadPlaceholder;
    ModelNode *node;

    rwdata->ModelFileHeader = cmodeldef;
    rwdata->RwDatas = &pmodel->datas[pmodeldef->numRecords];

    pnode->Child = cmodeldef->RootNode;

    node = pnode->Child;

    while (node)
    {
        node->Parent = pnode;
        node = node->Next;
    }

    pmodeldef->numRecords += modelCalculateRwDataIndexes(pnode->Child);
}


/**
 * This function can be called repeatedly to iterate a model's display lists.
 *
 * On the first call, the value passed as nodeptr should point to a NULL value.
 * Each time the function is called, it will update *gdlptr to point to the next
 * display list, and will update *nodeptr to point to the current node. On
 * subsequent calls, the same values should be passed as nodeptr and gdlptr so
 * the function can continue correctly.
 *
 * Note that some node types support multiple display lists, so the function
 * may return the same node while it iterates the display lists for that node.
 */
void modelIterateDisplayLists(ModelFileHeader *fileheader, ModelNode **nodeptr, Gfx **gdlptr)
{
    ModelNode *node = *nodeptr;
    union ModelRoData *rodata;
    Gfx *gdl = NULL;

    if (node == NULL)
    {
        node = fileheader->RootNode;
    }

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_DL:
                rodata = node->Data;

                if (node != *nodeptr)
                {
                    gdl = rodata->DisplayList.Primary;
                }
                else if (rodata->DisplayList.Secondary != *gdlptr)
                {
                    gdl = rodata->DisplayList.Secondary;
                }
                break;

            case MODELNODE_OPCODE_DLCOLLISION:
                rodata = node->Data;

                if (node != *nodeptr)
                {
                    gdl = rodata->DisplayListCollisions.Primary;
                }
                else if (rodata->DisplayListCollisions.Secondary != *gdlptr)
                {
                    gdl = rodata->DisplayListCollisions.Secondary;
                }
                break;

            case MODELNODE_OPCODE_DLPRIMARY:
                rodata = node->Data;

                if (node != *nodeptr)
                {
                    gdl = rodata->DisplayListPrimary.Primary;
                }
                break;

            case MODELNODE_OPCODE_LOD:
                rodata = node->Data;
                node->Child = rodata->LOD.Affects;
                break;

            case MODELNODE_OPCODE_SWITCH:
                rodata = node->Data;
                node->Child = rodata->Switch.Controls;
                break;

            case MODELNODE_OPCODE_BSP:
                modelApplyReorderRelationsByArg(node, TRUE);
                break;
        }

        if (gdl) { break; }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }

    *gdlptr = gdl;
    *nodeptr = node;
}


void modelNodeReplaceGdl(ModelNode *node, Gfx *find, Gfx *replacement)
{
    union ModelRoData *rodata;
    u32 type = node->Opcode & 0xff;

    switch (type) {
        case MODELNODE_OPCODE_DL:
            rodata = node->Data;

            if (rodata->DisplayList.Primary == find)
            {
                rodata->DisplayList.Primary = replacement;
                return;
            }

            if (rodata->DisplayList.Secondary == find)
            {
                rodata->DisplayList.Secondary = replacement;
                return;
            }
            break;

        case MODELNODE_OPCODE_DLCOLLISION:
            rodata = node->Data;

            if (rodata->DisplayListCollisions.Primary == find)
            {
                rodata->DisplayListCollisions.Primary = replacement;
                return;
            }

            if (rodata->DisplayListCollisions.Secondary == find)
            {
                rodata->DisplayListCollisions.Secondary = replacement;
                return;
            }
            break;

        case MODELNODE_OPCODE_DLPRIMARY:
            rodata = node->Data;

            if (rodata->DisplayListPrimary.Primary == find)
            {
                rodata->DisplayListPrimary.Primary = replacement;
                return;
            }
            break;
    }
}


/**
 * How much to stretch a joint's matrix along z when it bends by twice `halfangle`, so that
 * the skin around an elbow or knee doesn't look pinched.
 */
f32 modelGetBendStretchScale(f32 halfangle) 
{
    return sqrtf(((sinf(halfangle) / cosf(halfangle)) + 1.0f));
}
