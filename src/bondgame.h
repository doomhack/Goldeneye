#ifndef _BONDGAME_H_
#define _BONDGAME_H_
#include <ultra64.h>
//#include <bondaicommands.h> // game ai commands actually... this shouldnt be here as they are only needed by setups
#include <bondconstants.h>  // game constants
#include <bondtypes.h>      // game structs and types

extern u32 *_bssSegmentEnd NO_SDATA;

extern u32 *_codeSegmentStart NO_SDATA;
extern u32 *_codeSegmentEnd NO_SDATA;
extern u32 *_codeSegmentRomStart NO_SDATA;
extern u32 *_codeSegmentRomEnd NO_SDATA;

extern u32 *_csegmentSegmentStart NO_SDATA;
extern u32 *_csegmentSegmentEnd NO_SDATA;
extern u32 *_cdataSegmentRomStart NO_SDATA;
extern u32 *_cdataSegmentRomEnd NO_SDATA;

extern u32 *_inflateSegmentVaddrStart NO_SDATA;
extern u32 *_inflateSegmentVaddrEnd NO_SDATA;
extern u32 *_inflateSegmentRomStart NO_SDATA;
extern u32 *_inflateSegmentRomEnd NO_SDATA;

extern u32 *_gameSegmentStart NO_SDATA;
extern u32 *_gameSegmentEnd NO_SDATA;
extern u32 *_heapSegmentStart NO_SDATA;
extern u32 *_gameSegmentRomStart NO_SDATA;
extern u32 *_gameSegmentRomEnd NO_SDATA;

extern u32* _animation_dataSegmentRomStart NO_SDATA;
extern u32* _animation_dataSegmentStart NO_SDATA;
extern u32* _animation_dataSegmentEnd NO_SDATA;

extern u8 sp_boot[SP_BOOT_SZ];
extern u8 sp_rmon[SP_RMON_SZ];
extern u8 sp_idle[SP_IDLE_SZ];
extern u8 sp_shed[SP_SHED_SZ];
extern u8 sp_main[SP_MAIN_SZ];
extern u8 sp_audi[SP_AUDI_SZ];



#endif
