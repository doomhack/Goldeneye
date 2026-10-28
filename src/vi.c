#include <ultra64.h>
#include <PR/gbi.h>
/**
 * EU .data, offset from start of data_seg : 0x3740
*/

OSMesg vi_c_debug_MSG[0x8];
OSMesgQueue vi_c_debug_MQ;


#ifndef VERSION_EU
Gfx viDLEnd[] = {
	gsSPEndDisplayList()
};
#endif


void viDebugRemoved(void) {
    return;
}

void viInit(void)
{
    osCreateMesgQueue(&vi_c_debug_MQ, vi_c_debug_MSG, 8);
}




