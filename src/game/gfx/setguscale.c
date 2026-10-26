#include <ultra64.h>
#include "game/prop/explosion.h"

void explosionInitScaleMtx(void) {
    guScale((Mtx *)&g_ExplosionScaleMtx, 0.1f, 0.1f, 0.1f);
}

