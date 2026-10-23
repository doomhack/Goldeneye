#include <ultra64.h>
#include <bondgame.h>

#include "fr.h"

/* SCREEN_HEIGHT #define changes based on version (PAL or NTSC) */
u8 cfb_16[NUM_VIDEO_FRAME_BUFFERS][SCREEN_WIDTH * VI_MAX_SCALEX * SCREEN_HEIGHT * 2];


