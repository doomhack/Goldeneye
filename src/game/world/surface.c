/**
 * The sounds and bullet impacts of each kind of surface (see surface.h).
 */

#include <ultra64.h>
#include "bondconstants.h"
#include "game/world/surface.h"

#define SFX(...) (const u16[]){ __VA_ARGS__ }
#define IMPACTS(...) (const u8[]){ __VA_ARGS__ }

const SurfaceHit g_SurfaceHits[HIT_TYPE_COUNT] = {
    [HIT_DEFAULT]   = { SFX(HIT_BULLET_STONE1_SFX, HIT_BULLET_STONE2_SFX), IMPACTS(7), 0, 1 },
    [HIT_STONE]     = { SFX(HIT_BULLET_STONE1_SFX, HIT_BULLET_STONE2_SFX), IMPACTS(1), 2, 1 },
    [HIT_WOOD]      = { SFX(HIT_BULLET_WOOD_SFX, HIT_BULLET_WOOD2_SFX), IMPACTS(1), 2, 1 },
    [HIT_METAL]     = { SFX(HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_A4_SFX, HIT_BULLET_METAL_A3_SFX), IMPACTS(7), 3, 1 },
    [HIT_GLASS]     = { SFX(HIT_BULLET_GLASS_SFX), IMPACTS(4, 5, 6), 1, 3 },
    [HIT_WATER]     = { SFX(HIT_BULLET_WATER_SFX), IMPACTS(0), 1, 0 },
    [HIT_SNOW]      = { SFX(HIT_BULLET_SNOW_SFX), IMPACTS(1), 1, 1 },
    [HIT_DIRT]      = { SFX(HIT_BULLET_DIRT1_SFX, HIT_BULLET_DIRT2_SFX), IMPACTS(2), 2, 1 },
    [HIT_MUD]       = { SFX(HIT_BULLET_MUD1_SFX, HIT_BULLET_MUD2_SFX, HIT_BULLET_MUD3_SFX), IMPACTS(2), 3, 1 },
    [HIT_TILE]      = { SFX(HIT_BULLET_TILE_SFX), IMPACTS(1), 1, 1 },
    [HIT_METALOBJ]  = { SFX(HIT_METAL_OBJECT1_SFX, HIT_METAL_OBJECT2_SFX), IMPACTS(1, 7), 2, 2 },
    [HIT_CHR]       = { SFX(HIT_BULLET_FLESH_SFX), IMPACTS(2), 1, 1 },
    [HIT_GLASS_XLU] = { SFX(HIT_BULLET_GLASS_SFX), IMPACTS(0x11, 0x12, 0x13), 1, 3 },
};
