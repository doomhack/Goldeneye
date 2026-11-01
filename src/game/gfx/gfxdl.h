#ifndef _GFXDL_H_
#define _GFXDL_H_
#include <ultra64.h>
#include <gbi_extension.h>

/*
 * Reading display lists on the CPU.
 *
 * Collision, bullet hits and the light fixtures read the triangles in room and model
 * display lists. Everything that depends on how the graphics microcode encodes vertex
 * loads and triangles is here, so a change of microcode only changes this file.
 *
 * Triangles are numbered within their command as in HitThing.triNum: a command holds
 * gfxTriCount() of them, numbered gfxFirstTri() up. Fast3D's G_TRI1 has triangle 0;
 * GE's G_TRI4 has triangles 1 to 4 (one with all three indices 0 is not drawn).
 */

static inline u32 gfxOp(const Gfx *cmd)
{
    return cmd->words.w0 >> 24;
}

/** The end of a display list (gSPEndDisplayList) */
static inline bool gfxIsEnd(const Gfx *cmd)
{
    return gfxOp(cmd) == (u8)G_ENDDL;
}

/** A vertex load (gSPVertex) */
static inline bool gfxIsVtx(const Gfx *cmd)
{
    return gfxOp(cmd) == (u8)G_VTX;
}

/** A vertex load's first vertex buffer index */
static inline s32 gfxVtxFirst(const Gfx *cmd)
{
    return (cmd->words.w0 >> 16) & 0xf;
}

/** The number of vertices a vertex load loads */
static inline s32 gfxVtxCount(const Gfx *cmd)
{
    return ((cmd->words.w0 >> 20) & 0xf) + 1;
}

/** A vertex load's address (segmented or physical) */
static inline u32 gfxVtxAddr(const Gfx *cmd)
{
    return cmd->words.w1;
}

/** A command that draws triangles */
static inline bool gfxIsTris(const Gfx *cmd)
{
    u32 op = gfxOp(cmd);

    return (op == (u8)G_TRI1) || (op == (u8)G_TRI4);
}

/** The number of triangles in a triangle command (0 for any other command) */
static inline s32 gfxTriCount(const Gfx *cmd)
{
    u32 op = gfxOp(cmd);

    return (op == (u8)G_TRI1) ? 1 : (op == (u8)G_TRI4) ? 4 : 0;
}

/** The number of a triangle command's first triangle */
static inline s32 gfxFirstTri(const Gfx *cmd)
{
    return (gfxOp(cmd) == (u8)G_TRI4) ? 1 : 0;
}

/** Gets triangle `tri` of a triangle command as vertex buffer indices */
static inline void gfxGetTri(const Gfx *cmd, s32 tri, s32 idx[3])
{
    u32 w0 = cmd->words.w0;
    u32 w1 = cmd->words.w1;

    if (gfxOp(cmd) == (u8)G_TRI1)
    {
        idx[0] = ((w1 >> 16) & 0xff) / 10;
        idx[1] = ((w1 >> 8) & 0xff) / 10;
        idx[2] = (w1 & 0xff) / 10;
    }
    else
    {
        s32 t = tri - 1;

        idx[0] = (w1 >> (8 * t)) & 0xf;
        idx[1] = (w1 >> (8 * t + 4)) & 0xf;
        idx[2] = (w0 >> (4 * t)) & 0xf;
    }
}

/**
 * The texture a command draws with, from the last G_SETTIMG before it (but after `start`,
 * the start of its display list), or -1. The image address is 8 bytes past the texture's
 * header, whose first halfword is the texture's number.
 */
static inline s32 gfxFindTextureNum(const Gfx *cmd, const Gfx *start)
{
    const Gfx *settimg = cmd;

    if ((gfxOp(cmd) != G_SETTIMG) && (start < cmd))
    {
        do
        {
            settimg--;
        }
        while ((gfxOp(settimg) != G_SETTIMG) && (start < settimg));
    }

    if (settimg == start)
    {
        return -1;
    }

    return *(u16 *)((settimg->words.w1 - 8) | 0x80000000);
}

#endif
