/**
 * The path graph guards follow. Waypoints sit on pads and link to their neighbours;
 * waypoints are also grouped, and the groups link to each other. Every list in the graph
 * is of indexes, ended by -1.
 *
 * A route is found in two steps: a breadth-first search over the groups, then within each
 * group over its waypoints. A search numbers the nodes by their distance from where it
 * starts, in their dist fields, then marks the route by adding ROUTE_MARK to the dist of
 * each node on it, working back from the end. Following the marked numbers up from the
 * start gives the route. Only one search can use the graph at a time.
 */

#include <ultra64.h>
#include "game/nav/waypoint.h"
#include "bondtypes.h"
#include "dbgscreen.h"
#include "game/chr/chrai.h"
#include "random.h"

#define ROUTE_MARK 10000

static waypoint *getPoint(s32 index)
{
    return &g_CurrentSetup.pathwaypoints[index];
}

static waygroup *getGroup(s32 index)
{
    return &g_CurrentSetup.waypointgroups[index];
}

static s32 listLength(const s32 *list)
{
    s32 len = 0;

    while (list[len] >= 0)
    {
        len++;
    }

    return len;
}

static bool listContains(const s32 *list, s32 value)
{
    for (; *list >= 0; list++)
    {
        if (*list == value)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/* Groups. The group table ends at a group with no neighbour list. */

static waygroup *findGroupByDist(const s32 *groupnums, s32 dist)
{
    for (; *groupnums >= 0; groupnums++)
    {
        waygroup *group = getGroup(*groupnums);

        if (group->dist == dist)
        {
            return group;
        }
    }

    return NULL;
}

/** Numbers the groups by their distance from from: up to to, or with all, every group it reaches */
static bool floodGroups(waygroup *from, waygroup *to, bool all)
{
    waygroup *groups = g_CurrentSetup.waypointgroups;
    waygroup *group;
    bool more = TRUE;
    s32 dist;

    for (group = groups; group->neighbours != NULL; group++)
    {
        group->dist = -1;
    }

    from->dist = 0;

    for (dist = 0; (all || to->dist < 0) && more; dist++)
    {
        more = FALSE;

        for (group = groups; group->neighbours != NULL; group++)
        {
            if (group->dist == dist)
            {
                const s32 *n;

                more = TRUE;

                for (n = group->neighbours; *n >= 0; n++)
                {
                    if (getGroup(*n)->dist < 0)
                    {
                        getGroup(*n)->dist = dist + 1;
                    }
                }
            }
        }
    }

    return more;
}

/** Marks the shortest route between two groups. Returns FALSE if there isn't one. */
static bool markGroupRoute(waygroup *from, waygroup *to)
{
    waygroup *group = to;
    s32 dist;

    if (!floodGroups(from, to, FALSE))
    {
        return FALSE;
    }

    for (dist = to->dist - 1; dist >= 0; dist--)
    {
        group->dist += ROUTE_MARK;
        group = findGroupByDist(group->neighbours, dist);
    }

    group->dist += ROUTE_MARK;

    return TRUE;
}

/** The first link from a waypoint of groupa to one of groupb, or two NULLs */
static void findLinkPoints(waygroup *groupa, waygroup *groupb, waypoint **pointa, waypoint **pointb)
{
    const s32 *pointnums;

    for (pointnums = groupa->waypoints; *pointnums >= 0; pointnums++)
    {
        waypoint *point = getPoint(*pointnums);
        const s32 *n;

        for (n = point->neighbours; *n >= 0; n++)
        {
            if (getGroup(getPoint(*n)->groupNum) == groupb)
            {
                *pointa = point;
                *pointb = getPoint(*n);

                return;
            }
        }
    }

    *pointb = NULL;
    *pointa = NULL;
}

/* Waypoints within a group */

static waypoint *findPointByDist(const s32 *pointnums, s32 dist, s32 groupnum)
{
    for (; *pointnums >= 0; pointnums++)
    {
        waypoint *point = getPoint(*pointnums);

        if (point->groupNum == groupnum && point->dist == dist)
        {
            return point;
        }
    }

    return NULL;
}

/** Numbers from's group's waypoints by their distance from it: up to to, or with all, all of them */
static void floodPoints(waypoint *from, waypoint *to, bool all)
{
    s32 groupnum = from->groupNum;
    const s32 *members = getGroup(groupnum)->waypoints;
    const s32 *m;
    bool more = TRUE;
    s32 dist;

    for (m = members; *m >= 0; m++)
    {
        getPoint(*m)->dist = -1;
    }

    from->dist = 0;

    for (dist = 0; (all || to->dist < 0) && more; dist++)
    {
        more = FALSE;

        for (m = members; *m >= 0; m++)
        {
            waypoint *point = getPoint(*m);

            if (point->dist == dist && point->neighbours != NULL)
            {
                const s32 *n;

                more = TRUE;

                for (n = point->neighbours; *n >= 0; n++)
                {
                    waypoint *neighbour = getPoint(*n);

                    if (neighbour->groupNum == groupnum && neighbour->dist < 0)
                    {
                        neighbour->dist = dist + 1;
                    }
                }
            }
        }
    }
}

/**
 * Writes the route from one waypoint to another in the same group into route, ended by
 * NULL: as much of it as fits in maxLen entries. Returns the entries written, NULL too.
 */
static s32 findRouteInGroup(waypoint *from, waypoint *to, waypoint **route, s32 maxLen)
{
    waypoint **out = route;

    if (maxLen >= 2)
    {
        waypoint *point = to;
        s32 dist;
        s32 step;

        floodPoints(from, to, FALSE);

        for (dist = to->dist - 1; dist >= 0; dist--)
        {
            point->dist += ROUTE_MARK;
            point = findPointByDist(point->neighbours, dist, from->groupNum);
        }

        point->dist += ROUTE_MARK;

        *out++ = from;
        point = from;

        for (step = 1; ROUTE_MARK + step <= to->dist && step < maxLen - 1; step++)
        {
            point = findPointByDist(point->neighbours, ROUTE_MARK + step, from->groupNum);
            *out++ = point;
        }
    }

    *out++ = NULL;

    return out - route;
}

/**
 * Writes the route from frompoint to topoint into route, ended by NULL: as much of it as
 * fits in maxLen entries. Returns the entries written, NULL too.
 */
s32 waypointFindRoute(waypoint *frompoint, waypoint *topoint, waypoint **route, s32 maxLen)
{
    waypoint **out = route;

    if (g_CurrentSetup.waypointgroups != NULL)
    {
        waygroup *fromgroup = getGroup(frompoint->groupNum);
        waygroup *togroup = getGroup(topoint->groupNum);

        if (markGroupRoute(fromgroup, togroup))
        {
            waypoint *point = frompoint;
            waygroup *group = fromgroup;
            s32 dist;

            /* To the waypoint linking each group on the route to the next, and over */
            for (dist = fromgroup->dist + 1; dist <= togroup->dist && maxLen >= 2; dist++)
            {
                waygroup *nextgroup = findGroupByDist(group->neighbours, dist);
                waypoint *exitpoint;
                waypoint *entrypoint;
                s32 written;

                findLinkPoints(group, nextgroup, &exitpoint, &entrypoint);
                written = findRouteInGroup(point, exitpoint, out, maxLen) - 1;

                maxLen -= written;
                out += written;

                point = entrypoint;
                group = nextgroup;
            }

            out += findRouteInGroup(point, topoint, out, maxLen) - 1;
        }
    }

    *out++ = NULL;

    return out - route;
}

static waypoint *findRandomPointByDist(const s32 *pointnums, s32 dist)
{
    s32 len = listLength(pointnums);
    s32 start = randomGetNext() % len;
    s32 i;

    /* From a random place in the list, so any of the matches is as likely */
    for (i = 0; i < len; i++)
    {
        waypoint *point = getPoint(pointnums[(start + i) % len]);

        if (point->dist == dist)
        {
            return point;
        }
    }

    return NULL;
}

static waygroup *findRandomGroupByDist(const s32 *groupnums, s32 dist)
{
    s32 len = listLength(groupnums);
    s32 start = randomGetNext() % len;
    s32 i;

    for (i = 0; i < len; i++)
    {
        waygroup *group = getGroup(groupnums[(start + i) % len]);

        if (group->dist == dist)
        {
            return group;
        }
    }

    return NULL;
}

/**
 * The next waypoint from pointa for a guard heading for pointb, or NULL: a step that
 * doesn't take it closer, chosen at random, or failing that one that does. (It is for
 * guards who should look like they are searching, not walking straight there.)
 */
waypoint *waypointFindNextStepToward(waypoint *pointa, waypoint *pointb)
{
    waygroup *groupa;
    waygroup *groupb;

    if (g_CurrentSetup.waypointgroups == NULL)
    {
        return NULL;
    }

    groupa = getGroup(pointa->groupNum);
    groupb = getGroup(pointb->groupNum);

    if (groupa == groupb)
    {
        waypoint *point;
        waypoint *wp;

        /* Clear every dist, so a neighbour outside the group reads as unreached */
        for (wp = g_CurrentSetup.pathwaypoints; wp->padID >= 0; wp++)
        {
            wp->dist = -1;
        }

        floodPoints(pointb, pointa, TRUE);

        point = findRandomPointByDist(pointa->neighbours, -1);

        if (point == NULL)
        {
            point = findRandomPointByDist(pointa->neighbours, pointa->dist + 1);
        }

        return point;
    }

    floodGroups(groupb, groupa, FALSE);

    if (groupa->dist >= 0)
    {
        waygroup *group = findRandomGroupByDist(groupa->neighbours, -1);
        waypoint *exitpoint;
        waypoint *entrypoint;

        if (group != NULL)
        {
            waypoint *route[3];

            findLinkPoints(groupa, group, &exitpoint, &entrypoint);

            if (exitpoint == pointa)
            {
                return entrypoint;
            }

            if (findRouteInGroup(pointa, exitpoint, route, 3) >= 3)
            {
                return route[1];
            }
        }
        else
        {
            group = findGroupByDist(groupa->neighbours, groupa->dist - 1);

            if (group != NULL)
            {
                findLinkPoints(groupa, group, &exitpoint, &entrypoint);
                floodPoints(exitpoint, pointa, TRUE);

                return findPointByDist(pointa->neighbours, pointa->dist + 1, pointa->groupNum);
            }
        }
    }

    return NULL;
}

/**
 * Checks the level's path graph at load, and stops the game if it is broken: every link
 * must go both ways and not to itself, linked groups must have a waypoint link between
 * them, every waypoint must be in exactly one group, and each group's waypoints must link
 * up. Also sets each waypoint's group number.
 */
void waypointsValidate(void)
{
    waypoint *points = g_CurrentSetup.pathwaypoints;
    waygroup *groups = g_CurrentSetup.waypointgroups;
    bool broken = FALSE;
    s32 i;

    if (points != NULL)
    {
        for (i = 0; points[i].padID >= 0; i++)
        {
            const s32 *n;

            for (n = points[i].neighbours; *n >= 0; n++)
            {
                if (*n == i || !listContains(points[*n].neighbours, i))
                {
                    broken = TRUE;
                }
            }
        }
    }

    if (groups != NULL)
    {
        for (i = 0; groups[i].neighbours != NULL; i++)
        {
            const s32 *n;

            for (n = groups[i].neighbours; *n >= 0; n++)
            {
                if (*n == i || !listContains(groups[*n].neighbours, i))
                {
                    broken = TRUE;
                }
                else if (points != NULL)
                {
                    waypoint *pointa;
                    waypoint *pointb;

                    findLinkPoints(&groups[i], &groups[*n], &pointa, &pointb);

                    if (pointa == NULL || pointb == NULL)
                    {
                        broken = TRUE;
                    }
                }
            }
        }
    }

    if (points != NULL && groups != NULL)
    {
        waygroup *group;

        for (i = 0; groups[i].neighbours != NULL; i++)
        {
            const s32 *m;

            for (m = groups[i].waypoints; *m >= 0; m++)
            {
                if (points[*m].groupNum < 0)
                {
                    points[*m].groupNum = i;
                }
                else if (points[*m].groupNum != i)
                {
                    broken = TRUE;
                }
            }
        }

        for (i = 0; points[i].padID >= 0; i++)
        {
            if (points[i].groupNum < 0)
            {
                broken = TRUE;
            }
        }

        /* Spread a mark (dist 1) from each group's first waypoint until every waypoint has it */
        for (group = groups; group->neighbours != NULL; group++)
        {
            const s32 *m;
            bool changed;
            bool unreached;

            for (m = group->waypoints; *m >= 0; m++)
            {
                points[*m].dist = m == group->waypoints ? 1 : 0;
            }

            do
            {
                changed = FALSE;
                unreached = FALSE;

                for (m = group->waypoints; *m >= 0; m++)
                {
                    if (points[*m].dist == 1)
                    {
                        const s32 *n;

                        for (n = points[*m].neighbours; *n >= 0; n++)
                        {
                            if (points[*n].dist != 1)
                            {
                                points[*n].dist = 1;
                                changed = TRUE;
                            }
                        }
                    }
                }

                for (m = group->waypoints; *m >= 0; m++)
                {
                    if (points[*m].dist != 1)
                    {
                        unreached = TRUE;
                        break;
                    }
                }
            } while (changed && unreached);

            if (unreached)
            {
                broken = TRUE;
            }
        }
    }

    if (broken)
    {
        dbgFatal("waypoints: the level's path graph is broken", 0);
    }
}
