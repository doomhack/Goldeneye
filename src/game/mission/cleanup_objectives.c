#include <ultra64.h>
#include "bondtypes.h"
#include "game/mission/objective_status.h"


void objectivesFinish(void)
{
    int i;

    for (i = 0; i < OBJECTIVES_MAX; i++)
    {
        if (g_Objectives[i] != NULL && (g_Objectives[i]->unkD & 1) && g_ObjectiveStatuses[i] != OBJECTIVESTATUS_FAILED)
        {
            g_ObjectiveStatuses[i] = OBJECTIVESTATUS_COMPLETE;
        }
    }
}
