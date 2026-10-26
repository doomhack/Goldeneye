#ifndef _MODEL_H_
#define _MODEL_H_

#include <ultra64.h>
#include <bondtypes.h>
#include "bondconstants.h"


extern s32 g_MaxModelSlots;
extern s32 g_MaxAnimModelSlots;
extern s32 g_ModelIsLvResetting;

bool modelmgrCanSlotFitRwdata(Model *modelslot, ModelFileHeader *modeldef);
Model* modelmgrInstantiateModel(struct ModelFileHeader* arg0);
void modelFreeSlot(Model* model);
Model *modelmgrInstantiateModelWithAnim(ModelFileHeader *);
void modelAttachHead(Model *, ModelNode*,  ModelFileHeader *);
void modelFreeAnimSlot(Model *objinstance);
void modelSetLodDisabled(s32 param_1);
void modelSetLodScale(f32 param_1);
void modelSetVtxAllocator(s32 param_1);
void modelCalcDirToCamera(Model* model, coord3d* coord);
void modelGetDirToCamera(Model* model, coord3d* coord);
s32 modelFindNodeMtxIndex(ModelNode *node, s32 arg1);
Mtxf *modelFindNodeMtx(struct Model *model, struct ModelNode *node, s32 arg2);
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
f32 modelGetRadius(Model *arg0);
void modelLerp3(vec3d *v, vec3d *w, float frac);
f32 modelLerpAngle(f32 arg0, f32 angle, f32 mult);
void modelLerpAngles3(coord3d *arg0, coord3d *arg1, f32 mult);
u16 animReadRootMotion16(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, s32 frame, coord16 *out);
void modelCalcRootPosition(struct Model *arg0, struct ModelNode *arg1);
void modelCalcPosition(Model *arg0);
void modelCalcRootMtx(ModelRenderData* renderdata, Model* model, ModelNode* node);
void modelCalcJointMtx(ModelRenderData *arg0, Model *model, ModelNode *node);
void modelCalcYJointMtx(ModelRenderData *arg0, Model *model, ModelNode *node);
void modelCalcOffsetMtx(ModelRenderData* arg0, Model *model, ModelNode *node);
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
void modelUpdateMatrices(ModelRenderData *arg0, Model *model);
void modelCalcMatrices2(ModelRenderData* arg0, Model* arg1);
void modelCalcMatrices(ModelRenderData *arg0, struct Model *arg1);
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
void modelSetAnimSpeedToReachFrame(Model *model, f32 arg1, f32 arg2);
void modelSetAnimPlaySpeed(Model *model, f32 animation_rate, f32 startframe);
void modelSetRootMoveCallback(Model *model, s32 arg1);
void modelSetAnimFrame(Model* model, f32 frame);
void modelSetAnimFrame2(Model* model, f32 frame1, f32 frame2);
void modelSetAnimMergingEnabled(s32 arg0);
u32 modelIsAnimMergingEnabled(void);
void modelTickAnim(struct Model *model, s32 numticks, s32 update_chrstuff);
void modelApplyRenderModeType1(ModelRenderData *renderdata);
void modelApplyRenderModeType3(ModelRenderData *renderdata, bool isPrimary);
void modelApplyRenderModeType4(ModelRenderData *renderdata, bool isPrimary);
void modelApplyRenderModeType2(ModelRenderData *renderdata);
void modelApplyCullMode(ModelRenderData *renderdata);
void modelRenderNodeGundl(ModelRenderData* renderdata, ModelNode* arg1);
void modelRenderNodeDl(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelRenderNodeRotTex(ModelRenderData *renderdata, ModelNode *node);
void modelSelectTexture(ModelRenderData *renderdata, struct sImageTableEntry *tconfig, s32 arg2);
void modelRenderNodeOp05(s32 param_1,struct Model *param_2,struct ModelNode *param_3);
void modelRenderNodeTube(ModelRenderData* renderdata, Model* model, ModelNode* node);
void modelRenderNodeOp06(s32 param_1,struct Model *param_2, struct ModelNode *param_3);
void modelRenderNodeOp11(s32 param_1,struct Model *param_2,struct ModelNode *param_3);
void modelRenderNodeGunfire(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelSetShadowAlpha(s32 arg0);
void modelRenderNodeShadow(ModelRenderData *renderdata, Model *model, ModelNode *node);
void modelRenderNodeBbox(s32 param_1,struct Model *param_2,struct ModelNode *param_3);
void modelRenderNodeOp17(Gfx *param_1,struct Model *param_2, struct ModelNode *param_3);
void modelRenderNode(ModelRenderData* data, Model* model, ModelNode* node);
void modelRender(ModelRenderData *arg0, struct Model *);
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
void modelNodeReplaceGdl(u32 arg0, ModelNode *node, Gfx *find, Gfx *replacement);
bool modelTestRayIntersectsTransformedBBox(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx, coord3d *pos, coord3d *dir);

#ifndef VERSION_EU
void modelDebugBreak(void);
#endif

#endif

