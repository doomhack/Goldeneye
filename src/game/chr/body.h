#ifndef __BODY_H__
#define __BODY_H__

#include <ultra64.h>
#include <bondtypes.h>

Model *bodyCreateModel(s32 body, s32 head, ModelFileHeader *bodyheader, ModelFileHeader *headheader, s32 sunglasses, Model *model);
Model *bodyCreateModelNew(s32 body, s32 head, ModelFileHeader *body_header, ModelFileHeader *head_header, s32 sunglasses);
Model * bodyCreateModelWithFlags(s32 body, s32 head, u32 bitflags);
s32 bodyGetRandom(void);
s32 bodyChooseHead(s32 id);
s32 bodyGetRandomHead(s32 id);
void bodiesReset(u32 stagenum);

#endif
