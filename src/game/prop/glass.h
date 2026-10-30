#ifndef _GLASS_H_
#define _GLASS_H_
#include <ultra64.h>
#include <bondtypes.h>

/* Shard slots, shared by all players (so fewer each in multiplayer) */
#define SHARD_MAX 200

/* A falling, spinning triangle of broken glass */
struct Shard {
    s32 age;                   /* ticks; 0 is a free slot */
    coord3d pos;
    coord3d rot;
    coord3d vel;               /* per tick; y falls with gravity */
    coord3d rotvel;
    u32 unused34;
    Vtx vertices[3];
};

void glassCreateShards(coord3d *pos, coord3d *xaxis, coord3d *yaxis, coord3d *zaxis, f32 xmin, f32 xmax, f32 ymin, f32 ymax);
void glassCreateShard(coord3d *pos, f32 angle, f32 size);
void glassTickShards(void);
Gfx *glassRenderShards(Gfx *gdl);
void glassInitShards(void);
void glassResetShards(void);

#endif
