#include <ultra64.h>
#include "game/model/model.h"

//uncomment when actor is worked on
//#include "chr.h"
extern s32 get_ptr_allocated_block_for_vertices NO_SDATA;

void init_weapon_animation_groups_maybe(void) {
    modelSetVtxAllocator(&get_ptr_allocated_block_for_vertices);
    initWeaponAnimGroups();
}
