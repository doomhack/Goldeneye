#include <ultra64.h>
#include "game/model/chrobjdata.h"

void bheadInitGaitModel(void) {
  player_gait_object_header.RootNode = (int)&player_gait_hdr;
  return;
}

