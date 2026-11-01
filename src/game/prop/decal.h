#ifndef _DECAL_H_
#define _DECAL_H_
#include <ultra64.h>
#include <bondtypes.h>

#define SCORCH_BUFFER_LEN 20
#define IMPACT_BUFFER_LEN 100
#define IMPACT_TYPE_COUNT 20

/* The impact the paintball cheat swaps every bullet hole for */
#define IMPACT_TYPE_PAINTBALL 16

/* A scorch mark on the floor where an explosion went off */
struct Scorch {
    s16 room;                  /* -1 while the slot is free */
    Vtx vertices[4];           /* relative to the room */
};

/* A bullet hole or mark on a wall or an object */
struct Impact {
    // What the passes over every impact test comes first, in one 16-byte line.
    s16 room;                  /* -1 while the slot is free; 1 on an object */
    s16 type;                  /* index into the impact types */
    PropRecord *prop;          /* the object it's on, or NULL for a wall */
    s8 node;                   /* the object's model node it's on */
    s8 onGlass;                /* on glass, so it goes when the glass breaks */
    Vtx vertices[4];           /* relative to the room, or to the object's model node */
};

void scorchCreate(coord3d *pos, f32 size, s16 room);
Gfx *scorchesRender(Gfx *gdl);
void impactCreate(coord3d *pos, coord3d *normal, s16 type, s16 room, PropRecord *prop, s8 node, s8 onGlass);
void impactsRemoveFromProp(PropRecord *prop, bool onGlass);
void impactsRemoveTranslucent(PropRecord *prop);
Gfx *impactsRenderOnProp(Gfx *gdl, PropRecord *prop, s32 xlupass);
Gfx *impactsRenderOnWalls(Gfx *gdl);
void decalsInit(void);

#endif
