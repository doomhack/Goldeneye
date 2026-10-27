#ifndef _HEALTHBAR_H_
#define _HEALTHBAR_H_
#include <ultra64.h>
#include <bondtypes.h>

void watchMakeArcVerts(struct WatchVertex *vtx, s32 numverts, f32 scale, s32 arg3);
Gfx *watchRenderBackground(Gfx *gdl, struct WatchVertex *watch_verts, s32 unused_arg2, s32 drawFan);
Gfx *watchRenderQuad(Gfx *gdl, s32 *arg1);
Gfx *hudRenderGaugeBar(Gfx *gdl, uintptr_t vtxaddr, s32 numvertices);
void hudMakeDamageSegments(struct damage_display_val *arg0, s32 arg1, s32 arg2, f32 arg3);
struct WatchVertex *watchMakeRectVerts(struct WatchVertex *vtx, s32 startx, s32 startz, s32 width, s32 height, s32 horizontal_offset, s32 vertical_offset);

#endif
