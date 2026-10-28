#ifndef _TEXTRELATED_H_
#define _TEXTRELATED_H_
#include <ultra64.h>
#include "bondtypes.h"

enum TEXT_ORIENTATION {
    TEXT_ORIENTATION_SIGNED_ENUM = -1,
    ROT_NORMAL = 0,
    ROT_90CW
};

extern struct font * g_FontBankGothic;
extern struct fontchar * g_FontBankGothicChars;
extern struct font * g_FontZurich;
extern struct fontchar * g_FontZurichChars;

void textInit(void);
void textLoadFonts(void);


Gfx * textRenderRect(Gfx *gdl, s32 ulx, s32 uly, s32 lrx, s32 lry, u32 color);
void textMeasure(s32 *textheight, s32 *textwidth, char *text, struct fontchar *font1, struct font *font2, s32 lineheight);

Gfx *textSetupDraw(Gfx *gdl);
Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *chars, struct font *font, u32 colour, s32 width, s32 height, u32 yOffset, s32 lineheight);
Gfx *textRenderOutlined(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *chars, struct font *font, u32 colour, u32 colour2, s32 width, s32 height, s32 yOffset, s32 lineheight);

Gfx *textEndDraw(Gfx *gdl);
void textSetSpacing(s32 spacing);
void textSetWordWrap(s32 flag);
void textWrap(s32 wrapwidth, char *src, char *dst, struct fontchar *chars, struct font *font);
void textSetOverlapCorrection(s32 flag);

Gfx* textRenderBlackBox(Gfx *glist, s32 *ulx, s32 *uly, s32 *lrx, s32 *lry);

void textSetOrientation(s32 orientation);

#endif
