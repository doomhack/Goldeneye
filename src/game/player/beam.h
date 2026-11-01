#ifndef _BEAM_H_
#define _BEAM_H_
#include <ultra64.h>
#include <bondtypes.h>

// Beams: the tracers and laser beams drawn from a gun to what it hit

void beamCreate(struct BeamRecord *beam, ITEM_IDS item, coord3d *from, coord3d *to);
void gunCreateBeamForHand(enum GUNHAND hand);
Gfx *beamRender(Gfx *gdl, BeamRecord *flash, s32 zbufferMode);
void beamTick(BeamRecord *beam);

extern s32 g_NumLiveBeams;

#endif
