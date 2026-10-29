/**
 * Texture commands: the RDP commands that load a texture into TMEM and set up the tiles
 * that draw it.
 *
 * Model and room display lists in the ROM name their textures with Rare's own command,
 * G_SETTEX (gsSPUseTexture in gbi_extension.h). When one is loaded, texLoadFromGdl copies
 * its display list and replaces each G_SETTEX with real commands: a LoadBlock of all the
 * texture's levels of detail (and a LoadTLUT of its palette), and the tiles to draw it.
 * The tile settings are remembered as they're written, so a texture that sets a tile the
 * way it's already set writes nothing for it.
 *
 * texSelect does the same at draw time for one image from an image table (the HUD, sky,
 * explosions, menus), with the render mode to draw it with.
 *
 * TMEM addresses and row lengths are in 64-bit words. A 32-bit texture is split into two
 * 16-bit halves in TMEM, so its rows take as many words as a 16-bit texture's.
 */

#include <ultra64.h>
#include <R4300.h>
#include "gbi_extension.h"
#include "bondconstants.h"
#include "game/gfx/texcmds.h"
#include "game/gfx/texload.h"
#include "game/world/lightfixture.h"
#include "game/world/water.h"

/* Where palettes go in TMEM: the top half */
#define TMEM_PALETTE 0x100

/* Texture numbers of the water in Caverns, and in Dam and Complex */
#define TEXTURE_WATER_CAVERNS 1508
#define TEXTURE_WATER_DAM 1511

/* A tile's settings, as gDPSetTile takes them */
typedef struct
{
    s32 format;
    s32 depth;
    s32 line;
    s32 tmem;
    s32 cms;
    s32 cmt;
    s32 masks;
    s32 maskt;
    s32 shifts;
    s32 shiftt;
} TileSettings;

/* A tile's size, as gDPSetTileSize takes it (10.2 fixed point) */
typedef struct
{
    s32 uls;
    s32 ult;
    s32 lrs;
    s32 lrt;
} TileSize;

/* What texLoadFromGdl last set each tile and the palette mode to */
static struct
{
    TileSettings tiles[8];
    TileSize sizes[8];
    u8 tilesKnown; /* a bit for each tile whose settings are in tiles[] */
    u8 sizesKnown;
    s32 lutMode; /* -1 if not known */
} s_Rdp;

/* The load tile, set for loading a palette */
static const TileSettings s_PaletteLoadTile = { .line = 1, .tmem = TMEM_PALETTE };

static void forgetTileSettings(void)
{
    s_Rdp.tilesKnown = 0;
    s_Rdp.sizesKnown = 0;
    s_Rdp.lutMode = -1;
}

static bool tileSettingsEqual(const TileSettings *a, const TileSettings *b)
{
    return a->format == b->format && a->depth == b->depth && a->line == b->line
        && a->tmem == b->tmem && a->cms == b->cms && a->cmt == b->cmt
        && a->masks == b->masks && a->maskt == b->maskt
        && a->shifts == b->shifts && a->shiftt == b->shiftt;
}

static Gfx *setTile(Gfx *gdl, s32 tile, const TileSettings *settings)
{
    if ((s_Rdp.tilesKnown & (1 << tile)) && tileSettingsEqual(&s_Rdp.tiles[tile], settings))
    {
        return gdl;
    }

    s_Rdp.tiles[tile] = *settings;
    s_Rdp.tilesKnown |= 1 << tile;

    gDPSetTile(gdl++, settings->format, settings->depth, settings->line, settings->tmem, tile, 0,
               settings->cmt, settings->maskt, settings->shiftt,
               settings->cms, settings->masks, settings->shifts);

    return gdl;
}

static Gfx *setTileSize(Gfx *gdl, s32 tile, const TileSize *size)
{
    const TileSize *known = &s_Rdp.sizes[tile];

    if ((s_Rdp.sizesKnown & (1 << tile)) && known->uls == size->uls && known->ult == size->ult
        && known->lrs == size->lrs && known->lrt == size->lrt)
    {
        return gdl;
    }

    s_Rdp.sizes[tile] = *size;
    s_Rdp.sizesKnown |= 1 << tile;

    gDPSetTileSize(gdl++, tile, size->uls, size->ult, size->lrs, size->lrt);

    return gdl;
}

static Gfx *setLutMode(Gfx *gdl, s32 mode)
{
    if (mode != s_Rdp.lutMode)
    {
        s_Rdp.lutMode = mode;
        gDPSetTextureLUT(gdl++, mode);
    }

    return gdl;
}

/** The length of a row of `width` texels in TMEM */
static s32 lineWords(s32 depth, s32 width)
{
    switch (depth)
    {
        case G_IM_SIZ_4b:
            return (width + 15) / 16;
        case G_IM_SIZ_8b:
            return (width + 7) / 8;
        default:
            return (width + 3) / 4;
    }
}

/**
 * The pixel size a texture is loaded with. LoadBlock moves 4-bit and 8-bit textures as
 * 16-bit ones, 4 texels to a word.
 */
static s32 loadDepth(s32 depth)
{
    return depth == G_IM_SIZ_32b ? G_IM_SIZ_32b : G_IM_SIZ_16b;
}

/**
 * LoadBlock's dxt for rows `line` words long: how much to step a row counter for each
 * word loaded, in 1.11 fixed point, so that it counts the rows (and swaps the words of
 * odd ones, as TMEM wants). A 32-bit load moves half as many texels per word.
 */
static s32 loadBlockDxt(s32 depth, s32 line)
{
    s32 words = depth == G_IM_SIZ_32b ? line * 2 : line;

    return (2048 + words - 1) / words;
}

/** The mask bits for a tile `size` texels across: enough to cover it, at most 8 */
static s32 maskBits(s32 size)
{
    s32 bits = 0;

    for (size--; size > 0 && bits < 8; size >>= 1)
    {
        bits++;
    }

    return bits;
}

/** How many texels (as loaded, see loadDepth) all of a texture's levels of detail are */
static s32 texLoadTexels(const struct tex *tex)
{
    s32 numLods = tex->maxlod ? tex->maxlod : 1;
    s32 words = 0;

    for (s32 lod = 0; lod < numLods; lod++)
    {
        words += texGetHeightAtLod(tex, lod) * lineWords(tex->depth, texGetWidthAtLod(tex, lod));
    }

    return words * 4;
}

/** The TMEM a level of detail of a texture takes */
static s32 texLodWords(const struct tex *tex, s32 lod)
{
    return texGetHeightAtLod(tex, lod) * lineWords(tex->depth, texGetWidthAtLod(tex, lod));
}

/**
 * Loads a texture's palette into TMEM. It's stored after the pixels, `texels` 16-bit texels
 * into the texture (which gDPSetTextureImage has set as an image 1 texel wide). LoadTLUT's
 * columns only go up to 1023, so one further in than that is reached as a row instead.
 */
static Gfx *loadPalette(Gfx *gdl, const struct tex *tex, u32 texels)
{
    u32 row = 0x3ffU - tex->lastColour < texels ? 0x3ffU - tex->lastColour : 0;
    u32 column = texels - row;

    gDPLoadSync(gdl++);
    gDPLoadTLUT07(gdl++, column, row, tex->lastColour + column, row);
    gDPPipeSync(gdl++);

    return gdl;
}

/**
 * Loads all of a texture's levels of detail into TMEM at `tmem` with one LoadBlock, and its
 * palette if it has one. With a dxt of 0 the rows aren't counted, for textures whose odd
 * rows are already swapped.
 */
static Gfx *loadTexture(Gfx *gdl, const struct tex *tex, s32 tmem, s32 dxt)
{
    s32 depth = loadDepth(tex->depth);
    s32 texels = texLoadTexels(tex);

    gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);
    gdl = setTile(gdl, G_TX_LOADTILE, &(TileSettings){ .format = tex->gbiformat, .depth = depth, .tmem = tmem });
    gDPLoadSync(gdl++);
    gDPLoadBlock(gdl++, G_TX_LOADTILE, 0, 0, texels - 1, dxt);
    gDPPipeSync(gdl++);

    if (tex->lutmodeindex != 0)
    {
        gdl = setTile(gdl, G_TX_LOADTILE, &s_PaletteLoadTile);
        gdl = loadPalette(gdl, tex, texels);
    }

    return gdl;
}

/** The dxt for loading a texture whose odd rows the RDP swaps */
static s32 texLoadDxt(const struct tex *tex)
{
    return loadBlockDxt(tex->depth, lineWords(tex->depth, tex->width));
}

/** A G_SETTEX's wrap mode: 0 wrap, 1 clamp, 2 mirror */
static s32 wrapMode(s32 mode)
{
    if (mode == 1)
    {
        return G_TX_CLAMP;
    }

    if (mode == 2)
    {
        return G_TX_MIRROR;
    }

    return G_TX_WRAP;
}

/** Settings for a tile that draws level `lod` of a texture */
static TileSettings texTileSettings(const struct tex *tex, s32 lod)
{
    return (TileSettings){
        .format = tex->gbiformat,
        .depth = tex->depth,
        .line = lineWords(tex->depth, texGetWidthAtLod(tex, lod)),
        .masks = maskBits(texGetWidthAtLod(tex, lod)),
        .maskt = maskBits(texGetHeightAtLod(tex, lod)),
    };
}

/**
 * Sets up `tile` to draw level `lod` of a texture. A `uvOffset` of 2 moves it by half a
 * texel (the tile size is 10.2 fixed point), unless the texture's levels of detail are
 * stored.
 */
static Gfx *setTextureTile(Gfx *gdl, const struct tex *tex, s32 tile, s32 lod, const TileSettings *settings, s32 uvOffset)
{
    s32 offset = (uvOffset == 2 && !tex->hasExplicitLods) ? 2 : 0;
    TileSize size = {
        .uls = offset,
        .ult = offset,
        .lrs = ((texGetWidthAtLod(tex, lod) - 1) << 2) + offset,
        .lrt = ((texGetHeightAtLod(tex, lod) - 1) << 2) + offset,
    };

    gdl = setLutMode(gdl, tex->lutmodeindex << G_MDSFT_TEXTLUT);
    gdl = setTile(gdl, tile, settings);
    gdl = setTileSize(gdl, tile, &size);

    return gdl;
}

/** A G_SETTEX command's fields (see gsSPUseTexture) */
typedef struct
{
    s32 type;      /* TEXTURETYPE_* */
    s32 texnum;
    s32 detailnum; /* TEXTURETYPE_DETAIL's detail texture */
    s32 cms;       /* wrap modes, see wrapMode */
    s32 cmt;
    s32 uvOffset;  /* 2: move the tiles by half a texel */
    s32 shifts;    /* TEXTURETYPE_LOD and _DETAIL: tile 0's shifts */
    s32 shiftt;
    s32 minLevel;  /* TEXTURETYPE_LOD and _DETAIL: the LOD fraction tile 0 is clamped at */
} TexCmd;

static TexCmd readTexCmd(const Gfx *cmd)
{
    u32 w0 = cmd->words.w0;
    u32 w1 = cmd->words.w1;

    return (TexCmd){
        .type = w0 & 7,
        .texnum = w1 & 0xfff,
        .detailnum = (w1 >> 12) & 0xfff,
        .cms = (w0 >> 22) & 3,
        .cmt = (w0 >> 20) & 3,
        .uvOffset = (w0 >> 18) & 3,
        .shifts = (w0 >> 14) & 0xf,
        .shiftt = (w0 >> 10) & 0xf,
        .minLevel = w1 >> 24,
    };
}

/**
 * Sets up tiles `firstTile` on to draw each of a texture's levels of detail, which are in
 * TMEM from `tmem`. In two-cycle mode the RDP blends a tile with the next one, so a
 * texture with one level sets up the tile after it the same way.
 */
static Gfx *setLodTiles(Gfx *gdl, const struct tex *tex, const TexCmd *cmd, s32 firstTile, s32 tmem)
{
    for (s32 pass = 0; pass < (tex->maxlod == 1 ? 2 : 1); pass++)
    {
        s32 lodTmem = tmem;

        for (s32 lod = 0; lod < tex->maxlod; lod++)
        {
            TileSettings settings = texTileSettings(tex, lod);

            settings.tmem = lodTmem;
            settings.cms = wrapMode(cmd->cms);
            settings.cmt = wrapMode(cmd->cmt);
            settings.shifts = lod;
            settings.shiftt = lod;

            gdl = setTextureTile(gdl, tex, firstTile + pass + lod, lod, &settings, cmd->uvOffset);
            lodTmem += texLodWords(tex, lod);
        }
    }

    return gdl;
}

/**
 * Sets up tile 0 for the texture the RDP blends into with LOD (the detail texture, or for
 * TEXTURETYPE_LOD the texture itself). Its table entry can place the tile inside the
 * texture and shrink its masks.
 */
static Gfx *setLodTile0(Gfx *gdl, const struct tex *tex, const TexCmd *cmd)
{
    const struct image_entry *entry = &g_Textures[tex->texturenum];
    TileSettings settings = texTileSettings(tex, 0);

    gDPSetPrimColorViaWord(gdl++, cmd->minLevel, 0, 0xffffffff);

    settings.tmem = entry->flag3 + settings.line * entry->flag4;
    settings.masks -= entry->flag5;
    settings.maskt -= entry->flag6;
    settings.shifts = cmd->shifts;
    settings.shiftt = cmd->shiftt;

    return setTextureTile(gdl, tex, 0, 0, &settings, cmd->uvOffset);
}

/** Expands a G_SETTEX for texture `tex` (and `detail`, for TEXTURETYPE_DETAIL) */
static Gfx *expandTexCmd(Gfx *gdl, const TexCmd *cmd, const struct tex *tex, const struct tex *detail)
{
    switch (cmd->type)
    {
        case TEXTURETYPE_LOD:
            gdl = loadTexture(gdl, tex, 0, 0);
            gdl = setLodTile0(gdl, tex, cmd);
            gdl = setLodTiles(gdl, tex, cmd, 1, 0);
            break;

        case TEXTURETYPE_DETAIL:
        {
            // The detail texture goes first in TMEM, then the texture
            s32 tmem = texLodWords(detail, 0);

            gdl = loadTexture(gdl, detail, 0, texLoadDxt(detail));
            gDPTileSync(gdl++);
            gdl = loadTexture(gdl, tex, tmem, 0);
            gdl = setLodTile0(gdl, detail, cmd);
            gdl = setLodTiles(gdl, tex, cmd, 1, tmem);
            break;
        }

        case TEXTURETYPE_MIPMAP:
            gdl = loadTexture(gdl, tex, 0, 0);
            gdl = setLodTiles(gdl, tex, cmd, 0, 0);
            break;

        case TEXTURETYPE_TILE:
        case TEXTURETYPE_TILE_PRESWAPPED:
        {
            TileSettings settings = texTileSettings(tex, 0);

            settings.cms = wrapMode(cmd->cms);
            settings.cmt = wrapMode(cmd->cmt);

            gdl = loadTexture(gdl, tex, 0, texLoadDxt(tex));
            gdl = setTextureTile(gdl, tex, 0, 0, &settings, cmd->uvOffset);

            if (cmd->type == TEXTURETYPE_TILE)
            {
                gdl = setTextureTile(gdl, tex, 1, 0, &settings, cmd->uvOffset);
            }
            break;
        }
    }

    return gdl;
}

/**
 * Sets the number of levels of detail in the gSPTexture for the texture that's about to
 * be drawn. `lastTexture` is where the display list's last gSPTexture went in the output.
 * If no triangles have been drawn with it yet, it's changed in place. Otherwise a copy of
 * it with the new count is written (unless it already has that count), or a new one if
 * there isn't one.
 */
static Gfx *setTextureLods(Gfx *gdl, Gfx *lastTexture, const struct tex *tex, bool drawnSince)
{
    u32 lods = tex->maxlod ? tex->maxlod - 1 : 0;

    if (!drawnSince)
    {
        if (lastTexture != NULL)
        {
            lastTexture->words.w0 = (lastTexture->words.w0 & ~(7 << 11)) | (lods << 11);
        }
    }
    else if (lastTexture != NULL)
    {
        u32 w0 = (lastTexture->words.w0 & ~(7 << 11)) | (lods << 11);

        if (w0 != lastTexture->words.w0)
        {
            gdl->words.w0 = w0;
            gdl->words.w1 = lastTexture->words.w1;
            gdl++;
        }
    }
    else
    {
        gSPTexture(gdl++, 0xffff, 0xffff, lods, G_TX_RENDERTILE, G_ON);
    }

    return gdl;
}

/**
 * Copies a display list from `src` (`srcsize` bytes) to `dst`, loading each texture it
 * names into `pool` (NULL for the shared pool) and replacing its G_SETTEX with the
 * commands that draw it. Also notes where the light fixtures' commands are (see
 * lightfixture.c). Returns the size of the new display list, in bytes.
 */
s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, struct texpool *pool)
{
    Gfx *out = dst;
    Gfx *lastTexture = NULL;
    bool drawnSince = true; /* triangles drawn since the last gSPTexture or G_SETTEX */
    bool synced = false;    /* a pipe sync since the last triangles */
    bool inLight = false;   /* in a light fixture's commands */

    forgetTileSettings();

    if (pool == NULL)
    {
        pool = &g_TexSharedPool;
    }

    for (Gfx *in = src; in < src + (srcsize >> 3); in++)
    {
        // (The immediate commands' opcodes are negative)
        switch (in->words.w0 >> 24)
        {
            case G_SETTEX:
            {
                TexCmd cmd = readTexCmd(in);
                struct tex *tex;
                struct tex *detail = NULL;

                if (!synced)
                {
                    gDPPipeSync(out++);
                    synced = true;
                }

                if (inLight)
                {
                    lightFixtureEntryEnd(out);
                    inLight = false;
                }

                texLoadFromTextureNum(cmd.texnum, pool);
                tex = texFindInPool(cmd.texnum, pool);

                if (tex == NULL)
                {
                    break;
                }

                out = setTextureLods(out, lastTexture, tex, drawnSince);
                drawnSince = false;

                if (cmd.type == TEXTURETYPE_DETAIL)
                {
                    texLoadFromTextureNum(cmd.detailnum, pool);
                    detail = texFindInPool(cmd.detailnum, pool);
                }

                if (cmd.type != TEXTURETYPE_DETAIL || detail != NULL)
                {
                    out = expandTexCmd(out, &cmd, tex, detail);
                }

                // The water's texture is followed by the commands that animate it.
                // (Complex has Dam's water too, though it's too dark to see.)
                if (cmd.texnum == TEXTURE_WATER_CAVERNS)
                {
                    out = waterSetupGdl(out, TRUE);
                    forgetTileSettings();
                }

                if (cmd.texnum == TEXTURE_WATER_DAM)
                {
                    out = waterSetupGdlCi(out, TRUE);
                    forgetTileSettings();
                }

                if (lightIsLightTexture(cmd.texnum))
                {
                    lightFixtureEntryBegin(out);
                    inLight = true;
                }
                break;
            }

            case G_RDPPIPESYNC:
                synced = true;
                *out++ = *in;
                break;

            case (u8)G_TRI1:
            case (u8)G_TRI4:
                drawnSince = true;
                synced = false;
                *out++ = *in;
                break;

            case (u8)G_TEXTURE:
                lastTexture = out;
                drawnSince = false;
                *out++ = *in;
                break;

            default:
                *out++ = *in;
                break;
        }
    }

    if (inLight)
    {
        lightFixtureEntryEnd(out);
    }

    return (u8 *)out - (u8 *)dst;
}

/**
 * Copies `len` bytes of display list from `src` to `dst`, which may overlap it from
 * above: the last command is copied first.
 */
void texCopyGdls(Gfx *src, Gfx *dst, s32 len)
{
    for (s32 i = (len >> 3) - 1; i >= 0; i--)
    {
        dst[i].force_structure_alignment = src[i].force_structure_alignment;
    }
}

/* texSelect's render modes for each blend mode: without the z-buffer, with it, and decal.
   Each is a pair: the first-cycle and second-cycle versions of the mode. */
static const u32 s_RenderModes[4][3][2] = {
    [TEXBLEND_OPAQUE - 1] = {
        { G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2 },
        { G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2 },
        { G_RM_AA_ZB_OPA_DECAL, G_RM_AA_ZB_OPA_DECAL2 },
    },
    [TEXBLEND_TRANSLUCENT - 1] = {
        { G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2 },
        { G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2 },
        { G_RM_AA_ZB_XLU_DECAL, G_RM_AA_ZB_XLU_DECAL2 },
    },
    [TEXBLEND_CUTOUT - 1] = {
        { G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2 },
        { G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2 },
        { G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2 },
    },
    [TEXBLEND_CLOUD - 1] = {
        { G_RM_CLD_SURF, G_RM_CLD_SURF2 },
        { G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2 },
        { G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2 },
    },
};

/**
 * Sets the cycle type and render mode. In two-cycle mode the first cycle passes its
 * result on to the second.
 */
static Gfx *setRenderMode(Gfx *gdl, u32 blendMode, s32 numCycles, s32 zbufferMode)
{
    s32 blend = (blendMode >= TEXBLEND_TRANSLUCENT && blendMode <= TEXBLEND_CLOUD) ? blendMode - 1 : 0;
    s32 zbuffer = zbufferMode == TEXZBUF_OFF ? 0 : zbufferMode >= TEXZBUF_DECAL ? 2 : 1;
    const u32 *mode = s_RenderModes[blend][zbuffer];

    gDPPipeSync(gdl++);

    if (numCycles == 1)
    {
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        gDPSetRenderMode(gdl++, mode[0], mode[1]);
    }
    else
    {
        gDPSetCycleType(gdl++, G_CYC_2CYCLE);
        gDPSetRenderMode(gdl++, G_RM_PASS, mode[1]);
    }

    return gdl;
}

/** The texels a LoadBlock of `numLods` levels of a `width` x `height` image loads */
static s32 loadBlockTexels(s32 depth, s32 width, s32 height, s32 numLods)
{
    s32 texels = 0;

    for (s32 lod = 0; lod < (numLods > 0 ? numLods : 1); lod++)
    {
        texels += lineWords(depth, width) * 4 * height;

        if (width >= 2)
        {
            width >>= 1;
        }

        if (height >= 2)
        {
            height >>= 1;
        }
    }

    return texels;
}

/**
 * Writes the commands to draw with an image from an image table: its render mode (see
 * TexBlendMode and TexZbufferMode), loading it into TMEM and its tiles, offset by
 * `uvOffset` (10.2 fixed point: 2 is half a texel). An image whose `level` isn't 0 is
 * drawn with that many levels of detail, in two-cycle mode. A NULL image draws with the
 * vertex colours alone.
 *
 * The image table entry's `index` is a texture number until the texture is loaded, then
 * its physical address.
 */
void texSelect(Gfx **gdlptr, sImageTableEntry *image, u32 blendMode, s32 zbufferMode, u32 uvOffset)
{
    Gfx *gdl = *gdlptr;
    s32 xparam = zbufferMode >= TEXZBUF_DECAL ? zbufferMode : 0;
    struct tex *tex;
    s32 format;
    s32 depth;
    s32 lutMode = G_TT_NONE;
    bool mipmapped;
    s32 numLods;
    s32 width = image ? image->width : 0;
    s32 height = image ? image->height : 0;
    s32 lastTexel;
    s32 tmem = 0;

    if (image == NULL)
    {
        gdl = setRenderMode(gdl, blendMode, 1, zbufferMode);
        gSPTextureL(gdl++, 0xffff, 0xffff, 0, xparam, G_TX_RENDERTILE, G_ON);
        gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
        *gdlptr = gdl;
        return;
    }

    if (image->index < MAX_TEXTURES)
    {
        texLoad(&image->index, NULL);
    }

    tex = texFindByData((u8 *)PHYS_TO_K0(image->index), NULL);

    if (tex != NULL)
    {
        format = tex->gbiformat;
        depth = tex->depth;
        lutMode = tex->lutmodeindex << G_MDSFT_TEXTLUT;
    }
    else
    {
        format = image->format;
        depth = image->depth;
    }

    mipmapped = image->level != 0;
    numLods = mipmapped ? image->level : 1;

    if (mipmapped && tex != NULL && tex->hasExplicitLods)
    {
        lastTexel = texLoadTexels(tex) - 1;
    }
    else
    {
        lastTexel = loadBlockTexels(depth, width, height, numLods) - 1;
    }

    gdl = setRenderMode(gdl, blendMode, mipmapped ? 2 : 1, zbufferMode);
    gSPTextureL(gdl++, 0xffff, 0xffff, numLods - 1, xparam, G_TX_RENDERTILE, G_ON);
    gDPSetTextureLOD(gdl++, mipmapped ? G_TL_LOD : G_TL_TILE);

    if (format == G_IM_FMT_CI)
    {
        gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    }
    else if (!mipmapped)
    {
        if (format == G_IM_FMT_I)
        {
            gDPSetCombineMode(gdl++, G_CC_MODULATEI, G_CC_MODULATEI);
        }
        else
        {
            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
        }
    }
    else
    {
        if (format == G_IM_FMT_I)
        {
            gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEI2);
        }
        else
        {
            gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
        }
    }

    gDPSetTextureImage(gdl++, format, loadDepth(depth), 1, image->index);

    if (mipmapped)
    {
        gDPSetTile(gdl++, format, loadDepth(depth), 0, 0, G_TX_LOADTILE, 0,
                   G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
        gDPLoadSync(gdl++);
        gDPLoadBlock(gdl++, G_TX_LOADTILE, 0, 0, lastTexel, 0);
    }
    else
    {
        gDPSetTile(gdl++, format, loadDepth(depth), 0, 0, G_TX_LOADTILE, 0,
                   image->flagsT, G_TX_NOMASK, G_TX_NOLOD, image->flagsS, G_TX_NOMASK, G_TX_NOLOD);
        gDPLoadSync(gdl++);
        gDPLoadBlock(gdl++, G_TX_LOADTILE, 0, 0, lastTexel, loadBlockDxt(depth, lineWords(depth, width)));
    }

    gDPPipeSync(gdl++);

    if (format == G_IM_FMT_CI)
    {
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, TMEM_PALETTE, G_TX_LOADTILE, 0,
                   G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
        gdl = loadPalette(gdl, tex, lastTexel + 1);
        gDPSetTextureLUT(gdl++, lutMode);
    }
    else
    {
        gDPSetTextureLUT(gdl++, G_TT_NONE);
    }

    for (s32 tile = 0; tile < numLods; tile++)
    {
        s32 line;

        if (tile > 0)
        {
            if (tex != NULL && tex->hasExplicitLods)
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

        line = lineWords(depth, width);

        gDPSetTile(gdl++, format, depth, line, tmem, tile, 0,
                   image->flagsT, maskBits(height), tile, image->flagsS, maskBits(width), tile);
        gDPSetTileSize(gdl++, tile, uvOffset, uvOffset, ((width - 1) << 2) + uvOffset, ((height - 1) << 2) + uvOffset);

        tmem += line * height;
    }

    *gdlptr = gdl;
}
