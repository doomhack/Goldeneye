#ifndef _WAYPOINT_H_
#define _WAYPOINT_H_
#include <ultra64.h>
#include "bondtypes.h"

void waygroupFindLinkPoints(waygroup *groupa, waygroup *groupb, waypoint **pointa, waypoint **pointb);

void waypointsValidate(void);

#endif
