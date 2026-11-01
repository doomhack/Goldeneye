#ifndef _VIDEO_H_
#define _VIDEO_H_
#include <ultra64.h>

#define NUM_VIDEO_FRAME_BUFFERS    2
#define NUM_VIDEO_SETTINGS         NUM_VIDEO_FRAME_BUFFERS

#define WIDESCREEN_ASPECT ((f32)(16.0f/9.0f))


#define	SCREEN_HEIGHT_MIN   240
#define	SCREEN_HEIGHT_240   240
#define	SCREEN_HEIGHT_272   272
#define	SCREEN_HEIGHT_330   330
#define	SCREEN_HEIGHT_MAX   480
#define	SCREEN_HEIGHT_MAX_EU   572

#define SCREEN_WIDTH_MIN    320
#define SCREEN_WIDTH_320    320
#define SCREEN_WIDTH_440    440
#define SCREEN_WIDTH_MAX    640

#define VIEWPORT_WIDTH_FULLSCREEN SCREEN_WIDTH_320
#define VIEWPORT_WIDTH_WIDESCREEN SCREEN_WIDTH_320
#define VIEWPORT_WIDTH_CINEMA     SCREEN_WIDTH_320

#define SCREEN_HALF_WIDTH (SCREEN_WIDTH_320 / 2)

/* 304 = 320 - 16 ?? */
#define VIEWPORT_HEIGHT_FULLSCREEN 304
#define VIEWPORT_HEIGHT_WIDESCREEN 248
#define VIEWPORT_HEIGHT_CINEMA     190

#define VIEWPORT_HEIGHT_DEFAULT_NTSC    220
#define VIEWPORT_HEIGHT_DEFAULT_PAL     272

#define VIEWPORT_OFFSET_HEIGHT_WIDESCREEN_NTSC 180
#define VIEWPORT_OFFSET_HEIGHT_WIDESCREEN_PAL  216

#define VIEWPORT_OFFSET_HEIGHT_CINEMA_NTSC     136
#define VIEWPORT_OFFSET_HEIGHT_CINEMA_PAL      162

/* 159 = (640 / 4) - 1 */
#define VIEWPORT_WIDTH_4P 159
/* 109 = (440 / 4) - 1 */
#define VIEWPORT_HEIGHT_4P_109 109
/* PAL */
#define VIEWPORT_HEIGHT_4P_135 135

#define WIDESCREEN_SCALE_FACTOR_NTSC 40.0f
#define WIDESCREEN_SCALE_FACTOR_PAL  56.0f

#define CINEMA_SCALE_FACTOR_NTSC  84.0f
#define CINEMA_SCALE_FACTOR_PAL  110.0f

#define WIDESCREEN_ULY_SCALE_FACTOR_NTSC -20.0f
#define WIDESCREEN_ULY_SCALE_FACTOR_PAL  -29.0f

#define CINEMA_ULY_SCALE_FACTOR_NTSC -42.0f
#define CINEMA_ULY_SCALE_FACTOR_PAL  -52.0f


#define VIEWPORT_ULY_2P_PLAYER_1       10

#define VIEWPORT_ULY_2P_PLAYER_2_NTSC       121
#define VIEWPORT_ULY_2P_PLAYER_2_PAL        137

#define VIEWPORT_ULY_4P_PLAYER_12      10

#define VIEWPORT_ULY_4P_PLAYER_34_NTSC      121
#define VIEWPORT_ULY_4P_PLAYER_34_PAL       137

#define VIEWPORT_ULY_CAM_FULLSCREEN    13
#define VIEWPORT_ULY_CAM_WIDESCREEN    41
#define VIEWPORT_ULY_CAM_CINEMA        70

#define VIEWPORT_ULY_WIDESCREEN_OFFSET_NTSC 30
#define VIEWPORT_ULY_WIDESCREEN_OFFSET_PAL  29

#define VIEWPORT_ULY_CINEMA_OFFSET     52
#define VIEWPORT_ULY_DEFAULT           10

#define Z_BUFFER_HEIGHT_NTSC 120
#define Z_BUFFER_HEIGHT_PAL  136

#define Z_BUFFER_4_3_HEIGHT 330
#define Z_BUFFER_4_3_WIDTH  440


// 4:3
#define ASPECT_RATIO_SD    (1.3333334f)
#define ASPECT_RATIO_PAL    (1.1764706f)

#define ASPECT_RATIO_EU_400_330    (1.5873016119f)
#define ASPECT_RATIO_EU_320_272    (1.40056025982f)

#if defined(VERSION_EU)
/* PAL */
#define	SCREEN_HEIGHT                      SCREEN_HEIGHT_272
#define	VIEWPORT_HEIGHT_4P                 VIEWPORT_HEIGHT_4P_135
#define WIDESCREEN_SCALE_FACTOR            WIDESCREEN_SCALE_FACTOR_PAL
#define CINEMA_SCALE_FACTOR                CINEMA_SCALE_FACTOR_PAL
#define VIEWPORT_OFFSET_HEIGHT_WIDESCREEN  VIEWPORT_OFFSET_HEIGHT_WIDESCREEN_PAL
#define VIEWPORT_HEIGHT_DEFAULT            VIEWPORT_HEIGHT_DEFAULT_PAL
#define VIEWPORT_OFFSET_HEIGHT_CINEMA      VIEWPORT_OFFSET_HEIGHT_CINEMA_PAL
#define VIEWPORT_ULY_2P_PLAYER_2           VIEWPORT_ULY_2P_PLAYER_2_PAL
#define WIDESCREEN_ULY_SCALE_FACTOR        WIDESCREEN_ULY_SCALE_FACTOR_PAL
#define CINEMA_ULY_SCALE_FACTOR            CINEMA_ULY_SCALE_FACTOR_PAL
#define VIEWPORT_ULY_4P_PLAYER_34          VIEWPORT_ULY_4P_PLAYER_34_PAL
#define VIEWPORT_ULY_WIDESCREEN_OFFSET     VIEWPORT_ULY_WIDESCREEN_OFFSET_PAL
#define Z_BUFFER_HEIGHT                    Z_BUFFER_HEIGHT_PAL
#define ASPECT_RATIO                       ASPECT_RATIO_PAL
#else
/* NTSC */
#define	SCREEN_HEIGHT                      SCREEN_HEIGHT_240
#define	VIEWPORT_HEIGHT_4P                 VIEWPORT_HEIGHT_4P_109
#define WIDESCREEN_SCALE_FACTOR            WIDESCREEN_SCALE_FACTOR_NTSC
#define CINEMA_SCALE_FACTOR                CINEMA_SCALE_FACTOR_NTSC
#define VIEWPORT_OFFSET_HEIGHT_WIDESCREEN  VIEWPORT_OFFSET_HEIGHT_WIDESCREEN_NTSC
#define VIEWPORT_HEIGHT_DEFAULT            VIEWPORT_HEIGHT_DEFAULT_NTSC
#define VIEWPORT_OFFSET_HEIGHT_CINEMA      VIEWPORT_OFFSET_HEIGHT_CINEMA_NTSC
#define VIEWPORT_ULY_2P_PLAYER_2           VIEWPORT_ULY_2P_PLAYER_2_NTSC
#define WIDESCREEN_ULY_SCALE_FACTOR        WIDESCREEN_ULY_SCALE_FACTOR_NTSC
#define CINEMA_ULY_SCALE_FACTOR            CINEMA_ULY_SCALE_FACTOR_NTSC
#define VIEWPORT_ULY_4P_PLAYER_34          VIEWPORT_ULY_4P_PLAYER_34_NTSC
#define VIEWPORT_ULY_WIDESCREEN_OFFSET     VIEWPORT_ULY_WIDESCREEN_OFFSET_NTSC
#define Z_BUFFER_HEIGHT                    Z_BUFFER_HEIGHT_NTSC
#define ASPECT_RATIO                       ASPECT_RATIO_SD
#endif


#define SCREEN_WIDTH      SCREEN_WIDTH_320

#define XSCALE_MAX      0x400
#define YSCALE_MAX      0x800

#define FOV_Y_F         60.0f

typedef struct VideoSettings_s {
    /**
     * enum VIDEOMODE.
     */
    u8 mode;
    s8 field_01;
    s8 field_02;
    s8 field_03;
    s16 x;
    s16 y;
    f32 fovy;
    f32 aspect;
    f32 znear;
    f32 zfar;

    /**
     * Screen width.
     */
    s16 bufx;

    /**
     * Screen height.
     */
    s16 bufy;
    s16 viewx;
    s16 viewy;
    s16 viewleft;
    s16 viewtop;
    s32 usezbuf;
    u8* framebuf;

    /**
     * Logical to physical pixel scale, as a fraction per axis (mul / div).
     * Everything above the RDP works in logical coordinates (320 wide in game, 440x330 in the
     * front end); only the framebuffer, Z-buffer, viewport and 2D draw calls are scaled.
     * In game: x is 1/1 or 2/1 (hi-res option), y is 1/1.
     * Front end: 440x330 logical drawn into a 640x240 (NTSC) or 640x286 (PAL) framebuffer.
     */
    s16 sxMul;
    s16 sxDiv;
    s16 syMul;
    s16 syDiv;
} VideoSettings;

extern VideoSettings *g_ViBackData;

/* Logical x / y (or width / height) to physical framebuffer pixels for the frame being built.
 * Also works on 10.2 fixed point coordinates (texture rectangles). */
#define VI_SX(x) (((x) * g_ViBackData->sxMul) / g_ViBackData->sxDiv)
#define VI_SY(y) (((y) * g_ViBackData->syMul) / g_ViBackData->syDiv)

/* Same for an inclusive right / bottom edge, as used by fill-mode rectangles */
#define VI_SX_INCL(x) (VI_SX((x) + 1) - 1)
#define VI_SY_INCL(y) (VI_SY((y) + 1) - 1)

/* Physical y back to logical (rounds down) */
#define VI_UNSY(y) (((y) * g_ViBackData->syDiv) / g_ViBackData->syMul)

/* Texture step per physical pixel for a texture rectangle drawn 1 texel per logical pixel
 * (s5.10, 0x400 = 1 texel) */
#define VI_DSDX(d) (((d) * g_ViBackData->sxDiv) / g_ViBackData->sxMul)
#define VI_DTDY(d) (((d) * g_ViBackData->syDiv) / g_ViBackData->syMul)

/* True when the frame is drawn at a different size than its logical coordinates */
#define VI_IS_SCALED() ((g_ViBackData->sxMul != g_ViBackData->sxDiv) || (g_ViBackData->syMul != g_ViBackData->syDiv))

extern OSViMode *viMode;
extern u32 g_viOriginalHstart;
extern u32 g_viOriginalVstart0;
extern u32 g_viOriginalVstart1;
extern Mtx *g_viProjectionMatrix;

/* Hi-res doubles the width. The framebuffers are always sized for it. */
#define VI_MAX_SCALEX 2

/*
 * The front end lays its screens out at 440x330 and draws them into a framebuffer of the size
 * the TV shows: 640x240 on NTSC (330 lines scaled by 8/11) and 640x286 on PAL (13/15, the
 * 286 lines the original 440x330 mode filled). The VI then shows it 1:1.
 */
#define FRONT_LOGICAL_WIDTH   440
#define FRONT_LOGICAL_HEIGHT  330
#define FRONT_PHYS_WIDTH      640
#define FRONT_SCALEX_MUL      16
#define FRONT_SCALEX_DIV      11
#if defined(VERSION_EU)
#define FRONT_SCALEY_MUL      13
#define FRONT_SCALEY_DIV      15
#define FRONT_PHYS_HEIGHT     286
#else
#define FRONT_SCALEY_MUL      8
#define FRONT_SCALEY_DIV      11
#define FRONT_PHYS_HEIGHT     240
#endif

/* Framebuffer height: the taller of the game screen and the front end */
#if FRONT_PHYS_HEIGHT > SCREEN_HEIGHT
#define VI_FB_HEIGHT FRONT_PHYS_HEIGHT
#else
#define VI_FB_HEIGHT SCREEN_HEIGHT
#endif

#define VI_FB_SIZE (SCREEN_WIDTH * VI_MAX_SCALEX * VI_FB_HEIGHT * 2)

/* SCREEN_HEIGHT #define changes based on version (PAL or NTSC) */
extern u8 cfb_16[NUM_VIDEO_FRAME_BUFFERS][VI_FB_SIZE];

void viShake(f32 intensity);

void viInitVideoSettings(void);
s16 viGetX(void);
s16 viGetY(void);
s16 viGetViewLeft(void);
s16 viGetViewTop(void);
s16 viGetViewWidth(void);
s16 viGetViewHeight(void);

#ifdef __GNUC__
/* Inlined at call sites; fr.c still provides the real functions */
extern inline __attribute__((gnu_inline)) s16 viGetX(void) { return g_ViBackData->x; }
extern inline __attribute__((gnu_inline)) s16 viGetY(void) { return g_ViBackData->y; }
extern inline __attribute__((gnu_inline)) s16 viGetViewLeft(void) { return g_ViBackData->viewleft; }
extern inline __attribute__((gnu_inline)) s16 viGetViewTop(void) { return g_ViBackData->viewtop; }
extern inline __attribute__((gnu_inline)) s16 viGetViewWidth(void) { return g_ViBackData->viewx; }
extern inline __attribute__((gnu_inline)) s16 viGetViewHeight(void) { return g_ViBackData->viewy; }
#endif
u8 *viGetFrameBuf2(void);
void viSetFrameBuf2(u8 *buf);
void viSetFovY(f32 fovy);
f32 viGetFovY(void);
void viSetAspect(f32 aspect);
void viSetZRange(f32 near, f32 far);
void viSetUseZBuf(s32 usezbuf);
void viSetViewSize(s16 x, s16 y);
void viSetViewPosition(s16 left, s16 top);
void viInitBuffers(void);
void viVsyncRelated(void);
void viUpdateMode(void);
void viSetBuf(s16 x, s16 y);
void viSetXY(s16 x, s16 y);
void viSetScaleX(s32 scalex);
void viSetScale(s32 xmul, s32 xdiv, s32 ymul, s32 ydiv);
void viSetFrontScale(void);
Gfx *viSetFillColor(Gfx *gdl, s32 r, s32 g, s32 b);
Gfx *viFillScreen(Gfx *gdl);
Gfx *viSetupScreensForNumPlayers(Gfx *gdl);


void viSetVideoMode(s32 m);

Gfx *viClearZBufCurrentPlayer(Gfx *gdl);
Gfx *viSetupCurrentPlayerView(Gfx *gdl);
void viGetZRange(f32 *zrange);
u16 viGetPerspNorm(void);

#endif


