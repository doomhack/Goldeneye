#ifndef _TEXCMDS_H_
#define _TEXCMDS_H_
#include <ultra64.h>
#include "bondtypes.h"
#include "game/gfx/texload.h"

/** How texSelect blends what it draws */
enum TexBlendMode
{
    TEXBLEND_OPAQUE = 1,
    TEXBLEND_TRANSLUCENT,
    TEXBLEND_CUTOUT, /* G_RM_*_TEX_EDGE: texels are drawn or not, by their alpha */
    TEXBLEND_CLOUD,  /* G_RM_*_CLD_SURF: translucent, without antialiasing */
};

/**
 * How texSelect uses the z-buffer. Any value from TEXZBUF_DECAL up is decal, and is also
 * passed on in the gSPTexture's reserved byte (the bullet impacts use 8).
 */
enum TexZbufferMode
{
    TEXZBUF_OFF,
    TEXZBUF_ON,
    TEXZBUF_DECAL, /* on, for things drawn onto a surface */
};

s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, struct texpool *pool);
void texCopyGdls(Gfx *src, Gfx *dst, s32 len);
void texSelect(Gfx **gdlptr, sImageTableEntry *image, u32 blendMode, s32 zbufferMode, u32 uvOffset);

#endif
