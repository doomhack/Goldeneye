#ifndef _PROPROOMS_H_
#define _PROPROOMS_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

#define ROOMPROPSFOUND_LEN 512
#define ROOMPROPBLOCKS_LEN 256
#define ROOMPROPBLOCK_LEN 16

/* 15 prop numbers, then the number of the room's next block (see proprooms.c) */
struct roomproplistblock
{
    s16 propnums[ROOMPROPBLOCK_LEN];
};

extern s16 *g_RoomPropsFound;
extern u32 g_NumRoomPropsFound;
extern s16 *g_RoomFirstPropBlock;
extern struct roomproplistblock *g_RoomPropBlocks;

void roomPropListsInit(void);
void propGetRoomIds(PropRecord *prop, s32 *roomids);
bool roomListsOverlap(s32 *rooms1, s32 *rooms2);
void propAddToRoom(PropRecord *prop, s16 room);
void propRemoveFromRoom(PropRecord *prop, s16 room);
void propAddToRooms(PropRecord *prop);
void propRemoveFromRooms(PropRecord *prop);
void propUpdateRooms(PropRecord *prop);
void propCalcRooms(PropRecord *prop, coord3d *bbmin, coord3d *bbmax, f32 radius);
void roomGetProps(s32 *rooms);
void propsDefragRoomProps(void);

#endif
