/**
 * Which props are in which rooms.
 *
 * Each prop keeps the rooms it overlaps in prop->rooms (up to 7, ended by 0xff). Each
 * room keeps a list of the props in it, so that collision, line-of-sight and shot tests
 * only look at props in nearby rooms (roomGetProps) instead of all 600.
 *
 * The room lists are made of blocks of 15 prop numbers (g_RoomPropBlocks, 256 blocks
 * shared by all rooms). Entry 15 of a block is the number of the room's next block, or
 * -1. An empty entry is -1, and a block not in use has -2 as its first entry.
 * g_RoomFirstPropBlock is each room's first block. Removing props leaves holes, which
 * propsDefragRoomProps fills once a frame.
 */

#include <ultra64.h>
#include <memp.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/proprooms.h"
#include "game/prop/prop.h"
#include "game/prop/setup.h"
#include "game/chr/chr.h"
#include "game/nav/stan.h"
#include "game/world/bg.h"

/* Entries in a block that hold props; the last entry links to the next block */
#define BLOCK_NUM_PROPS 15
#define BLOCK_NEXT      15

#define ENTRY_EMPTY      -1
#define BLOCK_FREE       -2

/* A prop is in up to this many rooms */
#define PROP_MAX_ROOMS 7

/* Props found by roomGetProps, ended by -1 */
s16 *g_RoomPropsFound;
u32 g_NumRoomPropsFound;

s16 *g_RoomFirstPropBlock;
struct roomproplistblock *g_RoomPropBlocks;


/**
 * Allocate the room lists for the level, all empty.
 */
void roomPropListsInit(void)
{
    g_RoomPropsFound = mempAlloc(ROOMPROPSFOUND_LEN * sizeof(s16), MEMPOOL_STAGE);
    g_RoomFirstPropBlock = mempAlloc(((g_MaxNumRooms * 4) + 0xf) & ~0xf, MEMPOOL_STAGE);
    g_RoomPropBlocks = mempAlloc(ROOMPROPBLOCKS_LEN * sizeof(struct roomproplistblock), MEMPOOL_STAGE);

    g_RoomPropsFound[0] = -1;

    for (s32 i = 0; i < g_MaxNumRooms; i++)
    {
        g_RoomFirstPropBlock[i] = -1;
    }

    for (s32 i = 0; i < ROOMPROPBLOCKS_LEN; i++)
    {
        g_RoomPropBlocks[i].propnums[0] = BLOCK_FREE;

        for (s32 j = 1; j < ROOMPROPBLOCK_LEN; j++)
        {
            g_RoomPropBlocks[i].propnums[j] = ENTRY_EMPTY;
        }
    }
}


/**
 * The rooms a prop is in, ended by -1. A player's prop without a body is only in the
 * room of the tile it stands on.
 */
void propGetRoomIds(PropRecord *prop, s32 *roomids)
{
    StandTile *stan = prop->stan;

    if (stan == NULL)
    {
        roomids[0] = -1;
    }
    else if (prop->type == PROP_TYPE_VIEWER && prop->obj == NULL)
    {
        roomids[0] = stan->room;
        roomids[1] = -1;
    }
    else
    {
        s32 i;

        for (i = 0; prop->rooms[i] != 0xff; i++)
        {
            roomids[i] = prop->rooms[i];
        }

        roomids[i] = -1;
    }
}


/* Whether two room lists (ended by -1) share a room */
bool roomListsOverlap(s32 *rooms1, s32 *rooms2)
{
    for (s32 *a = rooms1; *a >= 0; a++)
    {
        for (s32 *b = rooms2; *b >= 0; b++)
        {
            if (*a == *b)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}


/* Put a prop number in the first empty entry of a block. FALSE when it is full. */
static bool roomBlockAddProp(s16 propnum, s32 block)
{
    for (s32 i = 0; i < BLOCK_NUM_PROPS; i++)
    {
        if (g_RoomPropBlocks[block].propnums[i] < 0)
        {
            g_RoomPropBlocks[block].propnums[i] = propnum;
            return TRUE;
        }
    }

    return FALSE;
}


/* Take a free block, empty it and link it after prevblock (or make it the room's first
   when prevblock is -1). -1 when there are no free blocks. */
static s32 roomBlockAllocate(s32 room, s32 prevblock)
{
    for (s32 i = 0; i < ROOMPROPBLOCKS_LEN; i++)
    {
        if (g_RoomPropBlocks[i].propnums[0] != BLOCK_FREE)
        {
            continue;
        }

        for (s32 j = 0; j < ROOMPROPBLOCK_LEN; j++)
        {
            g_RoomPropBlocks[i].propnums[j] = ENTRY_EMPTY;
        }

        if (prevblock >= 0)
        {
            g_RoomPropBlocks[prevblock].propnums[BLOCK_NEXT] = i;
        }
        else
        {
            g_RoomFirstPropBlock[room] = i;
        }

        return i;
    }

    return -1;
}


/* Add a prop to a room's list, taking a new block when its blocks are full. If no block
   is free the prop is left out. */
void propAddToRoom(PropRecord *prop, s16 room)
{
    if (room < 0)
    {
        return;
    }

    s16 propnum = prop - g_Props;
    s32 prevblock = -1;
    s32 block;

    for (block = g_RoomFirstPropBlock[room]; block >= 0; block = g_RoomPropBlocks[block].propnums[BLOCK_NEXT])
    {
        if (roomBlockAddProp(propnum, block))
        {
            return;
        }

        prevblock = block;
    }

    block = roomBlockAllocate(room, prevblock);

    if (block >= 0)
    {
        roomBlockAddProp(propnum, block);
    }
}


/* Remove a prop from a room's list. Blocks found empty on the way are freed. */
void propRemoveFromRoom(PropRecord *prop, s16 room)
{
    if (room < 0)
    {
        return;
    }

    s16 propnum = prop - g_Props;
    bool removed = FALSE;
    s32 prevblock = -1;
    s16 block = g_RoomFirstPropBlock[room];

    while (block >= 0)
    {
        s16 *propnums = g_RoomPropBlocks[block].propnums;
        bool populated = FALSE;

        for (s32 i = 0; i < BLOCK_NUM_PROPS; i++)
        {
            if (propnum == propnums[i])
            {
                propnums[i] = ENTRY_EMPTY;
                removed = TRUE;
            }
            else if (!populated && propnums[i] >= 0)
            {
                populated = TRUE;
            }
        }

        if (!populated)
        {
            // Free the block and unlink it
            propnums[0] = BLOCK_FREE;

            if (prevblock >= 0)
            {
                g_RoomPropBlocks[prevblock].propnums[BLOCK_NEXT] = propnums[BLOCK_NEXT];
            }
            else
            {
                g_RoomFirstPropBlock[room] = propnums[BLOCK_NEXT];
            }
        }
        else
        {
            prevblock = block;
        }

        if (removed)
        {
            return;
        }

        block = propnums[BLOCK_NEXT];
    }
}


/* Add a prop to the lists of all the rooms it is in */
void propAddToRooms(PropRecord *prop)
{
    for (u8 *room = prop->rooms; *room != 0xff; room++)
    {
        propAddToRoom(prop, *room);
    }
}


/* Remove a prop from the lists of all the rooms it is in. Unless the prop has
   PROPFLAG_00000010, it then counts as in no room. */
void propRemoveFromRooms(PropRecord *prop)
{
    for (u8 *room = prop->rooms; *room != 0xff; room++)
    {
        propRemoveFromRoom(prop, *room);
    }

    if (!(prop->flags & PROPFLAG_00000010))
    {
        prop->rooms[0] = 0xff;
    }
}


/* Work out again which rooms a guard or object is in */
void propUpdateRooms(PropRecord *prop)
{
    if (prop->type == PROP_TYPE_CHR)
    {
        chrDetectRooms(prop->chr);
    }
    else if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON)
    {
        setupUpdateObjectRoomPosition(prop->obj);
    }
}


/**
 * Work out which rooms a prop overlaps, into prop->rooms. It starts from the rooms the
 * prop is in already (with PROPFLAG_00000008; a projectile off the tile grid uses its
 * own list), or else the rooms of the floor tiles within radius of it. Then it adds the
 * rooms its bounding box reaches through portals.
 */
void propCalcRooms(PropRecord *prop, coord3d *bbmin, coord3d *bbmax, f32 radius)
{
    s32 rooms[PROP_MAX_ROOMS];
    s32 count = 0;

    if (prop->flags & PROPFLAG_00000008)
    {
        ObjectRecord *obj = NULL;
        u8 *src;
        s32 i;

        if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_DOOR)
        {
            obj = prop->obj;
        }

        if (obj != NULL && (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) && (obj->projectile->flags & PROJECTILEFLAG_OFFGRID))
        {
            src = obj->projectile->rooms;
        }
        else
        {
            src = prop->rooms;
        }

        for (i = 0; src[i] != 0xff; i++)
        {
            rooms[i] = src[i];
        }

        count = i;
    }
    else
    {
        StandTile *tile = prop->stan;

        stanCircleRooms(&tile, prop->pos.x, prop->pos.z, radius, rooms, &count, PROP_MAX_ROOMS);
    }

    bgFindRoomsInBbox(bbmin, bbmax, rooms, &count, PROP_MAX_ROOMS);

    s32 i;

    for (i = 0; i < count; i++)
    {
        prop->rooms[i] = rooms[i];
    }

    prop->rooms[i] = 0xff;
}


/**
 * Collect the props in any of a list of rooms (ended by -1) into g_RoomPropsFound, each
 * once, ended by -1.
 */
void roomGetProps(s32 *rooms)
{
    s16 *end = g_RoomPropsFound;

    for (; *rooms >= 0; rooms++)
    {
        for (s32 block = g_RoomFirstPropBlock[*rooms]; block >= 0; block = g_RoomPropBlocks[block].propnums[BLOCK_NEXT])
        {
            for (s32 i = 0; i < BLOCK_NUM_PROPS; i++)
            {
                s32 propnum = g_RoomPropBlocks[block].propnums[i];

                if (propnum < 0)
                {
                    continue;
                }

                s16 *found = g_RoomPropsFound;

                while (found < end && *found != propnum)
                {
                    found++;
                }

                if (found == end)
                {
                    *end++ = propnum;
                }
            }
        }
    }

    *end++ = -1;
    g_NumRoomPropsFound = end - g_RoomPropsFound;
}


/**
 * Fill the holes in each room's list by moving props back from later blocks, freeing
 * the blocks that empty. Run once a frame.
 */
void propsDefragRoomProps(void)
{
    for (s32 room = 0; room < g_MaxNumRooms; room++)
    {
        s32 prevblock = g_RoomFirstPropBlock[room];

        if (prevblock < 0)
        {
            continue;
        }

        s32 nextblock = g_RoomPropBlocks[prevblock].propnums[BLOCK_NEXT];

        while (nextblock >= 0)
        {
            s16 *prev = g_RoomPropBlocks[prevblock].propnums;

            for (s32 j = 0; j < BLOCK_NUM_PROPS; j++)
            {
                if (prev[j] >= 0)
                {
                    continue;
                }

                s16 *next = g_RoomPropBlocks[nextblock].propnums;
                s32 k;

                // Move the next block's first prop into the hole
                for (k = 0; k < BLOCK_NUM_PROPS; k++)
                {
                    if (next[k] >= 0)
                    {
                        prev[j] = next[k];
                        next[k] = ENTRY_EMPTY;
                        break;
                    }
                }

                // Free the next block if that emptied it
                for (; k < BLOCK_NUM_PROPS; k++)
                {
                    if (next[k] >= 0)
                    {
                        break;
                    }
                }

                if (k == BLOCK_NUM_PROPS)
                {
                    next[0] = BLOCK_FREE;
                    prev[BLOCK_NEXT] = next[BLOCK_NEXT];
                    nextblock = prev[BLOCK_NEXT];

                    if (nextblock < 0)
                    {
                        break;
                    }
                }
            }

            if (nextblock >= 0)
            {
                prevblock = nextblock;
                nextblock = g_RoomPropBlocks[nextblock].propnums[BLOCK_NEXT];
            }
        }
    }
}
