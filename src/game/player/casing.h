#ifndef _CASING_H_
#define _CASING_H_
#include <ultra64.h>
#include <bondtypes.h>

// Ejected cartridge casings: creating them as a gun fires, moving and drawing them

CasingRecord *casingCreate(ModelFileHeader *header, Mtxf *mtx);
void gunEjectCasings(GUNHAND handnum, f32 floor_y_pos);
void casingTick(CasingRecord *casing);
void casingsTick(void);
void casingRender(CasingRecord *casing, Gfx * *gdl);
void casingsRender(Gfx * *gdl);
void casingsLoadModels(void);

#endif
