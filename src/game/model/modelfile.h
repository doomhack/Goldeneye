#ifndef _MODELFILE_H_
#define _MODELFILE_H_
#include <ultra64.h>
#include <bondtypes.h>
#include "game/gfx/texload.h"

void modelFileLoadTextures(const ModelFileHeader *header, const char *name, const u8 *dst, struct texpool *texpool);
void modelFileLoad(struct ModelFileHeader *header, const char *name, u8 *dst, s32 size, struct texpool *texpool);
void modelFileLoadToPool(ModelFileHeader *header, const char *name);

#endif
