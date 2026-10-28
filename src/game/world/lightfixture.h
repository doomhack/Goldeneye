#ifndef _LIGHTFIXTURE_H_
#define _LIGHTFIXTURE_H_

#include <ultra64.h>
#include <bondtypes.h>

typedef struct s_lightfixture {
    s16 room_index;
    s16 RESERVED;
    Gfx *ptr_start_pertinent_DL;
    Gfx *ptr_end_pertinent_DL;
} s_lightfixture;

struct s_darkened_light {
    u16 room_index;
    u16 vtx_index;
};

void lightFixtureEntryEnd(Gfx *DL);
bool lightIsLightTexture(s32 imageID);
void lightFixtureEntryBegin(Gfx *DL);

void lightFixtureBreak(Gfx * hit_gfx, u32 tri_type, s32 room_index);
void lightFixtureClearRoom(s32 room_index);
void lightFixtureInitTables(void);
void lightRedarkenRoom(s32 room_index);

#endif
