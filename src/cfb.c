#include <ultra64.h>
#include <bondgame.h>

#include "fr.h"

/*
 * The colour framebuffers and the gameplay z-buffer are in separate 1 MB RDRAM banks
 * (ge007.ld places them): the framebuffers at 0x80700000 (bank 7, with the permanent pool
 * after them) and the z-buffer at the top of bank 6 (with the animation frames below it).
 * Each bank keeps its own open page, so the RDP's colour and depth accesses don't close
 * each other's pages, and neither bank holds the CPU's globals and stacks or the level
 * data the RSP reads while the RDP draws.
 */

/* SCREEN_HEIGHT #define changes based on version (PAL or NTSC) */
u8 cfb_16[NUM_VIDEO_FRAME_BUFFERS][SCREEN_WIDTH * VI_MAX_SCALEX * SCREEN_HEIGHT * 2]
    __attribute__((section(".bss.cfb"), aligned(64)));

/* Wide enough for hi-res, and for the front end's 440x330 (4:3) mode */
u8 g_GameZbuf[SCREEN_WIDTH * VI_MAX_SCALEX * SCREEN_HEIGHT * 2]
    __attribute__((section(".bss.zbuf"), aligned(64)));
