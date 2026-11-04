#include <ultra64.h>
#include <bondgame.h>

#include "fr.h"

/*
 * The colour framebuffers and the gameplay z-buffer each get a 1 MB RDRAM bank to
 * themselves (ge007.ld places them): the framebuffers at 0x80700000 (bank 7) and the
 * z-buffer at 0x80600000 (bank 6). Each bank keeps its own open page, so the RDP's colour
 * and depth accesses don't close each other's pages, and neither shares a bank with the
 * CPU's globals and stacks or with the level data the RSP reads.
 */

/* SCREEN_HEIGHT #define changes based on version (PAL or NTSC) */
u8 cfb_16[NUM_VIDEO_FRAME_BUFFERS][SCREEN_WIDTH * VI_MAX_SCALEX * SCREEN_HEIGHT * 2]
    __attribute__((section(".bss.cfb"), aligned(64)));

/* Wide enough for hi-res, and for the front end's 440x330 (4:3) mode */
u8 g_GameZbuf[SCREEN_WIDTH * VI_MAX_SCALEX * SCREEN_HEIGHT * 2]
    __attribute__((section(".bss.zbuf"), aligned(64)));
