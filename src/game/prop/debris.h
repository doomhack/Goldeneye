#ifndef _DEBRIS_H_
#define _DEBRIS_H_
#include <ultra64.h>
#include <bondtypes.h>

/* Debris slots, shared by all players (so fewer each in multiplayer) */
#define DEBRIS_MAX 200

/* A grey chip of rubble, thrown out by an explosion or a bullet hitting a wall. It tumbles
 * and falls for a few seconds. */
struct Debris {
    s32 age;                   /* ticks; 0 is a free slot */
    coord3d pos;
    coord3d rot;
    coord3d vel;               /* per tick; y falls with gravity */
    coord3d rotvel;
    Vtx vertices[4];
};

void debrisCreate(coord3d *pos, f32 scatter, f32 hvel, f32 vvel, f32 size);
void debrisTick(void);
Gfx *debrisRender(Gfx *gdl);
void debrisInit(void);

#endif
