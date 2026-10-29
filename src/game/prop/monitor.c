/**
 * Monitors: the screens in levels, and the screens on the rocket launcher and taser, show
 * an image that scrolls, zooms, rotates and changes colour. Each screen runs a small
 * script (monitorscripts.c) each time it is drawn.
 *
 * Image positions and scales move smoothly: a command sets a target and a time, and each
 * tick moves a fraction of the way. Scripts can jump to other scripts at random, which
 * uses the game's random numbers, so a screen being drawn or not changes everything after.
 */

#include <ultra64.h>
#include <math.h>
#include <bondconstants.h>
#include "game/prop/monitor.h"
#include "game/gfx/dyn.h"
#include "game/gfx/imagebank.h"
#include "game/gfx/texcmds.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/model/model.h"
#include "random.h"

#if defined(VERSION_EU)
#define MONITOR_TIMER_DELTA g_GlobalTimerDeltaRaw
#else
#define MONITOR_TIMER_DELTA g_GlobalTimerDelta
#endif

/* Images numbered below this are in the monitor image bank; anything else is a pointer */
#define MONITOR_NUM_IMAGES 100

extern const u32 *const g_MonitorScripts[MONITOR_NUM_SCRIPTS];
extern const u32 monScriptRocketSight[];
extern const u32 monScriptTaser[];

MonitorRecord g_GunScreenRocket;
MonitorRecord g_GunScreenTaser;

static const MonitorRecord s_InitialMonitor = {
    .script = NULL,
    .pc = 0,
    .waitTicks = -1,
    .image = NULL,
    .rotation = 0.0f,
    .scaleX = { 1.0f, 0.0f, 0.0f, 1.0f, 1.0f },
    .scaleY = { 1.0f, 0.0f, 0.0f, 1.0f, 1.0f },
    .midX = { 0.5f, 0.0f, 0.0f, 0.5f, 0.5f },
    .midY = { 0.5f, 0.0f, 0.0f, 0.5f, 0.5f },
    .red = { 0xff, 0xff, 0xff },
    .green = { 0xff, 0xff, 0xff },
    .blue = { 0xff, 0xff, 0xff },
    .alpha = { 0xff, 0xff, 0xff },
    .colourFrac = 1.0f,
    .colourRate = 0.0f,
};

static struct sImageTableEntry *imageFromArg(u32 arg)
{
    if (arg < MONITOR_NUM_IMAGES)
    {
        return &g_ImagesMonitor[arg];
    }

    return (struct sImageTableEntry *)arg;
}

static void startScript(MonitorRecord *mon, const u32 *script)
{
    mon->script = script;
    mon->pc = 0;
}

static void reset(MonitorRecord *mon, const u32 *script)
{
    *mon = s_InitialMonitor;
    mon->image = imageFromArg(0);
    startScript(mon, script);
}

/** Resets the gun screens for a new level */
void monitorsReset(void)
{
    reset(&g_GunScreenRocket, monScriptRocketSight);
    reset(&g_GunScreenTaser, monScriptTaser);
}

static const u32 *getScript(s32 scriptNum)
{
    if (scriptNum < 0 || scriptNum >= MONITOR_NUM_SCRIPTS)
    {
        scriptNum = 0;
    }

    return g_MonitorScripts[scriptNum];
}

/** Sets up a level monitor showing one of the numbered scripts */
void monitorInit(MonitorRecord *mon, s32 scriptNum)
{
    reset(mon, getScript(scriptNum));
}

/** Switches to another script, keeping the screen as it is */
void monitorSetScript(MonitorRecord *mon, s32 scriptNum)
{
    startScript(mon, getScript(scriptNum));
}

void monitorSetImage(MonitorRecord *mon, struct sImageTableEntry *image)
{
    mon->image = image;
}

static void startTween(MonitorTween *tween, f32 to, u32 ticks)
{
    tween->frac = 0.0f;
    tween->rate = 1.0f / ticks;
    tween->from = tween->value;
    tween->to = to;
}

static f32 argToFraction(u32 arg)
{
    return (s32)arg * (1.0f / 1024.0f);
}

static void startFade(MonitorChannel *channel, u8 to)
{
    channel->from = channel->value;
    channel->to = to;
}

/** Runs commands until one waits */
static void runScript(MonitorRecord *mon)
{
    while (TRUE)
    {
        const u32 *cmd = &mon->script[mon->pc];

        switch (cmd[0])
        {
            case MONCMD_STOP_SCROLL:
                mon->midX.rate = 0.0f;
                mon->midY.rate = 0.0f;
                mon->pc += 1;
                break;
            case MONCMD_SCROLL_BY_X:
                startTween(&mon->midX, mon->midX.value + argToFraction(cmd[1]), cmd[2]);
                mon->pc += 3;
                break;
            case MONCMD_SCROLL_BY_Y:
                startTween(&mon->midY, mon->midY.value + argToFraction(cmd[1]), cmd[2]);
                mon->pc += 3;
                break;
            case MONCMD_SCROLL_TO_X:
                startTween(&mon->midX, argToFraction(cmd[1]), cmd[2]);
                mon->pc += 3;
                break;
            case MONCMD_SCROLL_TO_Y:
                startTween(&mon->midY, argToFraction(cmd[1]), cmd[2]);
                mon->pc += 3;
                break;
            case MONCMD_SCALE_X:
                startTween(&mon->scaleX, argToFraction(cmd[1]), cmd[2]);
                mon->pc += 3;
                break;
            case MONCMD_SCALE_Y:
                startTween(&mon->scaleY, argToFraction(cmd[1]), cmd[2]);
                mon->pc += 3;
                break;
            case MONCMD_SET_IMAGE:
                mon->image = imageFromArg(cmd[1]);
                mon->pc += 2;
                break;
            case MONCMD_WAIT:
                if (mon->waitTicks < 0)
                {
                    mon->waitTicks = cmd[1];
                    return;
                }

                mon->waitTicks -= g_ClockTimer;

                if (mon->waitTicks >= 0)
                {
                    return;
                }

                mon->pc += 2;
                break;
            case MONCMD_GOTO:
                startScript(mon, (const u32 *)cmd[1]);
                break;
            case MONCMD_GOTO_CHANCE:
                if ((randomGetNext() >> 16) < cmd[2])
                {
                    startScript(mon, (const u32 *)cmd[1]);
                }
                else
                {
                    mon->pc += 3;
                }
                break;
            case MONCMD_RESTART:
                mon->pc = 0;
                break;
            case MONCMD_STOP:
                return;
            case MONCMD_FADE_TO:
                mon->colourFrac = 0.0f;
                mon->colourRate = 1.0f / cmd[2];
                startFade(&mon->red, cmd[1] >> 24);
                startFade(&mon->green, cmd[1] >> 16);
                startFade(&mon->blue, cmd[1] >> 8);
                startFade(&mon->alpha, cmd[1]);
                mon->pc += 3;
                break;
            case MONCMD_SET_ROTATION:
                mon->rotation = (s32)cmd[1] * M_TAU_F / M_U16_MAX_VALUE_F;
                mon->pc += 2;
                break;
            case MONCMD_ROTATE:
                mon->rotation += MONITOR_TIMER_DELTA * (s32)cmd[1] * M_TAU_F / M_U16_MAX_VALUE_F;

                if (mon->rotation >= M_TAU_F)
                {
                    mon->rotation -= M_TAU_F;
                }

                if (mon->rotation < 0.0f)
                {
                    mon->rotation += M_TAU_F;
                }

                mon->pc += 2;
                break;
        }
    }
}

static void tickTween(MonitorTween *tween)
{
    if (tween->rate > 0.0f)
    {
        tween->frac += tween->rate * MONITOR_TIMER_DELTA;

        if (tween->frac < 1.0f)
        {
            tween->value = tween->from + (tween->to - tween->from) * tween->frac;
        }
        else
        {
            tween->frac = 1.0f;
            tween->rate = 0.0f;
            tween->value = tween->to;
        }
    }
}

static void lerpChannel(MonitorChannel *channel, f32 frac)
{
    channel->value = channel->from + (s32)((channel->to - channel->from) * frac);
}

static void tickColour(MonitorRecord *mon)
{
    if (mon->colourRate > 0.0f)
    {
        mon->colourFrac += mon->colourRate * MONITOR_TIMER_DELTA;

        if (mon->colourFrac < 1.0f)
        {
            lerpChannel(&mon->red, mon->colourFrac);
            lerpChannel(&mon->green, mon->colourFrac);
            lerpChannel(&mon->blue, mon->colourFrac);
            lerpChannel(&mon->alpha, mon->colourFrac);
        }
        else
        {
            mon->colourFrac = 1.0f;
            mon->colourRate = 0.0f;
            mon->red.value = mon->red.to;
            mon->green.value = mon->green.to;
            mon->blue.value = mon->blue.to;
            mon->alpha.value = mon->alpha.to;
        }
    }
}

/** Maps the image onto the screen's four corners, as texture coordinates */
static void setTextureCoords(const MonitorRecord *mon, Vertex *vertices)
{
    const struct sImageTableEntry *image = mon->image;
    f32 halfX1 = mon->scaleX.value / 2.0f;
    f32 halfY1 = mon->scaleY.value / 2.0f;
    f32 halfX2 = halfX1;
    f32 halfY2 = halfY1;

    if (mon->rotation != 0.0f)
    {
        /* Rotating the square of coordinates by its corners: the half-diagonal is sqrt 2 */
        f32 cosRot = cosf(mon->rotation) * 1.4141999f;
        f32 sinRot = sinf(mon->rotation) * 1.4141999f;

        halfX1 *= cosRot;
        halfY1 *= sinRot;
        halfX2 *= sinRot;
        halfY2 *= cosRot;
    }

    /* 10.5 fixed point texels */
    vertices[0].s = image->width * (mon->midX.value + halfX1) * 32.0f;
    vertices[0].t = image->height * (mon->midY.value + halfY1) * 32.0f;
    vertices[1].s = image->width * (mon->midX.value - halfX2) * 32.0f;
    vertices[1].t = image->height * (mon->midY.value + halfY2) * 32.0f;
    vertices[2].s = image->width * (mon->midX.value - halfX1) * 32.0f;
    vertices[2].t = image->height * (mon->midY.value - halfY1) * 32.0f;
    vertices[3].s = image->width * (mon->midX.value + halfX2) * 32.0f;
    vertices[3].t = image->height * (mon->midY.value - halfY2) * 32.0f;
}

/**
 * Runs the screen's script for this frame, and draws it on the model's screen node: a
 * quad (the node's collision display list) with its own four vertices, branched to from
 * the node's display list.
 */
Gfx *monitorTickAndDraw(Model *model, ModelNode *node, MonitorRecord *mon, Gfx *gdl, s32 zbufferMode, s32 blendMode)
{
    union ModelRoData *rodata;
    union ModelRwData *rwdata;
    Vertex *vertices;
    Gfx *branch;
    s32 i;

    if (node == NULL || (node->Opcode & 0xff) != MODELNODE_OPCODE_DLCOLLISION)
    {
        return gdl;
    }

    vertices = dynAllocateVertices(4);
    branch = gdl++;
    rodata = node->Data;
    rwdata = modelGetNodeRwData(model, node);

    runScript(mon);
    tickTween(&mon->scaleX);
    tickTween(&mon->scaleY);
    tickTween(&mon->midX);
    tickTween(&mon->midY);
    tickColour(mon);

    rwdata->DisplayListCollisions.gdl = gdl;
    rwdata->DisplayListCollisions.Vertices = vertices;

    for (i = 0; i < 4; i++)
    {
        vertices[i] = rodata->DisplayListCollisions.Vertices[i];
    }

    if (mon->image != NULL)
    {
        setTextureCoords(mon, vertices);
    }

    for (i = 0; i < 4; i++)
    {
        vertices[i].r = mon->red.value;
        vertices[i].g = mon->green.value;
        vertices[i].b = mon->blue.value;
        vertices[i].a = mon->alpha.value;
    }

    if (mon->alpha.value < 255)
    {
        blendMode = 2;
    }

    gSPSetGeometryMode(gdl++, G_CULL_BACK);

    texSelect(&gdl, mon->image, blendMode, zbufferMode, 2);

    gSPMatrix(gdl++, osVirtualToPhysical(model->render_pos), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(gdl++, SPSEGMENT_MODEL_VTX, osVirtualToPhysical(vertices));
    gSPVertex(gdl++, 0x04000000, 4, 0);
    gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPEndDisplayList(gdl++);

    gSPBranchList(branch, gdl);

    return gdl;
}
