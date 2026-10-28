#ifndef _ZBUF_H_
#define _ZBUF_H_

#include <ultra64.h>

#include <PR/gbi.h>

Gfx *zbufClearCurrentPlayer(Gfx *gdl);
Gfx *zbufInit(Gfx *gdl);

void zbufDeallocate(void);

void zbufSetBuffer(void *buffer, s32 width, s32 height);

#endif
