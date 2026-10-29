#ifndef _TEXLOAD_H_
#define _TEXLOAD_H_
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/gfx/texdecode.h"

/**
 * Memory that decoded textures are kept in. It fills from both ends: the pixels from
 * `start` up, and a struct tex describing each texture from `end` down.
 */
struct texpool
{
    u8 *start;
    struct tex *end;
    u8 *leftpos;
    struct tex *rightpos;
};

/** A decoded texture in a pool */
struct tex
{
    u16 texturenum : 12;
    u16 unused : 4;
    u8 *data;
    u8 width;
    u8 height;
    u8 lastColour;           // palette size - 1
    u8 maxlod : 3;           // levels of detail
    u8 gbiformat : 3;        // G_IM_FMT_*
    u8 depth : 2;            // G_IM_SIZ_*
    u32 lutmodeindex : 2;    // G_TT_* >> G_MDSFT_TEXTLUT
    u32 hasExplicitLods : 1; // the smaller levels' sizes are in g_TexCacheItems
    u32 unused2 : 29;
};

/** A texture's entry in the table of all textures (g_Textures) */
struct image_entry
{
    u8 hitSound         : 4;  // HitType-Sound (should be enum HIT_TYPE, but it needs to be unsigned)
    u32 hitTexture : 4;  // HitType-Texture
    u32 dataoffset : 24; // where it is in the textures segment of the ROM (texResolveTableOffsets)
    u32 flag3 : 4; // Detailflag1 used once with value 0x38D2 (S/T offset of detail)
    u32 flag4 : 4;
    u32 flag5 : 4; // DetailFlag2  difference from detail image, to large image, in terms of 2 to the power of value. It is subtracted from the small one, and absolute value or something. (Zoinkity)
    u32 flag6 : 4;
};

/** The sizes of a texture's stored (explicit) levels of detail, after the first */
struct texcacheitem
{
    s16 texturenum;
    u8 widths[7];
    u8 heights[7];
};

extern struct texcacheitem g_TexCacheItems[];
extern struct texpool g_TexSharedPool;
extern struct image_entry g_Textures[];
extern s32 g_TexCacheCount;

void texResolveTableOffsets(void);
void texInitSharedPool(void);
void texInitPool(struct texpool *pool, u8 *start, s32 len);
struct tex *texFindInPool(s32 texturenum, struct texpool *pool);
void texLoad(u32 *updateword, struct texpool *pool);
void texLoadFromTextureNum(s32 texturenum, struct texpool *pool);
void texLoadFromModelFileHeader(ModelFileHeader *header, struct texpool *pool);
void texLoadFromDisplayList(Gfx *gdl, struct texpool *pool);

#endif
