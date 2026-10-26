#include <ultra64.h>
#include <memp.h>
#include "image.h"
#include "initmttex.h"
#include "player.h"

#define COOP_EXTRA_TEXTURE_BYTES (512 * 1024)

void set_mt_tex_alloc(void)
{  
    g_TexCacheCount = 0;

    if (tokenFind(1, "-mt"))
    {
        bytes = strtol(tokenFind(1, "-mt"), 0x0, 0) * 1024; //get KB
    }

    if (g_CoopMode)
    {
        // The per-level sizes are tuned for solo, where Bond's body textures
        // go in the weapon buffer. Co-op loads both bodies MP style into this
        // pool, and textures that no longer fit draw as garbage.
        bytes += COOP_EXTRA_TEXTURE_BYTES;
    }

    texInitPool(&g_TexSharedPool, mempAllocBytesInBank(bytes, MEMPOOL_STAGE), bytes);
}
