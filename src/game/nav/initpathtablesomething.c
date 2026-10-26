#include <ultra64.h>
#include <bondtypes.h>
#include "game/nav/stan.h"


/**
 * NTSC address 0x7F0067C0.
*/
s32 stanResolvePadTile(struct PadRecord *pad, char *tilename, struct StandTile **tile_stack)
{
    struct coord3d coord;
   
    *tile_stack = stanMatchTileName(tilename);
    
    if ((*tile_stack == NULL) || (stanIsInsideTriWorld(*tile_stack, pad->pos.f[0], pad->pos.f[2]) == 0))
    {
        coord.f[0] = pad->pos.f[0];
        coord.f[1] = pad->pos.f[1];
        coord.f[2] = pad->pos.f[2];
        
        *tile_stack = stanFindNearestLegalPos(&coord.f[0], &coord.f[1], &coord.f[2], 0);
        
        if ((*tile_stack != NULL) && (stanWalkLineNoCallback(tile_stack, coord.f[0], coord.f[2], pad->pos.f[0], pad->pos.f[2]) != 0))
        {
            return 2;
        }
        
        *tile_stack = NULL;
        
        return 0;
    }
    
    return 1;
}
 
