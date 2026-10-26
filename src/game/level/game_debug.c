#include <ultra64.h>
#include <deb.h>
#include "game/asset/fileload.h"
#include <memp.h>
#include "game/level/game_debug.h"

// data
//D:8004EAE0
s32 D_8004EAE0[] = {0, 0, 0, 0};

void gameInit(void) {
    debTryAdd(&D_8004EAE0, "game_c_debug");
}

void reset_mem_bank_5(void) {
    fileForgetMePool();
    mempResetPool(MEMPOOL_ME);
    fileForgetPool(MEMPOOL_ME);
}

void sub_GAME_7F0D1A7C(void) {
    mempClosePool(MEMPOOL_ME);
    fileForgetPool(MEMPOOL_ME);
}





