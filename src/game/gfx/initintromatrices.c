#include <ultra64.h>
#include <memp.h>
#include "game/menu/title.h"
#include "game/gfx/initintromatrices.h"

void titleAllocMatrices(void)
{
    D_8002A7D0 = 0;
    matrixBufferRareLogo0     = mempAlloc(0x80, MEMPOOL_STAGE);
    matrixBufferGunbarrel0    = mempAlloc(0x40, MEMPOOL_STAGE);
    matrixBufferRareLogo1     = mempAlloc(0x80, MEMPOOL_STAGE);
    matrixBufferRareLogo2     = mempAlloc(0x80, MEMPOOL_STAGE);
    matrixBufferGunbarrel1    = mempAlloc(0x80, MEMPOOL_STAGE);
    matrixBufferIntroBackdrop = mempAlloc(0x80, MEMPOOL_STAGE);
    matrixBufferIntroBond     = mempAlloc(0x80, MEMPOOL_STAGE);
}


