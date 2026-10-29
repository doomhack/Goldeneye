/**
 * Turning a model's display list into one the RDP can draw: texLoadFromGdl copies a display
 * list, loads every texture it names and replaces each texture command with the commands
 * that load it into TMEM and set up its tiles (the texHandleType* and texWrite* helpers).
 * The tile state is cached (g_TexTileStates, g_TexTileSizes) so repeated settings are
 * skipped.
 *
 * Also holds the surface sound and impact tables: which bullet hit sound and impact
 * texture each kind of surface uses.
 */

#include <ultra64.h>
#include "gbi_extension.h"
#include "game/world/lightfixture.h"
#include "game/level/lv.h"
#include "game/gfx/texload.h"
#include "game/gfx/tex.h"
#include "game/world/water.h"

s32 g_TexLutMode;
TexTileState g_TexTileStates[8]; /* last gDPSetTile per tile, to skip redundant commands */
TexTileSize g_TexTileSizes[8];   /* last gDPSetTileSize per tile */

u16 default_hit_sfx[] = {HIT_BULLET_STONE1_SFX, HIT_BULLET_STONE2_SFX};
u8 default_impact_types[] = {0x7, 0x0, 0x0, 0x0};
struct image_sound isnd_default = {default_hit_sfx, default_impact_types, 0x0, 0x1};

u16 stone_hit_sfx[] = {HIT_BULLET_STONE1_SFX, HIT_BULLET_STONE2_SFX};
u8 stone_impact_types[] = {0x1, 0x0, 0x0, 0x0};
struct image_sound isnd_stone = {stone_hit_sfx, stone_impact_types, 0x2, 0x1};

u16 wood_hit_sfx[] = {HIT_BULLET_WOOD_SFX, HIT_BULLET_WOOD2_SFX};
u8 wood_impact_types[] = {0x1, 0x0, 0x0, 0x0};
struct image_sound isnd_wood = {wood_hit_sfx, wood_impact_types, 0x2, 0x1};

u16 metal_hit_sfx[] = {HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_A4_SFX, HIT_BULLET_METAL_A3_SFX, 0x00};
u8 metal_impact_types[] = {0x7, 0x0, 0x0, 0x0};
struct image_sound isnd_metal = {metal_hit_sfx, metal_impact_types, 0x3, 0x1};

u16 glass_hit_sfx[] = {HIT_BULLET_GLASS_SFX, 0x00};
u8 glass_impact_types[] = {0x4, 0x5, 0x6, 0x0};
struct image_sound isnd_glass = {glass_hit_sfx, glass_impact_types, 0x1, 0x3};

u16 water_hit_sfx[] = {HIT_BULLET_WATER_SFX, 0x00};
u8 water_impact_types[] = {0, 0, 0, 0};
struct image_sound isnd_water = {water_hit_sfx, water_impact_types, 0x1, 0x0};

u16 snow_hit_sfx[] = {HIT_BULLET_SNOW_SFX, 0x00};
u8 snow_impact_types[] = {0x1, 0x0, 0x0, 0x0};
struct image_sound isnd_snow = {snow_hit_sfx, snow_impact_types, 0x1, 0x1};

u16 dirt_hit_sfx[] = {HIT_BULLET_DIRT1_SFX, HIT_BULLET_DIRT2_SFX};
u8 dirt_impact_types[] = {0x2, 0x0, 0x0, 0x0};
struct image_sound isnd_dirt = {dirt_hit_sfx, dirt_impact_types, 0x2, 0x1};

u16 mud_hit_sfx[] = {HIT_BULLET_MUD1_SFX, HIT_BULLET_MUD2_SFX, HIT_BULLET_MUD3_SFX, 0x00};
u8 mud_impact_types[] = {0x2, 0x0, 0x0, 0x0};
struct image_sound isnd_mud = {mud_hit_sfx, mud_impact_types, 0x3, 0x1};

u16 tile_hit_sfx[] = {HIT_BULLET_TILE_SFX, 0x00};
u8 tile_impact_types[] = {0x1, 0x0, 0x0, 0x0};
struct image_sound isnd_tile = {tile_hit_sfx, tile_impact_types, 0x1, 0x1};

u16 metal_object_hit_sfx[] = {HIT_METAL_OBJECT1_SFX, HIT_METAL_OBJECT2_SFX};
u8 metal_object_impact_types[] = {0x1, 0x7, 0x0, 0x0};
struct image_sound isnd_metalobj = {metal_object_hit_sfx, metal_object_impact_types, 0x2, 0x2};

u16 character_hit_sfx[] = {HIT_BULLET_FLESH_SFX, 0x00};
u8 character_impact_types[] = {0x2, 0x0, 0x0, 0x0};
struct image_sound isnd_chr = {character_hit_sfx, character_impact_types, 0x1, 0x1};

u16 translucent_glass_hit_sfx[] = {HIT_BULLET_GLASS_SFX, 0x00};
u8 translucent_glass_impact_types[] = {0x11, 0x12, 0x13, 0x0};
struct image_sound isnd_glass_xlu = {translucent_glass_hit_sfx, translucent_glass_impact_types, 0x1, 0x3};

struct image_sound *g_HitTypeSounds[] = { 
    &isnd_default,
    &isnd_stone,
    &isnd_wood,
    &isnd_metal,
    &isnd_glass,
    &isnd_water,
    &isnd_snow,
    &isnd_dirt,
    &isnd_mud,
    &isnd_tile,
    &isnd_metalobj,
    &isnd_chr,
    &isnd_glass_xlu
};

char * image_sound_names[] = {
    "HIT_DEFAULT",
    "HIT_STONE",
    "HIT_WOOD",
    "HIT_METAL",
    "HIT_GLASS",
    "HIT_WATER",
    "HIT_SNOW",
    "HIT_DIRT",
    "HIT_MUD",
    "HIT_TILE",
    "HIT_METALOBJ",
    "HIT_CHR",
    "HIT_GLASS_XLU"
};


void texResetTileState(void)
{
    int i;

    for (i=0; i<8; i++)
    {
        g_TexTileStates[i].inuse = 0;
        g_TexTileSizes[i].inuse = 0;
    }
    g_TexLutMode = -1;
}


u32 texSetLutMode(s32 mode) {
    if (mode == g_TexLutMode) {
        return 0;
    }
    g_TexLutMode = mode;
    return 1;
}


s32 texTrySetTileState(s32 tile, s32 format, s32 depth, s32 line, s32 tmem, s32 smode, s32 tmode, s32 masks, s32 maskt, s32 shifts, s32 shiftt)
{
    s32 retval;

    retval = 0;
    if ((g_TexTileStates[tile].inuse == 0) || (format != g_TexTileStates[tile].format) || 
     (depth != g_TexTileStates[tile].depth) || (line != g_TexTileStates[tile].line) || 
     (tmem != g_TexTileStates[tile].tmem) || (smode != g_TexTileStates[tile].smode) || 
     (tmode != g_TexTileStates[tile].tmode) || (masks != g_TexTileStates[tile].masks) || 
     (maskt != g_TexTileStates[tile].maskt) || (shifts != g_TexTileStates[tile].shifts) || 
     (shiftt != g_TexTileStates[tile].shiftt))
    {
        retval = 1;
        g_TexTileStates[tile].inuse = 1;
        g_TexTileStates[tile].format = format;
        g_TexTileStates[tile].depth = depth;
        g_TexTileStates[tile].line = line;
        g_TexTileStates[tile].tmem = tmem;
        g_TexTileStates[tile].smode = smode;
        g_TexTileStates[tile].tmode = tmode;
        g_TexTileStates[tile].masks = masks;
        g_TexTileStates[tile].maskt = maskt;
        g_TexTileStates[tile].shifts = shifts;
        g_TexTileStates[tile].shiftt = shiftt;

    }
    return retval;
}


s32 texTrySetTileSize(s32 tile, s32 uls, s32 ult, s32 lrs, s32 lrt)
{
    s32 retval;

    retval = 0;
    if ((((g_TexTileSizes[tile].inuse == 0) || (uls != g_TexTileSizes[tile].uls)) || (ult != g_TexTileSizes[tile].ult)) ||
       ((lrs != g_TexTileSizes[tile].lrs || (lrt != g_TexTileSizes[tile].lrt))))
    {
        retval = 1;
        g_TexTileSizes[tile].inuse = 1;
        g_TexTileSizes[tile].uls = uls;
        g_TexTileSizes[tile].ult = ult;
        g_TexTileSizes[tile].lrs = lrs;
        g_TexTileSizes[tile].lrt = lrt;
    }
    return retval;
}


s32 texGetLineSizeInBytes(struct tex *tex, s32 lod)
{
    s32 depth = tex->depth;
    s32 width = texGetWidthAtLod(tex, lod);

    if (depth == G_IM_SIZ_32b)
    {
        return (width + 3) / 4;
    }

    if (depth == G_IM_SIZ_16b)
    {
        return (width + 3) / 4;
    }

    if (depth == G_IM_SIZ_8b)
    {
        return (width + 7) / 8;
    }

    return (width + 15) / 16;
}


s32 texGetSizeInBytes(struct tex *tex, s32 lod)
{
    return texGetHeightAtLod(tex, lod) * texGetLineSizeInBytes(tex, lod);
}


void texGetDepthAndSize(struct tex *tex, s32 *deptharg, s32 *lenarg)
{
    s32 depth = tex->depth;
    s32 maxlod = tex->maxlod ? tex->maxlod : 1;
    s32 lod;

    *lenarg = 0;

    if (depth == G_IM_SIZ_32b)
    {
        *deptharg = G_IM_SIZ_32b;
    }
    else if (depth == G_IM_SIZ_16b)
    {
        *deptharg = G_IM_SIZ_16b;
    }
    else if (depth == G_IM_SIZ_8b)
    {
        *deptharg = G_IM_SIZ_16b;
    }
    else
    {
        *deptharg = G_IM_SIZ_16b;
    }

    for (lod = 0; lod < maxlod; lod++) {
        *lenarg += texGetSizeInBytes(tex, lod) * 4;
    }
}


s32 texDimensionToMask(s32 dim) {
    s32 i = 0;

    dim--;

    while (dim > 0 && i < 8) {
        dim >>= 1;
        i++;
    }

    return i;
}





u32 texModeToGbiMode(s32 mode) {
    if (mode == 1) {
        return G_TX_CLAMP;
    }
    if (mode == 2) {
        return G_TX_MIRROR;
    }
    return G_TX_WRAP;
}


s32 texCeil(f32 value)
{
    return (s32)value < (f32)value ? (s32)value+1 : (s32)value;
}


s32 texGetLoadBlockDxt(struct tex *tex)
{
    s32 depth;
    s32 line;

    line = texGetLineSizeInBytes(tex, 0U);
    depth = tex->depth;
    if (depth == 3)
    {
        return texCeil((2.0f / (f32) (line * 4)) * 2048.0f);
    }
    if (depth == 2)
    {
        return texCeil((4.0f / (f32) (line * 4)) * 2048.0f);
    }
    if (depth == 1)
    {
        return texCeil((8.0f / (f32) (line * 8)) * 2048.0f);
    }
    return texCeil((16.0f / (f32) (line * 0x10)) * 2048.0f);
}


Gfx *texWriteTileFromDefinition(Gfx *gdl, struct tex *tex, s32 offset, s32 shifts, s32 shiftt, s32 minlevel)
{
    struct image_entry *s0 = &g_Textures[tex->texturenum];
    s32 masks;
    s32 maskt;
    s32 line;
    s32 uls;
    s32 ult;
    s32 lrs;
    s32 lrt;

    masks = texDimensionToMask(tex->width);
    maskt = texDimensionToMask(tex->height);

    line = texGetLineSizeInBytes(tex, 0);

    gDPSetPrimColorViaWord(gdl++, minlevel, 0, 0xffffffff);

    if (texSetLutMode(tex->lutmodeindex << G_MDSFT_TEXTLUT))
    {
        gDPSetTextureLUT(gdl++, tex->lutmodeindex << G_MDSFT_TEXTLUT);
    }

    if (texTrySetTileState(0, tex->gbiformat, tex->depth, line, s0->flag3 + line * s0->flag4, 0, 0, masks - s0->flag5, maskt - s0->flag6, shifts, shiftt))
    {
        gDPSetTile(gdl++, tex->gbiformat, tex->depth, line, s0->flag3 + line * s0->flag4, 0, 0,
                texModeToGbiMode(0), maskt - s0->flag6, shiftt,
                texModeToGbiMode(0), masks - s0->flag5, shifts);
    }

    uls = (offset == 2 && !tex->hasExplicitLods ? 2 : 0) + 0;
    ult = (offset == 2 && !tex->hasExplicitLods ? 2 : 0) + 0;
    lrs = (offset == 2 && !tex->hasExplicitLods ? 2 : 0) + ((tex->width - 1) << 2);
    lrt = (offset == 2 && !tex->hasExplicitLods ? 2 : 0) + ((tex->height - 1) << 2);

    if (texTrySetTileSize(0, uls, ult, lrs, lrt))
    {
        gDPSetTileSize(gdl++, 0, uls, ult, lrs, lrt);
    }

    return gdl;
}

Gfx* texWriteTextureCmd(Gfx* gdl, Gfx* prev, struct tex* tex, s32 write)
{
    s32 lod = tex->maxlod ? tex->maxlod - 1 : 0;

    if (write)
    {
        if (prev != NULL)
        {
            u32 w0 = (prev->words.w0 & ~0x3800) | (lod << 11);

            if (w0 != prev->words.w0)
            {
                gdl->words.w0 = w0;
                gdl->words.w1 = prev->words.w1;
                gdl++;
            }
        }
        else
        {
            gSPTexture(gdl++, 0xffff, 0xffff, lod, G_TX_RENDERTILE, G_ON);
        }
    }
    else
    {
        prev->words.w0 &= ~0x3800;
        prev->words.w0 |= lod << 11;
    }

    return gdl;
}


Gfx *texWriteLoadToTmemAddr(Gfx *gdl, struct tex *tex, s32 tmemoffset)
{
	s32 depth;
	s32 len;

	texGetDepthAndSize(tex, &depth, &len);

	if (tex->lutmodeindex == 0)
	{
		gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);

		if (texTrySetTileState(7, tex->gbiformat, depth, 0, tmemoffset, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, tex->gbiformat, depth, 0, tmemoffset, 7, 0, 0, 0, 0, 0, 0, 0);
		}

		gDPLoadSync(gdl++);
		gDPLoadBlock(gdl++, 7, 0, 0, len - 1, 0);
		gDPPipeSync(gdl++);
	}
	else
	{
		u32 uls;
		u32 ult;

		gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);

		if (texTrySetTileState(7, tex->gbiformat, depth, 0, tmemoffset, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, tex->gbiformat, depth, 0, tmemoffset, 7, 0, 0, 0, 0, 0, 0, 0);
		}

		gDPLoadSync(gdl++);
		gDPLoadBlock(gdl++, 7, 0, 0, len - 1, 0);
		gDPPipeSync(gdl++);

		uls = len;

		if ((u32)(0x3ff - tex->lastColour) < uls)
		{
			ult = 0x3ff - tex->lastColour;
		}
		else
		{
			ult = 0;
		}

		uls -= ult;

		if (texTrySetTileState(7, 0, 0, 1, 0x100, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, 0, 0, 1, 0x100, 7, 0, 0, 0, 0, 0, 0, 0);
		}
        
		gDPLoadSync(gdl++);
		gDPLoadTLUT07(gdl++, uls, ult, tex->lastColour + uls, ult);
		gDPPipeSync(gdl++);
	}

	return gdl;
}


Gfx *texWriteTileLods(Gfx *gdl, struct tex *tex, s32 smode, s32 tmode, s32 offset, s32 basetile, s32 tmemaddr)
{
    s32 tmem;
    s32 masks;
    s32 tile;
    s32 maskt;
    s32 line;
    s32 ult;
    s32 lrs;
    s32 lrt;
    s32 lod;
    s32 uls;
    bool hasExplicitLods;
    s32 end;
    s32 size;
    s32 pad2;

    tmem = tmemaddr;
    end = tex->maxlod;
    end = end + basetile;

    for (tile = basetile; tile < (end ^ 0); tile++)
    {
        pad2 = basetile;
        lod = tile - pad2;
        masks = texDimensionToMask(texGetWidthAtLod(tex, lod));
        maskt = texDimensionToMask(texGetHeightAtLod(tex, lod));
        line = texGetLineSizeInBytes(tex, lod);
        size = texGetSizeInBytes(tex, lod);
        hasExplicitLods = tex->hasExplicitLods;

        if (texSetLutMode(tex->lutmodeindex << 14))
        {
            gDPSetTextureLUT(gdl++, tex->lutmodeindex << G_MDSFT_TEXTLUT);
        }

        if (texTrySetTileState(tile, tex->gbiformat, tex->depth, line, tmem, smode, tmode, masks, maskt, tile - pad2, tile - pad2))
        {
            gDPSetTile(gdl++, tex->gbiformat, tex->depth, line, tmem, tile, 0, texModeToGbiMode(tmode), maskt, lod, texModeToGbiMode(smode), masks, tile - basetile);
        }

        uls = (((offset == 2) && (hasExplicitLods == 0)) ? (2) : (0)) + 0;
        ult = (((offset == 2) && (hasExplicitLods == 0)) ? (2) : (0)) + 0;
        lrs = ((texGetWidthAtLod(tex, lod) - 1) << 2) + (((offset == 2) && (hasExplicitLods == 0)) ? (2) : (0));
        lrt = ((texGetHeightAtLod(tex, lod) - 1) << 2) + (((offset == 2) && (hasExplicitLods == 0)) ? (2) : (0));

        if (texTrySetTileSize(tile, uls, ult, lrs, lrt))
        {
            gDPSetTileSize(gdl++, tile, uls, ult, lrs, lrt);
        }

        tmem += size;
    }

    lod = tile - pad2;
    
    return gdl;
}


Gfx *texWriteLoadToTmemZero(Gfx *gdl, struct tex *tex)
{
	s32 depth;
	s32 len;
	s32 dxt;

	dxt = texGetLoadBlockDxt(tex);
	texGetDepthAndSize(tex, &depth, &len);

	if (tex->lutmodeindex == 0)
	{
		gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);
		if (texTrySetTileState(7, tex->gbiformat, depth, 0, 0, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, tex->gbiformat, depth, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0);
		}
		gDPLoadSync(gdl++);
		gDPLoadBlock(gdl++, 7, 0, 0, len - 1, dxt);
		gDPPipeSync(gdl++);
	}
	else
	{
		u32 uls;
		u32 ult;

		gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);
		if (texTrySetTileState(7, tex->gbiformat, depth, 0, 0, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, tex->gbiformat, depth, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0);
		}
		gDPLoadSync(gdl++);
		gDPLoadBlock(gdl++, 7, 0, 0, len - 1, dxt);
		gDPPipeSync(gdl++);

		uls = len;
		if ((u32)(0x3ff - tex->lastColour) < uls)
		{
			ult = 0x3ff - tex->lastColour;
		}
		else
		{
			ult = 0;
		}
		uls -= ult;
		if (texTrySetTileState(7, 0, 0, 1, 0x100, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, 0, 0, 1, 0x100, 7, 0, 0, 0, 0, 0, 0, 0);
		}
		gDPLoadSync(gdl++);
		gDPLoadTLUT07(gdl++, uls, ult, tex->lastColour + uls, ult);
		gDPPipeSync(gdl++);
	}

	return gdl;
}


Gfx *texWriteTile(Gfx *gdl, struct tex *tex, s32 smode, s32 tmode, s32 offset, s32 tile)
{
    s32 masks;
    s32 maskt;
    s32 line;
    s32 uls;
    s32 ult;
    s32 lrs;
    s32 lrt;
    s32 explicitLods;

    masks = texDimensionToMask(tex->width);
    maskt = texDimensionToMask(tex->height);
    line = texGetLineSizeInBytes(tex, 0);
    explicitLods = tex->hasExplicitLods;

    if (texSetLutMode(tex->lutmodeindex << G_MDSFT_TEXTLUT))
    {
        gDPSetTextureLUT(gdl++, tex->lutmodeindex << G_MDSFT_TEXTLUT);
    }

    if (texTrySetTileState(tile, tex->gbiformat, tex->depth, line, 0, smode, tmode, masks, maskt, 0, 0))
    {
        gDPSetTile(gdl++, tex->gbiformat, tex->depth, line, 0x0000, tile, 0,
                texModeToGbiMode(tmode), maskt, G_TX_NOLOD,
                texModeToGbiMode(smode), masks, G_TX_NOLOD);
    }

    uls = (offset == 2 && explicitLods == 0 ? 2 : 0) + 0;
    ult = (offset == 2 && explicitLods == 0 ? 2 : 0) + 0;
    lrs = (offset == 2 && explicitLods == 0 ? 2 : 0) + ((tex->width - 1) << 2);
    lrt = (offset == 2 && explicitLods == 0 ? 2 : 0) + ((tex->height - 1) << 2);

    if (texTrySetTileSize(tile, uls, ult, lrs, lrt))
    {
        gDPSetTileSize(gdl++, tile, uls, ult, lrs, lrt);
    }

    return gdl;
}


Gfx *texHandleType2(Gfx *gdl, struct tex *tex, s32 smode, s32 tmode, s32 offset)
{
    gdl = texWriteLoadToTmemAddr(gdl,tex,0);
    gdl = texWriteTileLods(gdl,tex,smode,tmode,offset,0,0);
    if (tex->maxlod == 1) {
        gdl = texWriteTileLods(gdl,tex,smode,tmode,offset,1,0);
    }
    return gdl;
}


Gfx * texHandleType1(Gfx *gdl,struct tex *tex1,s32 smode,s32 tmode,s32 offset,struct tex *tex2,u32 shifts,u32 shiftt,u32 min)
{
    u32 size;

    size = texGetSizeInBytes(tex2,0);
    gdl = texWriteLoadToTmemZero(gdl,tex2);
    gDPTileSync(gdl++);
    gdl = texWriteLoadToTmemAddr(gdl,tex1,size);
    gdl = texWriteTileFromDefinition(gdl,tex2,offset,shifts,shiftt,min);
    gdl = texWriteTileLods(gdl,tex1,smode,tmode,offset,1,size);
    if (tex1->maxlod == 1) {
        gdl = texWriteTileLods(gdl,tex1,smode,tmode,offset,2,size);
    }
    return gdl;
}


Gfx * texHandleType0(Gfx *gdl,struct tex *tex,s32 smode,s32 tmode,s32 offset,u32 shifts,u32 shiftt,u32 min)
{
    gdl = texWriteLoadToTmemAddr(gdl,tex,0);
    gdl = texWriteTileFromDefinition(gdl,tex,offset,shifts,shiftt,min);
    gdl = texWriteTileLods(gdl,tex,smode,tmode,offset,1,0);
    if (tex->maxlod == 1) {
        gdl = texWriteTileLods(gdl,tex,smode,tmode,offset,2,0);
    }
    return gdl;
}


Gfx *texHandleType4(Gfx *DL, struct tex *tex,s32 smode,s32 tmode,s32 offset)
{
    DL = texWriteLoadToTmemZero(DL,tex);
    DL = texWriteTile(DL,tex,smode,tmode,offset,0);
    return DL;
}


Gfx *texHandleType3(Gfx *DL, struct tex *tex,s32 smode,s32 tmode,s32 offset)
{
    DL = texWriteLoadToTmemZero(DL,tex);
    DL = texWriteTile(DL,tex,smode,tmode,offset,0);
    DL = texWriteTile(DL,tex,smode,tmode,offset,1);
    return DL;
}





const char PADDINGHACKING[4] = "";

s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, void *texpool)
{
    s32         texnum;
    struct tex *tex;
    Gfx        *saved = NULL;
    s32         count;
    s32         texnum2;
    s32         syncEmitted = FALSE;
    s32         smode;
    s32         tmode;
    s32         offset;
    s32         shifts;
    s32         shiftt;
    s32         min;
    s32         writeTexFlag = TRUE;
    s32         lightPending = FALSE;
    Gfx        *in           = src;
    Gfx        *out          = dst;

    count = srcsize >> 3;

    texResetTileState();

    if (texpool == NULL)
    {
        texpool = &g_TexSharedPool;
    }

    while (count > 0)
    {
        switch (*(u8 *)in)
        {
            case G_NOOP:
                if (!syncEmitted)
                {
                    gDPPipeSync(out++);
                    syncEmitted = TRUE;
                }

                if (lightPending)
                {
                    lightFixtureEntryEnd(out);
                    lightPending = FALSE;
                }

                texnum = in->words.w1 & 0xfff;

                texLoadFromTextureNum(texnum, texpool);
                tex = texFindInPool(texnum, texpool);

                if (tex != NULL)
                {
                    out          = texWriteTextureCmd(out, saved, tex, writeTexFlag);
                    writeTexFlag = FALSE;

                    switch (in->words.w0 & 7)
                    {
                        case TEXTURETYPE_LOD:
                            min    = (in->words.w1 >> 24) & 0xff;
                            smode  = (in->words.w0 >> 22) & 3;
                            tmode  = (in->words.w0 >> 20) & 3;
                            offset = (in->words.w0 >> 18) & 3;
                            shifts = (in->words.w0 >> 14) & 0xf;
                            shiftt = (in->words.w0 >> 10) & 0xf;

                            out = texHandleType0(out, tex, smode, tmode, offset, shifts, shiftt, min);
                            break;

                        case TEXTURETYPE_DETAIL:
                        {
                            struct tex *tex2;

                            texnum2 = (in->words.w1 >> 12) & 0xfff;

                            texLoadFromTextureNum(texnum2, texpool);
                            tex2 = texFindInPool(texnum2, texpool);

                            if (tex2 != NULL)
                            {
                                min    = ((*in).words.w1 >> 24) & 0xff;
                                smode  = (in->words.w0 >> 22) & 3;
                                tmode  = (in->words.w0 >> 20) & 3;
                                offset = (in->words.w0 >> 18) & 3;
                                shifts = (in->words.w0 >> 14) & 0xf;
                                shiftt = (in->words.w0 >> 10) & 0xf;

                                out = texHandleType1(out, tex, smode, tmode, offset, tex2, shifts, shiftt, min);
                            }
                            break;
                        }

                        case TEXTURETYPE_MIPMAP:
                            smode  = (in->words.w0 >> 22) & 3;
                            tmode  = (in->words.w0 >> 20) & 3;
                            offset = (in->words.w0 >> 18) & 3;

                            out = texHandleType2(out, tex, smode, tmode, offset);
                            break;

                        case TEXTURETYPE_TILE:
                            smode  = (in->words.w0 >> 22) & 3;
                            tmode  = (in->words.w0 >> 20) & 3;
                            offset = (in->words.w0 >> 18) & 3;
                            out    = texHandleType3(out, tex, smode, tmode, offset);
                            break;

                        case TEXTURETYPE_TILE_PRESWAPPED:
                            smode  = (in->words.w0 >> 22) & 3;
                            tmode  = (in->words.w0 >> 20) & 3;
                            offset = (in->words.w0 >> 18) & 3;
                            out    = texHandleType4(out, tex, smode, tmode, offset);
                            break;
                    }

                    // Caverns water
                    if (texnum == 1508)
                    {
                        out = waterSetupGdl(out, TRUE);
                        texResetTileState();
                    }

                    /**
                     * This texture is used on the water in Dam and Complex. You can't actually
                     * see the texture in Complex because it's at the bottom of the pits and the
                     * vertex coloring is pitch black. You can still tell it's water by
                     * the splash sound it makes when shot.
                     */
                    if (texnum == 1511)
                    {
                        out = waterSetupGdlCi(out, TRUE);
                        texResetTileState();
                    }

                    if (lightIsLightTexture(texnum))
                    {
                        lightFixtureEntryBegin(out);
                        lightPending = TRUE;
                    }
                }

                in++;
                break;

            case G_RDPPIPESYNC:
                syncEmitted = TRUE;
                *(out++)    = *(in++);
                break;

            case 0xb1:
            case 0xbf:
                writeTexFlag = TRUE;
                syncEmitted  = FALSE;
                *(out++)     = *(in++);
                break;

            case 0xbb:
                saved        = out;
                writeTexFlag = FALSE;
                *(out++)     = *(in++);
                break;

            case 0xba:
                *(out++) = *(in++);
                break;

            default:
                *(out++) = *(in++);
                break;
        }

        count--;
    }

    if (lightPending)
    {
        lightFixtureEntryEnd(out);
    }

    return ((s32)out) - ((s32)dst);
}


/***
*/
void texCopyGdls(Gfx *src, Gfx *dst, s32 len)
{
    len = (len >> 3);
    src = src + (len - 1);
    dst = dst + (len - 1);

    while (len--)
    {
        dst->force_structure_alignment = src->force_structure_alignment;
        dst--;
        src--;
    }
}
