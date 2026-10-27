#include <ultra64.h>
#include <boss.h>
#include "game/mission/objective.h"
#include "game/mission/objective_status.h"



/*
 * Clears Objectives and states.
 *
 * NTSC address 0x7F004EC0.
 */
void objectivesReset(void)
{
    s32 i;

    OBJECTIVESTATUS *start;
    OBJECTIVESTATUS *end;

    if (bossGetStageNum() != LEVELID_TITLE)
    {
        g_ObjectiveLastIndex = -1;

        for (i = 0; i < OBJECTIVES_MAX; i++)
        {
            g_ObjectiveStatuses[i] = OBJECTIVESTATUS_INCOMPLETE;
        }
    }

#if defined(VERSION_EU) || defined(BACKPORT_R1)
    g_ObjectiveStatusDisplayDisabled = FALSE;
#endif

    for (i = 0; i < OBJECTIVES_MAX; i++)
    {
        g_Objectives[i] = NULL;
    }
    
    g_Tags = NULL;
    g_BriefingTexts = NULL;
    g_RoomEnteredCriteria = NULL;
    g_DepositCriteria = NULL;
    g_PictureCriteria = NULL;
}

void tagAdd(struct TagObjectRecord *arg0)
{
    arg0->NextTag = g_Tags;
    g_Tags = arg0;
}

void briefingTextAdd(struct setup_objective_text *arg0)
{
    arg0->next = g_BriefingTexts;
    g_BriefingTexts = arg0;
}


/*
* Address: 0x7f004fb8
*/
void objectiveAdd(struct objective_entry* objective)
{
    g_Objectives[objective->menu] = objective;

    if (g_ObjectiveLastIndex < objective->menu)
    {
        g_ObjectiveLastIndex = objective->menu;
    }
}

void objectiveAddRoomEntered(struct criteria_roomentered *arg0)
{
    arg0->next = g_RoomEnteredCriteria;
    g_RoomEnteredCriteria = arg0;
}

void objectiveAddDeposit(struct criteria_deposit *arg0)
{
    arg0->next = g_DepositCriteria;
    g_DepositCriteria = arg0;
}


void objectiveAddPicture(struct criteria_picture *arg0)
{
    arg0->next = g_PictureCriteria;
    g_PictureCriteria = arg0;
}

