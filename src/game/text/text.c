/**
 * Drawing text: the two fonts (Bank Gothic and Zurich Bold), and measuring, wrapping and
 * drawing strings glyph by glyph (textRender, textRenderOutlined, textMeasure, textWrap).
 * See docs/subsystems/text.md.
 */
#include <ultra64.h>
#include <memp.h>
#include "game/text/text.h"
#include "bondtypes.h"
#include "game/text/language.h"
#include <fr.h>

#define SPACE_WIDTH 5

#define M_COLOR_R(x) (u8)(x >> 0x18)
#define M_COLOR_G(x) (u8)(x >> 0x10)
#define M_COLOR_B(x) (u8)(x >> 0x08)
#define M_COLOR_A(x) (u8)(x >> 0x00)

// data
s32 g_TextUnused = 0; // Unused
s32 g_TextSpacing = 0;
s32 g_TextOrientation = 0;
s32 g_TextWordWrap = 0;
s32 g_TextOverlapCorrection = -1;
s32 g_TextBilevelFilter = FALSE;
s32 g_TextX = 0;
s32 g_TextY = 0;
s32 g_TextS = 0;
s32 g_TextT = 0;
struct font * g_FontBankGothic = NULL;
struct fontchar * g_FontBankGothicChars = NULL;
struct font * g_FontZurich = NULL;
struct fontchar * g_FontZurichChars = NULL;

// forward declarations

Gfx *textRenderGlyph(Gfx *gdl, s32 *x, s32 *y, struct fontchar *curchar, struct fontchar *prevchar,
		struct font *font, s32 clipX, s32 clipY, s32 clipWidth, s32 clipHeight, s32 yOffset);
Gfx *textRenderGlyphOutlined(Gfx *gdl, s32 *x, s32 *y, struct fontchar *curchar, 
                struct fontchar *prevchar, struct font *font, s32 clipX, s32 clipY, 
                u32 textColour, u32 outlineColour, s32 clipWidth, s32 clipHeight, 
                s32 yOffset);
Gfx *textDrawGlyphQuad(Gfx *gdl, s32 x, s32 y, struct fontchar *curchar, s32 clipX, s32 clipY, s32 clipWidth, s32 clipHeight);

// -------------------------------------------------------------------------------------------------

void textInit(void /*, char* asset */)
{
}

void textSetSpacing(s32 spacing) {
    g_TextSpacing = -spacing;
}

void textSetOrientation(s32 orientation) {
    g_TextOrientation = orientation;
}

void textSetWordWrap(s32 flag) {
    g_TextWordWrap = flag;
}

void textSetOverlapCorrection(s32 flag) {
    g_TextOverlapCorrection = flag;
}

extern u8 _fontbankgothicSegmentEnd NO_SDATA;
extern u8 _fontbankgothicSegmentRomStart NO_SDATA;
extern u8 _fontzurichboldSegmentEnd NO_SDATA;
extern u8 _fontzurichboldSegmentRomStart NO_SDATA;
extern u8 _fontzurichboldSegmentStart NO_SDATA;
extern u8 _fontbankgothicSegmentStart NO_SDATA;

void textLoadFonts(void)
{
    u32 len;
	s32 i;
    
    g_TextSpacing = 0;
    g_TextOrientation = 0;
    g_TextWordWrap = 0;
    g_TextOverlapCorrection = -1;
    g_TextBilevelFilter = 0;
    g_TextX = 0;
    g_TextY = 0;
    g_TextS = 0;
    g_TextT = 0;

    len = (romptr_t)&_fontbankgothicSegmentEnd - (romptr_t)&_fontbankgothicSegmentStart;
	g_FontBankGothic = (struct font *)mempAlloc(len, MEMPOOL_STAGE);
	g_FontBankGothicChars = g_FontBankGothic->chars;

	romCopy(g_FontBankGothic, (void *) &_fontbankgothicSegmentRomStart, len);

    // Convert pointers
	for (i = 0; i < 94; i++) {
		g_FontBankGothicChars[i].pixeldata += (uintptr_t)g_FontBankGothic;
	}

    len = (romptr_t)&_fontzurichboldSegmentEnd - (romptr_t)&_fontzurichboldSegmentStart;
	g_FontZurich = (struct font *)mempAlloc(len, MEMPOOL_STAGE);
	g_FontZurichChars = g_FontZurich->chars;

	romCopy(g_FontZurich, (void *) &_fontzurichboldSegmentRomStart, len);

    // Convert pointers
	for (i = 0; i < 94; i++) {
		g_FontZurichChars[i].pixeldata += (uintptr_t)g_FontZurich;
	}
}

Gfx *textSetupDraw(Gfx *gdl) //fontGfxSetup
{
	gDPPipeSync(gdl++);
	gDPSetCycleType(gdl++, G_CYC_1CYCLE);
	gDPSetColorDither(gdl++, G_CD_DISABLE);
	gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
	gDPSetCombineLERP(gdl++,
			0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0,
			0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
	gDPSetTexturePersp(gdl++, G_TP_NONE);
	gDPSetAlphaCompare(gdl++, G_AC_NONE);
	gDPSetTextureLOD(gdl++, G_TL_TILE);
	gDPSetTextureConvert(gdl++, G_TC_FILT);
	gDPSetTextureLUT(gdl++, G_TT_NONE);

	if (g_TextBilevelFilter) {
		gDPSetTextureFilter(gdl++, G_TF_AVERAGE);
	} else {
		gDPSetTextureFilter(gdl++, G_TF_BILERP);
	}

	return gdl;
}

Gfx *textEndDraw(Gfx *gdl)
{
	gDPPipeSync(gdl++);
	gDPSetColorDither(gdl++, G_CD_BAYER);
	gDPSetTexturePersp(gdl++, G_TP_PERSP);
	gDPSetTextureLOD(gdl++, G_TL_LOD);

	return gdl;
}

Gfx* textRenderBlackBox(Gfx *glist, s32 *ulx, s32 *uly, s32 *lrx, s32 *lry)
{
    gDPSetRenderMode(glist++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(glist++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(glist++, 0, 0, 0x00, 0x00, 0x00, 0x00);
    gDPFillRectangle(glist++, VI_SX(*ulx), *uly, VI_SX(*lrx), *lry);
    gDPSetRenderMode(glist++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(glist++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
    
    return glist;
}

Gfx * textRenderRect(Gfx *gdl, s32 ulx, s32 uly, s32 lrx, s32 lry, u32 color)
{
    u32 r = (u8)(color >> 0x18);
    u32 g = (u8)(color >> 0x10);
    u32 b = (u8)(color >> 0x8);
    u32 a = (u8)(color >> 0);
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, r, g, b, a);
    gDPFillRectangle(gdl++, VI_SX(ulx), uly, VI_SX(lrx), lry);
    gDPSetCombineLERP(gdl++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
    return gdl;
}

/**
 * Render a single character with no outline.
 */
Gfx *textRenderGlyph(Gfx *gdl, s32 *x, s32 *y, struct fontchar *curchar, struct fontchar *prevchar,
		struct font *font, s32 clipX, s32 clipY, s32 clipWidth, s32 clipHeight, s32 yOffset)
{
    s32 stack;
    s32 stack2;
	s32 kerningOffset;
    s32 drawY;

	drawY = *y + yOffset;
	kerningOffset = font->kerning[prevchar->kerningindex * 13 + curchar->kerningindex] + g_TextSpacing;
    *x -= (kerningOffset - 1);

    if (g_TextOrientation || (*x > 0 && *x <= viGetX() && drawY + curchar->baseline <= viGetY()))
    {
        if (clipX + clipWidth >= *x
				&& clipY + clipHeight >= curchar->baseline + drawY
				&& *x >= clipX
				&& curchar->baseline + drawY + curchar->height >= clipY)
        {
            gDPSetTextureLUT(gdl++, G_TT_NONE);
            gDPLoadTextureBlock(gdl++, curchar->pixeldata, G_IM_FMT_I, G_IM_SIZ_8b, ((curchar->width + 7) & 0xF8), curchar->height, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, 0, 0);

            if ((*x + curchar->width) <= clipX + clipWidth)
            {
				if (clipY <= curchar->baseline + drawY)
                {
					if (curchar->baseline + drawY + curchar->height <= clipY + clipHeight)
                    {
                        if (g_TextOrientation)
                        {
                            // E5000000 --------:
                            // B4000000 --------:
                            // B3000000 0400FC00:
                            gSPTextureRectangleFlip(gdl++,
                                /* xl */ VI_SX((((drawY - curchar->baseline) - curchar->height) * 4) + g_TextY),
                                /* yl */ ((*x * 4) + g_TextX),
                                /* xh */ VI_SX(((drawY - curchar->baseline) * 4) + g_TextY),
                                /* yh */ ((*x + curchar->width) * 4) + g_TextX ,
                                /* tile */  G_TX_RENDERTILE,
                                /* s */ g_TextT,
                                /* t */ ((curchar->height - 1) << 5) + g_TextS,
                                /* dsdx */ 0x400,
                                /* dsdy */ (u16)-(0x400 / g_ViBackData->scalex));
                        }
                        else
                        {
                            // E4000000 --------:
                            // B4000000 --------:
                            // B3000000 04000400:
                            gSPTextureRectangle(gdl++,
                                /* xl */ VI_SX((*x * 4) + g_TextX),
                                /* yl */ ((drawY + curchar->baseline) * 4) + g_TextY,
                                /* xh */ VI_SX(((*x + curchar->width) * 4) + g_TextX),
                                /* yh */ ((curchar->baseline + drawY + curchar->height) * 4) + g_TextY,
                                /* tile */  G_TX_RENDERTILE,
                                /* s */ g_TextS,
                                /* t */ g_TextT,
                                /* dsdx */ (0x400 / g_ViBackData->scalex),
                                /* dsdy */ 0x400);
                        }

                    }
                    else if (curchar->baseline + drawY <= clipY + clipHeight)
                    {
                        // E4000000 --------:
                        // B4000000 --------:
                        // B3000000 04000400:
                        gSPTextureRectangle(gdl++,
                            /* xl */ VI_SX((*x * 4) + g_TextX),
                            /* yl */ ((drawY + curchar->baseline) * 4) + g_TextY,
                            /* xh */ VI_SX(((*x + curchar->width) * 4) + g_TextX),
                            /* yh */ clipY + clipHeight + g_TextY,
                            /* tile */  G_TX_RENDERTILE,
                            /* s */ g_TextS,
                            /* t */ g_TextT,
                            /* dsdx */ (0x400 / g_ViBackData->scalex),
                            /* dsdy */ 0x400);
                    }
                }
                else
                {
                    if (curchar->baseline + drawY + curchar->height >= clipY)
                    {
                        // E4000000 --------:
                        // B4000000 --------:
                        // B3000000 04000400:
                        gSPTextureRectangle(gdl++,
                            /* xl */ VI_SX((*x * 4) + g_TextX),
                            /* yl */ (clipY * 4) + g_TextY,
                            /* xh */ VI_SX(((*x + curchar->width) * 4) + g_TextX),
                            /* yh */ ((curchar->baseline + drawY + curchar->height) * 4) + g_TextY,
                            /* tile */  G_TX_RENDERTILE,
                            /* s */ g_TextS,
                            /* t */ (((clipY - drawY) - curchar->baseline) << 5) + g_TextT,
                            /* dsdx */ (0x400 / g_ViBackData->scalex),
                            /* dsdy */ 0x400);
                    }
                }
            }
        }
    }
    
    *x += curchar->width;
    return gdl;
}

/**
 * Render plain text with no outline e.g. briefings and watch menu text.
 */
Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text,
                struct fontchar *chars, struct font *font, u32 colour,
                s32 width, s32 height, u32 yOffset, s32 lineheight)
{
    s32 savedx;
	s32 savedy;
    s32 prevchar;
    s32 stack = 1;
    s32 stack2;
    s32 stack3;
    s32 stack4;
    //s32 stack5;
    
    savedx = *x;
	savedy = *y;
    prevchar = 'H';

	if (lineheight == 0) {
		lineheight = chars['['].height + chars['['].baseline;
	}

    stack = colour; // ????????????
    gDPSetPrimColor(gdl++, 0, 0, M_COLOR_R(colour), M_COLOR_G(colour), M_COLOR_B(colour), M_COLOR_A(colour));
    
    while (*text != '\0')
    {
		if (*text == ' ') {
			*x += 5;
			prevchar = 'H';
			text++;
		} else if (*text == '\n') {
			prevchar = 'H';

            if ((g_TextOverlapCorrection >= 0) && (savedx == *x))
            {
                *y += g_TextOverlapCorrection;
            }
            else
            {
                *y += lineheight;
            }
            
			text++;
			*x = savedx;
		} else if (*text < 0x80) {
			gdl = textRenderGlyph(gdl, x, y, &chars[*text - 0x21], &chars[prevchar - 0x21], font, savedx, savedy, width, height, yOffset);
			prevchar = *text;
			text++;
		} else {
			// Not in the font.
			text++;
		}
	}
    return gdl;
}

/**
 * Draw individual characters with an outline.
 */
Gfx *textRenderGlyphOutlined(Gfx *gdl, s32 *x, s32 *y, struct fontchar *curchar, 
                       /*sp70*/ struct fontchar *prevchar, struct font *font, s32 clipX, s32 clipY, 
                       /*sp80*/u32 textColour, u32 outlineColour, s32 clipWidth, s32 clipHeight, 
                       /*sp90*/ s32 yOffset)
{
    s32 offsetX;
    s32 offsetY;
    s32 kerningOffset;
    s32 drawY;
    s32 tmpx;
    s32 tmpy;
    u32 rgba;

    drawY = *y + yOffset;

    kerningOffset = font->kerning[prevchar->kerningindex * 13 + curchar->kerningindex] + g_TextSpacing;
    *x -= (kerningOffset - 1);

    if (*x > 0
            && *x <= viGetX()
            && drawY + curchar->baseline <= viGetY()
            && *x <= clipX + clipWidth
            && drawY + curchar->baseline <= clipY + clipHeight
            && *x >= clipX
            && drawY + curchar->baseline + curchar->height >= clipY) {
        gDPSetTextureLUT(gdl++, G_TT_NONE);
        gDPLoadTextureBlock(gdl++, curchar->pixeldata, G_IM_FMT_I, G_IM_SIZ_8b, ((curchar->width + 7) & 0xF8), curchar->height, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, 0, 0);

        /**
         * Render outlines.
         * This is done by rendering the character 8 times in diffent offsets around the origin:
         * left/right/up/down and the four diagonals.
         */
        rgba = outlineColour;
        gDPSetPrimColor(gdl++, 0, 0, M_COLOR_R(rgba), M_COLOR_G(rgba), M_COLOR_B(rgba), M_COLOR_A(rgba));

        for (offsetX = -1; offsetX < 2; offsetX++) {
            for (offsetY = -1; offsetY < 2; offsetY++) {
                tmpx = *x + offsetX;
                tmpy = drawY + offsetY;

                if (offsetX != 0 || offsetY != 0) { // Skip the center since that will be covered by the main character anyway.
                    gdl = textDrawGlyphQuad(gdl, tmpx, tmpy, curchar, clipX, clipY, clipWidth, clipHeight);
                }
            }
        }

        // Render main character.
        tmpx = *x;
        tmpy = drawY;
        rgba = textColour;
        gDPSetPrimColor(gdl++, 0, 0, M_COLOR_R(rgba), M_COLOR_G(rgba), M_COLOR_B(rgba), M_COLOR_A(rgba));
        gdl = textDrawGlyphQuad(gdl, tmpx, tmpy, curchar, clipX, clipY, clipWidth, clipHeight);
    }

    *x += curchar->width;

    return gdl;
}

/**
 * Draw a quad for the current glyph, clipping it inside the region defined by clipX, clipY, clipWidth, and clipHeight.
 */
Gfx *textDrawGlyphQuad(Gfx *gdl, s32 x, s32 y, struct fontchar *curchar, s32 clipX, s32 clipY, s32 clipWidth, s32 clipHeight)
{
    if (clipX + clipWidth >= curchar->width + x) // Check if the glyph's right edge is inside or touching the clipping rect's right edge.
    {
        if (curchar->baseline + y >= clipY) // Check if the glyph's top is below the clip top.
        {
            if (clipY + clipHeight >= curchar->baseline + y + curchar->height) // Check if the glyph's bottom is above the clip bottom.
            {
                if (g_TextOrientation) // Check if vertical text. Not sure if ever used for outlined text?
                {
                    gSPTextureRectangleFlip(gdl++, 
                        /* xl */ VI_SX(((y - curchar->baseline) - curchar->height) << 2), 
                        /* yl */ x << 2, 
                        /* xh */ VI_SX((y - curchar->baseline) << 2), 
                        /* yh */ (x + curchar->width) << 2, 
                        /* tile */ G_TX_RENDERTILE, 
                        /* s */ 0, 
                        /* t */ (curchar->height - 1) << 5, 
                        /* dsdx */ 0x400,
                        /* dsdy */ (u16)-(0x400 / g_ViBackData->scalex));
                }
                else
                {
                    gSPTextureRectangle(gdl++, 
                        /* xl */ VI_SX(x << 2), 
                        /* yl */ (y + curchar->baseline) << 2, 
                        /* xh */ VI_SX((x + curchar->width) << 2), 
                        /* yh */ (curchar->baseline + y + curchar->height) << 2, 
                        /* tile */ G_TX_RENDERTILE, 
                        /* s */ 0, 
                        /* t */ 0, 
                        /* dsdx */ (0x400 / g_ViBackData->scalex),
                        /* dsdy */ 0x400);
                }
            }
            else if (clipY + clipHeight >= curchar->baseline + y) // Glyph bottom is below the clipping rectangle.
            {
                // Bottom coordinates clipped to savedy + height.
                gSPTextureRectangle(gdl++, 
                    /* xl */ VI_SX(x << 2), 
                    /* yl */ (y + curchar->baseline) << 2, 
                    /* xh */ VI_SX((x + curchar->width) << 2), 
                    /* yh */ (clipY + clipHeight << 2), 
                    /* tile */ G_TX_RENDERTILE, 
                    /* s */ 0, 
                    /* t */ 0, 
                    /* dsdx */ (0x400 / g_ViBackData->scalex),
                    /* dsdy */ 0x400); 
            }
        }
        else if (curchar->baseline + y + curchar->height >= clipY) // Glyph top is above the clipping rectangle.
        {
            gSPTextureRectangle(gdl++, // Top clipped to fit.
                /* xl */ VI_SX(x << 2), 
                /* yl */ clipY<< 2, 
                /* xh */ VI_SX((x + curchar->width) << 2), 
                /* yh */ (curchar->baseline + y + curchar->height) << 2, 
                /* tile */ G_TX_RENDERTILE, 
                /* s */ 0, 
                /* t */ ((clipY - curchar->baseline) - y) << 5, 
                /* dsdx */ (0x400 / g_ViBackData->scalex),
                /* dsdy */ 0x400);
        }
    }

    return gdl;
}

/**
 * Draw strings of text with an outline.
 * Used for ammo counter, bottom left HUD messages, countdown timers.
 */
Gfx *textRenderOutlined(Gfx *gdl, s32 *x, s32 *y, 
                    char *text, struct fontchar *chars, struct font *font, 
                    u32 colour, u32 colour2, s32 width, s32 height, 
                    s32 yOffset, s32 lineheight)
{
	s32 savedy;
    s32 savedx;
    s32 prevchar;
    
    savedx = *x;
	savedy = *y;
    prevchar = 'H';

	if (lineheight == 0) {
		lineheight = chars['['].height + chars['['].baseline;
	}

    while (*text != '\0')
    {
		if (*text == ' ') {
			*x += SPACE_WIDTH;
			prevchar = 'H';
			text++;
		} else if (*text == '\n') {
			prevchar = 'H';
			*x = savedx;
            *y += lineheight;
			text++;
		} else if (*text < 0x80) {
            // Render individual characters with outline.
            gdl = textRenderGlyphOutlined(gdl, x, y, &chars[*text - 0x21], &chars[prevchar - 0x21], font, savedx, savedy, colour, colour2, width, height, yOffset);
			prevchar = *text;
			text++;
		} else {
			// Not in the font.
			text++;
		}
	}

    return gdl;
}

void textMeasure(s32 *textheight, s32 *textwidth, char *text, struct fontchar *font1, struct font *font2, s32 lineheight)
{
    char prevchar;
	s32 longest;
	s32 tmp;

	prevchar = 'H';
    tmp = 0;
	longest = 0;
	*textheight = 0;
	*textwidth = 0;

    if (lineheight == 0)
    {
		lineheight = font1['['].baseline + font1['['].height;
    }
    
    while (*text != '\0')
    {
        if (*text == ' ')
        {
            // Space
            if (text[1] != '\n') {
                *textwidth += SPACE_WIDTH;
            }

            prevchar = 'H';
            text++;
        }
        else if (*text == '\n')
        {
            // Line break
            if (*textwidth > longest) {
                longest = *textwidth;
            }

            *textwidth = 0;
            *textheight += lineheight;
            text++;
        }
        else if (*text < 0x80)
        {
            // Normal single-byte character
            tmp = font2->kerning[font1[prevchar - 0x21].kerningindex * 13 + font1[*text - 0x21].kerningindex] + g_TextSpacing - 1;
            *textwidth = font1[*text - 0x21].width + *textwidth - tmp;

            prevchar = *text;
            text++;
        }
        else
        {
            // Not in the font.
            text++;
        }
    };
    
    if (*textwidth < longest)
    {
        *textwidth = longest;
    }
}

void textWrap(s32 wrapwidth, char *src, char *dst, struct fontchar *chars, struct font *font)
{
	s32 curlinewidth = 0;
	bool itfits;
	s32 wordlen;
	s32 wordwidth;
	s32 wordheight = 0;
	s32 more = 1;
	s32 v1;
	s32 i;
	u32 stack;
	char curword[32];

    while (more == 1) {
		// Load the next word
		wordwidth = 0;
		wordlen = 0;
		v1 = 0;

        // Read one word into the curword buffer.
        while (*src > ' ')
        {
            curword[wordlen] = *src;
			v1 += chars[*src - 0x21].width;
			src++;
			wordlen++;
        }
        
        // Null terminate the word
        curword[wordlen] = '\0';

        textMeasure(&wordheight, &wordwidth, curword, chars, font, 0);

        // Track the current line's pixel length
        curlinewidth += wordwidth;

        if (curlinewidth <= wrapwidth) {
            itfits = 1;
        } else {
            itfits = 0;
        }

        if (*src == '\n')
        {
            // Write a new line and indent
			if (!itfits) {
				*dst = '\n';
				dst++;

				for (i = 0; i < g_TextWordWrap; i++) {
					*dst = ' ';
					dst++;
				}
			}

			curlinewidth = 0;

			// Write the current word
			for (i = 0; i < wordlen; i++) {
				*dst = curword[i];
				dst++;
			}

			// Write the original new line that was in src
			*dst = '\n';
			dst++;
        }
        else if (*src == ' ')
        {
            if (!itfits)
            {
				// Write a new line and indent
				*dst = '\n';
				dst++;

				for (i = 0; i < g_TextWordWrap; i++)
                {
					*dst = ' ';
					dst++;
				}

				curlinewidth = g_TextWordWrap * SPACE_WIDTH + wordwidth;
			}

			curlinewidth += SPACE_WIDTH;

			// Write the current word
			for (i = 0; i < wordlen; i++) {
				*dst = curword[i];
				dst++;
			}

			// Write the trailing space
			*dst = ' ';
			dst++;
        }
        else if (*src == '\0')
        {
            more = 0;

			if (!itfits) {
				// Write a new line and indent
				*dst = '\n';
				dst++;

				for (i = 0; i < g_TextWordWrap; i++) {
					*dst = ' ';
					dst++;
				}
			}

			// Write the current word
			for (i = 0; i < wordlen; i++) {
				*dst = curword[i];
				dst++;
			}

			// Write the null terminator
			*dst = '\0';
        }

        src++;
    }
}

// Unreferenced
void textUnused(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {

}

// Unreferenced
u32 textIdentity(u32 param_1,u32 param_2){
  return param_1;
}


