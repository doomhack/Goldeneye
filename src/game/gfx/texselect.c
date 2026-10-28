/**
 * texSelect sets up the RDP to draw with one image from an image table (the HUD, sky,
 * explosions, menus): it loads the texture, sets the render mode (texSetRenderMode) and
 * writes the tile and LoadBlock commands. The texLoadBlock* helpers work out a LoadBlock's
 * texel count and dxt for each pixel size.
 */

#include <ultra64.h>
#include <R4300.h>
#include "game/gfx/tex.h"
#include "game/gfx/image.h"
#include "bondtypes.h"
#include "game/gfx/texselect.h"
#include "gbi_extension.h"

s32 texMaskBits(int number)
{
    if (number < 2) {
        return 0;
    }

    if (number < 3) {
        return 1;
    }

    if (number < 5) {
        return 2;
    }

    if (number < 9) {
        return 3;
    }

    if (number < 0x11) {
        return 4;
    }

    if (number < 0x21) {
        return 5;
    }

    if (number < 0x41) {
        return 6;
    }

    if (number < 0x81) {
        return 7;
    }

    return 8;
}


s32 texLoadBlockDxt4b(s32 width)
{
    f32 dxt;
    s32 whole;

    dxt = 32768.0f / ((((s32) (width + 15)) / 16) * 16);
    whole = dxt;

    return (whole & 0xFFFFFFFF)
         + ((s32) ((dxt - whole) + 0.999999f));
}


s32 texLoadBlockDxt8b(s32 width)
{
    f32 dxt;
    s32 whole;

    dxt = 16384.0f / ((((s32) (width + 7)) / 8) * 8);
    whole = dxt;

    return (whole & 0xFFFFFFFF)
         + ((s32) ((dxt - whole) + 0.999999f));
}


s32 texLoadBlockDxt16b(s32 width)
{
    f32 dxt;
    s32 whole;

    dxt = 8192.0f / ((((s32) (width + 3)) / 4) * 4);
    whole = dxt;

    return (whole & 0xFFFFFFFF)
         + ((s32) ((dxt - whole) + 0.999999f));
}


s32 texLoadBlockDxt32b(s32 width)
{
    f32 dxt;
    s32 whole;

    dxt = 4096.0f / ((((s32) (width + 3)) / 4) * 4);
    whole = dxt;

    return (whole & 0xFFFFFFFF)
         + ((s32) ((dxt - whole) + 0.999999f));
}


s32 texLoadBlockTexels4b(s32 width, s32 height, s32 numlods)
{
    s32 ret;
    ret = 0;

    if (numlods <= 0) { numlods = 1; }

    while (numlods > 0)
    {
        ret += ((width + 15) / 16) * 4 * height;
        numlods--;
        if (width >= 2) { width >>= 1; }
        if (height >= 2) { height >>= 1; }
    }
    return ret;
}


s32 texLoadBlockTexels8b(s32 width, s32 height, s32 numlods)
{
    s32 ret;
    ret = 0;

    if (numlods <= 0) { numlods = 1; }

    while (numlods > 0)
    {
        ret += ((width + 7) / 8) * 4 * height;
        numlods--;
        if (width >= 2) { width >>= 1; }
        if (height >= 2) { height >>= 1; }
    }
    return ret;
}


s32 texLoadBlockTexels16b(s32 width, s32 height, s32 numlods)
{
    s32 ret;
    ret = 0;

    if (numlods <= 0) { numlods = 1; }

    while (numlods > 0)
    {
        ret += ((width + 3) / 4) * 4 * height;
        numlods--;
        if (width >= 2) { width >>= 1; }
        if (height >= 2) { height >>= 1; }
    }
    return ret;
}


s32 texLoadBlockTexels32b(s32 width, s32 height, s32 numlods)
{
    s32 ret;
    ret = 0;

    if (numlods <= 0) { numlods = 1; }

    while (numlods > 0)
    {
        ret += ((width + 3) / 4) * 4 * height;
        numlods--;
        if (width >= 2) { width >>= 1; }
        if (height >= 2) { height >>= 1; }
    }
    return ret;
}


void texSetRenderMode(Gfx **gdlptr, s32 blendMode, s32 numcycles, s32 zbufferMode)
{
    Gfx *gdl = *gdlptr;

    if (numcycles == 1)
    {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);

        switch (blendMode)
        {
            default:
            case 1:
                if (zbufferMode)
                {
                    if (zbufferMode >= 2)
                    {
                        gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_DECAL, G_RM_AA_ZB_OPA_DECAL2);
                    }
                    else
                    {
                        gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
                    }
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
                }
                break;

            case 2:
                if (zbufferMode)
                {
                    if (zbufferMode >= 2)
                    {
                        gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_DECAL, G_RM_AA_ZB_XLU_DECAL2);
                    }
                    else
                    {
                        gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
                    }
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
                }
                break;

            case 3:
                if (zbufferMode)
                {
                    gDPSetRenderMode(gdl++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
                }
                break;

            case 4:
                if (zbufferMode)
                {
                    gDPSetRenderMode(gdl++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                }
                break;
        }
    }
    else
    {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_2CYCLE);

        switch (blendMode)
        {
            default:
            case 1:
                if (zbufferMode)
                {
                    if (zbufferMode >= 2)
                    {
                        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_DECAL2);
                    }
                    else
                    {
                        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
                    }
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_OPA_SURF2);
                }
                break;

            case 2:
                if (zbufferMode)
                {
                    if (zbufferMode >= 2)
                    {
                        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_XLU_DECAL2);
                    }
                    else
                    {
                        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
                    }
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_XLU_SURF2);
                }
                break;

            case 3:
                if (zbufferMode)
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_TEX_EDGE2);
                }
                break;

            case 4:
                if (zbufferMode)
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_ZB_CLD_SURF2);
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_CLD_SURF2);
                }
                break;
        }
    }

    *gdlptr = gdl;
}


/**
 * Perfect Dark texSelect.
*/
void texSelect(Gfx **gdlptr, struct sImageTableEntry *tconfig, u32 blendMode, s32 zbufferMode, u32 ulst)
{    
	Gfx *gdl;
    struct tex *tex;
    s32 tile;

    gdl = *gdlptr;

    if (tconfig == NULL)
    {
        texSetRenderMode(&gdl, blendMode, 1, zbufferMode);

        if (zbufferMode >= 2)
        {
            gSPTextureL(gdl++, 0xffff, 0xffff, 0, zbufferMode, G_TX_RENDERTILE, G_ON);
        }
        else
        {
            gSPTextureL(gdl++, 0xffff, 0xffff, 0, 0, G_TX_RENDERTILE, G_ON);
        }

        gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    }
    else
    {
        s32 width;
        s32 height;
        
		u8 format;
		u8 depth;
		s32 lutmode;
		s32 depth2;
		s32 lrs;
        s32 dxt;
		s32 line;
            
        u16* imgheader;

        tex = NULL;
        
        width = tconfig->width;
        height = tconfig->height;
        
        if ((u32) tconfig->index < NUM_TEXTURES)
        {
            texLoad(&tconfig->index, NULL);
        }

        imgheader = (u16 *)PHYS_TO_K0(tconfig->index);
        tex = texFindInPool((imgheader)[-4], NULL);

        if (tconfig->level == 0)
        {
            if (tex != NULL)
            {
                format = tex->gbiformat;
                depth = tex->depth;
                lutmode = tex->lutmodeindex << G_MDSFT_TEXTLUT;                
            }
            else
            {
                format = tconfig->format;
                depth = tconfig->depth;
            }

            switch (depth)
            {
                default:
                    break;
                case G_IM_SIZ_32b:
                    depth2 = G_IM_SIZ_32b;
                    lrs = texLoadBlockTexels32b(width, height, 1) - 1;
                    dxt = texLoadBlockDxt32b(width);
                    line = (s32) (width + 3) >> 2;
                    break;
                case G_IM_SIZ_16b:
                    depth2 = G_IM_SIZ_16b;
                    lrs = texLoadBlockTexels16b(width, height, 1) - 1;
                    dxt = texLoadBlockDxt16b(width);
                    line = (s32) (width + 3) >> 2;
                    break;
                case G_IM_SIZ_8b:
                    depth2 = G_IM_SIZ_16b;
                    lrs = texLoadBlockTexels8b(width, height, 1) - 1;
                    dxt = texLoadBlockDxt8b(width);
                    line = (s32) (width + 7) >> 3;
                    break;
                case G_IM_SIZ_4b:
                    depth2 = G_IM_SIZ_16b;
                    lrs = texLoadBlockTexels4b(width, height, 1) - 1;
                    dxt = texLoadBlockDxt4b(width);
                    line = (s32) (width + 0xF) >> 4;
                    break;
            }

            texSetRenderMode(&gdl, blendMode, 1, zbufferMode);

            if (zbufferMode >= 2)
            {
                gSPTextureL(gdl++, 0xffff, 0xffff, 0, zbufferMode, G_TX_RENDERTILE, G_ON);
            }
            else
            {
                gSPTextureL(gdl++, 0xffff, 0xffff, 0, 0, G_TX_RENDERTILE, G_ON);
            }
    
            gDPSetTextureLOD(gdl++, G_TL_TILE);

            switch (format)
            {
                case G_IM_FMT_RGBA:
                    gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                break;

                case G_IM_FMT_IA:
                    gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                break;

                case G_IM_FMT_I:
                    gDPSetCombineMode(gdl++, G_CC_MODULATEI, G_CC_MODULATEI);
                break;

                case G_IM_FMT_CI:
                    switch (lutmode)
                    {
                        case G_TT_RGBA16:
                            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                        break;

                        case G_TT_IA16:
                            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                        break;
                    }
                break;
            }

            gDPSetTextureImage(gdl++, format, depth2, 1, tconfig->index);

            gDPSetTile(gdl++, format, depth2, 0, 0, G_TX_LOADTILE, 0,
                tconfig->flagsT, G_TX_NOMASK, G_TX_NOLOD,
                tconfig->flagsS, G_TX_NOMASK, G_TX_NOLOD);

            gDPLoadSync(gdl++);

            gDPLoadBlock(gdl++, G_TX_LOADTILE, 0, 0, lrs, dxt);
            
            gDPPipeSync(gdl++);

            if (format == G_IM_FMT_CI)
            {
                u32 tlutCount = lrs + 1;
				u32 tlutStart = (0x3ffU - tex->lastColour) < tlutCount ? (0x3ffU - tex->lastColour) : 0;

				tlutCount -= tlutStart;
                
                gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 0x0100, G_TX_LOADTILE, 0,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
                gDPLoadSync(gdl++);

                gDPLoadTLUT07(gdl++, tlutCount, tlutStart, tex->lastColour + tlutCount, tlutStart);
                gDPPipeSync(gdl++);

                gDPSetTextureLUT(gdl++, lutmode);
            }
            else
            {
                gDPSetTextureLUT(gdl++, G_TT_NONE);
            }

            gDPSetTile(gdl++, format, depth, line, 0, G_TX_RENDERTILE, 0,
                tconfig->flagsT, texMaskBits(height), G_TX_NOLOD,
                tconfig->flagsS, texMaskBits(width), G_TX_NOLOD);

            gDPSetTileSize(gdl++, G_TX_RENDERTILE, ulst, ulst, (((width - 1) << 2) + ulst), (((height - 1) << 2) + ulst));
        }
        else
        {
            s32 tmem;
			s32 lod;
			u8 format;
			u8 depth;
			s32 lutmode;
			s32 depth2;
			s32 lrs;

            tmem = 0;
            lod = (s32)tconfig->level;

            if (tex != NULL)
            {
                format = tex->gbiformat;
                depth = tex->depth;
                lutmode = tex->lutmodeindex << G_MDSFT_TEXTLUT;
            }
            else
            {
                format = tconfig->format;
                depth = tconfig->depth;
            }

            if ((tex != NULL) && (tex->hasExplicitLods))
            {
                texGetDepthAndSize(tex, &depth2, &lrs);
            }
            else
            {
                switch (depth)
                {
                    case G_IM_SIZ_32b:
                        depth2 = G_IM_SIZ_32b;
                        lrs = texLoadBlockTexels32b(width, height, lod) - 1;
                    break;
                    
                    case G_IM_SIZ_16b:
                        depth2 = G_IM_SIZ_16b;
                        lrs = texLoadBlockTexels16b(width, height, lod) - 1;
                    break;
                    
                    case G_IM_SIZ_8b:
                        depth2 = G_IM_SIZ_16b;
                        lrs = texLoadBlockTexels8b(width, height, lod) - 1;
                    break;
                    
                    case G_IM_SIZ_4b:
                        depth2 = G_IM_SIZ_16b;
                        lrs = texLoadBlockTexels4b(width, height, lod) - 1;
                    break;
                }
            }

            texSetRenderMode(&gdl, blendMode, 2, zbufferMode);

            if (zbufferMode >= 2)
            {
                gSPTextureL(gdl++, 0xffff, 0xffff, lod - 1, zbufferMode, G_TX_RENDERTILE, G_ON);
            }
            else
            {
                gSPTexture(gdl++, 0xffff, 0xffff, lod - 1, G_TX_RENDERTILE, G_ON);
            }

            gDPSetTextureLOD(gdl++, G_TL_LOD);

            switch (format)
            {
                case G_IM_FMT_RGBA:
                    gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
                break;

                case G_IM_FMT_IA:
                    gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
                break;

                case G_IM_FMT_I:
                    gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEI2);
                break;

                case G_IM_FMT_CI:
                    switch (lutmode)
                    {
                        case G_TT_RGBA16:
                            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                        break;

                        case G_TT_IA16:
                            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                        break;
                    }
                break;
            }

            gDPSetTextureImage(gdl++, format, depth2, 1, tconfig->index);

            gDPSetTile(gdl++, format, depth2, 0, 0, G_TX_LOADTILE, 0,
                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            
            gDPLoadSync(gdl++);

            gDPLoadBlock(gdl++, G_TX_LOADTILE, 0, 0, lrs, 0);

            gDPPipeSync(gdl++);

            if (format == G_IM_FMT_CI)
            {
                u32 tlutCount = lrs + 1;
				u32 tlutStart = (0x3ffU - tex->lastColour) < tlutCount ? (0x3ffU - tex->lastColour) : 0;

				tlutCount -= tlutStart;
                
                gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 0x0100, G_TX_LOADTILE, 0,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
                gDPLoadSync(gdl++);

                gDPLoadTLUT07(gdl++, tlutCount, tlutStart, tex->lastColour + tlutCount, tlutStart);
                gDPPipeSync(gdl++);

                gDPSetTextureLUT(gdl++, lutmode);
            }
            else
            {
                gDPSetTextureLUT(gdl++, G_TT_NONE);
            }
    
            for (tile = 0; tile < lod; tile++)
            {
                s32 line;

                if (tile > 0)
                {
                    if ((tex != NULL) && (tex->hasExplicitLods))
                    {
                        width = texGetWidthAtLod(tex, tile);
                        height = texGetHeightAtLod(tex, tile);
                    }
                    else
                    {
                        if (width >= 2)
                        {
                            width >>= 1;
                        }
                        
                        if (height >= 2)
                        {
                            height >>= 1;
                        }
                    }
                }

                switch (depth)
                {
                    default:
                        break;
                    case G_IM_SIZ_32b:
                        line = (s32) (width + 3) / 4;
                        break;
                    case G_IM_SIZ_16b:
                        line = (s32) (width + 3) / 4;
                        break;
                    case G_IM_SIZ_8b:
                        line = (s32) (width + 7) / 8;
                        break;
                    case G_IM_SIZ_4b:
                        line = (s32) (width + 0xF) / 16;
                        break;
                }
                
                gDPSetTile(gdl++, format, depth, line, tmem, tile, 0,
                    tconfig->flagsT, texMaskBits(height), tile,
                    tconfig->flagsS, texMaskBits(width), tile);

                gDPSetTileSize(gdl++, tile, ulst, ulst, (((width - 1) << 2) + ulst), (((height - 1) << 2) + ulst));

                tmem += line * height;
            }
        }
        
    }

    *gdlptr = gdl;
}


