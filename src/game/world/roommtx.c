/**
 * The room matrix cache. Each room is drawn with its own matrix, which scales it from bg
 * units to world units and places it relative to the camera's origin room
 * (roomMtxSetOriginRoom). roomMtxLoad finds or builds the matrix for a room and the
 * current origin; matrices not used for two frames are freed by roomMtxTick.
 */

#include <ultra64.h>
#include "game/player/player.h"
#include "game/world/bg.h"
#include "game/world/roommtx.h"
#include "game/math/matrix.h"

#ifdef VERSION_EU
#define AMT300 100
#else
#define AMT300 300
#endif

// bss
u8 g_RoomMtxAge[AMT300];

s32 g_RoomMtxRoom[AMT300]; //mtxbufferroom
s32 g_RoomMtxOrigin[AMT300];
Mtx g_RoomMtxs[AMT300];


/**
 * Initialize room and player-related data structures.
 * Sets all rooms and players to an initial state.
 */
void roomMtxInit(void)
{
    int i;

    for (i=0; i<getPlayerCount(); i++)
    {
        g_playerPointers[i]->curRoomIndex = -1;
    }

    for (i=0; i<AMT300; i++)
    {
      g_RoomMtxRoom[i] = -1;
      g_RoomMtxAge[i] = 2;

      g_RoomMtxOrigin[i] = -1;


    }

    for (i=0; i<bgGetMaxNumRooms(); ++i)
    {
        g_BgRoomInfo[i].mtxIndex = -1;
    }
}


/**
 * Set the player's room field.
 */
void roomMtxSetOriginRoom(s32 roomIndex) {
  g_CurrentPlayer->curRoomIndex = roomIndex;
}


/**
 * Assigns a room index to a specific room ID.
 */
void roomMtxAssign(int mtx,int room)
{

    g_BgRoomInfo[room].mtxIndex = mtx;
    g_RoomMtxRoom[mtx] = room;
}


/**
 * Removes the room index assignment for a specific room ID.
 */
void roomMtxUnassign(int mtx,int room)
{

    g_BgRoomInfo[room].mtxIndex = -1;
    g_RoomMtxRoom[mtx] = -1;
}


/**
 * Resets a room's state to its initial condition.
 */
void roomMtxFree(int roomIndex)
{
    if (g_RoomMtxRoom[roomIndex] != -1) {
        roomMtxUnassign(roomIndex,g_RoomMtxRoom[roomIndex]);
    }
    g_RoomMtxAge[roomIndex] = 2;
    g_RoomMtxOrigin[roomIndex] = -1;
}


/**
 * Finds and returns the first available room index.
 * Returns 0 if no available room is found.
 */
s32 roomMtxFindFree(void)
{
    s32 i;

    for (i = 0; i<AMT300; i++)
    {
        if (((s32) g_RoomMtxAge[i] >= 2) && (g_RoomMtxOrigin[i] == -1))
        {
            return i;
        }
    }
    return 0;
}


/**
 * Updates the status flags for rooms, resetting those that are inactive.
 *
 */
void roomMtxTick(void)
{
    s32 i;

    for(i = 0; i<AMT300; ++i)
    {
        if (g_RoomMtxOrigin[i] > -1)
        {
            g_RoomMtxAge[i]++;

            if (g_RoomMtxAge[i] >= 2)
            {
                roomMtxFree(i);
            }
        }
    }
}




/**
 * Manages room index allocation and matrix setup for a given room.
 *
 */
s32 roomMtxGet(s32 room)
{
    s32 mtx;
    Mtxf roomTransformMatrix;

    mtx = g_BgRoomInfo[room].mtxIndex;

    if ((mtx == -1) || (g_CurrentPlayer->curRoomIndex != g_RoomMtxOrigin[mtx]))
    {
        if (mtx != -1)
        {
            roomMtxUnassign(mtx, room);
        }

        mtx = roomMtxFindFree();
        roomMtxAssign(mtx, room);

        g_RoomMtxAge[mtx] = 0;
    }
    else
    {
        g_RoomMtxAge[mtx] = 0;
        return mtx;
    }

    g_RoomMtxOrigin[mtx] = g_CurrentPlayer->curRoomIndex;

    mtx4LoadIdentity(&roomTransformMatrix);

    // set room size according to level scaling
    roomTransformMatrix.m[0][0] = g_BgLevelScaleInv;
    roomTransformMatrix.m[1][1] = g_BgLevelScaleInv;
    roomTransformMatrix.m[2][2] = g_BgLevelScaleInv;

    // room translation to position it relative to the player
    roomTransformMatrix.m[3][0] = (g_BgRoomFiles[room].pos.f[0] * g_BgLevelScaleInv) - g_CurrentPlayer->current_model_pos.f[0];
    roomTransformMatrix.m[3][1] = (g_BgRoomFiles[room].pos.f[1] * g_BgLevelScaleInv) - g_CurrentPlayer->current_model_pos.f[1];
    roomTransformMatrix.m[3][2] = (g_BgRoomFiles[room].pos.f[2] * g_BgLevelScaleInv) - g_CurrentPlayer->current_model_pos.f[2];

    mtxF2L(&roomTransformMatrix, (Mtxf *)&g_RoomMtxs[mtx]);

    return mtx;
}


/**
 * Updates the display list with the room matrix for a specific room roomID.
 */
Gfx * roomMtxLoad(Gfx *gdl,int roomID)
{
    s32 roomIndex;

    roomIndex = roomMtxGet(roomID);
    gSPMatrix(gdl++, &g_RoomMtxs[roomIndex], G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    return gdl;
}


/**
 * Returns the position of a room by its roomID.
 */
coord3d* roomGetPos(s32 roomID)
{
    return &g_BgRoomFiles[roomID].pos;
}


/**
 * Retrieves and scales the position of a room by its roomID.
 */
void roomGetWorldPos(s32 roomID, coord3d *scaledPos)
{
    scaledPos->x = g_BgRoomFiles[roomID].pos.x * g_BgLevelScaleInv;
    scaledPos->y = g_BgRoomFiles[roomID].pos.y * g_BgLevelScaleInv;
    scaledPos->z = g_BgRoomFiles[roomID].pos.z * g_BgLevelScaleInv;
}

