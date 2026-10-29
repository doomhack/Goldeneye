#ifndef _WAYPOINT_H_
#define _WAYPOINT_H_
#include <ultra64.h>
#include "bondtypes.h"

s32 waypointFindRoute(waypoint *frompoint, waypoint *topoint, waypoint **route, s32 maxLen);
waypoint *waypointFindNextStepToward(waypoint *pointa, waypoint *pointb);
void waypointsValidate(void);

#endif
