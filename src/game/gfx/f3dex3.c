/*
 * F3DEX3 spike (throwaway): translates a frame's Fast3D display lists into
 * F3DEX3 ones at the end of the frame, following every G_DL, so the same scene
 * can be drawn and timed by either microcode. Only for measuring: the real port
 * converts the asset display lists at build time.
 *
 * Encodings are from GE's include/PR/gbi.h (Fast3D, plus Rare's G_TRI4) and
 * F3DEX3's gbi.h (HackerN64/F3DEX3 91a8528).
 */
#include <ultra64.h>
#include "game/gfx/f3dex3.h"
#include "memp.h"

/* Fast3D opcodes (top byte) */
#define F3D_SPNOOP        0x00
#define F3D_MTX           0x01
#define F3D_MOVEMEM       0x03
#define F3D_VTX           0x04
#define F3D_DL            0x06
#define F3D_TRI4          0xB1
#define F3D_RDPHALF_CONT  0xB2
#define F3D_RDPHALF_2     0xB3
#define F3D_RDPHALF_1     0xB4
#define F3D_LINE3D        0xB5
#define F3D_CLEARGEOM     0xB6
#define F3D_SETGEOM       0xB7
#define F3D_ENDDL         0xB8
#define F3D_SETOTHERMODE_L 0xB9
#define F3D_SETOTHERMODE_H 0xBA
#define F3D_TEXTURE       0xBB
#define F3D_MOVEWORD      0xBC
#define F3D_POPMTX        0xBD
#define F3D_CULLDL        0xBE
#define F3D_TRI1          0xBF

/* Fast3D movemem indices */
#define F3D_MV_VIEWPORT 0x80
#define F3D_MV_LOOKATY  0x82
#define F3D_MV_LOOKATX  0x84
#define F3D_MV_L0       0x86
#define F3D_MV_L7       0x94

/* Fast3D moveword indices */
#define F3D_MW_MATRIX    0x00
#define F3D_MW_NUMLIGHT  0x02
#define F3D_MW_CLIP      0x04
#define F3D_MW_SEGMENT   0x06
#define F3D_MW_FOG       0x08
#define F3D_MW_LIGHTCOL  0x0A
#define F3D_MW_POINTS    0x0C
#define F3D_MW_PERSPNORM 0x0E

/* F3DEX3 opcodes */
#define X3_NOOP         0x00
#define X3_VTX          0x01
#define X3_CULLDL       0x03
#define X3_TRI1         0x05
#define X3_TRI2         0x06
#define X3_TEXTURE      0xD7
#define X3_POPMTX       0xD8
#define X3_GEOMETRYMODE 0xD9
#define X3_MTX          0xDA
#define X3_MOVEWORD     0xDB
#define X3_MOVEMEM      0xDC
#define X3_DL           0xDE
#define X3_ENDDL        0xDF
#define X3_SPNOOP       0xE0
#define X3_RDPHALF_1    0xE1
#define X3_SETOTHERMODE_L 0xE2
#define X3_SETOTHERMODE_H 0xE3
#define X3_RDPHALF_2    0xF1

#define X3_MV_VIEWPORT  8
#define X3_MV_LIGHT     10
#define X3_MW_FX        0x00
#define X3_MW_NUMLIGHT  0x02
#define X3_MW_SEGMENT   0x06
#define X3_MW_FOG       0x08
#define X3_MW_LIGHTCOL  0x0A
#define X3_MWO_PERSPNORM 0x06
#define X3_MW_HALFWORD_FLAG 0x8000
#define X3_NEW_MAXZ     0x7FFF

#define SH(v, s, w) (((u32)(v) & ((1u << (w)) - 1)) << (s))

/*
 * Per frame output, double buffered (the RSP reads one while the CPU fills the
 * other). Each call depth writes into its own arena, so every translated list is
 * contiguous and a call stays a plain G_DL, as in the Fast3D frame.
 */
#define XL_MAX_DEPTH 12
#define XL_DATA_SIZE (32 * 1024)
static const s32 s_ArenaSizes[XL_MAX_DEPTH] = {
    64 * 1024, 192 * 1024, 128 * 1024, 64 * 1024, 32 * 1024, 32 * 1024,
    16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024,
};
#define XL_BUF_SIZE ((64 + 192 + 128 + 64 + 32 + 32 + 16 * 6) * 1024 + XL_DATA_SIZE)

volatile s32 g_UcodeMode = UCODE_FAST3D;
s32 g_PerfSkip;
volatile s32 g_XlDebug;
volatile u32 g_XlSrcFirst; /* spike debugging: last frame's Fast3D list */ /* spike debugging: 1 = TRI4 as TRI1s, 2 = clear G_TL_LOD, 4 = perspnorm 0xFFFF at start, 8 = no lights */
s32 g_F3dex3Unsupported;
volatile s32 g_F3dex3LastOp;
volatile s32 g_F3dex3SkippedRaw; /* raw RDP commands dropped (sky) */
volatile F3DEX3YieldDataFooter g_F3dex3Counters;
volatile u32 g_F3dex3CounterSum[4];
volatile u32 g_F3dex3CounterTasks;

static u8 *s_Buffers[2];
static s32 s_BufIndex;

static Gfx *s_ArenaPos[XL_MAX_DEPTH];
static Gfx *s_ArenaEnd[XL_MAX_DEPTH];
static s32 s_Depth;
static u8 *s_DataPos;
static u8 *s_DataEnd;
static u32 s_Segments[16];
static s32 s_Overflow;

const char *f3dex3ModeName(s32 mode)
{
    switch (mode)
    {
        case UCODE_F3DEX3_NOC: return "F3DEX3 NOC";
        case UCODE_F3DEX3_NOC_PA: return "F3DEX3 NOC PA";
        case UCODE_F3DEX3_NOC_PB: return "F3DEX3 NOC PB";
        default: return "FAST3D";
    }
}

void f3dex3AllocateBuffers(void)
{
    s_Buffers[0] = mempAlloc(XL_BUF_SIZE * 2, MEMPOOL_PERMANENT);
    s_Buffers[1] = s_Buffers[0] + XL_BUF_SIZE;
}

/** A Fast3D address (segmented or physical) as a CPU pointer */
static void *resolve(u32 addr)
{
    u32 seg = (addr >> 24) & 0xF;
    u32 phys = (s_Segments[seg] + (addr & 0x00FFFFFF)) & 0x1FFFFFFF;
    return (void *)PHYS_TO_K0(phys);
}

static void *allocData(s32 size)
{
    u8 *p = s_DataPos;

    s_DataPos += (size + 0xF) & ~0xF;

    if (s_DataPos > s_DataEnd)
    {
        s_Overflow = 1;
        s_DataPos = p;
    }

    return p;
}

static void emit(u32 w0, u32 w1)
{
    Gfx *out = s_ArenaPos[s_Depth];

    if (out >= s_ArenaEnd[s_Depth])
    {
        s_Overflow = 1;
        return;
    }

    out->words.w0 = w0;
    out->words.w1 = w1;
    s_ArenaPos[s_Depth] = out + 1;
}

static void unsupported(u32 w0)
{
    g_F3dex3Unsupported++;
    g_F3dex3LastOp = w0;
}

static u32 geomBits(u32 f3d)
{
    u32 x = f3d & (0x1 | 0x4 | 0x10000 | 0x20000 | 0x40000 | 0x80000 | 0x100000);

    if (f3d & 0x200) x |= 0x00200000;  /* G_SHADING_SMOOTH */
    if (f3d & 0x1000) x |= 0x00000200; /* G_CULL_FRONT */
    if (f3d & 0x2000) x |= 0x00000400; /* G_CULL_BACK */

    return x;
}

/* Tri vertex index bytes: Fast3D uses index*10, F3DEX3 index*2 */
static u32 tri1Word(s32 a, s32 b, s32 c)
{
    return SH(a * 2, 16, 8) | SH(b * 2, 8, 8) | SH(c * 2, 0, 8);
}

static Gfx *translateList(Gfx *src, s32 depth);

/** Emits a Fast3D G_DL (call or branch); returns true when it was a branch */
static bool translateCall(u32 w0, u32 w1, s32 depth)
{
    bool branch = ((w0 >> 16) & 0xFF) != 0; /* G_DL_NOPUSH */
    Gfx *target = resolve(w1);
    Gfx *start;

    if (depth + 1 >= XL_MAX_DEPTH)
    {
        unsupported(w0);
        return branch;
    }

    s_Depth = depth + 1;
    start = s_ArenaPos[s_Depth];
    translateList(target, depth + 1);
    s_Depth = depth;

    emit(SH(X3_DL, 24, 8) | SH(branch ? 1 : 0, 16, 8), OS_K0_TO_PHYSICAL(start));

    return branch;
}

static void translateTri4(u32 w0, u32 w1)
{
    s32 tris[4][3];
    s32 n = 0;
    s32 i;

    for (i = 0; i < 4; i++)
    {
        s32 x = (w1 >> (i * 8)) & 0xF;
        s32 y = (w1 >> (i * 8 + 4)) & 0xF;
        s32 z = (w0 >> (i * 4)) & 0xF;

        if (x | y | z)
        {
            tris[n][0] = x;
            tris[n][1] = y;
            tris[n][2] = z;
            n++;
        }
    }

    if (g_XlDebug & 1)
    {
        for (i = 0; i < n; i++)
        {
            emit(SH(X3_TRI1, 24, 8) | tri1Word(tris[i][0], tris[i][1], tris[i][2]), 0);
        }
        return;
    }

    for (i = 0; i + 1 < n; i += 2)
    {
        emit(SH(X3_TRI2, 24, 8) | tri1Word(tris[i][0], tris[i][1], tris[i][2]),
             tri1Word(tris[i + 1][0], tris[i + 1][1], tris[i + 1][2]));
    }

    if (i < n)
    {
        emit(SH(X3_TRI1, 24, 8) | tri1Word(tris[i][0], tris[i][1], tris[i][2]), 0);
    }
}

static void translateMovemem(u32 w0, u32 w1)
{
    u32 index = (w0 >> 16) & 0xFF;

    if (index == F3D_MV_VIEWPORT)
    {
        Vp *src = resolve(w1);
        Vp *dst = allocData(sizeof(Vp));

        *dst = *src;
        dst->vp.vscale[1] = -src->vp.vscale[1];
        dst->vp.vscale[2] = (s32)src->vp.vscale[2] * X3_NEW_MAXZ / G_MAXZ;
        dst->vp.vtrans[2] = (s32)src->vp.vtrans[2] * X3_NEW_MAXZ / G_MAXZ;

        emit(SH(X3_MOVEMEM, 24, 8) | SH((sizeof(Vp) - 1) / 8, 19, 5) | SH(0, 8, 8) | X3_MV_VIEWPORT,
             OS_K0_TO_PHYSICAL(dst));
    }
    else if (index == F3D_MV_LOOKATX)
    {
        /* gSPLookAt sends X then Y from consecutive 16 byte lights; F3DEX3 takes both dirs at once */
        u8 *src = resolve(w1);
        u8 *dst = allocData(8);
        s32 i;

        for (i = 0; i < 4; i++)
        {
            dst[i] = src[8 + i];
            dst[4 + i] = src[16 + 8 + i];
        }

        dst[3] = 0;
        dst[7] = 0;

        emit(SH(X3_MOVEMEM, 24, 8) | SH((8 - 1) / 8, 19, 5) | SH(8 / 8, 8, 8) | X3_MV_LIGHT,
             OS_K0_TO_PHYSICAL(dst));
    }
    else if (index == F3D_MV_LOOKATY)
    {
        /* sent with LOOKATX */
    }
    else if ((index >= F3D_MV_L0) && (index <= F3D_MV_L7))
    {
        s32 n = ((index - F3D_MV_L0) / 2) + 1;
        u8 *src = resolve(w1);
        u8 *dst = allocData(16);
        s32 i;

        for (i = 0; i < 16; i++)
        {
            dst[i] = src[i];
        }

        dst[3] = 0;  /* type: directional */
        dst[15] = 0; /* size */

        emit(SH(X3_MOVEMEM, 24, 8) | SH((16 - 1) / 8, 19, 5) | SH(((n - 1) * 0x10 + 0x10) / 8, 8, 8) | X3_MV_LIGHT,
             OS_K0_TO_PHYSICAL(dst));
    }
    else
    {
        unsupported(w0);
    }
}

static void translateMoveword(u32 w0, u32 w1)
{
    u32 index = w0 & 0xFF;
    u32 offset = (w0 >> 8) & 0xFFFF;

    switch (index)
    {
        case F3D_MW_SEGMENT:
            s_Segments[(offset / 4) & 0xF] = w1 & 0x1FFFFFFF;
            emit(SH(X3_MOVEWORD, 24, 8) | SH(X3_MW_SEGMENT, 16, 8) | (offset & 0xFFF), w1);
            break;
        case F3D_MW_FOG:
            if (g_XlDebug & 128) break;
            emit(SH(X3_MOVEWORD, 24, 8) | SH(X3_MW_FOG, 16, 8) | 0, w1);
            break;
        case F3D_MW_NUMLIGHT:
        {
            /* Fast3D NUML(n) = 0x80000000 + (n + 1) * 32; F3DEX3 NUML(n) = n * 16 */
            s32 n = (s32)((w1 & 0x7FFFFFFF) / 32) - 1;
            if (n < 0) n = 0;
            emit(SH(X3_MOVEWORD, 24, 8) | SH(X3_MW_NUMLIGHT, 16, 8) | 0, n * 0x10);
            break;
        }
        case F3D_MW_PERSPNORM:
            if (g_XlDebug & 32) break;
            emit(SH(X3_MOVEWORD, 24, 8) | SH(X3_MW_FX, 16, 8) | X3_MWO_PERSPNORM | X3_MW_HALFWORD_FLAG, w1);
            break;
        case F3D_MW_CLIP:
            /* clip ratio is fixed in F3DEX3 */
            break;
        default:
            unsupported(w0);
            break;
    }
}

static Gfx *translateList(Gfx *src, s32 depth)
{
    while (!s_Overflow)
    {
        u32 w0 = src->words.w0;
        u32 w1 = src->words.w1;
        u32 op = w0 >> 24;

        src++;

        switch (op)
        {
            case F3D_SPNOOP:
                break;
            case F3D_MTX:
            {
                /* Fast3D: projection 1, load 2, push 4; F3DEX3: projection 4, load 2, push 1, sent as p ^ push ^ load */
                u32 p = (w0 >> 16) & 0xFF;
                u32 x = ((p & 1) ? 0x04 : 0) | (p & 2) | ((p & 4) ? 0x01 : 0);
                emit(SH(X3_MTX, 24, 8) | SH((64 - 1) / 8, 19, 5) | SH(x ^ 0x01 ^ 0x02, 0, 8), w1);
                break;
            }
            case F3D_MOVEMEM:
                translateMovemem(w0, w1);
                break;
            case F3D_VTX:
            {
                u32 n = ((w0 >> 20) & 0xF) + 1;
                u32 v0 = (w0 >> 16) & 0xF;
                emit(SH(X3_VTX, 24, 8) | SH(n, 12, 8) | SH(v0 + n, 1, 7), w1);
                break;
            }
            case F3D_DL:
                if (((w0 >> 16) & 0xFF) != 0)
                {
                    /* A branch: carry on translating the target inline, at the same depth */
                    src = resolve(w1);
                    break;
                }
                if (translateCall(w0, w1, depth))
                {
                    return src;
                }
                break;
            case F3D_TRI4:
                translateTri4(w0, w1);
                break;
            case F3D_TRI1:
            {
                s32 flag = (w1 >> 24) & 0xFF;
                s32 a = ((w1 >> 16) & 0xFF) / 10;
                s32 b = ((w1 >> 8) & 0xFF) / 10;
                s32 c = (w1 & 0xFF) / 10;
                u32 tri = (flag == 1) ? tri1Word(b, c, a) : (flag == 2) ? tri1Word(c, a, b) : tri1Word(a, b, c);
                emit(SH(X3_TRI1, 24, 8) | tri, 0);
                break;
            }
            case F3D_RDPHALF_1:
                if ((src->words.w0 >> 24) == F3D_RDPHALF_CONT)
                {
                    /* A raw RDP command streamed through RDPHALF_1/CONT/2 (the sky's
                       triangles). F3DEX3 has no passthrough for these: drop it. */
                    while (((src->words.w0 >> 24) != F3D_RDPHALF_2) && ((src->words.w0 >> 24) != F3D_ENDDL))
                    {
                        src++;
                    }
                    if ((src->words.w0 >> 24) == F3D_RDPHALF_2) src++;
                    g_F3dex3SkippedRaw++;
                    break;
                }
                emit(SH(X3_RDPHALF_1, 24, 8) | (w0 & 0xFFFFFF), w1);
                break;
            case F3D_RDPHALF_2:
                emit(SH(X3_RDPHALF_2, 24, 8) | (w0 & 0xFFFFFF), w1);
                break;
            case F3D_SETGEOM:
                if (g_XlDebug & 64) w1 &= ~0x60000;
                emit(SH(X3_GEOMETRYMODE, 24, 8) | 0xFFFFFF, geomBits(w1));
                break;
            case F3D_CLEARGEOM:
                emit(SH(X3_GEOMETRYMODE, 24, 8) | (~geomBits(w1) & 0xFFFFFF), 0);
                break;
            case F3D_ENDDL:
                emit(SH(X3_ENDDL, 24, 8), 0);
                return src;
            case F3D_SETOTHERMODE_L:
            case F3D_SETOTHERMODE_H:
            {
                u32 sft = (w0 >> 8) & 0xFF;
                u32 len = w0 & 0xFF;
                if ((g_XlDebug & 2) && (op == F3D_SETOTHERMODE_H) && (sft == G_MDSFT_TEXTLOD)) w1 = 0;
                u32 cmd = (op == F3D_SETOTHERMODE_L) ? X3_SETOTHERMODE_L : X3_SETOTHERMODE_H;
                emit(SH(cmd, 24, 8) | SH(32 - sft - len, 8, 8) | SH(len - 1, 0, 8), w1);
                break;
            }
            case F3D_TEXTURE:
            {
                u32 level = (w0 >> 11) & 7;
                u32 tile = (w0 >> 8) & 7;
                u32 on = w0 & 0xFF;
                emit(SH(X3_TEXTURE, 24, 8) | SH(level, 11, 3) | SH(tile, 8, 3) | SH(on ? 1 : 0, 1, 7), w1);
                break;
            }
            case F3D_MOVEWORD:
                translateMoveword(w0, w1);
                break;
            case F3D_POPMTX:
                emit(SH(X3_POPMTX, 24, 8) | SH((64 - 1) / 8, 19, 5), 64);
                break;
            case F3D_CULLDL:
            {
                u32 vstart = (w0 & 0xFFFF) / 40;
                u32 vend = (w1 / 40) - 1;
                emit(SH(X3_CULLDL, 24, 8) | SH(vstart * 2, 0, 16), SH(vend * 2, 0, 16));
                break;
            }
            default:
                if ((g_XlDebug & 8) && (op == 0xFD))
                {
                    emit(SH(X3_MOVEWORD, 24, 8) | SH(X3_MW_FX, 16, 8) | 0x16, 0xFFFFFFFF);
                }

                if (op >= 0xC0)
                {
                    /* RDP commands are the same for both microcodes */
                    emit(w0, w1);
                }
                else
                {
                    unsupported(w0);
                }
                break;
        }
    }

    return src;
}

Gfx *f3dex3TranslateFrame(Gfx *first, Gfx *end, Gfx **outEnd)
{
    Gfx *start;
    u8 *p;
    s32 i;

    (void)end;
    g_XlSrcFirst = (u32)(uintptr_t)first;

    s_BufIndex ^= 1;
    p = s_Buffers[s_BufIndex];

    for (i = 0; i < XL_MAX_DEPTH; i++)
    {
        s_ArenaPos[i] = (Gfx *)p;
        p += s_ArenaSizes[i];
        s_ArenaEnd[i] = (Gfx *)p;
    }

    s_DataPos = p;
    s_DataEnd = p + XL_DATA_SIZE;
    s_Depth = 0;
    s_Overflow = 0;
    g_F3dex3Unsupported = 0;
    start = s_ArenaPos[0];

    for (i = 0; i < 16; i++)
    {
        s_Segments[i] = 0;
    }

    /* Fast3D starts each task with no geometry mode bits; make sure none of F3DEX3's
       own (Fresnel, packed normals, ...) are left on */
    emit(SH(X3_GEOMETRYMODE, 24, 8), 0);

    if (g_XlDebug & 4)
    {
        emit(SH(X3_MOVEWORD, 24, 8) | SH(X3_MW_FX, 16, 8) | X3_MWO_PERSPNORM | X3_MW_HALFWORD_FLAG, 0xFFFF);
    }

    translateList(first, 0);

    if (s_Overflow)
    {
        /* Too big: draw nothing this frame rather than a broken list */
        s_ArenaPos[0] = start;
        s_Depth = 0;
        s_Overflow = 0;
        emit(SH(0xE9, 24, 8), 0); /* gDPFullSync */
        emit(SH(X3_ENDDL, 24, 8), 0);
        g_F3dex3Unsupported = -1;
    }

    *outEnd = s_ArenaPos[0];
    return start;
}
