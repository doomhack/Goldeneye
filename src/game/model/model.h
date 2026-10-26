#ifndef _MODEL_H_
#define _MODEL_H_

#include <ultra64.h>
#include <bondtypes.h>
#include "bondconstants.h"
#include "game/gfx/image.h"
#include "game/model/anim.h"

struct bondstruct_unk_op07_related {
    s32 unk00;
    s32 unk04;
    s32 unk0C;
};

struct AnimModelSlot {
    s16 unk00;
    s16 rwdatalen;
    s32 unk04;
    s32 obj;
    s32 unk0c;
    void *datas;
    s32 unk14;
    s32 unk18;
    s32 unk1c;
    s32 unk20;
    s32 unk24;
    s32 unk28;
    s32 unk2c;
    s32 unk30;
    s32 unk34;
    s32 unk38;
    s32 unk3c;
    s32 unk40;
    s32 unk44;
    s32 unk48;
    s32 unk4c;
    s32 unk50;
    s32 unk54;
    s32 unk58;
    s32 unk5c;
    s32 unk60;
    s32 unk64;
    s32 unk68;
    s32 unk6c;
    s32 unk70;
    s32 unk74;
    s32 unk78;
    s32 unk7c;
    s32 unk80;
    s32 unk84;
    s32 unk88;
    s32 unk8c;
    s32 unk90;
    s32 unk94;
    s32 unk98;
    s32 unk9c;
    s32 unka0;
    s32 unka4;
    s32 unka8;
    s32 unkac;
    s32 unkb0;
    s32 unkb4;
    s32 unkb8;

    // is this struct size 0xbc or 0xc0 ?
    //s32 unkbc;
};

struct ModelSlot {
    s16 unk00;
    s16 rwdatalen;
    s32 unk04;
    s32 obj;
    s32 unk0c;
    void *datas;
    s32 unk14;
    s32 unk18;
    s32 unk1c;
};

extern struct AnimModelSlot *g_AnimModelSlots;
extern struct ModelSlot *g_ModelSlots;

extern s32 g_ModelLodDisabled;
extern f32 g_ModelLodScale;
extern u32 g_ModelAnimMergingEnabled;
extern s32 D_80036410;
extern s32 D_80036418;
extern s32 D_8003641C;
extern u32 g_ModelShadowAlpha;

extern coord3d D_80036094;
extern coord3d D_800360A0;
extern coord3d D_800360AC;
extern coord3d D_800360B8;
extern coord3d D_80036244;
extern coord3d D_80036254;

extern struct Vertex* (*g_ModelVtxAllocator)(s32 numvertices);
extern void (*g_ModelJointPositionedFunc)(s32 mtxindex, Mtxf *mtx);
extern struct bondstruct_unk_op07_related g_ModelTubeAngleTable[];
extern Vertex D_800363E0;
extern Vtx D_800363F8;
extern coord3d D_80036408;


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
void modelCalcMatricesNoAnim(ModelRenderData* arg0, Model* arg1);
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

