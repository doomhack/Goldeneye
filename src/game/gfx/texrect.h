#ifndef _TEXRECT_H_
#define _TEXRECT_H_
#include <ultra64.h>
#include "game/player/player.h"

void texRectDrawImage(Gfx **gdlptr, f32 *position, f32 *size, s32 twidth, s32 theight, s32 flipXY, s32 flipX, s32 flipY, s32 r, s32 g, s32 b, s32 alpha, s32 mipmapped_texture, s32 highlight_mode);

#endif
