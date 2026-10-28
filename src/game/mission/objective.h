#ifndef _OBJECTIVE_H_
#define _OBJECTIVE_H_
#include <ultra64.h>
#include <bondtypes.h>

extern struct objective_entry * g_Objectives[OBJECTIVES_MAX];
extern  OBJECTIVESTATUS g_ObjectiveStatuses[OBJECTIVES_MAX]; //This is an array of 10 OBJECTIVESTATUS,

extern struct TagObjectRecord *g_Tags;
extern struct watchMenuObjectiveText *g_BriefingTexts;
extern struct criteria_roomentered *g_RoomEnteredCriteria;
extern struct criteria_deposit *g_DepositCriteria;
extern struct criteria_picture *g_PictureCriteria;

extern s32 g_ObjectiveLastIndex;
extern s32 g_ObjectiveStatusDisplayDisabled;

void             objectivesCheckPicture(void);
bool             objectivesAllComplete(void);
TagObjectRecord *tagFind(s32 TagID);
s32              objectiveGetCount(void);
ObjectRecord *   objFindByTagId(s32 TagID);
OBJECTIVESTATUS  objectiveCheckStatus(s32 objectiveNum);
void objectivesCheckDeposit(s32 weaponnum, s32 roomid);
void objectivesTickStatusMessages(void);
void objectivesDisableStatusDisplay(void);

void objectivesFinish(void);

void objectivesReset(void);

void tagAdd(struct TagObjectRecord *tag);
void briefingTextAdd(struct watchMenuObjectiveText *text);
void objectiveAddRoomEntered(struct criteria_roomentered *criteria);
void objectiveAddDeposit(struct criteria_deposit *criteria);
void objectiveAddPicture(struct criteria_picture *criteria);
void objectiveAdd(struct objective_entry* objective);

u8 * briefingGetText(WATCH_BRIEFING_PAGE page);
s32 objectiveGetDifficulty(s32 objectiveIndex);
char *objectiveGetText(int objectiveIndex);
void objectivesCheckRoomEntered(s32 roomid);

#endif
