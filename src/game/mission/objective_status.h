#ifndef _OBJECTIVE_STATUS_H_
#define _OBJECTIVE_STATUS_H_
#include <ultra64.h>
#include <bondtypes.h>

extern struct objective_entry * g_Objectives[OBJECTIVES_MAX];
extern  OBJECTIVESTATUS g_ObjectiveStatuses[OBJECTIVES_MAX]; //This is an array of 10 OBJECTIVESTATUS,

extern u32 *g_Tags;
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
#endif
