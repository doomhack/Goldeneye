/**
 * The path graph guards follow. Waypoints sit on pads and link to their neighbours;
 * waypoints are also grouped, and the groups link to each other. A route is found in two
 * steps: a breadth-first search over the groups (waygroupsMarkRoute), then within each
 * group over its waypoints (waypointFindRouteInGroup). Both searches number the nodes by
 * distance in a dist field, then follow the numbers from one end to the other.
 */

#include <ultra64.h>
#include "game/nav/waypoint.h"
#include "bondtypes.h"
#include "game/chr/chrai.h"
#include "random.h"


waygroup *waygroupFindByDist(s32 *groupnums, s32 value)
{
    waygroup *groups = g_CurrentSetup.waypointgroups;

    while (*groupnums >= 0)
    {
        waygroup *group = &groups[*groupnums];

        if (group->dist == value)
        {
            return group;
        }

        groupnums++;
    }

    return NULL;
}


/**
 * For each group number in the given list assign value to their
 * dist if their dist has no value (ie. is negative).
 */
void waygroupsSetUnvisitedDist(s32 *groupnums, s32 value)
{
    waygroup *groups = g_CurrentSetup.waypointgroups;

    while (*groupnums >= 0)
    {
        waygroup *group = &groups[*groupnums];

        if (group->dist < 0)
        {
            group->dist = value;
        }

        groupnums++;
    }
}


/**
 * Iterate the given groups and find any with a dist matching dist.
 * For all groups that match, iterate their neighbouring groups and set their
 * dist to dist + 1, but only if their they have no existing dist value.
 *
 * Return true if any matched.
 */
bool waygroupsPropagateDist(struct waygroup *group, s32 dist)
{
    bool result = FALSE;

    while (group->neighbours)
    {
        if (group->dist == dist)
        {
            result = TRUE;
            waygroupsSetUnvisitedDist(group->neighbours, dist + 1);
        }

        group++;
    }

    return result;
}


bool waygroupsFloodDist(struct waygroup *from, struct waygroup *to, struct waygroup *groups, s32 all)
{
    bool result = TRUE;
    struct waygroup *group;
    s32 i;

    for (group = groups; group->neighbours; group++)
    {
        group->dist = -1;
    }

    from->dist = 0;

    for (i = 0; (all || to->dist < 0) && result; i++)
    {
        result = waygroupsPropagateDist(groups, i);
    }

    return result;
}


bool waygroupsMarkRoute(waygroup *from, waygroup *to, waygroup *groups)
{
    bool result = waygroupsFloodDist(from, to, groups, 0);

    if (result)
    {
        waygroup *curto = to;
        s32 i = curto->dist - 1;

        while (i >= 0)
        {
            curto->dist += 10000;
            curto = waygroupFindByDist(curto->neighbours, i);
            i--;
        }

        curto->dist += 10000;
    }

    return result;
}


waypoint *waypointFindByDist(s32 *pointnums, s32 dist, s32 groupnum)
{
    waypoint *points = g_CurrentSetup.pathwaypoints;

    while (*pointnums >= 0)
    {
        waypoint *point = &points[*pointnums];

        if (point->groupNum == groupnum && point->dist == dist)
        {
            return point;
        }

        *pointnums++;
    }

    return NULL;
}


void waypointsSetUnvisitedDist(s32 *pointnums, s32 value, s32 groupnum)
{
    waypoint *waypoints = g_CurrentSetup.pathwaypoints;

    while (*pointnums >= 0)
    {
        waypoint *waypoint = &waypoints[*pointnums];

        if (waypoint->groupNum == groupnum && waypoint->dist < 0)
        {
            waypoint->dist = value;
        }

        *pointnums++;
    }
}


bool waypointsPropagateDist(s32 *pointnums, s32 dist, s32 groupnum)
{
    bool result = FALSE;
    waypoint *points = g_CurrentSetup.pathwaypoints;

    while (*pointnums >= 0)
    {
        waypoint *point = &points[*pointnums];

        if (dist == point->dist && point->neighbours)
        {
            result = TRUE;
            waypointsSetUnvisitedDist(point->neighbours, dist + 1, groupnum);
        }

        pointnums++;
    }

    return result;
}


void waypointsFloodDist(waypoint *from, waypoint *to, s32 all)
{
    waygroup *groups = g_CurrentSetup.waypointgroups;
    waypoint *points = g_CurrentSetup.pathwaypoints;
    waypoint *point;
    s32 *pointnums = groups[from->groupNum].waypoints;
    s32 i;
    bool more;

    while (*pointnums >= 0)
    {
        point = &points[*pointnums];
        point->dist = -1; // reset waypoint
        pointnums++;
    }

    from->dist = 0;

    more = TRUE;

    for (i = 0; (all || to->dist < 0) && more; i++)
    {
        more = waypointsPropagateDist(groups[from->groupNum].waypoints, i, from->groupNum);
    }
}


void waypointMarkRoute(waypoint *from, waypoint *to)
{
    waypoint *curto;
    s32 value;

    waypointsFloodDist(from, to, 0);

    value = to->dist - 1;
    curto = to;

    while (value >= 0)
    {
        curto->dist += 10000;
        curto = waypointFindByDist(curto->neighbours, value, from->groupNum);

        value--;
    }

    curto->dist += 10000;
}


s32 waypointFindRouteInGroup(waypoint *from, waypoint *to, waypoint **arr, s32 arrlen)
{
    waypoint **arrptr = arr;
    waypoint *curfrom;
    s32 i;

    if (arrlen >= 2)
    {
        waypointMarkRoute(from, to);

        *arr = from;
        arrptr++;

        curfrom = from;
        arrlen += 9999;
        i = 10001;

        while (i <= to->dist && i < arrlen)
        {
            curfrom = waypointFindByDist(curfrom->neighbours, i, from->groupNum);
            *arrptr = curfrom;
            arrptr++;
            i++;
        }
    }

    *arrptr = NULL;
    arrptr++;

    return arrptr - arr;
}


void waygroupFindLinkPoints(waygroup *groupa, waygroup *groupb, waypoint **pointa, waypoint **pointb)
{
    waypoint *points = g_CurrentSetup.pathwaypoints;
    waygroup *groups = g_CurrentSetup.waypointgroups;
    s32 *groupapointnums = groupa->waypoints;

    while (*groupapointnums >= 0)
    {
        waypoint *groupapoint = &points[*groupapointnums];
        s32 *neighbournums = groupapoint->neighbours;

        while (*neighbournums >= 0)
        {
            waypoint *neighbour = &points[*neighbournums];

            if (groupb == &groups[neighbour->groupNum])
            {
                *pointa = groupapoint;
                *pointb = neighbour;
                return;
            }

            neighbournums++;
        }

        groupapointnums++;
    }
    *pointb = NULL;
    *pointa = NULL;
}


/**
 * Find a route from frompoint to topoint. The arr argument will be populated
 * with pointers to the route's waypoints. If arr is not big enough then only
 * the first part of the route will be populated into the array.
 *
 * The return value is the number of elements populated into the array.
 */
s32 waypointFindRoute(waypoint *frompoint, waypoint *topoint, waypoint **arr, s32 arrlen)
{
    waypoint **arrptr = arr;
    waygroup *groups = g_CurrentSetup.waypointgroups;

    if (groups)
    {
        waygroup *fromgroup = &groups[frompoint->groupNum];
        waygroup *togroup = &groups[topoint->groupNum];

        if (waygroupsMarkRoute(fromgroup, togroup, groups))
        {
            waypoint *curfrompoint = frompoint;
            waygroup *curfromgroup = fromgroup;
            s32 i;

            for (i = fromgroup->dist + 1; i <= togroup->dist && arrlen >= 2; i++)
            {
                s32 numwritten;
                waygroup *nextfromgroup = waygroupFindByDist(curfromgroup->neighbours, i);
                waypoint *tmppoint;
                waypoint *nextfrompoint;

                waygroupFindLinkPoints(curfromgroup, nextfromgroup, &tmppoint, &nextfrompoint);
                numwritten = waypointFindRouteInGroup(curfrompoint, tmppoint, arrptr, arrlen) - 1;

                arrlen -= numwritten;
                arrptr += numwritten;

                curfrompoint = nextfrompoint;
                curfromgroup = nextfromgroup;
            }

            arrptr += waypointFindRouteInGroup(curfrompoint, topoint, arrptr, arrlen) - 1;
        }
    }

    *arrptr = NULL;
    arrptr++;

    return arrptr - arr;
}


// Resets waypoint distance, seems useless since
// waypointsFloodDist already does this for its own
// waypoint group. Even if waypointsFloodDist doesn't
// reset waypoints outside of its own group, is that really needed?
void waypointsResetDist(void)
{
    waypoint * waypoint;
    waypoint = g_CurrentSetup.pathwaypoints;

    while (waypoint->padID >= 0)
    {
        waypoint->dist = -1;
        waypoint++;
    }
}


waypoint *waypointFindRandomByDist(s32 *pointnums, s32 dist)
{
	s32 len = 0;
	s32 randomindex;
	s32 i;

	while (pointnums[len] >= 0)
    {
		len++;
	}

	// This is effectively randomly dividing the pointnums list into two,
	// then checking the upper portion before the lower portion. Both loops
	// have the same logic so this seems unusual, but there is reason to do
	// this if they want the returned waypoint to be any random waypoint that
	// meets the dist criteria, with equal weighting.

	randomindex = randomGetNext() % len;

	for (i = randomindex; i < len; i++)
    {
		waypoint *point = &g_CurrentSetup.pathwaypoints[pointnums[i]];

		if (point->dist == dist)
        {
			return point;
		}
	}

	for (i = 0; i < randomindex; i++)
    {
		waypoint *point = &g_CurrentSetup.pathwaypoints[pointnums[i]];

		if (point->dist == dist)
        {
			return point;
		}
	}

	return NULL;
}


waygroup *waygroupFindRandomByDist(s32 *groupnums, s32 dist)
{
    s32 len = 0;
    s32 randomindex;
    s32 i;

    while (groupnums[len] >= 0)
    {
        len++;
    }

    // Similar to the above function, return a random waygroup
    // which matches the dist criteria.
    randomindex = randomGetNext() % len;

    for (i = randomindex; i < len; i++)
    {
        waygroup *group = &g_CurrentSetup.waypointgroups[groupnums[i]];

        if (group->dist == dist)
        {
            return group;
        }
    }

    for (i = 0; i < randomindex; i++)
    {
        waygroup *group = &g_CurrentSetup.waypointgroups[groupnums[i]];

        if (group->dist == dist)
        {
            return group;
        }
    }

    return NULL;
}


waypoint *waypointFindNextStepToward(waypoint *pointa, waypoint *pointb)
{
    if (g_CurrentSetup.waypointgroups)
    {
        waygroup *groupa = &g_CurrentSetup.waypointgroups[pointa->groupNum];
        waygroup *groupb = &g_CurrentSetup.waypointgroups[pointb->groupNum];
        waypoint *result;

        if (groupa == groupb)
        {
            waypointsResetDist();
            waypointsFloodDist(pointb, pointa, 1);

            result = waypointFindRandomByDist(pointa->neighbours, -1);

            if (result)
            {
                return result;
            }

            result = waypointFindRandomByDist(pointa->neighbours, pointa->dist + 1);

            if (result)
            {
                return result;
            }
        }
        else
        {
            waygroupsFloodDist(groupb, groupa, g_CurrentSetup.waypointgroups, 0);

            if (groupa->dist >= 0)
            {
                waygroup *tmpgroup = waygroupFindRandomByDist(groupa->neighbours, -1);

                if (tmpgroup)
                {
                    waypoint *exitPoint;
                    waypoint *entryPoint;
                    waypoint *arr[3];

                    waygroupFindLinkPoints(groupa, tmpgroup, &exitPoint, &entryPoint);

                    if (exitPoint == pointa)
                    {
                        return entryPoint;
                    }

                    if (waypointFindRouteInGroup(pointa, exitPoint, arr, 3) >= 3)
                    {
                        return arr[1];
                    }
                }
                else
                {
                    waygroup *tmpgroup = waygroupFindByDist(groupa->neighbours, groupa->dist - 1);

                    if (tmpgroup)
                    {
                        waypoint *exitPoint;
                        waypoint *entryPoint;

                        waygroupFindLinkPoints(groupa, tmpgroup, &exitPoint, &entryPoint);
                        waypointsFloodDist(exitPoint, pointa, 1);
                        result = waypointFindByDist(pointa->neighbours, pointa->dist + 1, pointa->groupNum);

                        if (result)
                        {
                            return result;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}


/**
 * Checks the level's path graph at load and hangs if it is broken. Ryan spent a few minutes looking at this and left some notes about what this function does:
 *
 * It keeps an error flag. If set anywhere, the function goes into an infinite loop at the end.
 * The first outer loop appears to be verifying that the waypoints don't list themselves as their own neighbour.
 * The second outer loop appears to do the same but for waygroups. It also calls waygroupFindLinkPoints, which is a more simple version of PD's waypointFindSegmentIntoGroup, so it's likely verifying that there is a waypoint path between this group and each neighbour.
 * The third outer loop is iterating waygroups, then iterating each group's waypoints, and assigning each waypoint's groupNum if it's less than 0. If a value is stored and it's wrong, the error variable is set.
 * The fourth outer loop is checking that all waypoints have a groupNum assigned.
 * The fifth outer loop I'm not sure about without actually decompiling it and naming stuff. It appears to be using the waypoint->dist property temporary so it can validate something. If it doesn't like it then the error variable is set.
 * The final outer loop is the infinite loop which occurs if the error variable is true.
 */

void waypointsValidate(void)
{
    stagesetup *setup;
    waygroup *groups;
    s32 hasError;
    waypoint *waypoints;
    s32 waypointIndex;
    waygroup *currentWaypoint;

    hasError = 0;
    setup = &g_CurrentSetup;
    waypoints = setup->pathwaypoints;
    groups = setup->waypointgroups;


    if (waypoints != NULL)
    {
        s32 neighbourIndex;
        s32 neighbourNum;

        waypointIndex = 0;
        currentWaypoint = waypoints;

        while (((waypoint *) currentWaypoint)->padID >= 0)
        {
            neighbourIndex = 0;

            while ((neighbourNum = ((waypoint *) currentWaypoint)->neighbours[neighbourIndex]) >= 0)
            {
                if (neighbourNum == waypointIndex)
                {
                    if (g_CurrentSetup.padnames != NULL)
                    {
                    }
                    else if (1)
                    {
                    }
                    hasError = 1;
                }
                else
                {
                    waypoint *linkedWaypoint;
                    s32 reverseIndex;

                    reverseIndex = 0;
                    linkedWaypoint = &waypoints[neighbourNum];

                    while ((linkedWaypoint->neighbours[reverseIndex] >= 0) && (waypointIndex != linkedWaypoint->neighbours[reverseIndex]))
                    {
                        reverseIndex++;
                    }
 
                    if (waypointIndex != linkedWaypoint->neighbours[reverseIndex])
                    {
                        hasError = 1;
                    }
                }
                neighbourIndex++;
            }
 
            waypointIndex++;
            currentWaypoint = &waypoints[waypointIndex];
        }
 
    }
    {
        waygroup *assignmentGroupCursor;
        waypoint *validationWaypointCursor;
        waygroup *floodGroupCursor;
        s32 changed;
        s32 floodReverseIndex;
        waypoint *connectionWaypoint1;
        waypoint *connectionWaypoint2;
        s32 *member;
        waygroup *floodGroup;
        s32 disconnected;
        waypoint *floodWaypoint;
        s32 floodWaypointNum;
        s32 memberIndex;
        waypoint *assignmentWaypoint;
        s32 assignmentGroupIndex;
        s32 linkedWaypointNum;
        waypoint *validationWaypoint;

        if (groups != NULL)
        {
            waygroup *linkedGroup;
            waygroup *validationGroup;
            s32 validationGroupIndex;
            s32 groupNeighbourIndex;
            s32 reverseIndex;
            s32 waypointNum;
            /* was validationGroupCursors[-3] (an IDO stack-matching trick): out of
             * bounds, which GCC -Os turned into a trap instruction */
            waygroup *validationGroupCursor;

            validationGroupIndex = 0;
            validationGroup = groups;

            if (validationGroup->neighbours != NULL)
            {
                validationGroupCursor = groups;

                do
                {
                    groupNeighbourIndex = 0;
                    waypointNum = validationGroup->neighbours[groupNeighbourIndex];

                    while (waypointNum >= 0)
                    {
                        if (waypointNum == validationGroupIndex)
                        {
                            if (g_CurrentSetup.boundpadnames != NULL)
                            {
                            }
                            else
                            {
                            }
                            hasError = 1;
                        }
                        else
                        {
                            linkedGroup = &groups[waypointNum];
                            reverseIndex = 0;

                            while ((linkedGroup->neighbours[reverseIndex] >= 0) && (validationGroupIndex != linkedGroup->neighbours[reverseIndex]))
                            {
                                reverseIndex++;

                                if ((!(&g_CurrentSetup)) && (validationGroupIndex + (linkedGroup != 0)))
                                {
                                }
                                if ((!(&g_CurrentSetup)) && (validationGroupIndex + (groupNeighbourIndex != 0)))
                                {
                                }
                            }
 
                            if (validationGroupIndex != linkedGroup->neighbours[reverseIndex])
                            {
                                if (g_CurrentSetup.boundpadnames != NULL)
                                {
                                }
                                else
                                {
                                }
                                hasError = 1;
                            }
                            else
                                if (g_CurrentSetup.pathwaypoints != NULL)
                                {
                                    waygroupFindLinkPoints(validationGroup, linkedGroup, &connectionWaypoint1, &connectionWaypoint2);

                                    if ((connectionWaypoint1 == NULL) || (connectionWaypoint2 == NULL))
                                    {
                                        hasError = 1;
                                    }
                                }
                        }
                        groupNeighbourIndex++;
                        waypointNum = validationGroup->neighbours[groupNeighbourIndex];
                    }
 
                    validationGroupIndex++;
                    validationGroupCursor++;
                    validationGroup = validationGroupCursor;
                }
                while (validationGroupCursor->neighbours != NULL);
            }
        }
        if ((waypoints != NULL) && (groups != NULL))
        {
            {
                assignmentGroupIndex = 0;
                floodGroup = groups;
                validationWaypoint = waypoints + hasError * 0;

                if (floodGroup->neighbours != NULL)
                {
                    currentWaypoint = groups;
                    assignmentGroupCursor = floodGroup;

                    do
                    {
                        waypointIndex = 0;


                        member = floodGroup->waypoints;

                        while ((*member) >= 0)
                        {
                            assignmentWaypoint = &waypoints[*member];

                            if (assignmentWaypoint->groupNum < 0)
                            {
                                assignmentWaypoint->groupNum = assignmentGroupIndex;
                            }
                            else if (assignmentWaypoint->groupNum != assignmentGroupIndex)
                            {
                                    hasError = 1;
                            }

                            waypointIndex += 4;
                            member = (s32 *) (((char *) floodGroup->waypoints) + waypointIndex);
                        }
 
                        currentWaypoint++;
                        floodGroup = currentWaypoint;
                        assignmentGroupIndex++;
                    }
                    while (currentWaypoint->neighbours != NULL);
                }
 
            }

            floodGroup = groups;

            {
                assignmentGroupIndex = 0;
                validationWaypointCursor = validationWaypoint;
                floodGroupCursor = groups;
                currentWaypoint = floodGroupCursor;

                while (validationWaypointCursor->padID >= 0)
                {
                    if (validationWaypointCursor->groupNum < 0)
                    {
                        hasError = 1;
                    }

                    assignmentGroupIndex++;
                    validationWaypointCursor = &waypoints[assignmentGroupIndex];
                }
 
            }

            if (groups->neighbours != NULL)
            {
                do
                {
                    memberIndex = 0;
                    floodWaypointNum = floodGroup->waypoints[memberIndex];

                    while (floodWaypointNum >= 0)
                    {
                        floodWaypoint = &waypoints[floodWaypointNum];

                        if (memberIndex == 0)
                        {
                            floodWaypoint->dist = 1;
                        }
                        else
                        {
                            floodWaypoint->dist = 0;

                        }

                        memberIndex++;
                        floodWaypointNum = floodGroup->waypoints[memberIndex];
                    }
 
                    while (1)
                    {
                        memberIndex = 0;
                        floodWaypointNum = floodGroup->waypoints[memberIndex];
                        changed = 0;
                        disconnected = 0;

                        while (floodWaypointNum >= 0)
                        {
                            floodWaypoint = &waypoints[floodWaypointNum];

                            if (floodWaypoint->dist == 1)
                            {
                                floodReverseIndex = 0;
                                linkedWaypointNum = floodWaypoint->neighbours[floodReverseIndex];

                                if (linkedWaypointNum >= 0)
                                {
                                    do
                                    {
                                        assignmentWaypoint = &waypoints[linkedWaypointNum];

                                        if (assignmentWaypoint->dist != 1)
                                        {
                                            assignmentWaypoint->dist = 1;
                                            changed = 1;
                                        }

                                        floodReverseIndex++;
                                        linkedWaypointNum = floodWaypoint->neighbours[floodReverseIndex];
                                    }
                                    while (linkedWaypointNum >= 0);
                                }
                            }

                            memberIndex++;
                            floodWaypointNum = floodGroup->waypoints[memberIndex];
                        }
 
                        memberIndex = 0;
                        floodWaypointNum = floodGroup->waypoints[memberIndex];

                        while (floodWaypointNum >= 0)
                        {
                            memberIndex++;
                            floodWaypoint = &waypoints[floodWaypointNum];

                            if (floodWaypoint->dist != 1)
                            {
                                disconnected = 1;
                                break;
                            }

                            floodWaypointNum = floodGroup->waypoints[memberIndex];
                        }
 
                        if ((!changed) || (!disconnected))
                        {
                            if (((!validationWaypoint) && (!validationWaypoint)) && (!validationWaypoint))
                            {
                            }

                            if (disconnected)
                            {
                                if (g_CurrentSetup.boundpadnames != NULL)
                                {
                                }
                                else
                                {
                                }
                                hasError = 1;
                            }
                            break;
                        }
                    }
 
                    currentWaypoint = ((waygroup *) currentWaypoint) + 1;
                    floodGroup = (waygroup *) currentWaypoint;
                }
                while (((waygroup *) currentWaypoint)->neighbours != NULL);
            }
        }
    }

    if (hasError)
    {
        while (1)
        {
            // Intentional crash/freeze
        }
 
    }
}
