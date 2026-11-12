#ifndef _MODEL_H_
#define _MODEL_H_

#include <ultra64.h>
#include <bondtypes.h>
#include "bondconstants.h"
#include "game/gfx/texload.h"
#include "game/model/anim.h"

/* One row per 1/64 turn of pitch towards the camera: which of the tube's view entries to use for each yaw */
struct TubeAngleRow {
    s32 first; // first view entry of this row
    s32 round; // added to the 16-bit yaw before shifting
    s32 shift; // yaw >> shift = entry within the row
};

extern Model *g_AnimModelSlots;
extern ModelBase *g_ModelSlots;

extern s32 g_ModelLodDisabled;
extern f32 g_ModelLodScale;
extern u32 g_ModelAnimMergingEnabled;
extern u32 g_ModelShadowAlpha;


extern struct Vertex* (*g_ModelVtxAllocator)(s32 numvertices);
extern void (*g_ModelJointPositionedFunc)(s32 mtxindex, Mtxf *mtx);
extern const struct TubeAngleRow g_ModelTubeAngleTable[];
extern const Vertex g_ModelGunfireVtxTemplate;
extern const Vtx g_ModelShadowVtxTemplate;


// tentative signature
PropRecord *chrGiveWeapon(ChrRecord *self, s32 PropID, ITEM_IDS ItemID, s32 flags);

void modelmgrSetLevelResetting(bool resetting);
void modelmgrResetSlotCounts(void);
void modelmgrAllocateModelSlots(s32 numobjs);
void modelmgrAllocateAnimModelSlots(s32 numanimated);
f32 modelGetBendStretchScale(f32 halfangle);



extern s32 g_MaxModelSlots;
extern s32 g_MaxAnimModelSlots;
extern s32 g_ModelIsLvResetting;

bool modelmgrCanSlotFitRwdata(const Model *modelslot, const ModelFileHeader *modeldef);
Model* modelmgrInstantiateModel(struct ModelFileHeader* header);
void modelFreeSlot(Model* model);
Model *modelmgrInstantiateModelWithAnim(ModelFileHeader *);
void modelAttachHead(const Model *, ModelNode*,  ModelFileHeader *);
void modelFreeAnimSlot(Model *objinstance);
void modelSetLodDisabled(s32 disabled);
void modelSetLodScale(f32 scale);
void modelSetVtxAllocator(Vertex *(*allocator)(s32 numvertices));
void modelCalcDirToCamera(const Model* model, coord3d* coord);
void modelGetDirToCamera(const Model* model, coord3d* coord);
s32 modelFindNodeMtxIndex(const ModelNode *node, s32 which);
Mtxf *modelFindNodeMtx(const struct Model *model, const struct ModelNode *node, s32 which);
Mtxf *modelGetRootMtx(const Model *objinst);
f32 modelGetRootDepth(const Model *objinst);
union ModelRwData* modelGetNodeRwData(const Model *Objinst, const ModelNode *root);
union ModelRwData *modelGetAttachedRwData(const Model *model, const ModelNode *node, s32 index);

/*
 * A node's RwDataIndex is a word index into its model's rwdata. The top bit marks nodes of a
 * model that has been attached under a HEAD node (modelAttachPart): their index counts from
 * that HEAD's RwDatas block instead, which needs a walk up the tree to find. Every other node
 * can never have a HEAD above it, so its record is simply model->datas[index].
 */
#define MODEL_RWINDEX_ATTACHED 0x8000
#define MODEL_RWINDEX(i)       ((i) & ~MODEL_RWINDEX_ATTACHED)

static inline union ModelRwData *modelGetRwDataByIndex(const Model *model, const ModelNode *node, u32 index)
{
    if (__builtin_expect((index & MODEL_RWINDEX_ATTACHED) == 0, 1))
    {
        return (union ModelRwData *)&model->datas[index];
    }

    return modelGetAttachedRwData(model, node, MODEL_RWINDEX(index));
}

/* Record of a node whose type is known, e.g. modelGetNodeRwDataOf(model, node, Switch). */
#define modelGetNodeRwDataOf(model, node, Type) \
    (&modelGetRwDataByIndex((model), (node), (node)->Data->Type.RwDataIndex)->Type)
void modelGetNodePosition(const Model *objinst, const ModelNode *part, coord3d *offset);
void modelSetNodePosition(const Model *model, const ModelNode *node, const coord3d *pos);
void modelGetRootPosition(const Model *objinst, coord3d *offset);
void modelSetRootPosition(const Model *objinst, const coord3d *offset);
f32 modelGetRootRotY(const Model *objinst);
void modelSetRootRotY(const Model *model, f32 angle);
void modelSetScale(Model *objinst, f32 scale);
void modelSetAnimTranslationScale(Model* model, f32 scale);
f32 modelGetNodeRadius(const Model *model, const ModelNode *node);
f32 modelGetRadius(const Model *model);
void modelLerp3(coord3d *v, const coord3d *w, float frac);
f32 modelLerpAngle(f32 from, f32 angle, f32 mult);
void modelLerpAngles3(coord3d *dst, const coord3d *target, f32 mult);
u16 animReadRootMotion16(s32 jointnum, s32 flip, const ModelSkeleton *skeleton, const ModelAnimation *anim, s32 frame, coord16 *out);
void modelCalcRootPosition(struct Model *model, const struct ModelNode *modelNode);
void modelCalcPosition(Model *model);
void modelCalcRootMtx(const ModelRenderData* renderdata, const Model* model, const ModelNode* node);
void modelCalcJointMtx(const ModelRenderData *renderdata, const Model *model, const ModelNode *node);
void modelCalcYJointMtx(const ModelRenderData *renderData, const Model *model, const ModelNode *node);
void modelCalcOffsetMtx(const ModelRenderData* renderdata, const Model *model, const ModelNode *node);
void modelUpdateDistanceRelations(const Model* model, ModelNode* node);
void modelApplyDistanceRelations(const Model* model, ModelNode* node);
void modelApplyToggleRelations(const Model* model, ModelNode* node);
void modelApplyHeadRelations(const Model* model, ModelNode* bodynode);
void modelApplyReorderRelationsByArg(ModelNode *basenode, bool visible);
void modelApplyReorderRelations(const Model* model, ModelNode* node);
void modelUpdateReorderRelations(const Model *model, ModelNode *node);
void modelUpdateTubeIndex(const Model *model, const ModelNode *node);
void modelUpdateRelationsQuick(const Model *model, const ModelNode *parent);
void modelUpdateNodeRelations(const Model *model);
void modelUpdateMatrices(const ModelRenderData *renderdata, const Model *model);
void modelCalcMatricesNoAnim(ModelRenderData* renderdata, Model* model);
void modelCalcMatrices(ModelRenderData *renderdata, struct Model *model);
struct ModelAnimation * modelGetAnim(const struct Model* model);
s8 modelGetAnimFlip(const Model *model);
f32 modelGetAnimFrame(const Model *model);
f32 modelGetAnimEndFrame(const Model *model);
f32 modelGetAnimSpeed(const Model *model);
f32 modelGetAbsAnimSpeed(const Model *model);
s32 modelConstrainOrWrapAnimFrame(s32 frame, const ModelAnimation *anim, f32 endframe);
void modelCopyAnimForMerge(Model *model, f32 timemerge);
void modelSetAnimation2(Model *, ModelAnimation *, s32, f32, f32, f32);
void modelSetAnimationWithMerge(Model *model, ModelAnimation *modelAnimation, s32 flip, f32 startframe, f32 speed, f32 timemerge, s32 domerge);
void modelSetAnimation(Model *model, ModelAnimation *modelAnimation, s32 flip, f32 startframe, f32 speed, f32 merge);
void modelSetAnimLooping(Model *model, f32 loopframe, f32 loopmerge);
void modelSetAnimEndFrame(Model *model, f32 endframe);
void modelSetAnimFlipFunction(Model *model, void (*callback)(void));
void modelSetAnimSpeed(Model *model, f32 anim_speed, f32 startframe);
void modelSetAnimSpeedToReachFrame(Model *model, f32 targetframe, f32 duration);
void modelSetAnimPlaySpeed(Model *model, f32 g_ChrAnimRate, f32 startframe);
void modelSetRootMoveCallback(Model *model, s32 callback);
void modelSetAnimFrame(Model* model, f32 frame);
void modelSetAnimFrame2(Model* model, f32 frame1, f32 frame2);
void modelSetAnimMergingEnabled(s32 enabled);
u32 modelIsAnimMergingEnabled(void);
void modelTickAnim(struct Model *model, s32 numticks, s32 update_chrstuff);
void modelApplyRenderModeType1(ModelRenderData *renderdata);
void modelApplyRenderModeType3(ModelRenderData *renderdata, bool isPrimary);
void modelApplyRenderModeType4(ModelRenderData *renderdata, bool isPrimary);
void modelApplyRenderModeType2(ModelRenderData *renderdata);
void modelApplyCullMode(ModelRenderData *renderdata);
void modelRenderNodeGundl(ModelRenderData* renderdata, const ModelNode* node);
void modelRenderNodeDl(ModelRenderData *renderdata, const Model *model, const ModelNode *node);
void modelRenderNodeRotTex(ModelRenderData *renderdata, const ModelNode *node);
void modelSelectTexture(ModelRenderData *renderdata, struct sImageTableEntry *tconfig, s32 blendMode);
void modelRenderNodeTube(ModelRenderData* renderdata, const Model* model, const ModelNode* node);
void modelRenderNodeGunfire(ModelRenderData *renderdata, const Model *model, const ModelNode *node);
void modelSetShadowAlpha(s32 alpha);
void modelRenderNodeShadow(ModelRenderData *renderdata, const Model *model, const ModelNode *node);
void modelRenderNode(ModelRenderData* data, const Model* model, ModelNode* node);
void modelRender(ModelRenderData *mrData, struct Model *);
u8 *animGetFrame(const ModelAnimation *anim, s32 frame);
void modelPromoteNodeOffsetsToPointers(ModelNode *node, u32 vma, u32 fileramaddr);
void modelFilePromotePointers(const ModelFileHeader *header, s32 vma, const void *addr);
s32 modelCalculateRwDataIndexes(ModelNode *basenode, u32 attached);
void modelCalculateRwDataLen(struct ModelFileHeader *objheader);
void modelInitRwData(const Model *model, ModelNode *startnode);
void modelInit(struct Model *objinst, struct ModelFileHeader *header, u32 *data);
void animInit(struct Model *objinst, struct ModelFileHeader *header, u32 *data);
void modelAttachPart(const Model *pmodel, ModelFileHeader *pmodeldef, ModelNode *pnode, ModelFileHeader *cmodeldef);
void modelIterateDisplayLists(const ModelFileHeader *fileheader, ModelNode **nodeptr, Gfx **gdlptr);
void modelNodeReplaceGdl(const ModelNode *node, const Gfx *find, Gfx *replacement);
bool modelTestRayIntersectsTransformedBBox(const ModelRoData_BoundingBoxRecord *bbox, const Mtxf *mtx, const coord3d *pos, const coord3d *dir);

bool modelTestRayIntersectsNodeBBox(const Model *model, const ModelNode *node, const coord3d *pos, const coord3d *dir);
s32 modelTestRayIntersectsNodeOp17(const Model *model, const ModelNode *node, const coord3d *raypos, const coord3d *raydir);
u32 modelFindRayHitNext(const Model *model, const coord3d *raypos, const coord3d *raydir, ModelNode **nodeptr);

#endif

