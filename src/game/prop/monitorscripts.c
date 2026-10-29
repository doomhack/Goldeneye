/**
 * The monitor scripts (see monitor.c). Scripts 0 to 51 are numbered for the setup files
 * and AI scripts; the rest are parts of them, and the two gun screens.
 *
 * The random ones work as a small state machine: monScriptRandomImage picks an image, then
 * monScriptPickTint a tint, monScriptPickMove a scroll or zoom, and monScriptPickNext
 * either flashes, tints again or goes back for a new image.
 */

#include <ultra64.h>
#include <assets/oddtextures.h>
#include "game/prop/monitor.h"

#define MON_STOP_SCROLL()           MONCMD_STOP_SCROLL
#define MON_SCROLL_BY_X(x, ticks)   MONCMD_SCROLL_BY_X, x, ticks
#define MON_SCROLL_BY_Y(y, ticks)   MONCMD_SCROLL_BY_Y, y, ticks
#define MON_SCROLL_TO_X(x, ticks)   MONCMD_SCROLL_TO_X, x, ticks
#define MON_SCROLL_TO_Y(y, ticks)   MONCMD_SCROLL_TO_Y, y, ticks
#define MON_SCALE_X(x, ticks)       MONCMD_SCALE_X, x, ticks
#define MON_SCALE_Y(y, ticks)       MONCMD_SCALE_Y, y, ticks
#define MON_SCALE(s, ticks)         MON_SCALE_X(s, ticks), MON_SCALE_Y(s, ticks)
#define MON_SET_IMAGE(image)        MONCMD_SET_IMAGE, image
#define MON_WAIT(ticks)             MONCMD_WAIT, ticks
#define MON_GOTO(script)            MONCMD_GOTO, (u32)(script)
#define MON_GOTO_CHANCE(script, c)  MONCMD_GOTO_CHANCE, (u32)(script), c
#define MON_RESTART()               MONCMD_RESTART
#define MON_STOP()                  MONCMD_STOP
#define MON_FADE_TO(rgba, ticks)    MONCMD_FADE_TO, rgba, ticks
#define MON_SET_ROTATION(angle)     MONCMD_SET_ROTATION, angle
#define MON_ROTATE(angle)           MONCMD_ROTATE, angle

/* Chances out of 65536 (CHANCE_ALWAYS misses 1 time in 65536) */
#define CHANCE_2_PERCENT  0x051E
#define CHANCE_10_PERCENT 0x1999
#define CHANCE_20_PERCENT 0x3333
#define CHANCE_40_PERCENT 0x6666
#define CHANCE_60_PERCENT 0x9999
#define CHANCE_ALWAYS     0xFFFF

/* Colours, 0xRRGGBBAA */
#define COLOR_WHITE                0xFFFFFFFF
#define COLOR_SILVER               0xC8C8C8FF
#define COLOR_MINESHAFT            0x383838FF
#define COLOR_MINESHAFT2           0x323232FF
#define COLOR_MINESHAFT3           0x202020FF
#define COLOR_CODGRAY              0x0A0A0AFF
#define COLOR_GRAY                 0x808080FF
#define COLOR_BLACK                0x000000FF
#define COLOR_PERSIANRED           0xC83232FF
#define COLOR_THUNDERBIRD          0xC81414FF
#define COLOR_ALIZARINCRIMSON      0xDC2828FF
#define COLOR_DIESEL               0x280000FF
#define COLOR_LOTUS                0x803838FF
#define COLOR_GOVERNORBAY          0x3232C8FF
#define COLOR_CYPRUS               0x004040FF
#define COLOR_MINSK                0x303080FF
#define COLOR_PESTO                0x808032FF
#define COLOR_VERDUNGREEN          0x404000FF
#define COLOR_APPLE                0x32C832FF
#define COLOR_APPLE2               0x38C838FF
#define COLOR_STRONGGREEN          0x00C800FF
#define COLOR_GREEN                0x0AFF0AFE
#define COLOR_TOMTHUMB             0x406440FF
#define COLOR_SANFELIX             0x0A640AFE
#define COLOR_SEAGREEN             0x308030FF
#define COLOR_GREENKELP            0x1E381EFF
#define COLOR_DARKGREEN            0x0A960AFF
#define COLOR_ALMOSTDARKGREEN      0x0A960AFE
#define COLOR_DARKGREEN2           0x0A780AFF
#define COLOR_DARKERGREEN          0x087808FF
#define COLOR_DEEPFIR              0x003C00FF
#define COLOR_BARELYGREEN          0x00800000
#define COLOR_BARELYGREENOPAQUE    0x008000FF

/* The scripts jump to each other, in both directions */
extern const u32 monScriptBond[];
extern const u32 monScriptDesktopsSatellite[];
extern const u32 monScriptAstrological[];
extern const u32 monScriptSatelliteTargeting[];
extern const u32 monScriptThreeWaves[];
extern const u32 monScriptWave[];
extern const u32 monScriptGreenTextUp[];
extern const u32 monScriptRedTextDown[];
extern const u32 monScriptGreenTextDown[];
extern const u32 monScriptRedBarGraph[];
extern const u32 monScriptBlueBarGraph[];
extern const u32 monScriptGreenBarGraph[];
extern const u32 monScriptRadarStepA[];
extern const u32 monScriptRadarStepB[];
extern const u32 monScriptRadarSweep[];
extern const u32 monScriptRadar[];
extern const u32 monScriptSpinningCube[];
extern const u32 monScriptWorldMap[];
extern const u32 monScriptLocationArming[];
extern const u32 monScriptRedTarget[];
extern const u32 monScriptKarlYelling[];
extern const u32 monScriptSkateboard[];
extern const u32 monScriptPoliceGuy[];
extern const u32 monScriptOff[];
extern const u32 monScriptRandomOfSeven[];
extern const u32 monScriptRandomOfFour[];
extern const u32 monScriptRandomImage[];
extern const u32 monScriptImageShuttle1[];
extern const u32 monScriptImageShuttle2[];
extern const u32 monScriptImageEarthFull1[];
extern const u32 monScriptImageEarthFull2[];
extern const u32 monScriptImageBlueStars[];
extern const u32 monScriptImageGalaxy1[];
extern const u32 monScriptImageGalaxy2[];
extern const u32 monScriptImageEarthText[];
extern const u32 monScriptImageTargetEarth[];
extern const u32 monScriptImageGalaxy3[];
extern const u32 monScriptPickTint[];
extern const u32 monScriptTintRed[];
extern const u32 monScriptTintGreen[];
extern const u32 monScriptTintBlue[];
extern const u32 monScriptPickMove[];
extern const u32 monScriptMoveScrollRight[];
extern const u32 monScriptMoveScrollUpFast[];
extern const u32 monScriptMoveScrollUp[];
extern const u32 monScriptMoveScrollZoom1[];
extern const u32 monScriptMoveScrollZoom2[];
extern const u32 monScriptPickNext[];
extern const u32 monScriptFlash[];
extern const u32 monScriptRedBrightening[];
extern const u32 monScriptGreenBrightening[];
extern const u32 monScriptGreySolid[];
extern const u32 monScriptRedSolid[];
extern const u32 monScriptGreenSolid[];
extern const u32 monScriptBlackSolid[];
extern const u32 monScriptTaser[];

const u32 monScriptBond[] = {
    MON_SET_IMAGE(IMGBOND),
    MON_SCROLL_BY_X(0x400, 20),
    MON_WAIT(20),
    MON_SCROLL_BY_Y(0x400, 20),
    MON_FADE_TO(COLOR_BLACK, 20),
    MON_WAIT(20),
    MON_SCALE(0x200, 20),
    MON_FADE_TO(COLOR_WHITE, 20),
    MON_WAIT(20),
    MON_SCALE(0x400, 20),
    MON_WAIT(20),
    MON_RESTART()
};

const u32 monScriptDesktopsSatellite[] = {
    MON_SET_IMAGE(IMG2DMATH),
    MON_SCROLL_BY_X(0x400, 20),
    MON_WAIT(20),
    MON_SCROLL_BY_Y(0x400, 20),
    MON_FADE_TO(COLOR_BLACK, 20),
    MON_WAIT(20),
    MON_SCALE(0x200, 20),
    MON_FADE_TO(COLOR_WHITE, 20),
    MON_WAIT(20),
    MON_SCALE(0x400, 20),
    MON_WAIT(20),
    MON_SET_IMAGE(IMGSATELLITE),
    MON_SCROLL_BY_X(0x400, 20),
    MON_WAIT(20),
    MON_SCROLL_BY_Y(0x400, 20),
    MON_FADE_TO(COLOR_BLACK, 20),
    MON_WAIT(20),
    MON_SCALE(0x200, 20),
    MON_FADE_TO(COLOR_WHITE, 20),
    MON_WAIT(20),
    MON_SCALE(0x400, 20),
    MON_WAIT(20),
    MON_SET_IMAGE(IMGDESKTOP),
    MON_SCROLL_BY_X(0x400, 20),
    MON_WAIT(20),
    MON_SCROLL_BY_Y(0x400, 20),
    MON_FADE_TO(COLOR_BLACK, 20),
    MON_WAIT(20),
    MON_SCALE(0x200, 20),
    MON_FADE_TO(COLOR_WHITE, 20),
    MON_WAIT(20),
    MON_SCALE(0x400, 20),
    MON_WAIT(20),
    MON_SET_IMAGE(IMGDESKTOPSTAGGERED),
    MON_SCROLL_BY_X(0x400, 20),
    MON_WAIT(20),
    MON_SCROLL_BY_Y(0x400, 20),
    MON_FADE_TO(COLOR_BLACK, 20),
    MON_WAIT(20),
    MON_SCALE(0x200, 20),
    MON_FADE_TO(COLOR_WHITE, 20),
    MON_WAIT(20),
    MON_SCALE(0x400, 20),
    MON_WAIT(20),
    MON_RESTART(),
};

const u32 monScriptAstrological[] = {
    MON_SET_IMAGE(IMGSHUTTLE1), MON_WAIT(80),
    MON_SET_IMAGE(IMGSHUTTLE2), MON_WAIT(80),
    MON_SET_IMAGE(IMGEARTHFULL1), MON_WAIT(80),
    MON_SET_IMAGE(IMGEARTHFULL2), MON_WAIT(80),
    MON_SET_IMAGE(IMGBLUESTARS), MON_WAIT(80),
    MON_SET_IMAGE(IMGGALAXY1), MON_WAIT(80),
    MON_SET_IMAGE(IMGGALAXY2), MON_WAIT(80),
    MON_SET_IMAGE(IMGEARTHTEXT), MON_WAIT(80),
    MON_SET_IMAGE(IMGTARGETEARTH), MON_WAIT(80),
    MON_SET_IMAGE(IMGGALAXY3), MON_WAIT(80),
    MON_RESTART(),
};

const u32 monScriptSatelliteTargeting[] = {
    MON_SET_IMAGE(IMGEARTH), MON_WAIT(80),
    MON_SET_IMAGE(IMGDESKTOPBANG), MON_WAIT(80),
    MON_SET_IMAGE(IMGHEATMAP), MON_WAIT(80),
    MON_SET_IMAGE(IMG2DMATH), MON_WAIT(80),
    MON_SET_IMAGE(IMGSATELLITE), MON_WAIT(80),
    MON_WAIT(80),
    MON_SET_IMAGE(IMGTARGETEARTH), MON_WAIT(80),
    MON_SET_IMAGE(IMGEARTHFULL2), MON_WAIT(80),
    MON_RESTART()
};

const u32 monScriptThreeWaves[] = {
    MON_FADE_TO(COLOR_MINESHAFT3, 1),
    MON_SET_IMAGE(IMGSINE),
    MON_SCROLL_BY_X(0x800, 120),
    MON_WAIT(120),
    MON_SCALE_X(0x100, 1),
    MON_SCALE_Y(0x200, 60),
    MON_SCROLL_BY_X(0xFFFFE000, 120),
    MON_WAIT(120),
    MON_SCALE_X(0x400, 1),
    MON_SCALE_Y(0x400, 60),
    MON_SCALE_Y(0x400, 60),
    MON_SCROLL_BY_X(0x800, 120),
    MON_WAIT(120),
    MON_SCALE_X(0x80, 1),
    MON_SCALE_Y(0x800, 60),
    MON_SCALE_Y(0x400, 120),
    MON_SCROLL_BY_Y(0x400, 60),
    MON_SCROLL_BY_X(0x200, 120),
    MON_WAIT(120),
    MON_RESTART()
};

const u32 monScriptWave[] = {
    MON_FADE_TO(COLOR_MINESHAFT3, 1),
    MON_SET_IMAGE(IMGSINE),
    MON_SCALE_X(0x80, 1),
    MON_SCALE_Y(0x800, 60),
    MON_SCALE_Y(0x400, 120),
    MON_SCROLL_BY_Y(0x400, 10),
    MON_SCROLL_BY_X(0x200, 40),
    MON_WAIT(120),
    MON_RESTART()
};

const u32 monScriptGreenTextUp[] = {
    MON_SET_IMAGE(IMGTEXT),
    MON_FADE_TO(COLOR_BARELYGREENOPAQUE, 1),
    MON_SCROLL_BY_Y(0xFFFFFE00, 80),
    MON_WAIT(120),
    MON_SCROLL_BY_Y(0xFFFFFF00, 20),
    MON_WAIT(120),
    MON_SCROLL_BY_Y(0xFFFFFF80, 10),
    MON_WAIT(40),
    MON_SCROLL_BY_Y(0xFFFFFE00, 40),
    MON_WAIT(60),
    MON_SCROLL_BY_Y(0xFFFFFFC0, 30),
    MON_WAIT(120),
    MON_RESTART()
};

const u32 monScriptRedTextDown[] = {
    MON_SET_IMAGE(IMGTEXT),
    MON_FADE_TO(COLOR_DIESEL, 1),
    MON_SCROLL_BY_Y(0x200, 80),
    MON_WAIT(120),
    MON_SCROLL_BY_Y(0x100, 20),
    MON_WAIT(120),
    MON_SCROLL_BY_Y(0x80, 10),
    MON_WAIT(40),
    MON_SCROLL_BY_Y(0x200, 40),
    MON_WAIT(60),
    MON_SCROLL_BY_Y(0x40, 30),
    MON_WAIT(120),
    MON_SCROLL_BY_Y(0x100, 20),
    MON_WAIT(120),
    MON_SCROLL_BY_Y(0x80, 10),
    MON_RESTART()
};

const u32 monScriptGreenTextDown[] = {
    MON_SET_IMAGE(IMGTEXT),
    MON_FADE_TO(COLOR_DEEPFIR, 1),
    MON_SCROLL_BY_Y(0x200, 80),
    MON_WAIT(120),
    MON_SCROLL_BY_Y(0x80, 10),
    MON_WAIT(40),
    MON_SCROLL_BY_Y(0x100, 20),
    MON_WAIT(120),
    MON_SCROLL_BY_Y(0x80, 10),
    MON_WAIT(40),
    MON_SCROLL_BY_Y(0x200, 40),
    MON_WAIT(60),
    MON_SCROLL_BY_Y(0x40, 30),
    MON_WAIT(120),
    MON_RESTART()
};

const u32 monScriptRedBarGraph[] = {
    MON_SET_IMAGE(IMGBARS),
    MON_FADE_TO(COLOR_VERDUNGREEN, 1),
    MON_SCROLL_BY_X(0x280, 1),
    MON_WAIT(10),
    MON_RESTART()
};

const u32 monScriptBlueBarGraph[] = {
    MON_SET_IMAGE(IMGBARS),
    MON_FADE_TO(COLOR_CYPRUS, 1),
    MON_SCROLL_BY_X(0x280, 1),
    MON_WAIT(10),
    MON_RESTART()
};

const u32 monScriptGreenBarGraph[] = {
    MON_SET_IMAGE(IMGBARS),
    MON_FADE_TO(COLOR_TOMTHUMB, 1),
    MON_SCROLL_BY_X(0xFFFFFD80, 1),
    MON_WAIT(10),
    MON_RESTART()
};

const u32 monScriptRadarStepA[] = {
    MON_FADE_TO(COLOR_GREEN, 20),
    MON_GOTO(monScriptRadarSweep)
};

const u32 monScriptRadarStepB[] = {
    MON_FADE_TO(COLOR_SANFELIX, 20),
    MON_GOTO(monScriptRadarSweep)
};

const u32 monScriptRadarSweep[] = {
    MON_ROTATE(0xB6),
    MON_WAIT(1),
    MON_GOTO_CHANCE(monScriptRadarStepA, CHANCE_2_PERCENT),
    MON_GOTO_CHANCE(monScriptRadarStepB, 0x147A),
    MON_RESTART()
};

const u32 monScriptRadar[] = {
    MON_SET_IMAGE(IMGTRIANGLE),
    MON_FADE_TO(COLOR_ALMOSTDARKGREEN, 1),
    MON_GOTO(monScriptRadarStepB)
};

const u32 monScriptSpinningCube[] = {
    MON_SET_IMAGE(IMGCUBE1),
    MON_FADE_TO(COLOR_MINSK, 30),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGCUBE2),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGCUBE3),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGCUBE4),
    MON_WAIT(5),
    MON_RESTART()
};

const u32 monScriptWorldMap[] = {
    MON_SET_IMAGE(IMGWORLDMAP),
    MON_FADE_TO(COLOR_SEAGREEN, 30),
    MON_SCROLL_BY_X(0xFFFFFC00, 1024),
    MON_WAIT(1440),
    MON_SCROLL_TO_X(0x288, 360),
    MON_SCROLL_TO_Y(0x3AA, 360),
    MON_SCALE(0x80, 300),
    MON_FADE_TO(COLOR_BLACK, 60),
    MON_WAIT(60),
    MON_FADE_TO(COLOR_GRAY, 10),
    MON_WAIT(90),
    MON_FADE_TO(COLOR_APPLE2, 30),
    MON_WAIT(30),
    MON_FADE_TO(COLOR_LOTUS, 60),
    MON_WAIT(60),
    MON_FADE_TO(COLOR_GRAY, 60),
    MON_WAIT(60),
    MON_SCROLL_TO_X(0x200, 360),
    MON_SCROLL_TO_Y(0x200, 360),
    MON_SCALE(0x400, 720),
    MON_WAIT(300),
    MON_SET_IMAGE(IMGWORLDMAP),
    MON_WAIT(420),
    MON_FADE_TO(COLOR_STRONGGREEN, 30),
    MON_WAIT(30),
    MON_FADE_TO(COLOR_GREENKELP, 60),
    MON_WAIT(60),
    MON_RESTART()
};

const u32 monScriptLocationArming[] = {
    MON_FADE_TO(COLOR_BLACK, 1),
    MON_FADE_TO(COLOR_SILVER, 400),
    MON_SET_IMAGE(IMGLOCATION),
    MON_WAIT(680),
    MON_SET_IMAGE(IMGBEGINARMING),
    MON_WAIT(680),
    MON_SET_IMAGE(IMGSEVERNAYA),
    MON_WAIT(180),
    MON_FADE_TO(COLOR_PESTO, 1),
    MON_SET_IMAGE(IMGSEVERNAYA),
    MON_WAIT(200),
    MON_RESTART()
};

const u32 monScriptRedTarget[] = {
    MON_SCALE(0x400, 1),
    MON_FADE_TO(COLOR_THUNDERBIRD, 1),
    MON_SET_IMAGE(IMGAIMER),
    MON_WAIT(600),
    MON_FADE_TO(COLOR_SILVER, 5),
    MON_WAIT(5),
    MON_FADE_TO(COLOR_MINESHAFT, 60),
    MON_SET_IMAGE(IMGSTATIC),
    MON_FADE_TO(COLOR_CODGRAY, 100),
    MON_WAIT(400),
    MON_RESTART()
};

const u32 monScriptKarlYelling[] = {
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_SET_IMAGE(IMGTALK1),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGTALK2),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGTALK3),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGTALK4),
    MON_WAIT(10),
    MON_SET_IMAGE(IMGTALK2),
    MON_WAIT(5),
    MON_RESTART()
};

const u32 monScriptSkateboard[] = {
    MON_SET_IMAGE(IMGSKATEBOARD4),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_SCROLL_BY_X(0x264, 30),
    MON_SET_IMAGE(IMGSKATEBOARD1),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_SET_IMAGE(IMGSKATEBOARD2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_SET_IMAGE(IMGSKATEBOARD3),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_SCROLL_BY_X(0x19C, 40),
    MON_SET_IMAGE(IMGSKATEBOARD2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_SET_IMAGE(IMGSKATEBOARD1),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_FADE_TO(COLOR_DARKGREEN, 0),
    MON_WAIT(3),
    MON_FADE_TO(COLOR_DARKERGREEN, 0),
    MON_WAIT(2),
    MON_RESTART()
};

const u32 monScriptPoliceGuy[] = {
    MON_FADE_TO(COLOR_DARKGREEN2, 0),
    MON_SET_IMAGE(IMGFIST1),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGFIST2),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGFIST3),
    MON_WAIT(5),
    MON_SET_IMAGE(IMGFIST4),
    MON_WAIT(5),
    MON_RESTART()
};

const u32 monScriptOff[] = {
    MON_SET_IMAGE(IMGSINE),
    MON_FADE_TO(COLOR_BARELYGREEN, 1),
    MON_WAIT(5),
    MON_RESTART()
};

const u32 monScriptRandomOfSeven[] = {
    MON_GOTO_CHANCE(monScriptWave, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptKarlYelling, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptRedBarGraph, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptBlueBarGraph, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptGreenBarGraph, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptRedTextDown, CHANCE_20_PERCENT),
    MON_GOTO_CHANCE(monScriptGreenTextDown, CHANCE_40_PERCENT),
    MON_GOTO_CHANCE(monScriptGreenTextUp, CHANCE_ALWAYS),
    MON_RESTART()
};

const u32 monScriptRandomOfFour[] = {
    MON_GOTO_CHANCE(monScriptThreeWaves, CHANCE_2_PERCENT),
    MON_GOTO_CHANCE(monScriptRedBarGraph, CHANCE_2_PERCENT),
    MON_GOTO_CHANCE(monScriptGreenTextUp, CHANCE_2_PERCENT),
    MON_GOTO_CHANCE(monScriptRandomImage, CHANCE_60_PERCENT),
    MON_RESTART()
};

const u32 monScriptRandomImage[] = {
    MON_GOTO_CHANCE(monScriptImageShuttle1, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageShuttle2, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageEarthFull1, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageEarthFull2, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageBlueStars, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageGalaxy1, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageGalaxy2, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageEarthText, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageTargetEarth, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptImageGalaxy3, CHANCE_10_PERCENT),
    MON_WAIT(100),
    MON_RESTART()
};

const u32 monScriptImageShuttle1[] = {
    MON_SET_IMAGE(IMGSHUTTLE1),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageShuttle2[] = {
    MON_SET_IMAGE(IMGSHUTTLE2),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageEarthFull1[] = {
    MON_SET_IMAGE(IMGEARTHFULL1),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageEarthFull2[] = {
    MON_SET_IMAGE(IMGEARTHFULL2),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageBlueStars[] = {
    MON_SET_IMAGE(IMGBLUESTARS),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageGalaxy1[] = {
    MON_SET_IMAGE(IMGGALAXY1),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageGalaxy2[] = {
    MON_SET_IMAGE(IMGGALAXY2),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageEarthText[] = {
    MON_SET_IMAGE(IMGEARTHTEXT),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageTargetEarth[] = {
    MON_SET_IMAGE(IMGTARGETEARTH),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptImageGalaxy3[] = {
    MON_SET_IMAGE(IMGGALAXY3),
    MON_WAIT(20),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_ALWAYS)
};

const u32 monScriptPickTint[] = {
    MON_GOTO_CHANCE(monScriptTintRed, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptTintGreen, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptTintBlue, CHANCE_10_PERCENT),
    MON_FADE_TO(COLOR_SILVER, 60),
    MON_GOTO(monScriptPickMove)
};

const u32 monScriptTintRed[] = {
    MON_FADE_TO(COLOR_PERSIANRED, 60),
    MON_GOTO(monScriptPickMove)
};

const u32 monScriptTintGreen[] = {
    MON_FADE_TO(COLOR_APPLE, 60),
    MON_GOTO(monScriptPickMove)
};

const u32 monScriptTintBlue[] = {
    MON_FADE_TO(COLOR_GOVERNORBAY, 60),
    MON_GOTO(monScriptPickMove)
};

const u32 monScriptPickMove[] = {
    MON_WAIT(50),
    MON_GOTO_CHANCE(monScriptMoveScrollRight, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptMoveScrollUpFast, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptMoveScrollUp, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptMoveScrollZoom1, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptMoveScrollZoom2, CHANCE_10_PERCENT),
    MON_WAIT(300),
    MON_GOTO(monScriptPickNext)
};

const u32 monScriptMoveScrollRight[] = {
    MON_SCROLL_BY_X(0x800, 120),
    MON_WAIT(120),
    MON_GOTO(monScriptPickNext)
};

const u32 monScriptMoveScrollUpFast[] = {
    MON_SCROLL_BY_Y(0x2000, 50),
    MON_WAIT(200),
    MON_GOTO(monScriptPickNext)
};

const u32 monScriptMoveScrollUp[] = {
    MON_SCROLL_BY_Y(0x2000, 200),
    MON_WAIT(200),
    MON_GOTO(monScriptPickNext)
};

const u32 monScriptMoveScrollZoom1[] = {
    MON_SCROLL_TO_X(0x288, 300),
    MON_SCROLL_TO_Y(0x3AA, 300),
    MON_SCALE(0x80, 200),
    MON_WAIT(300),
    MON_SCROLL_TO_X(0x200, 50),
    MON_SCROLL_TO_Y(0x200, 200),
    MON_SCALE(0x400, 720),
    MON_WAIT(600),
    MON_GOTO(monScriptPickNext)
};

const u32 monScriptMoveScrollZoom2[] = {
    MON_SCROLL_TO_X(0x320, 400),
    MON_SCROLL_TO_Y(0x190, 400),
    MON_SCALE(0x80, 200),
    MON_WAIT(300),
    MON_SCROLL_TO_X(0xC8, 200),
    MON_SCROLL_TO_Y(0x190, 800),
    MON_SCALE(0x200, 720),
    MON_WAIT(800),
    MON_SCALE(0x400, 720),
    MON_SCROLL_TO_X(0x200, 100),
    MON_SCROLL_TO_Y(0x200, 60),
    MON_WAIT(500),
    MON_GOTO(monScriptPickNext)
};

const u32 monScriptPickNext[] = {
    MON_WAIT(50),
    MON_GOTO_CHANCE(monScriptPickTint, CHANCE_10_PERCENT),
    MON_GOTO_CHANCE(monScriptFlash, CHANCE_20_PERCENT),
    MON_GOTO(monScriptRandomImage)
};

const u32 monScriptFlash[] = {
    MON_WAIT(50),
    MON_FADE_TO(COLOR_WHITE, 10),
    MON_FADE_TO(COLOR_BLACK, 5),
    MON_FADE_TO(COLOR_WHITE, 10),
    MON_WAIT(25),
    MON_FADE_TO(COLOR_BLACK, 200),
    MON_WAIT(500),
    MON_GOTO(monScriptRandomImage)
};

const u32 monScriptRedBrightening[] = {
    MON_SET_IMAGE(IMGKEYBOARDKEY),
    MON_SCALE(0x200, 0),
    MON_FADE_TO(COLOR_ALIZARINCRIMSON, 60),
    MON_WAIT(60),
    MON_FADE_TO(COLOR_MINESHAFT2, 10),
    MON_WAIT(10),
    MON_RESTART()
};

const u32 monScriptGreenBrightening[] = {
    MON_SET_IMAGE(IMGKEYBOARDKEY),
    MON_SCALE(0x200, 0),
    MON_FADE_TO(COLOR_APPLE, 60),
    MON_WAIT(60),
    MON_FADE_TO(COLOR_MINESHAFT2, 10),
    MON_WAIT(10),
    MON_RESTART()
};

const u32 monScriptGreySolid[] = {
    MON_SET_IMAGE(IMGKEYBOARDKEY),
    MON_SCALE(0x200, 0),
    MON_FADE_TO(COLOR_MINESHAFT2, 10),
    MON_WAIT(10),
    MON_RESTART()
};

const u32 monScriptRedSolid[] = {
    MON_SET_IMAGE(IMGKEYBOARDKEY),
    MON_SCALE(0x200, 0),
    MON_FADE_TO(COLOR_ALIZARINCRIMSON, 10),
    MON_WAIT(10),
    MON_RESTART()
};

const u32 monScriptGreenSolid[] = {
    MON_SET_IMAGE(IMGKEYBOARDKEY),
    MON_SCALE(0x200, 0),
    MON_FADE_TO(COLOR_APPLE, 10),
    MON_WAIT(10),
    MON_RESTART()
};

const u32 monScriptBlackSolid[] = {
    MON_SET_IMAGE(IMGBOND),
    MON_FADE_TO(COLOR_BLACK, 0),
    MON_STOP()
};

const u32 monScriptTaser[] = {
    MON_SET_IMAGE(IMGBOND),
    MON_SCROLL_BY_X(0x400, 20), MON_WAIT(20),
    MON_SCROLL_BY_Y(0x400, 20), MON_FADE_TO(COLOR_BLACK, 20), MON_WAIT(20),
    MON_SCALE(0x200, 20), MON_FADE_TO(COLOR_WHITE, 20), MON_WAIT(20),
    MON_SCALE(0x400, 20), MON_WAIT(20),
    MON_RESTART()
};

const u32 *const g_MonitorScripts[MONITOR_NUM_SCRIPTS] = {
    monScriptBond, /* 0 */
    monScriptDesktopsSatellite, /* 1 */
    monScriptAstrological, /* 2 */
    monScriptThreeWaves, /* 3 */
    monScriptWave, /* 4 */
    monScriptGreenTextUp, /* 5 */
    monScriptRedTextDown, /* 6 */
    monScriptGreenTextDown, /* 7 */
    monScriptRedBarGraph, /* 8 */
    monScriptBlueBarGraph, /* 9 */
    monScriptGreenBarGraph, /* 10 */
    monScriptRadar, /* 11 */
    monScriptSpinningCube, /* 12 */
    monScriptLocationArming, /* 13 */
    monScriptRedTarget, /* 14 */
    monScriptSatelliteTargeting, /* 15 */
    monScriptWorldMap, /* 16 */
    monScriptKarlYelling, /* 17 */
    monScriptSkateboard, /* 18 */
    monScriptPoliceGuy, /* 19 */
    monScriptOff, /* 20 */
    monScriptRandomOfSeven, /* 21 */
    monScriptRandomOfFour, /* 22 */
    monScriptRandomImage, /* 23 */
    monScriptImageShuttle1, /* 24 */
    monScriptImageShuttle2, /* 25 */
    monScriptImageEarthFull1, /* 26 */
    monScriptImageEarthFull2, /* 27 */
    monScriptImageBlueStars, /* 28 */
    monScriptImageGalaxy1, /* 29 */
    monScriptImageGalaxy2, /* 30 */
    monScriptImageEarthText, /* 31 */
    monScriptImageTargetEarth, /* 32 */
    monScriptImageGalaxy3, /* 33 */
    monScriptPickTint, /* 34 */
    monScriptTintRed, /* 35 */
    monScriptTintGreen, /* 36 */
    monScriptTintBlue, /* 37 */
    monScriptPickMove, /* 38 */
    monScriptMoveScrollRight, /* 39 */
    monScriptMoveScrollUpFast, /* 40 */
    monScriptMoveScrollUp, /* 41 */
    monScriptMoveScrollZoom1, /* 42 */
    monScriptMoveScrollZoom2, /* 43 */
    monScriptPickNext, /* 44 */
    monScriptFlash, /* 45 */
    monScriptRedBrightening, /* 46 */
    monScriptGreenBrightening, /* 47 */
    monScriptGreySolid, /* 48 */
    monScriptRedSolid, /* 49 */
    monScriptGreenSolid, /* 50 */
    monScriptBlackSolid, /* 51 */
};
