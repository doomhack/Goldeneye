#ifndef _GBI_EXT_H_
#    define _GBI_EXT_H_
#include <PR/gbi.h>

/**
 * This file should contain most of the extensions to gbi avoiding loosing
 * changes should the OS ever be updated.
 */

/**
   These are some helper constants. Since they relate to nothing else except GBI
   macros, Ill place them here, to save reserving names that might be used
   elsewhere - though doubtfull.
 */
#define MASK_2 1
#define MASK_4 2
#define MASK_8 3
#define MASK_16 4
#define MASK_32 5
#define MASK_64 6
#define MASK_128 7
#define MASK_256 8
#define MASK_512 9
#define MASK_1024 10

typedef enum
{
    TextureTypes_SIGNED_ENUM = -1,
    TEXTURETYPE_LOD,
    TEXTURETYPE_DETAIL,
    TEXTURETYPE_MIPMAP,
    TEXTURETYPE_TILE,
    TEXTURETYPE_TILE_PRESWAPPED
} TextureTypes;

/* Set the Texel Scale (0.0 - 1.0) for multiplying UV's eg, 0.5 * (31 << 6) = 31.0 */
#define CALC_TEXSCALE(scale) scale < 1.0f ? scale * 0x10000 : 0xffff

/* Set the Lower Right S Coordinate using texture width */
#define CALC_LRS(width, height, siz) ((((width) * (height) + siz##_INCR) >> siz##_SHIFT) - 1)

#define CALC_TILESIZE(texel)         ((texel)-1) << G_TEXTURE_IMAGE_FRAC



/* Viewport (Vp) Y scale and Z scale/translation: F3DEX3 wants Y negated and a bigger Z range */
#define GE_VP_YSCALE(s) (-(s))
#define GE_VP_ZHALF     (G_NEW_MAXZ / 2)

/* GE's texture command, replaced with real ones when a display list is loaded (texcmds.c) */
#define G_SETTEX 0xc0

/*
 * gsSP4Triangles, from GE's original microcode's 4 triangle command (G_TRI4): here two
 * commands, each a G_TRI2, a G_TRI1 or a no-op, so that
 * triangles whose three indices are all 0 are left out as G_TRI4 does.
 */
#define GE_TRI_EMPTY(x, y, z) ((((x) | (y) | (z))) == 0)
#define GE_TRIPAIR_W0(x1, y1, z1, x2, y2, z2)                                               \
    (GE_TRI_EMPTY(x1, y1, z1)                                                               \
         ? (GE_TRI_EMPTY(x2, y2, z2) ? _SHIFTL(G_SPNOOP, 24, 8)                             \
                                     : (_SHIFTL(G_TRI1, 24, 8) | __gsSP1Triangle_w1(x2, y2, z2))) \
         : (_SHIFTL(GE_TRI_EMPTY(x2, y2, z2) ? G_TRI1 : G_TRI2, 24, 8) | __gsSP1Triangle_w1(x1, y1, z1)))
#define GE_TRIPAIR_W1(x1, y1, z1, x2, y2, z2) \
    ((GE_TRI_EMPTY(x1, y1, z1) || GE_TRI_EMPTY(x2, y2, z2)) ? 0 : __gsSP1Triangle_w1(x2, y2, z2))
#define gsSP4Triangles(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4)    \
    { GE_TRIPAIR_W0(x1, y1, z1, x2, y2, z2), GE_TRIPAIR_W1(x1, y1, z1, x2, y2, z2) }, \
    { GE_TRIPAIR_W0(x3, y3, z3, x4, y4, z4), GE_TRIPAIR_W1(x3, y3, z3, x4, y4, z4) }

/* Fade Modes ... for GE? */
#define G_CC_MODULATEIFADE    TEXEL0, 0, SHADE, 0, 0, 0, 0, ENVIRONMENT
#define G_CC_MODULATERGBFADE  G_CC_MODULATEIFADE
#define G_CC_MODULATEIFADEA   TEXEL0, 0, SHADE, 0, TEXEL0, 0, ENVIRONMENT, 0
#define G_CC_MODULATEFADE     TEXEL0, 0, SHADE, 0, ENVIRONMENT, 0, TEXEL0, 0
#define G_CC_MODULATERGBFADEA G_CC_MODULATEIFADEA
#define G_CC_FADE             SHADE, 0, ENVIRONMENT, 0, SHADE, 0, ENVIRONMENT, 0
#define G_CC_FADEA            TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0
#define G_CC_DECALFADE        0, 0, 0, TEXEL0, 0, 0, 0, ENVIRONMENT
#define G_CC_DECALFADEA       0, 0, 0, TEXEL0, TEXEL0, 0, ENVIRONMENT, 0
#define G_CC_BLENDRGBFADEA    TEXEL0, SHADE, TEXEL0_ALPHA, SHADE, 0, 0, 0, ENVIRONMENT
#define G_CC_ADDRGBFADE       TEXEL0, 0, TEXEL0, SHADE, 0, 0, 0, ENVIRONMENT
#define G_CC_SHADEFADEA       0, 0, 0, SHADE, 0, 0, 0, ENVIRONMENT


/* Custom version of RM_AA_ZB_XLU_SURF with Z_UPD */
#define RM_CUSTOM_AA_ZB_XLU_SURF(clk) \
    RM_AA_ZB_XLU_SURF(clk) | Z_UPD



#define G_RM_CUSTOM_AA_ZB_XLU_SURF  RM_CUSTOM_AA_ZB_XLU_SURF(1)
#define G_RM_CUSTOM_AA_ZB_XLU_SURF2 RM_CUSTOM_AA_ZB_XLU_SURF(2)




#define	gDPLoadTLUT06(pkt, a, b, c, d)				                                        \
{                                                                                           \
    Gfx *_g = (Gfx *)pkt;                                                                   \
    _g->words.w0 = _SHIFTL(G_LOADTLUT, 24, 8) | _SHIFTL((a), 14, 10) | _SHIFTL((b), 2, 10); \
    _g->words.w1 = _SHIFTL(0x06, 24, 8) | _SHIFTL((c), 14, 10) | _SHIFTL((d), 2, 10);       \
}
#define	gDPLoadTLUT07(pkt, a, b, c, d)				                                        \
{                                                                                           \
    Gfx *_g = (Gfx *)pkt;                                                                   \
    _g->words.w0 = _SHIFTL(G_LOADTLUT, 24, 8) | _SHIFTL((a), 14, 10) | _SHIFTL((b), 2, 10); \
    _g->words.w1 = _SHIFTL(0x07, 24, 8) | _SHIFTL((c), 14, 10) | _SHIFTL((d), 2, 10);       \
}

#define	gDPLoadTLUTCmd2(pkt, tile, count)				\
{									\
	Gfx *_g = (Gfx *)pkt;						\
									\
	_g->words.w0 = _SHIFTL(G_LOADTLUT, 24, 8) | 0xff0;			\
	_g->words.w1 = _SHIFTL((tile), 24, 3) | _SHIFTL((count), 14, 10) | 0xff0;\
}

/*
 Use Texture from bank.
 If detail, use shift eg 15 for 2x detail per base texel, 14 fpr 4x.

 Confirm params from decomp in CO expand function

 @param        cms: s-axis mirror, no-mirror, wrap, and clamp flags
 @param        cmt: t-axis mirror, no-mirror, wrap, and clamp flags eg G_TX_CLAMP
 @param       tile: Number of tiles
 @param     shifts: s-coordinate shift value or G_TX_NOLOD
 @param     shiftt: t-coordinate shift value or G_TX_NOLOD
 @param       type: texture type: E.g. TEXTURETYPE_DETAIL
 @param   minlevel: Clamp at tile 0 + frac (eg, 0.5)
 @param  detail_id: Detail Texture ID
 @param texture_id: Texture ID
 */
#define gsSPUseTexture(cms, cmt, tile, shifts, shiftt, type, minlevel, detail_id, texture_id) \
{                                                    \
    {                                                \
        (_SHIFTL(G_SETTEX, 24, 8) |                  \
         _SHIFTL((cms), 22, 2) |                     \
         _SHIFTL((cmt), 20, 2) |                     \
         _SHIFTL((tile), 18, 2) |                    \
         _SHIFTL((shifts), 14, 4) |                  \
         _SHIFTL((shiftt), 10, 4) |                  \
         _SHIFTL((type), 0, 3)),                     \
        (_SHIFTL((minlevel), 24, 8) |                \
         _SHIFTL((detail_id), 12, 12) |              \
         _SHIFTL((texture_id), 0, 12))               \
    }                                                \
}


/**
 * Like gDPSetPrimColor, but is useful when the input colour is already in
 * RGBA format. It avoids unnecessary bitshifting and masking.
 */
#define	gDPSetPrimColorViaWord(pkt, m, l, rgba)     \
{                                                   \
	Gfx *_g = (Gfx *)(pkt);                         \
	_g->words.w0 =	(_SHIFTL(G_SETPRIMCOLOR, 24, 8) \
			| _SHIFTL(m, 8, 8)                      \
			| _SHIFTL(l, 0, 8));                    \
	_g->words.w1 =  (rgba);                         \
}

#define gDPSetEnvColorViaWord(pkt, rgba) gDPSetColor(pkt, G_SETENVCOLOR, rgba)
#define gDPSetFogColorViaWord(pkt, rgba) gDPSetColor(pkt, G_SETFOGCOLOR, rgba)



/*
 * guLookAtReflect for the game's LookAt. libultra writes Fast3D's layout (two 16 byte
 * lights, direction at byte 8 of each); F3DEX3's is two 4 byte directions.
 */
#include <PR/gu.h>
static inline void geLookAtReflect(Mtx *m, LookAt *l, float xEye, float yEye, float zEye,
                                   float xAt, float yAt, float zAt, float xUp, float yUp, float zUp)
{
    union {
        long long int align;
        signed char bytes[32];
    } fast3d;

    guLookAtReflect(m, (LookAt *)&fast3d, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);

    for (int i = 0; i < 3; i++)
    {
        l->l[0].l.dir[i] = fast3d.bytes[8 + i];
        l->l[1].l.dir[i] = fast3d.bytes[24 + i];
    }

    l->l[0].l.pad1 = 0;
    l->l[1].l.pad1 = 0;
}

#endif
