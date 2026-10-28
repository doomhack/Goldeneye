#ifndef _MODEL_H_
#define _MODEL_H_

#include <ultra64.h>
#include <bondtypes.h>
#include "bondconstants.h"
#include "game/gfx/image.h"
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
extern struct TubeAngleRow g_ModelTubeAngleTable[];
extern Vertex g_ModelGunfireVtxTemplate;
extern Vtx g_ModelShadowVtxTemplate;


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

bool modelmgrCanSlotFitRwdata(Model *modelslot, ModelFileHeader *modeldef);
Model* modelmgrInstantiateModel(struct ModelFileHeader* header);
void modelFreeSlot(Model* model);
Model *modelmgrInstantiateModelWithAnim(ModelFileHeader *);
void modelAttachHead(Model *, ModelNode*,  ModelFileHeader *);
void modelFreeAnimSlot(Model *objinstance);
void modelSetLodDisabled(s32 disabled);
void modelSetLodScale(f32 scale);
void modelSetVtxAllocator(Vertex *(*allocator)(s32 numvertices));
void modelCalcDirToCamera(Model* model, coord3d* coord);
void modelGetDirToCamera(Model* model, coord3d* coord);
s32 modelFindNodeMtxIndex(ModelNode *node, s32 which);
Mtxf *modelFindNodeMtx(struct Model *model, struct ModelNode *node, s32 which);
Mtxf *modelGetRootMtx(Model *objinst);
f32 modelGetRootDepth(Model *objinst);
union ModelRwData* modelGetNodeRwData(Model *Objinst, ModelNode *root);
void modelGetNodePosition(Model *objinst, ModelNode *part, coord3d *offset);
void modelSetNodePosition(Model *model, ModelNode *node, coord3d *pos);
void modelGetRootPosition(Model *objinst, coord3d *offset);
void modelSetRootPosition(Model *objinst, coord3d *offset);
f32 modelGetRootRotY(Model *objinst);
void modelSetRootRotY(Model *model, f32 angle);
void modelSetScale(Model *objinst, f32 scale);
void modelSetAnimTranslationScale(Model* model, f32 scale);
f32 modelGetNodeRadius(Model *model, ModelNode *node);
f32 modelGetRadius(Model *model);
void modelLerp3(vec3d *v, vec3d *w, float frac);
f32 modelLerpAngle(f32 from, f32 angle, f32 mult);
void modelLerpAngles3(coord3d *dst, coord3d *target, f32 mult);
u16 animReadRootMotion16(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, s32 frame, coord16 *out);
void modelCalcRootPosition(struct Model *model, struct ModelNode *modelNode);
void modelCalcPosition(Model *model);
void modelCalcRootMtx(ModelRenderData* renderdata, Model* model, ModelNode* node);
void modelCalcJointMtx(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelCalcYJointMtx(ModelRenderData *renderData, Model *model, ModelNode *node);
void modelCalcOffsetMtx(ModelRenderData* renderdata, Model *model, ModelNode *node);
void modelUpdateDistanceRelations(Model* model, ModelNode* node);
void modelApplyDistanceRelations(Model* model, ModelNode* node);
void modelApplyToggleRelations(Model* model, ModelNode* node);
void modelApplyHeadRelations(Model* model, ModelNode* bodynode);
void modelApplyReorderRelationsByArg(ModelNode *basenode, bool visible);
void modelApplyReorderRelations(Model* model, ModelNode* node);
void modelUpdateReorderRelations(Model *model, ModelNode *node);
void modelUpdateTubeIndex(Model *model, ModelNode *node);
void modelUpdateRelationsQuick(Model *model, ModelNode *parent);
void modelUpdateNodeRelations(Model *model);
void modelUpdateMatrices(ModelRenderData *renderdata, Model *model);
void modelCalcMatricesNoAnim(ModelRenderData* renderdata, Model* model);
void modelCalcMatrices(ModelRenderData *renderdata, struct Model *model);
struct ModelAnimation * modelGetAnim(struct Model* model);
s8 modelGetAnimFlip(Model *model);
f32 modelGetAnimFrame(Model *model);
f32 modelGetAnimEndFrame(Model *model);
f32 modelGetAnimSpeed(Model *model);
f32 modelGetAbsAnimSpeed(Model *model);
s32 modelConstrainOrWrapAnimFrame(s32 frame, ModelAnimation *anim, f32 endframe);
void modelCopyAnimForMerge(Model *model, f32 timemerge);
void modelSetAnimation2(Model *, ModelAnimation *, s32, f32, f32, f32);
void modelSetAnimationWithMerge(Model *model, ModelAnimation *modelAnimation, s32 flip, f32 startframe, f32 speed, f32 timemerge, s32 domerge);
void modelSetAnimation(Model *model, ModelAnimation *modelAnimation, s32 flip, f32 startframe, f32 speed, f32 merge);
void modelSetAnimLooping(Model *model, f32 loopframe, f32 loopmerge);
void modelSetAnimEndFrame(Model *model, f32 endframe);
void modelSetAnimFlipFunction(Model *model, void *callback);
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
void modelRenderNodeGundl(ModelRenderData* renderdata, ModelNode* node);
void modelRenderNodeDl(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelRenderNodeRotTex(ModelRenderData *renderdata, ModelNode *node);
void modelSelectTexture(ModelRenderData *renderdata, struct sImageTableEntry *tconfig, s32 blendMode);
void modelRenderNodeOp05(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelRenderNodeTube(ModelRenderData* renderdata, Model* model, ModelNode* node);
void modelRenderNodeOp06(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelRenderNodeOp11(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelRenderNodeGunfire(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelSetShadowAlpha(s32 alpha);
void modelRenderNodeShadow(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelRenderNodeOp17(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelRenderNode(ModelRenderData* data, Model* model, ModelNode* node);
void modelRender(ModelRenderData *mrData, struct Model *);
s32 animLoadFrame(ModelAnimation* anim, s32 frame, ModelSkeleton* unused);
void animResetFrameBuffer(void);
void modelPromoteNodeOffsetsToPointers(ModelNode *node, u32 vma, u32 fileramaddr);
void modelFilePromotePointers(ModelFileHeader *header, s32 vma, u32 addr);
s32 modelCalculateRwDataIndexes(ModelNode *basenode);
void modelCalculateRwDataLen(struct ModelFileHeader *objheader);
void modelInitRwData(Model *model, ModelNode *startnode);
void modelInit(struct Model *objinst, struct ModelFileHeader *header, u32 *data);
void animInit(struct Model *objinst, struct ModelFileHeader *header, u32 *data);
void modelAttachPart(Model *pmodel, ModelFileHeader *pmodeldef, ModelNode *pnode, ModelFileHeader *cmodeldef);
void modelIterateDisplayLists(ModelFileHeader *fileheader, ModelNode **nodeptr, Gfx **gdlptr);
void modelNodeReplaceGdl(u32 unused, ModelNode *node, Gfx *find, Gfx *replacement);
bool modelTestRayIntersectsTransformedBBox(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx, coord3d *pos, coord3d *dir);

bool modelTestRayIntersectsNodeBBox(Model *model, ModelNode *node, coord3d *pos, coord3d *dir);
s32 modelTestRayIntersectsNodeOp17(Model *model, ModelNode *node, coord3d *raypos, coord3d *raydir);
u32 modelFindRayHitNext(Model *model, coord3d *raypos, coord3d *raydir, ModelNode **nodeptr);

#endif

