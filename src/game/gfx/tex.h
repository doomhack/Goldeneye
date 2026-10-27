#ifndef _UNK_0CC4C0_H_
#define _UNK_0CC4C0_H_
#include <ultra64.h>

#include "game/gfx/image.h"

struct image_sound
{
    /**
     * Array of sound effect ids.
    */
    u16 *sfx;

    u8 *thing2;

    /**
     * Number of items in sfx array.
    */
    s16 sfx_len;

    /**
     * Number of items in thing2 array.
    */
    s16 thing2_len;
};

typedef struct TexTileState
{
    s32 inuse;
    s32 format;
    s32 depth;
    s32 line;
    s32 tmem;
    s32 smode;
    s32 tmode;
    s32 masks;
    s32 maskt;
    s32 shifts;
    s32 shiftt;
} TexTileState;

typedef struct TexTileSize
{
    s32 inuse;
    s32 uls;
    s32 ult;
    s32 lrs;
    s32 lrt;
} TexTileSize;


extern struct image_sound *g_HitTypeSounds[];

void texCopyGdls(Gfx *src, Gfx *dst, s32 len);
void texGetDepthAndSize(struct tex *tex, s32 *deptharg, s32 *lenarg);
s32 texGetHeightAtLod(struct tex *tex, s32 lod);
s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, void *texpool);

#endif
