#ifndef _OBJECTHANDLER_
#define _OBJECTHANDLER_
#include <ultra64.h>
#include <bondtypes.h>
#include "game/gfx/image.h"

struct bondstruct_unk_animation_related {
    char* uselessPointer; // Is incremented like a count when an animation is copied from ROM to RAM but it's never read
    char* animBufferPtr1; 
    char* animBufferPtr2; 
};

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

extern struct JointListEntry *g_JointListFree;
extern s32 g_ModelLodDisabled;
extern f32 g_ModelLodScale;
extern u32 g_ModelAnimMergingEnabled;
extern s32 D_80036410;
extern struct bondstruct_unk_animation_related* g_AnimFrameBuffer;
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

void modelFileLoadToPool(ModelFileHeader *header,char *name);
void modelFileLoadToAddr(ModelFileHeader *header,int *recallstring,int *targetloc,int sizeleft);

// tentative signature
PropRecord *chrGiveWeapon(ChrRecord *self, s32 PropID, ITEM_IDS ItemID, s32 flags);

// called with struct ChrRecord->field_20
JointListEntry* jointListAddModel(JointListEntry* head, Model* context);
void jointListFree(JointListEntry *entry);
void jointListDraw(ModelRenderData *arg0, JointListEntry *entry);
void jointListCalcDepths(JointListEntry *arg0);
JointListEntry *jointListSortByDepth(JointListEntry *modelhit);
s32 jointListFindHitNext(JointListEntry **entryptr, coord3d *raypos, coord3d *raydir, Model **outModel, ModelNode **inoutNode);
s32 jointListFindHit(JointListEntry **entryptr, coord3d *modelRayStart, coord3d *modelRayDir, Model **outModel, ModelNode **outNode);

void modelFileLoad(struct ModelFileHeader *objheader, u8 *name, u8* dst, s32 size, struct texpool * buffer);


#endif
