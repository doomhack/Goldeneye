#include <ultra64.h>
#include "game/model/model.h"

//uncomment when actor is worked on
//#include "chr.h"
extern s32 dynAllocateVerticesDiscard NO_SDATA;

void gunsInitAnimGroups(void) {
    modelSetVtxAllocator(&dynAllocateVerticesDiscard);
    initWeaponAnimGroups();
}
